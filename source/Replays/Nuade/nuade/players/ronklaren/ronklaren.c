// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/ronklaren.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Enums

typedef enum RkEffect {
    RK_EFFECT_SET_ARPEGGIO = 0x80,
    RK_EFFECT_SET_PORTAMENTO = 0x81,
    RK_EFFECT_SET_INSTRUMENT = 0x82,
    RK_EFFECT_END_SONG = 0x83,
    RK_EFFECT_CHANGE_ADSR_SPEED = 0x84,
    RK_EFFECT_END_SONG2 = 0x85,
    RK_EFFECT_END_OF_TRACK = 0xff
} RkEffect;

// Tables

static const uint16_t rk_periods[70]
    = { 6848, 6464, 6096, 5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3616, 3424, 3232, 3048, 2880, 2712, 2560,
        2416, 2280, 2152, 2032, 1920, 1808, 1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960,  904,
        856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  452,  428,  404,  381,  360,  339,  320,
        302,  285,  269,  254,  240,  226,  214,  202,  190,  180,  170,  160,  151,  143,  135,  127

      };

// Reader helpers (big-endian)

typedef struct RkReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} RkReader;

static void reader_init(RkReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool reader_eof(const RkReader* r) {
    return r->pos > r->size;
}

static void reader_seek(RkReader* r, size_t pos) {
    r->pos = pos;
}

static void reader_skip(RkReader* r, size_t bytes) {
    r->pos += bytes;
}

static uint8_t reader_read_u8(RkReader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static int8_t reader_read_i8(RkReader* r) {
    return (int8_t)reader_read_u8(r);
}

static uint16_t reader_read_b_u16(RkReader* r) {
    uint8_t hi = reader_read_u8(r);
    uint8_t lo = reader_read_u8(r);
    return (uint16_t)((hi << 8) | lo);
}

static int16_t reader_read_b_i16(RkReader* r) {
    return (int16_t)reader_read_b_u16(r);
}

static uint32_t reader_read_b_u32(RkReader* r) {
    uint8_t a = reader_read_u8(r);
    uint8_t b = reader_read_u8(r);
    uint8_t c = reader_read_u8(r);
    uint8_t d = reader_read_u8(r);
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d;
}

static int32_t reader_read_b_i32(RkReader* r) {
    return (int32_t)reader_read_b_u32(r);
}

static size_t reader_read_into(RkReader* r, uint8_t* buf, size_t count) {
    size_t avail = 0;
    if (r->pos < r->size)
        avail = r->size - r->pos;
    if (count > avail)
        count = avail;
    if (count > 0) {
        memcpy(buf, r->data + r->pos, count);
        r->pos += count;
    }
    return count;
}

static size_t reader_read_signed(RkReader* r, int8_t* buf, size_t count) {
    return reader_read_into(r, (uint8_t*)buf, count);
}

static int8_t* reader_read_sample_data(RkReader* r, int length) {
    if (length <= 0)
        return nullptr;
    int8_t* data = (int8_t*)malloc(length);
    if (!data)
        return nullptr;
    size_t read = reader_read_signed(r, data, length);
    if ((int)read != length) {
        free(data);
        return nullptr;
    }
    return data;
}

// Module struct

#define AMIGA_EXECUTABLE_HUNK_SIZE 32
#define HEADER_SIZE 32
#define MIN_FILE_SIZE 0xa40
#define RK_MAX_COMMANDS_PER_TICK 0x20000

// A module pointer (relative to the code hunk) as a file offset. A damaged pointer past the int range saturates, so it
// lands outside the file and its read fails.
static int file_offset_of(int32_t pointer) {
    return pointer > INT32_MAX - AMIGA_EXECUTABLE_HUNK_SIZE ? INT32_MAX : pointer + AMIGA_EXECUTABLE_HUNK_SIZE;
}

// Forward declarations

static void initialize_sound(RkModule* m, int song_number);
static void play_tick(RkModule* m);

// Finding functions (binary search in player code)

static uint32_t be32_at(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static bool find_number_of_sub_songs(const uint8_t* buf, int buf_len, int* out_count) {
    int index;
    *out_count = 0;

    // Skip all the JMP instructions
    for (index = HEADER_SIZE; index < (buf_len - 6); index += 6) {
        if ((buf[index] != 0x4e) || (buf[index + 1] != 0xf9))
            break;
    }

    if (index >= (buf_len - 6))
        return false;

    for (; index < (buf_len - 6); index += 8) {
        if ((buf[index] != 0x30) || (buf[index + 1] != 0x3c))
            break;
        (*out_count)++;
    }

    return *out_count != 0;
}

static bool check_for_vblank_player(const uint8_t* buf, int buf_len, uint32_t* irq_offset, uint16_t* cia_value) {
    int index = HEADER_SIZE + 6;
    *irq_offset = 0;

    if ((buf[index] != 0x4e) || (buf[index + 1] != 0xf9))
        return false;

    uint32_t play_offset = be32_at(buf + index + 2);

    if (play_offset >= (uint32_t)buf_len - 1)
        return false;

    index = (int)play_offset;

    if ((buf[index] != 0x41) || (buf[index + 1] != 0xfa))
        return false;

    *irq_offset = play_offset;
    *cia_value = 14187;

    return true;
}

static bool find_song_speed_and_irq_offset(const uint8_t* buf, int buf_len, uint32_t* irq_offset, uint16_t* cia_value) {
    int index = HEADER_SIZE;
    int init_offset = 0;
    *irq_offset = 0;

    int i;
    for (i = 0; i < 2; i++) {
        if ((buf[index] != 0x4e) || (buf[index + 1] != 0xf9))
            return false;

        uint32_t target = be32_at(buf + index + 2);

        if (target >= (uint32_t)buf_len - 1)
            return false;
        init_offset = (int)target;

        if (((buf[init_offset] == 0x61) && (buf[init_offset + 1] == 0x00))
            || ((buf[init_offset] == 0x33) && (buf[init_offset + 1] == 0xfc)))
            break;

        index += 6;
    }

    if (i == 2)
        return check_for_vblank_player(buf, buf_len, irq_offset, cia_value);

    index = init_offset;

    uint8_t cia_lo = 0, cia_hi = 0;

    for (; index < (buf_len - 10); index += 2) {
        if ((buf[index] == 0x4e) && (buf[index + 1] == 0x75))
            break;

        if ((buf[index] == 0x13) && (buf[index + 1] == 0xfc)) {
            uint8_t value = buf[index + 3];
            uint32_t adr = ((uint32_t)buf[index + 4] << 24) | ((uint32_t)buf[index + 5] << 16)
                           | ((uint32_t)buf[index + 6] << 8) | buf[index + 7];
            index += 6;

            if (adr == 0xbfd400)
                cia_lo = value;
            else if (adr == 0xbfd500)
                cia_hi = value;
        } else if ((buf[index] == 0x23) && (buf[index + 1] == 0xfc)) {
            uint32_t adr = ((uint32_t)buf[index + 2] << 24) | ((uint32_t)buf[index + 3] << 16)
                           | ((uint32_t)buf[index + 4] << 8) | buf[index + 5];
            uint32_t dest_adr = ((uint32_t)buf[index + 6] << 24) | ((uint32_t)buf[index + 7] << 16)
                                | ((uint32_t)buf[index + 8] << 8) | buf[index + 9];
            index += 8;

            if (dest_adr == 0x00000078)
                *irq_offset = adr;
        }
    }

    *cia_value = (uint16_t)((cia_hi << 8) | cia_lo);

    return (*irq_offset != 0) && (*irq_offset < (uint32_t)buf_len) && (*cia_value != 0);
}

static bool find_sub_song_info(const uint8_t* buf, int buf_len, long module_length, uint32_t irq_offset,
                               uint32_t* sub_song_offset) {
    int index;
    *sub_song_offset = 0;

    // Find where the global address is set into A0 (the whole 4-byte instruction in the buffer: a damaged IRQ offset
    // can be odd)
    for (index = (int)irq_offset; index < (buf_len - 3); index += 2) {
        if ((buf[index] == 0x41) && (buf[index + 1] == 0xfa))
            break;
    }

    if (index >= (buf_len - 3))
        return false;

    uint32_t global_offset = (uint32_t)(((buf[index + 2] << 8) | buf[index + 3]) + index + 2);
    index += 4;

    if (global_offset >= (uint32_t)module_length)
        return false;

    // Find where the sub-song is initialized
    for (; index < (buf_len - 12); index += 2) {
        if ((buf[index] == 0x4e) && ((buf[index + 1] == 0x73) || (buf[index + 1] == 0x75)))
            return false;

        if ((buf[index] == 0x02) && (buf[index + 1] == 0x40) && (buf[index + 2] == 0x00) && (buf[index + 3] == 0x0f)
            && (buf[index + 4] == 0x53) && (buf[index + 5] == 0x40) && (buf[index + 6] == 0xe9)
            && (buf[index + 7] == 0x48) && (buf[index + 8] == 0x47) && (buf[index + 9] == 0xf0))
            break;
    }

    if (index >= (buf_len - 12))
        return false;

    *sub_song_offset
        = global_offset + (uint32_t)((buf[index + 10] << 8) | buf[index + 11]) + AMIGA_EXECUTABLE_HUNK_SIZE;

    return *sub_song_offset < (uint32_t)module_length;
}

static bool find_instrument_and_arpeggio_offsets(const uint8_t* buf, int buf_len, long module_length,
                                                 uint32_t* instr_offset, uint32_t* arp_offset) {
    int index;
    *instr_offset = 0;
    *arp_offset = 0;

    for (index = HEADER_SIZE; index < (buf_len - 4); index += 2) {
        if ((buf[index] == 0x0c) && (buf[index + 1] == 0x12) && (buf[index + 2] == 0x00)) {
            if (buf[index + 3] == 0x82) {
                for (; index < (buf_len - 4); index += 2) {
                    if ((buf[index] == 0x49) && (buf[index + 1] == 0xfa)) {
                        *instr_offset = (uint32_t)(((buf[index + 2] << 8) | buf[index + 3]) + index + 2)
                                        + AMIGA_EXECUTABLE_HUNK_SIZE;
                        break;
                    }
                }
            }

            if (buf[index + 3] == 0x80) {
                for (; index < (buf_len - 4); index += 2) {
                    if ((buf[index] == 0x49) && (buf[index + 1] == 0xfa)) {
                        *arp_offset = (uint32_t)(((buf[index + 2] << 8) | buf[index + 3]) + index + 2)
                                      + AMIGA_EXECUTABLE_HUNK_SIZE;
                        break;
                    }
                }
            }
        }

        if ((*instr_offset != 0) && (*arp_offset != 0))
            break;
    }

    if (index >= (buf_len - 4))
        return false;

    return (*instr_offset < (uint32_t)module_length) && (*arp_offset < (uint32_t)module_length);
}

static void enable_track_features(const uint8_t* buf, int buf_len, bool* clear_adsr_on_portamento) {
    int index;
    *clear_adsr_on_portamento = false;

    for (index = HEADER_SIZE; index < (buf_len - 4); index += 2) {
        if ((buf[index] == 0x0c) && (buf[index + 1] == 0x12) && (buf[index + 2] == 0x00)) {
            if (buf[index + 3] == 0x81) {
                if (index < (buf_len - 10)) {
                    if ((buf[index + 8] == 0x42) && (buf[index + 9] == 0x68))
                        *clear_adsr_on_portamento = true;
                }
            }
        }
    }
}

// FindEffectByteCount

static int find_effect_byte_count(uint8_t effect) {
    if (effect < 128)
        return 1;

    switch (effect) {
        case 0x80: // SetArpeggio
        case 0x82: // SetInstrument
        case 0x84: // ChangeAdsrSpeed
            return 1;
        case 0x81: // SetPortamento
            return 3;
        case 0x83: // EndSong
        case 0x85: // EndSong2
        case 0xff: // EndOfTrack
            return 0;
    }

    return 0; // Unknown effect
}

// Loading

// Load sub-song track offsets
static uint32_t* load_sub_song_info(RkReader* r, int* num_sub_songs, uint32_t sub_song_offset, size_t module_length) {
    reader_seek(r, sub_song_offset);

    int count = *num_sub_songs;
    uint32_t* offsets = (uint32_t*)calloc(count * 4, sizeof(uint32_t));
    if (!offsets)
        return nullptr;

    for (int i = 0; i < count; i++) {
        for (int j = 0; j < 4; j++) {
            offsets[i * 4 + j] = reader_read_b_u32(r) + AMIGA_EXECUTABLE_HUNK_SIZE;

            if (offsets[i * 4 + j] >= module_length) {
                *num_sub_songs = i;
                return offsets;
            }
        }

        if (reader_eof(r)) {
            free(offsets);
            return nullptr;
        }
    }

    return offsets;
}

// Load a single track list
static RkTrack* load_single_track_list(RkReader* r, int* out_count) {
    int capacity = 16;
    RkTrack* list = (RkTrack*)malloc(capacity * sizeof(RkTrack));
    if (!list)
        return nullptr;
    int count = 0;

    for (;;) {
        int32_t track_offset = reader_read_b_i32(r);
        if (track_offset < 0)
            break;

        reader_skip(r, 2);
        int16_t transpose = reader_read_b_i16(r);

        reader_skip(r, 2);
        uint16_t repeat_times = reader_read_b_u16(r);

        if (reader_eof(r)) {
            free(list);
            return nullptr;
        }

        if (count >= capacity) {
            capacity *= 2;
            RkTrack* new_list = (RkTrack*)realloc(list, capacity * sizeof(RkTrack));
            if (!new_list) {
                free(list);
                return nullptr;
            }
            list = new_list;
        }

        list[count].track_number = file_offset_of(track_offset);
        list[count].transpose = transpose;
        list[count].number_of_repeat_times = repeat_times;
        count++;
    }

    *out_count = count;
    return list;
}

// Load all track lists for each sub-song
static bool load_track_lists(RkModule* m, RkReader* r, int num_sub_songs, const uint32_t* offsets) {
    m->sub_songs = (RkSongInfo*)calloc(num_sub_songs, sizeof(RkSongInfo));
    if (!m->sub_songs)
        return false;
    m->num_sub_songs = num_sub_songs;

    for (int i = 0; i < num_sub_songs; i++) {
        for (int j = 0; j < 4; j++) {
            uint32_t track_list_offset = offsets[i * 4 + j];
            reader_seek(r, track_list_offset);

            int track_count = 0;
            RkTrack* track_list = load_single_track_list(r, &track_count);
            if (!track_list)
                return false;

            // A voice starts on the first entry: an empty list is damaged
            if (track_count == 0) {
                free(track_list);
                return false;
            }

            m->sub_songs[i].positions[j].tracks = track_list;
            m->sub_songs[i].positions[j].track_count = track_count;
        }
    }

    return true;
}

// Load a single track
static uint8_t* load_single_track(RkReader* r, int* out_len) {
    int capacity = 64;
    uint8_t* bytes = (uint8_t*)malloc(capacity);
    if (!bytes)
        return nullptr;
    int count = 0;

    for (;;) {
        uint8_t byt = reader_read_u8(r);
        if (reader_eof(r)) {
            free(bytes);
            return nullptr;
        }

        if (count >= capacity) {
            if (capacity > INT_MAX / 2) {
                free(bytes);
                return nullptr;
            }
            capacity *= 2;
            uint8_t* nb = (uint8_t*)realloc(bytes, capacity);
            if (!nb) {
                free(bytes);
                return nullptr;
            }
            bytes = nb;
        }
        bytes[count++] = byt;

        if (byt == 0xff)
            break;

        int effect_count = find_effect_byte_count(byt);

        for (; effect_count > 0; effect_count--) {
            if (count >= capacity) {
                if (capacity > INT_MAX / 2) {
                    free(bytes);
                    return nullptr;
                }
                capacity *= 2;
                uint8_t* nb = (uint8_t*)realloc(bytes, capacity);
                if (!nb) {
                    free(bytes);
                    return nullptr;
                }
                bytes = nb;
            }
            bytes[count++] = reader_read_u8(r);
        }
    }

    *out_len = count;
    return bytes;
}

// Load all tracks and fix track offsets
static bool load_tracks(RkModule* m, RkReader* r) {
    // Collect unique track offsets
    int max_tracks = 0;
    for (int i = 0; i < m->num_sub_songs; i++)
        for (int j = 0; j < 4; j++)
            max_tracks += m->sub_songs[i].positions[j].track_count;

    // Collect unique offsets
    int* unique_offsets = (int*)calloc(max_tracks, sizeof(int));
    int unique_count = 0;

    for (int i = 0; i < m->num_sub_songs; i++) {
        for (int j = 0; j < 4; j++) {
            RkPositionList* pl = &m->sub_songs[i].positions[j];
            for (int k = 0; k < pl->track_count; k++) {
                int off = pl->tracks[k].track_number;
                bool found = false;
                for (int u = 0; u < unique_count; u++) {
                    if (unique_offsets[u] == off) {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    unique_offsets[unique_count++] = off;
            }
        }
    }

    // Sort offsets
    for (int i = 0; i < unique_count - 1; i++)
        for (int j = i + 1; j < unique_count; j++)
            if (unique_offsets[i] > unique_offsets[j]) {
                int tmp = unique_offsets[i];
                unique_offsets[i] = unique_offsets[j];
                unique_offsets[j] = tmp;
            }

    // Load each track
    m->num_tracks = unique_count;
    m->tracks = (uint8_t**)calloc(unique_count, sizeof(uint8_t*));
    if (!m->tracks) {
        free(unique_offsets);
        return false;
    }

    int* track_lengths = (int*)calloc(unique_count, sizeof(int));

    for (int i = 0; i < unique_count; i++) {
        reader_seek(r, unique_offsets[i]);
        int len = 0;
        m->tracks[i] = load_single_track(r, &len);
        if (!m->tracks[i]) {
            free(unique_offsets);
            free(track_lengths);
            return false;
        }
        track_lengths[i] = len;
    }

    // Convert offsets to indices in position lists
    for (int i = 0; i < m->num_sub_songs; i++) {
        for (int j = 0; j < 4; j++) {
            RkPositionList* pl = &m->sub_songs[i].positions[j];
            for (int k = 0; k < pl->track_count; k++) {
                int off = pl->tracks[k].track_number;
                for (int u = 0; u < unique_count; u++) {
                    if (unique_offsets[u] == off) {
                        pl->tracks[k].track_number = u;
                        break;
                    }
                }
            }
        }
    }

    m->track_offsets = unique_offsets;
    m->track_lengths = track_lengths;
    return true;
}

// Find max instrument and arpeggio counts
static void find_max_used(RkModule* m, int* instr_count, int* arp_count) {
    *instr_count = 0;
    *arp_count = 0;

    for (int t = 0; t < m->num_tracks; t++) {
        uint8_t* track = m->tracks[t];
        int len = 0;
        // Find length by scanning for 0xff
        for (int i = 0;; i++) {
            if (track[i] == 0xff) {
                len = i + 1;
                break;
            }
            i += find_effect_byte_count(track[i]);
        }

        for (int i = 0; i < len; i++) {
            uint8_t effect = track[i];

            if (effect == 0x82) { // SetInstrument
                int val = track[i + 1] + 1;
                if (val > *instr_count)
                    *instr_count = val;
            } else if (effect == 0x80) { // SetArpeggio
                int val = track[i + 1] + 1;
                if (val > *arp_count)
                    *arp_count = val;
            }

            i += find_effect_byte_count(effect);
        }
    }
}

// Load arpeggios
static bool load_arpeggios(RkModule* m, RkReader* r, uint32_t arp_offset, int arp_count) {
    reader_seek(r, arp_offset);

    m->num_arpeggios = arp_count;
    m->arpeggios = (int8_t**)calloc(arp_count, sizeof(int8_t*));
    if (!m->arpeggios)
        return false;

    for (int i = 0; i < arp_count; i++) {
        m->arpeggios[i] = (int8_t*)malloc(12);
        if (!m->arpeggios[i])
            return false;
        if (reader_read_signed(r, m->arpeggios[i], 12) != 12)
            return false;
    }

    return true;
}

// Load instruments
static bool load_instruments(RkModule* m, RkReader* r, uint32_t instr_offset, int instr_count) {
    // Every voice starts on instrument 0, so a module whose tracks select none is damaged
    if (instr_count == 0)
        return false;

    reader_seek(r, instr_offset);

    m->num_instruments = instr_count;
    m->instruments = (RkInstrument*)calloc(instr_count, sizeof(RkInstrument));
    if (!m->instruments)
        return false;

    for (int i = 0; i < instr_count; i++) {
        RkInstrument* instr = &m->instruments[i];

        instr->sample_number = file_offset_of(reader_read_b_i32(r));
        instr->vibrato_number = file_offset_of(reader_read_b_i32(r));
        instr->type = reader_read_u8(r) == 0 ? RK_INSTRUMENT_SYNTHESIS : RK_INSTRUMENT_SAMPLE;
        instr->phase_speed = reader_read_u8(r);
        instr->phase_length_in_words = reader_read_u8(r);
        instr->vibrato_speed = reader_read_u8(r);
        instr->vibrato_depth = reader_read_u8(r);
        instr->vibrato_delay = reader_read_u8(r);

        for (int j = 0; j < 4; j++)
            instr->adsr[j].point = reader_read_u8(r);

        for (int j = 0; j < 4; j++)
            instr->adsr[j].increment = reader_read_u8(r);

        instr->phase_value = reader_read_i8(r);
        instr->phase_direction = reader_read_i8(r) < 0;
        instr->phase_position = reader_read_u8(r);

        if (reader_eof(r))
            return false;

        reader_skip(r, 7);

        if (instr->vibrato_speed == 0)
            instr->vibrato_number = -1;
        else if (instr->vibrato_number < 0)
            return false; // -1 means no vibrato table; a used table must be in the file
    }

    return true;
}

// Load vibratos
static bool load_vibratos(RkModule* m, RkReader* r) {
    // Collect unique vibrato offsets
    int max_entries = m->num_instruments;
    int* unique_offsets = (int*)calloc(max_entries, sizeof(int));
    int unique_count = 0;

    for (int i = 0; i < m->num_instruments; i++) {
        if (m->instruments[i].vibrato_number == -1)
            continue;
        int off = m->instruments[i].vibrato_number;
        bool found = false;
        for (int u = 0; u < unique_count; u++) {
            if (unique_offsets[u] == off) {
                found = true;
                break;
            }
        }
        if (!found)
            unique_offsets[unique_count++] = off;
    }

    // Sort
    for (int i = 0; i < unique_count - 1; i++)
        for (int j = i + 1; j < unique_count; j++)
            if (unique_offsets[i] > unique_offsets[j]) {
                int tmp = unique_offsets[i];
                unique_offsets[i] = unique_offsets[j];
                unique_offsets[j] = tmp;
            }

    // Load each vibrato table
    m->num_vibratos = unique_count;
    m->vibratos = (int8_t**)calloc(unique_count, sizeof(int8_t*));
    m->vibrato_lengths = (int*)calloc(unique_count, sizeof(int));
    if (!m->vibratos || !m->vibrato_lengths) {
        free(unique_offsets);
        return false;
    }

    for (int i = 0; i < unique_count; i++) {
        reader_seek(r, unique_offsets[i]);

        uint32_t table_offset = reader_read_b_u32(r) + AMIGA_EXECUTABLE_HUNK_SIZE;
        uint16_t length = reader_read_b_u16(r);

        if (reader_eof(r)) {
            free(unique_offsets);
            return false;
        }

        // Keep at least one byte so a position read stays valid for an empty table
        int8_t* table = (int8_t*)calloc(length ? (size_t)length * 2 : 1, 1);
        if (!table) {
            free(unique_offsets);
            return false;
        }

        reader_seek(r, table_offset);

        if (reader_read_signed(r, table, length * 2) != (size_t)(length * 2)) {
            free(table);
            free(unique_offsets);
            return false;
        }

        m->vibratos[i] = table;
        m->vibrato_lengths[i] = length ? length * 2 : 1;
    }

    // Convert offsets to indices in instruments
    for (int i = 0; i < m->num_instruments; i++) {
        if (m->instruments[i].vibrato_number == -1)
            continue;
        int off = m->instruments[i].vibrato_number;
        for (int u = 0; u < unique_count; u++) {
            if (unique_offsets[u] == off) {
                m->instruments[i].vibrato_number = u;
                break;
            }
        }
    }

    free(unique_offsets);
    return true;
}

// Load sample info
static bool load_samples(RkModule* m, RkReader* r) {
    // Collect unique sample info offsets
    int max_entries = m->num_instruments;
    int* unique_offsets = (int*)calloc(max_entries, sizeof(int));
    int unique_count = 0;

    for (int i = 0; i < m->num_instruments; i++) {
        int off = m->instruments[i].sample_number;
        bool found = false;
        for (int u = 0; u < unique_count; u++) {
            if (unique_offsets[u] == off) {
                found = true;
                break;
            }
        }
        if (!found)
            unique_offsets[unique_count++] = off;
    }

    // Sort
    for (int i = 0; i < unique_count - 1; i++)
        for (int j = i + 1; j < unique_count; j++)
            if (unique_offsets[i] > unique_offsets[j]) {
                int tmp = unique_offsets[i];
                unique_offsets[i] = unique_offsets[j];
                unique_offsets[j] = tmp;
            }

    m->num_samples = unique_count;
    m->samples = (RkSample*)calloc(unique_count, sizeof(RkSample));
    if (!m->samples) {
        free(unique_offsets);
        return false;
    }

    for (int i = 0; i < unique_count; i++) {
        reader_seek(r, unique_offsets[i]);

        m->samples[i].sample_number = file_offset_of(reader_read_b_i32(r));
        m->samples[i].length_in_words = reader_read_b_u16(r);
        m->samples[i].phase_index = reader_read_b_u16(r);

        if (reader_eof(r)) {
            free(unique_offsets);
            return false;
        }
    }

    // Convert offsets to indices
    for (int i = 0; i < m->num_instruments; i++) {
        int off = m->instruments[i].sample_number;
        for (int u = 0; u < unique_count; u++) {
            if (unique_offsets[u] == off) {
                m->instruments[i].sample_number = u;
                break;
            }
        }
    }

    free(unique_offsets);
    return true;
}

// Load sample data
static bool load_sample_data(RkModule* m, RkReader* r) {
    int max_entries = m->num_samples;
    int* unique_offsets = (int*)calloc(max_entries, sizeof(int));
    int unique_count = 0;

    for (int i = 0; i < m->num_samples; i++) {
        int off = m->samples[i].sample_number;
        bool found = false;
        for (int u = 0; u < unique_count; u++) {
            if (unique_offsets[u] == off) {
                found = true;
                break;
            }
        }
        if (!found)
            unique_offsets[unique_count++] = off;
    }

    // Sort
    for (int i = 0; i < unique_count - 1; i++)
        for (int j = i + 1; j < unique_count; j++)
            if (unique_offsets[i] > unique_offsets[j]) {
                int tmp = unique_offsets[i];
                unique_offsets[i] = unique_offsets[j];
                unique_offsets[j] = tmp;
            }

    m->num_sample_data = unique_count;
    m->sample_data = (int8_t**)calloc(unique_count, sizeof(int8_t*));
    if (!m->sample_data) {
        free(unique_offsets);
        return false;
    }

    for (int i = 0; i < m->num_samples; i++) {
        int off = m->samples[i].sample_number;
        int idx = -1;
        for (int u = 0; u < unique_count; u++) {
            if (unique_offsets[u] == off) {
                idx = u;
                break;
            }
        }

        if (m->sample_data[idx] == nullptr) {
            // Descriptors can share data with different lengths: hold the longest, so each one stays inside it
            int length = 0;
            for (int j = 0; j < m->num_samples; j++) {
                if (m->samples[j].sample_number == off && m->samples[j].length_in_words * 2 > length)
                    length = m->samples[j].length_in_words * 2;
            }
            reader_seek(r, off);
            m->sample_data[idx] = reader_read_sample_data(r, length);
            if (!m->sample_data[idx]) {
                free(unique_offsets);
                return false;
            }
        }
    }

    // Convert offsets to indices
    for (int i = 0; i < m->num_samples; i++) {
        int off = m->samples[i].sample_number;
        for (int u = 0; u < unique_count; u++) {
            if (unique_offsets[u] == off) {
                m->samples[i].sample_number = u;
                break;
            }
        }
    }

    free(unique_offsets);
    return true;
}

// Channel operations (IChannel equivalents)

static void channel_mute(RkChannel* ch) {
    ch->active = false;
    ch->play_pending = false;
}

static void channel_play_sample(RkChannel* ch, int8_t* data, uint32_t length) {
    ch->sample_data = data;
    ch->sample_length = length;
    ch->active = true;
    ch->play_pending = true;
    ch->loop_length = 0;
}

static void channel_set_loop(RkChannel* ch, uint32_t length) {
    ch->loop_length = length;
}

static void channel_set_amiga_period(RkChannel* ch, uint32_t period) {
    ch->period = (uint16_t)period;
}

static void channel_set_amiga_volume(RkChannel* ch, uint16_t vol) {
    ch->volume = vol;
}

// PlaySample - prepare hardware staging

static void play_sample(RkVoiceInfo* vi, RkModule* m) {
    RkInstrument* instr = vi->instrument;
    RkSample* sample = &m->samples[instr->sample_number];
    int8_t* data = m->sample_data[sample->sample_number];

    vi->set_hardware = true;
    vi->sample_number = vi->instrument_number;
    vi->sample_data = data;
    vi->sample_length = sample->length_in_words * 2U;
    vi->set_loop = (instr->type == RK_INSTRUMENT_SYNTHESIS);

    channel_mute(&m->channels[vi->channel_number]);
}

// Effect handlers

static void do_effect_phasing(RkVoiceInfo* vi, RkInstrument* instr, RkModule* m) {
    if (instr->phase_speed != 0) {
        if (vi->phase_speed_counter == 0) {
            vi->phase_speed_counter = (uint8_t)(instr->phase_speed - 1);

            RkSample* sample = &m->samples[instr->sample_number];
            int8_t* data = m->sample_data[sample->sample_number];

            int phase_start = sample->phase_index - instr->phase_length_in_words;
            int phase_index = phase_start + instr->phase_position;

            if (phase_index >= 0 && phase_index < (int)(sample->length_in_words * 2))
                data[phase_index] = instr->phase_value;

            if (instr->phase_direction) {
                instr->phase_position--;

                if (instr->phase_position == 0) {
                    instr->phase_direction = !instr->phase_direction;
                    instr->phase_value = (int8_t)~instr->phase_value;
                }
            } else {
                instr->phase_position++;

                if ((instr->phase_length_in_words * 2) == instr->phase_position) {
                    instr->phase_direction = !instr->phase_direction;
                    instr->phase_value = (int8_t)~instr->phase_value;
                }
            }
        } else {
            vi->phase_speed_counter--;
        }
    }
}

static void do_effect_portamento(RkVoiceInfo* vi, RkChannel* ch) {
    if (vi->portamento_increment != 0) {
        uint16_t period = vi->period;

        if (period <= vi->portamento_end_period) {
            period += vi->portamento_increment;
            if (period > vi->portamento_end_period)
                period = vi->portamento_end_period;
        } else {
            period -= vi->portamento_increment;
            if (period < vi->portamento_end_period)
                period = vi->portamento_end_period;
        }

        vi->period = period;
        channel_set_amiga_period(ch, period);
    }
}

static void do_effect_arpeggio(RkVoiceInfo* vi, RkChannel* ch) {
    // Only do arpeggio if no portamento is active
    if (vi->portamento_increment == 0) {
        int transposed_note = vi->arpeggio_values[vi->arpeggio_position] + vi->current_note + vi->transpose;
        if (transposed_note >= 70)
            transposed_note = 69;
        if (transposed_note < 0)
            transposed_note = 0;

        vi->period = rk_periods[transposed_note];
        channel_set_amiga_period(ch, vi->period);

        if (vi->arpeggio_position == 0)
            vi->arpeggio_position = 11;
        else
            vi->arpeggio_position--;
    }
}

static void do_effect_vibrato(RkVoiceInfo* vi, RkChannel* ch, RkInstrument* instr, RkModule* m) {
    if (instr->vibrato_speed != 0) {
        if (vi->vibrato_delay == 0) {
            int8_t* vibrato_table = m->vibratos[instr->vibrato_number];
            uint16_t period = (uint16_t)(vibrato_table[vi->vibrato_position] * instr->vibrato_depth + vi->period);
            channel_set_amiga_period(ch, period);

            int new_position = (int)vi->vibrato_position - instr->vibrato_speed;
            if (new_position < 0)
                new_position = m->vibrato_lengths[instr->vibrato_number] - 1;

            vi->vibrato_position = (uint16_t)new_position;
        } else {
            vi->vibrato_delay--;
        }
    }
}

static void do_effect_adsr(RkVoiceInfo* vi, RkChannel* ch, RkInstrument* instr, RkModule* m) {
    vi->adsr_speed_counter--;

    if (vi->adsr_speed_counter < 0) {
        vi->adsr_speed_counter = (int8_t)vi->adsr_speed;

        RkAdsrPoint* point = &instr->adsr[vi->adsr_state];

        int volume = vi->volume;

        if (volume > point->point) {
            volume -= point->increment;

            if (volume <= point->point) {
                volume = point->point;
                if (vi->adsr_state < 3)
                    vi->adsr_state++;
            }
        } else {
            volume += point->increment;

            if (volume >= m->global_volume) {
                volume = m->global_volume;
                if (vi->adsr_state < 3)
                    vi->adsr_state++;
            } else if (volume >= point->point) {
                volume = point->point;
                if (vi->adsr_state < 3)
                    vi->adsr_state++;
            }
        }

        vi->volume = (uint8_t)volume;
        channel_set_amiga_volume(ch, (uint16_t)(volume & 0x3f));
    }
}

static void effect_handler(RkVoiceInfo* vi, RkModule* m) {
    RkInstrument* instr = vi->instrument;
    RkChannel* ch = &m->channels[vi->channel_number];

    do_effect_phasing(vi, instr, m);
    do_effect_portamento(vi, ch);
    do_effect_arpeggio(vi, ch);
    do_effect_vibrato(vi, ch, instr, m);
    do_effect_adsr(vi, ch, instr, m);
}

// Track parsing

static void parse_track_arpeggio(RkVoiceInfo* vi, RkModule* m) {
    uint8_t arp_number = vi->track_data[vi->track_data_position + 1];
    vi->track_data_position += 2;

    vi->arpeggio_values = m->arpeggios[arp_number];
}

static void parse_track_portamento(RkVoiceInfo* vi, RkModule* m) {
    uint8_t end_note = vi->track_data[vi->track_data_position + 1];
    uint8_t increment = vi->track_data[vi->track_data_position + 2];
    uint8_t wait_counter = vi->track_data[vi->track_data_position + 3];
    vi->track_data_position += 4;

    int transposed_note = end_note + vi->transpose;
    if (transposed_note >= 70)
        transposed_note = 69;
    if (transposed_note < 0)
        transposed_note = 0;

    if (m->clear_adsr_state_on_portamento)
        vi->adsr_state = 0;

    vi->portamento_end_period = rk_periods[transposed_note];
    vi->portamento_increment = increment;
    vi->wait_counter = (uint8_t)(wait_counter * 4 - 1);
}

static void parse_track_instrument(RkVoiceInfo* vi, RkModule* m) {
    uint8_t instr_number = vi->track_data[vi->track_data_position + 1];
    vi->track_data_position += 2;

    vi->volume = 0;
    vi->adsr_state = 0;
    vi->vibrato_position = 0;

    vi->instrument = &m->instruments[instr_number];
    vi->instrument_number = instr_number;

    play_sample(vi, m);
}

static void parse_track_end_song(RkModule* m) {
    m->setup_new_sub_song = true;
    m->sub_song_number = 0;

    m->loop_count++;
}

static void parse_track_change_adsr_speed(RkVoiceInfo* vi) {
    uint8_t speed = vi->track_data[vi->track_data_position + 1];
    vi->track_data_position += 2;

    vi->adsr_speed = speed;
}

// The song has looped once every channel has wrapped its position list since the last loop.
static void note_channel_end(RkModule* m, int channel) {
    m->end_reached[channel] = true;
    for (int i = 0; i < 4; i++) {
        if (!m->end_reached[i])
            return;
    }
    m->loop_count++;
    memset(m->end_reached, 0, sizeof(m->end_reached));
}

static void parse_track_end_of_track(RkVoiceInfo* vi, RkModule* m) {
    if (vi->track_repeat_counter == 0) {
        vi->track_list_position++;

        if (vi->track_list_position == vi->position_list->track_count) {
            vi->track_list_position = 0;
            note_channel_end(m, vi->channel_number);
        }

        RkTrack* track = &vi->position_list->tracks[vi->track_list_position];

        vi->track_data = m->tracks[track->track_number];
        vi->track_data_position = 0;
        vi->transpose = track->transpose;
        vi->track_repeat_counter = (uint16_t)(track->number_of_repeat_times - 1);
    } else {
        vi->track_repeat_counter--;
        vi->track_data_position = 0;
        vi->transpose = vi->position_list->tracks[vi->track_list_position].transpose;

        // Battle Squadron title has a track that repeats more than 9000 times
        if (vi->track_repeat_counter > 9000) {
            note_channel_end(m, vi->channel_number);
        }
    }
}

static bool parse_track_new_note(RkVoiceInfo* vi, RkModule* m) {
    uint8_t note = vi->track_data[vi->track_data_position];
    uint8_t wait_count = vi->track_data[vi->track_data_position + 1];
    vi->track_data_position += 2;

    int transposed_note = note + vi->transpose;
    if (transposed_note >= 70)
        transposed_note = 69;
    if (transposed_note < 0)
        transposed_note = 0;

    vi->current_note = note;
    vi->period = rk_periods[transposed_note];

    if (wait_count == 0)
        return true;

    vi->wait_counter = (uint8_t)(wait_count * 4 - 1);
    vi->adsr_state = 0;

    play_sample(vi, m);
    vi->started_period = vi->period;
    vi->started_instrument = vi->instrument_number;

    return false;
}

static void parse_track_data(RkVoiceInfo* vi, RkModule* m) {
    vi->portamento_increment = 0;

    if (vi->instrument->vibrato_delay != 0)
        vi->vibrato_delay = (uint8_t)(vi->instrument->vibrato_delay * 4 - 1);

    bool take_one_more = true;
    // A track list without a note never yields; stop such a scan, it goes on next tick. The bound is far above a real
    // song (a 65536-times repeat of an empty track still fits).
    int commands_left = RK_MAX_COMMANDS_PER_TICK;

    do {
        if (--commands_left < 0)
            break;

        if (vi->track_data[vi->track_data_position] == RK_EFFECT_SET_ARPEGGIO) {
            parse_track_arpeggio(vi, m);
        }

        if (vi->track_data[vi->track_data_position] == RK_EFFECT_SET_PORTAMENTO) {
            parse_track_portamento(vi, m);
            take_one_more = false;
            continue;
        }

        if (vi->track_data[vi->track_data_position] == RK_EFFECT_SET_INSTRUMENT)
            parse_track_instrument(vi, m);

        if (vi->track_data[vi->track_data_position] == RK_EFFECT_END_SONG) {
            parse_track_end_song(m);
            return;
        }

        if (vi->track_data[vi->track_data_position] == RK_EFFECT_CHANGE_ADSR_SPEED)
            parse_track_change_adsr_speed(vi);

        if (vi->track_data[vi->track_data_position] == RK_EFFECT_END_SONG2) {
            parse_track_end_song(m);
            return;
        }

        if (vi->track_data[vi->track_data_position] >= 0x80)
            parse_track_end_of_track(vi, m);
        else
            take_one_more = parse_track_new_note(vi, m);
    } while (take_one_more);

    channel_set_amiga_period(&m->channels[vi->channel_number], vi->period);
}

// Process voice

static void process_voice(RkVoiceInfo* vi, RkModule* m) {
    RkChannel* ch = &m->channels[vi->channel_number];

    if (vi->set_hardware) {
        channel_play_sample(ch, vi->sample_data, vi->sample_length);

        if (vi->set_loop)
            channel_set_loop(ch, vi->sample_length);

        vi->set_hardware = false;
    }

    if (vi->wait_counter == 0)
        parse_track_data(vi, m);
    else {
        vi->wait_counter--;
        effect_handler(vi, m);
    }
}

// Initialize

static void initialize_voice(RkModule* m, int voice) {
    RkPositionList* pl = &m->sub_songs[m->sub_song_number - 1].positions[voice];
    RkVoiceInfo* vi = &m->voices[voice];

    memset(vi, 0, sizeof(RkVoiceInfo));

    vi->channel_number = voice;
    vi->position_list = pl;
    vi->track_list_position = 0;
    vi->track_data = m->tracks[pl->tracks[0].track_number];
    vi->track_data_position = 0;
    vi->track_repeat_counter = (uint16_t)(pl->tracks[0].number_of_repeat_times - 1);
    vi->wait_counter = 0;

    vi->instrument = &m->instruments[0];
    vi->instrument_number = 0;

    vi->arpeggio_values = m->default_arpeggio;
    vi->arpeggio_position = 0;

    vi->current_note = 0;
    vi->transpose = pl->tracks[0].transpose;
    vi->period = 0;

    vi->portamento_end_period = 0;
    vi->portamento_increment = 0;

    vi->vibrato_delay = 0;
    vi->vibrato_position = 0;

    vi->adsr_state = 0;
    vi->adsr_speed = 0;
    vi->adsr_speed_counter = 0;
    vi->volume = 0;

    vi->phase_speed_counter = 0;

    vi->set_hardware = false;
    vi->sample_number = 0;
    vi->sample_data = nullptr;
    vi->sample_length = 0;
    vi->set_loop = false;

    vi->started_period = 0;
    vi->started_instrument = -1;
}

static void switch_sub_song(RkModule* m) {
    if (m->sub_song_number == 0)
        m->sub_song_number = (uint16_t)(m->current_song + 1);

    for (int i = 0; i < 4; i++)
        initialize_voice(m, i);

    m->setup_new_sub_song = false;
    m->sub_song_number = 0;
}

static void initialize_sound(RkModule* m, int song_number) {
    m->global_volume = 0x3f;
    m->setup_new_sub_song = true;
    m->sub_song_number = (uint16_t)(song_number + 1);
    m->loop_count = 0;

    for (int i = 0; i < 4; i++) {
        m->end_reached[i] = false;
        initialize_voice(m, i);
    }
}

// Play tick

static void play_tick(RkModule* m) {
    if (m->setup_new_sub_song)
        switch_sub_song(m);
    else {
        for (int i = 0; i < 4; i++)
            process_voice(&m->voices[i], m);
    }
}

// Voice sync

// Hand the registers the tick wrote to the voice. Re-stopping a one-shot that already ended is harmless, so no
// read-back is needed.
static void sync_voice(RkModule* m, int voice) {
    RkChannel* c = &m->channels[voice];
    const uint32_t index = (uint32_t)voice;

    if (!c->active || c->sample_data == nullptr || c->sample_length == 0) {
        nuade_voice_stop(m->nuade_voices, index);
    } else if (c->play_pending) {
        const NuadeSample sample = { .data = c->sample_data,
                                     .format = NuadeSampleFormat_S8,
                                     .length = c->sample_length,
                                     .loop_start = 0,
                                     .loop_length = c->loop_length };
        nuade_voice_play(m->nuade_voices, index, &sample, 0);
    }
    c->play_pending = false;

    const bool silent = c->period == 0;
    nuade_voice_set_pitch(m->nuade_voices, index, silent ? 0.0 : nuade_paula_period_hz(c->period));
    nuade_voice_set_volume(m->nuade_voices, index, silent ? 0.0f : (float)c->volume / 64.0f);
    // Amiga panning: Paula channels 0 and 3 are left, 1 and 2 right
    nuade_voice_set_pan(m->nuade_voices, index, (voice == 0 || voice == 3) ? -1.0f : 1.0f);
}

bool rk_tick(RkModule* module) {
    const uint32_t loops = module->loop_count;
    play_tick(module);
    for (int i = 0; i < RK_CHANNELS; i++)
        sync_voice(module, i);
    return module->loop_count != loops;
}

uint32_t rk_tick_length(const RkModule* module) {
    return module->cia_value;
}

// Public API

bool rk_probe(const uint8_t* data, size_t size) {
    if (!data || size < 40 + 23)
        return false;
    const uint32_t hunk_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
    return hunk_id == 0x3f3 && memcmp(data + 40, "RON_KLAREN_SOUNDMODULE!", 23) == 0;
}

RkModule* rk_create(const uint8_t* data, size_t size) {
    if (!data || size < (size_t)AMIGA_EXECUTABLE_HUNK_SIZE + MIN_FILE_SIZE)
        return nullptr;

    // We need to read the file including the Amiga executable hunk
    // Check for Amiga executable hunks
    if (size < 44)
        return nullptr;

    uint32_t hunk_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
    if (hunk_id != 0x3f3)
        return nullptr;

    // Check identifier at offset 40
    if (memcmp(data + 40, "RON_KLAREN_SOUNDMODULE!", 23) != 0)
        return nullptr;

    // Load search buffer (player code starting after hunk header)
    int search_len = MIN_FILE_SIZE;
    uint8_t* search_buf = (uint8_t*)malloc(search_len);
    if (!search_buf)
        return nullptr;
    memcpy(search_buf, data + AMIGA_EXECUTABLE_HUNK_SIZE, search_len);

    int num_sub_songs = 0;
    if (!find_number_of_sub_songs(search_buf, search_len, &num_sub_songs)) {
        free(search_buf);
        return nullptr;
    }

    uint32_t irq_offset = 0;
    uint16_t cia_value = 0;
    if (!find_song_speed_and_irq_offset(search_buf, search_len, &irq_offset, &cia_value)) {
        free(search_buf);
        return nullptr;
    }

    uint32_t sub_song_offset = 0;
    if (!find_sub_song_info(search_buf, search_len, size, irq_offset, &sub_song_offset)) {
        free(search_buf);
        return nullptr;
    }

    uint32_t instr_offset = 0, arp_offset = 0;
    if (!find_instrument_and_arpeggio_offsets(search_buf, search_len, size, &instr_offset, &arp_offset)) {
        free(search_buf);
        return nullptr;
    }

    bool clear_adsr_on_portamento = false;
    enable_track_features(search_buf, search_len, &clear_adsr_on_portamento);

    free(search_buf);

    RkModule* m = (RkModule*)calloc(1, sizeof(RkModule));
    if (!m)
        return nullptr;

    // Keep original data for export
    m->original_data = (uint8_t*)malloc(size);
    if (m->original_data) {
        memcpy(m->original_data, data, size);
        m->original_size = size;
    }
    m->cia_value = cia_value;
    m->clear_adsr_state_on_portamento = clear_adsr_on_portamento;
    memset(m->default_arpeggio, 0, sizeof(m->default_arpeggio));

    // Create reader over entire file
    RkReader reader;
    reader_init(&reader, data, size);

    // Load sub-song info
    uint32_t* sub_song_offsets = load_sub_song_info(&reader, &num_sub_songs, sub_song_offset, size);
    if (!sub_song_offsets) {
        rk_destroy(m);
        return nullptr;
    }

    if (!load_track_lists(m, &reader, num_sub_songs, sub_song_offsets)) {
        free(sub_song_offsets);
        rk_destroy(m);
        return nullptr;
    }
    free(sub_song_offsets);

    if (!load_tracks(m, &reader)) {
        rk_destroy(m);
        return nullptr;
    }

    int instr_count = 0, arp_count = 0;
    find_max_used(m, &instr_count, &arp_count);

    if (!load_arpeggios(m, &reader, arp_offset, arp_count)) {
        rk_destroy(m);
        return nullptr;
    }

    if (!load_instruments(m, &reader, instr_offset, instr_count)) {
        rk_destroy(m);
        return nullptr;
    }

    if (!load_vibratos(m, &reader)) {
        rk_destroy(m);
        return nullptr;
    }

    if (!load_samples(m, &reader)) {
        rk_destroy(m);
        return nullptr;
    }

    if (!load_sample_data(m, &reader)) {
        rk_destroy(m);
        return nullptr;
    }

    return m;
}

void rk_destroy(RkModule* module) {
    if (!module)
        return;

    if (module->sub_songs) {
        for (int i = 0; i < module->num_sub_songs; i++) {
            for (int j = 0; j < 4; j++) {
                free(module->sub_songs[i].positions[j].tracks);
            }
        }
        free(module->sub_songs);
    }

    if (module->tracks) {
        for (int i = 0; i < module->num_tracks; i++)
            free(module->tracks[i]);
        free(module->tracks);
    }
    free(module->track_offsets);
    free(module->track_lengths);

    if (module->arpeggios) {
        for (int i = 0; i < module->num_arpeggios; i++)
            free(module->arpeggios[i]);
        free(module->arpeggios);
    }

    if (module->vibratos) {
        for (int i = 0; i < module->num_vibratos; i++)
            free(module->vibratos[i]);
        free(module->vibratos);
    }
    free(module->vibrato_lengths);

    if (module->instruments)
        free(module->instruments);

    if (module->samples)
        free(module->samples);

    if (module->sample_data) {
        for (int i = 0; i < module->num_sample_data; i++)
            free(module->sample_data[i]);
        free(module->sample_data);
    }

    if (module->original_data)
        free(module->original_data);
    free(module);
}

int rk_subsong_count(const RkModule* module) {
    if (!module)
        return 0;
    return module->num_sub_songs;
}

bool rk_start(RkModule* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong < 0 || subsong >= module->num_sub_songs)
        return false;

    module->nuade_voices = voices;
    memset(module->channels, 0, sizeof(module->channels));
    module->current_song = subsong;
    initialize_sound(module, subsong);
    return true;
}

int rk_channel_count(const RkModule* module) {
    (void)module;
    return RK_CHANNELS;
}

uint32_t rk_loop_count(const RkModule* module) {
    return module->loop_count;
}

RkVoiceState rk_voice_state(const RkModule* module, int channel) {
    if (!module || channel < 0 || channel >= RK_CHANNELS) {
        return (RkVoiceState) { .period = 0, .instrument = -1 };
    }
    const RkVoiceInfo* vi = &module->voices[channel];
    return (RkVoiceState) { .period = vi->started_period, .instrument = vi->started_instrument };
}

// Edit API

int rk_get_instrument_count(const RkModule* module) {
    return module ? module->num_instruments : 0;
}

float rk_get_instrument_param(const RkModule* module, int inst, const char* param) {
    if (!module || inst < 0 || inst >= module->num_instruments || !param)
        return -1.0f;
    const RkInstrument* in = &module->instruments[inst];

    if (strcmp(param, "sampleNumber") == 0)
        return (float)in->sample_number;
    if (strcmp(param, "vibratoNumber") == 0)
        return (float)in->vibrato_number;
    if (strcmp(param, "type") == 0)
        return (float)in->type;
    if (strcmp(param, "phaseSpeed") == 0)
        return (float)in->phase_speed;
    if (strcmp(param, "phaseLengthInWords") == 0)
        return (float)in->phase_length_in_words;
    if (strcmp(param, "vibratoSpeed") == 0)
        return (float)in->vibrato_speed;
    if (strcmp(param, "vibratoDepth") == 0)
        return (float)in->vibrato_depth;
    if (strcmp(param, "vibratoDelay") == 0)
        return (float)in->vibrato_delay;
    if (strcmp(param, "adsrPoint0") == 0)
        return (float)in->adsr[0].point;
    if (strcmp(param, "adsrIncrement0") == 0)
        return (float)in->adsr[0].increment;
    if (strcmp(param, "adsrPoint1") == 0)
        return (float)in->adsr[1].point;
    if (strcmp(param, "adsrIncrement1") == 0)
        return (float)in->adsr[1].increment;
    if (strcmp(param, "adsrPoint2") == 0)
        return (float)in->adsr[2].point;
    if (strcmp(param, "adsrIncrement2") == 0)
        return (float)in->adsr[2].increment;
    if (strcmp(param, "adsrPoint3") == 0)
        return (float)in->adsr[3].point;
    if (strcmp(param, "adsrIncrement3") == 0)
        return (float)in->adsr[3].increment;
    if (strcmp(param, "phaseValue") == 0)
        return (float)in->phase_value;
    if (strcmp(param, "phaseDirection") == 0)
        return in->phase_direction ? 1.0f : 0.0f;
    if (strcmp(param, "phasePosition") == 0)
        return (float)in->phase_position;

    return -1.0f;
}

void rk_set_instrument_param(RkModule* module, int inst, const char* param, float value) {
    if (!module || inst < 0 || inst >= module->num_instruments || !param)
        return;
    RkInstrument* in = &module->instruments[inst];
    uint8_t v8 = (uint8_t)value;
    int8_t sv8 = (int8_t)value;

    if (strcmp(param, "sampleNumber") == 0) {
        in->sample_number = (int)value;
        return;
    }
    if (strcmp(param, "vibratoNumber") == 0) {
        in->vibrato_number = (int)value;
        return;
    }
    if (strcmp(param, "type") == 0) {
        in->type = (RkInstrumentType)(int)value;
        return;
    }
    if (strcmp(param, "phaseSpeed") == 0) {
        in->phase_speed = v8;
        return;
    }
    if (strcmp(param, "phaseLengthInWords") == 0) {
        in->phase_length_in_words = v8;
        return;
    }
    if (strcmp(param, "vibratoSpeed") == 0) {
        in->vibrato_speed = v8;
        return;
    }
    if (strcmp(param, "vibratoDepth") == 0) {
        in->vibrato_depth = v8;
        return;
    }
    if (strcmp(param, "vibratoDelay") == 0) {
        in->vibrato_delay = v8;
        return;
    }
    if (strcmp(param, "adsrPoint0") == 0) {
        in->adsr[0].point = v8;
        return;
    }
    if (strcmp(param, "adsrIncrement0") == 0) {
        in->adsr[0].increment = v8;
        return;
    }
    if (strcmp(param, "adsrPoint1") == 0) {
        in->adsr[1].point = v8;
        return;
    }
    if (strcmp(param, "adsrIncrement1") == 0) {
        in->adsr[1].increment = v8;
        return;
    }
    if (strcmp(param, "adsrPoint2") == 0) {
        in->adsr[2].point = v8;
        return;
    }
    if (strcmp(param, "adsrIncrement2") == 0) {
        in->adsr[2].increment = v8;
        return;
    }
    if (strcmp(param, "adsrPoint3") == 0) {
        in->adsr[3].point = v8;
        return;
    }
    if (strcmp(param, "adsrIncrement3") == 0) {
        in->adsr[3].increment = v8;
        return;
    }
    if (strcmp(param, "phaseValue") == 0) {
        in->phase_value = sv8;
        return;
    }
    if (strcmp(param, "phaseDirection") == 0) {
        in->phase_direction = value != 0.0f;
        return;
    }
    if (strcmp(param, "phasePosition") == 0) {
        in->phase_position = v8;
        return;
    }
}

size_t rk_export(const RkModule* module, uint8_t* out, size_t max_size) {
    if (!module || !module->original_data)
        return 0;
    size_t total = module->original_size;
    if (!out)
        return total;
    if (max_size < total)
        return 0;
    memcpy(out, module->original_data, total);
    return total;
}

// Cell edit API

// Locate the loaded track byte at a file offset; -1 when no track holds it.
static int find_track_byte(const RkModule* module, uint32_t file_offset, int* out_index) {
    for (int t = 0; t < module->num_tracks; t++) {
        int start = module->track_offsets[t];
        if ((int)file_offset >= start && (int)file_offset < start + module->track_lengths[t]) {
            *out_index = (int)file_offset - start;
            return t;
        }
    }
    return -1;
}

int rk_set_cell(RkModule* module, int position, int channel, uint32_t file_offset, uint8_t note) {
    if (!module || !module->tracks || !module->track_offsets || module->num_sub_songs < 1)
        return 0;
    if (channel < 0 || channel > 3)
        return 0;

    // The grid shows sub-song 1, one pattern per position-list entry; the
    // note it shows is the one heard, i.e. with that entry's transpose added.
    RkPositionList* pl = &module->sub_songs[0].positions[channel];
    if (position < 0 || position >= pl->track_count)
        return 0;

    int index = 0;
    int t = find_track_byte(module, file_offset, &index);
    if (t < 0 || t != pl->tracks[position].track_number || index + 1 >= module->track_lengths[t])
        return 0;

    // Only the note byte of a note command changes. The wait byte stays: it
    // is the row's duration, which the grid derives its rows from, and a
    // different byte here would change the command's length and
    // desynchronise the rest of the track.
    uint8_t* data = module->tracks[t];
    int raw = (int)note - pl->tracks[position].transpose;
    if (data[index] >= 0x80 || raw < 0 || raw >= 0x80)
        return 0;

    data[index] = (uint8_t)raw;

    // Keep the file image in step so rk_export carries the edit.
    if (module->original_data && file_offset < module->original_size)
        module->original_data[file_offset] = (uint8_t)raw;
    return 1;
}

int rk_get_cell(const RkModule* module, uint32_t file_offset) {
    if (!module || !module->tracks || !module->track_offsets)
        return -1;

    int index = 0;
    int t = find_track_byte(module, file_offset, &index);
    if (t < 0 || index + 1 >= module->track_lengths[t])
        return -1;

    return (module->tracks[t][index] << 8) | module->tracks[t][index + 1];
}
