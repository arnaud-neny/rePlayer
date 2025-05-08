//////////////////////////////////////////////////////////////////////
//
//				CoreGenerator
//
//				druttis@darkface.pp.se
//
//////////////////////////////////////////////////////////////////////
//#include <windows.h>
#include "Voice.h"
#include "../blwtbl/blwtbl.h"
#include <cstdio>
namespace psycle::plugins::druttis::sublime {
using namespace psycle::plugin_interface;

//////////////////////////////////////////////////////////////////////
//
//				Machine Defs
//
//////////////////////////////////////////////////////////////////////
#define MAC_NAME "Sublime"
#define MAC_VERSION "1.1"
int const IMAC_VERSION		=	0x0110;
#define MAC_AUTHOR "Druttis"
#define NUM_PARAM_COLS 4
//////////////////////////////////////////////////////////////////////
//
//				Track Defs
//
//////////////////////////////////////////////////////////////////////
#define MAX_TRACK_POLY 4
//////////////////////////////////////////////////////////////////////
//
//				Param Constants
//
//////////////////////////////////////////////////////////////////////
wf_type oscwaves[] = { WF_BLANK, WF_SINE,
	WF_BLTRIANGLE, WF_BLSQUARE, WF_BLSAWTOOTH, WF_BLPARABOLA,
	//Vowel-A, E, I, O, U
	WF_BLSAWTOOTH, WF_BLSAWTOOTH, WF_BLSAWTOOTH, WF_BLSAWTOOTH,	WF_BLSAWTOOTH
};
wf_type lfowaves[] = {WF_SINE, WF_TRIANGLE, WF_SQUARE, WF_SAWTOOTH,WF_REVSAWTOOTH};

char const *str_pm_types[] = {"off", "sub", "add", "mul"};
char const *str_yes_no[] = {"no", "yes"};
char const *str_mod_dests[] = {"off", "OSC 1", "OSC 2", "OSC 1&2",
	"OSC - Mix", "OSC - FM", "OSC - PM","OSC 1&2 - PM.Amt.",
	"FLT 1 - Freq.", "FLT 2 - Freq.", "LFO 1 - Freq.", "LFO 2 - Freq."
};
char const *str_lfo1_dests[] = {"off", "OSC 1", "OSC 2", "OSC 1&2",
	"OSC - Mix", "OSC - FM", "OSC - PM", "OSC 1&2 - PM.Amt.",
	"FLT 1 - Freq.","FLT 2 - Freq."
};
char const *str_lfo2_dests[] = {"off", "OSC 1", "OSC 2", "OSC 1&2",
	"OSC - Mix", "OSC - FM", "OSC - PM", "OSC 1&2 - PM.Amt.",
	"FLT 1 - Freq.", "FLT 2 - Freq.", "AMP - Level"
};
char const *str_flt1_types[] = {"LP-12", "LP-24", "LP-36",
	"HP-12", "HP-24", "Moog LP-24", "Moog HP-24","Moog BP-24"
};
char const *str_flt2_modes[] = {"disabled", "enabled", "linked"};
//////////////////////////////////////////////////////////////////////
//
//				Machine Parameters
//
//////////////////////////////////////////////////////////////////////
#define PARAM_OSC1_WAVE 0
CMachineParameter const param_osc1_wave = {"OSC 1 - Wave", "OSC 1 - Wave", 1, 10, MPF_STATE, 1};

#define PARAM_OSC1_PHASE 1
CMachineParameter const param_osc1_phase = {"OSC 1 - Phase", "OSC 1 - Phase", 0, 256, MPF_STATE, 0};

#define PARAM_OSC1_SEMI 2
CMachineParameter const param_osc1_semi = {"OSC 1 - Semi", "OSC 1 - Semi", -48, 48, MPF_STATE, 0};

#define PARAM_OSC1_CENT 3
CMachineParameter const param_osc1_cent = {"OSC 1 - Cent", "OSC 1 - Cent", -64, 64, MPF_STATE, 0};

#define PARAM_OSC1_PM_TYPE 4
CMachineParameter const param_osc1_pm_type = {"OSC 1 - PM.Type", "OSC 1 - PM.Type", 0, 3, MPF_STATE, 0};

#define PARAM_OSC1_PM_AMOUNT 5
CMachineParameter const param_osc1_pm_amount = {"OSC 1 - PM.Amount", "OSC 1 - PM.Amount", 0, 256, MPF_STATE, 0};

#define PARAM_OSC1_KBD_TRACK 6
CMachineParameter const param_osc1_kbd_track = {"OSC 1 - Kbd.Track", "OSC 1 - Kbd.Track", 0, 1, MPF_STATE, 1};

#define PARAM_OSC2_WAVE 7
CMachineParameter const param_osc2_wave = {"OSC 2 - Wave", "OSC 2 - Wave", 0, 10, MPF_STATE, 0};

#define PARAM_OSC2_PHASE 8
CMachineParameter const param_osc2_phase = {"OSC 2 - Phase", "OSC 2 - Phase", 0, 256, MPF_STATE, 0};

#define PARAM_OSC2_SEMI 9
CMachineParameter const param_osc2_semi = {"OSC 2 - Semi", "OSC 2 - Semi", -48, 48, MPF_STATE, 0};

#define PARAM_OSC2_CENT 10
CMachineParameter const param_osc2_cent = {"OSC 2 - Cent", "OSC 2 - Cent", -64, 64, MPF_STATE, 0};

#define PARAM_OSC2_PM_TYPE 11
CMachineParameter const param_osc2_pm_type = {"OSC 2 - PM.Type", "OSC 2 - PM.Type", 0, 3, MPF_STATE, 0};

#define PARAM_OSC2_PM_AMOUNT 12
CMachineParameter const param_osc2_pm_amount = {"OSC 2 - PM.Amount", "OSC 2 - PM.Amount", 0, 256, MPF_STATE, 0};

#define PARAM_OSC2_KBD_TRACK 13
CMachineParameter const param_osc2_kbd_track = {"OSC 2 - Kbd.Track", "OSC 2 - Kbd.Track", 0, 1, MPF_STATE, 1};

#define PARAM_OSC_PHASE_RAND 14
CMachineParameter const param_osc_phase_rand = {"OSC - Phase rnd.", "OSC - Phase rnd.", 0, 256, MPF_STATE, 0};

#define PARAM_OSC_FM 15
CMachineParameter const param_osc_fm = {"OSC - FM", "OSC - FM", 0, 256, MPF_STATE, 0};

#define PARAM_OSC_PM 16
CMachineParameter const param_osc_pm = {"OSC - PM", "OSC - PM", 0, 256, MPF_STATE, 0};

#define PARAM_OSC_MIX 17
CMachineParameter const param_osc_mix = {"OSC - Mix", "OSC - Mix", 0, 256, MPF_STATE, 128};

#define PARAM_OSC_RING_MOD 18
CMachineParameter const param_osc_ring_mod = {"OSC - Ring mod", "OSC - Ring mod", 0, 1, MPF_STATE, 0};

#define PARAM_NOISE_DECAY 19
CMachineParameter const param_noise_decay = {"NOISE - Decay", "NOISE - Decay", 0, 1024, MPF_STATE, 0};

#define PARAM_NOISE_COLOR 20
CMachineParameter const param_noise_color = {"NOISE - Color", "NOISE - Color", 0, 256, MPF_STATE, 0};

#define PARAM_NOISE_LEVEL 21
CMachineParameter const param_noise_level = {"NOISE - Level", "NOISE - Level", 0, 256, MPF_STATE, 0};

#define PARAM_MOD_DELAY 22
CMachineParameter const param_mod_delay = {"MOD - Delay", "MOD - Delay", 0, 1024, MPF_STATE, 0};

#define PARAM_MOD_ATTACK 23
CMachineParameter const param_mod_attack = {"MOD - Attack", "MOD - Attack", 0, 1024, MPF_STATE, 0};

#define PARAM_MOD_DECAY 24
CMachineParameter const param_mod_decay = {"MOD - Decay", "MOD - Decay", 0, 1024, MPF_STATE, 0};

#define PARAM_MOD_SUSTAIN 25
CMachineParameter const param_mod_sustain = {"MOD - Sustain", "MOD - Sustain", 0, 256, MPF_STATE, 0};

#define PARAM_MOD_LENGTH 26
CMachineParameter const param_mod_length = {"MOD - Length", "MOD - Length", -1, 1024, MPF_STATE, -1};

#define PARAM_MOD_RELEASE 27
CMachineParameter const param_mod_release = {"MOD - Release", "MOD - Release", 0, 1024, MPF_STATE, 0};

#define PARAM_MOD_AMOUNT 28
CMachineParameter const param_mod_amount = {"MOD - Amount", "MOD - Amount", -256, 256, MPF_STATE, 0};

#define PARAM_MOD_DEST 29
CMachineParameter const param_mod_dest = {"MOD - Dest.", "MOD - Dest.", 0, MOD_MAX_DEST, MPF_STATE, 0};

#define PARAM_LFO1_WAVE 30
CMachineParameter const param_lfo1_wave = {"LFO 1 - Wave", "LFO 1 - Wave", 0, 4, MPF_STATE, 0};

#define PARAM_LFO1_RATE 31
CMachineParameter const param_lfo1_rate = {"LFO 1 - Rate", "LFO 1 - Rate", 0, 512, MPF_STATE, 0};

#define PARAM_LFO1_AMOUNT 32
CMachineParameter const param_lfo1_amount = {"LFO 1 - Amount", "LFO 1 - Amount", -256, 256, MPF_STATE, 0};

#define PARAM_LFO1_DEST 33
CMachineParameter const param_lfo1_dest = {"LFO 1 - Dest.", "LFO 1 - Dest.", 0, LFO1_MAX_DEST, MPF_STATE, 0};

#define PARAM_LFO2_DELAY 34
CMachineParameter const param_lfo2_delay = {"LFO 2 - Delay", "LFO 2 - Delay", 0, 1024, MPF_STATE, 0};

#define PARAM_LFO2_RATE 35
CMachineParameter const param_lfo2_rate = {"LFO 2 - Rate", "LFO 2 - Rate", 0, 512, MPF_STATE, 0};

#define PARAM_LFO2_AMOUNT 36
CMachineParameter const param_lfo2_amount = {"LFO 2 - Amount", "LFO 2 - Amount", -256, 256, MPF_STATE, 0};

#define PARAM_LFO2_DEST 37
CMachineParameter const param_lfo2_dest = {"LFO 2 - Dest.", "LFO 2 - Dest.", 0, LFO2_MAX_DEST, MPF_STATE, 0};

#define PARAM_VCF_DELAY 38
CMachineParameter const param_vcf_delay = {"VCF - Delay", "VCF - Delay", 0, 1024, MPF_STATE, 0};

#define PARAM_VCF_ATTACK 39
CMachineParameter const param_vcf_attack = {"VCF - Attack", "VCF - Attack", 0, 1024, MPF_STATE, 0};

#define PARAM_VCF_DECAY 40
CMachineParameter const param_vcf_decay = {"VCF - Decay", "VCF - Decay", 0, 1024, MPF_STATE, 0};

#define PARAM_VCF_SUSTAIN 41
CMachineParameter const param_vcf_sustain = { "VCF - Sustain", "VCF - Sustain", 0, 256, MPF_STATE, 0};

#define PARAM_VCF_LENGTH 42
CMachineParameter const param_vcf_length = {"VCF - Length", "VCF - Length", -1, 1024, MPF_STATE, -1};

#define PARAM_VCF_RELEASE 43
CMachineParameter const param_vcf_release = {"VCF - Release", "VCF - Release", 0, 1024, MPF_STATE, 0};

#define PARAM_VCF_AMOUNT 44
CMachineParameter const param_vcf_amount = {"VCF - Amount", "VCF - Amount", -256, 256, MPF_STATE, 0};

#define PARAM_FLT1_TYPE 45
CMachineParameter const param_flt1_type = {"FLT 1 - Type", "FLT 1 - Type", 0, 4, MPF_STATE, 0};

#define PARAM_FLT1_FREQ 46
CMachineParameter const param_flt1_freq = {"FLT 1 - Freq.", "FLT 1 - Freq.", 0, 256, MPF_STATE, 256};

#define PARAM_FLT1_Q 47
CMachineParameter const param_flt1_q = {"FLT 1 - Q", "FLT 1 - Q", 0, 256, MPF_STATE, 0};

#define PARAM_FLT1_KBD_TRACK 48
CMachineParameter const param_flt1_kbd_track = {"FLT 1 - Kbd.Track", "FLT 1 - Kbd.Track", 0, 256, MPF_STATE, 0};

#define PARAM_FLT2_MODE 49
CMachineParameter const param_flt2_mode = {"FLT 2 - Mode", "FLT 2 - Mode", 0, 2, MPF_STATE, 0};

#define PARAM_FLT2_FREQ 50
CMachineParameter const param_flt2_freq = {"FLT 2 - Freq.", "FLT 2 - Freq.", 0, 256, MPF_STATE, 0};

#define PARAM_FLT2_Q 51
CMachineParameter const param_flt2_q = {"FLT 2 - Q", "FLT 2 - Q", 0, 256, MPF_STATE, 0};

#define PARAM_VCA_ATTACK 52
CMachineParameter const param_vca_attack = {"VCA - Attack", "VCA - Attack", 0, 1024, MPF_STATE, 1};

#define PARAM_VCA_DECAY 53
CMachineParameter const param_vca_decay = {"VCA - Decay", "VCA - Decay", 0, 1024, MPF_STATE, 0};

#define PARAM_VCA_SUSTAIN 54
CMachineParameter const param_vca_sustain = {"VCA - Sustain", "VCA - Sustain", 0, 256, MPF_STATE, 256};

#define PARAM_VCA_LENGTH 55
CMachineParameter const param_vca_length = {"VCA - Length", "VCA - Length", -1, 1024, MPF_STATE, -1};

#define PARAM_VCA_RELEASE 56
CMachineParameter const param_vca_release = {"VCA - Release", "VCA - Release", 0, 1024, MPF_STATE, 4};

#define PARAM_AMP_LEVEL 57
CMachineParameter const param_amp_level = {"AMP - Level", "AMP - Level", 0, 512, MPF_STATE, 256};

#define PARAM_GLIDE 58
CMachineParameter const param_glide = {"Glide", "Glide", 0, 256, MPF_STATE, 0};

#define PARAM_INERTIA 59
CMachineParameter const param_inertia = {"Inertia", "Inertia", 0, 1024, MPF_STATE, 16};

//////////////////////////////////////////////////////////////////////
//
//				Machine Parameter List
//
//////////////////////////////////////////////////////////////////////
CMachineParameter const *pParams[] =
{
	&param_osc1_wave,
	&param_osc1_phase,
	&param_osc1_semi,
	&param_osc1_cent,
	&param_osc1_pm_type,
	&param_osc1_pm_amount,
	&param_osc1_kbd_track,
	&param_osc2_wave,
	&param_osc2_phase,
	&param_osc2_semi,
	&param_osc2_cent,
	&param_osc2_pm_type,
	&param_osc2_pm_amount,
	&param_osc2_kbd_track,
	&param_osc_phase_rand,
	&param_osc_fm,
	&param_osc_pm,
	&param_osc_mix,
	&param_osc_ring_mod,
	&param_noise_decay,
	&param_noise_color,
	&param_noise_level,
	&param_mod_delay,
	&param_mod_attack,
	&param_mod_decay,
	&param_mod_sustain,
	&param_mod_length,
	&param_mod_release,
	&param_mod_amount,
	&param_mod_dest,
	&param_lfo1_wave,
	&param_lfo1_rate,
	&param_lfo1_amount,
	&param_lfo1_dest,
	&param_lfo2_delay,
	&param_lfo2_rate,
	&param_lfo2_amount,
	&param_lfo2_dest,
	&param_vcf_delay,
	&param_vcf_attack,
	&param_vcf_decay,
	&param_vcf_sustain,
	&param_vcf_length,
	&param_vcf_release,
	&param_vcf_amount,
	&param_flt1_type,
	&param_flt1_freq,
	&param_flt1_q,
	&param_flt1_kbd_track,
	&param_flt2_mode,
	&param_flt2_freq,
	&param_flt2_q,
	&param_vca_attack,
	&param_vca_decay,
	&param_vca_sustain,
	&param_vca_length,
	&param_vca_release,
	&param_amp_level,
	&param_glide,
	&param_inertia
};
//////////////////////////////////////////////////////////////////////
//
//				Machine Info
//
//////////////////////////////////////////////////////////////////////
CMachineInfo MacInfo (
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
	NUM_PARAM_COLS
);
//////////////////////////////////////////////////////////////////////
//
//				Machine Class
//
//////////////////////////////////////////////////////////////////////
class mi : public CMachineInterface
{
	//////////////////////////////////////////////////////////////////
	//
	//				Machine Variables
	//
	//////////////////////////////////////////////////////////////////
private:
	// Static Generic
	static int m_instances;
	// Static Custom

	// Instance Generic
	Globals m_globals;
	SharedBuffers m_buffers;
	Voice m_voices[MAX_TRACKS][MAX_TRACK_POLY];
	int m_nvoices[MAX_TRACKS];
	// Instance Custom
	//////////////////////////////////////////////////////////////////
	//
	//				Machine Methods
	//
	//////////////////////////////////////////////////////////////////
public:
	// Generic
	mi();
	virtual ~mi();
	virtual void Load();
	virtual void Unload();
	virtual void Command();
	virtual void Init();
	virtual void Stop();
	virtual void ParameterTweak(int par, int val);
	virtual bool DescribeValue(char* txt,int const param, int const value);
	virtual void SequencerTick();
	virtual void SeqTick(int chan, int note, int inst, int cmd, int val);
	virtual void Work(float *psamplesleft, float *psamplesright , int numsamples, int numtracks);
	// Custom
};
PSYCLE__PLUGIN__INSTANTIATOR("sublime", mi, MacInfo)
//////////////////////////////////////////////////////////////////////
//
//				Initialize Static Machine Variables
//
//////////////////////////////////////////////////////////////////////
// Static Generic
int mi::m_instances = 0;
//////////////////////////////////////////////////////////////////////
//
//				Machine Constructor
//
//////////////////////////////////////////////////////////////////////
mi::mi()
{
	Vals = new int[MacInfo.numParameters];
	for (int t = 0; t < MAX_TRACKS; t++)
	{
		for (int v = 0; v < MAX_TRACK_POLY; v++)
		{
			m_voices[t][v].m_globals = &m_globals;
			m_voices[t][v].m_buffers = &m_buffers;
		}
		m_nvoices[t] = 0;
	}
	if (mi::m_instances == 0)
		Load();
	mi::m_instances++;
	Stop();
}
//////////////////////////////////////////////////////////////////////
//
//				Machine Destructor
//
//////////////////////////////////////////////////////////////////////
mi::~mi()
{
	delete[] Vals;
	mi::m_instances--;
	if (mi::m_instances == 0)
		Unload();
}
//////////////////////////////////////////////////////////////////////
//
//				Load
//
//////////////////////////////////////////////////////////////////////
void mi::Load()
{
	InitializeDSP();
}
//////////////////////////////////////////////////////////////////////
//
//				load
//
//////////////////////////////////////////////////////////////////////
void mi::Unload()
{
	DestroyDSP();
}
//////////////////////////////////////////////////////////////////////
//
//				Command
//
//////////////////////////////////////////////////////////////////////
void mi::Command()
{
	pCB->MessBox(
		"Command decription"
		"\n"
		"0Cxx - Velocity\n"
		"0Dxx - Glide ( based on 1/4 of a tick )\n"
		,
		MAC_AUTHOR " " MAC_NAME " v." MAC_VERSION
		,
		0
	);
}
//////////////////////////////////////////////////////////////////////
//
//				Init
//
//////////////////////////////////////////////////////////////////////
void mi::Init()
{
	int sr = pCB->GetSamplingRate();
	m_globals.m_samplingrate = sr;
	UpdateWaveforms(sr);

	m_globals.m_lfo2_wave.Get(lfowaves[0]);
	Stop();
}
//////////////////////////////////////////////////////////////////////
//
//				Stop
//
//////////////////////////////////////////////////////////////////////
void mi::Stop()
{
	for (int t = 0; t < MAX_TRACKS; t++)
	{
		for (int v = 0; v < MAX_TRACK_POLY; v++)
		{
			m_voices[t][v].Reset();
		}
		m_nvoices[t] = 0;
	}
}
//============================================================================
//				SequencerTick
//============================================================================
void mi::SequencerTick()
{
	m_globals.m_ticklength = pCB->GetTickLength();

	if (pCB->GetSamplingRate() != m_globals.m_samplingrate) {
		int sr = pCB->GetSamplingRate();
		m_globals.m_samplingrate = sr;
		UpdateWaveforms(sr);
		//Waves need to be set again, since array is reallocated.
		m_globals.m_osc1_wave.Get(oscwaves[Vals[PARAM_OSC1_WAVE]]);
		m_globals.m_osc2_wave.Get(oscwaves[Vals[PARAM_OSC2_WAVE]]);
		m_globals.m_lfo1_wave.Get(lfowaves[Vals[PARAM_LFO1_WAVE]]);
		m_globals.m_lfo2_wave.Get(lfowaves[0]);
		m_globals.m_lfo1_incr.SetTarget((float) Vals[PARAM_LFO1_RATE] * WAVESIZE / (12.8f * m_globals.m_samplingrate));
		m_globals.m_lfo2_incr.SetTarget((float) Vals[PARAM_LFO2_RATE] * WAVESIZE / (12.8f * m_globals.m_samplingrate));
	}
}
//////////////////////////////////////////////////////////////////////
//
//				ParameterTweak
//
//////////////////////////////////////////////////////////////////////
void mi::ParameterTweak(int par, int val)
{
	int tmp;

	Vals[par] = val;

	switch (par)
	{
	case PARAM_OSC1_WAVE :
		m_globals.m_osc1_wave.Get(oscwaves[val]);
		m_globals.m_osc1_vowelnum = (val > 5 ? val - 6 : -1);
		break;
	case PARAM_OSC1_PHASE :
		m_globals.m_osc1_phase = (float) val * WAVEFSIZE / 256.0f;
		break;
	case PARAM_OSC1_SEMI :
	case PARAM_OSC1_CENT :
		m_globals.m_osc1_tune = (float) Vals[PARAM_OSC1_SEMI] + (float) Vals[PARAM_OSC1_CENT] / 128.0f;
		break;
	case PARAM_OSC1_PM_TYPE :
		m_globals.m_osc1_pm_type = val;
		break;
	case PARAM_OSC1_PM_AMOUNT :
		m_globals.m_osc1_pm_amount.SetTarget((float) val * WAVEFSIZE / 256.0f);
		break;
	case PARAM_OSC1_KBD_TRACK :
		m_globals.m_osc1_kbd_track = (val ? true : false);
		break;
	case PARAM_OSC2_WAVE :
		m_globals.m_osc2_wave.Get(oscwaves[val]);
		m_globals.m_osc2_vowelnum = (val > 5 ? val - 6 : -1);
		break;
	case PARAM_OSC2_PHASE :
		m_globals.m_osc2_phase = (float) val * WAVEFSIZE / 256.0f;
		break;
	case PARAM_OSC2_SEMI :
	case PARAM_OSC2_CENT :
		m_globals.m_osc2_tune = (float) Vals[PARAM_OSC2_SEMI] + (float) Vals[PARAM_OSC2_CENT] / 128.0f;
		break;
	case PARAM_OSC2_PM_TYPE :
		m_globals.m_osc2_pm_type = val;
		break;
	case PARAM_OSC2_PM_AMOUNT :
		m_globals.m_osc2_pm_amount.SetTarget((float) val * WAVEFSIZE / 256.0f);
		break;
	case PARAM_OSC2_KBD_TRACK :
		m_globals.m_osc2_kbd_track = (val ? true : false);
		break;
	case PARAM_OSC_PHASE_RAND :
		m_globals.m_phase_rand = (float) val * WAVEFSIZE / 256.0f;
		break;
	case PARAM_OSC_FM :
		m_globals.m_osc_fm.SetTarget((float) val / 256.0f);
		break;
	case PARAM_OSC_PM :
		m_globals.m_osc_pm.SetTarget((float) val / 256.0f);
		break;
	case PARAM_OSC_MIX :
		m_globals.m_osc_mix.SetTarget((float) val / 256.0f);
		break;
	case PARAM_OSC_RING_MOD :
		m_globals.m_osc_ring_mod = (val ? true : false);
		break;
	case PARAM_NOISE_DECAY :
		m_globals.m_noise_decay = val * 4;
		break;
	case PARAM_NOISE_COLOR :
		m_globals.m_noise_color.SetTarget((float) val / 256.0f);
		break;
	case PARAM_NOISE_LEVEL :
		m_globals.m_noise_level.SetTarget((float) val / 256.0f);
		break;
	case PARAM_MOD_DELAY :
		m_globals.m_mod_delay = val * 4;
		break;
	case PARAM_MOD_ATTACK :
		m_globals.m_mod_attack = val * 4;
		break;
	case PARAM_MOD_DECAY :
		m_globals.m_mod_decay = val * 4;
		break;
	case PARAM_MOD_SUSTAIN :
		m_globals.m_mod_sustain = (float) val / 256.0f;
		break;
	case PARAM_MOD_LENGTH :
		m_globals.m_mod_length = val * 4;
		break;
	case PARAM_MOD_RELEASE :
		m_globals.m_mod_release = val * 4;
		break;
	case PARAM_MOD_AMOUNT :
		m_globals.m_mod_amount.SetTarget((float) val / 128.0f);
		break;
	case PARAM_MOD_DEST :
		m_globals.m_mod_dest = val;
		m_globals.m_mod_amount.Invalidate();
		break;
	case PARAM_LFO1_WAVE :
		m_globals.m_lfo1_wave.Get(lfowaves[val]);
		break;
	case PARAM_LFO1_RATE :
		m_globals.m_lfo1_incr.SetTarget((float) val * WAVESIZE / (12.8f * m_globals.m_samplingrate));
		break;
	case PARAM_LFO1_AMOUNT :
		m_globals.m_lfo1_amount.SetTarget((float) val / 128.0f);
		break;
	case PARAM_LFO1_DEST :
		m_globals.m_lfo1_dest = val;
		m_globals.m_lfo1_amount.Invalidate();
		break;
	case PARAM_LFO2_DELAY :
		m_globals.m_lfo2_delay = val * 4;
		break;
	case PARAM_LFO2_RATE :
		m_globals.m_lfo2_incr.SetTarget((float) val * WAVESIZE / (12.8f * m_globals.m_samplingrate));
		break;
	case PARAM_LFO2_AMOUNT :
		m_globals.m_lfo2_amount.SetTarget((float) val / 128.0f);
		break;
	case PARAM_LFO2_DEST :
		m_globals.m_lfo2_dest = val;
		m_globals.m_lfo2_amount.Invalidate();
		break;
	case PARAM_VCF_DELAY :
		m_globals.m_vcf_delay = val * 4;
		break;
	case PARAM_VCF_ATTACK :
		m_globals.m_vcf_attack = val * 4;
		break;
	case PARAM_VCF_DECAY :
		m_globals.m_vcf_decay = val * 4;
		break;
	case PARAM_VCF_SUSTAIN :
		m_globals.m_vcf_sustain = (float) val / 256.0f;
		break;
	case PARAM_VCF_LENGTH :
		m_globals.m_vcf_length = val * 4;
		break;
	case PARAM_VCF_RELEASE :
		m_globals.m_vcf_release = val * 4;
		break;
	case PARAM_VCF_AMOUNT :
		m_globals.m_vcf_amount.SetTarget((float) val / 128.0f);
		break;
	case PARAM_FLT1_TYPE :
		m_globals.m_flt1_type = val;
		break;
	case PARAM_FLT1_FREQ :
		m_globals.m_flt1_freq.SetTarget((float) val/m_globals.m_samplingrate * (44100.f/256.0f) );
		break;
	case PARAM_FLT1_Q :
		m_globals.m_flt1_q.SetTarget((float) val / 256.0f);
		break;
	case PARAM_FLT1_KBD_TRACK :
		m_globals.m_flt1_kbd_track = (float) val / 128.0f;
		break;
	case PARAM_FLT2_MODE :
		m_globals.m_flt2_mode = val;
		break;
	case PARAM_FLT2_FREQ :
		m_globals.m_flt2_freq.SetTarget((float) val/m_globals.m_samplingrate * (44100.f/256.0f) );
		break;
	case PARAM_FLT2_Q :
		m_globals.m_flt2_q.SetTarget((float) val / 256.0f);
		break;
	case PARAM_VCA_ATTACK :
		m_globals.m_vca_attack = val * 4;
		break;
	case PARAM_VCA_DECAY :
		m_globals.m_vca_decay = val * 4;
		break;
	case PARAM_VCA_SUSTAIN :
		m_globals.m_vca_sustain = (float) val / 256.0f;
		break;
	case PARAM_VCA_LENGTH :
		m_globals.m_vca_length = val * 4;
		break;
	case PARAM_VCA_RELEASE :
		m_globals.m_vca_release = val * 4;
		break;
	case PARAM_AMP_LEVEL :
		m_globals.m_amp_level.SetTarget((float) val * 64);
		break;
	case PARAM_GLIDE :
		m_globals.m_glide = val;
		break;
	case PARAM_INERTIA :
		m_globals.m_inertia = val * 4;
		tmp = millis2samples(m_globals.m_inertia, m_globals.m_samplingrate);
		m_globals.m_osc1_pm_amount.SetLength(tmp);
		m_globals.m_osc2_pm_amount.SetLength(tmp);
		m_globals.m_osc_fm.SetLength(tmp);
		m_globals.m_osc_pm.SetLength(tmp);
		m_globals.m_osc_mix.SetLength(tmp);
		m_globals.m_noise_color.SetLength(tmp);
		m_globals.m_noise_level.SetLength(tmp);
		m_globals.m_mod_amount.SetLength(tmp);
		m_globals.m_lfo1_amount.SetLength(tmp);
		m_globals.m_lfo1_incr.SetLength(tmp);
		m_globals.m_lfo2_amount.SetLength(tmp);
		m_globals.m_lfo2_incr.SetLength(tmp);
		m_globals.m_vcf_amount.SetLength(tmp);
		m_globals.m_flt1_freq.SetLength(tmp);
		m_globals.m_flt1_q.SetLength(tmp);
		m_globals.m_flt2_freq.SetLength(tmp);
		m_globals.m_flt2_q.SetLength(tmp);
		m_globals.m_amp_level.SetLength(tmp);
		break;
	}
}
//////////////////////////////////////////////////////////////////////
//
//				DescribeValue
//
//////////////////////////////////////////////////////////////////////
bool mi::DescribeValue(char* txt,int const param, int const value)
{
	WAVEFORM wf;
	int tmp;
	float ftmp;
	switch (param)
	{
	case PARAM_OSC1_WAVE :
	case PARAM_OSC2_WAVE :
		if (value > 5)
		{
			sprintf(txt, "Vowel - %s", Formant::GetVowelName(value - 6));
		}
		else
		{
			wf.index = oscwaves[value];
			GetWaveInfo(&wf);
			sprintf(txt, "%s", wf.pname);
		}
		return true;
	case PARAM_LFO1_WAVE :
		wf.index = lfowaves[value];
		GetWaveInfo(&wf);
		sprintf(txt, "%s", wf.pname);
		return true;
	case PARAM_OSC1_PHASE :
	case PARAM_OSC2_PHASE :
		sprintf(txt, "%.0f degrees", 360.0f * (float) value / 256.0f);
		return true;
	case PARAM_OSC1_PM_AMOUNT :
	case PARAM_OSC2_PM_AMOUNT :
	case PARAM_OSC_PHASE_RAND :
	case PARAM_OSC_FM :
	case PARAM_OSC_PM :
	case PARAM_NOISE_COLOR :
	case PARAM_NOISE_LEVEL :
	case PARAM_MOD_SUSTAIN :
	case PARAM_VCF_SUSTAIN :
	case PARAM_FLT1_Q :
	case PARAM_FLT2_Q :
	case PARAM_FLT1_KBD_TRACK :
		sprintf(txt, "%.2f %%", 100.0f * (float) value / 256.0f);
		return true;
	case PARAM_VCA_SUSTAIN :
		if (value > 0) sprintf(txt, "%.02f dB", 20.0f * std::log10((float) value / 256.0f * Vals[PARAM_AMP_LEVEL]/512.f));
		else sprintf(txt, "-inf dB");
		return true;
	case PARAM_AMP_LEVEL :
		if (value > 0) sprintf(txt, "%.02f dB", 20.0f * std::log10((float) value / 512.0f));
		else sprintf(txt, "-inf dB");
		return true;
	case PARAM_OSC1_SEMI :
	case PARAM_OSC2_SEMI :
		tmp = value / 12;
		sprintf(txt, "%d oct %d semis", tmp, value - tmp * 12);
		return true;
	case PARAM_OSC1_CENT :
	case PARAM_OSC2_CENT :
		sprintf(txt, "%.0f cents", 100.0f * (float) value / 128.0f);
		return true;
	case PARAM_MOD_AMOUNT :
	case PARAM_LFO1_AMOUNT :
	case PARAM_LFO2_AMOUNT :
	case PARAM_VCF_AMOUNT :
		sprintf(txt, "%.2f %%", 100.0f * (float) value / 128.0f);
		return true;
	case PARAM_OSC1_PM_TYPE :
	case PARAM_OSC2_PM_TYPE :
		sprintf(txt, "%s", str_pm_types[value]);
		return true;
	case PARAM_OSC1_KBD_TRACK :
	case PARAM_OSC2_KBD_TRACK :
	case PARAM_OSC_RING_MOD :
		sprintf(txt, "%s", str_yes_no[value]);
		return true;
	case PARAM_OSC_MIX :
		ftmp = 100.0f * (float) value / 256.0f;
		sprintf(txt, "%.2f : %.2f", 100.0f - ftmp, ftmp);
		return true;
	case PARAM_NOISE_DECAY :
	case PARAM_MOD_DELAY :
	case PARAM_MOD_ATTACK :
	case PARAM_MOD_DECAY :
	case PARAM_MOD_RELEASE :
	case PARAM_LFO2_DELAY :
	case PARAM_VCF_DELAY :
	case PARAM_VCF_ATTACK :
	case PARAM_VCF_DECAY :
	case PARAM_VCF_RELEASE :
	case PARAM_VCA_ATTACK :
	case PARAM_VCA_DECAY :
	case PARAM_VCA_RELEASE :
	case PARAM_INERTIA :
		sprintf(txt, "%.0f ms", (float) value * 4.0f);
		return true;
	case PARAM_GLIDE :
		sprintf(txt, "%.2f ticks", (float) value * 0.25f);
		return true;
	case PARAM_MOD_LENGTH :
	case PARAM_VCF_LENGTH :
	case PARAM_VCA_LENGTH :
		if (value > -1)
		{
			sprintf(txt, "%.0f ms", (float) value * 4.0f);
		}
		else
		{
			sprintf(txt, "until noteoff");
		}
		return true;
	case PARAM_MOD_DEST :
		sprintf(txt, "%s", str_mod_dests[value]);
		return true;
	case PARAM_LFO1_RATE :
	case PARAM_LFO2_RATE :
		sprintf(txt, "%.2f Hz", (float) value * 44100.0f / 256.0f / 2205.0f);
		return true;
	case PARAM_LFO1_DEST :
		sprintf(txt, "%s", str_lfo1_dests[value]);
		return true;
	case PARAM_LFO2_DEST :
		sprintf(txt, "%s", str_lfo2_dests[value]);
		return true;
	case PARAM_FLT1_TYPE :
		sprintf(txt, "%s", str_flt1_types[value]);
		return true;
	case PARAM_FLT2_MODE :
		sprintf(txt, "%s", str_flt2_modes[value]);
		return true;
	case PARAM_FLT1_FREQ :
	case PARAM_FLT2_FREQ :
		// All them shoud be a bit logarithmic towards the end.
		if(Vals[PARAM_FLT1_TYPE] == 0 || Vals[PARAM_FLT1_TYPE] == 3) {
			std::sprintf(txt,"%.0f Hz",4000.f*(float)value/144.f);
		} else if(Vals[PARAM_FLT1_TYPE] == 1 || Vals[PARAM_FLT1_TYPE] == 4) {
			std::sprintf(txt,"%.0f Hz",4000.f*(float)value/100.f);
		} else if(Vals[PARAM_FLT1_TYPE] == 4) {
			std::sprintf(txt,"%.0f Hz",15000.f*(float)value/203.f);
		} else {
			std::sprintf(txt,"%.0f Hz",8000.f*(float)value/158.f);
		}
		return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////
//
//				SequencerTick
//
//////////////////////////////////////////////////////////////////////
void mi::SeqTick(int track, int note, int inst, int cmd, int val)
{
	if (m_nvoices[track] == MAX_TRACK_POLY)
	{
		for (int i = 1; i < MAX_TRACK_POLY; i++)
			m_voices[track][i - 1] = m_voices[track][i];
		m_nvoices[track]--;
	}
	Voice *old = (m_nvoices[track] > 0 ? &m_voices[track][m_nvoices[track] - 1] : 0);
	if (m_voices[track][m_nvoices[track]].NoteOn(old, note, cmd, val))
		m_nvoices[track]++;
}
//////////////////////////////////////////////////////////////////////
//
//				Work
//
//////////////////////////////////////////////////////////////////////
void mi::Work(float *psamplesleft, float *psamplesright, int numsamples, int numtracks)
{
	//////////////////////////////////////////////////////////////////
	//
	//				Inertia params
	//
	//////////////////////////////////////////////////////////////////
	m_globals.HandleInertia(numsamples);
	//////////////////////////////////////////////////////////////////
	//
	//				Clear main out
	//
	//////////////////////////////////////////////////////////////////
	Fill(m_buffers.m_out, 0.0f, numsamples);
	//////////////////////////////////////////////////////////////////
	//
	//				Render voices
	//
	//////////////////////////////////////////////////////////////////
	int nvoices;
	for (int t = 0; t < numtracks; t++)
	{
		nvoices = m_nvoices[t];
		for (int v = nvoices - 1; v >= 0; v--)
		{
			if (m_voices[t][v].IsIdle())
			{
				for (int i = v + 1; i < nvoices; i++)
					m_voices[t][i - 1] = m_voices[t][i];
				m_voices[t][nvoices - 1].Reset();
				nvoices--;
			}
			else
			{
				m_voices[t][v].Work(m_buffers.m_out, numsamples);
			}
		}
		m_nvoices[t] = nvoices;
	}
	//////////////////////////////////////////////////////////////////
	//
	//				Mix amp
	//
	//////////////////////////////////////////////////////////////////
	float *pout = m_buffers.m_out - 1;
	--psamplesleft;
	--psamplesright;
	do
	{
		*++psamplesleft = *++psamplesright = *++pout;
	}
	while (--numsamples);
}
}