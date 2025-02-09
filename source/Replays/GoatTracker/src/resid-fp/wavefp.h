//  ---------------------------------------------------------------------------
//  This file is part of reSID, a MOS6581 SID emulator engine.
//  Copyright (C) 2004  Dag Lem <resid@nimrod.no>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//  ---------------------------------------------------------------------------

#ifndef __WAVEFP_H__
#define __WAVEFP_H__

#include "siddefsfp.h"

extern float dac[12];
extern float wftable[11][4096];

// ----------------------------------------------------------------------------
// A 24 bit accumulator is the basis for waveform generation. FREQ is added to
// the lower 16 bits of the accumulator each cycle.
// The accumulator is set to zero when TEST is set, and starts counting
// when TEST is cleared.
// The noise waveform is taken from intermediate bits of a 23 bit shift
// register. This register is clocked by bit 19 of the accumulator.
// ----------------------------------------------------------------------------
class WaveformGeneratorFP
{
public:
  WaveformGeneratorFP();

  void set_chip_model(chip_model model);

  inline void clock();
  inline void synchronize(WaveformGeneratorFP& dest, WaveformGeneratorFP& source);
  void reset();

  void writeFREQ_LO(reg8 value);
  void writeFREQ_HI(reg8 value);
  void writePW_LO(reg8 value);
  void writePW_HI(reg8 value);
  void writeCONTROL_REG(WaveformGeneratorFP& source, reg8 value);
  reg8 readOSC(WaveformGeneratorFP& source);

  inline float output(WaveformGeneratorFP& source);

protected:
  void clock_noise(const bool clock);
  reg12 outputN___();
  void set_nonlinearity(float nl);
  void rebuild_wftable();
  void calculate_waveform_sample(float o[12]);

  chip_model model;

  // Tell whether the accumulator MSB was set high on this cycle.
  bool msb_rising;

  reg24 accumulator;
  reg24 shift_register;
  reg8 previous;
  int noise_overwrite_delay;

  // Fout  = (Fn*Fclk/16777216)Hz
  reg16 freq;
  // PWout = (PWn/40.95)%
  reg12 pw;

  // The control register right-shifted 4 bits; used for output function
  // table lookup.
  reg8 waveform;

  // The remaining control register bits.
  bool test, ring_mod, sync;
  // The gate bit is handled by the EnvelopeGenerator.

  // zero level offset of waveform (< 0)
  float wave_zero;

  float previous_dac;

friend class VoiceFP;
friend class SIDFP;
};

// ----------------------------------------------------------------------------
// SID clocking - 1 cycle.
// ----------------------------------------------------------------------------
inline
void WaveformGeneratorFP::clock()
{
  /* no digital operation if test bit is set. Only emulate analog fade. */
  if (test) {
    if (noise_overwrite_delay != 0) {
	if (-- noise_overwrite_delay == 0) {
	    shift_register |= 0x7ffffc;
	    clock_noise(false);
	}
    }
    return;
  }

  reg24 accumulator_prev = accumulator;

  // Calculate new accumulator value;
  accumulator += freq;
  accumulator &= 0xffffff;

  // Check whether the MSB became set high. This is used for synchronization.
  msb_rising = !(accumulator_prev & 0x800000) && (accumulator & 0x800000);

  // Shift noise register once for each time accumulator bit 19 is set high.
  if (!(accumulator_prev & 0x080000) && (accumulator & 0x080000)) {
    clock_noise(true);
  }
}

// ----------------------------------------------------------------------------
// Synchronize oscillators.
// This must be done after all the oscillators have been clock()'ed since the
// oscillators operate in parallel.
// Note that the oscillators must be clocked exactly on the cycle when the
// MSB is set high for hard sync to operate correctly. See SID::clock().
// ----------------------------------------------------------------------------
inline
void WaveformGeneratorFP::synchronize(WaveformGeneratorFP& sync_dest, WaveformGeneratorFP &sync_source)
{
  // A special case occurs when a sync source is synced itself on the same
  // cycle as when its MSB is set high. In this case the destination will
  // not be synced. This has been verified by sampling OSC3.
  if (msb_rising && sync_dest.sync && !(sync && sync_source.msb_rising)) {
    sync_dest.accumulator = 0;
  }
}

// ----------------------------------------------------------------------------
// Select one of 16 possible combinations of waveforms.
// ----------------------------------------------------------------------------
inline
float WaveformGeneratorFP::output(WaveformGeneratorFP& sync_source)
{
  if (waveform == 0 || waveform > 7) {
    return previous_dac;
  }
  /* waveforms 1 .. 7 left */

  /* Phase for all waveforms */
  reg12 phase = accumulator >> 12;
  /* pulse on/off generates 4 more variants after the main pulse types */
  int variant = waveform >= 4 && (test || phase >= pw) ? 3 : -1;

  /* triangle waveform XOR circuit. Since the table already makes a triangle
   * wave internally, we only need to account for the sync source here.
   * Flipping the top bit suffices to reproduce the original SID ringmod */
  phase ^= ((waveform & 3) == 1 && ring_mod && (sync_source.accumulator & 0x800000)) ? 0x800 : 0x00;

  return wftable[waveform + variant][phase];
}

#endif // not __WAVEFP_H__
