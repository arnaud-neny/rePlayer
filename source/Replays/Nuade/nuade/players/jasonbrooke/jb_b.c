// SPDX-License-Identifier: MIT
// Jason C. Brooke player, version B: the replayer of the early game drivers (Feud, Pac-Land, World Darts), translated
// from the module code.
//
// The four channels loop one 128-byte block of a sample bank each, forever; the driver only picks the block, the
// period and the volume once per tick. The interrupt: when the tempo counter runs out, the sequencer steps (voices
// 0 to 3); then every voice ticks (envelope, pitch sequence, vibrato, slide), and the registers are written.

#include "jb_internal.h"
#include <string.h>

typedef JbBVoice Voice;

#define BLOCK_WORDS 0x40
#define BLOCK_SHIFT 7

// Bits of Voice.flags
enum {
    Flag_Odd = 0x01, // toggles every tick
    Flag_Slide = 0x04,
    Flag_NoteMoves = 0x08, // the note number changes on the ticks between steps
    Flag_VibratoHold = 0x10,
    Flag_VibratoUp = 0x20,
    Flag_Vibrato = 0x40,
    Flag_NoteDown = 0x80,
    Flag_Kept = Flag_VibratoHold | Flag_VibratoUp | Flag_Vibrato, // what a new row keeps
};

// Voice tick

// The volume envelope: the level byte counts down in steps of 0x40 (the top two bits are a duration), 0xC0 and above
// holds; when it runs out the next level is read from the envelope data
static void envelope_step(const JbModule* m, const JbBConfig* c, Voice* v) {
    const JbBTables* t = &c->tables;
    if (v->envelope_volume >= 0xc0) {
        return;
    }
    const uint8_t before = v->envelope_volume;
    v->envelope_volume = (uint8_t)(before - 0x40);
    if (before >= 0x40) {
        return;
    }
    const uint8_t start = jbm_u8(m, t->envelope_start + v->envelope_number);
    const uint32_t offset = c->envelope_wraps ? (uint8_t)(start + v->envelope_position) : start + v->envelope_position;
    v->envelope_position++;
    v->envelope_volume = jbm_u8(m, t->envelope + offset);
}

// The vibrato: a depth that ramps up and down by the speed byte, offsetting the period by depth - span / 2
static uint16_t vibrato(const JbBConfig* c, Voice* v, uint16_t period) {
    const uint8_t span = v->vibrato_depth;
    uint8_t depth = v->vibrato_sum;
    const bool hold = c->vibrato_hold && (v->flags & Flag_VibratoHold) != 0 && (v->flags & Flag_Odd) == 0;
    if (!hold) {
        if ((v->flags & Flag_VibratoUp) != 0) {
            depth = (uint8_t)(depth + v->vibrato_speed);
            if (depth >= span) {
                depth = span;
                v->flags ^= Flag_VibratoUp;
            }
        } else if (depth < v->vibrato_speed) {
            depth = 0;
            v->flags ^= Flag_VibratoUp;
        } else {
            depth = (uint8_t)(depth - v->vibrato_speed);
        }
        v->vibrato_sum = depth;
    }
    return (uint16_t)(period + depth - (span >> 1));
}

static void voice_tick(JbB* b, JbModule* m, Voice* v) {
    const JbBConfig* c = &b->config;
    const JbBTables* t = &c->tables;
    envelope_step(m, c, v);
    unsigned volume = v->envelope_volume & 0x3f;
    if (t->master != 0) {
        const uint8_t scale = jbm_u8(m, t->master);
        if (scale < 0x40) {
            volume = (volume * scale) >> 6;
        }
    }
    v->volume = (uint8_t)volume;

    // pitch: the sequence value (a byte with the top bit set loops) and the note and transpose index the period table
    uint8_t position = (uint8_t)(v->sequence_position + 1);
    uint8_t pitch = jbm_u8(m, t->pitch_sequences + position);
    if (pitch >= 0x80) {
        position = v->sequence_loop;
        pitch &= 0x7f;
    }
    v->sequence_position = position;
    pitch = (uint8_t)(pitch + v->note + v->transpose);
    uint16_t period = jbm_u16(m, t->periods + (uint8_t)(pitch << 1));
    if ((v->flags & Flag_Vibrato) != 0) {
        period = vibrato(c, v, period);
    }
    v->flags ^= Flag_Odd;
    if ((v->flags & Flag_Slide) != 0) {
        if (v->slide_delay != 0) {
            v->slide_delay--;
        } else {
            v->slide_sum = (uint16_t)(v->slide_sum + (uint16_t)(int16_t)v->slide_step);
            period = (uint16_t)(period + v->slide_sum);
        }
    }
    v->period = period;
}

// Sequencer

typedef enum CommandResult {
    CommandResult_Continue,
    CommandResult_EndRow,
} CommandResult;

// Runs `command` for voice `index`; `p` is the stream offset after the command byte and moves over its arguments
static CommandResult run_command(JbB* b, JbModule* m, int index, JbBCommand command, uint32_t* p) {
    const JbBConfig* c = &b->config;
    Voice* v = &b->voices[index];
    switch (command) {
        case JbBCommand_KeyOff:
            v->envelope_volume = 0xc0;
            return CommandResult_EndRow;
        case JbBCommand_ClearFlags:
            v->flags = 0;
            break;
        case JbBCommand_Slide:
            v->slide_step = (int8_t)jbm_u8(m, *p);
            v->slide_sum = 0;
            v->flags |= Flag_Slide;
            v->slide_delay = jbm_u8(m, *p + 1);
            *p += 2;
            break;
        case JbBCommand_NoteDown:
            v->flags |= Flag_NoteDown | Flag_NoteMoves;
            break;
        case JbBCommand_NoteUp:
            v->flags |= Flag_NoteMoves;
            break;
        case JbBCommand_NextTrack: { // a zero word ends the track list and the list starts over
            const uint32_t list = jb_offset16(v->track_list);
            uint8_t position = v->track_position;
            if (jbm_u16(m, list + position) == 0) {
                position = 0;
                b->looped |= jb_voice_wrapped(b->wrapped, index);
            }
            *p = jb_offset16(jbm_u16(m, list + position));
            v->track_position = (uint8_t)(position + 2);
            break;
        }
        case JbBCommand_Vibrato: {
            v->vibrato_speed = jbm_u8(m, *p);
            const unsigned doubled = 2u * jbm_u8(m, *p + 1);
            *p += 2;
            if (c->vibrato_hold) {
                v->flags = doubled > 0xff ? Flag_Vibrato | Flag_VibratoHold : Flag_Vibrato;
                v->vibrato_depth = (uint8_t)doubled;
                v->vibrato_sum = (uint8_t)doubled >> 1;
            } else {
                v->flags = Flag_Vibrato;
                v->vibrato_sum = doubled >> 1;
                v->vibrato_depth = (uint8_t)doubled;
            }
            break;
        }
        case JbBCommand_Stop: // the driver stops running and leaves the tick
            b->running = false;
            b->stopped = true;
            for (int i = 0; i < JB_VOICES; i++) {
                nuade_paula_channel_dma_off(&b->voices[i].paula);
            }
            break;
        case JbBCommand_Transpose:
            v->transpose = jbm_u8(m, (*p)++);
            break;
        case JbBCommand_AdkCon:
            (*p)++; // modulation bits: no module uses them
            break;
        case JbBCommand_Count:
        case JbBCommand_Unknown:
            break;
    }
    return CommandResult_Continue;
}

static void sequence_step(JbB* b, JbModule* m, int index) {
    const JbBTables* t = &b->config.tables;
    Voice* v = &b->voices[index];
    if (v->halted) {
        return;
    }
    v->row_counter--;
    if (v->row_counter != 0) {
        if ((v->flags & Flag_NoteMoves) != 0) {
            v->note = (uint8_t)(v->note + ((v->flags & Flag_NoteDown) != 0 ? -1 : 1));
        }
        return;
    }
    v->flags &= Flag_Kept;
    uint32_t p = jb_offset16(v->stream);
    for (uint32_t bytes = 0;; bytes++) {
        if (bytes == JB_MAX_STEP_BYTES) {
            v->halted = true;
            return;
        }
        const uint8_t byte = jbm_u8(m, p++);
        if (byte < 0x80) {
            v->note = byte;
            v->note_started = true;
            v->envelope_position = 0;
            v->envelope_volume = 0;
            v->sequence_position = v->sequence_loop;
            break;
        } else if (byte < 0x90) {
            const unsigned command = byte - 0x80u;
            const CommandResult result
                = run_command(b, m, index, command < JbBCommand_Count ? (JbBCommand)command : JbBCommand_Unknown, &p);
            if (b->stopped) {
                return;
            } else if (result == CommandResult_EndRow) {
                break;
            }
        } else if (byte >= 0xe0) {
            v->row_length = byte - 0xdf;
        } else if (byte >= 0xb8) {
            v->envelope_number = byte - 0xb8;
        } else if (byte >= 0xa0) {
            v->block = byte - 0xa0;
        } else {
            v->sequence_loop = jbm_u8(m, t->sequence_codes + byte + 0x60u);
        }
    }
    v->row_counter = v->row_length;
    v->stream = (uint16_t)p;
}

void jb_b_start(JbB* b, const JbModule* m, int subsong) {
    const JbBTables* t = &b->config.tables;
    const JbBSong song = jb_b_song(m, t, subsong);
    b->tempo = song.tempo;
    b->counter = 1;
    b->running = true;
    b->stopped = false;
    b->looped = false;
    memset(b->wrapped, 0, sizeof(b->wrapped));
    for (int i = 0; i < JB_VOICES; i++) {
        Voice* v = &b->voices[i];
        *v = (Voice) { .track_position = 2,
                       .track_list = song.track_lists[i],
                       .stream = jbm_u16(m, jb_offset16(song.track_lists[i])),
                       .row_counter = 1 };
        // The init routine writes AUDxLEN, then switches the DMA on. Pac-Land also points AUDxLC at the first block;
        // the others leave it where the previous owner of the channel left it. Volume is 0 until the first tick.
        v->paula.len_words = BLOCK_WORDS;
        v->paula.lc = jbm_sample(m, t->blocks);
        nuade_paula_channel_dma_on(&v->paula);
    }
}

JbTickResult jb_b_tick(JbB* b, JbModule* m, NuadeVoices* voices) {
    const JbBTables* t = &b->config.tables;
    b->looped = false;
    if (b->running) {
        b->counter--;
        if (b->counter == 0) {
            b->counter = b->tempo;
            for (int i = 0; i < JB_VOICES && !b->stopped; i++) {
                sequence_step(b, m, i);
            }
        }
        for (int i = 0; i < JB_VOICES && !b->stopped; i++) {
            voice_tick(b, m, &b->voices[i]);
        }
    }
    if (b->running) {
        for (int i = 0; i < JB_VOICES; i++) {
            Voice* v = &b->voices[i];
            v->paula.period = v->period;
            v->paula.volume = v->volume;
            v->paula.lc = jbm_sample(m, t->blocks + ((uint32_t)v->block << BLOCK_SHIFT));
        }
    }
    for (int i = 0; i < JB_VOICES; i++) {
        nuade_paula_channel_sync(&b->voices[i].paula, (uint32_t)i, voices);
    }
    return (JbTickResult) { .looped = b->looped, .ended = b->stopped };
}
