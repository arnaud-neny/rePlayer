// SPDX-License-Identifier: MIT
// Riff Raff module: the executable's hunks, the four known replayer versions and the readers of the song data.
//
// The replayer is part of every module and was rewritten for each game, so the port recognises a module by its code:
// the code from the init entry to the DMA bit table, with its relocated longs zeroed, must hash to a known version, and
// the role of every relocated long (which table or variable it addresses) must hash to that version's roles. The
// addresses themselves are read from fixed places of the code. A module that matches none is refused (UADE plays it).

#include "rr_internal.h"

#include "nuade_base.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define RR_HUNK_HEADER 0x3F3u
#define RR_HUNK_CODE 0x3E9u
#define RR_HUNK_RELOC32 0x3ECu
#define RR_HUNK_END 0x3F2u
#define RR_SIZE_MASK 0x3FFFFFFFu
#define RR_HEADER_BYTES 0x44u     // the fields the eagleplayer reads (up to the sample length table pointer)
#define RR_FIRST_WORD 0x70FF4E75u // moveq #-1,d0 / rts
#define RR_MAGIC_RIFF 0x52494646u
#define RR_MAGIC_RAFF 0x52414646u
#define RR_PERIOD_MARKER 0x35803280u // the first two periods of every period table
#define RR_MAX_SAMPLE_INSTRUMENTS 256u
#define RR_FADE_BYTES 8u
#define RR_ALL_VOICES 0xFu
#define RR_HUNK_HEADER_BYTES 0x20u

// The known versions. The slots are the places in the code (from the init entry) that hold each table's address; the
// hashes were taken from the corpus files.

static const RrSlot rr_protector_slots[] = {
    { RrRole_Voices, 0x2 },     { RrRole_Tracks, 0xF6 },      { RrRole_StartPattern, 0x108 },
    { RrRole_Mask, 0x16E },     { RrRole_LoopPtrs, 0x1EE },   { RrRole_LoopLens, 0x1F4 },
    { RrRole_Periods, 0x2E2 },  { RrRole_Rates, 0x2F6 },      { RrRole_DmaBits, 0x342 },
    { RrRole_Commands, 0x3B6 }, { RrRole_SamplePtrs, 0x3D6 }, { RrRole_SampleLens, 0x3DC },
    { RrRole_Patterns, 0x48A },
};

static const uint16_t rr_protector_commands[]
    = { 0x3C0, 0x3F0, 0x42A, 0x5DE, 0x5F0, 0x5FE, 0x60C, 0x626, 0x64A, 0x660, 0x670 };

static const RrSlot rr_sidewinder_slots[] = {
    { RrRole_Voices, 0x4 },       { RrRole_Mask, 0x10 },          { RrRole_Extra0, 0x178 },
    { RrRole_Tracks, 0x100 },     { RrRole_StartPattern, 0x112 }, { RrRole_LoopPtrs, 0x202 },
    { RrRole_LoopLens, 0x208 },   { RrRole_Periods, 0x300 },      { RrRole_Rates, 0x314 },
    { RrRole_DmaBits, 0x362 },    { RrRole_Commands, 0x3DA },     { RrRole_SamplePtrs, 0x3FA },
    { RrRole_SampleLens, 0x400 }, { RrRole_Patterns, 0x4B4 },
};

static const uint16_t rr_sidewinder_commands[]
    = { 0x3E4, 0x414, 0x450, 0x60E, 0x620, 0x62E, 0x63C, 0x656, 0x67A, 0x690, 0x6A0 };

static const RrSlot rr_tbird_slots[] = {
    { RrRole_Voices, 0x2 },         { RrRole_Extra0, 0xB6 },    { RrRole_Tracks, 0x10A },
    { RrRole_StartPattern, 0x11E }, { RrRole_Mask, 0x19A },     { RrRole_LoopPtrs, 0x216 },
    { RrRole_LoopLens, 0x21C },     { RrRole_Periods, 0x31E },  { RrRole_Rates, 0x332 },
    { RrRole_DmaBits, 0x37E },      { RrRole_Commands, 0x3F2 }, { RrRole_SamplePtrs, 0x412 },
    { RrRole_SampleLens, 0x418 },   { RrRole_Patterns, 0x4C6 },
};

static const uint16_t rr_tbird_commands[]
    = { 0x3FC, 0x42C, 0x466, 0x5A4, 0x5B6, 0x5C4, 0x5D2, 0x5EC, 0x610, 0x626, 0x636 };

static const RrSlot rr_bombfusion_slots[] = {
    { RrRole_Extra0, 0xC },        { RrRole_Voices, 0x12 },        { RrRole_Silence, 0x24 },
    { RrRole_TrackStart0, 0x118 }, { RrRole_StartPattern, 0x12C }, { RrRole_TrackStart1, 0x13C },
    { RrRole_TrackStart2, 0x160 }, { RrRole_TrackStart3, 0x184 },  { RrRole_Extra1, 0x1AC },
    { RrRole_Extra2, 0x1B8 },      { RrRole_Mask, 0x1C4 },         { RrRole_Periods, 0x326 },
    { RrRole_Rates, 0x33A },       { RrRole_DmaBits, 0x398 },      { RrRole_Commands, 0x41A },
    { RrRole_SamplePtrs, 0x43A },  { RrRole_SampleLens, 0x440 },   { RrRole_SamplePtrs2, 0x45A },
    { RrRole_SampleLens2, 0x460 }, { RrRole_Patterns, 0x53E },
};

static const uint16_t rr_bombfusion_commands[]
    = { 0x424, 0x474, 0x490, 0x4BC, 0x4F6, 0x692, 0x6A4, 0x6B2, 0x6C0, 0x6DA, 0x702, 0x718, 0x728 };

static const RrVersion rr_versions[] = {
    {
        .name = "Protector",
        .driver = RrDriver_Looping,
        .mask_source = RrMaskSource_File,
        .record_bytes = 0x36,
        .code_len = 0x6B0,
        .second_pass = 0x1C0,
        .tick = 0x16C,
        .code_hash = 0x0E53A4ED6BC12B6CULL,
        .role_hash = 0xD1E0EE7FF2C62F5EULL,
        .slots = rr_protector_slots,
        .slot_count = (uint32_t)nuade_sizeof_array(rr_protector_slots),
        .commands = rr_protector_commands,
        .command_count = (uint32_t)nuade_sizeof_array(rr_protector_commands),
    },
    {
        .name = "Side Winder II",
        .driver = RrDriver_Looping,
        .mask_source = RrMaskSource_Init,
        .record_bytes = 0x36,
        .code_len = 0x6E0,
        .second_pass = 0x1D4,
        .tick = 0x17E,
        .code_hash = 0x66F709495BD0C119ULL,
        .role_hash = 0x563CFB315EDE93B8ULL,
        .slots = rr_sidewinder_slots,
        .slot_count = (uint32_t)nuade_sizeof_array(rr_sidewinder_slots),
        .commands = rr_sidewinder_commands,
        .command_count = (uint32_t)nuade_sizeof_array(rr_sidewinder_commands),
    },
    {
        .name = "T'Bird",
        .driver = RrDriver_Clamped,
        .mask_source = RrMaskSource_Subsongs,
        .record_bytes = 0x36,
        .code_len = 0x676,
        .second_pass = 0x1E8,
        .tick = 0x198,
        .code_hash = 0xD14055040303E01CULL,
        .role_hash = 0xF6D088F1DA819206ULL,
        .slots = rr_tbird_slots,
        .slot_count = (uint32_t)nuade_sizeof_array(rr_tbird_slots),
        .commands = rr_tbird_commands,
        .command_count = (uint32_t)nuade_sizeof_array(rr_tbird_commands),
    },
    {
        .name = "Bomb Fusion",
        .driver = RrDriver_OneShot,
        .mask_source = RrMaskSource_File,
        .record_bytes = 0x3C,
        .code_len = 0x768,
        .second_pass = 0x1A8,
        .tick = 0, // one routine does both passes
        .code_hash = 0x774EC91DBB396A36ULL,
        .role_hash = 0xF8B2E702EF9DE2B8ULL,
        .slots = rr_bombfusion_slots,
        .slot_count = (uint32_t)nuade_sizeof_array(rr_bombfusion_slots),
        .commands = rr_bombfusion_commands,
        .command_count = (uint32_t)nuade_sizeof_array(rr_bombfusion_commands),
    },
};

// The pattern commands of the jump tables (a version's table has 11 or 13 entries).

static const RrCommand rr_looping_commands[] = {
    RrCommand_SetSample,  RrCommand_VolumeSlide,   RrCommand_MasterFade, RrCommand_SetVolume,
    RrCommand_Modulation, RrCommand_Modulation,    RrCommand_Rest,       RrCommand_RestSilence,
    RrCommand_PitchSlide, RrCommand_PitchSlideOff, RrCommand_Portamento,
};

static const RrCommand rr_oneshot_commands[] = {
    RrCommand_SetSample,  RrCommand_Loop,          RrCommand_Loop,       RrCommand_VolumeSlide, RrCommand_MasterFade,
    RrCommand_SetVolume,  RrCommand_Modulation,    RrCommand_Modulation, RrCommand_Rest,        RrCommand_RestSilence,
    RrCommand_PitchSlide, RrCommand_PitchSlideOff, RrCommand_Portamento,
};

static uint8_t rr_command_arg_bytes(RrCommand command) {
    switch (command) {
        case RrCommand_VolumeSlide:
        case RrCommand_MasterFade:
        case RrCommand_PitchSlide:
            return 2;
        case RrCommand_SetSample:
        case RrCommand_SetVolume:
        case RrCommand_Rest:
        case RrCommand_RestSilence:
        case RrCommand_Portamento:
            return 1;
        default:
            return 0;
    }
}

RrCommandInfo rr_command_info(RrDriver driver, uint8_t byte) {
    const RrCommand* table = driver == RrDriver_OneShot ? rr_oneshot_commands : rr_looping_commands;
    const uint32_t count = driver == RrDriver_OneShot ? (uint32_t)nuade_sizeof_array(rr_oneshot_commands)
                                                      : (uint32_t)nuade_sizeof_array(rr_looping_commands);
    const uint32_t index = (uint32_t)byte - RR_COMMAND_BASE;
    if (byte < RR_COMMAND_BASE || index >= count) {
        return (RrCommandInfo) { .command = RrCommand_Invalid, .arg_bytes = 0 };
    }
    return (RrCommandInfo) { .command = table[index], .arg_bytes = rr_command_arg_bytes(table[index]) };
}

// Signatures: FNV-1a over the code and over the roles.
#define RR_FNV_OFFSET 0xCBF29CE484222325ULL
#define RR_FNV_PRIME 0x100000001B3ULL

static uint64_t rr_mix_byte(uint64_t hash, uint8_t byte) {
    return (hash ^ byte) * RR_FNV_PRIME;
}

static uint64_t rr_mix_u32(uint64_t hash, uint32_t value) {
    for (uint32_t i = 0; i < 4; i++) {
        hash = rr_mix_byte(hash, (uint8_t)(value >> (8 * i)));
    }
    return hash;
}

// The span of addresses a role covers (the mask word and its copy, the two bytes of an extra variable).

static uint32_t rr_role_span(RrRole role) {
    if (role == RrRole_Mask) {
        return 4;
    }
    if (role >= RrRole_Extra0) {
        return 2;
    }
    return 1;
}

typedef struct RrBinding {
    uint32_t value[RrRole_Count];
    bool bound[RrRole_Count];
} RrBinding;

// The role of a relocated long's value: the voice records (with the fade words in front), or the table whose address
// it is. False when it addresses none of them.

static bool rr_classify(const RrBinding* binding, uint32_t record_bytes, uint32_t value, RrRole* role,
                        uint32_t* delta) {
    const uint32_t voices = binding->value[RrRole_Voices];
    if (value >= voices - RR_FADE_BYTES
        && value - (voices - RR_FADE_BYTES) < RR_FADE_BYTES + RR_VOICES * record_bytes) {
        *role = RrRole_Voices;
        *delta = value - (voices - RR_FADE_BYTES);
        return true;
    }
    for (uint32_t r = 0; r < RrRole_Count; r++) {
        if (r == RrRole_Voices || !binding->bound[r]) {
            continue;
        }
        if (value >= binding->value[r] && value - binding->value[r] < rr_role_span((RrRole)r)) {
            *role = (RrRole)r;
            *delta = value - binding->value[r];
            return true;
        }
    }
    return false;
}

static uint32_t rr_u32(const RrModule* module, uint32_t at) {
    return nuade_read_be32(module->image + at);
}

static uint16_t rr_u16_raw(const RrModule* module, uint32_t at) {
    return nuade_read_be16(module->image + at);
}

// The executable's hunk header: one code hunk, flagged chip memory.

typedef struct RrHunkHeader {
    uint32_t id;
    uint32_t resident_names; // none
    uint32_t table_size;     // one hunk
    uint32_t first;
    uint32_t last;
    uint32_t size_word; // memory flags in the top byte, the size in longs below
    uint32_t code_id;
    uint32_t code_longs;
} RrHunkHeader;

static RrHunkHeader rr_read_hunk_header(const uint8_t* data) {
    return (RrHunkHeader) {
        .id = nuade_read_be32(data),
        .resident_names = nuade_read_be32(data + 4),
        .table_size = nuade_read_be32(data + 8),
        .first = nuade_read_be32(data + 12),
        .last = nuade_read_be32(data + 16),
        .size_word = nuade_read_be32(data + 20),
        .code_id = nuade_read_be32(data + 24),
        .code_longs = nuade_read_be32(data + 28),
    };
}

// The hunks. The module must be one code hunk with its relocation table, as the eagleplayer's Check2 expects.

static bool rr_parse_hunks(RrModule* module, const uint8_t* data, size_t size) {
    if (size < RR_HUNK_HEADER_BYTES + 4 || size > RR_MAX_MODULE_BYTES) {
        return false;
    }
    const RrHunkHeader hunk = rr_read_hunk_header(data);
    const uint32_t longs = hunk.size_word & RR_SIZE_MASK;
    // Check2 wants memory flags (the module's samples play from chip memory)
    if (hunk.id != RR_HUNK_HEADER || hunk.resident_names != 0 || hunk.table_size != 1 || hunk.first != 0
        || hunk.last != 0 || (hunk.size_word >> 24) == 0 || hunk.code_id != RR_HUNK_CODE || hunk.code_longs != longs) {
        return false;
    }
    const uint64_t code_bytes = (uint64_t)longs * 4;
    if (code_bytes < RR_HEADER_BYTES || code_bytes + RR_HUNK_HEADER_BYTES + 8 > size
        || code_bytes > RR_MAX_MODULE_BYTES) {
        return false;
    }
    module->size = (uint32_t)code_bytes;
    module->image = nuade_paula_padded_copy(data + RR_HUNK_HEADER_BYTES, module->size);
    module->reloc = calloc(1, module->size);
    if (module->image == nullptr || module->reloc == nullptr) {
        return false;
    }
    size_t pos = RR_HUNK_HEADER_BYTES + code_bytes;
    if (nuade_read_be32(data + pos) != RR_HUNK_RELOC32) {
        return false;
    }
    pos += 4;
    for (;;) {
        if (size - pos < 4) {
            return false;
        }
        const uint32_t count = nuade_read_be32(data + pos);
        pos += 4;
        if (count == 0) {
            break;
        }
        if (size - pos < 4 || nuade_read_be32(data + pos) != 0 || (uint64_t)count * 4 > size - pos - 4) {
            return false;
        }
        pos += 4;
        for (uint32_t i = 0; i < count; i++, pos += 4) {
            const uint32_t at = nuade_read_be32(data + pos);
            if (at > module->size - 4 || (at & 1u) != 0 || module->reloc[at] != 0) {
                return false;
            }
            for (uint32_t k = 0; k < 4; k++) {
                module->reloc[at + k] = (uint8_t)(1 + k);
            }
        }
    }
    return size - pos >= 4 && nuade_read_be32(data + pos) == RR_HUNK_END;
}

// The module header with the tests of Check2: the words, three nonzero entries, and the pointers relocated.

static bool rr_read_header(RrModule* module) {
    if (rr_u32(module, 0) != RR_FIRST_WORD || rr_u32(module, 4) != RR_MAGIC_RIFF
        || rr_u32(module, 8) != RR_MAGIC_RAFF) {
        return false;
    }
    const uint32_t at_second_pass = 12, at_init = 16, at_subsongs = 20, at_tick = 24, at_name = 28, at_author = 32,
                   at_sample_ptrs = 40, at_sample_lens = 44;
    module->header = (RrHeader) {
        .second_pass = rr_u32(module, at_second_pass),
        .init = rr_u32(module, at_init),
        .subsong_count = rr_u32(module, at_subsongs),
        .tick = rr_u32(module, at_tick),
        .name = rr_u32(module, at_name),
        .author = rr_u32(module, at_author),
        .sample_ptrs = rr_u32(module, at_sample_ptrs),
        .sample_lens = rr_u32(module, at_sample_lens),
    };
    const RrHeader* h = &module->header;
    const uint32_t relocated[] = { at_second_pass, at_init, at_name, at_author, at_sample_ptrs, at_sample_lens };
    for (uint32_t i = 0; i < nuade_sizeof_array(relocated); i++) {
        if (module->reloc[relocated[i]] != 1) {
            return false;
        }
    }
    return h->second_pass != 0 && h->init != 0 && h->subsong_count != 0 && module->reloc[at_subsongs] == 0
           && (h->tick == 0 || module->reloc[at_tick] == 1);
}

static bool rr_range_in(const RrModule* module, uint32_t at, uint32_t bytes) {
    return at <= module->size && bytes <= module->size - at;
}

// The version whose code the module carries; null when it is none of them.

static const RrVersion* rr_find_version(const RrModule* module) {
    const uint32_t init = module->header.init;
    for (uint32_t v = 0; v < nuade_sizeof_array(rr_versions); v++) {
        const RrVersion* version = &rr_versions[v];
        if (init < RR_HEADER_BYTES || !rr_range_in(module, init, (uint32_t)version->code_len + 16u)) {
            continue;
        }
        if (module->header.second_pass != init + version->second_pass
            || module->header.tick != (version->tick != 0 ? init + version->tick : 0)) {
            continue;
        }
        uint64_t hash = RR_FNV_OFFSET;
        for (uint32_t i = 0; i < version->code_len; i++) {
            hash = rr_mix_byte(hash, module->reloc[init + i] != 0 ? 0 : module->image[init + i]);
        }
        if (hash == version->code_hash) {
            return version;
        }
    }
    return nullptr;
}

// The addresses at the version's slots, then the role of every relocated long of the code.

static bool rr_bind_roles(RrModule* module, const RrVersion* version, RrBinding* binding) {
    const uint32_t init = module->header.init;
    for (uint32_t i = 0; i < version->slot_count; i++) {
        const RrSlot* slot = &version->slots[i];
        const uint32_t at = init + slot->at;
        if (binding->bound[slot->role] || module->reloc[at] != 1) {
            return false;
        }
        binding->value[slot->role] = rr_u32(module, at);
        binding->bound[slot->role] = true;
    }
    if (!binding->bound[RrRole_Voices] || binding->value[RrRole_Voices] < RR_FADE_BYTES) {
        return false;
    }
    uint64_t hash = RR_FNV_OFFSET;
    for (uint32_t at = init; at < init + version->code_len; at++) {
        if (module->reloc[at] != 1) {
            continue;
        }
        RrRole role = RrRole_Voices;
        uint32_t delta = 0;
        if (!rr_classify(binding, version->record_bytes, rr_u32(module, at), &role, &delta)) {
            return false;
        }
        hash = rr_mix_u32(hash, at - init);
        hash = rr_mix_u32(hash, (uint32_t)role);
        hash = rr_mix_u32(hash, delta);
    }
    return hash == version->role_hash;
}

static bool rr_offset_within(uint32_t at, uint32_t start, uint32_t bytes) {
    return at >= start && at - start < bytes;
}

// A range with `bytes` bytes from `at` shares a byte with the range.

static bool rr_range_hit(const RrRange* range, uint32_t at, uint32_t bytes) {
    return at < range->end && range->start < at + bytes;
}

static bool rr_add_range(RrModule* module, uint32_t* count, RrRange blocked[], uint32_t start, uint32_t bytes) {
    if (!rr_range_in(module, start, bytes) || *count >= RR_MAX_RANGES) {
        return false;
    }
    for (uint32_t i = 0; i < *count; i++) {
        if (rr_range_hit(&blocked[i], start, bytes)) {
            return false;
        }
    }
    blocked[*count] = (RrRange) { .start = start, .end = start + bytes };
    (*count)++;
    return true;
}

// The addresses must make the memory map the code assumes: the replayer's variables and the code itself are not song
// data, and the tables lie where the eagleplayer's patching looks for them.

static bool rr_check_layout(RrModule* module, const RrVersion* version) {
    const uint32_t* base = module->base;
    for (uint32_t r = 0; r < RrRole_Count; r++) {
        if (r != RrRole_Silence && !(r >= RrRole_TrackStart0 && r <= RrRole_TrackStart3) && (base[r] & 1u) != 0) {
            return false; // words and longs of the original must be at even addresses
        }
    }
    if (base[RrRole_DmaBits] != module->header.init + version->code_len
        || !rr_range_in(module, base[RrRole_DmaBits], 16) || base[RrRole_Periods] != base[RrRole_DmaBits] + 8
        || rr_u16_raw(module, base[RrRole_DmaBits]) != 1 || rr_u16_raw(module, base[RrRole_DmaBits] + 2) != 2
        || rr_u16_raw(module, base[RrRole_DmaBits] + 4) != 4 || rr_u16_raw(module, base[RrRole_DmaBits] + 6) != 8
        || rr_u32(module, base[RrRole_Periods]) != RR_PERIOD_MARKER) {
        return false;
    }
    if (module->header.sample_ptrs != base[RrRole_SamplePtrs]
        || module->header.sample_lens != base[RrRole_SampleLens]) {
        return false;
    }
    RrRange blocked[RR_MAX_RANGES];
    uint32_t count = 0;
    const uint32_t voices = base[RrRole_Voices];
    if (voices < RR_FADE_BYTES || !rr_add_range(module, &count, blocked, module->header.init, version->code_len)
        || !rr_add_range(module, &count, blocked, voices - RR_FADE_BYTES,
                         RR_FADE_BYTES + RR_VOICES * version->record_bytes)
        || !rr_add_range(module, &count, blocked, base[RrRole_Mask], 4)) {
        return false;
    }
    if (version->driver == RrDriver_OneShot) {
        // the three words behind the mask copy: written by the init routine, the key flag and the pause flag are 0
        if (base[RrRole_Extra0] != base[RrRole_Mask] + 4 || base[RrRole_Extra2] != base[RrRole_Mask] + 6
            || base[RrRole_Extra1] != base[RrRole_Mask] + 8 || !rr_range_in(module, base[RrRole_Mask], 10)
            || rr_u16_raw(module, base[RrRole_Extra1]) != 0 || rr_u16_raw(module, base[RrRole_Extra2]) != 0) {
            return false;
        }
        blocked[count - 1].end = base[RrRole_Mask] + 10;
    } else if (base[RrRole_Extra0] != 0
               && (base[RrRole_Extra0] != module->header.init - 2 || !rr_range_in(module, base[RrRole_Extra0], 2))) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        module->blocked[i] = blocked[i];
    }
    module->blocked_count = count;
    return true;
}

// What the eagleplayer's InitPlayer does to the sample tables: when the first sample pointer is zero it becomes the
// address of the eagleplayer's four zero bytes of chip memory (its second hunk), and so does every zero in the table
// that follows the length table (n entries, n = sample count). The port keeps those bytes behind the module.

static bool rr_check_samples(RrModule* module) {
    const uint32_t ptrs = module->base[RrRole_SamplePtrs];
    const uint32_t lens = module->base[RrRole_SampleLens];
    if (lens <= ptrs || (lens - ptrs) % 4 != 0) {
        return false;
    }
    const uint32_t n = (lens - ptrs) / 4;
    const uint32_t start = lens + 2 * n;
    if (!rr_range_in(module, start, 4 * n) || !rr_range_in(module, ptrs, 4)) {
        return false;
    }
    // `tst.l (a3)` sees the relocated value: a relocated zero is not zero
    module->zero_is_base = rr_u32(module, ptrs) == 0 && module->reloc[ptrs] == 0;
    module->zero_fix = (RrRange) { .start = start, .end = start + 4 * n };
    return true;
}

// Bytes the original would read other values from: code, variables, relocated longs, and the longs the eagleplayer
// overwrites (the zero sample pointers it replaces, the word Side Winder II's init sets in front of its code).

static bool rr_build_unreadable(RrModule* module) {
    module->unreadable_prefix = calloc((size_t)module->size + 1, sizeof(uint32_t));
    if (module->unreadable_prefix == nullptr) {
        return false;
    }
    const uint32_t first_pointer = module->base[RrRole_SamplePtrs];
    const uint32_t extra = module->base[RrRole_Extra0];
    const bool extra_before_code = module->version->driver != RrDriver_OneShot && extra != 0;
    for (uint32_t i = 0; i < module->size; i++) {
        bool bad = module->reloc[i] != 0;
        for (uint32_t r = 0; r < module->blocked_count; r++) {
            bad = bad
                  || rr_offset_within(i, module->blocked[r].start, module->blocked[r].end - module->blocked[r].start);
        }
        if (module->zero_is_base) {
            bad = bad || rr_offset_within(i, first_pointer, 4)
                  || rr_offset_within(i, module->zero_fix.start, module->zero_fix.end - module->zero_fix.start);
        }
        bad = bad || (extra_before_code && rr_offset_within(i, extra, 2));
        module->unreadable_prefix[i + 1] = module->unreadable_prefix[i] + (bad ? 1u : 0u);
    }
    return true;
}

void rr_module_free(RrModule* module) {
    free(module->image);
    free(module->reloc);
    free(module->unreadable_prefix);
    *module = (RrModule) { 0 };
}

bool rr_module_load(RrModule* module, const uint8_t* data, size_t size) {
    *module = (RrModule) { 0 };
    RrBinding binding = { 0 };
    const RrVersion* version = nullptr;
    if (rr_parse_hunks(module, data, size) && rr_read_header(module)) {
        version = rr_find_version(module);
    }
    bool ok = version != nullptr;
    if (ok) {
        module->version = version;
        ok = rr_bind_roles(module, version, &binding);
    }
    if (ok) {
        memcpy(module->base, binding.value, sizeof(module->base));
        ok = rr_check_layout(module, version) && rr_check_samples(module) && rr_build_unreadable(module);
    }
    if (ok) {
        const uint32_t subsong_count = module->header.subsong_count;
        const bool subsongs = version->mask_source == RrMaskSource_Subsongs;
        ok = subsong_count >= 1 && subsong_count <= (subsongs ? RR_MAX_SUBSONGS : 1u);
        if (ok && subsongs) {
            // subsongs after the first load their voice mask only when this flag word of the module is set
            module->mask_flag = rr_u16_raw(module, module->base[RrRole_Extra0]) != 0;
            ok = module->mask_flag
                 && rr_range_in(module, module->base[RrRole_Tracks], subsong_count * RR_SUBSONG_TABLE_BYTES);
        }
    }
    if (ok) {
        // the jump table must hold the handlers this port knows, in this order
        for (uint32_t i = 0; i < version->command_count && ok; i++) {
            ok = rr_range_in(module, module->base[RrRole_Commands] + 4 * i, 4)
                 && rr_u32(module, module->base[RrRole_Commands] + 4 * i) == module->header.init + version->commands[i];
        }
    }
    if (!ok) {
        rr_module_free(module);
    }
    return ok;
}

// Readers.

static bool rr_readable(const RrModule* module, uint32_t at, uint32_t bytes) {
    return rr_range_in(module, at, bytes) && module->unreadable_prefix[at + bytes] == module->unreadable_prefix[at];
}

bool rr_module_u8(const RrModule* module, uint32_t at, uint8_t* out) {
    if (!rr_readable(module, at, 1)) {
        return false;
    }
    *out = module->image[at];
    return true;
}

bool rr_module_u16(const RrModule* module, uint32_t at, uint16_t* out) {
    if ((at & 1u) != 0 || !rr_readable(module, at, 2)) {
        return false;
    }
    *out = rr_u16_raw(module, at);
    return true;
}

bool rr_module_pointer(const RrModule* module, uint32_t at, uint32_t* out) {
    if ((at & 1u) != 0 || !rr_range_in(module, at, 4)) {
        return false;
    }
    for (uint32_t i = 0; i < module->blocked_count; i++) {
        if (rr_range_hit(&module->blocked[i], at, 4)) {
            return false;
        }
    }
    const uint32_t value = rr_u32(module, at);
    if (module->reloc[at] == 1) {
        if (value >= module->size) {
            return false;
        }
        *out = value;
        return true;
    }
    const bool fixed
        = value == 0
          && (at == module->base[RrRole_SamplePtrs] || (at >= module->zero_fix.start && at < module->zero_fix.end));
    if (!fixed || !module->zero_is_base) {
        return false;
    }
    *out = module->size;
    return true;
}

bool rr_module_sample_ok(const RrModule* module, uint32_t at, uint16_t words) {
    if (at == module->size) {
        return words >= 1 && words <= RR_ZERO_BUFFER_WORDS;
    }
    return rr_readable(module, at, words != 0 ? 2u * words : NUADE_PAULA_MAX_SAMPLE_BYTES);
}

static bool rr_module_table_sample(const RrModule* module, RrRole ptrs, RrRole lens, uint32_t instrument,
                                   RrSample* out) {
    uint32_t start = 0;
    uint16_t words = 0;
    if (instrument >= RR_MAX_SAMPLE_INSTRUMENTS
        || !rr_module_pointer(module, module->base[ptrs] + 4 * instrument, &start)
        || !rr_module_u16(module, module->base[lens] + 2 * instrument, &words)) {
        return false;
    }
    start &= ~1u; // Paula ignores AUDxLC bit 0
    if (!rr_module_sample_ok(module, start, words)) {
        return false;
    }
    *out = (RrSample) { .start = start, .words = words };
    return true;
}

bool rr_module_sample(const RrModule* module, uint32_t instrument, RrSample* out) {
    return rr_module_table_sample(module, RrRole_SamplePtrs, RrRole_SampleLens, instrument, out);
}

bool rr_module_loop(const RrModule* module, uint32_t instrument, RrSample* out) {
    return rr_module_table_sample(module, RrRole_LoopPtrs, RrRole_LoopLens, instrument, out);
}

bool rr_module_period(const RrModule* module, uint32_t index, uint16_t* out) {
    return rr_module_u16(module, module->base[RrRole_Periods] + RR_PERIOD_ENTRY_BYTES * index, out);
}

bool rr_module_rate(const RrModule* module, uint32_t index, uint16_t* out) {
    return rr_module_u16(module, module->base[RrRole_Rates] + RR_RATE_ENTRY_BYTES * index, out);
}

bool rr_module_pattern(const RrModule* module, uint32_t number, uint32_t* out) {
    return rr_module_pointer(module, module->base[RrRole_Patterns] + 4 * number, out);
}

static void rr_set_enabled(RrSongStart* song, uint16_t mask) {
    for (uint32_t v = 0; v < RR_VOICES; v++) {
        song->enabled[v] = (mask >> v & 1u) != 0;
    }
}

bool rr_module_song_start(const RrModule* module, uint32_t subsong, RrSongStart* out) {
    const RrVersion* version = module->version;
    uint16_t mask = 0;
    if (subsong >= module->header.subsong_count) {
        return false;
    }
    *out = (RrSongStart) { 0 };
    if (version->driver == RrDriver_OneShot) {
        for (uint32_t v = 0; v < RR_VOICES; v++) {
            out->track_list[v] = module->base[RrRole_TrackStart0 + v];
            if (out->track_list[v] >= module->size) {
                return false;
            }
        }
    } else {
        const uint32_t table = module->base[RrRole_Tracks] + subsong * RR_SUBSONG_TABLE_BYTES;
        for (uint32_t v = 0; v < RR_VOICES; v++) {
            if (!rr_module_pointer(module, table + 4 * v, &out->track_list[v])) {
                return false;
            }
        }
    }
    switch (version->mask_source) {
        case RrMaskSource_File:
            mask = rr_u16_raw(module, module->base[RrRole_Mask]);
            break;
        case RrMaskSource_Init:
            mask = RR_ALL_VOICES;
            break;
        case RrMaskSource_Subsongs:
            if (!rr_module_u16(module,
                               module->base[RrRole_Tracks] + subsong * RR_SUBSONG_TABLE_BYTES + RR_SUBSONG_MASK_OFFSET,
                               &mask)) {
                return false;
            }
            break;
    }
    rr_set_enabled(out, mask);
    return true;
}

bool rr_module_voice_start(const RrModule* module, uint32_t voice, RrVoiceStart* out) {
    *out = (RrVoiceStart) { 0 };
    if (module->version->driver == RrDriver_OneShot) {
        return true; // its init routine writes every field that is read
    }
    const uint32_t record = module->base[RrRole_Voices] + voice * module->version->record_bytes;
    out->instrument = rr_u16_raw(module, record + RR_VOICE_INSTRUMENT_OFFSET);
    out->transpose = rr_u16_raw(module, record + RR_VOICE_TRANSPOSE_OFFSET);
    // an instrument indexes the tables and a transposition the period table by byte
    return out->instrument < RR_MAX_SAMPLE_INSTRUMENTS && out->transpose < 256;
}

uint32_t rr_module_silence(const RrModule* module) {
    // AUDxLC ignores bit 0
    return module->version->driver == RrDriver_OneShot ? module->base[RrRole_Silence] & ~1u : module->size;
}

bool rr_module_track_list_ends(const RrModule* module, uint8_t byte) {
    return module->version->driver == RrDriver_OneShot ? byte >= RR_COMMAND_BASE : byte == RR_TRACK_END;
}

// The walk. A pattern is summarised once: the note and rate ranges it reads (the tables are read at note +
// transposition, so the note range is checked per use), whether the replayer returns from it (a note or a rest) and
// how many event loop iterations it needs between two returns (the port runs at most RR_EVENT_BUDGET of them in one
// call; a module that needs more is refused).

typedef struct RrPatternSummary {
    bool done;
    bool returns;
    bool has_note;
    uint8_t min_note;
    uint8_t max_note;
    bool has_rate;
    uint8_t min_rate;
    uint8_t max_rate;
    uint32_t steps;        // iterations up to and including the end byte
    uint32_t first_return; // ... up to and including the first note or rest
    uint32_t max_gap;      // between two returns
    uint32_t tail;         // after the last return up to and including the end byte
} RrPatternSummary;

typedef struct RrWalk {
    const RrModule* module;
    RrPatternSummary patterns[RR_PATTERN_SLOTS + 1]; // the pattern numbers, then the start pattern
    bool instrument_seen[RR_MAX_SAMPLE_INSTRUMENTS];
} RrWalk;

#define RR_START_PATTERN_SLOT RR_PATTERN_SLOTS

static void rr_summary_note(RrPatternSummary* s, uint8_t note) {
    s->min_note = s->has_note && s->min_note < note ? s->min_note : note;
    s->max_note = s->has_note && s->max_note > note ? s->max_note : note;
    s->has_note = true;
}

static void rr_summary_rate(RrPatternSummary* s, uint8_t rate) {
    s->min_rate = s->has_rate && s->min_rate < rate ? s->min_rate : rate;
    s->max_rate = s->has_rate && s->max_rate > rate ? s->max_rate : rate;
    s->has_rate = true;
}

// A note or a rest ends a call of the event loop.

static void rr_summary_return(RrPatternSummary* s, uint32_t* since) {
    if (!s->returns) {
        s->first_return = *since;
    } else if (*since > s->max_gap) {
        s->max_gap = *since;
    }
    s->returns = true;
    *since = 0;
}

// Reads the pattern at `start` up to its end byte. False when the pattern leaves the module, uses a command the port
// does not model or reads the module where the original would read something else.

static bool rr_walk_pattern(RrWalk* walk, uint32_t start, RrPatternSummary* summary) {
    const RrModule* module = walk->module;
    uint32_t pos = start;
    uint32_t steps = 0;
    uint32_t since = 0;
    for (;;) {
        uint8_t byte = 0;
        if (!rr_module_u8(module, pos++, &byte)) {
            return false;
        }
        steps++;
        since++;
        if (byte == RR_PATTERN_END) {
            summary->steps = steps;
            summary->tail = since;
            return true;
        }
        uint8_t arg = 0;
        if (byte < RR_COMMAND_BASE) {
            if (!rr_module_u8(module, pos++, &arg)) {
                return false;
            }
            rr_summary_note(summary, byte);
            rr_summary_rate(summary, arg);
            rr_summary_return(summary, &since);
            continue;
        }
        const RrCommandInfo info = rr_command_info(module->version->driver, byte);
        if (info.command == RrCommand_Invalid || info.command == RrCommand_Portamento) {
            return false;
        }
        if (info.arg_bytes != 0 && !rr_module_u8(module, pos, &arg)) {
            return false;
        }
        pos += info.arg_bytes;
        if (info.command == RrCommand_SetSample) {
            walk->instrument_seen[arg] = true;
        } else if (info.command == RrCommand_Rest || info.command == RrCommand_RestSilence) {
            rr_summary_rate(summary, arg);
            rr_summary_return(summary, &since);
        }
    }
}

static bool rr_walk_summary(RrWalk* walk, uint32_t slot, uint32_t start, RrPatternSummary** out) {
    RrPatternSummary* summary = &walk->patterns[slot];
    if (!summary->done) {
        if (!rr_walk_pattern(walk, start, summary)) {
            return false;
        }
        summary->done = true;
    }
    *out = summary;
    return true;
}

// The tables a pattern reads with a transposition: every period and rate between the extremes must be readable.

static bool rr_walk_tables(const RrWalk* walk, const RrPatternSummary* s, uint32_t transpose) {
    const RrModule* module = walk->module;
    if (s->has_note) {
        const uint32_t first = module->base[RrRole_Periods] + RR_PERIOD_ENTRY_BYTES * (s->min_note + transpose);
        const uint32_t last = module->base[RrRole_Periods] + RR_PERIOD_ENTRY_BYTES * (s->max_note + transpose);
        if ((first & 1u) != 0 || !rr_readable(module, first, last - first + RR_PERIOD_ENTRY_BYTES)) {
            return false;
        }
    }
    if (s->has_rate) {
        const uint32_t first = module->base[RrRole_Rates] + RR_RATE_ENTRY_BYTES * s->min_rate;
        const uint32_t last = module->base[RrRole_Rates] + RR_RATE_ENTRY_BYTES * s->max_rate;
        if (!rr_readable(module, first, last - first + RR_RATE_ENTRY_BYTES)) {
            return false;
        }
    }
    return true;
}

// Adds iterations to the run of event loop iterations since the last return; false when it exceeds the budget.

static bool rr_walk_count(uint64_t* run, uint32_t iterations) {
    *run += iterations;
    return *run <= RR_EVENT_BUDGET;
}

// The run is carried through the entries of a voice: a pattern that returns resets it to its tail, one that does not
// adds all its plays (a pattern plays repeat + 1 times).

static bool rr_walk_plays(uint64_t* run, const RrPatternSummary* s, uint32_t plays) {
    if (!s->returns) {
        return rr_walk_count(run, plays * s->steps);
    }
    if (s->max_gap > RR_EVENT_BUDGET || (plays > 1 && (uint64_t)s->tail + s->first_return > RR_EVENT_BUDGET)
        || !rr_walk_count(run, s->first_return)) {
        return false;
    }
    *run = s->tail;
    return true;
}

// A track list is a loop: its entries run in order and the end byte restarts it (the transposition set in the first
// round carries into the second, then the state repeats).

static bool rr_walk_track_list(RrWalk* walk, uint32_t start, uint32_t transpose, uint64_t* run) {
    const RrModule* module = walk->module;
    const bool oneshot = module->version->driver == RrDriver_OneShot;
    for (uint32_t round = 0; round < 2; round++) {
        uint32_t pos = start;
        bool returns = false;
        for (;;) {
            uint8_t byte = 0;
            if (!rr_module_u8(module, pos++, &byte) || !rr_walk_count(run, 1)) {
                return false;
            }
            if (rr_module_track_list_ends(module, byte)) {
                break;
            }
            if (!oneshot && byte == RR_TRACK_TRANSPOSE) {
                if (!rr_module_u8(module, pos++, &byte)) {
                    return false;
                }
                transpose = byte;
                continue;
            }
            uint8_t repeat = 0;
            uint32_t pattern = 0;
            RrPatternSummary* summary = nullptr;
            if (!rr_module_u8(module, pos++, &repeat) || !rr_module_pattern(module, byte, &pattern)
                || !rr_walk_summary(walk, byte, pattern, &summary) || !rr_walk_tables(walk, summary, transpose)
                || !rr_walk_plays(run, summary, (uint32_t)repeat + 1)) {
                return false;
            }
            returns = returns || summary->returns;
        }
        if (!returns) {
            return false; // the replayer would loop through the list without ever returning
        }
    }
    return true;
}

// A voice: its start pattern, once, then its track list.

static bool rr_walk_voice(RrWalk* walk, uint32_t voice, uint32_t track_list) {
    const RrModule* module = walk->module;
    RrVoiceStart start = { 0 };
    RrPatternSummary* summary = nullptr;
    uint64_t run = 0;
    if (!rr_module_voice_start(module, voice, &start)) {
        return false;
    }
    walk->instrument_seen[start.instrument] = true;
    return rr_walk_summary(walk, RR_START_PATTERN_SLOT, module->base[RrRole_StartPattern], &summary)
           && rr_walk_tables(walk, summary, start.transpose) && rr_walk_plays(&run, summary, 1)
           && rr_walk_track_list(walk, track_list, start.transpose, &run);
}

static bool rr_walk_instruments(const RrWalk* walk) {
    const RrModule* module = walk->module;
    for (uint32_t i = 0; i < RR_MAX_SAMPLE_INSTRUMENTS; i++) {
        RrSample sample = { 0 };
        if (walk->instrument_seen[i]
            && (!rr_module_sample(module, i, &sample)
                || (module->version->driver != RrDriver_OneShot && !rr_module_loop(module, i, &sample)))) {
            return false;
        }
    }
    return true;
}

bool rr_module_validate(const RrModule* module) {
    RrWalk* walk = calloc(1, sizeof(*walk));
    if (walk == nullptr) {
        return false;
    }
    walk->module = module;
    const bool oneshot = module->version->driver == RrDriver_OneShot;
    // what the init routine leaves in the voices: the eagleplayer's zero bytes (or the silent word)
    bool ok = rr_module_sample_ok(module, oneshot ? rr_module_silence(module) : module->size, 1);
    for (uint32_t subsong = 0; ok && subsong < module->header.subsong_count; subsong++) {
        RrSongStart song = { 0 };
        ok = rr_module_song_start(module, subsong, &song);
        for (uint32_t voice = 0; ok && voice < RR_VOICES; voice++) {
            ok = !song.enabled[voice] || rr_walk_voice(walk, voice, song.track_list[voice]);
        }
    }
    ok = ok && rr_walk_instruments(walk);
    free(walk);
    return ok;
}
