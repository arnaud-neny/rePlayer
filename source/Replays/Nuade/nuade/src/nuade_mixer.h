// Raw voice mixer: nearest sample, no filter, no ramp. Implements the nuade_voice_* calls of include/nuade/voice.h.

#pragma once

#include "nuade_base.h"
#include "nuade_test.h"

NuadeVoices* nuade_mixer_create(u32 voice_count);
void nuade_mixer_destroy(NuadeVoices* mixer);

// Silence every voice and restart the frame counter at the given output rate. The observer is kept.
void nuade_mixer_reset(NuadeVoices* mixer, u32 sample_rate);

void nuade_mixer_set_observer(NuadeVoices* mixer, NuadeVoiceObserver observer, void* user);

// Scale every voice's pan by `separation` (0..1) when mixing; 1 (the default) mixes the pans as set. Kept across
// nuade_mixer_reset. Events and the voice state report the pan the player set.
void nuade_mixer_set_separation(NuadeVoices* mixer, f32 separation);

// Add `frames` frames of output to `stereo` (interleaved) starting at frame `offset`. `voice_out` is 0 or one buffer
// per voice; voice_out[v][offset + i] is set to voice v's left + right contribution (not added).
void nuade_mixer_mix(NuadeVoices* mixer, float* stereo, float* const* voice_out, u32 offset, u32 frames);

// Move every voice forward by `frames` frames exactly as nuade_mixer_mix does (loop wraps, queued switches, one-shot
// ends, observer events), without reading or writing any sample. A player that reads the voice state back sees the same
// state as after a real render.
void nuade_mixer_advance(NuadeVoices* mixer, u32 frames);
void nuade_mixer_pan(NuadeVoices* mixer, float* old_pan, float* new_pan); // rePlayer
