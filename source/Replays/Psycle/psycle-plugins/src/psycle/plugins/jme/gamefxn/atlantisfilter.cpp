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


#include "atlantisfilter.h"
#define piX2		 6.283185307179586476925286766559

namespace psycle::plugins::jme::gamefxn {
CSIDFilter::CSIDFilter()
{
	m_f = m_fb = 0;
	reset();
};


void CSIDFilter::reset()
{
	m_low=m_high=m_band=0;
};

void CSIDFilter::setAlgorithm(eAlgorithm a_algo)
{
	m_Algorithm = a_algo;
}

void CSIDFilter::recalculateCoeffs(const float a_fFrequency, const float a_fFeedback, const float sampleRate)
{
	//m_f = (200.0+(17800.0*6.0*a_fFrequency))*44100.0/sampleRate*piX2/985248.0;
	float newfreq = (a_fFrequency < sampleRate*0.5f)? a_fFrequency : sampleRate *0.5f;
	m_f =(1.0f+(534.0f*newfreq))*44100.f/sampleRate*0.00127545253726566f; 
	float f2 = 2.0f*(a_fFeedback-(a_fFrequency*a_fFrequency)); 
	if (f2 < 0.0f) f2 = 0.0f;
	m_fb = 1.0f/(0.707f+f2);
}
}