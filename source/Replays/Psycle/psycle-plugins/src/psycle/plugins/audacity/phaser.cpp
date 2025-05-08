//////////////////////////////////////////////////////////////////////
// AudaCity Phaser effect plugin for PSYCLE by Sartorius
//
//   Original
/**********************************************************************

	Audacity: A Digital Audio Editor

	Phaser.cpp

	Effect programming:
	Nasca Octavian Paul

	UI programming:
	Dominic Mazzoni (with the help of wxDesigner)
	Vaughan Johnson (Preview)

**********************************************************************/
#include <psycle/plugin_interface.hpp>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>

#ifndef M_PI
	#define M_PI 3.14159265359f // note that's supposed to be a double!
#endif

namespace psycle::plugins::audacity_phaser {

using namespace psycle::plugin_interface;

CMachineParameter const paraLFOFreq       = { "LFO Freq"       , "LFOFreq"      ,    1,  40, MPF_STATE,  4 };
CMachineParameter const paraLFOStartPhase = { "LFO start phase", "LFOStartPhase",    0, 359, MPF_STATE,  0 };
CMachineParameter const paraDepth         = { "Depth"          , "Depth"        ,    0, 100, MPF_STATE, 70 };
CMachineParameter const paraStages        = { "Stages"         , "Stages"       ,    2,  24, MPF_STATE,  2 };
CMachineParameter const paraDryWet        = { "Dry/Wet"        , "Dry/Wet"      ,    0, 100, MPF_STATE, 50 };
CMachineParameter const paraFB            = { "Feedback"       , "Feedback"     , -100, 100, MPF_STATE,  0 };

CMachineParameter const * audacity_phaser_parameters[] = { 
	&paraLFOFreq,
	&paraLFOStartPhase,
	&paraDepth,
	&paraStages,
	&paraDryWet,
	&paraFB
};

CMachineInfo const audacity_phaser_info (
	MI_VERSION,
	0x0120,
	EFFECT,
	sizeof audacity_phaser_parameters / sizeof *audacity_phaser_parameters,
	audacity_phaser_parameters,
	"Audacity Phaser"
		#ifndef NDEBUG
			" (debug build)"
		#endif
	,
	"APhaser",
	"Nasca Octavian Paul/Sartorius",
	"About",
	1
);

class audacity_phaser : public CMachineInterface {
	public:
		audacity_phaser();
		virtual ~audacity_phaser();

		virtual void Init();
		virtual void SequencerTick();
		virtual void Work(float * samplesleft, float * samplesright, int numsamples, int tracks);
		virtual bool DescribeValue(char * txt, int const param, int const value);
		virtual void Command();
		virtual void ParameterTweak(int par, int val);

	private:
		unsigned int const static MAX_STAGES = 24;
		float const static phaser_lfo_shape;
		/// how many samples are processed before compute the lfo value again
		unsigned int const static lfo_skip_samples;
		
		float freq;
		float startphase;
		float fb;
		float depth;
		int stages;
		float drywet;

		// state variables
		unsigned long skipcount;
		float old_l[MAX_STAGES];
		float old_r[MAX_STAGES];
		float gain_l, gain_r;
		float fbout_l, fbout_r;
		float lfoskip;
		float phase_l, phase_r;
};

PSYCLE__PLUGIN__INSTANTIATOR("audacity-phaser", audacity_phaser, audacity_phaser_info)

float const audacity_phaser::phaser_lfo_shape = 4.0f;
/// how many samples are processed before compute the lfo value again
unsigned int const audacity_phaser::lfo_skip_samples = 20;

audacity_phaser::audacity_phaser() {
	Vals = new int[audacity_phaser_info.numParameters];
}

audacity_phaser::~audacity_phaser() {
	delete[] Vals;
}

void audacity_phaser::Init() {
	freq = 0.4f;
	startphase = 0;
	depth = .7f;

	lfoskip = freq * 2 * M_PI / pCB->GetSamplingRate();
	skipcount = 0;
	gain_l = gain_r = 0;
	fbout_l = fbout_r = 0;

	phase_l = startphase; phase_r = phase_l + M_PI;

	for(int j = 0; j < MAX_STAGES; ++j) {
		old_l[j] = 0;
		old_r[j] = 0;
	}
}

void audacity_phaser::SequencerTick() {
	lfoskip = freq * 2 * M_PI / pCB->GetSamplingRate();
}

void audacity_phaser::Command() {
	pCB->MessBox("Audacity Phaser", "APhaser", 0);
}

void audacity_phaser::ParameterTweak(int par, int val) {
	Vals[par]=val;
	switch(par) {
		case 0: freq = (float)val * .1f; lfoskip = freq * 2 * M_PI / pCB->GetSamplingRate(); break;
		case 1: startphase = float(val * M_PI / 180); phase_l = startphase; phase_r = phase_l + M_PI; break;
		case 2: depth = float(val * .01f);  break;
		case 3: stages = (val%2==1)?val-1:val; break;
		case 4: drywet = val * .01f; break;
		case 5: fb = val * .01f; break;
		default: ;
	}
}

// Function that describes value on client's displaying
bool audacity_phaser::DescribeValue(char * txt, int const param, int const value) {
	switch(param) {
		case 0: std::sprintf(txt,"%.1f Hz",(float)value*.1f); return true;
		case 1: std::sprintf(txt,"%i°",value); return true;
		case 2: std::sprintf(txt,"%i%%",value); return true;
		case 3: std::sprintf(txt,"%i",(value%2==1)?value-1:value); return true;
		case 4: std::sprintf(txt,"%i%%:%i%%",(100-value),value); return true;
		case 5: std::sprintf(txt,"%.2f",(float)value*0.01f); return true;
		default: return false;
	}
}

// Work... where all is cooked
void audacity_phaser::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks) {
	float static anti_denormal = 1.0e-20f;

	do {
		float in_l = *psamplesleft;
		float in_r = *psamplesright;

		float m_l = in_l + fbout_l * fb;
		float m_r = in_r + fbout_r * fb;

		if((skipcount++) % lfo_skip_samples == 0) {
			// compute sine between 0 and 1

			gain_l = (1 + std::cos(skipcount * lfoskip + phase_l)) / 2;
			// change lfo shape
			gain_l =(std::exp(gain_l * phaser_lfo_shape) - 1) / (std::exp(phaser_lfo_shape) - 1);
			gain_l = 1 - gain_l * depth; // attenuate the lfo

			gain_r = (1 + std::cos(skipcount * lfoskip + phase_r)) / 2;
			// change lfo shape
			gain_r =(std::exp(gain_r * phaser_lfo_shape) - 1) / (std::exp(phaser_lfo_shape) - 1);
			gain_r = 1 - gain_r * depth; // attenuate the lfo
		}

		// phasing routine
		for(int j = 0; j < stages; ++j) {
			float tmp;
			
			tmp = old_l[j];
			old_l[j] = gain_l * tmp + m_l + anti_denormal;
			m_l = tmp - gain_l * old_l[j];

			tmp = old_r[j];
			old_r[j] = gain_r * tmp + m_r + anti_denormal;
			m_r = tmp - gain_r * old_r[j];

			anti_denormal = -anti_denormal;
		}

		fbout_l = m_l;
		fbout_r = m_r;

		*psamplesleft  = m_l * drywet + in_l * (1 - drywet);
		*psamplesright = m_r * drywet + in_r * (1 - drywet);

		++psamplesleft;
		++psamplesright;

	} while(--numsamples);
}
}