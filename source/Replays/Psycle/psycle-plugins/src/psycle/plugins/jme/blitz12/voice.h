/*								Blitz (C)2005 by jme
		Programm is based on Arguru Bass. Filter seems to be Public Domain.

		This plugin is free software; you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation; either version 2 of the License, or
		(at your option) any later version.\n"\

		This plugin is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
		GNU General Public License for more details.

		You should have received a copy of the GNU General Public License
		along with this program; if not, write to the Free Software
		Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#pragma once
#include "filter.h"
#include "lfo.h"
#include "pwm.h"
namespace psycle::plugins::jme::blitz12 {
#define WAVE_SINE								0
#define				WAVE_ADLIB2								1
#define				WAVE_ADLIB3								2
#define				WAVE_ADLIB4								3
#define WAVE_SINE12								4
#define WAVE_SINECOSINE				5
#define				WAVE_HALFSINE				6
#define				WAVE_TRIANGLE				7
#define				WAVE_UPDOWN								8
#define WAVE_UPDOWNX2				9
#define WAVE_SAWTOOTH				10
#define WAVE_SAWTOOTHX2				11
#define				WAVE_EXPSAW								12
#define WAVE_MIDSINE				13
#define WAVE_STEPS								14
#define WAVE_SAWSQUARE				15
#define WAVE_SQUARE								16
#define WAVE_SQUAREX2				17
#define WAVE_TRISTATE				18
#define WAVE_POLYNEG				19
#define WAVE_POLYNEG2				20
#define WAVE_POLYNEG3				21
#define				WAVE_FORMANT2				22
#define				WAVE_FORMANT3				23
#define				WAVE_FORMANT4				24
#define				WAVE_FORMANT5				25
#define				WAVE_FORMANT6				26
#define				WAVE_FORMANT8				27
#define				WAVE_FORMANT10				28
#define WAVE_RANDOM								29
#define WAVE_RANDOM2				30
#define WAVE_RANDOM3				31
#define WAVE_RANDOM4				32
#define WAVE_RANDOM5				33

struct VOICEPAR
{
	int globalVolume;
	int globalCoarse;
	int globalFine;
	int globalGlide;
	int globalStereo;

	int arpPattern;
	int arpSpeed;
	int arpShuffle;
	int arpRetrig;

	int lfoDelay;
	int lfoDepth;
	int lfoSpeed;
	int lfoDestination;

	int oscVolume[4];
	int oscCoarse[4];
	int oscFine[4];
	int oscWaveform[4];
	float oscFeedback[4];
	int oscOptions[4];
	int oscFuncType[4];
	int oscFuncSym[4];
	int oscSymDrift[4];
	int oscSymDriftRange[4];
	int oscSymDriftSpeed[4];
	int oscSymLfoRange[4];
	int oscSymLfoSpeed[4];

	int rm1;
	int rm2;

	int modA;
	int modD;
	float modEnvAmount;

	int ampA;
	int ampD;
	int ampS;
	long ampD2;
	int ampR;
	int ampScaling;
	float ampVelocity;
	float ampTrack;

	int fltType;
	float fltCutoff;
	int fltResonance;
	int fltTrack;
	int fltSweep;
	int fltSpeed;
	int fltA;
	int fltD;
	int fltS;
	long fltD2;
	int fltR;
	int fltScaling;
	float fltVelocity;
	float fltEnvAmount;

	signed short WaveTable[34][4100];

	int initposition[4];
	float syncvibe;
	float filtvibe;
	int restartfx;
	float stereoLR[2];
	int stereoPos;
};

class CSynthTrack  
{
public:
	void calcOneWave(int osc);
	void calcWaves(int mask);
	float freqlimit(float freq) {
		if (freq > 96) freq = 96;
		return freq;
	}
	int pwmcount;
	int fxcount;
	signed short WaveBuffer[9][2100]; // the last is only for temporary data
	void InitEffect(int cmd,int val);
	void PerformFx();
	void DoGlide();
	float Filter(float x);
	void NoteOff();
	void NoteStop();
	void GetEnvMod();
	float GetEnvAmp();
	void GetEnvFlt();
	float NextGlide;
	float LastGlide;
	float DefGlide;
	float DCOglide;
	float semiglide;
	void GetSample(float* slr);
	void InitVoice(VOICEPAR *voicePar);
	void ResetSym();
	void NoteOn(int note, VOICEPAR *voicePar, int spd, float velocity);
	int nextNote;
	int nextSpd;
	void RealNoteOn();
	void Retrig();
	void NoteTie(int note);
	void changeLfoDepth(int val);
	void changeLfoSpeed(int val);
	CSynthTrack();
	virtual ~CSynthTrack();
	int ampEnvStage;

private:
	float satClip;
	float cmCtl[4];
	float fbCtl[4];
	float fmCtl[2][4];
	float fmCtl2[2][4];
	float fmData1;
	float fmData2;
	float fmData3;
	float fmData4;
	int curBuf[4];
	int nextBuf[4];
	int oldBuf[4];
	float nextVol;
	float fastRelease;
	float minFade;
	float rcVol;
	float rcVolCutoff;
	float rcCut;
	float rcCutCutoff;
	float voiceVol;
	float softenHighNotes;
	int fltEnvStage;
	float basenote;
	float rbasenote;
	float dbasenote; //default (if no track)
	float semitone;
	float rsemitone;
	bool stopRetrig;
	int oscArpTranspose[4];
	int arpInput[4];
	int arpNotes;
	int arpList[22];
	int arpLen;
	int arpSpeed[2];
	int arpShuffle;
	int arpCount;
	int arpIndex;
	int curArp;
	bool tuningChange;
	float volMulti;
	pwm synfx[4];
	int synbase[4];
	int synposLast[4];
	int currentStereoPos;
	bool firstGlide;

	int lfocount;
	lfo lfoViber;
	float lfoViberSample;
	float lfoViberLast;
	filter m_filter;

	int updateCount;
	short timetocompute;
	void updateTuning();

	float fltResonance;
	int sp_cmd;
	int sp_val;
	float sp_gl;

	int last_mod[2];
	float dco1Position;
	float dco1Pitch;
	float rdco1Pitch;
	float dco1Last;

	float dco2Position;
	float dco2Pitch;
	float rdco2Pitch;
	float dco2Last;

	float dco3Position;
	float dco3Pitch;
	float rdco3Pitch;
	float dco3Last;

	float dco4Position;
	float dco4Pitch;
	float rdco4Pitch;
	float dco4Last;

	// ChanFX
	float bend;

	// Mod Envelope
	int modEnvStage;
	float modEnvValue;
	float modEnvCoef;
	float modEnvLast;

	// Envelope [Amplitude]
	float ampEnvValue;
	float ampEnvCoef;
	float ampEnvSustainLevel;
	float masterVolume;
	float osc1Vol;
	float osc2Vol;
	float osc3Vol;
	float osc4Vol;
	float rm1Vol;
	float rm2Vol;

	// Filter Envelope
	float fltEnvValue;
	float fltEnvCoef;
	float fltEnvSustainLevel;
	float fltCutoff;
	float fltAmt;
	VOICEPAR *vpar;

	float speedup;
	float speedup2;
	
	inline int f2i(double d)
	{
			return ((int)d) & 2047;
	};

	inline float freqChange(float freq)
	{
		if (freq > 666.0f) freq = 666.0f;
		return freq*0.25f; //4x oversampling
	};

};

}