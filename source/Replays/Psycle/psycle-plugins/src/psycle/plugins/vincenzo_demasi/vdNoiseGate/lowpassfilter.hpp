/*      Copyright (C) 2002 Vincenzo Demasi.

		This plugin is free software; you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation; either version 2 of the License, or
		(at your option) any later version.\n"\

		This plugin is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
		GNU General Public License for more details.

		You should have received a copy of the GNU General Public License
		along with this program; if not, write to the Free Software
		Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

		Vincenzo Demasi. E-Mail: <v.demasi@tiscali.it>
*/

#pragma once
namespace psycle::plugins::vincenzo_demasi::vdNoiseGate {
class LowPassFilter
{

private:
	float f, q, fb, buf0, buf1;
public:

	LowPassFilter()
	{
		f = q = fb = buf0 = buf1 = 0.0f;
	}

	~LowPassFilter()
	{
	}

	inline void setCutOff(float fval)
	{
		/* WARNING: f must be in [0, 1] (test ignored) */
		f = fval;
		fb = q + q / (1.0f -f);
	}

	inline void setResonance(float qval)
	{
		/* WARNING: q must be in [0, 1] (test ignored) */
		q = qval;
		fb = q + q / (1.0f -f);
	}

	inline float process(float sample)
	{
		buf0 += f * (sample - buf0 + fb * (buf0 - buf1));
		buf1 += f * (buf0 - buf1);
		return buf1;
	};
};
}