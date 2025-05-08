#pragma once
/*		CSIDFilter (C)2008 Jeremy Evers, http:://jeremyevers.com

		This program is free software; you can redistribute it and/or modify
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
*/

#define PIf	3.1415926535897932384626433832795f;
#define ANTIDENORMAL 1e-15f // could be 1e-18f, but this is still way below audible noise threshold
namespace psycle::plugins::jme::blitzn {
enum eAlgorithm {
	FILTER_ALGO_SID_LPF = 0,
	FILTER_ALGO_SID_HPF,
	FILTER_ALGO_SID_BPF,
	FILTER_ALGO_SID_LPF_HPF,
	FILTER_ALGO_SID_LPF_BPF,
	FILTER_ALGO_SID_LPF_HPF_BPF,
	FILTER_ALGO_SID_HPF_BPF
};

class CSIDFilter {
public:
	CSIDFilter();

	void setAlgorithm(eAlgorithm a_algo);
	void reset();
	void recalculateCoeffs(const float a_fFrequency, const float a_fFeedback, const float sampleRate);
	inline void process(float& sample);
private:

	eAlgorithm m_Algorithm;

	// pre-calculated coefficients
	float m_f, m_fb;

	// filter data
	float m_low, m_high, m_band;
};

inline void CSIDFilter::process(float& sample)
{
	const float f = m_f;
	const float fb = m_fb;
	float low = m_low;
	float band = m_band;
	float high = m_high;

	low -= (f*band);
	band -= (f*high);
	high = (band*fb) - low - sample + ANTIDENORMAL;

	switch (m_Algorithm)
	{
		case FILTER_ALGO_SID_LPF:
		{
			sample = low;
			break;
		}
		case FILTER_ALGO_SID_HPF:
		{
			sample = high;
			break;
		}
		case FILTER_ALGO_SID_BPF:
		{
			sample = band;
			break;
		}
		case FILTER_ALGO_SID_LPF_HPF:
		{
			sample = low + high;
			break;
		}
		case FILTER_ALGO_SID_LPF_BPF:
		{
			sample = low + band;
			break;
		}
		case FILTER_ALGO_SID_LPF_HPF_BPF:
		{
			sample = low + band + high;
			break;
		}
		case FILTER_ALGO_SID_HPF_BPF:
		{
			sample = band + high;
			break;
		}
		default:
			break;
	}
	m_low = low;
	m_band = band;
	m_high = high;
}
}