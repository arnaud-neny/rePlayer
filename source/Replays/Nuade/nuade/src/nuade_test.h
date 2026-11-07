
// nuade test hooks: observe what the framework does with a tick player's voices. Not needed for playback.

#pragma once

#include <nuade/nuade.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum NuadeVoiceEventKind {
    NuadeVoiceEvent_Play,   // nuade_voice_play: `sample`, `value` = start frame
    NuadeVoiceEvent_Queue,  // nuade_voice_queue: `sample`
    NuadeVoiceEvent_Stop,   // nuade_voice_stop
    NuadeVoiceEvent_Pitch,  // `value` = hz
    NuadeVoiceEvent_Volume, // `value` = linear volume
    NuadeVoiceEvent_Pan,    // `value` = pan
    NuadeVoiceEvent_Loop,   // the mixer wrapped a looping sample back to its loop start
    NuadeVoiceEvent_Switch, // the mixer moved on to the queued sample (`sample`)
    NuadeVoiceEvent_End,    // a one-shot ran off its end with nothing queued: the voice is idle
} NuadeVoiceEventKind;

typedef struct NuadeVoiceEvent {
    uint64_t frame; // output frame since start: where the call's tick starts, or where the mixer reached the event
    uint32_t voice;
    NuadeVoiceEventKind kind;
    double value;
    const NuadeSample* sample; // only valid during the callback; 0 when the kind has none
} NuadeVoiceEvent;

typedef void (*NuadeVoiceObserver)(const NuadeVoiceEvent* event, void* user);

// Report every voice call of a tick player and every mixer event. Pass 0 to stop. Self-rendering players have no voices
// and report nothing.
void nuade_set_voice_observer(NuadeSong* song, NuadeVoiceObserver observer, void* user);

// Write a tick player's Paula event trace to `path` (0 or "" stops it); `with_bytes` also writes the first 512 bytes of
// every buffer. False for a self-rendering player, or when the file cannot be opened (the old trace is then kept).
bool nuade_set_paula_trace(NuadeSong* song, const char* path, bool with_bytes);

// Write the player's per-tick state to `path` ("-" = stderr, "" = off); see NuadePlayer.dump_state. False if the player
// has none or the file cannot be opened.
bool nuade_set_dump_state(NuadeSong* song, const char* path);

#ifdef __cplusplus
}
#endif
