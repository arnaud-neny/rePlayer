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
#include "atlantisfilter.h"
#include "aaf.h"
#include "lfo.h"
#include "pwm.h"
namespace psycle::plugins::jme::blitzn {
#define HUGEANTIDENORMAL				0.0001f
#define FILTER_CALC_TIME				64

#define WAVE_SINE					0
#define	WAVE_ADLIB2					1
#define	WAVE_ADLIB3					2
#define	WAVE_ADLIB4					3
#define WAVE_SINE12					4
#define WAVE_SINECOSINE				5
#define	WAVE_HALFSINE				6
#define	WAVE_TRIANGLE				7
#define	WAVE_UPDOWN					8
#define WAVE_UPDOWNX2				9
#define WAVE_SAWTOOTH				10
#define WAVE_SAWTOOTHX2				11
#define	WAVE_EXPSAW					12
#define WAVE_MIDSINE				13
#define WAVE_STEPS					14
#define WAVE_SAWSQUARE				15
#define WAVE_SQUARE					16
#define WAVE_SQUAREX2				17
#define WAVE_TRISTATE				18
#define WAVE_POLYNEG				19
#define WAVE_POLYNEG2				20
#define WAVE_POLYNEG3				21
#define	WAVE_FORMANT2				22
#define	WAVE_FORMANT3				23
#define	WAVE_FORMANT4				24
#define	WAVE_FORMANT5				25
#define	WAVE_FORMANT6				26
#define	WAVE_FORMANT8				27
#define	WAVE_FORMANT10				28
#define WAVE_RANDOM					29
#define WAVE_RANDOM2				30
#define WAVE_RANDOM3				31
#define WAVE_RANDOM4				32
#define WAVE_RANDOM5				33
#define WAVE_OSC1WORKBUFFER			34
#define WAVE_OSC2WORKBUFFER			35
#define WAVE_OSC3WORKBUFFER			36
#define WAVE_OSC4WORKBUFFER			37
#define WAVETABLES					38
//#define OVERSAMPLING				16
#define FREQDIV						1.0f/(float)vpar->oversamplesAmt
#define BufferTemp					8


struct VOICEPAR
{
	int globalVolume;
	int globalCoarse;
	int globalFine;
	int globalGlide;
	int globalStereo;

	int arpPattern;
	int arpSpeed[2];
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

	//todo: check if all the wavetable size is needed. Take in mind the work buffer wavetables.
	signed short WaveTable[38][4100];

	int initposition[4];
	float syncvibe;
	float filtvibe;
	int restartfx;
	float stereoLR[2];
	int stereoPos;

	unsigned int waveTableSize;
	float wavetableCorrection;
	unsigned int sampleRate;
	float srCorrection;
	int oversamplesAmt;
};

class CSynthTrack  
{
public:
	CSynthTrack();
	virtual ~CSynthTrack();
	void InitVoice(VOICEPAR *voicePar);
	void OnSampleRateChange();
	void ResetSym();
	void NoteOn(int note, VOICEPAR *voicePar, int spd, float velocity);
	void InitEffect(int cmd,int val);
	void NoteOff();
	void NoteStop();

	void calcOneWave(int osc);
	void calcWaves(int mask);
	void PerformFx();
	void DoGlide();
	void RealNoteOn(bool arpClear);
	void Retrig();
	void NoteTie(int note);
	void OnChangeLfoDepth();
	void OnChangeLfoSpeed();
	float Filter(float x);
	inline void GetSample(float* slr);
	inline void GetEnvMod();
	inline float GetEnvAmp();
	inline void GetEnvFlt();


	int pwmcount;
	int fxcount;
	signed short WaveBuffer[9][2100]; // 8x buffer, 1x temp
	float NextGlide;
	float LastGlide;
	float DefGlide;
	float DCOglide;
	float semiglide;
	int nextNote;
	int nextSpd;
	int ampEnvStage;

private:
	void updateTuning();
	inline float freqChange(float freq);
	inline float getPitch(int oscCoarse, int oscFine, int arpTranspose, int p_vibadd,float modEnv, int options);

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
	float oscArpTranspose[4];
	int arpInput[4];
	int arpNotes;
	float arpList[22];
	int arpLen;
	int arpShuffle;
	int arpCount;
	int arpIndex;
	float curArp;
	bool tuningChange;
	float volMulti;
	pwm synfx[4];
	int synbase[4];
	int synposLast[4];
	int currentStereoPos;
	bool firstGlide;

	lfo lfoViber;
	float lfoViberSample;
	float lfoViberLast;
	filter m_filter;
	CSIDFilter a_filter;
	AAF16 aaf1;
	AAF16 aaf2;
	AAF16 aaf3;
	AAF16 aaf4;

	int updateCount;
	short timetocompute;

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
	
	int oscWorkBufferDifferent[4];
};

inline float CSynthTrack::freqChange(float freq)
{
		freq *= FREQDIV * vpar->wavetableCorrection;
		if (freq >= vpar->waveTableSize/2.f) freq = vpar->waveTableSize/2.f;
		return freq;
}

inline float CSynthTrack::getPitch(int oscCoarse, int oscFine, int arpTranspose, int p_vibadd,float modEnv, int options)
{
	if (options != 7 && options != 8 ) 
		return (float)pow(2.0, (bend+rbasenote+rsemitone+arpTranspose
								+vpar->globalCoarse+oscCoarse+(vpar->globalFine+oscFine+modEnv+p_vibadd)*0.00390625)/12.0);
	else
		return (float)pow(2.0, (dbasenote
								+vpar->globalCoarse+oscCoarse+(vpar->globalFine+oscFine+modEnv+p_vibadd)*0.00390625)/12.0);
}

inline void CSynthTrack::GetSample(float* slr)
{
	if(!ampEnvStage) {
		return;
	}

	//todo: Think about substituting pwmcount for a call to PerformFx
	pwmcount--;
	if (pwmcount < 0){
		pwmcount = 500;
		calcWaves(15);
	}

	if (vpar->lfoDelay && lfoViber.next()) {
		lfoViberLast=lfoViberSample;
		lfoViberSample=(float)lfoViber.getPosition();
		if (lfoViberSample != lfoViberLast) tuningChange=true;
	} 

	//Arpeggio
	fxcount--;
	if (fxcount < 0){
		fxcount = 50;
		GetEnvMod();
		if (modEnvValue != modEnvLast) tuningChange=true;

	}
	arpCount--;
	if (arpCount < 0){
		arpCount = vpar->arpSpeed[arpShuffle];
		if ((arpLen>1)&(stopRetrig == false)&((vpar->arpRetrig == 1)||((vpar->arpRetrig == 2)&(arpShuffle == 0)))) Retrig();
		arpShuffle=1-arpShuffle;
		arpIndex++;
		if (arpIndex >= arpLen) arpIndex = 0;
		curArp=arpList[arpIndex];
		tuningChange=true;
	}

	oldBuf[0]=curBuf[0]>>2;
	oldBuf[1]=curBuf[1]>>2;
	oldBuf[2]=curBuf[2]>>2;
	oldBuf[3]=curBuf[3]>>2;

	float output=0.0f;
	float output1=0.0f;
	float output2=0.0f;
	float output3=0.0f;
	float output4=0.0f;
	float decOutput1 = 0.0f;
	float decOutput2 = 0.0f;
	float decOutput3 = 0.0f;
	float decOutput4 = 0.0f;

	float sample = 0.0f;
	float phase = 0.0f;
	int pos = 0;
	if (tuningChange) updateTuning();
	int c = 0;


	bool dco1Enable = vpar->oscVolume[0] || vpar->rm1 || vpar->oscOptions[1]==1 || vpar->oscOptions[1]==2 || vpar->oscOptions[1]==8 || (vpar->oscFuncType[0]>=44) & (vpar->oscFuncType[0]!=47) || (vpar->oscFuncType[0]>=46) & (vpar->oscFuncType[0]!=49);
	bool dco2Enable = vpar->oscVolume[1] || vpar->rm1 || vpar->oscOptions[2]==1 || vpar->oscOptions[2]==2 || vpar->oscOptions[2]==8 || (vpar->oscFuncType[1]>=44) & (vpar->oscFuncType[1]!=47) || (vpar->oscFuncType[1]>=46) & (vpar->oscFuncType[1]!=49);
	bool dco3Enable = vpar->oscVolume[2] || vpar->rm2 || vpar->oscOptions[3]==1 || vpar->oscOptions[3]==2 || vpar->oscOptions[3]==8 || (vpar->oscFuncType[2]>=44) & (vpar->oscFuncType[2]!=47) || (vpar->oscFuncType[2]>=46) & (vpar->oscFuncType[2]!=49);
	bool dco4Enable = vpar->oscVolume[3] || vpar->rm2 || vpar->oscOptions[0]==1 || vpar->oscOptions[0]==2 || vpar->oscOptions[0]==8 || (vpar->oscFuncType[3]>=44) & (vpar->oscFuncType[3]!=47) || (vpar->oscFuncType[3]>=46) & (vpar->oscFuncType[3]!=49);

	for (c=0; c<vpar->oversamplesAmt; c++){
		if (dco1Enable){
			phase = dco1Position+dco1Last+fmData4;
			while (phase >= 2048.0f) phase -= 2048.0f;
			while (phase < 0.0f) phase += 2048.0f;
			pos = (int)phase;
			sample = WaveBuffer[curBuf[0]][pos];
			output1 = aaf1.process((WaveBuffer[curBuf[0]][pos+1] - sample) * (phase - (float)pos) + sample);
			if (vpar->oscFeedback[0] > 0.0f) dco1Last=(float)(HUGEANTIDENORMAL+output1)*(HUGEANTIDENORMAL+vpar->oscFeedback[0])*(HUGEANTIDENORMAL+fbCtl[0]);
			else dco1Last=0.0f;
			dco1Position+=rdco1Pitch;
			if(dco1Position>=2048.0f){
				dco1Position-=2048.0f;
				if (nextBuf[0]){
					curBuf[0]^=4;
					nextBuf[0]=0;
				}
				if ((vpar->oscOptions[1] == 1) || (vpar->oscOptions[1] == 2)|| (vpar->oscOptions[1] == 8)){
					dco2Position=dco1Position;
					if (nextBuf[1]){
						curBuf[1]^=4;
						nextBuf[1]=0;
					}
					if ((vpar->oscOptions[2] == 1) || (vpar->oscOptions[2] == 2)|| (vpar->oscOptions[2] == 8)){
						dco3Position=dco1Position;
						if (nextBuf[2]){
							curBuf[2]^=4;
							nextBuf[2]=0;
						}
						if ((vpar->oscOptions[3] == 1) || (vpar->oscOptions[3] == 2)|| (vpar->oscOptions[3] == 8)){
							dco4Position=dco1Position;
							if (nextBuf[3]){
								curBuf[3]^=4;
								nextBuf[3]=0;
							}
						}
					}
				}												
			}
		}

		if (dco2Enable){
			phase = dco2Position+dco2Last+fmData1;
			while (phase >= 2048.0f) phase -= 2048.0f;
			while (phase < 0.0f) phase += 2048.0f;
			pos = (int)phase;
			sample = WaveBuffer[1+curBuf[1]][pos];
			output2 = aaf2.process((WaveBuffer[1+curBuf[1]][pos+1] - sample) * (phase - (float)pos) + sample);
			if (vpar->oscFeedback[1] > 0.0f) dco2Last=(float)(HUGEANTIDENORMAL+output2)*(HUGEANTIDENORMAL+vpar->oscFeedback[1])*(HUGEANTIDENORMAL+fbCtl[1]);
			else dco2Last = 0.0f;
			dco2Position+=rdco2Pitch;
			if(dco2Position>=2048.0f){
				dco2Position-=2048.0f;
				if (nextBuf[1]){
					curBuf[1]^=4;
					nextBuf[1]=0;
				}
				if ((vpar->oscOptions[2] == 1) || (vpar->oscOptions[2] == 2)|| (vpar->oscOptions[2] == 8)){
					dco3Position=dco2Position;
					if (nextBuf[2]){
						curBuf[2]^=4;
						nextBuf[2]=0;
					}
					if ((vpar->oscOptions[3] == 1) || (vpar->oscOptions[3] == 2)|| (vpar->oscOptions[3] == 8)){
						dco4Position=dco2Position;
						if (nextBuf[3]){
							curBuf[3]^=4;
							nextBuf[3]=0;
						}
						if ((vpar->oscOptions[0] == 1) || (vpar->oscOptions[0] == 2)|| (vpar->oscOptions[0] == 8)){
							dco1Position=dco2Position;
							if (nextBuf[0]){
								curBuf[0]^=4;
								nextBuf[0]=0;
							}
						}
					}
				}
			}
		}

		if (dco3Enable){
			phase = dco3Position+dco3Last+fmData2;
			while (phase >= 2048.0f) phase -= 2048.0f;
			while (phase < 0.0f) phase += 2048.0f;
			pos = (int)phase;
			sample = WaveBuffer[2+curBuf[2]][pos];
			output3 = aaf3.process((WaveBuffer[2+curBuf[2]][pos+1] - sample) * (phase - (float)pos) + sample);
			if (vpar->oscFeedback[2] > 0.0f) dco3Last=(float)(HUGEANTIDENORMAL+output3)*(HUGEANTIDENORMAL+vpar->oscFeedback[2])*(HUGEANTIDENORMAL+fbCtl[2]);
			else dco3Last = 0.0f;
			dco3Position+=rdco3Pitch;
			if(dco3Position>=2048.0f){
				dco3Position-=2048.0f;
				if (nextBuf[2]){
					curBuf[2]^=4;
					nextBuf[2]=0;
				}
				if ((vpar->oscOptions[3] == 1) || (vpar->oscOptions[3] == 2)|| (vpar->oscOptions[3] == 8)){
					dco4Position=dco3Position;
					if (nextBuf[3]){
						curBuf[3]^=4;
						nextBuf[3]=0;
					}
					if ((vpar->oscOptions[0] == 1) || (vpar->oscOptions[0] == 2)|| (vpar->oscOptions[0] == 8)){
						dco1Position=dco3Position;
						if (nextBuf[0]){
							curBuf[0]^=4;
							nextBuf[0]=0;
						}
						if ((vpar->oscOptions[1] == 1) || (vpar->oscOptions[1] == 2)|| (vpar->oscOptions[1] == 8)){
							dco2Position=dco3Position;
							if (nextBuf[1]){
								curBuf[1]^=4;
								nextBuf[1]=0;
							}
						}
					}
				}
			}
		}

		if (dco4Enable){
			phase = dco4Position+dco4Last+fmData3;
			while (phase >= 2048.0f) phase -= 2048.0f;
			while (phase < 0.0f) phase += 2048.0f;
			pos = (int)phase;
			sample = WaveBuffer[3+curBuf[3]][pos];
			output4 = aaf4.process((WaveBuffer[3+curBuf[3]][pos+1] - sample) * (phase - (float)pos) + sample);
			if (vpar->oscFeedback[3] > 0.0f) dco4Last=(float)(HUGEANTIDENORMAL+output4)*(HUGEANTIDENORMAL+vpar->oscFeedback[3])*(HUGEANTIDENORMAL+fbCtl[3]);
			else dco4Last=0.0f;
			dco4Position+=rdco4Pitch;
			if(dco4Position>=2048.0f){
				dco4Position-=2048.0f;
				if (nextBuf[3]){
					curBuf[3]^=4;
					nextBuf[3]=0;
				}
				if ((vpar->oscOptions[0] == 1) || (vpar->oscOptions[0] == 2)|| (vpar->oscOptions[0] == 8)){
					dco1Position=dco4Position;
					if (nextBuf[0]){
						curBuf[0]^=4;
						nextBuf[0]=0;
					}
					if ((vpar->oscOptions[1] == 1) || (vpar->oscOptions[1] == 2)|| (vpar->oscOptions[1] == 8)){
						dco2Position=dco4Position;
						if (nextBuf[1]){
							curBuf[1]^=4;
							nextBuf[1]=0;
						}
						if ((vpar->oscOptions[2] == 1) || (vpar->oscOptions[2] == 2)|| (vpar->oscOptions[2] == 8)){
							dco3Position=dco4Position;
							if (nextBuf[2]){
								curBuf[2]^=4;
								nextBuf[2]=0;
							}
						}
					}
				}
			}
		}

		
		fmData1 = 0.0f;
		fmData2 = 0.0f;
		fmData3 = 0.0f;
		fmData4 = 0.0f;

		if (dco1Enable){
			fmData1 += (ANTIDENORMAL+fmCtl[oldBuf[0]][0])*(ANTIDENORMAL+output1);
			fmData3 += (ANTIDENORMAL+fmCtl2[oldBuf[0]][0])*(ANTIDENORMAL+output1);
		}

		if (dco2Enable){
			fmData2 += (ANTIDENORMAL+fmCtl[oldBuf[1]][1])*(ANTIDENORMAL+output2);
			fmData4 += (ANTIDENORMAL+fmCtl2[oldBuf[1]][1])*(ANTIDENORMAL+output2);
		}

		if (dco3Enable){
			fmData1 += (ANTIDENORMAL+fmCtl2[oldBuf[2]][2])*(ANTIDENORMAL+output3);
			fmData3 += (ANTIDENORMAL+fmCtl[oldBuf[2]][2])*(ANTIDENORMAL+output3);
		}

		if (dco4Enable){
			fmData2 += (ANTIDENORMAL+fmCtl2[oldBuf[3]][3])*(ANTIDENORMAL+output4);
			fmData4 += (ANTIDENORMAL+fmCtl[oldBuf[3]][3])*(ANTIDENORMAL+output4);
		}


		decOutput1 += output1;
		decOutput2 += output2;
		decOutput3 += output3;
		decOutput4 += output4;
	}

	output1 = decOutput1 * FREQDIV;
	output2 = decOutput2 * FREQDIV;
	output3 = decOutput3 * FREQDIV;
	output4 = decOutput4 * FREQDIV;

	output=((ANTIDENORMAL+output1)*(ANTIDENORMAL+osc1Vol))+((ANTIDENORMAL+output2)*(ANTIDENORMAL+osc2Vol))+((ANTIDENORMAL+output3)*(ANTIDENORMAL+osc3Vol))+((ANTIDENORMAL+output4)*(ANTIDENORMAL+osc4Vol))+((ANTIDENORMAL+output1)*(ANTIDENORMAL+output2)*(ANTIDENORMAL+rm1Vol))+((ANTIDENORMAL+output3)*(ANTIDENORMAL+output4)*(ANTIDENORMAL+rm2Vol));				

	//master sat
	if ((vpar->oscFuncType[0]==47)||(vpar->oscFuncType[1]==47)||(vpar->oscFuncType[2]==47)||(vpar->oscFuncType[3]==47)){
		long long1 = (ANTIDENORMAL+output)*(ANTIDENORMAL+satClip);
		if (long1 > 16384) long1=16384;
		if (long1 < -16384) long1=-16384;
		output=long1;
	}

	//cutoff mod
	float cutmod = 0.0f;
	if (vpar->oscFuncType[0]==46) cutmod+=((output1+16384)*(ANTIDENORMAL+cmCtl[0]));
	if (vpar->oscFuncType[1]==46) cutmod+=((output2+16384)*(ANTIDENORMAL+cmCtl[1]));
	if (vpar->oscFuncType[2]==46) cutmod+=((output3+16384)*(ANTIDENORMAL+cmCtl[2]));
	if (vpar->oscFuncType[3]==46) cutmod+=((output4+16384)*(ANTIDENORMAL+cmCtl[3]));

	GetEnvFlt();
	if (vpar->fltType){
		if(!timetocompute--) {
			timetocompute=FILTER_CALC_TIME;
			int realcutoff=int(vpar->fltCutoff+(vpar->filtvibe*0.005)+(rbasenote*0.1*vpar->fltTrack)+(fltEnvValue*vpar->fltEnvAmount*((1-vpar->fltVelocity)+(voiceVol*vpar->fltVelocity))));
			if(realcutoff<1)realcutoff=1;
			if(realcutoff>250)realcutoff=250;
			rcCut+=(realcutoff-50-rcCut)*rcCutCutoff;
			int cf = rcCut+cutmod;
			
			// old filter
			if (vpar->fltType <= 10) m_filter.setfilter(vpar->fltType, cf, vpar->fltResonance);

			// atlantis sid filter
			if (cf > 256.0f) cf = 256.0f;
			if (cf < 0.0f) cf = 0.0f;

			a_filter.recalculateCoeffs(((float)cf)/256.0f, (float)vpar->fltResonance/256.0f, vpar->sampleRate);
		}
		if (vpar->fltType <= 10) output = m_filter.res(output); // old filter
		else {
			a_filter.setAlgorithm((eAlgorithm)(vpar->fltType - 11));
			a_filter.process(output); // atlantis sid filter
		}
		
	}

	rcVol+=(((GetEnvAmp()*((1-vpar->ampVelocity)+(voiceVol*vpar->ampVelocity))*softenHighNotes)-rcVol)*rcVolCutoff);
	output*=rcVol;
	slr[0]=output*vpar->stereoLR[currentStereoPos];
	slr[1]=output*vpar->stereoLR[1-currentStereoPos];
}
inline void CSynthTrack::GetEnvMod(){
	switch (modEnvStage){
	case 1:
		modEnvValue+=modEnvCoef;
		if(modEnvValue>1.0f){
			modEnvCoef=1.0f/(float)vpar->modD;
			modEnvStage=2;
		}
		break;
	case 2:
		modEnvValue-=modEnvCoef;
		if(modEnvValue<0){
			modEnvValue=0;
			modEnvStage=0;
		}
		break;
	}
}

inline float CSynthTrack::GetEnvAmp(){
	switch(ampEnvStage)
	{
	case 1: // Attack
		ampEnvValue+=ampEnvCoef;
		
		if(ampEnvValue>1.0f)
		{
			ampEnvCoef=((1.0f-ampEnvSustainLevel)/(float)vpar->ampD)*speedup;
			if (ampEnvCoef>minFade) ampEnvCoef=minFade;
			ampEnvStage=2;
		}

		return ampEnvValue;
	break;

	case 2: // Decay
		ampEnvValue = ampEnvValue - (ampEnvValue * ampEnvCoef * 5.0f)*vpar->srCorrection;

		if((ampEnvValue<ampEnvSustainLevel) || (ampEnvValue<0.001f))
		{
			ampEnvValue=ampEnvSustainLevel;
			ampEnvCoef=((ampEnvSustainLevel)/(float)vpar->ampD2)*speedup;
			if (ampEnvCoef>minFade) ampEnvCoef=minFade;
			ampEnvStage=3;

			if(!vpar->ampS)
			ampEnvStage=0;
		}

		return ampEnvValue;
	break;

	case 3:
		// Decay 2
		ampEnvValue = ampEnvValue - (ampEnvValue * ampEnvCoef * 5.0f)*vpar->srCorrection;
		
		if(ampEnvValue<=0.001f){
			ampEnvValue=0.0f;
			ampEnvStage=0;
		}
		return ampEnvValue;
	break;

	case 4: // Release
		ampEnvValue = ampEnvValue - (ampEnvValue * ampEnvCoef * 5.0f)*vpar->srCorrection;

		if(ampEnvValue<0.001f){
			ampEnvValue=0.0f;
			ampEnvStage=0;
		}
		return ampEnvValue;
	break;
	
	case 5: // FastRelease
		ampEnvValue-=ampEnvCoef;

		if(ampEnvValue<0.001f)
		{
			RealNoteOn((sp_cmd == 0xCD) || (sp_cmd == 0xCE));
			ampEnvValue=0.0f;
			ampEnvStage=1;
			ampEnvCoef=(1.0f/(float)vpar->ampA)*speedup;
			if (ampEnvCoef>minFade) ampEnvCoef=minFade;
		}

		return ampEnvValue;
	break;

	case 6: // FastRelease for Arpeggio
		ampEnvValue-=ampEnvCoef;

		if(ampEnvValue<0.001f)
		{
			ampEnvValue=0.0f;
			ampEnvStage=1;
			ampEnvCoef=(1.0f/(float)vpar->ampA)*speedup;
			if (ampEnvCoef>minFade) ampEnvCoef=minFade;
		}

		return ampEnvValue;
	break;

	}

	return 0;

}

inline void CSynthTrack::GetEnvFlt()
{
	switch(fltEnvStage)
	{
		case 1: // Attack
		fltEnvValue+=fltEnvCoef;
		
		if(fltEnvValue>1.0f)
		{
			fltEnvCoef=((1.0f-fltEnvSustainLevel)/(float)vpar->fltD)*speedup2;
			if (fltEnvCoef>minFade) fltEnvCoef=minFade;
			fltEnvStage=2;
		}
	break;

	case 2: // Decay
		fltEnvValue-=fltEnvCoef;
		
		if(fltEnvValue<fltEnvSustainLevel)
		{
			fltEnvValue=fltEnvSustainLevel;
			fltEnvCoef=((fltEnvSustainLevel)/(float)vpar->fltD2)*speedup2;
			if (fltEnvCoef>minFade) fltEnvCoef=minFade;
			fltEnvStage=3;
		}
	break;

	case 3:
		// Decay 2
		fltEnvValue-=fltEnvCoef;
		
		if(fltEnvValue<=0)
		{
			fltEnvValue=0;
			fltEnvStage=0;
		}
	break;

	case 4: // Release
		fltEnvValue-=fltEnvCoef;

		if(fltEnvValue<0.0f)
		{
			fltEnvValue=0.0f;
			fltEnvStage=0;
		}
	break;

	case 5: // FastRelease
		fltEnvValue-=fltEnvCoef;

		if(fltEnvValue<0.0f)
		{
			fltEnvValue=0.0f;
			fltEnvStage=1;
			fltEnvCoef=(1.0f/(float)vpar->fltA)*speedup2;
			if (fltEnvCoef>minFade) fltEnvCoef=minFade;
		}
	break;
	}
}
}