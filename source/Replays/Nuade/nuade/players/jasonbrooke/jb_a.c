// SPDX-License-Identifier: MIT
// Jason C. Brooke player, version A: the replayer of the 1987-90 game drivers, translated from the module code.
//
// One interrupt (50 Hz) does, in this order: the PWM step (a pulse waveform that sweeps or a noise pattern), the tick
// of each voice (sample start, volume, envelope, pitch sequence, vibrato, slide, period), and, when the tempo
// accumulator carries, the sequencer step of each voice from the last to the first (rows of track bytes).
//
// The sound effect path of the driver (voice flag set by the game) never runs under the eagleplayer, so it is left
// out; so is the second call the game makes to restart a song. A command byte the driver does not know would run
// arbitrary code: the voice stops stepping there.

#include "jb_internal.h"
#include <string.h>

typedef JbAVoice Voice;

// Bits of Voice.flags
typedef enum JbAFlag {
    JbAFlag_Odd = 0x01,        // toggles every tick
    JbAFlag_NoEnvelope = 0x02, // the next note keeps the envelope
    JbAFlag_Slide = 0x04,
    JbAFlag_NoteMoves = 0x08, // the note number changes on the ticks between steps
    JbAFlag_VibratoRising = 0x10,
    JbAFlag_Tie = 0x20, // the next note does not restart the DMA
    JbAFlag_Vibrato = 0x40,
    JbAFlag_NoteDown = 0x80,
    JbAFlag_Kept = JbAFlag_Vibrato | JbAFlag_VibratoRising, // what a new row keeps
} JbAFlag;

// An accumulator the driver adds a step to every tick; true on a carry
static bool accumulate(uint8_t* accumulator, uint8_t step) {
    const unsigned sum = (unsigned)*accumulator + step;
    *accumulator = (uint8_t)sum;
    return sum > 0xff;
}

// The PWM step

static void pwm_sweep(JbAGlobals* g, JbModule* m, const JbATables* t) {
    const uint8_t limit = g->pwm_phase != 0 ? g->pwm_high : g->pwm_low;
    if (g->pwm_index < limit) {
        jbm_set8(m, t->pwm + g->pwm_index, 0x80);
        g->pwm_index++;
    } else if (g->pwm_index > limit) {
        jbm_set8(m, t->pwm + g->pwm_index, 0);
        g->pwm_index--;
    } else {
        g->pwm_phase = (uint8_t)~g->pwm_phase;
    }
}

// The noise form: the index advances by the seed, the seed rotates into the waveform
static void pwm_noise(JbAGlobals* g, JbModule* m, const JbATables* t) {
    uint8_t index = g->pwm_index;
    uint8_t seed = g->pwm_phase;
    index = (uint8_t)(index + seed);
    seed = (uint8_t)((seed >> 1) | (seed << 7));
    jbm_set8(m, t->pwm + index, seed);
    const uint8_t limit = g->pwm_phase2 != 0 ? g->pwm_high : g->pwm_low;
    g->pwm_phase = 1;
    if (index >= limit) {
        if (index == limit) {
            g->pwm_phase2 = (uint8_t)~g->pwm_phase2;
        }
        g->pwm_phase = 0xff;
    }
    g->pwm_index = index;
}

// Voice tick

// Starts the volume envelope `number`
static void envelope_start(const JbModule* m, const JbATables* t, Voice* v, uint8_t number) {
    const uint8_t position = jbm_u8(m, t->envelope_start + number);
    v->envelope_position = position;
    v->envelope_volume = jbm_u8(m, t->envelope_volume + position);
    v->envelope_timer = 1;
}

// Restart the sample at the next tick
static void sample_restart(Voice* v) {
    v->dma_phase = 2;
    nuade_paula_channel_dma_off(&v->paula);
}

// The tick after a note starts the sample (AUDxLC/LEN, DMA on); the one after that points the registers at the loop,
// or at the silent sample for a one-shot. Returns the period multiplier of the sample.
static uint16_t sample_setup(const JbModule* m, const JbATables* t, Voice* v, uint8_t index) {
    const JbASample sample = jb_a_sample(m, t, index);
    if (v->dma_phase == 0) {
        return sample.multiplier;
    }
    uint32_t address = sample.address;
    uint16_t length = sample.length;
    int16_t loop = 0;
    v->dma_phase--;
    if (v->dma_phase == 0) {
        loop = sample.loop;
        if (loop < 0) {
            address = t->silence_address;
            length = jbm_u16(m, t->silence_length);
            loop = 0;
        }
    }
    v->paula.len_words = (uint16_t)(length - (uint16_t)loop) >> 1;
    v->paula.lc = jbm_sample(m, address + (uint32_t)(int32_t)loop);
    nuade_paula_channel_dma_on(&v->paula);
    return sample.multiplier;
}

// The volume envelope: a step list of (delta, duration); the volume gains the current delta each tick
static void envelope_step(const JbModule* m, const JbATables* t, Voice* v) {
    if (v->envelope_timer == 0) {
        return;
    }
    uint8_t position = v->envelope_position;
    v->envelope_timer--;
    if (v->envelope_timer == 0) {
        v->envelope_timer = jbm_u8(m, t->envelope + position + 1u);
        if (v->envelope_timer == 0) {
            return;
        }
        position = (uint8_t)(position + 2);
        v->envelope_position = position;
    }
    v->envelope_volume = (uint8_t)(v->envelope_volume + jbm_u8(m, t->envelope + position));
}

// The pitch sequence of the old driver: offsets added to the note. A byte with the top bit set loops to the sequence's
// loop start and gives its low 7 bits as the offset; 0xFF only holds the position and gives 0 (the Rockford form gives
// 0x7F).
static uint8_t sequence_old(const JbModule* m, const JbAConfig* c, Voice* v) {
    const uint8_t position = (uint8_t)(v->sequence_position + 1);
    const uint8_t value = jbm_u8(m, c->tables.pitch_sequences + position);
    if (value < 0x80) {
        v->sequence_position = position;
        return value;
    } else if (value == 0xff && !c->sequence_mask_form) {
        return 0;
    }
    v->sequence_position = v->sequence_loop;
    return value & 0x7f;
}

// The pitch sequence of the new driver: the sequence program gives a start; a positive byte ends the sequence (the
// next tick starts over), 0xFF holds the position and gives 0
static uint8_t sequence_new(const JbModule* m, const JbAConfig* c, Voice* v) {
    const uint8_t start = jbm_u8(m, c->tables.sequence_starts + v->sequence_program);
    const uint8_t position = (uint8_t)(start + v->sequence_position);
    const uint8_t value = jbm_u8(m, c->tables.pitch_sequences + position);
    if (value == 0xff) {
        return 0;
    }
    v->sequence_position = value < 0x80 ? 0 : (uint8_t)(v->sequence_position + 1);
    return value;
}

// The vibrato offsets the period by a triangle that the speed byte walks; on the way back (bit 4 clear) the driver
// subtracts the speed from the counter but then offsets by the speed, not the counter
static uint16_t vibrato(Voice* v, uint16_t period) {
    uint8_t swing = v->vibrato_speed != 0 ? v->vibrato_speed : (uint8_t)(v->flags & JbAFlag_Odd);
    bool turn;
    if ((v->flags & JbAFlag_VibratoRising) != 0) {
        swing = (uint8_t)(swing + v->vibrato_sum);
        v->vibrato_sum = swing;
        turn = swing == v->vibrato_depth;
    } else {
        v->vibrato_sum = (uint8_t)(v->vibrato_sum - swing);
        turn = v->vibrato_sum == 0;
    }
    if (turn) {
        v->flags ^= JbAFlag_VibratoRising;
    }
    return (uint16_t)(period + swing - (v->vibrato_depth >> 1));
}

static void voice_tick(JbA* a, JbModule* m, Voice* v) {
    const JbAConfig* c = &a->config;
    const JbATables* t = &c->tables;
    if (a->globals.play == 0) {
        v->paula.volume = 0;
        nuade_paula_channel_dma_off(&v->paula);
        return;
    }
    const uint16_t multiplier = sample_setup(m, t, v, v->sample);

    // volume: the envelope level (6 bits) scaled by the master volume and the voice volume
    const uint32_t level = ((uint32_t)(v->envelope_volume >> 2) * a->globals.master) & 0xffffu;
    v->paula.volume = (uint8_t)((level * v->volume) >> 16) & 0x7f;
    envelope_step(m, t, v);

    // pitch: the sequence value, the note and the transposes index the period table
    uint8_t pitch = c->new_syntax ? sequence_new(m, c, v) : sequence_old(m, c, v);
    if (!c->pitch_uses_sequence) {
        pitch = 0;
    }
    pitch = (uint8_t)(pitch + v->note + v->transpose);
    if (t->global_transpose != 0) {
        pitch = (uint8_t)(pitch + a->globals.transpose);
    }
    const uint32_t scaled = (uint32_t)jbm_u16(m, t->periods + (uint8_t)(pitch << 1)) * multiplier;
    uint16_t period = (uint16_t)((scaled << 3) >> 16);
    if ((v->flags & JbAFlag_Vibrato) != 0) {
        period = vibrato(v, period);
    }
    v->flags ^= JbAFlag_Odd;
    if ((v->flags & JbAFlag_Slide) != 0) {
        if (v->slide_delay != 0) {
            v->slide_delay--;
        } else {
            v->slide_sum = (uint16_t)(v->slide_sum + v->slide_step);
            period = (uint16_t)(period + v->slide_sum);
        }
    }
    v->paula.period = period;
}

// Sequencer

typedef enum CommandResult {
    CommandResult_Continue, // read the next byte
    CommandResult_EndRow,   // the row is decoded
    CommandResult_Abort,    // the driver left the step without saving its position
} CommandResult;

// Runs command `command` of voice `index`; `p` is the stream offset after the command byte and moves over its arguments
static CommandResult run_command(JbA* a, JbModule* m, int index, JbACommand command, uint32_t* p) {
    const JbATables* t = &a->config.tables;
    Voice* v = &a->voices[index];
    JbAGlobals* g = &a->globals;
    switch (command) {
        case JbACommand_Rest:
            v->sample = 0;
            sample_restart(v);
            return CommandResult_EndRow;
        case JbACommand_ClearFlags:
            v->flags = 0;
            break;
        case JbACommand_Slide:
            v->slide_step = jbm_s16(m, *p);
            v->slide_sum = 0;
            v->flags |= JbAFlag_Slide;
            v->slide_delay = jbm_u8(m, *p + 2);
            *p += 3;
            break;
        case JbACommand_NoteDown:
            v->flags |= JbAFlag_NoteDown | JbAFlag_NoteMoves;
            break;
        case JbACommand_NoteUp:
            v->flags |= JbAFlag_NoteMoves;
            break;
        case JbACommand_NextTrack: {
            uint8_t position = (uint8_t)(v->track_position + 2);
            if (position == v->track_end) {
                position = v->track_loop;
                a->looped |= jb_voice_wrapped(a->wrapped, index);
            }
            const uint32_t list = jb_offset16((uint16_t)(v->track_list + position));
            *p = jb_offset16(jbm_u16(m, list));
            v->track_position = position;
            break;
        }
        case JbACommand_Vibrato:
            v->vibrato_speed = jbm_u8(m, *p);
            v->vibrato_sum = jbm_u8(m, *p + 1);
            v->vibrato_depth = (uint8_t)(v->vibrato_sum * 2);
            v->flags |= JbAFlag_Vibrato;
            *p += 2;
            break;
        case JbACommand_Stop:
            g->play = 0;
            for (int i = 0; i < JB_VOICES; i++) {
                nuade_paula_channel_dma_off(&a->voices[i].paula);
            }
            a->stopped = true;
            return CommandResult_Abort;
        case JbACommand_Transpose:
            v->transpose = jbm_u8(m, (*p)++);
            break;
        case JbACommand_AdkCon:
            (*p)++; // modulation bits: no module uses them
            break;
        case JbACommand_Tie:
            v->flags |= JbAFlag_Tie;
            break;
        case JbACommand_TieNoEnvelope:
            v->flags |= JbAFlag_Tie | JbAFlag_NoEnvelope;
            break;
        case JbACommand_NoEnvelope:
            v->flags |= JbAFlag_NoEnvelope;
            break;
        case JbACommand_PwmSet:
            g->pwm_high = jbm_u8(m, *p);
            g->pwm_low = jbm_u8(m, *p + 1);
            jbm_set16(m, t->pwm_length, (uint16_t)(jbm_u8(m, *p + 2) + 1));
            v->sample_next = 1;
            *p += 3;
            break;
        case JbACommand_Volume:
            v->volume = jbm_u8(m, (*p)++);
            break;
        case JbACommand_SkipByte:
            (*p)++;
            break;
        case JbACommand_GlobalTranspose:
            g->transpose = jbm_u8(m, (*p)++);
            break;
        case JbACommand_Tempo:
            g->tempo = jbm_u8(m, (*p)++);
            break;
        case JbACommand_ArpProgram:
            v->sequence_program = jbm_u8(m, (*p)++);
            break;
        case JbACommand_LoopStart:
            v->loop_count = jbm_u8(m, (*p)++);
            v->loop_position = (uint16_t)*p;
            break;
        case JbACommand_LoopTwice:
            v->loop_count = 2;
            v->loop_position = (uint16_t)*p;
            break;
        case JbACommand_LoopEnd:
            v->loop_count--;
            if (v->loop_count != 0) {
                *p = jb_offset16(v->loop_position);
            }
            break;
        case JbACommand_Invalid:
            return CommandResult_Abort;
    }
    return CommandResult_Continue;
}

// A note byte: start the envelope and the sample
static void note_start(JbA* a, const JbModule* m, Voice* v, uint8_t note) {
    const JbAConfig* c = &a->config;
    v->note = note;
    v->note_started = true;
    if ((v->flags & JbAFlag_NoEnvelope) == 0) {
        envelope_start(m, &c->tables, v, v->envelope_number);
    }
    v->sequence_position = c->new_syntax ? 0 : v->sequence_loop;
    v->sample = v->sample_next;
    if ((v->flags & JbAFlag_Tie) == 0) {
        sample_restart(v);
    }
}

// What a track byte is: a note ends the row, a command runs, anything else sets a voice parameter
typedef enum ByteKind {
    ByteKind_Parameter,
    ByteKind_Note,
    ByteKind_Command,
} ByteKind;

typedef struct DecodedByte {
    ByteKind kind;
    uint8_t note;
    JbACommand command;
} DecodedByte;

// The old syntax: notes 0 .. 0x7F, commands 0x80 up to the note limit, then (counting up) the row length, the envelope
// number, the sample number and the pitch sequence loop start
static DecodedByte decode_old(const JbModule* m, const JbAConfig* c, Voice* v, uint8_t byte) {
    if (byte < 0x80) {
        return (DecodedByte) { .kind = ByteKind_Note, .note = byte };
    } else if (byte < c->note_limit) {
        const uint8_t index = byte - 0x80;
        return (DecodedByte) { .kind = ByteKind_Command,
                               .command = index < c->command_count ? c->commands[index] : JbACommand_Invalid };
    } else if (byte >= 0xe0) {
        v->row_length = byte - 0xdf;
    } else if (byte >= 0xc0) {
        v->envelope_number = byte - 0xc0;
    } else if (byte >= 0xa0) {
        v->sample_next = byte - 0xa0;
    } else {
        v->sequence_loop = jbm_u8(m, c->tables.sequence_codes + byte + 0x60u);
    }
    return (DecodedByte) { .kind = ByteKind_Parameter };
}

// The new syntax: row lengths up to the limit, commands 0x41 .. 0x5F, envelope numbers 0x60 .. 0x7F, notes 0x80 ..
// 0xDF, sample numbers 0xE0 and up
static DecodedByte decode_new(const JbAConfig* c, Voice* v, uint8_t byte) {
    if ((int8_t)byte < (int8_t)c->note_limit) {
        if (byte < c->note_limit) {
            v->row_length = byte;
        } else if (byte >= 0xe0) {
            v->sample_next = byte - 0xe0;
        } else {
            return (DecodedByte) { .kind = ByteKind_Note, .note = (uint8_t)(byte - 0x80) };
        }
    } else if (byte >= 0x60) {
        v->envelope_number = byte - 0x60;
    } else {
        const unsigned index = (unsigned)byte - 0x41u;
        return (DecodedByte) { .kind = ByteKind_Command,
                               .command = index < c->command_count ? c->commands[index] : JbACommand_Invalid };
    }
    return (DecodedByte) { .kind = ByteKind_Parameter };
}

// One sequencer step of voice `index`: between rows the note may move; on a row, read bytes until a note or rest
static void sequence_step(JbA* a, JbModule* m, int index) {
    const JbAConfig* c = &a->config;
    Voice* v = &a->voices[index];
    if (v->halted) {
        return;
    }
    v->row_counter--;
    if (v->row_counter != 0) {
        if ((v->flags & JbAFlag_NoteMoves) != 0) {
            v->note = (uint8_t)(v->note + ((v->flags & JbAFlag_NoteDown) != 0 ? -1 : 1));
        }
        return;
    }
    v->flags &= JbAFlag_Kept;
    uint32_t p = jb_offset16(v->stream);
    for (uint32_t bytes = 0;; bytes++) {
        if (bytes == JB_MAX_STEP_BYTES) {
            v->halted = true;
            return;
        }
        const uint8_t byte = jbm_u8(m, p++);
        const DecodedByte decoded = c->new_syntax ? decode_new(c, v, byte) : decode_old(m, c, v, byte);
        if (decoded.kind == ByteKind_Note) {
            note_start(a, m, v, decoded.note);
            break;
        } else if (decoded.kind == ByteKind_Command) {
            const CommandResult result = run_command(a, m, index, decoded.command, &p);
            if (result == CommandResult_Abort) {
                return;
            } else if (result == CommandResult_EndRow) {
                break;
            }
        }
    }
    v->row_counter = v->row_length;
    v->stream = (uint16_t)p;
}

void jb_a_start(JbA* a, const JbModule* m, int subsong) {
    const JbATables* t = &a->config.tables;
    a->globals = jb_a_globals(m, t);
    const JbASong song = jb_a_song(m, t, subsong);
    a->globals.tempo = song.tempo;
    a->globals.slow_acc = 0xff;
    a->globals.tempo_acc = 0xff;
    a->globals.play = 0xff;
    memset(a->wrapped, 0, sizeof(a->wrapped));
    a->looped = false;
    a->stopped = false;
    for (int i = 0; i < JB_VOICES; i++) {
        const JbATrack* track = &song.tracks[i];
        a->voices[i] = (Voice) { .track_end = track->end,
                                 .track_loop = track->loop,
                                 .track_list = track->list,
                                 .stream = jbm_u16(m, jb_offset16(track->list)),
                                 .volume = 0xff,
                                 .row_counter = 1 };
    }
}

JbTickResult jb_a_tick(JbA* a, JbModule* m, NuadeVoices* voices) {
    JbAGlobals* g = &a->globals;
    a->looped = false;
    if (!accumulate(&g->slow_acc, g->slow_down)) {
        if (a->config.pwm_noise) {
            pwm_noise(g, m, &a->config.tables);
        } else {
            pwm_sweep(g, m, &a->config.tables);
        }
        for (int i = 0; i < JB_VOICES; i++) {
            voice_tick(a, m, &a->voices[i]);
        }
        if (g->play != 0 && accumulate(&g->tempo_acc, g->tempo)) {
            // The stop command ends the loop over the voices
            for (int i = JB_VOICES - 1; i >= 0 && !a->stopped; i--) {
                sequence_step(a, m, i);
            }
        }
    }
    for (int i = 0; i < JB_VOICES; i++) {
        nuade_paula_channel_sync(&a->voices[i].paula, (uint32_t)i, voices);
    }
    return (JbTickResult) { .looped = a->looped, .ended = a->stopped };
}
