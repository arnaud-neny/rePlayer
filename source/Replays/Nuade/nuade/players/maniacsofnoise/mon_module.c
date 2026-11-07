// SPDX-License-Identifier: MIT
// Maniacs of Noise player: the module. The eagleplayer's Check2 accepts a module whose first three words are jumps
// (4EFA) and whose first jump enters a routine that loads the data block address (4BFA) and clears bit 0 of a variable
// in it (08AD 0000); the third jump enters the routine that starts a song, which compares the song number with the
// song count byte. The player additionally requires the replayer's code to be one of the known versions, the jumps to
// lead where that version has them, and every song to use only commands the replayer can run.

#include "mon_internal.h"

#define JUMP_OPCODE 0x4EFA          // jmp d16(pc)
#define LOAD_BASE_OPCODE 0x4BFA     // lea d16(pc),a5
#define CLEAR_BIT_OPCODE 0x08AD0000 // bclr.b #0,d16(a5)
#define COMPARE_COUNT_OPCODE 0xB02D // cmp.b d16(a5),d0
#define LOAD_PC_A3_OPCODE 0x47FA    // lea d16(pc),a3
#define COMPARE_COUNT_OFFSET 0x16   // from the start-song routine to the compare
#define ENTRY_JUMPS 3
#define SIGNATURE_SEED 0xCBF29CE484222325ULL
#define SIGNATURE_PRIME 0x100000001B3ULL
#define MAX_SUBSONGS 0x7F // the start routine compares the song number with the count as signed bytes

// The signatures are those of the code of the first module of each version; the modules of one version differ in the
// text before the code, which they do not cover. MonVersion_DeathTrap has a second signature: the module of Chinese
// Chess (in-game) switches the audio filter the other way, which only changes the CIA bit the replayer writes.
// The voice records are those of the data block as the image holds them.
static const MonLayout k_layouts[] = {
    { .version = MonVersion_Gyroscope,
      .signature = 0xFA6BCD0FF900A6F7ULL,
      .interrupt_entry = 0x156,
      .start_entry = 0x62,
      .base_entry = 0x662,
      .song_table = 0x1CA,
      .song_stride = 16,
      .period_table = 0x13A,
      .silent_buffer = 0x12C,
      .subsong_count = 0x134,
      .envelope_mask = 0x121,
      .voice_records = 0x20,
      .voice_stride = 0x40,
      .record_volume_high = 0x14,
      .record_slide_length = 0x1E,
      .record_slide_from = 0x20 },
    { .version = MonVersion_FlimbosQuest,
      .signature = 0xB64E92DF48E1501DULL,
      .interrupt_entry = 0x17E,
      .start_entry = 0x8A,
      .base_entry = 0x6A2,
      .song_table = 0x1E2,
      .song_stride = 16,
      .period_table = 0x152,
      .silent_buffer = 0x144,
      .subsong_count = 0x14C,
      .envelope_mask = 0x139,
      .voice_records = 0x38,
      .voice_stride = 0x40,
      .record_volume_high = 0x14,
      .record_slide_length = 0x1E,
      .record_slide_from = 0x22 },
    { .version = MonVersion_DeathTrap,
      .signature = 0xE7D3E4869AC694CFULL,
      .interrupt_entry = 0x19A,
      .start_entry = 0xA4,
      .base_entry = 0x68C,
      .song_table = 0x5C2,
      .song_stride = 12,
      .period_table = 0x1B2,
      .silent_buffer = 0x1A4,
      .subsong_count = 0x1AC,
      .envelope_mask = 0x199,
      .voice_records = 0x48,
      .voice_stride = 0x54,
      .record_volume_high = 0x1C,
      .record_period_fixed = 0x3A,
      .record_swing = 0x53,
      .record_slide_length = 0x26,
      .record_slide_from = 0x2A },
    { .version = MonVersion_DeathTrap,
      .signature = 0x423B2D249E7E8B0FULL,
      .interrupt_entry = 0x19A,
      .start_entry = 0xA4,
      .base_entry = 0x68C,
      .song_table = 0x5C2,
      .song_stride = 12,
      .period_table = 0x1B2,
      .silent_buffer = 0x1A4,
      .subsong_count = 0x1AC,
      .envelope_mask = 0x199,
      .voice_records = 0x48,
      .voice_stride = 0x54,
      .record_volume_high = 0x1C,
      .record_period_fixed = 0x3A,
      .record_swing = 0x53,
      .record_slide_length = 0x26,
      .record_slide_from = 0x2A },
    { .version = MonVersion_DragonWars,
      .signature = 0xC90A6CA2679CC325ULL,
      .interrupt_entry = 0x19A,
      .start_entry = 0xA4,
      .base_entry = 0x6FA,
      .song_table = 0x5C2,
      .song_stride = 12,
      .period_table = 0x1B2,
      .silent_buffer = 0x1A4,
      .subsong_count = 0x1AC,
      .envelope_mask = 0x199,
      .voice_records = 0x48,
      .voice_stride = 0x54,
      .record_volume_high = 0x1C,
      .record_swing = 0x53,
      .record_slide_length = 0x26,
      .record_slide_from = 0x2A },
    { .version = MonVersion_MysteryWorldsEnd,
      .signature = 0x190BD113467DBD36ULL,
      .code_start = 0x30,
      .interrupt_entry = 0x19A,
      .start_entry = 0xA4,
      .base_entry = 0x7DA,
      .song_table = 0x5C2,
      .song_stride = 12,
      .period_table = 0x1B2,
      .silent_buffer = 0x1A4,
      .subsong_count = 0x1AC,
      .envelope_mask = 0x199,
      .wave_table = 0x1AA4,
      .voice_records = 0x48,
      .voice_stride = 0x54,
      .record_volume_high = 0x1C,
      .record_period_fixed = 0x3A,
      .record_swing = 0x53,
      .record_slide_length = 0x26,
      .record_slide_from = 0x2A },
};

uint8_t mon_u8(const MonModule* m, uint32_t address) {
    return address < m->size ? m->bytes[address] : 0;
}

uint16_t mon_u16(const MonModule* m, uint32_t address) {
    return (uint16_t)((uint16_t)mon_u8(m, address) << 8 | mon_u8(m, address + 1));
}

uint32_t mon_u32(const MonModule* m, uint32_t address) {
    return (uint32_t)mon_u16(m, address) << 16 | mon_u16(m, address + 2);
}

// The same reads on the bytes of an image that is not yet a MonModule.

typedef struct RawImage {
    const uint8_t* bytes;
    uint32_t size;
} RawImage;

static uint16_t raw_u16(const RawImage* image, uint32_t address) {
    return address <= image->size && image->size - address >= 2 ? nuade_read_be16(image->bytes + address) : 0;
}

// The address a pc-relative displacement word at `address` leads to (the displacement counts from its own address).

static uint32_t pc_target(const RawImage* image, uint32_t address) {
    return address + (uint32_t)(int32_t)(int16_t)raw_u16(image, address);
}

// FNV-1a over the code from the first entry to the data block. The displacement of a pc-relative load of
// MON_VOICE_MASK_ADDRESS into a3 counts as zero, so that the signature does not depend on where the code starts. False
// when the code lies outside the image.

static bool code_signature(const RawImage* image, uint32_t first, uint32_t end, uint64_t* out) {
    if (first > end || end > image->size) {
        return false;
    }
    uint64_t hash = SIGNATURE_SEED;
    for (uint32_t at = first; at < end; at++) {
        // The word the byte belongs to, counted from the first entry; a displacement follows its opcode word
        const uint32_t word = at - ((at - first) & 1);
        const bool masked = word >= first + 2 && raw_u16(image, word - 2) == LOAD_PC_A3_OPCODE
                            && pc_target(image, word) == MON_VOICE_MASK_ADDRESS;
        hash = (hash ^ (masked ? 0 : image->bytes[at])) * SIGNATURE_PRIME;
    }
    *out = hash;
    return true;
}

typedef struct Entries {
    uint32_t init;
    uint32_t interrupt;
    uint32_t start_song;
    uint32_t base;
    uint32_t count_offset; // of the song count byte from the data block
} Entries;

// Check2 of the eagleplayer: the entries of the three jumps and the data block.

static bool check_entries(const RawImage* image, Entries* out) {
    uint32_t jumps[ENTRY_JUMPS];
    for (uint32_t i = 0; i < ENTRY_JUMPS; i++) {
        if (raw_u16(image, i * 4) != JUMP_OPCODE) {
            return false;
        }
        jumps[i] = pc_target(image, i * 4 + 2);
    }
    out->init = jumps[0];
    out->interrupt = jumps[1];
    out->start_song = jumps[2];
    if (raw_u16(image, out->init) != LOAD_BASE_OPCODE
        || ((uint32_t)raw_u16(image, out->init + 4) << 16 | raw_u16(image, out->init + 6)) != CLEAR_BIT_OPCODE) {
        return false;
    }
    out->base = pc_target(image, out->init + 2);
    if (raw_u16(image, out->start_song + COMPARE_COUNT_OFFSET) != COMPARE_COUNT_OPCODE) {
        return false;
    }
    out->count_offset = (uint32_t)(int32_t)(int16_t)raw_u16(image, out->start_song + COMPARE_COUNT_OFFSET + 2);
    return true;
}

// The layout of the version whose code and entry positions the module has.

static const MonLayout* find_layout(const RawImage* image, const Entries* entries) {
    uint64_t signature;
    if (!code_signature(image, entries->init, entries->base, &signature)) {
        return nullptr;
    }
    for (size_t i = 0; i < nuade_sizeof_array(k_layouts); i++) {
        const MonLayout* layout = &k_layouts[i];
        if (layout->signature == signature && (layout->code_start == 0 || layout->code_start == entries->init)
            && layout->subsong_count == entries->count_offset
            && entries->interrupt - entries->init == layout->interrupt_entry
            && entries->start_song - entries->init == layout->start_entry
            && entries->base - entries->init == layout->base_entry) {
            return layout;
        }
    }
    return nullptr;
}

bool mon_locate(const uint8_t* bytes, size_t size, MonModule* out) {
    if (size > UINT32_MAX / 2) {
        return false;
    }
    const RawImage image = { .bytes = bytes, .size = (uint32_t)size };
    Entries entries;
    if (!check_entries(&image, &entries)) {
        return false;
    }
    const MonLayout* layout = find_layout(&image, &entries);
    if (layout == nullptr) {
        return false;
    }
    // The song count byte and the song records have to be inside the image
    const uint32_t count_at = entries.base + layout->subsong_count;
    if (count_at >= image.size || image.bytes[count_at] == 0 || image.bytes[count_at] > MAX_SUBSONGS
        || entries.base > image.size || layout->song_table >= image.size - entries.base) {
        return false;
    }
    *out = (MonModule) {
        .bytes = bytes,
        .size = image.size,
        .layout = layout,
        .base = entries.base,
        .songs = entries.base + layout->song_table,
        .subsong_count = image.bytes[count_at],
    };
    out->instruments = out->songs + out->subsong_count * layout->song_stride;
    return mon_walk_songs(out);
}

uint32_t mon_song_address(const MonModule* m, uint16_t offset) {
    return m->songs + (uint32_t)(int32_t)(int16_t)offset;
}

const int8_t* mon_sample(const MonModule* m, uint32_t address) {
    return address < m->size ? (const int8_t*)(m->bytes + (address & ~1u)) : nullptr;
}

uint16_t mon_period_entry(const MonModule* m, unsigned note) {
    return mon_u16(m, m->base + m->layout->period_table + note * 2);
}

bool mon_read_song(const MonModule* m, uint32_t number, MonSong* out) {
    if (number >= m->subsong_count) {
        return false;
    }
    const uint32_t record = m->songs + number * m->layout->song_stride;
    for (uint32_t i = 0; i < MON_VOICES; i++) {
        out->tracks[i] = mon_u16(m, record + i * 2);
    }
    out->master_volume = mon_u8(m, record + 8);
    out->tempo = mon_u8(m, record + 10);
    return true;
}

MonInstrument mon_read_instrument(const MonModule* m, uint32_t address) {
    if (address == MON_NO_ADDRESS) {
        return (MonInstrument) { 0 };
    }
    return (MonInstrument) {
        .kind = mon_u8(m, address),
        .wave_ticks = mon_u8(m, address + 1),
        .restart_waves = (mon_u8(m, address + 1) & 1) != 0,
        .tuning = mon_u16(m, address + 2),
        .sample = mon_u32(m, address + 4),
        .wave_list = mon_u16(m, address + 4),
        .length_words = mon_u16(m, address + 8),
        .loop_start = mon_u16(m, address + 0xA),
        .envelope = mon_u16(m, address + 0xC),
        .volume = mon_u8(m, address + 0xE),
        .envelope_speed = mon_u8(m, address + 0xF),
        .vibrato = mon_u16(m, address + 0x10),
        .vibrato_delay = mon_u8(m, address + 0x12),
        .vibrato_speed = mon_u8(m, address + 0x13),
        .arpeggio = mon_u16(m, address + 0x14),
        .arpeggio_speed = mon_u8(m, address + 0x16),
    };
}

MonWave mon_read_wave(const MonModule* m, uint8_t index) {
    const uint32_t entry = m->layout->wave_table + (uint32_t)index * 4;
    return (MonWave) { .length_words = mon_u16(m, entry), .offset = mon_u16(m, entry + 2) };
}
