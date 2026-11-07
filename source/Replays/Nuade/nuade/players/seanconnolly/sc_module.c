// SPDX-License-Identifier: MIT
// Sean Connolly module: the known replayer versions, the locator that recognises them and the readers of the image.
//
// The module is recognised by its code. The code of a version, from the init entry (the sequencer builds: from the DMA
// bit table in front of it) to the start of the period table, with the displacement words of its table-addressing
// `lea d16(pc),An` zeroed, must hash to the version's hash. The table addresses are read from those words. A module
// that matches none is refused (UADE plays it).

#include "sc_internal.h"

#include "nuade_base.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define SCO_BRA_W 0x6000u
#define SCO_ENTRY_TABLE_WORDS 3 // the bra.w words at 0, 4 and 8 that the eagleplayer's Check2 tests
#define SCO_FNV_OFFSET 0xCBF29CE484222325ULL
#define SCO_FNV_PRIME 0x100000001B3ULL

#define SCO_ROLE(role) (1u << (role))

// The sample build's variables that the port reads from the file, from the init entry (in Surf Ninjas: 0x28A, 0x26A,
// 0x14E) and the data the eagleplayer's InitPlayer looks at.
#define SCO_V2_DMA_MASKS_AT (-0x122)
#define SCO_V2_ZERO_AT (-0x142)
#define SCO_V2_ZERO_BYTES 16
#define SCO_V2_FLAGS_AT (-0x25E)
#define SCO_V2_TEXT_START 0x10
#define SCO_V2_SIZE_AT_ARP (-4) // the longs in front of the arpeggio table: the sample data's end and start
#define SCO_V2_SIZE_START_AT_ARP (-8)
#define SCO_V2_SINGLE_SIZES_A 0xBF2u // the eagleplayer reports one subsong for a module with these sample data sizes
#define SCO_V2_SINGLE_SIZES_B 0x12u
#define SCO_FORM 0x464F524Du

// The known versions. The slots are the displacement words of the `lea d16(pc),An` that address a table, from the init
// entry; the hashes were taken from the corpus files.

static const ScoSlot sco_reaxion_slots[] = {
    { ScoRole_Songs, 0x3C },          { ScoRole_Streams, 0x44 },       { ScoRole_PatternData, 0xAC },
    { ScoRole_Sfx, 0x13E },           { ScoRole_SeqTable, 0x34E },     { ScoRole_SeqBase, 0x356 },
    { ScoRole_Period, 0x3A2 },        { ScoRole_Period, 0x3C0 },       { ScoRole_Period, 0x446 },
    { ScoRole_Wave8, 0x4F0 },         { ScoRole_Wave16, 0x502 },       { ScoRole_Wave16, 0x50C },
    { ScoRole_SampleTable, 0x57A },   { ScoRole_SampleBase, 0x586 },   { ScoRole_Instruments, 0x5DC },
    { ScoRole_InstWaveTable, 0x5F2 }, { ScoRole_InstWaveBase, 0x5FA }, { ScoRole_Wave8, 0x60C },
    { ScoRole_Wave16, 0x61C },        { ScoRole_Wave16, 0x626 },       { ScoRole_Period, 0x648 },
    { ScoRole_Period, 0x7DE },        { ScoRole_SampleBase, 0x800 },   { ScoRole_SampleTable, 0x804 },
    { ScoRole_Period, 0x8B4 },        { ScoRole_Instruments, 0x8CE },  { ScoRole_PatternTable, 0xA52 },
    { ScoRole_Period, 0xA9C },        { ScoRole_Period, 0xC62 },       { ScoRole_Instruments, 0xC82 },
    { ScoRole_Wave8, 0xD70 },         { ScoRole_Wave16, 0xD86 },       { ScoRole_Wave16, 0xD94 },
    { ScoRole_SampleTable, 0xE0A },   { ScoRole_SampleBase, 0xE1A },   { ScoRole_SampleTable, 0xE7E },
    { ScoRole_SampleBase, 0xE82 },
};

static const ScoSlot sco_soccer_slots[] = {
    { ScoRole_Songs, 0x3C },          { ScoRole_Streams, 0x44 },       { ScoRole_PatternData, 0xAC },
    { ScoRole_Sfx, 0x11A },           { ScoRole_SeqTable, 0x32A },     { ScoRole_SeqBase, 0x332 },
    { ScoRole_Period, 0x37E },        { ScoRole_Period, 0x39C },       { ScoRole_Period, 0x422 },
    { ScoRole_Wave8, 0x4CC },         { ScoRole_Wave16, 0x4DE },       { ScoRole_Wave16, 0x4E8 },
    { ScoRole_SampleTable, 0x556 },   { ScoRole_SampleBase, 0x562 },   { ScoRole_Instruments, 0x5B8 },
    { ScoRole_InstWaveTable, 0x5CE }, { ScoRole_InstWaveBase, 0x5D6 }, { ScoRole_Wave8, 0x5E8 },
    { ScoRole_Wave16, 0x5F8 },        { ScoRole_Wave16, 0x602 },       { ScoRole_Period, 0x624 },
    { ScoRole_Period, 0x7BA },        { ScoRole_SampleBase, 0x7DC },   { ScoRole_SampleTable, 0x7E0 },
    { ScoRole_Period, 0x890 },        { ScoRole_Instruments, 0x8AA },  { ScoRole_PatternTable, 0xA2E },
    { ScoRole_Period, 0xA78 },        { ScoRole_Period, 0xC3E },       { ScoRole_Instruments, 0xC5E },
    { ScoRole_Wave8, 0xD4C },         { ScoRole_Wave16, 0xD62 },       { ScoRole_Wave16, 0xD70 },
    { ScoRole_SampleTable, 0xDE6 },   { ScoRole_SampleBase, 0xDF6 },   { ScoRole_SampleTable, 0xE5A },
    { ScoRole_SampleBase, 0xE5E },
};

static const ScoSlot sco_surf_slots[] = {
    { ScoRole_Songs, 0x8 },           { ScoRole_Songs, 0xC0 },        { ScoRole_Instruments, 0x386 },
    { ScoRole_Instruments, 0x428 },   { ScoRole_Wave8, 0x54E },       { ScoRole_Wave8, 0x58C },
    { ScoRole_SampleBase, 0x5C0 },    { ScoRole_SampleTable, 0x5C8 }, { ScoRole_Period, 0x63E },
    { ScoRole_Period, 0x660 },        { ScoRole_SampleTable, 0x6B8 }, { ScoRole_SampleBase, 0x6C4 },
    { ScoRole_WaveSequences, 0x774 }, { ScoRole_Wave8, 0x7B8 },       { ScoRole_Period, 0x7E6 },
    { ScoRole_SeqTable, 0x82E },      { ScoRole_SeqBase, 0x836 },     { ScoRole_Period, 0x880 },
    { ScoRole_Period, 0x8B2 },        { ScoRole_Period, 0x914 },      { ScoRole_Streams, 0x95E },
    { ScoRole_PatternTable, 0xA98 },  { ScoRole_PatternData, 0xAA0 }, { ScoRole_Period, 0xAD8 },
    { ScoRole_Period, 0xC6A },        { ScoRole_Instruments, 0xCA2 }, { ScoRole_Wave8, 0xD40 },
    { ScoRole_SampleBase, 0xD66 },    { ScoRole_SampleTable, 0xD6E }, { ScoRole_WaveSequences, 0xDE2 },
    { ScoRole_Wave8, 0xE0A },         { ScoRole_Period, 0xE1E },      { ScoRole_SampleTable, 0xED0 },
    { ScoRole_SampleBase, 0xED8 },
};

#define SCO_SEQUENCER_ROLES                                                                                          \
    (SCO_ROLE(ScoRole_Songs) | SCO_ROLE(ScoRole_Streams) | SCO_ROLE(ScoRole_PatternData)                             \
     | SCO_ROLE(ScoRole_PatternTable) | SCO_ROLE(ScoRole_Instruments) | SCO_ROLE(ScoRole_InstWaveTable)              \
     | SCO_ROLE(ScoRole_InstWaveBase) | SCO_ROLE(ScoRole_SeqTable) | SCO_ROLE(ScoRole_SeqBase)                       \
     | SCO_ROLE(ScoRole_Period) | SCO_ROLE(ScoRole_Wave8) | SCO_ROLE(ScoRole_Wave16) | SCO_ROLE(ScoRole_SampleTable) \
     | SCO_ROLE(ScoRole_SampleBase))

#define SCO_SAMPLER_ROLES                                                                          \
    (SCO_ROLE(ScoRole_Songs) | SCO_ROLE(ScoRole_Streams) | SCO_ROLE(ScoRole_PatternData)           \
     | SCO_ROLE(ScoRole_PatternTable) | SCO_ROLE(ScoRole_Instruments) | SCO_ROLE(ScoRole_SeqTable) \
     | SCO_ROLE(ScoRole_SeqBase) | SCO_ROLE(ScoRole_Period) | SCO_ROLE(ScoRole_Wave8)              \
     | SCO_ROLE(ScoRole_WaveSequences) | SCO_ROLE(ScoRole_SampleTable) | SCO_ROLE(ScoRole_SampleBase))

static const ScoVersion sco_versions[] = {
    {
        .name = "Reaxion",
        .family = ScoFamily_Sequencer,
        .prefix_bytes = 8,
        .code_len = 0xF64,
        .tick_bra = 4,
        .tick_at = 0x174,
        .hash = 0x7A451E9F52F654A9ULL,
        .slots = sco_reaxion_slots,
        .slot_count = (uint32_t)nuade_sizeof_array(sco_reaxion_slots),
        .required_roles = SCO_SEQUENCER_ROLES,
        .var_at = -0x314,
        .noise_at = -0xFE,
        .seed_at = -0xFE + 0xF0,
        .noise_words = 0x78,
        .noise_alternates = false,
    },
    {
        .name = "Soccer Superstars",
        .family = ScoFamily_Sequencer,
        .prefix_bytes = 8,
        .code_len = 0xF4C,
        .tick_bra = 4,
        .tick_at = 0x150,
        .hash = 0x4136BE56C68039A7ULL,
        .slots = sco_soccer_slots,
        .slot_count = (uint32_t)nuade_sizeof_array(sco_soccer_slots),
        .required_roles = SCO_SEQUENCER_ROLES,
        .var_at = -0x2A4,
        .noise_at = -0x8E,
        .seed_at = -0x8E + 0x80,
        .noise_words = 0x40,
        .noise_alternates = true,
        .pitch_command_repeats = true,
    },
    {
        .name = "Surf Ninjas",
        .family = ScoFamily_Sampler,
        .prefix_bytes = 0,
        .code_len = 0xEF6,
        .tick_bra = 8,
        .tick_at = 0x1BA,
        .hash = 0xBD5EB2EA8C75B4B2ULL,
        .slots = sco_surf_slots,
        .slot_count = (uint32_t)nuade_sizeof_array(sco_surf_slots),
        .required_roles = SCO_SAMPLER_ROLES,
        .var_at = SCO_V2_FLAGS_AT,
        .noise_at = -0x90,
        .seed_at = -0x9A,
        .noise_words = 0x40,
        .noise_alternates = false, // the sample build always fills the buffers alternately: see sco_v2.c
    },
};

static bool exempt_variable(const ScoModule* module, uint32_t at) {
    const ScoVersion* version = module->version;
    const uint32_t noise = sco_noise_address(module);
    const uint32_t seed = sco_seed_address(module);
    return at - noise < (uint32_t)version->noise_words * 2 || at - seed < 6;
}

// A table, stream or pattern read. The replayer's variables are live state in the original, kept in structs here: a
// read that lands in them (the noise buffer and seed apart, which the image does hold) is flagged.

uint8_t sco_u8(const ScoModule* module, uint32_t at) {
    if (at - module->var_start < module->entry - module->var_start && !exempt_variable(module, at)) {
        *module->var_read = true;
    }
    return at < module->size ? module->image[at] : 0;
}

static uint8_t peek8(const ScoModule* module, uint32_t at) {
    return at < module->size ? module->image[at] : 0;
}

uint16_t sco_peek16(const ScoModule* module, uint32_t at) {
    return (uint16_t)((peek8(module, at) << 8) | peek8(module, at + 1));
}

uint32_t sco_peek32(const ScoModule* module, uint32_t at) {
    return ((uint32_t)sco_peek16(module, at) << 16) | sco_peek16(module, at + 2);
}

bool sco_module_var_read(const ScoModule* module) {
    return *module->var_read;
}

uint16_t sco_u16(const ScoModule* module, uint32_t at) {
    return (uint16_t)((sco_u8(module, at) << 8) | sco_u8(module, at + 1));
}

uint32_t sco_u32(const ScoModule* module, uint32_t at) {
    return ((uint32_t)sco_u16(module, at) << 16) | sco_u16(module, at + 2);
}

static uint32_t indexed_address(uint32_t base, uint16_t index) {
    return base + (uint32_t)(int32_t)(int16_t)index;
}

uint8_t sco_u8_indexed(const ScoModule* module, uint32_t base, uint16_t index) {
    return sco_u8(module, indexed_address(base, index));
}

uint16_t sco_u16_indexed(const ScoModule* module, uint32_t base, uint16_t index) {
    return sco_u16(module, indexed_address(base, index));
}

uint32_t sco_u32_indexed(const ScoModule* module, uint32_t base, uint16_t index) {
    return sco_u32(module, indexed_address(base, index));
}

void sco_write8(ScoModule* module, uint32_t at, uint8_t value) {
    if (at < module->size) {
        module->image[at] = value;
    }
}

void sco_write16(ScoModule* module, uint32_t at, uint16_t value) {
    sco_write8(module, at, (uint8_t)(value >> 8));
    sco_write8(module, at + 1, (uint8_t)value);
}

const int8_t* sco_chip(const ScoModule* module, uint32_t at) {
    const uint32_t clamped = at < module->size ? at : module->size;
    return (const int8_t*)module->image + (clamped & ~1u);
}

ScoSampleBounds sco_sample_bounds(const ScoModule* module, uint32_t entry_offset) {
    const uint32_t table = module->base[ScoRole_SampleTable];
    const uint32_t base = module->base[ScoRole_SampleBase];
    return (ScoSampleBounds) { .start = base + sco_u32(module, table + entry_offset),
                               .end = base + sco_u32(module, table + entry_offset + 4) };
}

void sco_registers_write(const ScoModule* module, const ScoRegisters* registers, NuadePaulaChannel* paula) {
    paula->lc = sco_chip(module, registers->lc);
    paula->len_words = registers->len_words;
    paula->period = registers->period;
    paula->volume = (uint8_t)(registers->volume & 0x7F);
}

void sco_module_reset(ScoModule* module) {
    memcpy(module->image + module->var_start, module->original + module->var_start, module->entry - module->var_start);
    *module->var_read = false;
}

static uint64_t fnv_byte(uint64_t hash, uint8_t byte) {
    return (hash ^ byte) * SCO_FNV_PRIME;
}

// The signature of a version: the code with its slot words zeroed.

static uint64_t code_hash(const uint8_t* data, uint32_t entry, const ScoVersion* version) {
    const uint32_t start = entry - version->prefix_bytes;
    const uint32_t length = version->prefix_bytes + version->code_len;
    uint64_t hash = SCO_FNV_OFFSET;
    for (uint32_t i = 0; i < length; i++) {
        bool masked = false;
        for (uint32_t s = 0; s < version->slot_count; s++) {
            const uint32_t slot = version->prefix_bytes + version->slots[s].at;
            masked = masked || i == slot || i == slot + 1;
        }
        hash = fnv_byte(hash, masked ? 0 : data[start + i]);
    }
    return hash;
}

// The module's own address of each table: the target of its slots. Every slot of a role must name the same address, in
// the data behind the code.

static bool bind_roles(ScoModule* module, const uint8_t* data) {
    const ScoVersion* version = module->version;
    uint32_t bound = 0;
    const uint32_t data_start = module->entry + version->code_len;
    for (uint32_t i = 0; i < version->slot_count; i++) {
        const ScoSlot* slot = &version->slots[i];
        const uint32_t word = module->entry + slot->at;
        const int64_t target = (int64_t)word + (int16_t)nuade_read_be16(data + word);
        if (target < (int64_t)data_start || target > (int64_t)module->size) {
            return false;
        }
        if ((bound & SCO_ROLE(slot->role)) != 0 && module->base[slot->role] != (uint32_t)target) {
            return false;
        }
        bound |= SCO_ROLE(slot->role);
        module->base[slot->role] = (uint32_t)target;
    }
    return (bound & version->required_roles) == version->required_roles;
}

// The eagleplayer's end-of-song flag of the sample build: the long after the first zero long of the module's text (from
// offset 0x10 in steps of 2). SongEnd is called when that long is 0, so it must be the voices' flags the replayer
// clears.

static bool sampler_flags_found(const uint8_t* data, uint32_t size, uint32_t entry) {
    uint32_t at = SCO_V2_TEXT_START;
    while (at <= size - 4 && nuade_read_be32(data + at) != 0) {
        at += 2;
    }
    return at <= size - 4 && at + 4 == (uint32_t)((int32_t)entry + SCO_V2_FLAGS_AT);
}

// What the sample build's replayer reads from its variables before it writes them: the DMA bit table and the four longs
// loaded into registers at the start of the voice routine.

static bool sampler_variables_ok(const uint8_t* data, uint32_t size, uint32_t entry) {
    const uint32_t masks = (uint32_t)((int32_t)entry + SCO_V2_DMA_MASKS_AT);
    const uint32_t zeros = (uint32_t)((int32_t)entry + SCO_V2_ZERO_AT);
    if (entry < (uint32_t)-SCO_V2_FLAGS_AT) {
        return false;
    }
    for (uint32_t i = 0; i < 4; i++) {
        if (nuade_read_be16(data + masks + i * 2) != (1u << i)) {
            return false;
        }
    }
    for (uint32_t i = 0; i < SCO_V2_ZERO_BYTES; i++) {
        if (data[zeros + i] != 0) {
            return false;
        }
    }
    return sampler_flags_found(data, size, entry);
}

// The subsong count the eagleplayer's SubSongRange reports: the song table ends where the streams start. For the sample
// build it is 1 when the size of the sample data that InitPlayer works out has one of two values.

static bool sampler_single_subsong(const ScoModule* module) {
    const uint32_t arp = module->base[ScoRole_SeqTable];
    const uint32_t base = module->base[ScoRole_SampleBase];
    uint32_t end_offset = sco_u32(module, arp + (uint32_t)SCO_V2_SIZE_AT_ARP);
    const uint32_t start_offset = sco_u32(module, arp + (uint32_t)SCO_V2_SIZE_START_AT_ARP);
    const uint32_t at = base + end_offset;
    uint32_t size = 0;
    if (sco_u32(module, at) == SCO_FORM) {
        size = end_offset + sco_u32(module, at + 4) + 8;
    } else {
        uint32_t extra = end_offset - start_offset;
        if ((int32_t)extra > 10) {
            extra = 4;
        }
        size = end_offset + extra;
    }
    return size == SCO_V2_SINGLE_SIZES_A || size == SCO_V2_SINGLE_SIZES_B;
}

static bool find_subsongs(ScoModule* module) {
    const uint32_t songs = module->base[ScoRole_Songs];
    const uint32_t streams = module->base[ScoRole_Streams];
    if (streams < songs) {
        return false;
    }
    module->subsong_count = (streams - songs) / SCO_SONG_BYTES;
    if (module->version->family == ScoFamily_Sampler && sampler_single_subsong(module)) {
        module->subsong_count = 1;
    }
    return module->subsong_count >= 1 && module->subsong_count <= SCO_MAX_SUBSONGS;
}

// The version whose code the module has. The module's header must lead to the init routine (the first bra.w) and to the
// version's interrupt routine.

static const ScoVersion* find_version(const uint8_t* data, uint32_t size, uint32_t entry) {
    for (uint32_t i = 0; i < (uint32_t)nuade_sizeof_array(sco_versions); i++) {
        const ScoVersion* version = &sco_versions[i];
        if (entry < version->prefix_bytes || version->code_len > size - entry) {
            continue;
        }
        const uint32_t bra = version->tick_bra;
        const int64_t tick = (int64_t)bra + 2 + (int16_t)nuade_read_be16(data + bra + 2);
        if (tick == (int64_t)entry + version->tick_at && code_hash(data, entry, version) == version->hash) {
            return version;
        }
    }
    return nullptr;
}

bool sco_module_load(ScoModule* module, const uint8_t* data, size_t size) {
    memset(module, 0, sizeof(*module));
    if (size < (SCO_ENTRY_TABLE_WORDS * 4u + 4u) || size > SCO_MAX_MODULE_BYTES) {
        return false;
    }
    // Check2: bra.w at 0, 4 and 8; the first one leads to the init routine
    for (uint32_t i = 0; i < SCO_ENTRY_TABLE_WORDS; i++) {
        if (nuade_read_be16(data + i * 4) != SCO_BRA_W) {
            return false;
        }
    }
    const int64_t entry = 2 + (int64_t)(int16_t)nuade_read_be16(data + 2);
    if (entry < 0 || entry >= (int64_t)size) {
        return false;
    }
    module->entry = (uint32_t)entry;
    module->size = (uint32_t)size;
    module->version = find_version(data, module->size, module->entry);
    if (module->version == nullptr) {
        return false;
    }
    if (module->version->family == ScoFamily_Sampler && !sampler_variables_ok(data, module->size, module->entry)) {
        return false;
    }
    if (module->entry < (uint32_t)-module->version->var_at) {
        return false;
    }
    module->image = nuade_paula_padded_copy(data, size);
    module->original = malloc(size);
    module->var_read = calloc(1, sizeof(*module->var_read));
    module->var_start = (uint32_t)((int32_t)module->entry + module->version->var_at);
    if (module->image == nullptr || module->original == nullptr || module->var_read == nullptr) {
        sco_module_free(module);
        return false;
    }
    memcpy(module->original, data, size);
    if (!bind_roles(module, data) || !find_subsongs(module)
        || (module->version->family == ScoFamily_Sequencer && !sc3_modeled(module))) {
        sco_module_free(module);
        return false;
    }
    *module->var_read = false;
    return true;
}

void sco_module_free(ScoModule* module) {
    free(module->image);
    free(module->original);
    free(module->var_read);
    module->var_read = nullptr;
    module->image = nullptr;
    module->original = nullptr;
}
