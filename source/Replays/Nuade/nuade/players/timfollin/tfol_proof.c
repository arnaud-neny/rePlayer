// SPDX-License-Identifier: MIT
// Tim Follin player: the open-time proof. The replayer runs on the original's memory: streams that leave the file, a
// call stack that grows into the player's variables, a note before the first instrument, relocation writes and sweep
// writes into bytes the replayer reads. The port models none of these, so the module is refused (UADE then plays it)
// unless a walk of every stream of every playable song shows that the replayer stays inside the modelled behaviour.
//
// The walk follows each voice's stream through all commands (call, return, jump, loops with their counters) up to every
// note, and continues from the state after the note; it stops at a state it has seen. The state is everything that
// decides where the stream goes: position, call stack, loop start and count, and whether an instrument and a note
// length are set. One work budget covers the whole module.

#include "nuade_utils.h"
#include "tfol_internal.h"
#include "tfol_layout.h"
#include <stdlib.h>
#include <string.h>

#define TFOL_PROOF_STATES 400000u
#define TFOL_PROOF_STEPS 40000000u
#define TFOL_NO_LOOP 0xffffffffu

typedef struct TfolWalkState {
    uint32_t position;
    uint32_t loop_start; // TFOL_NO_LOOP before the first loop start
    uint32_t stack[TFOL_STACK_PER_VOICE];
    uint8_t depth;
    uint8_t loop_count;
    bool has_instrument;
    bool has_note_length;
} TfolWalkState;

typedef struct TfolProof {
    const TfolModule* module;
    // One bit per file byte.
    uint8_t* read;    // read as data: stream bytes, arguments, addresses, instrument table entries, length words
    uint8_t* sample;  // played as a sample
    uint8_t* written; // written by the relocation list
    uint8_t* swept;   // written by the sweeps
    TfolWalkState* states;
    int32_t* index; // hash table of state numbers, -1 empty
    uint32_t state_count;
    uint32_t next_state; // the next state to walk
    uint64_t steps;
    TfolSong song_record;
    bool instrument_ok[256]; // instrument table entries of the song that were checked
    bool instrument_used[256];
    bool failed;
} TfolProof;

static bool tfol_proof_in_file(const TfolProof* proof, uint64_t offset, uint64_t length) {
    return offset <= proof->module->size && length <= proof->module->size - offset;
}

// Sets the bits of a range already known to be inside the file.
static void tfol_bits_set(uint8_t* bits, uint32_t offset, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        bits[(offset + i) >> 3] |= (uint8_t)(1u << ((offset + i) & 7));
    }
}

static bool tfol_bits_overlap(const TfolProof* proof, const uint8_t* a, const uint8_t* b) {
    const size_t bytes = (proof->module->size + 7) / 8;
    for (size_t i = 0; i < bytes; i++) {
        if ((a[i] & b[i]) != 0) {
            return true;
        }
    }
    return false;
}

// Checks an entry of the song's instrument table: the entry, the record's length word and the sample are inside the
// file; remembers what they cover.
static bool tfol_proof_instrument(TfolProof* proof, uint8_t index) {
    if (proof->instrument_ok[index]) {
        return true;
    }
    const uint32_t entry = proof->song_record.instrument_table + index * 4u;
    if (!tfol_proof_in_file(proof, entry, 4)) {
        return false;
    }
    const TfolInstrument inst = tfol_module_instrument(proof->module, proof->song_record.instrument_table, index);
    if (!inst.valid) {
        return false;
    }
    tfol_bits_set(proof->read, entry, 4);
    tfol_bits_set(proof->read, inst.offset, 2);
    tfol_bits_set(proof->sample, inst.sample, inst.sample_bytes);
    proof->instrument_ok[index] = true;
    return true;
}

static uint32_t tfol_state_hash(const TfolWalkState* s) {
    const uint8_t* p = (const uint8_t*)s;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < sizeof(*s); i++) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

// Adds a state to the walk unless it is known. False when the state budget is spent.
static bool tfol_proof_add_state(TfolProof* proof, const TfolWalkState* state) {
    const uint32_t slots = TFOL_PROOF_STATES * 2;
    uint32_t slot = tfol_state_hash(state) % slots;
    while (proof->index[slot] >= 0) {
        if (memcmp(&proof->states[proof->index[slot]], state, sizeof(*state)) == 0) {
            return true;
        }
        slot = (slot + 1) % slots;
    }
    if (proof->state_count >= TFOL_PROOF_STATES) {
        return false;
    }
    proof->states[proof->state_count] = *state;
    proof->index[slot] = (int32_t)proof->state_count++;
    return true;
}

// One command. Sets *ended when the stream stops.
static bool tfol_proof_command(TfolProof* proof, TfolWalkState* s, const TfolEvent* ev, bool* ended) {
    s->position = ev->next;
    switch (ev->command) {
        case TfolCommand_Instrument:
            if (!tfol_proof_instrument(proof, ev->args[0])) {
                return false;
            }
            proof->instrument_used[ev->args[0]] = true;
            s->has_instrument = true;
            return true;
        case TfolCommand_Call:
            if (s->depth >= TFOL_STACK_PER_VOICE) {
                return false;
            }
            s->stack[s->depth++] = s->position;
            s->position = ev->target;
            return true;
        case TfolCommand_Return:
            if (s->depth == 0) {
                return false;
            }
            s->position = s->stack[--s->depth];
            s->stack[s->depth] = 0;
            return true;
        case TfolCommand_LoopStart:
            s->loop_count = ev->args[0];
            s->loop_start = s->position;
            return true;
        case TfolCommand_LoopEnd:
            if (s->loop_start == TFOL_NO_LOOP) {
                return false;
            }
            s->loop_count--;
            if (s->loop_count != 0) {
                s->position = s->loop_start;
            }
            return true;
        case TfolCommand_NoteLength:
            s->has_note_length = ev->args[0] != 0;
            return true;
        case TfolCommand_WaveFlags:
            // Bit 6 switches to the instrument table entry in bits 0..5 after every note.
            return (ev->args[0] & TFOL_WAVE_SECOND_SAMPLE) == 0
                   || tfol_proof_instrument(proof, ev->args[0] & TFOL_WAVE_SAMPLE_INDEX);
        case TfolCommand_Stop:
            *ended = true;
            return true;
        case TfolCommand_Jump:
            s->position = ev->target;
            return true;
        case TfolCommand_Nop:
        case TfolCommand_Envelope:
        case TfolCommand_Portamento:
        case TfolCommand_Vibrato:
        case TfolCommand_PitchModulation:
        case TfolCommand_Transpose:
        case TfolCommand_EnvelopeRestart:
        case TfolCommand_SweepRate:
        case TfolCommand_EnvelopeStep:
        case TfolCommand_SkipTranspose:
            return true;
        case TfolCommand_Count:
            break;
    }
    return false;
}

// Runs the stream from `s` up to the next note, with the same event limit as the replayer's fetch; the state after the
// note is added to the walk.
static bool tfol_proof_segment(TfolProof* proof, TfolWalkState s) {
    for (uint32_t step = 0; step < TFOL_FETCH_LIMIT; step++) {
        if (++proof->steps > TFOL_PROOF_STEPS) {
            return false;
        }
        const TfolEvent ev = tfol_module_read_event(proof->module, s.position, s.has_note_length);
        if (!ev.valid) {
            return false;
        }
        tfol_bits_set(proof->read, ev.start, ev.next - ev.start);
        if (ev.is_note) {
            if (!s.has_instrument) {
                return false;
            }
            s.position = ev.next;
            return tfol_proof_add_state(proof, &s);
        }
        bool ended = false;
        if (!tfol_proof_command(proof, &s, &ev, &ended)) {
            return false;
        }
        if (ended) {
            return true;
        }
    }
    return false;
}

// Walks one voice of the current song to a fixed point.
static bool tfol_proof_voice(TfolProof* proof, uint32_t stream) {
    TfolWalkState start;
    memset(&start, 0, sizeof(start));
    start.position = stream;
    start.loop_start = TFOL_NO_LOOP;
    if (!tfol_proof_add_state(proof, &start)) {
        return false;
    }
    while (proof->next_state < proof->state_count) {
        if (!tfol_proof_segment(proof, proof->states[proof->next_state++])) {
            return false;
        }
    }
    return true;
}

// The sweeps of the instruments below 4 write into the record's header and the first bytes of the sample.
static bool tfol_proof_mark_sweeps(TfolProof* proof) {
    for (uint32_t number = 0; number < 256; number++) {
        if (!proof->instrument_used[number] || (int8_t)number >= TFOL_SWEEP_INSTRUMENTS) {
            continue;
        }
        const TfolInstrument inst
            = tfol_module_instrument(proof->module, proof->song_record.instrument_table, (uint8_t)number);
        const uint32_t sweep = inst.sample + TFOL_SWEEP_LOWEST;
        const uint32_t sweep_bytes = TFOL_SWEEP_LAST - TFOL_SWEEP_LOWEST + 1;
        if (!tfol_proof_in_file(proof, sweep, sweep_bytes)) {
            return false;
        }
        tfol_bits_set(proof->swept, inst.offset + TFOL_RESET_FIRST, TFOL_RESET_FILLED + TFOL_RESET_ZEROED);
        tfol_bits_set(proof->swept, sweep, sweep_bytes);
    }
    return true;
}

// Walks every voice of one playable song.
static bool tfol_proof_song(TfolProof* proof, uint32_t song) {
    proof->song_record = tfol_module_song(proof->module, song);
    memset(proof->instrument_ok, 0, sizeof(proof->instrument_ok));
    memset(proof->instrument_used, 0, sizeof(proof->instrument_used));
    proof->state_count = 0;
    proof->next_state = 0;
    memset(proof->index, 0xff, TFOL_PROOF_STATES * 2 * sizeof(int32_t));
    if (song == TFOL_SILENT_SONG) {
        return true;
    }
    for (uint32_t voice = 0; voice < proof->song_record.voices; voice++) {
        if (!tfol_proof_voice(proof, proof->song_record.stream[voice])) {
            return false;
        }
    }
    return tfol_proof_mark_sweeps(proof);
}

// InitPlayer's second relocation loop: a list of longs at table + $194 that ends at a long with a zero high word; each
// entry is an address that gets the offset added, and the long at +2 of what it names gets the offset added too. The
// port does not apply these writes, so they must not reach any byte the replayer reads.
static bool tfol_proof_relocation_list(TfolProof* proof) {
    uint32_t at = proof->module->table + TFOL_RELOCATION_LIST_OFFSET;
    for (;;) {
        if (!tfol_proof_in_file(proof, at, 4)) {
            return false;
        }
        const uint32_t entry = nuade_read_be32(proof->module->mem + at);
        if ((entry >> 16) == 0) {
            return true;
        }
        const uint32_t record = tfol_module_offset(proof->module, entry);
        if (!tfol_proof_in_file(proof, record, 1) || !tfol_proof_in_file(proof, record + 2u, 4)) {
            return false;
        }
        tfol_bits_set(proof->written, at, 4);
        tfol_bits_set(proof->written, record + 2u, 4);
        at += 4;
    }
}

static bool tfol_proof_run(TfolProof* proof) {
    const TfolModule* m = proof->module;
    if (!tfol_proof_relocation_list(proof)) {
        return false;
    }
    for (uint32_t subsong = 1; subsong <= m->subsong_count; subsong++) {
        if (!tfol_proof_song(proof, subsong & (TFOL_SONGS - 1))) {
            return false;
        }
    }
    // The table (relocated longs, voice counts) is read through the readers of the port, not as stream data.
    uint8_t* table = (uint8_t*)calloc((m->size + 7) / 8, 1);
    if (table == nullptr) {
        return false;
    }
    tfol_bits_set(table, m->table, TFOL_TABLE_SIZE);
    const bool clean
        = !tfol_bits_overlap(proof, proof->read, table) && !tfol_bits_overlap(proof, proof->written, table)
          && !tfol_bits_overlap(proof, proof->swept, table) && !tfol_bits_overlap(proof, proof->read, proof->written)
          && !tfol_bits_overlap(proof, proof->sample, proof->written) && !tfol_bits_overlap(proof, proof->sample, table)
          && !tfol_bits_overlap(proof, proof->read, proof->swept)
          && !tfol_bits_overlap(proof, proof->written, proof->swept);
    free(table);
    return clean;
}

bool tfol_module_prove(const TfolModule* module) {
    const size_t bits = (module->size + 7) / 8;
    TfolProof proof = { .module = module };
    proof.read = (uint8_t*)calloc(bits, 1);
    proof.sample = (uint8_t*)calloc(bits, 1);
    proof.written = (uint8_t*)calloc(bits, 1);
    proof.swept = (uint8_t*)calloc(bits, 1);
    proof.states = (TfolWalkState*)malloc(TFOL_PROOF_STATES * sizeof(TfolWalkState));
    proof.index = (int32_t*)malloc(TFOL_PROOF_STATES * 2 * sizeof(int32_t));
    const bool ok = proof.read && proof.sample && proof.written && proof.swept && proof.states && proof.index
                    && tfol_proof_run(&proof);
    free(proof.read);
    free(proof.sample);
    free(proof.written);
    free(proof.swept);
    free(proof.states);
    free(proof.index);
    return ok;
}
