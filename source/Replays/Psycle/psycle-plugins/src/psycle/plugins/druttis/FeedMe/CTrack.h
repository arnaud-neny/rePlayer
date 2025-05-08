//============================================================================
//
//				CTrack.h
//				--------
//				druttis@darkface.pp.se
//
//============================================================================
#pragma once
#include "../dsp/CDsp.h"
#include "../dsp/CEnvelope.h"
#include "../dsp/wtfmlib.h"
namespace psycle::plugins::druttis::feedme {
//============================================================================
//				Defines
//============================================================================
#define NUMWAVEFORMS 6
#define MAX_OVERTONES 6
#define GLOBAL_TICKS 32
//============================================================================
//				CTrack globals
//============================================================================
struct TRACKDATA
{
	// current sampling rate.
	int					samplingrate;
	//	Index of the waveform table
	int					waveform;
	// unary fraction of feedback
	afloat				feedback;
	// Index of overtone table
	int					overtype;
	// phase offset for the left channel. It is automated directly in the CTrack's work funcion!
	// scale 0 to WAVESIZE-1
	afloat				phase;
	//todo: afloat?
	float				chorus;
	// Index of the waveform table
	int					distform;
	// unary fraction of distortion amount.
	afloat				distortion;
	// amplitude attack time in samples
	int					attack;
	// amplitude release time in samples
	int					release;
	//todo: afloat?
	// unary fraction of amplitude of the waveform
	float				amplitude;
	// speed (amount of samples to increase per sample to increase vibrato_pos)  multiplied by GLOBAL_TICKS
	float				vibrato_rate;
	//todo: afloat?
	// unary fraction of amplitude of the vibrato.
	float				vibrato_amount;
	// Vibrato attack time in samples divided by GLOBAL_TICKS
	int					vibrato_delay;
	// filter attack time in samples  divided by GLOBAL_TICKS
	int					vcf_attack;
	// filter decay time in samples  divided by GLOBAL_TICKS
	int					vcf_decay;
	// unary fraction of filter amplitude at sustain point.
	float				vcf_sustain;
	// filter release time in samples  divided by GLOBAL_TICKS
	int					vcf_release;
	//todo: not afloat?
	// unary fraction of filter envelope amount.
	afloat				vcf_amount;
	// index of filter type
	int					filter_type;
	// unary fraction of filter frequency, at current samplerate
	afloat				filter_freq;
	// unary fraction of filter ressonance.
	afloat				filter_res;
	// samples of param change inertia, divided by GLOBAL_TICKS
	int					inertia;
	// note cut delay time in samples divided by GLOBAL_TICKS
	int					note_cut;
	// sync_mode bitwise operator.
	int					sync_mode;
};
//============================================================================
//				CTrack class
//============================================================================
class CTrack
{
	//------------------------------------------------------------------------
	//				Static variables
	//------------------------------------------------------------------------
private:
	static float				wavetable[NUMWAVEFORMS][WAVESIZE];
	static float				overtonemults[MAX_OVERTONES];
	//------------------------------------------------------------------------
	//				Data
	//------------------------------------------------------------------------
	//				Globals
public:
	TRACKDATA*					globals;
	//				Osc 1
	// floating point position of the left channel, in samples.
	float						osc1_pos;
	// speed (amount of samples per sample to increase osc1_pos)
	float						osc1_speed;
	// target speed (used in conjunction with slide)
	float						osc1_target_speed;
	// overtones output buffers
	float						osc1_out[MAX_OVERTONES + 1];
	//				Osc 2
	// floating point position of the right channel, in samples.
	float						osc2_pos;
	// speed (amount of samples per sample to increase osc2_pos)
	float						osc2_speed;
	// target speed (used in conjunction with slide)
	float						osc2_target_speed;
	// overtones output buffers
	float						osc2_out[MAX_OVERTONES + 1];
	// unary fraction of how much to increase oscx_speed to reach osc2_target. (each GLOBAL_TICKS samples)
	float						slide_speed;
	//				Vibrato
	// floating point position of the vibrato osci, in samples.
	float						vibrato_pos;
	// speed of the vibrato for osc1 (to add to osc1_speed)
	float						vibrato_osc1_speed;
	// speed of the vibrato for osc2 (to add to osc2_speed)
	float						vibrato_osc2_speed;
	//	amount of vibrato (used by the vibrato attack time).
	float						vibrato_dtime;
	//				VCA
	// amplitude envelope
	CEnvelope					vca_env;
	//				VCF & filter
	// filter envelope
	CEnvelope					vcf_env;
	FILTER						vcf_data1;
	FILTER						vcf_data2;
	//	scaled amplitude multiplier, prepared for direct output
	float						amplitude;
	// amount of samples before note cut divided by GLOBAL_TICKS.
	int							note_cut;
	//	amount of samples before the next recalculation of variables.
	int							ticks_remaining;
	//------------------------------------------------------------------------
	//				Methods
	//------------------------------------------------------------------------
public:
	CTrack();
	~CTrack();
	static void Init();
	static void Destroy();
	void Stop();
	void NoteOff();
	void NoteOn(int note, int volume, bool slide=false);
	//------------------------------------------------------------------------
	//				Setup slide
	//------------------------------------------------------------------------
	void SetFreq(int note)
	{
		osc1_target_speed = CDsp::GetIncrement((float) (note - globals->chorus), WAVESIZE, globals->samplingrate);
		osc2_target_speed = CDsp::GetIncrement((float) (note + globals->chorus), WAVESIZE, globals->samplingrate);
	}
	//------------------------------------------------------------------------
	//				IsFinished
	//------------------------------------------------------------------------
	inline bool IsFinished()
	{
		return vca_env.IsFinished();
	}
	//------------------------------------------------------------------------
	//				GetSampleExp with overtones and feedback
	//------------------------------------------------------------------------
	static inline float GetSample(float* wavetable, int type, float* buf, float fb, float time)
	{
		register float out = 0.0f;
		register float tmp;
		switch (type)
		{
		case 0 :
			tmp = time;
			out += buf[0] = GetWTSample(wavetable, tmp + buf[0] * fb);
			break;
		case 1 :
			tmp = time;
			out += buf[1] = GetWTSample(wavetable, tmp + buf[1] * fb);
			tmp *= 2.0f;
			out += buf[0] = GetWTSample(wavetable, tmp + buf[0] * fb);
			break;
		case 2 :
			tmp = time;
			out += buf[2] = GetWTSample(wavetable, tmp + buf[2] * fb);
			tmp *= 2.0f;
			out += buf[1] = GetWTSample(wavetable, tmp + buf[1] * fb);
			tmp *= 2.0f;
			out += buf[0] = GetWTSample(wavetable, tmp + buf[0] * fb);
			break;
		case 3 :
			tmp = time;
			out += buf[3] = GetWTSample(wavetable, tmp + buf[3] * fb);
			tmp *= 2.0f;
			out += buf[2] = GetWTSample(wavetable, tmp + buf[2] * fb);
			tmp *= 2.0f;
			out += buf[1] = GetWTSample(wavetable, tmp + buf[1] * fb);
			tmp *= 2.0f;
			out += buf[0] = GetWTSample(wavetable, tmp + buf[0] * fb);
			break;
		case 4 :
			tmp = 1;
			out += buf[1] = GetWTSample(wavetable, time * tmp + buf[1] * fb);
			++tmp;
			out += buf[0] = GetWTSample(wavetable, time * tmp + buf[0] * fb);
			break;
		case 5:
			tmp = 1;
			out += buf[2] = GetWTSample(wavetable, time * tmp + buf[2] * fb);
			++tmp;
			out += buf[1] = GetWTSample(wavetable, time * tmp + buf[1] * fb);
			++tmp;
			out += buf[0] = GetWTSample(wavetable, time * tmp + buf[0] * fb);
			break;
		}
		return out * overtonemults[type];
	}

	//============================================================================
	//				Work
	//============================================================================
	inline void Work(float *psamplesleft, float *psamplesright, int numsamples)
	{
		//
		//
		float vca_out;
		// current speed, with vibrato
		float osc1_spd;
		// current speed, with vibrato
		float osc2_spd;
		// current position of osc2, with the offset.
		float osc2_tme;
		// Amount of vibrato currently being applied.
		float vibrato_out;
		int amount;
		register int nsamples;
		float dist;
		float dist2;
		float ndis;
		register float *pleft;
		register float *pright;
		float out1[256];
		float out2[256];

		float *pwaveform = wavetable[globals->waveform];
		float *pdistform = wavetable[globals->distform];

		do {
			//
			//	Tick handling
			if (!ticks_remaining) {
				ticks_remaining = GLOBAL_TICKS;
				//
				//	Handle note cut
				if (note_cut > 0) {
					note_cut--;
					if (!note_cut) {
						NoteOff();
					}
				}
				//
				//	Vibrato
				if (globals->vibrato_delay == 0.0f) {
					vibrato_out = 1.0f;
				} else {
					vibrato_out = vibrato_dtime;
					if (vibrato_dtime < 1.0f) {
						vibrato_dtime += 1.0f / globals->vibrato_delay;
						if (vibrato_dtime > 1.0f)
							vibrato_dtime = 1.0f;
					}
				}
				vibrato_out *= globals->vibrato_amount * GetWTSample(wavetable[0], vibrato_pos);
				vibrato_pos += globals->vibrato_rate;
				while (vibrato_pos >= WAVESIZE)
					vibrato_pos -= WAVESIZE;
				//
				//	Vibrato -> Freq
				vibrato_osc1_speed = vibrato_out * osc1_speed * 0.125f;
				vibrato_osc2_speed = vibrato_out * osc2_speed * 0.125f;
				//
				//	Filter
				float vcf_out = vcf_env.Next() * globals->vcf_amount.current;
				CDsp::InitFilter(&vcf_data1, globals->filter_freq.current + vcf_out, globals->filter_res.current);
				CDsp::InitFilter(&vcf_data2, globals->filter_freq.current + vcf_out, globals->filter_res.current);
				//
				//	Slide
				if (osc1_speed != osc1_target_speed) {
					float dest = (osc1_target_speed - osc1_speed) * slide_speed;
					osc1_speed += dest;
					dest = (osc2_target_speed - osc2_speed) * slide_speed;
					osc2_speed += dest;
				}
			}
			//
			//	Compute samples to render this iteration
			amount = numsamples;
			if (amount > ticks_remaining)
				amount = ticks_remaining;
			numsamples -= amount;
			ticks_remaining -= amount;
			//
			//	Oscilators
			osc1_spd = osc1_speed + vibrato_osc1_speed;
			osc2_spd = osc2_speed + vibrato_osc2_speed;
			osc2_tme = osc2_pos + globals->phase.current;
			//
			pleft = out1 - 1;
			pright = out2 - 1;
			nsamples = amount;
			do
			{
				*++pleft = GetSample(pwaveform, globals->overtype, osc1_out, globals->feedback.current, osc1_pos);
				*++pright = GetSample(pwaveform, globals->overtype, osc2_out, globals->feedback.current, osc2_tme);

				osc1_pos += osc1_spd;
				osc2_tme += osc2_spd;
			}
			while (--nsamples);
			//
			osc2_pos = osc2_tme - globals->phase.current;
			//
			//	Distort
			dist = globals->distortion.current;
			dist2 = dist * WAVESIZE;
			ndis = 1.0f - dist;
			//
			pleft = out1 - 1;
			pright = out2 - 1;
			nsamples = amount;
			do {
				++pleft;
				++pright;
				*pleft = *pleft * ndis + GetWTSample(pdistform, *pleft * dist2) * dist;
				*pright = *pright * ndis + GetWTSample(pdistform, *pright * dist2) * dist;
			} while (--nsamples);
			//
			//	Filter
			switch (globals->filter_type)
			{
				case 0:
					CDsp::LPFilter12(&vcf_data1, out1, amount);
					CDsp::LPFilter12(&vcf_data2, out2, amount);
					break;
				case 1:
					CDsp::LPFilter24(&vcf_data1, out1, amount);
					CDsp::LPFilter24(&vcf_data2, out2, amount);
					break;
				case 4:
					CDsp::LPFilter36(&vcf_data1, out1, amount);
					CDsp::LPFilter36(&vcf_data2, out2, amount);
					break;
				case 2:
					CDsp::HPFilter12(&vcf_data1, out1, amount);
					CDsp::HPFilter12(&vcf_data2, out2, amount);
					break;
				case 3:
					CDsp::HPFilter24(&vcf_data1, out1, amount);
					CDsp::HPFilter24(&vcf_data2, out2, amount);
					break;
			}
			//
			//	Amplify & Output
			pleft = out1 - 1;
			pright = out2 - 1;
			nsamples = amount;
			do
			{
				vca_out = vca_env.Next() * amplitude;
				*++psamplesleft += *++pleft * vca_out;
				*++psamplesright += *++pright * vca_out;
			}
			while (--nsamples);
		}
		while (numsamples);
		//
		//	Limit OSC times
		while (osc1_pos >= WAVESIZE)
			osc1_pos -= WAVESIZE;
		while (osc2_pos >= WAVESIZE)
			osc2_pos -= WAVESIZE;
	}
};
}