// SPDX-License-Identifier: MIT
// Beathoven Synthesizer player: the voice routine of the basic and the extended replayer.
//
// A song is four track lists. A track list is an array of 8 byte entries (the address of a command stream and the
// voice's note offset) ended by an entry whose address is 0; the track returns to a loop entry at its end. A command
// stream is words, ended by 0: 0x80 sets the instrument (the address of a 14 byte record: volume, the length of the
// played-once sample, the length and the end of the loop, the sample address and a one-shot flag), 0x81 sets the slide
// step (0 = no slide), 0x82 switches legato on or off, 0x83 holds the note, 0x84 starts an arpeggio, 0x85 sets the note
// offset, anything else is a note: the number and the length in ticks. The length is multiplied by the module's tempo.
//
// Every tick the play routine runs the voice routine for each voice. It writes the repeat registers, counts the note
// down and, when it has run out, reads commands up to the next note; then it slides or arpeggiates the period, writes
// it and switches the voice's DMA on.

#include "beathoven_internal.h"
#include "nuade_tracker.h"

// Commands

typedef enum CommandKind {
    Command_End, // the stream ended
    Command_Instrument,
    Command_Slide,
    Command_Legato,
    Command_Hold,
    Command_Arpeggio,
    Command_NoteBias,
    Command_Note,
} CommandKind;

// The words of a command: for a note the command word is the note number
typedef struct Command {
    CommandKind kind;
    BthCommandWords words;
} Command;

// The command at the voice's stream position. Hold and arpeggio commands exist in the extended replayers, the note
// offset command in one more; to the others they are notes.
static Command read_command(const BthPlayer* p, const BthVoice* v) {
    const BthModule* m = &p->work;
    const BthReplayer* r = &p->replayer;
    const bool extended = r->engine == BthEngine_Extended;
    Command c = { .kind = Command_Note, .words = bthm_command_words(m, v->command) };
    const uint16_t word = c.words.word;
    if (word == BTH_COMMAND_END) {
        c.kind = Command_End;
    } else if (word == BTH_COMMAND_INSTRUMENT) {
        c.kind = Command_Instrument;
    } else if (word == BTH_COMMAND_SLIDE) {
        c.kind = Command_Slide;
    } else if (word == BTH_COMMAND_LEGATO) {
        c.kind = Command_Legato;
    } else if (word == BTH_COMMAND_HOLD && extended) {
        c.kind = Command_Hold;
    } else if (word == BTH_COMMAND_ARPEGGIO && extended) {
        c.kind = Command_Arpeggio;
    } else if (word == BTH_COMMAND_NOTE_BIAS && r->bias_command) {
        c.kind = Command_NoteBias;
    }
    return c;
}

// The value of a word the replayer uses
static uint16_t word_value(const BthPlayer* p, const BthWord* word) {
    return word->in_module ? bthm_u16(&p->work, word->address) : word->constant;
}

// The repeat registers: where a playing sample loops to. A voice that plays its instrument once or holds a note
// repeats the silent sample.
static void write_repeat_registers(const BthPlayer* p, BthVoice* v) {
    if (v->one_shot || v->hold) {
        bth_set_address(p, v, p->replayer.silent, 1);
        return;
    }
    const uint32_t start = v->sample + (uint32_t)(int32_t)v->loop_start;
    const uint32_t end = v->sample + (uint32_t)(int32_t)v->loop_end;
    bth_set_address(p, v, start, (uint16_t)((end - start) >> 1));
}

// The command stream ended: the next track entry's stream. The entry whose stream address is 0 ends the track list.
static void next_stream(const BthPlayer* p, BthVoice* v) {
    v->track += TRACK_ENTRY_BYTES;
    if (bthm_track_entry(&p->work, v->track).stream == 0) {
        bth_wrap_track(v);
    }
    const BthTrackEntry entry = bthm_track_entry(&p->work, v->track);
    v->command = entry.stream;
    v->note_base = entry.note_base;
}

// The instrument command: the record's volume, sample and lengths
static void set_instrument(const BthPlayer* p, BthVoice* v, const Command* command) {
    const BthInstrument i = bthm_instrument(&p->work, command->words.record);
    v->one_shot = i.one_shot;
    v->paula.volume = bth_paula_volume(i.volume);
    v->length_words = (i.one_shot ? i.one_shot_length : (uint16_t)i.loop_start) >> 1;
    v->sample = i.sample + p->replayer.sample_base;
    bth_set_address(p, v, v->sample, v->length_words);
    v->loop_start = i.loop_start;
    v->loop_end = i.loop_end;
    v->one_shot_length = i.one_shot_length;
    v->command += BTH_COMMAND_INSTRUMENT_BYTES;
    v->instrument = bth_instrument_index(p, command->words.record, BTH_INSTRUMENT_BYTES);
}

// The length of a note or command: its length word times the tempo, as a 16 bit product
static uint16_t note_duration(const BthPlayer* p, const Command* command) {
    return (uint16_t)((uint32_t)command->words.operand * word_value(p, &p->replayer.tempo));
}

// The period table entry of a note number: the table is indexed by the note, the track's note offset and the
// module's, in 16 bits
static uint16_t table_period(const BthPlayer* p, const BthVoice* v, uint16_t note) {
    const BthReplayer* r = &p->replayer;
    uint16_t index = (uint16_t)(note - 1 + (uint16_t)v->note_base);
    index = (uint16_t)(index + (r->bias_in_record ? v->note_bias : word_value(p, &r->note_bias)));
    return bth_table_word(p, r->period_table + (uint32_t)(int32_t)(int16_t)(uint16_t)(index << 1));
}

static void start_note(const BthPlayer* p, BthVoice* v, const Command* command) {
    v->target_period = table_period(p, v, command->words.word);
    v->duration = note_duration(p, command);
    v->command += BTH_COMMAND_SHORT_BYTES;
    v->note = nuade_period_to_note(v->target_period);
}

// What the commands of a row leave for the rest of the voice routine
typedef struct RowOutcome {
    bool restart; // the commands switched the voice's DMA off: it has to be switched on again
    bool hold;    // a hold command ended the row: the sample registers keep their value
} RowOutcome;

// DMACON set right after a DMACON clear, with no wait in between: Paula has not seen the channel go off, so a voice
// that was playing at the start of the tick plays on (the registers written meanwhile are its next repeat). A voice
// that was off starts.
static void dma_on_after_off(BthVoice* v) {
    if (v->was_running) {
        v->paula.dma = true;
        v->paula.dma_started = false;
    } else {
        nuade_paula_channel_dma_on(&v->paula);
    }
}

// The slide command sets the step; a step of 0 ends the slide
static void set_slide(BthVoice* v, const Command* command) {
    v->slide_step = command->words.operand;
    v->slide = v->slide_step != 0;
    if (v->slide) {
        dma_on_after_off(v);
    }
    v->command += BTH_COMMAND_SHORT_BYTES;
}

// The legato command switches legato on or off
static void set_legato(BthVoice* v, const Command* command) {
    v->legato = command->words.operand != 0;
    if (v->legato) {
        dma_on_after_off(v);
    }
    v->command += BTH_COMMAND_SHORT_BYTES;
}

// The commands of the basic replayer
static RowOutcome read_commands_basic(const BthPlayer* p, BthVoice* v) {
    RowOutcome outcome = { 0 };
    if (!v->slide && !v->legato) {
        nuade_paula_channel_dma_off(&v->paula);
    }
    for (int guard = 0; guard < BTH_MAX_COMMANDS_PER_TICK; guard++) {
        const Command command = read_command(p, v);
        switch (command.kind) {
            case Command_End:
                next_stream(p, v);
                break;
            case Command_Instrument:
                nuade_paula_channel_dma_off(&v->paula);
                set_instrument(p, v, &command);
                break;
            case Command_Slide:
                set_slide(v, &command);
                break;
            case Command_Legato:
                set_legato(v, &command);
                break;
            default:
                start_note(p, v, &command);
                return outcome;
        }
    }
    return outcome;
}

// The hold command: the note is held at its end. The voice repeats the silent sample from the end of the loop on.
static void hold_note(const BthPlayer* p, BthVoice* v, const Command* command, RowOutcome* outcome) {
    v->hold = true;
    if (!v->one_shot) {
        nuade_paula_channel_dma_off(&v->paula);
        outcome->restart = true;
        const uint32_t start = v->sample + (uint32_t)(int32_t)v->loop_end;
        const uint16_t remaining = (uint16_t)(v->one_shot_length - (uint16_t)v->loop_end);
        bth_set_address(p, v, start, remaining >> 1);
    }
    v->duration = note_duration(p, command);
    v->command += BTH_COMMAND_SHORT_BYTES;
    outcome->hold = true;
}

// The arpeggio command: a count and that many note numbers that the voice cycles through
static void start_arpeggio(const BthPlayer* p, BthVoice* v, const Command* command) {
    v->arpeggio = true;
    v->duration = note_duration(p, command);
    v->arpeggio_count = command->words.count;
    v->arpeggio_table = v->command + BTH_COMMAND_ARPEGGIO_HEADER_BYTES;
    v->arpeggio_index = 0;
    // The size of the command is a 16 bit sum, sign extended
    const uint16_t bytes = (uint16_t)(2u * command->words.count + BTH_COMMAND_ARPEGGIO_HEADER_BYTES);
    v->command += (uint32_t)(int32_t)(int16_t)bytes;
}

// The commands of the extended replayer: every command but the hold switches the voice's DMA off, so the voice restarts
// at the end of the routine
static RowOutcome read_commands_extended(const BthPlayer* p, BthVoice* v) {
    RowOutcome outcome = { 0 };
    v->hold = false;
    for (int guard = 0; guard < BTH_MAX_COMMANDS_PER_TICK; guard++) {
        const Command command = read_command(p, v);
        if (command.kind == Command_End) {
            next_stream(p, v);
            continue;
        }
        if (command.kind != Command_Hold) {
            v->arpeggio = false;
            nuade_paula_channel_dma_off(&v->paula);
            outcome.restart = true;
        }
        switch (command.kind) {
            case Command_Instrument:
                set_instrument(p, v, &command);
                break;
            case Command_Slide:
                set_slide(v, &command);
                break;
            case Command_Legato:
                set_legato(v, &command);
                break;
            case Command_NoteBias:
                v->note_bias = command.words.operand;
                v->command += BTH_COMMAND_SHORT_BYTES;
                break;
            case Command_Hold:
                hold_note(p, v, &command, &outcome);
                return outcome;
            case Command_Arpeggio:
                start_arpeggio(p, v, &command);
                return outcome;
            default:
                start_note(p, v, &command);
                return outcome;
        }
    }
    return outcome;
}

// The registers a new note starts the sample with, unless the voice is legato or sliding
static void write_start_registers(const BthPlayer* p, BthVoice* v) {
    if (!v->slide && !v->legato) {
        bth_set_address(p, v, v->sample, v->length_words);
    }
}

// Whether period `a` is below `b`: the comparison after a slide step is unsigned or signed
static bool period_below(const BthPlayer* p, uint16_t a, uint16_t b) {
    return p->replayer.slide_signed ? (int16_t)a < (int16_t)b : a < b;
}

// The slide moves the period one step toward the note's and stops there
static void slide_period(const BthPlayer* p, BthVoice* v) {
    uint16_t period = v->period;
    if (period == v->target_period) {
        return;
    }
    if (period > v->target_period) {
        period = (uint16_t)(period - v->slide_step);
        if (period_below(p, period, v->target_period)) {
            period = v->target_period;
        }
    } else {
        period = (uint16_t)(period + v->slide_step);
        if (!period_below(p, period, v->target_period)) {
            period = v->target_period;
        }
    }
    v->period = period;
}

// The arpeggio takes the period of the next table entry on every tick
static void arpeggio_period(const BthPlayer* p, BthVoice* v) {
    const uint32_t offset = (uint32_t)(int32_t)(int16_t)v->arpeggio_index << 1;
    v->target_period = table_period(p, v, bthm_u16(&p->work, v->arpeggio_table + offset));
    bth_advance_arpeggio(v);
    v->period = v->target_period;
}

void bth_update_voice(BthPlayer* p, BthVoice* v) {
    const bool extended = p->replayer.engine == BthEngine_Extended;
    RowOutcome outcome = { 0 };
    v->was_running = v->paula.dma;
    write_repeat_registers(p, v);
    v->duration--;
    if (v->duration == 0) {
        outcome = extended ? read_commands_extended(p, v) : read_commands_basic(p, v);
        if (!outcome.hold) {
            write_start_registers(p, v);
        }
    }
    if (v->arpeggio) {
        arpeggio_period(p, v);
    } else if (v->slide) {
        slide_period(p, v);
    } else {
        v->period = v->target_period;
    }
    v->paula.period = v->period;
    // The extended replayer restarts the voice after a command switched it off; the basic one, when the period or
    // the voice allows it
    const bool free_running = !v->slide && !v->legato;
    const bool restart = extended ? outcome.restart : (v->period & 0xFF) != 0;
    if (restart || free_running) {
        nuade_paula_channel_dma_on(&v->paula);
    }
}
