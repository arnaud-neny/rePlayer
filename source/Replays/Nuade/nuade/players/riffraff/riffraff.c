// SPDX-License-Identifier: MIT
// Riff Raff player: the replayer of the modules, one function per routine. A tick counts every voice's note duration
// down and reads the next pattern events when it ends, runs the volume envelope, the master fade (voice 0 only) and the
// pitch slide, then starts the notes (DMA off, sample registers); the interrupt then switches the DMA of all voices on
// and writes the repeat registers (the eagleplayer waits for the DMA start before each of the two).
//
// Left out: the ADKCON writes of the modulation flags (the eagleplayer's patch makes them clear bits, which are clear),
// the key and pause flags of Bomb Fusion (never set), the master volume scaling of the eagleplayer (identity at full
// volume) and the raster waits.

#include "rr_internal.h"

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

#define RR_INITIAL_VOLUME 0x40
#define RR_INITIAL_PERIOD 0x7C
#define RR_MAX_FADE_LEVEL 0x40

static const int8_t* rr_chip(const RrPlayer* player, uint32_t offset) {
    return (const int8_t*)(player->module.image + offset);
}

// A byte of the song. The walk at open proved every read in range; a failed read ends the pattern.

static uint8_t rr_song_byte(const RrPlayer* player, uint32_t at) {
    uint8_t byte = RR_PATTERN_END;
    rr_module_u8(&player->module, at, &byte);
    return byte;
}

// AUDxVOL: the clamped version holds the volume under the master level.

static void rr_write_volume(RrPlayer* player, uint32_t v) {
    RrVoice* voice = &player->voices[v];
    uint16_t volume = voice->volume;
    if (player->module.version->driver == RrDriver_Clamped && player->fade.level < volume) {
        volume = player->fade.level;
    }
    voice->paula.volume = (uint8_t)volume;
}

// $2bc: the volume envelope steps every (reload + 1) ticks towards the target volume.

static void rr_volume_envelope(RrPlayer* player, uint32_t v) {
    RrVoice* voice = &player->voices[v];
    voice->envelope_counter = (int16_t)(voice->envelope_counter - 1);
    if (voice->envelope_counter < 0) {
        voice->envelope_counter = (int16_t)voice->envelope_reload;
        if (voice->volume_step != 0) {
            voice->volume = (uint16_t)(voice->volume + voice->volume_step);
            const bool reached
                = voice->volume_step > 0 ? voice->volume >= voice->volume_target : voice->volume < voice->volume_target;
            if (reached) {
                voice->volume = voice->volume_target;
                voice->volume_target = 0;
                voice->volume_step = 0;
            }
        }
    }
    rr_write_volume(player, v);
}

// $55a: the master fade moves its level one step towards its target every (reload + 1) ticks and pulls the voices'
// volumes along: they follow it down when above it, and up when at or below it.

static void rr_master_fade(RrPlayer* player) {
    RrFade* fade = &player->fade;
    if (fade->level != fade->target) {
        const bool down = fade->target < fade->level;
        const uint16_t old_level = fade->level; // the voices are compared with the level before this step
        fade->counter = (int16_t)(fade->counter - 1);
        if (fade->counter < 0) {
            fade->counter = (int16_t)fade->reload;
            fade->level = (uint16_t)(fade->level + (down ? -1 : 1));
            for (uint32_t k = 0; k < RR_VOICES; k++) {
                RrVoice* voice = &player->voices[k];
                if (down && old_level < voice->volume) {
                    voice->volume--;
                } else if (!down && old_level >= voice->volume) {
                    voice->volume++;
                }
            }
        }
    }
    rr_write_volume(player, 0);
}

// $31a: the pitch slide adds its step to the period until it reaches the target. Bomb Fusion writes AUDxPER only when
// a slide upwards ends; the other versions write it every tick.

static void rr_pitch_slide(RrPlayer* player, uint32_t v) {
    RrVoice* voice = &player->voices[v];
    bool write = player->module.version->driver != RrDriver_OneShot;
    if (voice->slide_step > 0) {
        voice->period = (uint16_t)(voice->period + voice->slide_step);
        if (!voice->slide_endless && voice->period >= voice->slide_target) {
            voice->period = voice->slide_target;
            voice->slide_target = 0;
            voice->slide_step = 0;
            write = true;
        }
    } else if (voice->slide_step < 0) {
        voice->period = (uint16_t)(voice->period + voice->slide_step);
        if (voice->period < voice->slide_target) {
            voice->period = voice->slide_target;
            voice->slide_target = 0;
            voice->slide_step = 0;
        }
    }
    if (write) {
        voice->paula.period = voice->period;
    }
}

// The eagleplayer's patch at the point where a voice restarts its track list: SongEnd once every voice has done so.

static void rr_voice_wrapped(RrPlayer* player, uint32_t v) {
    player->wrap_pending[v] = false;
    for (uint32_t k = 0; k < RR_VOICES; k++) {
        if (player->wrap_pending[k]) {
            return;
        }
    }
    player->looped = true;
    memcpy(player->wrap_pending, player->wrap_initial, sizeof(player->wrap_pending));
}

// One iteration of the event loop (a pattern byte or a track list byte) of the call that is running; false when the
// call has used its budget. The open-time walk refuses modules that need more, so this only stops a module the walk
// does not know.

static bool rr_event_step(uint32_t* steps) {
    return (*steps)++ < RR_EVENT_BUDGET;
}

// $506: the pattern ended; the next track list entry names the next pattern and how often it repeats.

static void rr_next_track_entry(RrPlayer* player, uint32_t v, uint32_t* steps) {
    RrVoice* voice = &player->voices[v];
    const bool oneshot = player->module.version->driver == RrDriver_OneShot;
    uint32_t pos = voice->track_pos;
    while (rr_event_step(steps)) {
        const uint8_t byte = rr_song_byte(player, pos++);
        if (rr_module_track_list_ends(&player->module, byte)) {
            pos = voice->track_restart;
            rr_voice_wrapped(player, v);
            continue;
        }
        if (!oneshot && byte == RR_TRACK_TRANSPOSE) {
            voice->transpose = rr_song_byte(player, pos++);
            continue;
        }
        uint32_t pattern = 0;
        rr_module_pattern(&player->module, byte, &pattern);
        voice->pattern_start = pattern;
        voice->pattern_pos = pattern;
        voice->repeat = rr_song_byte(player, pos++);
        voice->track_pos = pos;
        return;
    }
    voice->track_pos = pos;
}

static void rr_set_sample(RrPlayer* player, uint32_t v, uint8_t instrument) {
    RrVoice* voice = &player->voices[v];
    RrSample sample = { 0 };
    voice->instrument = instrument;
    if (rr_module_sample(&player->module, instrument, &sample)) {
        voice->sample_start = sample.start;
        voice->sample_words = sample.words;
    }
}

// A volume slide request: step by one towards `target` every (rate + 1) ticks.

static void rr_set_volume_slide(RrVoice* voice, uint8_t target, uint8_t rate) {
    if (target == voice->volume) {
        return;
    }
    voice->volume_step = target > voice->volume ? 1 : -1;
    voice->volume_target = target;
    voice->envelope_reload = rate;
    voice->envelope_counter = rate;
}

// Runs one pattern command. False when the command ends the voice's event processing (a rest).

static bool rr_run_command(RrPlayer* player, uint32_t v, RrCommandInfo info) {
    RrVoice* voice = &player->voices[v];
    uint8_t arg[2] = { 0, 0 };
    for (uint32_t i = 0; i < info.arg_bytes; i++) {
        arg[i] = rr_song_byte(player, voice->pattern_pos++);
    }
    uint16_t rate = 0;
    switch (info.command) {
        case RrCommand_SetSample:
            rr_set_sample(player, v, arg[0]);
            break;
        case RrCommand_VolumeSlide:
            rr_set_volume_slide(voice, arg[0], arg[1]);
            break;
        case RrCommand_MasterFade:
            player->fade.target = arg[0];
            player->fade.reload = arg[1];
            player->fade.counter = arg[1];
            // the routine goes on into the voice's own slide setup with the same two bytes, which does not start a
            // step: only the target and the rate change
            voice->volume_target = arg[0];
            voice->envelope_reload = arg[1];
            voice->envelope_counter = arg[1];
            break;
        case RrCommand_SetVolume:
            voice->volume = arg[0];
            break;
        case RrCommand_Rest:
        case RrCommand_RestSilence:
            rr_module_rate(&player->module, arg[0], &rate);
            voice->wait = rate;
            if (info.command == RrCommand_RestSilence) {
                voice->paula.lc = rr_chip(player, rr_module_silence(&player->module));
                voice->paula.len_words = 1;
            }
            return false;
        case RrCommand_PitchSlide:
            voice->slide_step = (int16_t)(((uint16_t)arg[0] << 8) | arg[1]);
            voice->slide_endless = true;
            break;
        case RrCommand_PitchSlideOff:
            voice->slide_step = 0;
            voice->slide_endless = false;
            break;
        case RrCommand_Modulation:
        case RrCommand_Loop:
        case RrCommand_Portamento:
        case RrCommand_Invalid:
            break;
    }
    return true;
}

// $372: reads events until one returns: a note (period, duration, DMA restart) or a rest.

static void rr_next_event(RrPlayer* player, uint32_t v) {
    RrVoice* voice = &player->voices[v];
    const RrDriver driver = player->module.version->driver;
    uint32_t steps = 0;
    while (rr_event_step(&steps)) {
        const uint8_t byte = rr_song_byte(player, voice->pattern_pos++);
        if (byte == RR_PATTERN_END) {
            voice->repeat = (int16_t)(voice->repeat - 1);
            if (voice->repeat < 0) {
                rr_next_track_entry(player, v, &steps);
            } else {
                voice->pattern_pos = voice->pattern_start;
            }
            continue;
        }
        if (byte >= RR_COMMAND_BASE) {
            if (!rr_run_command(player, v, rr_command_info(driver, byte))) {
                return;
            }
            continue;
        }
        uint16_t period = 0;
        uint16_t rate = 0;
        rr_module_period(&player->module, (uint32_t)byte + voice->transpose, &period);
        rr_module_rate(&player->module, rr_song_byte(player, voice->pattern_pos++), &rate);
        voice->period = period;
        voice->wait = rate;
        voice->start_dma = true;
        return;
    }
    voice->wait = 0xFFFF;
}

// $3b8: the notes that were read start: sample registers, then DMA off (the interrupt switches it on again).

static void rr_start_notes(RrPlayer* player) {
    for (uint32_t v = 0; v < RR_VOICES; v++) {
        RrVoice* voice = &player->voices[v];
        if (!player->enabled[v] || !voice->start_dma) {
            continue;
        }
        voice->paula.lc = rr_chip(player, voice->sample_start);
        voice->paula.len_words = voice->sample_words;
        voice->paula.period = voice->period;
        nuade_paula_channel_dma_off(&voice->paula);
        voice->start_dma = false;
    }
}

// $2c0: the repeat registers, written after the DMA start.

static void rr_write_repeats(RrPlayer* player) {
    const bool oneshot = player->module.version->driver == RrDriver_OneShot;
    for (uint32_t v = 0; v < RR_VOICES; v++) {
        RrVoice* voice = &player->voices[v];
        if (!player->enabled[v]) {
            continue;
        }
        RrSample loop = { .start = rr_module_silence(&player->module), .words = 1 };
        if (!oneshot) {
            rr_module_loop(&player->module, voice->instrument, &loop);
        }
        voice->paula.lc = rr_chip(player, loop.start);
        voice->paula.len_words = loop.words;
    }
}

// The tick routine ($212) and the interrupt around it.

static void rr_run_voices(RrPlayer* player) {
    for (uint32_t v = 0; v < RR_VOICES; v++) {
        if (!player->enabled[v]) {
            continue;
        }
        RrVoice* voice = &player->voices[v];
        voice->wait = (uint16_t)(voice->wait - 1);
        if (voice->wait == 0) {
            rr_next_event(player, v);
        }
        rr_volume_envelope(player, v);
        if (v == 0) {
            rr_master_fade(player);
        }
        rr_pitch_slide(player, v);
    }
}

RrTickResult rr_tick(RrPlayer* player, NuadeVoices* voices) {
    player->looped = false;
    rr_run_voices(player);
    rr_start_notes(player);
    for (uint32_t v = 0; v < RR_VOICES; v++) {
        if (player->enabled[v]) {
            nuade_paula_channel_dma_on(&player->voices[v].paula);
        }
    }
    rr_write_repeats(player);
    for (uint32_t v = 0; v < RR_VOICES; v++) {
        nuade_paula_channel_sync(&player->voices[v].paula, v, voices);
    }
    return (RrTickResult) { .looped = player->looped };
}

// The init routine: every voice gets its initial registers and its track list; the master fade starts at full level.

bool rr_start(RrPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    RrSongStart song = { 0 };
    if (player == nullptr || !rr_module_song_start(&player->module, subsong, &song)) {
        return false;
    }
    // everything that can fail comes before the first change of state; a disabled voice never reads its words
    RrVoiceStart starts[RR_VOICES] = { { 0 } };
    for (uint32_t v = 0; v < RR_VOICES; v++) {
        if (song.enabled[v] && !rr_module_voice_start(&player->module, v, &starts[v])) {
            return false;
        }
    }
    const uint32_t silence = rr_module_silence(&player->module);
    memcpy(player->enabled, song.enabled, sizeof(player->enabled));
    player->fade = (RrFade) { .level = RR_MAX_FADE_LEVEL, .target = RR_MAX_FADE_LEVEL };
    player->looped = false;
    for (uint32_t v = 0; v < RR_VOICES; v++) {
        // the eagleplayer's flags: voice 3 only counts when it is enabled
        player->wrap_initial[v] = v < 3 || player->enabled[3];
        player->wrap_pending[v] = player->wrap_initial[v];
        player->voices[v] = (RrVoice) {
            .volume = RR_INITIAL_VOLUME,
            .sample_start = silence,
            .sample_words = 1,
            .period = RR_INITIAL_PERIOD,
            .wait = 1,
            .pattern_pos = player->module.base[RrRole_StartPattern],
            .track_pos = song.track_list[v],
            .track_restart = song.track_list[v],
            .transpose = starts[v].transpose,
            .instrument = starts[v].instrument,
        };
        NuadePaulaChannel* paula = &player->voices[v].paula;
        paula->lc = rr_chip(player, silence);
        paula->len_words = 1;
        paula->period = RR_INITIAL_PERIOD;
        paula->volume = RR_INITIAL_VOLUME;
        if (voices != nullptr) {
            nuade_voice_stop(voices, v);
        }
    }
    return true;
}

bool rr_probe(const uint8_t* data, size_t size) {
    RrModule module;
    if (!rr_module_load(&module, data, size)) {
        return false;
    }
    const bool ok = rr_module_validate(&module);
    rr_module_free(&module);
    return ok;
}

RrPlayer* rr_create(const uint8_t* data, size_t size) {
    RrPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    if (!rr_module_load(&player->module, data, size) || !rr_module_validate(&player->module)) {
        rr_module_free(&player->module);
        free(player);
        return nullptr;
    }
    const RrModule* module = &player->module;
    for (uint32_t i = 0; i < RR_TITLE_BYTES - 1; i++) {
        uint8_t c = 0;
        if (!rr_module_u8(module, module->header.name + i, &c) || c == 0) {
            break;
        }
        player->title[i] = (char)c;
    }
    return player;
}

void rr_destroy(RrPlayer* player) {
    if (player != nullptr) {
        rr_module_free(&player->module);
        free(player);
    }
}

uint32_t rr_subsong_count(const RrPlayer* player) {
    return player->module.header.subsong_count;
}

const char* rr_title(const RrPlayer* player) {
    return player->title;
}

RrVoiceState rr_voice_state(const RrPlayer* player, uint32_t voice) {
    const RrVoice* state = &player->voices[voice];
    return (RrVoiceState) { .period = state->period, .instrument = state->instrument };
}
