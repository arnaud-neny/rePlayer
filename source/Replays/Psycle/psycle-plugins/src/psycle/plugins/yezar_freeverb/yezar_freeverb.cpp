#include <psycle/plugin_interface.hpp>
#include "RevModel.hpp"
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cmath>
namespace psycle::plugins::yezar_freeverb {
/////////////////////////////////////////////////////////////////////
// Arguru's psycle port of Yezar freeverb
using namespace psycle::plugin_interface;

#define NCOMBS				1
#define NALLS				12

CMachineParameter const paraCombdelay = {"Absortion", "Damp", 1, 640, MPF_STATE, 320};
CMachineParameter const paraCombseparator = {"Stereo Width", "Width", 1, 640, MPF_STATE, 126};
CMachineParameter const paraAPdelay = {"Room size", "Room size", 1, 640, MPF_STATE, 175};
CMachineParameter const paraDry = {"Dry Amount", "Dry", 0, 640, MPF_STATE, 256};
CMachineParameter const paraWet = {"Wet Amount", "Wet", 0, 640, MPF_STATE, 128};

CMachineParameter const *pParameters[] = {
	&paraCombdelay,
	&paraCombseparator,
	&paraAPdelay,
	&paraDry,
	&paraWet,
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0110,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"Jezar Freeverb"
		#ifndef NDEBUG
		" (Debug build)"
		#endif
		,
	"Freeverb",
	"Jezar",
	"About",
	2
);

class mi : public CMachineInterface {
	public:
		mi();
		virtual ~mi();
		virtual void Init();
		virtual void SequencerTick();
		virtual void Work(float *psamplesleft, float* psamplesright, int numsamples, int tracks);
		virtual bool DescribeValue(char* txt,int const param, int const value);
		virtual void Command();
		virtual void ParameterTweak(int par, int val);
	private:
		revmodel reverb;
		int currentSR;
};

PSYCLE__PLUGIN__INSTANTIATOR("arguru-freeverb", mi, MacInfo)

mi::mi() {
	Vals = new int[MacInfo.numParameters];
}

mi::~mi() {
	delete[] Vals;
}

void mi::Init() {
	currentSR = pCB->GetSamplingRate();
	reverb.setdefaultvalues(currentSR);
}

void mi::SequencerTick() {
	if (currentSR != pCB->GetSamplingRate()) {
		currentSR = pCB->GetSamplingRate();
		reverb.recalculatebuffers(currentSR);
	}
}

void mi::ParameterTweak(int par, int val) {
	Vals[par]=val;

	// Set damp
	reverb.setdamp(Vals[0]*0.0015625f);
	// Set width
	reverb.setwidth(Vals[1]*0.0015625f);
	// Set room size
	reverb.setroomsize(Vals[2]*0.0015625f);
	// Set reverb wet/dry
	reverb.setdry(Vals[3]*0.0015625f);
	reverb.setwet(Vals[4]*0.0015625f);
}

void mi::Command() {
	pCB->MessBox("Ported in 31/5/2000 by Juan Antonio Arguelles Rius for Psycl3!","·-=<([Freeverb])>=-·",0);
}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks) {
	reverb.processreplace(psamplesleft,psamplesright,psamplesleft,psamplesright,numsamples,1);
}

// Function that describes value on client's displaying
bool mi::DescribeValue(char* txt,int const param, int const value) {
	///\todo use a switch statement
	if(param==0) {
		sprintf(txt,"%.f%%",value*0.15625f);
		return true;
	} else if(param==1) {
		sprintf(txt,"%.0f degrees",value*0.28125f);
		return true;
	} else if(param==2) {
		sprintf(txt,"%.1f mtrs.",(float)value*0.17f);
		return true;
	} else if(param==3) {
		sprintf(txt,"%.1f%%",float(value)*0.31250f);
		return true;
	} else if(param==4) {
		sprintf(txt,"%.1f%%",float(value)*0.46875f);
		return true;
	}
	return false;
}
}