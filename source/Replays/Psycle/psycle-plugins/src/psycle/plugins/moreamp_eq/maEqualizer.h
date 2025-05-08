//////////////////////////////////////////////////////////////////////
// Modified for PSYCLE by Sartorius
//

// maEqualizer.h

/*
* MoreAmp
* Copyright     (C)     2004-2005 pmisteli
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the     GNU     General Public License as published     by
* the Free Software     Foundation;     either version 2 of     the     License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY;     without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public     License
* along with this program; if not, write to     the     Free Software
* Foundation, Inc.,     59 Temple Place, Suite 330,     Boston, MA 02111-1307 USA
*
*/

/*
*   Copyright (C) 2002-2006  Felipe Rivera <liebremx at users.sourceforge.net>
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*
*   Coefficient stuff
*
*   $Id: maEqualizer.h 3616 2006-12-17 20:15:14Z johan-boule $
*/

#ifndef __MAEQUALIZER_H__
#define __MAEQUALIZER_H__
namespace psycle::plugins::moreamp_eq {
#if defined DIVERSALIS__COMPILER__MICROSOFT
#define MSVC_ALIGN __declspec(align(16))
#else
#define MSVC_ALIGN
#endif
#if defined DIVERSALIS__COMPILER__GNU
#define GNU_ALIGN __attribute__((aligned))
#else
#define GNU_ALIGN
#endif

#if !defined M_PI
#define M_PI       3.14159265358979323846
#endif

#if !defined M_SQRT2
#define M_SQRT2    1.41421356237309504880
#endif

#define EQBANDS31 31                                    // larger number of pcm equalizer bands (sliders)
#define EQBANDS10 10                                    // lesser number of pcm equalizer bands

#define EQSLIDERMAX 64                                  // pcm equalizer slider limits
#define EQSLIDERMIN 0

#define EQ_MAX_BANDS EQBANDS31
#define EQ_CHANNELS 2

#define B20 0
#define B25 1
#define B31 2
#define B40 3
#define B50 4
#define B62 5
#define B80 6
#define B100 7
#define B125 8
#define B160 9
#define B200 10
#define B250 11
#define B315 12
#define B400 13
#define B500 14
#define B630 15
#define B800 16
#define B1000 17
#define B1250 18
#define B1600 19
#define B2000 20
#define B2500 21
#define B3150 22
#define B4000 23
#define B5000 24
#define B6300 25
#define B8000 26
#define B10000 27
#define B12500 28
#define B16000 29
#define B20000 30

#define _DIV 31
#define PRE_AMP 32
#define BANDS 33
#define EXTRA 34
#define LINK 35

struct sIIRCoefficients
{
	float beta;
	float alpha; 
	float gamma;
	float dummy;
};

/* Coefficient history for the IIR filter */
struct sXYData
{
	float x[3]; /* x[n], x[n-1], x[n-2] */
	float y[3]; /* y[n], y[n-1], y[n-2] */
	float dummy[2];
};

struct band
{
	sIIRCoefficients *coeffs;
	const double *cfs;
	double octave;
	int band_count;
};

#define GAIN_F0 1.0
#define GAIN_F1 GAIN_F0 / M_SQRT2

//#define TETA(f) (2*M_PI*(double)f/bands[n].sfreq)
#define TETA(f) (2*M_PI*double(f/pCB->GetSamplingRate()))
#define TWOPOWER(value) (value * value)

#define BETA2(tf0, tf) \
	(TWOPOWER(GAIN_F1)*TWOPOWER(std::cos(tf0)) \
	- 2.0 * TWOPOWER(GAIN_F1) * std::cos(tf) * std::cos(tf0) \
	+ TWOPOWER(GAIN_F1) \
	- TWOPOWER(GAIN_F0) * TWOPOWER(std::sin(tf)))
#define BETA1(tf0, tf) \
	(2.0 * TWOPOWER(GAIN_F1) * TWOPOWER(std::cos(tf)) \
	+ TWOPOWER(GAIN_F1) * TWOPOWER(std::cos(tf0)) \
	- 2.0 * TWOPOWER(GAIN_F1) * std::cos(tf) * std::cos(tf0) \
	- TWOPOWER(GAIN_F1) + TWOPOWER(GAIN_F0) * TWOPOWER(sin(tf)))
#define BETA0(tf0, tf) \
	(0.25 * TWOPOWER(GAIN_F1) * TWOPOWER(std::cos(tf0)) \
	- 0.5 * TWOPOWER(GAIN_F1) * std::cos(tf) * std::cos(tf0) \
	+ 0.25 * TWOPOWER(GAIN_F1) \
	- 0.25 * TWOPOWER(GAIN_F0) * TWOPOWER(std::sin(tf)))

#define GAMMA(beta, tf0) ((0.5 + beta) * std::cos(tf0))
#define ALPHA(beta) ((0.5 - beta)/2.0)
}
#endif // __MAEQUALIZER_H__
