// SPDX-License-Identifier: MIT

#include "sonix.h"
#include "nuade_paula.h"
#include "nuade_utils.h"
#include <nuade/voice.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef nullptr
#define nullptr ((void*)0)
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t i16;
typedef int32_t i32;
typedef int8_t i8;
typedef float f32;
typedef double f64;

#define SNX_SS_BASE_PERIOD 0x1AB9u
// Both SSTech and IFFTech produce the same final hardware period:
// SSTech raw = (ratio * 0x1AB9) >> 15 then /16 via assembly scaling.
// IFFTech raw = (ratio * 0x358) >> 16 then *1 (unity scaling).
// Both yield: (ratio * 0x358) >> 16.
#define SNX_SS_IFF_PITCH_SCALE 0x358u
#define SNX_MIN_SAMPLE_HZ 48.0   // slower than this is no pitch: the note is dropped
#define SNX_NOMINAL_RATE 48000.0 // rate a sample of unknown speed is assumed to play at
#define SONIX_MAX_FILE_SIZE 0x7fffffffu

// Semitone ratio table shared by AIFFTech and SSTech pitch computation.
// ratio[0]=0x8000 (unison), ratio[11]=0x43CE (semitone -11).
// clang-format off
static const u16 sonix_ratio_table[12] = { 
    0x8000, 0x78D1, 0x7209, 0x6BA2, 0x6598, 0x5FE4, 0x5A82, 0x556E, 0x50A3, 0x4C1C, 0x47D6, 0x43CE 
};
// clang-format on

bool sonix_song_set_instrument_pcm8_ex(SonixSong* song, u8 instrument_index, const i8* pcm_data, u32 num_samples,
                                       u32 loop_start, u32 loop_len, u8 base_note, u32 base_period, u32 source_rate_hz);

bool sonix_song_set_instrument_mod(SonixSong* song, u8 instrument_index, u16 vib_depth, u16 vib_speed, u16 vib_delay);

static void sonix_release_note(SonixSong* song, int ch);
// Technique class of an instrument; StartNOTE compares the tech objects of old and new instrument.
static int snx_inst_tech(const SonixSong* song, u8 inst) {
    if (song->instrument_is_ss[inst])
        return 1;
    if (song->instrument_is_synth[inst])
        return 2;
    if (song->instrument_is_iff[inst])
        return 3;
    return 0;
}

static void sonix_start_note(SonixSong* song, int ch, int note, int velocity);

// Forward declaration for use in compute_smus_pitch_offset
static const u16 smus_tempo_table[128];

// Compute the SMUS pitch_offset (tempo table byte index) from the SHDR pitch word.
// Port of assembly LoadSCORE lines 6463-6479:
//   D1 = 0xE100000 / pitch_word
//   Linear search tempo table for first entry <= D1
//   Return byte offset (index * 2)
static u16 compute_smus_pitch_offset(u16 shdr_pitch_word) {
    if (shdr_pitch_word < 0x0E11u)
        return 0;
    u16 quotient = (u16)(0x0E100000u / (u32)shdr_pitch_word);
    for (u16 d0 = 0; d0 < 256; d0 += 2) {
        if (smus_tempo_table[d0 / 2] <= quotient) {
            return d0;
        }
    }
    return 255; // assembly: SUBQ.W #1,D0 when loop exhausts
}

// SMUS and TINY run the same asm score engine (PlayScore: tempo index at 2(A6), pitch scaling at 4(A6),
// master volume at 0(A6), per-note velocity); only the SNX format has the older cmd81/hw_vol engine.
static bool sonix_uses_score_engine(const SonixSong* song) {
    return song->metadata.format == SONIX_FORMAT_SMUS || song->metadata.format == SONIX_FORMAT_TINY;
}

static bool has_mark(const u8* data, u32 size, const char* mark) {
    if (data == nullptr || mark == nullptr || size < 4) {
        return false;
    }

    return memcmp(data, mark, 4) == 0;
}

static SonixFormat detect_format(const u8* data, u32 size) {
    if (data == nullptr || size < 4) {
        return SONIX_FORMAT_UNKNOWN;
    }

    if (has_mark(data, size, "FORM") && size >= 12 && memcmp(data + 8, "SMUS", 4) == 0) {
        return SONIX_FORMAT_SMUS;
    }

    if (has_mark(data, size, "SNX1")) {
        return SONIX_FORMAT_SNX;
    }

    if (size >= 2) {
        u32 w = ((u32)data[0] << 8) | (u32)data[1];
        if ((w & 0x00F0u) != 0) {
            return SONIX_FORMAT_TINY;
        }
        return SONIX_FORMAT_SNX;
    }

    if (size >= 48) {
        return SONIX_FORMAT_TINY;
    }

    return SONIX_FORMAT_UNKNOWN;
}

// Bytes from an IFF chunk header to the next chunk: header, payload and pad byte. 64-bit, so that a length near 4 GiB
// cannot wrap to a small advance.

static u64 smus_chunk_advance(u32 len) {
    return 8u + (u64)len + (len & 1u);
}

static bool parse_smus(SonixSong* song) {
    const u8* data = song->data;
    u32 size = song->size;

    if (size < 24 || !has_mark(data, size, "FORM") || memcmp(data + 8, "SMUS", 4) != 0) {
        snprintf(song->error, sizeof(song->error), "not a FORM/SMUS file");
        return false;
    }

    if (memcmp(data + 12, "SHDR", 4) != 0 || nuade_read_be32(data + 16) < 4 || data[23] == 0) {
        snprintf(song->error, sizeof(song->error), "invalid SHDR");
        return false;
    }

    // SHDR layout (assembly lbC000388):
    // bytes 20-21: pitch offset word → 2(A5)  (assembly processes through tempo lookup)
    // byte 22:     score volume       → 0(A5)  (if < 0x80, doubled; assembly line 6452-6457)
    // byte 23:     track count (1-4)
    {
        u8 vol_raw = data[22];
        if (vol_raw < 0x80u) {
            vol_raw = (u8)(vol_raw << 1);
        }
        // Assembly PlaySCORE snaps 0(A6) to this value via RampVOLUME(duration=0).
        // Used as the score-level master volume in SyntTech (asm line 7711).
        song->smus_ramp_target = vol_raw;
        // SMUS tick timing is driven by smus_update_tempo (from pitch word),
        // not snx_header_tempo_base. Set a safe default for the initial tick.
        song->snx_header_tempo_base = vol_raw;
        if (song->snx_header_tempo_base == 0) {
            song->snx_header_tempo_base = 0xA5u;
        }
    }
    // Pitch offset: assembly LoadSCORE (lines 6463-6479) computes a tempo table
    // byte index from the SHDR pitch word: D1 = 0xE100000 / word, then linear
    // search the 128-entry tempo table for the first entry <= D1.
    // PlaySCORE copies this index to 2(A6) for tempo lookup.
    song->snx_pitch_offset = compute_smus_pitch_offset(nuade_read_be16(data + 20));

    u32 pos = 24;
    if (pos + 8 <= size && memcmp(data + pos, "NAME", 4) == 0) {
        u32 len = nuade_read_be32(data + pos + 4);
        u64 adv = smus_chunk_advance(len);
        if (pos + adv > size) {
            snprintf(song->error, sizeof(song->error), "truncated NAME chunk");
            return false;
        }
        pos += (u32)adv;
    }

    if (pos + 8 > size || memcmp(data + pos, "SNX1", 4) != 0) {
        snprintf(song->error, sizeof(song->error), "missing SNX1 chunk");
        return false;
    }

    bool saw_snx1 = false;
    {
        u32 len = nuade_read_be32(data + pos + 4);
        u64 adv = smus_chunk_advance(len);
        u32 snx1_data = pos + 8; // offset of SNX1 data within file
        if (pos + adv > size) {
            snprintf(song->error, sizeof(song->error), "truncated SNX1 chunk");
            return false;
        }

        // Extract fields from SNX1 chunk data (score structure).
        // Assembly LoadSCORE stores:
        //   0(A5) = score volume (from SHDR volume byte, doubled if < 0x80)
        //   2(A5) = tempo index (computed from SHDR pitch word)  → already set above
        //   4(A5) = SNX1 word 0  (note pitch word)
        //   6(A5) = SNX1 word 1
        // PlaySCORE copies: 2(A5)→2(A6) (tempo), 6(A5)→4(A6) (pitch scaling)
        song->smus_snx1_offset = (u16)snx1_data;
        song->smus_snx1_len = (u16)len;
        // smus_ramp_target already set from SHDR volume byte above
        // snx_pitch_offset already computed from SHDR above; do NOT overwrite
        song->smus_note_pitch_word = (len >= 2) ? nuade_read_be16(data + snx1_data) : 0x0080;
        // Assembly: PlaySCORE copies 6(A5)→4(A6) (pitch scaling for synth env)
        song->smus_pitch_scaling = (len >= 4) ? nuade_read_be16(data + snx1_data + 2) : 0x0080;
        // Per-channel instrument presence flags from SNX1 longs at offset 8.
        // Assembly: LoadSCORE reads 4 longs into $20(A5), then PlaySCORE copies
        // them to $3A(A6) per channel. These gate note playback in PlaySMUS.
        {
            for (int i = 0; i < SONIX_NUM_CHANNELS; i++) {
                u32 flag_off = 8 + (u32)i * 4;
                if (flag_off + 4 <= len) {
                    u32 raw = nuade_read_be32(data + snx1_data + flag_off);
                    song->smus_inst_present[i] = (raw != 0);
                    song->smus_inst_present_raw[i] = (u16)(raw & 0xFFFF);
                } else {
                    song->smus_inst_present[i] = false;
                    song->smus_inst_present_raw[i] = 0;
                }
            }
        }
        // Per-channel base velocity: assembly initializes Buffer2+$12+ch*4 to 0x00FF
        // during LoadSCORE (line 6394). 0x84xx commands may update velocity during
        // playback, but the init must be 0xFF.
        for (int i = 0; i < SONIX_NUM_CHANNELS; i++) {
            song->smus_base_velocity[i] = 0xFF;
        }

        pos += (u32)adv;
        saw_snx1 = true;
    }

    bool saw_trak = false;
    u32 trak_start[SONIX_NUM_CHANNELS] = { 0 };
    u32 trak_end[SONIX_NUM_CHANNELS] = { 0 };
    int trak_count = 0;

    while (pos + 8 <= size) {
        const u8* ch = data + pos;
        u32 len = nuade_read_be32(ch + 4);
        u64 adv = smus_chunk_advance(len);
        if (pos + adv > size) {
            snprintf(song->error, sizeof(song->error), "truncated chunk");
            return false;
        }

        if (memcmp(ch, "INS1", 4) == 0) {
            if (len < 2) {
                snprintf(song->error, sizeof(song->error), "INS1 too short");
                return false;
            }

            if (ch[8] > 63 || ch[9] != 0) {
                snprintf(song->error, sizeof(song->error), "INS1 header invalid");
                return false;
            }

            song->metadata.num_ins_chunks++;
            song->metadata.num_real_samples++;

            {
                u8 inst = ch[8];
                const u8* name_ptr = ch + 12;
                u32 name_len = len >= 4 ? len - 4 : 0;
                u32 n = 0;
                if (name_len > 0 && inst < 64) {
                    while (n < name_len && n < 63 && name_ptr[n] != 0) {
                        song->instrument_names[inst][n] = (char)name_ptr[n];
                        n++;
                    }
                    song->instrument_names[inst][n] = '\0';
                }
            }
        } else if (memcmp(ch, "TRAK", 4) == 0) {
            saw_trak = true;
            song->metadata.num_track_chunks++;
            if (trak_count < SONIX_NUM_CHANNELS && len > 0) {
                trak_start[trak_count] = pos + 8;
                trak_end[trak_count] = pos + 8 + len;
                trak_count++;
            }
        } else if (memcmp(ch, "FORM", 4) == 0) {
            // Nested FORM sometimes appears in some archives; ignore.
        }

        pos += (u32)adv;
    }

    if (!saw_snx1 || !saw_trak) {
        snprintf(song->error, sizeof(song->error), "missing SNX1/TRAK");
        return false;
    }

    if (trak_count == 0) {
        snprintf(song->error, sizeof(song->error), "no TRAK chunks");
        return false;
    }
    // Songs may have fewer than 4 TRAK chunks (SHDR byte 3 = track count).
    // Unused channels keep start=end=0 and will immediately end on first tick.

    for (int i = 0; i < SONIX_NUM_CHANNELS; i++) {
        song->snx_track_start[i] = trak_start[i];
        song->snx_track_end[i] = trak_end[i];
    }

    return true;
}

bool sonix_song_set_instrument_mod(SonixSong* song, u8 instrument_index, u16 vib_depth, u16 vib_speed, u16 vib_delay) {
    if (song == nullptr || instrument_index >= 64) {
        return false;
    }

    song->instrument_vib_depth[instrument_index] = vib_depth;
    song->instrument_vib_speed[instrument_index] = vib_speed;
    song->instrument_vib_delay[instrument_index] = vib_delay;

    return true;
}

// Mirrors basic sanity checks from the original Check2/Check5 paths.

static bool parse_tiny(SonixSong* song) {
    const u8* d = song->data;
    u32 size = song->size;

    if (size < 64) {
        snprintf(song->error, sizeof(song->error), "tiny too small");
        return false;
    }

    // Header fields (assembly PLAYSCORE lines 4508-4536)
    // Offset 0: volume word (for RAMPVOLUME)
    u16 vol_word = nuade_read_be16(d + 0);
    // PlayScore ramps 0(A6) to this word with duration 0, so it is the score master volume as-is.
    song->smus_ramp_target = vol_word;
    // Offset 2: pitch_offset
    song->snx_pitch_offset = nuade_read_be16(d + 2);
    // Offset 4: note pitch word (same semantics as SMUS note_pitch_word)
    song->smus_note_pitch_word = nuade_read_be16(d + 4);
    // Offset 6: pitch scaling
    song->smus_pitch_scaling = nuade_read_be16(d + 6);

    // Per-channel velocity words at offset 0x10 + ch*4, reading word at +2
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        u32 base = 0x10u + (u32)ch * 4u;
        if (base + 4 <= size) {
            song->smus_base_velocity[ch] = nuade_read_be16(d + base + 2);
        }
    }

    // Per-channel instrument gate at offset 0x20 + ch*4 (non-zero = present)
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        u32 base = 0x20u + (u32)ch * 4u;
        if (base + 4 <= size) {
            u32 gate = nuade_read_be32(d + base);
            song->tiny_inst_present[ch] = (gate != 0);
        }
    }

    // Per-channel inst_present_raw from word at offset 0x28 + ch*4
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        u32 base = 0x20u + (u32)ch * 4u;
        if (base + 4 <= size) {
            song->smus_inst_present_raw[ch] = nuade_read_be16(d + base + 2);
        }
    }

    // Track data offsets at offsets 48, 52, 56, 60 (0x30-0x3C)
    u32 first_track = size;
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        u32 off = nuade_read_be32(d + 48 + (u32)ch * 4u);
        if (off == 0 || off >= size) {
            // Channel has no track data - mark as ended
            song->snx_track_start[ch] = 0;
            song->snx_track_end[ch] = 0;
            continue;
        }
        song->snx_track_start[ch] = off;
        if (off < first_track)
            first_track = off;
    }

    // Set track end = next track start or file end
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        if (song->snx_track_start[ch] == 0)
            continue;
        u32 next = size;
        for (int j = 0; j < SONIX_NUM_CHANNELS; j++) {
            if (j == ch || song->snx_track_start[j] == 0)
                continue;
            if (song->snx_track_start[j] > song->snx_track_start[ch] && song->snx_track_start[j] < next) {
                next = song->snx_track_start[j];
            }
        }
        song->snx_track_end[ch] = next;
    }

    // Instrument name table at offset 0x40
    // One 4-char name per slot (0x81xx selects slot xx) up to the first track; an empty slot means "no
    // instrument" and does not end the table.
    for (int slot = 0; slot < 64; slot++) {
        u32 name_off = 0x40u + (u32)slot * 4u;
        if (name_off + 4 > first_track || name_off + 4 > size)
            break;
        if (nuade_read_be32(d + name_off) == 0)
            continue;
        memcpy(song->instrument_names[slot], d + name_off, 4);
        song->instrument_names[slot][4] = '\0';
    }

    // Volume ramp init: snap ramp slot 0 target from header volume word
    // Assembly: RAMPVOLUME D1 with vol_word from 0(A5)
    if (vol_word > 0) {
        song->snx_ramp_target[0] = (u16)((vol_word & 0xFFu) << 8);
        song->snx_ramp_current[0] = song->snx_ramp_target[0];
    }

    return true;
}

static bool parse_snx(SonixSong* song) {
    const u8* d = song->data;
    u32 size = song->size;
    u32 d3 = 20;
    u32 a0 = 0;
    u32 a1 = 0;
    u32 lens[SONIX_NUM_CHANNELS];
    u32 starts[SONIX_NUM_CHANNELS];
    u32 ends[SONIX_NUM_CHANNELS];

    if (size < 32) {
        snprintf(song->error, sizeof(song->error), "snx too small");
        return false;
    }

    for (int i = 0; i < 4; i++) {
        u32 off = nuade_read_be32(d + a0);
        if (off == 0 || (off & 1u) != 0 || (off & 0x80000000u) != 0) {
            snprintf(song->error, sizeof(song->error), "snx invalid section offset");
            return false;
        }
        lens[i] = off;
        d3 += off;
        a0 += 4;
    }

    if (d3 >= size) {
        snprintf(song->error, sizeof(song->error), "snx summed offsets out of range");
        return false;
    }

    starts[0] = 0x14u;
    ends[0] = starts[0] + lens[0];

    for (int i = 1; i < 4; i++) {
        starts[i] = ends[i - 1];
        ends[i] = starts[i] + lens[i];
    }

    for (int i = 0; i < 4; i++) {
        if (starts[i] >= size || ends[i] > size || starts[i] >= ends[i]) {
            snprintf(song->error, sizeof(song->error), "snx track bounds invalid");
            return false;
        }
        song->snx_track_start[i] = starts[i];
        song->snx_track_end[i] = ends[i];
    }

    a0 = starts[0];

    for (int i = 0; i < 4; i++) {
        if (a0 >= size) {
            snprintf(song->error, sizeof(song->error), "snx section cursor out of range");
            return false;
        }

        if ((d[a0] & 0x80u) == 0u) {
            snprintf(song->error, sizeof(song->error), "snx section marker invalid");
            return false;
        }

        if (!(d[a0] == 0xFFu && a0 + 1 < size && d[a0 + 1] == 0xFFu) && d[a0] > 0x84u) {
            snprintf(song->error, sizeof(song->error), "snx section marker out of range");
            return false;
        }

        a0 += lens[i];
        a1 += 4;
    }

    {
        u32 tail_pos = ends[3];
        int inst = 0;
        while (tail_pos < size && inst < 64) {
            u32 n = 0;
            while (tail_pos < size && song->data[tail_pos] == 0) {
                tail_pos++;
            }
            if (tail_pos >= size) {
                break;
            }
            while (tail_pos < size && song->data[tail_pos] != 0 && n < 63) {
                u8 c = song->data[tail_pos++];
                if (c < 32 || c > 126) {
                    break;
                }
                song->instrument_names[inst][n++] = (char)c;
            }
            song->instrument_names[inst][n] = '\0';
            if (n > 0) {
                inst++;
            }
            while (tail_pos < size && song->data[tail_pos] != 0) {
                tail_pos++;
            }
        }
    }

    song->metadata.num_track_chunks = 4;
    // Assembly InitScore (line 1735): MOVE.W #$80,4(A5), hardcoded pitch offset.
    // PlayScore copies 4(A5) → 2(A6).  0x1080 - 0x0080 = 0x1000 = unity scaling.
    song->snx_pitch_offset = 0x0080u;
    song->smus_pitch_scaling = 0x0080u;
    {
        u16 t = nuade_read_be16(d + 0x12u);
        song->snx_header_tempo_base = (u8)(t & 0xFFu);
        if (song->snx_header_tempo_base == 0) {
            song->snx_header_tempo_base = 0xA5u;
        }
    }
    return true;
}

// SMUS tempo lookup table (128 entries) from assembly lbW000CAE (line 7286).
// Upper 4 bits = ticks-per-beat, lower 12 bits = timing divisor.
// clang-format off

// SMUS duration translation table (assembly lbW0007C2, line 6828).
// Maps the low nibble of the TRAK word's low byte to actual beat counts.
// Standard durations: 0=whole(32), 1=half(16), 2=quarter(8), 3=eighth(4), 4=sixteenth(2).
// Dotted durations:   8=dotted whole(48), 9=dotted half(24), 10=dotted quarter(12),
//                     11=dotted eighth(6), 12=dotted sixteenth(3).
// Values 0xFF are invalid and the word is zeroed out during preprocessing.
static const u8 smus_duration_table[16] = {
    0x20, 0x10, 0x08, 0x04, 0x02, 0xFF, 0xFF, 0xFF,
    0x30, 0x18, 0x0C, 0x06, 0x03, 0xFF, 0xFF, 0xFF,
};

static const u16 smus_tempo_table[128] = {
    0xFA83, 0xF525, 0xEFE4, 0xEAC0, 0xE5B9, 0xE0CC, 0xDBFB, 0xD744,
    0xD2A8, 0xCE24, 0xC9B9, 0xC567, 0xC12C, 0xBD08, 0xB8FB, 0xB504,
    0xB123, 0xAD58, 0xA9A1, 0xA5FE, 0xA270, 0x9EF5, 0x9B8D, 0x9837,
    0x94F4, 0x91C3, 0x8EA4, 0x8B95, 0x8898, 0x85AA, 0x82CD, 0x8000,
    0x7D41, 0x7A92, 0x77F2, 0x7560, 0x72DC, 0x7066, 0x6DFD, 0x6BA2,
    0x6954, 0x6712, 0x64DC, 0x62B3, 0x6096, 0x5E84, 0x5C7D, 0x5A82,
    0x5891, 0x56AC, 0x54D0, 0x52FF, 0x5138, 0x4F7A, 0x4DC6, 0x4C1B,
    0x4A7A, 0x48E1, 0x4752, 0x45CA, 0x444C, 0x42D5, 0x4166, 0x4000,
    0x3EA0, 0x3D49, 0x3BF9, 0x3AB0, 0x396E, 0x3833, 0x36FE, 0x35D1,
    0x34AA, 0x3389, 0x326E, 0x3159, 0x304B, 0x2F42, 0x2E3E, 0x2D41,
    0x2C48, 0x2B56, 0x2A68, 0x297F, 0x289C, 0x27BD, 0x26E3, 0x260D,
    0x253D, 0x2470, 0x23A9, 0x22E5, 0x2226, 0x216A, 0x20B3, 0x2000,
    0x1F50, 0x1EA4, 0x1DFC, 0x1D58, 0x1CB7, 0x1C19, 0x1B7F, 0x1AE8,
    0x1A55, 0x19C4, 0x1937, 0x18AC, 0x1825, 0x17A1, 0x171F, 0x16A0,
    0x1624, 0x15AB, 0x1534, 0x14BF, 0x144E, 0x13DE, 0x1371, 0x1306,
    0x129E, 0x1238, 0x11D4, 0x1172, 0x1113, 0x10B5, 0x1059, 0x1000,
};
// clang-format on

static bool parse_song(SonixSong* song) {
    if (song->metadata.format == SONIX_FORMAT_SMUS) {
        return parse_smus(song);
    }

    if (song->metadata.format == SONIX_FORMAT_TINY) {
        return parse_tiny(song);
    }

    if (song->metadata.format == SONIX_FORMAT_SNX) {
        return parse_snx(song);
    }

    snprintf(song->error, sizeof(song->error), "unknown format");
    return false;
}

static void snx_update_tick_timing(SonixSong* song) {
    static const u32 CLOCK = (SONIX_CIA_PAL_HZ / 50u) * 125u; // 14187 * 125 = 1773375
    u8 cur = song->tempo_cur ? song->tempo_cur : (song->tempo_base ? song->tempo_base : 0xA5u);
    u32 timer = CLOCK / (u32)cur;

    if (timer == 0)
        timer = 1;

    // Match the assembly's integer CIA timer arithmetic exactly.
    // Assembly (InitSound): Clock = dtg_Timer * 125, where dtg_Timer = 709379/50 = 14187.
    // Assembly (lbC01E798): timer = Clock / tempo (integer division).
    // A CIA timer loaded with N counts N+1 cycles. UADE ticks are (N+1) cycles long: with N the native song drifted
    // ~7e-5 ahead.
    song->tick_length = timer + 1u;

    // Assembly (lbC01E798): ticks_per_beat = 0x4B0000 / timer_value. The timer_value is the raw tempo byte.
    if (cur > 0) {
        song->snx_ticks_per_beat = (u16)(0x4B0000u / (u32)cur);
    }
}

static u32 snx_note_to_period(int note) {
    // Mirrors Sonix note->period path used by the original player core.
    // clang-format off
    static const u16 k_note_ratio[12] = { 
        0x8000, 0x78D1, 0x7209, 0x6BA2, 0x6598, 0x5FE4, 0x5A82, 0x556E, 0x50A3, 0x4C1C, 0x47D6, 0x43CE 
    };
    // clang-format on

    u32 base = 0xD5C8u;

    if (note < 0x24)
        note = 0x24;
    if (note > 0x6B)
        note = 0x6B;

    u32 rel = (u32)(note - 0x24);
    u32 octave = rel / 12u;
    u32 semi = rel % 12u;
    u32 period = (base * (u32)k_note_ratio[semi]) >> (octave + 17u);
    if (period < 2u)
        period = 2u;

    return period;
}

static f64 snx_note_to_hz(int note) {

    u32 period = snx_note_to_period(note);
    return NUADE_PAULA_CLOCK_PAL / (2.0 * (f64)period);
}

// Compute sample_inc matching the assembly's AIFF tech period computation.
// Assembly flow (lbC01F944 + lbC01FA6A):
// 1. Start with base_rate (16-bit source sample rate)
// 2. Subtract base_note from note; adjust rate by octave (halve/double)
// 3. Fine-tune by semitone: rate = (rate << 15) / ratio_table[semi]
// 4. period = 0x369E99 / rate  (NTSC clock, integer division)
// 5. final_period = (0x1080 * period) >> 12  (pitch scaling, 2(A6)=0 for SNX)
// 6. UADE plays at NUADE_PAULA_CLOCK_PAL / final_period
// 7. the result is the effective sample rate in Hz
static f64 snx_compute_hw_sample_hz(u32 base_rate, int note, int base_note, u16 pitch_offset) {

    if (base_rate == 0)
        return 0.0;

    u32 rate = base_rate;
    if (rate > 0xFFFFu)
        rate = 0xFFFFu;

    int diff = note - base_note;

    // Octave adjustment: halve rate for each octave down, double for up
    // (assembly lines 3735-3746)
    while (diff < 0) {
        rate >>= 1;
        if (rate == 0)
            return 0.0;
        diff += 12;
    }
    while (diff >= 12) {
        rate <<= 1;
        if (rate > 0xFFFFu)
            return 0.0; // overflow
        diff -= 12;
    }

    // Semitone fine-tune: rate = (rate << 15) / ratio[semi]
    // (assembly lines 3748-3755)
    if (diff > 0) {
        u32 scaled = rate << 15;
        u32 divisor = sonix_ratio_table[diff];
        if (divisor == 0)
            return 0.0;
        rate = scaled / divisor;
        if (rate == 0 || rate > 0xFFFFu)
            return 0.0;
    }

    if (rate == 0)
        return 0.0;

    // period = NTSC_CLOCK / rate  (assembly line 3760: DIVU.W)
    u32 period = 0x369E99u / rate;
    if (period == 0 || period > 0xFFFFu)
        return 0.0;

    // Pitch scaling: final = ((0x1080 - pitch_offset) * period) >> 12
    // pitch_offset = 2(A6), from song header offset 2.
    // Default 0x0080 gives unity (0x1000 * period >> 12 = period).
    // Assembly lines 3803-3807: MOVE.W #$1080,D0; SUB.W 2(A6),D0;
    // MULU.W 0(A2),D0; ASL.L #4,D0; SWAP D0
    u32 scale = (u32)(0x1080u - pitch_offset);
    u32 final_period = (scale * period) >> 12;

    if (final_period == 0)
        return 0.0;

    return NUADE_PAULA_CLOCK_PAL / (f64)final_period;
}

// OneFilter: 64-band IIR filter bank pre-computation.
//    Assembly lines 2866-2901 (OneFilter) + table at lbW01F0FE.
//    Generates 64 filtered copies (each 128 bytes) of the source waveform.
//    Band 0 (coeff $8000) = brightest, band 63 (coeff $100) = most filtered.
// clang-format off
static const u16 snx_filter_coeffs[64] = { 
    0x8000, 0x7683, 0x6DBA, 0x6597, 0x5E10, 0x5717, 0x50A2, 0x4AA8, 0x451F, 0x4000, 0x3B41, 0x36DD, 0x32CB,
    0x2F08, 0x2B8B, 0x2851, 0x2554, 0x228F, 0x2000, 0x1DA0, 0x1B6E, 0x1965, 0x1784, 0x15C5, 0x1428, 0x12AA,
    0x1147, 0x1000, 0x0ED0, 0x0DB7, 0x0CB2, 0x0BC2, 0x0AE2, 0x0A14, 0x0955, 0x08A3, 0x0800, 0x0768, 0x06DB,
    0x0659, 0x05E1, 0x0571, 0x050A, 0x04AA, 0x0451, 0x0400, 0x03B4, 0x036D, 0x032C, 0x02F0, 0x02B8, 0x0285,
    0x0255, 0x0228, 0x0200, 0x01DA, 0x01B6, 0x0196, 0x0178, 0x015C, 0x0142, 0x012A, 0x0114, 0x0100 
};
// clang-format on

// cmd83 pitch modulation table (assembly: lbW01E3B2, 64 entries).
// Index 32 = 0x8000 = unity (no modulation).
// Values > 0x8000 increase period (lower pitch), < 0x8000 decrease period (higher pitch).
// Index = ((cmd83_value + 0x80) & 0xFF) >> 2.
// clang-format off
static const u16 snx_pitch_mod_table[64] = {
    0xC24D, 0xBFC8, 0xBD4C, 0xBAD8, 0xB86C, 0xB608, 0xB3AC, 0xB158,
    0xAF0C, 0xACC7, 0xAA8A, 0xA854, 0xA626, 0xA3FF, 0xA1DF, 0x9FC6,
    0x9DB4, 0x9BA9, 0x99A4, 0x97A6, 0x95AF, 0x93BF, 0x91D5, 0x8FF1,
    0x8E13, 0x8C3C, 0x8A6B, 0x88A0, 0x86DA, 0x851B, 0x8362, 0x81AE,
    0x8000, 0x7E57, 0x7CB4, 0x7B16, 0x797E, 0x77EB, 0x765D, 0x74D4,
    0x7351, 0x71D2, 0x7059, 0x6EE4, 0x6D74, 0x6C09, 0x6AA2, 0x6941,
    0x67E4, 0x668B, 0x6537, 0x63E7, 0x629C, 0x6154, 0x6012, 0x5ED3,
    0x5D98, 0x5C62, 0x5B2F, 0x5A01, 0x58D6, 0x57B0, 0x568D, 0x556E,
};
// clang-format on

static void snx_one_filter(const i8* source, i8* output) {
    i16 d3 = 0;                        // feedback accumulator
    i16 d4 = (i16)(source[127] * 128); // last sample scaled up

    for (int band = 0; band < 64; band++) {
        u16 coeff = snx_filter_coeffs[band];
        // damping = (0x8000 - coeff) * 0xE666 >> 16
        i16 d2 = (i16)((u32)(0x8000u - coeff) * 0xE666u >> 16);
        i16 d1 = (i16)(coeff >> 1); // half coefficient

        for (int samp = 0; samp < 128; samp++) {
            i16 d6 = (i16)(source[samp] * 128); // input scaled up
            i32 prod;
            d6 = d6 - d4;                              // difference from prev output
            prod = (i32)d6 * (i32)d1;                  // scale by cutoff
            d6 = (i16)((i32)((u32)prod << 2) >> 16);   // ASL.L #2 (wraps), SWAP
            d3 = d3 + d6;                              // accumulate feedback
            d4 = d4 + d3;                              // update output
            output[band * 128 + samp] = (i8)(d4 >> 7); // ROR.W #7 + MOVE.B
            // Apply damping: D3 = (D3 * D2) << 1 >> 16
            d3 = (i16)(((i32)d3 * (i32)d2 * 2) >> 16);
        }
    }
}

static void snx_compute_filter_bank(SonixSong* song, u8 inst) {
    if (!song->synth_wave_set[inst])
        return;
    if (song->synth_filter_bank[inst] == nullptr) {
        song->synth_filter_bank[inst] = (i8*)calloc(64, 128);
        if (song->synth_filter_bank[inst] == nullptr)
            return;
    }
    snx_one_filter(song->synth_wave[inst], song->synth_filter_bank[inst]);
}

// The byte right after a 128-byte filter band (the ring-mod resampler reads it, see snx_synth_render_tick). The
// loader packs the synth instruments' 8192-byte banks back to back in instrument order (asm `add.l #8192,D5`), so
// after band 63 comes band 0 of the next synth instrument; nothing (0 here) follows the last one.
static i8 snx_filter_bank_byte_after(const SonixSong* song, u8 inst, const i8* band) {
    const i8* bank = song->synth_filter_bank[inst];
    if (band < bank + 63 * 128)
        return band[128];
    for (int next = inst + 1; next < 64; next++) {
        if (song->synth_filter_bank[next] != nullptr)
            return song->synth_filter_bank[next][0];
    }
    return 0;
}

static u8 snx_compute_hw_vol(u8 cmd81, u8 velocity) {
    // Assembly (lbC01E8E2):
    // D2 = cmd81 + 1, D3 = velocity * 2 + 1
    // hw_vol = (D2 * D3) >> 8, result is 0..255.
    u32 hw_vol = (((u32)cmd81 + 1u) * ((u32)velocity * 2u + 1u)) >> 8;
    if (hw_vol > 255u)
        hw_vol = 255u;
    return (u8)hw_vol;
}

// Assembly FUN_002204c8 / lbC01EAB8 (line 2315):
// Called once per tick. Updates all 8 ramp slots linearly.
// step = (speed * ticks_per_beat * 2) >> 16
// If step >= |target - current|: snap to target, clear speed.
// Else: current += step (toward target).
static void snx_ramp_volume_tick(SonixSong* song) {
    for (int i = 0; i < 8; i++) {
        u16 spd = song->snx_ramp_speed[i];

        if (spd == 0)
            continue;

        u16 cur = song->snx_ramp_current[i];
        u16 tgt = song->snx_ramp_target[i];
        u32 raw = (u32)spd * (u32)song->snx_ticks_per_beat;

        // Overflow check: assembly ADD.L D0,D0 / BCS checks carry = old bit 31.
        // Must test BEFORE the shift, since <<1 wraps in 32-bit C arithmetic.
        bool carry = (raw & 0x80000000u) != 0;
        raw <<= 1;

        if (carry) {
            song->snx_ramp_speed[i] = 0;
            song->snx_ramp_current[i] = tgt;
            continue;
        }

        u16 step = (u16)(raw >> 16);
        u16 diff;
        i16 signed_step;

        if (tgt >= cur) {
            diff = tgt - cur;
            signed_step = (i16)step;
        } else {
            diff = cur - tgt;
            signed_step = -(i16)step;
        }

        if (step >= diff) {
            song->snx_ramp_speed[i] = 0;
            song->snx_ramp_current[i] = tgt;
        } else {
            song->snx_ramp_current[i] = (u16)((i16)cur + signed_step);
        }
    }
}

// SMUS tempo update (port of lbC0007F2, assembly lines 6845-6881).
// Uses pitch_offset to index the 128-entry tempo table.
// Upper 4 bits of table entry = ticks-per-beat (smus_tpb).
// Timer value derived from the lower 12 bits controls tick timing.
static void smus_update_tempo(SonixSong* song) {
    u16 index = song->snx_pitch_offset >> 1;

    if (index > 127)
        index = 127;
    if (index == song->smus_last_tempo_idx)
        return; // no change
    song->smus_last_tempo_idx = index;

    u16 entry = smus_tempo_table[index];
    u16 tpb_raw = entry >> 12;
    if (tpb_raw == 0)
        tpb_raw = 1;
    song->smus_tpb = tpb_raw;

    // Assembly lbC0007F2 lines 6853-6865:
    // D2 = entry >> 12 (tpb), D1 = entry with upper bits
    // D1 <<= 12 gives: ((entry & 0xFFFF) << 16) >> 1 / (tpb << 12)
    // Then timer = D1 * 0x2E9C >> 15 → CIA timer value
    u16 divisor = tpb_raw << 12;
    if (divisor == 0)
        divisor = 1;
    u32 timer_val;
    {
        u32 d1 = (u32)entry;
        d1 <<= 16;
        d1 >>= 1;
        timer_val = (u16)(d1 / divisor);
    }

    // Assembly stores this pre-transform value at $36(A6) and uses it for
    // portamento, ramp volume, synth envelope, etc.  The C code's
    // snx_ticks_per_beat must equal $36(A6), NOT 0x4B0000/CIA_timer.
    song->snx_ticks_per_beat = (u16)timer_val;

    timer_val = (timer_val * 0x2E9Cu) >> 15;
    if (timer_val == 0)
        timer_val = 1;

    // Update tick timing using the same Bresenham method as SNX
    song->tick_length = timer_val + 1u;
}

// SMUS track scan (port of lbC0006E4, assembly lines 6743-6784).
// Pre-scans track data to determine initial instrument and wait per channel.
// start_tick controls how far into the track to advance. With start_tick=0,
// all channels start from the beginning with no initial instrument or wait.
// asm LoadSCORE preprocessing (6548-6580): the last 0x84xx word of a track sets the track's base
// velocity ($12(A4) = (lo & 0x7F) * 2) for every note; the words themselves are zeroed.
static void smus_preload_track_velocity(SonixSong* song, int ch) {
    u32 pos = song->snx_track_start[ch];
    u32 end = song->snx_track_end[ch];

    if (pos == 0 || pos >= end)
        return;
    while (pos + 2 <= end) {
        u16 word = nuade_read_be16(song->data + pos);
        pos += 2;
        if (word == 0xFFFFu)
            break;
        if ((word >> 8) == 0x84u)
            song->smus_base_velocity[ch] = (u16)((word & 0x7Fu) * 2u);
    }
}

static void smus_scan_tracks(SonixSong* song, u32 start_tick) {
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        u32 pos = song->snx_track_start[ch];
        u32 end = song->snx_track_end[ch];
        i32 remaining = (i32)start_tick;

        song->smus_stream_pos[ch] = pos;
        song->smus_init_inst[ch] = 0;
        song->smus_init_wait[ch] = 0;

        if (pos == 0 || pos >= end)
            continue;

        while (pos + 2 <= end) {
            u16 word;
            u8 hi, lo;

            song->smus_stream_pos[ch] = pos;
            if (remaining <= 0)
                break;

            word = nuade_read_be16(song->data + pos);
            pos += 2;
            if (word == 0)
                continue;
            if (word == 0xFFFFu)
                break;
            hi = (u8)(word >> 8);
            lo = (u8)(word & 0xFF);

            if (hi >= 0x82u)
                continue; // 0x82xx/0x84xx zeroed during preprocessing, skip
            if (hi == 0x81u) {
                // Instrument command: store register + 1
                song->smus_init_inst[ch] = lo + 1;
                continue;
            }
            // Note or cmd80: translate low nibble through duration table
            {
                u8 dur_idx = lo & 0x0Fu;
                u8 duration = smus_duration_table[dur_idx];
                if (duration == 0xFFu)
                    continue; // invalid, zeroed in asm
                remaining -= (i32)duration;
                if (remaining < 0) {
                    song->smus_init_wait[ch] = (u8)(-(i32)remaining);
                }
            }
        }
        // Update stream position to where we ended
        song->smus_stream_pos[ch] = pos;
    }
}

// SMUS channel init (port of lbC000754, assembly lines 6786-6821).
// Called at song start and on each loop restart. Sets up per-channel state
// from the scan results and resets counters.
static void smus_init_channels(SonixSong* song) {
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        // Release any active note
        if (song->smus_has_inst[ch]) {
            sonix_release_note(song, ch);
        }
        // Clear release counter
        song->smus_release_wait[ch] = 0;
        // Set note wait from scan init_wait + 1
        song->smus_note_wait[ch] = (u32)song->smus_init_wait[ch] + 1;
        // Set initial instrument from scan (init_inst is register+1, or 0 for none)
        if (song->smus_init_inst[ch] > 0) {
            u8 inst = song->smus_init_inst[ch] - 1;
            song->smus_cur_inst[ch] = inst;
            song->smus_has_inst[ch] = (inst < 64);
        } else {
            song->smus_cur_inst[ch] = 0;
            song->smus_has_inst[ch] = false;
        }
        // Reset velocity
        song->smus_velocity[ch] = 0;
        song->smus_velocity_init[ch] = 0;
        // Track ended flag
        song->snx_track_ended[ch] = false;
    }
    // Reset tick counter to start value (assembly: MOVE.L 12(A6),$10(A6))
    song->smus_tick_count = 0;
}

static void snx_reset_runtime(SonixSong* song) {
    song->tempo_base = song->snx_header_tempo_base ? song->snx_header_tempo_base : 0xA5u;
    song->tempo_cur = song->tempo_base;
    snx_update_tick_timing(song);
    song->first_tick_length = song->tick_length;

    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        song->snx_track_pos[ch] = song->snx_track_start[ch];
        song->snx_wait[ch] = 0;
        song->snx_cmd81[ch] = 0xFF;
        song->snx_cmd80[ch] = 0;
        song->snx_cmd83[ch] = 0;
        song->snx_track_ended[ch] = false;
        song->snx_note_on[ch] = false;
        song->snx_note_velocity[ch] = 0;
        song->snx_hw_vol[ch] = 0;
        song->snx_use_sample[ch] = false;
        song->snx_inst_index[ch] = 0;
        song->snx_sample_hz[ch] = 0.0;
        song->snx_sample_base_hz[ch] = 0.0;
        song->snx_vib_phase[ch] = 0.0;
        song->snx_vib_delay[ch] = 0;
    }

    // Assembly PlayScore: snap all 8 ramp slots to full volume (0xFF00 in 8.8).
    // In the assembly, PlayScore first snaps to 0 then ramps up from the score
    // data. We snap to full since the ramp completes quickly and we don't yet
    // parse the score's initial ramp parameters.
    for (int i = 0; i < 8; i++) {
        song->snx_ramp_current[i] = 0xFF00u;
        song->snx_ramp_target[i] = 0xFF00u;
        song->snx_ramp_speed[i] = 0;
    }

    // Initialize TINY-specific state
    if (song->metadata.format == SONIX_FORMAT_TINY) {
        song->smus_prescaler = 0;
        song->smus_tpb = 1;
        song->smus_last_tempo_idx = 0xFFFFu; // force first update
        song->smus_tick_count = 0;
        song->smus_max_ticks = -1; // loop on all-ended
        for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
            song->smus_note_wait[ch] = 1;
            song->smus_release_wait[ch] = 0;
            song->smus_cur_inst[ch] = 0;
            song->smus_has_inst[ch] = false;
            song->smus_stream_pos[ch] = song->snx_track_start[ch];
            song->snx_track_ended[ch] = false;
        }
        // Pre-scan tracks for initial instrument (similar to smus_scan_tracks)
        for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
            u32 pos = song->snx_track_start[ch];
            u32 end = song->snx_track_end[ch];
            if (pos == 0 || pos >= end)
                continue;
            // Scan for initial 0x81xx instrument command before first note/wait
            while (pos + 2 <= end) {
                u16 word = nuade_read_be16(song->data + pos);
                if (word == 0) {
                    pos += 2;
                    continue;
                }
                if (word == 0xFFFFu)
                    break;
                u8 hi = (u8)(word >> 8);
                u8 lo = (u8)(word & 0xFF);
                if (hi == 0x81u) {
                    song->smus_cur_inst[ch] = lo;
                    song->smus_has_inst[ch] = (lo < 64);
                    song->snx_cmd80[ch] = lo;
                    pos += 2;
                    continue;
                }
                break; // stop at first non-instrument command
            }
            song->smus_stream_pos[ch] = pos;
        }
        smus_update_tempo(song);
    }

    // Initialize SMUS-specific state
    if (song->metadata.format == SONIX_FORMAT_SMUS) {
        song->smus_prescaler = 0;
        song->smus_tpb = 1;
        song->smus_last_tempo_idx = 0xFFFFu; // force first update
        song->smus_tick_count = 0;
        song->smus_max_ticks = -1; // loop on all-ended
        for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
            smus_preload_track_velocity(song, ch);
            song->smus_note_wait[ch] = 0;
            song->smus_release_wait[ch] = 0;
            song->smus_velocity[ch] = 0;
            song->smus_velocity_init[ch] = 0;
            song->smus_cur_inst[ch] = 0;
            song->smus_has_inst[ch] = false;
            song->smus_stream_pos[ch] = song->snx_track_start[ch];
        }
        // Run scan and init
        smus_scan_tracks(song, 0);
        smus_init_channels(song);
        // Force initial tempo update
        smus_update_tempo(song);
    }

    song->debug_note_events = 0;
    song->debug_loop_resets++;
}

// Synttech period computation (assembly lbC01EC82-lbC01EC94).
// Note range: 36-107 (assembly $24-$6B). Period = 0xD5C8 * ratio[semi] >> (oct+17).
// Returns the unfolded period before the halving loop.
static u32 snx_synttech_period(int note) {
    int adj = note - 36;
    if (adj < 0)
        adj = 0;
    if (adj > 71)
        adj = 71;
    int oct = adj / 12;
    int semi = adj % 12;
    u32 period = (u32)0xD5C8u * (u32)sonix_ratio_table[semi];
    period >>= (oct + 17);
    return period;
}

// Sample increment for a 128-sample synth waveform cycle (used at note-on).
// At note-on, pcm_len=128. The assembly halves both period and DMA length by
// the decimation shift, so the frequency is always Paula_clock / (period * 4).
// rate = Paula_clock * 32 / period for pcm_len=128.

static f64 snx_synttech_sample_hz(int note) {
    u32 period = snx_synttech_period(note);
    if (period == 0)
        return 0.0;
    return NUADE_PAULA_CLOCK_PAL * 32.0 / (f64)period;
}

// Pitch offset used by sample/SS/IFF techs. SNX keeps it in 2(A6); SMUS uses 4(A6)
// (SNX1 word 1) because 2(A6) holds the tempo index there.
static u16 sonix_pitch_offset(const SonixSong* song) {
    bool smus = sonix_uses_score_engine(song);
    return smus ? song->smus_pitch_scaling : song->snx_pitch_offset;
}

// SStech note start (SMUS asm lbC0014F4 state 1, SNX asm lbC01F2.. state 1): base period and vibrato state.
// cmd83 pitch modulation of an integer period (asm lbC01E328 with table lbW01E3B2: MULU, ADD.L D0,D0, SWAP =
// period * table >> 15). index = ((cmd83 + 0x80) & 0xFF) >> 2, table[32] = 0x8000 = unity; cmd83 == 0 is skipped.
static u32 snx_apply_cmd83_period(const SonixSong* song, int ch, u32 period) {
    u8 raw = (u8)song->snx_cmd83[ch];
    if (raw == 0)
        return period;
    return (period * snx_pitch_mod_table[(u8)(raw + 0x80u) >> 2]) >> 15;
}

// SStech base period constant: SNX 0x1AB9 with the final scale >> 16, SMUS/TINY 8x that with >> 19.
static u32 ss_base_const(const SonixSong* song) {
    return sonix_uses_score_engine(song) ? 0xD5C8u : SNX_SS_BASE_PERIOD;
}

static u32 ss_period_shift(const SonixSong* song) {
    return sonix_uses_score_engine(song) ? 19u : 16u;
}

static void ss_note_on(SonixSong* song, int ch, u8 inst, int semitone) {
    u32 tpb = song->snx_ticks_per_beat;
    u32 delay = (u32)song->instrument_vib_delay[inst] << 15;

    song->ss_base_period[ch] = (u16)((ss_base_const(song) * sonix_ratio_table[semitone]) >> 15);
    song->ss_vib_index[ch] = 0;
    song->ss_vib_delay[ch] = (u16)((tpb > 0 ? delay / tpb : 0) >> 1);
}

// SStech per-tick pitch (SMUS asm lbC0014F4 from lbC00161C, SNX asm 3167-3216): triangle vibrato, then
// period = base * (0x1000 + vib*depth/128 - (pitch_offset - 0x80)) >> 19 (SMUS) / >> 16 (SNX).
static void ss_pitch_tick(SonixSong* song, int ch) {
    u8 inst = song->snx_inst_index[ch] & 63u;
    u32 tpb = song->snx_ticks_per_beat;
    u16 base = song->ss_base_period[ch];

    if (base == 0 || !song->snx_use_sample[ch])
        return;

    if (song->ss_vib_delay[ch] > 0) {
        song->ss_vib_delay[ch]--;
    } else {
        u32 step = (((u32)song->instrument_vib_speed[inst] * tpb) << 7) >> 16; // ASL.L #7 wraps in 32 bits
        song->ss_vib_index[ch] = (u16)(song->ss_vib_index[ch] + step + 0x40);
    }

    u16 v = (u16)((song->ss_vib_index[ch] >> 7) + 0x80);
    if (v & 0x100)
        v ^= 0xFF;
    v ^= 0x80;
    i16 vib = (i16)(-(i8)(u8)v);

    i16 mod = (i16)((i16)(vib * (i16)song->instrument_vib_depth[inst]) >> 7);
    u16 factor = (u16)(mod - ((i16)sonix_pitch_offset(song) - 0x80) + 0x1000);
    u32 period = ((u32)base * factor) >> ss_period_shift(song);
    if (song->metadata.format == SONIX_FORMAT_SNX)
        period = snx_apply_cmd83_period(song, ch, period);

    song->snx_sample_hz[ch] = (period > 0) ? NUADE_PAULA_CLOCK_PAL / (f64)period : 0.0;
}

// Voices: Paula's registers as the player holds them (active pcm, loop, period, volume) are handed to the framework's
// voices at the end of every tick; what the voices did in between (wrap, switch to the queued synth buffer, run off
// the end) is read back at the start of the next one.

static bool snx_is_synth_buf(const SonixSong* song, int ch, const i8* pcm) {
    return pcm != nullptr && (pcm == song->synth_buf[ch][0] || pcm == song->synth_buf[ch][1]);
}

static bool snx_loop_valid(u32 len, u32 loop_start, u32 loop_len) {
    return loop_len > 1 && loop_start < len && loop_start + loop_len <= len;
}

static NuadeSample snx_voice_sample(const i8* pcm, u32 len, u32 loop_start, u32 loop_len) {
    return (NuadeSample) { .data = pcm,
                           .format = NuadeSampleFormat_S8,
                           .length = len,
                           .loop_start = loop_start,
                           .loop_length = snx_loop_valid(len, loop_start, loop_len) ? loop_len : 0 };
}

// Left, right, right, left: the Amiga's hard stereo.
static void snx_set_pans(SonixSong* song) {
    if (song->voices == nullptr)
        return;
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        const bool left = (ch == 0 || ch == 3);
        nuade_voice_set_pan(song->voices, (u32)ch, left ? -1.0f : 1.0f);
    }
}

// An instrument slot a score can play: loaded sample/zones, a synth, or an SS instrument (which may have no
// octave in range and then drops every note, see sonix_start_note).
static bool snx_inst_loaded(const SonixSong* song, u8 inst) {
    return inst < 64
           && (song->instrument_pcm[inst] != nullptr || song->instrument_is_synth[inst] || song->instrument_is_ss[inst]
               || song->inst_zone_count[inst] > 0);
}

static void sonix_start_note(SonixSong* song, int ch, int note, int velocity) {
    u8 inst = song->snx_cmd80[ch] & 63u;

    // Save previous channel state before resetting.
    // Assembly StartNOTE reads 1(A1) to conditionally preserve portamento:
    //   state=0 (idle):    clear port_value + phase
    //   state=1 (playing): keep port_value + phase
    //   state=2 (released): keep port_value, clear phase
    bool prev_active = song->snx_use_sample[ch];
    bool prev_playing = prev_active && (song->ss_port_phase[ch] < 6);
    // A synth voice that is already running keeps its Paula DMA: the asm only writes the new AUDxLC/LEN/PER
    // and re-enables DMA (a no-op while it runs), so the old buffer plays on until its loop wraps.
    bool prev_synth_running = prev_active && song->instrument_is_synth[song->snx_inst_index[ch] & 63u]
                              && snx_is_synth_buf(song, ch, song->snx_active_pcm[ch]);
    u32 prev_pcm_len = song->snx_active_pcm_len[ch];

    // Assembly StartNOTE (asm 7143-7157): if the channel is busy and the new instrument belongs to a
    // different technique (SS / Synth / IFF / AIFF) the old note is stopped first (StopNOTE: status 0), so the
    // portamento state starts from scratch.
    if (prev_active && snx_inst_tech(song, song->snx_inst_index[ch] & 63u) != snx_inst_tech(song, inst)) {
        prev_active = false;
        prev_playing = false;
    }

    // Synttech (asm 7526-7535 / NP SynthesisFormat.cs): the note is a byte and anything outside
    // 36..107 is dropped; an already playing note keeps ticking, an idle channel stays silent.
    if (song->instrument_is_synth[inst] && song->synth_wave_set[inst]) {
        int note_byte = note & 0xFF;
        if (note_byte < 36 || note_byte >= 108) {
            if (!prev_active)
                song->snx_note_on[ch] = false;
            // The instrument pointer is stored before the note range check (asm lbC000B0E): the voice keeps
            // ticking with the new instrument's parameters.
            song->snx_inst_index[ch] = inst;
            return;
        }
        note = note_byte;
    }

    if (note < 1)
        note = 1;
    if (note > 127)
        note = 127;
    if (velocity < 0)
        velocity = 0;
    if (velocity > 255)
        velocity = 255;

    f64 hz = snx_note_to_hz(note);

    // Playback fields a cancelled note must leave untouched (the previous note keeps ticking).
    struct {
        bool note_on, use_sample;
        const i8* pcm;
        u32 pcm_len, loop_start, loop_len;
        f64 hz, base_hz, vib_phase;
        u16 vib_delay;
    } saved = { song->snx_note_on[ch],        song->snx_use_sample[ch],        song->snx_active_pcm[ch],
                song->snx_active_pcm_len[ch], song->snx_active_loop_start[ch], song->snx_active_loop_len[ch],
                song->snx_sample_hz[ch],      song->snx_sample_base_hz[ch],    song->snx_vib_phase[ch],
                song->snx_vib_delay[ch] };

    song->snx_note_on[ch] = true;
    song->snx_note_velocity[ch] = (u8)velocity;
    song->snx_inst_index[ch] = inst;
    song->snx_use_sample[ch] = false;
    song->snx_active_pcm[ch] = nullptr;
    song->snx_active_pcm_len[ch] = 0;
    song->snx_active_loop_start[ch] = 0;
    song->snx_active_loop_len[ch] = 0;
    song->snx_sample_hz[ch] = 0.0;
    song->snx_sample_base_hz[ch] = 0.0;
    song->snx_vib_phase[ch] = 0.0;
    song->snx_vib_delay[ch] = 0;
    // Check for multi-sample zones first (assembly: linked list search at lbC01F960)
    if (song->inst_zone_count[inst] > 0 || song->instrument_is_ss[inst]) {
        int zi;
        for (zi = 0; zi < song->inst_zone_count[inst]; zi++) {
            const SonixInstZone* z = &song->inst_zones[inst][zi];
            if (note >= z->low_key && note <= z->high_key && z->pcm != nullptr && z->pcm_len > 0) {
                f64 si;
                if (song->instrument_is_ss[inst] || song->instrument_is_iff[inst]) {
                    // SS/IFF tech pitch: period from note's semitone only.
                    // Raw period = (ratio * 0x358) >> 16, then pitch_offset scaling:
                    // IFFTech asm (lines 3455-3461): (0x1080 - pitch_offset) * period >> 12
                    // SSTech asm (lines 3200-3213): period * (0x1000 + vib - adj) >> 16
                    // Both yield the same result at no-vibrato.
                    int semitone = note % 12;
                    if (song->instrument_is_ss[inst]) {
                        ss_note_on(song, ch, inst, semitone);
                    }
                    u32 scale = (u32)(0x1080u - sonix_pitch_offset(song));
                    u32 final_period;
                    if (sonix_uses_score_engine(song)) {
                        // SMUS IFFtech (asm 8543-8570, 8621-8626): base = 0xD5C8 * ratio >> 15, period = scale * base
                        // >> 19
                        u32 base = ((u32)0xD5C8u * sonix_ratio_table[semitone]) >> 15;
                        final_period = (scale * base) >> 19;
                    } else if (song->instrument_is_ss[inst] && song->metadata.format == SONIX_FORMAT_SNX) {
                        // SNX SSTech (asm 3101-3106, 3214-3216): base = (0x1AB9 * ratio) >> 15, period = base *
                        // (0x1080 - pitch_offset) >> 16. 0x1AB9 is not exactly 8 * 0x358, so no pre-rounding.
                        u32 base = (SNX_SS_BASE_PERIOD * sonix_ratio_table[semitone]) >> 15;
                        final_period = (scale * base) >> 16;
                    } else {
                        u32 raw_period = ((u32)sonix_ratio_table[semitone] * SNX_SS_IFF_PITCH_SCALE) >> 16;
                        final_period = (scale * raw_period) >> 12;
                    }
                    if (final_period > 0) {
                        si = NUADE_PAULA_CLOCK_PAL / (f64)final_period;
                    } else {
                        si = 0.0;
                    }
                } else {
                    si = snx_compute_hw_sample_hz(z->source_rate, note, (int)z->base_note, sonix_pitch_offset(song));
                }
                if (si > SNX_MIN_SAMPLE_HZ) {
                    song->snx_use_sample[ch] = true;
                    song->snx_sample_hz[ch] = si;
                    song->snx_active_pcm[ch] = z->pcm;
                    song->snx_active_pcm_len[ch] = z->pcm_len;
                    song->snx_active_loop_start[ch] = z->loop_start;
                    song->snx_active_loop_len[ch] = z->loop_len;
                }
                break;
            }
        }
        // Assembly: if no zone matched, CLR.B 0(A1) cancels the note (lbC01F996); a note that was
        // already playing keeps ticking (with the new velocity and instrument).
        if (!song->snx_use_sample[ch]) {
            // A technique change already ran StopNOTE (prev_active false): the voice is silent, nothing keeps ticking.
            song->snx_note_on[ch] = saved.note_on && prev_active;
            song->snx_use_sample[ch] = saved.use_sample && prev_active;
            song->snx_active_pcm[ch] = saved.pcm;
            song->snx_active_pcm_len[ch] = saved.pcm_len;
            song->snx_active_loop_start[ch] = saved.loop_start;
            song->snx_active_loop_len[ch] = saved.loop_len;
            song->snx_sample_hz[ch] = saved.hz;
            song->snx_sample_base_hz[ch] = saved.base_hz;
            song->snx_vib_phase[ch] = saved.vib_phase;
            song->snx_vib_delay[ch] = saved.vib_delay;
            return;
        }
    } else if (song->instrument_is_synth[inst] && song->synth_wave_set[inst]) {
        // Synttech instrument: use 128-byte waveform with Synttech period formula.
        // Assembly lbC01EC82: period = 0xD5C8 * ratio[semi] >> (oct+17), note -= 36.
        f64 si = snx_synttech_sample_hz(note);
        if (si > SNX_MIN_SAMPLE_HZ) {
            song->snx_use_sample[ch] = true;
            song->snx_sample_hz[ch] = si;
            // Point to the base waveform; per-tick rendering may override
            song->snx_active_pcm[ch] = song->synth_wave[inst];
            song->snx_active_pcm_len[ch] = 128;
            song->snx_active_loop_start[ch] = 0;
            song->snx_active_loop_len[ch] = 128;
            if (prev_synth_running) {
                song->snx_active_pcm[ch] = saved.pcm;
                song->snx_active_pcm_len[ch] = prev_pcm_len;
                song->snx_active_loop_len[ch] = prev_pcm_len;
            }
        }
        // Note-to-note period slide (assembly lines 7528-7548 SMUS, 2488-2522 SNX).
        // If there's a previous period, slide from old to new over N ticks.
        // If no previous period (first note), set directly.
        {
            u16 new_period = (u16)snx_synttech_period(note);
            u16 old_period = song->synth_cur_period[ch];
            if (old_period != 0) {
                // Compute slide duration from slide rate (assembly lines 7536-7542)
                u16 rate = song->synth_slide_rate[inst];
                u32 d = (u32)rate << 15;
                u16 tpb = song->snx_ticks_per_beat;
                u16 counter = (u16)((tpb > 0 ? d / tpb : 0) >> 3) + 1;
                i16 diff = (i16)new_period - (i16)old_period;
                i16 delta = (i16)(diff / (i16)counter);
                // Assembly adjusts start period: start = target - delta*counter
                u16 start = (u16)((i16)new_period - delta * (i16)counter);
                song->synth_cur_period[ch] = start;
                song->synth_slide_counter[ch] = counter;
                song->synth_slide_delta[ch] = delta;
            } else {
                // First note: set period directly (assembly line 7530-7531)
                song->synth_cur_period[ch] = new_period;
                song->synth_slide_counter[ch] = 0;
                song->synth_slide_delta[ch] = 0;
            }
        }
        // Initialize synthesis per-channel state (assembly lines 2516-2533)
        if (song->synth_c2[inst] == 0) {
            song->synth_blend_accum[ch] = 0;
        }
        // $18(A2) = 1 on note start (assembly line 2516)
        song->synth_ring_dir[ch] = 1;

        // Initialize synthesis envelope state (assembly lines 5519-5524).
        // If envelope loop mode != 0, start the envelope accumulator.
        if (song->synth_env_loop_mode[inst] != 0) {
            song->synth_env_accum[ch] = 0;
            // Delay = ((env_delay << 16) >> 1) / tpb >> 2 (assembly lines 5522-5523)
            {
                u16 tpb = song->snx_ticks_per_beat;
                u32 d = ((u32)song->synth_env_delay_init[inst] << 16) >> 1;
                song->synth_env_delay_ctr[ch] = (i16)(tpb > 0 ? (d / tpb) >> 2 : 0);
            }
            song->synth_env_value[ch] = (i16)(i8)song->synth_env_table[inst][0];
        } else {
            // Mode 0: asm only clears the delay ($12(A2)); the table index and value ($10/$14) keep their
            // previous contents and the LFO keeps stepping every tick (wrapping).
            song->synth_env_delay_ctr[ch] = 0;
        }
    } else if (song->instrument_pcm[inst] != nullptr && song->instrument_pcm_len[inst] > 0) {
        u8 base = song->instrument_base_note[inst] ? song->instrument_base_note[inst] : 60u;
        if (song->instrument_source_rate[inst] >= 4000u) {
            // AIFF instruments: source_rate from COMM chunk.
            // Replicate the assembly's exact integer period computation
            // (AIFFTech at lbC01F944 + lbC01FA6A).
            f64 si = snx_compute_hw_sample_hz(song->instrument_source_rate[inst], note, (int)base,
                                              sonix_pitch_offset(song));
            if (si > SNX_MIN_SAMPLE_HZ) {
                song->snx_use_sample[ch] = true;
                song->snx_sample_hz[ch] = si;
            }
        } else if (song->instrument_base_period[inst] > 0) {
            // SampledSound instruments: base_period is a Paula period.
            // Derive source rate and use the same HW period computation.
            u32 bp = song->instrument_base_period[inst];
            u32 derived_rate = 0x369E99u / bp;
            f64 si = snx_compute_hw_sample_hz(derived_rate, note, (int)base, sonix_pitch_offset(song));
            if (si > SNX_MIN_SAMPLE_HZ) {
                song->snx_use_sample[ch] = true;
                song->snx_sample_hz[ch] = si;
            }
        } else {
            // Fallback for instruments without rate or period info.
            f64 base_hz = snx_note_to_hz((int)base);
            if (base_hz > 1.0) {
                song->snx_use_sample[ch] = true;
                song->snx_sample_hz[ch] = hz / base_hz * SNX_NOMINAL_RATE;
                if (song->snx_sample_hz[ch] < 0.02 * SNX_NOMINAL_RATE)
                    song->snx_sample_hz[ch] = 0.02 * SNX_NOMINAL_RATE;
                if (song->snx_sample_hz[ch] > 8.0 * SNX_NOMINAL_RATE)
                    song->snx_sample_hz[ch] = 8.0 * SNX_NOMINAL_RATE;
            }
        }
        if (song->snx_use_sample[ch] && song->snx_active_pcm[ch] == nullptr) {
            song->snx_active_pcm[ch] = song->instrument_pcm[inst];
            song->snx_active_pcm_len[ch] = song->instrument_pcm_len[inst];
            song->snx_active_loop_start[ch] = song->instrument_loop_start[inst];
            song->snx_active_loop_len[ch] = song->instrument_loop_len[inst];
        }
    }
    if (song->snx_use_sample[ch]) {
        song->snx_sample_base_hz[ch] = song->snx_sample_hz[ch];
        song->snx_vib_delay[ch] = song->instrument_vib_delay[inst];
    }
    // Initialize AIFF tech envelope for non-SS non-synth instruments.
    // Assembly (AIFFTech lbC01FA2C-lbC01FA60, lines 3785-3798):
    // StartNote sets 10(A1)=0 (D3=0) before dispatching to tech.
    // Tech handler: tech_vol = 0 (attack starts from silence).
    // attack_speed = 0xFF00 / (attack_time + 1)
    // decay_speed = 0xFF00 / (decay_time + 1)
    if (song->snx_use_sample[ch] && song->instrument_is_iff[inst]) {
        // IFF tech: instant volume from VHDR field. Just set the sustain flag.
        song->aiff_note_active[ch] = true;
    } else if (song->snx_use_sample[ch] && !song->instrument_is_ss[inst] && !song->instrument_is_synth[inst]) {
        u16 atk_time = 0, dec_time = 0;
        // Get attack/decay from matched zone or per-instrument defaults
        if (song->inst_zone_count[inst] > 0) {
            int zi;
            for (zi = 0; zi < song->inst_zone_count[inst]; zi++) {
                const SonixInstZone* z = &song->inst_zones[inst][zi];
                if (note >= z->low_key && note <= z->high_key) {
                    atk_time = z->attack_time;
                    dec_time = z->decay_time;
                    break;
                }
            }
        } else {
            atk_time = song->inst_attack_time[inst];
            dec_time = song->inst_decay_time[inst];
        }
        // Assembly: StartNote always clears 10(A1) to 0 before tech dispatch,
        // so tech_vol always starts at 0 (attack from silence).
        song->aiff_tech_vol[ch] = 0;
        song->aiff_attack_speed[ch] = (u16)(0xFF00u / ((u32)atk_time + 1));
        song->aiff_decay_speed[ch] = (u16)(0xFF00u / ((u32)dec_time + 1));
        song->aiff_note_active[ch] = true;
    }
    song->snx_hw_vol[ch] = snx_compute_hw_vol(song->snx_cmd81[ch], song->snx_note_velocity[ch]);
    // A synth note on a running synth voice does not restart DMA; any other note does (after StopNOTE if the tech
    // changed).
    if (song->snx_use_sample[ch] && !(prev_synth_running && song->instrument_is_synth[inst])) {
        song->voice_play[ch] = true;
    }
    song->debug_note_events++;
    song->debug_total_note_events++;

    // Initialize portamento envelope on note start (SS and Synth techs).
    // SMUS SStech (lines 8203-8208): conditional clearing based on prev status.
    // SNX SSTech (lines 3124-3125): unconditional clearing.
    // prev_playing maps to status==1, prev_active maps to status!=0.
    if (song->instrument_is_ss[inst] || song->instrument_is_synth[inst]) {
        // SNX SSTech (asm 3123-3124, flag 10(A1) is always 0 from the SNX player): value and phase restart.
        bool restart_port = song->metadata.format == SONIX_FORMAT_SNX && song->instrument_is_ss[inst];
        if (restart_port || !prev_playing) {
            song->ss_port_phase[ch] = 0;
        }
        if (restart_port || !prev_active) {
            song->ss_port_value[ch] = 0;
        }
    }
}

static void sonix_release_note(SonixSong* song, int ch) {
    song->snx_pending_start[ch].valid = false; // asm ReleaseNOTE: CLR.B 0(A1) drops a not yet started note
    u8 inst = song->snx_inst_index[ch] & 63u;
    if (song->snx_use_sample[ch] && (song->instrument_is_ss[inst] || song->instrument_is_synth[inst])) {
        // Assembly: ReleaseNote sets status=2 for ALL tech instruments (tech_type==1).
        // Both SS and Synth techs enter release phase (6) to ramp volume to 0.
        song->ss_port_phase[ch] = 6;
    } else if (song->snx_use_sample[ch] && !song->instrument_is_ss[inst] && !song->instrument_is_synth[inst]) {
        // AIFF tech: enter decay phase (assembly: ReleaseNote sets 0(A1)=2,
        // then next tick's AIFFTech clears 10(A1) → decay ramp begins).
        song->aiff_note_active[ch] = false;
    } else {
        song->snx_note_on[ch] = false;
        song->snx_hw_vol[ch] = 0;
    }
}

static void snx_queue_note_start(SonixSong* song, int ch, int note, int velocity) {
    // Assembly StartNote writes the volume (8(A1)) as soon as the track engine reads the note; only the tech
    // start is deferred. A release in the same pass drops the start but the voice keeps the new volume.
    song->snx_hw_vol[ch] = snx_compute_hw_vol(song->snx_cmd81[ch], (u8)velocity);
    song->snx_pending_start[ch].valid = true;
    song->snx_pending_start[ch].note = note;
    song->snx_pending_start[ch].velocity = velocity;
    song->snx_pending_start[ch].cmd80 = song->snx_cmd80[ch];
    song->snx_pending_start[ch].cmd81 = song->snx_cmd81[ch];
}

// Instrument-tick stage: start the note requested during the track pass, with the instrument and volume
// commands that were current when the request was made.
static void snx_flush_note_start(SonixSong* song, int ch) {
    if (!song->snx_pending_start[ch].valid)
        return;
    song->snx_pending_start[ch].valid = false;
    u8 cmd80 = song->snx_cmd80[ch];
    u8 cmd81 = song->snx_cmd81[ch];
    song->snx_cmd80[ch] = song->snx_pending_start[ch].cmd80;
    song->snx_cmd81[ch] = song->snx_pending_start[ch].cmd81;
    song->shown_note[ch] = song->snx_pending_start[ch].note;
    song->shown_inst[ch] = song->snx_cmd80[ch] & 63u;
    sonix_start_note(song, ch, song->snx_pending_start[ch].note, song->snx_pending_start[ch].velocity);
    song->snx_cmd80[ch] = cmd80;
    song->snx_cmd81[ch] = cmd81;
}

static void snx_update_sample_mod_tick(SonixSong* song, int ch) {
    if (!song->snx_use_sample[ch] || song->snx_sample_base_hz[ch] <= 0.0) {
        return;
    }

    u8 inst = song->snx_inst_index[ch] & 63u;
    u16 depth = song->instrument_vib_depth[inst];
    u16 speed = song->instrument_vib_speed[inst];
    if (song->instrument_is_ss[inst]) {
        ss_pitch_tick(song, ch); // includes cmd83 (integer period)
        return;
    } else if (depth == 0 || speed == 0) {
        song->snx_sample_hz[ch] = song->snx_sample_base_hz[ch];
    } else if (song->snx_vib_delay[ch] > 0) {
        song->snx_vib_delay[ch]--;
        song->snx_sample_hz[ch] = song->snx_sample_base_hz[ch];
    } else {
        f64 m;
        song->snx_vib_phase[ch] += (f64)speed * 0.015;
        m = sin(song->snx_vib_phase[ch]);
        song->snx_sample_hz[ch] = song->snx_sample_base_hz[ch] * (1.0 + m * ((f64)depth * 0.0015));
    }

    // cmd83 pitch modulation (assembly: lbC01E328) on the integer period the tech produced.
    if (song->snx_cmd83[ch] != 0 && song->snx_sample_hz[ch] > 0.0) {
        u32 period = (u32)(NUADE_PAULA_CLOCK_PAL / song->snx_sample_hz[ch] + 0.5);
        period = snx_apply_cmd83_period(song, ch, period);
        song->snx_sample_hz[ch] = (period > 0) ? NUADE_PAULA_CLOCK_PAL / (f64)period : 0.0;
    }
}

static void snx_process_channel_tick(SonixSong* song, int ch) {
    int guard = 1024;

    if (song->snx_track_ended[ch]) {
        return;
    }

    // NOTE: snx_update_sample_mod_tick is called AFTER track commands
    // (from snx_process_tick), matching assembly PlaySNX order:
    // 1. lbC01E7DA: track engine for all channels (sets cmd83, volume, etc.)
    // 2. lbC01E996: instrument ticks for all channels (applies cmd83, vibrato)

    if (song->snx_wait[ch] > 0) {
        song->snx_wait[ch]--;
        if (song->snx_wait[ch] > 0) {
            return;
        }
        // Assembly pattern: TST.L, SUBQ.L #1, BNE - when counter
        // decrements to 0 it falls through to process commands.
    }

    while (guard-- > 0) {
        u32 pos = song->snx_track_pos[ch];
        if (pos + 2 > song->snx_track_end[ch]) {
            song->snx_track_ended[ch] = true;
            sonix_release_note(song, ch);
            return;
        }

        u16 cmd = nuade_read_be16(song->data + pos);
        song->snx_track_pos[ch] = pos + 2;

        if (cmd == 0) {
            continue;
        }

        if (cmd == 0xFFFFu) {
            // Assembly: CLR.L $3B2(A5) - end this channel's track.
            // Song-level looping happens when all 4 channels have ended.
            song->snx_track_ended[ch] = true;
            sonix_release_note(song, ch);
            return;
        }

        if ((cmd & 0xC000u) == 0xC000u) {
            song->snx_wait[ch] = (u32)(cmd & 0x3FFFu);
            song->debug_wait_commands++;
            if (song->snx_wait[ch] == 0) {
                continue;
            }
            return;
        }

        u8 op_hi = (u8)(cmd >> 8);
        u8 op_lo = (u8)(cmd & 0xFFu);

        if ((op_hi & 0x80u) != 0) {
            if (op_hi == 0x81u) {
                song->snx_cmd81[ch] = op_lo;
            } else if (op_hi == 0x80u) {
                song->snx_cmd80[ch] = op_lo;
            } else if (op_hi == 0x83u) {
                song->snx_cmd83[ch] = (i8)op_lo;
            } else if (op_hi == 0x82u) {
                if (op_lo > 0) {
                    song->tempo_cur = op_lo;
                    snx_update_tick_timing(song);
                }
            }
            // Assembly: all cmd80-83 handlers branch back to lbC01E820
            // (read next command). They do NOT consume a tick.
            continue;
        }

        {
            int vel = (int)op_lo;
            int note = (int)op_hi;

            // Assembly (lbC01E8E2): TST.B D3 / BNE / BSR.L ReleaseNote
            // velocity 0 always means release, then continue reading.
            if (vel == 0) {
                sonix_release_note(song, ch);
                continue;
            }

            snx_queue_note_start(song, ch, note, vel);
        }
    }
}

// Synthesis envelope tick: updates the per-channel envelope accumulator.
// Assembly lines 5538-5554 (Ghidra thunk_FUN_00220a46, around puVar15[8-10]).
// The envelope scans a 128-byte table at instrument $A4 (file 0xC4).
// Accumulator advances each tick; table index = accum >> 8.
// The resulting value modulates filter cutoff, volume, and pitch.
static void snx_synth_envelope_tick(SonixSong* song, int ch) {
    u8 inst = song->snx_inst_index[ch] & 63u;

    if (!song->instrument_is_synth[inst] || song->synth_cur_period[ch] == 0)
        return;

    i16 delay = song->synth_env_delay_ctr[ch];
    if (delay < 0) {
        // Expired (0xFFFF) - no more updates
        return;
    }
    if (delay > 0) {
        // Counting down delay
        song->synth_env_delay_ctr[ch] = delay - 1;
        return;
    }
    // delay == 0: advance the envelope accumulator
    {
        u16 rate = song->synth_env_scan_rate[inst];
        u16 tpb = song->snx_ticks_per_beat;
        u16 delta = (u16)((u32)rate * (u32)tpb * 0x40u >> 16);
        u16 accum = song->synth_env_accum[ch];
        u16 new_accum = accum + delta;

        bool one_shot = song->synth_env_loop_mode[inst] > 0; // loop mode <= 0 wraps freely
        if (sonix_uses_score_engine(song)) {
            // SMUS asm lbC000F78-lbC000F9C: a carry expires the envelope (delay = 0xFFFF) and leaves the accumulator
            // and the latched value as they are.
            if (one_shot && new_accum < accum) {
                song->synth_env_delay_ctr[ch] = -1;
                return;
            }
        } else if (one_shot && (new_accum < accum || new_accum >= 0xFE00u)) {
            // SNX asm lbC01ED34-lbC01ED62: a carry or reaching 0xFE00 expires the envelope and clamps the
            // accumulator to 0xFE00, whose table entry is still latched.
            song->synth_env_delay_ctr[ch] = -1;
            new_accum = 0xFE00u;
        }
        song->synth_env_accum[ch] = new_accum;
        song->synth_env_value[ch] = (i16)(i8)song->synth_env_table[inst][new_accum >> 8];
    }
}

// Synttech pitch envelope modulation.
// Assembly lines 2626-2638: modulates the DMA period based on envelope value
// and the instrument's pitch_scale parameter ($1B4/file 0x1D4).
// Formula: mod = (env_val * pitch_scale) >> 7 - (pitch_offset - 0x80) + 0x1000
//          modulated_period = (base_period * mod) >> 12
// Only updates sample_inc; synth_cur_period stays unmodified (used for decimation).
static void snx_synth_pitch_modulate(SonixSong* song, int ch) {
    u8 inst = song->snx_inst_index[ch] & 63u;

    if (!song->instrument_is_synth[inst])
        return;
    if (!song->snx_use_sample[ch])
        return;

    // Assembly lines 7662-7673 SMUS / 2626-2638 SNX:
    // This runs unconditionally for synth instruments (even when pitch_scale=0).
    // With pitch_scale=0, mod=0 so factor=0x1000-pitch_adj which still applies
    // the pitch offset adjustment to the period.
    u16 pitch_scale = song->synth_env_pitch_scale[inst];
    i16 env_val = song->synth_env_value[ch];
    u16 base_period = song->synth_cur_period[ch];

    // Assembly MULS.W + ASR.W #7
    i16 mod = (i16)((i16)(env_val * (i16)pitch_scale) >> 7);

    // Assembly SMUS path: MOVE.W 4(A6),D2; SUBI.W #$80,D2; SUB.W D2,D1
    i16 pitch_adj = (i16)song->smus_pitch_scaling - 0x80;
    mod -= pitch_adj;

    // Assembly: ADDI.W #$1000,D1
    i16 factor = mod + 0x1000;

    // Assembly (7708-7735): the base period is halved down to the Paula range first, then the
    // integer factor is applied: period = ((base >> halvings) * factor) >> 12.
    u32 halvings = 5u - song->synth_decimation_shift[ch];
    u32 hw_period = (((u32)base_period >> halvings) * (u16)factor) >> 12;
    if (hw_period == 0)
        hw_period = 1;

    // Paula plays hw_period-clocked bytes of the (128 >> shift)-byte buffer.
    song->snx_sample_hz[ch] = NUADE_PAULA_CLOCK_PAL / (f64)hw_period;
}

// Synttech per-tick waveform rendering.
// Assembly lines 2699-2808: three modes based on $1C2/$1C4.
// Regenerates the 128-byte output waveform from the base wave each tick.
static void snx_synth_render_tick(SonixSong* song, int ch) {
    u8 inst = song->snx_inst_index[ch] & 63u;

    if (!song->instrument_is_synth[inst] || !song->synth_wave_set[inst])
        return;
    // The asm ticks the synth every CIA tick, also after note-off (volume is 0 then), so the blend/ring
    // accumulator, envelope and slide keep running into the next note. Only skip before the first note.
    if (song->synth_cur_period[ch] == 0)
        return;
    bool audible = song->snx_use_sample[ch] && song->snx_note_on[ch];

    // Select waveform source from filter bank (assembly lines 5605-5614).
    // Compute filter offset from envelope, filter_base, filter_range.
    // If filter bank exists, use the selected filtered waveform.
    const i8* wave;
    if (song->synth_filter_bank[inst] != nullptr) {
        i16 env_val = song->synth_env_value[ch];
        u16 fbase = song->synth_filter_base[inst];
        u16 frange = song->synth_filter_range[inst];
        u16 fenv = song->synth_filter_env_sens[inst];
        u16 port_hi = (u16)(song->ss_port_value[ch] >> 16);
        i16 filt;
        u16 band;

        // filter_offset = ((env * filter_env) >> 8) + ((base ^ 0xFF) - (port_hi * range >> 8))
        filt = (i16)(((i16)(env_val * (i16)fenv) >> 8) + (i16)((fbase ^ 0xFFu) - (((u32)port_hi * (u32)frange) >> 8)));
        band = (u16)((u16)filt & 0xFFu) >> 2; // 0-63
        if (band > 63)
            band = 63;
        wave = song->synth_filter_bank[inst] + band * 128;
    } else {
        wave = song->synth_wave[inst];
    }

    // The asm indexes the source with a 16-bit offset and no wrap in ring mode: the second half can read
    // wave[128], the byte right after the 128-byte wave. That is the first byte of the next filter band
    // (see snx_filter_bank_byte_after) or, without a filter bank, the envelope table ($A4).
    const i8 wave_128 = song->synth_filter_bank[inst] != nullptr ? snx_filter_bank_byte_after(song, inst, wave)
                                                                 : song->synth_env_table[inst][0];

    u16 c2 = song->synth_c2[inst];
    u16 c4 = song->synth_c4[inst];
    u16 tpb = song->snx_ticks_per_beat;

    // Per-tick period slide (assembly lines 7640-7646 SMUS, 2609-2615 SNX).
    // If slide counter > 0, advance period toward target.
    if (song->synth_slide_counter[ch] > 0) {
        song->synth_slide_counter[ch]--;
        song->synth_cur_period[ch] = (u16)((i16)song->synth_cur_period[ch] + song->synth_slide_delta[ch]);
    }

    // Compute decimation shift from current period (assembly lines 2607-2622).
    // Initial shift=5, halve period while > 0x1AC, decrement shift each time.
    // This determines how many source bytes to skip per output byte.
    // The shift also sets the Paula DMA length: 64 >> shift words = 128 >> shift bytes.
    {
        u16 per = song->synth_cur_period[ch];
        int shift = 5;
        while (per > 0x1ACu && shift > 0) {
            per >>= 1;
            shift--;
        }
        song->synth_decimation_shift[ch] = (u8)shift;
        // step = 1 << shift; output count = 128 >> shift

        if (c2 == 0) {
            // Simple copy with decimation (assembly lines 2707-2715):
            // Read every step-th sample from source, write one byte per iteration.
            // Output count = 128 >> shift bytes (= dma_bytes).
            u16 step = (u16)(1 << shift);
            u16 count = 128u >> shift;
            int src = 0;
            for (int i = 0; i < (int)count; i++) {
                song->synth_output[ch][i] = wave[src & 127];
                src += step;
            }
        } else if (c4 == 0) {
            // Blend mode with decimation (assembly lines 2718-2754):
            // Accumulator += (c2 * tpb) >> 13. Offset = accum >> 9.
            // Read source and offset with step, write one byte per iteration.
            u16 delta = (u16)(((u32)c2 * (u32)tpb) >> 13);
            u16 accum = (u16)song->synth_blend_accum[ch] + delta;
            u16 offset;
            u16 step = (u16)(1 << shift);
            u16 count = 128u >> shift;
            int src = 0;
            song->synth_blend_accum[ch] = (i16)accum;
            offset = accum >> 9;
            for (int i = 0; i < (int)count; i++) {
                i16 a = (i16)wave[src & 127];
                i16 b = (i16)wave[(src + offset) & 127];
                song->synth_output[ch][i] = (i8)((a + b) >> 1);
                src += step;
            }
        } else {
            // Ring modulation (SMUS asm lbC00119E-lbC001220, SNX lines 2756-2808):
            // 16-bit accumulator oscillates; overflow wraps, then the wrapped value is negated
            // and the direction reverses. Output is 2 * (64 >> shift) bytes: each 64-byte source
            // half is Bresenham-resampled to (words +/- mod) bytes.
            u16 rate = (u16)(((u32)c2 * (u32)tpb) >> 11);
            i16 dir = song->synth_ring_dir[ch];
            i16 step_val = (i16)((i32)rate * dir);
            i16 accum = (i16)song->synth_blend_accum[ch];
            i32 sum = (i32)accum + step_val;
            i16 wrapped = (i16)sum;
            int out_pos = 0;

            if (sum != wrapped) {
                if (wrapped == (i16)0x8000)
                    wrapped = (i16)(wrapped + dir);
                dir = (i16)-dir;
                wrapped = (i16)-wrapped;
                song->synth_ring_dir[ch] = dir;
            }
            song->synth_blend_accum[ch] = wrapped;

            i32 mod = ((i32)wrapped * (i32)(i16)c4) >> (17 + shift);
            i32 words = 64 >> shift;
            i32 half[2] = { words + mod, words - mod };

            // The resampler's ADDX takes the 68k X flag. The UADE CPU core's SUB (and NEG) never write X, so X is
            // only set by the ADDs: the carry of "ADD.W mod,words" before the first half (set whenever mod < 0),
            // the carry of the err refill on a borrow, and the carry of every index ADDX.
            bool x = (u32)(u16)words + (u32)(u16)mod > 0xFFFFu;
            for (int h = 0; h < 2; h++) {
                u32 count = (u32)(u16)half[h];
                if (count == 0)
                    continue;
                u32 quot = 0x40u / count;
                u32 rem = 0x40u % count;
                u32 src = (u32)h * 0x40u;
                u16 err = 0;
                for (u32 i = 0; i < count && out_pos < 128; i++) {
                    song->synth_output[ch][out_pos++] = src == 128 ? wave_128 : wave[src & 127];
                    bool borrow = err < rem;
                    err = (u16)(err - rem);
                    if (borrow) {
                        x = (u32)err + count > 0xFFFFu;
                        err = (u16)(err + count);
                    }
                    u32 next = (src & 0xFFFFu) + quot + (x ? 1u : 0u);
                    x = next > 0xFFFFu;
                    src = next & 0xFFFFu;
                }
            }
            while (out_pos < 128) {
                song->synth_output[ch][out_pos++] = 0;
            }
        }
    }

    // Hand the rendered buffer to Paula.
    // Assembly sets DMA length = 64 >> shift words = 128 >> shift bytes.
    // Paula loops only this portion of the output buffer.
    // sample_hz is scaled accordingly in snx_synth_pitch_modulate.
    //
    // The asm alternates two chip buffers, and Paula latches a new AUDxLC/LEN only when the current loop wraps. So
    // the voice starts the new buffer at once when the channel is not already looping a synth buffer (note start), and
    // otherwise queues it behind the one that plays. A queued buffer keeps following the latest render until it takes
    // over.
    {
        const bool looping = song->snx_use_sample[ch] && snx_is_synth_buf(song, ch, song->snx_active_pcm[ch]);
        const bool refresh = !audible && looping && song->synth_pending[ch];
        if (!audible && !refresh)
            return;

        NuadeVoiceState state = { 0 };
        if (song->voices != nullptr)
            nuade_voice_get_state(song->voices, (u32)ch, &state);

        // The buffer that is not playing: the queued one, or the other one to start with.
        const bool playing_first = state.playing && state.sample->data == song->synth_buf[ch][0];
        const u32 index = playing_first ? 1u : 0u;
        const u32 dma_bytes = 128u >> song->synth_decimation_shift[ch];
        memcpy(song->synth_buf[ch][index], song->synth_output[ch], sizeof(song->synth_buf[ch][index]));

        const f64 position = song->voice_play[ch] ? 0.0 : state.position;
        if (!refresh && (!looping || position == 0.0)) {
            song->synth_pending[ch] = false;
            song->snx_active_pcm[ch] = song->synth_buf[ch][index];
            song->snx_active_pcm_len[ch] = dma_bytes;
            song->snx_active_loop_start[ch] = 0;
            song->snx_active_loop_len[ch] = dma_bytes;
            song->voice_play[ch] = true;
        } else {
            song->synth_pending[ch] = true;
            song->voice_queued[ch] = snx_voice_sample(song->synth_buf[ch][index], dma_bytes, 0, dma_bytes);
            song->voice_queue[ch] = true;
        }
    }
}

// AIFF tech volume envelope: per-tick processing.
// Assembly: AIFFTech at lbC01FA6A (lines 3814-3853).
// Attack: ramp 2(A2) from 0 toward 0xFF00 using attack speed.
// Decay: ramp 2(A2) from current toward 0 using decay speed.
// Step = (speed * ticks_per_beat * 2) >> 16.
// When tech_vol reaches 0 during decay, the note is killed.
static void snx_aiff_tech_tick(SonixSong* song, int ch) {
    u8 inst = song->snx_inst_index[ch] & 63u;

    if (!song->snx_use_sample[ch] || song->instrument_is_ss[inst] || song->instrument_is_synth[inst]
        || song->instrument_is_iff[inst]) {
        return;
    }

    u16 tpb = song->snx_ticks_per_beat;
    u16 current = song->aiff_tech_vol[ch];

    u32 step_raw;
    u16 step;

    if (song->aiff_note_active[ch]) {
        // Attack phase: ramp toward 0xFF00
        if (current >= 0xFF00u) {
            song->aiff_tech_vol[ch] = 0xFF00u;
            return;
        }
        step_raw = (u32)song->aiff_attack_speed[ch] * (u32)tpb;
        step_raw += step_raw; // *2, assembly: ADD.L D0,D0
        if (step_raw < (u32)song->aiff_attack_speed[ch] * (u32)tpb) {
            // Overflow (carry) → clamp to max
            song->aiff_tech_vol[ch] = 0xFF00u;
            return;
        }
        step = (u16)(step_raw >> 16);
        {
            u32 sum = (u32)current + (u32)step;
            if (sum > 0xFFFFu || (u16)sum >= 0xFF00u) {
                song->aiff_tech_vol[ch] = 0xFF00u;
            } else {
                song->aiff_tech_vol[ch] = (u16)sum;
            }
        }
    } else {
        // Decay phase: ramp toward 0
        if (current == 0) {
            // Assembly: CLR.L $1C(A1): end the note
            song->snx_note_on[ch] = false;
            return;
        }
        step_raw = (u32)song->aiff_decay_speed[ch] * (u32)tpb;
        step_raw += step_raw; // *2
        if (step_raw < (u32)song->aiff_decay_speed[ch] * (u32)tpb) {
            // Overflow → clamp to 0
            song->aiff_tech_vol[ch] = 0;
            song->snx_note_on[ch] = false;
            return;
        }
        step = (u16)(step_raw >> 16);
        if (step >= current) {
            song->aiff_tech_vol[ch] = 0;
            song->snx_note_on[ch] = false;
        } else {
            song->aiff_tech_vol[ch] = current - step;
        }
    }
}

// SS portamento envelope: per-tick processing.
// Assembly: 4-stage envelope (phases 0,2,4,6) that modulates volume.
// Speed decode: bits 0-4 = base, bits 5-7 = shift.
// step = (base+0x21) * ticks_per_beat * 8 >> (shift ^ 7).
static void snx_ss_portamento_tick(SonixSong* song, int ch) {
    u8 inst = song->snx_inst_index[ch] & 63u;

    if (!song->instrument_is_ss[inst] && !song->instrument_is_synth[inst])
        return;

    u16 phase = song->ss_port_phase[ch];
    u16 phase_idx = phase >> 1; // 0,1,2,3
    if (phase_idx > 3)
        phase_idx = 3;

    // Target: word from instrument, convert to 16.16
    u32 target = (u32)song->ss_port_target[inst][phase_idx] << 16;

    // Speed decode: bits 0-4 = base, bits 5-7 = shift
    u16 spd_param = song->ss_port_speed[inst][phase_idx];
    u32 shift = (spd_param >> 5) ^ 7;
    u32 base = (spd_param & 0x1Fu) + 0x21u;
    // A speed word above 0xff gives a shift of 32 or more, which empties the long.
    u32 step = shift < 32 ? (base * (u32)song->snx_ticks_per_beat * 8u) >> shift : 0;

    u32 current = song->ss_port_value[ch];
    u32 distance = (target >= current) ? (target - current) : (current - target);

    if (distance <= step) {
        current = target;
        if (phase < 4) {
            song->ss_port_phase[ch] = phase + 2;
        }
    } else {
        if (current > target)
            current -= step;
        else
            current += step;
    }

    song->ss_port_value[ch] = current;

    // Assembly: if port_val == 0 and in release phase, end note
    if (current == 0 && phase >= 6) {
        song->snx_note_on[ch] = false;
    }
}

// SMUS per-channel tick processing (port of lbC000874-lbC000934).
// Returns true if this channel has ended (0xFFFF encountered).
static bool smus_process_channel_tick(SonixSong* song, int ch) {
    int guard = 256;

    if (song->snx_track_ended[ch])
        return true;

    // Step 1: Release wait (assembly $23A check, lines 6897-6905)
    if (song->smus_release_wait[ch] > 0) {
        song->smus_release_wait[ch]--;
        if (song->smus_release_wait[ch] == 0 && song->smus_inst_present[ch]) {
            sonix_release_note(song, ch);
        }
        return false; // skip command reading
    }

    // Step 2: Note wait (assembly $24A check, lines 6907-6910)
    if (song->smus_note_wait[ch] > 0) {
        song->smus_note_wait[ch]--;
        if (song->smus_note_wait[ch] > 0)
            return false;
        // Falls through to command reading when wait reaches 0
    }

    // Step 3: Read commands from stream.
    // SMUS TRAK data uses compressed durations: the low nibble of notes/cmd80
    // indexes into smus_duration_table[] to get actual beat counts.
    // The assembly preprocesses this in LoadSCORE (lines 6533-6568).
    // We translate at read time since our data buffer is const.
    {
        u32 pos = song->smus_stream_pos[ch];
        u32 end = song->snx_track_end[ch];

        if (pos == 0 || pos >= end) {
            song->snx_track_ended[ch] = true;
            return true;
        }

        while (guard-- > 0 && pos + 2 <= end) {
            u16 word = nuade_read_be16(song->data + pos);
            pos += 2;

            if (word == 0)
                continue; // skip zeros
            if (word == 0xFFFFu) {
                // End of track
                song->smus_stream_pos[ch] = pos;
                song->snx_track_ended[ch] = true;
                return true;
            }

            {
                u8 hi = (u8)(word >> 8);
                u8 raw_lo = (u8)(word & 0xFF);

                if (hi >= 0x82u) {
                    // Assembly: 0x82xx sets tempo data at 8(A5) and 10(A5), then zeroes the word.
                    // 0x84xx sets per-channel velocity at $12(A4), then zeroes the word.
                    // Other >= 0x82: skip (zeroed in assembly).
                    if (hi == 0x82u) {
                        // Assembly lines 6548-6559: decode tempo control from low byte.
                        // 8(A5) = ((lo >> 3) & 0x1F) + 1   (divisor)
                        // 10(A5) = 1 << (lo & 7)            (bitmask)
                        // Note: these values are stored in the score structure during
                        // preprocessing. The UADE runtime does not appear to reference
                        // them, but we decode and store for completeness.
                        song->smus_tempo_divisor = (u16)(((raw_lo >> 3) & 0x1F) + 1);
                        song->smus_tempo_bitmask = (u16)(1u << (raw_lo & 7));
                    }
                    // These commands are zeroed during preprocessing, so skipped
                    continue;
                }

                if (hi == 0x81u) {
                    // Instrument select: use register number directly
                    song->smus_cur_inst[ch] = raw_lo;
                    song->smus_has_inst[ch] = (raw_lo < 64);
                    continue;
                }

                // Note (hi < 0x80) or cmd80 (hi == 0x80):
                // Translate low nibble through duration table.
                {
                    u8 dur_idx = raw_lo & 0x0Fu;
                    u8 duration = smus_duration_table[dur_idx];
                    if (duration == 0xFFu) {
                        // Invalid duration: word was zeroed in assembly preprocessing
                        continue;
                    }

                    song->smus_stream_pos[ch] = pos;

                    if (hi == 0x80u) {
                        // Wait command: translated duration -> note_wait
                        song->smus_note_wait[ch] = duration;
                        return false;
                    }

                    // Note: hi = note number, duration = translated beat count
                    {
                        // Assembly $3A(A5) check: channel must have instrument presence
                        // flag set (from SNX1 data) before any note can trigger.
                        if (!song->smus_inst_present[ch]) {
                            song->smus_note_wait[ch] = duration;
                            return false;
                        }

                        u8 inst = song->smus_cur_inst[ch];
                        bool has_instrument = song->smus_has_inst[ch] && snx_inst_loaded(song, inst);

                        if (!has_instrument) {
                            // No valid instrument: just set wait and done
                            song->smus_note_wait[ch] = duration;
                            return false;
                        }

                        // Compute note pitch: (pitch_word >> 4) - 8 + note_byte
                        {
                            i16 pitch = (i16)song->smus_note_pitch_word;
                            int computed_note;
                            int velocity;

                            pitch >>= 4; // arithmetic shift
                            pitch -= 8;
                            computed_note = (int)pitch + (int)hi;

                            // Set instrument for sonix_start_note
                            song->snx_cmd80[ch] = inst;

                            // Base velocity from 0x84xx command or default
                            velocity = song->smus_base_velocity[ch];
                            // Assembly: CMPI.W #1,$3C(A5) / BEQ _StartNOTE / LSR.W #1,D2
                            // Halve velocity when inst_present lower word != 1
                            if (song->smus_inst_present_raw[ch] != 1)
                                velocity >>= 1;

                            song->shown_note[ch] = hi;
                            song->shown_inst[ch] = inst;
                            sonix_start_note(song, ch, computed_note, velocity);
                        }

                        // Split duration into release_wait and note_wait.
                        // Assembly: init_shift = duration * 0xC000 >> 16 (≈ duration * 3/4).
                        // release_wait = init_shift, note_wait = duration - init_shift.
                        {
                            u16 init_shift = (u16)(((u32)duration * 0xC000u) >> 16);
                            song->smus_release_wait[ch] = init_shift;
                            song->smus_note_wait[ch] = duration - init_shift;
                        }
                        return false; // note consumes this beat
                    }
                }
            }
        }

        // If we exhausted the track data without finding a note/wait or 0xFFFF,
        // the track is done (handles tracks without explicit 0xFFFF terminator).
        if (pos + 2 > end) {
            song->smus_stream_pos[ch] = pos;
            song->snx_track_ended[ch] = true;
            return true;
        }
    }
    return false;
}

// SMUS top-level tick handler (port of lbC000846, assembly lines 6883-7004).
// Called once per CIA tick. The prescaler divides ticks into beats.
static void smus_process_tick(SonixSong* song) {
    song->tick_count++;

    // Ramp volume tick runs every tick (same as SNX)
    snx_ramp_volume_tick(song);

    // Prescaler check (assembly lines 6883-6886):
    // If prescaler > 0, decrement and return.
    if (song->smus_prescaler > 0) {
        song->smus_prescaler--;
        if (song->smus_prescaler > 0)
            goto instrument_ticks;
    }

    // Update tempo from pitch offset (assembly line 6887-6888)
    smus_update_tempo(song);

    // Check if song is playing (assembly: TST.W 10(A6), line 6889)
    if (!song->running)
        goto instrument_ticks;

    // Reload prescaler from tpb (assembly line 6891)
    song->smus_prescaler = song->smus_tpb;

    // Main beat processing loop with restart (assembly lines 6892-6995)
    int restarts = 0;
    for (;;) {
        u16 ended_count = 0;

        // Process all 4 channels
        for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
            if (smus_process_channel_tick(song, ch)) {
                ended_count++;
            }
        }

        // Increment tick count (assembly lines 6970-6971)
        song->smus_tick_count++;

        // Check song end conditions (assembly lines 6975-6982)
        if (song->smus_max_ticks < 0) {
            // Use all-ended check
            if (ended_count < 4)
                break; // not all ended, normal case
        } else {
            // Check against max tick count
            if (song->smus_tick_count < (u32)song->smus_max_ticks)
                break;
        }

        // Song ended: restart (assembly lines 6990-6995). Bail out if the restart ends again at once.
        if (++restarts > 4)
            break;
        song->debug_loop_resets++;
        smus_scan_tracks(song, 0);
        smus_init_channels(song);
    }

instrument_ticks:
    // Run instrument ticks for all channels (same as SNX, runs every CIA tick)
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        snx_update_sample_mod_tick(song, ch);
        snx_aiff_tech_tick(song, ch);
        snx_ss_portamento_tick(song, ch);
        snx_synth_envelope_tick(song, ch);
        snx_synth_render_tick(song, ch);
        snx_synth_pitch_modulate(song, ch);
    }
    song->debug_total_ticks++;
}

// TINY per-channel tick processing (port of assembly lines 4688-4757).
// Structurally identical to SMUS but uses raw durations (no duration table)
// and has no 0x82xx/0x84xx commands.
static bool tiny_process_channel_tick(SonixSong* song, int ch) {
    int guard = 256;

    if (song->snx_track_ended[ch])
        return true;

    // Step 1: Release wait
    if (song->smus_release_wait[ch] > 0) {
        song->smus_release_wait[ch]--;
        if (song->smus_release_wait[ch] == 0 && song->tiny_inst_present[ch]) {
            sonix_release_note(song, ch);
        }
        return false;
    }

    // Step 2: Note wait
    if (song->smus_note_wait[ch] > 0) {
        song->smus_note_wait[ch]--;
        if (song->smus_note_wait[ch] > 0)
            return false;
    }

    // Step 3: Read commands from stream
    {
        u32 pos = song->smus_stream_pos[ch];
        u32 end = song->snx_track_end[ch];

        if (pos == 0 || pos >= end) {
            song->snx_track_ended[ch] = true;
            return true;
        }

        while (guard-- > 0 && pos + 2 <= end) {
            u16 word = nuade_read_be16(song->data + pos);
            pos += 2;

            if (word == 0)
                continue;
            if (word == 0xFFFFu) {
                song->smus_stream_pos[ch] = pos;
                song->snx_track_ended[ch] = true;
                return true;
            }

            {
                u8 hi = (u8)(word >> 8);
                u8 lo = (u8)(word & 0xFF);

                if (hi >= 0x82u) {
                    // TINY has no 0x82xx/0x84xx commands, skip
                    continue;
                }

                if (hi == 0x81u) {
                    // Instrument select: lo indexes name table
                    song->smus_cur_inst[ch] = lo;
                    song->smus_has_inst[ch] = (lo < 64);
                    song->snx_cmd80[ch] = lo;
                    continue;
                }

                if (hi == 0x80u) {
                    // Wait command: raw lo byte is duration (no table lookup)
                    song->smus_stream_pos[ch] = pos;
                    song->smus_note_wait[ch] = lo;
                    return false;
                }

                // Note: hi = note number, lo = raw duration
                {
                    song->smus_stream_pos[ch] = pos;

                    if (!song->tiny_inst_present[ch]) {
                        song->smus_note_wait[ch] = lo;
                        return false;
                    }

                    u8 inst = song->smus_cur_inst[ch];
                    bool has_instrument = song->smus_has_inst[ch] && snx_inst_loaded(song, inst);

                    if (!has_instrument) {
                        song->smus_note_wait[ch] = lo;
                        return false;
                    }

                    // Compute note pitch: (pitch_word >> 4) - 8 + note_byte
                    {
                        i16 pitch = (i16)song->smus_note_pitch_word;
                        int computed_note;
                        int velocity;

                        pitch >>= 4;
                        pitch -= 8;
                        computed_note = (int)pitch + (int)hi;

                        song->snx_cmd80[ch] = inst;

                        velocity = song->smus_base_velocity[ch];
                        if (velocity == 0)
                            velocity = 0x80;
                        if (song->smus_inst_present_raw[ch] != 1)
                            velocity >>= 1;

                        song->shown_note[ch] = hi;
                        song->shown_inst[ch] = inst;
                        sonix_start_note(song, ch, computed_note, velocity);
                    }

                    // Duration split: release_wait = duration * 3/4, note_wait = remainder
                    {
                        u16 init_shift = (u16)(((u32)lo * 0xC000u) >> 16);
                        song->smus_release_wait[ch] = init_shift;
                        song->smus_note_wait[ch] = lo - init_shift;
                    }
                    return false;
                }
            }
        }

        if (pos + 2 > end) {
            song->smus_stream_pos[ch] = pos;
            song->snx_track_ended[ch] = true;
            return true;
        }
    }
    return false;
}

// TINY top-level tick handler (port of assembly lines 4644-4782).
// Identical structure to smus_process_tick: prescaler, tempo, channel ticks.
static void tiny_process_tick(SonixSong* song) {
    song->tick_count++;

    snx_ramp_volume_tick(song);

    if (song->smus_prescaler > 0) {
        song->smus_prescaler--;
        if (song->smus_prescaler > 0)
            goto instrument_ticks;
    }

    smus_update_tempo(song);

    if (!song->running)
        goto instrument_ticks;

    song->smus_prescaler = song->smus_tpb;

    int restarts = 0;
    for (;;) {
        u16 ended_count = 0;

        for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
            if (tiny_process_channel_tick(song, ch)) {
                ended_count++;
            }
        }

        song->smus_tick_count++;

        if (song->smus_max_ticks < 0) {
            if (ended_count < 4)
                break;
        } else {
            if (song->smus_tick_count < (u32)song->smus_max_ticks)
                break;
        }

        // Song ended: restart. A score that ends again straight after the restart (all tracks empty)
        // would never leave this loop, so give up on the tick after a few tries.
        if (++restarts > 4)
            break;
        song->debug_loop_resets++;
        // Reset track positions and state for TINY restart
        for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
            if (song->smus_has_inst[ch]) {
                sonix_release_note(song, ch);
            }
            song->smus_release_wait[ch] = 0;
            song->smus_note_wait[ch] = 1;
            song->smus_stream_pos[ch] = song->snx_track_start[ch];
            song->snx_track_ended[ch] = false;
        }
        song->smus_tick_count = 0;
    }

instrument_ticks:
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        snx_update_sample_mod_tick(song, ch);
        snx_aiff_tech_tick(song, ch);
        snx_ss_portamento_tick(song, ch);
        snx_synth_envelope_tick(song, ch);
        snx_synth_render_tick(song, ch);
        snx_synth_pitch_modulate(song, ch);
    }
    song->debug_total_ticks++;
}

static void snx_process_tick(SonixSong* song) {
    song->tick_count++;

    // Assembly PlaySNX: ramp tick is called first (BSR.L lbC01EAB8).
    snx_ramp_volume_tick(song);

    // Assembly PlaySNX order:
    // 1. lbC01EAB8: ramp volume (all channels) (done above)
    // 2. lbC01EB14: release note checks (all channels)
    // 3. lbC01E7DA: track engine (all channels' commands)
    // 4. lbC01E996: instrument ticks (all channels' vibrato, cmd83, tech)
    //
    // The track engine (step 3) contains a restart loop: when all 4 channels
    // end, it reinitializes them and jumps back to lbC01E802 to re-process
    // the channels in the same tick. Step 4 (instrument ticks) runs only
    // once, AFTER step 3 finishes (including any restart iterations).
    int restarts = 0;
    for (;;) {
        for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
            snx_process_channel_tick(song, ch);
        }

        // Assembly (lbC01E8A8/lbC01E8C0): when all channels have ended,
        // call SongEnd and reinitialize, then loop back to re-process.
        bool all_ended = true;
        for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
            if (!song->snx_track_ended[ch]) {
                all_ended = false;
                break;
            }
        }
        if (!all_ended)
            break;

        // Bail out if the restart ends again at once (tracks without a wait would never leave this loop).
        if (++restarts > 4)
            break;
        song->debug_loop_resets++;
        for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
            song->snx_track_pos[ch] = song->snx_track_start[ch];
            song->snx_track_ended[ch] = false;
            song->snx_wait[ch] = 0;
            sonix_release_note(song, ch);
        }
    }

    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        snx_flush_note_start(song, ch);
        snx_update_sample_mod_tick(song, ch);
        snx_aiff_tech_tick(song, ch);
        snx_ss_portamento_tick(song, ch);
        snx_synth_envelope_tick(song, ch);
        snx_synth_render_tick(song, ch);
        snx_synth_pitch_modulate(song, ch);
    }
    song->debug_total_ticks++;
}

// Value the asm hands to Paula (ChangeVolume: `and.w #$7F`), clamped by Paula to its 0..64 range, normalised to
// 0..0.5 (the mixer sums four channels).
static f32 snx_paula_volume(u32 v) {
    v &= 0x7Fu;
    if (v > 64u)
        v = 64u;
    return (f32)v / 64.0f;
}

// Raw value the asm hands to Paula as AUDxVOL for this channel (before the Paula clamp), per technique.
static u32 snx_channel_paula_vol(const SonixSong* song, int ch) {
    // Volume from ramp slot and hw_vol.
    // Assembly tech routines: paula_vol = ((ramp+1) * velocity) >> 8 ...
    // ramp_val is 8.8 fixed point; use the integer part (>> 8).
    u16 ramp_val = song->snx_ramp_current[ch];
    {
        u8 inst_idx = song->snx_inst_index[ch] & 63u;
        if (song->snx_use_sample[ch] && song->instrument_is_ss[inst_idx]) {
            u16 inst_v = song->ss_inst_vol[inst_idx];
            u16 port_hi = (u16)(song->ss_port_value[ch] >> 16);
            u32 v;
            if (sonix_uses_score_engine(song)) {
                // SMUS SStech volume (asm lines 8347-8359):
                // paula_vol = ((ramp_target+1)*velocity>>8 +1)*inst_vol>>8 * port_hi>>10
                // Result is 0-64 range (Paula), divide by 64*2 to normalize.
                u32 ramp_t = song->smus_ramp_target;
                u16 velocity = song->snx_note_velocity[ch];
                v = ((ramp_t + 1) * velocity) >> 8;
                v = ((v + 1) * inst_v) >> 8;
                v = (v * (u32)port_hi) >> 10;
                return v;
            } else {
                // SNX SS: lbC01E308 + SS tech volume.
                // ramp_component = (ramp_current * (hw_vol+1)) >> 16
                // paula_vol = ((ramp_component+1) * inst_vol >> 8) * port_hi >> 10
                // Result is 0-254 range, divide by 64*2 to normalize.
                u16 hw_v = song->snx_hw_vol[ch];
                u32 ramp_component = ((u32)ramp_val * (u32)(hw_v + 1)) >> 16;
                v = ((ramp_component + 1) * inst_v) >> 8;
                v = (v * (u32)port_hi) >> 10;
                return v;
            }
        } else if (song->snx_use_sample[ch] && song->instrument_is_synth[inst_idx]) {
            // SyntTech volume: assembly lines 2637-2664.
            // 1. base_vol, optionally modulated by envelope*vol_scale
            // 2. Optionally multiplied by portamento (if vol_mode!=0), or zeroed in release
            // 3. Combined ramp*hw_vol via lbC01E308: (ramp*(hw_vol+1))>>16
            // 4. Final: (ramp_combined * env_vol) >> 8, +1, >>2
            u16 base_v = song->synth_base_vol[inst_idx];
            u32 v;

            // Envelope -> volume modulation (asm lines 2637-2644):
            // if vol_scale != 0: base_v += (-env_val * vol_scale) >> 8
            {
                u16 vol_scale = song->synth_env_vol_scale[inst_idx];
                if (vol_scale != 0) {
                    i16 env_val = song->synth_env_value[ch];
                    i16 mod = (i16)((i16)(-env_val) * (i16)vol_scale >> 8);
                    base_v = (u16)((i16)base_v + mod);
                }
            }

            // Portamento modulation or release zero (asm lines 2645-2655)
            if (song->synth_port_flag[inst_idx]) {
                u16 port_hi = (u16)(song->ss_port_value[ch] >> 16);
                v = ((u32)base_v * (u32)port_hi) >> 8;
            } else {
                v = base_v;
                if (song->ss_port_phase[ch] == 6)
                    v = 0;
            }

            // Volume formula differs between SNX and SMUS:
            // SNX (asm lines 2658-2665): lbC01E308 combines ramp×hw_vol,
            //   then multiplies by base_v.
            // SMUS (asm lines 7687-7695): two separate multiplies:
            //   first by coarse_ramp (0(A6)), then by velocity (8(A1)).
            if (sonix_uses_score_engine(song)) {
                u32 coarse_ramp = song->smus_ramp_target;
                u16 velocity = song->snx_note_velocity[ch];
                v = (v & 0xFFu) + 1;
                v = (v * coarse_ramp) >> 8;
                v = ((v + 1) * velocity) >> 8;
            } else {
                u32 ramp_combined = ((u32)ramp_val * ((u32)song->snx_hw_vol[ch] + 1)) >> 16;
                v = (v & 0xFFu) + 1;
                v = (ramp_combined * v) >> 8;
            }
            v = (v + 1) >> 2;
            return v;
        } else if (song->snx_use_sample[ch] && song->instrument_is_iff[inst_idx]) {
            if (song->aiff_note_active[ch]) {
                u32 v;
                if (sonix_uses_score_engine(song)) {
                    // SMUS IFF tech volume (asm lines 8627-8638):
                    // v = ((ramp_target+1)*velocity>>8 +1) * (vhdr_vol/2) >> 17
                    // Result is 0-64 Paula range; divide by 64*2 to normalize.
                    u32 ramp_t = song->smus_ramp_target;
                    u16 velocity = song->snx_note_velocity[ch];
                    u32 vhdr_half = song->iff_vhdr_volume[inst_idx] >> 1;
                    v = ((ramp_t + 1) * velocity) >> 8;
                    v = ((v + 1) * (u16)vhdr_half) >> 17;
                    return v;
                } else {
                    // SNX IFF tech volume (asm IFFTech lines 3463-3472):
                    // ramp_component = (ramp_current * (hw_vol+1)) >> 16
                    // vol = (ramp_component + 1) * (vhdr_vol >> 1) >> 16 >> 1
                    // Result is 0-254 range; divide by 64*2 to normalize.
                    u32 ramp_combined = ((u32)ramp_val * ((u32)song->snx_hw_vol[ch] + 1)) >> 16;
                    u32 vhdr_half = song->iff_vhdr_volume[inst_idx] >> 1;
                    v = (ramp_combined + 1) * (u16)vhdr_half;
                    v >>= 16;
                    v >>= 1;
                    return v;
                }
            } else {
                return 0;
            }
        } else {
            // AIFF tech volume (assembly lbC01FAD6, lines 3846-3852):
            // ramp_combined = (ramp_current * (hw_vol + 1)) >> 16   [lbC01E308]
            // v = (ramp_combined + 1) * tech_vol >> 16 + 1, >> 2
            // Result is 0-64 Paula range; divide by 64*2 to normalize.
            u32 ramp_combined = ((u32)ramp_val * ((u32)song->snx_hw_vol[ch] + 1)) >> 16;
            u32 v = (ramp_combined + 1) * (u32)song->aiff_tech_vol[ch];
            v >>= 16;
            v = (v + 1) >> 2;
            return v;
        }
    }
}

// SMUS IFF tech (asm lbC001990): Paula's first block is only the one-shot head [0, ls) and the loop is installed one
// tick later. SS and AIFF start on the whole sample.
static bool snx_voice_head_only(const SonixSong* song, int ch) {
    u32 len = song->snx_active_pcm_len[ch];
    u32 ls = song->snx_active_loop_start[ch];
    u32 ll = song->snx_active_loop_len[ch];
    return ll > 1 && ls < len && ls + ll <= len && sonix_uses_score_engine(song)
           && song->instrument_is_iff[song->snx_inst_index[ch] & 63u];
}

// What the voices did since the last tick, applied to the player's view of Paula.
static void snx_poll_voice(SonixSong* song, int ch) {
    if (!song->snx_use_sample[ch])
        return;

    NuadeVoiceState state;
    nuade_voice_get_state(song->voices, (u32)ch, &state);

    if (state.playing) {
        const NuadeSample* current = state.sample;
        if (current->data != (const void*)song->snx_active_pcm[ch]) {
            // The queued buffer took over at the loop wrap (a synth's rewritten buffer, an IFF note's loop).
            song->snx_active_pcm[ch] = (const i8*)current->data;
            song->snx_active_pcm_len[ch] = current->length;
            song->snx_active_loop_start[ch] = current->loop_start;
            song->snx_active_loop_len[ch] = current->loop_length;
            song->synth_pending[ch] = false;
        }
        return;
    }

    // SStech (and IFF/AIFF) one-shot ran off its end: Paula plays the silent dummy loop (asm lbC0015D4 / lbC01E2C0 /
    // SMUS IFF lbC001996..lbC0019B0 with an empty loop) while the voice keeps ticking and AUDxVOL stays as written; the
    // note only ends when the envelope reaches 0 (SMUS IFF: ReleaseNOTE zeroes the volume, StopNOTE or the next note
    // ends it), and a new note inherits the port.
    const u8 inst = song->snx_inst_index[ch] & 63u;
    const bool keeps_port
        = song->instrument_is_ss[inst] || song->instrument_is_iff[inst] || song->metadata.format == SONIX_FORMAT_SNX;
    if (song->snx_active_pcm[ch] == nullptr || song->snx_active_pcm_len[ch] == 0) {
        song->snx_use_sample[ch] = false;
    } else if (!keeps_port) {
        // Non-looped sample reached end: fully stop channel.
        // Assembly CLR.L $1C(A1) clears the active flag.
        song->snx_note_on[ch] = false;
        song->snx_use_sample[ch] = false;
        song->snx_hw_vol[ch] = 0;
        song->snx_sample_hz[ch] = 0.0;
    }
}

// The player's Paula registers for this channel, written to its voice.
static void snx_sync_voice(SonixSong* song, int ch) {
    NuadeVoices* voices = song->voices;
    const bool on = song->snx_use_sample[ch] && song->snx_active_pcm[ch] != nullptr && song->snx_active_pcm_len[ch] > 0;

    if (!on) {
        nuade_voice_stop(voices, (u32)ch);
        song->voice_play[ch] = false;
        song->voice_queue[ch] = false;
        song->synth_pending[ch] = false;
    } else if (song->voice_play[ch]) {
        const i8* pcm = song->snx_active_pcm[ch];
        const u32 loop_start = song->snx_active_loop_start[ch];
        const u32 loop_len = song->snx_active_loop_len[ch];
        if (loop_start > 0 && snx_voice_head_only(song, ch)) {
            // The asm starts DMA on the one-shot head and installs the loop one tick later: the loop is the queued
            // sample.
            const NuadeSample head = snx_voice_sample(pcm, loop_start, 0, 0);
            const NuadeSample loop = snx_voice_sample(pcm + loop_start, loop_len, 0, loop_len);
            nuade_voice_play(voices, (u32)ch, &head, 0);
            nuade_voice_queue(voices, (u32)ch, &loop);
        } else {
            const NuadeSample sample = snx_voice_sample(pcm, song->snx_active_pcm_len[ch], loop_start, loop_len);
            nuade_voice_play(voices, (u32)ch, &sample, 0);
        }
        song->voice_play[ch] = false;
        song->voice_queue[ch] = false;
        song->synth_pending[ch] = false;
    } else if (song->voice_queue[ch]) {
        nuade_voice_queue(voices, (u32)ch, &song->voice_queued[ch]);
        song->voice_queue[ch] = false;
    }

    nuade_voice_set_pitch(voices, (u32)ch, song->snx_sample_hz[ch]);
    nuade_voice_set_volume(voices, (u32)ch, snx_paula_volume(snx_channel_paula_vol(song, ch)));
}

SonixSong* sonix_song_create(const u8* data, u32 size) {
    // Positions are u32: below 2 GiB, a position inside the file plus a few bytes cannot wrap.
    if (data == nullptr || size == 0 || size > SONIX_MAX_FILE_SIZE) {
        return nullptr;
    }

    SonixSong* song = (SonixSong*)calloc(1, sizeof(SonixSong));
    if (song == nullptr) {
        return nullptr;
    }

    song->data = data;
    song->size = size;
    song->metadata.num_channels = SONIX_NUM_CHANNELS;
    song->metadata.format = detect_format(data, size);
    song->metadata.has_form_header = has_mark(data, size, "FORM");
    song->metadata.valid = false;
    song->error[0] = '\0';
    song->running = false;

    song->metadata.valid = parse_song(song);
    song->runtime_track_engine
        = (song->metadata.valid
           && (song->metadata.format == SONIX_FORMAT_SNX || song->metadata.format == SONIX_FORMAT_SMUS
               || song->metadata.format == SONIX_FORMAT_TINY));
    if (!song->metadata.valid && song->error[0] == '\0') {
        snprintf(song->error, sizeof(song->error), "parse failed");
    }

    return song;
}

void sonix_song_destroy(SonixSong* song) {
    if (song == nullptr) {
        return;
    }
    for (int i = 0; i < 64; i++) {
        free(song->instrument_pcm[i]);
        song->instrument_pcm[i] = nullptr;
        song->instrument_pcm_len[i] = 0;
        free(song->synth_filter_bank[i]);
        song->synth_filter_bank[i] = nullptr;
        for (int z = 0; z < song->inst_zone_count[i]; z++) {
            free(song->inst_zones[i][z].pcm);
        }
        song->inst_zone_count[i] = 0;
    }
    sonix_set_dump_file(song, "");
    free(song);
}

bool sonix_set_dump_file(SonixSong* song, const char* path) {
    if (song->dump_file != nullptr && song->dump_file != stderr) {
        fclose(song->dump_file);
    }
    song->dump_file = nullptr;
    if (path[0] == '\0') {
        return true;
    }
    song->dump_file = strcmp(path, "-") == 0 ? stderr : fopen(path, "w");
    return song->dump_file != nullptr;
}

static void snx_dump_tick_state(SonixSong* song) {
    FILE* f = song->dump_file;
    if (!f)
        return;

    u32 tick = song->tick_count;

    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        u8 inst = song->snx_inst_index[ch] & 63u;
        fprintf(f, "T %u ch=%d inst=%u note_on=%d use_smp=%d", tick, ch, inst, song->snx_note_on[ch],
                song->snx_use_sample[ch]);
        fprintf(f, " hw_vol=%u ramp=%u ramp_tgt=%u", song->snx_hw_vol[ch], song->snx_ramp_current[ch],
                song->snx_ramp_target[ch]);
        fprintf(f, " wait=%u pos=%u", song->snx_wait[ch], song->snx_track_pos[ch]);

        // Pitch state
        NuadeVoiceState voice;
        nuade_voice_get_state(song->voices, (u32)ch, &voice);
        fprintf(f, " smp_hz=%.3f smp_pos=%.1f pcm_len=%u", song->snx_sample_hz[ch], voice.position,
                song->snx_active_pcm_len[ch]);

        // AIFF tech envelope
        fprintf(f, " aiff_vol=%u aiff_active=%d", song->aiff_tech_vol[ch], song->aiff_note_active[ch]);

        // SS portamento
        fprintf(f, " ss_phase=%u ss_port=0x%08X", song->ss_port_phase[ch], song->ss_port_value[ch]);

        // Synth state (only if instrument is synth)
        if (song->instrument_is_synth[inst]) {
            fprintf(f, " syn_period=%u syn_env=%d syn_blend=%d", song->synth_cur_period[ch], song->synth_env_value[ch],
                    song->synth_blend_accum[ch]);
        }

        // SNX-specific: cmd81 (volume), cmd83 (transpose)
        if (song->metadata.format == SONIX_FORMAT_SNX) {
            fprintf(f, " cmd81=%u cmd83=%d tempo=%u", song->snx_cmd81[ch], song->snx_cmd83[ch], song->tempo_cur);
        }

        // SMUS-specific
        if (song->metadata.format == SONIX_FORMAT_SMUS) {
            fprintf(f, " velocity=%u prescaler=%u tpb=%u", song->snx_note_velocity[ch], song->smus_prescaler,
                    song->smus_tpb);
        }

        fprintf(f, "\n");
    }
}

void sonix_song_start(SonixSong* song, NuadeVoices* voices) {
    if (song == nullptr) {
        return;
    }
    song->running = true;
    song->voices = voices;
    song->first_tick = true;
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++) {
        song->voice_play[ch] = false;
        song->voice_queue[ch] = false;
        song->synth_pending[ch] = false;
        song->shown_note[ch] = -1;
        song->shown_inst[ch] = -1;
    }
    song->tick_count = 0;
    if (song->runtime_track_engine) {
        snx_reset_runtime(song);
    }
    snx_set_pans(song);
}

u32 sonix_song_tick(SonixSong* song, bool* looped) {
    *looped = false;
    if (!song->running || !song->runtime_track_engine) {
        return SONIX_CIA_PAL_HZ;
    }
    if (song->first_tick) {
        song->first_tick = false;
        return song->first_tick_length;
    }

    const u32 loop_resets = song->debug_loop_resets;
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++)
        snx_poll_voice(song, ch);

    if (song->metadata.format == SONIX_FORMAT_SMUS) {
        smus_process_tick(song);
    } else if (song->metadata.format == SONIX_FORMAT_TINY) {
        tiny_process_tick(song);
    } else {
        snx_process_tick(song);
    }
    for (int ch = 0; ch < SONIX_NUM_CHANNELS; ch++)
        snx_sync_voice(song, ch);
    snx_dump_tick_state(song);

    *looped = song->debug_loop_resets != loop_resets;
    return song->tick_length;
}

bool sonix_song_is_finished(const SonixSong* song) {
    (void)song;
    return false;
}

const SonixSongMetadata* sonix_song_get_metadata(const SonixSong* song) {
    if (song == nullptr) {
        return nullptr;
    }

    return &song->metadata;
}

const char* sonix_format_name(SonixFormat format) {
    switch (format) {
        case SONIX_FORMAT_SNX:
            return "SNX";
        case SONIX_FORMAT_SMUS:
            return "SMUS";
        case SONIX_FORMAT_TINY:
            return "TINY";
        default:
            return "UNKNOWN";
    }
}

const char* sonix_song_get_error(const SonixSong* song) {
    if (song == nullptr || song->error[0] == '\0') {
        return "";
    }
    return song->error;
}

const char* sonix_song_get_instrument_name(const SonixSong* song, u8 instrument_index) {
    if (song == nullptr || instrument_index >= 64) {
        return "";
    }
    return song->instrument_names[instrument_index];
}

u32 sonix_song_get_debug_note_events(const SonixSong* song) {
    if (song == nullptr) {
        return 0;
    }
    return song->debug_total_note_events;
}

bool sonix_song_describe_event(const SonixSong* song, int ch, u32 pos, NuadeEvent* out) {
    if (song == nullptr || ch < 0 || ch >= SONIX_NUM_CHANNELS || pos < song->snx_track_start[ch]
        || pos + 2 > song->snx_track_end[ch]) {
        return false;
    }
    const u16 word = nuade_read_be16(song->data + pos);
    const u8 hi = (u8)(word >> 8);
    const u8 lo = (u8)(word & 0xFFu);
    const bool snx = song->metadata.format == SONIX_FORMAT_SNX;
    *out = (NuadeEvent) { .pos = pos, .note = -1, .instrument = -1 };
    char* text = out->text;
    const size_t cap = sizeof(out->text);

    if (word == 0) {
        snprintf(text, cap, "nop");
    } else if (word == 0xFFFFu) {
        snprintf(text, cap, "end");
    } else if (snx && (word & 0xC000u) == 0xC000u) {
        snprintf(text, cap, "wait %u", word & 0x3FFFu);
    } else if (hi == 0x80u && snx) {
        out->instrument = lo;
        snprintf(text, cap, "inst %u", lo);
    } else if (hi == 0x81u && !snx) {
        out->instrument = lo;
        snprintf(text, cap, "inst %u", lo);
    } else if (hi == 0x81u) {
        snprintf(text, cap, "vol %u", lo);
    } else if (hi == 0x82u) {
        snprintf(text, cap, "tempo %u", lo);
    } else if (hi == 0x83u && snx) {
        snprintf(text, cap, "bend %d", (int)(i8)lo);
    } else if (hi >= 0x80u) {
        if (hi == 0x80u && !snx) {
            const u8 d = smus_duration_table[lo & 0x0Fu];
            snprintf(text, cap, "rest %u", song->metadata.format == SONIX_FORMAT_TINY ? lo : d);
        } else {
            snprintf(text, cap, "cmd %02x%02x", hi, lo);
        }
    } else if (snx) {
        out->note = hi;
        if (lo == 0) {
            snprintf(text, cap, "n%u off", hi);
        } else {
            snprintf(text, cap, "n%u v%u", hi, lo);
        }
    } else {
        out->note = hi;
        const u32 d = song->metadata.format == SONIX_FORMAT_TINY ? lo : smus_duration_table[lo & 0x0Fu];
        snprintf(text, cap, "n%u d%u", hi, d);
    }
    return true;
}

u32 sonix_song_get_tick_count(const SonixSong* song) {
    if (song == nullptr)
        return 0;

    return song->tick_count;
}

u32 sonix_song_get_debug_wait_commands(const SonixSong* song) {
    if (song == nullptr)
        return 0;

    return song->debug_wait_commands;
}

u32 sonix_song_get_debug_loop_resets(const SonixSong* song) {
    if (song == nullptr)
        return 0;

    return song->debug_loop_resets;
}

bool sonix_song_has_runtime_track_engine(const SonixSong* song) {
    if (song == nullptr)
        return false;
    return song->runtime_track_engine;
}

bool sonix_song_set_instrument_pcm8(SonixSong* song, u8 instrument_index, const i8* pcm_data, u32 num_samples,
                                    u32 loop_start, u32 loop_len, u8 base_note) {
    return sonix_song_set_instrument_pcm8_ex(song, instrument_index, pcm_data, num_samples, loop_start, loop_len,
                                             base_note, 0, 0);
}

bool sonix_song_set_instrument_pcm8_ex(SonixSong* song, u8 instrument_index, const i8* pcm_data, u32 num_samples,
                                       u32 loop_start, u32 loop_len, u8 base_note, u32 base_period,
                                       u32 source_rate_hz) {
    if (song == nullptr || instrument_index >= 64 || pcm_data == nullptr || num_samples == 0) {
        return false;
    }

    i8* copy = (i8*)malloc(num_samples);
    if (copy == nullptr) {
        return false;
    }

    memcpy(copy, pcm_data, num_samples);

    free(song->instrument_pcm[instrument_index]);
    song->instrument_pcm[instrument_index] = copy;
    song->instrument_pcm_len[instrument_index] = num_samples;
    song->instrument_base_note[instrument_index] = base_note ? base_note : 60u;
    song->instrument_base_period[instrument_index] = base_period;
    song->instrument_source_rate[instrument_index] = source_rate_hz;

    if (loop_len > 1 && loop_start < num_samples && loop_len <= num_samples - loop_start) {
        song->instrument_loop_start[instrument_index] = loop_start;
        song->instrument_loop_len[instrument_index] = loop_len;
    } else {
        song->instrument_loop_start[instrument_index] = 0;
        song->instrument_loop_len[instrument_index] = 0;
    }

    return true;
}

bool sonix_song_add_instrument_zone(SonixSong* song, u8 instrument_index, const i8* pcm_data, u32 num_samples,
                                    u32 loop_start, u32 loop_len, u8 base_note, u8 low_key, u8 high_key,
                                    u32 source_rate_hz) {
    if (song == nullptr || instrument_index >= 64 || pcm_data == nullptr || num_samples == 0) {
        return false;
    }

    u8 zi = song->inst_zone_count[instrument_index];
    if (zi >= SNX_MAX_ZONES) {
        return false;
    }

    SonixInstZone* zone = &song->inst_zones[instrument_index][zi];
    zone->pcm = (i8*)malloc(num_samples);
    if (zone->pcm == nullptr) {
        return false;
    }
    memcpy(zone->pcm, pcm_data, num_samples);
    zone->pcm_len = num_samples;
    zone->base_note = base_note ? base_note : 60u;
    zone->low_key = low_key;
    zone->high_key = high_key;
    zone->source_rate = source_rate_hz;

    if (loop_len > 1 && loop_start < num_samples && loop_len <= num_samples - loop_start) {
        zone->loop_start = loop_start;
        zone->loop_len = loop_len;
    } else {
        zone->loop_start = 0;
        zone->loop_len = 0;
    }

    song->inst_zone_count[instrument_index] = zi + 1;
    return true;
}

void sonix_song_set_instrument_attack_decay(SonixSong* song, u8 instrument_index, u16 attack_time, u16 decay_time) {
    if (song == nullptr || instrument_index >= 64)
        return;
    song->inst_attack_time[instrument_index] = attack_time;
    song->inst_decay_time[instrument_index] = decay_time;
}

void sonix_song_set_zone_attack_decay(SonixSong* song, u8 instrument_index, u8 zone_index, u16 attack_time,
                                      u16 decay_time) {
    if (song == nullptr || instrument_index >= 64 || zone_index >= SNX_MAX_ZONES)
        return;
    if (zone_index >= song->inst_zone_count[instrument_index])
        return;
    song->inst_zones[instrument_index][zone_index].attack_time = attack_time;
    song->inst_zones[instrument_index][zone_index].decay_time = decay_time;
}

void sonix_song_set_instrument_ss(SonixSong* song, u8 instrument_index, bool is_ss) {
    if (song == nullptr || instrument_index >= 64)
        return;
    song->instrument_is_ss[instrument_index] = is_ss;
}

void sonix_song_set_instrument_iff(SonixSong* song, u8 instrument_index, bool is_iff, u32 vhdr_volume) {
    if (song == nullptr || instrument_index >= 64)
        return;
    song->instrument_is_iff[instrument_index] = is_iff;
    song->iff_vhdr_volume[instrument_index] = vhdr_volume;
}

void sonix_song_set_instrument_synth(SonixSong* song, u8 instrument_index, bool is_synth) {
    if (song == nullptr || instrument_index >= 64)
        return;

    song->instrument_is_synth[instrument_index] = is_synth;
}

void sonix_song_set_synth_vol_params(SonixSong* song, u8 instrument_index, u16 base_vol, u16 port_flag) {
    if (song == nullptr || instrument_index >= 64)
        return;

    song->synth_base_vol[instrument_index] = base_vol;
    song->synth_port_flag[instrument_index] = port_flag;
}

void sonix_song_set_ss_envelope(SonixSong* song, u8 inst, u16 inst_vol, const u16 targets[4], const u16 speeds[4]) {
    if (song == nullptr || inst >= 64)
        return;

    song->ss_inst_vol[inst] = inst_vol;

    for (int j = 0; j < 4; j++) {
        song->ss_port_target[inst][j] = targets[j];
        song->ss_port_speed[inst][j] = speeds[j];
    }
}

void sonix_song_set_synth_wave(SonixSong* song, u8 instrument_index, const i8* wave128) {
    if (song == nullptr || instrument_index >= 64 || wave128 == nullptr)
        return;

    memcpy(song->synth_wave[instrument_index], wave128, 128);
    song->synth_wave_set[instrument_index] = true;

    // Pre-compute the 64-band filter bank from the waveform
    snx_compute_filter_bank(song, instrument_index);
}

void sonix_song_set_synth_blend_params(SonixSong* song, u8 instrument_index, u16 c2, u16 c4) {
    if (song == nullptr || instrument_index >= 64)
        return;

    song->synth_c2[instrument_index] = c2;
    song->synth_c4[instrument_index] = c4;
}

void sonix_song_set_synth_filter_params(SonixSong* song, u8 instrument_index, u16 filter_base, u16 filter_range,
                                        u16 filter_env_sens) {
    if (song == nullptr || instrument_index >= 64)
        return;

    song->synth_filter_base[instrument_index] = filter_base;
    song->synth_filter_range[instrument_index] = filter_range;
    song->synth_filter_env_sens[instrument_index] = filter_env_sens;
}

void sonix_song_set_synth_env_params(SonixSong* song, u8 instrument_index, u16 scan_rate, int16_t loop_mode,
                                     u16 delay_init, u16 vol_scale, u16 pitch_scale) {
    if (song == nullptr || instrument_index >= 64)
        return;

    song->synth_env_scan_rate[instrument_index] = scan_rate;
    song->synth_env_loop_mode[instrument_index] = (i16)loop_mode;
    song->synth_env_delay_init[instrument_index] = delay_init;
    song->synth_env_vol_scale[instrument_index] = vol_scale;
    song->synth_env_pitch_scale[instrument_index] = pitch_scale;
}

void sonix_song_set_synth_slide_rate(SonixSong* song, u8 instrument_index, u16 slide_rate) {
    if (song == nullptr || instrument_index >= 64)
        return;
    song->synth_slide_rate[instrument_index] = slide_rate;
}

void sonix_song_set_synth_env_table(SonixSong* song, u8 instrument_index, const int8_t* table256) {
    if (song == nullptr || instrument_index >= 64 || table256 == nullptr)
        return;

    memcpy(song->synth_env_table[instrument_index], table256, 256);
}
