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

#include "voice.h"
#include <cmath>
#define TWOPI																6.28318530717958647692528676655901f
#define ANTIDENORMAL					1e-15f

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
namespace psycle::plugins::jme::blitzn {
CSynthTrack::CSynthTrack() :
	pwmcount(0)
	,fxcount(0)
	,NextGlide(0.0f)
	,LastGlide(0.0f)
	,DefGlide(99999999.0f)
	,DCOglide(0.0f)
	,semiglide(0.0f)
	,nextNote(1)
	,nextSpd(0)
	,ampEnvStage(0)
	,satClip(0.0f)
	,fmData1(0.0f)
	,fmData2(0.0f)
	,fmData3(0.0f)
	,fmData4(0.0f)
	,nextVol(0.0f)
	,fastRelease(128.0f)
	,minFade(0.008f)
	,rcVol(0.0f)
	,rcVolCutoff(0.25f)
	,rcCut(0.0f)
	,rcCutCutoff(0.25f)
	,voiceVol(1.0f)
	,softenHighNotes(0.0f)
	,fltEnvStage(0)
	,basenote(0.0f)
	,rbasenote(0.0f)
	,dbasenote(64+1.235f)
	,semitone(0.0f)
	,rsemitone(0.0f)
	,arpNotes(0)
	,arpLen(0)
	,arpShuffle(0)
	,arpCount(0)
	,arpIndex(0)
	,curArp(0)
	,volMulti(0.0f)
	,currentStereoPos(0)
	,updateCount(1)
	,timetocompute(FILTER_CALC_TIME)
	,fltResonance(0.0f)
	,sp_cmd(0)
	,sp_val(0)
	,dco1Position(0.0f)
	,dco1Pitch(0.0f)
	,rdco1Pitch(0.0f)
	,dco1Last(0.0f)
	,dco2Position(0.0f)
	,dco2Pitch(0.0f)
	,rdco2Pitch(0.0f)
	,dco2Last(0.0f)
	,dco3Position(0.0f)
	,dco3Pitch(0.0f)
	,rdco3Pitch(0.0f)
	,dco3Last(0.0f)
	,dco4Position(0.0f)
	,dco4Pitch(0.0f)
	,rdco4Pitch(0.0f)
	,dco4Last(0.0f)
	,bend(0.0f)
	,modEnvStage(0)
	,modEnvValue(0.0f)
	,modEnvCoef(0.0f)
	,modEnvLast(0.0f)
	,ampEnvValue(0.0f)
	,ampEnvCoef(0.0f)
	,ampEnvSustainLevel(0.0f)
	,masterVolume(0.0f)
	,osc1Vol(0.0f)
	,osc2Vol(0.0f)
	,osc3Vol(0.0f)
	,osc4Vol(0.0f)
	,rm1Vol(0.0f)
	,rm2Vol(0.0f)
	,fltEnvValue(0.0f)
	,fltEnvCoef(0.0f)
	,fltEnvSustainLevel(0.0f)
	,fltCutoff(0.0f)
	,fltAmt(0.0f)
	,speedup(0.0f)
	,speedup2(0.0f)
{
	for(int i(0);i<4;++i)
	{
		cmCtl[i]=0.0f;
		fbCtl[i]=1.0f;
		curBuf[i]=0;
		nextBuf[i]=0;
		fmCtl[0][i]=1.0f;
		fmCtl[1][i]=1.0f;
		fmCtl2[0][i]=1.0f;
		fmCtl2[1][i]=1.0f;
	}

	lfoViber.setSkipStep(200);

	arpInput[0]=0;
	m_filter.Init(44100);
	firstGlide = true;
}

CSynthTrack::~CSynthTrack(){
}

void CSynthTrack::InitVoice(VOICEPAR *voicePar){
	vpar=voicePar;
}
void CSynthTrack::OnSampleRateChange() {
	///\todo: use wavetablesize and wtCorrection
	m_filter.Init(vpar->sampleRate);
	if (vpar->fltType <= 10) 
	{
		m_filter.setfilter(vpar->fltType, vpar->fltCutoff, vpar->fltResonance);
	}
	else {
		a_filter.recalculateCoeffs(((float)vpar->fltCutoff)/256.0f, (float)vpar->fltResonance/256.0f, vpar->sampleRate);
	}
	aaf1.Setup(16000, AAF16::lowpass, -3.0f, vpar->sampleRate*vpar->oversamplesAmt);
	aaf2.Setup(16000, AAF16::lowpass, -3.0f, vpar->sampleRate*vpar->oversamplesAmt);
	aaf3.Setup(16000, AAF16::lowpass, -3.0f, vpar->sampleRate*vpar->oversamplesAmt);
	aaf4.Setup(16000, AAF16::lowpass, -3.0f, vpar->sampleRate*vpar->oversamplesAmt);
	synfx[0].setSampleRate(vpar->sampleRate);
	synfx[1].setSampleRate(vpar->sampleRate);
	synfx[2].setSampleRate(vpar->sampleRate);
	synfx[3].setSampleRate(vpar->sampleRate);
	lfoViber.setSkipStep(200.f*vpar->sampleRate/44100.f);
	if (ampEnvStage) {
		ampEnvStage = 0;
	}
}
void CSynthTrack::ResetSym(){
	for (int i=0; i<4; i++){
		if (vpar->oscOptions[i]==10){
			synfx[i].once=true;
			synfx[i].twice=false;
		}
		else if (vpar->oscOptions[i]==11){
			synfx[i].once=false;
			synfx[i].twice=true;
		}
		else {
			synfx[i].once=false;
			synfx[i].twice=false;
		}
	}

	synbase[0]=vpar->initposition[0];
	synfx[0].reset();
	synbase[1]=vpar->initposition[1];
	synfx[1].reset();
	synbase[2]=vpar->initposition[2];
	synfx[2].reset();
	synbase[3]=vpar->initposition[3];
	synfx[3].reset();
	pwmcount=0;

	oscWorkBufferDifferent[0] = 15; // set all channels to update (%1111)
	oscWorkBufferDifferent[1] = 15; // set all channels to update (%1111)
	oscWorkBufferDifferent[2] = 15; // set all channels to update (%1111)
	oscWorkBufferDifferent[3] = 15; // set all channels to update (%1111)
}

void CSynthTrack::NoteTie(int note){
	//Compensation for the difference between wavetable size and base note.
	basenote=note+0.2344f;
	rsemitone=0.0f;
	semiglide=0.0f;
	bend=0;
	if (NextGlide){
		DCOglide = NextGlide;
		NextGlide = 0;
	} else {
		if (vpar->globalGlide == 0) { DefGlide=256.0f; }
		else DefGlide=float((vpar->globalGlide*vpar->globalGlide)*0.0000625f);
		DCOglide = DefGlide;
	}
}

void CSynthTrack::NoteOn(int note, VOICEPAR *voicePar, int spd, float velocity){
	stopRetrig=true;
	vpar=voicePar;
	nextNote=note;
	nextSpd=spd;
	nextVol=velocity+HUGEANTIDENORMAL;
	ampEnvSustainLevel=(float)vpar->ampS*0.0039062f;
	if(ampEnvStage==0){
		RealNoteOn(true); // THIS LINE DIFFERS FROM RETRIG()
		ampEnvStage=1;
		ampEnvCoef=(1.0f/(float)vpar->ampA)*speedup;
		if (ampEnvCoef>minFade) ampEnvCoef=minFade;
	}else{
		ampEnvStage=5;
		ampEnvCoef=ampEnvValue/fastRelease;
		if(ampEnvCoef<=0.0f) ampEnvCoef=0.03125f;
	}
	fltEnvSustainLevel=(float)vpar->fltS*0.0039062f;
	if(fltEnvStage==0){
		fltEnvStage=1;
		fltEnvCoef=(1.0f/(float)vpar->fltA)*speedup2;
		if (fltEnvCoef>minFade) fltEnvCoef=minFade;
	}else{
		fltEnvStage=5;
		fltEnvCoef=fltEnvValue/fastRelease;
		if(fltEnvCoef<=0.0f)fltEnvCoef=0.03125f;
	}
}

void CSynthTrack::OnChangeLfoDepth(){
	lfoViber.setLevel(vpar->lfoDepth);
}

void CSynthTrack::OnChangeLfoSpeed(){
	lfoViber.setSpeed(vpar->lfoSpeed);
}

void CSynthTrack::RealNoteOn(bool arpClear){
	voiceVol=nextVol;
	stopRetrig=false;
	updateCount=1;
	int note=nextNote;
	int spd=nextSpd;
	vpar->stereoPos=1-vpar->stereoPos;
	currentStereoPos=vpar->stereoPos;

	fltResonance=(float)vpar->fltResonance/256.0f;
	modEnvStage=1;
	modEnvCoef=(1.0f/(float)vpar->modA);

	ResetSym();
	pwmcount=500;
	synposLast[0]=9999;
	synposLast[1]=9999;
	synposLast[2]=9999;
	synposLast[3]=9999;
	synbase[0]=vpar->initposition[0];
	synbase[1]=vpar->initposition[1];
	synbase[2]=vpar->initposition[2];
	synbase[3]=vpar->initposition[3];
	curBuf[0]=4;
	curBuf[1]=4;
	curBuf[2]=4;
	curBuf[3]=4;
	calcWaves(15);
	nextBuf[0]=0;
	nextBuf[1]=0;
	nextBuf[2]=0;
	nextBuf[3]=0;
	curBuf[0]=0;
	curBuf[1]=0;
	curBuf[2]=0;
	curBuf[3]=0;

	lfoViberSample=0;
	lfoViber.setLevel(vpar->lfoDepth);
	lfoViber.setSpeed(vpar->lfoSpeed);
	lfoViber.setDelay(vpar->lfoDelay);
	lfoViber.reset();

	softenHighNotes=1.0f-((float)pow(2.0,(note-7)/12.0)*vpar->ampTrack*0.015625);
	if (softenHighNotes < 0.0f) softenHighNotes = 0.0f;

	fxcount=0;
	if (NextGlide){
		DCOglide = NextGlide;
		NextGlide = 0;
	} else {
		if (vpar->globalGlide == 0) { DefGlide=256.0f; }
		else DefGlide=float((vpar->globalGlide*vpar->globalGlide)*0.0000625f);
		DCOglide = DefGlide;
	}
	//Compensation for the difference between wavetable size and base note.
	basenote=note+0.2344f;
	rsemitone=0.0f;
	semiglide=0.0f;
	bend=0;

	modEnvValue=0.0f;
	modEnvLast=0.0f;

	arpShuffle=0;
	arpCount=0;  // means arpeggio code has to be executed
	arpLen=1;
	arpIndex=-1; // next arp position is position 0
	if (arpClear){
		curArp=0.0f; // current transpose
		arpInput[1]=0.0f;arpInput[2]=0.0f;arpInput[3]=0.0f;
		arpList[0]=0.0f;arpList[1]=0.0f;arpList[2]=0.0f;
		arpList[3]=0.0f;arpList[4]=0.0f;arpList[5]=0.0f;arpList[6]=0.0f;arpList[7]=0.0f;arpList[8]=0.0f;arpList[9]=0.0f;arpList[10]=0.0f;arpList[11]=0.0f;arpList[12]=0.0f;
		arpList[13]=0.0f;arpList[14]=0.0f;arpList[15]=0.0f;arpList[16]=0.0f;arpList[17]=0.0f;arpList[18]=0.0f;arpList[19]=0.0f;arpList[20]=0.0f;arpList[21]=0.0f;
		oscArpTranspose[0]=0.0f;
		oscArpTranspose[1]=0.0f;
		oscArpTranspose[2]=0.0f;
		oscArpTranspose[3]=0.0f;
	}
	float spdcoef;

	if(spd<65) spdcoef=(float)spd*0.015625f;
	else spdcoef=1.0f;

	volMulti = 0.0039062f*spdcoef;

	speedup=((float)vpar->ampScaling*basenote*0.0015f)+1.0f;
	speedup2=((float)vpar->fltScaling*basenote*0.0015f)+1.0f;
	if (speedup<1.0f) speedup=1.0f;
	if (speedup2<1.0f) speedup2=1.0f;

	if (vpar->oscOptions[0] == 9) dco1Position=0.0f;
	if (vpar->oscOptions[1] == 9) dco2Position=0.0f;
	if (vpar->oscOptions[2] == 9) dco3Position=0.0f;
	if (vpar->oscOptions[3] == 9) dco4Position=0.0f;
	if (vpar->oscOptions[0] == 12 || vpar->oscOptions[1] == 12 || vpar->oscOptions[2] == 12 || vpar->oscOptions[3] == 12){
		dco1Position=0.0f;
		dco2Position=0.0f;
		dco3Position=0.0f;
		dco4Position=0.0f;
	}
}

void CSynthTrack::updateTuning() {
	tuningChange=false;
	float modEnv = modEnvValue*vpar->modEnvAmount;
	float vibadd;
	if (vpar->lfoDelay == 0) vibadd=0.125f*(float)vpar->syncvibe;
	else vibadd = 0.125f*(float)lfoViberSample;
	for (int c = 0; c < 4; c++){
		switch (vpar->oscOptions[c]){
		case 2: oscArpTranspose[c] = 0; break;      // arpless sync
		case 3: oscArpTranspose[c] = 0; break;      // arp note 1
		case 4: oscArpTranspose[c] = arpInput[1]; break; // 2
		case 5: oscArpTranspose[c] = arpInput[2]; break; // 3
		case 6: oscArpTranspose[c] = arpInput[3]; break; // 4
		case 7: oscArpTranspose[c] = 0; break;      // no track
		case 8: oscArpTranspose[c] = 0; break;      // no track + sync
		default: oscArpTranspose[c] = curArp; break; // ---

		}
	}
	switch (vpar->lfoDestination) {
	case 0:
		//all osc
		dco1Pitch= getPitch(vpar->oscCoarse[0], vpar->oscFine[0], oscArpTranspose[0], vibadd, modEnv, vpar->oscOptions[0]);
		dco2Pitch= getPitch(vpar->oscCoarse[1], vpar->oscFine[1], oscArpTranspose[1], vibadd, modEnv, vpar->oscOptions[1]);
		dco3Pitch= getPitch(vpar->oscCoarse[2], vpar->oscFine[2], oscArpTranspose[2], vibadd, modEnv, vpar->oscOptions[2]);
		dco4Pitch= getPitch(vpar->oscCoarse[3], vpar->oscFine[3], oscArpTranspose[3], vibadd, modEnv, vpar->oscOptions[3]);
		break;
	case 1: // osc 2+3+4
		dco1Pitch= getPitch(vpar->oscCoarse[0], vpar->oscFine[0], oscArpTranspose[0], 0, modEnv, vpar->oscOptions[0]);
		dco2Pitch= getPitch(vpar->oscCoarse[1], vpar->oscFine[1], oscArpTranspose[1], vibadd, modEnv, vpar->oscOptions[1]);
		dco3Pitch= getPitch(vpar->oscCoarse[2], vpar->oscFine[2], oscArpTranspose[2], vibadd, modEnv, vpar->oscOptions[2]);
		dco4Pitch= getPitch(vpar->oscCoarse[3], vpar->oscFine[3], oscArpTranspose[3], vibadd, modEnv, vpar->oscOptions[3]);
		break;
	case 2: // osc 2+3
		dco1Pitch= getPitch(vpar->oscCoarse[0], vpar->oscFine[0], oscArpTranspose[0], 0, modEnv, vpar->oscOptions[0]);
		dco2Pitch= getPitch(vpar->oscCoarse[1], vpar->oscFine[1], oscArpTranspose[1], vibadd, modEnv, vpar->oscOptions[1]);
		dco3Pitch= getPitch(vpar->oscCoarse[2], vpar->oscFine[2], oscArpTranspose[2], vibadd, modEnv, vpar->oscOptions[2]);
		dco4Pitch= getPitch(vpar->oscCoarse[3], vpar->oscFine[3], oscArpTranspose[3], 0, modEnv, vpar->oscOptions[3]);
		break;
	case 3: // osc 2+4
		dco1Pitch= getPitch(vpar->oscCoarse[0], vpar->oscFine[0], oscArpTranspose[0], 0, modEnv, vpar->oscOptions[0]);
		dco2Pitch= getPitch(vpar->oscCoarse[1], vpar->oscFine[1], oscArpTranspose[1], vibadd, modEnv, vpar->oscOptions[1]);
		dco3Pitch= getPitch(vpar->oscCoarse[2], vpar->oscFine[2], oscArpTranspose[2], 0, modEnv, vpar->oscOptions[2]);
		dco4Pitch= getPitch(vpar->oscCoarse[3], vpar->oscFine[3], oscArpTranspose[3], vibadd, modEnv, vpar->oscOptions[3]);
		break;
	case 4: // osc 3+4
		dco1Pitch= getPitch(vpar->oscCoarse[0], vpar->oscFine[0], oscArpTranspose[0], 0, modEnv, vpar->oscOptions[0]);
		dco2Pitch= getPitch(vpar->oscCoarse[1], vpar->oscFine[1], oscArpTranspose[1], 0, modEnv, vpar->oscOptions[1]);
		dco3Pitch= getPitch(vpar->oscCoarse[2], vpar->oscFine[2], oscArpTranspose[2], vibadd, modEnv, vpar->oscOptions[2]);
		dco4Pitch= getPitch(vpar->oscCoarse[3], vpar->oscFine[3], oscArpTranspose[3], vibadd, modEnv, vpar->oscOptions[3]);
		break;
	case 5: // osc 2
		dco1Pitch= getPitch(vpar->oscCoarse[0], vpar->oscFine[0], oscArpTranspose[0], 0, modEnv, vpar->oscOptions[0]);
		dco2Pitch= getPitch(vpar->oscCoarse[1], vpar->oscFine[1], oscArpTranspose[1], vibadd, modEnv, vpar->oscOptions[1]);
		dco3Pitch= getPitch(vpar->oscCoarse[2], vpar->oscFine[2], oscArpTranspose[2], 0, modEnv, vpar->oscOptions[2]);
		dco4Pitch= getPitch(vpar->oscCoarse[3], vpar->oscFine[3], oscArpTranspose[3], 0, modEnv, vpar->oscOptions[3]);
		break;
	case 6: // osc 3
		dco1Pitch= getPitch(vpar->oscCoarse[0], vpar->oscFine[0], oscArpTranspose[0], 0, modEnv, vpar->oscOptions[0]);
		dco2Pitch= getPitch(vpar->oscCoarse[1], vpar->oscFine[1], oscArpTranspose[1], 0, modEnv, vpar->oscOptions[1]);
		dco3Pitch= getPitch(vpar->oscCoarse[2], vpar->oscFine[2], oscArpTranspose[2], vibadd, modEnv, vpar->oscOptions[2]);
		dco4Pitch= getPitch(vpar->oscCoarse[3], vpar->oscFine[3], oscArpTranspose[3], 0, modEnv, vpar->oscOptions[3]);
		break;
	case 7: // osc 4
		dco1Pitch= getPitch(vpar->oscCoarse[0], vpar->oscFine[0], oscArpTranspose[0], 0, modEnv, vpar->oscOptions[0]);
		dco2Pitch= getPitch(vpar->oscCoarse[1], vpar->oscFine[1], oscArpTranspose[1], 0, modEnv, vpar->oscOptions[1]);
		dco3Pitch= getPitch(vpar->oscCoarse[2], vpar->oscFine[2], oscArpTranspose[2], 0, modEnv, vpar->oscOptions[2]);
		dco4Pitch= getPitch(vpar->oscCoarse[3], vpar->oscFine[3], oscArpTranspose[3], vibadd, modEnv, vpar->oscOptions[3]);
		break;
	}
	rdco1Pitch=freqChange(dco1Pitch);
	rdco2Pitch=freqChange(dco2Pitch);
	rdco3Pitch=freqChange(dco3Pitch);
	rdco4Pitch=freqChange(dco4Pitch);
}

void CSynthTrack::Retrig(){
	ampEnvSustainLevel=(float)vpar->ampS*0.0039062f;
	if(ampEnvStage==0){
		ampEnvStage=1;
		ampEnvCoef=(1.0f/(float)vpar->ampA)*speedup;
		if (ampEnvCoef>minFade) ampEnvCoef=minFade;
	}else{
		ampEnvStage=6;
		ampEnvCoef=ampEnvValue/fastRelease;
		if(ampEnvCoef<=0.0f) ampEnvCoef=0.03125f;
	}
	fltEnvSustainLevel=(float)vpar->fltS*0.0039062f;
	if(fltEnvStage==0){
		fltEnvStage=1;
		fltEnvCoef=(1.0f/(float)vpar->fltA)*speedup2;
		if (fltEnvCoef>minFade) fltEnvCoef=minFade;
	}else{
		fltEnvStage=5;
		fltEnvCoef=fltEnvValue/fastRelease;
		if(fltEnvCoef<=0.0f)fltEnvCoef=0.03125f;
	}
}


void CSynthTrack::calcOneWave(int osc){
	synposLast[osc]=9999999;
	calcWaves(1<<osc);
}

void CSynthTrack::calcWaves(int mask){
	int c;
	synfx[0].setRange(vpar->oscSymLfoRange[0]);
	synfx[0].setSpeed(vpar->oscSymLfoSpeed[0]);
	synfx[1].setRange(vpar->oscSymLfoRange[1]);
	synfx[1].setSpeed(vpar->oscSymLfoSpeed[1]);
	synfx[2].setRange(vpar->oscSymLfoRange[2]);
	synfx[2].setSpeed(vpar->oscSymLfoSpeed[2]);
	synfx[3].setRange(vpar->oscSymLfoRange[3]);
	synfx[3].setSpeed(vpar->oscSymLfoSpeed[3]);
	synfx[0].next();
	synfx[1].next();
	synfx[2].next();
	synfx[3].next();

	//Continuous Drift?
	if ((vpar->oscOptions[0]>=13) & (vpar->oscOptions[0] <= 16)) synbase[0]=vpar->initposition[0];
	if ((vpar->oscOptions[1]>=13) & (vpar->oscOptions[0] <= 16)) synbase[1]=vpar->initposition[1];
	if ((vpar->oscOptions[2]>=13) & (vpar->oscOptions[0] <= 16)) synbase[2]=vpar->initposition[2];
	if ((vpar->oscOptions[3]>=13) & (vpar->oscOptions[0] <= 16)) synbase[3]=vpar->initposition[3];

	float float1, float2, float3, float4, float5 = 0;
	float size1, size2, step1(0), step2, phase;
	int work1, work2 = 0;
	long long1 = 0;
	long long2 = 0;
	int buf = 0;
	int bit = 0;
	for (int i=0; i<4; i++) {
		bit = 1<<i;
		if (bit & mask){
			int pos = (synbase[i]+synfx[i].getPosition()+vpar->oscFuncSym[i])&2047;
			float fpos = (float)pos;
			float adpos = fpos;
			if (adpos < HUGEANTIDENORMAL) adpos = HUGEANTIDENORMAL;
			if (vpar->oscFuncType[i] != 43) fbCtl[i]=1.0f;
			if ((vpar->oscFuncType[i] < 44) || (vpar->oscFuncType[i] > 45)){
				fmCtl[0][i]=0.0f;
				fmCtl[1][i]=0.0f;
			}
			if ((vpar->oscFuncType[i] < 48) || (vpar->oscFuncType[i] > 49)){
				fmCtl2[0][i]=0.0f;
				fmCtl2[1][i]=0.0f;
			}
			if ((pos != synposLast[i]) || ((vpar->oscWaveform[i] == WAVE_OSC1WORKBUFFER) & ((oscWorkBufferDifferent[i] & bit) != 0)) || ((vpar->oscWaveform[i] == WAVE_OSC2WORKBUFFER) & ((oscWorkBufferDifferent[i] & bit) != 0)) ||  ((vpar->oscWaveform[i] == WAVE_OSC3WORKBUFFER) & ((oscWorkBufferDifferent[i] & bit) != 0)) || ((vpar->oscWaveform[i] == WAVE_OSC4WORKBUFFER) & ((oscWorkBufferDifferent[i] & bit) != 0)) ) {
				synposLast[i]=pos;
				buf=4-curBuf[i]+i;
				//mark storage as read
				oscWorkBufferDifferent[0] = oscWorkBufferDifferent[0] & (15 ^ bit);
				oscWorkBufferDifferent[1] = oscWorkBufferDifferent[1] & (15 ^ bit);
				oscWorkBufferDifferent[2] = oscWorkBufferDifferent[2] & (15 ^ bit);
				oscWorkBufferDifferent[3] = oscWorkBufferDifferent[3] & (15 ^ bit);
				signed short* sourceWave = &vpar->WaveTable[vpar->oscWaveform[i]][0];
				switch (vpar->oscFuncType[i]) {
				case 0: // no function, just make a copy
				for (c=0;c<2048;c++){
					WaveBuffer[buf][c]=sourceWave[(c)];
				}
				break;
				case 1: // stretch&squash
					size1 = fpos+1.0f;
					size2 = 2048.0f-size1;
					if (size1!=0.0f) { step1 = 1024.0f/size1; phase=0.0f;} else { phase=1024.0f; }
					if (size2!=0.0f) { step2 = 1024.0f/size2; } else { step2 = 0.0f; }
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=sourceWave[int(phase)];
						if (phase < 1023.0f) phase+=step1; else phase+=step2;
					}
					break;
				case 2: // stretch&squash 2
					size1 = fpos+1.0f;
					size2 = 2048.0f-size1;
					if (size1!=0.0f) { step1 = 1024.0f/size1; phase = 0.0f; } else { phase=1024.0f; }
					if (size2!=0.0f) { step2 = 1024.0f/size2; } else { step2 = 0.0f; }
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=sourceWave[int(phase+phase)];
						if (phase < 1023.0f) phase+=step1; else phase+=step2;
					}
					break;
				case 3: // pw am
					for(c=0;c<2048;c++){
						if (c < pos) WaveBuffer[buf][c]=sourceWave[c];
						else WaveBuffer[buf][c]=sourceWave[c]>>1;
					}
					break;
				case 4: // squash&squash
					float1 = 0.0f;
					float2 = 0.0f;
					float4 = (2047.0f - fpos)/2.0f;   // 1st halve size in byte
					float5 = 1024.0f-float4;				// 2nd halve size in byte
					if (float4!=0.0f) float1 = 1024.0f/float4;	// 1st halve step
					else float1 = 1024.0f;
					if (float5!=0.0f) float2 = 1024.0f/float5;	// 2nd halve step
					else float2 = 0.0f;
					float3 = 0.0f;																				//source phase
					for(c=0;c<2100;c++){
						if (c<1024) {
							if (float3<1024) {
								if (float4) {
									WaveBuffer[buf][c]=sourceWave[(int)float3];
									float3+=float1;
								} else {
									WaveBuffer[buf][c]=0.0f;
									float3=1024.0f;
								}
							} else {
								if (float5) {
									WaveBuffer[buf][c]=0.0f;
									float3=1024.0f;
								}
							}
						} else {
							if (float3<2048.0f) {
								if (float4 != 0.0f) {
								WaveBuffer[buf][c]=sourceWave[(int)float3];
								float3+=float1;
								} else WaveBuffer[buf][c]=0.0f;
							} else {
								if (float5 != 0.0f) WaveBuffer[buf][c]=0.0f;
							}
						}
					}
					break;
				case 5: // Muted Sync
					float1 = 0.0f;
					float2 = adpos*6.0f/2047.0f+1.0f;
					for(c=0;c<2048;c++){
						if (float1<2047.0f){
							WaveBuffer[buf][c]=sourceWave[(int)float1];
							float1+=float2;
						if (float1 > 2047.0f) float1=2048.0f;
						} else WaveBuffer[buf][c]=0.0f;
					}
					break;
				case 6: // Syncfake
					float1 = 0.0f;
					float2 = adpos*6.0f/2047.0f+1.0f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=sourceWave[(int)float1];
						float1+=float2;
						if (float1 > 2047.0f) float1-=2048.0f;
					}
					break;
				case 7: // Restart
					work1 = 0;
					work2 = 2047-pos;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=sourceWave[work1];
						work1++; if (work1 > work2) work1 = 0;
					}
					break;
				case 8: // Negator
					for(c=0;c<2048;c++){
						if (c<pos)WaveBuffer[buf][c]=sourceWave[c];
						else WaveBuffer[buf][c]=0-sourceWave[c];
					}
					break;
				case 9: // Double Negator
					if (pos > 1023) pos = 2047-pos;
					for(c=0;c<1024;c++){
						if (c<pos){
							WaveBuffer[buf][c]=sourceWave[c];
							WaveBuffer[buf][2047-c]=sourceWave[2047-c];
						} else {
							WaveBuffer[buf][c]=0-sourceWave[c];
							WaveBuffer[buf][2047-c]=0-sourceWave[2047-c];
						}
					}
					break;
				case 10: // Rect Negator
					for(c=0;c<2048;c++){
					if (((pos+c)&2047)<1024)WaveBuffer[buf][c]=sourceWave[c];
						else WaveBuffer[buf][c]=0-sourceWave[c];
					}
					break;
				case 11: // Octaving
					float1 = (2047.0f-adpos)/2047.0f;
					float2 = adpos/2047.0f;
					if (float1 < HUGEANTIDENORMAL) float1 = HUGEANTIDENORMAL;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for (c=0;c<2048;c++){
						WaveBuffer[buf][c]=(signed short)((sourceWave[c]*float1)+(sourceWave[(c+c)&2047]*float2));
					}
					break;
				case 12: // FSK
					work1=0;
					work2=2047-pos;
					for (c=0;c<2048;c++){
						if (c<work2){
							WaveBuffer[buf][c]=sourceWave[c];
							work1=c+1;
						}
						else {
							WaveBuffer[buf][c]=sourceWave[work1];
							work1+=2; if (work1>2047) work1 -= 2048;
						}
					}
					break;
				case 13: // Mixer
					for (c=0;c<2048;c++){
						WaveBuffer[buf][c]=(sourceWave[c]>>1)+(sourceWave[(c+pos)&2047]>>1);
					}
					break;
				case 14: // Dual Mixer
					for (c=0;c<2048;c++){
						WaveBuffer[buf][c]=(signed short)((sourceWave[c]*0.3333333f)+(sourceWave[(c+pos)&2047]*0.3333333f)+(sourceWave[(c-pos)&2047]*0.3333333f));
					}
					break;
				case 15: // Fbk.Mixer
					for (c=0;c<2048;c++){
						WaveBuffer[buf][c]=(WaveBuffer[buf][c]>>1)+(sourceWave[(c+pos)&2047]>>1);
					}
					break;
				case 16: // Inv.Mixer
					for (c=0;c<2048;c++){
						WaveBuffer[buf][c]=(sourceWave[c]>>1)-(sourceWave[(c+pos)&2047]>>1);
					}
					break;
				case 17: // TriMix
					float1 = (2047.0f-adpos)/2047.0f;
					float2 = adpos/2047.0f;
					if (float1 < HUGEANTIDENORMAL) float1 = HUGEANTIDENORMAL;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=(signed short)((sourceWave[c]*float1)+(vpar->WaveTable[WAVE_TRIANGLE][c]*float2));
					}
					break;
				case 18: // SawMix
					float1 = (2047.0f-adpos)/2047.0f;
					float2 = adpos/2047.0f;
					if (float1 < HUGEANTIDENORMAL) float1 = HUGEANTIDENORMAL;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					work1 = 0;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=(signed short)((sourceWave[c]*float1)+((work1-16384)*float2));
						work1+=16;
					}
					break;
				case 19: // SqrMix
					float1 = (2047.0f-adpos)/2047.0f;
					float2 = adpos/2047.0f;
					if (float1 < HUGEANTIDENORMAL) float1 = HUGEANTIDENORMAL;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=(signed short)((sourceWave[c]*float1)+(vpar->WaveTable[WAVE_SQUARE][c]*float2));
					}
					break;
				case 20: // Tremolo
					float1 = (2047.0f-adpos)/2047.0f;
					if (float1 < HUGEANTIDENORMAL) float1 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=(signed short)((float)sourceWave[c]*float1);
					}
					break;
				case 21: // PM Sine 1
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_SINE][2047&(int)(c+(sourceWave[c]*float2))];
					}
					break;
				case 22: // PM Sine 2
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_SINE][2047&(int)(c+(sourceWave[(c+c)&2047]*float2))];
					}
					break;
				case 23: // PM Sine 3
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_SINE][2047&(int)(c+(sourceWave[(c+c+c)&2047]*float2))];
					}
					break;
				case 24: // PM Adlib2 1
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB2][2047&(int)(c+(sourceWave[c]*float2))];
					}
					break;
				case 25: // PM Adlib2 2
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB2][2047&(int)(c+(sourceWave[(c+c)&2047]*float2))];
					}
					break;
				case 26: // PM Adlib2 3
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB2][2047&(int)(c+(sourceWave[(c+c+c)&2047]*float2))];
					}
					break;
				case 27: // PM Adlib3 1
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB3][2047&(int)(c+(sourceWave[c]*float2))];
					}
					break;
				case 28: // PM Adlib3 2
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB3][2047&(int)(c+(sourceWave[(c+c)&2047]*float2))];
					}
					break;
				case 29: // PM Adlib3 3
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB3][2047&(int)(c+(sourceWave[(c+c+c)&2047]*float2))];
					}
					break;
				case 30: // PM Adlib4 1
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB4][2047&(int)(c+(sourceWave[c]*float2))];
					}
					break;
				case 31: // PM Adlib4 2
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB4][2047&(int)(c+(sourceWave[(c+c)&2047]*float2))];
					}
					break;
				case 32: // PM Adlib4 3
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB4][2047&(int)(c+(sourceWave[(c+c+c)&2047]*float2))];
					}
					break;												
				case 33: // PM Wave 1
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=sourceWave[2047&(int)(c+(sourceWave[c]*float2))];
					}
					break;
				case 34: // PM Wave 2
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=sourceWave[2047&(int)(c+(sourceWave[(c+c)&2047]*float2))];
					}
					break;
				case 35: // PM Wave 3
					float2 = adpos*0.00025f;
					if (float2 < HUGEANTIDENORMAL) float2 = HUGEANTIDENORMAL;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=sourceWave[2047&(int)(c+(sourceWave[(c+c+c)&2047]*float2))];
					}
					break;												
				case 36: // Dual Fix PM
					float2 = 0.125f; //16384:2048=1/8=0.125f
					for (c=0;c<2048;c++) WaveBuffer[BufferTemp][c]=sourceWave[2047&(int)(c+(sourceWave[(c+pos)&2047]*float2))];
					for (c=0;c<2048;c++) WaveBuffer[buf][c]=WaveBuffer[BufferTemp][2047&(int)(c+(sourceWave[(c-pos)&2047]*float2))];
					break;
				case 37: // Multiply
					for (c=0;c<2048;c++) WaveBuffer[buf][c]=sourceWave[c]*sourceWave[(c+pos)&2047]*0.0001f;;
					break;
				case 38: // AND
					for (c=0;c<2048;c++) WaveBuffer[buf][c]=sourceWave[c]&sourceWave[(c+pos)&2047];
					break;
				case 39: // EOR
					for (c=0;c<2048;c++) WaveBuffer[buf][c]=sourceWave[c]^sourceWave[(c+pos)&2047];
					break;
				case 40: // Boost (Hard Clip)
					float1 = pos*pos*0.000000015*pos+1.0f;
					for (c=0;c<2048;c++){
						long1=sourceWave[c]*float1;
						if (long1 > 16384) long1=16384;
						if (long1 < -16384) long1=-16384;
						WaveBuffer[buf][c]=long1;
					}
					break;
				case 41: // RM to AM (Upright)
					float1 = (2047.0f-fpos)/2047.0f*16384.0f;
					float2 = fpos/2047.0f*0.5f;
					for (c=0;c<2048;c++) WaveBuffer[buf][c]=(sourceWave[c]+16384)*float2+float1;
					break;
				case 42: // RM to AM (Down)
					float1 = (2047.0f-fpos)/2047*16384;
					float2 = fpos/2047*0.5f;
					for (c=0;c<2048;c++) WaveBuffer[buf][c]=float1-((sourceWave[c]-16384)*float2);
					break;
				case 43: // Feedback Ctrl
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=sourceWave[c];
					fbCtl[i]=(fpos-1024.0f)*0.0009765625f;
					break;
				case 44: // FM next +
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=sourceWave[c];
					fmCtl[buf>>2][i]=adpos*0.00048828125f;
					break;
				case 45: // FM next -
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=sourceWave[c];
					fmCtl[buf>>2][i]=0.0f-(adpos*0.00048828125f);
					break;
				case 46: // Filter Mod
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=sourceWave[c];
					cmCtl[i]=adpos*0.0000048828125f;
					break;
				case 47: // Chan Sat
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=sourceWave[c];
					satClip = adpos*adpos*0.000000015f*adpos+1.0f;
					break;
				case 48: // FM last +
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=sourceWave[c];
					fmCtl2[buf>>2][i]=adpos*0.00048828125f;
					break;
				case 49: // FM last -
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=sourceWave[c];
					fmCtl2[buf>>2][i]=0.0f-(adpos*0.00048828125f);
					break;
				case 50: // X Rotator
					for (c=0;c<2048;c++){
						WaveBuffer[buf][c]=sourceWave[(c+pos)&2047];
					}
					break;
				case 51: // Y Rotator
					long2 = adpos/2046.0f*32768.0f;
					for (c=0;c<2048;c++){
						long1=sourceWave[c]+long2;
						if (long1 > 16384) long1-=32768;
						if (long1 < -16384) long1+=32768;
						WaveBuffer[buf][c]=long1;
					}
					break;
				case 52: // Boost II (Wrap)
					float1 = adpos/2047.0f;
					float1 += float1+1.0f;
					for (c=0;c<2048;c++){
						long1=sourceWave[c]*float1;
						while (long1 > 16384) long1-=32768;
						while (long1 < -16384) long1+=32768;
						WaveBuffer[buf][c]=long1;
					}
					break;
				case 53: // Sync & Fade
					float1 = 0;
					float2 = adpos*96.0f/2047.0f+1.0f;
					float3 = 1.0f;
					float4 = 1.0f/2048.0f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=sourceWave[(int)float1]*float3;
						float1+=float2;
						float3-=float4;
						if (float1 > 2047.0f) float1-=2048.0f;
					}
					break;
				case 54: // Forward & Backward
					for(c=0;c<1024;c++){
						WaveBuffer[buf][c]=sourceWave[(pos+c+c)&2047];
						WaveBuffer[buf][2047-c]=WaveBuffer[buf][c];
					}
					break;
				default: // nothing
					break;
				}

				// make a copy of the first sample
				WaveBuffer[buf][2048]=WaveBuffer[buf][0];

				// a new buffer is now present
				nextBuf[i]=1;

				// store in selecatable waves ram
				for (c=0;c<2048;c++) vpar->WaveTable[WAVE_OSC1WORKBUFFER+i][c] = WaveBuffer[buf][c];
				oscWorkBufferDifferent[i] = 15; // set all channels to update (%1111)
			}
		}
	}
}


void CSynthTrack::NoteOff()
{
	stopRetrig=true;
	if((ampEnvStage!=4)&&(ampEnvStage!=0)){
		ampEnvStage=4;
		ampEnvCoef=(ampEnvValue/(float)vpar->ampR)*speedup;
		if (ampEnvCoef>minFade) ampEnvCoef=minFade;
	}
	if((fltEnvStage!=4)&&(fltEnvStage!=0)){
		fltEnvStage=4;
		fltEnvCoef=(fltEnvValue/(float)vpar->fltR)*speedup2;
		if (fltEnvCoef>minFade) fltEnvCoef=minFade;
	}
}

void CSynthTrack::NoteStop()
{
	stopRetrig=true;
	if(ampEnvStage){
		ampEnvStage=4;
		ampEnvCoef=ampEnvValue/32.0f;
	}
	if (fltEnvStage){
		fltEnvStage=4;
		fltEnvCoef=fltEnvValue/32.0f;
	}
}

void CSynthTrack::DoGlide() {

	// Glide Handler
	if (firstGlide){
		rbasenote=basenote;
		firstGlide = false;
	} else {
		if(rbasenote<basenote){
			rbasenote+=DCOglide;
			if(rbasenote>basenote) rbasenote=basenote;
			tuningChange=true;
		}else{
			if (rbasenote>basenote){
				rbasenote-=DCOglide;
				if(rbasenote<basenote) rbasenote=basenote;
				tuningChange=true;
			}
		}
	}
	// Semi Glide Handler
	if(rsemitone<semitone){
		rsemitone+=semiglide;
		if(rsemitone>semitone) rsemitone=semitone;
		tuningChange=true;
	}else{
		if (rsemitone>semitone){
			rsemitone-=semiglide;
			if(rsemitone<semitone) rsemitone=semitone;
			tuningChange=true;
		}
	}
}

void CSynthTrack::PerformFx()
{
	float shift;

	masterVolume=(float)vpar->globalVolume*volMulti*0.005f;
	osc1Vol=vpar->oscVolume[0]*masterVolume;
	osc2Vol=vpar->oscVolume[1]*masterVolume;
	osc3Vol=vpar->oscVolume[2]*masterVolume;
	osc4Vol=vpar->oscVolume[3]*masterVolume;
	rm1Vol=vpar->rm1*masterVolume*0.0001f;
	rm2Vol=vpar->rm2*masterVolume*0.0001f;

	// Perform tone glide
	DoGlide();

	if ( ((sp_cmd & 0xF0) == 0xD0) || ((sp_cmd & 0xF0) == 0xE0)) semiglide=sp_gl;

	if ( ((sp_cmd & 0xF0) < 0xC0) && (sp_cmd != 0x0C) && (sp_cmd != 0)) {
		arpInput[1] = sp_cmd>>4;
		arpInput[2] = sp_cmd&0x0f;
		arpInput[3] = sp_val;
		switch (vpar->arpPattern) {
		case 0: // 1oct Up
			arpList[0]=(float)arpInput[0];
			arpList[1]=(float)arpInput[1];
			arpList[2]=(float)arpInput[2];
			if ((float)arpInput[3] == 0) {
				arpLen=3;
			} else {
				arpLen=4;
				arpList[3]=(float)arpInput[3];
			}
			break;
		case 1: // 2oct Up
			arpList[0]=(float)arpInput[0];
			arpList[1]=(float)arpInput[1];
			arpList[2]=(float)arpInput[2];
			if ((float)arpInput[3] == 0) {
				arpLen=6;
				arpList[3]=(float)arpInput[0]+12.0f;
				arpList[4]=(float)arpInput[1]+12.0f;
				arpList[5]=(float)arpInput[2]+12.0f;
			} else {
				arpLen=8;
				arpList[3]=(float)arpInput[3];
				arpList[4]=(float)arpInput[0]+12.0f;
				arpList[5]=(float)arpInput[1]+12.0f;
				arpList[6]=(float)arpInput[2]+12.0f;
				arpList[7]=(float)arpInput[3]+12.0f;
			}
			break;
		case 2: // 3oct Up
			arpList[0]=(float)arpInput[0];
			arpList[1]=(float)arpInput[1];
			arpList[2]=(float)arpInput[2];
			if ((float)arpInput[3] == 0) {
				arpLen=9;
				arpList[3]=(float)arpInput[0]+12.0f;
				arpList[4]=(float)arpInput[1]+12.0f;
				arpList[5]=(float)arpInput[2]+12.0f;
				arpList[6]=(float)arpInput[0]+24.0f;
				arpList[7]=(float)arpInput[1]+24.0f;
				arpList[8]=(float)arpInput[2]+24.0f;
			} else {
				arpLen=12;
				arpList[3]=(float)arpInput[3];
				arpList[4]=(float)arpInput[0]+12.0f;
				arpList[5]=(float)arpInput[1]+12.0f;
				arpList[6]=(float)arpInput[2]+12.0f;
				arpList[7]=(float)arpInput[3]+12.0f;
				arpList[8]=(float)arpInput[0]+24.0f;
				arpList[9]=(float)arpInput[1]+24.0f;
				arpList[10]=(float)arpInput[2]+24.0f;
				arpList[11]=(float)arpInput[3]+24.0f;
			}
			break;
		case 3: // 1oct Down
			if ((float)arpInput[3] == 0) {
				arpLen=3;
				arpList[0]=(float)arpInput[2];
				arpList[1]=(float)arpInput[1];
				arpList[2]=(float)arpInput[0];
			} else {
				arpLen=4;
				arpList[0]=(float)arpInput[3];
				arpList[1]=(float)arpInput[2];
				arpList[2]=(float)arpInput[1];
				arpList[3]=(float)arpInput[0];
			}												
			break;
		case 4: // 2oct Down
			if ((float)arpInput[3] == 0) {
				arpLen=6;
				arpList[0]=(float)arpInput[2]+12.0f;
				arpList[1]=(float)arpInput[1]+12.0f;
				arpList[2]=(float)arpInput[0]+12.0f;
				arpList[3]=(float)arpInput[2];
				arpList[4]=(float)arpInput[1];
				arpList[5]=(float)arpInput[0];
			} else {
				arpLen=8;
				arpList[0]=(float)arpInput[3]+12.0f;
				arpList[1]=(float)arpInput[2]+12.0f;
				arpList[2]=(float)arpInput[1]+12.0f;
				arpList[3]=(float)arpInput[0]+12.0f;
				arpList[4]=(float)arpInput[3];
				arpList[5]=(float)arpInput[2];
				arpList[6]=(float)arpInput[1];
				arpList[7]=(float)arpInput[0];
			}												
			break;
		case 5: // 3oct Down
			if ((float)arpInput[3] == 0) {
				arpLen=9;
				arpList[0]=(float)arpInput[2]+24.0f;
				arpList[1]=(float)arpInput[1]+24.0f;
				arpList[2]=(float)arpInput[0]+24.0f;
				arpList[3]=(float)arpInput[2]+12.0f;
				arpList[4]=(float)arpInput[1]+12.0f;
				arpList[5]=(float)arpInput[0]+12.0f;
				arpList[6]=(float)arpInput[2];
				arpList[7]=(float)arpInput[1];
				arpList[8]=(float)arpInput[0];
			} else {
				arpLen=12;
				arpList[0]=(float)arpInput[3]+24.0f;
				arpList[1]=(float)arpInput[2]+24.0f;
				arpList[2]=(float)arpInput[1]+24.0f;
				arpList[3]=(float)arpInput[0]+24.0f;
				arpList[4]=(float)arpInput[3]+12.0f;
				arpList[5]=(float)arpInput[2]+12.0f;
				arpList[6]=(float)arpInput[1]+12.0f;
				arpList[7]=(float)arpInput[0]+12.0f;
				arpList[8]=(float)arpInput[3];
				arpList[9]=(float)arpInput[2];
				arpList[10]=(float)arpInput[1];
				arpList[11]=(float)arpInput[0];
			}												
			break;
		case 6: // 1oct Up/Down
			if ((float)arpInput[3] == 0) {
				arpLen=4;
				arpList[0]=(float)arpInput[0];
				arpList[1]=(float)arpInput[1];
				arpList[2]=(float)arpInput[2];
				arpList[3]=(float)arpInput[1];
			} else {
				arpLen=6;
				arpList[0]=(float)arpInput[0];
				arpList[1]=(float)arpInput[1];
				arpList[2]=(float)arpInput[2];
				arpList[3]=(float)arpInput[3];
				arpList[4]=(float)arpInput[2];
				arpList[5]=(float)arpInput[1];
			}												
			break;
		case 7: // 2oct Up/Down
			if ((float)arpInput[3] == 0) {
				arpLen=10;
				arpList[0]=(float)arpInput[0];
				arpList[1]=(float)arpInput[1];
				arpList[2]=(float)arpInput[2];
				arpList[3]=(float)arpInput[0]+12.0f;
				arpList[4]=(float)arpInput[1]+12.0f;
				arpList[5]=(float)arpInput[2]+12.0f;
				arpList[6]=(float)arpInput[1]+12.0f;
				arpList[7]=(float)arpInput[0]+12.0f;
				arpList[8]=(float)arpInput[2];
				arpList[9]=(float)arpInput[1];
			} else {
				arpLen=14;
				arpList[0]=(float)arpInput[0];
				arpList[1]=(float)arpInput[1];
				arpList[2]=(float)arpInput[2];
				arpList[3]=(float)arpInput[3];
				arpList[4]=(float)arpInput[0]+12.0f;
				arpList[5]=(float)arpInput[1]+12.0f;
				arpList[6]=(float)arpInput[2]+12.0f;
				arpList[7]=(float)arpInput[3]+12.0f;
				arpList[8]=(float)arpInput[2]+12.0f;
				arpList[9]=(float)arpInput[1]+12.0f;
				arpList[10]=(float)arpInput[0]+12.0f;
				arpList[11]=(float)arpInput[3];
				arpList[12]=(float)arpInput[2];
				arpList[13]=(float)arpInput[1];
			}												
			break;
		case 8: // 3oct Up/Down
			if ((float)arpInput[3] == 0) {
				arpLen=16;
				arpList[0]=(float)arpInput[0];
				arpList[1]=(float)arpInput[1];
				arpList[2]=(float)arpInput[2];
				arpList[3]=(float)arpInput[0]+12.0f;
				arpList[4]=(float)arpInput[1]+12.0f;
				arpList[5]=(float)arpInput[2]+12.0f;
				arpList[6]=(float)arpInput[0]+24.0f;
				arpList[7]=(float)arpInput[1]+24.0f;
				arpList[8]=(float)arpInput[2]+24.0f;
				arpList[9]=(float)arpInput[1]+24.0f;
				arpList[10]=(float)arpInput[0]+24.0f;
				arpList[11]=(float)arpInput[2]+12.0f;
				arpList[12]=(float)arpInput[1]+12.0f;
				arpList[13]=(float)arpInput[0]+12.0f;
				arpList[14]=(float)arpInput[2];
				arpList[15]=(float)arpInput[1];
			} else {
				arpLen=22;
				arpList[0]=(float)arpInput[0];
				arpList[1]=(float)arpInput[1];
				arpList[2]=(float)arpInput[2];
				arpList[3]=(float)arpInput[3];
				arpList[4]=(float)arpInput[0]+12.0f;
				arpList[5]=(float)arpInput[1]+12.0f;
				arpList[6]=(float)arpInput[2]+12.0f;
				arpList[7]=(float)arpInput[3]+12.0f;
				arpList[8]=(float)arpInput[0]+24.0f;
				arpList[9]=(float)arpInput[1]+24.0f;
				arpList[10]=(float)arpInput[2]+24.0f;
				arpList[11]=(float)arpInput[3]+24.0f;
				arpList[12]=(float)arpInput[2]+24.0f;
				arpList[13]=(float)arpInput[1]+24.0f;
				arpList[14]=(float)arpInput[0]+24.0f;
				arpList[15]=(float)arpInput[3]+12.0f;
				arpList[16]=(float)arpInput[2]+12.0f;
				arpList[17]=(float)arpInput[1]+12.0f;
				arpList[18]=(float)arpInput[0]+12.0f;
				arpList[19]=(float)arpInput[3];
				arpList[20]=(float)arpInput[2];
				arpList[21]=(float)arpInput[1];
			}												
			break;
		case 9: //1oct Down/Up
			if ((float)arpInput[3] == 0) {
				arpLen=4;
				arpList[0]=(float)arpInput[2];
				arpList[1]=(float)arpInput[1];
				arpList[2]=(float)arpInput[0];
				arpList[3]=(float)arpInput[1];
			} else {
				arpLen=6;
				arpList[0]=(float)arpInput[3];
				arpList[1]=(float)arpInput[2];
				arpList[2]=(float)arpInput[1];
				arpList[3]=(float)arpInput[0];
				arpList[4]=(float)arpInput[1];
				arpList[5]=(float)arpInput[2];
			}												
			break;
		case 10: //2oct Down/Up
			if ((float)arpInput[3] == 0) {
				arpLen=10;
				arpList[0]=(float)arpInput[2]+12.0f;
				arpList[1]=(float)arpInput[1]+12.0f;
				arpList[2]=(float)arpInput[0]+12.0f;
				arpList[3]=(float)arpInput[2];
				arpList[4]=(float)arpInput[1];
				arpList[5]=(float)arpInput[0];
				arpList[6]=(float)arpInput[1];
				arpList[7]=(float)arpInput[2];
				arpList[8]=(float)arpInput[0]+12.0f;
				arpList[9]=(float)arpInput[1]+12.0f;
			} else {
				arpLen=14;
				arpList[0]=(float)arpInput[3]+12.0f;
				arpList[1]=(float)arpInput[2]+12.0f;
				arpList[2]=(float)arpInput[1]+12.0f;
				arpList[3]=(float)arpInput[0]+12.0f;
				arpList[4]=(float)arpInput[3];
				arpList[5]=(float)arpInput[2];
				arpList[6]=(float)arpInput[1];
				arpList[7]=(float)arpInput[0];
				arpList[8]=(float)arpInput[1];
				arpList[9]=(float)arpInput[2];
				arpList[10]=(float)arpInput[3];
				arpList[11]=(float)arpInput[0]+12.0f;
				arpList[12]=(float)arpInput[1]+12.0f;
				arpList[13]=(float)arpInput[2]+12.0f;
			}												
			break;
		case 11: //3oct Down/Up
			if ((float)arpInput[3] == 0) {
				arpLen=16;
				arpList[0]=(float)arpInput[2]+24.0f;
				arpList[1]=(float)arpInput[1]+24.0f;
				arpList[2]=(float)arpInput[0]+24.0f;
				arpList[3]=(float)arpInput[2]+12.0f;
				arpList[4]=(float)arpInput[1]+12.0f;
				arpList[5]=(float)arpInput[0]+12.0f;
				arpList[6]=(float)arpInput[2];
				arpList[7]=(float)arpInput[1];
				arpList[8]=(float)arpInput[0];
				arpList[9]=(float)arpInput[1];
				arpList[10]=(float)arpInput[2];
				arpList[11]=(float)arpInput[0]+12.0f;
				arpList[12]=(float)arpInput[1]+12.0f;
				arpList[13]=(float)arpInput[2]+12.0f;
				arpList[14]=(float)arpInput[0]+24.0f;
				arpList[15]=(float)arpInput[1]+24.0f;
			} else {
				arpLen=22;
				arpList[0]=(float)arpInput[3]+24.0f;
				arpList[1]=(float)arpInput[2]+24.0f;
				arpList[2]=(float)arpInput[1]+24.0f;
				arpList[3]=(float)arpInput[0]+24.0f;
				arpList[4]=(float)arpInput[3]+12.0f;
				arpList[5]=(float)arpInput[2]+12.0f;
				arpList[6]=(float)arpInput[1]+12.0f;
				arpList[7]=(float)arpInput[0]+12.0f;
				arpList[8]=(float)arpInput[3];
				arpList[9]=(float)arpInput[2];
				arpList[10]=(float)arpInput[1];
				arpList[11]=(float)arpInput[0];
				arpList[12]=(float)arpInput[1];
				arpList[13]=(float)arpInput[2];
				arpList[14]=(float)arpInput[3];
				arpList[15]=(float)arpInput[0]+12.0f;
				arpList[16]=(float)arpInput[1]+12.0f;
				arpList[17]=(float)arpInput[2]+12.0f;
				arpList[18]=(float)arpInput[3]+12.0f;
				arpList[19]=(float)arpInput[0]+24.0f;
				arpList[20]=(float)arpInput[1]+24.0f;
				arpList[21]=(float)arpInput[2]+24.0f;
			}												
			break;
		}
		updateTuning(); // not very good here but still ok
	} else {
		switch (sp_cmd) {
			/* 0xC1 : Pitch Up */
			case 0xC1:
				shift=(float)sp_val*0.001f;
				bend+=shift;
			break;
			/* 0xC2 : Pitch Down */
			case 0xC2:
				shift=(float)sp_val*0.001f;
				bend-=shift;
			break;
			/* 0xC5 Intervall */
			case 0xC5:
				arpList[0]=0;
				if (sp_val > 127) arpList[1]=128-sp_val;
				else arpList[1]=sp_val;
				arpLen=2;
			break;
			/* 0xC6 Touchtaping */
			case 0xC6:
				if (sp_val > 127) arpList[0]=128-sp_val;
				else arpList[0]=sp_val;
				arpLen=1;
				if (curArp != arpList[0]){
					curArp=arpList[0];
					updateTuning();
				}
			break;
			/* 0xC7 Touchtaping with Retrig */
			case 0xC7:
				if (sp_val > 127) arpList[0]=128-sp_val;
				else arpList[0]=sp_val;
				arpLen=1;
				if (curArp != arpList[0]){
					curArp=arpList[0];
					updateTuning();
				}
			break;
			/* 0xCD Interval Bend Down */
			case 0xCD:	
				arpLen=2;
				shift=(float)sp_val*0.001f;
				arpList[1]-=shift;
			break;
			/* 0xCE Interval Bend Up */
			case 0xCE:
				arpLen=2;
				shift=(float)sp_val*0.001f;
				arpList[1]+=shift;
			break;
		}
	}

	if (!vpar->lfoDelay && vpar->syncvibe!=lfoViberLast){
		lfoViberLast=vpar->syncvibe;
		updateTuning();
	}

}

void CSynthTrack::InitEffect(int cmd, int val)
{
	sp_cmd=cmd;
	sp_val=val;

	// Init glide
	if (cmd == 0xC3 || cmd == 0xC4) {
		if (!val) NextGlide = LastGlide;
		else {
			NextGlide=(float)(val*val)*0.0000625f;
			LastGlide=NextGlide;
		}
	}

	//SemiGlide
	if ( ((cmd & 0xF0) == 0xD0) || ((cmd & 0xF0) == 0xE0)){
		sp_gl=(val*val)*0.0000625f;
		if ((cmd & 0xF0) == 0xD0) semitone=0.0f-(float)(cmd&15);
		else {
			if ((cmd & 0xF0) == 0xE0) semitone=(float)(cmd&15);
		else {semitone=0.0f;
				semiglide=DefGlide;
			}
		}
	}

	// Touchtaping
	if (cmd == 0xC6) { arpLen=1; arpCount=-1; }
	// Touchtaping with Retrig
	if (cmd == 0xC7) { Retrig(); arpLen=1; arpCount=-1; }
	if (cmd == 0xCC ||cmd == 0x0C) voiceVol=(float)val/255.0f+HUGEANTIDENORMAL;
}
}