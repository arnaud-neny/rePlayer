//============================================================================
//
//				CVoice.cpp
//				----------
//				druttis@darkface.pp.se
//
//============================================================================
#include "CVoice.h"
#include <psycle/helpers/math.hpp>
#include <psycle/plugin_interface.hpp>
namespace psycle::plugins::druttis::plucked_string {
float const LOWEST_FREQ = 20.0f;

//============================================================================
//				Init
//============================================================================
void CVoice::Init()
{
	currentFreq = -1.0f;
	lastFreq = LOWEST_FREQ;
	loopGain = 0.999f;

	long length = (long) (globals->srate / LOWEST_FREQ) + 1;
	lastLength = (float) length;
	delayLine.Init(length);
	combDelay.Init(length);
	pluckAmp = 0.3f;
	globals->pluckPos = 0.25f;
	combDelay.SetDelay(globals->pluckPos * lastLength + 0.5f);

	loopFilt.Init();
	loopFilt.SetRawCoeffs(-1.33373f,0.446191f,-1.03f,0.2154f);
	loopFilt.SetGain(0.604595f);

	vca.SetAttack(0.0f);
	vca.SetDecay(0.0f);
	vca.SetSustain(1.0f);
	vca.SetRelease(256.0f*(globals->srate/44100.f));

	globals->vib_speed = 192.0f / globals->srate;
	globals->vib_amount = 0.05f;
	globals->vib_delay = 256.0f;

}
//============================================================================
//				UpdateSampleRate
//============================================================================
void CVoice::UpdateSampleRate()
{
	long length = (long) (globals->srate / 20.0f + 1.0f);
	delayLine.Init(length);
	SetFreq(lastFreq);
	combDelay.Init(length);
	Pluck(pluckAmp);
	vca.SetRelease(256.0f*(globals->srate/44100.f));
}
//============================================================================
//				Clear
//============================================================================
void CVoice::Clear()
{
	delayLine.Clear();
	combDelay.Clear();
	loopFilt.Clear();
}
//============================================================================
//				SetFreq
//============================================================================
void CVoice::SetFreq(float frequency)
{
	float loopFilterDelay = 0.5f;
	lastFreq = frequency;
	lastLength = (globals->srate / lastFreq);
	delayLine.SetDelay(lastLength - loopFilterDelay);
	loopGain = globals->damping + (frequency * 0.00005f);
	if (loopGain > 1.0f)
		loopGain = 1.0f;
}
//============================================================================
//				Pluck
//============================================================================
void CVoice::Pluck(float amplitude)
{
	combDelay.SetDelay(globals->pluckPos * lastLength + 0.5f);
	pluckAmp = amplitude;
	plucker = amplitude;
	combDelay.Clear();
}
//============================================================================
//				Stop
//============================================================================
void CVoice::Stop()
{
	vca.Kill();
	currentFreq = -1.0f;
}
//============================================================================
//				NoteOff
//============================================================================
void CVoice::NoteOff()
{
	loopGain *= 0.995f;
	vca.Stop();
}
//============================================================================
//				NoteOn
//============================================================================
void CVoice::NoteOn(int note, int volume, int cmd, int val)
{
	//
	//
	if (cmd == 0x0e) {
		noteDelay = val;
	} else {
		noteDelay = 0;
	}
	//
	//
	float sn = globals->schoolness * (float) sin((float) note * PI2 / 9.12345f);
	SetFreq(CDsp::GetFreq((float) note + sn));
	if ((cmd != 0x0d) || (currentFreq == -1)) {
		Clear();
		Pluck((float) volume / 255.0f);
		slideSpeed = 0.0f;
		currentFreq = lastFreq;
	} else {
		if (val == 0)
			val = 16;
		slideSpeed = (lastFreq - currentFreq) / (float) (val * 2);
	}
	lastCurrentFreq = currentFreq;
	//
	//
	vib_phase = 0.0f;
	vib_dtime = 0.0f;
	vca.Begin();
}


}