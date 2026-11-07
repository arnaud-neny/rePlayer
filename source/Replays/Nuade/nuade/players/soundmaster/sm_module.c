// SPDX-License-Identifier: MIT
// Sound Master player: the module check (the eagleplayer's Check2 and InitPlayer reduced to what the known code
// versions need), the table locator and the readers of the module's records.

#include "nuade_utils.h"
#include "sm_internal.h"

#define OFFSETS_TABLE_COUNT 7u
#define OFFSETS_HEADER_BYTES 5u // speed, pattern length, first and last position, filter

// The Fixed layout: the tables follow the silent word at fixed distances.
#define FIXED_POSITIONS_BYTES 0x200u
#define FIXED_BLOCKS_BYTES 0x800u
#define FIXED_INSTRUMENT_AREA 0x380u // the instrument table's room: 64 records of 14 bytes
#define FIXED_WAVE_BYTES 0x100u
#define FIXED_ARPEGGIO_BYTES 0x100u
#define FIXED_LENGTH_COLUMN 0x80u
#define FIXED_LOOP_OFFSET_COLUMN 0xC0u
#define FIXED_LOOP_LENGTH_COLUMN 0x100u
#define FIXED_SAMPLE_DATA 0x140u
#define FIXED_DMA_OFF_BEFORE_HEADER 0xB // the word of the previous tick's wave switches, before the header
#define FIXED_FADE_BEFORE_HEADER 4      // the global fade

#define SUBSONG_TABLE_AFTER_DUMMY 2u
#define SUBSONG_TABLE_MAX 8u
#define VOICE_DMA_AT 4u // the voice record's DMACON bit
#define DMA_VOICE_BITS 0xFu

#define MAX_INSTRUMENTS 64u
#define MAX_PATTERNS 256u
#define MAX_BLOCKS 256u
#define MAX_POSITIONS 256u

uint8_t smst_u8(const SmstModule* m, uint32_t address) {
    return address < m->limit ? m->memory[address] : 0;
}

uint16_t smst_u16(const SmstModule* m, uint32_t address) {
    return (uint16_t)(smst_u8(m, address) << 8 | smst_u8(m, address + 1));
}

uint32_t smst_u32(const SmstModule* m, uint32_t address) {
    return (uint32_t)smst_u16(m, address) << 16 | smst_u16(m, address + 2);
}

const int8_t* smst_sample_pointer(const SmstModule* m, uint32_t address) {
    return (const int8_t*)(m->memory + (address < m->size ? (address & ~1u) : m->size));
}

uint16_t smst_period(const SmstModule* m, uint8_t index) {
    return smst_u16(m, m->version->period_at + 2u * index);
}

// The sample slot a sample number selects. Offsets layout: ten bytes a slot, the offset counted from the slot table,
// which the replayer adds the sample base to. Fixed layout: four columns, indexed with byte arithmetic that drops the
// high bits of the number.

SmstSlot smst_slot(const SmstModule* m, uint8_t number) {
    const SmstTables* t = &m->tables;
    if (m->version->layout == SmstLayout_Offsets) {
        const uint32_t slot = t->samples + (uint32_t)(number & m->version->sample_index_mask) * SLOT_SIZE;
        return (SmstSlot) { .sample = t->samples + smst_u32(m, slot) + t->sample_bias,
                            .length = smst_u16(m, slot + 4),
                            .loop_offset = smst_u16(m, slot + 6),
                            .loop_length = smst_u16(m, slot + 8) };
    }
    const uint8_t word = (uint8_t)(number << 1);
    const uint8_t pointer = (uint8_t)(word << 1);
    const uint32_t columns = t->samples;
    return (SmstSlot) { .sample = t->sample_bias + smst_u32(m, columns + pointer),
                        .length = smst_u16(m, columns + FIXED_LENGTH_COLUMN + word),
                        .loop_offset = smst_u16(m, columns + FIXED_LOOP_OFFSET_COLUMN + word),
                        .loop_length = smst_u16(m, columns + FIXED_LOOP_LENGTH_COLUMN + word) };
}

SmstInstrument smst_instrument(const SmstModule* m, uint8_t number) {
    const uint32_t at = m->tables.instruments + (uint32_t)number * m->version->instrument_size;
    SmstInstrument ins = { .wave = smst_u8(m, at),
                           .attack = smst_u8(m, at + 1),
                           .peak = smst_u8(m, at + 2),
                           .decay = smst_u8(m, at + 3),
                           .sustain = smst_u8(m, at + 4),
                           .vibrato_delay = smst_u8(m, at + 5),
                           .vibrato_depth = smst_u8(m, at + 6),
                           .vibrato_speed = smst_u8(m, at + 7),
                           .release = smst_u8(m, at + 8),
                           .arpeggio_start = smst_u8(m, at + 9),
                           .arpeggio_length = smst_u8(m, at + 10),
                           .arpeggio_repeat = smst_u8(m, at + 11),
                           .arpeggio_note = smst_u8(m, at + 12),
                           .note_offset = smst_u8(m, at + 13) };
    if (m->version->sweep) {
        ins.sweep_period = smst_u8(m, at + 14);
        ins.sweep_step = smst_u8(m, at + 15);
    }
    return ins;
}

// The seven versions, named in the notes by their code size. The signature is FNV-1a over the code before the
// variables: it covers the code, the displacements of every table and variable it addresses, and the period table. The
// module's own bytes start at the variables.

// clang-format off
static const SmstVersion s_versions[] = {
    // fixed-2064
    { .vars = 0x810, .code_size = 0x808, .code_hash = 0x40a9f9e1e04d42ddull,
      .writes = { { -0x8, 0xdc } },
      .layout = SmstLayout_Fixed, .init = SmstInit_NoSubsongs, .dma_flow = SmstDmaFlow_RowThenNote,
      .loop_start = SmstLoopStart_Never,
      .voice_size = 0x34, .instrument_size = 14, .speed_at = 0xdd, .dummy_at = 0xe6, .period_at = 0x788,
      .voice_fade = false, .row_commands = true, .dummy_voices = false, .sweep = false, .release = true,
      .arpeggio_hold = false, .wave_skip_bit5 = false, .wave_repeat_check = false,
      .period_sets_last_note = false, .period_wide = true,
      .rest_after_release = false, .release_picks_instrument = false, .vibrato_limit = false },
    // fixed-2176
    { .vars = 0x880, .code_size = 0x878, .code_hash = 0x1a998d65d89d7905ull,
      .writes = { { -0x8, 0xec } },
      .layout = SmstLayout_Fixed, .init = SmstInit_NoSubsongs, .dma_flow = SmstDmaFlow_RowThenNote,
      .loop_start = SmstLoopStart_SelectedWave,
      .voice_size = 0x38, .instrument_size = 16, .speed_at = 0xed, .dummy_at = 0xf6, .period_at = 0x81c,
      .voice_fade = false, .row_commands = true, .dummy_voices = false, .sweep = true, .release = true,
      .arpeggio_hold = true, .wave_skip_bit5 = false, .wave_repeat_check = true,
      .period_sets_last_note = true, .period_wide = false,
      .rest_after_release = false, .release_picks_instrument = false, .vibrato_limit = true },
    // offsets-2058
    { .vars = 0x80a, .code_size = 0x7f6, .code_hash = 0x536915ded7342e8cull,
      .writes = { { -0x18, 0xc8 } },
      .layout = SmstLayout_Offsets, .init = SmstInit_NoSubsongs, .dma_flow = SmstDmaFlow_Combined,
      .loop_start = SmstLoopStart_Instrument,
      .voice_size = 0x32, .instrument_size = 16, .speed_at = 0xc9, .data_at = 0xec, .dummy_at = 0xea,
      .period_at = 0x796, .sample_index_mask = 0x1f,
      .voice_fade = true, .row_commands = true, .dummy_voices = true, .sweep = false, .release = false,
      .arpeggio_hold = false, .wave_skip_bit5 = true, .wave_repeat_check = true,
      .period_sets_last_note = false, .period_wide = false,
      .rest_after_release = true, .release_picks_instrument = true, .vibrato_limit = true },
    // offsets-2086
    { .vars = 0x826, .code_size = 0x812, .code_hash = 0xa26d50cb1f876c92ull,
      .writes = { { -0x18, 0xc8 }, { 0xcb, 0xcd } },
      .layout = SmstLayout_Offsets, .init = SmstInit_Window, .dma_flow = SmstDmaFlow_Combined,
      .loop_start = SmstLoopStart_Instrument,
      .voice_size = 0x32, .instrument_size = 16, .speed_at = 0xc9, .data_at = 0xec, .dummy_at = 0xea,
      .period_at = 0x7b2, .sample_index_mask = 0x1f,
      .voice_fade = true, .row_commands = true, .dummy_voices = true, .sweep = false, .release = false,
      .arpeggio_hold = false, .wave_skip_bit5 = true, .wave_repeat_check = true,
      .period_sets_last_note = false, .period_wide = false,
      .rest_after_release = true, .release_picks_instrument = true, .vibrato_limit = true },
    // offsets-2246
    { .vars = 0x8c6, .code_size = 0x8b2, .code_hash = 0x02c470da68d63347ull,
      .writes = { { -0x18, 0xf0 }, { 0xf3, 0xf5 } },
      .layout = SmstLayout_Offsets, .init = SmstInit_Table, .dma_flow = SmstDmaFlow_RowThenNote,
      .loop_start = SmstLoopStart_Instrument,
      .voice_size = 0x3c, .instrument_size = 16, .speed_at = 0xf1, .data_at = 0x12c, .dummy_at = 0x112,
      .period_at = 0x852, .sample_index_mask = 0x7f,
      .voice_fade = true, .row_commands = true, .dummy_voices = false, .sweep = true, .release = true,
      .arpeggio_hold = true, .wave_skip_bit5 = false, .wave_repeat_check = true,
      .period_sets_last_note = false, .period_wide = false,
      .rest_after_release = true, .release_picks_instrument = false, .vibrato_limit = true },
    // offsets-2352
    { .vars = 0x930, .code_size = 0x91c, .code_hash = 0x80fcd3bec80c4bcaull,
      .writes = { { -0x68, -0x64 }, { -0x14, 0xd0 } },
      .builtin_at = 0xa0, .builtin_count = 8, .builtin_template = -0x64, .builtin_origin = 0x103f2,
      .layout = SmstLayout_Offsets, .init = SmstInit_Mode, .dma_flow = SmstDmaFlow_Combined,
      .loop_start = SmstLoopStart_Instrument,
      .voice_size = 0x34, .instrument_size = 16, .speed_at = 0xd1, .data_at = 0xf4, .dummy_at = 0xf2,
      .period_at = 0x86c, .sample_index_mask = 0x1f,
      .voice_fade = true, .row_commands = false, .dummy_voices = true, .sweep = false, .release = false,
      .arpeggio_hold = false, .wave_skip_bit5 = true, .wave_repeat_check = true,
      .period_sets_last_note = false, .period_wide = false,
      .rest_after_release = true, .release_picks_instrument = true, .vibrato_limit = true },
    // offsets-2526
    { .vars = 0x9de, .code_size = 0x9ca, .code_hash = 0xb021d6d14c1085baull,
      .writes = { { -0x90, -0x8c }, { -0x14, 0xd8 } },
      .builtin_at = 0x64, .builtin_count = 12, .builtin_template = -0x8c, .builtin_origin = -0xada,
      .layout = SmstLayout_Offsets, .init = SmstInit_Mode, .dma_flow = SmstDmaFlow_Combined,
      .loop_start = SmstLoopStart_Instrument,
      .voice_size = 0x36, .instrument_size = 16, .speed_at = 0xd9, .data_at = 0xfc, .dummy_at = 0xfa,
      .period_at = 0x8f2, .sample_index_mask = 0x1f,
      .voice_fade = true, .row_commands = false, .dummy_voices = true, .sweep = false, .release = false,
      .arpeggio_hold = false, .wave_skip_bit5 = true, .wave_repeat_check = true,
      .period_sets_last_note = false, .period_wide = false,
      .rest_after_release = true, .release_picks_instrument = true, .vibrato_limit = true },
};
// clang-format on

static uint64_t fnv1a(const uint8_t* data, size_t size) {
    uint64_t hash = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ data[i]) * 0x100000001b3ull;
    }
    return hash;
}

const SmstVersion* smst_find_version(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < nuade_sizeof_array(s_versions); i++) {
        const SmstVersion* v = &s_versions[i];
        if (size > v->vars && fnv1a(data, v->code_size) == v->code_hash) {
            return v;
        }
    }
    return nullptr;
}

uint8_t smst_initial_wave_switches(const SmstModule* m) {
    return (uint8_t)smst_u16(m, m->version->vars + m->version->speed_at - FIXED_DMA_OFF_BEFORE_HEADER);
}

uint8_t smst_initial_fade(const SmstModule* m) {
    return smst_u8(m, m->version->vars + m->version->speed_at - FIXED_FADE_BEFORE_HEADER);
}

SmstSubsong smst_subsong(const SmstModule* m, uint32_t index) {
    const uint32_t at
        = m->version->vars + m->version->dummy_at + SUBSONG_TABLE_AFTER_DUMMY + index * SUBSONG_ENTRY_SIZE;
    return (SmstSubsong) { .position = smst_u8(m, at), .loop_start = smst_u8(m, at + 1), .end = smst_u8(m, at + 2) };
}

int smst_count_subsongs(const SmstModule* m) {
    switch (m->version->init) {
        case SmstInit_Window:
            return 2;
        case SmstInit_Table: {
            // InitPlayer walks the table from its second entry until one reads 0, 0, 1
            int count = 1;
            for (uint32_t i = 1; i < SUBSONG_TABLE_MAX; i++) {
                const SmstSubsong s = smst_subsong(m, i);
                if (s.position == 0 && s.loop_start == 0 && s.end == 1) {
                    break;
                }
                count++;
            }
            return count;
        }
        case SmstInit_NoSubsongs:
        case SmstInit_Mode:
            break;
    }
    return 1;
}

// True when the bytes [from, from + bytes) of the module memory, counted modulo 2^32 like the addresses of the
// original, include one the replayer writes.

static bool reaches_writes(const SmstModule* m, uint32_t from, uint32_t bytes) {
    for (int i = 0; i < SMST_MAX_WRITES; i++) {
        const SmstRange* w = &m->version->writes[i];
        if (w->from == w->to) {
            continue;
        }
        const uint32_t first = m->version->vars + (uint32_t)w->from;
        const uint32_t length = (uint32_t)(w->to - w->from);
        if ((uint32_t)(first - from) < bytes || (uint32_t)(from - first) < length) {
            return true;
        }
    }
    return false;
}

// The tables the replayer reads as data, with the most it can reach through any note, row or position number.

static bool tables_clear_of_writes(const SmstModule* m) {
    const SmstTables* t = &m->tables;
    const SmstVersion* v = m->version;
    const uint32_t pattern_bytes = (MAX_PATTERNS - 1) * m->pattern_length + 257;
    uint32_t positions = MAX_POSITIONS * 8 + 8;
    uint32_t blocks = MAX_BLOCKS * 8 + 8;
    uint32_t samples = (v->sample_index_mask + 1) * SLOT_SIZE;
    if (v->layout == SmstLayout_Fixed) {
        positions = FIXED_POSITIONS_BYTES + 0x100;
        blocks = FIXED_BLOCKS_BYTES;
        samples = 0x204;
    }
    for (int i = 0; i < SMST_VOICES; i++) {
        if (reaches_writes(m, t->blocks + m->voice_column[i], blocks)) {
            return false;
        }
    }
    return !reaches_writes(m, t->positions, positions)
           && !reaches_writes(m, t->instruments, MAX_INSTRUMENTS * v->instrument_size)
           && !reaches_writes(m, t->arpeggio - 1, 257) && !reaches_writes(m, t->wave - 1, 257)
           && !reaches_writes(m, t->patterns, pattern_bytes) && !reaches_writes(m, t->samples, samples)
           && !reaches_writes(m, t->dummy, 2);
}

bool smst_locate(SmstModule* m, const SmstVersion* version) {
    m->version = version;
    const uint32_t vars = version->vars;
    const uint32_t header = vars + version->speed_at;
    const uint32_t header_bytes
        = OFFSETS_HEADER_BYTES + (version->layout == SmstLayout_Offsets ? OFFSETS_TABLE_COUNT * 4u : 4u);
    if (m->size < header || m->size - header < header_bytes) {
        return false;
    }
    m->speed = smst_u8(m, header);
    m->pattern_length = smst_u8(m, header + 1);
    m->first_position = smst_u8(m, header + 2);
    m->last_position = smst_u8(m, header + 3);

    SmstTables* t = &m->tables;
    t->dummy = vars + version->dummy_at;
    const uint32_t offsets = header + OFFSETS_HEADER_BYTES;
    if (version->layout == SmstLayout_Offsets) {
        const uint32_t data = vars + version->data_at;
        t->positions = data;
        t->blocks = data + smst_u32(m, offsets);
        t->instruments = data + smst_u32(m, offsets + 4);
        t->arpeggio = data + smst_u32(m, offsets + 8);
        t->wave = data + smst_u32(m, offsets + 12);
        t->patterns = data + smst_u32(m, offsets + 16);
        t->sample_bias = smst_u32(m, offsets + 20);
        t->samples = data + smst_u32(m, offsets + 24);
    } else {
        const uint32_t pattern_bytes = smst_u32(m, offsets);
        t->positions = t->dummy + 2;
        t->blocks = t->positions + FIXED_POSITIONS_BYTES;
        t->instruments = t->blocks + FIXED_BLOCKS_BYTES;
        t->arpeggio = t->instruments + FIXED_INSTRUMENT_AREA;
        t->wave = t->arpeggio + FIXED_ARPEGGIO_BYTES;
        t->patterns = t->wave + FIXED_WAVE_BYTES;
        t->samples = t->patterns + pattern_bytes;
        t->sample_bias = t->samples + FIXED_SAMPLE_DATA;
        // the word of wave switches the init does not clear: a DMACON write of it must clear channels only
        if (smst_u16(m, header - FIXED_DMA_OFF_BEFORE_HEADER) > DMA_VOICE_BITS) {
            return false;
        }
    }
    for (uint32_t i = 0; i < SMST_VOICES; i++) {
        const uint32_t record = vars + i * version->voice_size;
        m->voice_column[i] = smst_u32(m, record);
        // the port gives every voice its own channel: the record must name it
        if (smst_u16(m, record + VOICE_DMA_AT) != 1u << i) {
            return false;
        }
    }
    if (!tables_clear_of_writes(m)) {
        return false;
    }
    if (version->builtin_count != 0) {
        // the init fills these slots of the module's own table: inside the image, and not over the code
        const uint64_t first = (uint64_t)t->samples + version->builtin_at;
        if (first + version->builtin_count * SLOT_SIZE > m->limit || first < vars) {
            return false;
        }
    }
    return true;
}
