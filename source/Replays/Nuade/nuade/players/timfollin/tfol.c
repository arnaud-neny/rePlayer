// SPDX-License-Identifier: MIT
// Tim Follin player (Follin Player II): a four voice sample player. Every voice runs a byte stream of notes and
// commands; a note starts the instrument's sample, an amplitude envelope, vibrato, portamento and a pitch modulation
// run every tick, and the first instruments are swept: a run of bytes in the sample grows and shrinks.
//
// One tick: the voices that asked for a second sample (wave_flags bit 6) switch to it, then every voice runs its
// sweep, envelope, pitch effects and note counter, and fetches the next events when the note counter runs out.

#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include "tfol_internal.h"
#include "tfol_layout.h"
#include <stdlib.h>
#include <string.h>

#define TFOL_STACK_EMPTY 0xffffffffu

// Notes index a table of 128 words; the bytes after the table in the eagleplayer are part of it.
#define TFOL_NOTE_MASK 0x7f

// The sweep writes these values.
#define TFOL_SWEEP_UP_VALUE 0x80
#define TFOL_SWEEP_DOWN_VALUE 0
#define TFOL_ENVELOPE_PEAK 0x3f

typedef enum TfolEnvelopeState {
    TfolEnvelope_Attack = 0,
    TfolEnvelope_Release = 1,
    TfolEnvelope_Hold = 2,
} TfolEnvelopeState;

// The note period table of the eagleplayer: 95 periods, then bytes of the eagleplayer's own data. The word after the
// last entry lies behind the eagleplayer's code; a song patched to play note 127 in UADE writes AUDxPER 0 (measured).
static const uint16_t tfol_periods[128] = {
    0x0eee, 0x0e18, 0x0d4d, 0x0c8e, 0x0bda, 0x0b2f, 0x0a8f, 0x09f7, 0x0968, 0x08e1, 0x0861, 0x07e9, 0x0777,
    0x070c, 0x06a7, 0x0647, 0x05ed, 0x0598, 0x0547, 0x04fc, 0x04b4, 0x0470, 0x0431, 0x03f4, 0x03bc, 0x0386,
    0x0353, 0x0324, 0x02f6, 0x02cc, 0x02a4, 0x027e, 0x025a, 0x0238, 0x0218, 0x01fa, 0x01de, 0x01c3, 0x01aa,
    0x0192, 0x017b, 0x0166, 0x0152, 0x013f, 0x012d, 0x011c, 0x010c, 0x00fd, 0x00ef, 0x00e1, 0x00d5, 0x00c9,
    0x00be, 0x00b3, 0x00a9, 0x009f, 0x0096, 0x008e, 0x0086, 0x007f, 0x0077, 0x0071, 0x006a, 0x0064, 0x005f,
    0x0059, 0x0054, 0x0050, 0x004b, 0x0047, 0x0043, 0x003f, 0x003c, 0x0038, 0x0035, 0x0032, 0x002f, 0x002d,
    0x002a, 0x0028, 0x0026, 0x0024, 0x0022, 0x0020, 0x001e, 0x001c, 0x001b, 0x0019, 0x0018, 0x0016, 0x0015,
    0x0014, 0x0013, 0x0012, 0x0011, 0x0010, 0x0008, 0x0a14, 0x1e28, 0x323c, 0x4650, 0x0a14, 0x1e28, 0x323c,
    0x4650, 0x204d, 0x494b, 0x452f, 0x5469, 0x6d20, 0x466f, 0x6c6c, 0x696e, 0x2c20, 0x696d, 0x7072, 0x6f76,
    0x6564, 0x2062, 0x7920, 0x414e, 0x4459, 0x2053, 0x494c, 0x5641, 0x2000, 0x0000, 0x0000,
};

bool tfol_probe(const uint8_t* data, size_t size) {
    TfolModule module;
    return tfol_module_locate(data, size, &module);
}

TfolCreateResult tfol_create(const uint8_t* data, size_t size) {
    TfolCreateResult result = { .status = TfolStatus_InvalidData };
    TfolModule module;
    if (!tfol_module_locate(data, size, &module)) {
        return result;
    }
    TfolPlayer* player = (TfolPlayer*)calloc(1, sizeof(*player));
    uint8_t* original = (uint8_t*)malloc(size);
    uint8_t* mem = nuade_paula_padded_copy(data, size);
    if (player == nullptr || original == nullptr || mem == nullptr) {
        free(player);
        free(original);
        free(mem);
        result.status = TfolStatus_OutOfMemory;
        return result;
    }
    memcpy(original, data, size);
    module.mem = mem;
    module.original = original;
    player->module = module;
    result.player = player;
    result.status = TfolStatus_Ok;
    return result;
}

void tfol_destroy(TfolPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module.mem);
    free(player->module.original);
    free(player);
}

uint32_t tfol_subsong_count(const TfolPlayer* player) {
    return player->module.subsong_count;
}

// Sets the sample registers of a voice from an instrument; the sample address clears bit 0 like Paula does.
static void tfol_load_sample(TfolVoice* v, const TfolModule* m, const TfolInstrument* inst) {
    const uint32_t sample = inst->sample & ~1u;
    v->paula.lc = sample < m->size ? (const int8_t*)(m->mem + sample) : nullptr;
    v->paula.len_words = inst->length_words;
}

static void tfol_stop_voice(TfolVoice* v) {
    v->active = false;
    v->retrigger = false;
    nuade_paula_channel_dma_off(&v->paula);
    v->paula.volume = 0;
}

// InitSound: the song's streams, and the state of every voice.
bool tfol_start(TfolPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong >= player->module.subsong_count) {
        return false;
    }
    TfolModule* m = &player->module;
    memcpy(m->mem, m->original, m->size);
    player->song = (subsong + 1) & (TFOL_SONGS - 1);
    player->song_record = tfol_module_song(m, player->song);
    const TfolSong* song = &player->song_record;
    const bool song_silent = player->song == TFOL_SILENT_SONG;
    for (uint32_t i = 0; i < TFOL_STACK_ENTRIES; i++) {
        player->stack[i] = TFOL_STACK_EMPTY;
    }
    for (uint32_t i = 0; i < TFOL_VOICES; i++) {
        TfolVoice* v = &player->voices[i];
        *v = (TfolVoice) { 0 };
        const bool present = i < song->voices;
        v->active = present && !song_silent;
        v->stream = song->stream[i];
        v->stack_top = i * TFOL_STACK_PER_VOICE;
        v->note_counter = present ? 1 : 0;
        nuade_voice_stop(voices, i);
    }
    return true;
}

// The second sample of a voice: entry wave_flags & 0x3f of the instrument table, started with DMA already running so
// that Paula takes it as the repeat.
static void tfol_switch_to_second_sample(TfolPlayer* player, TfolVoice* v) {
    v->retrigger = false;
    const TfolInstrument inst
        = tfol_module_instrument(&player->module, player->song_record.instrument_table, v->second_sample_entry);
    if (!inst.valid) {
        return;
    }
    tfol_load_sample(v, &player->module, &inst);
    nuade_paula_channel_dma_on(&v->paula);
}

// The sweep: a run of sample bytes of the instrument fills with 0x80 position by position, then empties again.
static void tfol_sweep(TfolPlayer* player, TfolVoice* v) {
    v->sweep_counter--;
    if (v->sweep_counter != 0) {
        return;
    }
    v->sweep_counter = v->sweep_rate;
    uint8_t position = v->sweep_position;
    uint8_t value = 0;
    if (!v->sweep_down) {
        position++;
        if (position == TFOL_SWEEP_LAST) {
            v->sweep_down = true;
        }
        value = TFOL_SWEEP_UP_VALUE;
    } else {
        position--;
        if (position == TFOL_SWEEP_FIRST) {
            v->sweep_down = false;
        }
        value = TFOL_SWEEP_DOWN_VALUE;
    }
    if (v->has_instrument) {
        tfol_module_poke(&player->module, v->instrument.sample + position, value);
    }
    v->sweep_position = position;
}

// Restarts the sweep of the voice's instrument.
static void tfol_reset_sweep(TfolPlayer* player, TfolVoice* v) {
    v->sweep_counter = v->sweep_rate;
    v->sweep_down = false;
    v->sweep_position = TFOL_SWEEP_START;
    if (!v->has_instrument) {
        return;
    }
    for (uint32_t i = 0; i < TFOL_RESET_FILLED + TFOL_RESET_ZEROED; i++) {
        const uint8_t value = i < TFOL_RESET_FILLED ? TFOL_SWEEP_UP_VALUE : 0;
        tfol_module_poke(&player->module, v->instrument.offset + TFOL_RESET_FIRST + i, value);
    }
}

// The amplitude envelope: attack up to 0x3f, then release down to the sustain level.
static void tfol_envelope(TfolVoice* v) {
    if (v->envelope_state == TfolEnvelope_Attack) {
        v->envelope_counter--;
        if (v->envelope_counter >= 0) {
            return;
        }
        v->envelope_counter = (int8_t)v->envelope_attack_rate;
        v->level = (uint8_t)(v->level + v->envelope_step);
        if (v->level == TFOL_ENVELOPE_PEAK) {
            v->envelope_state = TfolEnvelope_Release;
            v->envelope_counter = (int8_t)v->envelope_release_rate;
        }
    } else if (v->envelope_state == TfolEnvelope_Release) {
        v->envelope_counter--;
        if (v->envelope_counter >= 0) {
            return;
        }
        v->envelope_counter = (int8_t)v->envelope_release_rate;
        if (v->level == v->envelope_sustain) {
            v->envelope_state = TfolEnvelope_Hold;
            return;
        }
        v->level = (uint8_t)(v->level - v->envelope_step);
        if ((int8_t)v->level < 0) {
            v->envelope_state = TfolEnvelope_Hold;
        }
    }
}

static void tfol_set_note_period(TfolVoice* v, uint8_t note) {
    v->note = note;
    v->period = tfol_periods[note & TFOL_NOTE_MASK];
    v->paula.period = v->period;
}

// Moves the period by the pitch modulation amount, up or down; the direction flips every `ticks` ticks.
static void tfol_pitch_modulation(TfolVoice* v) {
    if (v->vibrato_direction != 0) {
        v->period = (uint16_t)(v->period + v->pitch_modulation_amount);
    } else {
        v->period = (uint16_t)(v->period - v->pitch_modulation_amount);
    }
    v->paula.period = v->period;
    v->pitch_modulation_counter--;
    if (v->pitch_modulation_counter != 0) {
        return;
    }
    if (v->pitch_modulation_ticks != 0) {
        v->pitch_modulation_counter = (uint8_t)(v->pitch_modulation_ticks << 1);
        v->vibrato_direction ^= 1;
    }
}

// The vibrato: alternates the note by +-depth. Returns true when it set the period (the portamento is then skipped).
static bool tfol_vibrato(TfolVoice* v) {
    if (v->vibrato_counter == 0) {
        return false;
    }
    v->vibrato_counter--;
    if (v->vibrato_counter != 0) {
        return false;
    }
    v->vibrato_direction ^= 1;
    uint8_t note = v->note;
    if (v->vibrato_direction != 0) {
        note = (uint8_t)(note + v->vibrato_depth);
        v->vibrato_counter = v->vibrato_up_ticks;
    } else {
        note = (uint8_t)(note - v->vibrato_depth);
        v->vibrato_counter = v->vibrato_down_ticks;
    }
    tfol_set_note_period(v, note);
    return true;
}

// The portamento: the note moves by `rate` per tick towards the target and stops on it.
static void tfol_portamento(TfolVoice* v) {
    if (v->portamento_rate == 0 || v->note == v->portamento_target) {
        return;
    }
    uint8_t note = v->note;
    if (note < v->portamento_target) {
        note = (uint8_t)(note + v->portamento_rate);
        if (note >= v->portamento_target) {
            note = v->portamento_target;
        }
    } else {
        note = (uint8_t)(note - v->portamento_rate);
        if (note < v->portamento_target) {
            note = v->portamento_target;
        }
    }
    tfol_set_note_period(v, note);
}

// A note: the instrument's sample starts with DMA on, and the period follows.
static void tfol_start_note(TfolPlayer* player, TfolVoice* v) {
    if (v->has_instrument) {
        tfol_load_sample(v, &player->module, &v->instrument);
        nuade_paula_channel_dma_on(&v->paula);
    }
    if (v->second_sample) {
        v->retrigger = true;
    }
    v->paula.period = v->period;
}

// A note byte of the stream: sets the period, starts the sample, reads the length and restarts the effects.
static void tfol_play_note(TfolPlayer* player, TfolVoice* v, const TfolEvent* ev) {
    uint8_t note = 0;
    if (v->skip_transpose) {
        v->skip_transpose = false;
    } else {
        note = v->transpose;
    }
    note = (uint8_t)(note + ev->note);
    if (v->portamento_rate != 0) {
        v->portamento_target = note;
        note = v->note;
    }
    v->note = note;
    v->period = tfol_periods[note & TFOL_NOTE_MASK];
    tfol_start_note(player, v);

    v->note_counter = v->note_length != 0 ? v->note_length : ev->args[0];
    v->stream = ev->next;

    v->vibrato_counter = v->vibrato_up_ticks;
    uint8_t direction = v->pitch_modulation_delay;
    if (direction != 0) {
        v->pitch_modulation_delay_counter = direction;
        v->pitch_modulation_counter = v->pitch_modulation_ticks;
        direction = v->pitch_modulation_direction_start;
    }
    v->vibrato_direction = direction;

    if (v->envelope_restart) {
        v->envelope_state = v->envelope_state_start;
        v->envelope_counter
            = (int8_t)(v->envelope_state != TfolEnvelope_Attack ? v->envelope_release_rate : v->envelope_attack_rate);
        v->level = v->envelope_level_start;
    }
}

static void tfol_stack_push(TfolPlayer* player, TfolVoice* v, uint32_t value) {
    if (v->stack_top >= TFOL_STACK_ENTRIES) {
        tfol_stop_voice(v);
        return;
    }
    player->stack[v->stack_top++] = value;
}

static uint32_t tfol_stack_pop(TfolPlayer* player, TfolVoice* v) {
    if (v->stack_top == 0 || player->stack[v->stack_top - 1] == TFOL_STACK_EMPTY) {
        tfol_stop_voice(v);
        return 0;
    }
    return player->stack[--v->stack_top];
}

// The instrument command: the instrument number selects an entry of the song's instrument table.
static void tfol_select_instrument(TfolPlayer* player, TfolVoice* v, uint8_t number) {
    v->instrument_number = number;
    v->instrument = tfol_module_instrument(&player->module, player->song_record.instrument_table, number);
    v->has_instrument = v->instrument.valid;
}

static void tfol_set_envelope(TfolVoice* v, const uint8_t* args) {
    v->envelope_level_start = (uint8_t)((args[0] >> 4) * 4);
    v->envelope_sustain = (uint8_t)((args[0] & 0xf) * 4);
    v->envelope_attack_rate = args[1] >> 4;
    v->envelope_release_rate = args[1] & 0xf;
    v->envelope_state_start = args[2];
}

// Runs one command of the stream. Returns false when the fetch ends.
static bool tfol_run_command(TfolPlayer* player, TfolVoice* v, const TfolEvent* ev, uint32_t* at) {
    const uint8_t* args = ev->args;
    switch (ev->command) {
        case TfolCommand_Nop:
        case TfolCommand_Count:
            break;
        case TfolCommand_Instrument:
            tfol_select_instrument(player, v, args[0]);
            break;
        case TfolCommand_Call:
            tfol_stack_push(player, v, *at);
            *at = ev->target;
            break;
        case TfolCommand_Return:
            *at = tfol_stack_pop(player, v);
            break;
        case TfolCommand_LoopStart:
            v->loop_count = args[0];
            v->loop_start = *at;
            break;
        case TfolCommand_LoopEnd:
            v->loop_count--;
            if (v->loop_count != 0) {
                *at = v->loop_start;
            }
            break;
        case TfolCommand_Envelope:
            tfol_set_envelope(v, args);
            break;
        case TfolCommand_Portamento:
            v->portamento_rate = args[0];
            break;
        case TfolCommand_Vibrato:
            v->vibrato_depth = args[0];
            v->vibrato_up_ticks = args[1];
            v->vibrato_down_ticks = args[2];
            break;
        case TfolCommand_PitchModulation:
            v->pitch_modulation_delay = args[0];
            v->pitch_modulation_amount = args[1];
            v->pitch_modulation_ticks = args[2];
            v->pitch_modulation_direction_start = args[3];
            break;
        case TfolCommand_Transpose:
            v->transpose = args[0];
            break;
        case TfolCommand_NoteLength:
            v->note_length = args[0];
            break;
        case TfolCommand_EnvelopeRestart:
            v->envelope_restart = args[0] != 0;
            break;
        case TfolCommand_WaveFlags:
            v->note_off = (args[0] & TFOL_WAVE_NOTE_OFF) != 0;
            v->second_sample = (args[0] & TFOL_WAVE_SECOND_SAMPLE) != 0;
            v->second_sample_entry = args[0] & TFOL_WAVE_SAMPLE_INDEX;
            break;
        case TfolCommand_SweepRate:
            v->sweep_rate = args[0];
            tfol_reset_sweep(player, v);
            break;
        case TfolCommand_Stop:
            tfol_stop_voice(v);
            return false;
        case TfolCommand_EnvelopeStep:
            v->envelope_step = args[0];
            break;
        case TfolCommand_SkipTranspose:
            v->skip_transpose = true;
            break;
        case TfolCommand_Jump:
            *at = ev->target;
            break;
    }
    return v->active;
}

// Reads the stream until a note starts or the voice stops.
static void tfol_fetch(TfolPlayer* player, TfolVoice* v) {
    uint32_t at = v->stream;
    for (uint32_t steps = 0; steps < TFOL_FETCH_LIMIT; steps++) {
        const TfolEvent ev = tfol_module_read_event(&player->module, at, v->note_length != 0);
        if (!ev.valid) {
            break;
        }
        at = ev.next;
        if (ev.is_note) {
            tfol_play_note(player, v, &ev);
            return;
        }
        if (!tfol_run_command(player, v, &ev, &at)) {
            return;
        }
    }
    tfol_stop_voice(v);
}

// The note counter: one tick before the end a note-off voice cuts its DMA, at the end the next events are read.
static void tfol_note_counter(TfolPlayer* player, TfolVoice* v) {
    v->note_counter--;
    if (v->note_counter == 1) {
        if (v->note_off) {
            nuade_paula_channel_dma_off(&v->paula);
        }
    } else if (v->note_counter == 0) {
        tfol_fetch(player, v);
    }
}

static void tfol_update_voice(TfolPlayer* player, TfolVoice* v) {
    if (!v->active) {
        return;
    }
    if ((int8_t)v->instrument_number < TFOL_SWEEP_INSTRUMENTS) {
        tfol_sweep(player, v);
    }
    tfol_envelope(v);
    v->paula.volume = v->level;

    bool period_set = false;
    bool run_vibrato = true;
    if (v->pitch_modulation_delay != 0) {
        if (v->pitch_modulation_delay_counter != 0) {
            v->pitch_modulation_delay_counter--;
        }
        if (v->pitch_modulation_delay_counter == 0) {
            tfol_pitch_modulation(v);
            run_vibrato = false;
        }
    }
    if (run_vibrato) {
        period_set = tfol_vibrato(v);
    }
    if (!period_set) {
        tfol_portamento(v);
    }
    tfol_note_counter(player, v);
}

TfolTickResult tfol_tick(TfolPlayer* player, NuadeVoices* voices) {
    for (uint32_t i = 0; i < TFOL_VOICES; i++) {
        if (player->voices[i].retrigger) {
            tfol_switch_to_second_sample(player, &player->voices[i]);
        }
    }
    bool any_active = false;
    for (uint32_t i = 0; i < TFOL_VOICES; i++) {
        tfol_update_voice(player, &player->voices[i]);
        any_active |= player->voices[i].active;
    }
    for (uint32_t i = 0; i < TFOL_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return (TfolTickResult) { .ended = !any_active };
}

TfolVoiceState tfol_voice_state(const TfolPlayer* player, uint32_t voice) {
    const TfolVoice* v = &player->voices[voice];
    return (TfolVoiceState) { .period = v->period, .instrument = v->instrument_number };
}
