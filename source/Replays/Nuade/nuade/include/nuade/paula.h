
// One Paula audio channel as a tick player drives it: the registers it writes during a tick, which the framework hands
// to a voice of the mixer at the end of the tick. Players keep these in their state; the calls that drive them are
// internal to the framework.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// lc must have len_words * 2 bytes readable behind it (NUADE_PAULA_MAX_SAMPLE_BYTES for len_words 0); a buffer from
// nuade_paula_padded_copy guarantees that for any lc inside its first `size` bytes.
typedef struct NuadePaulaChannel {
    const int8_t* lc;       // AUDxLC, latched when DMA starts and at every repeat; null when it points nowhere
    uint16_t len_words;     // AUDxLEN; 0 plays 65536 words. Every sample repeats whole.
    uint16_t period;        // AUDxPER, 0 until written; 0 plays as 65535 (nuade_paula_period_hz)
    uint8_t volume;         // AUDxVOL as written (low byte); plays at nuade_paula_volume(volume), 0..64
    bool dma;               // the channel's DMACON bit
    bool dma_started;       // DMA went from off to on since the last hand-off to the voice
    const int8_t* start_lc; // AUDxLC/AUDxLEN as that DMA start latched them
    uint16_t start_len_words;
    bool voice_on;         // the voice plays
    const int8_t* sent_lc; // the sample the voice holds (playing or queued)
    uint16_t sent_len_words;
} NuadePaulaChannel;

#ifdef __cplusplus
}
#endif
