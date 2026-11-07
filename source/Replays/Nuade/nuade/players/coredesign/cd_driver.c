// SPDX-License-Identifier: MIT
// Core Design player: the driver's tick (voices, envelopes, the three track sequencers, sound effects).
//
// Per tick the driver runs the four voice updates first (registers, DMA start, envelope), then the song's tracks, which
// start notes. A note start cuts the voice's DMA; the next tick's voice update writes the registers and starts the
// DMA, and the audio interrupt that this raises sets up the loop part for the next block.

#include "cd_internal.h"
#include "nuade_tracker.h"
#include <string.h>

#define SILENCE_WORDS 0x80   // the silent buffer that a finished note loops
#define PAULA_VOLUME_SHIFT 1 // the driver's volume is 8 bit, Paula's 7
#define TIMER_STOPPED 0xFFFF
#define MAX_TRACK_STEPS 4096 // order list entries and patterns one track tick may run through

// Sound effect voices: the early driver plays on voice 3. The late driver picks voice 0, or voice 1 when the subsong
// asks for a voice (`param` other than 0).
#define SFX_EARLY_VOICE 3
#define SFX_LATE_VOICE 0
#define SFX_LATE_VOICE_ASKED 1
#define SFX_PARAM_ANY 0

// Order list commands: a byte below 0x80 is a pattern number.
#define ORDER_REPEAT 0x80 // 0x80 to 0xBF: repeat count in the low five bits
#define ORDER_REPEAT_MASK 0x1F
#define ORDER_INSTRUMENT 0xC0 // 0xC0 to 0xFD: instrument slot in the low bits, the offset of its instrument follows
#define ORDER_TRANSPOSE 0xFE  // the transposition follows
#define ORDER_END 0xFF

// Pattern events and commands: a command is a byte from 0x80 on.
#define COMMAND_FIRST 0x80
#define EVENT_REST 0x7F              // the number of ticks follows
#define EVENT_PERIOD 0x7E            // the period follows, low byte first
#define COMMAND_INSTRUMENT_LAST 0x88 // 0x80 to 0x88: select an instrument slot
#define COMMAND_VIBRATO 0x89         // 0x89 to 0xBF: select a vibrato, number in the low nibble
#define COMMAND_VIBRATO_LAST 0xBF
#define COMMAND_VIBRATO_MASK 0x0F
#define COMMAND_NONE 0xC2 // does nothing; the others of 0xC0 to 0xFE skip three bytes
#define COMMAND_SKIP 3
#define COMMAND_END 0xFF // the pattern ends

#define VIBRATO_STEP_DELAY 7 // a step: bits 3 to 7 the note offset, bits 0 to 2 the delay

// AUDxLC / AUDxLEN of a sample of the hunk: AUDxLC ignores its lowest bit; a sample plays zeros past the hunk's end.
// An address past the hunk is memory the module does not own: it plays the zeros behind the hunk, the only memory
// with room for any length.

static void set_sample(const CdMemory* m, NuadePaulaChannel* paula, uint32_t offset, uint16_t length_words) {
    offset &= ~1u;
    paula->lc = (const int8_t*)(m->data + (offset <= m->size ? offset : m->size));
    paula->len_words = length_words;
}

static bool is_command(uint8_t byte) {
    return byte >= COMMAND_FIRST;
}

// The audio interrupt that a voice's DMA start raises. It sets the sample block that follows the first one in the
// voice's record (the tick writes it to the registers), once: the eagleplayer's patch disables the interrupt.

static void audio_interrupt(const CdModule* module, CdVoice* v) {
    if (v->irq_done || !v->active) {
        return;
    }
    v->irq_done = true;
    const bool loops = module->version.attack_flag ? v->has_attack : !v->one_shot;
    if (loops) {
        const CdInstrument instrument = cd_read_instrument(&module->memory, v->instrument);
        v->block = instrument.sample + 2 + (uint32_t)(int32_t)instrument.loop_offset;
        v->block_words = instrument.loop_words;
    } else if (v->one_shot) {
        v->block = module->tables.silence;
        v->block_words = SILENCE_WORDS;
    }
}

// Start a note: the registers are set up, the DMA is cut and the volume is 0. The next voice update starts the DMA.

static void voice_start(const CdModule* module, CdVoice* v, uint32_t instrument_offset, uint16_t period,
                        uint16_t timer) {
    const CdInstrument instrument = cd_read_instrument(&module->memory, instrument_offset);
    v->instrument = instrument_offset;
    v->period = period;
    v->timer = timer;
    v->volume = 0;
    v->envelope = CdEnvelope_Attack;
    v->paula.volume = 0;
    nuade_paula_channel_dma_off(&v->paula);
    v->active = true;
    v->irq_done = true;
    v->one_shot = instrument.one_shot;
    v->has_attack = instrument.has_attack;
    v->block = instrument.sample + 2;
    v->block_words = module->version.attack_flag && instrument.has_attack ? (uint16_t)instrument.loop_offset
                                                                          : cd_u16(&module->memory, instrument.sample);
    v->display_note = nuade_period_to_note(period);
    v->display_instrument = (int)((instrument_offset - module->header.instruments) / CD_INSTRUMENT_SIZE);
}

// One step of the volume envelope. The volume is an 8 bit value: a sum that turns negative ends the stage.

static void envelope_step(const CdModule* module, CdVoice* v) {
    const CdInstrument instrument = cd_read_instrument(&module->memory, v->instrument);
    switch (v->envelope) {
        case CdEnvelope_Attack:
            v->volume = (uint8_t)(v->volume + instrument.attack_rate);
            if ((int8_t)v->volume < 0 || instrument.peak <= (int8_t)v->volume) {
                v->volume = (uint8_t)instrument.peak;
                v->envelope = CdEnvelope_Decay;
            }
            break;
        case CdEnvelope_Decay:
            v->volume = (uint8_t)(v->volume + instrument.decay_rate);
            if ((int8_t)v->volume < 0 || instrument.sustain_level >= (int8_t)v->volume) {
                v->volume = (uint8_t)instrument.sustain_level;
                v->envelope = CdEnvelope_Sustain;
            }
            break;
        case CdEnvelope_Sustain:
            if (v->timer == 0) {
                v->envelope = CdEnvelope_Release;
            }
            break;
        case CdEnvelope_Release:
            v->volume = (uint8_t)(v->volume + instrument.release_rate);
            if ((int8_t)v->volume < 0 && !v->one_shot) {
                // The note ends: the voice loops the silent buffer from now on. A one-shot note keeps adding the
                // release rate, so its volume wraps around.
                v->active = false;
                set_sample(&module->memory, &v->paula, module->tables.silence, SILENCE_WORDS);
            }
            break;
    }
}

// The tick of one voice: write the registers, start the DMA if it is off, run the envelope.

static void voice_update(const CdModule* module, CdVoice* v) {
    if (!v->active) {
        return;
    }
    NuadePaulaChannel* paula = &v->paula;
    paula->volume = (uint8_t)(v->volume >> PAULA_VOLUME_SHIFT);
    paula->period = v->period;
    set_sample(&module->memory, paula, v->block, v->block_words);
    if (!paula->dma) {
        v->irq_done = false;
        nuade_paula_channel_dma_on(paula);
        audio_interrupt(module, v);
    }
    if (v->timer != 0 && v->timer != TIMER_STOPPED) {
        v->timer--;
    }
    envelope_step(module, v);
}

// The vibrato of a track: a sequence of note offsets that the period of the voice follows, each step held for a few
// ticks.

static void vibrato_step(const CdModule* module, CdTrack* t, CdVoice* v) {
    if (!t->vibrato_running) {
        return;
    }
    if (t->vibrato_number == 0) {
        t->vibrato_running = false;
        return;
    }
    if (!t->vibrato_restart) {
        t->vibrato_delay--;
        if (t->vibrato_delay != 0) {
            return;
        }
        t->vibrato_steps--;
        if (t->vibrato_steps != 0) {
            const uint8_t step = cd_u8(&module->memory, t->vibrato_pos++);
            t->vibrato_delay = step & VIBRATO_STEP_DELAY;
            v->period = cd_read_period(&module->memory, &module->tables, (uint8_t)((step >> 3) + t->note));
            return;
        }
    }
    const CdVibrato vibrato = cd_read_vibrato(&module->memory, &module->tables, t->vibrato_number);
    if (!vibrato.restart && !t->vibrato_restart) {
        t->vibrato_steps = 1;
        return;
    }
    t->vibrato_restart = false;
    t->vibrato_delay = vibrato.delay;
    t->vibrato_steps = vibrato.steps;
    t->vibrato_pos = vibrato.sequence;
    v->period = cd_read_period(&module->memory, &module->tables, t->note);
}

// A pattern command (a negative byte): select an instrument, end the pattern, select a vibrato, or skip.

static void pattern_command(const CdModule* module, CdTrack* t) {
    const uint8_t command = cd_u8(&module->memory, t->pattern_pos++);
    if (command <= COMMAND_INSTRUMENT_LAST) {
        t->instrument = module->header.instruments + t->slots[command % CD_INSTRUMENT_SLOTS];
    } else if (command == COMMAND_END) {
        t->pattern_ended = true;
    } else if (command >= COMMAND_VIBRATO && command <= COMMAND_VIBRATO_LAST) {
        t->vibrato_select = command & COMMAND_VIBRATO_MASK;
    } else if (command != COMMAND_NONE) {
        t->pattern_pos += COMMAND_SKIP;
    }
}

// The pattern ended: play it again while repeats are left, else read the order list up to the next pattern. False if
// the order list ended.

static bool track_advance(const CdModule* module, CdTrack* t) {
    const CdMemory* m = &module->memory;
    t->repeats--;
    if (t->repeats != 0) {
        t->pattern_pos = t->pattern_start;
        t->pattern_ended = false;
        return true;
    }
    t->repeats = 1;
    // The order list position is stored only when a pattern is found: after the end command the track keeps reading
    // that command.
    uint32_t pos = t->order_pos;
    for (;;) {
        const uint8_t entry = cd_u8(m, pos++);
        if (entry < ORDER_REPEAT) {
            t->order_pos = pos;
            t->pattern_start = cd_read_pattern(m, &module->tables, entry);
            t->pattern_pos = t->pattern_start;
            t->pattern_ended = false;
            return true;
        }
        if (entry == ORDER_TRANSPOSE) {
            t->transpose = cd_u8(m, pos++);
        } else if (entry == ORDER_END) {
            t->active = false;
            return false;
        } else if (entry < ORDER_INSTRUMENT) {
            t->repeats = entry & ORDER_REPEAT_MASK;
        } else {
            t->slots[entry % CD_INSTRUMENT_SLOTS] = cd_u8(m, pos++);
        }
    }
}

// One tick of the pattern: count the current event down and, when it is due, read the next one. Returns false when the
// pattern ended before an event could be read.

static bool pattern_step(const CdModule* module, CdTrack* t, CdVoice* v) {
    const CdMemory* m = &module->memory;
    t->note_ticks--;
    if (t->note_ticks != 0) {
        if (is_command(cd_u8(m, t->pattern_pos))) {
            pattern_command(module, t);
        }
        return true;
    }
    while (is_command(cd_u8(m, t->pattern_pos))) {
        pattern_command(module, t);
        if (t->pattern_ended) {
            return false;
        }
    }
    const uint8_t event = cd_u8(m, t->pattern_pos++);
    if (event == EVENT_REST) {
        t->note_ticks = cd_u8(m, t->pattern_pos++);
        return true;
    }
    uint16_t period;
    if (event == EVENT_PERIOD) {
        period = cd_u8(m, t->pattern_pos);
        period |= (uint16_t)(cd_u8(m, t->pattern_pos + 1) << 8);
        t->pattern_pos += 2;
    } else {
        t->note = (uint8_t)(event + t->transpose + module->version.note_offset);
        period = cd_read_period(m, &module->tables, t->note);
    }
    t->vibrato_running = true;
    t->vibrato_restart = true;
    t->vibrato_number = t->vibrato_select;
    const uint8_t length = cd_u8(m, t->pattern_pos++);
    t->note_ticks = length;
    voice_start(module, v, t->instrument, period, length);
    return true;
}

static void track_tick(const CdModule* module, CdTrack* t, CdVoice* v) {
    vibrato_step(module, t, v);
    for (int step = 0; step < MAX_TRACK_STEPS; step++) {
        if (t->pattern_ended && !track_advance(module, t)) {
            return;
        }
        if (pattern_step(module, t, v)) {
            return;
        }
    }
    t->active = false;
}

// The tick of the song: the tracks play on voices 1 to 3. When every order list has ended, the flag drops and the next
// tick restarts the song.

static void music_step(CdPlayer* player) {
    if (!player->music_active) {
        return;
    }
    player->music_active = false;
    for (int i = 0; i < CD_TRACKS; i++) {
        player->music_active |= player->tracks[i].active;
    }
    if (!player->music_active) {
        return;
    }
    for (int i = 0; i < CD_TRACKS; i++) {
        track_tick(&player->module, &player->tracks[i], &player->voices[i + 1]);
    }
}

// Start song `song`: all DMA off, the voices that play the song silenced, every track at the start of its order list.
// The tracks keep their vibrato state.

static void song_init(CdPlayer* player, uint8_t song) {
    const CdModule* module = &player->module;
    for (int i = 0; i < CD_VOICES; i++) {
        nuade_paula_channel_dma_off(&player->voices[i].paula);
    }
    for (int i = 1; i < CD_VOICES; i++) {
        CdVoice* v = &player->voices[i];
        v->active = false;
        v->irq_done = false;
        v->one_shot = false;
        v->has_attack = false;
    }
    for (int i = 0; i < CD_TRACKS; i++) {
        CdTrack* t = &player->tracks[i];
        t->order_pos = cd_read_song_track(&module->memory, &module->tables, song, i);
        t->transpose = 0;
        t->vibrato_select = 0;
        t->pattern_ended = true;
        t->repeats = 1;
        t->note_ticks = 1;
        t->instrument = module->header.instruments;
        for (int slot = 0; slot < CD_INSTRUMENT_SLOTS; slot++) {
            t->slots[slot] = (uint8_t)(slot * CD_INSTRUMENT_SIZE);
        }
        t->active = true;
    }
    player->music_active = true;
}

// A sound effect plays one instrument at one period on a voice that depends on the driver. The eagleplayer starts a
// subsong with the "repeat" argument set, which these choices assume. A song that plays blocks the effects.

static void sound_effect_start(CdPlayer* player, const CdSubsong* subsong) {
    if (player->song_playing) {
        return;
    }
    const CdModule* module = &player->module;
    int voice;
    uint16_t timer;
    if (module->version.effect_voice_by_param) {
        voice = subsong->param == SFX_PARAM_ANY ? SFX_LATE_VOICE : SFX_LATE_VOICE_ASKED;
        timer = 0;
    } else {
        voice = SFX_EARLY_VOICE;
        timer = subsong->param;
    }
    // The instrument offset is a 16 bit product that the driver adds with a sign extension.
    const uint16_t offset = (uint16_t)(subsong->index * CD_INSTRUMENT_SIZE);
    voice_start(module, &player->voices[voice], module->header.instruments + (uint32_t)(int32_t)(int16_t)offset,
                subsong->period, timer);
}

// The driver's start routine: a subsong is a song or a sound effect.

static void subsong_start(CdPlayer* player, int subsong) {
    const CdSubsong entry = cd_read_subsong(&player->module.memory, &player->module.tables, (uint32_t)subsong);
    if (entry.sound_effect) {
        sound_effect_start(player, &entry);
        return;
    }
    player->subsong = subsong;
    song_init(player, (uint8_t)entry.index);
    player->song_playing = true;
}

void cd_driver_start(CdPlayer* player, int subsong) {
    memset(player->voices, 0, sizeof(player->voices));
    memset(player->tracks, 0, sizeof(player->tracks));
    player->song_playing = false;
    player->music_active = false;
    player->looped = false;
    for (int i = 0; i < CD_VOICES; i++) {
        player->voices[i].display_note = -1;
        player->voices[i].display_instrument = -1;
    }
    subsong_start(player, subsong);
}

void cd_driver_tick(CdPlayer* player) {
    player->looped = false;
    for (int i = 0; i < CD_VOICES; i++) {
        voice_update(&player->module, &player->voices[i]);
    }
    if (!player->song_playing) {
        return;
    }
    if (!player->music_active) {
        player->looped = true; // the eagleplayer's patch calls SongEnd here
        subsong_start(player, player->subsong);
    }
    music_step(player);
}
