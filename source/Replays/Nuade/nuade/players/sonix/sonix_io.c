// SPDX-License-Identifier: MIT

#include "sonix_io.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_utils.h"
#include "sonix.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helpers

static double read_ieee_extended80_be(const uint8_t* p) {
    uint16_t exp = nuade_read_be16(p);
    uint64_t mant = 0;
    for (int i = 0; i < 8; i++) {
        mant = (mant << 8) | (uint64_t)p[2 + i];
    }
    if (exp == 0 || mant == 0) {
        return 0.0;
    }
    int sign = (exp & 0x8000u) ? -1 : 1;
    exp &= 0x7FFFu;
    double frac = (double)mant / (double)((uint64_t)1 << 63);
    return sign * ldexp(frac, (int)exp - 16383);
}

// File reads through NuadeIo

// Read a whole file into a malloc'd buffer (the loaders below free or realloc it). Caller frees *out_data.
static bool read_file(const NuadeIo* io, const char* path, uint8_t** out_data, uint32_t* out_size) {
    NuadeIoReadResult r = NuadeIo_read_url_to_memory(io, path);
    if (!r.data)
        return false;

    bool ok = false;
    if (r.size <= UINT32_MAX) {
        uint8_t* buf = (uint8_t*)malloc(r.size ? (size_t)r.size : 1);
        if (buf) {
            memcpy(buf, r.data, (size_t)r.size);
            *out_data = buf;
            *out_size = (uint32_t)r.size;
            ok = true;
        }
    }
    NuadeIo_free_url_to_memory(io, r.data);
    return ok;
}

// AIFF marker struct (C equivalent)

typedef struct {
    uint16_t id;
    uint32_t pos;
} SonixAiffMarker;

// 8SVX multi-octave zone info

typedef struct {
    const uint8_t* data; // p ointer into input buffer (NOT owned)
    uint32_t length;
    uint32_t loop_start;
    uint32_t loop_len;
    uint8_t low_key;
    uint8_t high_key;
} SonixSvxOctaveZone;

// AIFF zone info

typedef struct {
    int8_t* pcm; // malloc'd, caller frees
    uint32_t pcm_len;
    uint32_t loop_start;
    uint32_t loop_len;
    uint8_t base_note;
    uint8_t low_key;
    uint8_t high_key;
    uint32_t source_rate_hz;
    uint16_t attack_time;
    uint16_t decay_time;
} SonixAiffZone;

// Format decoders
// Resolve loop markers from AIFF MARK+INST chunks
static void resolve_aiff_loop(const SonixAiffMarker* markers, int num_markers, uint16_t sustain_mode,
                              uint16_t sustain_begin, uint16_t sustain_end, uint32_t pcm_size, uint32_t* loop_start,
                              uint32_t* loop_len) {
    *loop_start = 0;
    *loop_len = 0;

    if (sustain_mode != 0 && sustain_begin != 0 && sustain_end != 0) {
        uint32_t lp0 = 0, lp1 = 0;
        bool got0 = false, got1 = false;
        for (int i = 0; i < num_markers; i++) {
            if (markers[i].id == sustain_begin) {
                lp0 = markers[i].pos;
                got0 = true;
            } else if (markers[i].id == sustain_end) {
                lp1 = markers[i].pos;
                got1 = true;
            }
        }
        if (got0 && got1 && lp1 > lp0 && lp1 <= pcm_size) {
            *loop_start = lp0;
            *loop_len = lp1 - lp0;
        }
    }
    if (*loop_len == 0 && num_markers > 0) {
        uint32_t lp0 = 0, lp1 = 0;
        bool got0 = false, got1 = false;
        for (int i = 0; i < num_markers; i++) {
            if (markers[i].id == 1) {
                lp0 = markers[i].pos;
                got0 = true;
            } else if (markers[i].id == 2) {
                lp1 = markers[i].pos;
                got1 = true;
            }
        }
        if (got0 && got1 && lp1 > lp0 && lp1 <= pcm_size) {
            *loop_start = lp0;
            *loop_len = lp1 - lp0;
        }
    }
}

// Parse MARK chunk, return number of markers parsed
static int parse_aiff_markers(const uint8_t* data, size_t data_pos, uint32_t len, SonixAiffMarker* markers,
                              int max_markers) {
    uint16_t n = nuade_read_be16(data + data_pos);
    size_t mp = data_pos + 2;
    int count = 0;
    for (uint16_t mi = 0; mi < n && count < max_markers; mi++) {
        if (mp + 7 > data_pos + len)
            break;
        markers[count].id = nuade_read_be16(data + mp);
        markers[count].pos = nuade_read_be32(data + mp + 2);
        uint8_t slen = data[mp + 6];
        mp += 7 + slen;
        if ((slen & 1u) == 0u)
            mp += 1;
        count++;
    }
    return count;
}

// Reads "<attack> <decay>" from an AIFF NAME chunk, as sscanf("%d %d") does for numbers that fit an int (sscanf has
// undefined behaviour for numbers that do not).

static bool parse_attack_decay(const char* text, int* attack, int* decay) {
    int values[2];
    const char* p = text;
    for (int i = 0; i < 2; i++) {
        char* end = NULL;
        errno = 0;
        long v = strtol(p, &end, 10);
        if (end == p || errno == ERANGE || v < INT_MIN || v > INT_MAX)
            return false;
        values[i] = (int)v;
        p = end;
    }
    *attack = values[0];
    *decay = values[1];
    return true;
}

static int decode_8svx_zones(const uint8_t* data, uint32_t size, SonixSvxOctaveZone* zones, int max_zones,
                             uint16_t* source_rate_hz, uint32_t* vhdr_volume) {
    if (size < 12)
        return 0;
    if (memcmp(data, "FORM", 4) != 0)
        return 0;
    if (memcmp(data + 8, "8SVX", 4) != 0)
        return 0;

    uint32_t form_size = nuade_read_be32(data + 4);
    size_t end = form_size < size - 8 ? 8 + (size_t)form_size : size;

    uint32_t oneshot = 0, repeat = 0, samples_per_hi_cycle = 0;
    uint32_t vhdr_vol = 0x10000;
    uint16_t sample_rate = 0;
    uint8_t ct_octave = 1, compression = 0;
    bool have_vhdr = false;
    size_t body_pos = 0, body_size = 0;

    size_t pos = 12;
    while (pos + 8 <= end) {
        char id[5] = { 0 };
        memcpy(id, data + pos, 4);
        uint32_t sz = nuade_read_be32(data + pos + 4);
        size_t chunk_data = pos + 8;
        size_t chunk_end = sz < end - chunk_data ? chunk_data + sz : end;

        if (memcmp(id, "VHDR", 4) == 0 && sz >= 20 && chunk_end - chunk_data >= 20) {
            oneshot = nuade_read_be32(data + chunk_data);
            repeat = nuade_read_be32(data + chunk_data + 4);
            samples_per_hi_cycle = nuade_read_be32(data + chunk_data + 8);
            sample_rate = nuade_read_be16(data + chunk_data + 12);
            ct_octave = data[chunk_data + 14];
            compression = data[chunk_data + 15];
            vhdr_vol = nuade_read_be32(data + chunk_data + 16);
            have_vhdr = true;
        } else if (memcmp(id, "BODY", 4) == 0) {
            body_pos = chunk_data;
            body_size = chunk_end - chunk_data;
        }

        if (chunk_end == end)
            break;
        pos = chunk_end + (sz & 1);
    }

    if (!have_vhdr || body_size == 0 || compression != 0)
        return 0;
    if (ct_octave == 0)
        ct_octave = 1;
    *source_rate_hz = sample_rate;
    *vhdr_volume = vhdr_vol;

    uint16_t octave_shift = 0;
    {
        uint32_t d2 = oneshot, d3 = repeat, d4 = samples_per_hi_cycle;
        for (;;) {
            d2 >>= 1;
            d3 >>= 1;
            d4 >>= 1;
            octave_shift++;
            uint32_t d0 = d2 | d3;
            if (d0 & 1)
                break;
            if (d4 == 1)
                break;
            if ((uint8_t)(octave_shift + ct_octave) >= 8)
                break;
        }
    }

    (void)samples_per_hi_cycle;
    int num_zones = 0;
    for (int oct = 0; oct < (int)ct_octave && num_zones < max_zones; oct++) {
        int note_base = (10 - (int)octave_shift - oct) * 12;
        int low = note_base;
        int high = note_base + 11;

        if (high < 0 || low > 127)
            continue;
        if (low < 0)
            low = 0;
        if (high > 127)
            high = 127;

        // 64-bit: lengths from the file must not wrap past the body check.
        uint64_t oct_len = (uint64_t)oneshot << oct;
        uint64_t oct_offset = (uint64_t)oneshot * ((1u << oct) - 1u);

        if (oct_offset + oct_len > body_size)
            continue;

        zones[num_zones].data = data + body_pos + oct_offset;
        zones[num_zones].length = (uint32_t)oct_len;
        zones[num_zones].low_key = (uint8_t)low;
        zones[num_zones].high_key = (uint8_t)high;

        if (repeat > 0) {
            uint32_t ls = oneshot << oct;
            uint32_t ll = repeat << oct;
            zones[num_zones].loop_start = ls;
            zones[num_zones].loop_len = ll;
            oct_len = ((uint64_t)oneshot + repeat) << oct;
            oct_offset = ((uint64_t)oneshot + repeat) * ((1u << oct) - 1u);
            if (oct_offset + oct_len <= body_size) {
                zones[num_zones].length = (uint32_t)oct_len;
                zones[num_zones].data = data + body_pos + oct_offset;
            }
        } else {
            zones[num_zones].loop_start = 0;
            zones[num_zones].loop_len = 0;
        }

        num_zones++;
    }

    return num_zones;
}

static bool decode_aiff_pcm8(const uint8_t* data, uint32_t size, int8_t** out_pcm, uint32_t* out_pcm_len,
                             uint32_t* out_loop_start, uint32_t* out_loop_len, uint8_t* out_base_note,
                             uint32_t* out_source_rate_hz, uint8_t* out_low_key, uint8_t* out_high_key,
                             bool* out_have_inst) {
    size_t form_pos = (size_t)-1;
    size_t form_end = 0;
    size_t pos;
    uint32_t num_frames = 0;
    uint16_t sample_bits = 0;
    uint8_t inst_base_note = 60;
    uint8_t inst_low_key = 0;
    uint8_t inst_high_key = 127;
    bool found_inst = false;
    uint16_t sustain_mode = 0, sustain_begin = 0, sustain_end = 0;
    SonixAiffMarker markers[64];
    int num_markers = 0;
    const uint8_t* ssnd = NULL;
    uint32_t ssnd_size = 0;
    uint32_t ssnd_offset = 0;

    *out_loop_start = 0;
    *out_loop_len = 0;
    *out_base_note = 60;
    *out_source_rate_hz = 0;
    *out_pcm = NULL;
    *out_pcm_len = 0;

    for (size_t i = 0; i + 12 <= size; i++) {
        if (memcmp(data + i, "FORM", 4) != 0)
            continue;
        uint32_t sz = nuade_read_be32(data + i + 4);
        if (sz > size - i - 8)
            continue;
        if (memcmp(data + i + 8, "AIFF", 4) == 0 || memcmp(data + i + 8, "AIFC", 4) == 0) {
            form_pos = i;
            form_end = i + 8 + sz;
            break;
        }
    }

    if (form_pos == (size_t)-1)
        return false;

    pos = form_pos + 12;
    while (pos + 8 <= form_end && pos + 8 <= size) {
        const uint8_t* ch = data + pos;
        uint32_t len = nuade_read_be32(ch + 4);
        size_t data_pos = pos + 8;
        if (len > size - data_pos)
            break;

        if (memcmp(ch, "COMM", 4) == 0 && len >= 18) {
            num_frames = nuade_read_be32(data + data_pos + 2);
            sample_bits = nuade_read_be16(data + data_pos + 6);
            double sr = read_ieee_extended80_be(data + data_pos + 8);
            if (sr > 1000.0 && sr < 200000.0) {
                *out_source_rate_hz = (uint32_t)(sr + 0.5);
            }
        } else if (memcmp(ch, "INST", 4) == 0 && len >= 14) {
            found_inst = true;
            inst_base_note = data[data_pos + 0];
            inst_low_key = data[data_pos + 2];
            inst_high_key = data[data_pos + 3];
            sustain_mode = nuade_read_be16(data + data_pos + 8);
            sustain_begin = nuade_read_be16(data + data_pos + 10);
            sustain_end = nuade_read_be16(data + data_pos + 12);
        } else if (memcmp(ch, "MARK", 4) == 0 && len >= 2) {
            num_markers = parse_aiff_markers(data, data_pos, len, markers, 64);
        } else if (memcmp(ch, "SSND", 4) == 0 && len >= 8) {
            ssnd = data + data_pos;
            ssnd_size = len;
            ssnd_offset = nuade_read_be32(ssnd + 0);
        }

        pos = data_pos + ((len + 1u) & ~1u);
    }

    if (ssnd == NULL || ssnd_size < 8 || sample_bits != 8)
        return false;

    {
        size_t sample_data_pos = (size_t)(ssnd - data) + 8 + ssnd_offset;
        if (sample_data_pos >= size)
            return false;
        size_t avail = size - sample_data_pos;
        size_t want = num_frames ? (size_t)num_frames : (ssnd_size - 8 - ssnd_offset);
        size_t take = (want < avail) ? want : avail;
        if (take == 0)
            return false;

        int8_t* pcm = (int8_t*)malloc(take);
        if (!pcm)
            return false;
        for (size_t i = 0; i < take; i++) {
            pcm[i] = (int8_t)data[sample_data_pos + i];
        }
        *out_pcm = pcm;
        *out_pcm_len = (uint32_t)take;
    }

    *out_base_note = inst_base_note ? inst_base_note : 60;
    *out_have_inst = found_inst;
    *out_low_key = found_inst ? inst_low_key : 0;
    *out_high_key = found_inst ? inst_high_key : 127;

    resolve_aiff_loop(markers, num_markers, sustain_mode, sustain_begin, sustain_end, *out_pcm_len, out_loop_start,
                      out_loop_len);

    return true;
}

static bool decode_aiff_zone(const uint8_t* data, size_t chunk_size, SonixAiffZone* zone) {
    if (chunk_size < 12)
        return false;
    if (memcmp(data + 8, "AIFF", 4) != 0 && memcmp(data + 8, "AIFC", 4) != 0)
        return false;

    size_t pos = 12;
    uint32_t num_frames = 0;
    uint16_t sample_bits = 0;
    uint8_t inst_base_note = 60;
    uint8_t inst_low_key = 0;
    uint8_t inst_high_key = 127;
    uint16_t sustain_mode = 0, sustain_begin = 0, sustain_end = 0;
    SonixAiffMarker markers[64];
    int num_markers = 0;
    const uint8_t* ssnd = NULL;
    uint32_t ssnd_size = 0;
    uint32_t ssnd_offset = 0;
    bool have_inst = false;

    zone->pcm = NULL;
    zone->pcm_len = 0;
    zone->loop_start = 0;
    zone->loop_len = 0;
    zone->source_rate_hz = 0;
    zone->attack_time = 0;
    zone->decay_time = 0;

    while (pos + 8 <= chunk_size) {
        const uint8_t* ch = data + pos;
        uint32_t len = nuade_read_be32(ch + 4);
        size_t data_pos = pos + 8;
        if (len > chunk_size - data_pos)
            break;

        if (memcmp(ch, "COMM", 4) == 0 && len >= 18) {
            num_frames = nuade_read_be32(data + data_pos + 2);
            sample_bits = nuade_read_be16(data + data_pos + 6);
            double sr = read_ieee_extended80_be(data + data_pos + 8);
            if (sr > 1000.0 && sr < 200000.0) {
                zone->source_rate_hz = (uint32_t)(sr + 0.5);
            }
        } else if (memcmp(ch, "INST", 4) == 0 && len >= 14) {
            have_inst = true;
            inst_base_note = data[data_pos + 0];
            inst_low_key = data[data_pos + 2];
            inst_high_key = data[data_pos + 3];
            sustain_mode = nuade_read_be16(data + data_pos + 8);
            sustain_begin = nuade_read_be16(data + data_pos + 10);
            sustain_end = nuade_read_be16(data + data_pos + 12);
        } else if (memcmp(ch, "MARK", 4) == 0 && len >= 2) {
            num_markers = parse_aiff_markers(data, data_pos, len, markers, 64);
        } else if (memcmp(ch, "SSND", 4) == 0 && len >= 8) {
            ssnd = data + data_pos;
            ssnd_size = len;
            ssnd_offset = nuade_read_be32(ssnd + 0);
        } else if (memcmp(ch, "NAME", 4) == 0 && len > 0) {
            char buf[256];
            size_t copy_len = len < 255 ? len : 255;
            memcpy(buf, data + data_pos, copy_len);
            buf[copy_len] = '\0';
            int a = -1, b = -1;
            if (parse_attack_decay(buf, &a, &b)) {
                if (a < 0)
                    a = 0;
                if (b < 0)
                    b = 0;
                zone->attack_time = (uint16_t)a;
                zone->decay_time = (uint16_t)b;
            }
        }

        pos = data_pos + ((len + 1u) & ~1u);
    }

    if (ssnd == NULL || ssnd_size < 8 || sample_bits != 8)
        return false;

    {
        size_t sample_data_pos = (size_t)(ssnd - data) + 8 + ssnd_offset;
        if (sample_data_pos >= chunk_size)
            return false;
        size_t avail = chunk_size - sample_data_pos;
        size_t want = num_frames ? (size_t)num_frames : (ssnd_size - 8 - ssnd_offset);
        size_t take = (want < avail) ? want : avail;
        if (take == 0)
            return false;

        zone->pcm = (int8_t*)malloc(take);
        if (!zone->pcm)
            return false;
        for (size_t i = 0; i < take; i++) {
            zone->pcm[i] = (int8_t)data[sample_data_pos + i];
        }
        zone->pcm_len = (uint32_t)take;
    }

    zone->base_note = inst_base_note ? inst_base_note : 60;
    zone->low_key = have_inst ? inst_low_key : 0;
    zone->high_key = have_inst ? inst_high_key : 127;

    resolve_aiff_loop(markers, num_markers, sustain_mode, sustain_begin, sustain_end, zone->pcm_len, &zone->loop_start,
                      &zone->loop_len);

    return true;
}

#define SONIX_MAX_AIFF_ZONES 64

static int decode_aiff_zones(const uint8_t* data, uint32_t size, SonixAiffZone* zones, int max_zones) {
    if (size < 12 || memcmp(data, "LIST", 4) != 0)
        return 0;
    if (memcmp(data + 8, "AIFF", 4) != 0)
        return 0;

    int count = 0;
    size_t pos = 12;
    while (pos + 12 <= size && count < max_zones) {
        if (memcmp(data + pos, "FORM", 4) != 0) {
            pos++;
            continue;
        }
        uint32_t sz = nuade_read_be32(data + pos + 4);
        if (sz > size - pos - 8)
            break;
        size_t chunk_end = pos + 8 + sz;
        if (memcmp(data + pos + 8, "AIFF", 4) == 0 || memcmp(data + pos + 8, "AIFC", 4) == 0) {
            if (decode_aiff_zone(data + pos, 8 + (size_t)sz, &zones[count])) {
                count++;
            }
        }
        pos = chunk_end;
        if (pos & 1)
            pos++;
    }

    return count;
}

static bool parse_aiff_name_attack_decay(const uint8_t* data, uint32_t size, uint16_t* attack_time,
                                         uint16_t* decay_time) {
    size_t form_pos = (size_t)-1;
    size_t form_end = 0;
    *attack_time = 0;
    *decay_time = 0;

    for (size_t i = 0; i + 12 <= size; i++) {
        if (memcmp(data + i, "FORM", 4) != 0)
            continue;
        uint32_t sz = nuade_read_be32(data + i + 4);
        if (sz > size - i - 8)
            continue;
        if (memcmp(data + i + 8, "AIFF", 4) == 0 || memcmp(data + i + 8, "AIFC", 4) == 0) {
            form_pos = i;
            form_end = i + 8 + sz;
            break;
        }
    }
    if (form_pos == (size_t)-1)
        return false;

    size_t pos = form_pos + 12;
    while (pos + 8 <= form_end && pos + 8 <= size) {
        const uint8_t* ch = data + pos;
        uint32_t len = nuade_read_be32(ch + 4);
        size_t data_pos = pos + 8;
        if (len > size - data_pos)
            break;
        if (memcmp(ch, "NAME", 4) == 0 && len > 0) {
            char buf[256];
            size_t copy_len = len < 255 ? len : 255;
            memcpy(buf, data + data_pos, copy_len);
            buf[copy_len] = '\0';
            int a = -1, b = -1;
            if (parse_attack_decay(buf, &a, &b)) {
                if (a < 0)
                    a = 0;
                if (a > 65535)
                    a = 65535;
                if (b < 0)
                    b = 0;
                if (b > 65535)
                    b = 65535;
                *attack_time = (uint16_t)a;
                *decay_time = (uint16_t)b;
                return true;
            }
            return false;
        }
        pos = data_pos + ((len + 1u) & ~1u);
    }
    return false;
}

static void read_cstr(const uint8_t* data, uint32_t size, size_t off, size_t max_len, char* out, size_t out_size) {
    out[0] = '\0';
    if (off >= size)
        return;
    size_t j = 0;
    for (size_t i = 0; i < max_len && (off + i) < size && j < out_size - 1; i++) {
        uint8_t c = data[off + i];
        if (c == 0)
            break;
        if (c < 32 || c > 126)
            break;
        out[j++] = (char)c;
    }
    out[j] = '\0';
}

static void parse_sampled_sound_ref(const uint8_t* instr, uint32_t instr_size, const char* fallback_name,
                                    char* out_name, size_t out_name_size) {
    out_name[0] = '\0';
    char sig[33];
    read_cstr(instr, instr_size, 0, 32, sig, sizeof(sig));
    if (strcmp(sig, "SampledSound") != 0)
        return;

    char sample_name[128];
    read_cstr(instr, instr_size, 0x44, 32, sample_name, sizeof(sample_name));
    if (sample_name[0] == '\0') {
        snprintf(out_name, out_name_size, "%s", fallback_name);
    } else {
        snprintf(out_name, out_name_size, "%s", sample_name);
    }
}

static uint8_t closest_note_from_period(uint32_t period) {
    static const uint16_t k_note_ratio[12]
        = { 0x8000, 0x78D1, 0x7209, 0x6BA2, 0x6598, 0x5FE4, 0x5A82, 0x556E, 0x50A3, 0x4C1C, 0x47D6, 0x43CE };
    uint32_t best_note = 60;
    uint32_t best_diff = 0xFFFFFFFFu;
    for (uint32_t note = 0x24; note <= 0x6B; note++) {
        uint32_t rel = note - 0x24;
        uint32_t octave = rel / 12u;
        uint32_t semi = rel % 12u;
        uint32_t p = (0xD5C8u * (uint32_t)k_note_ratio[semi]) >> (octave + 17u);
        uint32_t d = (p > period) ? (p - period) : (period - p);
        if (d < best_diff) {
            best_diff = d;
            best_note = note;
        }
    }
    return (uint8_t)best_note;
}

static uint8_t parse_sampled_sound_base_note(const uint8_t* instr, uint32_t instr_size) {
    if (instr_size < 0x24 || memcmp(instr, "SampledSound", 11) != 0)
        return 60;
    uint32_t q24_8 = nuade_read_be32(instr + 0x20);
    uint32_t period = q24_8 >> 8;
    if (period >= 100 && period <= 1200)
        return closest_note_from_period(period);
    return 60;
}

static uint32_t parse_sampled_sound_base_period(const uint8_t* instr, uint32_t instr_size) {
    if (instr_size < 0x24 || memcmp(instr, "SampledSound", 11) != 0)
        return 0;
    uint32_t q24_8 = nuade_read_be32(instr + 0x20);
    uint32_t period = q24_8 >> 8;
    if (period >= 100 && period <= 1200)
        return period;
    return 0;
}

static bool decode_synthesis_wave(const uint8_t* instr, uint32_t instr_size, int8_t** out_pcm, uint32_t* out_pcm_len,
                                  int8_t** out_wave128, uint32_t* out_wave_len, uint16_t* out_c2, uint16_t* out_c4) {
    *out_c2 = 0;
    *out_c4 = 0;
    *out_pcm = NULL;
    *out_pcm_len = 0;
    *out_wave128 = NULL;
    *out_wave_len = 0;

    if (instr_size < 0xC4 + 64)
        return false;

    bool has_synth_header = (memcmp(instr, "Synthesis", 9) == 0);
    bool has_zero_header = true;
    for (int i = 0; i < 32 && has_zero_header; i++) {
        if (instr[i] != 0)
            has_zero_header = false;
    }
    if (!has_synth_header && !has_zero_header)
        return false;

    // FM waveform as PCM fallback (64 bytes)
    int8_t* pcm = (int8_t*)malloc(64);
    if (!pcm)
        return false;
    for (size_t i = 0; i < 64; i++) {
        pcm[i] = (int8_t)instr[0xC4 + i];
    }
    *out_pcm = pcm;
    *out_pcm_len = 64;

    // 128-byte base waveform
    if (instr_size >= 0x44 + 128) {
        int8_t* wave = (int8_t*)malloc(128);
        if (wave) {
            for (size_t i = 0; i < 128; i++) {
                wave[i] = (int8_t)instr[0x44 + i];
            }
            *out_wave128 = wave;
            *out_wave_len = 128;
        }
    }

    if (instr_size >= 0x1E6) {
        *out_c2 = nuade_read_be16(instr + 0x1E2);
        *out_c4 = nuade_read_be16(instr + 0x1E4);
    }

    return true;
}

// TINY type-1 instruments (asm TinyLoad / INITINSTRUMENT: first long 1 = SYNTHTECH) are a regular 502-byte
// Synthesis file without its 0x44-byte name header: the 128-byte wave starts at offset 8. Rebuild the regular
// layout (zero header) so the Synthesis path below reads every field at its usual offset.

#define TINY_SYNTH_FILE_SIZE 442u
#define TINY_SYNTH_HEADER 8u
#define SYNTH_WAVE_OFFSET 0x44u

static void expand_tiny_synth_instr(uint8_t** data, uint32_t* size) {
    if (*size != TINY_SYNTH_FILE_SIZE || nuade_read_be32(*data) != 1)
        return;
    uint32_t full = SYNTH_WAVE_OFFSET + (*size - TINY_SYNTH_HEADER);
    uint8_t* expanded = (uint8_t*)calloc(1, full);
    if (!expanded)
        return;
    memcpy(expanded + SYNTH_WAVE_OFFSET, *data + TINY_SYNTH_HEADER, *size - TINY_SYNTH_HEADER);
    free(*data);
    *data = expanded;
    *size = full;
}

static bool decode_ss_pcm8(const uint8_t* data, uint32_t size, int8_t** out_pcm, uint32_t* out_pcm_len,
                           uint32_t* out_loop_start, uint32_t* out_loop_len) {
    *out_loop_start = 0;
    *out_loop_len = 0;
    *out_pcm = NULL;
    *out_pcm_len = 0;

    if (size == 0)
        return false;

    if (size >= 12 && memcmp(data, "FORM", 4) == 0) {
        uint32_t form_size = nuade_read_be32(data + 4);
        if (form_size + 8 <= size) {
            size_t pos = 12;
            size_t end = form_size + 8;
            uint32_t oneshot = 0, repeat = 0;
            bool has_body = false;
            while (pos + 8 <= end && pos + 8 <= size) {
                const uint8_t* ch = data + pos;
                uint32_t len = nuade_read_be32(ch + 4);
                size_t data_pos = pos + 8;
                if (data_pos + len > size)
                    break;
                if (memcmp(ch, "VHDR", 4) == 0 && len >= 12) {
                    oneshot = nuade_read_be32(data + data_pos + 0);
                    repeat = nuade_read_be32(data + data_pos + 4);
                } else if (memcmp(ch, "BODY", 4) == 0) {
                    int8_t* pcm = (int8_t*)malloc(len);
                    if (!pcm)
                        break;
                    for (uint32_t i = 0; i < len; i++) {
                        pcm[i] = (int8_t)data[data_pos + i];
                    }
                    *out_pcm = pcm;
                    *out_pcm_len = len;
                    has_body = true;
                }
                pos = data_pos + ((len + 1u) & ~1u);
            }
            if (has_body) {
                if (repeat > 1 && oneshot + repeat <= *out_pcm_len) {
                    *out_loop_start = oneshot;
                    *out_loop_len = repeat;
                }
                return true;
            }
        }
    }

    size_t offset = (size > 62) ? 62 : 0;
    if (offset >= size)
        return false;

    uint32_t pcm_len = (uint32_t)(size - offset);
    int8_t* pcm = (int8_t*)malloc(pcm_len);
    if (!pcm)
        return false;
    for (uint32_t i = 0; i < pcm_len; i++) {
        pcm[i] = (int8_t)data[offset + i];
    }
    *out_pcm = pcm;
    *out_pcm_len = pcm_len;

    if (size >= 4) {
        uint32_t h0 = nuade_read_be16(data + 0);
        uint32_t h1 = nuade_read_be16(data + 2);
        uint32_t ls = h0 * 2u;
        uint32_t ll = h1 * 2u;
        if (ll > 2 && ls < pcm_len && (ls + ll) <= pcm_len) {
            *out_loop_start = ls;
            *out_loop_len = ll;
        }
    }

    return pcm_len > 0;
}

static uint8_t parse_ss_base_note(const uint8_t* ss_data, uint32_t ss_size) {
    if (ss_size < 2)
        return 60;
    uint32_t p = nuade_read_be16(ss_data + 0);
    if (p >= 100 && p <= 1200)
        return closest_note_from_period(p);
    return 60;
}

// Instrument directory discovery

// Song dir, then Instruments and instruments at its parent, grandparent and great-grandparent levels (in that
// order: the first match of a file name wins).
#define SONIX_MAX_SEARCH_DIRS 7

typedef struct {
    char dirs[SONIX_MAX_SEARCH_DIRS][512];
    const char* list[SONIX_MAX_SEARCH_DIRS];
    int count;
} SonixSearchDirs;

// Paths that do not fit are skipped instead of truncated.
static void add_search_subdir(SonixSearchDirs* sd, const char* dir, const char* name) {
    if (nuade_path_join(dir, name, sd->dirs[sd->count], sizeof(sd->dirs[0]))) {
        sd->list[sd->count] = sd->dirs[sd->count];
        sd->count++;
    }
}

static void collect_search_dirs(const char* song_path, SonixSearchDirs* sd) {
    sd->count = 0;
    char dir[512];
    if (!nuade_path_parent(song_path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), ".");
    }
    snprintf(sd->dirs[0], sizeof(sd->dirs[0]), "%s", dir);
    sd->list[0] = sd->dirs[0];
    sd->count = 1;

    for (int level = 0; level < 3; level++) {
        add_search_subdir(sd, dir, "Instruments");
        add_search_subdir(sd, dir, "instruments");
        char up[512];
        if (!nuade_path_parent(dir, up, sizeof(up))) {
            break;
        }
        snprintf(dir, sizeof(dir), "%s", up);
    }
}

// Orchestrator

int sonix_song_load_instruments(SonixSong* song, const NuadeIo* io, const char* song_path) {
    const SonixSongMetadata* meta = sonix_song_get_metadata(song);
    if (!meta || !sonix_song_has_runtime_track_engine(song))
        return 0;
    if (meta->format != SONIX_FORMAT_SNX && meta->format != SONIX_FORMAT_SMUS && meta->format != SONIX_FORMAT_TINY)
        return 0;

    if (io == NULL || song_path == NULL)
        return 0;

    SonixSearchDirs sd;
    collect_search_dirs(song_path, &sd);

    NuadeFileIndex* files = nuade_file_index_create(io, sd.list, (size_t)sd.count);
    if (files == NULL)
        return 0;

    int loaded = 0;

    // Simulate Amiga chip memory: all raw .ss files are loaded contiguously
    // into a single block (matching UADE's AllocMem behavior). When a zone's
    // DMA extends past one file's data, it reads into the next file's data.
    uint8_t* chip_mem = NULL;
    size_t chip_total = 0;
    size_t chip_capacity = 0;

    // Deferred zone creation for raw .ss instruments (need all .ss files
    // loaded into chip_mem before creating zones)
    typedef struct {
        uint8_t inst_index;
        size_t chip_offset;
        uint32_t ss_size;
        uint16_t total_half, non_loop_half;
        uint8_t min_oct, max_oct;
    } DeferredSS;
    DeferredSS deferred_ss[64];
    int deferred_ss_count = 0;

    for (int i = 0; i < 64; i++) {
        const char* iname = sonix_song_get_instrument_name(song, (uint8_t)i);
        if (!iname || !iname[0])
            continue;

        char instr_key[160];
        snprintf(instr_key, sizeof(instr_key), "%s.instr", iname);
        const char* instr_path = nuade_file_index_find(files, instr_key);
        if (!instr_path)
            continue;

        uint8_t* instr_data = NULL;
        uint32_t instr_size = 0;
        if (!read_file(io, instr_path, &instr_data, &instr_size))
            continue;

        expand_tiny_synth_instr(&instr_data, &instr_size);

        // Handle compact TINY .instr reference files (32 bytes):
        // Offset 2: type word (2 = SS reference)
        // Offset 4: 4-char instrument name → look up <name>.ss
        // We extract the reference name and jump to SS loading below.
        char tiny_ss_ref[8];
        tiny_ss_ref[0] = '\0';
        if (instr_size == 32 && instr_data[0] == 0 && instr_data[1] == 0 && nuade_read_be16(instr_data + 2) == 2) {
            memcpy(tiny_ss_ref, instr_data + 4, 4);
            tiny_ss_ref[4] = '\0';
        }

        // Try synthesis
        {
            int8_t* pcm = NULL;
            uint32_t pcm_len = 0;
            int8_t* wave128 = NULL;
            uint32_t wave_len = 0;
            uint16_t synth_c2 = 0, synth_c4 = 0;
            if (decode_synthesis_wave(instr_data, instr_size, &pcm, &pcm_len, &wave128, &wave_len, &synth_c2,
                                      &synth_c4)) {
                if (sonix_song_set_instrument_pcm8(song, (uint8_t)i, pcm, pcm_len, 0, pcm_len, 0)) {
                    loaded++;
                }
                sonix_song_set_instrument_synth(song, (uint8_t)i, true);
                if (wave128) {
                    sonix_song_set_synth_wave(song, (uint8_t)i, wave128);
                    sonix_song_set_synth_blend_params(song, (uint8_t)i, synth_c2, synth_c4);
                }
                if (instr_size >= 0x1F6) {
                    uint16_t base_vol = nuade_read_be16(instr_data + 0x1CC);
                    uint16_t port_flag = nuade_read_be16(instr_data + 0x1CE);
                    sonix_song_set_synth_vol_params(song, (uint8_t)i, base_vol, port_flag);
                    uint16_t targets[4], speeds[4];
                    for (int j = 0; j < 4; j++) {
                        targets[j] = nuade_read_be16(instr_data + 0x1E6 + j * 2);
                        speeds[j] = nuade_read_be16(instr_data + 0x1EE + j * 2);
                    }
                    sonix_song_set_ss_envelope(song, (uint8_t)i, 0, targets, speeds);

                    uint16_t filt_base = nuade_read_be16(instr_data + 0x1D6);
                    uint16_t filt_range = nuade_read_be16(instr_data + 0x1D8);
                    uint16_t filt_env = nuade_read_be16(instr_data + 0x1DA);
                    sonix_song_set_synth_filter_params(song, (uint8_t)i, filt_base, filt_range, filt_env);

                    uint16_t env_rate = nuade_read_be16(instr_data + 0x1DC);
                    int16_t env_loop = (int16_t)nuade_read_be16(instr_data + 0x1DE);
                    uint16_t env_delay = nuade_read_be16(instr_data + 0x1E0);
                    uint16_t env_vscale = nuade_read_be16(instr_data + 0x1D0);
                    uint16_t env_pscale = nuade_read_be16(instr_data + 0x1D4);
                    sonix_song_set_synth_env_params(song, (uint8_t)i, env_rate, env_loop, env_delay, env_vscale,
                                                    env_pscale);

                    if (instr_size >= 0x1D4) {
                        uint16_t slide_rate = nuade_read_be16(instr_data + 0x1D2);
                        sonix_song_set_synth_slide_rate(song, (uint8_t)i, slide_rate);
                    }
                }

                if (instr_size >= 0xC4 + 256) {
                    sonix_song_set_synth_env_table(song, (uint8_t)i, (const int8_t*)(instr_data + 0xC4));
                }

                free(pcm);
                free(wave128);
                free(instr_data);
                continue;
            }
            free(pcm);
            free(wave128);
        }

        // Try 8SVX
        if (instr_size >= 12 && memcmp(instr_data + 8, "8SVX", 4) == 0) {
            SonixSvxOctaveZone svx_zones[8];
            uint16_t svx_rate = 0;
            uint32_t svx_vol = 0x10000;
            int nz = decode_8svx_zones(instr_data, instr_size, svx_zones, 8, &svx_rate, &svx_vol);
            if (nz > 0) {
                for (int zi = 0; zi < nz; zi++) {
                    sonix_song_add_instrument_zone(song, (uint8_t)i, (const int8_t*)svx_zones[zi].data,
                                                   svx_zones[zi].length, svx_zones[zi].loop_start,
                                                   svx_zones[zi].loop_len, svx_zones[zi].low_key, svx_zones[zi].low_key,
                                                   svx_zones[zi].high_key, 0);
                }
                sonix_song_set_instrument_pcm8_ex(song, (uint8_t)i, (const int8_t*)svx_zones[0].data,
                                                  svx_zones[0].length, svx_zones[0].loop_start, svx_zones[0].loop_len,
                                                  svx_zones[0].low_key, 0, svx_rate);
                sonix_song_set_instrument_iff(song, (uint8_t)i, true, svx_vol);
                loaded++;
                free(instr_data);
                continue;
            }
        }

        // Try LIST/AIFF (multi-sample)
        if (instr_size >= 12 && memcmp(instr_data, "LIST", 4) == 0) {
            SonixAiffZone zones[SONIX_MAX_AIFF_ZONES];
            int nz = decode_aiff_zones(instr_data, instr_size, zones, SONIX_MAX_AIFF_ZONES);
            if (nz > 0) {
                for (int zi = 0; zi < nz; zi++) {
                    SonixAiffZone* z = &zones[zi];
                    sonix_song_add_instrument_zone(song, (uint8_t)i, z->pcm, z->pcm_len, z->loop_start, z->loop_len,
                                                   z->base_note, z->low_key, z->high_key, z->source_rate_hz);
                    sonix_song_set_zone_attack_decay(song, (uint8_t)i, (uint8_t)zi, z->attack_time, z->decay_time);
                }
                SonixAiffZone* z0 = &zones[0];
                sonix_song_set_instrument_pcm8_ex(song, (uint8_t)i, z0->pcm, z0->pcm_len, z0->loop_start, z0->loop_len,
                                                  z0->base_note, 0, z0->source_rate_hz);
                loaded++;
                for (int zi = 0; zi < nz; zi++)
                    free(zones[zi].pcm);
                free(instr_data);
                continue;
            }
        }

        // Try single AIFF
        {
            int8_t* pcm = NULL;
            uint32_t pcm_len = 0;
            uint32_t loop_start = 0, loop_len = 0;
            uint8_t base_note = 60;
            uint32_t source_rate_hz = 0;
            uint8_t aiff_low_key = 0, aiff_high_key = 127;
            bool aiff_have_inst = false;

            if (decode_aiff_pcm8(instr_data, instr_size, &pcm, &pcm_len, &loop_start, &loop_len, &base_note,
                                 &source_rate_hz, &aiff_low_key, &aiff_high_key, &aiff_have_inst)) {
                if (aiff_have_inst) {
                    sonix_song_add_instrument_zone(song, (uint8_t)i, pcm, pcm_len, loop_start, loop_len, base_note,
                                                   aiff_low_key, aiff_high_key, source_rate_hz);
                    uint16_t atk_z = 0, dec_z = 0;
                    if (parse_aiff_name_attack_decay(instr_data, instr_size, &atk_z, &dec_z)) {
                        sonix_song_set_zone_attack_decay(song, (uint8_t)i, 0, atk_z, dec_z);
                    }
                }
                if (sonix_song_set_instrument_pcm8_ex(song, (uint8_t)i, pcm, pcm_len, loop_start, loop_len, base_note,
                                                      0, source_rate_hz)) {
                    uint16_t atk = 0, dec = 0;
                    if (parse_aiff_name_attack_decay(instr_data, instr_size, &atk, &dec)) {
                        sonix_song_set_instrument_attack_decay(song, (uint8_t)i, atk, dec);
                    }
                    loaded++;
                }
                free(pcm);
                free(instr_data);
                continue;
            }

            free(pcm);
        }

        // Try SampledSound + .ss file
        char sample_ref[128];
        parse_sampled_sound_ref(instr_data, instr_size, iname, sample_ref, sizeof(sample_ref));

        // Fall back to compact TINY .instr reference
        if (sample_ref[0] == '\0' && tiny_ss_ref[0] != '\0') {
            snprintf(sample_ref, sizeof(sample_ref), "%s", tiny_ss_ref);
        }

        if (sample_ref[0] == '\0') {
            free(instr_data);
            continue;
        }

        char ss_key[160];
        snprintf(ss_key, sizeof(ss_key), "%s.ss", sample_ref);
        const char* ss_path = nuade_file_index_find(files, ss_key);
        if (!ss_path) {
            snprintf(ss_key, sizeof(ss_key), "%s.ss", iname);
            ss_path = nuade_file_index_find(files, ss_key);
            if (!ss_path) {
                free(instr_data);
                continue;
            }
        }

        uint8_t* ss_data = NULL;
        uint32_t ss_size = 0;
        if (!read_file(io, ss_path, &ss_data, &ss_size)) {
            free(instr_data);
            continue;
        }

        // Check if .ss is IFF/AIFF
        if (ss_size >= 12 && memcmp(ss_data, "FORM", 4) == 0) {
            int8_t* pcm = NULL;
            uint32_t pcm_len = 0;
            uint32_t loop_start = 0, loop_len = 0;
            uint8_t base_note = parse_sampled_sound_base_note(instr_data, instr_size);
            uint32_t base_period = parse_sampled_sound_base_period(instr_data, instr_size);
            if (decode_ss_pcm8(ss_data, ss_size, &pcm, &pcm_len, &loop_start, &loop_len)) {
                if (base_note == 60) {
                    uint8_t from_ss = parse_ss_base_note(ss_data, ss_size);
                    if (from_ss != 60)
                        base_note = from_ss;
                }
                sonix_song_set_instrument_pcm8_ex(song, (uint8_t)i, pcm, pcm_len, loop_start, loop_len, base_note,
                                                  base_period, 0);
                loaded++;
            }
            free(pcm);
            free(ss_data);
            free(instr_data);
            continue;
        }

        // Raw .ss format: multi-octave
        if (ss_size < 62) {
            free(ss_data);
            free(instr_data);
            continue;
        }

        uint16_t total_half = nuade_read_be16(ss_data + 0);
        uint16_t non_loop_half = nuade_read_be16(ss_data + 2);
        // The asm range check (SSTech lbC00DB52) compares the octave bytes as signed values.
        int8_t min_oct = (int8_t)ss_data[4];
        int8_t max_oct = (int8_t)ss_data[5];

        if (total_half == 0 || max_oct > 10 || (min_oct < 0 && min_oct <= max_oct)) {
            free(ss_data);
            free(instr_data);
            continue;
        }

        // An empty octave range (min > max, e.g. max 0xff) is a valid SS instrument that drops every note:
        // the channel still adopts it (instrument pointer and velocity) while an earlier note keeps ticking.
        if (min_oct <= max_oct) {
            // Append this .ss file to the contiguous chip memory buffer
            size_t needed = chip_total + ss_size;
            if (needed > chip_capacity) {
                chip_capacity = needed + needed / 2 + 4096;
                chip_mem = (uint8_t*)realloc(chip_mem, chip_capacity);
            }
            memcpy(chip_mem + chip_total, ss_data, ss_size);

            // Defer zone creation until all .ss files are in chip_mem
            DeferredSS* def = &deferred_ss[deferred_ss_count++];
            def->inst_index = (uint8_t)i;
            def->chip_offset = chip_total;
            def->ss_size = ss_size;
            def->total_half = total_half;
            def->non_loop_half = non_loop_half;
            def->min_oct = (uint8_t)min_oct;
            def->max_oct = (uint8_t)max_oct;
            chip_total = needed;
        }

        // Set SS metadata now (doesn't depend on zone data)
        sonix_song_set_instrument_ss(song, (uint8_t)i, true);

        if (instr_size >= 0x7A) {
            uint16_t inst_vol = nuade_read_be16(instr_data + 0x68);
            uint16_t targets[4] = {
                nuade_read_be16(instr_data + 0x6A),
                nuade_read_be16(instr_data + 0x6C),
                nuade_read_be16(instr_data + 0x6E),
                nuade_read_be16(instr_data + 0x70),
            };
            uint16_t speeds[4] = {
                nuade_read_be16(instr_data + 0x72),
                nuade_read_be16(instr_data + 0x74),
                nuade_read_be16(instr_data + 0x76),
                nuade_read_be16(instr_data + 0x78),
            };
            sonix_song_set_ss_envelope(song, (uint8_t)i, inst_vol, targets, speeds);
            // SStech vibrato: depth 0x7A, speed 0x7C, delay 0x7E (asm $5A/$5C/$5E), SMUS and SNX alike.
            if (meta->format != SONIX_FORMAT_TINY && instr_size >= 0x80) {
                sonix_song_set_instrument_mod(song, (uint8_t)i, nuade_read_be16(instr_data + 0x7A),
                                              nuade_read_be16(instr_data + 0x7C), nuade_read_be16(instr_data + 0x7E));
            }
        } else if (tiny_ss_ref[0] != '\0' && instr_size == 32) {
            uint16_t inst_vol = nuade_read_be16(instr_data + 0x08);
            uint16_t targets[4] = {
                nuade_read_be16(instr_data + 0x0A),
                nuade_read_be16(instr_data + 0x0C),
                nuade_read_be16(instr_data + 0x0E),
                nuade_read_be16(instr_data + 0x10),
            };
            uint16_t speeds[4] = {
                nuade_read_be16(instr_data + 0x12),
                nuade_read_be16(instr_data + 0x14),
                nuade_read_be16(instr_data + 0x16),
                nuade_read_be16(instr_data + 0x18),
            };
            sonix_song_set_ss_envelope(song, (uint8_t)i, inst_vol, targets, speeds);
            // The compact file is the SMUS SampledSound tail from 0x60 on: vibrato depth/speed/delay at 0x1A/0x1C/0x1E.
            sonix_song_set_instrument_mod(song, (uint8_t)i, nuade_read_be16(instr_data + 0x1A),
                                          nuade_read_be16(instr_data + 0x1C), nuade_read_be16(instr_data + 0x1E));
        }

        loaded++;

        free(ss_data);
        free(instr_data);
    }

    // Second pass: create zones for raw .ss instruments using complete chip memory
    for (int d = 0; d < deferred_ss_count; d++) {
        const DeferredSS* def = &deferred_ss[d];
        const size_t pcm_base = 62;
        size_t cum_offset = 0;

        for (int oct = (int)def->min_oct; oct <= (int)def->max_oct; oct++) {
            uint32_t total_bytes = (uint32_t)def->total_half << oct;
            uint32_t non_loop_bytes = (uint32_t)def->non_loop_half << oct;
            uint32_t loop_bytes = total_bytes - non_loop_bytes;

            size_t pcm_offset = pcm_base + cum_offset;
            if (pcm_offset >= def->ss_size)
                break;

            int midi_low = (10 - oct) * 12;
            int midi_high = midi_low + 11;
            if (midi_low < 0)
                midi_low = 0;
            if (midi_high > 127)
                midi_high = 127;

            // Notes outside [min_oct, max_oct] are cancelled by the tech (asm 8194-8201), so the edge
            // zones must not be widened.
            uint8_t low_key = (uint8_t)midi_low;
            uint8_t high_key = (uint8_t)midi_high;

            size_t chip_pcm_start = def->chip_offset + pcm_offset;
            size_t chip_available = (chip_pcm_start + total_bytes <= chip_total)
                                        ? total_bytes
                                        : (chip_pcm_start < chip_total ? chip_total - chip_pcm_start : 0);
            const int8_t* zone_pcm;
            int8_t* padded = NULL;
            if (chip_available >= total_bytes) {
                zone_pcm = (const int8_t*)(chip_mem + chip_pcm_start);
            } else {
                padded = (int8_t*)calloc(total_bytes, 1);
                if (padded && chip_available > 0) {
                    memcpy(padded, chip_mem + chip_pcm_start, chip_available);
                }
                zone_pcm = padded ? (const int8_t*)padded : (const int8_t*)(chip_mem + chip_pcm_start);
            }

            sonix_song_add_instrument_zone(song, def->inst_index, zone_pcm, total_bytes, non_loop_bytes, loop_bytes,
                                           low_key, low_key, high_key, 0);
            free(padded);
            cum_offset += total_bytes;
        }
    }

    free(chip_mem);
    nuade_file_index_destroy(files);
    return loaded;
}
