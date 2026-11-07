// SPDX-License-Identifier: MIT
// What the Jochen Hippel replayers (COSO, 7 voice) have in common: the period table, the vibrato, the portamento and
// the voice volume in percent. Header only, so a player can use it without the other being compiled in.

#pragma once

#include <nuade/players/hippel.h>
#include <stdbool.h>
#include <stdint.h>

// Entries in the period table. A note index past it reads whatever memory follows the table in the replayer.
#define HIP_PERIOD_COUNT 72

// The voice volume in percent of the full volume
#define HIP_VOLUME_FULL 100

// The period of a note (index & 0x7F), 0 for an index past the table.

static inline uint16_t hip_note_period(uint8_t index) {
    static const uint16_t periods[HIP_PERIOD_COUNT] = {
        1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960,  906,  856,  808,  762,  720,  678,  640,
        604,  570,  538,  508,  480,  453,  428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,
        214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,  113,  113,  113,  113,  113,  113,
        113,  113,  113,  113,  113,  113,  3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1812,
    };
    return index < HIP_PERIOD_COUNT ? periods[index] : 0;
}

// A note starts the vibrato from its full depth.

static inline void hip_vibrato_start(HipVibrato* vibrato, uint8_t speed, uint8_t depth, uint8_t delay) {
    vibrato->speed = speed;
    vibrato->depth = depth;
    vibrato->delay = delay;
    vibrato->value = depth;
    vibrato->rising = false; // a new note starts falling from the full depth
}

// One tick of the vibrato: the value moves between 0 and the depth, the period changes by (value - depth / 2) / 1024 of
// itself.

static inline uint16_t hip_vibrato_apply(HipVibrato* vibrato, uint16_t period) {
    if (vibrato->delay != 0) {
        vibrato->delay--;
        return period;
    }
    int16_t value = vibrato->value;
    const int16_t depth = vibrato->depth;
    if (!vibrato->rising) {
        value = (int16_t)(value - vibrato->speed);
        if (value < 0) {
            vibrato->rising = true;
            value = 0;
        }
    } else {
        value = (int16_t)(value + vibrato->speed);
        if (value > depth) {
            vibrato->rising = false;
            value = depth;
        }
    }
    vibrato->value = (uint8_t)value;
    const int32_t offset = (int32_t)(int16_t)(value - (depth >> 1)) * (int16_t)period;
    return (uint16_t)(period + (offset >> 10));
}

// One tick of the portamento: `accumulator` grows by the speed (a byte, negative = slide down) and the period changes
// by accumulator / 1024 of itself.

static inline uint16_t hip_portamento_apply(uint16_t* accumulator, uint8_t speed, uint16_t period) {
    if (!(speed & 0x80)) {
        *accumulator = (uint16_t)(*accumulator + speed);
        return (uint16_t)(period - (((uint32_t)*accumulator * period) >> 10));
    }
    *accumulator = (uint16_t)(*accumulator + (uint8_t)-speed);
    return (uint16_t)(period + (((uint32_t)*accumulator * period) >> 10));
}

// The voice volume level a track entry sets (0 = full, 1..15 quieter in steps of 6 %).

static inline uint8_t hip_level_percent(uint8_t level) {
    return level == 0 ? HIP_VOLUME_FULL : (uint8_t)((16 - level) * 6);
}

// A volume sequence value scaled to a voice's percentage.

static inline uint8_t hip_scaled_volume(uint8_t volume, uint8_t percent) {
    return (uint8_t)((uint32_t)volume * percent / HIP_VOLUME_FULL);
}
