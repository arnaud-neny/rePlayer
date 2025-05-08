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

#include <psycle/plugin_interface.hpp>
#include "../vdabout.hpp"
#include "lowpassfilter.hpp"
namespace psycle::plugins::vincenzo_demasi::vdNoiseGate {
enum {
	LTHRESHOLD=0,
	RTHRESHOLD,
	LOCKTHRESHOLD,
	PREGAIN,
	CUTOFF,
	RESONANCE,
	LOWPASS,
	GAIN
};

#define PARCOLS 2

#define VDPLUGINNAME "vd's noise gate filter"
#define VDSHORTNAME  "NoiseGate"
#define VDAUTHOR     "V. Demasi (Built on " __DATE__ ")"
#define VDCOMMAND    "License"

#define MIN_THRESHOLD      0
#define MAX_THRESHOLD  32768
#define SET_LTHRESHOLD  6000
#define SET_RTHRESHOLD  6000
#define MIN_CUTOFF         0
#define MAX_CUTOFF        99
#define SET_CUTOFF        50
#define MIN_RESONANCE      0
#define MAX_RESONANCE     99
#define SET_RESONANCE     50
#define MIN_LOWPASS        0
#define MAX_LOWPASS        1
#define SET_LOWPASS        1
#define MIN_PREGAIN        0
#define MAX_PREGAIN      400
#define SET_PREGAIN      100
#define MIN_GAIN           0
#define MAX_GAIN         100
#define SET_GAIN         100
#define GAIN_NORM     100.0f
#define MIN_LOCK           0
#define MAX_LOCK           1
#define SET_LOCK           1

using psycle::plugin_interface::CMachineParameter;
using psycle::plugin_interface::MPF_STATE;

CMachineParameter const parLeftThreshold =
{
	"Left Threshold", "Left Threshold (Magnitude)", MIN_THRESHOLD, MAX_THRESHOLD, MPF_STATE, SET_LTHRESHOLD
};

CMachineParameter const parRightThreshold =
{
	"Right Threshold", "Right Threshold (Magnitude)", MIN_THRESHOLD, MAX_THRESHOLD, MPF_STATE, SET_RTHRESHOLD
};

CMachineParameter const parLockThreshold =
{
	"Lock Threshold", "Lock Threshold (on/off)", MIN_LOCK, MAX_LOCK, MPF_STATE, SET_LOCK
};

CMachineParameter const parPreGain =
{
	"Pre-Gain", "Pre-Gain (%)", MIN_PREGAIN, MAX_PREGAIN, MPF_STATE, SET_PREGAIN
};

CMachineParameter const parCutOff =
{
	"Cut Off", "Cut Off", MIN_CUTOFF, MAX_CUTOFF, MPF_STATE, SET_CUTOFF
};

CMachineParameter const parResonance =
{
	"Resonance", "Resonance", MIN_RESONANCE, MAX_RESONANCE, MPF_STATE, SET_RESONANCE
};

CMachineParameter const parLowPass =
{
	"Low Pass", "Low Pass Filter (on/off)", MIN_LOWPASS, MAX_LOWPASS, MPF_STATE, SET_LOWPASS
};

CMachineParameter const parGain =
{
	"Gain", "Gain (%)", MIN_GAIN, MAX_GAIN, MPF_STATE, SET_GAIN
};

CMachineParameter const *pParameters[] =
{
	&parLeftThreshold,
	&parRightThreshold,
	&parLockThreshold,
	&parPreGain,
	&parCutOff,
	&parResonance,
	&parLowPass,
	&parGain
};

psycle::plugin_interface::CMachineInfo const MacInfo (
	psycle::plugin_interface::MI_VERSION,
	psycle::plugin_interface::EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
#ifdef _DEBUG
	VDPLUGINNAME " (Debug Build)",
#else
	VDPLUGINNAME,
#endif
	VDSHORTNAME,
	VDAUTHOR,
	VDCOMMAND,
	PARCOLS
);

class mi : public psycle::plugin_interface::CMachineInterface {
	public:
		mi();
		virtual ~mi();

		virtual void Init();
		virtual void Command();
		virtual void ParameterTweak(int par, int val);
		virtual void SequencerTick();
		virtual bool DescribeValue(char *txt,int const param, int const value);
		virtual void Work(float *pleftsamples, float *prightsamples, int samplesnum, int tracks);
	private:
		float lThreshold, rThreshold;
		LowPassFilter leftFilter, rightFilter;
		float pregain, gain;
		int lastThresholdModified;
		int lowPassOn;
		int currentSR;
};
}