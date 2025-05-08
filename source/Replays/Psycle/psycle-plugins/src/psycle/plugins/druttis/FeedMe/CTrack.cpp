//============================================================================
//
//				CTrack.cpp
//				----------
//				druttis@darkface.pp.se
//
//============================================================================
#include "CTrack.h"
#include <memory>
#include <cstring>
namespace psycle::plugins::druttis::feedme {
//============================================================================
//				Static variables
//============================================================================
float CTrack::wavetable[NUMWAVEFORMS][WAVESIZE];
float CTrack::overtonemults[MAX_OVERTONES];
//============================================================================
//				Constructor
//============================================================================
CTrack::CTrack()
:vibrato_pos(0.0f)
,vibrato_dtime(0.0f)
,osc1_speed(-1)
,osc2_speed(-1)
{
}

//============================================================================
//				Destructor
//============================================================================
CTrack::~CTrack()
{
}
//============================================================================
//				Init				(Initialize static data)
//============================================================================
void CTrack::Init()
{
	for (int i = 0; i < WAVESIZE; i++) {
		float t = (float) i / (float) WAVESIZE;
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
		//				Exp
		wavetable[5][i] = (float) (t * t - 0.5f) * 2.0f;
	}
	//
	//Create overtone multipliers (value to normalize amplitude of the output)
	overtonemults[0] = 1.0f / 1.0f;
	overtonemults[1] = 1.0f / 2.5f;
	overtonemults[2] = 1.0f / 4.0f;
	overtonemults[3] = 1.0f / 5.5f;
	overtonemults[4] = 1.0f / 2.5f;
	overtonemults[5] = 1.0f / 4.0f;
}
//============================================================================
//				Destroy
//============================================================================
void CTrack::Destroy()
{
}
//============================================================================
//				Stop
//============================================================================
void CTrack::Stop()
{
	vca_env.Kill();
	vcf_env.Kill();
	vibrato_dtime = 0.0f;
	osc1_speed = -1;
	osc2_speed = -1;
}
//============================================================================
//				NoteOff
//============================================================================
void CTrack::NoteOff()
{
	vca_env.Stop();
	vcf_env.Stop();
}
//============================================================================
//				NoteOn
//============================================================================
void CTrack::NoteOn(int note, int volume, bool slide)
{
	SetFreq(note);

	//	Osc 1
	if (!slide || osc1_speed == -1)
		osc1_speed = osc1_target_speed;
	memset(osc1_out, 0, sizeof(osc1_out));
	osc1_pos = 0.0f;
	//	Osc 2
	if (!slide || osc2_speed == -1)
		osc2_speed = osc2_target_speed;
	memset(osc2_out, 0, sizeof(osc2_out));
	osc2_pos = 0.0f;

	//	Reset ticks
	ticks_remaining = 0;
	if ((globals->sync_mode & 1) != 0) {
		//	Reset vibrato
		vibrato_pos = 0.0f;
		vibrato_dtime = 0.0f;
	}

	//	Setup & start VCF
	vcf_env.SetAttack(globals->vcf_attack);
	vcf_env.SetDecay(globals->vcf_decay);
	vcf_env.SetSustain(globals->vcf_sustain);
	vcf_env.SetRelease(globals->vcf_release);
	vcf_data1.buf0 = vcf_data1.buf1 = 0.0f;
	vcf_data2.buf0 = vcf_data2.buf1 = 0.0f;
	if ((globals->sync_mode & 2) != 0) {
		vcf_env.Begin();
	}

	//	Compute velocity & amplitude
	float velocity = (float) volume / 255.0f;

	amplitude = velocity * 16384.0f * globals->amplitude;

	//	Setup & start VCA
	vca_env.SetAttack(globals->attack);
	vca_env.SetRelease(globals->release);
	vca_env.Begin();
}
}