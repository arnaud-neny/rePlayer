//============================================================================
//
//				WGFlute.cpp
//				----------
//				druttis@darkface.pp.se
//
//============================================================================
#include "CVoice.h"
#include <psycle/plugin_interface.hpp>
#include <cstdio>
#include <memory>
namespace psycle::plugins::druttis::phantom {
using namespace psycle::plugin_interface;

//============================================================================
//				Defines
//============================================================================
#define MAC_NAME				"Phantom"
#define MAC_VERSION				"1.2"
int const IMAC_VERSION = 0x0120;
#define MAC_AUTHOR				"Druttis"
#define	MAX_VOICES				2
#define NUM_TICKS				32
//============================================================================
//				Wavetable
//============================================================================
#define SINE(x) ((float) sin((float) (x) * PI2))
float				wavetable[NUMWAVEFORMS][WAVESIZE];
char const *wave_str[NUMWAVEFORMS] = {"Sine","Triangle","Square", "Saw. up",
					"Saw. down", "Sine->Saw.", "Square-Sine", "Sine->Blank"};
char const			*filt_str[5] = { "LP-12", "LP-24", "HP-12", "HP-24", "LP-36" };
//============================================================================
//				Parameters
//============================================================================
#define PARAM_ROWS 11

#define PARAM_OSC1_WAVE 0
CMachineParameter const paramOsc1Wave = { "OSC-1 Wave", "OSC-1 Wave", 1, NUMWAVEFORMS, MPF_STATE, 6 };
#define PARAM_OSC2_WAVE (PARAM_OSC1_WAVE + 1)
CMachineParameter const paramOsc2Wave = { "OSC-2 Wave", "OSC-2 Wave", 1, NUMWAVEFORMS, MPF_STATE, 6 };
#define PARAM_OSC3_WAVE (PARAM_OSC2_WAVE + 1)
CMachineParameter const paramOsc3Wave = { "OSC-3 Wave", "OSC-3 Wave", 1, NUMWAVEFORMS, MPF_STATE, 6 };
#define PARAM_OSC4_WAVE (PARAM_OSC3_WAVE + 1)
CMachineParameter const paramOsc4Wave = { "OSC-4 Wave", "OSC-4 Wave", 1, NUMWAVEFORMS, MPF_STATE, 6 };
#define PARAM_OSC5_WAVE (PARAM_OSC4_WAVE + 1)
CMachineParameter const paramOsc5Wave = { "OSC-5 Wave", "OSC-5 Wave", 1, NUMWAVEFORMS, MPF_STATE, 6 };
#define PARAM_OSC6_WAVE (PARAM_OSC5_WAVE + 1)
CMachineParameter const paramOsc6Wave = { "OSC-6 Wave", "OSC-6 Wave", 1, NUMWAVEFORMS, MPF_STATE, 6 };

#define PARAM_OSC1_PHASE (PARAM_OSC1_WAVE + PARAM_ROWS)
CMachineParameter const paramOsc1Phase = { "OSC-1 Phase", "OSC-1 Phase", 0, 255, MPF_STATE, 0 };
#define PARAM_OSC2_PHASE (PARAM_OSC1_PHASE + 1)
CMachineParameter const paramOsc2Phase = { "OSC-2 Phase", "OSC-2 Phase", 0, 255, MPF_STATE, 0 };
#define PARAM_OSC3_PHASE (PARAM_OSC2_PHASE + 1)
CMachineParameter const paramOsc3Phase = { "OSC-3 Phase", "OSC-3 Phase", 0, 255, MPF_STATE, 0 };
#define PARAM_OSC4_PHASE (PARAM_OSC3_PHASE + 1)
CMachineParameter const paramOsc4Phase = { "OSC-4 Phase", "OSC-4 Phase", 0, 255, MPF_STATE, 0 };
#define PARAM_OSC5_PHASE (PARAM_OSC4_PHASE + 1)
CMachineParameter const paramOsc5Phase = { "OSC-5 Phase", "OSC-5 Phase", 0, 255, MPF_STATE, 0 };
#define PARAM_OSC6_PHASE (PARAM_OSC5_PHASE + 1)
CMachineParameter const paramOsc6Phase = { "OSC-6 Phase", "OSC-6 Phase", 0, 255, MPF_STATE, 0 };

#define PARAM_OSC1_SEMI (PARAM_OSC1_PHASE + PARAM_ROWS)
CMachineParameter const paramOsc1Semi = { "OSC-1 Semi", "OSC-1 Semi", -24, 24, MPF_STATE, 0 };
#define PARAM_OSC2_SEMI (PARAM_OSC1_SEMI + 1)
CMachineParameter const paramOsc2Semi = { "OSC-2 Semi", "OSC-2 Semi", -24, 24, MPF_STATE, 0 };
#define PARAM_OSC3_SEMI (PARAM_OSC2_SEMI + 1)
CMachineParameter const paramOsc3Semi = { "OSC-3 Semi", "OSC-3 Semi", -24, 24, MPF_STATE, 0 };
#define PARAM_OSC4_SEMI (PARAM_OSC3_SEMI + 1)
CMachineParameter const paramOsc4Semi = { "OSC-4 Semi", "OSC-4 Semi", -24, 24, MPF_STATE, 0 };
#define PARAM_OSC5_SEMI (PARAM_OSC4_SEMI + 1)
CMachineParameter const paramOsc5Semi = { "OSC-5 Semi", "OSC-5 Semi", -24, 24, MPF_STATE, 0 };
#define PARAM_OSC6_SEMI (PARAM_OSC5_SEMI + 1)
CMachineParameter const paramOsc6Semi = { "OSC-6 Semi", "OSC-6 Semi", -24, 24, MPF_STATE, -12 };

#define PARAM_OSC1_FINE (PARAM_OSC1_SEMI + PARAM_ROWS)
CMachineParameter const paramOsc1Fine = { "OSC-1 Fine", "OSC-1 Fine", -64, 64, MPF_STATE, -12 };
#define PARAM_OSC2_FINE (PARAM_OSC1_FINE + 1)
CMachineParameter const paramOsc2Fine = { "OSC-2 Fine", "OSC-2 Fine", -64, 64, MPF_STATE, -6 };
#define PARAM_OSC3_FINE (PARAM_OSC2_FINE + 1)
CMachineParameter const paramOsc3Fine = { "OSC-3 Fine", "OSC-3 Fine", -64, 64, MPF_STATE, 0 };
#define PARAM_OSC4_FINE (PARAM_OSC3_FINE + 1)
CMachineParameter const paramOsc4Fine = { "OSC-4 Fine", "OSC-4 Fine", -64, 64, MPF_STATE, 6 };
#define PARAM_OSC5_FINE (PARAM_OSC4_FINE + 1)
CMachineParameter const paramOsc5Fine = { "OSC-5 Fine", "OSC-5 Fine", -64, 64, MPF_STATE, 12 };
#define PARAM_OSC6_FINE (PARAM_OSC5_FINE + 1)
CMachineParameter const paramOsc6Fine = { "OSC-6 Fine", "OSC-6 Fine", -64, 64, MPF_STATE, 0 };

#define PARAM_OSC1_LEVEL (PARAM_OSC1_FINE + PARAM_ROWS)
CMachineParameter const paramOsc1Level = { "OSC-1 Level", "OSC-1 Level", 0, 255, MPF_STATE, 255 };
#define PARAM_OSC2_LEVEL (PARAM_OSC1_LEVEL + 1)
CMachineParameter const paramOsc2Level = { "OSC-2 Level", "OSC-2 Level", 0, 255, MPF_STATE, 255 };
#define PARAM_OSC3_LEVEL (PARAM_OSC2_LEVEL + 1)
CMachineParameter const paramOsc3Level = { "OSC-3 Level", "OSC-3 Level", 0, 255, MPF_STATE, 255 };
#define PARAM_OSC4_LEVEL (PARAM_OSC3_LEVEL + 1)
CMachineParameter const paramOsc4Level = { "OSC-4 Level", "OSC-4 Level", 0, 255, MPF_STATE, 255 };
#define PARAM_OSC5_LEVEL (PARAM_OSC4_LEVEL + 1)
CMachineParameter const paramOsc5Level = { "OSC-5 Level", "OSC-5 Level", 0, 255, MPF_STATE, 255 };
#define PARAM_OSC6_LEVEL (PARAM_OSC5_LEVEL + 1)
CMachineParameter const paramOsc6Level = { "OSC-6 Level", "OSC-6 Level", 0, 255, MPF_STATE, 255 };

#define PARAM_VCA_ATTACK 6
CMachineParameter const paramVcaAttack = { "VCA Attack", "VCA Attack", 1, 255, MPF_STATE, 64 };
#define PARAM_VCA_DECAY (PARAM_VCA_ATTACK + PARAM_ROWS)
CMachineParameter const paramVcaDecay = { "VCA Decay", "VCA Decay", 1, 255, MPF_STATE, 1 };
#define PARAM_VCA_SUSTAIN (PARAM_VCA_DECAY + PARAM_ROWS)
CMachineParameter const paramVcaSustain = { "VCA Sustain", "VCA Sustain", 0, 255, MPF_STATE, 255 };
#define PARAM_VCA_RELEASE (PARAM_VCA_SUSTAIN + PARAM_ROWS)
CMachineParameter const paramVcaRelease = { "VCA Release", "VCA Release", 1, 255, MPF_STATE, 128 };
#define PARAM_AMP_LEVEL (PARAM_VCA_RELEASE + PARAM_ROWS)
CMachineParameter const paramAmpLevel = { "Amp. Level", "Amp. Level", 0, 255, MPF_STATE, 85 };

#define PARAM_VCF_ATTACK 7
CMachineParameter const paramVcfAttack = { "VCF Attack", "VCF Attack", 1, 255, MPF_STATE, 128 };
#define PARAM_VCF_DECAY (PARAM_VCF_ATTACK + PARAM_ROWS)
CMachineParameter const paramVcfDecay = { "VCF Decay", "VCF Decay", 1, 255, MPF_STATE, 128 };
#define PARAM_VCF_SUSTAIN (PARAM_VCF_DECAY + PARAM_ROWS)
CMachineParameter const paramVcfSustain = { "VCF Sustain", "VCF Sustain", 0, 255, MPF_STATE, 128 };
#define PARAM_VCF_RELEASE (PARAM_VCF_SUSTAIN + PARAM_ROWS)
CMachineParameter const paramVcfRelease = { "VCF Release", "VCF Release", 1, 255, MPF_STATE, 255 };
#define PARAM_VCF_AMOUNT (PARAM_VCF_RELEASE + PARAM_ROWS)
CMachineParameter const paramVcfAmount = { "VCF Amount", "VCF Amount", -127, 127, MPF_STATE, 48 };

#define PARAM_FILTER_TYPE 8
CMachineParameter const paramFilterType = { "Filter Type", "Filter Type", 0, 4, MPF_STATE, 0 };
#define PARAM_FILTER_FREQ (PARAM_FILTER_TYPE + PARAM_ROWS)
CMachineParameter const paramFilterFreq = { "Filter Freq.", "Filter Freq.", 0, 255, MPF_STATE, 64 };
#define PARAM_FILTER_RES (PARAM_FILTER_FREQ + PARAM_ROWS)
CMachineParameter const paramFilterRes = { "Filter Res.", "Filter Res.", 0, 255, MPF_STATE, 0 };
#define PARAM_FILTER_RATE (PARAM_FILTER_RES + PARAM_ROWS)
CMachineParameter const paramFilterRate = { "F. LFO Rate", "F. LFO Rate", 0, 255, MPF_STATE, 0 };
#define PARAM_FILTER_AMOUNT (PARAM_FILTER_RATE + PARAM_ROWS)
CMachineParameter const paramFilterAmount = { "F. LFO Amt.", "F. LFO Amt.", 0, 8160, MPF_STATE, 0 };

#define PARAM_PHASER_MIN 9
CMachineParameter const paramPhaserMin = { "Phaser Min.", "Phaser Min.", 0, 255, MPF_STATE, 0 };
#define PARAM_PHASER_MAX (PARAM_PHASER_MIN + PARAM_ROWS)
CMachineParameter const paramPhaserMax = { "Phaser Max.", "Phaser Max.", 0, 255, MPF_STATE, 0 };
#define PARAM_PHASER_RATE (PARAM_PHASER_MAX + PARAM_ROWS)
CMachineParameter const paramPhaserRate = { "Phaser Rate", "Phaser Rate", 0, 8160, MPF_STATE, 0 };
#define PARAM_PHASER_FB (PARAM_PHASER_RATE + PARAM_ROWS)
CMachineParameter const paramPhaserFB = { "Phaser F.B.", "Phaser F.B.", 0, 255, MPF_STATE, 0 };
#define PARAM_PHASER_DEPTH (PARAM_PHASER_FB + PARAM_ROWS)
CMachineParameter const paramPhaserDepth = { "Phaser Depth", "Phaser Depth", 0, 255, MPF_STATE, 0 };

#define PARAM_CHORUS_DELAY 10
CMachineParameter const paramChorusDelay = { "Chorus Delay", "Chorus Delay", 0, 255, MPF_STATE, 0 };
#define PARAM_CHORUS_DEPTH (PARAM_CHORUS_DELAY + PARAM_ROWS)
CMachineParameter const paramChorusDepth = { "Chorus Depth", "Chorus Depth", 0, 255, MPF_STATE, 0 };
#define PARAM_CHORUS_RATE (PARAM_CHORUS_DEPTH + PARAM_ROWS)
CMachineParameter const paramChorusRate = { "Chorus Rate", "Chorus Rate", 0, 8160, MPF_STATE, 0 };
#define PARAM_CHORUS_FB (PARAM_CHORUS_RATE + PARAM_ROWS)
CMachineParameter const paramChorusFB = { "Chorus F.B.", "Chorus F.B.", 0, 255, MPF_STATE, 0 };
#define PARAM_CHORUS_MIX (PARAM_CHORUS_FB + PARAM_ROWS)
CMachineParameter const paramChorusMix = { "Chorus Mix", "Chorus Mix", 0, 255, MPF_STATE, 0 };

//============================================================================
//				Parameter list
//============================================================================

CMachineParameter const *pParams[] = {
	&paramOsc1Wave,
	&paramOsc2Wave,
	&paramOsc3Wave,
	&paramOsc4Wave,
	&paramOsc5Wave,
	&paramOsc6Wave,
	&paramVcaAttack,
	&paramVcfAttack,
	&paramFilterType,
	&paramPhaserMin,
	&paramChorusDelay,
	&paramOsc1Phase,
	&paramOsc2Phase,
	&paramOsc3Phase,
	&paramOsc4Phase,
	&paramOsc5Phase,
	&paramOsc6Phase,
	&paramVcaDecay,
	&paramVcfDecay,
	&paramFilterFreq,
	&paramPhaserMax,
	&paramChorusDepth,
	&paramOsc1Semi,
	&paramOsc2Semi,
	&paramOsc3Semi,
	&paramOsc4Semi,
	&paramOsc5Semi,
	&paramOsc6Semi,
	&paramVcaSustain,
	&paramVcfSustain,
	&paramFilterRes,
	&paramPhaserRate,
	&paramChorusRate,
	&paramOsc1Fine,
	&paramOsc2Fine,
	&paramOsc3Fine,
	&paramOsc4Fine,
	&paramOsc5Fine,
	&paramOsc6Fine,
	&paramVcaRelease,
	&paramVcfRelease,
	&paramFilterRate,
	&paramPhaserFB,
	&paramChorusFB,
	&paramOsc1Level,
	&paramOsc2Level,
	&paramOsc3Level,
	&paramOsc4Level,
	&paramOsc5Level,
	&paramOsc6Level,
	&paramAmpLevel,
	&paramVcfAmount,
	&paramFilterAmount,
	&paramPhaserDepth,
	&paramChorusMix
};

//============================================================================
//				Machine info
//============================================================================
CMachineInfo const MacInfo (
	MI_VERSION,
	IMAC_VERSION,
	GENERATOR,
	sizeof pParams / sizeof *pParams,
	pParams,
#ifdef _DEBUG
	MAC_NAME " " MAC_VERSION " (Debug)",
#else
	MAC_NAME " " MAC_VERSION,
#endif
	MAC_NAME,
	MAC_AUTHOR " on " __DATE__,
	"Command Help",
	5
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
	GLOBALS globals;
	CVoice				voices[MAX_TRACKS][MAX_VOICES];
	int					ticks_remaining;

	//				Phaser
	float				phaser_min;
	float				phaser_max;
	float				phaser_increment;
	Phaser				phaser_left;
	Phaser				phaser_right;
	float				phaser_left_phase;
	float				phaser_right_phase;

	//				Chorus
	float				chorus_delay;
	float				chorus_depth;
	float				chorus_increment;
	Chorus				chorus_left;
	Chorus				chorus_right;
	float				chorus_left_phase;
	float				chorus_right_phase;

	//
	float				inertia_samples;
};

PSYCLE__PLUGIN__INSTANTIATOR("phantom", mi, MacInfo)

//////////////////////////////////////////////////////////////////////
//
//				Constructor
//
//////////////////////////////////////////////////////////////////////

mi::mi()
{
	Vals = new int[MacInfo.numParameters];

	inertia_samples=0.0f;
	globals.filter_freq.current=0.0f;
	globals.filter_res.current=0.0f;

	globals.samplingrate=0;
	for (int ti = 0; ti < MAX_TRACKS; ti++) {
		for (int vi = 0; vi < MAX_VOICES; vi++)
			voices[ti][vi].globals = &globals;
	}
	Stop();
	//
	//
	int i;
	float t;

	//				Render
	for (i = 0; i < WAVESIZE; i++) {
		t = (float) i / (float) WAVESIZE;
		//				Sine
		wavetable[0][i] = (float) sin(t * PI2);
		//				Triangle
		if (t < 0.25f) {
			wavetable[1][i] = t * 4.0f;
		} else if (t < 0.75f) {
			wavetable[1][i] = (0.5f - t) * 4.0f;
		} else {
			wavetable[1][i] = (t - 1.0f) * 4.0f;
		}
		//				Pulse
		wavetable[2][i] = (t < 0.5f ? 1.0f : -1.0f);
		//				Ramp up
		wavetable[3][i] = (t - 0.5f) * 2.0f;
		//				Ramp down
		wavetable[4][i] = (0.5f - t) * 2.0f;
		//				Half sine half ramp up
		//float j = 0.0f;
		//float tmp = 0.0f;
		if (t < 0.5f)
			wavetable[5][i] = (float) sin(t * 2.0f * PI2);
		else
			wavetable[5][i] = (t - 0.75f) * 4.0f;
		//				Square - Sine
		wavetable[6][i] = wavetable[2][i] - wavetable[0][i];
		//				Half Sine half blank
		if (t < 0.5f)
			wavetable[7][i] = 0.0f;
		else
			wavetable[7][i] = (t - 0.75f) * 4.0f;

	}
}

//////////////////////////////////////////////////////////////////////
//
//				Destructor
//
//////////////////////////////////////////////////////////////////////

mi::~mi()
{
	Stop();
	delete[] Vals;
}

//////////////////////////////////////////////////////////////////////
//
//				Init
//
//////////////////////////////////////////////////////////////////////

void mi::Init()
{
	globals.samplingrate= pCB->GetSamplingRate();

	inertia_samples = (4096.0f/NUM_TICKS)*(globals.samplingrate/44100.f);
	//
	//
	phaser_left.Init();
	phaser_right.Init();
	phaser_left_phase = 0.0f;
	phaser_right_phase = 0.0f;
	//
	//
	chorus_left.Init((paramChorusDelay.MaxValue*8.f+paramChorusDepth.MaxValue*4.f)*(globals.samplingrate/44100.f));
	chorus_right.Init((paramChorusDelay.MaxValue*8.f+paramChorusDepth.MaxValue*4.f)*(globals.samplingrate/44100.f));
	chorus_left_phase = 0.0f;
	chorus_right_phase = 0.0f;
	//
	//
	CDsp::Fill(globals.buf, 0.0f, 256);
}

//////////////////////////////////////////////////////////////////////
//
//				Stop
//
//////////////////////////////////////////////////////////////////////

void mi::Stop()
{
	for (int ti = 0; ti < MAX_TRACKS; ti++) {
		for (int vi = 0; vi < MAX_VOICES; vi++)
			voices[ti][vi].Stop();
	}
	ticks_remaining = NUM_TICKS;
	CDsp::Fill(globals.buf, 0.0f, 256);
}

//////////////////////////////////////////////////////////////////////
//
//				Command
//
//////////////////////////////////////////////////////////////////////

void mi::Command()
{
	pCB->MessBox(
		"Phantom project\n\n"
		"Only 1 command so far :) Plz post ideas in the boards at psycle.pastnotecut.org\n"
		"0x0C - Volume / Velocity\n\n"
		"---------------------------\n"
		"druttis@darkface.pp.se\n",
		MAC_AUTHOR " " MAC_NAME " v." MAC_VERSION,
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

	switch (par) {
		case PARAM_OSC1_WAVE:
		case PARAM_OSC2_WAVE:
		case PARAM_OSC3_WAVE:
		case PARAM_OSC4_WAVE:
		case PARAM_OSC5_WAVE:
		case PARAM_OSC6_WAVE:
			globals.osc_wave[par - PARAM_OSC1_WAVE] = val - 1;
			break;
		case PARAM_OSC1_PHASE:
		case PARAM_OSC2_PHASE:
		case PARAM_OSC3_PHASE:
		case PARAM_OSC4_PHASE:
		case PARAM_OSC5_PHASE:
		case PARAM_OSC6_PHASE:
			globals.osc_phase[par - PARAM_OSC1_PHASE] = (float) (WAVESIZE * val) / 256.0f;
			break;
		case PARAM_OSC1_SEMI:
		case PARAM_OSC2_SEMI:
		case PARAM_OSC3_SEMI:
		case PARAM_OSC4_SEMI:
		case PARAM_OSC5_SEMI:
		case PARAM_OSC6_SEMI:
			globals.osc_semi[par - PARAM_OSC1_SEMI] = (float) val;
			break;
		case PARAM_OSC1_FINE:
		case PARAM_OSC2_FINE:
		case PARAM_OSC3_FINE:
		case PARAM_OSC4_FINE:
		case PARAM_OSC5_FINE:
		case PARAM_OSC6_FINE:
			globals.osc_fine[par - PARAM_OSC1_FINE] = (float) val / 128.0f;
			break;
		case PARAM_OSC1_LEVEL:
		case PARAM_OSC2_LEVEL:
		case PARAM_OSC3_LEVEL:
		case PARAM_OSC4_LEVEL:
		case PARAM_OSC5_LEVEL:
		case PARAM_OSC6_LEVEL:
			globals.osc_level[par - PARAM_OSC1_LEVEL] = (float) val / 255.0f;
			break;
		case PARAM_VCA_ATTACK:
			globals.vca_attack = (float) val * 256.0f * (float)globals.samplingrate / 44100.f;
			break;
		case PARAM_VCA_DECAY:
			globals.vca_decay = (float) val * 256.0f * (float)globals.samplingrate / 44100.f;
			break;
		case PARAM_VCA_SUSTAIN:
			globals.vca_sustain = (float) val / 255.0f;
			break;
		case PARAM_VCA_RELEASE:
			globals.vca_release = (float) val * 256.0f * (float)globals.samplingrate / 44100.f;
			break;
		case PARAM_AMP_LEVEL:
			globals.amp_level = (float) val / 255.0f;
			break;
		case PARAM_VCF_ATTACK:
			globals.vcf_attack = (float) val * (256.f/NUM_TICKS) * (float)globals.samplingrate / 44100.f;
			break;
		case PARAM_VCF_DECAY:
			globals.vcf_decay = (float) val * (256.f/NUM_TICKS) * (float)globals.samplingrate / 44100.f;
			break;
		case PARAM_VCF_SUSTAIN:
			globals.vcf_sustain = (float) val / 255.0f;
			break;
		case PARAM_VCF_RELEASE:
			globals.vcf_release = (float) val * (256.f/NUM_TICKS) * (float)globals.samplingrate / 44100.f;
			break;
		case PARAM_VCF_AMOUNT:
			globals.vcf_amount = (float) val / 127.0f;
			break;
		case PARAM_FILTER_TYPE:
			globals.filter_type = val;
			break;
		case PARAM_FILTER_FREQ:
			SetAFloat(globals.filter_freq, (float) val / 255.0f * (44100.f/globals.samplingrate), inertia_samples);
			break;
		case PARAM_FILTER_RES:
			SetAFloat(globals.filter_res, (float) val / 255.0f, inertia_samples);
			break;
		case PARAM_FILTER_RATE:
			globals.filter_increment = (float) (val * WAVESIZE*NUM_TICKS/408.f) /(float) globals.samplingrate;
			break;
		case PARAM_FILTER_AMOUNT:
			globals.filter_amount = (float) val / 255.0f;
			break;
		case PARAM_PHASER_MIN:
			phaser_min = (float) val * 128.0f/globals.samplingrate;
			break;
		case PARAM_PHASER_MAX:
			phaser_max = (float) val *  128.0f/globals.samplingrate;
			break;
		case PARAM_PHASER_RATE:
			phaser_increment = (float) (val * WAVESIZE/408.f) /(float) globals.samplingrate;
			break;
		case PARAM_PHASER_FB:
			tmp = (float) val / 255.0f;
			phaser_left.SetFeedback(tmp);
			phaser_right.SetFeedback(tmp);
			break;
		case PARAM_PHASER_DEPTH:
			tmp = (float) val / 510.0f;
			phaser_left.SetDepth(tmp);
			phaser_right.SetDepth(tmp);
			break;
		case PARAM_CHORUS_DELAY:
			chorus_delay = (float) val * 8.0f * (globals.samplingrate/44100.f);
			break;
		case PARAM_CHORUS_DEPTH:
			chorus_depth = (float) val * 4.0f * (globals.samplingrate/44100.f);
			break;
		case PARAM_CHORUS_RATE:
			chorus_increment = (float) (val * WAVESIZE/408.f) /(float) globals.samplingrate;
			break;
		case PARAM_CHORUS_FB:
			tmp = (float) val / 255.0f;
			chorus_left.SetFeedback(tmp);
			chorus_right.SetFeedback(tmp);
			break;
		case PARAM_CHORUS_MIX:
			tmp = (float) val / 510.0f;
			chorus_left.SetMix(tmp);
			chorus_right.SetMix(tmp);
			break;
	}
}

//////////////////////////////////////////////////////////////////////
//
//				DescribeValue
//
//////////////////////////////////////////////////////////////////////

bool mi::DescribeValue(char* txt,int const param, int const value) {

	switch (param) {
		case PARAM_OSC1_WAVE:
		case PARAM_OSC2_WAVE:
		case PARAM_OSC3_WAVE:
		case PARAM_OSC4_WAVE:
		case PARAM_OSC5_WAVE:
		case PARAM_OSC6_WAVE:
			sprintf(txt, "%s", wave_str[value - 1]);
			break;
		case PARAM_OSC1_PHASE:
		case PARAM_OSC2_PHASE:
		case PARAM_OSC3_PHASE:
		case PARAM_OSC4_PHASE:
		case PARAM_OSC5_PHASE:
		case PARAM_OSC6_PHASE:
			sprintf(txt, "%.2f deg.", (float) value * 360.0f / 256.0f);
			break;
		case PARAM_OSC1_LEVEL:
		case PARAM_OSC2_LEVEL:
		case PARAM_OSC3_LEVEL:
		case PARAM_OSC4_LEVEL:
		case PARAM_OSC5_LEVEL:
		case PARAM_OSC6_LEVEL:
			if (value > 0) sprintf(txt, "%.02f dB", 20.0f * std::log10((float) value / 255.0f));
			else sprintf(txt, "-inf dB");
			break;
		case PARAM_OSC1_FINE:
		case PARAM_OSC2_FINE:
		case PARAM_OSC3_FINE:
		case PARAM_OSC4_FINE:
		case PARAM_OSC5_FINE:
		case PARAM_OSC6_FINE:
			sprintf(txt, "%.0f cent", (float) value / 1.28f);
			break;
		case PARAM_VCA_ATTACK:
		case PARAM_VCA_DECAY:
		case PARAM_VCA_RELEASE:
		case PARAM_VCF_ATTACK:
		case PARAM_VCF_DECAY:
		case PARAM_VCF_RELEASE:
			sprintf(txt, "%.2f ms.", (float) value * 256.0f / 44.1f);
			break;
		case PARAM_VCA_SUSTAIN:
		case PARAM_VCF_SUSTAIN:
			if (value > 0) sprintf(txt, "%.02f dB", 20.0f * std::log10((float) value / 255.0f));
			else sprintf(txt, "-inf dB");
			break;
		case PARAM_AMP_LEVEL:
			if (value > 0) sprintf(txt, "%.02f dB", 20.0f * std::log10((float) value / 255.0f));
			else sprintf(txt, "-inf dB");
			break;
		case PARAM_VCF_AMOUNT:
			sprintf(txt, "%.2f %%", (float) value * 100.0f / 255.0f);
			break;
		case PARAM_FILTER_FREQ:
			// All them shoud be a bit logarithmic towards the end.
			if(Vals[PARAM_FILTER_TYPE] == 0 || Vals[PARAM_FILTER_TYPE] == 2) {
				std::sprintf(txt,"%.0f Hz",4000.f*(float)value/144.f);
			} else if(Vals[PARAM_FILTER_TYPE] == 1) {
				std::sprintf(txt,"%.0f Hz",4000.f*(float)value/100.f);
			} else if(Vals[PARAM_FILTER_TYPE] == 3) {
				std::sprintf(txt,"%.0f Hz",15000.f*(float)value/203.f);
			} else {
				std::sprintf(txt,"%.0f Hz",8000.f*(float)value/158.f);
			}
			break;
		case PARAM_FILTER_RES:
			//todo: in Q instead of percentage.
			sprintf(txt, "%.2f %%", (float) value * 100.0f / 255.0f);
			break;
		case PARAM_FILTER_TYPE:
			sprintf(txt, "%s", filt_str[value]);
			break;
		case PARAM_FILTER_RATE:
			sprintf(txt, "%.2f Hz", ((float) value / 408.));
			break;
		case PARAM_FILTER_AMOUNT:
			sprintf(txt, "%.2f", ((float) value * 100.0f / 255.0f));
			break;
		case PARAM_PHASER_MIN:
		case PARAM_PHASER_MAX:
			sprintf(txt, "%d Hz", (value * 64));
			break;
		case PARAM_PHASER_RATE:
			sprintf(txt, "%.2f Hz", ((float) value / 408.f));
			break;
		case PARAM_PHASER_FB:
			sprintf(txt, "%.2f %%", ((float) value * 100.0f / 255.0f));
			break;
		case PARAM_PHASER_DEPTH:
			sprintf(txt, "%.2f %%", ((float) value * 100.0f / 255.0f));
			break;
		case PARAM_CHORUS_DELAY:
			sprintf(txt, "%.2f ms", ((float) value * 8.0f / 44.1f));
			break;
		case PARAM_CHORUS_DEPTH:
			sprintf(txt, "%.2f %%", ((float) value * 100.0f / 255.0f));
			break;
		case PARAM_CHORUS_RATE:
			sprintf(txt, "%.2f Hz", ((float) value / 408.f));
			break;
		case PARAM_CHORUS_FB:
			sprintf(txt, "%.2f %%", ((float) value * 100.0f / 255.0f));
			break;
		case PARAM_CHORUS_MIX:
			sprintf(txt, "%.2f %%", ((float) value * 100.0f / 255.0f));
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
	//////////////////////////////////////////////////////////////////
	//				Check SR change.
	//////////////////////////////////////////////////////////////////
	if (globals.samplingrate != pCB->GetSamplingRate())
	{
		globals.samplingrate = pCB->GetSamplingRate();
		inertia_samples = (4096.0f/NUM_TICKS)*(globals.samplingrate/44100.f);

		globals.vca_attack = (float) Vals[PARAM_VCA_ATTACK] * 256.0f * (float)globals.samplingrate / 44100.f;
		globals.vca_decay = (float) Vals[PARAM_VCA_DECAY] * 256.0f * (float)globals.samplingrate / 44100.f;
		globals.vca_release = (float) Vals[PARAM_VCA_RELEASE] * 256.0f * (float)globals.samplingrate / 44100.f;
		globals.vcf_attack = (float) Vals[PARAM_VCF_ATTACK] * (256.f/NUM_TICKS) * (float)globals.samplingrate / 44100.f;
		globals.vcf_decay = (float) Vals[PARAM_VCF_DECAY] * (256.f/NUM_TICKS) * (float)globals.samplingrate / 44100.f;
		globals.vcf_release = (float) Vals[PARAM_VCF_RELEASE] * (256.f/NUM_TICKS) * (float)globals.samplingrate / 44100.f;

		SetAFloat(globals.filter_freq, (float) Vals[PARAM_FILTER_FREQ] / 255.0f * (44100.f/globals.samplingrate), 0);
		SetAFloat(globals.filter_res, globals.filter_res.target, inertia_samples);
		globals.filter_increment = (float) (Vals[PARAM_FILTER_RATE]  * WAVESIZE*NUM_TICKS/408.f) /(float) globals.samplingrate;

		phaser_min = (float) Vals[PARAM_PHASER_MIN] * 128.0f/globals.samplingrate;
		phaser_max = (float) Vals[PARAM_PHASER_MAX] * 128.0f/globals.samplingrate;
		phaser_increment = (float) (Vals[PARAM_PHASER_RATE] * WAVESIZE/408.f) /(float) globals.samplingrate;

		chorus_left.Init((paramChorusDelay.MaxValue*8.f+paramChorusDepth.MaxValue*4.f)*(globals.samplingrate/44100.f));
		chorus_right.Init((paramChorusDelay.MaxValue*8.f+paramChorusDepth.MaxValue*4.f)*(globals.samplingrate/44100.f));
		chorus_delay = (float) Vals[PARAM_CHORUS_DELAY] * 8.0f * (globals.samplingrate/44100.f);
		chorus_depth = (float) Vals[PARAM_CHORUS_DEPTH] * 4.0f * (globals.samplingrate/44100.f);
		chorus_increment = (float) (Vals[PARAM_CHORUS_RATE] * WAVESIZE/408.f) /(float) globals.samplingrate;


			
	}
}

//////////////////////////////////////////////////////////////////////
//
//				SequencerTick
//
//////////////////////////////////////////////////////////////////////

void mi::SeqTick(int channel, int note, int ins, int cmd, int val)
{
	//////////////////////////////////////////////////////////////////
	//				Volume
	//////////////////////////////////////////////////////////////////

	int vol = 254;

	//////////////////////////////////////////////////////////////////
	//				Handle commands
	//////////////////////////////////////////////////////////////////

	switch (cmd)
	{
		case 0x0c:				//				Volume
			vol = val - 2;
			if (vol < 0)
				vol = 0;
			break;
	}

	//////////////////////////////////////////////////////////////////
	//				Route
	//////////////////////////////////////////////////////////////////

	if (note==NOTE_NOTEOFF) {
		voices[channel][0].NoteOff();
	} else if (note<=NOTE_MAX) {
		voices[channel][0].NoteOff();
		voices[channel][1] = voices[channel][0];
		voices[channel][0].NoteOn(note, vol);
		voices[channel][0].ticks_remaining = 0;
	}

}

//////////////////////////////////////////////////////////////////////
//
//				Work
//
//////////////////////////////////////////////////////////////////////

void mi::Work(float *psamplesleft, float* psamplesright, int numsamples, int numtracks)
{
	//////////////////////////////////////////////////////////////////
	//				Variables
	//////////////////////////////////////////////////////////////////
	int ti;
	int vi;
	int amount;
	int amt;
	int nsamples;
	float *pleft;
	float *pright;
	float *psamplesleft2 = psamplesleft;
	float *psamplesright2 = psamplesright;
	float rnd;
	//////////////////////////////////////////////////////////////////
	//				Adjust sample pointers
	//////////////////////////////////////////////////////////////////
	--psamplesleft2;
	--psamplesright2;
	//////////////////////////////////////////////////////////////////
	//				Loop
	//////////////////////////////////////////////////////////////////
	do
	{
		//////////////////////////////////////////////////////////////
		//				Global tick handling
		//////////////////////////////////////////////////////////////
		if (ticks_remaining <= 0)
		{
			ticks_remaining = NUM_TICKS;
			CVoice::GlobalTick();
			//////////////////////////////////////////////////////////
			//				Intertia parameters
			//////////////////////////////////////////////////////////
			AnimateAFloat(globals.filter_freq);
			AnimateAFloat(globals.filter_res);
		}
		//////////////////////////////////////////////////////////////
		//				Compute amount of samples to render for all voices
		//////////////////////////////////////////////////////////////
		amount = numsamples;
		if (amount > ticks_remaining)
			amount = ticks_remaining;
		//////////////////////////////////////////////////////////////
		//				Render all voices now
		//////////////////////////////////////////////////////////////
		bool is_synth_active = false;
		for (ti = 0; ti < numtracks; ti++)
		{
			for (vi = 0; vi < MAX_VOICES; vi++)
			{
				if (voices[ti][vi].IsActive())
				{
					is_synth_active = true;
					pleft = psamplesleft2;
					pright = psamplesright2;
					nsamples = amount;
					do
					{
						//////////////////////////////////////////////
						//				Voice tick handing
						//////////////////////////////////////////////
						if (voices[ti][vi].ticks_remaining <= 0)
						{
							voices[ti][vi].ticks_remaining = NUM_TICKS;
							voices[ti][vi].VoiceTick();
						}
						//////////////////////////////////////////////
						//				Compute amount of samples to render this voice
						//////////////////////////////////////////////
						amt = nsamples;
						if (amt > voices[ti][vi].ticks_remaining)
							amt = voices[ti][vi].ticks_remaining;
						//////////////////////////////////////////////
						//				Render voice now
						//////////////////////////////////////////////
						voices[ti][vi].Work(pleft, pright, amt);
						//////////////////////////////////////////////
						//				Adjust for next voice iteration
						//////////////////////////////////////////////
						voices[ti][vi].ticks_remaining -= amt;
						pleft += amt;
						pright += amt;
						nsamples -= amt;
					}
					while (nsamples);
				}
			}
		}
		//////////////////////////////////////////////////////////////
		//				Chorus & Phase
		//////////////////////////////////////////////////////////////
        if (is_synth_active)
        {
			pleft = psamplesleft2;
			pright = psamplesright2;
			nsamples = amount;
			do
			{
				//////////////////////////////////////////////////////////
				//				Update phaser
				//////////////////////////////////////////////////////////
				phaser_left.SetDelay(phaser_min + (phaser_max - phaser_min) * (1.0f + get_sample_l(wavetable[0], phaser_left_phase, WAVEMASK)) * 0.5f);
				phaser_right.SetDelay(phaser_min + (phaser_max - phaser_min) * (1.0f - get_sample_l(wavetable[0], phaser_right_phase, WAVEMASK)) * 0.5f);
				phaser_left_phase = fand(phaser_left_phase + phaser_increment, WAVEMASK);
				phaser_right_phase = fand(phaser_right_phase + phaser_increment * 1.1111f, WAVEMASK);
				//////////////////////////////////////////////////////////
				//				Update chorus
				//////////////////////////////////////////////////////////
				chorus_left.SetDelay(chorus_delay + chorus_depth * (1.0f + get_sample_l(wavetable[0], chorus_left_phase, WAVEMASK)) * 0.5f);
				chorus_right.SetDelay(chorus_delay + chorus_depth * (1.0f - get_sample_l(wavetable[0], chorus_right_phase, WAVEMASK)) * 0.5f);
				chorus_left_phase = fand(chorus_left_phase + chorus_increment, WAVEMASK);
				chorus_right_phase = fand(chorus_right_phase + chorus_increment * 1.1111f, WAVEMASK);
				//////////////////////////////////////////////////////////
				// To prevent chorus (0) calls.
				//////////////////////////////////////////////////////////
				rnd = CDsp::GetRandomSignal() * 0.001f;
				//////////////////////////////////////////////////////////
				//				Left
				//////////////////////////////////////////////////////////
				++pleft;
				*pleft = phaser_left.Tick(chorus_left.Tick(*pleft + rnd));
				//////////////////////////////////////////////////////////
				//				Right
				//////////////////////////////////////////////////////////
				++pright;
				*pright = phaser_right.Tick(chorus_right.Tick(*pright + rnd));
			}
			while (--nsamples);
		}
        else
        {
            // synth inactive: don't process FX
            pleft = psamplesleft2;
            pright = psamplesright2;
            nsamples = amount;
            do
            {
            	*++pleft = 0.f;
            	*++pright = 0.f;
            }                     
            while (--nsamples);          
        }
		//////////////////////////////////////////////////////////////
		//				Adjust for next iteration
		//////////////////////////////////////////////////////////////
		ticks_remaining -= amount;
		psamplesleft2 += amount;
		psamplesright2 += amount;
		numsamples -= amount;
	}
	while (numsamples);
}
}