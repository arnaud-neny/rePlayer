//////////////////////////////////////////////////////////////////////
//
//				Voice.h
//
//				druttis@darkface.pp.se
//
//////////////////////////////////////////////////////////////////////
#pragma once
#include "Globals.h"
namespace psycle::plugins::druttis::sublime {
#define FM(x) (float) exp(x)
//////////////////////////////////////////////////////////////////////
//
//				Voice Class
//
//////////////////////////////////////////////////////////////////////
class Voice
{
	//////////////////////////////////////////////////////////////////
	//
	//				Voice variables
	//
	//////////////////////////////////////////////////////////////////
public:
	Globals				*m_globals;
	SharedBuffers		*m_buffers;
private:
	float				m_velocity;
	//				OSC 1
	Formant				m_osc1_formant;
	float				m_osc1_phase;
	Inertia				m_osc1_incr;
	//				OSC 2
	Formant				m_osc2_formant;
	float				m_osc2_phase;
	Inertia				m_osc2_incr;
	//				NOISE
	Inertia				m_noise_decay;
	//				MOD
	Envelope			m_mod;
	//				LFO 1
	float				m_lfo1_phase;
	//				LFO 2
	Inertia				m_lfo2_delay;
	float				m_lfo2_phase;
	//				VCF
	Envelope			m_vcf;
	//				VCA
	Envelope			m_vca;
	//				NOISE FLT
	float				b0;
	float				b1;
	float				b2;
	//				FLT 1
	Filter				m_flt1;
	//				FLT 2
	Filter				m_flt2;
	//
	//////////////////////////////////////////////////////////////////
	//
	//				Voice Constructor
	//
	//////////////////////////////////////////////////////////////////
public:
	Voice();
	~Voice();
	void Reset();
	void Stop(int samples);
	void NoteOff();
	bool NoteOn(Voice *old, int note, int cmd, int val);
	//////////////////////////////////////////////////////////////////
	//
	//				Returns true if voice is idle
	//
	//////////////////////////////////////////////////////////////////
	inline bool IsIdle()
	{
		return m_vca.IsIdle();
	}
	//////////////////////////////////////////////////////////////////
	//
	//				FillEnv
	//
	//////////////////////////////////////////////////////////////////
	void FillEnv(float *pout, Envelope *penv, float *pamount, int nsamples)
	{
		int amt;
		--pout;
		--pamount;
		do
		{
			amt = penv->Clip(nsamples);
			nsamples -= amt;
			if (amt > 2)
			{
				do
				{
					*++pout = penv->Next() * *++pamount;
					*++pout = penv->Next() * *++pamount;
					amt -= 2;
				}
				while (amt > 2);
			}
			do
			{
				*++pout = penv->Next() * *++pamount;
			}
			while (--amt);
		}
		while (nsamples);
	}
	//////////////////////////////////////////////////////////////////
	//
	//				FillEnv2
	//
	//////////////////////////////////////////////////////////////////
	void FillEnv2(float *pout1, float *pout2, Envelope *penv, float *pamount, int nsamples)
	{
		int amt;
		float out;
		--pout1;
		--pout2;
		--pamount;
		do
		{
			amt = penv->Clip(nsamples);
			nsamples -= amt;
			do
			{
				out = penv->Next() * *++pamount;
				*++pout1 = out;
				*++pout2 = out;
			}
			while (--amt);
		}
		while (nsamples);
	}
	//////////////////////////////////////////////////////////////////
	//
	//				AddEnv
	//
	//////////////////////////////////////////////////////////////////
	void AddEnv(float *pout, float *pin, Envelope *penv, float *pamount, int nsamples)
	{
		int amt;
		--pin;
		--pout;
		--pamount;
		do
		{
			amt = penv->Clip(nsamples);
			nsamples -= amt;
			do
			{
				*++pout = *++pin + penv->Next() * *++pamount;
			}
			while (--amt);
		}
		while (nsamples);
	}
	//////////////////////////////////////////////////////////////////
	//
	//				AddEnv2
	//
	//////////////////////////////////////////////////////////////////
	void AddEnv2(float *pout1, float *pin1, float *pout2, float *pin2, Envelope *penv, float *pamount, int nsamples)
	{
		int amt;
		float out;
		--pin1;
		--pin2;
		--pout1;
		--pout2;
		--pamount;
		do
		{
			amt = penv->Clip(nsamples);
			nsamples -= amt;
			do
			{
				out = penv->Next() * *++pamount;
				*++pout1 = *++pin1 + out;
				*++pout2 = *++pin2 + out;
			}
			while (--amt);
		}
		while (nsamples);
	}
	//////////////////////////////////////////////////////////////////
	//
	//				AddLFO
	//
	//////////////////////////////////////////////////////////////////
	float AddLFO(float* pout, float *pin, Waveform *pwave, float phase, float *pincr, float *pamount, int nsamples)
	{
		--pin;
		--pout;
		--pincr;
		--pamount;
		do
		{
			*++pout = *++pin + pwave->GetSample(phase) * *++pamount;
			phase += *++pincr;
		}
		while (--nsamples);
		return phase;
	}
	//////////////////////////////////////////////////////////////////
	//
	//				AddLFO2
	//
	//////////////////////////////////////////////////////////////////
	float AddLFO2(float* pout1, float *pin1, float* pout2, float *pin2, Waveform *pwave, float phase, float *pincr, float *pamount, int nsamples)
	{
		float out;
		--pin1;
		--pin2;
		--pout1;
		--pout2;
		--pincr;
		--pamount;
		do
		{
			out = pwave->GetSample(phase) * *++pamount;
			phase += *++pincr;
			*++pout1 = *++pin1 + out;
			*++pout2 = *++pin2 + out;
		}
		while (--nsamples);
		return phase;
	}
	//////////////////////////////////////////////////////////////////
	//
	//				AddLFO3
	//
	//////////////////////////////////////////////////////////////////
	float AddLFO3(float* pout, float *pin, Waveform *pwave, Inertia *pinertia, float phase, float *pincr, float *pamount, int nsamples)
	{
		int amt;
		--pin;
		--pout;
		--pincr;
		--pamount;
		do
		{
			amt = pinertia->Clip(nsamples);
			nsamples -= amt;
			do
			{
				*++pout = *++pin + pwave->GetSample(phase) * *++pamount * pinertia->Next();
				phase += *++pincr;
			}
			while (--amt);
		}
		while (nsamples);
		return phase;
	}
	//////////////////////////////////////////////////////////////////
	//
	//				AddLFO4
	//
	//////////////////////////////////////////////////////////////////
	float AddLFO4(float* pout1, float *pin1, float* pout2, float *pin2, Waveform *pwave, Inertia *pinertia, float phase, float *pincr, float *pamount, int nsamples)
	{
		int amt;
		float out;
		--pin1;
		--pin2;
		--pout1;
		--pout2;
		--pincr;
		--pamount;
		do
		{
			amt = pinertia->Clip(nsamples);
			nsamples -= amt;
			do
			{
				out = pwave->GetSample(phase) * *++pamount * pinertia->Next();
				phase += *++pincr;
				*++pout1 = *++pin1 + out;
				*++pout2 = *++pin2 + out;
			}
			while (--amt);
		}
		while (nsamples);
		return phase;
	}
	//////////////////////////////////////////////////////////////////
	//
	//				Work
	//
	//////////////////////////////////////////////////////////////////
	inline void Work(float *pout, int numsamples)
	{
		//////////////////////////////////////////////////////////////
		//				Some needed vars
		//////////////////////////////////////////////////////////////
		int amt;
		int nsamples;
		int idx;
		float incr;
		float out;
		float tmp;
//								Filter *pflt;
		//////////////////////////////////////////////////////////////
		//				Setup
		//////////////////////////////////////////////////////////////
		float *posc1_fm = m_buffers->m_osc1_fm_out - 1;
		float *posc2_fm = m_buffers->m_osc2_fm_out - 1;
		float *posc1_pm_amount = m_globals->m_posc1_pm_amount_out - 1;
		float *posc2_pm_amount = m_globals->m_posc2_pm_amount_out - 1;
		float *posc_fm = m_globals->m_posc_fm_out - 1;
		float *posc_pm = m_globals->m_posc_pm_out - 1;
		float *posc_mix = m_globals->m_posc_mix_out - 1;
		float *plfo1_incr = m_globals->m_plfo1_incr_out - 1;
		float *plfo2_incr = m_globals->m_plfo2_incr_out - 1;
		float *pvcf_amount = m_globals->m_pvcf_amount_out - 1;
		float *pflt1_freq = m_globals->m_pflt1_freq_out - 1;
		float *pflt1_q = m_globals->m_pflt1_q_out - 1;
		float *pflt2_freq = m_globals->m_pflt2_freq_out - 1;
		float *pflt2_q = m_globals->m_pflt2_q_out - 1;
		float *pamp_level = m_globals->m_pamp_level_out - 1;
		float *pout1 = m_buffers->m_out1 - 1;
		float *pout2 = m_buffers->m_out2 - 1;
		float *ppmtable = GetPMTable();
		//////////////////////////////////////////////////////////////
		//				Generate MOD
		//////////////////////////////////////////////////////////////
		switch (m_globals->m_mod_dest)
		{
		case MOD_LFO2 :
			AddEnv(m_buffers->m_mod_out, plfo2_incr + 1, &m_mod, m_globals->m_pmod_amount_out, numsamples);
			plfo2_incr = m_buffers->m_mod_out - 1;
			Fill(m_buffers->m_osc1_fm_out, 0.0f, numsamples);
			Fill(m_buffers->m_osc2_fm_out, 0.0f, numsamples);
			break;
		case MOD_LFO1 :
			AddEnv(m_buffers->m_mod_out, plfo1_incr + 1, &m_mod, m_globals->m_pmod_amount_out, numsamples);
			plfo1_incr = m_buffers->m_mod_out - 1;
			Fill(m_buffers->m_osc1_fm_out, 0.0f, numsamples);
			Fill(m_buffers->m_osc2_fm_out, 0.0f, numsamples);
			break;
		case DST_FLT2_FREQ :
			AddEnv(m_buffers->m_mod_out, pflt2_freq + 1, &m_mod, m_globals->m_pmod_amount_out, numsamples);
			pflt2_freq = m_buffers->m_mod_out - 1;
			Fill(m_buffers->m_osc1_fm_out, 0.0f, numsamples);
			Fill(m_buffers->m_osc2_fm_out, 0.0f, numsamples);
			break;
		case DST_FLT1_FREQ :
			AddEnv(m_buffers->m_mod_out, pflt1_freq + 1, &m_mod, m_globals->m_pmod_amount_out, numsamples);
			pflt1_freq = m_buffers->m_mod_out - 1;
			Fill(m_buffers->m_osc1_fm_out, 0.0f, numsamples);
			Fill(m_buffers->m_osc2_fm_out, 0.0f, numsamples);
			break;
		case DST_PHASE :
			AddEnv2(m_buffers->m_osc1_phase_out, posc1_pm_amount + 1, m_buffers->m_osc2_phase_out, posc2_pm_amount + 1, &m_mod, m_globals->m_pmod_amount_out, numsamples);
			posc1_pm_amount = m_buffers->m_osc1_phase_out - 1;
			posc2_pm_amount = m_buffers->m_osc2_phase_out - 1;
			Fill(m_buffers->m_osc1_fm_out, 0.0f, numsamples);
			Fill(m_buffers->m_osc2_fm_out, 0.0f, numsamples);
			break;
		case DST_PM :
			AddEnv(m_buffers->m_mod_out, posc_pm + 1, &m_mod, m_globals->m_pmod_amount_out, numsamples);
			posc_pm = m_buffers->m_mod_out - 1;
			Fill(m_buffers->m_osc1_fm_out, 0.0f, numsamples);
			Fill(m_buffers->m_osc2_fm_out, 0.0f, numsamples);
			break;
		case DST_FM :
			AddEnv(m_buffers->m_mod_out, posc_fm + 1, &m_mod, m_globals->m_pmod_amount_out, numsamples);
			posc_fm = m_buffers->m_mod_out - 1;
			Fill(m_buffers->m_osc1_fm_out, 0.0f, numsamples);
			Fill(m_buffers->m_osc2_fm_out, 0.0f, numsamples);
			break;
		case DST_MIX :
			AddEnv(m_buffers->m_mod_out, posc_mix + 1, &m_mod, m_globals->m_pmod_amount_out, numsamples);
			posc_mix = m_buffers->m_mod_out - 1;
			Fill(m_buffers->m_osc1_fm_out, 0.0f, numsamples);
			Fill(m_buffers->m_osc2_fm_out, 0.0f, numsamples);
			break;
		case DST_OSC12 :
			FillEnv2(m_buffers->m_osc1_fm_out, m_buffers->m_osc2_fm_out, &m_mod, m_globals->m_pmod_amount_out, numsamples);
			break;
		case DST_OSC2 :
			FillEnv(m_buffers->m_osc2_fm_out, &m_mod, m_globals->m_pmod_amount_out, numsamples);
			Fill(m_buffers->m_osc1_fm_out, 0.0f, numsamples);
			break;
		case DST_OSC1 :
			FillEnv(m_buffers->m_osc1_fm_out, &m_mod, m_globals->m_pmod_amount_out, numsamples);
			Fill(m_buffers->m_osc2_fm_out, 0.0f, numsamples);
			break;
		case DST_OFF :
		default :
			Fill(m_buffers->m_osc1_fm_out, 0.0f, numsamples);
			Fill(m_buffers->m_osc2_fm_out, 0.0f, numsamples);
		}
		//////////////////////////////////////////////////////////////
		//				LFO 1
		//////////////////////////////////////////////////////////////
		switch (m_globals->m_lfo1_dest)
		{
		case DST_FLT2_FREQ :
			m_lfo1_phase = AddLFO(m_buffers->m_lfo1_out, pflt2_freq + 1, &m_globals->m_lfo1_wave, m_lfo1_phase, plfo1_incr + 1, m_globals->m_plfo1_amount_out, numsamples);
			pflt2_freq = m_buffers->m_lfo1_out - 1;
			break;
		case DST_FLT1_FREQ :
			m_lfo1_phase = AddLFO(m_buffers->m_lfo1_out, pflt1_freq + 1, &m_globals->m_lfo1_wave, m_lfo1_phase, plfo1_incr + 1, m_globals->m_plfo1_amount_out, numsamples);
			pflt1_freq = m_buffers->m_lfo1_out - 1;
			break;
		case DST_PHASE :
			m_lfo1_phase = AddLFO2(m_buffers->m_osc1_phase_out, posc1_pm_amount + 1, m_buffers->m_osc2_phase_out, posc2_pm_amount + 1, &m_globals->m_lfo1_wave, m_lfo1_phase, plfo1_incr + 1, m_globals->m_plfo1_amount_out, numsamples);
			posc1_pm_amount = m_buffers->m_osc1_phase_out - 1;
			posc2_pm_amount = m_buffers->m_osc2_phase_out - 1;
			break;
		case DST_PM :
			m_lfo1_phase = AddLFO(m_buffers->m_lfo1_out, posc_pm + 1, &m_globals->m_lfo1_wave, m_lfo1_phase, plfo1_incr + 1, m_globals->m_plfo1_amount_out, numsamples);
			posc_pm = m_buffers->m_lfo1_out - 1;
			break;
		case DST_FM :
			m_lfo1_phase = AddLFO(m_buffers->m_lfo1_out, posc_fm + 1, &m_globals->m_lfo1_wave, m_lfo1_phase, plfo1_incr + 1, m_globals->m_plfo1_amount_out, numsamples);
			posc_fm = m_buffers->m_lfo1_out - 1;
			break;
		case DST_MIX :
			m_lfo1_phase = AddLFO(m_buffers->m_lfo1_out, posc_mix + 1, &m_globals->m_lfo1_wave, m_lfo1_phase, plfo1_incr + 1, m_globals->m_plfo1_amount_out, numsamples);
			posc_mix = m_buffers->m_lfo1_out - 1;
			break;
		case DST_OSC12 :
			m_lfo1_phase = AddLFO2(m_buffers->m_osc1_fm_out, m_buffers->m_osc1_fm_out, m_buffers->m_osc2_fm_out, m_buffers->m_osc2_fm_out, &m_globals->m_lfo1_wave, m_lfo1_phase, plfo1_incr + 1, m_globals->m_plfo1_amount_out, numsamples);
			break;
		case DST_OSC2 :
			m_lfo1_phase = AddLFO(m_buffers->m_osc2_fm_out, m_buffers->m_osc2_fm_out, &m_globals->m_lfo1_wave, m_lfo1_phase, plfo1_incr + 1, m_globals->m_plfo1_amount_out, numsamples);
			break;
		case DST_OSC1 :
			m_lfo1_phase = AddLFO(m_buffers->m_osc1_fm_out, m_buffers->m_osc1_fm_out, &m_globals->m_lfo1_wave, m_lfo1_phase, plfo1_incr + 1, m_globals->m_plfo1_amount_out, numsamples);
			break;
		case DST_OFF :
		default: ;
		}
		//////////////////////////////////////////////////////////////
		//				LFO 2
		//////////////////////////////////////////////////////////////
		switch (m_globals->m_lfo2_dest)
		{
		case LFO2_AMP :
			m_lfo2_phase = AddLFO3(m_buffers->m_lfo2_out, pamp_level + 1, &m_globals->m_lfo2_wave, &m_lfo2_delay, m_lfo2_phase, plfo2_incr + 1, m_globals->m_plfo2_amount_out, numsamples);
			pamp_level = m_buffers->m_lfo2_out - 1;
			break;
		case DST_FLT2_FREQ :
			m_lfo2_phase = AddLFO3(m_buffers->m_lfo2_out, pflt2_freq + 1, &m_globals->m_lfo2_wave, &m_lfo2_delay, m_lfo2_phase, plfo2_incr + 1, m_globals->m_plfo2_amount_out, numsamples);
			pflt2_freq = m_buffers->m_lfo2_out - 1;
			break;
		case DST_FLT1_FREQ :
			m_lfo2_phase = AddLFO3(m_buffers->m_lfo2_out, pflt1_freq + 1, &m_globals->m_lfo2_wave, &m_lfo2_delay, m_lfo2_phase, plfo2_incr + 1, m_globals->m_plfo2_amount_out, numsamples);
			pflt1_freq = m_buffers->m_lfo2_out - 1;
			break;
		case DST_PHASE :
			m_lfo2_phase = AddLFO4(m_buffers->m_osc1_phase_out, posc1_pm_amount + 1, m_buffers->m_osc2_phase_out, posc2_pm_amount + 1, &m_globals->m_lfo2_wave, &m_lfo2_delay, m_lfo2_phase, plfo2_incr + 1, m_globals->m_plfo2_amount_out, numsamples);
			posc1_pm_amount = m_buffers->m_osc1_phase_out - 1;
			posc2_pm_amount = m_buffers->m_osc2_phase_out - 1;
			break;
		case DST_PM :
			m_lfo2_phase = AddLFO3(m_buffers->m_lfo2_out, posc_pm + 1, &m_globals->m_lfo2_wave, &m_lfo2_delay, m_lfo2_phase, plfo2_incr + 1, m_globals->m_plfo2_amount_out, numsamples);
			posc_pm = m_buffers->m_lfo2_out - 1;
			break;
		case DST_FM :
			m_lfo2_phase = AddLFO3(m_buffers->m_lfo2_out, posc_fm + 1, &m_globals->m_lfo2_wave, &m_lfo2_delay, m_lfo2_phase, plfo2_incr + 1, m_globals->m_plfo2_amount_out, numsamples);
			posc_fm = m_buffers->m_lfo1_out - 1;
			break;
		case DST_MIX :
			m_lfo2_phase = AddLFO3(m_buffers->m_lfo2_out, posc_mix + 1, &m_globals->m_lfo2_wave, &m_lfo2_delay, m_lfo2_phase, plfo2_incr + 1, m_globals->m_plfo2_amount_out, numsamples);
			posc_mix = m_buffers->m_lfo1_out - 1;
			break;
		case DST_OSC12 :
			m_lfo2_phase = AddLFO4(m_buffers->m_osc1_fm_out, m_buffers->m_osc1_fm_out, m_buffers->m_osc2_fm_out, m_buffers->m_osc2_fm_out, &m_globals->m_lfo2_wave, &m_lfo2_delay, m_lfo2_phase, plfo2_incr + 1, m_globals->m_plfo2_amount_out, numsamples);
			break;
		case DST_OSC2 :
			m_lfo2_phase = AddLFO3(m_buffers->m_osc2_fm_out, m_buffers->m_osc2_fm_out, &m_globals->m_lfo2_wave, &m_lfo2_delay, m_lfo2_phase, plfo2_incr + 1, m_globals->m_plfo2_amount_out, numsamples);
			break;
		case DST_OSC1 :
			m_lfo2_phase = AddLFO3(m_buffers->m_osc1_fm_out, m_buffers->m_osc1_fm_out, &m_globals->m_lfo2_wave, &m_lfo2_delay, m_lfo2_phase, plfo2_incr + 1, m_globals->m_plfo2_amount_out, numsamples);
			break;
		case DST_OFF :
		default: ;
		}
		//////////////////////////////////////////////////////////////
		//				Noise
		//////////////////////////////////////////////////////////////
		if ((m_globals->m_noise_level.GetTarget() == 0.0f && m_globals->m_noise_level.IsIdle()) || m_noise_decay.IsIdle())
		{
			Fill(m_buffers->m_out2, 0.0f, numsamples);
		}
		else
		{
			float *pnoisecolor = m_globals->m_pnoise_color_out - 1;
			float *pnoiselevel = m_globals->m_pnoise_level_out - 1;
			pout2 = m_buffers->m_out2 - 1;
			nsamples = numsamples;
			do
			{
				amt = m_noise_decay.Clip(nsamples);
				nsamples -= amt;
				if ( m_noise_decay.IsIdle())
				{
					Fill(m_buffers->m_out2,0.0f, numsamples);
				}
				else do
				{
					tmp = rnd_signal() * *++pnoiselevel * m_noise_decay.Next();
					b0 = 0.99765f * b0 + tmp * 0.0990460f;
					b1 = 0.96300f * b1 + tmp * 0.2965164f;
					b2 = 0.57000f * b2 + tmp * 1.0526913f;
					out = b0 + b1 + b2 + tmp * 0.1848f;
					*++pout2 = out + (tmp - out) * *++pnoisecolor;
				}
				while (--amt);
			}
			while (nsamples);

		}

		//////////////////////////////////////////////////////////////
		//				OSC 2
		//////////////////////////////////////////////////////////////
		pout2 = m_buffers->m_out2 - 1;
		nsamples = numsamples;
		idx = (m_globals->m_osc2_wave.Get()->index == WF_BLANK ? -1 : m_globals->m_osc2_pm_type);
		switch (idx)
		{
		case PMODE_MULT :
			do
			{
				amt = m_osc2_incr.Clip(nsamples);
				nsamples -= amt;
				do
				{
					incr = m_osc2_incr.Next() * FM(*++posc2_fm);
					*++pout2 += m_globals->m_osc2_wave.GetSample(m_osc2_phase, incr)
							* (m_globals->m_osc2_wave.GetSample(m_osc2_phase + *++posc2_pm_amount, incr));
					m_osc2_phase += incr;
				}
				while (--amt);
			}
			while (nsamples);
			break;
		case PMODE_ADD :
			do
			{
				amt = m_osc2_incr.Clip(nsamples);
				nsamples -= amt;
				do
				{
					incr = m_osc2_incr.Next() * FM(*++posc2_fm);
					*++pout2 += m_globals->m_osc2_wave.GetSample(m_osc2_phase, incr)
							+ (m_globals->m_osc2_wave.GetSample(m_osc2_phase + *++posc2_pm_amount, incr));
					m_osc2_phase += incr;
				}
				while (--amt);
			}
			while (nsamples);
			break;
		case PMODE_SUB :
			do
			{
				amt = m_osc2_incr.Clip(nsamples);
				nsamples -= amt;
				do
				{
					incr = m_osc2_incr.Next() * FM(*++posc2_fm);
					*++pout2 += m_globals->m_osc2_wave.GetSample(m_osc2_phase, incr)
							- (m_globals->m_osc2_wave.GetSample(m_osc2_phase + *++posc2_pm_amount, incr));
					m_osc2_phase += incr;
				}
				while (--amt);
			}
			while (nsamples);
			break;
		case PMODE_OFF :
			do
			{
				amt = m_osc2_incr.Clip(nsamples);
				nsamples -= amt;
				do
				{
					incr = m_osc2_incr.Next() * FM(*++posc2_fm);
					*++pout2 += m_globals->m_osc2_wave.GetSample(m_osc2_phase, incr);
					m_osc2_phase += incr;
				}
				while (--amt);
			}
			while (nsamples);
			break;
		default: ;
		}
		idx = m_globals->m_osc2_vowelnum;
		if (idx > -1)
		{
			pout2 = m_buffers->m_out2 - 1;
			nsamples = numsamples;
			do
			{
				++pout2;
				*pout2 = m_osc2_formant.Next(*pout2, idx);
			}
			while (--nsamples);
		}
		//////////////////////////////////////////////////////////////
		//				OSC 1
		//////////////////////////////////////////////////////////////
		pout2 = m_buffers->m_out2 - 1;
		nsamples = numsamples;
		switch (m_globals->m_osc1_pm_type)
		{
		case PMODE_MULT :
			do
			{
				amt = m_osc1_incr.Clip(nsamples);
				nsamples -= amt;
				do
				{
					incr = m_osc1_incr.Next() * FM(*++posc1_fm + *++pout2 * *++posc_fm);
					idx = rint<int>(incr * incr2freq) & 0xffff;
					tmp = m_osc1_phase + *pout2 * *++posc_pm * ppmtable[idx];
					*++pout1 = m_globals->m_osc1_wave.GetSample(tmp, idx)
							* (m_globals->m_osc1_wave.GetSample(tmp + *++posc1_pm_amount, idx));
					m_osc1_phase += incr;
				}
				while (--amt);
			}
			while (nsamples);
			break;
		case PMODE_ADD :
			do
			{
				amt = m_osc1_incr.Clip(nsamples);
				nsamples -= amt;
				do
				{
					incr = m_osc1_incr.Next() * FM(*++posc1_fm + *++pout2 * *++posc_fm);
					idx = rint<int>(incr * incr2freq) & 0xffff;
					tmp = m_osc1_phase + *pout2 * *++posc_pm * ppmtable[idx];
					*++pout1 = m_globals->m_osc1_wave.GetSample(tmp, idx)
							+ (m_globals->m_osc1_wave.GetSample(tmp + *++posc1_pm_amount, idx));
					m_osc1_phase += incr;
				}
				while (--amt);
			}
			while (nsamples);
			break;
		case PMODE_SUB :
			do
			{
				amt = m_osc1_incr.Clip(nsamples);
				nsamples -= amt;
				do
				{
					incr = m_osc1_incr.Next() * FM(*++posc1_fm + *++pout2 * *++posc_fm);
					idx = rint<int>(incr * incr2freq) & 0xffff;
					tmp = m_osc1_phase + *pout2 * *++posc_pm * ppmtable[idx];
					*++pout1 = m_globals->m_osc1_wave.GetSample(tmp, idx)
							- (m_globals->m_osc1_wave.GetSample(tmp + *++posc1_pm_amount, idx));
					m_osc1_phase += incr;
				}
				while (--amt);
			}
			while (nsamples);
			break;
		case PMODE_OFF :
		default:
			do
			{
				amt = m_osc1_incr.Clip(nsamples);
				nsamples -= amt;
				do
				{
					incr = m_osc1_incr.Next() * FM(*++posc1_fm + *++pout2 * *++posc_fm);
					idx = rint<int>(incr * incr2freq) & 0xffff;
					tmp = m_osc1_phase + *pout2 * *++posc_pm * ppmtable[idx];
					*++pout1 = m_globals->m_osc1_wave.GetSample(tmp, idx);
					m_osc1_phase += incr;
				}
				while (--amt);
			}
			while (nsamples);
		}
		idx = m_globals->m_osc1_vowelnum;
		if (idx > -1)
		{
			pout1 = m_buffers->m_out1 - 1;
			nsamples = numsamples;
			do
			{
				++pout1;
				*pout1 = m_osc1_formant.Next(*pout1, idx);
			}
			while (--nsamples);
		}
		//////////////////////////////////////////////////////////////
		//				Mix
		//////////////////////////////////////////////////////////////
		pout1 = m_buffers->m_out1 - 1;
		pout2 = m_buffers->m_out2 - 1;
		nsamples = numsamples;
		if (m_globals->m_osc_ring_mod)
		{
			do
			{
				++pout1;
				*pout1 = *pout1 * *++pout2;
			}
			while (--nsamples);
		}
		else
		{
			do
			{
				out = *++pout1;
				*pout1 = out + (*++pout2 - out) * *++posc_mix;
			}
			while (--nsamples);
		}
		//////////////////////////////////////////////////////////////
		//				Filter Animation
		//////////////////////////////////////////////////////////////
		pout2 = m_buffers->m_out2 - 1;
		nsamples = numsamples;
		do
		{
			amt = m_vcf.Clip(nsamples);
			nsamples -= amt;
			do
			{
				*++pout2 = m_vcf.Next() * *++pvcf_amount;
			}
			while (--amt);
		}
		while (nsamples);
		if (m_globals->m_flt1_kbd_track != 0.0f)
		{
			tmp = m_osc1_incr.GetValue() * incr2freq * m_globals->m_flt1_kbd_track / m_globals->m_samplingrate;
			pout2 = m_buffers->m_out2 - 1;
			nsamples = numsamples;
			do
			{
				*++pout2 += tmp;
			}
			while (--nsamples);
		}
		//////////////////////////////////////////////////////////////
		//				Filter 1
		//////////////////////////////////////////////////////////////
		pout1 = m_buffers->m_out1 - 1;
		pout2 = m_buffers->m_out2 - 1;
		nsamples = numsamples;
		switch (m_globals->m_flt1_type)
		{
		case 0 :
			do
			{
				m_flt1.InitSimple(*++pflt1_freq + *++pout2, *++pflt1_q);
				++pout1;
				*pout1 = m_flt1.SimpleLP12(*pout1);
			}
			while (--nsamples);
			break;
		case 1 :
			do
			{
				m_flt1.InitSimple(*++pflt1_freq + *++pout2, *++pflt1_q);
				++pout1;
				*pout1 = m_flt1.SimpleLP24(*pout1);
			}
			while (--nsamples);
			break;
		case 2 :
			do
			{
				m_flt1.InitSimple(*++pflt1_freq + *++pout2, *++pflt1_q);
				++pout1;
				*pout1 = m_flt1.SimpleLP36(*pout1);
			}
			while (--nsamples);
			break;
		case 3 :
			do
			{
				m_flt1.InitSimple(*++pflt1_freq + *++pout2, *++pflt1_q);
				++pout1;
				*pout1 = m_flt1.SimpleHP12(*pout1);
			}
			while (--nsamples);
			break;
		case 4 :
			do
			{
				m_flt1.InitSimple(*++pflt1_freq + *++pout2, *++pflt1_q);
				++pout1;
				*pout1 = m_flt1.SimpleHP24(*pout1);
			}
			while (--nsamples);
			break;
		case 5 :
			do
			{
				m_flt1.InitMoog(*++pflt1_freq + *++pout2, *++pflt1_q);
				++pout1;
				*pout1 = m_flt1.MoogLP24(*pout1);
			}
			while (--nsamples);
			break;
		case 6 :
			do
			{
				m_flt1.InitMoog(*++pflt1_freq + *++pout2, *++pflt1_q);
				++pout1;
				*pout1 = m_flt1.MoogHP24(*pout1);
			}
			while (--nsamples);
			break;
		case 7 :
			do
			{
				m_flt1.InitMoog(*++pflt1_freq + *++pout2, *++pflt1_q);
				++pout1;
				*pout1 = m_flt1.MoogBP24(*pout1);
			}
			while (--nsamples);
			break;
		default: ;
		}
		//////////////////////////////////////////////////////////////
		//				Filter 2
		//////////////////////////////////////////////////////////////
		pout1 = m_buffers->m_out1 - 1;
		pout2 = m_buffers->m_out2 - 1;
		nsamples = numsamples;
		switch (m_globals->m_flt2_mode)
		{
		case FLT2_ENABLED :
			do
			{
				m_flt2.InitSimple(*++pflt2_freq, *++pflt2_q);
				++pout1;
				*pout1 = m_flt2.SimpleLP12(*pout1);
			}
			while (--nsamples);
			break;
		case FLT2_LINKED :
			do
			{
				m_flt2.InitSimple(*++pflt2_freq + *++pout2, *++pflt2_q);
				++pout1;
				*pout1 = m_flt2.SimpleLP12(*pout1);
			}
			while (--nsamples);
			break;
		default: ;
		}
		//////////////////////////////////////////////////////////////
		//				Out
		//////////////////////////////////////////////////////////////
		--pout;
		pout1 = m_buffers->m_out1 - 1;
		do
		{
			amt = m_vca.Clip(numsamples);
			numsamples -= amt;
			do
			{
				*++pout += *++pout1 * m_velocity * m_vca.Next() * *++pamp_level;
			}
			while (--amt);
		}
		while (numsamples);
		//
		//				Mask off shit
		//
		m_osc1_phase = fand(m_osc1_phase, WAVEMASK);
		m_osc2_phase = fand(m_osc2_phase, WAVEMASK);
		m_lfo1_phase = fand(m_lfo1_phase, WAVEMASK);
		m_lfo2_phase = fand(m_lfo2_phase, WAVEMASK);
	} 
};
}