
// nuade voice interface: what a tick player tells the framework. Plain units, nothing here is Amiga hardware.
//
// A player calls these from `start` and `tick`. The framework mixes the voices between ticks, so a call takes effect at
// the output frame where its tick starts. Voice state (pitch, volume, pan) stays until it is set again, like Paula's
// registers.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NuadeVoices NuadeVoices;

typedef enum NuadeSampleFormat {
    NuadeSampleFormat_S8 = 0, // signed 8-bit, full scale 128
    NuadeSampleFormat_S16,    // signed 16-bit, full scale 32768
} NuadeSampleFormat;

typedef struct NuadeSample {
    const void* data; // player memory; must stay valid while a voice uses it. The mixer reads it live, so a synth
                      // player may rewrite the buffer in place between ticks.
    NuadeSampleFormat format;
    uint32_t length;      // frames
    uint32_t loop_start;  // frames
    uint32_t loop_length; // frames; 0 = one-shot. A loop that does not fit in `length` is clipped to it.
} NuadeSample;

// What a voice is doing right now, as of the start of the current tick (the mixer settles a wrap or an end only when it
// mixes the next frame, so a position can sit at or past the end of the sample until then).
typedef struct NuadeVoiceState {
    bool playing;
    const NuadeSample* sample; // current sample; 0 when idle. Valid until the next voice call or mix.
    double position;           // source frames into `sample`
    double pitch_hz;           // as last set
    float volume;
    float pan;
    uint32_t wraps;       // loop wraps and switches to a queued sample since the voice was created or reset
    uint64_t event_frame; // output frame of the first wrap, switch or end since the last call on this voice
    uint64_t frame;       // output frames mixed since start: the frame this tick starts at
} NuadeVoiceState;

// Start a sample from `start_frame`, replacing the current sample and anything queued.
void nuade_voice_play(NuadeVoices* voices, uint32_t voice, const NuadeSample* sample, uint32_t start_frame);
// Take over when the current sample ends (at its loop end if it loops), then keep playing. On an idle voice this is
// the same as nuade_voice_play from frame 0. A second call replaces the queued sample (Paula's latched repeat).
void nuade_voice_queue(NuadeVoices* voices, uint32_t voice, const NuadeSample* sample);
void nuade_voice_stop(NuadeVoices* voices, uint32_t voice);

void nuade_voice_set_pitch(NuadeVoices* voices, uint32_t voice,
                           double hz); // source frames per second; 0 = hold position
void nuade_voice_set_volume(NuadeVoices* voices, uint32_t voice, float volume); // linear, 0..1 (default 1)
void nuade_voice_set_pan(NuadeVoices* voices, uint32_t voice, float pan);       // -1 left .. 1 right (default 0)

// False after nuade_voice_stop and once a one-shot ran off its end with nothing queued.
bool nuade_voice_is_playing(const NuadeVoices* voices, uint32_t voice);

void nuade_voice_get_state(const NuadeVoices* voices, uint32_t voice, NuadeVoiceState* state);

#ifdef __cplusplus
}
#endif
