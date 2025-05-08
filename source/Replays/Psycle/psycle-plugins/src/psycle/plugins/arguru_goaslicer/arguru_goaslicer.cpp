///\file
///\brief Arguru simple distortion/saturator plugin for PSYCLE
#include <psycle/plugin_interface.hpp>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <cstdio>

namespace psycle::plugins::arguru_goaslicer {

using namespace psycle::plugin_interface;

CMachineParameter const paraLength = {"Length","Length",1,8192,MPF_STATE,2048};
CMachineParameter const paraSlope = {"Slope","Slope",1,2048,MPF_STATE,512};

CMachineParameter const *pParameters[] = {
	&paraLength,
	&paraSlope
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0120,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"Arguru Goaslicer"
	#ifndef NDEBUG
		" (Debug build)"
	#endif
	,
	"Goaslicer",
	"J. Arguelles",
	"About",
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
		virtual void Command();
		virtual void ParameterTweak(int par, int val);
	private:
		float slopeAmount;
		int timerSamples;
		bool muted;
		bool changing;
		float m_CurrentVolume;
		float m_TargetVolume;
		int currentSR;
		int m_Timer;
};

PSYCLE__PLUGIN__INSTANTIATOR("arguru-goaslicer", mi, MacInfo)

mi::mi(): slopeAmount(0.01f), timerSamples(2048), muted(false), changing(false),
m_CurrentVolume(1.0f), m_TargetVolume(1.0f), currentSR(44100), m_Timer(0) {
	Vals = new int[MacInfo.numParameters];
}

mi::~mi() {
	delete[] Vals;
}
void mi::Init() {
	currentSR = pCB->GetSamplingRate();
	m_Timer = 0;
	muted = false;
	changing = false;
	m_CurrentVolume = 1.0f;
	m_TargetVolume = 1.0f;
}
void mi::SequencerTick() {
	if (currentSR != pCB->GetSamplingRate()) {
		currentSR = pCB->GetSamplingRate();
		//All parameters that are sample rate dependant are corrected here.
		float multiplier = currentSR/44100.0f;

		timerSamples = Vals[0]*multiplier;
		slopeAmount = Vals[1]*44100.f/(8192.f*currentSR);
	}
	m_Timer = 0;
	if (muted) {
		muted = false;
		changing = true;
	}
}

void mi::Command() {
	pCB->MessBox("Made 18/5/2000 by Juan Antonio Arguelles Rius for Psycl3!","-=<([aRgUrU's G-o-a-s-l-i-c-e-r])>=-",0);
}

void mi::ParameterTweak(int par, int val) {
	Vals[par] = val;
	//All parameters that are sample rate dependant are corrected here.
	float multiplier = currentSR/44100.0f;
	timerSamples = Vals[0]*multiplier;
	slopeAmount = Vals[1]*44100.f/(8192.f*currentSR);
}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks) {
	if (m_Timer < timerSamples && m_Timer+numsamples >= timerSamples) {
		int diff = timerSamples - m_Timer;
		m_Timer+= diff;
		numsamples-=diff;
		if (changing) {
			while(m_CurrentVolume<0.99f && diff--) {
				(*psamplesleft)*=m_CurrentVolume; (*psamplesright)*=m_CurrentVolume;
				psamplesleft++; psamplesright++;
				m_CurrentVolume+=slopeAmount;
			}
		}
		//Add the remaining samples
		psamplesleft+=diff;
		psamplesright+=diff;
		changing = true;
		muted = true;
	}
	m_Timer+=numsamples;
	if (!changing) {
		if(!muted) return;
		while(numsamples--) *psamplesleft++ = *psamplesright++ = 0;
	} else if (muted){
		// mute enabled
		while(m_CurrentVolume>0.01f && numsamples--) {
			(*psamplesleft)*=m_CurrentVolume; (*psamplesright)*=m_CurrentVolume;
			psamplesleft++; psamplesright++;
			m_CurrentVolume-=slopeAmount;
		}
		if (m_CurrentVolume<=0.01f) {
			while(numsamples--) *psamplesleft++ = *psamplesright++ = 0;
			changing=false;
		}
	} else {
		// mute disabled
		while(m_CurrentVolume<0.99f && numsamples--) {
			(*psamplesleft)*=m_CurrentVolume; (*psamplesright)*=m_CurrentVolume;
			psamplesleft++; psamplesright++;
			m_CurrentVolume+=slopeAmount;
		}
		if (m_CurrentVolume>=0.99f) {
			changing=false;
		}
	}
}

bool mi::DescribeValue(char* txt,int const param, int const value) {
	float multiplier = currentSR/44100.0f;
	switch(param) {
		case 0: {
			int samp = value*multiplier;
			if (samp < pCB->GetTickLength()) {
				std::sprintf(txt, "%.02f ticks (%dms)", (float)samp/pCB->GetTickLength(), 1000*samp/currentSR);
			} else {
				std::sprintf(txt, "1 tick (%dms)", 1000*pCB->GetTickLength()/currentSR);
			}
			return true;
		}
		case 1: {
			int slopms = 8192000000/(value*currentSR);
			std::sprintf(txt, "%d mcs", slopms);
			return true;
		}
		default: return false; // returning false will simply show the value as a raw integral number
	}
}
}