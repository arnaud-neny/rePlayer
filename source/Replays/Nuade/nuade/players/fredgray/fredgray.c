// SPDX-License-Identifier: MIT
// Fred Gray player: the engine, translated from the music code of the four known games (Eco, Black Lamp, Star Goose,
// Victory Road), with the changes the UADE eagleplayer makes to that code.
//
// One interrupt runs the request stage (a new subsong request or the next song section is started, and the section's
// routine runs), then the five voice routines for each of the three voices (volume envelope, slide, vibrato, command,
// pitch), then writes the Paula registers. A section routine is a list of FgOp the section decoder made from the code.
//
// What the eagleplayer changes: entry 0 of the section table calls the song end; and at the note start the
// `move sr` / `move (a7)+,sr` pair and the branch after it are removed, so a note always sets the voice's pitch
// at once (the slide target the branch would have skipped it for is set as well).

#include "fg_internal.h"
#include "nuade_tracker.h"
#include <stdlib.h>
#include <string.h>

#define COMMAND_SILENCE 1
// The kick mask: per voice one bit that skips the switch to silence and one that skips the switch to the sample
#define KICK_BITS (2 * FG_ACTIVE_VOICES)
#define COMMAND_FLAG_BITS 8

// Module records

// A voice record as the module initialises it
static FgVoice read_voice(const FgModule* m, uint32_t record) {
    FgVoice v;
    memset(&v, 0, sizeof(v));
    v.period = fg_u16(m, record + FgRecordField_Period);
    v.envelope_step = fg_u8(m, record + FgRecordField_EnvelopeStep);
    v.volume = fg_u8(m, record + FgRecordField_Volume);
    v.channel_shift = fg_u8(m, record + FgRecordField_ChannelShift);
    v.envelope_tick = fg_u8(m, record + FgRecordField_EnvelopeTick);
    v.active = fg_read_command(m, record + FgRecordField_Active);
    const uint8_t kick_bits = fg_u8(m, record + FgRecordField_KickBits);
    for (uint32_t bit = 0; bit < KICK_BITS; bit++) {
        v.kick_bits[bit] = ((kick_bits >> bit) & 1) != 0;
    }
    v.command = fg_u8(m, record + FgRecordField_Command);
    v.vibrato = (int16_t)fg_u16(m, record + FgRecordField_Vibrato);
    v.vibrato_left = fg_u8(m, record + FgRecordField_VibratoLeft);
    v.vibrato_phase = fg_u8(m, record + FgRecordField_VibratoPhase);
    return v;
}

// Paula registers

// AUDxLC points into the module (its padded copy: any AUDxLEN can follow, past the module it plays zeros). An address
// past the module points at memory the port does not have: none.
static void write_lc(FgPlayer* p, uint32_t voice, uint32_t offset) {
    const FgModule* m = &p->work;
    p->voices[voice].paula.lc = offset <= m->size ? (const int8_t*)(m->image + offset) : nullptr;
}

// AUDxLEN as the code writes it: 0 plays 65536 words.
static void write_len(FgPlayer* p, uint32_t voice, uint16_t words) {
    p->voices[voice].paula.len_words = words;
}

// The tick's last step: the registers of the three voices
static void write_registers(FgPlayer* p) {
    const FgLayout* l = &p->layout;
    for (uint32_t i = 0; i < FG_ACTIVE_VOICES; i++) {
        const uint16_t period = (uint16_t)(p->voices[i].period << 1);
        p->voices[i].paula.period = period;
        p->voices[i].paula.volume = (uint8_t)(p->voices[i].volume << 2);
    }
    if (!l->features.has_kick) {
        return;
    }
    // The switches to the silent wave all run before the switches to the sample
    for (uint32_t i = 0; i < FG_ACTIVE_VOICES; i++) {
        FgVoice* v = &p->voices[i];
        if (!p->song.kick_skip[i] && v->kick_sample) {
            write_lc(p, i, l->kick_silence);
            write_len(p, i, FG_WAVE_WORDS);
            v->kick_sample = false;
        }
    }
    for (uint32_t i = 0; i < FG_ACTIVE_VOICES; i++) {
        FgVoice* v = &p->voices[i];
        if (!p->song.kick_skip[FG_ACTIVE_VOICES + i] && !v->kick_sample) {
            write_lc(p, i, l->kick_sample);
            write_len(p, i, FG_SAMPLE_WORDS);
            v->kick_sample = true;
        }
    }
}

// The voice routines

// Eco: a 16 byte wave that changes every tick. Each tick adds the table (16 bytes) and the table shifted by the
// phase (0..15 on, so up to 30 bytes in). The locator checked that the wave is inside the module.
static void step_noise(FgPlayer* p) {
    const FgLayout* l = &p->layout;
    FgModule* m = &p->work;
    uint16_t phase = (uint16_t)((fg_u16(m, l->noise_phase) + 1) & 0xf);
    fg_set16(m, l->noise_phase, phase);
    for (uint32_t i = 0; i < FG_WAVE_BYTES; i++, phase++) {
        m->image[l->noise_wave + i] = (uint8_t)(fg_u8(m, l->noise_table + i) + fg_u8(m, l->noise_table + phase));
    }
}

// The volume envelope's step: the next entry of the envelope, capped by the section's cap. The module also looks up a
// second level (record byte 6) that nothing reads.
static void envelope_advance(FgPlayer* p, FgVoice* v) {
    const FgLayout* l = &p->layout;
    if (v->envelope_step == 0) {
        return;
    }
    v->envelope_step = (uint8_t)(v->envelope_step - 1);
    uint8_t level
        = fg_u8(&p->work, l->envelope + (uint8_t)((v->envelope_step ^ 0xf) + v->active.instrument.envelope_offset));
    if (l->features.has_cap && p->song.cap < level) {
        level = p->song.cap;
    }
    v->volume = level;
}

static void envelope_tick(FgPlayer* p, FgVoice* v) {
    v->envelope_tick = (uint8_t)(v->envelope_tick + 1);
    if ((uint8_t)(v->envelope_tick - v->active.instrument.envelope_speed) != 0) {
        return;
    }
    v->envelope_tick = 0;
    envelope_advance(p, v);
}

// Moves the pitch toward the target by the step. The words are sign extended and compared as unsigned longs, like the
// module's cmpa.w does.
static void slide(FgVoice* v) {
    FgCommand* a = &v->active;
    if (a->pitch == a->target) {
        return;
    }
    const uint32_t from = (uint32_t)(int32_t)(int16_t)a->pitch;
    const uint32_t to = (uint32_t)(int32_t)(int16_t)a->target;
    const uint32_t step = (uint32_t)(int32_t)(int16_t)a->step;
    a->pitch = (uint16_t)(from > to ? from - step : from + step);
}

static void vibrato(FgPlayer* p, FgVoice* v) {
    v->vibrato_phase = (uint8_t)((v->vibrato_phase + 1) & 0xf);
    const uint8_t index = (uint8_t)(v->vibrato_phase | v->active.instrument.vibrato_row);
    v->vibrato = (int8_t)fg_u8(&p->work, p->layout.vibrato + index);
    if (v->vibrato_left != 0) {
        v->vibrato_left--;
        v->vibrato = 0;
    }
}

// What every note start does after it has set the pitch
static void note_start(FgPlayer* p, FgVoice* v) {
    v->vibrato_left = v->active.instrument.vibrato_delay;
    v->envelope_tick = 0;
    v->vibrato_phase = 0;
    v->envelope_step = 0xf;
    envelope_advance(p, v);
    if (p->layout.features.has_noise) {
        fg_set16(&p->work, p->layout.noise_phase, 0);
    }
}

// The voice's command: a new one is loaded from the command table and starts a note; without one the duration runs
// down and the next command follows when it has
static void command(FgPlayer* p, uint32_t index) {
    const FgLayout* l = &p->layout;
    FgVoice* v = &p->voices[index];
    if (v->command == 0) {
        v->active.duration = (uint16_t)(v->active.duration - 1);
        if (v->active.duration == 0) {
            v->command = v->active.next_command;
        }
        return;
    }
    v->active = fg_read_command(&p->work, l->commands + (uint32_t)v->command * FgCommandField_Size);
    v->command = 0;
    if (l->features.wave_select) {
        // The flags pick the wave the voice's channel loops: 16 bytes per flag value, 0 and 1 are the silent and the
        // noise wave
        write_lc(p, index, l->wave_base + (uint8_t)(v->active.flags * FG_WAVE_BYTES));
    }
    if (l->features.has_kick) {
        // The voice's kick bits are set (skipped), then every bit of the flags, moved by the channel number, clears one
        // (armed): the flags decide what happens to this channel and, with the higher bits, to the next ones
        for (uint32_t bit = 0; bit < KICK_BITS; bit++) {
            if (v->kick_bits[bit]) {
                p->song.kick_skip[bit] = true;
            }
        }
        for (uint32_t bit = 0; bit < COMMAND_FLAG_BITS; bit++) {
            if (((v->active.flags >> bit) & 1) != 0 && bit + v->channel_shift < KICK_BITS) {
                p->song.kick_skip[bit + v->channel_shift] = false;
            }
        }
    }
    note_start(p, v);
}

// The engine period of a pitch: 48 steps to the octave, the period table holds the lowest octave and every octave
// halves the period
static uint16_t period_of_pitch(const FgPlayer* p, uint16_t pitch_value) {
    const uint16_t period = fg_u16(&p->work, p->layout.periods + 2u * (pitch_value % FG_PERIOD_STEPS));
    const uint32_t shift = (pitch_value / FG_PERIOD_STEPS) & 63;
    return shift >= 16 ? 0 : (uint16_t)(period >> shift);
}

static void pitch(FgPlayer* p, FgVoice* v) {
    v->period = period_of_pitch(p, (uint16_t)(v->active.pitch + (uint16_t)v->vibrato));
}

static void voice_tick(FgPlayer* p, uint32_t index) {
    FgVoice* v = &p->voices[index];
    envelope_tick(p, v);
    slide(v);
    vibrato(p, v);
    command(p, index);
    pitch(p, v);
}

// Sections

static void reset_voices(FgPlayer* p) {
    for (uint32_t i = 0; i < FG_ACTIVE_VOICES; i++) {
        p->voices[i].command = COMMAND_SILENCE;
    }
    if (p->layout.features.has_cap) {
        p->song.cap = 0xf;
    }
}

// The instrument setup: the instruments of the three voices, from the table of 4 byte records. The Star Goose and
// Victory Road setup runs only when the gate is set or at the start of a section.
static void setup_instruments(FgPlayer* p, const FgOp* op) {
    const FgLayout* l = &p->layout;
    if (l->features.gated_setup) {
        if (!p->song.setup_pending && p->song.position != 0) {
            return;
        }
        p->song.setup_pending = false;
    }
    for (uint32_t i = 0; i < FG_ACTIVE_VOICES; i++) {
        p->voices[i].active.instrument = fg_read_instrument(&p->work, l->instruments + op->instrument[i]);
    }
}

// A new note on a voice, from the byte of the note table at the step. `nibble`: the byte carries the vibrato row in its
// high nibble and the note in the low one.
static void track(FgPlayer* p, const FgOp* op, uint16_t step) {
    const FgFeatures* f = &p->layout.features;
    FgVoice* v = &p->voices[op->voice];
    uint16_t note = fg_u8(&p->work, op->table + (uint32_t)(int32_t)(int16_t)step);
    if (op->nibble) {
        const uint8_t row = (uint8_t)(note & 0xf0);
        note &= 0xf;
        if (note == 0) {
            return;
        }
        v->active.instrument.vibrato_row = row;
    } else if (note == 0) {
        return;
    }
    if (f->has_rest && (note & 0x40) != 0) {
        // A rest: the envelope restarts from its first level, and the next instrument setup runs
        v->active.instrument.envelope_offset = 0;
        p->song.setup_pending = true;
        return;
    }
    note = (uint16_t)((note + op->offset) & f->note_mask);
    if (f->has_transpose) {
        note = (uint16_t)(note + p->song.transpose);
    }
    v->active.target = (uint16_t)(note << 2);
    v->active.pitch = v->active.target;
    v->note_period = (uint16_t)(period_of_pitch(p, v->active.pitch) << 1);
    note_start(p, v);
}

// The channel mask: the commands the mask table allows at the current position start
static void channel_mask(FgPlayer* p, const FgOp* op) {
    const FgChannelMask* mask = &p->layout.mask;
    if (mask->writes_len) {
        write_len(p, mask->command_voice, mask->len_off);
    }
    const uint8_t hit = (uint8_t)(op->value & fg_u8(&p->work, mask->table + (p->song.position & mask->index_mask)));
    if (p->layout.features.mask_sets_setup) {
        p->song.setup_pending = hit != 0;
    }
    if (hit != 0) {
        p->voices[mask->command_voice].command = mask->command_value;
        if (mask->writes_len) {
            write_len(p, mask->command_voice, mask->len_on);
        }
    }
}

// Counts a step of the section; when the section's length is reached the next section is requested: the one after
// this for Advance, `to` for AdvanceTo
static void advance(FgPlayer* p, bool to_next, uint8_t to) {
    FgSong* s = &p->song;
    const uint16_t length = fg_u16(&p->work, p->layout.section_lengths + 2u * s->state);
    const uint16_t position = (uint16_t)(s->position + 1);
    if (position == length) {
        s->next_state = to_next ? (uint8_t)(s->state + 1) : to;
    }
    s->position = position;
}

// Does the branch's test hold for the step
static bool test_holds(const FgOp* op, uint16_t step) {
    switch (op->test) {
        case FgTest_Zero:
            return step == 0;
        case FgTest_NonZero:
            return step != 0;
        case FgTest_Equal:
            return step == op->value;
        case FgTest_NotEqual:
            return step != op->value;
        case FgTest_AtLeast:
            return step >= op->value;
        case FgTest_BitSet:
            return ((step >> op->value) & 1) != 0;
        case FgTest_BitClear:
            return ((step >> op->value) & 1) == 0;
        case FgTest_None:
            break;
    }
    return false;
}

// Runs a section routine for one tick. `step` is the routine's copy of the position.
static void run_section(FgPlayer* p, uint32_t section, uint16_t step) {
    const FgLayout* l = &p->layout;
    FgSong* s = &p->song;
    const FgOp* ops = l->op_pool + l->sections[section].first;
    const uint32_t count = l->sections[section].count;
    uint32_t at = 0;
    while (at < count) {
        const FgOp* op = &ops[at++];
        switch (op->kind) {
            case FgOp_Tempo: {
                const uint8_t counter = (uint8_t)(s->tempo_counter + 1);
                if (counter != (uint8_t)op->value) {
                    // Not a step tick: the module drops the section's return address and ends the section here
                    s->tempo_counter = counter;
                    return;
                }
                s->tempo_counter = 0;
                step = s->position;
                break;
            }
            case FgOp_Instruments:
                setup_instruments(p, op);
                break;
            case FgOp_Track:
                track(p, op, step);
                break;
            case FgOp_Command:
                p->voices[op->voice].command = (uint8_t)op->value;
                break;
            case FgOp_CommandFromTable:
                p->voices[op->voice].command = fg_u8(&p->work, op->table + (uint32_t)(int32_t)(int16_t)step);
                break;
            case FgOp_SlideStep:
                p->voices[op->voice].active.step = op->value;
                break;
            case FgOp_SetTranspose:
                s->transpose = op->value;
                break;
            case FgOp_AddTranspose:
                s->transpose = (uint16_t)(s->transpose + op->value);
                break;
            case FgOp_XorTranspose:
                s->transpose ^= op->value;
                break;
            case FgOp_SetNextState:
                s->next_state = (uint8_t)op->value;
                break;
            case FgOp_ClearState:
                s->state = 0;
                break;
            case FgOp_LoadStep:
                step = s->position;
                break;
            case FgOp_StepAnd:
                step &= op->value;
                break;
            case FgOp_StepShift:
                step = (uint16_t)(step >> op->value);
                break;
            case FgOp_Branch: {
                const bool taken = test_holds(op, step);
                step = (uint16_t)(step >> op->then_shift);
                if (taken) {
                    at = op->jump;
                }
                break;
            }
            case FgOp_Jump:
                at = op->jump;
                break;
            case FgOp_ChannelMask:
                channel_mask(p, op);
                break;
            case FgOp_SetLc:
                write_lc(p, op->voice, op->table);
                break;
            case FgOp_SetLen:
                write_len(p, op->voice, op->value);
                break;
            case FgOp_Reset:
                reset_voices(p);
                break;
            case FgOp_CapFromStep:
                s->cap = (uint8_t)((((step & 0xff) >> 4) + op->value) & 0xf);
                break;
            case FgOp_CapFromPosition:
                s->cap = (uint8_t)((((s->position & 0xff) >> 4) & 0xf) ^ 0xf);
                break;
            case FgOp_End:
                switch (op->end) {
                    case FgEnd_Return:
                        break;
                    case FgEnd_Advance:
                        advance(p, true, 0);
                        break;
                    case FgEnd_AdvanceTo:
                        advance(p, false, (uint8_t)op->value);
                        break;
                    case FgEnd_Reset:
                        reset_voices(p);
                        break;
                }
                return;
        }
    }
}

// The request stage: a pending request resets the song state and the voices; a pending section starts; the current
// section's routine runs. Section 0 is the song end.
static bool request_stage(FgPlayer* p, uint16_t step) {
    const FgLayout* l = &p->layout;
    FgSong* s = &p->song;
    if (s->request != 0) {
        s->next_state = s->request;
        s->request = 0;
        if (l->features.request_len) {
            write_len(p, l->request_len_voice, l->request_len_words);
        }
        if (l->features.has_transpose) {
            s->transpose = 0;
        }
        if (l->features.gated_setup) {
            s->setup_pending = false;
        }
        reset_voices(p);
    }
    if (s->next_state != 0) {
        s->state = s->next_state;
        s->next_state = 0;
        s->tempo_counter = 0;
        s->position = 0;
    }
    if (s->state == 0 || s->state >= l->section_count) {
        return true;
    }
    run_section(p, s->state, step);
    return false;
}

FgTickResult fg_tick(FgPlayer* p, NuadeVoices* voices) {
    const FgLayout* l = &p->layout;
    if (l->features.has_noise) {
        step_noise(p);
    }
    const bool song_end = request_stage(p, l->features.step_preloaded ? p->song.position : 0);
    for (uint32_t i = 0; i < FG_ACTIVE_VOICES; i++) {
        voice_tick(p, i);
    }
    write_registers(p);
    for (uint32_t i = 0; i < FG_VOICES; i++) {
        nuade_paula_channel_sync(&p->voices[i].paula, i, voices);
    }
    FgTickResult result = { .looped = song_end && !p->song_end_seen };
    p->song_end_seen = p->song_end_seen || song_end;
    return result;
}

// Player

FgPlayer* fg_create(const uint8_t* data, size_t size) {
    FgPlayer* p = (FgPlayer*)calloc(1, sizeof(FgPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    if (!fg_module_load(&p->original, data, size) || !fg_module_load(&p->work, data, size)
        || !fg_locate(&p->original, &p->layout)) {
        fg_destroy(p);
        return nullptr;
    }
    return p;
}

void fg_destroy(FgPlayer* p) {
    if (p == nullptr) {
        return;
    }
    fg_layout_free(&p->layout);
    fg_module_free(&p->original);
    fg_module_free(&p->work);
    free(p);
}

bool fg_probe(const uint8_t* data, size_t size) {
    FgPlayer* p = fg_create(data, size);
    fg_destroy(p);
    return p != nullptr;
}

int fg_subsong_count(const FgPlayer* p) {
    return (int)fg_header_subsongs(&p->original);
}

const char* fg_version_name(const FgPlayer* p) {
    return fg_traits(p->layout.version)->name;
}

const char* fg_title(const FgPlayer* p) {
    return fg_header_title(&p->original);
}

const char* fg_author(const FgPlayer* p) {
    return fg_header_author(&p->original);
}

// The module's init routine for a subsong: the first request from the subsong table, the audio registers, the DMA on
bool fg_start(FgPlayer* p, int subsong, NuadeVoices* voices) {
    const FgLayout* l = &p->layout;
    if (subsong < 0 || subsong >= fg_subsong_count(p)) {
        return false;
    }
    memcpy(p->work.image, p->original.image, p->work.size);
    memset(p->voices, 0, sizeof(p->voices));
    memset(&p->song, 0, sizeof(p->song));
    p->song_end_seen = false;
    for (uint32_t i = 0; i < FG_ACTIVE_VOICES; i++) {
        p->voices[i] = read_voice(&p->work, l->voice_base + i * l->features.voice_stride);
    }
    p->song.request = fg_u8(&p->work, l->init.subsong_map + (uint32_t)subsong);
    for (uint32_t i = 0; i < FG_ACTIVE_VOICES; i++) {
        write_lc(p, i, l->init.lc[i]);
        write_len(p, i, l->init.len_words);
        p->voices[i].paula.volume = 0;
        if (l->init.writes_period) {
            p->voices[i].paula.period = l->init.period;
        }
        nuade_paula_channel_dma_on(&p->voices[i].paula);
    }
    for (uint32_t i = 0; i < FG_VOICES; i++) {
        nuade_paula_channel_sync(&p->voices[i].paula, i, voices);
    }
    return true;
}

FgVoiceState fg_voice_state(const FgPlayer* p, int voice) {
    const uint16_t period = voice >= 0 && voice < FG_ACTIVE_VOICES ? p->voices[voice].note_period : 0;
    return (FgVoiceState) { .note = period != 0 ? nuade_period_to_note(period) : -1 };
}
