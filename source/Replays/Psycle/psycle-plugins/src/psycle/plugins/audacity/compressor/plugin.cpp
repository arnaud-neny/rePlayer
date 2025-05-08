//////////////////////////////////////////////////////////////////////
// AudaCity Compressor plugin for PSYCLE by Sartorius
//
// Using Audacity's Compressor implementation

#include <psycle/plugin_interface.hpp>
#include "Compressor.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>

namespace psycle::plugins::audacity_compressor {

using namespace psycle::plugin_interface;

CMachineParameter const paraThreshold = {"Threshold","Threshold", -36 , -1 ,MPF_STATE, -12 };
CMachineParameter const paraRatio = {"Ratio","Ratio", 100 , 2000 ,MPF_STATE, 200 };
CMachineParameter const paraAttackTime = {"Attack","Attack", 10 , 10000 ,MPF_STATE, 200 };
CMachineParameter const paraDecayTime = {"Decay","Decay", 100 ,	10000 ,MPF_STATE, 1000 };
CMachineParameter const paraUseGain = {"Old Gain method","Old Gain method", 0 , 1 ,MPF_STATE, 0 };
CMachineParameter const paraNoiseFloor = {"Noise floor","Noise floor", -61 , -21 ,MPF_STATE, -40 };
CMachineParameter const paraUsePeak = {"Compressor method","Compressor method", 0 , 1 ,MPF_STATE, 0 };
CMachineParameter const *pParameters[] = {
	&paraThreshold,
	&paraRatio,
	&paraAttackTime,
	&paraDecayTime,
	&paraUseGain,
	&paraNoiseFloor,
	&paraUsePeak
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0120,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"Audacity Compressor"
		#ifndef NDEBUG
		" (Debug build)"
		#endif
		,
	"ACompressor",
	"Dominic Mazzoni/Sartorius/JosepMa",
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
		EffectCompressor sl,sr;
		int samplerate;
};

PSYCLE__PLUGIN__INSTANTIATOR("audacity-compressor", mi, MacInfo)

mi::mi() {
	Vals = new int[MacInfo.numParameters];
}

mi::~mi() {
	delete[] Vals;
}

void mi::Init() {
	samplerate = pCB->GetSamplingRate();
	sl.setSampleRate(samplerate);
	sl.Init(MAX_BUFFER_LENGTH);
	sr.setSampleRate(samplerate);
	sr.Init(MAX_BUFFER_LENGTH);
}

void mi::SequencerTick() {
	if(samplerate!=pCB->GetSamplingRate()) Init();
}

void mi::Command() {
	pCB->MessBox("Audacity Compressor","ACompressor",0);
}

void mi::ParameterTweak(int par, int val) {
	Vals[par]=val;
	switch(par) {
		case 0:
			sl.setThresholddB(val);
			sr.setThresholddB(val);
			break;
		case 1:
			if (val > MacInfo.Parameters[par]->MaxValue) {
				val = MacInfo.Parameters[par]->MaxValue;
				Vals[par] = val;
			}
			sl.setRatio(val*.01);
			sr.setRatio(val*.01);
			break;
		case 2: 
			sl.setAttackSec(val*.001);
			sr.setAttackSec(val*.001);
			break;
		case 3: 
			sl.setDecaySec(val*.001);
			sr.setDecaySec(val*.001);
			break;
		case 4:
			sl.setNormalize(val==1);
			sr.setNormalize(val==1);
			break;
		case 5:
			sl.setNoiseFloordB(val);
			sr.setNoiseFloordB(val);
			break;
		case 6:
			sl.setPeakAnalisys(val==1);
			sr.setPeakAnalisys(val==1);
			break;
		default:
			break;
	}
}

// Work... where all is cooked 
void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks) {
	sl.Process(psamplesleft, numsamples);
	sr.Process(psamplesright, numsamples);
}

// Function that describes value on client's displaying
bool mi::DescribeValue(char* txt,int const param, int const value) {
	switch(param) {
		case 0:
			{
				float vall = 20 * std::log10(sl.lastLevel());
				float valr = 20 * std::log10(sr.lastLevel());
				std::sprintf(txt,"%i dB (%+05.01f, %+05.01f)",value, vall, valr);
			}
			return true;
		case 5:
			std::sprintf(txt,"%i dB",value);
			return true;
		case 1:
			std::sprintf(txt,"%.1f:1",(float)value*.01f);
			return true;
		case 2:
		case 3:
			std::sprintf(txt,"%.02f s",(float)value*.001f);
			return true;
		case 4:
			std::sprintf(txt,value?"on":"off");
			return true;
		case 6:
			std::sprintf(txt,value?"Upward (to full scale)":"downward (to threshold)");
			return true;
		default:
			return false;
	}
}
}