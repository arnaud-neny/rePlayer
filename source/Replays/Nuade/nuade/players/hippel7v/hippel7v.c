// SPDX-License-Identifier: MIT
// Jochen Hippel 7 voice player, translated from the replayer in UADE's "Jochen Hippel 7V player module V1.3" (the
// Wanted Team's adaptation of Hippel's 1990-91 code). Seven tracks: tracks 0..2 play through Paula channels 3, 1 and 2,
// tracks 3..6 are mixed in software into channel 0, whose audio interrupt (the end of a mix buffer) is the tick.
//
// One tick: the sequencers of the mixed tracks (when the speed counter runs out), every voice's sequences and effects,
// the mixer's voices are switched on and given their samples, the Paula channels are restarted and get their repeat,
// the sequencers of the Paula tracks follow (in the same tick the speed counter ran out), and the next buffer is mixed.

#include <nuade/players/hippel7v.h>

#include "h7v_internal.h"
#include "nuade_base.h"
#include <stdlib.h>

// Paula channel of each Paula track (the replayer's records point at AUD3, AUD1 and AUD2)
static const uint32_t k_paula_channel[H7V_PAULA_TRACKS] = { 3, 1, 2 };

// The mixer plays on channel 0
#define MIX_CHANNEL 0
#define START_MARGIN_FRAMES 3

H7vPlayer* h7v_create(const uint8_t* data, size_t size) {
    H7vPlayer* p = calloc(1, sizeof(H7vPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    if (!h7v_module_open(&p->module, data, size)) {
        free(p);
        return nullptr;
    }
    return p;
}

void h7v_destroy(H7vPlayer* player) {
    if (player != nullptr) {
        h7v_module_close(&player->module);
        free(player);
    }
}

int h7v_subsong_count(const H7vPlayer* player) {
    return player->module.subsong_count;
}

// The replayer's init with subsong n (1-based). The first track position (which no subsong plays) holds the mixer's
// settings: the sign of the tempo offset is its second byte, the clip mode the effect byte.

bool h7v_start(H7vPlayer* player, int subsong, uint32_t sample_rate, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || sample_rate == 0 || subsong < 0
        || subsong >= player->module.subsong_count) {
        return false;
    }
    const H7vModule* m = &player->module;
    H7vSong* s = &player->song;
    *s = (H7vSong) { 0 };
    const H7vSubsong entry = h7v_subsong(m, (uint32_t)subsong + 1);
    h7v_voices_init(s, m, &entry);
    const H7vMixerSettings settings = h7v_mixer_settings(m);
    h7v_mixer_init(&player->mixer, settings.hard_clip, settings.tempo_offset);
    for (uint32_t i = 0; i < H7V_VOICES; i++) {
        nuade_voice_stop(voices, i);
    }
    // The audio DMA of channel 0 starts: the first interrupt follows at once. The channel plays two words (4 bytes)
    // first; the second interrupt has to follow after the voice moved on to the first buffer (the mixer settles the end
    // of a sample a frame after it was reached), or the buffer queued before is replaced, so the first tick lasts
    // START_MARGIN_FRAMES longer. Every later tick follows the first by the exact length of the buffers.
    nuade_paula_channel_dma_on(&player->mixer.output);
    player->start_margin = (uint32_t)((uint64_t)START_MARGIN_FRAMES * H7V_TIMEBASE_HZ / sample_rate + 1);
    return true;
}

static void apply_sequencer_output(H7vMixer* mixer, H7vSequencerOutput output) {
    if (output.tempo_changed) {
        h7v_mixer_set_tempo(mixer, output.tempo_offset);
    }
}

// The sequencers of the tracks from `first` to `end` (mixed tracks 3..6, or Paula tracks 0..2)

static void run_sequencers(H7vPlayer* p, int first, int end) {
    for (int i = first; i < end; i++) {
        apply_sequencer_output(&p->mixer, h7v_sequencer_step(&p->module, &p->song, &p->song.voices[i]));
    }
}

// The tick after the song stopped itself: all audio DMA off.

static void stop_audio(H7vPlayer* p, NuadeVoices* voices) {
    H7vSong* s = &p->song;
    for (int i = 0; i < H7V_PAULA_TRACKS; i++) {
        NuadePaulaChannel* paula = &s->voices[i].paula;
        nuade_paula_channel_dma_off(paula);
        paula->volume = 0;
        nuade_paula_channel_sync(paula, k_paula_channel[i], voices);
    }
    nuade_paula_channel_dma_off(&p->mixer.output);
    nuade_paula_channel_sync(&p->mixer.output, MIX_CHANNEL, voices);
}

// Every voice's work for the tick: Paula channels first, then the mixed voices.

static void run_voices(H7vPlayer* p) {
    H7vSong* s = &p->song;
    for (int i = 0; i < H7V_PAULA_TRACKS; i++) {
        s->voices[i].restart_dma = false;
    }
    for (int i = 0; i < H7V_TRACKS; i++) {
        h7v_voice_tick(&p->module, s, &s->voices[i]);
    }
}

// A mixed voice that started a sample once is on in every tick from here on.

static void switch_on_started_voices(H7vSong* s) {
    for (int i = H7V_PAULA_TRACKS; i < H7V_TRACKS; i++) {
        s->voices[i].mixer_on = s->voices[i].mixer_on || s->voices[i].mixer_started;
    }
}

H7vTickResult h7v_tick(H7vPlayer* player, NuadeVoices* voices) {
    H7vSong* s = &player->song;
    H7vMixer* mixer = &player->mixer;
    NuadePaulaChannel* output = &mixer->output;
    s->looped = false;

    // The buffer that starts playing now lasts until the next interrupt; the interrupt sets the one after it
    const uint32_t playing_words = output->len_words;
    const uint32_t margin = player->start_margin;
    player->start_margin = 0;
    output->lc = (const int8_t*)mixer->buffer[mixer->next];
    output->len_words = mixer->words;

    if (s->stopped) {
        stop_audio(player, voices);
        return (H7vTickResult) { .length = playing_words * 2 * output->period, .ended = true };
    }

    s->speed_counter = (uint16_t)(s->speed_counter - 1);
    if (s->speed_counter == 0) {
        s->speed_counter = s->speed;
        run_sequencers(player, H7V_PAULA_TRACKS, H7V_TRACKS);
    }
    run_voices(player);
    h7v_mixer_voices_update(mixer, s);
    switch_on_started_voices(s);
    h7v_mixer_voices_update(mixer, s);
    for (int i = H7V_PAULA_TRACKS; i < H7V_TRACKS; i++) {
        h7v_write_repeat(&s->voices[i]);
    }
    for (int i = 0; i < H7V_PAULA_TRACKS; i++) {
        if (s->voices[i].restart_dma) {
            nuade_paula_channel_dma_on(&s->voices[i].paula);
        }
    }
    h7v_mixer_voices_update(mixer, s);
    for (int i = 0; i < H7V_PAULA_TRACKS; i++) {
        h7v_write_repeat(&s->voices[i]);
    }
    if (s->speed_counter == s->speed) {
        run_sequencers(player, 0, H7V_PAULA_TRACKS);
    }

    h7v_mixer_mix(mixer, s);
    mixer->next ^= 1;
    output->volume = H7V_MIX_VOLUME;

    for (int i = 0; i < H7V_PAULA_TRACKS; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, k_paula_channel[i], voices);
    }
    nuade_paula_channel_sync(output, MIX_CHANNEL, voices);
    return (H7vTickResult) { .length = playing_words * 2 * output->period + margin, .looped = s->looped };
}

int h7v_speed(const H7vPlayer* player) {
    return player->song.speed;
}

H7vTrackState h7v_track_state(const H7vPlayer* player, int track) {
    const H7vVoice* v = &player->song.voices[track];
    return (H7vTrackState) { .note = v->last_note, .instrument = v->last_instrument, .note_starts = v->note_starts };
}
