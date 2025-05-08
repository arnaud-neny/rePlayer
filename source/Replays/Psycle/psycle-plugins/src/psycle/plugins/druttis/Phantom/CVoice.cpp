//============================================================================
//
//				CVoice.cpp
//				----------
//				druttis@darkface.pp.se
//
//============================================================================
#include "CVoice.h"

namespace psycle::plugins::druttis::phantom {

//============================================================================
//				Constructor
//============================================================================
CVoice::CVoice()
{
}
//============================================================================
//				Destructor
//============================================================================
CVoice::~CVoice()
{
}
//============================================================================
//				Stop
//============================================================================
void CVoice::Stop()
{
	vca.Kill();
	vcf.Kill();
}
//============================================================================
//				NoteOff
//============================================================================
void CVoice::NoteOff()
{
	vca.Stop();
	vcf.Stop();
}
//============================================================================
//				NoteOn
//============================================================================
void CVoice::NoteOn(int note, int volume)
{
	//
	velocity = (float) volume / 255.0f;
	//
	float nnote;
	for (int i = 0; i < 6; i++) {
		nnote = (float) note + globals->osc_semi[i] + globals->osc_fine[i];
		osc_phase[i] = globals->osc_phase[i];
		osc_increment[i] = CDsp::GetIncrement(nnote, WAVESIZE, globals->samplingrate);
	}
	//
	vca.SetAttack(globals->vca_attack);
	vca.SetDecay(globals->vca_decay);
	vca.SetSustain(globals->vca_sustain);
	vca.SetRelease(globals->vca_release);
	vca.Begin();
	//
	vcf.SetAttack(globals->vcf_attack);
	vcf.SetDecay(globals->vcf_decay);
	vcf.SetSustain(globals->vcf_sustain);
	vcf.SetRelease(globals->vcf_release);
	vcf.Begin();
	//
	for (int i = 0; i < 6; i++)
	{
		memory[i][0] = memory[i][1] = memory[i][2] = memory[i][3] = memory[i][4] = memory[i][5] = memory[i][6] = memory[i][7] = memory[i][8] = memory[i][9] = 0.0;
	}
	//
	filter_phase = 0.0f;
	filter.buf0 = filter.buf1 = 0.0f;
}
}