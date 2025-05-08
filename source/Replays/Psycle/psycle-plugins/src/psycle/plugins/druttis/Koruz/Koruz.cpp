//============================================================================
//
//				Koruz.cpp
//				----------
//				druttis@darkface.pp.se
//
//============================================================================
#include <psycle/plugin_interface.hpp>
#include "../dsp/CDsp.h"
#include "../dsp/Chorus.h"
#include "../dsp/Phaser.h"
#include <cstring>
#include <cmath>
#include <cstdio>

namespace psycle::plugins::druttis::koruz {
using namespace psycle::plugin_interface;

//============================================================================
//				Defines
//============================================================================
#define MAC_NAME				"Koruz"
int const MAC_VERSION  = 0x0110;
#define MAC_AUTHOR				"Druttis"
//============================================================================
//				Wavetable
//============================================================================
#define SINE(x) ((float) sin((float) (x) * PI2))
#define WAVESIZE 4096
#define WAVEMASK 4095
float				wavetable[WAVESIZE];
//============================================================================
//				Parameters
//============================================================================
#define PARAM_PHASER_MIN 0
CMachineParameter const paramPhaserMin = { "Phaser Min.", "Phaser Min.", 0, 256, MPF_STATE, 8 };
#define PARAM_PHASER_MAX 1
CMachineParameter const paramPhaserMax = { "Phaser Max.", "Phaser Max.", 0, 256, MPF_STATE, 80 };
#define PARAM_PHASER_RATE 2
CMachineParameter const paramPhaserRate = { "Phaser Rate", "Phaser Rate", 0, 256, MPF_STATE, 16 };
#define PARAM_PHASER_FB 3
CMachineParameter const paramPhaserFB = { "Phaser F.B.", "Phaser F.B.", 0, 256, MPF_STATE, 192 };
#define PARAM_PHASER_DEPTH 4
CMachineParameter const paramPhaserDepth = { "Phaser Depth", "Phaser Depth", 0, 256, MPF_STATE, 128 };
#define PARAM_PHASER_PHASE 5
CMachineParameter const paramPhaserPhase = { "Phaser Phase", "Phaser Phase", 0, 256, MPF_STATE, 128 };
#define PARAM_PHASER_SWIRL 6
CMachineParameter const paramPhaserSwirl = { "Phaser Swirl", "Phaser Swirl", 0, 256, MPF_STATE, 192 };

#define PARAM_CHORUS_DELAY 7
CMachineParameter const paramChorusDelay = { "Chorus Delay", "Chorus Delay", 0, 256, MPF_STATE, 32 };
#define PARAM_CHORUS_DEPTH 8
CMachineParameter const paramChorusDepth = { "Chorus Depth", "Chorus Depth", 0, 256, MPF_STATE, 32 };
#define PARAM_CHORUS_RATE 9
CMachineParameter const paramChorusRate = { "Chorus Rate", "Chorus Rate", 0, 256, MPF_STATE, 16 };
#define PARAM_CHORUS_FB 10
CMachineParameter const paramChorusFB = { "Chorus F.B.", "Chorus F.B.", 0, 256, MPF_STATE, 0 };
#define PARAM_CHORUS_MIX 11
CMachineParameter const paramChorusMix = { "Chorus Mix", "Chorus Mix", 0, 256, MPF_STATE, 128 };
#define PARAM_CHORUS_PHASE 12
CMachineParameter const paramChorusPhase = { "Chorus Phase", "Chorus Phase", 0, 256, MPF_STATE, 128 };
#define PARAM_CHORUS_SWIRL 13
CMachineParameter const paramChorusSwirl = { "Chorus Swirl", "Chorus Swirl", 0, 256, MPF_STATE, 192 };

//============================================================================
//				Parameter list
//============================================================================

CMachineParameter const *pParams[] =
{
	&paramPhaserMin,
	&paramPhaserMax,
	&paramPhaserRate,
	&paramPhaserFB,
	&paramPhaserDepth,
	&paramPhaserPhase,
	&paramPhaserSwirl,
	&paramChorusDelay,
	&paramChorusDepth,
	&paramChorusRate,
	&paramChorusFB,
	&paramChorusMix,
	&paramChorusPhase,
	&paramChorusSwirl
};

//============================================================================
//				Machine info
//============================================================================
CMachineInfo const MacInfo (
	MI_VERSION,
	MAC_VERSION,
	EFFECT,
	sizeof pParams / sizeof *pParams,
	pParams,
#ifdef _DEBUG
	MAC_NAME " (Debug)",
#else
	MAC_NAME,
#endif
	MAC_NAME,
	MAC_AUTHOR " on " __DATE__,
	"Command Help",
	2
);

//============================================================================
//				Machine
//============================================================================
class mi : public CMachineInterface
{
public:
	mi();
	virtual ~mi();
	virtual void Init();
	virtual void Stop();
	virtual void Command();
	virtual void ParameterTweak(int par, int val);
	virtual bool DescribeValue(char* txt,int const param, int const value);
	virtual void SequencerTick();
	virtual void SeqTick(int channel, int note, int ins, int cmd, int val);
	virtual void Work(float *psamplesleft, float* psamplesright, int numsamples, int numtracks);
public:
	int currentSR;

	//				Phaser
	float				phaser_min;
	float				phaser_max;
	float				phaser_increment;
	Phaser				phaser_left;
	Phaser				phaser_right;
	float				phaser_left_phase;
	float				phaser_right_phase;
	float				phaser_swirl;

	//				Chorus
	float				chorus_delay;
	float				chorus_depth;
	float				chorus_increment;
	Chorus				chorus_left;
	Chorus				chorus_right;
	float				chorus_left_phase;
	float				chorus_right_phase;
	float				chorus_swirl;
};

PSYCLE__PLUGIN__INSTANTIATOR("koruz", mi, MacInfo)

//////////////////////////////////////////////////////////////////////
//
//				Constructor
//
//////////////////////////////////////////////////////////////////////

mi::mi()
{
	Vals = new int[MacInfo.numParameters];
	//
	int i;
	float t;
	for (i = 0; i < WAVESIZE; i++) {
		t = (float) i / (float) WAVESIZE;
		//				Sine
		wavetable[i] = (float) sin(t * PI2);
	}
}

//////////////////////////////////////////////////////////////////////
//
//				Destructor
//
//////////////////////////////////////////////////////////////////////

mi::~mi()
{
	delete[] Vals;
}

//////////////////////////////////////////////////////////////////////
//
//				Init
//
//////////////////////////////////////////////////////////////////////

void mi::Init()
{
	//
	//
	phaser_left.Init();
	phaser_right.Init();
	phaser_left_phase = 0.0f;
	phaser_right_phase = 0.0f;
	//
	//
	chorus_left.Init(2047);
	chorus_right.Init(2047);
	chorus_left_phase = 0.0f;
	chorus_right_phase = 0.0f;
	currentSR = pCB->GetSamplingRate();
}

//////////////////////////////////////////////////////////////////////
//
//				Stop
//
//////////////////////////////////////////////////////////////////////

void mi::Stop()
{
}

//////////////////////////////////////////////////////////////////////
//
//				Command
//
//////////////////////////////////////////////////////////////////////

void mi::Command()
{
	pCB->MessBox(
		"Koruz project\n\n"
		"Greetz to all psycle doods!\n\n"
		"---------------------------\n"
		"druttis@darkface.pp.se\n",
		MAC_AUTHOR " " MAC_NAME,
		0
	);
}

//////////////////////////////////////////////////////////////////////
//
//				ParameterTweak
//
//////////////////////////////////////////////////////////////////////

void mi::ParameterTweak(int par, int val)
{
	float tmp;

	Vals[par] = val;

	switch (par)
	{
		case PARAM_PHASER_MIN:
			phaser_min = (float) val * 128.0f / (float)currentSR;
			break;
		case PARAM_PHASER_MAX:
			phaser_max = (float) val * 128.0f / (float)currentSR;
			break;
		case PARAM_PHASER_RATE:
			phaser_increment = (float) (val * WAVESIZE) / 51.2f / (float)currentSR;
			break;
		case PARAM_PHASER_FB:
			tmp = (float) val / 256.0f;
			phaser_left.SetFeedback(tmp);
			phaser_right.SetFeedback(tmp);
			break;
		case PARAM_PHASER_DEPTH:
			tmp = (float) val / 512.0f;
			phaser_left.SetDepth(tmp);
			phaser_right.SetDepth(tmp);
			break;
		case PARAM_PHASER_PHASE:
			phaser_right_phase = phaser_left_phase + (float) val * (float) WAVEMASK / 512.0f;
			break;
		case PARAM_PHASER_SWIRL:
			phaser_swirl = (float) pow(2.0f, (float) val / 128.0f - 1.0f);
			break;
		case PARAM_CHORUS_DELAY:
			chorus_delay = (float) val * 0.0128f * (float)currentSR;
			break;
		case PARAM_CHORUS_DEPTH:
			chorus_depth = (float) val * 4.0f;
			break;
		case PARAM_CHORUS_RATE:
			chorus_increment = (float) (val * WAVESIZE) / 51.2f / (float)currentSR;
			break;
		case PARAM_CHORUS_FB:
			tmp = (float) val / 256.0f;
			chorus_left.SetFeedback(tmp);
			chorus_right.SetFeedback(tmp);
			break;
		case PARAM_CHORUS_MIX:
			tmp = (float) val / 512.0f;
			chorus_left.SetMix(tmp);
			chorus_right.SetMix(tmp);
			break;
		case PARAM_CHORUS_PHASE:
			chorus_right_phase = chorus_left_phase + (float) val * (float) WAVEMASK / 512.0f;
			break;
		case PARAM_CHORUS_SWIRL:
			chorus_swirl = (float) pow(2.0f, (float) val / 128.0f - 1.0f);
			break;
	}
}

//////////////////////////////////////////////////////////////////////
//
//				DescribeValue
//
//////////////////////////////////////////////////////////////////////

bool mi::DescribeValue(char* txt,int const param, int const value) {

	switch (param)
	{
		case PARAM_PHASER_MIN:
		case PARAM_PHASER_MAX:
			sprintf(txt, "%d Hz", (value * 64));
			break;
		case PARAM_PHASER_RATE:
			sprintf(txt, "%.2f Hz", ((float) value / 51.2f));
			break;
		case PARAM_PHASER_FB:
			sprintf(txt, "%.2f %%", ((float) value * 100.0f / 256.0f));
			break;
		case PARAM_PHASER_DEPTH:
			sprintf(txt, "%.2f %%", ((float) value * 100.0f / 256.0f));
			break;
		case PARAM_PHASER_PHASE:
			sprintf(txt, "%f deg.", (float) value * 0.3515625f);
			break;
		case PARAM_PHASER_SWIRL:
			sprintf(txt, "%.2f %%", (float) value / 1.28f - 100.0f);
			break;
		case PARAM_CHORUS_DELAY:
			sprintf(txt, "%.2f ms", ((float) value / 6.4f));
			break;
		case PARAM_CHORUS_DEPTH:
			sprintf(txt, "%.2f %%", ((float) value * 100.0f / 256.0f));
			break;
		case PARAM_CHORUS_RATE:
			sprintf(txt, "%.2f Hz", ((float) value / 51.2f));
			break;
		case PARAM_CHORUS_FB:
			sprintf(txt, "%.2f %%", ((float) value * 100.0f / 256.0f));
			break;
		case PARAM_CHORUS_MIX:
			sprintf(txt, "%.2f %%", ((float) value * 100.0f / 256.0f));
			break;
		case PARAM_CHORUS_PHASE:
			sprintf(txt, "%f deg.", (float) value * 0.3515625f);
			break;
		case PARAM_CHORUS_SWIRL:
			sprintf(txt, "%.2f %%", (float) value / 1.28f - 100.0f);
			break;
		default:
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////
//
//				SequencerTick
//
//////////////////////////////////////////////////////////////////////

void mi::SequencerTick()
{
	if(currentSR != pCB->GetSamplingRate())
	{
		currentSR = pCB->GetSamplingRate();
		phaser_min = (float) Vals[PARAM_PHASER_MIN] * 128.0f / (float)currentSR;
		phaser_max = (float) Vals[PARAM_PHASER_MAX] * 128.0f / (float)currentSR;
		phaser_increment = (float) (Vals[PARAM_PHASER_RATE] * WAVESIZE) / 51.2f / (float)currentSR;
		chorus_delay = (float) Vals[PARAM_CHORUS_DELAY] * 0.0128f * (float)currentSR;
		chorus_increment = (float) (Vals[PARAM_CHORUS_RATE] * WAVESIZE) / 51.2f / (float)currentSR;
	}
}

//////////////////////////////////////////////////////////////////////
//
//				SequencerTick
//
//////////////////////////////////////////////////////////////////////

void mi::SeqTick(int channel, int note, int ins, int cmd, int val)
{
	//				Insert code here for synths.
}

//////////////////////////////////////////////////////////////////////
//
//				Work
//
//////////////////////////////////////////////////////////////////////

void mi::Work(float *psamplesleft, float* psamplesright, int numsamples, int numtracks)
{
	float rnd;
	--psamplesleft;
	--psamplesright;
	do
	{
		//////////////////////////////////////////////////////////
		//				Update phaser
		//////////////////////////////////////////////////////////

		phaser_left.SetDelay(phaser_min + (phaser_max - phaser_min) * (1.0f + get_sample_l(wavetable, phaser_left_phase, WAVEMASK)) * 0.5f);
		phaser_right.SetDelay(phaser_min + (phaser_max - phaser_min) * (1.0f + get_sample_l(wavetable, phaser_right_phase, WAVEMASK)) * 0.5f);
		phaser_left_phase = fand(phaser_left_phase + phaser_increment, WAVEMASK);
		phaser_right_phase = fand(phaser_right_phase + phaser_increment * phaser_swirl, WAVEMASK);

		//////////////////////////////////////////////////////////
		//				Update chorus
		//////////////////////////////////////////////////////////

		chorus_left.SetDelay(chorus_delay + chorus_depth * (1.0f + get_sample_l(wavetable, chorus_left_phase, WAVEMASK)) * 0.5f);
		chorus_right.SetDelay(chorus_delay + chorus_depth * (1.0f + get_sample_l(wavetable, chorus_right_phase, WAVEMASK)) * 0.5f);
		chorus_left_phase = fand(chorus_left_phase + chorus_increment, WAVEMASK);
		chorus_right_phase = fand(chorus_right_phase + chorus_increment * chorus_swirl, WAVEMASK);
		
		//////////////////////////////////////////////////////////
		// To prevent phaser (0) calls.
		//////////////////////////////////////////////////////////

		rnd = CDsp::GetRandomSignal() * 0.001f;

		//////////////////////////////////////////////////////////
		//				Left
		//////////////////////////////////////////////////////////

		++psamplesleft;
		*psamplesleft = phaser_left.Tick(chorus_left.Tick(*psamplesleft + rnd));

		//////////////////////////////////////////////////////////
		//				Right
		//////////////////////////////////////////////////////////

		++psamplesright;
		*psamplesright = phaser_right.Tick(chorus_right.Tick(*psamplesright + rnd));
	}
	while (--numsamples);
}
}