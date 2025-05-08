//////////////////////////////////////////////////////////////////////
//
//				Voice.cpp
//
//				druttis@darkface.pp.se
//
//////////////////////////////////////////////////////////////////////
#include "Voice.h"
#include <psycle/plugin_interface.hpp>
namespace psycle::plugins::druttis::sublime {
//////////////////////////////////////////////////////////////////////
//
//				Voice constructor
//
//////////////////////////////////////////////////////////////////////
Voice::Voice()
{
}
//////////////////////////////////////////////////////////////////////
//
//				Voice destructor
//
//////////////////////////////////////////////////////////////////////
Voice::~Voice()
{
}
//////////////////////////////////////////////////////////////////////
//
//				Brutally stops the voice
//				Must be called before NoteOn if new note!
//
//////////////////////////////////////////////////////////////////////
void Voice::Reset()
{
	m_osc1_phase = (m_globals->m_osc1_phase + m_globals->m_phase_rand * (rnd_signal() + 1.0f) * 0.5f);
	m_osc1_incr.Reset();
	m_osc2_phase = (m_globals->m_osc2_phase + m_globals->m_phase_rand * (rnd_signal() + 1.0f) * 0.5f);
	m_osc2_incr.Reset();
	m_noise_decay.SetTarget(1.0f);
	m_noise_decay.Reset();
	m_mod.Reset();
	m_lfo1_phase = 0.0f;
	m_lfo2_delay.SetTarget(0.0f);
	m_lfo2_delay.Reset();
	m_lfo2_phase = 0.0f;
	m_vcf.Reset();
	m_vca.Reset();
	b0 = 0.0f;
	b1 = 0.0f;
	b2 = 0.0f;
	m_flt1.Reset();
	m_flt2.Reset();
	m_osc1_formant.Reset();
	m_osc2_formant.Reset();
}
//////////////////////////////////////////////////////////////////////
//
//				Stops the note
//
//////////////////////////////////////////////////////////////////////
void Voice::Stop(int samples)
{
//				m_mod.SetRelease(samples);
//				m_vcf.SetRelease(samples);
	m_vca.SetRelease(samples);
	NoteOff();
}
//////////////////////////////////////////////////////////////////////
//
//				Sets note off
//
//////////////////////////////////////////////////////////////////////
void Voice::NoteOff()
{
//				m_osc1_incr.Stop();
//				m_osc2_incr.Stop();
//				m_noise_decay.Stop();
	m_mod.Stop();
//				m_lfo2_delay.Stop();
	m_vcf.Stop();
	m_vca.Stop();
}
//////////////////////////////////////////////////////////////////////
//
//				Starts a note
//
//////////////////////////////////////////////////////////////////////
bool Voice::NoteOn(Voice *old, int note, int cmd, int val)
{
	// Only note off?
	if (note >= psycle::plugin_interface::NOTE_NOTEOFF)
	{
		if (note==psycle::plugin_interface::NOTE_NOTEOFF)
		{
			if (old)
			{
				old->NoteOff();
			}
		}
		return false;
	}

	Voice *voice = this;
	int inertia = m_globals->m_glide;
	
	m_velocity = 1.0f;

	switch (cmd)
	{
	case 0x0c :
		m_velocity = (float) val * 0.00390625f;
		break;
	case 0x0d :
		inertia = val;
		break;
	}

	if (old)
	{
		if (inertia)
		{
			voice = old;
			voice->m_velocity = m_velocity;
		}
		else
		{
			old->NoteOff();
			voice->m_osc1_incr.SetTarget(old->m_osc1_incr.GetValue());
			voice->m_osc2_incr.SetTarget(old->m_osc2_incr.GetValue());
//												voice->m_noise_decay.SetTarget(old->m_noise_decay.GetValue());
			voice->Reset();
		}
	}
	else
	{
		voice->Reset();
	}
	inertia *= m_globals->m_ticklength;
	inertia >>= 2;
	int sr = m_globals->m_samplingrate;
	//				OSC 1
	if (m_globals->m_osc1_kbd_track)
	{
		voice->m_osc1_incr.SetLength(inertia);
		voice->m_osc1_incr.SetTarget(note2incr(WAVESIZE, (float) note + m_globals->m_osc1_tune, sr));
	}
	else
	{
		voice->m_osc2_incr.SetLength(0);
		voice->m_osc1_incr.SetTarget(note2incr(WAVESIZE, m_globals->m_osc1_tune, sr));
	}
	//				OSC 2
	if (m_globals->m_osc2_kbd_track)
	{
		voice->m_osc2_incr.SetLength(inertia);
		voice->m_osc2_incr.SetTarget(note2incr(WAVESIZE, (float) note + m_globals->m_osc2_tune, sr));
	}
	else
	{
		voice->m_osc2_incr.SetLength(0);
		voice->m_osc2_incr.SetTarget(note2incr(WAVESIZE, m_globals->m_osc2_tune, sr));
	}
	if (voice == this)
	{
		//				NOISE
		voice->m_noise_decay.SetLength(millis2samples(m_globals->m_noise_decay, sr));
		voice->m_noise_decay.SetTarget(0.0f);
		//				MOD
		voice->m_mod.SetDelay(millis2samples(m_globals->m_mod_delay, sr));
		voice->m_mod.SetAttack(millis2samples(m_globals->m_mod_attack, sr));
		voice->m_mod.SetDecay(millis2samples(m_globals->m_mod_decay, sr));
		voice->m_mod.SetSustain(m_globals->m_mod_sustain);
		voice->m_mod.SetLength(millis2samples(m_globals->m_mod_length, sr));
		voice->m_mod.SetRelease(millis2samples(m_globals->m_mod_release, sr));
		voice->m_mod.Start();
		//				LFO 2
		voice->m_lfo2_delay.SetLength(millis2samples(m_globals->m_lfo2_delay, sr));
		voice->m_lfo2_delay.SetTarget(1.0f);

		//				VCF
		voice->m_vcf.SetDelay(millis2samples(m_globals->m_vcf_delay, sr));
		voice->m_vcf.SetAttack(millis2samples(m_globals->m_vcf_attack, sr));
		voice->m_vcf.SetDecay(millis2samples(m_globals->m_vcf_decay, sr));
		voice->m_vcf.SetSustain(m_globals->m_vcf_sustain);
		voice->m_vcf.SetLength(millis2samples(m_globals->m_vcf_length, sr));
		voice->m_vcf.SetRelease(millis2samples(m_globals->m_vcf_release, sr));
		voice->m_vcf.Start();
		//				VCA
		voice->m_vca.SetDelay(0);
		voice->m_vca.SetAttack(millis2samples(m_globals->m_vca_attack, sr));
		voice->m_vca.SetDecay(millis2samples(m_globals->m_vca_decay, sr));
		voice->m_vca.SetSustain(m_globals->m_vca_sustain);
		voice->m_vca.SetLength(millis2samples(m_globals->m_vca_length, sr));
		voice->m_vca.SetRelease(millis2samples(m_globals->m_vca_release, sr));
		voice->m_vca.Start();
	}
//				m_osc1_formant.Reset();
//				m_osc2_formant.Reset();
	//				Return
	return voice == this;
}
}