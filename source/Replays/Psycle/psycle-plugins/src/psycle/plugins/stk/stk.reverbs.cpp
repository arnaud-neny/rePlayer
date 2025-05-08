/////////////////////////////////////////////////////////////////////
// Dmitry "Sartorius" Kulikov stk Reverbs plugin for PSYCLE
// v1.1
//
// Based on The Synthesis ToolKit in C++ (STK)
// By Perry R. Cook and Gary P. Scavone, 1995-2004.
// http://ccrma.stanford.edu/software/stk/

#include <psycle/plugin_interface.hpp>
#include <cstdio>
#include <stk/Stk.h>
#include <stk/JCRev.h>
#include <stk/NRev.h>
#include <stk/PRCRev.h>
namespace psycle::plugins::stk::reverbs {
using namespace psycle::plugin_interface;

using namespace ::stk;

CMachineParameter const paraRev = {"Reverb", "Reverb", 0, 2, MPF_STATE, 1};
CMachineParameter const paraTime = {"Time", "Time", 1, 1024, MPF_STATE, 80};
CMachineParameter const paraDryWet = {"Dry/Wet", "Dry/Wet", 0,100, MPF_STATE, 25};
CMachineParameter const paraMixing = {"Mixing", "Mixing", 0, 1, MPF_STATE, 1};

CMachineParameter const *pParameters[] = 
{ 
		&paraRev,
		&paraTime,
		&paraDryWet,
		&paraMixing
};


CMachineInfo const MacInfo(
		MI_VERSION,
		0x0110,
		EFFECT,
		sizeof pParameters / sizeof *pParameters,
		pParameters,
		"stk Reverbs"
			#ifndef NDEBUG
				" (debug build)"
			#endif
			,
		"stk Reverbs",
		"Sartorius and STK developers",
		"Help",
		1
);

class mi : public CMachineInterface {
	public:
			mi();
			virtual ~mi();

			virtual void Init();
			virtual void SequencerTick();
			virtual void Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks);
			virtual bool DescribeValue(char* txt,int const param, int const value);
			virtual void ParameterTweak(int par, int val);
			virtual void Command();

	private:
			JCRev   jcrev[2];
			NRev    nrev[2];
			PRCRev  pcrrev[2];
			StkFloat samplerate;
};

PSYCLE__PLUGIN__INSTANTIATOR("stk-rev", mi, MacInfo)

mi::mi() {
	Vals = new int[MacInfo.numParameters];
}

mi::~mi() {
		delete[] Vals;
}
void mi::Command() {
	pCB->MessBox("stk Reverbs\n","stk Reverbs",0);
}

void mi::Init() {
	samplerate = (StkFloat)pCB->GetSamplingRate();
	Stk::setSampleRate(samplerate);
}

void mi::SequencerTick() {
	if(samplerate != (StkFloat)pCB->GetSamplingRate()) {
		samplerate = (StkFloat)pCB->GetSamplingRate();
		Stk::setSampleRate(samplerate);
		StkFloat const t60 = StkFloat(Vals[1]) * 0.03125;
		for(unsigned int i = 0; i < 2; ++i) {
			jcrev[i].setT60(t60);
			nrev[i].setT60(t60);
			pcrrev[i].setT60(t60);
		}
	}
}

void mi::ParameterTweak(int par, int val) {
	// Called when a parameter is changed by the host app / user gui
	Vals[par] = val;
	switch(par) {
		case 0:
		{
			switch(val) {
				case 0: jcrev[0].clear(); jcrev[1].clear(); break;
				case 1: nrev[0].clear(); nrev[1].clear(); break;
				case 2: pcrrev[0].clear(); pcrrev[1].clear(); break;
			}
			break;
		}
		case 1:
		{
			for(unsigned int i = 0; i < 2; ++i) {
				jcrev[i].setT60(StkFloat(val) * 0.03125);
				nrev[i].setT60(StkFloat(val) * 0.03125);
				pcrrev[i].setT60(StkFloat(val) * 0.03125);
			}
			break;
		}
		case 2:
		{
			StkFloat const drywet = StkFloat(val)*.01;
			jcrev[0].setEffectMix(drywet); jcrev[1].setEffectMix(drywet);
			nrev[0].setEffectMix(drywet); nrev[1].setEffectMix(drywet);
			pcrrev[0].setEffectMix(drywet); pcrrev[1].setEffectMix(drywet);
			break;
		}
	}
}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples,int tracks) {
	if(Vals[2] == 0) 
		return;

	if(Vals[3] == 0) {
		switch(Vals[0]) {
			case 0:
				do {
					*psamplesleft = jcrev[0].tick(StkFloat(*psamplesleft),0);
					*psamplesright = jcrev[1].tick(StkFloat(*psamplesright),1);
					++psamplesleft;
					++psamplesright;
				} while(--numsamples);
				break;
			case 1:
				do {
					*psamplesleft = nrev[0].tick(StkFloat(*psamplesleft),0);
					*psamplesright = nrev[1].tick(StkFloat(*psamplesright),1);
					++psamplesleft;
					++psamplesright;
				} while(--numsamples);
				break;
			case 2:
				do {
					*psamplesleft = pcrrev[0].tick(StkFloat(*psamplesleft),0);
					*psamplesright = pcrrev[1].tick(StkFloat(*psamplesright),1);
					++psamplesleft;
					++psamplesright;
				} while(--numsamples);
				break;
		}
	} else {
		switch(Vals[0]) {
			case 0:
				do {
					jcrev[0].tick(StkFloat(*psamplesleft));
					jcrev[1].tick(StkFloat(*psamplesright));
					*psamplesleft = float(jcrev[0].lastOut(0) + 0.3 * jcrev[1].lastOut(0));
					*psamplesright = float(jcrev[1].lastOut(1) + 0.3 * jcrev[0].lastOut(1));
					++psamplesleft;
					++psamplesright;
				} while(--numsamples);
				break;
			case 1:
				do {
					nrev[0].tick(StkFloat(*psamplesleft));
					nrev[1].tick(StkFloat(*psamplesright));
					*psamplesleft = float(nrev[0].lastOut(0) + 0.3 * nrev[1].lastOut(0));
					*psamplesright = float(nrev[1].lastOut(1) + 0.3 * nrev[0].lastOut(1));
					++psamplesleft;
					++psamplesright;
				} while(--numsamples);
				break;
			case 2:
				do {
					pcrrev[0].tick(StkFloat(*psamplesleft));
					pcrrev[1].tick(StkFloat(*psamplesright));
					*psamplesleft = float(pcrrev[0].lastOut(0) + 0.3 * pcrrev[1].lastOut(0));
					*psamplesright = float(pcrrev[1].lastOut(1) + 0.3 * pcrrev[0].lastOut(1));
					++psamplesleft;
					++psamplesright;
				} while(--numsamples);
				break;
		}
	}
}

// Function that describes value on client's displaying
bool mi::DescribeValue(char* txt, int const param, int const value) {
	switch(param) {
		case 0:
			switch(value) {
				case 0:
					std::sprintf(txt, "Church (JCRev)");
					return true;
				case 1:
					std::sprintf(txt, "Open (NRev)");
					return true;
				case 2:
					std::sprintf(txt, "Room (PCRRev)");
					return true;
			}
		break;
		case 1:
			if(value > 1919)
					std::sprintf(txt, "%0.2fs aah! are you nuts?", value * 0.03125);
			else
					std::sprintf(txt, "%0.2fs", value * 0.03125);
			return true;
		break;
		case 2:
			std::sprintf(txt, "%i%%:%i%%", 100 - value, value);
			return true;
		break;
		case 3:
			std::sprintf(txt, value == 0 ? "independent" : "mix channels");
			return true;
	}
	return false;
}
}