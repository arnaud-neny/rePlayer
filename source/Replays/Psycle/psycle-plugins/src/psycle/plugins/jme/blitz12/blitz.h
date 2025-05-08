/*								Blitz (C)2005-2009 by jme
		Programm is based on Arguru Bass. Filter seems to be Public Domain.

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
*/

#pragma once
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cmath>

#include <psycle/plugin_interface.hpp>
#include "voice.h"
#include "lfo.h"
#include "pwm.h"

#define MIN_ENV_TIME				1
#define MAX_ENV_TIME				65536
#define MAX_TRACKS				64
namespace psycle::plugins::jme::blitz12 {
class mi : public psycle::plugin_interface::CMachineInterface {
public:
	void InitWaveTable();
	void updateOsc(int osc);
	mi();
	virtual ~mi();

	virtual void Init();
	virtual void SequencerTick();
	virtual void Work(float *psamplesleft, float* psamplesright, int numsamples, int tracks);
	virtual bool DescribeValue(char* txt,int const param, int const value);
	virtual void Command();
	virtual void ParameterTweak(int par, int val);
	virtual void SeqTick(int channel, int note, int ins, int cmd, int val);
	virtual void Stop();

private:
	CSynthTrack track[MAX_TRACKS];
	VOICEPAR globals;
	int InitPos[4];
	int slomo;
	pwm InitLoop[4];
	lfo SyncViber;
	lfo FiltViber;
};
}