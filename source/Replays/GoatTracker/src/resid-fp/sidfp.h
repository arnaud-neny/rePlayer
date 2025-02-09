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

#ifndef __SIDFP_H__
#define __SIDFP_H__

#include "siddefsfp.h"
#include "voicefp.h"
#include "filterfp.h"
#include "extfiltfp.h"
#include "potfp.h"

class SIDFP
{
public:
  SIDFP();
  ~SIDFP();

  static float kinked_dac(const int x, const float nonlinearity, const int bits);

  void set_chip_model(chip_model model);
  FilterFP& get_filter() { return filter; }
  void enable_filter(bool enable);
  bool set_sampling_parameters(double clock_freq, sampling_method method,
			       double sample_freq, double pass_freq = 20000);
  void set_voice_nonlinearity(float nl);

  void clock();
  int clock(cycle_count& delta_t, short* buf, int n, int interleave = 1);
  int clock_fast(cycle_count& delta_t, short* buf, int n, int interleave = 1);
  void reset();
  
  // Read/write registers.
  reg8 read(reg8 offset);
  void write(reg8 offset, reg8 value);
  void mute(reg8 channel, bool enable);

  // 16-bit input (EXT IN).
  void input(int sample);

  // 16-bit output (AUDIO OUT).
  float output();

private:
  static double I0(double x);
  inline int clock_interpolate(cycle_count& delta_t, short* buf, int n,
				     int interleave);
  inline int clock_resample_interpolate(cycle_count& delta_t, short* buf,
					      int n, int interleave);
  inline void age_bus_value(cycle_count);

  VoiceFP voice[3];
  FilterFP filter;
  ExternalFilterFP extfilt;
  PotentiometerFP potx;
  PotentiometerFP poty;

  reg8 bus_value;
  cycle_count bus_value_ttl;

  // External audio input.
  float ext_in;

  // Sampling variables.
  sampling_method sampling;
  float cycles_per_sample;
  float sample_offset;
  int sample_index;
  int fir_N;
  int fir_RES;
  /* for linear interpolation mode */
  float sample_prev;

  // Ring buffer with overflow for contiguous storage of RINGSIZE samples.
  float* sample;

  // FIR_RES filter tables (FIR_N*FIR_RES).
  float* fir;

  /* analog parts are run at half the rate of digital ones. */
  float lastsample[3];
  unsigned char filtercyclegate;
};

#endif // not __SIDFP_H__
