// SPDX-License-Identifier: MIT
// Rob Hubbard ST player, translated from the replayers inside the "*.rho" modules and the UADE eagleplayer
// "RobHubbard_ST", which checks the module, patches it and calls the replayer's init, tick and stop.
//
// The replayer is an Atari ST driver for the YM2149 sound chip, made to drive Paula: three voices, each reading a list
// of streams (notes and commands) and keeping an envelope (a volume that rises and falls, a list of semitone offsets
// that repeats, a vibrato, a slide). Each tick it computes what the chip's registers would be (a period and a volume
// per voice, a noise period, the mixer register) and writes them to Paula: voice n plays on Paula channel n with a
// short square wave; the second channel plays noise (code bytes of the module) instead when the mixer register enables
// the noise for the second voice. The fourth channel is silent.
//
// Paula channels run for ever with their DMA on; the replayer only writes AUDxLC, AUDxLEN, AUDxPER and AUDxVOL.
// The module builds differ in the output stage (when it writes, how the noise is selected) and in the init; the
// sequencer and the envelopes are the same code. rhst_module.c describes each build as a RhstBuild.
//
// What the eagleplayer does that changes the replayer's output:
//   - the stores to the audio registers become calls that also record the value (module bytes change, which the noise
//     sample reads);
//   - the next-track command is wrapped: when the longest track list of the subsong wraps, SongEnd is called;
//   - the stop command (0x8E) calls SongEnd before it runs;
//   - the eagleplayer sets the replayer's "song runs" flag after the init.
//
// Order of work in a tick: the fourth channel's audio interrupt restarts its short waveform; the output stage runs the
// play routine (when the song runs), which steps the sequencer every `tempo` ticks and computes the registers of the
// three voices, and then writes them to Paula.

#include <nuade/players/robhubbardst.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "rhst_module.h"
#include <stdlib.h>

#define TONE_FIRST 0x00u // waveforms in the module header, four words each
#define TONE_SECOND 0x08u
#define TONE_SQUARE 0x10u // the second channel's tone
#define TONE_FOURTH 0x18u
#define TONE_THIRD 0x28u
#define TONE_WORDS 4
#define INIT_PERIOD 0x1BF
#define NOISE_FOURTH_PERIOD 0x9F6
#define NOISE_FOURTH_WORDS 0x64
#define MIXER_INIT 0xF8    // the mixer shadow the init stores
#define MIXER_OFF 0xFF     // the mixer register after a stop
#define MIXER_NOISE_ON 7   // tone off, noise on
#define MIXER_TONE_B 0x10  // second voice's noise bit; set = noise off (first and second builds)
#define MIXER_TONE_B_OFF 2 // second voice's tone bit; set = tone off (third build)
#define PERIOD_FLOOR_SHIFT 0x60
#define PERIOD_OCTAVE 0x18

bool rhst_probe(const uint8_t* data, size_t size) {
    RhstModule* module = rhst_module_open(data, size);
    rhst_module_close(module);
    return module != nullptr;
}

RhstPlayer* rhst_create(const uint8_t* data, size_t size) {
    RhstPlayer* player = (RhstPlayer*)calloc(1, sizeof(RhstPlayer));
    if (player == nullptr) {
        return nullptr;
    }
    player->module = rhst_module_open(data, size);
    if (player->module == nullptr) {
        free(player);
        return nullptr;
    }
    return player;
}

void rhst_destroy(RhstPlayer* player) {
    if (player != nullptr) {
        rhst_module_close(player->module);
    }
    free(player);
}

int rhst_subsong_count(const RhstPlayer* player) {
    return (int)player->module->song_count;
}

// Paula

static const int8_t* sample_at(const RhstModule* m, uint32_t offset) {
    return (const int8_t*)(m->image + offset);
}

static void set_sample(const RhstModule* m, NuadePaulaChannel* paula, uint32_t offset, uint16_t words) {
    paula->lc = sample_at(m, offset);
    paula->len_words = words;
}

// The hardware init: the four short waveforms, volume 0, DMA on.
static void hardware_init(RhstPlayer* p) {
    static const uint32_t tones[RHST_CHANNELS] = { TONE_SECOND, TONE_FIRST, TONE_THIRD, TONE_FOURTH };
    for (uint32_t i = 0; i < RHST_CHANNELS; i++) {
        NuadePaulaChannel* paula = &p->paula[i];
        set_sample(p->module, paula, tones[i], TONE_WORDS);
        paula->volume = 0;
        paula->period = INIT_PERIOD;
        nuade_paula_channel_dma_on(paula);
    }
}

// AUDxVOL from a chip volume byte: four Paula steps per chip step
static uint8_t paula_volume(uint8_t chip_volume) {
    return (uint8_t)((chip_volume << 2) & 0x3F);
}

static void store_voices(RhstPlayer* p) {
    for (uint32_t i = 0; i < RHST_RAW_VOICES; i++) {
        rhst_module_store_voice(p->module, i, &p->voices[i]);
    }
}

// Starting a subsong

// The init of a voice: its track list, its first stream, the first instrument.
static void init_voice(const RhstPlayer* p, RhstVoice* v, uint32_t voice) {
    const RhstModule* m = p->module;
    const uint32_t list = rhst_module_track_list(m, p->song, voice);
    const uint32_t first = rhst_module_instrument(m, 0);
    *v = (RhstVoice) {
        .track_list = list,
        .track_pos = 4,
        .stream = m->data + rhst_module_u32(m, list),
        .env_start = first,
        .env_at = first,
        .step_count = 1,
        .mixer_mask = m->mixer_mask[voice],
    };
}

bool rhst_start(RhstPlayer* player, int subsong, NuadeVoices* voices) {
    const RhstModule* m = player->module;
    if (subsong < 0 || (uint32_t)subsong >= m->song_count) {
        return false;
    }
    const RhstOutput output = m->output;
    *player = (RhstPlayer) { .module = player->module, .song = rhst_module_played_song(m, (uint32_t)subsong) };
    player->longest_list = rhst_module_longest_list(m, (uint32_t)subsong);
    player->mixer_shadow = MIXER_INIT;
    player->mixer_out = output == RhstOutput_Always ? MIXER_OFF : 0;
    player->tempo_count = 1;
    for (uint32_t i = 0; i < RHST_RAW_VOICES; i++) {
        init_voice(player, &player->voices[i], i);
    }
    hardware_init(player);
    store_voices(player);
    // The eagleplayer sets the flag after the init
    player->active = true;
    for (uint32_t i = 0; i < RHST_CHANNELS; i++) {
        nuade_voice_stop(voices, i);
    }
    return true;
}

// The sequencer

// The eagleplayer's wrapper of the stop command and of the next-track command calls SongEnd; the replayer goes on.
static void song_end(RhstPlayer* p) {
    p->song_ended = true;
}

// Stop: the song stops, the chip registers are cleared and the hardware init runs again.
static void stop_song(RhstPlayer* p) {
    p->active = false;
    for (uint32_t i = 0; i < RHST_RAW_VOICES; i++) {
        p->out_period[i] = 0;
        p->out_volume[i] = 0;
    }
    p->noise_out = 0;
    p->mixer_out = MIXER_OFF;
    hardware_init(p);
    p->ended = true;
}

// The next-track command: the next entry of the track list; its end starts the list over, and when that is the
// longest list of the subsong it is the song's end.
static void next_track(RhstPlayer* p, RhstVoice* v) {
    const RhstModule* m = p->module;
    uint32_t entry = rhst_module_u32(m, v->track_list + v->track_pos);
    v->track_pos += 4;
    if (entry == 0) {
        if (v->track_list == p->longest_list) {
            song_end(p);
        }
        v->track_pos = 4;
        entry = rhst_module_u32(m, v->track_list);
    }
    v->stream = m->data + entry;
}

typedef enum CommandResult {
    CommandResult_Next,    // the step goes on with the next byte
    CommandResult_EndStep, // the command ends the step (a rest)
    CommandResult_Stop,    // the song stopped
} CommandResult;

typedef enum StepResult {
    StepResult_Continue,
    StepResult_Stop,
} StepResult;

// The start of a note: the envelope starts unless the note is tied to the one before.
static void start_note(RhstVoice* v, uint8_t note) {
    v->note = note;
    if (!v->tie) {
        v->volume = v->start_volume;
        v->envelope = true;
        v->attack = true;
        v->rate = v->attack_init;
        v->phase_left = v->phase_init;
    }
}

static void command_mixer(RhstPlayer* p, const RhstVoice* v, RhstCommand command) {
    switch (command) {
        case RhstCommand_NoiseOnly:
            // tone off, noise on for this voice
            p->mixer_shadow |= v->mixer_mask & 0x07;
            p->mixer_shadow &= (uint8_t)~((v->mixer_mask & 0x07) << 3);
            break;
        case RhstCommand_ToneOnly:
            p->mixer_shadow |= v->mixer_mask & 0x38;
            p->mixer_shadow &= (uint8_t)~((v->mixer_mask & 0x38) >> 3);
            break;
        default:
            p->mixer_shadow &= (uint8_t)~v->mixer_mask;
            break;
    }
}

// A command from the jump table.
static CommandResult run_command(RhstPlayer* p, RhstVoice* v, uint32_t* at, RhstCommand command) {
    const RhstModule* m = p->module;
    switch (command) {
        case RhstCommand_Rest:
            v->volume = 0;
            v->envelope = false;
            return CommandResult_EndStep;
        case RhstCommand_RestKeep:
            v->envelope = false;
            return CommandResult_EndStep;
        case RhstCommand_VibratoOff:
        case RhstCommand_VibratoFull:
        case RhstCommand_VibratoHalf:
            v->vib_on = command != RhstCommand_VibratoOff;
            v->vib_half = command == RhstCommand_VibratoHalf;
            v->vib_rising = false;
            break;
        case RhstCommand_Slide:
            v->slide_total = 0;
            v->slide_step = (int8_t)rhst_module_u8(m, (*at)++);
            v->slide_delay = rhst_module_u8(m, (*at)++);
            v->slide = true;
            break;
        case RhstCommand_PortamentoUp:
            v->porta_up = true;
            v->portamento = true;
            break;
        case RhstCommand_PortamentoDown:
            v->portamento = true;
            break;
        case RhstCommand_NextTrack:
            next_track(p, v);
            *at = v->stream;
            break;
        case RhstCommand_Vibrato:
            v->vib_step = rhst_module_u8(m, (*at)++);
            v->vib_depth = rhst_module_u8(m, (*at)++);
            v->vib_pos = v->vib_depth;
            break;
        case RhstCommand_NoiseFreq:
            p->noise_freq = rhst_module_u8(m, (*at)++);
            break;
        case RhstCommand_ToneOnly:
        case RhstCommand_NoiseOnly:
        case RhstCommand_ToneAndNoise:
            command_mixer(p, v, command);
            break;
        case RhstCommand_Percussion:
            v->percussion = true;
            break;
        case RhstCommand_Tie:
            v->tie = true;
            break;
        case RhstCommand_NoiseTrigger:
            v->noise_trigger = true;
            break;
        case RhstCommand_Count:
            break; // past the jump table: refused when the module is opened
        case RhstCommand_Stop:
            song_end(p);
            stop_song(p);
            return CommandResult_Stop;
    }
    return CommandResult_Next;
}

// A voice's turn in the sequencer: hold the note, or read commands up to the next note or rest.
static StepResult step_voice(RhstPlayer* p, RhstVoice* v) {
    const RhstModule* m = p->module;
    if (--v->step_count != 0) {
        if (v->portamento) {
            v->note = (uint16_t)(v->note + (v->porta_up ? 1 : -1));
        }
        return StepResult_Continue;
    }
    v->toggle = v->percussion = v->slide = v->portamento = v->tie = v->envelope = v->attack = v->porta_up = false;
    v->noise_trigger = false;
    uint32_t at = v->stream;
    for (uint32_t reads = 0; reads < RHST_STEP_LIMIT; reads++) {
        const uint8_t byte = rhst_module_u8(m, at++);
        if (byte < RHST_NOTE_END) {
            start_note(v, byte);
            break;
        }
        if (byte <= RHST_COMMAND_LAST) {
            const CommandResult result = run_command(p, v, &at, (RhstCommand)(byte & RHST_COMMAND_MASK));
            if (result == CommandResult_Stop) {
                return StepResult_Stop;
            }
            if (result == CommandResult_EndStep) {
                break;
            }
        } else if (byte < RHST_INSTRUMENT_FIRST) {
            p->tempo = (uint16_t)(byte - RHST_TEMPO_FIRST + 1);
        } else if (byte < RHST_ENVELOPE_FIRST) {
            v->env_start = rhst_module_instrument(m, (uint32_t)(byte - RHST_INSTRUMENT_FIRST));
            v->env_at = v->env_start;
        } else if (byte < RHST_LENGTH_FIRST) {
            v->start_volume = (uint8_t)(byte - RHST_ENVELOPE_FIRST);
            v->attack_init = rhst_module_u8(m, at++);
            v->decay_init = rhst_module_u8(m, at++);
            v->phase_init = rhst_module_u8(m, at++);
        } else {
            v->step_length = (uint16_t)(byte - RHST_LENGTH_FIRST + 1);
        }
    }
    v->step_count = v->step_length;
    v->stream = at;
    return StepResult_Continue;
}

// The chip registers of a voice

// The volume envelope: a delay, an attack that adds `rate` to the volume up to 15, then a decay that subtracts it.
static void run_envelope(RhstVoice* v) {
    if (!v->envelope) {
        return;
    }
    if (v->rate >= 16) {
        v->rate = (uint8_t)(v->rate - 16);
        return;
    }
    if (v->attack) {
        const uint32_t sum = (uint32_t)v->rate + v->volume;
        v->volume = sum < 16 ? (uint8_t)sum : 15;
        if (v->phase_left >= 16) {
            v->phase_left = (uint8_t)(v->phase_left - 16);
        } else {
            v->attack = false;
            v->rate = v->decay_init;
        }
    } else {
        const int8_t volume = (int8_t)(v->volume - v->rate);
        v->volume = volume < 0 ? 0 : (uint8_t)volume;
        if (--v->phase_left == 0) {
            v->envelope = false;
        }
    }
}

// The vibrato triangle: `vib_pos` runs between 0 and twice the depth; the period moves by its distance from the middle,
// scaled up for the low notes (the period table steps are wider there).
static uint16_t run_vibrato(RhstVoice* v, bool toggle_before, uint16_t note_index2, uint16_t period) {
    const uint16_t top = (uint16_t)(v->vib_depth * 2);
    uint16_t pos = v->vib_pos;
    if (!(v->vib_half && toggle_before)) {
        if (!v->vib_rising) {
            pos = (uint16_t)(pos - v->vib_step);
            if ((int16_t)pos < 0) {
                v->vib_rising = true;
                pos = 0;
            }
        } else {
            pos = (uint16_t)(pos + v->vib_step);
            if (pos > top) {
                v->vib_rising = false;
                pos = top;
            }
        }
        v->vib_pos = pos;
    }
    uint16_t offset = (uint16_t)(pos - top / 2);
    uint16_t scale = (uint16_t)(note_index2 - PERIOD_FLOOR_SHIFT);
    while ((int16_t)scale < 0) {
        offset = (uint16_t)(offset << 1);
        scale = (uint16_t)(scale + PERIOD_OCTAVE);
    }
    return (uint16_t)(period + offset);
}

// The period of the voice's note: the semitone list steps along, the table gives the period.
static uint16_t note_period(RhstPlayer* p, RhstVoice* v, uint16_t* index2) {
    const RhstModule* m = p->module;
    uint32_t at = v->env_at;
    uint8_t semitone = rhst_module_u8(m, at++);
    if (semitone == RHST_LIST_END) {
        at = v->env_start;
        semitone = rhst_module_u8(m, at++);
    }
    v->env_at = at;
    *index2 = (uint16_t)(2 * (uint16_t)(semitone + v->note));
    return rhst_module_u16(m, (uint32_t)((int32_t)m->periods + (int16_t)*index2));
}

// The mixer register: the bits of this voice's mask come from the shadow, or from the noise trigger or percussion.
static void run_mixer(RhstPlayer* p, RhstVoice* v) {
    uint8_t bits;
    if (v->noise_trigger) {
        v->noise_trigger = false;
        p->noise_work = 0;
        bits = MIXER_NOISE_ON;
    } else {
        bits = p->mixer_shadow;
        if (v->toggle && v->percussion) {
            p->noise_work = p->noise_freq ^ 0x08;
            bits = MIXER_NOISE_ON;
        }
    }
    p->mixer_out = (uint8_t)((bits & v->mixer_mask) | (p->mixer_out & ~v->mixer_mask));
}

// The chip's period and volume of a voice for this tick.
static void run_voice(RhstPlayer* p, uint32_t index) {
    RhstVoice* v = &p->voices[index];
    const bool toggle_before = v->toggle;
    run_envelope(v);
    uint16_t index2 = 0;
    uint16_t period = note_period(p, v, &index2);
    if (v->vib_on) {
        period = run_vibrato(v, toggle_before, index2, period);
    }
    v->toggle = !v->toggle;
    if (v->slide) {
        const uint16_t delay = (uint16_t)(v->slide_delay - 1);
        if (delay != 0) {
            v->slide_delay = delay;
        } else {
            v->slide_total = (uint16_t)(v->slide_total + (uint16_t)v->slide_step);
            period = (uint16_t)(period + v->slide_total);
        }
    }
    run_mixer(p, v);
    p->out_period[index] = period;
    p->out_volume[index] = v->volume;
}

// The play routine: steps the sequencer every `tempo` ticks, then computes the registers of the three voices. The stop
// command ends the routine at once.
static void play(RhstPlayer* p) {
    if (!p->active) {
        return;
    }
    p->noise_work = p->noise_freq;
    if (--p->tempo_count == 0) {
        for (uint32_t i = 0; i < RHST_RAW_VOICES; i++) {
            if (step_voice(p, &p->voices[i]) == StepResult_Stop) {
                return;
            }
        }
        p->tempo_count = p->tempo;
    }
    for (uint32_t i = 0; i < RHST_RAW_VOICES; i++) {
        run_voice(p, i);
    }
    p->noise_out = p->noise_work;
}

// The output stage

static void write_tone(RhstPlayer* p, uint32_t channel, uint32_t voice) {
    NuadePaulaChannel* paula = &p->paula[channel];
    paula->volume = paula_volume(p->out_volume[voice]);
    paula->period = (uint16_t)(p->out_period[voice] << 2);
}

// The second channel: the tone, or the noise sample at a period from the noise register.
static void write_second_channel(RhstPlayer* p, bool noise, uint32_t noise_add, uint32_t noise_shift) {
    const RhstModule* m = p->module;
    NuadePaulaChannel* paula = &p->paula[1];
    if (noise) {
        set_sample(m, paula, m->noise, m->noise_words);
        paula->period = (uint16_t)(((p->noise_out + noise_add) & 0xFF) << noise_shift);
    } else {
        set_sample(m, paula, TONE_SQUARE, TONE_WORDS);
        paula->period = (uint16_t)(p->out_period[1] << 2);
    }
}

// The first and second builds write the voices the same way.
static void write_registers_tone_b(RhstPlayer* p) {
    write_tone(p, 0, 0);
    p->paula[1].volume = paula_volume(p->out_volume[1]);
    write_second_channel(p, (p->mixer_out & MIXER_TONE_B) == 0, 2, 4);
    write_tone(p, 2, 2);
}

static void output_idle(RhstPlayer* p) {
    if (p->active) {
        play(p);
        write_registers_tone_b(p);
    }
    if (!p->active) {
        p->paula[1].volume = 0;
    }
}

static void output_gated(RhstPlayer* p) {
    if (p->active) {
        play(p);
        write_registers_tone_b(p);
    }
}

// The third build: always writes. The fourth channel plays a window below the module (zeros) at volume 0 once the
// first tick is over.
static void output_always(RhstPlayer* p) {
    const RhstModule* m = p->module;
    play(p);
    write_tone(p, 0, 0);
    p->paula[1].volume = p->out_volume[1] & 0x80 ? 0 : paula_volume(p->out_volume[1]);
    write_second_channel(p, (p->mixer_out & MIXER_TONE_B_OFF) != 0, 4, 6);
    write_tone(p, 2, 2);
    if (!p->fourth_written) {
        p->fourth_written = true;
        return;
    }
    NuadePaulaChannel* fourth = &p->paula[3];
    fourth->volume = 0;
    fourth->lc = sample_at(m, m->size);
    fourth->len_words = NOISE_FOURTH_WORDS;
    fourth->period = NOISE_FOURTH_PERIOD;
}

RhstTickResult rhst_tick(RhstPlayer* player, NuadeVoices* voices) {
    player->song_ended = false;
    // The fourth channel's audio interrupt: it restarts the short waveform
    set_sample(player->module, &player->paula[3], TONE_FOURTH, TONE_WORDS);
    switch (player->module->output) {
        case RhstOutput_Idle:
            output_idle(player);
            break;
        case RhstOutput_Gated:
            output_gated(player);
            break;
        case RhstOutput_Always:
            output_always(player);
            break;
    }
    store_voices(player);
    for (uint32_t i = 0; i < RHST_CHANNELS; i++) {
        nuade_paula_channel_sync(&player->paula[i], i, voices);
    }
    return (RhstTickResult) { .looped = player->song_ended, .ended = player->ended };
}
