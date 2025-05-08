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

#include <stdio.h>
#include "vdFastVerb.hpp"
namespace psycle::plugins::vincenzo_demasi::vsFastVerb {
PSYCLE__PLUGIN__INSTANTIATOR("vsfastverb", mi, MacInfo)

mi::mi()
{
	// The constructor zone
	Vals = new int[MacInfo.numParameters];
}

mi::~mi()
{
	// Destroy dinamically allocated objects/memory here
	delete[] Vals;
}

void mi::Init()
{
	// Initialize your stuff here
	lastDelayModified = LDELAY;
	lastFeedbackModified = LFEEDBACK;
	lFeedback = rFeedback = 0.0f;
	currentSR = pCB->GetSamplingRate();
	leftFilter.changeSamplerate(currentSR);
	rightFilter.changeSamplerate(currentSR);
	memset(Vals,0,sizeof(int));
}

void mi::Command()
{
	// Called when user presses editor button
	// Probably you want to show your custom window here
	// or an about button
	pCB->MessBox(VDABOUTMESSAGE, VDABOUTCAPTION(VDPLUGINNAME), VDABOUTYPE);
}

void mi::ParameterTweak(int par, int val)
{
	Vals[par] = val;
	const float srMult = currentSR/44100.0f;
	switch(par)
	{
	case LDELAY:
		leftFilter.setDelay(val*srMult);
		if(Vals[LOCKDELAY])
		{
			Vals[RDELAY] = val;
			rightFilter.setDelay(val*srMult);
		}
		lastDelayModified = LDELAY;
		break;
	case RDELAY:
		rightFilter.setDelay(val*srMult);
		if(Vals[LOCKDELAY])
		{
			Vals[LDELAY] = val;
			leftFilter.setDelay(val*srMult);
		}
		lastDelayModified = RDELAY;
		break;
	case LFEEDBACK:
		leftFilter.setFeedback(lFeedback = val / GAIN_NORM);
		if(Vals[LOCKFEEDBACK])
		{
			Vals[RFEEDBACK] = val;
			rightFilter.setFeedback(rFeedback = lFeedback);
		}
		lastFeedbackModified = LFEEDBACK;
		break;
	case RFEEDBACK:
		rightFilter.setFeedback(rFeedback = val / GAIN_NORM);
		if(Vals[LOCKFEEDBACK])
		{
			Vals[LFEEDBACK] = val;
			leftFilter.setFeedback(lFeedback = rFeedback);
		}
		lastFeedbackModified = RFEEDBACK;
		break;
	case LOCKDELAY:
		if(val)
			if(lastDelayModified == LDELAY)
			{
				Vals[RDELAY] = Vals[LDELAY];
				rightFilter.setDelay(Vals[RDELAY]);
			}
			else
			{
				Vals[LDELAY] = Vals[RDELAY];
				leftFilter.setDelay(Vals[LDELAY]);
			}
		break;
	case LOCKFEEDBACK:
		if(val)
			if(lastFeedbackModified == LFEEDBACK)
			{
				Vals[RFEEDBACK] = Vals[LFEEDBACK];
				rightFilter.setFeedback(rFeedback = lFeedback);
			}
			else
			{
				Vals[LFEEDBACK] = Vals[RFEEDBACK];
				leftFilter.setFeedback(lFeedback = rFeedback);
			}
		break;
	}
}

// Called on each tick while sequencer is playing
void mi::SequencerTick()
{
	if (currentSR != pCB->GetSamplingRate()) {
		currentSR = pCB->GetSamplingRate();
		leftFilter.changeSamplerate(currentSR);
		leftFilter.setDelay(Vals[LDELAY]*currentSR/44100.0f);
		rightFilter.changeSamplerate(currentSR);
		rightFilter.setDelay(Vals[RDELAY]*currentSR/44100.0f);
	}

}

// Function that describes value on client's displaying
bool mi::DescribeValue(char *txt,int const param, int const value)
{
	switch (param)
	{
	case LDELAY:
	case RDELAY: sprintf(txt, "%.1f ms", (float)value / 44.100f); return true;
	case LFEEDBACK:
	case RFEEDBACK: sprintf(txt, "%d%%", value); return true;
	case LOCKDELAY:
	case LOCKFEEDBACK: sprintf(txt, "%s", value ? "on" : "off"); return true;
	}
	return false;
}

void mi::Work(float *pleftsamples, float *prightsamples , int samplesnum, int tracks)
{
	do
	{
		*pleftsamples = leftFilter.process(*pleftsamples);
		*prightsamples = rightFilter.process(*prightsamples);
		++pleftsamples;
		++prightsamples;
	}
	while(--samplesnum);
}
}