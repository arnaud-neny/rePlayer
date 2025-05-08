// ThunderPalace SoftSat - saturator/waveshaper plugin for Psycle

/*
	* Copyright (c) 2005 Thunder Palace Entertainment
	*
	* Permission to use, copy, modify, and distribute this software for any
	* purpose with or without fee is hereby granted, provided that the above
	* copyright notice and this permission notice appear in all copies.
	*
	* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
	* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
	* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
	* ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
	* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
	* ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
	* OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
	*
	* Alternatively, this software may be used under the terms of the GNU
	* General Public License, version 2 or later, as published by the Free
	* Software Foundation.
	*/

// This was made by Catatonic Porpoise <graue@oceanbase.org>

// It is based on the "Arguru's Distortion" machine which was released
// to the public domain.

// This effect uses a simple algorithm that was not invented by me,
// but rather, was posted on www.musicdsp.org by a Bram de Jong, to
// whom I am very grateful. If the link is still valid by the time
// you read this, you can see this original posting here:
//
//   http://www.musicdsp.org/showone.php?id=42

// version 0.2 - threshold now defaults to 32768 rather than to 512

#include <psycle/plugin_interface.hpp>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <cstdio>

namespace psycle::plugins::graue::softsat {
using namespace psycle::plugin_interface;

CMachineParameter const paraThreshold = {"Threshold", "Threshold level", 16, 32768, MPF_STATE, 32768};
CMachineParameter const paraHardness = {"Hardness", "Hardness", 1, 2048, MPF_STATE, 1024};

CMachineParameter const *pParameters[] = { 
	&paraThreshold,
	&paraHardness
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0100,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
#ifndef NDEBUG
	"ThunderPalace SoftSat (Debug build)",
#else
	"ThunderPalace SoftSat",
#endif
	"SoftSat",
	"Catatonic Porpoise",
	"About",
	3
);


class mi : public CMachineInterface
{
public:
	mi();
	virtual ~mi();

	virtual void Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks);
	virtual bool DescribeValue(char* txt,int const param, int const value);
	virtual void Command();
	virtual void ParameterTweak(int par, int val);

private:
	inline void ProcessChannel(float& sample);
	float gradation;
	float range;
};

PSYCLE__PLUGIN__INSTANTIATOR("thunderpalace-softsat", mi, MacInfo)

mi::mi()
{
	Vals = new int[MacInfo.numParameters];
}

mi::~mi()
{
	delete[] Vals;
}

void mi::Command()
{
	pCB->MessBox("Made September 9, 2005 by Catatonic Porpoise","About SoftSat",0);
}

void mi::ParameterTweak(int par, int val)
{
	Vals[par] = val;
	switch(par) {
	case 0: range = ((float) val) / ((gradation+1)/2);
		break;
	case 1: gradation = ((float) val) / 2049.0f;
		break;
	}
}

void mi::Work(float *psamplesleft, float *psamplesright, int numsamples, int tracks)
{
	do {
		float sl = *psamplesleft / range;
		float sr = *psamplesright / range;

		if (sl > 0) {
			ProcessChannel(sl);
		}
		else {
			sl = -sl;
			ProcessChannel(sl);
			sl = -sl;
		}

		if (sr > 0) {
			ProcessChannel(sr);
		}
		else {
			sr = -sr;
			ProcessChannel(sr);
			sr = -sr;
		}

		*psamplesleft = sl * range;
		*psamplesright = sr * range;

		++psamplesleft;
		++psamplesright;

	} while(--numsamples);
}

bool mi::DescribeValue(char* txt,int const param, int const value)
{
	if (param == 1) // hardness
	{
		sprintf(txt, "%.3f", ((float) value) / 2048.0f);
		return true;
	}

	return false; // use default (integer) display mechanism?
}


inline void mi::ProcessChannel(float& sample)
{
	if (sample > gradation)
	{
		sample = gradation + (sample-gradation)
			/ (1+((sample-gradation)/(1-gradation))
				* ((sample-gradation)/(1-gradation)));
	}
	if (sample > 1.0f)
		sample = (gradation + 1.0f) / 2.0f;
}
}