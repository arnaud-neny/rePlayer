#pragma once
#include <math.h>
#include "oscillator.hpp"
namespace psycle::plugins::jm_drums {
#define ST_NONOTE		0
#define ST_NOTESUSTAIN 3
#define ST_NOTERELEASE 5

typedef struct {
	float StartSpeed;				// Speed at which the Oscillator will run.
	float IncSpeed; 				// Speed at which the Oscillator speed is increased.
	int DecMode;					// Function that will be applied to the speed increase.
	float DecLength;				// Number of samples that DecFunction is applied.
	
	float SinVol;
	int AttackPos;
	int DecayPos;
	float AttackInc;
	float DecayDec;
	float SustainDec;
	int SLength;					// Total number of samples to play.

	int ThumpLength;				// Number of samples to play the thump.
	float ThumpVol;
	float ThumpDec;
	int ThumpFreq;

	float OutVol;					// Out Volume
	int compatibleMode;
	int Mix;
	int sinmix;
	int samplerate;

}DrumPars;


class Drum
{
public:
	Drum();
	virtual ~Drum();

	void NoteOn(int note,DrumPars* pars);
	void NoteOff();
	inline float GetSample();

	float OutVol;				// Master volume. Values between 0 and 1!
	int Chan;
	bool Started;				// Used only to avoid New notes to start when Machine is muted.
	int AmpEnvStage;			// Status of playback

private:
	Coscillator Drummy;
	Coscillator Thump;

	double IncSpeed;			// It should be DecSpeed, but we need a negative value.
	double ThumpIncSpeed;
	int DecMode;
	float DecConstant;
	float ThumpDecConstant;

	int Spos;					// Current position.
	int Slength;				// Length in Samples.
	int DecLength;				// Decrement lenght in samples
	int ThumpLength;

	float SinVol;				// Volume of sine signal
	int VolChangePos;
	int VolDecay;				// position up to which do decay

	float SinInc;
	float VolDecayDec;			// Decrement value for decay (negative value)
	float VolSustainDec;		// Decrement value from decay to end (negative value)

	float ThumpVol;
	float ThumpDec;
	double VolDec;				// Decrement for OutVol when doing noteOff()
};

float Drum::GetSample() {
	//Function Inlined to Speed Up the machine (1.9% usage inlined, 2.0~2.1% not inlined)
	
	float output;

	if (Spos<ThumpLength){
		output=((sin(Drummy.GetPos()) * SinVol) + (sin(Thump.GetPos()) * ThumpVol)) * OutVol;
		ThumpVol-=ThumpDec;
	}
	else output=(sin(Drummy.GetPos()) * SinVol) * OutVol;

	if (Spos<DecLength)
	{
		switch(DecMode){
		case 0: Drummy.IncSpeedin(IncSpeed); 
			Thump.IncSpeedin(ThumpIncSpeed);
			break;
		case 1: Drummy.IncSpeedin(IncSpeed*(2.25 - DecConstant*(1 + Spos)));
			Thump.IncSpeedin(ThumpIncSpeed*(2.25 - DecConstant*(1 + Spos*ThumpLength/DecLength)));
			break;
		case 2: Drummy.IncSpeedin(IncSpeed*((DecConstant - Spos)/DecLength)); 
			 Thump.IncSpeedin(ThumpIncSpeed*((ThumpDecConstant - Spos)/ThumpLength)); 
			break;
		case 3://fallthrough
		default:
			break;
		}
	}
	
	if ( Spos == Slength) NoteOff();

	switch(AmpEnvStage) {

	case ST_NOTESUSTAIN:

		if ( Spos==VolChangePos)
		{
			if ( Spos<VolDecay) { VolChangePos=VolDecay;				SinInc=VolDecayDec; }
			else { VolChangePos=Slength;				SinInc=VolSustainDec; }
		}
		SinVol+=SinInc;

		break;

	case ST_NOTERELEASE:
		if ( OutVol > 0.00003 ) OutVol -=VolDec;
		else {
			AmpEnvStage=ST_NONOTE;
		}
		break;

	default:break;
	}

	Spos++;

	return(output);
}}