
// Paula constants shared by the framework and the players.

#pragma once

// PAL Paula colour clock in Hz (what UADE runs); a period register counts ticks of this clock.
#define NUADE_PAULA_CLOCK_PAL 3546895.0

// Paula fetches at most one sample word per scanline (227 clocks), so a period below 113.5 cannot step the sample
// faster than the fetch: when the period runs out before the next fetch, the hardware plays the previous word again
// (some words twice; below 56.75 each word's two bytes alternate for the whole line). The pitch therefore caps at the
// fetch rate. The mixer plays at that fetch-limited rate without the repeated words.
#define NUADE_PAULA_WORD_FETCH_CLOCKS 227

// The volume 0..64 that an AUDxVOL value plays at. Paula reads bits 0-6 only: bit 6 set is full volume (64 whatever
// bits 0-5 hold), otherwise bits 0-5 are the volume.

static inline unsigned nuade_paula_volume(unsigned audxvol) {
    return (audxvol & 64) ? 64 : (audxvol & 63);
}

// The playback rate in Hz that a Paula period register gives. Period 0 plays as 65535.

static inline double nuade_paula_period_hz(double period) {
    if (period == 0.0) {
        period = 65535.0;
    } else if (period < NUADE_PAULA_WORD_FETCH_CLOCKS / 2.0) {
        period = NUADE_PAULA_WORD_FETCH_CLOCKS / 2.0;
    }
    return NUADE_PAULA_CLOCK_PAL / period;
}
