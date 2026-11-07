// SPDX-License-Identifier: MIT
// Beathoven Synthesizer player: the voice routine of BEATHOVEN100, the first replayer.
//
// Its track lists have the format of the later ones; the command streams are simpler: 0x80 sets the instrument (the
// address of a 12 byte record: volume, the length in bytes, the sample address), 0x81 plays the instrument with an
// arpeggio (a length, a count and that many note numbers that the voice cycles through, one per tick), 0x82 is a rest,
// and anything else is a note: the number and the length in ticks. A note plays the instrument's sample once; the
// voice repeats the silent sample after it. The replayer reaches the hardware through a helper routine, which switches
// the channel's DMA off, starts the sample and sets the period and the volume.

#include "beathoven_internal.h"
#include "nuade_tracker.h"

#define LENGTH_SCALE_SHIFT 2
#define LENGTH_SCALE_FACTOR 5

// Commands

typedef enum CommandKind {
    Command_End, // the stream ended
    Command_Instrument,
    Command_Arpeggio,
    Command_Rest,
    Command_Note,
} CommandKind;

// BEATHOVEN100's own command words (the instrument command is that of the others)
#define COMMAND_ARPEGGIO 0x81
#define COMMAND_REST 0x82

// The words of a command: for a note the command word is the note number
typedef struct Command {
    CommandKind kind;
    BthCommandWords words;
} Command;

static Command read_command(const BthPlayer* p, const BthVoice* v) {
    const BthModule* m = &p->work;
    Command c = { .kind = Command_Note, .words = bthm_command_words(m, v->command) };
    if (c.words.word == BTH_COMMAND_END) {
        c.kind = Command_End;
    } else if (c.words.word == BTH_COMMAND_INSTRUMENT) {
        c.kind = Command_Instrument;
    } else if (c.words.word == COMMAND_ARPEGGIO) {
        c.kind = Command_Arpeggio;
    } else if (c.words.word == COMMAND_REST) {
        c.kind = Command_Rest;
    }
    return c;
}

// A length of the stream. Some replayers scale it: (n / 4) * 5, in 16 bits.
static uint16_t scaled_length(const BthPlayer* p, uint16_t length) {
    return p->replayer.durations_scaled ? (uint16_t)((length >> LENGTH_SCALE_SHIFT) * LENGTH_SCALE_FACTOR) : length;
}

// The period of a note number: the table is indexed by the note, the track's note offset and the module's. The
// replayers scale the index to a byte offset with a 16 bit multiplication or a long shift.
static uint16_t note_period(const BthPlayer* p, const BthVoice* v, uint32_t note, int32_t note_offset,
                            uint32_t origin) {
    const BthReplayer* r = &p->replayer;
    uint32_t index = note + v->note_base - origin;
    if (r->uses_note_offset) {
        index += (uint32_t)note_offset;
    }
    const uint32_t offset = r->index_unsigned ? (uint32_t)(uint16_t)index * 2u : index << 1;
    return bth_table_word(p, r->period_table + offset);
}

// The helper's job for a note: start the instrument's sample. The channel plays it once, then repeats the silent
// sample.
static void start_sample(const BthPlayer* p, BthVoice* v) {
    const BthLegacyInstrument instrument = bthm_legacy_instrument(&p->work, v->instrument_address);
    bth_set_address(p, v, instrument.sample, (uint16_t)(instrument.length >> 1));
    nuade_paula_channel_dma_on(&v->paula);
    bth_set_address(p, v, p->replayer.silent, 1);
}

// The helper's job to set the period and the volume of the instrument
static void set_period(const BthPlayer* p, BthVoice* v, uint16_t period) {
    const BthLegacyInstrument instrument = bthm_legacy_instrument(&p->work, v->instrument_address);
    v->paula.period = period;
    v->paula.volume = bth_paula_volume(instrument.volume);
}

// The stream ended: the next track entry. Its stream address is 0 at the end of the track list, which returns to the
// loop entry; a loop entry that ends the list too is an endless loop in the replayer, which stops the voice here.
static bool next_stream(const BthPlayer* p, BthVoice* v) {
    v->track += TRACK_ENTRY_BYTES;
    for (int guard = 0; guard < BTH_MAX_COMMANDS_PER_TICK; guard++) {
        const BthTrackEntry entry = bthm_track_entry(&p->work, v->track);
        if (entry.stream != 0) {
            v->command = entry.stream;
            v->note_base = entry.note_base;
            return true;
        }
        bth_wrap_track(v);
    }
    return false;
}

// The arpeggio command: the instrument plays with the note numbers of the table
static void start_arpeggio(const BthPlayer* p, BthVoice* v, const Command* command) {
    v->arpeggio = true;
    v->arpeggio_count = command->words.count;
    v->duration = scaled_length(p, command->words.operand);
    v->arpeggio_table = v->command + BTH_COMMAND_ARPEGGIO_HEADER_BYTES;
    // The size of the command in bytes is the count and the three header words, doubled; the replayer that scales the
    // period index with mulu.w computes it with the same 16 bit multiplication
    const uint32_t words = (uint32_t)command->words.count + 3;
    v->command += p->replayer.index_unsigned ? (uint16_t)words * 2u : words << 1;
    nuade_paula_channel_dma_off(&v->paula);
    start_sample(p, v);
}

// The note command: the sample of the instrument with the period of the note
static void start_note(const BthPlayer* p, BthVoice* v, const Command* command, int32_t note_offset) {
    nuade_paula_channel_dma_off(&v->paula);
    v->duration = scaled_length(p, command->words.operand);
    const uint16_t period = note_period(p, v, command->words.word, note_offset, p->replayer.note_origin);
    start_sample(p, v);
    set_period(p, v, period);
    v->command += BTH_COMMAND_SHORT_BYTES;
    v->note = nuade_period_to_note(period);
    v->instrument = bth_instrument_index(p, v->instrument_address, BTH_LEGACY_INSTRUMENT_BYTES);
}

// The commands up to the next note, rest or arpeggio
static void read_commands(const BthPlayer* p, BthVoice* v, int32_t note_offset) {
    for (int guard = 0; guard < BTH_MAX_COMMANDS_PER_TICK; guard++) {
        const Command command = read_command(p, v);
        switch (command.kind) {
            case Command_End:
                if (!next_stream(p, v)) {
                    return;
                }
                break;
            case Command_Instrument:
                v->instrument_address = command.words.record;
                v->command += BTH_COMMAND_INSTRUMENT_BYTES;
                break;
            case Command_Arpeggio:
                start_arpeggio(p, v, &command);
                return;
            case Command_Rest:
                v->duration = scaled_length(p, command.words.operand);
                v->command += BTH_COMMAND_SHORT_BYTES;
                nuade_paula_channel_dma_off(&v->paula);
                return;
            case Command_Note:
                start_note(p, v, &command, note_offset);
                return;
        }
    }
}

// The arpeggio takes the next note number of the table on every tick
static void step_arpeggio(const BthPlayer* p, BthVoice* v, int32_t note_offset) {
    const uint32_t offset = (uint32_t)v->arpeggio_index * 2u;
    const uint32_t note = bthm_u16(&p->work, v->arpeggio_table + offset);
    set_period(p, v, note_period(p, v, note, note_offset, p->replayer.arpeggio_origin));
    bth_advance_arpeggio(v);
}

void bth_update_voice_legacy(BthPlayer* p, BthVoice* v, int32_t note_offset) {
    v->duration--;
    if (v->duration == 0) {
        v->arpeggio = false;
        read_commands(p, v, note_offset);
    }
    if (v->arpeggio) {
        step_arpeggio(p, v, note_offset);
    }
}
