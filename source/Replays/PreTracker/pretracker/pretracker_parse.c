#include "pretracker_internal.h"

#include <string.h>

typedef struct {
    u8 restart_pos;
    u8 num_patterns;
    u8 num_steps;
    u8 num_positions;
    u32 position_offset;
    u32 pattern_offset;
} SongLayout;

static bool range_fits(u32 offset, u32 length, u32 size) {
    return offset <= size && length <= size - offset;
}

static bool validate_layout(const SongLayout* layout, const u8* prt_data, u32 prt_size,
                            u32 position_end, u32 pattern_end) {
    u32 position_bytes = (u32)layout->num_positions * NUM_CHANNELS * 2;
    u32 pattern_bytes = (u32)layout->num_patterns * layout->num_steps * 3;
    if (position_end > prt_size || pattern_end > prt_size ||
        !range_fits(layout->position_offset, position_bytes, position_end) ||
        !range_fits(layout->pattern_offset, pattern_bytes, pattern_end))
        return false;

    const u8* positions = prt_data + layout->position_offset;
    for (u32 position = 0; position < layout->num_positions; ++position) {
        for (u32 channel = 0; channel < NUM_CHANNELS; ++channel) {
            u8 pattern_num = positions[(position * NUM_CHANNELS + channel) * 2];
            if (pattern_num > layout->num_patterns)
                return false;
        }
    }
    return true;
}

static bool get_v15_layout(const u8* prt_data, u32 prt_size, u8 num_subsongs,
                           int subsong, SongLayout* layout) {
    u32 posd_offset = read_be32(prt_data + 0x04);
    u32 patt_offset = read_be32(prt_data + 0x08);
    u32 inst_offset = read_be32(prt_data + 0x0C);
    u32 headers_size = (u32)num_subsongs * 8;
    if (posd_offset >= patt_offset || patt_offset >= prt_size || inst_offset > prt_size ||
        patt_offset > inst_offset || !range_fits(posd_offset, headers_size, patt_offset))
        return false;

    int selected = (subsong >= 0 && subsong < num_subsongs) ? subsong : 0;
    u32 positions_before = 0;
    for (int i = 0; i <= selected; ++i) {
        const u8* header = prt_data + posd_offset + (u32)i * 8;
        if (i == selected) {
            u32 pattern_relative = read_be32(header + 4);
            if (pattern_relative > inst_offset - patt_offset)
                return false;

            u32 pattern_end = inst_offset;
            if (i + 1 < num_subsongs) {
                const u8* next_header = header + 8;
                u32 next_relative = read_be32(next_header + 4);
                if (next_relative < pattern_relative || next_relative > inst_offset - patt_offset)
                    return false;
                pattern_end = patt_offset + next_relative;
            }

            layout->restart_pos = header[0];
            layout->num_patterns = header[1];
            layout->num_steps = header[2];
            layout->num_positions = header[3];
            u32 position_base = posd_offset + headers_size;
            u32 preceding_bytes = positions_before * NUM_CHANNELS * 2;
            if (!range_fits(position_base, preceding_bytes, patt_offset))
                return false;
            layout->position_offset = position_base + preceding_bytes;
            layout->pattern_offset = patt_offset + pattern_relative;
            return validate_layout(layout, prt_data, prt_size, patt_offset, pattern_end);
        }
        positions_before += header[3];
    }
    return false;
}

static void apply_layout(SongState* song, u8* prt_data, const SongLayout* layout) {
    song->pat_restart_pos = layout->restart_pos;
    song->pat_pos_len = layout->num_positions;
    song->num_patterns = layout->num_patterns;
    song->num_steps = layout->num_steps;
    song->pos_data_adr = prt_data + layout->position_offset;
    song->patterns_ptr = prt_data + layout->pattern_offset;
}

bool pretracker_read_name_record(const u8** cursor, const u8* end, char* output, size_t output_size) {
    const u8* start = *cursor;
    const u8* p = start;

    while (p < end && (size_t)(p - start) <= 23) {
        if (*p == 0) {
            size_t length = (size_t)(p - start);
            if (output != NULL) {
                if (output_size == 0 || length >= output_size)
                    return false;
                memcpy(output, start, length);
                output[length] = '\0';
            }
            *cursor = p + 1;
            return true;
        }
        ++p;
    }
    return false;
}

u32 pretracker_parse_song(SongState* song, u8* prt_data, u32 prt_size, int subsong) {
    memset(song, 0, sizeof(SongState));

    if (prt_size < 0x5A)
        return 0;

    u32 header = read_be32(prt_data);
    u8 version = (u8)(header & 0xFF);
    u32 magic = header & 0xFFFFFF00;
    if (magic != 0x50525400) {
        return 0;
    }

    u8 max_inst_names = MAX_INSTRUMENTS;
    u32 posd_offset = read_be32(prt_data + 0x04);
    u32 patt_offset = read_be32(prt_data + 0x08);
    u32 inst_offset = read_be32(prt_data + 0x0C);

    if (version == 0x1E) {
        if (prt_size < 0x5B)
            return 0;
        // V1.5 subsong support
        u8 num_subsongs = prt_data[0x5A];
        song->num_subsongs = num_subsongs > 0 ? num_subsongs : 1;

        SongLayout layout;
        for (int i = 0; i < song->num_subsongs; ++i) {
            if (!get_v15_layout(prt_data, prt_size, song->num_subsongs, i, &layout))
                return 0;
        }
        if (!get_v15_layout(prt_data, prt_size, song->num_subsongs, subsong, &layout))
            return 0;
        apply_layout(song, prt_data, &layout);

        max_inst_names = 2 * MAX_INSTRUMENTS;
//     } else if (version > 0x1B) { // rePlayer: try it anyway, allows Bubble Bobble
//         return 0;                // rePlayer 
    } else {
        song->num_subsongs = 1;
        SongLayout layout = {
            .restart_pos = prt_data[0x3C],
            .num_patterns = prt_data[0x3D],
            .num_steps = prt_data[0x3F],
            .num_positions = prt_data[0x3E],
            .position_offset = posd_offset,
            .pattern_offset = patt_offset,
        };
        if (posd_offset >= prt_size || patt_offset >= prt_size ||
            inst_offset > prt_size || patt_offset > inst_offset ||
            !validate_layout(&layout, prt_data, prt_size, patt_offset, inst_offset))
            return 0;
        apply_layout(song, prt_data, &layout);
    }

    song->num_waves = prt_data[0x41];
    if (song->num_waves > MAX_WAVES || posd_offset >= prt_size || patt_offset >= prt_size)
        return 0;

    // Skip instrument names
    if (inst_offset >= prt_size)
        return 0;
    const u8* end = prt_data + prt_size;
    const u8* name_ptr = prt_data + inst_offset;
    for (int i = 0; i < max_inst_names; i++) {
        if (!pretracker_read_name_record(&name_ptr, end, NULL, 0))
            return 0;
    }
    u8* ptr = (u8*)name_ptr;

    // Parse instrument infos
    u8 num_instruments = prt_data[0x40];
    u8 actual_instruments = num_instruments;
    if (actual_instruments > MAX_INSTRUMENTS) {
        actual_instruments = MAX_INSTRUMENTS;
    }

    u8* inst_info_base = ptr;
    if ((size_t)(end - inst_info_base) < (size_t)num_instruments * sizeof(InstrumentInfo))
        return 0;
    // d0 in asm = ptr to after all inst infos = inst_info_base + num_instruments * 8
    u8* inst_pattern_ptr = inst_info_base + (u32)num_instruments * 8;

    for (int i = 0; i < actual_instruments; i++) {
        u8* ii = inst_info_base + i * 8;
        UnpackedInstrumentInfo* uii = &song->inst_infos[i];

        // rePlayer begin fix forsaken by virgill
        ii[0] &= 15; ii[1] &= 15; ii[2] &= 15;  ii[3] &= 15;
        ii[4] &= 15; ii[6] &= 15;
//         if (ii[0] >= 16 || ii[1] >= 16 || ii[2] >= 16 || ii[3] >= 16 ||
//             ii[4] >= 16 || ii[6] >= 16)
//             return 0;
        //rePlayer end

        u8 vd = ii[0]; // vibrato_delay
        uii->vibrato_delay = (i16)(s_vib_delay_table[vd] + 1);

        u8 vdp = ii[1]; // vibrato_depth
        uii->vibrato_depth = (i16)s_vib_depth_table[vdp];

        u8 vs = ii[2]; // vibrato_speed
        i16 speed_val = (i16)s_vib_speed_table[vs];
        // muls uii_vibrato_depth(a4),d1; asr.w #4,d1
        uii->vibrato_speed = (i16)((speed_val * uii->vibrato_depth) >> 4);

        u8 atk = ii[3]; // adsr_attack
        uii->adsr_attack = s_fast_roll_off_16[atk];

        u8 dec = ii[4]; // adsr_decay
        uii->adsr_decay = (i16)s_ramp_up_16[dec];

        u8 sus = ii[5]; // adsr_sustain
        if (sus == 15) {
            sus = 16;
        }
        uii->adsr_sustain = (i16)((u16)sus << 6);

        u8 rel = ii[6]; // adsr_release
        uii->adsr_release = s_ramp_up_16[rel];

        u8 steps = ii[7]; // pattern_steps
        uii->pattern_steps = steps;

        // Store instrument pattern pointer, advance past pattern data
        if ((size_t)(end - inst_pattern_ptr) < (size_t)steps * 3)
            return 0;
        song->inst_patterns_table[i] = inst_pattern_ptr;
        inst_pattern_ptr += (u32)steps * 3;
    }

    // Skip wave names
    u32 wave_offset = read_be32(prt_data + 0x10);
    if (wave_offset >= prt_size)
        return 0;
    name_ptr = prt_data + wave_offset;
    for (int i = 0; i < MAX_WAVES; i++) {
        if (!pretracker_read_name_record(&name_ptr, end, NULL, 0))
            return 0;
    }
    ptr = (u8*)name_ptr;

    // Align to even address
    if ((uintptr_t)ptr & 1) {
        if (ptr == end)
            return 0;
        ptr++;
    }
    if ((size_t)(end - ptr) < (size_t)song->num_waves * sizeof(WaveInfo))
        return 0;
    song->waveinfo_ptr = (WaveInfo*)ptr;

    // Wave generation ordering
    if (version > 0x19) {
        memcpy(song->wavegen_order_table, prt_data + 0x42, MAX_WAVES);
    } else {
        for (int i = 0; i < MAX_WAVES; i++) {
            song->wavegen_order_table[i] = (u8)i;
        }
    }
    // rePlayer begin: fix wigged by Curt Cool
    for (int i = 0; i < MAX_WAVES; i++) {
        song->wavegen_order_table[i] %= song->num_waves; // sanitize range
    }
    // rePlayer end

    // Calculate sample sizes
    u32 total_chip_mem = 2;
    if (song->num_waves > 0) {
        WaveInfo* wi = song->waveinfo_ptr;
        for (int i = 0; i < song->num_waves; i++) {
            song->waveinfo_table[i] = wi;
            u32 std_len = ((u32)wi->sam_len + 1) * HQ_MAX_PERIOD;
            song->wavelength_table[i] = std_len;

            u32 total_len = std_len;
            if (wi->flags & WI_FLAG_EXTRA_OCTAVES) {
                total_len = (std_len * 15) / 8;
            }
            song->wavetotal_table[i] = total_len;
            total_chip_mem += total_len;
            wi++;
        }
    }

    return total_chip_mem;
}

bool pretracker_apply_subsong(SongState* song, u8* prt_data, u32 prt_size, int subsong) {
    if (prt_size < 0x5B || prt_data[3] != 0x1E)
        return false;

    u8 num_subsongs = prt_data[0x5A];
    if (num_subsongs == 0)
        num_subsongs = 1;

    SongLayout layout;
    if (!get_v15_layout(prt_data, prt_size, num_subsongs, subsong, &layout))
        return false;
    apply_layout(song, prt_data, &layout);
    return true;
}

void pretracker_rebuild_pattern_table(SongState* song) {
    u32 step_size = (u32)song->num_steps * 3;
    u8* pattern = song->patterns_ptr;
    memset(song->pattern_table, 0, sizeof(song->pattern_table));
    for (int index = 0; index < song->num_patterns; index++) {
        song->pattern_table[index] = pattern;
        pattern += step_size;
    }
}
