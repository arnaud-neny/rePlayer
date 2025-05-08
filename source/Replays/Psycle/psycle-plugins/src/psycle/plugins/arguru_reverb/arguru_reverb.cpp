///\file
///\brief Arguru reverb plugin for PSYCLE
#include <psycle/plugin_interface.hpp>
#include "AllPass.hpp"
#include "CombFilter.hpp"
#include "LowPass.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cmath>

namespace psycle::plugins::arguru_reverb {

using namespace psycle::plugin_interface;

#define NALLS				12

CMachineParameter const paraCombdelay = {"Pre Delay","Pre Delay time", 1 , 32768 ,MPF_STATE,929};
CMachineParameter const paraCombseparator = {"Comb Spread","Comb Filter separation", 16 , 512 ,MPF_STATE,126};
CMachineParameter const paraAPdelay = {"Room size","Room size", 1 , 640 ,MPF_STATE,175};
CMachineParameter const paraAPg = {"Feedback","Feedback", 1 , 1024 ,MPF_STATE,1001};
CMachineParameter const paraCutoff = {"Absortion","Absortion", 1 , 22050 ,MPF_STATE,7059};
CMachineParameter const paraDry = {"Dry","Dry", 0 , 256 ,MPF_STATE,256};
CMachineParameter const paraWet = {"Wet","Wet", 0 , 256 ,MPF_STATE,128};
CMachineParameter const paraNC = {"Filters","Number of allpass filters", 0 , NALLS ,MPF_STATE,NALLS};

CMachineParameter const *pParameters[] = 
{ 
	&paraCombdelay,
	&paraCombseparator,
	&paraAPdelay,
	&paraAPg,
	&paraCutoff,
	&paraDry,
	&paraWet,
	&paraNC
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0120,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"Arguru Reverb"
	#ifndef NDEBUG
		" (Debug build)"
	#endif
	,
	"Reverb",
	"J. Arguelles",
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
		virtual void SetAll(int delay);
		CCombFilter comb;
		CAllPass all[NALLS];
		CLowpass fl;
		CLowpass fr;
		int prevfilters;
		int currentSR;
		float srFactor;
};

PSYCLE__PLUGIN__INSTANTIATOR("arguru-reverb", mi, MacInfo)

mi::mi(): prevfilters(0), currentSR(0), srFactor(0.0f) {
	Vals = new int[MacInfo.numParameters];
}

mi::~mi() {
	delete[] Vals;
}

void mi::Init() {
	currentSR = pCB->GetSamplingRate();
	srFactor = currentSR/44100.0f;
	comb.Initialize(currentSR,0,0);
	for(int c=0;c<NALLS;c++) all[c].Initialize(paraAPdelay.MaxValue*srFactor*(c+1)+(c*c),0,0);
}

void mi::SequencerTick() {
	if (currentSR != pCB->GetSamplingRate()) {
		currentSR = pCB->GetSamplingRate();
		srFactor = currentSR/44100.0f;
		comb.Initialize(currentSR, Vals[0]*srFactor,Vals[1]*srFactor);
		for(int c=0;c<NALLS;c++) all[c].Initialize(paraAPdelay.MaxValue*srFactor*(c+1)+(c*c),0,0);
		fl.setCutoff(Vals[4]*0.0000453514739229024943310657596371882*44100.0/currentSR);
		fr.setCutoff(Vals[4]*0.0000453514739229024943310657596371882*44100.0/currentSR);
	}
}

void mi::ParameterTweak(int par, int val) {
	Vals[par]=val;

	if(par < 2) {
		comb.SetDelay(Vals[0]*srFactor,Vals[1]*srFactor);
	} else if(par == 2) {
		SetAll(Vals[2]);
	} else if(par == 4) {
		fl.setCutoff(Vals[4]*0.0000453514739229024943310657596371882*44100.0/currentSR);
		fr.setCutoff(Vals[4]*0.0000453514739229024943310657596371882*44100.0/currentSR);
	} else if(par == 7) {
		for(int c=prevfilters;c<Vals[par];c++) all[c].Clear();
		prevfilters=std::max(Vals[par]-1, 0);
	}
}

void mi::Command() {
	pCB->MessBox("Made 31/5/2000 by Juan Antonio Arguelles Rius for Psycl3!","·-=<([aRgUrU's R3V3RB])>=-·",0);
}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks) {
	// Compute intermediate variables
	// This should be computed on ParameterTweak for optimization,
	// using global intermediate variables, but,
	// anyway, a few calcs doesnt take too CPU.

	float const g=(float)Vals[3]*0.0009765f;
	float const dry_amount				=(float)Vals[5]*0.00390625f;
	float const wet_amount				=(float)Vals[6]*0.00390625f;
	int const na=Vals[7];
	
	--psamplesleft;
	--psamplesright;

	do {
		float const sl = *++psamplesleft;
		float const sr = *++psamplesright;
		
		comb.Work(sl,sr);
		float l_revresult=comb.left_output();
		float r_revresult=comb.right_output();
		
		for(int c=0;c<na;c++) {
			all[c].Work(l_revresult,r_revresult,g);
			l_revresult=all[c].left_output();
			r_revresult=all[c].right_output();
		}

		*psamplesleft=sl*dry_amount+fl.Process(l_revresult)*wet_amount;
		*psamplesright=sr*dry_amount+fr.Process(r_revresult)*wet_amount;

	} while(--numsamples);
	
}

bool mi::DescribeValue(char* txt,int const param, int const value) {

	switch(param){
		case 0:	//fallthrough
		case 1: std::sprintf(txt,"%.1f ms.",(float)value*0.0226757f); return true;
		case 2:	std::sprintf(txt,"%.1f mtrs.",(float)value*0.17f); return true;
		case 3: std::sprintf(txt,"%.1f%%",(float)value*0.0976562f);	return true;
		case 4: std::sprintf(txt,"%d Hzs.",value); return true;
		case 5: //fallthrough
		case 6:	std::sprintf(txt,"%.1f%%",float(value)*0.390625f); return true;
		case 7: //fallthrough
		default: return false;
	};
}

void mi::SetAll(int delay) {
	const int delaySamples = delay*srFactor;
	for(int c=0;c<NALLS;c++) all[c].SetDelay(delaySamples*(c+1)+(c*c), c*1.3f);
}
}