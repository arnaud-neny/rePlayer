/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 * Copyright 2011-2025 Leandro Nini <drfiemost@users.sourceforge.net>
 * Copyright 2007-2010 Antti Lankila
 * Copyright 2004 Dag Lem <resid@nimrod.no>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef EXTERNALFILTER_H
#define EXTERNALFILTER_H

#include "siddefs-fp.h"

#include <cstdint>

namespace reSIDfp
{

/**
 * The audio output stage in a Commodore 64 consists of two STC networks, a
 * low-pass RC filter with 3 dB frequency 16kHz followed by a DC-blocker which
 * acts as a high-pass filter with a cutoff dependent on the attached audio
 * equipment impedance. Here we suppose an impedance of 10kOhm resulting
 * in a 3 dB attenuation at 1.6Hz.
 * The STC networks are connected with a [BJT] based [common collector]
 * used as a voltage follower (featuring a 2SC1815 NPN transistor).
 *
 * To operate properly the 6581 audio output needs a pull-down resistor,
 * 1KOhm is the recommended value.
 *
 * ~~~
 *                                 9/12V
 * -----+
 * audio|       10k                  |
 *      +---o----R---o--------o-----(K)          +-----
 *  out |   |        |        |      |           |audio
 * -----+   R 1k     C 1000   |      |    10 uF  |
 *          |        |  pF    +-C----o-----C-----+ ~10k
 *                             470   |           |
 *         GND      GND         pF   R 1K        | amp
 *          *                   **   |           +-----
 *
 *                                  GND
 * ~~~
 *
 * * Only for the 6581.
 * ** The C64c board additionally includes a [bootstrap] capacitor to increase
 *    the input impedance of the common collector.
 *
 * The C128 boards include an additional capacitor and a resistor
 * to the ouptput stage.
 *
 * ~~~
 *                                 9/12V
 * -----+
 * audio|       10k                  |
 *      +---o----R---o--------o-----(K)                    +-----
 *  out |   |        |        |      |                     |audio
 * -----+   R 1k     C 1000   |      |        10 uF        |
 *          |        |  pF    +-C----o-----o----C---o------+ ~10k
 *                             470   |     |        |      |
 *         GND      GND         pF   R 1K  C 220pF  R 47K  | amp
 *          *                        |     |        |      +-----
 *
 *                                  GND   GND      GND
 * ~~~
 *
 * [BJT]: https://en.wikipedia.org/wiki/Bipolar_junction_transistor
 * [common collector]: https://en.wikipedia.org/wiki/Common_collector
 * [bootstrap]: https://en.wikipedia.org/wiki/Bootstrapping_(electronics)
 */
class ExternalFilter
{
    friend class State;

private:
    /// Lowpass filter voltage
    int32_t Vlp[2];

    /// Highpass filter voltage
    int32_t Vhp[2];

    int32_t w0lp_1_s7[2] = { 0 };

    int32_t w0hp_1_s17[2] = { 0 };

public:
    /**
     * SID clocking.
     *
     * @param input input sample, signed 16 bit
     * @return filtered sample, signed 16 bit
     */
    SampleI32 clock(SampleI32 input);

    /**
     * Constructor.
     */
    ExternalFilter();

    /**
     * Setup of the external filter sampling parameters.
     *
     * @param frequency the main system clock frequency
     */
    void setClockFrequency(double frequency);

    /**
     * SID reset.
     */
    void reset();
};

} // namespace reSIDfp

#if RESIDFP_INLINING || defined(EXTERNALFILTER_CPP)

namespace reSIDfp
{

RESIDFP_INLINE
SampleI32 ExternalFilter::clock(SampleI32 input)
{
    for (int i = 0; i < 2; ++i)
	{
		const int32_t Vi = (&input.left)[i] << 11;
		const int32_t dVlp = (w0lp_1_s7[i] * (Vi - Vlp[i]) >> 7);
		const int32_t dVhp = (w0hp_1_s17[i] * (Vlp[i] - Vhp[i]) >> 17);
		Vlp[i] += dVlp;
		Vhp[i] += dVhp;
	}
    return { (Vlp[0] - Vhp[0]) >> 11, (Vlp[1] - Vhp[1]) >> 11 };
}

} // namespace reSIDfp

#endif

#endif
