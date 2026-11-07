// SPDX-License-Identifier: MIT
// Opening a Rob Hubbard module: the checks of the UADE eagleplayer "RobHubbard" (Check2 and InitPlayer), its patch of
// the sequence data, and the identification of the replayer the module carries.

#include "rh_module.h"

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include "rh_code.h"
#include <stdlib.h>

// Check2: five `bra.w` (the entry table), then `lea d16(pc),a0` and, at 0x1C, `rts` + `lea d16(pc),a0`.
#define OP_BRA_W 0x6000
#define OP_LEA_A0_PC 0x41FA
#define CHECK_ENTRIES 5
#define CHECK_ENTRY_STRIDE 4
#define CHECK_LEA_AT 0x14
#define CHECK_RTS_LEA_AT 0x1C
#define CHECK_RTS_LEA 0x4E7541FAu

// InitPlayer finds its anchors by scanning for opcode words.
#define OP_MOVE_L_POSTINC_D2 0x2418 // move.l (a0)+,d2: first instruction of the sample header loop
#define SAMPLE_LOOP_SCAN_AT 0x40
#define SAMPLE_LOOP_SCAN_WORDS 8
#define OP_ADDA_L_IMMEDIATE_A0 0xD1FC
#define SAMPLE_GAP 0x40 // `adda.l #$40,a0` behind the lea: the first sample header is that far from its target
#define SAMPLE_LEA_SCAN_AT 0x36
#define SAMPLE_LEA_SCAN_WORDS 5
#define OP_LEA_D16_A3_A0 0x41EB // lea d16(a3),a0
#define SONG_LEA_SCAN_AT 0x82
#define SONG_LEA_SCAN_WORDS 10
#define OP_NOP 0x4E71

#define SAMPLE_HEADER_SIZE 6 // size (long) and a divisor (word)
#define SAMPLE_SIZE_MAX 0x10000

// The module's subsong records: a zero byte, the tempo, one long for each voice.
#define SUBSONG_TEMPO_AT 1
#define SUBSONG_TRACK_LISTS_AT 2

typedef struct Layout {
    uint32_t sample_count_at; // the moveq whose operand counts the samples
    uint32_t sample_count;
    uint32_t sample_target; // what the lea in front of the sample loop names
    uint32_t samples;       // first sample header
    uint32_t song_table;
    uint32_t subsong_count;
    uint32_t patch_from; // first byte the eagleplayer scans for the stop command
    uint32_t patch_count;
} Layout;

static bool has_word(size_t size, uint32_t at) {
    return at <= size && size - at >= 2;
}

static bool check_header(const uint8_t* data, size_t size) {
    if (size < CHECK_RTS_LEA_AT + 4) {
        return false;
    }
    for (uint32_t i = 0; i < CHECK_ENTRIES; i++) {
        if (nuade_read_be16(data + i * CHECK_ENTRY_STRIDE) != OP_BRA_W) {
            return false;
        }
    }
    return nuade_read_be16(data + CHECK_LEA_AT) == OP_LEA_A0_PC
           && nuade_read_be32(data + CHECK_RTS_LEA_AT) == CHECK_RTS_LEA;
}

// The offset of the first word `opcode` among `words` words from `from`; false if there is none.
static bool find_word(const uint8_t* data, size_t size, uint32_t from, uint32_t words, uint16_t opcode, uint32_t* at) {
    for (uint32_t i = 0; i < words; i++) {
        const uint32_t candidate = from + 2 * i;
        if (!has_word(size, candidate)) {
            return false;
        }
        if (nuade_read_be16(data + candidate) == opcode) {
            *at = candidate;
            return true;
        }
    }
    return false;
}

// A sample header: the size of the sample in bytes and the divisor of its base period; the sample follows.
typedef struct SampleHeader {
    uint32_t bytes;
    uint16_t divisor;
} SampleHeader;

// The caller has checked that SAMPLE_HEADER_SIZE bytes are there.
static SampleHeader read_sample_header(const uint8_t* data, uint32_t at) {
    return (SampleHeader) { .bytes = nuade_read_be32(data + at), .divisor = nuade_read_be16(data + at + 4) };
}

// The sample headers: `count` of them from `first`, each a size, a divisor and that many bytes, then a nop.
static bool sample_headers_fit(const uint8_t* data, size_t size, uint32_t first, uint32_t count) {
    uint64_t at = first;
    for (uint32_t i = 0; i < count; i++) {
        if (at > size || size - at < SAMPLE_HEADER_SIZE) {
            return false;
        }
        const SampleHeader header = read_sample_header(data, (uint32_t)at);
        if (header.bytes > SAMPLE_SIZE_MAX) {
            return false;
        }
        at += SAMPLE_HEADER_SIZE + header.bytes;
    }
    return at <= size && size - at >= 2 && nuade_read_be16(data + at) == OP_NOP;
}

// InitPlayer. The eagleplayer reads on past the end of the module where a corrupt one has no terminator; such a module
// is refused.
static bool read_layout(const uint8_t* data, size_t size, Layout* layout) {
    uint32_t at = 0;
    if (!find_word(data, size, SAMPLE_LOOP_SCAN_AT, SAMPLE_LOOP_SCAN_WORDS, OP_MOVE_L_POSTINC_D2, &at) || at < 2) {
        return false;
    }
    layout->sample_count_at = at - 2;
    layout->sample_count = (uint32_t)data[at - 1] + 1;

    uint32_t lea_at = 0;
    if (!find_word(data, size, SAMPLE_LEA_SCAN_AT, SAMPLE_LEA_SCAN_WORDS, OP_LEA_A0_PC, &lea_at)
        || !has_word(size, lea_at + 4)) {
        return false;
    }
    const int64_t target = (int64_t)(lea_at + 2) + (int16_t)nuade_read_be16(data + lea_at + 2);
    const int64_t samples = nuade_read_be16(data + lea_at + 4) == OP_ADDA_L_IMMEDIATE_A0 ? target + SAMPLE_GAP : target;
    if (target < 0 || samples > (int64_t)size
        || !sample_headers_fit(data, size, (uint32_t)samples, layout->sample_count)) {
        return false;
    }
    layout->sample_target = (uint32_t)target;
    layout->samples = (uint32_t)samples;

    uint32_t song_lea_at = 0;
    if (!find_word(data, size, SONG_LEA_SCAN_AT, SONG_LEA_SCAN_WORDS, OP_LEA_D16_A3_A0, &song_lea_at)
        || !has_word(size, song_lea_at + 2)) {
        return false;
    }
    const int16_t song_table = (int16_t)nuade_read_be16(data + song_lea_at + 2);
    if (song_table < 0) {
        return false;
    }
    layout->song_table = (uint32_t)song_table;
    // The subsong records end at the first record that starts with a zero word
    uint64_t record = layout->song_table;
    layout->subsong_count = 0;
    do {
        record += RH_SUBSONG_RECORD_SIZE;
        layout->subsong_count++;
        if (record > size || size - record < 2) {
            return false;
        }
    } while (nuade_read_be16(data + record) != 0);
    layout->patch_from = (uint32_t)record;
    // dbra counts the low word of the distance to the first sample header, and the loop runs once more
    layout->patch_count = (uint16_t)(layout->samples - layout->patch_from) + 1u;
    return (uint64_t)layout->patch_from + layout->patch_count <= size;
}

// The eagleplayer turns every stop command byte of the sequence data (and every such byte in front of the first sample)
// into the next-track command.
static void patch_stop_commands(uint8_t* data, const Layout* layout) {
    for (uint32_t i = 0; i < layout->patch_count; i++) {
        uint8_t* byte = &data[layout->patch_from + i];
        if (*byte == RhCommand_Stop) {
            *byte = RhCommand_NextTrack;
        }
    }
}

// The builds of the replayer

#define SILENCE_SHORT_WORDS 0x20
#define SILENCE_LONG_WORDS 0x100

static const RhVariant s_variants[] = {
    {
        // The instrument command takes a default for an operand with bit 7 set; the init plays subsong 0 always.
        .name = "gmusic",
        .code_hash = 0x2184169D564C7C16ull,
        .variables_size = 0x9E,
        .pc_count = 12,
        .pc_table
        = { RhTable_None, RhTable_None, RhTable_Samples, RhTable_Instruments, RhTable_Pointers, RhTable_Specials,
            RhTable_None, RhTable_Silence, RhTable_Instruments, RhTable_Waves, RhTable_Silence, RhTable_Periods },
        .silence_words = SILENCE_SHORT_WORDS,
        .instrument_shift = 5,
        .extended_instruments = true,
        .skip_negative_loop = true,
        .default_instrument = true,
        .ignores_subsong = true,
        .stop_command = true,
    },
    {
        .name = "flymus",
        .code_hash = 0xD3B6DD2001AF5BC6ull,
        .variables_size = 0x9E,
        .pc_count = 12,
        .pc_table
        = { RhTable_None, RhTable_None, RhTable_Samples, RhTable_Instruments, RhTable_Pointers, RhTable_Specials,
            RhTable_None, RhTable_Silence, RhTable_Instruments, RhTable_Waves, RhTable_Silence, RhTable_Periods },
        .silence_words = SILENCE_SHORT_WORDS,
        .instrument_shift = 5,
        .extended_instruments = true,
        .skip_negative_loop = true,
        .default_instrument = true,
        .stop_command = true,
    },
    {
        .name = "budokan",
        .code_hash = 0xD920DE2B43448210ull,
        .variables_size = 0xA4,
        .pc_count = 11,
        .pc_table
        = { RhTable_None, RhTable_None, RhTable_Samples, RhTable_Instruments, RhTable_Pointers, RhTable_Specials,
            RhTable_Silence, RhTable_Instruments, RhTable_Waves, RhTable_Silence, RhTable_Periods },
        .silence_words = SILENCE_SHORT_WORDS,
        .instrument_shift = 5,
        .extended_instruments = true,
        .skip_negative_loop = true,
        .stop_command = true,
        .volume_command = RhCommand_VolumeB,
    },
    {
        .name = "centurion",
        .code_hash = 0x13F596F80C9F59E7ull,
        .variables_size = 0xA4,
        .pc_count = 11,
        .pc_table
        = { RhTable_None, RhTable_None, RhTable_Samples, RhTable_Instruments, RhTable_Pointers, RhTable_Specials,
            RhTable_Silence, RhTable_Instruments, RhTable_Waves, RhTable_Silence, RhTable_Periods },
        .silence_words = SILENCE_SHORT_WORDS,
        .instrument_shift = 5,
        .extended_instruments = true,
        .skip_negative_loop = true,
        .stop_command = true,
        .legato_command = true,
        .volume_command = RhCommand_VolumeA,
    },
    {
        .name = "populous",
        .code_hash = 0x983F321435BEFF20ull,
        .variables_size = 0x29C,
        .pc_count = 9,
        .pc_table = { RhTable_None, RhTable_None, RhTable_Samples, RhTable_Instruments, RhTable_Pointers,
                      RhTable_Specials, RhTable_Instruments, RhTable_Waves, RhTable_Periods },
        .silence_after_code = 0x9C,
        .silence_words = SILENCE_LONG_WORDS,
        .instrument_shift = 5,
        .extended_instruments = true,
        .skip_negative_loop = true,
    },
    {
        // The populous build with a branch for a voice without an instrument, which the eagleplayer's patch of the init
        // makes unreachable.
        .name = "sanxion",
        .code_hash = 0xF5D581B317B92791ull,
        .variables_size = 0x29C,
        .pc_count = 9,
        .pc_table = { RhTable_None, RhTable_None, RhTable_Samples, RhTable_Instruments, RhTable_Pointers,
                      RhTable_Specials, RhTable_Instruments, RhTable_Waves, RhTable_Periods },
        .silence_after_code = 0x9C,
        .silence_words = SILENCE_LONG_WORDS,
        .instrument_shift = 5,
        .extended_instruments = true,
        .skip_negative_loop = true,
    },
    {
        .name = "sanxion-demo",
        .code_hash = 0x26812A8ADA7A41C8ull,
        .variables_size = 0x274,
        .pc_count = 7,
        .pc_table = { RhTable_None, RhTable_None, RhTable_Samples, RhTable_Instruments, RhTable_None,
                      RhTable_Instruments, RhTable_Periods },
        .silence_after_code = 0x74,
        .silence_words = SILENCE_LONG_WORDS,
        .instrument_shift = 4,
    },
};

static const RhVariant* find_variant(const RhCodeScan* scan) {
    for (size_t i = 0; i < nuade_sizeof_array(s_variants); i++) {
        if (s_variants[i].code_hash == scan->hash && s_variants[i].pc_count == scan->pc_count) {
            return &s_variants[i];
        }
    }
    return nullptr;
}

// Takes the table offsets from the pc-relative addresses of the code. A table that several of them name must be the
// same everywhere.
static bool resolve_tables(RhModule* m, const Layout* layout, const RhCodeScan* scan) {
    const RhVariant* variant = m->variant;
    uint32_t table[RhTable_Count] = { 0 };
    bool known[RhTable_Count] = { false };
    for (int i = 0; i < scan->pc_count; i++) {
        const RhTable role = variant->pc_table[i];
        if (role == RhTable_None) {
            continue;
        }
        if (known[role] && table[role] != scan->pc_targets[i]) {
            return false;
        }
        known[role] = true;
        table[role] = scan->pc_targets[i];
    }
    const bool silence_by_table = variant->silence_after_code == 0;
    if (!known[RhTable_Samples] || !known[RhTable_Instruments] || !known[RhTable_Periods]
        || known[RhTable_Silence] != silence_by_table || known[RhTable_Waves] != variant->extended_instruments
        || known[RhTable_Pointers] != known[RhTable_Specials] || table[RhTable_Samples] != layout->sample_target) {
        return false;
    }
    m->instruments = table[RhTable_Instruments];
    m->periods = table[RhTable_Periods];
    m->waves = table[RhTable_Waves];
    m->has_specials = known[RhTable_Specials];
    m->pointers = table[RhTable_Pointers];
    m->specials = table[RhTable_Specials];
    m->silence = silence_by_table ? table[RhTable_Silence] : scan->end + variant->silence_after_code;
    // The replayer's code and variables lie in front of the lowest table; the eagleplayer has patched the code and the
    // replayer writes its variables there
    m->tables_start = m->song_table < m->periods ? m->song_table : m->periods;
    const uint32_t others[] = { m->instruments, m->has_specials ? m->pointers : UINT32_MAX,
                                variant->extended_instruments ? m->waves : UINT32_MAX };
    for (size_t i = 0; i < nuade_sizeof_array(others); i++) {
        m->tables_start = others[i] < m->tables_start ? others[i] : m->tables_start;
    }
    // The variables take a fixed space behind the code (the hash pins their offsets); a table in them or in the code
    // is not one the replayer's own tables could be
    if (m->tables_start < scan->end + variant->variables_size) {
        return false;
    }
    const uint64_t record = 1u << variant->instrument_shift;
    // The records the init fills in: one for each sample, and three at the specials
    if ((uint64_t)m->instruments + (uint64_t)m->sample_count * record > m->size || m->periods >= m->size
        || m->silence >= m->size || (known[RhTable_Waves] && m->waves >= m->size)) {
        return false;
    }
    if (m->has_specials) {
        if (m->specials < m->instruments || (m->specials - m->instruments) % record != 0
            || (uint64_t)m->specials + 3 * record > m->size || m->pointers > m->size || m->size - m->pointers < 12) {
            return false;
        }
    }
    return true;
}

uint8_t rh_module_u8(const RhModule* module, uint32_t offset) {
    return offset < module->size ? module->data[offset] : 0;
}

uint16_t rh_module_u16(const RhModule* module, uint32_t offset) {
    return (uint16_t)((rh_module_u8(module, offset) << 8) | rh_module_u8(module, offset + 1));
}

uint32_t rh_module_u32(const RhModule* module, uint32_t offset) {
    return ((uint32_t)rh_module_u16(module, offset) << 16) | rh_module_u16(module, offset + 2);
}

void rh_module_store_u8(RhModule* module, uint32_t offset, uint8_t value) {
    if (offset < module->size) {
        module->data[offset] = value;
    }
}

static void store_u16(RhModule* module, uint32_t offset, uint16_t value) {
    rh_module_store_u8(module, offset, (uint8_t)(value >> 8));
    rh_module_store_u8(module, offset + 1, (uint8_t)value);
}

static void store_u32(RhModule* module, uint32_t offset, uint32_t value) {
    store_u16(module, offset, (uint16_t)(value >> 16));
    store_u16(module, offset + 2, (uint16_t)value);
}

uint32_t rh_module_instrument(const RhModule* module, uint32_t index) {
    return module->instruments + (index << module->variant->instrument_shift);
}

uint32_t rh_module_played_subsong(const RhModule* module, uint32_t subsong) {
    return module->variant->ignores_subsong ? 0 : subsong & 0xFF;
}

RhSubsong rh_module_read_subsong(const RhModule* module, uint32_t subsong) {
    const uint32_t record = module->song_table + subsong * RH_SUBSONG_RECORD_SIZE;
    RhSubsong result = { .tempo = rh_module_u8(module, record + SUBSONG_TEMPO_AT) };
    for (uint32_t voice = 0; voice < RH_VOICES; voice++) {
        result.track_list[voice] = rh_module_u32(module, record + SUBSONG_TRACK_LISTS_AT + 4 * voice);
    }
    return result;
}

uint32_t rh_module_track_entries(const RhModule* module, uint32_t list) {
    uint32_t count = 0;
    while (list + 4u * count < module->size && rh_module_u32(module, list + 4u * count) != 0) {
        count++;
    }
    return count;
}

uint32_t rh_module_resolve_instrument(const RhModule* module, uint32_t subsong, uint8_t operand) {
    if (module->variant->default_instrument && (operand & 0x80) != 0) {
        return (uint8_t)rh_module_track_entries(module, rh_module_read_subsong(module, subsong).track_list[3]);
    }
    return operand;
}

// Base period of a sample: 0x369E99 / divisor. A quotient that does not fit divu's 16 bits leaves the register as it
// was, and the replayer stores its low word.
static uint16_t base_period(uint16_t divisor) {
    const uint32_t quotient = 0x369E99u / divisor;
    return quotient > 0xFFFF ? 0x9E99 : (uint16_t)quotient;
}

// The fields of an instrument record (the extended ones only in the builds with extended_instruments).
typedef enum InstrumentField {
    InstrumentField_Sample = 0x0,
    InstrumentField_Repeat = 0x4,
    InstrumentField_Words = 0x8,
    InstrumentField_BasePeriod = 0xC,
    InstrumentField_Volume = 0xE,
    InstrumentField_VibratoDivisor = 0x10,
    InstrumentField_Wave = 0x12,
    InstrumentField_FerrisMax = 0x14,
    InstrumentField_FerrisMin = 0x16,
} InstrumentField;

RhInstrument rh_module_read_instrument(const RhModule* module, uint32_t record) {
    RhInstrument ins = {
        .sample = rh_module_u32(module, record + InstrumentField_Sample),
        .repeat = (int32_t)rh_module_u32(module, record + InstrumentField_Repeat),
        .words = rh_module_u16(module, record + InstrumentField_Words),
        .base_period = rh_module_u16(module, record + InstrumentField_BasePeriod),
        .volume = rh_module_u16(module, record + InstrumentField_Volume),
    };
    if (module->variant->extended_instruments) {
        ins.vibrato_divisor = rh_module_u16(module, record + InstrumentField_VibratoDivisor);
        ins.wave = (int16_t)rh_module_u16(module, record + InstrumentField_Wave);
        ins.ferris_max = rh_module_u16(module, record + InstrumentField_FerrisMax);
        ins.ferris_min = rh_module_u16(module, record + InstrumentField_FerrisMin);
    }
    return ins;
}

void rh_module_reset(RhModule* module) {
    memcpy(module->data, module->image, module->size);
    // The init walks the sample headers and fills in the sample, length and base period of one instrument record for
    // each
    uint32_t at = module->samples;
    for (uint32_t i = 0; i < module->sample_count; i++) {
        const SampleHeader header = read_sample_header(module->image, at);
        const uint32_t record = rh_module_instrument(module, i);
        store_u32(module, record + InstrumentField_Sample, at + SAMPLE_HEADER_SIZE);
        store_u16(module, record + InstrumentField_Words, (uint16_t)(header.bytes >> 1));
        store_u16(module, record + InstrumentField_BasePeriod, base_period(header.divisor));
        at += SAMPLE_HEADER_SIZE + header.bytes;
    }
    if (module->has_specials) {
        for (uint32_t i = 0; i < 3; i++) {
            store_u32(module, module->specials + (i << module->variant->instrument_shift) + InstrumentField_Sample,
                      rh_module_u32(module, module->pointers + 4 * i));
        }
    }
}

// What the port does not model

// The replayer divides by every sample's divisor, and traps on a zero.
static bool divisors_are_valid(const RhModule* m) {
    uint32_t at = m->samples;
    for (uint32_t i = 0; i < m->sample_count; i++) {
        const SampleHeader header = read_sample_header(m->image, at);
        if (header.divisor == 0) {
            return false;
        }
        at += SAMPLE_HEADER_SIZE + header.bytes;
    }
    return true;
}

// Whether [start, end) is data of the module that the replayer and the port see alike: inside the module and behind the
// lowest table, or inside the silence block. In front of the tables lie the replayer's code, which the eagleplayer has
// patched, and its variables, which the port keeps elsewhere.
static bool is_module_data(const RhModule* m, uint64_t start, uint64_t end) {
    if (start > end || end > m->size) {
        return false;
    }
    const uint64_t silence_end = (uint64_t)m->silence + 2ull * m->variant->silence_words;
    return start >= m->tables_start || (start >= m->silence && end <= silence_end);
}

// An instrument the song selects must be one the init has filled in, and everything the replayer does with it must stay
// in the module's data: the sample and its repeat, the vibrato wave and the bytes the ferris wheel writes.
static bool instrument_is_modelled(const RhModule* m, uint32_t index) {
    const RhVariant* variant = m->variant;
    const uint32_t record = rh_module_instrument(m, index);
    if (!is_module_data(m, record, (uint64_t)record + (1u << variant->instrument_shift))) {
        return false;
    }
    bool filled = index < m->sample_count;
    if (m->has_specials) {
        const uint32_t first = (m->specials - m->instruments) >> variant->instrument_shift;
        filled = filled || (index >= first && index < first + 3);
    }
    const RhInstrument ins = rh_module_read_instrument(m, record);
    if (!filled || ins.sample == 0 || !is_module_data(m, ins.sample, (uint64_t)ins.sample + 2ull * ins.words)) {
        return false;
    }
    const bool repeat_skipped = ins.repeat < 0 && variant->skip_negative_loop;
    // A repeat can lie past the sample (the voice then plays what follows it)
    const uint32_t repeat_at = ins.sample + (uint32_t)ins.repeat;
    if (ins.repeat != 0 && !repeat_skipped && !is_module_data(m, repeat_at, (uint64_t)repeat_at + 1)) {
        return false;
    }
    if (!variant->extended_instruments) {
        return true;
    }
    if (ins.vibrato_divisor != 0) {
        // The wave ends in a restart byte
        const uint32_t wave = m->waves + (uint32_t)(int32_t)ins.wave;
        uint32_t end = wave;
        while (end < m->size && m->data[end] != RH_VIBRATO_RESTART) {
            end++;
        }
        if (!is_module_data(m, wave, (uint64_t)end + 1)) {
            return false;
        }
    }
    if (ins.ferris_max != 0) {
        const uint32_t highest = ins.ferris_max > ins.ferris_min ? ins.ferris_max : ins.ferris_min;
        // The write position is a signed 16-bit index
        return ins.ferris_min >= 1 && highest + 1 < 0x8000
               && is_module_data(m, ins.sample, (uint64_t)ins.sample + highest + 2);
    }
    return true;
}

// Bytes of sequence data the check reads in all, for the whole module.
#define WALK_BUDGET 0x400000

typedef struct Walk {
    const RhModule* module;
    uint64_t budget;
    bool referenced[256];
} Walk;

// What the replayer has done on a voice so far.
typedef struct WalkVoice {
    uint32_t subsong;  // asked for: where a default instrument comes from
    bool selected;     // the voice has an instrument
    bool stopped;      // a stream stopped the replayer
    bool progressed;   // a note, a rest or the stop command ended a run of commands
    uint32_t commands; // read since the last note or rest
} WalkVoice;

// One stream, to its next-track command. A note or a rest before the first instrument would read an instrument the
// replayer has not got; a run of commands that the port's guard would cut short is refused.
static bool walk_stream(Walk* w, uint32_t start, WalkVoice* voice) {
    const RhModule* m = w->module;
    const RhVariant* variant = m->variant;
    uint32_t at = start;
    for (;;) {
        if (!is_module_data(m, at, (uint64_t)at + 1) || w->budget == 0) {
            return false;
        }
        w->budget--;
        const uint8_t byte = m->data[at++];
        if (byte < RH_COMMAND_FIRST) {
            at++;
            voice->commands = 0;
            voice->progressed = true;
            if (!voice->selected) {
                return false;
            }
            continue;
        }
        if (++voice->commands >= RH_COMMANDS_PER_TICK) {
            return false;
        }
        switch (byte) {
            case RhCommand_Instrument:
                if (!is_module_data(m, at, (uint64_t)at + 1)) {
                    return false;
                }
                w->referenced[rh_module_resolve_instrument(m, voice->subsong, m->data[at++])] = true;
                voice->selected = true;
                continue;
            case RhCommand_Slide:
                at++;
                continue;
            case RhCommand_Rest:
                at++;
                voice->commands = 0;
                voice->progressed = true;
                if (!voice->selected) {
                    return false;
                }
                continue;
            case RhCommand_NextTrack:
                return true;
            case RhCommand_Stop:
                if (variant->stop_command) {
                    voice->stopped = true;
                    voice->progressed = true;
                    return true;
                }
                continue;
            default:
                if (byte == variant->volume_command) {
                    at++;
                }
                continue;
        }
    }
}

// The streams of one voice of a subsong, in the order the replayer plays them. The track list ends in a 0 and some
// stream of it reaches a note, a rest or the stop command (else the replayer reads commands for ever).
static bool walk_voice(Walk* w, uint32_t subsong, uint32_t list) {
    const RhModule* m = w->module;
    WalkVoice voice = { .subsong = subsong };
    for (uint32_t i = 0; !voice.stopped; i++) {
        const uint64_t entry_at = (uint64_t)list + 4ull * i;
        if (!is_module_data(m, (uint32_t)entry_at, entry_at + 4)) {
            return false;
        }
        const uint32_t stream = rh_module_u32(m, (uint32_t)entry_at);
        if (stream == 0) {
            // The list starts over: the commands before the first note of its first stream are read in the same run as
            // the ones after the last note
            return i != 0 && voice.progressed && walk_stream(w, rh_module_u32(m, list), &voice);
        }
        if (!walk_stream(w, stream, &voice)) {
            return false;
        }
    }
    return true;
}

// The eagleplayer's InitSound counts the entries of every track list of the subsongs up to the one it was asked for,
// and reads on past the module where a list has no 0.
static bool track_lists_end(const RhModule* m) {
    for (uint32_t subsong = 0; subsong < m->subsong_count; subsong++) {
        const RhSubsong record = rh_module_read_subsong(m, subsong);
        for (int voice = 0; voice < RH_VOICES; voice++) {
            const uint64_t end
                = (uint64_t)record.track_list[voice] + 4ull * rh_module_track_entries(m, record.track_list[voice]);
            if (end + 4 > m->size) {
                return false;
            }
        }
    }
    return true;
}

// Walks every stream of every subsong once, in the order the replayer reaches them, and checks the instruments they
// select. The replayer reads memory the module does not hold (low memory, an instrument that was never selected) in
// the cases this refuses, and UADE's memory there is not the module's.
static bool streams_are_modelled(const RhModule* m) {
    Walk* w = (Walk*)calloc(1, sizeof(Walk));
    if (w == nullptr) {
        return false;
    }
    w->module = m;
    w->budget = WALK_BUDGET;
    bool modelled = !m->variant->default_instrument || track_lists_end(m);
    for (uint32_t subsong = 0; modelled && subsong < m->subsong_count; subsong++) {
        const RhSubsong played = rh_module_read_subsong(m, rh_module_played_subsong(m, subsong));
        for (int voice = 0; modelled && voice < RH_VOICES; voice++) {
            modelled = walk_voice(w, subsong, played.track_list[voice]);
        }
    }
    for (uint32_t index = 0; modelled && index < 256; index++) {
        modelled = !w->referenced[index] || instrument_is_modelled(m, index);
    }
    free(w);
    return modelled;
}

// The port starts the voices from nothing; the replayer starts from the variables in the file, which hold nothing in
// the modules there are. Refuse a module where they do (they are in front of the lowest table, but for the silence).
static bool variables_are_zero(const RhModule* m, uint32_t code_end) {
    const bool silence_inside = m->silence >= code_end && m->silence < m->tables_start;
    const uint32_t end = silence_inside ? m->silence : m->tables_start;
    for (uint32_t at = code_end; at < end; at++) {
        if (m->image[at] != 0) {
            return false;
        }
    }
    return true;
}

void rh_module_close(RhModule* module) {
    if (module != nullptr) {
        free(module->data);
        free(module->image);
    }
    free(module);
}

RhModule* rh_module_open(const uint8_t* data, size_t size) {
    Layout layout;
    if (size > INT32_MAX || !check_header(data, size) || !read_layout(data, size, &layout)) {
        return nullptr;
    }
    RhModule* m = (RhModule*)calloc(1, sizeof(RhModule));
    if (m == nullptr) {
        return nullptr;
    }
    m->size = size;
    m->image = nuade_paula_padded_copy(data, size);
    m->data = nuade_paula_padded_copy(data, size);
    if (m->image == nullptr || m->data == nullptr) {
        rh_module_close(m);
        return nullptr;
    }
    patch_stop_commands(m->image, &layout);
    RhCodeScan scan;
    m->sample_count = layout.sample_count;
    m->samples = layout.samples;
    m->song_table = layout.song_table;
    m->subsong_count = layout.subsong_count;
    // The init walks the sample headers of the patched module, whose lengths the patch may have changed
    if (!rh_scan_code(m->image, size, layout.song_table, layout.sample_count_at, &scan)
        || (m->variant = find_variant(&scan)) == nullptr || !resolve_tables(m, &layout, &scan)
        || !sample_headers_fit(m->image, size, m->samples, m->sample_count) || !divisors_are_valid(m)
        || !variables_are_zero(m, scan.end)) {
        rh_module_close(m);
        return nullptr;
    }
    rh_module_reset(m);
    if (!streams_are_modelled(m)) {
        rh_module_close(m);
        return nullptr;
    }
    return m;
}
