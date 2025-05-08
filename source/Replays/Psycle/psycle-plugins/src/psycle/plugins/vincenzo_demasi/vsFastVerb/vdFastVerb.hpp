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
#include "fastverbfilter.hpp"
namespace psycle::plugins::vincenzo_demasi::vsFastVerb {
enum {
	LDELAY=0,
	LFEEDBACK,
	RDELAY,
	RFEEDBACK,
	LOCKDELAY,
	LOCKFEEDBACK
};

#define PARCOLS   3

#define VDPLUGINNAME "vd's fastverb filter"
#define VDSHORTNAME  "FastVerb"
#define VDAUTHOR     "V. Demasi (Built on " __DATE__ ")"
#define VDCOMMAND    "License"
int const VDVERSION = 0x110;

#define MIN_DELAY  1
#define MAX_DELAY  22050
#define MIN_FEEDBACK   0
#define MAX_FEEDBACK   100
#define SET_LDELAY (MAX_DELAY / 2)
#define SET_RDELAY (MAX_DELAY / 2)
#define SET_LFEEDBACK  (MAX_FEEDBACK / 2)
#define SET_RFEEDBACK  (MAX_FEEDBACK / 2)
#define GAIN_NORM 100.0f
#define MIN_LOCK 0
#define MAX_LOCK 1
#define SET_LOCK 1

using psycle::plugin_interface::CMachineParameter;
using psycle::plugin_interface::MPF_STATE;

CMachineParameter const parLeftDelay =
{
	"Left Delay", "Left Delay (samples)", MIN_DELAY, MAX_DELAY, MPF_STATE, SET_LDELAY
};

CMachineParameter const parLeftFeedback =
{
	"Left Feedback", "Left Feedback (%)", MIN_FEEDBACK, MAX_FEEDBACK, MPF_STATE, SET_LFEEDBACK
};

CMachineParameter const parRightDelay =
{
	"Right Delay", "Right Delay (samples)", MIN_DELAY, MAX_DELAY, MPF_STATE, SET_RDELAY
};

CMachineParameter const parRightFeedback =
{
	"Right Feedback", "Right Feedback (%)", MIN_FEEDBACK, MAX_FEEDBACK, MPF_STATE, SET_RFEEDBACK
};

CMachineParameter const parLockDelay =
{
	"Lock Delay", "Lock Delay (on/off)", MIN_LOCK, MAX_LOCK, MPF_STATE, SET_LOCK
};

CMachineParameter const parLockFeedback =
{
	"Lock Feedback", "Lock Feedback (on/off)", MIN_LOCK, MAX_LOCK, MPF_STATE, SET_LOCK
};

CMachineParameter const *pParameters[] =
{
	&parLeftDelay,
	&parLeftFeedback,
	&parRightDelay,
	&parRightFeedback,
	&parLockDelay,
	&parLockFeedback
};

psycle::plugin_interface::CMachineInfo const MacInfo (
	psycle::plugin_interface::MI_VERSION,
	VDVERSION,
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
		FastverbFilter leftFilter;
		FastverbFilter rightFilter;
		int lastDelayModified, lastFeedbackModified;
		float lFeedback, rFeedback;
		int currentSR;
};
}