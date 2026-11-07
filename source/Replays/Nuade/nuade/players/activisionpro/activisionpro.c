// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/activisionpro.h>

#include "nuade_base.h"
#include "nuade_paula.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AVP_MAX_TABLE_ENTRIES 256 // subsongs, tracks, envelopes and instruments are all numbered by a byte

// Tables

static const uint8_t avp_empty_track[] = { 0xff };

static const uint16_t avp_periods[85]
    = { 1695, 1600, 1505, 1426, 1347, 1268, 1189, 1125, 1062, 1006, 951,  895,  1695, 1600, 1505, 1426, 1347,
        1268, 1189, 1125, 1062, 1006, 951,  895,  1695, 1600, 1505, 1426, 1347, 1268, 1189, 1125, 1062, 1006,
        951,  1790, 1695, 1600, 1505, 1426, 1347, 1268, 1189, 1125, 1062, 1006, 951,  895,  848,  800,  753,
        713,  674,  634,  595,  563,  531,  503,  476,  448,  424,  400,  377,  357,  337,  317,  298,  282,
        266,  252,  238,  224,  212,  200,  189,  179,  169,  159,  149,  141,  133,  126,  119,  112,  106

      };

static const uint8_t avp_vibrato_counters[19] = { 0, 1, 1, 1, 1, 4, 3, 2, 4, 3, 2, 1, 3, 2, 1, 3, 2, 2, 1

};

static const int8_t avp_vibrato_depths1[19] = { 0, 5, 4, 3, 2, 5, 5, 4, 4, 4, 3, 1, 3, 2, 0, 1, 1, 0, 0

};

static const int8_t avp_vibrato_depths2[19] = { 1, 32, 16, 8, 4, 32, 32, 16, 16, 16, 8, 2, 8, 4, 1, 2, 2, 1, 1

};

// Forward declarations

static void avp_initialize_sound(AvpModule* m, int sub_song);
static void avp_play_tick(AvpModule* m);
static void avp_parse_next_position(AvpModule* m, AvpVoiceInfo* vi, bool update_information);

// Reader helpers

typedef struct AvpReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} AvpReader;

static void avp_reader_init(AvpReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool avp_reader_eof(const AvpReader* r) {
    return r->pos > r->size;
}

static void avp_reader_seek(AvpReader* r, size_t pos) {
    r->pos = pos;
}

static uint8_t avp_reader_read_u8(AvpReader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static int8_t avp_reader_read_i8(AvpReader* r) {
    return (int8_t)avp_reader_read_u8(r);
}

static uint16_t avp_reader_read_b_u16(AvpReader* r) {
    uint8_t hi = avp_reader_read_u8(r);
    uint8_t lo = avp_reader_read_u8(r);
    return (uint16_t)((hi << 8) | lo);
}

static int16_t avp_reader_read_b_i16(AvpReader* r) {
    return (int16_t)avp_reader_read_b_u16(r);
}

static uint32_t avp_reader_read_b_u32(AvpReader* r) {
    uint8_t a = avp_reader_read_u8(r);
    uint8_t b = avp_reader_read_u8(r);
    uint8_t c = avp_reader_read_u8(r);
    uint8_t d = avp_reader_read_u8(r);
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d;
}

static size_t avp_reader_read_signed(AvpReader* r, int8_t* buf, size_t count) {
    size_t avail = r->pos < r->size ? r->size - r->pos : 0;
    if (count > avail)
        count = avail;
    if (count > 0) {
        memcpy(buf, r->data + r->pos, count);
        r->pos += count;
    }
    return count;
}

// Channel operations

static void avp_ch_mute(AvpChannel* ch) {
    ch->active = false;
}

static void avp_ch_play_sample(AvpChannel* ch, uint8_t samp_num, int8_t* data, uint32_t data_size,
                               uint32_t start_offset, uint32_t length) {
    (void)samp_num;
    ch->sample_data = data;
    ch->data_size = data_size;
    ch->sample_offset = start_offset;
    ch->sample_length = length;
    ch->active = true;
    ch->play_pending = true;
    ch->loop_length = 0;
    ch->loop_start = 0;
}

static void avp_ch_set_loop(AvpChannel* ch, uint32_t start, uint32_t length) {
    ch->loop_start = start;
    ch->loop_length = length;
}

static void avp_ch_set_amiga_period(AvpChannel* ch, uint32_t period) {
    ch->period = (uint16_t)period;
}

static void avp_ch_set_amiga_volume(AvpChannel* ch, uint16_t vol) {
    ch->volume = vol;
}

// Module identification (extract info from binary player code)

// The identification follows offsets found in the data, so every read is range checked; bytes outside the module
// read as zero.
typedef struct AvpScan {
    const uint8_t* data;
    int length;
} AvpScan;

static uint8_t avp_scan_byte(const AvpScan* scan, int index) {
    if (index < 0 || index >= scan->length)
        return 0;
    return scan->data[index];
}

#define SB(index) avp_scan_byte(buf, (index))

static int avp_find_start_offset(const uint8_t* buf, size_t buf_len) {
    for (int i = 0; i < 0x400 && i + 3 < (int)buf_len; i++) {
        if (buf[i] == 0x48 && buf[i + 1] == 0xe7 && buf[i + 2] == 0xfc && buf[i + 3] == 0xfe)
            return i;
    }
    return -1;
}

static bool avp_extract_init(const AvpScan* buf, size_t buf_len, int start_offset, int* sub_song_list_off,
                             int* pos_lists_off) {
    int index;
    int sl = (int)buf_len;

    for (index = start_offset; index < (sl - 6); index += 2) {
        if (SB(index) == 0xe9 && SB(index + 1) == 0x41 && SB(index + 2) == 0x70 && SB(index + 3) == 0x00
            && SB(index + 4) == 0x41 && SB(index + 5) == 0xfa)
            break;
    }
    if (index >= (sl - 6))
        return false;

    *sub_song_list_off = (((int8_t)SB(index + 6) * 256) | SB(index + 7)) + index + 6;

    for (; index < (sl - 4); index += 2) {
        if (SB(index) == 0x4e && SB(index + 1) == 0x75)
            return false;
        if (SB(index) == 0x61 && SB(index + 1) == 0x00)
            break;
    }
    if (index >= (sl - 4))
        return false;

    index = (((int8_t)SB(index + 2) * 256) | SB(index + 3)) + index + 2;
    if (index >= sl)
        return false;

    if (SB(index) != 0x7a || SB(index + 1) != 0x00)
        return false;
    if (SB(index + 6) != 0x49 || SB(index + 7) != 0xfa)
        return false;

    *pos_lists_off = (((int8_t)SB(index + 8) * 256) | SB(index + 9)) + index + 8;
    return true;
}

static int avp_find_enabled_effects(const AvpScan* buf, int buf_len, int start_offset, bool* have_set_note,
                                    bool* have_set_fixed_sample, bool* have_set_arpeggio, bool* have_set_sample,
                                    bool* have_arpeggio) {
    int index = start_offset;
    int sl = buf_len;

    for (; index < (sl - 8); index += 2) {
        if (SB(index) == 0x08 && SB(index + 1) == 0x31 && SB(index + 6) == 0x67)
            break;
    }
    if (index >= (sl - 8))
        return -1;

    *have_set_note = false;
    *have_set_fixed_sample = false;
    *have_set_arpeggio = false;
    *have_set_sample = false;
    *have_arpeggio = false;

    do {
        switch (SB(index + 3)) {
            case 0x01:
                *have_set_note = true;
                break;
            case 0x02:
                *have_set_fixed_sample = true;
                break;
            case 0x03:
                *have_set_arpeggio = true;
                break;
            case 0x04:
                *have_set_sample = true;
                break;
            case 0x05:
                *have_arpeggio = true;
                break;
        }

        // Each test branches forward over its handler; a branch that does not advance would rescan forever
        const int advance = (int8_t)SB(index + 7) + 8;
        if (advance <= 0)
            return -1;
        index += advance;
        if (index >= sl)
            return -1;
    } while (SB(index) == 0x08 && SB(index + 1) == 0x31 && SB(index + 6) == 0x67);

    return index;
}

static bool avp_extract_play(AvpModule* m, const AvpScan* buf, size_t buf_len, int start_offset) {
    int sl = (int)buf_len;
    int index;

    for (index = start_offset; index < (sl - 8); index += 2) {
        if (SB(index) == 0x2c && SB(index + 1) == 0x7c && SB(index + 6) == 0x4a && SB(index + 7) == 0x29)
            break;
    }
    if (index >= (sl - 8))
        return false;

    int start_of_play = index;
    int global_offset = 0;
    m->instruments_offset = 0;

    index -= 4;
    for (; index >= 0; index -= 2) {
        if (SB(index) == 0x4b && SB(index + 1) == 0xfa)
            m->instruments_offset = (((int8_t)SB(index + 2) * 256) | SB(index + 3)) + index + 2;
        else if (SB(index) == 0x43 && SB(index + 1) == 0xfa)
            global_offset = (((int8_t)SB(index + 2) * 256) | SB(index + 3)) + index + 2;
        if (m->instruments_offset != 0 && global_offset != 0)
            break;
    }
    if (m->instruments_offset == 0 || global_offset == 0)
        return false;

    for (index = start_of_play; index < (sl - 16); index += 2) {
        if (SB(index) == 0x53 && SB(index + 1) == 0x69 && SB(index + 4) == 0x67)
            break;
    }
    if (index >= (sl - 16))
        return false;

    if (SB(index + 6) == 0x70 && SB(index + 7) == 0x03)
        m->speed_variation_version = 1;
    else if (SB(index + 6) == 0x7a && SB(index + 7) == 0x00) {
        m->speed_variation_version = 2;
        if (SB(index + 12) != 0xda || SB(index + 13) != 0x29)
            return false;
        m->speed_variation_speed_increment_offset = global_offset + (((int8_t)SB(index + 14) * 256) | SB(index + 15));
    } else
        return false;

    index += 8;

    for (; index < (sl - 12); index += 2) {
        if (SB(index) == 0x7a && SB(index + 1) == 0x00 && SB(index + 2) == 0x1a && SB(index + 3) == 0x31
            && SB(index + 6) == 0xda && SB(index + 7) == 0x45 && SB(index + 8) == 0x49 && SB(index + 9) == 0xfa)
            break;
    }
    if (index >= (sl - 12))
        return false;

    m->track_offsets_offset = (((int8_t)SB(index + 10) * 256) | SB(index + 11)) + index + 10;
    index += 12;

    if (index >= (sl - 8))
        return false;
    if (SB(index) != 0x3a || SB(index + 1) != 0x34 || SB(index + 4) != 0x49 || SB(index + 5) != 0xfa)
        return false;

    m->tracks_offset = (((int8_t)SB(index + 6) * 256) | SB(index + 7)) + index + 6;
    index += 8;

    for (; index < (sl - 6); index += 2) {
        if (SB(index) == 0x18 && SB(index + 1) == 0x31)
            break;
    }
    if (index >= (sl - 6))
        return false;

    m->reset_volume = SB(index + 4) == 0x66;
    index += 6;

    for (; index < (sl - 10); index += 2) {
        if (SB(index) == 0x42 && SB(index + 1) == 0x31)
            break;
    }
    if (index >= (sl - 10))
        return false;
    index += 8;

    if (SB(index) == 0x08 && SB(index + 1) == 0x31)
        m->parse_track_version = 1;
    else if (SB(index) == 0x4a && SB(index + 1) == 0x34)
        m->parse_track_version = 2;
    else if (SB(index) == 0x1a && SB(index + 1) == 0x34)
        m->parse_track_version = 3;
    else if (SB(index) == 0x42 && SB(index + 1) == 0x30) {
        m->parse_track_version = 4;
        index += 2;
        for (; index < (sl - 4); index += 2) {
            if (SB(index) == 0x31 && SB(index + 1) == 0x85)
                break;
            if (SB(index) == 0x0c && SB(index + 1) == 0x05 && SB(index + 2) == 0x00 && SB(index + 3) == 0x84) {
                m->parse_track_version = 5;
                break;
            }
        }
        if (index >= (sl - 4))
            return false;
        index -= 2;
    } else
        return false;

    index += 2;
    for (; index < (sl - 2); index += 2) {
        if (SB(index) == 0x31 && SB(index + 1) == 0x85)
            break;
    }
    if (index >= (sl - 2))
        return false;
    index += 4;

    m->instrument_format_version = 0;
    if (index >= (sl - 16))
        return false;

    if (SB(index) == 0x13 && SB(index + 1) == 0xb5 && SB(index + 2) == 0x50 && SB(index + 3) == 0x02
        && SB(index + 6) == 0x13 && SB(index + 7) == 0xb5 && SB(index + 8) == 0x50 && SB(index + 9) == 0x07
        && SB(index + 12) == 0x13 && SB(index + 13) == 0xb5 && SB(index + 14) == 0x50 && SB(index + 15) == 0x0f)
        m->instrument_format_version = 1;
    else if (SB(index) == 0x11 && SB(index + 1) == 0xb5 && SB(index + 2) == 0x50 && SB(index + 3) == 0x01
             && SB(index + 6) == 0x13 && SB(index + 7) == 0xb5 && SB(index + 8) == 0x50 && SB(index + 9) == 0x02
             && SB(index + 12) == 0x13 && SB(index + 13) == 0xb5 && SB(index + 14) == 0x50 && SB(index + 15) == 0x07
             && SB(index + 18) == 0x13 && SB(index + 19) == 0xb5 && SB(index + 20) == 0x50 && SB(index + 21) == 0x0f)
        m->instrument_format_version = 2;
    else if (index + 39 < sl && SB(index) == 0x11 && SB(index + 1) == 0xb5 && SB(index + 2) == 0x50
             && SB(index + 3) == 0x01 && SB(index + 6) == 0x13 && SB(index + 7) == 0xb5 && SB(index + 8) == 0x50
             && SB(index + 9) == 0x02 && SB(index + 12) == 0x13 && SB(index + 13) == 0xb5 && SB(index + 14) == 0x50
             && SB(index + 15) == 0x03 && SB(index + 18) == 0x31 && SB(index + 19) == 0xb5 && SB(index + 20) == 0x50
             && SB(index + 21) == 0x04 && SB(index + 24) == 0x33 && SB(index + 25) == 0x75 && SB(index + 26) == 0x50
             && SB(index + 27) == 0x06 && SB(index + 30) == 0x13 && SB(index + 31) == 0xb5 && SB(index + 32) == 0x50
             && SB(index + 33) == 0x08 && SB(index + 36) == 0x13 && SB(index + 37) == 0xb5 && SB(index + 38) == 0x50
             && SB(index + 39) == 0x0f)
        m->instrument_format_version = 3;
    else
        return false;

    for (; index < (sl - 14); index += 2) {
        if (SB(index) == 0xe5 && SB(index + 1) == 0x45 && SB(index + 2) == 0x45 && SB(index + 3) == 0xfa)
            break;
    }
    if (index >= (sl - 14))
        return false;

    m->sample_start_offsets_offset = (((int8_t)SB(index + 4) * 256) | SB(index + 5)) + index + 4;
    if (SB(index + 10) != 0x45 || SB(index + 11) != 0xfa)
        return false;
    m->sample_data_offset = (((int8_t)SB(index + 12) * 256) | SB(index + 13)) + index + 12;

    index += 14;
    if (index >= (sl - 20))
        return false;

    m->have_separate_sample_info = false;
    if (SB(index + 12) == 0xca && SB(index + 13) == 0xfc && SB(index + 16) == 0x45 && SB(index + 17) == 0xfa) {
        m->have_separate_sample_info = true;
        m->sample_info_offset = (((int8_t)SB(index + 18) * 256) | SB(index + 19)) + index + 18;
        index += 18;
    }

    for (; index < (sl - 12); index += 2) {
        if (SB(index) == 0x6b && SB(index + 1) == 0x00 && SB(index + 4) == 0x4a && SB(index + 5) == 0x31)
            break;
    }
    if (index >= (sl - 12))
        return false;
    index += 10;

    if (SB(index) == 0x7a && SB(index + 1) == 0x00)
        m->portamento_vibrato_type = AVP_PORTAMENTO_VIBRATO_ONLY_ONE;
    else if (SB(index) == 0x53 && SB(index + 1) == 0x31)
        m->portamento_vibrato_type = AVP_PORTAMENTO_VIBRATO_BOTH_TOGETHER;
    else
        return false;

    for (; index < (sl - 2); index += 2) {
        if (SB(index) == 0xda && SB(index + 1) == 0x45)
            break;
    }
    if (index >= (sl - 2))
        return false;

    for (; index < (sl - 10); index += 2) {
        if (SB(index) == 0x9b && SB(index + 1) == 0x70)
            break;
    }
    if (index >= (sl - 10))
        return false;

    if (SB(index + 4) == 0x53 && SB(index + 5) == 0x31)
        m->vibrato_version = 1;
    else if (SB(index + 8) == 0x8a && SB(index + 9) == 0xf1)
        m->vibrato_version = 2;
    else
        return false;

    index = avp_find_enabled_effects(buf, sl, index + 10, &m->have_set_note, &m->have_set_fixed_sample,
                                     &m->have_set_arpeggio, &m->have_set_sample, &m->have_arpeggio);
    if (index == -1)
        return false;

    m->have_envelope = false;
    if (index >= (sl - 8))
        return false;

    if (SB(index + 4) == 0x6b && SB(index + 6) == 0x4a && SB(index + 7) == 0x31) {
        m->have_envelope = true;
        index += 8;
        for (; index < (sl - 10); index += 2) {
            if (SB(index) == 0xe9 && SB(index + 1) == 0x44 && (SB(index + 2) == 0x31 || SB(index + 2) == 0x11)
                && SB(index + 3) == 0x84 && SB(index + 6) == 0x45 && SB(index + 7) == 0xfa)
                break;
        }
        if (index >= (sl - 10))
            return false;
        m->envelopes_offset = (((int8_t)SB(index + 8) * 256) | SB(index + 9)) + index + 8;
    }

    return true;
}

// Loading

static bool avp_load_speed_variation(AvpModule* m, AvpReader* r) {
    if (m->speed_variation_version == 2) {
        avp_reader_seek(r, m->speed_variation_speed_increment_offset);
        m->speed_variation_speed_init = avp_reader_read_u8(r);
        if (avp_reader_eof(r))
            return false;
    }
    return true;
}

typedef struct AvpBytes {
    uint8_t* data;
    int count;
    int capacity;
} AvpBytes;

static bool avp_bytes_push(AvpBytes* bytes, uint8_t value) {
    if (bytes->count >= bytes->capacity) {
        if (bytes->capacity > INT_MAX / 2)
            return false;
        const int capacity = bytes->capacity == 0 ? 64 : bytes->capacity * 2;
        uint8_t* grown = (uint8_t*)realloc(bytes->data, (size_t)capacity);
        if (grown == nullptr)
            return false;
        bytes->data = grown;
        bytes->capacity = capacity;
    }
    bytes->data[bytes->count++] = value;
    return true;
}

static uint8_t* avp_load_position_list(AvpReader* r, int* out_len) {
    AvpBytes list = { 0 };

    for (;;) {
        uint8_t dat = avp_reader_read_u8(r);
        if (avp_reader_eof(r) || !avp_bytes_push(&list, dat))
            break;

        if ((dat >= 0xfd) || ((dat & 0x40) == 0)) {
            if (!avp_bytes_push(&list, avp_reader_read_u8(r)))
                break;
        }

        if (dat == 0xfe || dat == 0xff) {
            *out_len = list.count;
            return list.data;
        }
    }

    free(list.data);
    return nullptr;
}

static bool avp_load_sub_song_info(AvpModule* m, AvpReader* r) {
    int num_sub_songs = (m->position_lists_offset - m->sub_song_list_offset) / 16;
    if (num_sub_songs <= 0 || num_sub_songs > AVP_MAX_TABLE_ENTRIES)
        return false;
    m->num_song_infos = num_sub_songs;
    m->song_info_list = (AvpSongInfo*)calloc(num_sub_songs, sizeof(AvpSongInfo));
    if (!m->song_info_list)
        return false;

    for (int i = 0; i < num_sub_songs; i++) {
        avp_reader_seek(r, m->sub_song_list_offset + i * 16);

        uint16_t pos_offsets[4];
        for (int j = 0; j < 4; j++)
            pos_offsets[j] = avp_reader_read_b_u16(r);

        if (avp_reader_read_signed(r, m->song_info_list[i].speed_variation, 8) != 8)
            return false;

        for (int j = 0; j < 4; j++) {
            avp_reader_seek(r, m->position_lists_offset + pos_offsets[j]);
            int len = 0;
            m->song_info_list[i].position_lists[j] = avp_load_position_list(r, &len);
            if (!m->song_info_list[i].position_lists[j])
                return false;
            m->song_info_list[i].position_list_lengths[j] = len;
        }
    }

    return true;
}

static uint8_t* avp_load_single_track(AvpReader* r, int parse_track_version, int* out_len) {
    AvpBytes track = { 0 };
    bool ok = true;

    // Every byte of the track is copied; a failed allocation or the end of the data stops the load.
#define PUSH(b) (ok = avp_bytes_push(&track, (b)) && ok)

    while (ok) {
        uint8_t dat = avp_reader_read_u8(r);
        if (avp_reader_eof(r))
            break;
        PUSH(dat);

        if (dat == 0xff) {
            *out_len = track.count;
            return ok ? track.data : (free(track.data), nullptr);
        }

        if (parse_track_version == 3) {
            while ((dat & 0x80) != 0 && ok && !avp_reader_eof(r)) {
                PUSH(avp_reader_read_u8(r));
                dat = avp_reader_read_u8(r);
                PUSH(dat);
            }
        } else if (parse_track_version == 4 || parse_track_version == 5) {
            if (dat != 0x81) {
                while ((dat & 0x80) != 0 && ok && !avp_reader_eof(r)) {
                    PUSH(avp_reader_read_u8(r));
                    dat = avp_reader_read_u8(r);
                    PUSH(dat);
                }
            }
        } else {
            if ((dat & 0x80) != 0) {
                PUSH(avp_reader_read_u8(r));
                if (parse_track_version == 2)
                    PUSH(avp_reader_read_u8(r));
            }
        }

        PUSH(avp_reader_read_u8(r));
        if (avp_reader_eof(r))
            break;
    }

#undef PUSH
    free(track.data);
    return nullptr;
}

static bool avp_load_tracks(AvpModule* m, AvpReader* r) {
    int num_tracks = (m->tracks_offset - m->track_offsets_offset) / 2;
    if (num_tracks <= 0 || num_tracks > AVP_MAX_TABLE_ENTRIES)
        return false;
    m->num_tracks = num_tracks;

    int16_t* offsets = (int16_t*)calloc(num_tracks, sizeof(int16_t));
    m->tracks = (uint8_t**)calloc(num_tracks, sizeof(uint8_t*));
    m->track_lengths = (int*)calloc(num_tracks, sizeof(int));
    if (!offsets || !m->tracks || !m->track_lengths) {
        free(offsets);
        return false;
    }

    avp_reader_seek(r, m->track_offsets_offset);
    for (int i = 0; i < num_tracks; i++)
        offsets[i] = avp_reader_read_b_i16(r);
    if (avp_reader_eof(r)) {
        free(offsets);
        return false;
    }

    for (int i = 0; i < num_tracks; i++) {
        if (offsets[i] < 0)
            continue;

        avp_reader_seek(r, m->tracks_offset + offsets[i]);
        m->tracks[i] = avp_load_single_track(r, m->parse_track_version, &m->track_lengths[i]);
        if (!m->tracks[i]) {
            free(offsets);
            return false;
        }
    }

    free(offsets);
    return true;
}

static bool avp_load_envelopes(AvpModule* m, AvpReader* r) {
    if (!m->have_envelope)
        return true;

    int num = (m->instruments_offset - m->envelopes_offset) / 16;
    if (num <= 0 || num > AVP_MAX_TABLE_ENTRIES)
        return false;
    m->num_envelopes = num;
    m->envelopes = (AvpEnvelope*)calloc(num, sizeof(AvpEnvelope));
    if (!m->envelopes)
        return false;

    avp_reader_seek(r, m->envelopes_offset);

    for (int i = 0; i < num; i++) {
        for (int j = 0; j < 5; j++) {
            m->envelopes[i].points[j].ticks_to_wait = avp_reader_read_u8(r);
            m->envelopes[i].points[j].volume_increment_value = avp_reader_read_i8(r);
            m->envelopes[i].points[j].times_to_repeat = avp_reader_read_u8(r);
            if (avp_reader_eof(r))
                return false;
        }
        // Dragon breed fix: extra envelope point
        m->envelopes[i].points[5].ticks_to_wait = avp_reader_read_u8(r);
        m->envelopes[i].points[5].volume_increment_value = 0;
        m->envelopes[i].points[5].times_to_repeat = 0xff;
    }

    return true;
}

static void avp_load_instrument1(AvpReader* r, AvpInstrument* instr) {
    instr->sample_number = avp_reader_read_u8(r);
    instr->envelope_number = avp_reader_read_u8(r);
    instr->enabled_effect_flags = avp_reader_read_u8(r);
    avp_reader_read_u8(r); // skip 1
    instr->portamento_add = avp_reader_read_u8(r);
    avp_reader_read_u8(r);
    avp_reader_read_u8(r); // skip 2
    instr->stop_reset_effect_delay = avp_reader_read_u8(r);
    instr->sample_number2 = avp_reader_read_u8(r);
    avp_reader_read_signed(r, instr->arpeggio_table, 4);
    instr->fixed_or_transposed_note = avp_reader_read_u8(r);
    instr->vibrato_number = avp_reader_read_u8(r);
    instr->vibrato_delay = avp_reader_read_u8(r);
}

static void avp_load_instrument2(AvpReader* r, AvpInstrument* instr) {
    instr->sample_number = avp_reader_read_u8(r);
    instr->volume = avp_reader_read_u8(r);
    instr->enabled_effect_flags = avp_reader_read_u8(r);
    avp_reader_read_u8(r);
    instr->portamento_add = avp_reader_read_u8(r);
    avp_reader_read_u8(r);
    avp_reader_read_u8(r);
    instr->stop_reset_effect_delay = avp_reader_read_u8(r);
    instr->sample_number2 = avp_reader_read_u8(r);
    avp_reader_read_signed(r, instr->arpeggio_table, 4);
    instr->fixed_or_transposed_note = avp_reader_read_u8(r);
    instr->vibrato_number = avp_reader_read_u8(r);
    instr->vibrato_delay = avp_reader_read_u8(r);
}

static void avp_load_instrument3(AvpReader* r, AvpInstrument* instr) {
    instr->sample_number = avp_reader_read_u8(r);
    instr->volume = avp_reader_read_u8(r);
    instr->enabled_effect_flags = avp_reader_read_u8(r);
    instr->transpose = avp_reader_read_i8(r);
    instr->fine_tune = avp_reader_read_b_i16(r);
    instr->sample_start_offset = avp_reader_read_b_u16(r);
    instr->stop_reset_effect_delay = avp_reader_read_u8(r);
    avp_reader_read_signed(r, instr->arpeggio_table, 4);
    instr->fixed_or_transposed_note = avp_reader_read_u8(r);
    instr->vibrato_number = avp_reader_read_u8(r);
    instr->vibrato_delay = avp_reader_read_u8(r);
}

static bool avp_load_instruments(AvpModule* m, AvpReader* r) {
    int num = (m->track_offsets_offset - m->instruments_offset) / 16;
    if (num <= 0 || num > AVP_MAX_TABLE_ENTRIES)
        return false;
    m->num_instruments = num;
    m->instruments = (AvpInstrument*)calloc(num, sizeof(AvpInstrument));
    if (!m->instruments)
        return false;

    avp_reader_seek(r, m->instruments_offset);

    for (int i = 0; i < num; i++) {
        if (m->instrument_format_version == 1)
            avp_load_instrument1(r, &m->instruments[i]);
        else if (m->instrument_format_version == 2)
            avp_load_instrument2(r, &m->instruments[i]);
        else if (m->instrument_format_version == 3)
            avp_load_instrument3(r, &m->instruments[i]);
        else
            return false;
        if (avp_reader_eof(r))
            return false;
    }

    return true;
}

static bool avp_load_sample_info(AvpModule* m, AvpReader* r) {
    m->num_samples = 27;
    m->samples = (AvpSample*)calloc(27, sizeof(AvpSample));
    if (!m->samples)
        return false;

    if (m->have_separate_sample_info) {
        avp_reader_seek(r, m->sample_info_offset);
        for (int i = 0; i < 27; i++) {
            m->samples[i].length = avp_reader_read_b_u16(r);
            m->samples[i].loop_start = avp_reader_read_b_u16(r);
            m->samples[i].loop_length = avp_reader_read_b_u16(r);
            if (avp_reader_eof(r))
                return false;
        }
    }

    return true;
}

static bool avp_load_sample_data(AvpModule* m, AvpReader* r) {
    uint32_t start_offsets[28]; // one extra
    avp_reader_seek(r, m->sample_start_offsets_offset);
    for (int i = 0; i < 28; i++)
        start_offsets[i] = avp_reader_read_b_u32(r);
    if (avp_reader_eof(r))
        return false;

    for (int i = 0; i < 27; i++) {
        if (start_offsets[i + 1] < start_offsets[i])
            return false;
        uint32_t length = start_offsets[i + 1] - start_offsets[i];

        if (length == 0) {
            m->samples[i].length = 0;
            m->samples[i].loop_start = 0;
            m->samples[i].loop_length = 1;
        } else {
            avp_reader_seek(r, m->sample_data_offset + start_offsets[i]);

            if (!m->have_separate_sample_info) {
                m->samples[i].length = avp_reader_read_b_u16(r);
                m->samples[i].loop_start = avp_reader_read_b_u16(r);
                m->samples[i].loop_length = avp_reader_read_b_u16(r);
                if (length < 6)
                    return false;
                length -= 6;
            }

            if (length > r->size)
                return false;

            m->samples[i].sample_data = (int8_t*)malloc(length != 0 ? length : 1);
            if (!m->samples[i].sample_data)
                return false;
            m->samples[i].data_size = length;
            if (avp_reader_read_signed(r, m->samples[i].sample_data, length) != length)
                return false;
        }
    }

    return true;
}

// GetPeriod helper

// One byte of a table read with a position the module chose; past either end it reads as the end marker.
static uint8_t avp_byte_at(const uint8_t* data, int length, int index) {
    if (index < 0 || index >= length)
        return 0xff;
    return data[index];
}

static const AvpEnvelope* avp_envelope_for(const AvpModule* m, int number) {
    return &m->envelopes[number < m->num_envelopes ? number : 0];
}

static const AvpSample* avp_sample_for(const AvpModule* m, int number) {
    return &m->samples[number < m->num_samples ? number : 0];
}

static uint16_t avp_get_period(uint8_t note, int8_t transpose) {
    int index = note + transpose;
    if (index < 0)
        index = 0;
    else if (index >= 85)
        index = 84;
    return avp_periods[index];
}

// Initialize

static void avp_stop_and_reset(AvpModule* m);

static void avp_initialize_voice_info(AvpModule* m) {
    for (int i = 0; i < 4; i++) {
        AvpVoiceInfo* vi = &m->voices[i];
        memset(vi, 0, sizeof(AvpVoiceInfo));

        vi->speed_counter = 1;
        vi->speed_counter2 = 0;
        vi->max_speed_counter = 0;
        vi->tick_counter = 0;

        vi->position_list = m->current_song_info->position_lists[i];
        vi->position_list_length = m->current_song_info->position_list_lengths[i];
        vi->position_list_position = -1;
        vi->position_list_loop_enabled = false;
        vi->position_list_loop_count = 0;
        vi->position_list_loop_start = 0;

        vi->track_number = 0;
        vi->track_position = 0;
        vi->loop_track_counter = 0;

        vi->note = 0;
        vi->transpose = 0;
        vi->fine_tune = 0;
        vi->note_and_flag = 0;
        vi->period = 0;

        vi->instrument = &m->instruments[0];
        vi->instrument_number = (m->parse_track_version >= 3) ? 0 : -1;

        vi->sample_number = 0;
        vi->sample_data = nullptr;
        vi->sample_length = 0;
        vi->sample_loop_start = 0;
        vi->sample_loop_length = 0;

        vi->enabled_effects_flag = 0;
        vi->stop_reset_effect = false;
        vi->stop_reset_effect_delay = 0;

        vi->envelope = m->have_envelope ? &m->envelopes[0] : nullptr;
        vi->envelope_position = 0;
        vi->envelope_wait_counter = 0;
        vi->envelope_loop_count = 0;

        vi->mute = false;
        vi->volume = 0;
        vi->track_volume = 64;

        vi->portamento_value = 0;

        vi->vibrato_speed = 0;
        vi->vibrato_delay = 0;
        vi->vibrato_depth = 0;
        vi->vibrato_direction = false;
        vi->vibrato_count_direction = false;
        vi->vibrato_counter_max = 0;
        vi->vibrato_counter = 0;

        avp_parse_next_position(m, vi, false);
    }
}

static void avp_initialize_sound(AvpModule* m, int sub_song) {
    m->current_song_info = &m->song_info_list[sub_song];

    m->speed_variation_counter = 0;
    m->speed_index = 0;
    m->speed_variation2_counter = 255;
    m->speed_variation2_speed = m->speed_variation_speed_init;

    m->master_volume_fade_counter = 0;
    m->master_volume_fade_speed = -1;
    m->master_volume = 64;

    m->global_transpose = 0;

    m->has_ended = false;
    m->ended_channels = 0;

    avp_initialize_voice_info(m);
}

// Speed variation

static void avp_do_master_volume_fade(AvpModule* m) {
    if (m->master_volume_fade_speed >= 0) {
        m->master_volume_fade_counter--;
        if (m->master_volume_fade_counter < 0) {
            m->master_volume_fade_counter = m->master_volume_fade_speed;
            m->master_volume--;
            if (m->master_volume == 0)
                avp_stop_and_reset(m);
        }
    }
}

static void avp_do_speed_variation1(AvpModule* m) {
    if (m->speed_variation_version == 1) {
        m->speed_variation_counter--;
        if (m->speed_variation_counter < 0) {
            m->speed_index--;
            if (m->speed_index < 0)
                m->speed_index = 7;
            m->speed_variation_counter = m->current_song_info->speed_variation[m->speed_index];
        }
    }
}

static void avp_do_speed_variation2(AvpModule* m) {
    if (m->speed_variation_version == 2) {
        uint16_t counter = (uint16_t)(m->speed_variation2_counter + m->speed_variation2_speed);
        if (counter > 255)
            m->speed_variation_counter = 0;
        else
            m->speed_variation_counter = -1;
        m->speed_variation2_counter = (uint8_t)counter;
    }
}

// Parse next position

static void avp_parse_next_position(AvpModule* m, AvpVoiceInfo* vi, bool update_information) {
    (void)update_information;
    const uint8_t* pl = vi->position_list;
    const int pl_length = vi->position_list_length;

    // A position list read past its end reads as the end marker.
#define PL(i) avp_byte_at(pl, pl_length, (i))
    int8_t position = vi->position_list_position;
    bool one_more;

    do {
        one_more = false;
        position++;

        if (PL(position) >= 0xfe) {
            vi->track_number = PL(position);
            position = (int8_t)(PL(position + 1) - 1);
        } else {
            if (PL(position) == 0xfd) {
                position++;
                m->master_volume_fade_speed = (int8_t)PL(position);
                one_more = true;
            } else {
                if ((PL(position) & 0x40) != 0) {
                    if (vi->position_list_loop_enabled) {
                        vi->position_list_loop_count--;
                        if (vi->position_list_loop_count == 0) {
                            vi->position_list_loop_enabled = false;
                            one_more = true;
                            continue;
                        }
                        position = vi->position_list_loop_start;
                    } else {
                        vi->position_list_loop_enabled = true;
                        vi->position_list_loop_count = (uint8_t)(PL(position) & 0x3f);
                        vi->position_list_loop_start = ++position;
                    }
                }

                vi->loop_track_counter = PL(position++);
                vi->track_number = PL(position);

                if (m->parse_track_version == 5)
                    vi->max_speed_counter = 255;
            }
        }
    } while (one_more);

    vi->position_list_position = position;
}

#undef PL

// Stop and reset

static void avp_stop_and_reset(AvpModule* m) {
    m->master_volume = 64;
    m->master_volume_fade_speed = -1;
    m->master_volume_fade_counter = 0;
    m->speed_variation_counter = 0;

    for (int i = 0; i < 4; i++)
        avp_ch_mute(&m->channels[i]);

    avp_initialize_voice_info(m);
    m->has_ended = true;
    m->ended_channels = 0x0f;
}

// Counters

static void avp_do_counters(AvpVoiceInfo* vi) {
    if (vi->stop_reset_effect_delay == 0)
        vi->stop_reset_effect = true;
    else
        vi->stop_reset_effect_delay--;
    vi->tick_counter++;
}

// Effects

static void avp_do_vibrato(AvpModule* m, AvpVoiceInfo* vi) {
    if (vi->vibrato_delay != 0) {
        vi->vibrato_delay--;
        return;
    }

    if (vi->stop_reset_effect_delay == 0) {
        AvpInstrument* instr = vi->instrument;
        if (instr->vibrato_number != 0 && instr->vibrato_number < sizeof(avp_vibrato_counters)) {
            if (vi->vibrato_depth >= 0) {
                vi->vibrato_counter_max = avp_vibrato_counters[instr->vibrato_number];
                vi->vibrato_depth = m->vibrato_version == 1 ? avp_vibrato_depths1[instr->vibrato_number]
                                                            : avp_vibrato_depths2[instr->vibrato_number];
                vi->vibrato_count_direction = false;
                vi->vibrato_counter = 0;

                vi->period = avp_get_period(vi->note, vi->transpose);
                vi->vibrato_speed = (uint16_t)(vi->period - avp_get_period((uint8_t)(vi->note + 1), vi->transpose));

                if (m->vibrato_version == 1) {
                    for (;;) {
                        vi->vibrato_depth--;
                        if (vi->vibrato_depth < 0)
                            break;
                        vi->vibrato_speed /= 2;
                        if (vi->vibrato_speed == 0) {
                            vi->vibrato_speed = 1;
                            vi->vibrato_depth = -1;
                            break;
                        }
                    }
                } else {
                    int new_speed = vi->vibrato_speed / vi->vibrato_depth;
                    if (new_speed == 0)
                        new_speed = 1;
                    vi->vibrato_speed = (uint16_t)new_speed;
                    vi->vibrato_depth = -1;
                }
            } else {
                if (vi->vibrato_direction)
                    vi->period -= vi->vibrato_speed;
                else
                    vi->period += vi->vibrato_speed;

                if (vi->vibrato_count_direction) {
                    vi->vibrato_counter--;
                    if (vi->vibrato_counter == 0)
                        vi->vibrato_count_direction = false;
                } else {
                    vi->vibrato_counter++;
                    if (vi->vibrato_counter == vi->vibrato_counter_max) {
                        vi->vibrato_count_direction = true;
                        vi->vibrato_direction = !vi->vibrato_direction;
                    }
                }
            }
        }
    }
}

static void avp_do_portamento(AvpVoiceInfo* vi) {
    uint8_t pv = vi->portamento_value;
    if (pv >= 0xc0)
        vi->period += (uint16_t)(pv & 0x3f);
    else
        vi->period -= (uint16_t)(pv & 0x3f);
}

static void avp_do_set_note(AvpModule* m, AvpVoiceInfo* vi) {
    if (m->have_set_note && ((vi->enabled_effects_flag & 0x02) != 0)) {
        uint8_t note;
        if ((vi->tick_counter % 2) == 0)
            note = (uint8_t)(vi->note + vi->transpose);
        else {
            note = vi->instrument->fixed_or_transposed_note;
            if ((note & 0x80) != 0) {
                note &= 0x7f;
                note += vi->note;
            }
        }
        vi->period = avp_get_period(note, 0);
    }
}

static void avp_do_set_fixed_sample(AvpModule* m, AvpVoiceInfo* vi) {
    if (m->have_set_fixed_sample && ((vi->enabled_effects_flag & 0x04) != 0)) {
        uint8_t sample;
        if ((vi->tick_counter % 2) == 0)
            sample = vi->instrument->sample_number;
        else
            sample = 2;
        vi->sample_number = sample;
        if ((vi->period + vi->instrument->portamento_add) < 0x8000)
            vi->period += vi->instrument->portamento_add;
    }
}

static void avp_set_arpeggio(AvpVoiceInfo* vi, int index) {
    uint8_t note;
    int8_t arp_value = vi->instrument->arpeggio_table[index & 3];
    if (arp_value >= 0)
        note = (uint8_t)arp_value;
    else {
        note = vi->note;
        arp_value &= 0x7f;
        if (arp_value < 64)
            note += (uint8_t)arp_value;
        else
            note -= (uint8_t)(arp_value & 0x3f);
        note = (uint8_t)(note + vi->transpose);
    }
    vi->period = avp_get_period(note, 0);
}

static void avp_do_set_sample(AvpModule* m, AvpVoiceInfo* vi) {
    if (m->have_set_sample && ((vi->enabled_effects_flag & 0x10) != 0)) {
        if (vi->stop_reset_effect_delay != 0)
            vi->sample_number = vi->instrument->sample_number2;
        else if (!vi->stop_reset_effect)
            vi->sample_number = vi->instrument->sample_number;
    }
}

static void avp_do_arpeggio(AvpModule* m, AvpVoiceInfo* vi) {
    if (m->have_arpeggio && ((vi->enabled_effects_flag & 0x20) != 0)) {
        if (vi->stop_reset_effect_delay != 0)
            avp_set_arpeggio(vi, vi->tick_counter);
        else if (!vi->stop_reset_effect)
            vi->period = avp_get_period(vi->note, vi->transpose);
    }
}

static void avp_do_envelopes(AvpModule* m, AvpVoiceInfo* vi) {
    if (m->have_envelope) {
        if (((vi->note_and_flag & 0x80) == 0) && (vi->tick_counter == 0)) {
            AvpInstrument* instr = vi->instrument;
            const AvpEnvelope* env = avp_envelope_for(m, instr->envelope_number);

            vi->envelope = env;
            vi->envelope_loop_count = env->points[0].times_to_repeat;
            vi->envelope_position = 0;
            vi->envelope_wait_counter = 1;
            vi->volume = 0;
        }

        if (vi->envelope_wait_counter >= 0) {
            vi->envelope_wait_counter--;

            if (vi->envelope_wait_counter == 0) {
                const AvpEnvelope* env = vi->envelope;
                uint8_t pos = vi->envelope_position;

                int16_t volume = (int16_t)vi->volume;
                volume += env->points[pos].volume_increment_value;
                if (volume > 64)
                    volume = 64;
                else if (volume < 0)
                    volume = 0;
                vi->volume = (uint16_t)volume;

                vi->envelope_loop_count--;
                if (vi->envelope_loop_count == 0) {
                    pos++;
                    if (pos > 5)
                        pos = 5;
                    uint8_t ticks = env->points[pos].ticks_to_wait;
                    if (ticks >= 0xc0) {
                        pos = (uint8_t)((ticks & 0x3f) / 3);
                        if (pos > 5)
                            pos = 5;
                        ticks = env->points[pos].ticks_to_wait;
                    }
                    vi->envelope_wait_counter = (int8_t)ticks;
                    if (pos != 5)
                        vi->envelope_loop_count = env->points[pos].times_to_repeat;
                    vi->envelope_position = pos;
                } else {
                    vi->envelope_wait_counter = (int8_t)env->points[pos].ticks_to_wait;
                }
            }
        }
    }
}

// RunEffects1 / RunEffects2

static void avp_run_effects1(AvpModule* m, AvpVoiceInfo* vi) {
    switch (m->portamento_vibrato_type) {
        case AVP_PORTAMENTO_VIBRATO_ONLY_ONE:
            if (vi->portamento_value != 0)
                avp_do_portamento(vi);
            else
                avp_do_vibrato(m, vi);
            break;
        case AVP_PORTAMENTO_VIBRATO_BOTH_TOGETHER:
            avp_do_vibrato(m, vi);
            if (vi->portamento_value != 0)
                avp_do_portamento(vi);
            break;
    }

    avp_do_set_note(m, vi);
    avp_do_set_fixed_sample(m, vi);

    if (m->have_set_arpeggio && ((vi->enabled_effects_flag & 0x08) != 0))
        avp_set_arpeggio(vi, vi->tick_counter & 3);
    else {
        avp_do_set_sample(m, vi);
        avp_do_arpeggio(m, vi);
    }

    avp_do_envelopes(m, vi);
    avp_do_counters(vi);
}

static void avp_run_effects2(AvpModule* m, AvpVoiceInfo* vi) {
    avp_do_set_sample(m, vi);
    avp_do_arpeggio(m, vi);
    avp_do_envelopes(m, vi);
    avp_do_counters(vi);
}

// Parse next track position

static void avp_parse_next_track_position(AvpModule* m, int channel, AvpVoiceInfo* vi, AvpChannel* ch) {
    (void)channel;
    if (vi->track_number == 0xfe) {
        avp_stop_and_reset(m);
    }

    if (vi->track_number == 0xff) {
        vi->track_position = 0;
        avp_parse_next_position(m, vi, true);

        if (vi->track_number == 0xff) {
            m->ended_channels |= (1 << channel);
            if (m->ended_channels == 0x0f) // all 4 channels ended
                m->has_ended = true;
            return;
        }

        if (m->master_volume_fade_speed < 0) {
            m->ended_channels |= (1 << channel);
            if (m->ended_channels == 0x0f)
                m->has_ended = true;
        }
    }

    const uint8_t* track = avp_empty_track;
    int track_len = (int)sizeof(avp_empty_track);
    if (vi->track_number < m->num_tracks && m->tracks[vi->track_number] != nullptr) {
        track = m->tracks[vi->track_number];
        track_len = m->track_lengths[vi->track_number];
    }

    // A track read past its end reads as the end marker.
#define TRK(i) avp_byte_at(track, track_len, (i))

    if (m->reset_volume && vi->track_position == 0)
        vi->track_volume = 64;

    int8_t set_to_instrument = -1;

    uint8_t track_byte = TRK(vi->track_position);
    vi->track_position++;

    if (m->parse_track_version == 1)
        vi->speed_counter = (uint8_t)(track_byte & 0x3f);

    vi->portamento_value = 0;
    vi->stop_reset_effect = false;
    vi->mute = false;

    if (m->parse_track_version == 4 || m->parse_track_version == 5)
        vi->note_and_flag = 0;

    if ((m->parse_track_version == 1) && ((track_byte & 0x40) != 0)) {
        vi->envelope_wait_counter = 1;
        vi->envelope_loop_count = 1;
        vi->envelope_position = 3;
    }

    // The 8-bit cursor has 256 positions: a command chain longer than that cycles forever
    int chain_length = 0;
    bool one_more;
    do {
        one_more = false;

        if ((track_byte & 0x80) != 0) {
            switch (m->parse_track_version) {
                case 1:
                    if ((TRK(vi->track_position) & 0x80) != 0)
                        vi->portamento_value = TRK(vi->track_position);
                    else
                        set_to_instrument = (int8_t)TRK(vi->track_position);
                    vi->track_position++;
                    break;

                case 2:
                    set_to_instrument = (int8_t)(track_byte & 0x7f);
                    if ((TRK(vi->track_position) & 0x80) != 0)
                        vi->portamento_value = TRK(vi->track_position);
                    else
                        vi->track_volume = TRK(vi->track_position);
                    vi->track_position++;
                    track_byte = TRK(vi->track_position);
                    vi->track_position++;
                    break;

                case 3:
                    switch (track_byte) {
                        case 0x80:
                            set_to_instrument = (int8_t)TRK(vi->track_position);
                            break;
                        case 0x81:
                            vi->track_volume = TRK(vi->track_position);
                            break;
                        case 0x82:
                            vi->portamento_value = TRK(vi->track_position);
                            break;
                        case 0x8a:
                            vi->max_speed_counter = TRK(vi->track_position);
                            break;
                        case 0x8e:
                            vi->track_volume += TRK(vi->track_position);
                            break;
                    }
                    vi->track_position++;
                    track_byte = TRK(vi->track_position);
                    vi->track_position++;
                    one_more = true;
                    break;

                case 4:
                    one_more = true;
                    switch (track_byte) {
                        case 0x80:
                            set_to_instrument = (int8_t)TRK(vi->track_position);
                            break;
                        case 0x81:
                            vi->mute = true;
                            vi->note = 64;
                            vi->transpose = 0;
                            one_more = false;
                            break;
                        case 0x82:
                            vi->portamento_value = TRK(vi->track_position);
                            break;
                        case 0x83:
                            m->speed_variation2_speed = TRK(vi->track_position);
                            break;
                        case 0x87:
                            vi->note_and_flag = 0xff;
                            break;
                        case 0x8a:
                            vi->max_speed_counter = TRK(vi->track_position);
                            break;
                        case 0x8c:
                            vi->track_volume = TRK(vi->track_position);
                            break;
                        case 0x8d:
                            vi->track_volume += TRK(vi->track_position);
                            break;
                    }
                    if (one_more) {
                        vi->track_position++;
                        track_byte = TRK(vi->track_position);
                        vi->track_position++;
                    }
                    break;

                case 5:
                    one_more = true;
                    switch (track_byte) {
                        case 0x80:
                            set_to_instrument = (int8_t)TRK(vi->track_position);
                            break;
                        case 0x81:
                            vi->mute = true;
                            vi->note = 64;
                            vi->transpose = 0;
                            one_more = false;
                            break;
                        case 0x82:
                            vi->portamento_value = TRK(vi->track_position);
                            break;
                        case 0x83:
                            m->speed_variation2_speed = TRK(vi->track_position);
                            break;
                        case 0x84:
                            vi->note_and_flag = 0xff;
                            break;
                        case 0x85:
                            vi->max_speed_counter = TRK(vi->track_position);
                            break;
                        case 0x86:
                            m->global_transpose = (int8_t)TRK(vi->track_position);
                            break;
                        case 0x87:
                            vi->track_volume = TRK(vi->track_position);
                            break;
                        case 0x8b:
                            vi->track_volume += TRK(vi->track_position);
                            break;
                    }
                    if (one_more) {
                        vi->track_position++;
                        track_byte = TRK(vi->track_position);
                        vi->track_position++;
                    }
                    break;
            }
        } else if (m->parse_track_version == 4 || m->parse_track_version == 5) {
            vi->note = track_byte;
        }
    } while (one_more && vi->track_position <= track_len && ++chain_length < 256);

    if (m->parse_track_version == 4 || m->parse_track_version == 5) {
        vi->speed_counter = TRK(vi->track_position);
        vi->speed_counter2 = 0;
        vi->track_position++;
    } else {
        if (m->parse_track_version != 1)
            vi->speed_counter = track_byte;
        vi->note_and_flag = TRK(vi->track_position);
        vi->note = (uint8_t)(vi->note_and_flag & 0x7f);
        vi->track_position++;
    }

    if ((set_to_instrument >= 0) && (set_to_instrument != vi->instrument_number)) {
        vi->instrument_number = set_to_instrument;
        vi->note_and_flag = 0;
    }

    if ((vi->note_and_flag & 0x80) == 0) {
        int inst_idx = vi->instrument_number;
        if (inst_idx < 0)
            inst_idx = 0;
        if (inst_idx >= m->num_instruments)
            inst_idx = 0;
        AvpInstrument* instr = &m->instruments[inst_idx];

        vi->instrument = instr;
        vi->enabled_effects_flag = instr->enabled_effect_flags;
        vi->stop_reset_effect_delay = instr->stop_reset_effect_delay;
        vi->vibrato_delay = instr->vibrato_delay;
        vi->transpose = instr->transpose;
        vi->fine_tune = instr->fine_tune;
        vi->sample_number = instr->sample_number;

        if (!m->have_envelope)
            vi->volume = instr->volume;

        const AvpSample* samp = avp_sample_for(m, vi->sample_number);

        vi->sample_data = samp->sample_data;
        vi->sample_data_size = samp->data_size;
        vi->sample_length = samp->length;
        vi->sample_loop_start = samp->loop_start;
        vi->sample_loop_length = samp->loop_length;

        avp_ch_set_amiga_period(ch, 126);
        avp_ch_play_sample(ch, vi->sample_number, vi->sample_data, vi->sample_data_size, instr->sample_start_offset,
                           vi->sample_length * 2U);

        if (vi->sample_loop_length > 1)
            avp_ch_set_loop(ch, vi->sample_loop_start, vi->sample_loop_length * 2U);
    }

    vi->note = (uint8_t)(vi->note + m->global_transpose);
    vi->period = avp_get_period(vi->note, vi->transpose);

    if (ch->play_pending) {
        ch->started_period = vi->period;
        ch->started_instrument
            = vi->instrument_number < 0 || vi->instrument_number >= m->num_instruments ? 0 : vi->instrument_number;
    }

    if (TRK(vi->track_position) == 0xff) {
        vi->track_position = 0;
        vi->loop_track_counter--;
        if (vi->loop_track_counter == 0)
            avp_parse_next_position(m, vi, true);
    }

    vi->tick_counter = 0;
    vi->vibrato_depth = 0;
}

#undef TRK

// Play tick

static void avp_play_tick(AvpModule* m) {
    avp_do_master_volume_fade(m);
    avp_do_speed_variation2(m);

    for (int i = 0; i < 4; i++) {
        AvpVoiceInfo* vi = &m->voices[i];
        AvpChannel* ch = &m->channels[i];

        if (m->speed_variation_counter == 0) {
            vi->speed_counter--;
            vi->speed_counter2++;

            if (vi->speed_counter == 0) {
                avp_parse_next_track_position(m, i, vi, ch);
                avp_run_effects2(m, vi);
                continue;
            }
        }

        avp_run_effects1(m, vi);
    }

    avp_do_speed_variation1(m);

    for (int i = 0; i < 4; i++) {
        AvpVoiceInfo* vi = &m->voices[i];
        AvpChannel* ch = &m->channels[i];

        uint32_t period = (uint32_t)(vi->period - vi->fine_tune);
        avp_ch_set_amiga_period(ch, period);

        int volume;
        if (m->have_envelope)
            volume = (vi->volume * m->master_volume) / 64;
        else {
            if ((m->speed_variation_version == 2)
                && (vi->mute || ((m->parse_track_version != 5) && (vi->speed_counter <= vi->max_speed_counter))
                    || ((m->parse_track_version == 5) && (vi->speed_counter2 >= vi->max_speed_counter))))
                volume = 0;
            else
                volume = (vi->track_volume * vi->volume * m->master_volume) / 4096;
        }

        avp_ch_set_amiga_volume(ch, (uint16_t)volume);
    }
}

// Voices

static uint32_t avp_min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

// Paula channels 0 and 3 are left, 1 and 2 right.
static float avp_pan(int channel) {
    return (channel == 0 || channel == 3) ? -1.0f : 1.0f;
}

// Hand the registers the tick wrote to the voice.
static void avp_sync_voice(AvpModule* m, int channel) {
    AvpChannel* c = &m->channels[channel];
    const uint32_t index = (uint32_t)channel;

    if (!c->active || c->sample_data == nullptr) {
        nuade_voice_stop(m->nuade_voices, index);
    } else if (c->play_pending) {
        // The first pass runs to sample_length, then the loop takes over.
        const NuadeSample head = { .data = c->sample_data,
                                   .format = NuadeSampleFormat_S8,
                                   .length = avp_min_u32(c->sample_length, c->data_size),
                                   .loop_start = 0,
                                   .loop_length = 0 };
        nuade_voice_play(m->nuade_voices, index, &head, c->sample_offset);
        if (c->loop_length > 0 && c->loop_start < c->data_size) {
            // A queued sample starts at its first frame, so the loop is its own sample.
            const uint32_t loop_len = avp_min_u32(c->loop_length, c->data_size - c->loop_start);
            const NuadeSample loop = { .data = c->sample_data + c->loop_start,
                                       .format = NuadeSampleFormat_S8,
                                       .length = loop_len,
                                       .loop_start = 0,
                                       .loop_length = loop_len };
            nuade_voice_queue(m->nuade_voices, index, &loop);
        }
    }

    c->play_pending = false;

    nuade_voice_set_pitch(m->nuade_voices, index, c->period == 0 ? 0.0 : nuade_paula_period_hz(c->period));
    // Pitch 0 holds the position, so a period of 0 is silenced through the volume.
    nuade_voice_set_volume(m->nuade_voices, index, c->period == 0 ? 0.0f : (float)c->volume / 64.0f);
    nuade_voice_set_pan(m->nuade_voices, index, avp_pan(channel));
}

bool avp_tick(AvpModule* module) {
    // A one-shot that ran off its end is silent now.
    for (int i = 0; i < AVP_VOICES; i++) {
        AvpChannel* ch = &module->channels[i];
        ch->active = ch->active && nuade_voice_is_playing(module->nuade_voices, (uint32_t)i);
    }

    avp_play_tick(module);

    for (int i = 0; i < AVP_VOICES; i++) {
        avp_sync_voice(module, i);
    }
    // rePlayer begin
//     return !module->has_ended;
    bool has_ended = module->has_ended;
    module->has_ended = false;
    return !has_ended;
    // rePlayer end
}

// Public API

// Finds the player code inside the module and reads the table offsets and format variants out of it.
static bool avp_identify(AvpModule* m, const uint8_t* data, size_t size) {
    int buf_len = size < 4096 ? (int)size : 4096;

    int start_offset = avp_find_start_offset(data, buf_len);
    if (start_offset < 0)
        return false;

    const AvpScan scan = { data, size < INT32_MAX ? (int)size : INT32_MAX };
    if (!avp_extract_init(&scan, buf_len, start_offset, &m->sub_song_list_offset, &m->position_lists_offset))
        return false;

    return avp_extract_play(m, &scan, buf_len, start_offset);
}

AvpModule* avp_create(const uint8_t* data, size_t size) {
    if (!data || size < 1024)
        return nullptr;

    AvpModule* m = (AvpModule*)calloc(1, sizeof(AvpModule));
    if (!m)
        return nullptr;

    if (!avp_identify(m, data, size)) {
        avp_destroy(m);
        return nullptr;
    }

    AvpReader reader;
    avp_reader_init(&reader, data, size);

    if (!avp_load_speed_variation(m, &reader)) {
        avp_destroy(m);
        return nullptr;
    }
    if (!avp_load_sub_song_info(m, &reader)) {
        avp_destroy(m);
        return nullptr;
    }
    if (!avp_load_tracks(m, &reader)) {
        avp_destroy(m);
        return nullptr;
    }
    if (!avp_load_envelopes(m, &reader)) {
        avp_destroy(m);
        return nullptr;
    }
    if (!avp_load_instruments(m, &reader)) {
        avp_destroy(m);
        return nullptr;
    }
    if (!avp_load_sample_info(m, &reader)) {
        avp_destroy(m);
        return nullptr;
    }
    if (!avp_load_sample_data(m, &reader)) {
        avp_destroy(m);
        return nullptr;
    }

    return m;
}

bool avp_probe(const uint8_t* data, size_t size) {
    AvpModule* m = avp_create(data, size);
    if (!m)
        return false;
    avp_destroy(m);
    return true;
}

void avp_destroy(AvpModule* module) {
    if (!module)
        return;

    if (module->song_info_list) {
        for (int i = 0; i < module->num_song_infos; i++) {
            for (int j = 0; j < 4; j++)
                free(module->song_info_list[i].position_lists[j]);
        }
        free(module->song_info_list);
    }

    if (module->tracks) {
        for (int i = 0; i < module->num_tracks; i++)
            free(module->tracks[i]);
        free(module->tracks);
    }

    free(module->track_lengths);
    free(module->envelopes);
    free(module->instruments);

    if (module->samples) {
        for (int i = 0; i < module->num_samples; i++)
            free(module->samples[i].sample_data);
        free(module->samples);
    }

    free(module);
}

int avp_subsong_count(const AvpModule* module) {
    if (!module)
        return 0;
    return module->num_song_infos;
}

bool avp_start(AvpModule* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong < 0 || subsong >= module->num_song_infos)
        return false;
    module->nuade_voices = voices;
    for (int i = 0; i < AVP_VOICES; i++) {
        AvpChannel* ch = &module->channels[i];
        *ch = (AvpChannel) { .started_instrument = -1 };
    }
    module->subsong = subsong;
    avp_initialize_sound(module, subsong);
    return true;
}

uint32_t avp_ticks_per_step(const AvpModule* module, int subsong) {
    if (!module || subsong < 0 || subsong >= module->num_song_infos)
        return 0;
    if (module->speed_variation_version == 2) {
        // The step counter gains `speed` per tick and steps on passing 255.
        const uint32_t speed = module->speed_variation_speed_init;
        return speed == 0 ? 0 : (256U + speed / 2) / speed;
    }
    // A table entry of n holds the step for n + 1 ticks; the eight entries run in turn.
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++)
        sum += (uint32_t)module->song_info_list[subsong].speed_variation[i] + 1U;
    return (sum + 4U) / 8U;
}

AvpVoiceState avp_voice_state(const AvpModule* module, int voice) {
    if (!module || voice < 0 || voice >= AVP_VOICES)
        return (AvpVoiceState) { .period = 0, .instrument = -1 };
    const AvpChannel* c = &module->channels[voice];
    return (AvpVoiceState) { .period = c->started_period, .instrument = c->started_instrument };
}
