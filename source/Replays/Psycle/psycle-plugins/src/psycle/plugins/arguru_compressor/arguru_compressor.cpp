///\file
///\brief Arguru compressor plugin for PSYCLE
/// Reverse engineered by JosepMa from the dissasembled sources

#include <psycle/plugin_interface.hpp>
#include <psycle/helpers/math.hpp>
#include <universalis/stdlib/cstdint.hpp>
#include <cstdio> // for std::sprintf

namespace psycle::plugins::arguru_compressor {

using namespace psycle::plugin_interface;
using namespace psycle::helpers::math;
using namespace universalis::stdlib;

CMachineParameter const paraGain = {"Input Gain","Input Gain",0,128,MPF_STATE, 0};
CMachineParameter const paraThreshold = {"Threshold","Threshold",0,128,MPF_STATE,64};
CMachineParameter const paraRatio = {"Ratio","Ratio",0,16,MPF_STATE,16};
CMachineParameter const paraAttack = {"Attack","Attack",0,128,MPF_STATE,6};
CMachineParameter const paraRelease = {"Release","Release",0,128,MPF_STATE,0x2C};
CMachineParameter const paraClip = {"Soft clip","Soft clip",0,1,MPF_STATE,0};

enum { paramGain = 0, paramThreshold, paramRatio, paramAttack, paramRelease, paramClip };

CMachineParameter const *pParameters[] = { 
	&paraGain,
	&paraThreshold,
	&paraRatio,
	&paraAttack,
	&paraRelease,
	&paraClip
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0120,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"Arguru Compressor"
	#ifndef NDEBUG
		" (debug build)"
	#endif
	,
	"Compressor",
	"J. Arguelles & psycledelics",
	"About",
	1
);

class mi : public CMachineInterface {
	public:
		mi();
		~mi();
	
		/*override*/ void Init();
		/*override*/ void SequencerTick();
		/*override*/ void Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks);
		/*override*/ bool DescribeValue(char* txt,int const param, int const value);
		/*override*/ void Command();
		/*override*/ void ParameterTweak(int par, int val);
	private:
		float currentGain;
		//float targetGain;
		//float gainStep;
		int32_t currentSR;
		//enum action { none, actAttack, actRelease} currentAction;
	
};

PSYCLE__PLUGIN__INSTANTIATOR("arguru-compressor", mi, MacInfo)

mi::mi()
:
	currentGain(1.0f),
	//targetGain(1.0f),
	//gainStep(0.0f),
	currentSR(0)
	//, currentAction(none)
{
	Vals = new int[MacInfo.numParameters];
}

mi::~mi() {
	delete[] Vals;
}

void mi::Init() {
	currentSR = pCB->GetSamplingRate();
}

void mi::SequencerTick() {
	if (currentSR != pCB->GetSamplingRate()) {
		currentSR = pCB->GetSamplingRate();
	}
}

void mi::Command(){
	pCB->MessBox("By Arguru <arguru@smartelectronix.com>","Arguru compressor",0);
}

void mi::ParameterTweak(int par, int val) {
	Vals[par] = val;
}


void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks) {

	float const corrected_gain =  (Vals[paramGain]*0.015625000f+1.0f)*0.000030517578125f;
	float* pleft = psamplesleft;
	float* pright = psamplesright;

	for (int cont = numsamples; cont > 0; cont--) {
		*(pleft++) *= corrected_gain;
		*(pright++) *= corrected_gain;
	}

	if (Vals[paramRatio] != 0) {
		float const correctedthreshold = Vals[paramThreshold]*0.0078125000f;
		double const corrected_ratio = (Vals[paramRatio] <16)? 1.0f/(1.0f + Vals[paramRatio]) : 0.0f;
		double const attackconst = 1.0/((1.0+Vals[paramAttack])*currentSR*0.001);
		double const releaseconst = 1.0/((1.0+Vals[paramRelease])*currentSR*0.001);
		
		float* pleft = psamplesleft;
		float* pright = psamplesright;

		for (int cont = numsamples; cont > 0; cont--) {
			double targetGain;
			double const analyzedValue = std::max(fabs(*pleft),fabs(*pright));
			if(analyzedValue <= correctedthreshold) {
				targetGain = 1.0f;
			}
			else {
				targetGain = ((analyzedValue - correctedthreshold)*corrected_ratio+correctedthreshold)/analyzedValue;
			}
			double newgain = (targetGain - currentGain);
			if (targetGain < currentGain) {
				newgain*=attackconst;
			}
			else {
				newgain*=releaseconst;
			}

			currentGain += newgain;
			*(pleft++) *= currentGain;
			*(pright++) *= currentGain;
		}
	}

	if (Vals[paramClip] != 0) {
		float* pleft = psamplesleft;
		float* pright = psamplesright;
		for (int cont = numsamples; cont > 0; cont--) {
			*pleft =tanh(*pleft);
			*pright =tanh(*pright);
			pleft++;
			pright++;
		}
	}
	for (int cont = numsamples; cont > 0; cont--) {
		*(psamplesleft++) *= 32768.0f;
		*(psamplesright++) *= 32768.0f;
	}
}

bool mi::DescribeValue(char* txt,int const param, int const value) {
	switch(param) {
	case paramGain:
		std::sprintf(txt,"+%.1f dB", linear_to_deci_bell(1 + value * 0.015625f));
		return true;
	case paramThreshold:
		if(value == 0)
			std::sprintf(txt,"-inf. dB");
		else
				std::sprintf(txt,"%.1f dB", linear_to_deci_bell(value * 0.0078125f));
		return true;
	case paramRatio:
		if(value >= 16)
			std::sprintf(txt,"1:inf (Limiter)");
		else if(value == 0)
			std::sprintf(txt,"1:1 (Bypass)");
		else
			std::sprintf(txt,"1:%d", value + 1);
		return true;
	case paramAttack: //fallthrough
	case paramRelease:
		std::sprintf(txt,"%d ms.", value);
		return true;
	case paramClip:
			std::sprintf(txt, value == 0 ? "Off" : "On");
			return true;
	default: return false;
	}
}
}