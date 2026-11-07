// Raw voice mixer: nearest sample, no filter, no ramp.

#include "nuade_mixer.h"

#include <stdlib.h>

// Every voice is scaled by this so that four full-scale voices stay in range; a voice volume of 1 is the sample at
// full scale, whatever the player. A power of two, so it changes no rounding.
#define NUADE_HEADROOM 0.5f

typedef struct NuadeVoice {
    NuadeSample current;
    NuadeSample queued;
    bool playing;
    bool has_queued;
    f64 position; // source frames
    f64 step;     // source frames per output frame
    f64 pitch_hz;
    u32 wraps;
    u64 event_frame; // output frame of the first wrap, switch or end since the last call on this voice
    bool has_event;
    f32 volume;
    f32 pan;
} NuadeVoice;

struct NuadeVoices {
    u32 voice_count;
    u32 sample_rate;
    u64 frame;      // output frames mixed since reset
    f32 separation; // scales every pan: 1 = as the player set it, 0 = mono
    NuadeVoiceObserver observer;
    void* observer_user;
    NuadeVoice* voices;
};

static void emit(const NuadeVoices* mixer, u32 voice, NuadeVoiceEventKind kind, f64 value, const NuadeSample* sample) {
    if (mixer->observer != nullptr) {
        const NuadeVoiceEvent event
            = { .frame = mixer->frame, .voice = voice, .kind = kind, .value = value, .sample = sample };
        mixer->observer(&event, mixer->observer_user);
    }
}

static void reset_voice(NuadeVoice* voice) {
    *voice = (NuadeVoice) { .volume = 1.0f };
}

NuadeVoices* nuade_mixer_create(u32 voice_count) {
    NuadeVoices* mixer = (NuadeVoices*)calloc(1, sizeof(NuadeVoices));
    if (mixer == nullptr) {
        return nullptr;
    }
    mixer->voices = (NuadeVoice*)calloc(voice_count ? voice_count : 1, sizeof(NuadeVoice));
    if (mixer->voices == nullptr) {
        free(mixer);
        return nullptr;
    }
    mixer->voice_count = voice_count;
    mixer->separation = 1.0f;
    for (u32 i = 0; i < voice_count; i++) {
        reset_voice(&mixer->voices[i]);
    }
    return mixer;
}

void nuade_mixer_destroy(NuadeVoices* mixer) {
    if (mixer != nullptr) {
        free(mixer->voices);
        free(mixer);
    }
}

void nuade_mixer_reset(NuadeVoices* mixer, u32 sample_rate) {
    mixer->sample_rate = sample_rate;
    mixer->frame = 0;
    for (u32 i = 0; i < mixer->voice_count; i++) {
        reset_voice(&mixer->voices[i]);
    }
}

void nuade_mixer_set_observer(NuadeVoices* mixer, NuadeVoiceObserver observer, void* user) {
    mixer->observer = observer;
    mixer->observer_user = user;
}

void nuade_mixer_set_separation(NuadeVoices* mixer, f32 separation) {
    mixer->separation = separation;
}

// Voice calls

// A loop that does not fit in the sample is clipped to it; one that starts past the end is no loop.
static NuadeSample normalized(const NuadeSample* sample) {
    NuadeSample s = *sample;
    if (s.loop_length != 0) {
        if (s.loop_start >= s.length) {
            s.loop_length = 0;
        } else if (s.loop_length > s.length - s.loop_start) {
            s.loop_length = s.length - s.loop_start;
        }
    }
    return s;
}

// The voice a call is for. Any call starts a new observation period for nuade_voice_get_state's event_frame.
static NuadeVoice* voice_at(NuadeVoices* mixer, u32 voice) {
    if (mixer == nullptr || voice >= mixer->voice_count) {
        return nullptr;
    }
    mixer->voices[voice].has_event = false;
    return &mixer->voices[voice];
}

static void start_sample(NuadeVoice* v, const NuadeSample* sample, u32 start_frame) {
    v->current = normalized(sample);
    v->has_queued = false;
    v->playing = v->current.data != nullptr;
    v->position = (f64)start_frame;
}

void nuade_voice_play(NuadeVoices* mixer, uint32_t voice, const NuadeSample* sample, uint32_t start_frame) {
    NuadeVoice* v = voice_at(mixer, voice);
    if (v == nullptr || sample == nullptr) {
        return;
    }
    start_sample(v, sample, start_frame);
    emit(mixer, voice, NuadeVoiceEvent_Play, (f64)start_frame, sample);
}

void nuade_voice_queue(NuadeVoices* mixer, uint32_t voice, const NuadeSample* sample) {
    NuadeVoice* v = voice_at(mixer, voice);
    if (v == nullptr || sample == nullptr) {
        return;
    }
    if (v->playing) {
        v->queued = normalized(sample);
        v->has_queued = true;
    } else {
        start_sample(v, sample, 0);
    }
    emit(mixer, voice, NuadeVoiceEvent_Queue, 0.0, sample);
}

void nuade_voice_stop(NuadeVoices* mixer, uint32_t voice) {
    NuadeVoice* v = voice_at(mixer, voice);
    if (v == nullptr) {
        return;
    }
    v->playing = false;
    v->has_queued = false;
    emit(mixer, voice, NuadeVoiceEvent_Stop, 0.0, nullptr);
}

void nuade_voice_set_pitch(NuadeVoices* mixer, uint32_t voice, double hz) {
    NuadeVoice* v = voice_at(mixer, voice);
    if (v == nullptr) {
        return;
    }
    v->pitch_hz = hz > 0.0 ? hz : 0.0;
    v->step = mixer->sample_rate ? v->pitch_hz / (f64)mixer->sample_rate : 0.0;
    emit(mixer, voice, NuadeVoiceEvent_Pitch, hz, nullptr);
}

void nuade_voice_set_volume(NuadeVoices* mixer, uint32_t voice, float volume) {
    NuadeVoice* v = voice_at(mixer, voice);
    if (v == nullptr) {
        return;
    }
    v->volume = volume;
    emit(mixer, voice, NuadeVoiceEvent_Volume, volume, nullptr);
}

void nuade_voice_set_pan(NuadeVoices* mixer, uint32_t voice, float pan) {
    NuadeVoice* v = voice_at(mixer, voice);
    if (v == nullptr) {
        return;
    }
    v->pan = pan;
    emit(mixer, voice, NuadeVoiceEvent_Pan, pan, nullptr);
}

bool nuade_voice_is_playing(const NuadeVoices* mixer, uint32_t voice) {
    if (mixer == nullptr || voice >= mixer->voice_count) {
        return false;
    }
    const NuadeVoice* v = &mixer->voices[voice];
    // The mixer settles an end only when it mixes the next frame, so a one-shot that ran off its end on the last frame
    // of a tick still has `playing` set. Report it as finished now: a player reading this back would otherwise treat
    // the voice as live for one more tick and, for example, stop it and drop a sample queued behind it. A queued
    // sample keeps the voice playing (the switch happens on the next mixed frame).
    const bool ran_off_end = !v->has_queued && v->current.loop_length == 0 && v->position >= (f64)v->current.length;
    return v->playing && !ran_off_end;
}

void nuade_voice_get_state(const NuadeVoices* mixer, uint32_t voice, NuadeVoiceState* state) {
    *state = (NuadeVoiceState) { 0 };
    if (mixer == nullptr || voice >= mixer->voice_count) {
        return;
    }
    const NuadeVoice* v = &mixer->voices[voice];
    state->playing = v->playing;
    state->sample = v->playing ? &v->current : nullptr;
    state->position = v->playing ? v->position : 0.0;
    state->pitch_hz = v->pitch_hz;
    state->volume = v->volume;
    state->pan = v->pan;
    state->wraps = v->wraps;
    state->event_frame = v->has_event ? v->event_frame : 0;
    state->frame = mixer->frame;
}

// Mixing

static u32 sample_end(const NuadeSample* s) {
    return s->loop_length != 0 ? s->loop_start + s->loop_length : s->length;
}

static void note_event(const NuadeVoices* mixer, NuadeVoice* v) {
    if (!v->has_event) {
        v->has_event = true;
        v->event_frame = mixer->frame;
    }
}

// Move the position back inside the sample: wrap a loop, move on to the queued sample, or end the voice. Returns false
// when the voice is idle afterwards.
static bool settle(NuadeVoices* mixer, u32 index, NuadeVoice* v) {
    while (v->position >= (f64)sample_end(&v->current)) {
        const u32 end = sample_end(&v->current);
        if (v->has_queued) {
            v->current = v->queued;
            v->has_queued = false;
            v->position -= (f64)end;
            v->wraps++;
            note_event(mixer, v);
            emit(mixer, index, NuadeVoiceEvent_Switch, 0.0, &v->current);
        } else if (v->current.loop_length != 0) {
            while (v->position >= (f64)end) {
                v->position -= (f64)v->current.loop_length;
            }
            v->wraps++;
            note_event(mixer, v);
            emit(mixer, index, NuadeVoiceEvent_Loop, 0.0, nullptr);
        } else {
            v->playing = false;
            note_event(mixer, v);
            emit(mixer, index, NuadeVoiceEvent_End, 0.0, nullptr);
            return false;
        }
    }
    return true;
}

static f32 read_sample(const NuadeSample* s, u32 position) {
    if (s->format == NuadeSampleFormat_S16) {
        return (f32)((const i16*)s->data)[position] / 32768.0f;
    }
    return (f32)((const i8*)s->data)[position] / 128.0f;
}

void nuade_mixer_mix(NuadeVoices* mixer, float* stereo, float* const* voice_out, u32 offset, u32 frames) {
    for (u32 i = 0; i < frames; i++) {
        f32 left = 0.0f;
        f32 right = 0.0f;

        for (u32 n = 0; n < mixer->voice_count; n++) {
            NuadeVoice* v = &mixer->voices[n];
            f32 contribution = 0.0f;

            if (v->playing && settle(mixer, n, v)) {
                const f32 pan = (v->pan < -1.0f ? -1.0f : (v->pan > 1.0f ? 1.0f : v->pan)) * mixer->separation;

                const f32 s = read_sample(&v->current, (u32)v->position) * v->volume;
                const f32 l = s * (NUADE_HEADROOM * 0.5f * (1.0f - pan));
                const f32 r = s * (NUADE_HEADROOM * 0.5f * (1.0f + pan));
                left += l;
                right += r;
                contribution = l + r;
                v->position += v->step;
            }
            if (voice_out != nullptr) {
                voice_out[n][offset + i] = contribution;
            }
        }

        stereo[(offset + i) * 2] += left;
        stereo[(offset + i) * 2 + 1] += right;
        mixer->frame++;
    }
}

void nuade_mixer_advance(NuadeVoices* mixer, u32 frames) {
    for (u32 i = 0; i < frames; i++) {
        for (u32 n = 0; n < mixer->voice_count; n++) {
            NuadeVoice* v = &mixer->voices[n];
            if (v->playing && settle(mixer, n, v)) {
                v->position += v->step;
            }
        }
        mixer->frame++;
    }
}
// rePlayer begin
void nuade_mixer_pan(NuadeVoices* mixer, float* old_pan, float* new_pan) {
    for (u32 n = 0; n < mixer->voice_count; n++) {
        NuadeVoice* v = &mixer->voices[n];
        if (old_pan)
            old_pan[n] = v->pan;
        if (new_pan)
            nuade_voice_set_pan(mixer, n, new_pan[n]);
    }
}
// rePlayer end