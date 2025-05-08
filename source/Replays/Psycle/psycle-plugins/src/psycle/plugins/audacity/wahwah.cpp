//////////////////////////////////////////////////////////////////////
// AudaCity WahWah effect plugin for PSYCLE by Sartorius
//
//   Original
/**********************************************************************

	Audacity: A Digital Audio Editor

	Wahwah.cpp

	Effect programming:
	Nasca Octavian Paul

	UI programming:
	Dominic Mazzoni (with the help of wxDesigner)
	Vaughan Johnson (Preview)

**********************************************************************/

#include <psycle/plugin_interface.hpp>
#include <psycle/helpers/math.hpp>
#include <universalis/stdlib/cstdint.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace psycle::plugins::audacity_wahwah {

using namespace psycle::plugin_interface;
using namespace psycle::helpers::math;
using namespace universalis::stdlib;

uint32_t const LFO_SKIP_SAMPLES = 30;

#ifndef M_PI
	#define M_PI 3.14159265359f
#endif

CMachineParameter const paraLFOFreq = {"LFO Freq", "LFOFreq", 1, 100, MPF_STATE, 15 };
CMachineParameter const paraLFOStartPhase = {"LFO start phase", "LFOStartPhase", 0 , 359, MPF_STATE, 0 };
CMachineParameter const paraDepth = {"Depth", "Depth", 0, 100, MPF_STATE,70};
CMachineParameter const paraResonance = {"Resonance", "Resonance", 1, 100, MPF_STATE, 25 };
CMachineParameter const paraWahFreqOff = {"Wah freq offset", "WahFreqOff", 0, 100, MPF_STATE, 30 };

CMachineParameter const *pParameters[] = { 
	&paraLFOFreq,
	&paraLFOStartPhase,
	&paraDepth,
	&paraResonance,
	&paraWahFreqOff
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0120,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"Audacity WahWah"
	#ifndef NDEBUG
		" (Debug build)"
	#endif
	,
	"WahWah",
	"Nasca Octavian Paul/Sartorius",
	"About",
	1
);


class mi : public CMachineInterface
{
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
	inline void RecalcFilter( const float depth_mul_1_minus_freqofs );
	float phase;
	float lfoskip;
	unsigned int skipcount;
	float xn1_l, xn2_l, yn1_l, yn2_l;
	float xn1_r, xn2_r, yn1_r, yn2_r;
	float b0_l, b1_l, b2_l, a0_l, a1_l, a2_l;
	float b0_r, b1_r, b2_r, a0_r, a1_r, a2_r;
	float freq;
	float depth, freqofs, res;
	float sample_rate_factor;

};

PSYCLE__PLUGIN__INSTANTIATOR("wahwah", mi, MacInfo)

mi::mi() {
	Vals = new int[MacInfo.numParameters];
}

mi::~mi() {
	delete[] Vals;
}

void mi::Init() {
	freq = 1.5f;
	phase = 0;
	depth = .7f;
	freqofs = .3f;
	res = 2.5f;

	lfoskip = freq * 2 * M_PI / pCB->GetSamplingRate();
	skipcount = 0;
	xn1_l = 0;
	xn2_l = 0;
	yn1_l = 0;
	yn2_l = 0;

	xn1_r = 0;
	xn2_r = 0;
	yn1_r = 0;
	yn2_r = 0;

	b0_l = 0;
	b1_l = 0;
	b2_l = 0;
	a0_l = 0;
	a1_l = 0;
	a2_l = 0;

	b0_r = 0;
	b1_r = 0;
	b2_r = 0;
	a0_r = 0;
	a1_r = 0;
	a2_r = 0;
	sample_rate_factor = 44100.0f/pCB->GetSamplingRate();
}

void mi::SequencerTick(){
	lfoskip = freq * 2 * M_PI / pCB->GetSamplingRate();
	sample_rate_factor = 44100.0f/pCB->GetSamplingRate();
}

void mi::Command(){
	pCB->MessBox("Audacity WahWah","WahWah",0);
}

void mi::ParameterTweak(int par, int val)
{
	Vals[par]=val;
	switch(par)
	{
		case 0: freq = float(val) * .1f; lfoskip = freq * 2 * M_PI / pCB->GetSamplingRate(); break;
		case 1: phase = float(val)  * (0.0055555555555555555555555555555556f * M_PI); break;
		case 2: depth = float(val) * .01f;  break;
		case 3: res = 1.f / ( float(val) * .2f);  break;
		case 4:
			{
				if (val == paraWahFreqOff.MaxValue) {
					// freqofs = 1 causes high ressonance at cutoff frequency.
					freqofs = 0.9999f;
				}
				else {
					freqofs = float(val) * .01f;
				}
			}
			 break;
		default:
				break;
	}
}
inline void mi::RecalcFilter( const float depth_mul_1_minus_freqofs )
{
	float sintime, costime;
	float calc_1_time = float(skipcount) * lfoskip + phase;
	//Ensure proper values.
	if (calc_1_time > 4*M_PI) {
		skipcount-=pCB->GetSamplingRate()/freq;
	}
	sincos(calc_1_time, sintime, costime);
	{
		float frequency, omega, sn, cs, alpha;
		frequency = 1.f + costime; // Left channel
		frequency = frequency * depth_mul_1_minus_freqofs + freqofs;
		frequency = exp((frequency - 1.f) * 6.f);
		omega = M_PI * frequency * sample_rate_factor;
#if 0
		if (omega > 2*M_PI) do {
			omega = omega - 2*M_PI;
		} while(omega > 2*M_PI); 
#endif

		sincos(omega, sn, cs);
		alpha = sn * res;
		//b0_l = (1 - cs) * .5f;
		b1_l = 1.f - cs;
		b2_l = b0_l = b1_l * .5f;
		a0_l = 1.f + alpha;
		a1_l = -2.f * cs;
		a2_l = 1.f - alpha;
	}
	{
		float frequency, omega, sn, cs, alpha;
		frequency = 1.f - sintime; // Right channel
		frequency = frequency * depth_mul_1_minus_freqofs + freqofs;
		frequency = exp((frequency - 1.f) * 6.f);
		omega = M_PI * frequency * sample_rate_factor;
#if 0
		if (omega > 2*M_PI) do {
			omega = omega - 2*M_PI;
		} while(omega > 2*M_PI); 
#endif
		sincos(omega, sn, cs);

		alpha = sn * res;
		//b0_r = (1 - cs) * .5f;
		b1_r = 1.f - cs;
		b2_r = b0_r = b1_r * .5f;
		a0_r = 1.f + alpha;
		a1_r = -2.f * cs;
		a2_r = 1.f - alpha;
	}
}

// Work... where all is cooked 
void mi::Work(float *psamplesleft, float *psamplesright , int numsamples_in, int tracks)
{
	uint32_t numsamples = static_cast<uint32_t>(numsamples_in);
	const float depth_mul_1_minus_freqofs = depth * (1.f - freqofs) * .5f;

	if (skipcount == 0) {
		RecalcFilter(depth_mul_1_minus_freqofs);
	}
	do {
		const float recip_l = a0_l / (a0_l * a0_r);
		const float recip_r = a0_r / (a0_l * a0_r);
		uint32_t cont = std::min(LFO_SKIP_SAMPLES - (skipcount % LFO_SKIP_SAMPLES), numsamples);
		skipcount+=cont;
		numsamples-=cont;
		while (cont--) {
			// There is no need to normalize the input/output to [-1.0..1.0]
			const float in_l = *psamplesleft;
			const float in_r = *psamplesright;
			
			float out_l = (b0_l * in_l + b1_l * xn1_l + b2_l * xn2_l - a1_l * yn1_l - a2_l * yn2_l) * recip_r;
			erase_all_nans_infinities_and_denormals(out_l);
			xn2_l = xn1_l;
			xn1_l = in_l;
			yn2_l = yn1_l;
			yn1_l = out_l;

			float out_r = (b0_r * in_r + b1_r * xn1_r + b2_r * xn2_r - a1_r * yn1_r - a2_r * yn2_r) * recip_l;
			erase_all_nans_infinities_and_denormals(out_r); 
			xn2_r = xn1_r;
			xn1_r = in_r;
			yn2_r = yn1_r;
			yn1_r = out_r;

			*psamplesleft = out_l;
			*psamplesright = out_r;

			++psamplesleft;
			++psamplesright;
		};
		RecalcFilter(depth_mul_1_minus_freqofs);
	} while(numsamples);
}

// Function that describes value on client's displaying
bool mi::DescribeValue(char* txt,int const param, int const value)
{
	switch(param)
	{
		case 0:
			std::sprintf(txt,"%.1f Hz",(float)value*.1f);
			return true;
		case 1:
			std::sprintf(txt,"%i°",value);
			return true;
		case 2:
			std::sprintf(txt,"%i%%",value);
			return true;
		case 3:
			std::sprintf(txt,"%.1f",(float)value*.1f);
			return true;
		case 4:
			std::sprintf(txt,"%.0f Hz",exp((value*0.01f - 1.f) * 6.f)*22050);
			return true;
		default:
			return false;
	}
}
}