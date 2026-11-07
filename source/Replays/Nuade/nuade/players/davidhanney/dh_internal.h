// SPDX-License-Identifier: MIT
// David Hanney player: the sounds and tables built into the eagleplayer (dh_data.c).

#pragma once

#include <nuade/players/davidhanney.h>
#include <stdint.h>

#define DH_PERIODS 70 // entries of the period table

// The sixteen instruments as the eagleplayer holds them, before the sanitising at the start of a song.
extern const DhInstrument k_dh_instruments[DH_INSTRUMENTS];

extern const uint16_t k_dh_periods[DH_PERIODS];
extern const uint8_t k_dh_chords[DH_CHORDS][DH_CHORD_STEPS];

// The built-in wave of a wave_kind 1..3 instrument and its length in words.
const int8_t* dh_builtin_wave(uint16_t wave_kind, uint16_t* length_words);
