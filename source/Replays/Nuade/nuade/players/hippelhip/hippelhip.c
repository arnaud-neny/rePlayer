// SPDX-License-Identifier: MIT
// Jochen Hippel ".hip" player: the module check of the eagleplayer, the replayer versions, and the three engines
// behind one interface.
//
// Module: the replayer's 68k code (entry branches, play and init routines, period table, voice records), then
// Hippel's "TFMX" header and the song data: frequency sequences, volume sequences (instruments), patterns, tracks,
// subsongs, samples and the sample data. The tables follow each other; the header holds their sizes.

#include <nuade/players/hippelhip.h>

#include "hh_internal.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define TFMX_MAGIC 0x54464d58
#define ENTRY_MOVEM 0x48e7fffe // movem.l d0-d7/a0-a6,-(a7): the MCMD variant of the format starts like this
#define OPCODE_BRA_W 0x6000
#define OPCODE_BSR 0x61 // the high byte of bsr; a low byte of 0 is bsr.w (a word displacement follows), else bsr.b
#define OPCODE_LEA_PC_A0 0x41fa
#define OPCODE_PUSH_D0 0x2f00 // move.l d0,-(a7)
#define MAX_MODULE_SIZE 0x1000000

// The replayer versions: 12 layouts of the replayers in the corpus (23 distinct ones in 62 of the 63 modules; Astaroth
// has a replayer of its own design and is not played). A replayer is told by a hash of the module bytes in front of the
// header; replayers with the same layout differ in the song state the module file holds, or in code the port does not
// run (raster waits).

static const HhVersion k_versions[] = {
    // dragonflight_always, dragonflight_ani, dragonflight_dungeon, dragonflight_endsequence, dragonflight_fight, ...
    { .code_hashes = { 0xd29e12d8, 0x505076bc },
      .engine = &k_hh_engine_a,
      .start_sequence = 0x866,
      .period_table = 0xa04,
      .voice_records = 0x88a,
      .voices = 4 },
    // endless_piracy, wings_of_death_end, wings_of_death_intro, wings_of_death_outro
    { .code_hashes = { 0xd97197a1 },
      .engine = &k_hh_engine_a,
      .start_sequence = 0x866,
      .period_table = 0xa04,
      .voice_records = 0x88a,
      .voices = 4,
      .a_wait_skips_sweep = true },
    // crazy_comets, leaving_teramis_01, leaving_teramis_02, leaving_teramis_03, leaving_teramis_04, ...
    { .code_hashes = { 0x251016e7, 0x6a65a21c, 0x9e2af83e, 0xa525bb73, 0xae792aea, 0xf383cde7 },
      .engine = &k_hh_engine_b,
      .start_sequence = 0x826,
      .period_table = 0x994,
      .voice_records = 0x84a,
      .voices = 4,
      .shadow_before_header = 8,
      .init_flag_before_header = 6,
      .subsongs_before_header = 4 },
    // the_seven_gates_of_jambala_ingame_2
    { .code_hashes = { 0x91cb384a },
      .engine = &k_hh_engine_b,
      .start_sequence = 0x8fa,
      .period_table = 0xa68,
      .voice_records = 0x91e,
      .request_word = 0x116,
      .voices = 3,
      .shadow_before_header = 10,
      .init_flag_before_header = 8,
      .subsongs_before_header = 6,
      .effect_before_header = 2 },
    // warp_ingame_2
    { .code_hashes = { 0x6a2cc0a1 },
      .engine = &k_hh_engine_c,
      .start_sequence = 0x6d2,
      .period_table = 0x84e,
      .voice_records = 0x6fa,
      .voices = 2,
      .shadow_before_header = 8,
      .init_flag_before_header = 6,
      .subsongs_before_header = 4,
      .c_has_switch_command = true,
      .c_start_delay = 1 },
    // warp_ingame_1
    { .code_hashes = { 0x1f0cb02e },
      .engine = &k_hh_engine_c,
      .start_sequence = 0x710,
      .period_table = 0x83e,
      .voice_records = 0x738,
      .voices = 3,
      .shadow_before_header = 8,
      .init_flag_before_header = 6,
      .subsongs_before_header = 4,
      .c_has_switch_command = true,
      .c_start_delay = 0 },
    // cybernoid_2
    { .code_hashes = { 0x2dd8e928 },
      .engine = &k_hh_engine_c,
      .start_sequence = 0x720,
      .period_table = 0x89c,
      .voice_records = 0x748,
      .voices = 4,
      .shadow_before_header = 8,
      .init_flag_before_header = 6,
      .subsongs_before_header = 4,
      .c_has_switch_command = false,
      .c_start_delay = 1 },
    // battle_valley, comic_bakery, scoopmix, warp_title, warp_end
    { .code_hashes = { 0xc49cfa9e, 0xeb8efaf7, 0x843cf629, 0xf0414a95 },
      .engine = &k_hh_engine_c,
      .start_sequence = 0x772,
      .period_table = 0x8ee,
      .voice_records = 0x79a,
      .voices = 4,
      .shadow_before_header = 8,
      .init_flag_before_header = 6,
      .subsongs_before_header = 4,
      .c_has_switch_command = true,
      .c_start_delay = 1 },
    // prince: the code of battle_valley / comic_bakery assembled with short branches (0x24 bytes shorter)
    { .code_hashes = { 0x6437f42e },
      .engine = &k_hh_engine_c,
      .start_sequence = 0x74e,
      .period_table = 0x8ca,
      .voice_records = 0x776,
      .voices = 4,
      .shadow_before_header = 8,
      .init_flag_before_header = 6,
      .subsongs_before_header = 4,
      .c_has_switch_command = true,
      .c_start_delay = 1 },
    // 5th_gear, astaroth_2, hurricane-100bobs, shaolin_ingame_4, shaolin_title, ...
    { .code_hashes = { 0xa7424fa3, 0xbe61e2a, 0x7f34ed51 },
      .engine = &k_hh_engine_c,
      .start_sequence = 0x77a,
      .period_table = 0x8f6,
      .voice_records = 0x7a2,
      .voices = 4,
      .shadow_before_header = 8,
      .init_flag_before_header = 6,
      .subsongs_before_header = 4,
      .c_has_switch_command = true,
      .c_start_delay = 1 },
    // shaolin_ingame_1
    { .code_hashes = { 0x1bd3b22f },
      .engine = &k_hh_engine_c,
      .start_sequence = 0x836,
      .period_table = 0x9b2,
      .voice_records = 0x85e,
      .request_word = 0x10e,
      .voices = 3,
      .shadow_before_header = 10,
      .init_flag_before_header = 8,
      .subsongs_before_header = 6,
      .effect_before_header = 2,
      .c_has_switch_command = true,
      .c_start_delay = 1 },
    // shaolin_ingame_2
    { .code_hashes = { 0x3db1f1f0 },
      .engine = &k_hh_engine_c,
      .start_sequence = 0x83a,
      .period_table = 0x9b6,
      .voice_records = 0x862,
      .request_word = 0x10e,
      .voices = 3,
      .shadow_before_header = 10,
      .init_flag_before_header = 8,
      .subsongs_before_header = 6,
      .effect_before_header = 2,
      .c_has_switch_command = true,
      .c_start_delay = 1 },
};

// FNV-1a of the module bytes in front of the header
static uint32_t code_hash(const uint8_t* data, uint32_t length) {
    uint32_t hash = 0x811c9dc5u;
    for (uint32_t i = 0; i < length; i++) {
        hash = (hash ^ data[i]) * 0x01000193u;
    }
    return hash;
}

static const HhVersion* find_version(const uint8_t* data, uint32_t header) {
    const uint32_t hash = code_hash(data, header);
    for (size_t i = 0; i < nuade_sizeof_array(k_versions); i++) {
        for (int j = 0; j < HH_MAX_HASHES && k_versions[i].code_hashes[j] != 0; j++) {
            if (k_versions[i].code_hashes[j] == hash) {
                return &k_versions[i];
            }
        }
    }
    return nullptr;
}

// The eagleplayer's module check (with a bsr.b accepted where it wants a bsr.w), on the raw bytes. A read past the end
// fails the check.

typedef struct Bytes {
    const uint8_t* data;
    size_t size;
    bool ok;
} Bytes;

static uint8_t bytes_u8(Bytes* b, int64_t at) {
    if (at < 0 || (uint64_t)at >= b->size) {
        b->ok = false;
        return 0;
    }
    return b->data[at];
}

static uint16_t bytes_u16(Bytes* b, int64_t at) {
    return (uint16_t)((bytes_u8(b, at) << 8) | bytes_u8(b, at + 1));
}

static uint32_t bytes_u32(Bytes* b, int64_t at) {
    return ((uint32_t)bytes_u16(b, at) << 16) | bytes_u16(b, at + 2);
}

// A word that the replayer adds to an address with adda.w
static int64_t bytes_s16(Bytes* b, int64_t at) {
    return (int16_t)bytes_u16(b, at);
}

// Follows the bsr at *at to the routine it calls. The eagleplayer's check wants bsr.w; the replayer runs bsr.b just as
// well (Prince calls through two of them; the check refuses it, the user decided the port plays it). An odd bsr.b
// displacement would be an address error on the 68000.
static bool follow_bsr(Bytes* b, int64_t* at) {
    if (bytes_u8(b, *at) != OPCODE_BSR) {
        return false;
    }
    const int8_t displacement = (int8_t)bytes_u8(b, *at + 1);
    if (displacement == 0) {
        *at += 2 + bytes_s16(b, *at + 2);
        return true;
    }
    if ((displacement & 1) != 0) {
        return false;
    }
    *at += 2 + displacement;
    return true;
}

// From the first bytes to the "TFMX" header: the entry branch, the init routine's movem, then (when the entry is a
// short branch) two calls, and the lea of the header.
static bool find_header(Bytes* b, int64_t* header) {
    if (bytes_u32(b, 0) == ENTRY_MOVEM || bytes_u8(b, 0) != 0x60) {
        return false;
    }
    const uint8_t displacement = bytes_u8(b, 1);
    int64_t at;
    if (displacement == 0) {
        const int64_t target = bytes_s16(b, 2);
        if (target < 0 || (target & 1) != 0 || bytes_u16(b, 4) != OPCODE_BRA_W) {
            return false;
        }
        at = 2 + target;
        if (bytes_u32(b, at) != ENTRY_MOVEM) {
            return false;
        }
        at += 4;
    } else {
        if ((displacement & 1) != 0) {
            return false;
        }
        at = 2 + displacement;
        if (bytes_u32(b, at) != ENTRY_MOVEM) {
            return false;
        }
        at += 4;
        if (!follow_bsr(b, &at) || bytes_u16(b, at) != OPCODE_PUSH_D0) {
            return false;
        }
        at += 2;
        if (!follow_bsr(b, &at)) {
            return false;
        }
        if (bytes_u16(b, at) != OPCODE_LEA_PC_A0) {
            return false;
        }
        at += 0x14;
    }
    // the lea of the header: at the first or at the second word
    if (bytes_u16(b, at) != OPCODE_LEA_PC_A0) {
        at += 2;
        if (bytes_u16(b, at) != OPCODE_LEA_PC_A0) {
            return false;
        }
    }
    at += 2;
    *header = at + bytes_s16(b, at);
    return b->ok && bytes_u32(b, *header) == TFMX_MAGIC;
}

// The header behind "TFMX": how many of each record the tables hold
typedef struct TfmxHeader {
    uint8_t fseq_high;     // the high byte of the first count, which the check wants 0
    uint16_t fseq_last;    // frequency sequences - 1
    uint16_t vseq_last;    // volume sequences - 1
    uint16_t pattern_last; // patterns - 1
    uint16_t track_last;   // track positions - 1
    uint16_t pattern_size; // bytes
    uint16_t subsong_last; // subsong table entries - 1, which is the number of the last subsong
    uint16_t sample_count;
} TfmxHeader;

static TfmxHeader read_tfmx_header(Bytes* b, int64_t header) {
    return (TfmxHeader) { .fseq_high = bytes_u8(b, header + 4),
                          .fseq_last = bytes_u16(b, header + 4),
                          .vseq_last = bytes_u16(b, header + 6),
                          .pattern_last = bytes_u16(b, header + 8),
                          .track_last = bytes_u16(b, header + 0xa),
                          .pattern_size = bytes_u16(b, header + 0xc),
                          .subsong_last = bytes_u16(b, header + 0x10),
                          .sample_count = bytes_u16(b, header + 0x12) };
}

// A sample table entry, as far as the check looks
typedef struct SampleEntry {
    uint32_t start;  // into the sample data
    uint16_t length; // words
} SampleEntry;

static SampleEntry read_sample_entry(Bytes* b, int64_t entry) {
    return (SampleEntry) { .start = bytes_u32(b, entry + 0x12), .length = bytes_u16(b, entry + 0x16) };
}

// The header's own check: the sample table starts with a sample at 0 that has a length, and the second sample
// starts where the first ends.
static bool check_header(Bytes* b, int64_t header, const TfmxHeader* h) {
    if (h->fseq_high != 0 || h->subsong_last == 0) {
        return false;
    }
    uint32_t extent = (uint32_t)(uint16_t)(2 + h->fseq_last + h->vseq_last) * HH_SEQUENCE_RECORD_SIZE;
    extent += (uint32_t)(uint16_t)(1 + h->pattern_last) * h->pattern_size;
    extent += (uint32_t)(uint16_t)(1 + h->track_last) * HH_TRACK_ROW_SIZE;
    const uint32_t subsongs = (uint32_t)(uint16_t)(1 + h->subsong_last) * HH_SUBSONG_RECORD_SIZE;
    extent += subsongs + HH_HEADER_SIZE;
    const int64_t table = header + extent;
    const SampleEntry first = read_sample_entry(b, table);
    const SampleEntry second = read_sample_entry(b, table + HH_SAMPLE_RECORD_SIZE);
    if (first.start != 0 || first.length == 0) {
        return false;
    }
    return b->ok && second.start == (uint32_t)first.length * 2;
}

// A table follows the one before; the replayer adds the low word of the size, sign extended (adda.w)
static uint32_t table_end(uint32_t table, uint32_t size) {
    return table + (uint32_t)(int32_t)(int16_t)(uint16_t)size;
}

// The header's table sizes give the offset of every table
static void layout_tables(const TfmxHeader* h, HhModule* m) {
    m->pattern_size = h->pattern_size;
    m->fseqs = m->header + HH_HEADER_SIZE;
    m->vseqs = table_end(m->fseqs, (uint16_t)((h->fseq_last + 1) * HH_SEQUENCE_RECORD_SIZE));
    m->patterns = table_end(m->vseqs, (uint16_t)((h->vseq_last + 1) * HH_SEQUENCE_RECORD_SIZE));
    m->tracks = table_end(m->patterns, (uint32_t)(uint16_t)(h->pattern_last + 1) * h->pattern_size);
    m->subsongs = table_end(m->tracks, (uint32_t)(uint16_t)(h->track_last + 1) * HH_TRACK_ROW_SIZE);
    m->samples = table_end(m->subsongs, (uint32_t)(uint16_t)(h->subsong_last + 1) * HH_SUBSONG_RECORD_SIZE);
    m->sample_data = table_end(m->samples, (uint32_t)h->sample_count * HH_SAMPLE_RECORD_SIZE);
    m->subsong_count = h->subsong_last;
}

// The module as the replayer is given it, if this player runs its replayer: the module's layout (without its data)
// and the replayer version.
static const HhVersion* parse(const uint8_t* data, size_t size, HhModule* module) {
    if (data == nullptr || size < 0x40 || size > MAX_MODULE_SIZE) {
        return nullptr;
    }
    Bytes b = { .data = data, .size = size, .ok = true };
    int64_t header;
    if (!find_header(&b, &header)) {
        return nullptr;
    }
    const TfmxHeader tfmx = read_tfmx_header(&b, header);
    if (!check_header(&b, header, &tfmx)) {
        return nullptr;
    }
    const HhVersion* version = find_version(data, (uint32_t)header);
    if (version == nullptr) {
        return nullptr;
    }
    // the sound effect request word must be clear: the play routine would start an effect
    if (version->request_word != 0 && bytes_u16(&b, version->request_word) != 0) {
        return nullptr;
    }
    if (version->effect_before_header != 0 && bytes_u16(&b, header - version->effect_before_header) != 0) {
        return nullptr;
    }
    *module = (HhModule) { .size = (uint32_t)size, .header = (uint32_t)header };
    layout_tables(&tfmx, module);
    // the tables must be in the module
    if (!b.ok || module->samples > size) {
        return nullptr;
    }
    return version;
}

bool hh_probe(const uint8_t* data, size_t size) {
    HhModule module;
    return parse(data, size, &module) != nullptr;
}

HhPlayer* hh_create(const uint8_t* data, size_t size) {
    HhModule module;
    const HhVersion* version = parse(data, size, &module);
    if (version == nullptr) {
        return nullptr;
    }
    HhPlayer* p = calloc(1, sizeof(*p));
    uint8_t* original = malloc(size);
    uint8_t* work = nuade_paula_padded_copy(nullptr, size);
    if (p == nullptr || original == nullptr || work == nullptr) {
        free(p);
        free(original);
        free(work);
        return nullptr;
    }
    memcpy(original, data, size);
    p->original = original;
    p->version = version;
    p->module = module;
    p->module.data = work;
    memcpy(work, original, size);
    return p;
}

void hh_destroy(HhPlayer* player) {
    if (player != nullptr) {
        free(player->original);
        free(player->module.data);
        free(player);
    }
}

int hh_subsong_count(const HhPlayer* player) {
    return player->module.subsong_count;
}

const char* hh_replayer_name(const HhPlayer* player) {
    return player->version->engine->name;
}

// The eagleplayer starts a subsong: a table entry that ends before it starts is cleared, then the replayer's init
// reads it.
static void load_subsong(HhPlayer* p) {
    HhModule* m = &p->module;
    HhSong* song = &p->song;
    const uint32_t entry = m->subsongs + (uint32_t)(song->sndnum - 1) * HH_SUBSONG_RECORD_SIZE;
    if ((int16_t)(hhm_u16(m, entry + 2) - hhm_u16(m, entry)) < 0 && m->size >= 4 && entry <= m->size - 4) {
        memset(m->data + entry, 0, 4);
    }
    song->subsong
        = (HhSubsong) { .first = hhm_u16(m, entry), .last = hhm_u16(m, entry + 2), .speed = hhm_u16(m, entry + 4) };
}

bool hh_start(HhPlayer* player, int subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || subsong >= player->module.subsong_count) {
        return false;
    }
    HhModule* m = &player->module;
    memcpy(m->data, player->original, m->size);
    player->song = (HhSong) { .sndnum = subsong + 1 };
    for (int i = 0; i < HH_VOICES; i++) {
        nuade_voice_stop(voices, (uint32_t)i);
    }
    for (int i = 0; i < HH_VOICES; i++) {
        player->song.voices[i].note = hh_file_note(m, player->version, i);
    }
    load_subsong(player);
    hh_song_start(&player->song, m, player->version);
    return true;
}

HhTickResult hh_tick(HhPlayer* player, NuadeVoices* voices) {
    HhSong* song = &player->song;
    song->looped = false;
    hh_song_tick(song, &player->module, player->version, voices);
    return (HhTickResult) { .looped = song->looped };
}

int hh_speed(const HhPlayer* player) {
    return player->song.reload;
}

HhVoiceState hh_voice_state(const HhPlayer* player, int voice) {
    if (voice < 0 || voice >= HH_VOICES) {
        return (HhVoiceState) { .note = -1, .instrument = -1 };
    }
    const HhVoice* v = &player->song.voices[voice];
    const bool has_note = v->last_instrument >= 0 && v->note != 0;
    return (HhVoiceState) { .note = has_note ? (v->note + v->note_transpose) & 0x7f : -1,
                            .instrument = v->last_instrument };
}
