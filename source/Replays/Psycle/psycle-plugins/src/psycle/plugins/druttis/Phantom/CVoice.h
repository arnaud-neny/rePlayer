//============================================================================
//
// CVoice.h
// --------
// druttis@darkface.pp.se
//
//============================================================================
#pragma once
#include "../dsp/CDsp.h"
#include "../dsp/CEnvelope.h"
#include "../dsp/Phaser.h"
#include "../dsp/Chorus.h"
#include "../dsp/wtfmlib.h"
#include <cmath>
namespace psycle::plugins::druttis::phantom {
//============================================================================
// Defines
//============================================================================

#define NUMWAVEFORMS 8
#define WAVESIZE 4096
#define WAVEMASK 4095
extern float wavetable[NUMWAVEFORMS][WAVESIZE];

//============================================================================
// Voice globals
//============================================================================

struct GLOBALS
{
	// current sampling rate.
	int samplingrate;

	int osc_wave[6];
	float osc_phase[6];
	float osc_semi[6];
	float osc_fine[6];
	float osc_level[6];
	//
	float vca_attack;
	float vca_decay;
	float vca_sustain;
	float vca_release;
	float amp_level;
	//
	float vcf_attack;
	float vcf_decay;
	float vcf_sustain;
	float vcf_release;
	float vcf_amount;
	//
	int filter_type;
	afloat filter_freq;
	afloat filter_res;
	float filter_increment;
	float filter_amount;
	float buf[1024];

};
//============================================================================
// CVoice class
//============================================================================
class CVoice
{
	//------------------------------------------------------------------------
	//				Data
	//------------------------------------------------------------------------
	//				Globals
public:
	GLOBALS *globals;
	int ticks_remaining;

	//----------------------------------------------------------------
	// Declare your runtime variables here
	//----------------------------------------------------------------

	// Velocity
	float velocity;

	// Oscilator
	float osc_phase[6];
	float osc_increment[6];

	// VCA
	CEnvelope vca;

	// VCF
	CEnvelope vcf;

	// Filter
	FILTER filter;
	float filter_phase;

	double memory[6][10];
	//////////////////////////////////////////////////////////////////
	// Methods
	//////////////////////////////////////////////////////////////////

public:

	CVoice();
	~CVoice();
	void Stop();
	void NoteOff();
	void NoteOn(int note, int volume);

	//////////////////////////////////////////////////////////////////
	// IsFinished
	// returns true if voice is done playing
	//////////////////////////////////////////////////////////////////

	inline bool IsActive()
	{
		return !vca.IsFinished();
	}

	//////////////////////////////////////////////////////////////////
	// GlobalTick
	// Method to handle parameter inertia and suchs things
	//////////////////////////////////////////////////////////////////

	inline static void GlobalTick()
	{
	}

	//////////////////////////////////////////////////////////////////
	// VoiceTick
	// Method to handle voice specific things as LFO and envelopes
	// * tips, dont handle amplitude envelopes or lfo's here
	//////////////////////////////////////////////////////////////////

	inline void VoiceTick()
	{
		//------------------------------------------------------------
		// Setup filter
		//------------------------------------------------------------

		float filter_freq = globals->filter_freq.current;

		// VCF
		filter_freq += (vcf.Next() * globals->vcf_amount);

		// Filter - LFO (rate/amount)
		filter_freq += get_sample_l(wavetable[0], filter_phase, WAVEMASK) * globals->filter_amount;
		filter_phase = fand(filter_phase + globals->filter_increment, WAVEMASK);

		// Init filter
		CDsp::InitFilter(&filter, filter_freq, globals->filter_res.current);

		//------------------------------------------------------------
		// Setup phaser
		//------------------------------------------------------------
	}

	// Work
	// all sound generation is done here
	//////////////////////////////////////////////////////////////////

	inline void Work(float *psamplesleft, float *psamplesright, int numsamples)
	{
		register float out;
		//
		register int amount;
		//
		register float *pbuf;

		//--------------------------------------------------------
		// Osc add
		//--------------------------------------------------------

		amount = numsamples;
		pbuf = globals->buf;
		--pbuf;

		do
		{
			out = get_sample_l(wavetable[globals->osc_wave[0]], osc_phase[0], WAVEMASK) * globals->osc_level[0];
			out += get_sample_l(wavetable[globals->osc_wave[1]], osc_phase[1], WAVEMASK) * globals->osc_level[1];
			out += get_sample_l(wavetable[globals->osc_wave[2]], osc_phase[2], WAVEMASK) * globals->osc_level[2];
			out += get_sample_l(wavetable[globals->osc_wave[3]], osc_phase[3], WAVEMASK) * globals->osc_level[3];
			out += get_sample_l(wavetable[globals->osc_wave[4]], osc_phase[4], WAVEMASK) * globals->osc_level[4];
			out += get_sample_l(wavetable[globals->osc_wave[5]], osc_phase[5], WAVEMASK) * globals->osc_level[5];
			*++pbuf = out;
			osc_phase[0] += osc_increment[0];
			osc_phase[1] += osc_increment[1];
			osc_phase[2] += osc_increment[2];
			osc_phase[3] += osc_increment[3];
			osc_phase[4] += osc_increment[4];
			osc_phase[5] += osc_increment[5];
		}
		while (--amount);

		osc_phase[0] = fand(osc_phase[0], WAVEMASK);
		osc_phase[1] = fand(osc_phase[1], WAVEMASK);
		osc_phase[2] = fand(osc_phase[2], WAVEMASK);
		osc_phase[3] = fand(osc_phase[3], WAVEMASK);
		osc_phase[4] = fand(osc_phase[4], WAVEMASK);
		osc_phase[5] = fand(osc_phase[5], WAVEMASK);
		//------------------------------------------------------------
		// Filter
		//------------------------------------------------------------

		pbuf = globals->buf;

		switch (globals->filter_type)
		{
		case 0:
			CDsp::LPFilter12(&filter, pbuf, numsamples);
			break;
		case 1:
			CDsp::LPFilter24(&filter, pbuf, numsamples);
			break;
		case 4:
			CDsp::LPFilter36(&filter, pbuf, numsamples);
			break;
		case 2:
			CDsp::HPFilter12(&filter, pbuf, numsamples);
			break;
		case 3:
			CDsp::HPFilter24(&filter, pbuf, numsamples);
			break;
		}

		//------------------------------------------------------------
		// Output
		//------------------------------------------------------------

		--pbuf;

		amount = numsamples;

		do {
			//--------------------------------------------------------
			// Amp
			//--------------------------------------------------------

			out = *++pbuf;
			out *= vca.Next();
			out *= globals->amp_level;
			out *= velocity;
			out *= 16384;

			//--------------------------------------------------------
			// Write
			//--------------------------------------------------------

			*++psamplesleft += out;
			*++psamplesright += out;

		} while (--numsamples);
	}
};
}