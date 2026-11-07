// SPDX-License-Identifier: MIT
// Medley: a static proof, made when the module is opened, that playing any subsong reads nothing the port does not
// model, whatever the timing of the random voices and however long the song runs.
//
// A voice is a track position plus what outlives the tracks: its instrument, its transpose (command 8), its last note
// (0 before the first) and its two stacks (calls, loops). The proof explores every state a voice can reach, without
// running it: each step follows the track byte at the position (a pair of note and duration, or a command and its
// parameter) and every branch the replayer can take (a loop end loops or leaves, a command 4 takes any track of its
// list, a track end returns from the call or restarts the voice). At every pair that calculates a pitch it checks what
// the replayer then reads: the instrument record, its modulators, the wave of the note's range, the sample range, the
// period table index. Anything outside the file or the memory the port models fails the proof.
//
// The replayer reads the track in steps: from one pair with a duration to the next, it runs commands and skips pairs
// with a duration of 0, and ends the song if a step runs MDL_MAX_STEP_COMMANDS of them. The proof counts the items of
// the longest step to each state and refuses a file with a step that long, which also refuses a loop of commands alone
// (its loop end can always loop again). Everything the exploration records is bounded by one budget for the whole
// module, so a crafted file with many subsongs is refused, not explored for long.

#include "mdl_internal.h"
#include <stdlib.h>
#include <string.h>

// The most states and stack frames recorded for a module (the tables take about 50 MB at the limit), and the most
// states explored.
#define MDL_COVERAGE_STATES (1u << 20)
#define MDL_COVERAGE_STEPS (1u << 24)
#define MDL_VALUES 256
// The position of a restart source in the set of states (no file is this large).
#define MDL_SOURCE_POSITION 0xffffff00u

typedef struct MdlState {
    uint32_t position;
    uint8_t voice;
    uint8_t instrument;
    uint8_t transpose; // command 8
    uint8_t note;
    uint32_t calls; // the call stack, as an id of MdlCoverage.frame (0 = empty)
    uint32_t loops; // the loop stack
    // Not part of the state's identity: the items the step has run so far, the most any path to the state ran.
    uint32_t step_items;
} MdlState;

// A stack is a chain of frames: the return position of a call, or the first position of a loop body.
typedef struct MdlFrame {
    uint32_t parent;
    uint32_t position;
    uint32_t depth;
} MdlFrame;

// An open-addressing table of 128-bit keys with a value. A key's low half is never 0 (0 marks a free slot).
typedef struct MdlHashEntry {
    uint64_t low;
    uint64_t high;
    uint32_t value;
} MdlHashEntry;

typedef struct MdlHash {
    MdlHashEntry* entries;
    uint32_t capacity;
    uint32_t count;
} MdlHash;

// What the whole module may still record and explore.
typedef struct MdlBudget {
    uint32_t states;
    uint64_t steps;
} MdlBudget;

typedef struct MdlCoverage {
    MdlModule* module;
    const MdlSong* song;
    MdlBudget* budget;
    MdlHash seen;          // the states seen -> the longest step to them
    MdlHash frames_by_key; // (parent, position) -> frame id
    MdlState* work;
    uint32_t work_count;
    uint32_t work_capacity;
    MdlFrame* frames; // frame 0 is the empty stack
    uint32_t frame_count;
    uint32_t frame_capacity;
    // Where a voice's track end (with no call open) restarts it, and the states that ended so far.
    uint32_t* restarts[MDL_VOICES];
    uint32_t restart_count[MDL_VOICES];
    uint32_t restart_capacity[MDL_VOICES];
    MdlState* ends[MDL_VOICES];
    uint32_t end_count[MDL_VOICES];
    uint32_t end_capacity[MDL_VOICES];
    bool instrument_loaded[MDL_VALUES];
    bool value_checked[MDL_VALUES][MDL_VALUES]; // [instrument][note value]
    MdlInstrument instrument[MDL_VALUES];
    bool exhausted;
} MdlCoverage;

static uint32_t mdl_hash_slot(uint64_t low, uint64_t high, uint32_t capacity) {
    return (uint32_t)(((low ^ (high * 0xc2b2ae3d27d4eb4full)) * 0x9e3779b97f4a7c15ull) >> 32) & (capacity - 1);
}

// Finds the entry of a key, adding it (value 0) if it is new. Null when the budget or memory is out.

static MdlHashEntry* mdl_hash_entry(MdlCoverage* c, MdlHash* hash, uint64_t low, uint64_t high, bool* added) {
    if (hash->count * 2 >= hash->capacity) {
        const uint32_t capacity = hash->capacity == 0 ? 1024 : hash->capacity * 2;
        MdlHashEntry* entries = calloc(capacity, sizeof(*entries));
        if (entries == nullptr) {
            c->exhausted = true;
            return nullptr;
        }
        for (uint32_t i = 0; i < hash->capacity; i++) {
            if (hash->entries[i].low != 0) {
                uint32_t slot = mdl_hash_slot(hash->entries[i].low, hash->entries[i].high, capacity);
                while (entries[slot].low != 0) {
                    slot = (slot + 1) & (capacity - 1);
                }
                entries[slot] = hash->entries[i];
            }
        }
        free(hash->entries);
        hash->entries = entries;
        hash->capacity = capacity;
    }
    uint32_t slot = mdl_hash_slot(low, high, hash->capacity);
    while (hash->entries[slot].low != 0) {
        if (hash->entries[slot].low == low && hash->entries[slot].high == high) {
            *added = false;
            return &hash->entries[slot];
        }
        slot = (slot + 1) & (hash->capacity - 1);
    }
    if (c->budget->states == 0) {
        c->exhausted = true;
        return nullptr;
    }
    c->budget->states--;
    hash->count++;
    hash->entries[slot] = (MdlHashEntry) { .low = low, .high = high };
    *added = true;
    return &hash->entries[slot];
}

// Makes room for one more element of an array that grows by doubling; the array (null when the budget or memory is
// out).

static void* mdl_grow(MdlCoverage* c, void* array, uint32_t* capacity, uint32_t count, size_t element) {
    if (count < *capacity) {
        return array;
    }
    const uint32_t wanted = *capacity == 0 ? 64 : *capacity * 2;
    void* grown = wanted > MDL_COVERAGE_STATES ? nullptr : realloc(array, (size_t)wanted * element);
    if (grown == nullptr) {
        c->exhausted = true;
        return nullptr;
    }
    *capacity = wanted;
    return grown;
}

static uint64_t mdl_state_low(const MdlState* state) {
    return 1
           + ((uint64_t)state->position << 32 | (uint64_t)state->voice << 24 | (uint64_t)state->instrument << 16
              | (uint64_t)state->transpose << 8 | state->note);
}

static uint64_t mdl_state_high(const MdlState* state) {
    return (uint64_t)state->calls << 32 | state->loops;
}

// Records a state; true if it is new or the step to it is longer than the longest seen (the state is explored again,
// so a step that loops back on itself grows until it reaches the limit).

static bool mdl_record(MdlCoverage* c, const MdlState* state) {
    bool added = false;
    MdlHashEntry* entry = mdl_hash_entry(c, &c->seen, mdl_state_low(state), mdl_state_high(state), &added);
    if (entry == nullptr || (!added && entry->value >= state->step_items)) {
        return false;
    }
    entry->value = state->step_items;
    return true;
}

// Queues a state to explore, if it is new.

static void mdl_push(MdlCoverage* c, MdlState state) {
    if (state.step_items >= MDL_MAX_STEP_COMMANDS) {
        c->module->unmapped = true;
        return;
    }
    if (!mdl_record(c, &state)) {
        return;
    }
    MdlState* work = mdl_grow(c, c->work, &c->work_capacity, c->work_count, sizeof(*c->work));
    if (work != nullptr) {
        c->work = work;
        c->work[c->work_count++] = state;
    }
}

// The stack with `position` pushed on `parent`; the same frame every time. 0 (the empty stack) when the budget is out.

static uint32_t mdl_frame_push(MdlCoverage* c, uint32_t parent, uint32_t position) {
    bool added = false;
    MdlHashEntry* entry = mdl_hash_entry(c, &c->frames_by_key, 1 + (uint64_t)parent, position, &added);
    if (entry == nullptr) {
        return 0;
    }
    if (added) {
        MdlFrame* frames = mdl_grow(c, c->frames, &c->frame_capacity, c->frame_count, sizeof(*c->frames));
        if (frames == nullptr) {
            return 0;
        }
        c->frames = frames;
        c->frames[c->frame_count] = (MdlFrame) { parent, position, c->frames[parent].depth + 1 };
        entry->value = c->frame_count++;
    }
    return entry->value;
}

// A place a voice restarts at when its track ends with no call open: every state that ended so far goes there.

static void mdl_add_restart(MdlCoverage* c, int voice, uint32_t position) {
    for (uint32_t i = 0; i < c->restart_count[voice]; i++) {
        if (c->restarts[voice][i] == position) {
            return;
        }
    }
    uint32_t* restarts
        = mdl_grow(c, c->restarts[voice], &c->restart_capacity[voice], c->restart_count[voice], sizeof(*restarts));
    if (restarts == nullptr) {
        return;
    }
    c->restarts[voice] = restarts;
    c->restarts[voice][c->restart_count[voice]++] = position;
    for (uint32_t i = 0; i < c->end_count[voice]; i++) {
        MdlState state = c->ends[voice][i];
        state.position = position;
        mdl_push(c, state);
    }
}

// A state whose track ended with no call open: it restarts at every restart place of its voice, with the state it ended
// in (a voice keeps its instrument, transpose, note and loop stack, and the step goes on).

static void mdl_add_end(MdlCoverage* c, const MdlState* state) {
    MdlState marker = *state;
    marker.position = MDL_SOURCE_POSITION;
    if (!mdl_record(c, &marker)) {
        return;
    }
    const int voice = state->voice;
    MdlState* ends = mdl_grow(c, c->ends[voice], &c->end_capacity[voice], c->end_count[voice], sizeof(*ends));
    if (ends == nullptr) {
        return;
    }
    c->ends[voice] = ends;
    c->ends[voice][c->end_count[voice]++] = *state;
    for (uint32_t i = 0; i < c->restart_count[voice]; i++) {
        MdlState restart = *state;
        restart.position = c->restarts[voice][i];
        mdl_push(c, restart);
    }
}

// The sample range of a wave an instrument plays, at its worst: a looped window can start anywhere in either half.

static bool mdl_wave_playable(const MdlModule* module, const MdlInstrument* instrument, const MdlWave* wave) {
    switch (instrument->type) {
        case MdlInstrumentType_Looped: {
            const uint16_t words = (uint16_t)(wave->length >> 2);
            const uint16_t last = (uint16_t)((wave->length >> 1) - 1);
            const bool first_half = mdl_sample_inside(module, mdl_window_offset(wave, false, 0), words)
                                    && mdl_sample_inside(module, mdl_window_offset(wave, false, last), words);
            return first_half
                   && (!wave->second_half
                       || (mdl_sample_inside(module, mdl_window_offset(wave, true, 0), words)
                           && mdl_sample_inside(module, mdl_window_offset(wave, true, last), words)));
        }
        case MdlInstrumentType_Synth: {
            // Two reads at any offset up to the length minus one.
            const uint16_t mask = (uint16_t)(wave->length - 1);
            return wave->length != 0 && mask <= 0x7fff && (uint64_t)wave->data + wave->length <= module->size;
        }
        default:
            return mdl_sample_inside(module, wave->data, (uint16_t)(wave->length >> 1));
    }
}

// Reads an instrument record once and checks what does not depend on the note: the modulators' voice flags.

static bool mdl_load_instrument(MdlCoverage* c, uint8_t number) {
    if (c->instrument_loaded[number]) {
        return true;
    }
    const uint32_t offset = mdl_instrument_offset(c->module, number);
    if (offset == MDL_NO_OFFSET) {
        return false;
    }
    MdlInstrument* instrument = &c->instrument[number];
    *instrument = mdl_instrument(c->module, offset);
    c->instrument_loaded[number] = true;
    for (int i = 0; i < MDL_MODULATORS; i++) {
        const MdlModulatorSetup* setup = &instrument->modulator[i];
        if (setup->kind != MdlModulatorKind_Off
            && (!mdl_voice_flag_valid(setup->top_start) || !mdl_voice_flag_valid(setup->half_start))) {
            return false;
        }
    }
    return true;
}

// What the replayer reads when instrument `number` plays the note value `value` (the note plus the transposes, as a
// byte): the wave, its samples and the period table.

static bool mdl_value_covered(MdlCoverage* c, uint8_t number, uint8_t value) {
    const MdlInstrument* instrument = &c->instrument[number];
    MdlWave wave;
    if (instrument->type == MdlInstrumentType_Synth) {
        return mdl_wave(c->module, instrument->wave[0], &wave) && mdl_wave_playable(c->module, instrument, &wave);
    }
    const uint8_t region_wave = mdl_region_wave(instrument, mdl_note_region(value));
    return mdl_wave(c->module, region_wave, &wave) && wave.octave < MDL_REGIONS
           && mdl_wave_playable(c->module, instrument, &wave)
           && mdl_period_index_modelled(mdl_sample_period_index(value, instrument, &wave));
}

// A step that reaches a pitch: the replayer forms the note plus the song's, the voice's and the instrument's
// transposes.

static void mdl_check_pitch(MdlCoverage* c, const MdlState* state) {
    if (!mdl_load_instrument(c, state->instrument)) {
        c->module->unmapped = true;
        return;
    }
    const uint8_t value
        = (uint8_t)(state->note + c->song->transpose + state->transpose + c->instrument[state->instrument].transpose);
    if (c->value_checked[state->instrument][value]) {
        return;
    }
    c->value_checked[state->instrument][value] = true;
    if (!mdl_value_covered(c, state->instrument, value)) {
        c->module->unmapped = true;
    }
}

// Command 4: the first `count` pairs after the command each name a track the voice can jump to.

static void mdl_follow_choices(MdlCoverage* c, const MdlState* state, uint32_t pairs, uint8_t count) {
    for (uint16_t choice = 0; choice < count; choice++) {
        const uint32_t target = mdl_track_from_table_start(c->module, mdl_choice_track(c->module, pairs, choice));
        if (target != MDL_NO_OFFSET) {
            MdlState next = *state;
            next.position = target;
            mdl_push(c, next);
        }
    }
}

// Command 0: a track end returns from the call that is open, else the voice restarts.

static void mdl_explore_end(MdlCoverage* c, const MdlState* state) {
    if (state->calls == 0) {
        mdl_add_end(c, state);
        return;
    }
    MdlState back = *state;
    back.position = c->frames[state->calls].position;
    back.calls = c->frames[state->calls].parent;
    mdl_push(c, back);
}

// Commands 5, 6 and 7. A call or a loop on a full stack does nothing; a loop end loops or leaves.

static void mdl_explore_stack(MdlCoverage* c, MdlState next, uint8_t command, uint8_t param) {
    switch (command) {
        case MdlCommand_Call: {
            const uint32_t target = mdl_track_start(c->module, param);
            if (target != MDL_NO_OFFSET && c->frames[next.calls].depth < MDL_STACK_DEPTH) {
                MdlState call = next;
                call.position = target;
                call.calls = mdl_frame_push(c, next.calls, next.position);
                mdl_push(c, call);
                return;
            }
            break;
        }
        case MdlCommand_LoopStart:
            if (c->frames[next.loops].depth < MDL_STACK_DEPTH) {
                next.loops = mdl_frame_push(c, next.loops, next.position);
            }
            break;
        default: // MdlCommand_LoopEnd
            if (next.loops != 0) {
                MdlState again = next;
                again.position = c->frames[next.loops].position;
                mdl_push(c, again);
                next.loops = c->frames[next.loops].parent;
            }
            break;
    }
    mdl_push(c, next);
}

// One step of a state: the successors of the pair or command at its position.

static void mdl_explore(MdlCoverage* c, const MdlState* state) {
    MdlModule* module = c->module;
    const uint8_t byte = mdl_u8(module, state->position);
    const uint8_t param = mdl_u8(module, state->position + 1);
    MdlState next = *state;
    next.position = state->position + 2;
    next.step_items = state->step_items + 1;
    if ((byte & MDL_COMMAND_BIT) == 0) {
        if (byte != 0) {
            next.note = byte;
        }
        // A pair with a duration of 0 is skipped; any other ends the step and calculates the pitch.
        if (param != 0) {
            next.step_items = 0;
            mdl_check_pitch(c, &next);
        }
        mdl_push(c, next);
        return;
    }
    const uint8_t command = byte & MDL_COMMAND_MASK;
    switch (command) {
        case MdlCommand_End:
            mdl_explore_end(c, &next);
            return;
        case MdlCommand_Instrument:
            // An empty entry leaves the instrument as it was.
            if (mdl_instrument_offset(module, param) != MDL_NO_OFFSET) {
                if (!mdl_load_instrument(c, param)) {
                    module->unmapped = true;
                }
                next.instrument = param;
            }
            break;
        case MdlCommand_RandomTrack:
            mdl_follow_choices(c, &next, next.position, param);
            break;
        case MdlCommand_Call:
        case MdlCommand_LoopStart:
        case MdlCommand_LoopEnd:
            mdl_explore_stack(c, next, command, param);
            return;
        case MdlCommand_Transpose:
            next.transpose = param;
            break;
        default:
            break;
    }
    mdl_push(c, next);
}

// A voice's first state at `start`, and the place its track end goes back to.

static void mdl_start_voice(MdlCoverage* c, int voice, uint32_t start, uint8_t instrument) {
    if (start >= c->module->size) {
        c->module->unmapped = true;
        return;
    }
    mdl_add_restart(c, voice, start);
    mdl_push(c, (MdlState) { .position = start, .voice = (uint8_t)voice, .instrument = instrument });
}

// The states a subsong starts in: each used voice at its track with its instrument, a random voice at every track
// its list names.

static void mdl_start_voices(MdlCoverage* c) {
    MdlModule* module = c->module;
    const MdlSong* song = c->song;
    for (int voice = 0; voice < MDL_VOICES; voice++) {
        if (song->track[voice] == 0) {
            continue;
        }
        const uint32_t track = mdl_track_start(module, song->track[voice]);
        const uint8_t instrument = mdl_song_voice_instrument(song, voice);
        if (track == MDL_NO_OFFSET || !mdl_load_instrument(c, instrument)) {
            module->unmapped = true;
            return;
        }
        if (song->random_wait_min[voice] == 0) {
            mdl_start_voice(c, voice, track, instrument);
            continue;
        }
        const uint8_t last = (uint8_t)(mdl_random_list_count(module, track) - 1);
        for (uint16_t choice = 0; choice <= last; choice++) {
            const uint32_t start
                = mdl_track_start(module, mdl_choice_track(module, track + MDL_RANDOM_LIST_PAIRS, choice));
            if (start != MDL_NO_OFFSET) {
                mdl_start_voice(c, voice, start, instrument);
            }
        }
    }
}

static void mdl_release(MdlCoverage* c) {
    for (int voice = 0; voice < MDL_VOICES; voice++) {
        free(c->restarts[voice]);
        free(c->ends[voice]);
    }
    free(c->seen.entries);
    free(c->frames_by_key.entries);
    free(c->work);
    free(c->frames);
}

static bool mdl_song_covered(MdlCoverage* c, MdlModule* module, const MdlSong* song, MdlBudget* budget) {
    memset(c, 0, sizeof(*c));
    c->module = module;
    c->song = song;
    c->budget = budget;
    // Frame 0 is the empty stack.
    MdlFrame* frames = mdl_grow(c, c->frames, &c->frame_capacity, 0, sizeof(*c->frames));
    if (frames != nullptr) {
        c->frames = frames;
        c->frames[c->frame_count++] = (MdlFrame) { 0 };
        mdl_start_voices(c);
    }
    while (c->work_count != 0 && !module->unmapped && !c->exhausted) {
        if (budget->steps == 0) {
            c->exhausted = true;
            break;
        }
        budget->steps--;
        const MdlState state = c->work[--c->work_count];
        mdl_explore(c, &state);
    }
    const bool covered = !module->unmapped && !c->exhausted;
    mdl_release(c);
    return covered;
}

// True if an earlier subsong entry has the same song (its proof holds for this one).

static bool mdl_song_seen(const MdlSong* seen, uint16_t count, const MdlSong* song) {
    for (uint16_t i = 0; i < count; i++) {
        if (memcmp(&seen[i], song, sizeof(*song)) == 0) {
            return true;
        }
    }
    return false;
}

bool mdl_module_covered(MdlModule* module) {
    MdlCoverage* c = calloc(1, sizeof(*c));
    MdlSong* proven = calloc(module->subsong_count, sizeof(*proven));
    MdlBudget budget = { .states = MDL_COVERAGE_STATES, .steps = MDL_COVERAGE_STEPS };
    bool covered = c != nullptr && proven != nullptr;
    uint16_t proven_count = 0;
    module->unmapped = false;
    for (uint16_t number = 1; number <= module->subsong_count && covered; number++) {
        MdlSong song;
        if (!mdl_song(module, (uint8_t)number, &song)) {
            // An empty entry plays silence; a header outside the file is not modelled.
            covered = !module->unmapped;
        } else if (!mdl_song_seen(proven, proven_count, &song)) {
            covered = mdl_song_covered(c, module, &song, &budget);
            proven[proven_count++] = song;
        }
    }
    free(c);
    free(proven);
    return covered;
}
