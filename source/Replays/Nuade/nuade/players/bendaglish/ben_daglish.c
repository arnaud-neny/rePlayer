// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Collin
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/bendaglish.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Tables

#define FINE_TUNE_START_INDEX (4 * 12)
#define BD_MAX_TABLE_ENTRIES 256 // sub-songs, tracks and samples are all numbered by a byte

static const int s_fine_tune[] = {
    0x1000,   0x10F3,   0x11F5,   0x1306,   0x1429,   0x155B,   0x16A0,   0x17F9,   0x1965,   0x1AE9,   0x1C82,
    0x1E34,   0x2000,   0x21E7,   0x23EB,   0x260D,   0x2851,   0x2AB7,   0x2D41,   0x2FF2,   0x32CC,   0x35D1,
    0x3904,   0x3C68,   0x4000,   0x43CE,   0x47D6,   0x4C1B,   0x50A2,   0x556E,   0x5A82,   0x5FE4,   0x6597,
    0x6BA2,   0x7208,   0x78D0,   0x8000,   0x879C,   0x8FAC,   0x9837,   0xA145,   0xAADC,   0xB504,   0xBFC8,
    0xCB2F,   0xD744,   0xE411,   0xF1A1,

    0x10000,  0x10F38,  0x11F59,  0x1306F,  0x1428A,  0x155B8,  0x16A09,  0x17F91,  0x1965F,  0x1AE89,  0x1C823,
    0x1E343,  0x20000,  0x21E71,  0x23EB3,  0x260DF,  0x28514,  0x2AB70,  0x2D413,  0x2FF22,  0x32CBF,  0x35D13,
    0x39047,  0x3C686,  0x40000,  0x43CE3,  0x47D66,  0x4C1BF,  0x50A28,  0x556E0,  0x5A827,  0x5FE44,  0x6597F,
    0x6BA27,  0x7208F,  0x78D0D,  0x80000,  0x879C7,  0x8FACD,  0x9837F,  0xA1451,  0xAADC0,  0xB504F,  0xBFC88,
    0xCB2FF,  0xD7450,  0xE411F,  0xF1A1B,  0x100000, 0x10F38F, 0x11F59A, 0x1307B2, 0x1428A2, 0x155B81, 0x16A09E,
    0x17F910, 0x1965FE, 0x1AE8A0, 0x1C823E, 0x1E3438, 0x200000, 0x21E71F, 0x23EB35, 0x260DFC, 0x285145, 0x2AB702,
    0x2D413C, 0x2FF221, 0x32CBFD, 0x35D13F, 0x39047C, 0x3C6870,
};

#define FINE_TUNE_COUNT (sizeof(s_fine_tune) / sizeof(s_fine_tune[0]))

static const int8_t s_empty_sample[] = { 0, 0, 0, 0 };

// Byte reading helpers

static inline uint16_t read_be16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static inline int16_t read_be16s(const uint8_t* p) {
    return (int16_t)((p[0] << 8) | p[1]);
}

static inline uint32_t read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

// Period to frequency

static uint16_t bd_calc_period(int period_index, uint16_t fine_tune_period) {
    if (period_index < 0 || period_index >= (int)FINE_TUNE_COUNT) {
        return 0;
    }

    int ft = s_fine_tune[period_index];
    uint16_t period
        = (uint16_t)((((((uint32_t)ft & 0xffff) * fine_tune_period) >> 16) + (((uint32_t)ft >> 16) * fine_tune_period))
                     & 0xffff);
    return period;
}

// Format identification

// The identification follows offsets found in the data, so every read is range checked; bytes outside the module
// read as zero.
typedef struct BdScan {
    const uint8_t* data;
    size_t length;
} BdScan;

static uint8_t bd_scan_byte(const BdScan* scan, int index) {
    if (index < 0 || (size_t)index >= scan->length)
        return 0;
    return scan->data[index];
}

#define SB(index) bd_scan_byte(buf, (index))

static bool bd_extract_info_from_init(const BdScan* buf, size_t buf_len, int start_of_init,
                                      int* out_sub_song_list_offset, int* out_sample_info_offset_table_offset) {
    int search_length = (int)buf_len;
    int index;

    // Find sub-song information offset
    for (index = start_of_init; index < (search_length - 6); index += 2) {
        if (SB(index) == 0x41 && SB(index + 1) == 0xfa && SB(index + 4) == 0x22 && SB(index + 5) == 0x08) {
            break;
        }
    }

    if (index >= (search_length - 6)) {
        return false;
    }

    *out_sub_song_list_offset = (((int8_t)SB(index + 2) * 256) | SB(index + 3)) + index + 2;
    index += 4;

    // Find sample information offset table offset
    for (; index < (search_length - 6); index += 2) {
        if (SB(index) == 0x41 && SB(index + 1) == 0xfa && SB(index + 4) == 0x23 && SB(index + 5) == 0x48) {
            break;
        }
    }

    if (index >= (search_length - 6)) {
        return false;
    }

    *out_sample_info_offset_table_offset = (((int8_t)SB(index + 2) * 256) | SB(index + 3)) + index + 2;
    return true;
}

static bool bd_extract_info_from_play(const BdScan* buf, size_t buf_len, int start_of_play,
                                      int* out_track_offset_table_offset, int* out_tracks_offset) {
    int search_length = (int)buf_len;
    int index;

    // Find track offset table offset
    for (index = start_of_play; index < (search_length - 6); index += 2) {
        if (SB(index) == 0x47 && SB(index + 1) == 0xfa
            && ((SB(index + 4) == 0x48 && SB(index + 5) == 0x80) || (SB(index + 4) == 0xd0 && SB(index + 5) == 0x40))) {
            break;
        }
    }

    if (index >= (search_length - 6)) {
        return false;
    }

    *out_track_offset_table_offset = (((int8_t)SB(index + 2) * 256) | SB(index + 3)) + index + 2;
    index += 4;

    // Find tracks offset
    for (; index < (search_length - 6); index += 2) {
        if (SB(index) == 0x47 && SB(index + 1) == 0xfa && SB(index + 4) == 0xd6 && SB(index + 5) == 0xc0) {
            break;
        }
    }

    if (index >= (search_length - 6)) {
        return false;
    }

    *out_tracks_offset = (((int8_t)SB(index + 2) * 256) | SB(index + 3)) + index + 2;
    return true;
}

static bool bd_find_features_in_play(const BdScan* buf, size_t buf_len, int start_of_play, BdFeatures* f) {
    int search_length = (int)buf_len;
    int index;

    // Check for counter feature
    f->enable_counter = false;

    if (start_of_play >= (search_length - 16)) {
        return false;
    }

    if (SB(start_of_play + 4) == 0x10 && SB(start_of_play + 5) == 0x3a && SB(start_of_play + 8) == 0x67
        && SB(start_of_play + 14) == 0x53 && SB(start_of_play + 15) == 0x50) {
        index = (((int8_t)SB(start_of_play + 6) * 256) | SB(start_of_play + 7)) + start_of_play + 6;
        if (index >= search_length) {
            return false;
        }
        f->enable_counter = SB(index) != 0;
    }

    // Check effect calls
    f->enable_portamento = false;
    f->enable_volume_fade = false;

    for (index = start_of_play; index < (search_length - 2); index += 2) {
        if (SB(index) == 0x53 && SB(index + 1) == 0x2c) {
            break;
        }
    }

    if (index >= (search_length - 2)) {
        return false;
    }

    for (; index >= start_of_play; index -= 2) {
        if (SB(index) == 0x49 && SB(index + 1) == 0xfa) {
            break;
        }

        if (SB(index) == 0x61 && SB(index + 1) == 0x00) {
            int method_index = (((int8_t)SB(index + 2) * 256) | SB(index + 3)) + index + 2;

            if (method_index >= (search_length - 14)) {
                return false;
            }

            if (SB(method_index) == 0x4a && SB(method_index + 1) == 0x2c && SB(method_index + 4) == 0x67
                && SB(method_index + 6) == 0x6a && SB(method_index + 8) == 0x30 && SB(method_index + 9) == 0x29) {
                f->enable_portamento = true;
            } else if (SB(method_index) == 0x4a && SB(method_index + 1) == 0x2c && SB(method_index + 4) == 0x67
                       && SB(method_index + 6) == 0x4a && SB(method_index + 7) == 0x2c
                       && SB(method_index + 10) == 0x67) {
                f->enable_volume_fade = true;
            } else {
                return false;
            }
        }
    }

    // Check for position effects
    f->max_track_value = 0x80;

    for (index = start_of_play; index < (search_length - 6); index += 2) {
        if (SB(index) == 0x10 && SB(index + 1) == 0x1b) {
            break;
        }
    }

    if (index >= (search_length - 6)) {
        return false;
    }

    if ((SB(index + 2) == 0xb0 && SB(index + 3) == 0x3c) || (SB(index + 2) == 0x0c && SB(index + 3) == 0x00)) {
        f->max_track_value = SB(index + 5);
    }

    for (index += 4; index < (search_length - 6); index += 2) {
        if (((SB(index) == 0xb0 && SB(index + 1) == 0x3c) || (SB(index) == 0x0c && SB(index + 1) == 0x00))
            && SB(index + 4) == 0x6c) {
            break;
        }
    }

    if (index >= (search_length - 6)) {
        return false;
    }

    int effect = (SB(index + 2) << 8) | SB(index + 3);
    f->enable_c0_track_loop = effect == 0x00c0;
    f->enable_f0_track_loop = effect == 0x00f0;

    index = SB(index + 5) + index + 6;

    if (index >= (int)buf_len - 1) {
        return false;
    }

    if (SB(index) == 0x02 && SB(index + 1) == 0x40) {
        f->set_sample_mapping_version = 1;
    } else if (SB(index) == 0x04 && SB(index + 1) == 0x00) {
        f->set_sample_mapping_version = 2;
    } else {
        return false;
    }

    return true;
}

static bool bd_find_features_in_handle_effects(const BdScan* buf, size_t buf_len, int start_of_play, BdFeatures* f) {
    int search_length = (int)buf_len;
    int index;

    if (SB(start_of_play) != 0x61 || SB(start_of_play + 1) != 0x00) {
        return false;
    }

    int start_of_handle_effects = (((int8_t)SB(start_of_play + 2) * 256) | SB(start_of_play + 3)) + start_of_play + 2;

    // Find call to sample handler callback method
    for (index = start_of_handle_effects; index < (search_length - 2); index += 2) {
        if (SB(index) == 0x4e && SB(index + 1) == 0x90) {
            break;
        }
    }

    if (index >= (search_length - 2)) {
        return false;
    }

    int callback_index = index;

    // Search back after effect method calls
    f->enable_sample_effects = false;
    f->enable_final_volume_slide = false;

    for (; index >= start_of_handle_effects; index -= 2) {
        if (SB(index) == 0x4e && SB(index + 1) == 0x75) {
            break;
        }

        if (SB(index) == 0x61 && SB(index + 1) == 0x00) {
            int method_index = (((int8_t)SB(index + 2) * 256) | SB(index + 3)) + index + 2;

            if (method_index >= (search_length - 14)) {
                return false;
            }

            if (SB(method_index) == 0x30 && SB(method_index + 1) == 0x2b && SB(method_index + 4) == 0x67
                && ((SB(method_index + 6) == 0xb0 && SB(method_index + 7) == 0x7c)
                    || (SB(method_index + 6) == 0x0c && SB(method_index + 7) == 0x40))
                && SB(method_index + 8) == 0xff && SB(method_index + 9) == 0xff) {
                f->enable_sample_effects = true;
            } else if (SB(method_index) == 0x30 && SB(method_index + 1) == 0x2b && SB(method_index + 4) == 0x67
                       && SB(method_index + 6) == 0x53 && SB(method_index + 7) == 0x6b) {
                f->enable_final_volume_slide = true;
            } else {
                return false;
            }
        }
    }

    if (SB(callback_index + 6) != 0x6e && SB(callback_index + 6) != 0x66) {
        return false;
    }

    index = SB(callback_index + 7) + callback_index + 8;
    if (index >= (search_length - 6)) {
        return false;
    }

    // Check for setting DMA in sample handlers
    f->set_dma_in_sample_handlers = true;

    for (; index < search_length; index++) {
        if (SB(index) == 0x4e && SB(index + 1) == 0x75) {
            break;
        }
    }

    if (index >= search_length) {
        return false;
    }

    if (SB(index - 2) == 0x00 && SB(index - 1) == 0x96) {
        f->set_dma_in_sample_handlers = false;
    }

    // Check for master volume fade feature
    if (SB(start_of_handle_effects) == 0x61 && SB(start_of_handle_effects + 1) == 0x00) {
        index = (((int8_t)SB(start_of_handle_effects + 2) * 256) | SB(start_of_handle_effects + 3))
                + start_of_handle_effects + 2;

        if (index >= (search_length - 24)) {
            return false;
        }

        f->master_volume_fade_version = -1;

        if (SB(index) == 0x30 && SB(index + 1) == 0x3a && SB(index + 4) == 0x67 && SB(index + 5) == 0x00
            && SB(index + 8) == 0x41 && SB(index + 9) == 0xfa && SB(index + 18) == 0x30 && SB(index + 19) == 0x80) {
            f->master_volume_fade_version = 1;
        } else if (SB(index) == 0x30 && SB(index + 1) == 0x39 && SB(index + 6) == 0x67 && SB(index + 7) == 0x00
                   && SB(index + 10) == 0x41 && SB(index + 11) == 0xf9 && SB(index + 22) == 0x30
                   && SB(index + 23) == 0x80) {
            f->master_volume_fade_version = 1;
        } else if (SB(index) == 0x10 && SB(index + 1) == 0x3a && SB(index + 4) == 0x67 && SB(index + 5) == 0x00
                   && SB(index + 8) == 0x41 && SB(index + 9) == 0xfa && SB(index + 18) == 0x53
                   && SB(index + 19) == 0x00) {
            f->master_volume_fade_version = 2;
        } else {
            return false;
        }
    }

    return true;
}

static bool bd_find_features_in_parse_track_effect(const BdScan* buf, size_t buf_len, int start_of_method,
                                                   BdFeatures* f) {
    int search_length = (int)buf_len;
    int index;

    if ((SB(start_of_method + 2) != 0xb0 || SB(start_of_method + 3) != 0x3c)
        && (SB(start_of_method + 2) != 0x0c || SB(start_of_method + 3) != 0x00)) {
        return false;
    }

    f->max_sample_mapping_value = SB(start_of_method + 5);
    index = start_of_method + 8;

    if (index >= (search_length - 4)) {
        return false;
    }

    if (SB(index) != 0x02 || SB(index + 1) != 0x40 || SB(index + 2) != 0x00) {
        return false;
    }

    if (SB(index + 3) == 0x07) {
        f->get_sample_mapping_version = 1;
    } else if (SB(index + 3) == 0xff) {
        f->get_sample_mapping_version = 2;
    } else {
        return true;
    }

    for (index += 4; index < (search_length - 6); index += 2) {
        if (((SB(index) == 0xb0 && SB(index + 1) == 0x3c) || (SB(index) == 0x0c && SB(index + 1) == 0x00))
            && SB(index + 4) == 0x6c) {
            break;
        }
    }

    if (index >= (search_length - 6)) {
        return false;
    }

    f->uses_9x_track_effects = (SB(index + 3) & 0xf0) == 0x90;
    f->uses_cx_track_effects = (SB(index + 3) & 0xf0) == 0xc0;

    return true;
}

static bool bd_find_features_in_parse_track(const BdScan* buf, size_t buf_len, int start_of_play, BdFeatures* f) {
    int search_length = (int)buf_len;
    int index;

    for (index = start_of_play; index < (search_length - 4); index += 2) {
        if (SB(index) == 0x60 && SB(index + 1) == 0x00) {
            break;
        }
    }

    if (index >= (search_length - 4)) {
        return false;
    }

    index = (((int8_t)SB(index + 2) * 256) | SB(index + 3)) + index + 2;
    if (index >= search_length) {
        return false;
    }

    int start_of_parse_track = index;

    for (; index < (search_length - 8); index += 2) {
        if (SB(index) == 0x4a && SB(index + 1) == 0x2c && SB(index + 4) == 0x67) {
            break;
        }
    }

    if (index >= (search_length - 8)) {
        return false;
    }

    f->check_for_ticks = SB(index + 6) == 0x4a && SB(index + 7) == 0x2c;

    for (index += 8; index < (search_length - 6); index += 2) {
        if (SB(index) == 0x72 && SB(index + 1) == 0x00 && SB(index + 2) == 0x12 && SB(index + 3) == 0x1b) {
            break;
        }
    }

    if (index >= (search_length - 6)) {
        return false;
    }

    f->extra_tick_arg = SB(index + 4) == 0x66;

    for (index = start_of_parse_track; index < (search_length - 4); index += 2) {
        if (SB(index) == 0x61 && SB(index + 1) == 0x00) {
            break;
        }
    }

    if (index >= (search_length - 4)) {
        return false;
    }

    index = (((int8_t)SB(index + 2) * 256) | SB(index + 3)) + index + 2;
    if (index >= search_length) {
        return false;
    }

    return bd_find_features_in_parse_track_effect(buf, buf_len, index, f);
}

static bool bd_find_features(const BdScan* buf, size_t buf_len, int start_of_play, BdFeatures* f) {
    memset(f, 0, sizeof(*f));
    f->master_volume_fade_version = -1;

    if (!bd_find_features_in_play(buf, buf_len, start_of_play, f)) {
        return false;
    }
    if (!bd_find_features_in_handle_effects(buf, buf_len, start_of_play, f)) {
        return false;
    }
    if (!bd_find_features_in_parse_track(buf, buf_len, start_of_play, f)) {
        return false;
    }

    return true;
}

static bool bd_test_module(const uint8_t* data, size_t size, int* out_sub_song_list_offset,
                           int* out_track_offset_table_offset, int* out_tracks_offset,
                           int* out_sample_info_offset_table_offset, BdFeatures* features) {
    if (size < 0x1600) {
        return false;
    }

    size_t buf_len = size < 0x3000 ? size : 0x3000;
    const BdScan scan = { data, size };
    const BdScan* buf = &scan;

    // Check for BRA instructions at required offsets
    if (SB(0) != 0x60 || SB(1) != 0x00 || SB(4) != 0x60 || SB(5) != 0x00 || SB(10) != 0x60 || SB(11) != 0x00) {
        return false;
    }

    // Find the init function
    int start_of_init = (((int8_t)SB(2) * 256) | SB(3)) + 2;
    if (start_of_init < 0 || start_of_init >= (int)(buf_len - 14)) {
        return false;
    }

    if (SB(start_of_init) != 0x3f || SB(start_of_init + 1) != 0x00 || SB(start_of_init + 2) != 0x61
        || SB(start_of_init + 3) != 0x00 || SB(start_of_init + 6) != 0x3d || SB(start_of_init + 7) != 0x7c
        || SB(start_of_init + 12) != 0x41 || SB(start_of_init + 13) != 0xfa) {
        return false;
    }

    // Find the play function
    int start_of_play = (((int8_t)SB(6) * 256) | SB(7)) + 4 + 2;
    if (start_of_play < 0 || start_of_play >= (int)buf_len) {
        return false;
    }

    if (!bd_extract_info_from_init(buf, buf_len, start_of_init, out_sub_song_list_offset,
                                   out_sample_info_offset_table_offset)) {
        return false;
    }

    if (!bd_extract_info_from_play(buf, buf_len, start_of_play, out_track_offset_table_offset, out_tracks_offset)) {
        return false;
    }

    if (!bd_find_features(buf, buf_len, start_of_play, features)) {
        return false;
    }

    return true;
}

bool bd_probe(const uint8_t* data, size_t size) {
    int offsets[4];
    BdFeatures features;
    return data != nullptr && bd_test_module(data, size, &offsets[0], &offsets[1], &offsets[2], &offsets[3], &features);
}

// Position list command argument count

static int bd_position_command_arg_count(uint8_t cmd, const BdFeatures* f) {
    if (cmd < f->max_track_value) {
        return 0;
    }

    if (f->enable_c0_track_loop) {
        if (cmd < 0xa0) {
            return 0;
        }
        if (cmd < 0xc8) {
            return 1;
        }
    }

    if (f->enable_f0_track_loop) {
        if (cmd < 0xf0) {
            return 0;
        }
        if (cmd < 0xf8) {
            return 1;
        }
    }

    if (cmd == 0xfd && f->master_volume_fade_version > 0) {
        return 1;
    }

    if (cmd == 0xfe) {
        return 1;
    }

    return -1;
}

// Track command argument count

static int bd_track_command_arg_count(uint8_t cmd, uint8_t next_byte, const BdFeatures* f) {
    if (cmd < 0x7f) {
        if (f->extra_tick_arg && next_byte == 0) {
            return 2;
        }
        return 1;
    }

    if (cmd == 0x7f) {
        return 1;
    }

    if (cmd <= f->max_sample_mapping_value) {
        return 0;
    }

    if ((f->uses_cx_track_effects && cmd < 0xc0) || (f->uses_9x_track_effects && cmd < 0x9b)) {
        return 0;
    }

    // Portamento enable (3 args)
    if ((cmd == 0xc0 && f->uses_cx_track_effects && f->enable_portamento)
        || (cmd == 0x9b && f->uses_9x_track_effects && f->enable_portamento)) {
        return 3;
    }

    // Portamento disable (0 args)
    if ((cmd == 0xc1 && f->uses_cx_track_effects && f->enable_portamento)
        || (cmd == 0x9c && f->uses_9x_track_effects && f->enable_portamento)) {
        return 0;
    }

    // Volume fade enable (3 args)
    if ((cmd == 0xc2 && f->uses_cx_track_effects && f->enable_volume_fade)
        || (cmd == 0x9d && f->uses_9x_track_effects && f->enable_volume_fade)) {
        return 3;
    }

    // Volume fade disable (0 args)
    if ((cmd == 0xc3 && f->uses_cx_track_effects && f->enable_volume_fade)
        || (cmd == 0x9e && f->uses_9x_track_effects && f->enable_volume_fade)) {
        return 0;
    }

    // Portamento2 enable (1 arg)
    if ((cmd == 0xc4 && f->uses_cx_track_effects && f->enable_portamento)
        || (cmd == 0x9f && f->uses_9x_track_effects && f->enable_portamento)) {
        return 1;
    }

    // Portamento2 disable (0 args)
    if ((cmd == 0xc5 && f->uses_cx_track_effects && f->enable_portamento)
        || (cmd == 0xa0 && f->uses_9x_track_effects && f->enable_portamento)) {
        return 0;
    }

    // Channel volume (1 or 3 args)
    if ((cmd == 0xc6 && f->uses_cx_track_effects && f->enable_volume_fade)
        || (cmd == 0xa1 && f->uses_9x_track_effects && f->enable_volume_fade)) {
        return f->enable_final_volume_slide ? 3 : 1;
    }

    // Final volume slide disable (0 args)
    if ((cmd == 0xc7 && f->uses_cx_track_effects && f->enable_final_volume_slide)
        || (cmd == 0xa2 && f->uses_9x_track_effects && f->enable_final_volume_slide)) {
        return 0;
    }

    return -1;
}

// Module loading

static bool bd_load_sub_song_info(BdModule* mod) {
    const uint8_t* data = mod->file_data;
    size_t size = mod->file_size;
    int offset = mod->sub_song_list_offset;

    if (offset < 0 || offset >= (int)size) {
        return false;
    }

    // Count sub-songs: each entry is 8 bytes (4 x uint16), terminated when position reaches first position list
    int first_position_list = 0x7fffffff;
    size_t capacity = 16;
    mod->sub_songs = (BdSongInfo*)calloc(capacity, sizeof(BdSongInfo));
    if (mod->sub_songs == nullptr) {
        return false;
    }
    mod->sub_song_count = 0;

    int pos = offset;

    do {
        if (pos + 8 > (int)size) {
            return false;
        }

        if (mod->sub_song_count >= BD_MAX_TABLE_ENTRIES) {
            break; // a sub-song is picked by a byte, the rest is unreachable
        }

        if (mod->sub_song_count >= capacity) {
            capacity *= 2;
            BdSongInfo* new_songs = (BdSongInfo*)realloc(mod->sub_songs, capacity * sizeof(BdSongInfo));
            if (new_songs == nullptr) {
                return false;
            }
            mod->sub_songs = new_songs;
        }

        BdSongInfo* song = &mod->sub_songs[mod->sub_song_count];
        for (int i = 0; i < 4; i++) {
            song->position_lists[i] = read_be16(data + pos);
            pos += 2;

            if ((int)song->position_lists[i] < first_position_list) {
                first_position_list = (int)song->position_lists[i];
            }
        }

        mod->sub_song_count++;
    } while (pos < offset + first_position_list);

    return true;
}

static uint8_t* bd_load_single_position_list(const uint8_t* data, size_t size, int offset, const BdFeatures* f,
                                             size_t* out_len) {
    if (offset < 0 || offset >= (int)size) {
        return nullptr;
    }

    size_t capacity = 64;
    uint8_t* list = (uint8_t*)malloc(capacity);
    if (list == nullptr) {
        return nullptr;
    }
    size_t len = 0;
    int pos = offset;

    for (;;) {
        if (pos >= (int)size) {
            free(list);
            return nullptr;
        }

        uint8_t cmd = data[pos++];

        if (len >= capacity) {
            capacity *= 2;
            uint8_t* new_list = (uint8_t*)realloc(list, capacity);
            if (new_list == nullptr) {
                free(list);
                return nullptr;
            }
            list = new_list;
        }

        list[len++] = cmd;

        if (cmd == 0xff) {
            break;
        }

        int arg_count = bd_position_command_arg_count(cmd, f);
        if (arg_count == -1) {
            free(list);
            return nullptr;
        }

        for (int i = 0; i < arg_count; i++) {
            if (pos >= (int)size) {
                free(list);
                return nullptr;
            }

            if (len >= capacity) {
                capacity *= 2;
                uint8_t* new_list = (uint8_t*)realloc(list, capacity);
                if (new_list == nullptr) {
                    free(list);
                    return nullptr;
                }
                list = new_list;
            }

            list[len++] = data[pos++];
        }
    }

    *out_len = len;
    return list;
}

static bool bd_load_position_lists(BdModule* mod) {
    // Collect unique position list offsets
    size_t capacity = 16;
    mod->position_lists = (BdPositionList*)calloc(capacity, sizeof(BdPositionList));
    if (mod->position_lists == nullptr) {
        return false;
    }
    mod->position_list_count = 0;

    for (size_t s = 0; s < mod->sub_song_count; s++) {
        for (int ch = 0; ch < 4; ch++) {
            uint16_t pl_offset = mod->sub_songs[s].position_lists[ch];

            // Check if already loaded
            bool found = false;
            for (size_t j = 0; j < mod->position_list_count; j++) {
                if (mod->position_lists[j].offset_key == pl_offset) {
                    found = true;
                    break;
                }
            }

            if (found) {
                continue;
            }

            if (mod->position_list_count >= capacity) {
                capacity *= 2;
                BdPositionList* new_pls
                    = (BdPositionList*)realloc(mod->position_lists, capacity * sizeof(BdPositionList));
                if (new_pls == nullptr) {
                    return false;
                }
                mod->position_lists = new_pls;
            }

            size_t pl_len = 0;
            uint8_t* pl_data = bd_load_single_position_list(
                mod->file_data, mod->file_size, mod->sub_song_list_offset + pl_offset, &mod->features, &pl_len);

            if (pl_data == nullptr) {
                return false;
            }

            mod->position_lists[mod->position_list_count].offset_key = pl_offset;
            mod->position_lists[mod->position_list_count].data = pl_data;
            mod->position_lists[mod->position_list_count].length = pl_len;
            mod->position_list_count++;
        }
    }

    return true;
}

static uint8_t* bd_load_single_track(const uint8_t* data, size_t size, int offset, const BdFeatures* f,
                                     size_t* out_len) {
    if (offset < 0 || offset >= (int)size) {
        return nullptr;
    }

    size_t capacity = 128;
    uint8_t* track = (uint8_t*)malloc(capacity);
    if (track == nullptr) {
        return nullptr;
    }
    size_t len = 0;
    int pos = offset;

    for (;;) {
        if (pos >= (int)size) {
            free(track);
            return nullptr;
        }

        uint8_t cmd = data[pos++];

        if (len >= capacity) {
            capacity *= 2;
            uint8_t* new_track = (uint8_t*)realloc(track, capacity);
            if (new_track == nullptr) {
                free(track);
                return nullptr;
            }
            track = new_track;
        }

        track[len++] = cmd;

        if (cmd == 0xff) {
            break;
        }

        if (pos >= (int)size) {
            free(track);
            return nullptr;
        }

        uint8_t next_byte = data[pos];

        int arg_count = bd_track_command_arg_count(cmd, next_byte, f);
        if (arg_count == -1) {
            free(track);
            return nullptr;
        }

        if (arg_count > 0) {
            // next_byte is consumed as first arg
            if (len >= capacity) {
                capacity *= 2;
                uint8_t* new_track = (uint8_t*)realloc(track, capacity);
                if (new_track == nullptr) {
                    free(track);
                    return nullptr;
                }
                track = new_track;
            }

            track[len++] = next_byte;
            pos++;

            for (int i = 1; i < arg_count; i++) {
                if (pos >= (int)size) {
                    free(track);
                    return nullptr;
                }

                if (len >= capacity) {
                    capacity *= 2;
                    uint8_t* new_track = (uint8_t*)realloc(track, capacity);
                    if (new_track == nullptr) {
                        free(track);
                        return nullptr;
                    }
                    track = new_track;
                }

                track[len++] = data[pos++];
            }
        }
        // If arg_count == 0, next_byte was not consumed (seek back in C# - we just don't advance pos)
    }

    *out_len = len;
    return track;
}

static bool bd_load_tracks(BdModule* mod) {
    int number_of_tracks = (mod->sub_song_list_offset - mod->track_offset_table_offset) / 2;
    if (number_of_tracks <= 0) {
        return false;
    }
    if (number_of_tracks > BD_MAX_TABLE_ENTRIES) {
        number_of_tracks = BD_MAX_TABLE_ENTRIES; // a track is picked by a byte, the rest is unreachable
    }

    mod->track_count = (size_t)number_of_tracks;
    mod->tracks = (uint8_t**)calloc(mod->track_count, sizeof(uint8_t*));
    mod->track_lengths = (size_t*)calloc(mod->track_count, sizeof(size_t));

    if (mod->tracks == nullptr || mod->track_lengths == nullptr) {
        return false;
    }

    // Read track offset table
    int table_pos = mod->track_offset_table_offset;
    if (table_pos < 0 || table_pos + number_of_tracks * 2 > (int)mod->file_size) {
        return false;
    }

    for (int i = 0; i < number_of_tracks; i++) {
        uint16_t track_offset = read_be16(mod->file_data + table_pos + i * 2);
        int abs_offset = mod->tracks_offset + track_offset;

        size_t track_len = 0;
        uint8_t* track_data
            = bd_load_single_track(mod->file_data, mod->file_size, abs_offset, &mod->features, &track_len);

        if (track_data == nullptr) {
            return false;
        }

        mod->tracks[i] = track_data;
        mod->track_lengths[i] = track_len;
    }

    return true;
}

static bool bd_load_sample_info(BdModule* mod) {
    const uint8_t* data = mod->file_data;
    size_t size = mod->file_size;
    int table_offset = mod->sample_info_offset_table_offset;

    if (table_offset < 0 || table_offset >= (int)size) {
        return false;
    }

    // Read offset table
    size_t capacity = 16;
    uint32_t* offset_table = (uint32_t*)calloc(capacity, sizeof(uint32_t));
    if (offset_table == nullptr) {
        return false;
    }
    size_t offset_count = 0;
    uint32_t first_sample_info = 0xffffffff;

    int pos = table_offset;
    do {
        if (pos + 4 > (int)size) {
            free(offset_table);
            return false;
        }

        if (offset_count >= BD_MAX_TABLE_ENTRIES) {
            break; // a sample is picked by a byte, the rest is unreachable
        }

        if (offset_count >= capacity) {
            capacity *= 2;
            uint32_t* new_table = (uint32_t*)realloc(offset_table, capacity * sizeof(uint32_t));
            if (new_table == nullptr) {
                free(offset_table);
                return false;
            }
            offset_table = new_table;
        }

        uint32_t offset = read_be32(data + pos);
        pos += 4;

        if (offset < first_sample_info) {
            first_sample_info = offset;
        }

        offset_table[offset_count++] = offset;
    } while ((uint64_t)pos < (uint64_t)table_offset + first_sample_info);

    // Now read sample info and sample data
    mod->sample_count = offset_count;
    mod->samples = (BdSample*)calloc(mod->sample_count, sizeof(BdSample));

    if (mod->samples == nullptr) {
        free(offset_table);
        return false;
    }

    for (size_t i = 0; i < mod->sample_count; i++) {
        // The header is 28 bytes
        const uint64_t info_start = (uint64_t)table_offset + offset_table[i];
        if (info_start + 28 > size) {
            free(offset_table);
            return false;
        }

        BdSample* sample = &mod->samples[i];
        size_t info_pos = (size_t)info_start;

        uint32_t sample_data_offset = read_be32(data + info_pos);
        info_pos += 4;

        sample->loop_offset = read_be32(data + info_pos);
        const uint32_t raw_loop_offset = sample->loop_offset;
        info_pos += 4;
        if (sample->loop_offset > 0) {
            sample->loop_offset -= sample_data_offset;
        }

        sample->length = read_be16(data + info_pos);
        info_pos += 2;
        sample->loop_length = read_be16(data + info_pos);
        info_pos += 2;

        sample->volume = read_be16(data + info_pos);
        info_pos += 2;
        sample->volume_fade_speed = read_be16s(data + info_pos);
        info_pos += 2;

        sample->portamento_duration = read_be16s(data + info_pos);
        info_pos += 2;
        sample->portamento_add_value = read_be16s(data + info_pos);
        info_pos += 2;

        sample->vibrato_depth = read_be16(data + info_pos);
        info_pos += 2;
        sample->vibrato_add_value = read_be16(data + info_pos);
        info_pos += 2;

        sample->note_transpose = read_be16s(data + info_pos);
        info_pos += 2;
        sample->fine_tune_period = read_be16(data + info_pos);
        info_pos += 2;

        // A zero loop length makes Paula wrap over the module from the stored loop address.
        const size_t wrap_start = (size_t)table_offset + raw_loop_offset;
        const size_t wrap_avail = wrap_start < size ? size - wrap_start : 0;
        sample->wrap_data = (const int8_t*)(data + (wrap_start < size ? wrap_start : size));
        sample->wrap_length = (uint32_t)(wrap_avail < 0x20000u ? wrap_avail : 0x20000u);

        // Load sample data
        const uint64_t abs_sample_offset = (uint64_t)table_offset + sample_data_offset;
        int sample_end1 = sample->length * 2;
        int sample_end2 = (int)(sample->loop_offset + sample->loop_length * 2);
        int sample_len = sample_end1 > sample_end2 ? sample_end1 : sample_end2;

        // A loop that does not lie inside the sample data is not playable.
        if ((uint64_t)sample->loop_offset + (uint64_t)sample->loop_length * 2u
            > (uint64_t)(sample_len > 0 ? sample_len : 0)) {
            sample->loop_length = 0;
        }

        if (sample_len <= 0) {
            sample->sample_data = nullptr;
            continue;
        }

        if (abs_sample_offset + (uint64_t)sample_len > size) {
            free(offset_table);
            return false;
        }

        sample->sample_data = (int8_t*)malloc((size_t)sample_len);
        if (sample->sample_data == nullptr) {
            free(offset_table);
            return false;
        }

        memcpy(sample->sample_data, data + (size_t)abs_sample_offset, (size_t)sample_len);
    }

    free(offset_table);
    return true;
}

// Position list lookup

static uint8_t* bd_find_position_list(BdModule* mod, uint16_t offset_key, size_t* out_len) {
    for (size_t i = 0; i < mod->position_list_count; i++) {
        if (mod->position_lists[i].offset_key == offset_key) {
            *out_len = mod->position_lists[i].length;
            return mod->position_lists[i].data;
        }
    }

    *out_len = 0;
    return nullptr;
}

// Player state initialization

static void bd_init_sound(BdModule* mod, int sub_song) {
    BdPlayer* p = mod->player;

    if (sub_song < 0 || sub_song >= (int)mod->sub_song_count) {
        return;
    }

    BdSongInfo* song = &mod->sub_songs[sub_song];

    p->playing_info.enable_playing = true;
    p->playing_info.master_volume = 64;
    p->playing_info.master_volume_fade_speed = 0;
    p->playing_info.master_volume_fade_speed_counter = 1;
    p->playing_info.counter = 6;

    for (int i = 0; i < 4; i++) {
        BdVoiceInfo* v = &p->voices[i];
        memset(v, 0, sizeof(*v));

        size_t pl_len = 0;
        v->position_list = bd_find_position_list(mod, song->position_lists[i], &pl_len);
        v->position_list_length = pl_len;
        v->channel_enabled = true;
        v->switch_to_next_position = true;
        v->track_loop_counter = 1;
        v->ticks_left_for_next_track_command = 1;
        v->use_new_note = true;
        v->channel_volume = 0xffff;

        // Initialize sample mapping
        for (uint8_t j = 0; j < 10; j++) {
            v->sample_mapping[j] = j;
        }

        BdVoicePlaybackInfo* pb = &p->playback[i];
        memset(pb, 0, sizeof(*pb));
        pb->handler = BdSampleHandler_None;
        pb->pending_note = pb->pending_instrument = pb->started_note = pb->started_instrument = -1;
        pb->mixer = p->mixer;
        pb->voice = (uint32_t)i;
        nuade_voice_stop(pb->mixer, pb->voice);
    }
}

// Amiga hardware simulation

static NuadeSample bd_looping_sample(const int8_t* data, uint32_t length) {
    // Paula always loops the buffer it was given
    return (NuadeSample) { .data = data, .format = NuadeSampleFormat_S8, .length = length, .loop_length = length };
}

// Rewriting the loop registers takes effect when the current buffer ends. A voice that is not running ignores it:
// enabling DMA reloads the registers.
static void bd_set_loop(BdVoicePlaybackInfo* pb, uint32_t offset, uint32_t length) {
    if (pb->dma_enabled && pb->active) {
        const NuadeSample loop = bd_looping_sample(pb->sample_data + offset, length);
        nuade_voice_queue(pb->mixer, pb->voice, &loop);
    }
}

static void bd_mute_channel(BdVoicePlaybackInfo* pb) {
    pb->active = false;
    nuade_voice_stop(pb->mixer, pb->voice);
}

static void bd_enable_dma(BdVoicePlaybackInfo* pb) {
    if (!pb->dma_enabled) {
        if (pb->pending_note >= 0) {
            pb->started_note = pb->pending_note;
            pb->started_instrument = pb->pending_instrument;
            pb->pending_note = pb->pending_instrument = -1;
        }
        const NuadeSample sample = bd_looping_sample(pb->sample_data, pb->sample_length * 2u);
        nuade_voice_play(pb->mixer, pb->voice, &sample, 0);
        pb->active = true;
        pb->dma_enabled = true;
    }
}

// Sample handler callbacks

static void bd_handle_sample_loop(BdVoicePlaybackInfo* pb, BdSample* sample, bool set_dma_in_handlers) {
    if (pb->loop_delay_counter < 0x8000) {
        if (set_dma_in_handlers) {
            bd_enable_dma(pb);
        }

        pb->loop_delay_counter--;

        if (pb->loop_delay_counter == 0) {
            pb->loop_delay_counter = (uint16_t)~pb->loop_delay_counter;

            uint32_t loop_length = sample->loop_length * 2u;
            if (loop_length > 0) {
                bd_set_loop(pb, sample->loop_offset, loop_length);
            } else if (pb->dma_enabled && pb->active && sample->wrap_length > 0) {
                const NuadeSample wrap = bd_looping_sample(sample->wrap_data, sample->wrap_length);
                nuade_voice_queue(pb->mixer, pb->voice, &wrap);
            }
        }
    } else {
        if (pb->sample_play_ticks_counter <= 1) {
            pb->handler = BdSampleHandler_VolumeFade;
        }
    }
}

static void bd_handle_sample_volume_fade(BdVoicePlaybackInfo* pb, BdSample* sample) {
    pb->loop_delay_counter = 0x8000;
    pb->final_volume = (int16_t)(pb->final_volume + sample->volume_fade_speed);
}

static void bd_handle_sample_play_once(BdVoicePlaybackInfo* pb, BdSample* sample, bool set_dma_in_handlers) {
    (void)sample;

    if (pb->loop_delay_counter < 0x8000) {
        pb->loop_delay_counter--;

        if (pb->loop_delay_counter == 0) {
            pb->loop_delay_counter = (uint16_t)~pb->loop_delay_counter;

            // The Audio IRQ points the registers at an empty sample once the current buffer finishes
            if (pb->active) {
                const NuadeSample empty = bd_looping_sample(s_empty_sample, (uint32_t)sizeof(s_empty_sample));
                nuade_voice_queue(pb->mixer, pb->voice, &empty);
            }
        } else {
            if (set_dma_in_handlers) {
                bd_enable_dma(pb);
            }
        }
    } else {
        if (pb->sample_play_ticks_counter == 1) {
            pb->loop_delay_counter = 0x8000;
        }

        if (!pb->active) {
            pb->final_volume = 0;
        }
    }
}

// Effects

static void bd_do_sample_effects(BdVoicePlaybackInfo* pb, BdSample* sample) {
    if (pb->sample_portamento_duration != 0) {
        if (pb->sample_portamento_duration == -1) {
            return;
        }

        pb->sample_period_add_value += pb->sample_portamento_add_value;
        pb->sample_portamento_duration--;

        if (pb->sample_portamento_duration != 0) {
            return;
        }
    }

    pb->sample_period_add_value += pb->sample_vibrato_add_value;
    pb->sample_vibrato_depth--;

    if (pb->sample_vibrato_depth == 0) {
        if (sample->vibrato_depth != 0) {
            pb->sample_vibrato_depth = sample->vibrato_depth;
            pb->sample_vibrato_add_value = (int16_t)-pb->sample_vibrato_add_value;
        }
    }
}

static void bd_do_final_volume_slide(BdVoicePlaybackInfo* pb) {
    if (pb->final_volume_slide_speed != 0) {
        pb->final_volume_slide_speed_counter--;

        if (pb->final_volume_slide_speed_counter == 0) {
            pb->final_volume_slide_speed_counter = pb->final_volume_slide_speed;
            pb->final_volume = (int16_t)(pb->final_volume + pb->final_volume_slide_add_value);
        }
    }
}

static void bd_do_portamento(BdVoiceInfo* v, BdVoicePlaybackInfo* pb) {
    if (v->portamento_control_flag != 0) {
        if (v->portamento_control_flag >= 0x80) {
            int temp = (int16_t)(((int64_t)pb->note_period * (v->portamento_add_value & 0xffff) >> 16)
                                 + ((int64_t)pb->note_period * (v->portamento_add_value >> 16)))
                       - pb->note_period;
            v->portamento_add_value = v->portamento_duration_counter != 0 ? temp / v->portamento_duration_counter : 0;
            v->portamento_control_flag &= 0x7f;
        }

        if (v->portamento_start_delay_counter == 0) {
            if (v->portamento_duration_counter != 0) {
                v->portamento_duration_counter--;
                pb->portamento_add_value = (int16_t)(pb->portamento_add_value + v->portamento_add_value);
            }
        } else {
            v->portamento_start_delay_counter--;
        }
    }
}

static void bd_setup_sample(BdPlayer* p, const BdFeatures* f, BdVoicePlaybackInfo* pb, BdSample* sample,
                            uint8_t transposed_note, uint8_t play_ticks, int16_t volume, uint16_t volume_slide_speed,
                            int16_t volume_slide_add_value) {
    bd_mute_channel(pb);

    pb->dma_enabled = false;
    pb->sample_data = sample->sample_data;
    pb->sample_length = sample->length;

    pb->playing_sample = sample;
    pb->sample_play_ticks_counter = play_ticks;

    int period_index = -(transposed_note & 0x7f) + sample->note_transpose + FINE_TUNE_START_INDEX;
    pb->note_period = (period_index >= 0 && period_index < (int)FINE_TUNE_COUNT)
                          ? bd_calc_period(period_index, sample->fine_tune_period)
                          : 0;

    pb->sample_portamento_duration = sample->portamento_duration;

    if (pb->sample_portamento_duration >= 0) {
        pb->sample_vibrato_depth = (uint16_t)(sample->vibrato_depth / 2);
        if ((sample->vibrato_depth & 1) != 0) {
            pb->sample_vibrato_depth++;
        }

        pb->sample_portamento_add_value = (int16_t)((sample->portamento_add_value * pb->note_period) / 32768);
        pb->sample_vibrato_add_value = (int16_t)(((uint32_t)sample->vibrato_add_value * pb->note_period) / 32768);
    }

    pb->sample_period_add_value = 0;

    pb->handler = (sample->volume_fade_speed == 0) ? BdSampleHandler_PlayOnce : BdSampleHandler_Loop;

    pb->final_volume = volume;

    if (volume > p->playing_info.master_volume) {
        volume = p->playing_info.master_volume;
    }

    if (p->playing_info.master_volume < 0) {
        volume = 0;
    }

    // With a master volume fade the clamped volume is what the fade counts down from
    if (f->master_volume_fade_version > 0) {
        pb->final_volume = volume;
    }

    if (f->enable_final_volume_slide) {
        pb->final_volume_slide_speed = volume_slide_speed;
        pb->final_volume_slide_speed_counter = volume_slide_speed;
        pb->final_volume_slide_add_value = volume_slide_add_value;
    }

    pb->loop_delay_counter = 2;
}

static void bd_do_volume_fade(BdPlayer* p, const BdFeatures* f, BdVoiceInfo* v, BdVoicePlaybackInfo* pb) {
    if (v->volume_fade_running && v->volume_fade_duration_counter != 0 && v->sample_info2 != nullptr) {
        v->volume_fade_speed_counter--;

        if (v->volume_fade_speed_counter == 0) {
            v->volume_fade_duration_counter--;
            v->volume_fade_speed_counter = v->volume_fade_speed;

            int volume = v->volume_fade_value + v->volume_fade_add_value;
            v->volume_fade_value = (int16_t)volume;

            volume += v->sample_info2->volume;
            if (volume < 0) {
                v->volume_fade_duration_counter = 0;
            } else {
                if (volume > 64) {
                    volume = 64;
                }

                bd_setup_sample(p, f, pb, v->sample_info2, v->transposed_note, v->volume_fade_speed, (int16_t)volume, 0,
                                0);
            }
        }
    }
}

static void bd_do_master_volume_fade(BdPlayer* p, const BdFeatures* f) {
    if (p->playing_info.master_volume_fade_speed != 0) {
        p->playing_info.master_volume_fade_speed_counter--;

        if (p->playing_info.master_volume_fade_speed_counter == 0) {
            p->playing_info.master_volume_fade_speed_counter = p->playing_info.master_volume_fade_speed;

            if (f->master_volume_fade_version == 2) {
                p->playing_info.master_volume_fade_speed_counter--;
            }

            p->playing_info.master_volume--;

            if (p->playing_info.master_volume < 0) {
                p->playing_info.enable_playing = false;

                for (int i = 0; i < 4; i++) {
                    bd_mute_channel(&p->playback[i]);
                }
            }
        }
    }
}

// Voice effects

static void bd_handle_voice_effects(BdPlayer* p, BdVoicePlaybackInfo* pb, const BdFeatures* f) {
    if (pb->loop_delay_counter != 0) {
        BdSample* sample = pb->playing_sample;

        if (pb->sample_play_ticks_counter > 0) {
            pb->sample_play_ticks_counter--;
        }

        if (f->enable_sample_effects && sample != nullptr) {
            bd_do_sample_effects(pb, sample);
        }

        if (f->enable_final_volume_slide) {
            bd_do_final_volume_slide(pb);
        }

        // Dispatch sample handler
        if (sample != nullptr) {
            switch (pb->handler) {
                case BdSampleHandler_PlayOnce:
                    bd_handle_sample_play_once(pb, sample, f->set_dma_in_sample_handlers);
                    break;
                case BdSampleHandler_Loop:
                    bd_handle_sample_loop(pb, sample, f->set_dma_in_sample_handlers);
                    break;
                case BdSampleHandler_VolumeFade:
                    bd_handle_sample_volume_fade(pb, sample);
                    break;
                default:
                    break;
            }
        }

        int16_t volume = pb->final_volume;
        if (volume > 0) {
            if (volume > p->playing_info.master_volume) {
                volume = p->playing_info.master_volume;
            }

            if (volume < 0) {
                volume = 0;
            }

            // Calculate period with effects
            uint16_t period = (uint16_t)(pb->note_period + pb->sample_period_add_value + pb->portamento_add_value);

            pb->pitch_hz = nuade_paula_period_hz(period);

            if (!f->set_dma_in_sample_handlers) {
                bd_enable_dma(pb);
            }
        } else {
            bd_mute_channel(pb);
            pb->loop_delay_counter = 0;
        }
    }
}

// Track parsing

// The next byte of the voice's track; past its end it reads as zero.
static uint8_t bd_next_track_byte(const BdVoiceInfo* v, int* position) {
    const int index = (*position)++;
    if (index < 0 || (size_t)index >= v->track_length) {
        return 0;
    }
    return v->track[index];
}

static void bd_parse_track_effect(BdModule* mod, BdVoiceInfo* v, int* position) {
    const BdFeatures* f = &mod->features;
    uint8_t cmd = bd_next_track_byte(v, position);

    if (cmd <= f->max_sample_mapping_value) {
        int index = (f->get_sample_mapping_version == 1) ? (cmd & 0x07) : (cmd - 0x80);
        if (index >= 0 && index < 10 && v->sample_mapping[index] < (int)mod->sample_count) {
            v->sample_info = &mod->samples[v->sample_mapping[index]];
        }
    } else if (cmd == 0xff) {
        v->switch_to_next_position = true;
        (*position)--;
    } else if ((f->uses_cx_track_effects && cmd < 0xc0) || (f->uses_9x_track_effects && cmd < 0x9b)) {
        // Set control flag - not used
    } else if ((cmd == 0xc0 && f->uses_cx_track_effects && f->enable_portamento)
               || (cmd == 0x9b && f->uses_9x_track_effects && f->enable_portamento)) {
        v->portamento1_enabled = 255;
        v->portamento2_enabled = false;
        v->portamento_start_delay = bd_next_track_byte(v, position);
        v->portamento_duration = bd_next_track_byte(v, position);
        v->portamento_delta_note_number = (int8_t)bd_next_track_byte(v, position);
    } else if ((cmd == 0xc1 && f->uses_cx_track_effects && f->enable_portamento)
               || (cmd == 0x9c && f->uses_9x_track_effects && f->enable_portamento)) {
        v->portamento1_enabled = 0;
    } else if ((cmd == 0xc2 && f->uses_cx_track_effects && f->enable_volume_fade)
               || (cmd == 0x9d && f->uses_9x_track_effects && f->enable_volume_fade)) {
        v->volume_fade_enabled = true;
        v->volume_fade_init_speed = bd_next_track_byte(v, position);
        v->volume_fade_duration = bd_next_track_byte(v, position);
        v->volume_fade_init_add_value = (int8_t)bd_next_track_byte(v, position);
    } else if ((cmd == 0xc3 && f->uses_cx_track_effects && f->enable_volume_fade)
               || (cmd == 0x9e && f->uses_9x_track_effects && f->enable_volume_fade)) {
        v->volume_fade_enabled = false;
    } else if ((cmd == 0xc4 && f->uses_cx_track_effects && f->enable_portamento)
               || (cmd == 0x9f && f->uses_9x_track_effects && f->enable_portamento)) {
        v->portamento2_enabled = true;
        v->portamento1_enabled = 0;
        v->portamento_duration = bd_next_track_byte(v, position);
    } else if ((cmd == 0xc5 && f->uses_cx_track_effects && f->enable_portamento)
               || (cmd == 0xa0 && f->uses_9x_track_effects && f->enable_portamento)) {
        v->portamento2_enabled = false;
    } else if ((cmd == 0xc6 && f->uses_cx_track_effects && f->enable_volume_fade)
               || (cmd == 0xa1 && f->uses_9x_track_effects && f->enable_volume_fade)) {
        v->channel_volume = (uint16_t)((bd_next_track_byte(v, position) << 8) | 0xff);

        if (f->enable_final_volume_slide) {
            v->channel_volume_slide_speed = bd_next_track_byte(v, position);
            v->channel_volume_slide_add_value = (int8_t)bd_next_track_byte(v, position);
        }
    } else if ((cmd == 0xc7 && f->uses_cx_track_effects && f->enable_final_volume_slide)
               || (cmd == 0xa2 && f->uses_9x_track_effects && f->enable_final_volume_slide)) {
        v->channel_volume_slide_speed = 0;
        v->channel_volume = 0xffff;
    }
}

static bool bd_take_next_position(BdModule* mod, BdVoiceInfo* v, BdVoicePlaybackInfo* pb) {
    const BdFeatures* f = &mod->features;

    v->current_position = v->next_position;

    int position = v->next_position;
    uint8_t cmd;

    for (;;) {
        if (position < 0 || position >= (int)v->position_list_length) {
            v->channel_enabled = false;
            return true;
        }

        cmd = v->position_list[position++];
        if (cmd < f->max_track_value) {
            break;
        }

        if (cmd == 0xfe) {
            if (position >= (int)v->position_list_length) {
                v->channel_enabled = false;
                return true;
            }
            v->transpose = (int8_t)v->position_list[position++];
        } else if (cmd == 0xff) {
            if (pb->loop_delay_counter == 0 || pb->loop_delay_counter == 0x8000) {
                v->channel_enabled = false;
            }
            return true;
        } else if (cmd == 0xfd && f->master_volume_fade_version > 0) {
            if (position >= (int)v->position_list_length) {
                v->channel_enabled = false;
                return true;
            }
            mod->player->playing_info.master_volume_fade_speed = (int8_t)v->position_list[position++];
        } else if (f->enable_f0_track_loop && cmd < 0xf0) {
            v->track_loop_counter = (uint8_t)(cmd - 0xc8);
        } else if (f->enable_c0_track_loop && cmd < 0xc0) {
            v->track_loop_counter = (uint8_t)(cmd & 0x1f);
        } else {
            // Sample mapping
            int index;
            if (f->set_sample_mapping_version == 1) {
                index = cmd & 0x07;
            } else {
                index = cmd - 0xf0;
            }

            if (position >= (int)v->position_list_length) {
                v->channel_enabled = false;
                return true;
            }

            if (index >= 0 && index < 10) {
                v->sample_mapping[index] = (uint8_t)(v->position_list[position++] / 4);
            } else {
                position++;
            }
        }
    }

    v->switch_to_next_position = false;
    v->next_position = position;

    v->playing_track = cmd;

    if (cmd < (int)mod->track_count) {
        v->track = mod->tracks[cmd];
        v->track_length = mod->track_lengths[cmd];
    } else {
        v->track = nullptr;
        v->track_length = 0;
    }

    v->next_track_position = 0;

    return false;
}

static bool bd_parse_track(BdModule* mod, BdVoiceInfo* v, BdVoicePlaybackInfo* pb) {
    const BdFeatures* f = &mod->features;
    BdPlayer* p = mod->player;

    if (v->track == nullptr) {
        v->channel_enabled = false;
        return false;
    }

    int position = v->next_track_position;

    v->ticks_left_for_next_track_command--;

    if (v->ticks_left_for_next_track_command != 0) {
        if (position < (int)v->track_length && v->track[position] >= 0x80) {
            bd_parse_track_effect(mod, v, &position);
        }

        v->next_track_position = position;
        return false;
    }

    for (;;) {
        if (position >= (int)v->track_length) {
            v->switch_to_next_position = true;
            break;
        }

        if (v->track[position] < 0x80) {
            break;
        }

        bd_parse_track_effect(mod, v, &position);

        if (v->switch_to_next_position) {
            if (f->check_for_ticks && v->ticks_left_for_next_track_command == 0) {
                v->ticks_left_for_next_track_command = 1;
            }
            return true;
        }
    }

    if (position >= (int)v->track_length) {
        v->switch_to_next_position = true;
        v->ticks_left_for_next_track_command = 1;
        v->next_track_position = position;
        return true;
    }

    uint8_t note = v->track[position++];

    if (note == 0x7f) {
        if (position >= (int)v->track_length) {
            v->switch_to_next_position = true;
            v->ticks_left_for_next_track_command = 1;
            v->next_track_position = position;
            return true;
        }

        v->ticks_left_for_next_track_command = v->track[position++];
        v->next_track_position = position;
        return false;
    }

    pb->portamento_add_value = 0;

    note = (uint8_t)(note + v->transpose);
    v->transposed_note = note;

    if (f->enable_portamento) {
        v->portamento_control_flag = v->portamento1_enabled;
        if (v->portamento_control_flag != 0) {
            v->portamento_start_delay_counter = v->portamento_start_delay;
            v->portamento_duration_counter = v->portamento_duration;
            int ft_index = FINE_TUNE_START_INDEX - v->portamento_delta_note_number;
            if (ft_index >= 0 && ft_index < (int)FINE_TUNE_COUNT) {
                v->portamento_add_value = s_fine_tune[ft_index];
            }
        }
    }

    if (f->enable_volume_fade) {
        v->volume_fade_running = v->volume_fade_enabled;
        if (v->volume_fade_running) {
            v->volume_fade_speed = v->volume_fade_init_speed;
            v->volume_fade_speed_counter = v->volume_fade_init_speed;
            v->volume_fade_duration_counter = v->volume_fade_duration;
            v->volume_fade_add_value = v->volume_fade_init_add_value;
            v->volume_fade_value = 0;
        }
    }

    if (position >= (int)v->track_length) {
        v->switch_to_next_position = true;
        v->ticks_left_for_next_track_command = 1;
        v->next_track_position = position;
        return true;
    }

    uint8_t ticks = v->track[position++];
    if (f->extra_tick_arg && ticks == 0) {
        if (position >= (int)v->track_length) {
            v->switch_to_next_position = true;
            v->ticks_left_for_next_track_command = 1;
            v->next_track_position = position;
            return true;
        }
        v->ticks_left_for_next_track_command = v->track[position++];
        ticks = 0xff;
    } else {
        v->ticks_left_for_next_track_command = ticks;
    }

    if (f->enable_portamento && v->portamento2_enabled) {
        v->portamento_control_flag = 0xff;
        v->portamento_start_delay_counter = 0;
        v->portamento_duration_counter = v->portamento_duration;

        uint8_t note1 = note;
        if (!v->use_new_note) {
            note = v->previous_transposed_note;
        }

        v->transposed_note = note;
        int ft_index = FINE_TUNE_START_INDEX - (note1 - note);
        if (ft_index >= 0 && ft_index < (int)FINE_TUNE_COUNT) {
            v->portamento_add_value = s_fine_tune[ft_index];
        }
    }

    v->previous_transposed_note = v->transposed_note;
    v->next_track_position = position;
    v->use_new_note = false;

    BdSample* sample = v->sample_info;
    v->sample_info2 = sample;

    if (sample == nullptr) {
        return false;
    }

    int16_t volume = f->enable_volume_fade ? (int16_t)(((uint32_t)sample->volume * v->channel_volume) / 16384)
                                           : (int16_t)sample->volume;

    pb->pending_note = note & 0x7f;
    pb->pending_instrument = (int32_t)(sample - mod->samples);
    bd_setup_sample(p, f, pb, sample, note, ticks, volume, v->channel_volume_slide_speed,
                    v->channel_volume_slide_add_value);

    return false;
}

static void bd_handle_voice(BdModule* mod, BdVoiceInfo* v, BdVoicePlaybackInfo* pb) {
    const BdFeatures* f = &mod->features;
    BdPlayer* p = mod->player;

    if (!v->channel_enabled) {
        return;
    }

    if (f->enable_portamento) {
        bd_do_portamento(v, pb);
    }

    if (f->enable_volume_fade) {
        bd_do_volume_fade(p, f, v, pb);
    }

    for (;;) {
        if (v->switch_to_next_position) {
            v->track_loop_counter--;

            if (v->track_loop_counter == 0) {
                v->track_loop_counter = 1;

                if (bd_take_next_position(mod, v, pb)) {
                    return;
                }
            } else {
                v->next_track_position = 0;
                v->switch_to_next_position = false;
            }
        }

        if (!bd_parse_track(mod, v, pb)) {
            break;
        }
    }
}

// Player tick

// True when the song ran out and restarted: that is its loop point.
static bool bd_player_tick(BdModule* mod) {
    BdPlayer* p = mod->player;
    const BdFeatures* f = &mod->features;

    // Handle effects first
    if (f->master_volume_fade_version > 0) {
        bd_do_master_volume_fade(p, f);
    }

    for (int i = 0; i < BD_VOICE_COUNT; i++) {
        bd_handle_voice_effects(p, &p->playback[i], f);
    }

    // Counter feature (subdivide ticks)
    if (f->enable_counter) {
        p->playing_info.counter--;
        if (p->playing_info.counter == 0) {
            p->playing_info.counter = 6;
            return false;
        }
    }

    if (!p->playing_info.enable_playing) {
        p->playing_info.enable_playing = true;

        // Restart song (matches C# RestartSong behavior)
        bd_init_sound(mod, p->current_subsong);
        return true;
    }

    p->playing_info.enable_playing = p->voices[0].channel_enabled || p->voices[1].channel_enabled
                                     || p->voices[2].channel_enabled || p->voices[3].channel_enabled;

    if (p->playing_info.enable_playing) {
        for (int i = 0; i < 4; i++) {
            bd_handle_voice(mod, &p->voices[i], &p->playback[i]);
        }
    }
    return false;
}

// Voices

// Hand the pitch and volume registers the tick wrote to the voices.
static void bd_sync_voices(BdPlayer* p) {
    for (uint32_t i = 0; i < BD_VOICE_COUNT; i++) {
        const BdVoicePlaybackInfo* pb = &p->playback[i];

        int16_t volume = pb->final_volume;
        if (volume > p->playing_info.master_volume) {
            volume = p->playing_info.master_volume;
        }
        if (volume < 0) {
            volume = 0;
        }

        nuade_voice_set_pitch(p->mixer, i, pb->pitch_hz);
        nuade_voice_set_volume(p->mixer, i, (float)volume / 64.0f);
    }
}

// Module cleanup

static void bd_module_cleanup(BdModule* mod) {
    if (mod->sub_songs != nullptr) {
        free(mod->sub_songs);
        mod->sub_songs = nullptr;
    }

    if (mod->position_lists != nullptr) {
        for (size_t i = 0; i < mod->position_list_count; i++) {
            free(mod->position_lists[i].data);
        }
        free(mod->position_lists);
        mod->position_lists = nullptr;
    }

    if (mod->tracks != nullptr) {
        for (size_t i = 0; i < mod->track_count; i++) {
            free(mod->tracks[i]);
        }
        free(mod->tracks);
        mod->tracks = nullptr;
    }

    if (mod->track_lengths != nullptr) {
        free(mod->track_lengths);
        mod->track_lengths = nullptr;
    }

    if (mod->samples != nullptr) {
        for (size_t i = 0; i < mod->sample_count; i++) {
            free(mod->samples[i].sample_data);
        }
        free(mod->samples);
        mod->samples = nullptr;
    }

    if (mod->file_data != nullptr) {
        free(mod->file_data);
        mod->file_data = nullptr;
    }

    if (mod->player != nullptr) {
        free(mod->player);
        mod->player = nullptr;
    }
}

// Public API

BdModule* bd_create(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) {
        return nullptr;
    }

    BdModule* mod = (BdModule*)calloc(1, sizeof(BdModule));
    if (mod == nullptr) {
        return nullptr;
    }

    // Test module format and extract offsets + features
    if (!bd_test_module(data, size, &mod->sub_song_list_offset, &mod->track_offset_table_offset, &mod->tracks_offset,
                        &mod->sample_info_offset_table_offset, &mod->features)) {
        free(mod);
        return nullptr;
    }

    // Keep a copy of the file data
    mod->file_data = (uint8_t*)malloc(size);
    if (mod->file_data == nullptr) {
        free(mod);
        return nullptr;
    }
    memcpy(mod->file_data, data, size);
    mod->file_size = size;

    // Load all data structures
    if (!bd_load_sub_song_info(mod)) {
        bd_module_cleanup(mod);
        free(mod);
        return nullptr;
    }

    if (!bd_load_position_lists(mod)) {
        bd_module_cleanup(mod);
        free(mod);
        return nullptr;
    }

    if (!bd_load_tracks(mod)) {
        bd_module_cleanup(mod);
        free(mod);
        return nullptr;
    }

    if (!bd_load_sample_info(mod)) {
        bd_module_cleanup(mod);
        free(mod);
        return nullptr;
    }

    // Create player
    mod->player = (BdPlayer*)calloc(1, sizeof(BdPlayer));
    if (mod->player == nullptr) {
        bd_module_cleanup(mod);
        free(mod);
        return nullptr;
    }

    return mod;
}

void bd_destroy(BdModule* module) {
    if (module == nullptr) {
        return;
    }

    bd_module_cleanup(module);
    free(module);
}

int bd_subsong_count(const BdModule* module) {
    if (module == nullptr) {
        return 0;
    }

    return (int)module->sub_song_count;
}

bool bd_start(BdModule* module, int subsong, NuadeVoices* voices) {
    if (module == nullptr || module->player == nullptr || voices == nullptr) {
        return false;
    }

    if (subsong < 0 || subsong >= (int)module->sub_song_count) {
        return false;
    }

    BdPlayer* p = module->player;
    p->mixer = voices;
    p->current_subsong = subsong;
    bd_init_sound(module, subsong);
    // Amiga panning: Paula channels 0 and 3 are left, 1 and 2 right
    for (uint32_t i = 0; i < BD_VOICE_COUNT; i++) {
        nuade_voice_set_pan(voices, i, (i == 0 || i == 3) ? -1.0f : 1.0f);
    }
    return true;
}

int bd_channel_count(const BdModule* module) {
    (void)module;
    return 4;
}

bool bd_tick(BdModule* module) {
    BdPlayer* p = module->player;
    for (uint32_t i = 0; i < BD_VOICE_COUNT; i++) {
        p->playback[i].active = p->playback[i].active && nuade_voice_is_playing(p->mixer, i);
    }
    const bool looped = bd_player_tick(module);
    bd_sync_voices(p);
    return looped;
}

bool bd_voice_started(const BdModule* module, uint32_t voice, int32_t* note, int32_t* instrument) {
    if (module == nullptr || note == nullptr || instrument == nullptr || voice >= BD_VOICE_COUNT
        || module->player->playback[voice].started_note < 0) {
        return false;
    }
    *note = module->player->playback[voice].started_note;
    *instrument = module->player->playback[voice].started_instrument;
    return true;
}
