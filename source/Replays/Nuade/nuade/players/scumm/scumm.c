// SPDX-License-Identifier: MIT
// SCUMM sound drivers: the replayers of the modules. The eagleplayer calls the driver once per tick; its first call
// (d0 = -1) initialises, the others (d0 = 0xF) play.
//
// The sweep effects (alarm, teleport) write Paula registers from a few counters. The tracker drivers read four event
// tracks: an event waits for its time, stops the channel it names and starts an instrument's note on it. Every tick
// then updates the notes that play: the envelopes give the period and the volume, the loop registers are rewritten,
// and a note ends after its duration (the two-channel driver lets it fade out, the four-channel one switches the
// channel off).
//
// Left out: the carry flag the drivers return (the eagleplayer ignores it) and their writes to the eagleplayer's work
// area that nothing reads. The four-channel driver also has a fade-out branch (the record's phase word) that no note
// can reach: a note start always clears the phase and nothing sets it.

#include "scm_internal.h"

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

#define SCM_IDLE_PERIOD 150 // AUDxPER of a channel before the driver's first write

#define SCM_ALARM_SHORT_PAIRS 2 // the pairs 0 and 2 bounce between the short limits, 1 and 3 between the long ones
#define SCM_ALARM_SHORT_LOW 0xAA
#define SCM_ALARM_SHORT_HIGH 0xFA
#define SCM_ALARM_LONG_LOW 0x19A
#define SCM_ALARM_LONG_HIGH 0x3B6
// The channel periods the init routine writes: the first of each pair, and the pair's step.
#define SCM_ALARM_INIT_PERIOD_0 0x2D0
#define SCM_ALARM_INIT_PERIOD_1 0x122
#define SCM_ALARM_INIT_PERIOD_2 0x2BC
#define SCM_ALARM_INIT_PERIOD_3 0x10E
// The second half of the alarm's waveform.
#define SCM_ALARM_HALF_BYTES SCM_SWEEP_SAMPLE_BYTES

// The teleports: one voice at a time, a cycle of four ticks.
#define SCM_TELEPORT_DOWN_START 0x1F4
#define SCM_TELEPORT_DOWN_END 0x80
#define SCM_TELEPORT_DOWN_VOLUME_BIAS 0x76
#define SCM_TELEPORT_UP_START 0x80
#define SCM_TELEPORT_UP_END 0x1F4
#define SCM_TELEPORT_UP_VOLUME_TOP 0x200
#define SCM_TELEPORT_STEP 4
#define SCM_TELEPORT_CYCLE 4
#define SCM_TELEPORT_PERIOD_SHIFT 4

static void scm_set_sample(NuadePaulaChannel* paula, const int8_t* lc, uint16_t len_words) {
    paula->lc = lc;
    paula->len_words = len_words;
}

// DMACON with all four audio bits set.

static void scm_dma_on_all(ScummPlayer* player) {
    for (uint32_t v = 0; v < SCUMM_VOICES; v++) {
        nuade_paula_channel_dma_on(&player->paula[v]);
    }
}

// The alarm's init routine: all four voices start (DMACON with every bit), the periods fall by 10 from voice to voice
// and the waveform alternates between its two halves.

static void scm_alarm_init(ScummPlayer* player) {
    static const ScmBounce start[SCM_ALARM_PAIRS] = {
        { SCM_ALARM_INIT_PERIOD_0, (uint16_t)-10 },
        { SCM_ALARM_INIT_PERIOD_1, 10 },
        { SCM_ALARM_INIT_PERIOD_2, (uint16_t)-5 },
        { SCM_ALARM_INIT_PERIOD_3, 7 },
    };
    const uint32_t first = player->module.samples;
    scm_dma_on_all(player);
    for (uint32_t v = 0; v < SCUMM_VOICES; v++) {
        const uint32_t half = first + (v % 2 != 0 ? SCM_ALARM_HALF_BYTES : 0);
        scm_set_sample(&player->paula[v], scm_module_sample(&player->module, half), SCM_SWEEP_WORDS);
        player->paula[v].period = start[v].period;
        player->paula[v].volume = SCM_SWEEP_VOLUME;
    }
    memcpy(player->alarm, start, sizeof(start));
}

// The word array of the alarm as the tick reads it: period, step, period, step, ...

static uint16_t scm_alarm_word(const ScummPlayer* player, uint32_t index) {
    const ScmBounce* pair = &player->alarm[index / 2];
    return index % 2 == 0 ? pair->period : pair->step;
}

// A period moves by its step and turns around at a limit.

static void scm_alarm_bounce(ScmBounce* pair, int16_t low, int16_t high) {
    int16_t period = (int16_t)(uint16_t)(pair->period + pair->step);
    int16_t step = (int16_t)pair->step;
    if (period < low) {
        period = low;
        step = (int16_t)-step;
    }
    if (period > high) {
        period = high;
        step = (int16_t)-step;
    }
    pair->period = (uint16_t)period;
    pair->step = (uint16_t)step;
}

// The alarm's tick. The channel periods come from the first four words of the array: voice 1 plays the step of the
// first pair, voice 3 the step of the second (the original steps its pointer by one word per voice).

static void scm_alarm_tick(ScummPlayer* player) {
    if (player->first_tick) {
        scm_alarm_init(player);
        return;
    }
    for (uint32_t v = 0; v < SCUMM_VOICES; v++) {
        player->paula[v].period = scm_alarm_word(player, v);
    }
    for (uint32_t i = 0; i < SCM_ALARM_PAIRS; i++) {
        if (i % SCM_ALARM_SHORT_PAIRS == 0) {
            scm_alarm_bounce(&player->alarm[i], SCM_ALARM_SHORT_LOW, SCM_ALARM_SHORT_HIGH);
        } else {
            scm_alarm_bounce(&player->alarm[i], SCM_ALARM_LONG_LOW, SCM_ALARM_LONG_HIGH);
        }
    }
}

// The voice a teleport tick plays: the sample base again with the period of the step and a volume that follows it.

static void scm_teleport_play(ScummPlayer* player, uint16_t period, int16_t volume) {
    NuadePaulaChannel* paula = &player->paula[player->teleport.cycle];
    scm_set_sample(paula, scm_module_sample(&player->module, player->module.samples), SCM_SWEEP_WORDS);
    paula->period = period;
    paula->volume = (uint8_t)volume;
}

static void scm_teleport_tick(ScummPlayer* player) {
    const bool down = player->module.version->driver == ScmDriver_TeleportDown;
    ScmTeleport* teleport = &player->teleport;
    if (player->first_tick) {
        scm_dma_on_all(player);
        teleport->cycle = 0;
        teleport->base = down ? SCM_TELEPORT_DOWN_START : SCM_TELEPORT_UP_START;
        scm_teleport_play(player, teleport->base, SCM_SWEEP_VOLUME);
        teleport->cycle = 1;
        return;
    }
    const uint16_t period = (uint16_t)((teleport->cycle << SCM_TELEPORT_PERIOD_SHIFT) + teleport->base);
    int16_t volume
        = down ? (int16_t)(period - SCM_TELEPORT_DOWN_VOLUME_BIAS) : (int16_t)(SCM_TELEPORT_UP_VOLUME_TOP - period);
    if (volume > SCM_SWEEP_VOLUME) {
        volume = SCM_SWEEP_VOLUME;
    }
    scm_teleport_play(player, period, volume);
    teleport->cycle = (uint16_t)((teleport->cycle + 1) % SCM_TELEPORT_CYCLE);
    if (teleport->cycle != 0) {
        return;
    }
    bool done = false;
    if (down) {
        teleport->base = (uint16_t)(teleport->base - SCM_TELEPORT_STEP);
        done = (int16_t)teleport->base <= SCM_TELEPORT_DOWN_END;
    } else {
        teleport->base = (uint16_t)(teleport->base + SCM_TELEPORT_STEP);
        done = (int16_t)teleport->base >= SCM_TELEPORT_UP_END;
    }
    if (done) {
        for (uint32_t v = 0; v < SCUMM_VOICES; v++) {
            nuade_paula_channel_dma_off(&player->paula[v]);
        }
    }
}

// The Paula voice of a driver channel.

static uint32_t scm_voice(const ScummPlayer* player, uint32_t channel) {
    return player->module.engine->first_voice + channel;
}

static uint32_t scm_channel_count(const ScummPlayer* player) {
    return player->module.engine->channels;
}

// The channel's instrument stops: the two-channel driver switches the DMA off, the four-channel one only frees the
// channel (its note start restarts the DMA). The channel number is masked as the original does.

static void scm_stop_channel(ScummPlayer* player, uint16_t channel) {
    const uint32_t c = channel % scm_channel_count(player);
    if (player->playing[c] >= SCM_MAX_INSTRUMENTS) {
        return;
    }
    if (player->module.engine->two_channel) {
        // the bits of the channel and of the channel two above it
        nuade_paula_channel_dma_off(&player->paula[c]);
        nuade_paula_channel_dma_off(&player->paula[scm_voice(player, c)]);
    }
    player->playing[c] = SCM_CHANNEL_FREE;
}

// A note starts: the instrument's record takes the event's duration and pitch offset, the channel gets the sample
// and the first period and volume of the envelopes, and its DMA starts.

static void scm_start_note(ScummPlayer* player, const ScmEvent* event) {
    if (event->channel >= player->module.engine->channels || event->instrument >= player->module.engine->instruments) {
        return; // refused at load time
    }
    ScmNote* note = &player->notes[event->instrument];
    *note = (ScmNote) { .duration = event->duration,
                        .pitch_offset = event->pitch_offset,
                        .channel = (uint8_t)event->channel };
    player->playing[event->channel] = (uint8_t)event->instrument;
    const ScmInstrument instrument = scm_module_instrument(&player->module, event->instrument);
    const uint32_t voice = scm_voice(player, note->channel);
    NuadePaulaChannel* paula = &player->paula[voice];
    player->last_instrument[voice] = (int)event->instrument;
    if (player->module.version->stops_dma_on_start) {
        nuade_paula_channel_dma_off(paula);
    }
    const uint32_t start = scm_module_sample_offset(&player->module, instrument.start_offset);
    uint32_t zero_at = 0;
    if (scm_module_zero_write(&player->module, start, &zero_at)) {
        scm_module_clear_long(&player->module, zero_at);
    }
    scm_set_sample(paula, scm_module_sample(&player->module, start), (uint16_t)(instrument.start_bytes >> 1));
    paula->period = (uint16_t)(scm_module_envelope(&player->module, instrument.pitch_envelope, 0) + note->pitch_offset);
    paula->volume = (uint8_t)scm_module_envelope(&player->module, instrument.volume_envelope, 0);
    nuade_paula_channel_dma_on(paula);
}

// An envelope position moves on and stops at 0xFE.

static uint16_t scm_next_position(uint16_t position) {
    return (int16_t)(position + 1u) < (int16_t)SCM_ENVELOPE_POSITIONS ? (uint16_t)(position + 1u)
                                                                      : (uint16_t)(SCM_ENVELOPE_POSITIONS - 1u);
}

// The two-channel driver writes 0xFF to byte 0x10 + instrument of its work area when a faded-out note's volume is not
// above 0: instead of the channel's byte it clears the byte of the instrument number, so the channel keeps updating
// the note when the instrument number is the other channel's (0 or 1 only; the load-time check keeps the others in
// the eagleplayer's Check2 code).

static void scm_release_ends(ScummPlayer* player, uint32_t instrument) {
    if (instrument < player->module.engine->channels) {
        player->playing[instrument] = SCM_CHANNEL_FREE;
    }
}

// One tick of a playing note: the loop registers, then period and volume from the envelopes.

static void scm_update_note(ScummPlayer* player, uint32_t index) {
    ScmNote* note = &player->notes[index];
    const bool two_channel = player->module.engine->two_channel;
    if (!two_channel && note->volume_position == 0) {
        // the tick after the note start only counts
        note->volume_position = 1;
        return;
    }
    const ScmInstrument instrument = scm_module_instrument(&player->module, index);
    const uint32_t voice = scm_voice(player, note->channel);
    NuadePaulaChannel* paula = &player->paula[voice];
    const uint32_t loop = scm_module_sample_offset(&player->module, instrument.loop_offset);
    const uint16_t loop_words = (uint16_t)(instrument.loop_bytes >> 1);
    scm_set_sample(paula, scm_module_sample(&player->module, loop), loop_words);
    if (!two_channel && loop_words == 1) {
        scm_module_clear_long(&player->module, loop);
    }
    const uint16_t volume = scm_module_envelope(&player->module, instrument.volume_envelope, note->volume_position);
    paula->period = (uint16_t)(scm_module_envelope(&player->module, instrument.pitch_envelope, note->pitch_position)
                               + note->pitch_offset);
    paula->volume = (uint8_t)volume;
    note->volume_position = scm_next_position(note->volume_position);
    note->pitch_position = scm_next_position(note->pitch_position);
    if (note->releasing) {
        if ((int16_t)volume <= 0) {
            scm_release_ends(player, index);
        }
        return;
    }
    // subq.w #1 / bgt: the test sees the result without the 16 bit wrap (a duration of 0x8000 wraps to 0x7FFF with V
    // set)
    const int32_t left = note->duration - 1;
    note->duration = (int16_t)left;
    if (left > 0) {
        return;
    }
    if (two_channel) {
        note->releasing = true;
        note->volume_position = 0;
        note->pitch_position = 0;
    } else {
        nuade_paula_channel_dma_off(paula);
        player->playing[note->channel] = SCM_CHANNEL_FREE;
    }
}

// The tracker drivers' init routine: the tracks start at 0, no channel plays; the four-channel driver silences the
// volume registers and switches all DMA on, the two-channel one switches it all off.

static void scm_tracker_init(ScummPlayer* player) {
    memset(player->tracks, 0, sizeof(player->tracks));
    memset(player->playing, SCM_CHANNEL_FREE, sizeof(player->playing));
    for (uint32_t v = 0; v < SCUMM_VOICES; v++) {
        if (player->module.engine->two_channel) {
            nuade_paula_channel_dma_off(&player->paula[v]);
        } else {
            player->paula[v].volume = 0;
            nuade_paula_channel_dma_on(&player->paula[v]);
        }
    }
}

// The events of one track that are due. Returns false when the driver ends the tick.

static bool scm_run_track(ScummPlayer* player, uint32_t t) {
    ScmTrack* track = &player->tracks[t];
    const ScmTrackEnd end = t == 0 ? player->module.version->first_track_end : ScmTrackEnd_Restart;
    // A list that restarts without waiting is refused at load time: the guard only keeps a broken module from hanging.
    for (uint32_t guard = 0; guard < (SCM_MAX_TRACK_EVENTS + 1u) * 2u; guard++) {
        const ScmEvent event = scm_module_event(&player->module, t, track->position);
        if ((int16_t)track->timer < event.time) {
            return true;
        }
        if (event.pitch_offset == SCM_EVENT_END) {
            if (end != ScmTrackEnd_Freeze) {
                *track = (ScmTrack) { 0 };
            }
            if (end != ScmTrackEnd_Restart) {
                return false;
            }
            continue;
        }
        scm_stop_channel(player, event.channel);
        scm_start_note(player, &event);
        track->position++;
    }
    return true;
}

static void scm_tracker_tick(ScummPlayer* player) {
    if (player->first_tick) {
        scm_tracker_init(player);
    }
    for (uint32_t t = 0; t < SCM_TRACKS; t++) {
        if (!scm_run_track(player, t)) {
            return;
        }
    }
    for (uint32_t c = 0; c < scm_channel_count(player); c++) {
        if (player->playing[c] < SCM_MAX_INSTRUMENTS) {
            scm_update_note(player, player->playing[c]);
        }
    }
    for (uint32_t t = 0; t < SCM_TRACKS; t++) {
        player->tracks[t].timer++;
    }
}

void scumm_tick(ScummPlayer* player, NuadeVoices* voices) {
    switch (player->module.version->driver) {
        case ScmDriver_Alarm:
            scm_alarm_tick(player);
            break;
        case ScmDriver_TeleportDown:
        case ScmDriver_TeleportUp:
            scm_teleport_tick(player);
            break;
        case ScmDriver_Duo:
        case ScmDriver_Quad:
            scm_tracker_tick(player);
            break;
    }
    player->first_tick = false;
    for (uint32_t v = 0; v < SCUMM_VOICES; v++) {
        nuade_paula_channel_sync(&player->paula[v], v, voices);
    }
}

bool scumm_start(ScummPlayer* player) {
    if (player == nullptr) {
        return false;
    }
    memcpy(player->module.image, player->pristine, player->module.size);
    player->first_tick = true;
    memset(player->alarm, 0, sizeof(player->alarm));
    player->teleport = (ScmTeleport) { 0 };
    memset(player->notes, 0, sizeof(player->notes));
    memset(player->paula, 0, sizeof(player->paula));
    for (uint32_t v = 0; v < SCUMM_VOICES; v++) {
        // The eagleplayer's audio setup leaves every channel on a silent word: a DMA start before the driver's first
        // register writes plays it and takes the driver's registers as its repeat. The padding behind the module is
        // zero; an odd module size is rounded up so that the word does not hold the module's last byte.
        player->paula[v] = (NuadePaulaChannel) { .lc = scm_module_sample(&player->module, player->module.size + 1),
                                                 .len_words = 1,
                                                 .period = SCM_IDLE_PERIOD };
        player->last_instrument[v] = -1;
    }
    return true;
}

ScummPlayer* scumm_create(const uint8_t* data, size_t size) {
    ScummPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    if (!scm_module_load(&player->module, data, size)) {
        free(player);
        return nullptr;
    }
    player->pristine = malloc(player->module.size);
    if (player->pristine == nullptr) {
        scumm_destroy(player);
        return nullptr;
    }
    memcpy(player->pristine, player->module.image, player->module.size);
    return player;
}

void scumm_destroy(ScummPlayer* player) {
    if (player != nullptr) {
        scm_module_free(&player->module);
        free(player->pristine);
        free(player);
    }
}

bool scumm_probe(const uint8_t* data, size_t size) {
    ScummPlayer* player = scumm_create(data, size);
    const bool known = player != nullptr;
    scumm_destroy(player);
    return known;
}

ScummVoiceState scumm_voice_state(const ScummPlayer* player, uint32_t voice) {
    const NuadePaulaChannel* paula = &player->paula[voice];
    return (ScummVoiceState) { .period = paula->dma ? paula->period : 0, .instrument = player->last_instrument[voice] };
}
