//////////////////////////////////////////////////////////////////////
//
//				Globals.h
//
//				druttis@darkface.pp.se
//
//////////////////////////////////////////////////////////////////////
#pragma once
#include "Shared.h"
#include "../dsp/Dsp.h"
#include "../blwtbl/Waveform.h"
namespace psycle::plugins::druttis::sublime {
//////////////////////////////////////////////////////////////////////
//
// PMODES (PM.Type)
//
//////////////////////////////////////////////////////////////////////
#define PMODE_OFF 0
#define PMODE_SUB 1
#define PMODE_ADD 2
#define PMODE_MULT 3
//////////////////////////////////////////////////////////////////////
//
// General Modulation Destinations
//
//////////////////////////////////////////////////////////////////////
#define DST_OFF 0
#define DST_OSC1 1
#define DST_OSC2 2
#define DST_OSC12 3
#define DST_MIX 4
#define DST_FM 5
#define DST_PM 6
#define DST_PHASE 7
#define DST_FLT1_FREQ 8
#define DST_FLT2_FREQ 9
//////////////////////////////////////////////////////////////////////
//
// MOD - Destinations
//
//////////////////////////////////////////////////////////////////////
#define MOD_LFO1 10
#define MOD_LFO2 11
#define MOD_MAX_DEST 11
//////////////////////////////////////////////////////////////////////
// LFO 1 - Destinations
//////////////////////////////////////////////////////////////////////
#define LFO1_MAX_DEST 9
//////////////////////////////////////////////////////////////////////
// LFO 2 - Destinations
//////////////////////////////////////////////////////////////////////
#define LFO2_AMP 10
#define LFO2_MAX_DEST 10
//////////////////////////////////////////////////////////////////////
// FLT 2 - MODES
//////////////////////////////////////////////////////////////////////
#define FLT2_DISABLED 0
#define FLT2_ENABLED 1
#define FLT2_LINKED 2
//////////////////////////////////////////////////////////////////////
//
//				Globals class
//
//////////////////////////////////////////////////////////////////////
class Globals
{
	//////////////////////////////////////////////////////////////////
	// Variables
	//////////////////////////////////////////////////////////////////
public:
	//////////////////////////////////////////////////////////////////
	// Generic stuf
	//////////////////////////////////////////////////////////////////
	int m_samplingrate;
	int m_ticklength;
	//////////////////////////////////////////////////////////////////
	// OSC 1
	//////////////////////////////////////////////////////////////////
	Waveform m_osc1_wave;
	int m_osc1_vowelnum;
	float m_osc1_phase;
	float m_osc1_tune; // semi + cent / 256 (cent = -0.5 to 0.5)
	int m_osc1_pm_type;
	Inertia m_osc1_pm_amount;
	bool m_osc1_kbd_track;
	//////////////////////////////////////////////////////////////////
	// OSC 2
	//////////////////////////////////////////////////////////////////
	Waveform m_osc2_wave;
	int m_osc2_vowelnum;
	float m_osc2_phase;
	float m_osc2_tune; // semi + cent / 256 (cent = -0.5 to 0.5)
	int m_osc2_pm_type;
	Inertia m_osc2_pm_amount;
	bool m_osc2_kbd_track;
	//////////////////////////////////////////////////////////////////
	// OSC STUF
	//////////////////////////////////////////////////////////////////
	float m_phase_rand;
	Inertia m_osc_fm;
	Inertia m_osc_pm;
	Inertia m_osc_mix;
	bool m_osc_ring_mod;
	//////////////////////////////////////////////////////////////////
	// NOISE
	//////////////////////////////////////////////////////////////////
	int m_noise_decay;
	Inertia m_noise_color;
	Inertia m_noise_level;
	//////////////////////////////////////////////////////////////////
	// MOD
	//////////////////////////////////////////////////////////////////
	int m_mod_delay;
	int m_mod_attack;
	int m_mod_decay;
	float m_mod_sustain;
	int m_mod_length;
	int m_mod_release;
	Inertia m_mod_amount;
	int m_mod_dest;
	//////////////////////////////////////////////////////////////////
	// LFO 1
	//////////////////////////////////////////////////////////////////
	Waveform m_lfo1_wave;
	Inertia m_lfo1_incr;
	Inertia m_lfo1_amount;
	int m_lfo1_dest;
	//////////////////////////////////////////////////////////////////
	// LFO 2
	//////////////////////////////////////////////////////////////////
	Waveform m_lfo2_wave;
	int m_lfo2_delay;
	Inertia m_lfo2_incr;
	Inertia m_lfo2_amount;
	int m_lfo2_dest;
	//////////////////////////////////////////////////////////////////
	// VCF
	//////////////////////////////////////////////////////////////////
	int m_vcf_delay;
	int m_vcf_attack;
	int m_vcf_decay;
	float m_vcf_sustain;
	int m_vcf_length;
	int m_vcf_release;
	Inertia m_vcf_amount;
	//////////////////////////////////////////////////////////////////
	// FLT 1
	//////////////////////////////////////////////////////////////////
	int m_flt1_type;
	Inertia m_flt1_freq;
	Inertia m_flt1_q;
	float m_flt1_kbd_track;
	//////////////////////////////////////////////////////////////////
	// FLT 2
	//////////////////////////////////////////////////////////////////
	int m_flt2_mode;
	Inertia m_flt2_freq;
	Inertia m_flt2_q;
	//////////////////////////////////////////////////////////////////
	// VCA
	//////////////////////////////////////////////////////////////////
	int m_vca_attack;
	int m_vca_decay;
	float m_vca_sustain;
	int m_vca_length;
	int m_vca_release;
	//////////////////////////////////////////////////////////////////
	// AMP - Level
	//////////////////////////////////////////////////////////////////
	Inertia m_amp_level;
	//////////////////////////////////////////////////////////////////
	// Glide
	//////////////////////////////////////////////////////////////////
	int m_glide;
	//////////////////////////////////////////////////////////////////
	// Inertia
	//////////////////////////////////////////////////////////////////
	int m_inertia;
	//////////////////////////////////////////////////////////////////
	// Buffers for inertia (calculated on HandleInertia)( TOTAL 19K )
	//////////////////////////////////////////////////////////////////
	float *m_posc1_pm_amount_out;
	float *m_posc2_pm_amount_out;
	float *m_posc_fm_out;
	float *m_posc_pm_out;
	float *m_posc_mix_out;
	float *m_pnoise_color_out;
	float *m_pnoise_level_out;
	float *m_pmod_amount_out;
	float *m_plfo1_amount_out;
	float *m_plfo1_incr_out;
	float *m_plfo2_amount_out;
	float *m_plfo2_incr_out;
	float *m_pvcf_amount_out;
	float *m_pflt1_freq_out;
	float *m_pflt1_q_out;
	float *m_pflt2_freq_out;
	float *m_pflt2_q_out;
	float *m_pamp_level_out;
	//////////////////////////////////////////////////////////////////
	// Flag if buffers have been written to
	//////////////////////////////////////////////////////////////////
	bool m_rendered;
	//////////////////////////////////////////////////////////////////
	// Constructor
	//////////////////////////////////////////////////////////////////
public:
	Globals();
	//////////////////////////////////////////////////////////////////
	// Destructor
	//////////////////////////////////////////////////////////////////
	~Globals();
	//////////////////////////////////////////////////////////////////
	// HandleInertia
	//////////////////////////////////////////////////////////////////
	void HandleInertia(int numsamples);
};
}