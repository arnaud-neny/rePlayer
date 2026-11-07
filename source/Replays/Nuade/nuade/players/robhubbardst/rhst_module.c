// SPDX-License-Identifier: MIT
// Rob Hubbard ST modules: what the UADE eagleplayer "RobHubbard_ST" checks and patches, and which replayer builds the
// port reproduces.
//
// A module is an Atari ST replayer made to drive Paula: the code is in the module, the eagleplayer finds its entry
// points by scanning for instruction patterns and calls them. The eagleplayer's Check2 tests the first two longwords
// (the waveforms of the first two channels) and the first instruction of the hardware init at 0x38.
//
// Three builds exist (one per module); each is recognised by a hash of the module bytes that are code and tables. The
// song data behind the tables is not hashed: the port walks every stream the replayer can reach and refuses the module
// when the replayer would read anything the port does not model.

#include "rhst_module.h"

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>

#define HEADER_LONG0 0x00407F40u // Check2: the first waveform
#define HEADER_LONG1 0x00C081C0u
#define INIT_LEA_AT 0x38u // Check2: `lea.l -$12(pc),a0` of the hardware init
#define INIT_LEA 0x41FAFFEEu
#define CODE_START 0x38u
#define FIRST_INSTRUMENT_SCAN 0xE548D1F0u // InitPlayer scans for this longword from the module start, even offsets
#define SONG_RECORD_BYTES 12u             // three track list offsets
#define SONG_LIMIT_SIGNED 2730u // the first build indexes the song table with a signed word: song * 12 + 8 < 0x8000
#define WALK_BUDGET (1u << 22)  // list entries, stream bytes and envelope bytes the open-time walk may read
#define LIST_LIMIT (1u << 16)   // entries of one track list
#define ENVELOPE_LIMIT 4096u    // bytes of one semitone list
#define PORTAMENTO_MARGIN 32    // semitones a held note can drift: at most the 31 holds of the longest note
#define RECORD_SIZE 0x66u       // bytes of a voice record in the module
#define LOAD_ADDRESS 0x10000u   // where UADE loads the module; the records keep absolute addresses

// Offsets in a voice record
typedef enum RecordField {
    RecordField_Flags = 0x00,
    RecordField_Stream = 0x04,
    RecordField_TrackList = 0x08,
    RecordField_TrackPos = 0x0C,
    RecordField_SlideTotal = 0x10,
    RecordField_EnvStart = 0x14,
    RecordField_EnvAt = 0x18,
    RecordField_SlideStep = 0x1C,
    RecordField_SlideDelay = 0x20,
    RecordField_StepCount = 0x28,
    RecordField_StepLength = 0x2C,
    RecordField_Note = 0x30,
    RecordField_Volume = 0x34,
    RecordField_AttackInit = 0x38,
    RecordField_DecayInit = 0x3C,
    RecordField_Rate = 0x40,
    RecordField_PhaseInit = 0x44,
    RecordField_PhaseLeft = 0x48,
    RecordField_StartVolume = 0x4C,
    RecordField_VibDepth = 0x50,
    RecordField_VibStep = 0x54,
    RecordField_VibPos = 0x58,
    RecordField_VibFlags = 0x5C,
    RecordField_NoiseTrigger = 0x60,
    RecordField_MixerMask = 0x64,
} RecordField;

// The builds

// Patches of battleships.rho, goldrunner.rho and jupiterprobe.rho, as found in UADE's memory after InitPlayer
static const RhstPatch s_patches_a[] = {
    { 0x10e, 2, { 0x4e, 0xb9 } },
    { 0x111, 3, { 0x00, 0xd6, 0xa4 } },
    { 0x120, 2, { 0x4e, 0xb9 } },
    { 0x123, 3, { 0x00, 0xd6, 0xba } },
    { 0x130, 2, { 0x4e, 0xb9 } },
    { 0x133, 3, { 0x00, 0xd6, 0x1e } },
    { 0x164, 2, { 0x4e, 0xb9 } },
    { 0x167,
      17,
      { 0x00, 0xd6, 0x42, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71 } },
    { 0x184, 2, { 0x4e, 0xb9 } },
    { 0x187, 3, { 0x00, 0xd6, 0x6a } },
    { 0x196, 2, { 0x4e, 0xb9 } },
    { 0x199, 3, { 0x00, 0xd6, 0x80 } },
    { 0x1ce, 2, { 0x4e, 0xb9 } },
    { 0x1d1, 3, { 0x00, 0xd6, 0xba } },
    { 0x200, 2, { 0x4e, 0xb9 } },
    { 0x203, 3, { 0x00, 0xd6, 0x1e } },
    { 0x274, 2, { 0x4e, 0xb9 } },
    { 0x277, 7, { 0x00, 0xd6, 0xe0, 0x4e, 0x71, 0x4e, 0x71 } },
    { 0x2b6, 2, { 0xff, 0xf8 } },
    { 0x3d0, 2, { 0xff, 0xf6 } },
    { 0x450, 2, { 0x4e, 0xb9 } },
    { 0x454, 2, { 0xd5, 0xb8 } },
    { 0x46a,
      18,
      { 0x4e, 0xb9, 0x00, 0x00, 0xd5, 0xdc, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71 } },
};

static const RhstPatch s_patches_b[] = {
    { 0x116, 2, { 0x4e, 0xb9 } },
    { 0x119, 3, { 0x00, 0xd6, 0xa4 } },
    { 0x128, 2, { 0x4e, 0xb9 } },
    { 0x12b, 3, { 0x00, 0xd6, 0xba } },
    { 0x140, 2, { 0x4e, 0xb9 } },
    { 0x143, 3, { 0x00, 0xd6, 0x1e } },
    { 0x174, 2, { 0x4e, 0xb9 } },
    { 0x177,
      17,
      { 0x00, 0xd6, 0x42, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71 } },
    { 0x194, 2, { 0x4e, 0xb9 } },
    { 0x197, 3, { 0x00, 0xd6, 0x6a } },
    { 0x1a6, 2, { 0x4e, 0xb9 } },
    { 0x1a9, 3, { 0x00, 0xd6, 0x80 } },
    { 0x1d8, 2, { 0x4e, 0xb9 } },
    { 0x1db, 3, { 0x00, 0xd6, 0xba } },
    { 0x20a, 2, { 0x4e, 0xb9 } },
    { 0x20d, 3, { 0x00, 0xd6, 0x1e } },
    { 0x27c, 2, { 0x4e, 0xb9 } },
    { 0x27f, 7, { 0x00, 0xd6, 0xe0, 0x4e, 0x71, 0x4e, 0x71 } },
    { 0x2ba, 2, { 0xff, 0xf8 } },
    { 0x3e2, 2, { 0xff, 0xf6 } },
    { 0x462, 2, { 0x4e, 0xb9 } },
    { 0x466, 2, { 0xd5, 0xb8 } },
    { 0x47c,
      18,
      { 0x4e, 0xb9, 0x00, 0x00, 0xd5, 0xdc, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71 } },
};

static const RhstPatch s_patches_c[] = {
    { 0x102, 2, { 0x4e, 0xb9 } },
    { 0x105, 3, { 0x00, 0xd6, 0xa4 } },
    { 0x114, 2, { 0x4e, 0xb9 } },
    { 0x117, 3, { 0x00, 0xd6, 0xba } },
    { 0x128, 2, { 0x4e, 0xb9 } },
    { 0x12b, 3, { 0x00, 0xd6, 0x1e } },
    { 0x140, 2, { 0xfe, 0xf4 } },
    { 0x15c, 2, { 0x4e, 0xb9 } },
    { 0x15f,
      17,
      { 0x00, 0xd6, 0x42, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71 } },
    { 0x17c, 2, { 0x4e, 0xb9 } },
    { 0x17f, 3, { 0x00, 0xd6, 0x6a } },
    { 0x18e, 2, { 0x4e, 0xb9 } },
    { 0x191, 3, { 0x00, 0xd6, 0x80 } },
    { 0x1f2, 6, { 0x4e, 0xb9, 0x00, 0x00, 0xd6, 0x16 } },
    { 0x21a, 2, { 0x4e, 0x71 } },
    { 0x21e, 6, { 0x4e, 0xb9, 0x00, 0x00, 0xd6, 0x16 } },
    { 0x256, 2, { 0x4e, 0xb9 } },
    { 0x259, 7, { 0x00, 0xd6, 0xe0, 0x4e, 0x71, 0x4e, 0x71 } },
    { 0x28c, 2, { 0x4e, 0x71 } },
    { 0x296, 2, { 0xff, 0xf8 } },
    { 0x374, 2, { 0xff, 0xf6 } },
    { 0x3f4, 2, { 0x4e, 0xb9 } },
    { 0x3f8, 2, { 0xd5, 0xb8 } },
    { 0x40e,
      18,
      { 0x4e, 0xb9, 0x00, 0x00, 0xd5, 0xdc, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71, 0x4e, 0x71 } },
};

// battleships.rho, goldrunner.rho, jupiterprobe.rho
static const RhstBuild s_builds[] = {
    {
        .code_hash = 0xb6f4b889bce73553ULL,
        .periods = 0x6c8,
        .instruments = 0x788,
        .commands = 0x7a8,
        .records = 0x7f0,
        .data = 0x922,
        .noise = 0x19c,
        .noise_words = 0x64,
        .output = RhstOutput_Idle,
        .plays_subsong = true,
        .clears_records = false,
        .patches = s_patches_a,
        .patch_count = sizeof(s_patches_a) / sizeof(s_patches_a[0]),
    },
    {
        .code_hash = 0x194ce4f6734f48a6ULL,
        .periods = 0x6da,
        .instruments = 0x79a,
        .commands = 0x7ba,
        .records = 0x802,
        .data = 0x934,
        .noise = 0xca,
        .noise_words = 0x64,
        .output = RhstOutput_Gated,
        .plays_subsong = false,
        .clears_records = false,
        .patches = s_patches_b,
        .patch_count = sizeof(s_patches_b) / sizeof(s_patches_b[0]),
    },
    {
        .code_hash = 0x29e00a109c6f27caULL,
        .periods = 0x66c,
        .instruments = 0x72c,
        .commands = 0x78a,
        .records = 0x7d2,
        .data = 0x908,
        .noise
        = 0x34, // the eagleplayer redirects the lea of the noise sample (0x140); the original points below the module
        .noise_words = 0x3e8,
        .output = RhstOutput_Always,
        .plays_subsong = false,
        .clears_records = true,
        .patches = s_patches_c,
        .patch_count = sizeof(s_patches_c) / sizeof(s_patches_c[0]),
    },
};

// The mixer bits of the three voices in the module files
static const uint8_t s_file_masks[RHST_RECORD_COUNT] = { 0x09, 0x12, 0x24 };

// Module reads

uint8_t rhst_module_u8(const RhstModule* module, uint32_t offset) {
    return offset < module->size ? module->image[offset] : 0;
}

uint16_t rhst_module_u16(const RhstModule* module, uint32_t offset) {
    if (module->size < 2 || offset > module->size - 2) {
        return 0;
    }
    return (uint16_t)((module->image[offset] << 8) | module->image[offset + 1]);
}

uint32_t rhst_module_u32(const RhstModule* module, uint32_t offset) {
    if (module->size < 4 || offset > module->size - 4) {
        return 0;
    }
    return ((uint32_t)module->image[offset] << 24) | ((uint32_t)module->image[offset + 1] << 16)
           | ((uint32_t)module->image[offset + 2] << 8) | module->image[offset + 3];
}

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t fnv1a(uint64_t hash, const uint8_t* bytes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        hash = (hash ^ bytes[i]) * 0x100000001B3ULL;
    }
    return hash;
}

static const RhstBuild* find_build(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < sizeof(s_builds) / sizeof(s_builds[0]); i++) {
        const RhstBuild* b = &s_builds[i];
        if (size < b->data + SONG_RECORD_BYTES) {
            continue;
        }
        uint64_t hash = 0xCBF29CE484222325ULL;
        hash = fnv1a(hash, data + CODE_START, b->instruments - CODE_START);
        hash = fnv1a(hash, data + b->commands, b->records - b->commands);
        if (hash == b->code_hash) {
            return b;
        }
    }
    return nullptr;
}

// Song table

// The first longword of the song table is the size of the table: three lists per song, one longword each.
static uint32_t song_count_of(const uint8_t* data, size_t size, const RhstBuild* b) {
    const uint32_t table_bytes = be32(data + b->data);
    if (table_bytes % SONG_RECORD_BYTES != 0 || table_bytes / SONG_RECORD_BYTES == 0
        || table_bytes > 0xFFFF * SONG_RECORD_BYTES) {
        return 0;
    }
    const uint32_t songs = table_bytes / SONG_RECORD_BYTES;
    if (b->plays_subsong && songs > SONG_LIMIT_SIGNED) {
        return 0;
    }
    return size - b->data >= table_bytes ? songs : 0;
}

uint32_t rhst_module_track_list(const RhstModule* module, uint32_t song, uint32_t voice) {
    const uint32_t entry = rhst_module_u32(module, module->data + 4 * (song * RHST_RECORD_COUNT + voice));
    return module->data + entry;
}

uint32_t rhst_module_played_song(const RhstModule* module, uint32_t subsong) {
    return module->build->plays_subsong ? subsong : 0;
}

uint32_t rhst_module_longest_list(const RhstModule* module, uint32_t subsong) {
    uint32_t best = 0;
    uint32_t best_length = 0;
    for (uint32_t voice = 0; voice < RHST_RECORD_COUNT; voice++) {
        const uint32_t length = module->list_counts[subsong * RHST_RECORD_COUNT + voice];
        if (length >= best_length) {
            best = rhst_module_track_list(module, subsong, voice);
            best_length = length;
        }
    }
    return best;
}

uint32_t rhst_module_instrument(const RhstModule* module, uint32_t index) {
    const uint32_t base = module->build->instruments;
    return base + rhst_module_u32(module, base + 4 * index);
}

// What the replayer can reach

typedef struct Walk {
    const RhstModule* module;
    uint32_t budget; // one budget for the whole module
} Walk;

typedef struct VoiceWalk {
    bool duration_set;  // a length command ran: notes and rests hold for 1..32 steps instead of 65536
    bool porta_pending; // a portamento command of the step that is not over yet
    bool porta_used;
    bool any_note;
    bool any_step;          // a note or a rest ended a step
    uint32_t bytes_in_step; // stream bytes the replayer reads since the last note or rest
    uint8_t max_note;
    bool envelope_used[RHST_INSTRUMENT_COUNT];
    bool envelope_ok[RHST_INSTRUMENT_COUNT];
    uint8_t envelope_max[RHST_INSTRUMENT_COUNT]; // the largest semitone offset of the envelope
} VoiceWalk;

static bool take_budget(Walk* w) {
    if (w->budget == 0) {
        return false;
    }
    w->budget--;
    return true;
}

static bool in_records(const RhstModule* m, uint32_t offset) {
    const uint32_t first = m->build->records;
    return offset >= first && offset < first + RHST_RECORD_COUNT * RECORD_SIZE;
}

// An envelope is a list of semitone offsets that ends with 0x87 and then starts over. It must lie in the static part of
// the module and hold at least one offset (the replayer takes the first byte as an offset without testing it); its
// largest offset bounds the period table reads.
static bool scan_envelope(Walk* w, uint32_t at, uint8_t* max_value) {
    const RhstModule* m = w->module;
    *max_value = 0;
    for (uint32_t n = 0; n < ENVELOPE_LIMIT; n++) {
        if (at >= m->size || in_records(m, at) || !take_budget(w)) {
            return false;
        }
        const uint8_t b = m->image[at++];
        if (b == RHST_LIST_END) {
            return n > 0;
        }
        if (b > *max_value) {
            *max_value = b;
        }
    }
    return false;
}

static bool use_instrument(Walk* w, VoiceWalk* vw, uint32_t index) {
    if (index >= RHST_INSTRUMENT_COUNT) {
        return false;
    }
    if (!vw->envelope_used[index]) {
        vw->envelope_used[index] = true;
        vw->envelope_ok[index] = scan_envelope(w, rhst_module_instrument(w->module, index), &vw->envelope_max[index]);
    }
    return vw->envelope_ok[index];
}

// A note or a rest ends a step of the replayer.
static bool end_step(VoiceWalk* vw) {
    if (!vw->duration_set) {
        return false;
    }
    vw->bytes_in_step = 0;
    vw->any_step = true;
    return true;
}

// One stream, from its first byte to the command that leaves it (next track, stop). A step may go on into the next
// stream, so the bytes of a step are counted across streams.
static bool walk_stream(Walk* w, VoiceWalk* vw, uint32_t at) {
    const RhstModule* m = w->module;
    for (;;) {
        if (at >= m->size || !take_budget(w) || ++vw->bytes_in_step > RHST_STEP_LIMIT) {
            return false;
        }
        const uint8_t b = m->image[at++];
        if (b < RHST_NOTE_END) {
            if (!end_step(vw)) {
                return false;
            }
            vw->porta_pending = false;
            vw->any_note = true;
            if (b > vw->max_note) {
                vw->max_note = b;
            }
        } else if (b <= RHST_COMMAND_LAST) {
            switch (b & RHST_COMMAND_MASK) {
                case RhstCommand_Rest:
                case RhstCommand_RestKeep:
                    // A rest keeps the pitch of the note before; a portamento there would drift without bound
                    if (vw->porta_pending || !end_step(vw)) {
                        return false;
                    }
                    break;
                case RhstCommand_Slide:
                case RhstCommand_Vibrato:
                    at += 2;
                    break;
                case RhstCommand_NoiseFreq:
                    at += 1;
                    break;
                case RhstCommand_PortamentoDown:
                case RhstCommand_PortamentoUp:
                    vw->porta_pending = true;
                    vw->porta_used = true;
                    break;
                case RhstCommand_NextTrack:
                    return true;
                case RhstCommand_Stop:
                    // Nothing runs after a stop: the step state does not carry on
                    vw->bytes_in_step = 0;
                    vw->porta_pending = false;
                    return true;
                case RhstCommand_VibratoOff:
                case RhstCommand_VibratoFull:
                case RhstCommand_VibratoHalf:
                case RhstCommand_ToneOnly:
                case RhstCommand_NoiseOnly:
                case RhstCommand_ToneAndNoise:
                case RhstCommand_Percussion:
                case RhstCommand_Tie:
                case RhstCommand_NoiseTrigger:
                    break;
                default:
                    return false; // past the jump table
            }
        } else if (b < RHST_INSTRUMENT_FIRST) {
            // tempo
        } else if (b < RHST_ENVELOPE_FIRST) {
            if (!use_instrument(w, vw, b - RHST_INSTRUMENT_FIRST)) {
                return false;
            }
        } else if (b < RHST_LENGTH_FIRST) {
            at += 3;
        } else {
            vw->duration_set = true;
        }
    }
}

// Every note index the voice can reach must read module bytes that never change: the period table, or the tables
// behind it (a note past the table reads them as periods). The voice records change.
static bool periods_in_range(const RhstModule* m, const VoiceWalk* vw) {
    if (!vw->any_note) {
        return true;
    }
    uint32_t env_max = 0;
    for (uint32_t i = 0; i < RHST_INSTRUMENT_COUNT; i++) {
        if (vw->envelope_used[i] && vw->envelope_max[i] > env_max) {
            env_max = vw->envelope_max[i];
        }
    }
    const uint32_t margin = vw->porta_used ? PORTAMENTO_MARGIN : 0;
    const uint32_t top = vw->max_note + env_max + margin;
    return 2 * margin <= m->periods && m->periods + 2 * top + 2 <= m->build->records;
}

// The entries of a track list: the longwords up to a zero, which must lie inside the module and the budget.
static bool list_entries(Walk* w, uint32_t list, uint32_t* count) {
    const RhstModule* m = w->module;
    if (list < m->data || list >= m->size) {
        return false;
    }
    for (uint32_t n = 0; n < LIST_LIMIT; n++) {
        if (4 * n > m->size - list || m->size - list - 4 * n < 4 || !take_budget(w)) {
            return false;
        }
        if (rhst_module_u32(m, list + 4 * n) == 0) {
            *count = n;
            return true;
        }
    }
    return false;
}

// A voice plays its list, then starts it over. A step can span the end of the list into its start, so the list is
// walked twice with the state of the first pass carried into the second.
static bool walk_voice(Walk* w, uint32_t song, uint32_t voice) {
    const RhstModule* m = w->module;
    VoiceWalk vw = { 0 };
    // The voice starts on the first instrument
    if (!use_instrument(w, &vw, 0)) {
        return false;
    }
    const uint32_t list = rhst_module_track_list(m, song, voice);
    const uint32_t count = m->list_counts[song * RHST_RECORD_COUNT + voice];
    if (count == 0) {
        return false;
    }
    for (uint32_t pass = 0; pass < 2; pass++) {
        for (uint32_t i = 0; i < count; i++) {
            const uint32_t entry = rhst_module_u32(m, list + 4 * i);
            if (entry >= m->size - m->data || !walk_stream(w, &vw, m->data + entry)) {
                return false;
            }
        }
        // A list that never ends a step would read commands for ever
        if (!vw.any_step) {
            return false;
        }
    }
    return periods_in_range(m, &vw);
}

// The eagleplayer's InitSound reads the lists of every song; all must end inside the module.
static bool lists_terminate(Walk* w, uint32_t* counts) {
    for (uint32_t song = 0; song < w->module->song_count; song++) {
        for (uint32_t voice = 0; voice < RHST_RECORD_COUNT; voice++) {
            if (!list_entries(w, rhst_module_track_list(w->module, song, voice),
                              &counts[song * RHST_RECORD_COUNT + voice])) {
                return false;
            }
        }
    }
    return true;
}

static bool streams_are_modelled(RhstModule* m) {
    Walk w = { .module = m, .budget = WALK_BUDGET };
    m->list_counts = (uint32_t*)calloc((size_t)m->song_count * RHST_RECORD_COUNT, sizeof(uint32_t));
    if (m->list_counts == nullptr || !lists_terminate(&w, m->list_counts)) {
        return false;
    }
    const uint32_t songs = m->build->plays_subsong ? m->song_count : 1;
    for (uint32_t song = 0; song < songs; song++) {
        for (uint32_t voice = 0; voice < RHST_RECORD_COUNT; voice++) {
            if (!walk_voice(&w, song, voice)) {
                return false;
            }
        }
    }
    return true;
}

// Voice records

static void put_u16(uint8_t* record, RecordField field, uint16_t value) {
    record[field] = (uint8_t)(value >> 8);
    record[field + 1] = (uint8_t)value;
}

static void put_u32(uint8_t* record, RecordField field, uint32_t value) {
    put_u16(record, field, (uint16_t)(value >> 16));
    put_u16(record, (RecordField)(field + 2), (uint16_t)value);
}

void rhst_module_store_voice(RhstModule* module, uint32_t index, const RhstVoice* v) {
    if (!module->noise_reads_records) {
        return;
    }
    uint8_t* record = module->image + module->build->records + index * RECORD_SIZE;
    record[RecordField_Flags] = (uint8_t)((v->toggle ? 0x01 : 0) | (v->percussion ? 0x02 : 0) | (v->slide ? 0x04 : 0)
                                          | (v->portamento ? 0x08 : 0) | (v->tie ? 0x10 : 0) | (v->envelope ? 0x20 : 0)
                                          | (v->attack ? 0x40 : 0) | (v->porta_up ? 0x80 : 0));
    put_u32(record, RecordField_Stream, LOAD_ADDRESS + v->stream);
    put_u32(record, RecordField_TrackList, LOAD_ADDRESS + v->track_list);
    put_u32(record, RecordField_TrackPos, v->track_pos);
    put_u16(record, RecordField_SlideTotal, v->slide_total);
    put_u32(record, RecordField_EnvStart, LOAD_ADDRESS + v->env_start);
    put_u32(record, RecordField_EnvAt, LOAD_ADDRESS + v->env_at);
    put_u16(record, RecordField_SlideStep, (uint16_t)v->slide_step);
    put_u16(record, RecordField_SlideDelay, v->slide_delay);
    put_u16(record, RecordField_StepCount, v->step_count);
    put_u16(record, RecordField_StepLength, v->step_length);
    put_u16(record, RecordField_Note, v->note);
    record[RecordField_Volume] = v->volume;
    record[RecordField_AttackInit] = v->attack_init;
    record[RecordField_DecayInit] = v->decay_init;
    record[RecordField_Rate] = v->rate;
    record[RecordField_PhaseInit] = v->phase_init;
    record[RecordField_PhaseLeft] = v->phase_left;
    record[RecordField_StartVolume] = v->start_volume;
    put_u16(record, RecordField_VibDepth, v->vib_depth);
    put_u16(record, RecordField_VibStep, v->vib_step);
    put_u16(record, RecordField_VibPos, v->vib_pos);
    record[RecordField_VibFlags]
        = (uint8_t)((v->vib_on ? 0x40 : 0) | (v->vib_half ? 0x80 : 0) | (v->vib_rising ? 0x20 : 0));
    record[RecordField_NoiseTrigger] = v->noise_trigger ? 1 : 0;
    record[RecordField_MixerMask] = v->mixer_mask;
}

// Open

// The voice records of the file are zero but for the mixer bits (the init of the third build clears them anyway).
static bool records_are_known(const uint8_t* data, const RhstBuild* b) {
    if (b->clears_records) {
        return true;
    }
    for (uint32_t v = 0; v < RHST_RECORD_COUNT; v++) {
        for (uint32_t i = 0; i < RECORD_SIZE; i++) {
            const uint8_t expected = i == RecordField_MixerMask ? s_file_masks[v] : 0;
            if (data[b->records + v * RECORD_SIZE + i] != expected) {
                return false;
            }
        }
    }
    return true;
}

// InitPlayer finds the semitone list the voices start on: it scans for a longword from the module start (even offsets)
// and follows two offsets from the first match. It must be the first instrument, where the build keeps it.
static bool first_instrument_is_known(const uint8_t* data, size_t size, const RhstBuild* b) {
    size_t at = 0;
    while (at + 4 <= size && be32(data + at) != FIRST_INSTRUMENT_SCAN) {
        at += 2;
    }
    if (at < 2 || at + 4 > size) {
        return false;
    }
    const size_t word_at = at - 2;
    const int32_t offset = (int16_t)((data[word_at] << 8) | data[word_at + 1]);
    const int64_t table = (int64_t)word_at + offset;
    if (table < 0 || (uint64_t)table > size - 4) {
        return false;
    }
    const uint64_t first = (uint64_t)table + be32(data + table);
    return first == (uint64_t)b->instruments + be32(data + b->instruments);
}

void rhst_module_close(RhstModule* module) {
    if (module != nullptr) {
        free(module->image);
        free(module->list_counts);
    }
    free(module);
}

// The module as the eagleplayer and the init leave it.
static void apply_build(RhstModule* module, const RhstBuild* build) {
    module->build = build;
    module->periods = build->periods;
    module->data = build->data;
    module->noise = build->noise;
    module->noise_words = build->noise_words;
    module->output = build->output;
    module->noise_reads_records = build->noise + 2u * build->noise_words > build->records;
    for (size_t i = 0; i < build->patch_count; i++) {
        const RhstPatch* p = &build->patches[i];
        for (uint32_t k = 0; k < p->size; k++) {
            module->image[p->offset + k] = p->bytes[k];
        }
    }
    for (uint32_t v = 0; v < RHST_RECORD_COUNT; v++) {
        module->mixer_mask[v] = build->clears_records ? 0 : s_file_masks[v];
    }
    if (build->clears_records) {
        for (uint32_t i = 0; i < RHST_RECORD_COUNT * RECORD_SIZE; i++) {
            module->image[build->records + i] = 0;
        }
    }
}

RhstModule* rhst_module_open(const uint8_t* data, size_t size) {
    if (size < INIT_LEA_AT + 4 || size > 0x7FFFFFFFu || be32(data) != HEADER_LONG0 || be32(data + 4) != HEADER_LONG1
        || be32(data + INIT_LEA_AT) != INIT_LEA) {
        return nullptr;
    }
    const RhstBuild* build = find_build(data, size);
    if (build == nullptr || size < build->records + RHST_RECORD_COUNT * RECORD_SIZE || !records_are_known(data, build)
        || !first_instrument_is_known(data, size, build)) {
        return nullptr;
    }
    const uint32_t songs = song_count_of(data, size, build);
    if (songs == 0) {
        return nullptr;
    }
    RhstModule* module = (RhstModule*)calloc(1, sizeof(RhstModule));
    if (module == nullptr) {
        return nullptr;
    }
    module->size = (uint32_t)size;
    module->song_count = songs;
    module->image = nuade_paula_padded_copy(data, size);
    if (module->image == nullptr) {
        free(module);
        return nullptr;
    }
    apply_build(module, build);
    if (!streams_are_modelled(module)) {
        rhst_module_close(module);
        return nullptr;
    }
    return module;
}
