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
#define FILTER_CALC_TIME				64
#define TWOPI																6.28318530717958647692528676655901f
namespace psycle::plugins::jme::blitz12 {
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

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
	,lfocount(0)
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

	arpInput[0]=0;
	m_filter.init(44100);
	firstGlide = true;
}

CSynthTrack::~CSynthTrack(){
}

void CSynthTrack::InitVoice(VOICEPAR *voicePar){
	vpar=voicePar;
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
}

void CSynthTrack::NoteTie(int note){
	note+=6;
	basenote=note+1.235f;
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
	nextVol=velocity+0.00000001f;
	ampEnvSustainLevel=(float)vpar->ampS*0.0039062f;
	if(ampEnvStage==0){
		RealNoteOn(); // THIS LINE DIFFERS FROM RETRIG()
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

void CSynthTrack::changeLfoDepth(int val){
	lfoViber.setLevel(vpar->lfoDepth);
}

void CSynthTrack::changeLfoSpeed(int val){
	lfoViber.setSpeed(vpar->lfoSpeed);
}

void CSynthTrack::RealNoteOn(){
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

	softenHighNotes=1.0f-((float)pow(2.0,note/12.0)*vpar->ampTrack*0.015625);
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
	basenote=note+7.235f;
	rsemitone=0.0f;
	semiglide=0.0f;
	bend=0;

	modEnvValue=0.0f;
	modEnvLast=0.0f;

	arpShuffle=0;
	arpCount=0;
	arpLen=1;
	arpIndex=-1;
	curArp=0;
	arpInput[1]=0;arpInput[2]=0;arpInput[3]=0;
	arpList[0]=0;arpList[1]=0;arpList[2]=0;
	arpList[3]=0;arpList[4]=0;arpList[5]=0;arpList[6]=0;arpList[7]=0;arpList[8]=0;arpList[9]=0;arpList[10]=0;arpList[11]=0;arpList[12]=0;
	arpList[13]=0;arpList[14]=0;arpList[15]=0;arpList[16]=0;arpList[17]=0;arpList[18]=0;arpList[19]=0;arpList[20]=0;arpList[21]=0;
	oscArpTranspose[0]=0;
	oscArpTranspose[1]=0;
	oscArpTranspose[2]=0;
	oscArpTranspose[3]=0;
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
	case 0:				//all osc
		if (vpar->oscOptions[0] != 8 && vpar->oscOptions[0] != 7 ) 
		dco1Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[0]+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		else
		dco1Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		if (vpar->oscOptions[1] != 8 && vpar->oscOptions[1] != 7) 
		dco2Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[1]+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		else
		dco2Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		if (vpar->oscOptions[2] != 8 && vpar->oscOptions[2] != 7) 
		dco3Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[2]+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		else
		dco3Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		if (vpar->oscOptions[3] != 8 && vpar->oscOptions[3] != 7) 
		dco4Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[3]+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		else
		dco4Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		break;
	case 1: // osc 2+3+4
		if (vpar->oscOptions[0] != 8 && vpar->oscOptions[0] != 7) 
		dco1Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[0]+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		else
		dco1Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		if (vpar->oscOptions[1] != 8 && vpar->oscOptions[1] != 7) 
		dco2Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[1]+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		else
		dco2Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		if (vpar->oscOptions[2] != 8 && vpar->oscOptions[2] != 7) 
		dco3Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[2]+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		else
		dco3Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		if (vpar->oscOptions[3] != 8 && vpar->oscOptions[3] != 7) 
		dco4Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[3]+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		else
		dco4Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		break;
	case 2: // osc 2+3
		if (vpar->oscOptions[0] != 8 && vpar->oscOptions[0] != 9)
		dco1Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[0]+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		else
		dco1Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		if (vpar->oscOptions[1] != 8 && vpar->oscOptions[1] != 7) 
		dco2Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[1]+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		else
		dco2Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		if (vpar->oscOptions[2] != 8 && vpar->oscOptions[2] != 7) 
		dco3Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[2]+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		else
		dco3Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		if (vpar->oscOptions[3] != 8 && vpar->oscOptions[3] != 7) 
		dco4Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[3]+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		else
		dco4Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		break;
	case 3: // osc 2+4
		if (vpar->oscOptions[0] != 8 && vpar->oscOptions[0] != 7) 
		dco1Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[0]+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		else
		dco1Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		if (vpar->oscOptions[1] != 8 && vpar->oscOptions[1] != 7) 
		dco2Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[1]+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		else
		dco2Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		if (vpar->oscOptions[2] != 8 && vpar->oscOptions[2] != 7) 
		dco3Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[2]+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		else
		dco3Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		if (vpar->oscOptions[3] != 8 && vpar->oscOptions[3] != 7) 
		dco4Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[3]+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		else
		dco4Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		break;
	case 4: // osc 3+4
		if (vpar->oscOptions[0] != 8 && vpar->oscOptions[0] != 7) 
		dco1Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[0]+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		else
		dco1Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		if (vpar->oscOptions[1] != 8 && vpar->oscOptions[1] != 7) 
		dco2Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[1]+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		else
		dco2Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		if (vpar->oscOptions[2] != 8 && vpar->oscOptions[2] != 7) 
		dco3Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[2]+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		else
		dco3Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		if (vpar->oscOptions[3] != 8 && vpar->oscOptions[3] != 7) 
		dco4Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[3]+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		else
		dco4Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		break;
	case 5: // osc 2
		if (vpar->oscOptions[0] != 8 && vpar->oscOptions[0] != 7) 
		dco1Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[0]+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		else
		dco1Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		if (vpar->oscOptions[1] != 8 && vpar->oscOptions[1] != 7) 
		dco2Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[1]+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		else
		dco2Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		if (vpar->oscOptions[2] != 8 && vpar->oscOptions[2] != 7) 
		dco3Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[2]+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		else
		dco3Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		if (vpar->oscOptions[3] != 8 && vpar->oscOptions[3] != 7) 
		dco4Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[3]+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		else
		dco4Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		break;
	case 6: // osc 3
		if (vpar->oscOptions[0] != 8 && vpar->oscOptions[0] != 7) 
		dco1Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[0]+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		else
		dco1Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		if (vpar->oscOptions[1] != 8 && vpar->oscOptions[1] != 7) 
		dco2Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[1]+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		else
		dco2Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		if (vpar->oscOptions[2] != 8 && vpar->oscOptions[2] != 7) 
		dco3Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[2]+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		else
		dco3Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		if (vpar->oscOptions[3] != 8 && vpar->oscOptions[3] != 7) 
		dco4Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[3]+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		else
		dco4Pitch=(float)pow(2.0,(rbasenote+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vpar->globalFine+vpar->oscFine[3])*0.0039062f)))/12.0);
		break;
	case 7: // osc 4
		if (vpar->oscOptions[0] != 8 && vpar->oscOptions[0] != 7) 
		dco1Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[0]+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		else
		dco1Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[0]+(((modEnv+vpar->globalFine+vpar->oscFine[0])*0.0039062f)))/12.0);
		if (vpar->oscOptions[1] != 8 && vpar->oscOptions[1] != 7) 
		dco2Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[1]+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		else
		dco2Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[1]+(((modEnv+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		if (vpar->oscOptions[2] != 8 && vpar->oscOptions[2] != 7) 
		dco3Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[2]+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		else
		dco3Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[2]+(((modEnv+vpar->globalFine+vpar->oscFine[2])*0.0039062f)))/12.0);
		if (vpar->oscOptions[3] != 8 && vpar->oscOptions[3] != 7) 
		dco4Pitch=(float)pow(2.0,(bend+rbasenote+rsemitone+oscArpTranspose[3]+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
		else
		dco4Pitch=(float)pow(2.0,(dbasenote+vpar->globalCoarse+vpar->oscCoarse[3]+(((modEnv+vibadd+vpar->globalFine+vpar->oscFine[1])*0.0039062f)))/12.0);
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

void CSynthTrack::GetSample(float* slr)
{
	float output=0.0f;
	float output1=0.0f;
	float output2=0.0f;
	float output3=0.0f;
	float output4=0.0f;
	//float outputRM1=0.0f;
	//float outputRM2=0.0f;

	updateCount--;
	if (updateCount < 1){
		updateCount = 128;
		masterVolume=(float)vpar->globalVolume*volMulti*0.005f;
		osc1Vol=vpar->oscVolume[0]*masterVolume;
		osc2Vol=vpar->oscVolume[1]*masterVolume;
		osc3Vol=vpar->oscVolume[2]*masterVolume;
		osc4Vol=vpar->oscVolume[3]*masterVolume;
		rm1Vol=vpar->rm1*masterVolume*0.0001f;
		rm2Vol=vpar->rm2*masterVolume*0.0001f;
		arpSpeed[0]=vpar->arpSpeed;
		if (vpar->arpShuffle) arpSpeed[1]=vpar->arpShuffle; else arpSpeed[1]=arpSpeed[0];
	}


	if(ampEnvStage)				{
		pwmcount--;
		if (pwmcount < 0){
			pwmcount = 500;
			calcWaves(15);
		}

		if (vpar->lfoDelay){
			lfocount--;
			if (lfocount < 0){
				lfocount = 200;
				lfoViber.next();
				lfoViberLast=lfoViberSample;
				lfoViberSample=(float)lfoViber.getPosition();
				if (lfoViberSample != lfoViberLast) tuningChange=true;
			}
		} else {
			if (lfoViberLast!=vpar->syncvibe){
				lfoViberLast=vpar->syncvibe;
				tuningChange=true;
			}
		}
		//Arpeggio
		fxcount--;
		if (fxcount < 0){
			fxcount = 50;
			GetEnvMod();
			if (modEnvValue != modEnvLast) tuningChange=true;

			arpCount--;
			if (arpCount < 0){
				arpCount = arpSpeed[arpShuffle];
				if ((arpLen>1)&(stopRetrig == false)&((vpar->arpRetrig == 1)||((vpar->arpRetrig == 2)&(arpShuffle == 0)))) Retrig();
				arpShuffle=1-arpShuffle;
				arpIndex++;
				if (arpIndex >= arpLen) arpIndex = 0;
				curArp=arpList[arpIndex];
				tuningChange=true;
			}
		}

		oldBuf[0]=curBuf[0]>>2;
		oldBuf[1]=curBuf[1]>>2;
		oldBuf[2]=curBuf[2]>>2;
		oldBuf[3]=curBuf[3]>>2;

		if (tuningChange) updateTuning();
		int c = 0;
		if ( vpar->oscVolume[0] || vpar->rm1 || vpar->oscOptions[1]==1 || vpar->oscOptions[1]==2 || vpar->oscOptions[1]==8 || (vpar->oscFuncType[0]>=44) & (vpar->oscFuncType[0]!=47) || (vpar->oscFuncType[0]>=46) & (vpar->oscFuncType[0]!=49)){
			if (vpar->oscFuncType[0]){
				for (c=0; c<4; c++){
					output1 += WaveBuffer[curBuf[0]][f2i(dco1Position+dco1Last+fmData4)];
					dco1Last=(float)(0.00000001f+output1)*(0.00000001f+vpar->oscFeedback[0])*(0.00000001f+fbCtl[0]);
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
							if ((vpar->oscOptions[2] == 1) || (vpar->oscOptions[2] == 2)|| (vpar->oscOptions[1] == 8)){
								dco3Position=dco1Position;
								if (nextBuf[2]){
									curBuf[2]^=4;
									nextBuf[2]=0;
								}
								if ((vpar->oscOptions[3] == 1) || (vpar->oscOptions[3] == 2)|| (vpar->oscOptions[1] == 8)){
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
			} else {
				for (c=0; c<4; c++){
					output1 += vpar->WaveTable[vpar->oscWaveform[0]][f2i(dco1Position+dco1Last+fmData4)];
					dco1Last=(float)(0.00000001f+output1)*(0.00000001f+vpar->oscFeedback[0])*(0.00000001f+fbCtl[0]);
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
							if ((vpar->oscOptions[2] == 1) || (vpar->oscOptions[2] == 2)|| (vpar->oscOptions[1] == 8)){
								dco3Position=dco1Position;
								if (nextBuf[2]){
									curBuf[2]^=4;
									nextBuf[2]=0;
								}
								if ((vpar->oscOptions[3] == 1) || (vpar->oscOptions[3] == 2)|| (vpar->oscOptions[1] == 8)){
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
			}
			output1 *= 0.25f;
		}

		if ( vpar->oscVolume[1] || vpar->rm1 || vpar->oscOptions[2]==1 || vpar->oscOptions[2]==2 || vpar->oscOptions[2]==8 || (vpar->oscFuncType[1]>=44) & (vpar->oscFuncType[1]!=47) || (vpar->oscFuncType[1]>=46) & (vpar->oscFuncType[1]!=49)){
			if (vpar->oscFuncType[1]){
				for (c=0; c<4; c++){
					output2 += WaveBuffer[1+curBuf[1]][f2i(dco2Position+dco2Last+fmData1)];
					dco2Last=(float)(0.00000001f+output2)*(0.00000001f+vpar->oscFeedback[1])*(0.00000001f+fbCtl[1]);
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
							if ((vpar->oscOptions[3] == 1) || (vpar->oscOptions[3] == 2)|| (vpar->oscOptions[2] == 8)){
								dco4Position=dco2Position;
								if (nextBuf[3]){
									curBuf[3]^=4;
									nextBuf[3]=0;
								}
								if ((vpar->oscOptions[0] == 1) || (vpar->oscOptions[0] == 2)|| (vpar->oscOptions[2] == 8)){
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
			} else {
				for (c=0; c<4; c++){
					output2 += vpar->WaveTable[vpar->oscWaveform[1]][f2i(dco2Position+dco2Last+fmData1)];
					dco2Last=(float)(0.00000001f+output2)*(0.00000001f+vpar->oscFeedback[1])*(0.00000001f+fbCtl[1]);
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
							if ((vpar->oscOptions[3] == 1) || (vpar->oscOptions[3] == 2)|| (vpar->oscOptions[2] == 8)){
								dco4Position=dco2Position;
								if (nextBuf[3]){
									curBuf[3]^=4;
									nextBuf[3]=0;
								}
								if ((vpar->oscOptions[0] == 1) || (vpar->oscOptions[0] == 2)|| (vpar->oscOptions[2] == 8)){
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
			}
			output2 *= 0.25f;
		}

		if ( vpar->oscVolume[2] || vpar->rm2 || vpar->oscOptions[3]==1 || vpar->oscOptions[3]==2 || vpar->oscOptions[3]==8 || (vpar->oscFuncType[2]>=44) & (vpar->oscFuncType[2]!=47)|| (vpar->oscFuncType[2]>=46) & (vpar->oscFuncType[2]!=49)){
			if (vpar->oscFuncType[2]){
				for (c=0; c<4; c++){
					output3 += WaveBuffer[2+curBuf[2]][f2i(dco3Position+dco3Last+fmData2)];
					dco3Last=(float)(0.00000001f+output3)*(0.00000001f+vpar->oscFeedback[2])*(0.00000001f+fbCtl[2]);
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
							if ((vpar->oscOptions[0] == 1) || (vpar->oscOptions[0] == 2)|| (vpar->oscOptions[3] == 8)){
								dco1Position=dco3Position;
								if (nextBuf[0]){
									curBuf[0]^=4;
									nextBuf[0]=0;
								}
								if ((vpar->oscOptions[1] == 1) || (vpar->oscOptions[1] == 2)|| (vpar->oscOptions[3] == 8)){
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
			} else {
				for (c=0; c<4; c++){
					output3 += vpar->WaveTable[vpar->oscWaveform[2]][f2i(dco3Position+dco3Last+fmData2)];				
					dco3Last=(float)(0.00000001f+output3)*(0.00000001f+vpar->oscFeedback[2])*(0.00000001f+fbCtl[2]);
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
							if ((vpar->oscOptions[0] == 1) || (vpar->oscOptions[0] == 2)|| (vpar->oscOptions[3] == 8)){
								dco1Position=dco3Position;
								if (nextBuf[0]){
									curBuf[0]^=4;
									nextBuf[0]=0;
								}
								if ((vpar->oscOptions[1] == 1) || (vpar->oscOptions[1] == 2)|| (vpar->oscOptions[3] == 8)){
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
			}
			output3 *= 0.25f;
		}

		if ( vpar->oscVolume[3] || vpar->rm2 || vpar->oscOptions[0]==1 || vpar->oscOptions[0]==2 || vpar->oscOptions[0]==8 || (vpar->oscFuncType[3]>=44) & (vpar->oscFuncType[3]!=47) || (vpar->oscFuncType[3]>=46) & (vpar->oscFuncType[3]!=49)){
			if (vpar->oscFuncType[3]){
				for (c=0; c<4; c++){
					output4 += WaveBuffer[3+curBuf[3]][f2i(dco4Position+dco4Last+fmData3)];
					dco4Last=(float)(0.00000001f+output4)*(0.00000001f+vpar->oscFeedback[3])*(0.00000001f+fbCtl[3]);
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
							if ((vpar->oscOptions[1] == 1) || (vpar->oscOptions[1] == 2)|| (vpar->oscOptions[0] == 8)){
								dco2Position=dco4Position;
								if (nextBuf[1]){
									curBuf[1]^=4;
									nextBuf[1]=0;
								}
								if ((vpar->oscOptions[2] == 1) || (vpar->oscOptions[2] == 2)|| (vpar->oscOptions[0] == 8)){
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
			} else {
				for (c=0; c<4; c++){
					output4 += vpar->WaveTable[vpar->oscWaveform[3]][f2i(dco4Position+dco4Last+fmData3)];
					dco4Last=(float)(0.00000001f+output4)*(0.00000001f+vpar->oscFeedback[3])*(0.00000001f+fbCtl[3]);
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
							if ((vpar->oscOptions[1] == 1) || (vpar->oscOptions[1] == 2)|| (vpar->oscOptions[0] == 8)){
								dco2Position=dco4Position;
								if (nextBuf[1]){
									curBuf[1]^=4;
									nextBuf[1]=0;
								}
								if ((vpar->oscOptions[2] == 1) || (vpar->oscOptions[2] == 2)|| (vpar->oscOptions[0] == 8)){
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
			}
			output4 *= 0.25f;
		}

		fmData1=(0.00000001f+fmCtl[oldBuf[0]][0])*(0.00000001f+output1)+(0.00000001f+fmCtl2[oldBuf[2]][2])*(0.00000001f+output3);
		fmData2=(0.00000001f+fmCtl[oldBuf[1]][1])*(0.00000001f+output2)+(0.00000001f+fmCtl2[oldBuf[3]][3])*(0.00000001f+output4);
		fmData3=(0.00000001f+fmCtl[oldBuf[2]][2])*(0.00000001f+output3)+(0.00000001f+fmCtl2[oldBuf[0]][0])*(0.00000001f+output1);
		fmData4=(0.00000001f+fmCtl[oldBuf[3]][3])*(0.00000001f+output4)+(0.00000001f+fmCtl2[oldBuf[1]][1])*(0.00000001f+output2);

		output=((0.00000001f+output1)*(0.00000001f+osc1Vol))+((0.00000001f+output2)*(0.00000001f+osc2Vol))+((0.00000001f+output3)*(0.00000001f+osc3Vol))+((0.00000001f+output4)*(0.00000001f+osc4Vol))+((0.00000001f+output1)*(0.00000001f+output2)*(0.00000001f+rm1Vol))+((0.00000001f+output3)*(0.00000001f+output4)*(0.00000001f+rm2Vol));				

		//master sat
		if ((vpar->oscFuncType[0]==47)||(vpar->oscFuncType[1]==47)||(vpar->oscFuncType[2]==47)||(vpar->oscFuncType[3]==47)){
			long long1 = (0.00000001f+output)*(0.00000001f+satClip);
			if (long1 > 16384) long1=16384;
			if (long1 < -16384) long1=-16384;
			output=long1;
		}

		//cutoff mod
		float cutmod = 0.0f;
		if (vpar->oscFuncType[0]==46) cutmod+=((output1+16384)*(0.00000001f+cmCtl[0]));
		if (vpar->oscFuncType[1]==46) cutmod+=((output2+16384)*(0.00000001f+cmCtl[1]));
		if (vpar->oscFuncType[2]==46) cutmod+=((output3+16384)*(0.00000001f+cmCtl[2]));
		if (vpar->oscFuncType[3]==46) cutmod+=((output4+16384)*(0.00000001f+cmCtl[3]));

		GetEnvFlt();
		if (vpar->fltType){
			if(!timetocompute--) {
				int realcutoff=int(vpar->fltCutoff+(vpar->filtvibe*0.005)+(rbasenote*0.1*vpar->fltTrack)+(fltEnvValue*vpar->fltEnvAmount*((1-vpar->fltVelocity)+(voiceVol*vpar->fltVelocity))));
				if(realcutoff<1)realcutoff=1;
				if(realcutoff>250)realcutoff=250;
				rcCut+=(realcutoff-50-rcCut)*rcCutCutoff;
				int cf = rcCut+cutmod;
				m_filter.setfilter(vpar->fltType, cf, vpar->fltResonance);
				timetocompute=FILTER_CALC_TIME;
			} output = m_filter.res(output);
		}

		rcVol+=(((GetEnvAmp()*((1-vpar->ampVelocity)+(voiceVol*vpar->ampVelocity))*softenHighNotes)-rcVol)*rcVolCutoff);
		output*=rcVol;
		slr[0]=output*vpar->stereoLR[currentStereoPos];
		slr[1]=output*vpar->stereoLR[1-currentStereoPos];
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

	float float1, float2, float3, float4, float5 = 0;
	float size1, size2, step1(0), step2, phase;
	int work1, work2 = 0;
	long long1 = 0;
	long long2 = 0;
	int buf = 0;
	for (int i=0; i<4; i++) {
		if (1<<i & mask){
			int pos = (synbase[i]+synfx[i].getPosition()+vpar->oscFuncSym[i])&2047;
			if (vpar->oscFuncType[i] != 43) fbCtl[i]=1.0f;
			if ((vpar->oscFuncType[i] < 44) || (vpar->oscFuncType[i] > 45)){
				fmCtl[0][i]=0.0f;
				fmCtl[1][i]=0.0f;
			}
			if ((vpar->oscFuncType[i] < 48) || (vpar->oscFuncType[i] > 49)){
				fmCtl2[0][i]=0.0f;
				fmCtl2[1][i]=0.0f;
			}
			if (pos != synposLast[i]) {
				synposLast[i]=pos;
				buf=4-curBuf[i]+i;
				switch (vpar->oscFuncType[i]) {
				case 0: break; // no function
				case 1: // stretch&squash
					size1 = float(pos+1); size2 = float(2048.0f-size1);
					if (size1!=0.0f) { step1 = 1024.0f/(size1); phase=0.0f;} else { phase=1024.0f; }
					if (size2!=0.0f) { step2 = 1024.0f/(size2); } else { step2 = 0.0f; }
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][int(phase)];
						if (phase < 1023.0f) phase+=step1; else phase+=step2;
					}
					break;
				case 2: // stretch&squash 2
					size1 = float(pos+1); size2 = float(2048.0f-size1);
					if (size1!=0.0f) { step1 = 1024.0f/(size1); phase = 0.0f; } else { phase=1024.0f; }
					if (size2!=0.0f) { step2 = 1024.0f/(size2); } else { step2 = 0.0f; }
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][int(phase+phase)];
						if (phase < 1023.0f) phase+=step1; else phase+=step2;
					}
					break;
				case 3: // pw am
					for(c=0;c<2048;c++){
						if (c < pos) WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c];
						else WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c]>>1;
					}
					break;
				case 4: // squash&squash
					float1 = 0;
					float2 = 0;
					float4 = (2047-pos)/2.0f;   // 1st halve size in byte
					float5 = 1024-float4;								// 2nd halve size in byte
					if (float4!=0) float1 = 1024/float4; // 1st halve step
					else float1 = 1024;
					if (float5!=0) float2 = 1024/float5; // 2nd halve step
					else float2 = 0;
					float3 = 0;																				//source phase
					for(c=0;c<2100;c++){
						if (c<1024) {
							if (float3<1024) {
								if (float4) {
									WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][(int)float3];
									float3+=float1;
								} else {
									WaveBuffer[buf][c]=0;
									float3=1024;
								}
							} else {
								if (float5) {
									WaveBuffer[buf][c]=0;
									float3=1024;
								}
							}
						} else {
							if (float3<2048) {
								if (float4 != 0) {
								WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][(int)float3];
								float3+=float1;
								} else WaveBuffer[buf][c]=0;
							} else {
								if (float5 != 0) WaveBuffer[buf][c]=0;
							}
						}
					}
					break;
				case 5: // Muted Sync
					float1 = 0;
					float2 = float(pos*6)/2047+1;
					for(c=0;c<2048;c++){
						if (float1<2047){
							WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][(int)float1];
							float1+=float2;
						if (float1 > 2047) float1=2048;
						} else WaveBuffer[buf][c]=0;
					}
					break;
				case 6: // Syncfake
					float1 = 0;
					float2 = float(pos*6)/2047+1;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][(int)float1];
						float1+=float2;
						if (float1 > 2047) float1-=2048;
					}
					break;
				case 7: // Restart
					work1 = 0;
					work2 = 2047-pos;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][work1];
						work1++; if (work1 > work2) work1 = 0;
					}
					break;
				case 8: // Negator
					for(c=0;c<2048;c++){
						if (c<pos)WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c];
						else WaveBuffer[buf][c]=0-vpar->WaveTable[vpar->oscWaveform[i]][c];
					}
					break;
				case 9: // Double Negator
					if (pos > 1023) pos = 2047-pos;
					for(c=0;c<1024;c++){
						if (c<pos){
							WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c];
							WaveBuffer[buf][2047-c]=vpar->WaveTable[vpar->oscWaveform[i]][2047-c];
						} else {
							WaveBuffer[buf][c]=0-vpar->WaveTable[vpar->oscWaveform[i]][c];
							WaveBuffer[buf][2047-c]=0-vpar->WaveTable[vpar->oscWaveform[i]][2047-c];
						}
					}
					break;
				case 10: // Rect Negator
					for(c=0;c<2048;c++){
					if (((pos+c)&2047)<1024)WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c];
						else WaveBuffer[buf][c]=0-vpar->WaveTable[vpar->oscWaveform[i]][c];
					}
					break;
				case 11: // Octaving
					float1 = (2047-float(pos))/2047;
					float2 = float(pos)/2047;
					for (c=0;c<2048;c++){
						WaveBuffer[buf][c]=(signed short)((vpar->WaveTable[vpar->oscWaveform[i]][c]*float1)+(vpar->WaveTable[vpar->oscWaveform[i]][(c+c)&2047]*float2));
					}
					break;
				case 12: // FSK
					work1=0;
					work2=2047-pos;
					for (c=0;c<2048;c++){
						if (c<work2){
							WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c];
							work1=c+1;
						}
						else {
							WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][work1];
							work1+=2; if (work1>2047) work1 -= 2048;
						}
					}
					break;
				case 13: // Mixer
					for (c=0;c<2048;c++){
						WaveBuffer[buf][c]=(vpar->WaveTable[vpar->oscWaveform[i]][c]>>1)+(vpar->WaveTable[vpar->oscWaveform[i]][(c+pos)&2047]>>1);
					}
					break;
				case 14: // Dual Mixer
					for (c=0;c<2048;c++){
						WaveBuffer[buf][c]=(signed short)((vpar->WaveTable[vpar->oscWaveform[i]][c]*0.3333333)+(vpar->WaveTable[vpar->oscWaveform[i]][(c+pos)&2047]*0.3333333)+(vpar->WaveTable[vpar->oscWaveform[i]][(c-pos)&2047]*0.3333333));
					}
					break;
				case 15: // Fbk.Mixer
					for (c=0;c<2048;c++){
						WaveBuffer[buf][c]=(WaveBuffer[buf][c]>>1)+(vpar->WaveTable[vpar->oscWaveform[i]][(c+pos)&2047]>>1);
					}
					break;
				case 16: // Inv.Mixer
					for (c=0;c<2048;c++){
						WaveBuffer[buf][c]=(vpar->WaveTable[vpar->oscWaveform[i]][c]>>1)-(vpar->WaveTable[vpar->oscWaveform[i]][(c+pos)&2047]>>1);
					}
					break;
				case 17: // TriMix
					float1 = (2047-float(pos))/2047;
					float2 = float(pos)/2047;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=(signed short)((vpar->WaveTable[vpar->oscWaveform[i]][c]*float1)+(vpar->WaveTable[WAVE_TRIANGLE][c]*float2));
					}
					break;
				case 18: // SawMix
					float1 = (2047-float(pos))/2047;
					float2 = float(pos)/2047;
					work1 = 0;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=(signed short)((vpar->WaveTable[vpar->oscWaveform[i]][c]*float1)+((work1-16384)*float2));
						work1+=16;
					}
					break;
				case 19: // SqrMix
					float1 = (2047-float(pos))/2047;
					float2 = float(pos)/2047;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=(signed short)((vpar->WaveTable[vpar->oscWaveform[i]][c]*float1)+(vpar->WaveTable[WAVE_SQUARE][c]*float2));
					}
					break;
				case 20: // Tremelo
					float1 = (2047-float(pos))/2047;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=(signed short)((float)vpar->WaveTable[vpar->oscWaveform[i]][c]*float1);
					}
					break;
				case 21: // PM Sine 1
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_SINE][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][c]*float2))];
					}
					break;
				case 22: // PM Sine 2
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_SINE][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][(c+c)&2047]*float2))];
					}
					break;
				case 23: // PM Sine 3
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_SINE][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][(c+c+c)&2047]*float2))];
					}
					break;
				case 24: // PM Adlib2 1
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB2][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][c]*float2))];
					}
					break;
				case 25: // PM Adlib2 2
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB2][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][(c+c)&2047]*float2))];
					}
					break;
				case 26: // PM Adlib2 3
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB2][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][(c+c+c)&2047]*float2))];
					}
					break;
				case 27: // PM Adlib3 1
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB3][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][c]*float2))];
					}
					break;
				case 28: // PM Adlib3 2
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB3][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][(c+c)&2047]*float2))];
					}
					break;
				case 29: // PM Adlib3 3
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB3][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][(c+c+c)&2047]*float2))];
					}
					break;
				case 30: // PM Adlib4 1
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB4][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][c]*float2))];
					}
					break;
				case 31: // PM Adlib4 2
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB4][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][(c+c)&2047]*float2))];
					}
					break;
				case 32: // PM Adlib4 3
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[WAVE_ADLIB4][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][(c+c+c)&2047]*float2))];
					}
					break;												
				case 33: // PM Wave 1
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][c]*float2))];
					}
					break;
				case 34: // PM Wave 2
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][(c+c)&2047]*float2))];
					}
					break;
				case 35: // PM Wave 3
					float2 = float(pos)*0.00025f;
					for(c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][(c+c+c)&2047]*float2))];
					}
					break;												
				case 36: // Dual Fix PM
					float2 = 0.125f; //16384:2048=1/8=0.125f
					for (c=0;c<2048;c++) WaveBuffer[8][c]=vpar->WaveTable[vpar->oscWaveform[i]][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][(c+pos)&2047]*float2))];
					for (c=0;c<2048;c++) WaveBuffer[buf][c]=WaveBuffer[8][2047&(int)(c+(vpar->WaveTable[vpar->oscWaveform[i]][(c-pos)&2047]*float2))];
					break;
				case 37: // Multiply
					for (c=0;c<2048;c++) WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c]*vpar->WaveTable[vpar->oscWaveform[i]][(c+pos)&2047]*0.0001f;;
					break;
				case 38: // AND
					for (c=0;c<2048;c++) WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c]&vpar->WaveTable[vpar->oscWaveform[i]][(c+pos)&2047];
					break;
				case 39: // EOR
					for (c=0;c<2048;c++) WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c]^vpar->WaveTable[vpar->oscWaveform[i]][(c+pos)&2047];
					break;
				case 40: // Boost (Hard Clip)
					float1 = pos*pos*0.000000015*pos+1.0f;
					for (c=0;c<2048;c++){
						long1=vpar->WaveTable[vpar->oscWaveform[i]][c]*float1;
						if (long1 > 16384) long1=16384;
						if (long1 < -16384) long1=-16384;
						WaveBuffer[buf][c]=long1;
					}
					break;
				case 41: // RM to AM (Upright)
					float1 = (2047-float(pos))/2047*16384;
					float2 = float(pos)/2047*0.5f;
					for (c=0;c<2048;c++) WaveBuffer[buf][c]=(vpar->WaveTable[vpar->oscWaveform[i]][c]+16384)*float2+float1;
					break;
				case 42: // RM to AM (Down)
					float1 = (2047-float(pos))/2047*16384;
					float2 = float(pos)/2047*0.5f;
					for (c=0;c<2048;c++) WaveBuffer[buf][c]=float1-((vpar->WaveTable[vpar->oscWaveform[i]][c]-16384)*float2);
					break;
				case 43: // Feedback Ctrl
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c];
					fbCtl[i]=(pos-1024)*0.0009765625;
					break;
				case 44: // FM next +
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c];
					fmCtl[buf>>2][i]=pos*0.00048828125;
					break;
				case 45: // FM next -
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c];
					fmCtl[buf>>2][i]=0-(pos*0.00048828125);
					break;
				case 46: // Filter Mod
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c];
					cmCtl[i]=pos*0.0000048828125;
					break;
				case 47: // Chan Sat
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c];
					satClip = pos*pos*0.000000015*pos+1.0f;
					break;
				case 48: // FM last +
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c];
					fmCtl2[buf>>2][i]=pos*0.00048828125;
					break;
				case 49: // FM last -
					for(c=0;c<2048;c++)				WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][c];
					fmCtl2[buf>>2][i]=0-(pos*0.00048828125);
					break;
				case 50: // X Rotator
					for (c=0;c<2048;c++){
						WaveBuffer[buf][c]=vpar->WaveTable[vpar->oscWaveform[i]][(c+pos)&2047];
					}
					break;
				case 51: // Y Rotator
					long2 = pos/2046.0f*32768.0f;
					for (c=0;c<2048;c++){
						long1=vpar->WaveTable[vpar->oscWaveform[i]][c]+long2;
						if (long1 > 16384) long1-=32768;
						if (long1 < -16384) long1+=32768;
						WaveBuffer[buf][c]=long1;
					}
					break;
				case 52: // Boost II (Wrap)
					float1 = pos/2047.0f;
					float1 += float1+1.0f;
					for (c=0;c<2048;c++){
						long1=vpar->WaveTable[vpar->oscWaveform[i]][c]*float1;
						while (long1 > 16384) long1-=32768;
						while (long1 < -16384) long1+=32768;
						WaveBuffer[buf][c]=long1;
					}
					break;

				}
				nextBuf[i]=1; // a new buffer is now present
			}
		}
	}
}

void CSynthTrack::GetEnvMod(){
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

float CSynthTrack::GetEnvAmp(){
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
		ampEnvValue-=ampEnvCoef;

		if(ampEnvValue<ampEnvSustainLevel)
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
		ampEnvValue-=ampEnvCoef;
		
		if(ampEnvValue<=0){
			ampEnvValue=0;
			ampEnvStage=0;
		}
		return ampEnvValue;
	break;

	case 4: // Release
		ampEnvValue-=ampEnvCoef;

		if(ampEnvValue<0.0f){
			ampEnvValue=0.0f;
			ampEnvStage=0;
		}
		return ampEnvValue;
	break;
	
	case 5: // FastRelease
		ampEnvValue-=ampEnvCoef;

		if(ampEnvValue<0.0f)
		{
			RealNoteOn();
			ampEnvValue=0.0f;
			ampEnvStage=1;
			ampEnvCoef=(1.0f/(float)vpar->ampA)*speedup;
			if (ampEnvCoef>minFade) ampEnvCoef=minFade;
		}

		return ampEnvValue;
	break;

	case 6: // FastRelease for Arpeggio
		ampEnvValue-=ampEnvCoef;

		if(ampEnvValue<0.0f)
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

void CSynthTrack::GetEnvFlt()
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

void CSynthTrack::NoteOff()
{
	stopRetrig=true;
	if((ampEnvStage!=4)&(ampEnvStage!=0)){
		ampEnvStage=4;
		ampEnvCoef=(ampEnvValue/(float)vpar->ampR)*speedup;
		if (ampEnvCoef>minFade) ampEnvCoef=minFade;
	}
	if((fltEnvStage!=4)&(fltEnvStage!=0)){
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

	// Perform tone glide
	DoGlide();

	if ( ((sp_cmd & 0xF0) == 0xD0) || ((sp_cmd & 0xF0) == 0xE0)) semiglide=sp_gl;

	if ( ((sp_cmd & 0xF0) < 0xC0) & (sp_cmd != 0x0C)) {
		if (sp_cmd != 0) {
			arpInput[1] = sp_cmd>>4;
			arpInput[2] = sp_cmd&0x0f;
			arpInput[3] = sp_val;
			switch (vpar->arpPattern) {
			case 0: // 1oct Up
				arpList[0]=arpInput[0];
				arpList[1]=arpInput[1];
				arpList[2]=arpInput[2];
				if (arpInput[3] == 0) {
					arpLen=3;
				} else {
					arpLen=4;
					arpList[3]=arpInput[3];
				}
				break;
			case 1: // 2oct Up
				arpList[0]=arpInput[0];
				arpList[1]=arpInput[1];
				arpList[2]=arpInput[2];
				if (arpInput[3] == 0) {
					arpLen=6;
					arpList[3]=arpInput[0]+12;
					arpList[4]=arpInput[1]+12;
					arpList[5]=arpInput[2]+12;
				} else {
					arpLen=8;
					arpList[3]=arpInput[3];
					arpList[4]=arpInput[0]+12;
					arpList[5]=arpInput[1]+12;
					arpList[6]=arpInput[2]+12;
					arpList[7]=arpInput[3]+12;
				}
				break;
			case 2: // 3oct Up
				arpList[0]=arpInput[0];
				arpList[1]=arpInput[1];
				arpList[2]=arpInput[2];
				if (arpInput[3] == 0) {
					arpLen=9;
					arpList[3]=arpInput[0]+12;
					arpList[4]=arpInput[1]+12;
					arpList[5]=arpInput[2]+12;
					arpList[6]=arpInput[0]+24;
					arpList[7]=arpInput[1]+24;
					arpList[8]=arpInput[2]+24;
				} else {
					arpLen=12;
					arpList[3]=arpInput[3];
					arpList[4]=arpInput[0]+12;
					arpList[5]=arpInput[1]+12;
					arpList[6]=arpInput[2]+12;
					arpList[7]=arpInput[3]+12;
					arpList[8]=arpInput[0]+24;
					arpList[9]=arpInput[1]+24;
					arpList[10]=arpInput[2]+24;
					arpList[11]=arpInput[3]+24;
				}
				break;
			case 3: // 1oct Down
				if (arpInput[3] == 0) {
					arpLen=3;
					arpList[0]=arpInput[2];
					arpList[1]=arpInput[1];
					arpList[2]=arpInput[0];
				} else {
					arpLen=4;
					arpList[0]=arpInput[3];
					arpList[1]=arpInput[2];
					arpList[2]=arpInput[1];
					arpList[3]=arpInput[0];
				}												
				break;
			case 4: // 2oct Down
				if (arpInput[3] == 0) {
					arpLen=6;
					arpList[0]=arpInput[2]+12;
					arpList[1]=arpInput[1]+12;
					arpList[2]=arpInput[0]+12;
					arpList[3]=arpInput[2];
					arpList[4]=arpInput[1];
					arpList[5]=arpInput[0];
				} else {
					arpLen=8;
					arpList[0]=arpInput[3]+12;
					arpList[1]=arpInput[2]+12;
					arpList[2]=arpInput[1]+12;
					arpList[3]=arpInput[0]+12;
					arpList[4]=arpInput[3];
					arpList[5]=arpInput[2];
					arpList[6]=arpInput[1];
					arpList[7]=arpInput[0];
				}												
				break;
			case 5: // 3oct Down
				if (arpInput[3] == 0) {
					arpLen=9;
					arpList[0]=arpInput[2]+24;
					arpList[1]=arpInput[1]+24;
					arpList[2]=arpInput[0]+24;
					arpList[3]=arpInput[2]+12;
					arpList[4]=arpInput[1]+12;
					arpList[5]=arpInput[0]+12;
					arpList[6]=arpInput[2];
					arpList[7]=arpInput[1];
					arpList[8]=arpInput[0];
				} else {
					arpLen=12;
					arpList[0]=arpInput[3]+24;
					arpList[1]=arpInput[2]+24;
					arpList[2]=arpInput[1]+24;
					arpList[3]=arpInput[0]+24;
					arpList[4]=arpInput[3]+12;
					arpList[5]=arpInput[2]+12;
					arpList[6]=arpInput[1]+12;
					arpList[7]=arpInput[0]+12;
					arpList[8]=arpInput[3];
					arpList[9]=arpInput[2];
					arpList[10]=arpInput[1];
					arpList[11]=arpInput[0];
				}												
				break;
			case 6: // 1oct Up/Down
				if (arpInput[3] == 0) {
					arpLen=4;
					arpList[0]=arpInput[0];
					arpList[1]=arpInput[1];
					arpList[2]=arpInput[2];
					arpList[3]=arpInput[1];
				} else {
					arpLen=6;
					arpList[0]=arpInput[0];
					arpList[1]=arpInput[1];
					arpList[2]=arpInput[2];
					arpList[3]=arpInput[3];
					arpList[4]=arpInput[2];
					arpList[5]=arpInput[1];
				}												
				break;
			case 7: // 2oct Up/Down
				if (arpInput[3] == 0) {
					arpLen=10;
					arpList[0]=arpInput[0];
					arpList[1]=arpInput[1];
					arpList[2]=arpInput[2];
					arpList[3]=arpInput[0]+12;
					arpList[4]=arpInput[1]+12;
					arpList[5]=arpInput[2]+12;
					arpList[6]=arpInput[1]+12;
					arpList[7]=arpInput[0]+12;
					arpList[8]=arpInput[2];
					arpList[9]=arpInput[1];
				} else {
					arpLen=14;
					arpList[0]=arpInput[0];
					arpList[1]=arpInput[1];
					arpList[2]=arpInput[2];
					arpList[3]=arpInput[3];
					arpList[4]=arpInput[0]+12;
					arpList[5]=arpInput[1]+12;
					arpList[6]=arpInput[2]+12;
					arpList[7]=arpInput[3]+12;
					arpList[8]=arpInput[2]+12;
					arpList[9]=arpInput[1]+12;
					arpList[10]=arpInput[0]+12;
					arpList[11]=arpInput[3];
					arpList[12]=arpInput[2];
					arpList[13]=arpInput[1];
				}												
				break;
			case 8: // 3oct Up/Down
				if (arpInput[3] == 0) {
					arpLen=16;
					arpList[0]=arpInput[0];
					arpList[1]=arpInput[1];
					arpList[2]=arpInput[2];
					arpList[3]=arpInput[0]+12;
					arpList[4]=arpInput[1]+12;
					arpList[5]=arpInput[2]+12;
					arpList[6]=arpInput[0]+24;
					arpList[7]=arpInput[1]+24;
					arpList[8]=arpInput[2]+24;
					arpList[9]=arpInput[1]+24;
					arpList[10]=arpInput[0]+24;
					arpList[11]=arpInput[2]+12;
					arpList[12]=arpInput[1]+12;
					arpList[13]=arpInput[0]+12;
					arpList[14]=arpInput[2];
					arpList[15]=arpInput[1];
				} else {
					arpLen=22;
					arpList[0]=arpInput[0];
					arpList[1]=arpInput[1];
					arpList[2]=arpInput[2];
					arpList[3]=arpInput[3];
					arpList[4]=arpInput[0]+12;
					arpList[5]=arpInput[1]+12;
					arpList[6]=arpInput[2]+12;
					arpList[7]=arpInput[3]+12;
					arpList[8]=arpInput[0]+24;
					arpList[9]=arpInput[1]+24;
					arpList[10]=arpInput[2]+24;
					arpList[11]=arpInput[3]+24;
					arpList[12]=arpInput[2]+24;
					arpList[13]=arpInput[1]+24;
					arpList[14]=arpInput[0]+24;
					arpList[15]=arpInput[3]+12;
					arpList[16]=arpInput[2]+12;
					arpList[17]=arpInput[1]+12;
					arpList[18]=arpInput[0]+12;
					arpList[19]=arpInput[3];
					arpList[20]=arpInput[2];
					arpList[21]=arpInput[1];
				}												
				break;
			case 9: //1oct Down/Up
				if (arpInput[3] == 0) {
					arpLen=4;
					arpList[0]=arpInput[2];
					arpList[1]=arpInput[1];
					arpList[2]=arpInput[0];
					arpList[3]=arpInput[1];
				} else {
					arpLen=6;
					arpList[0]=arpInput[3];
					arpList[1]=arpInput[2];
					arpList[2]=arpInput[1];
					arpList[3]=arpInput[0];
					arpList[4]=arpInput[1];
					arpList[5]=arpInput[2];
				}												
				break;
			case 10: //2oct Down/Up
				if (arpInput[3] == 0) {
					arpLen=10;
					arpList[0]=arpInput[2]+12;
					arpList[1]=arpInput[1]+12;
					arpList[2]=arpInput[0]+12;
					arpList[3]=arpInput[2];
					arpList[4]=arpInput[1];
					arpList[5]=arpInput[0];
					arpList[6]=arpInput[1];
					arpList[7]=arpInput[2];
					arpList[8]=arpInput[0]+12;
					arpList[9]=arpInput[1]+12;
				} else {
					arpLen=14;
					arpList[0]=arpInput[3]+12;
					arpList[1]=arpInput[2]+12;
					arpList[2]=arpInput[1]+12;
					arpList[3]=arpInput[0]+12;
					arpList[4]=arpInput[3];
					arpList[5]=arpInput[2];
					arpList[6]=arpInput[1];
					arpList[7]=arpInput[0];
					arpList[8]=arpInput[1];
					arpList[9]=arpInput[2];
					arpList[10]=arpInput[3];
					arpList[11]=arpInput[0]+12;
					arpList[12]=arpInput[1]+12;
					arpList[13]=arpInput[2]+12;
				}												
				break;
			case 11: //3oct Down/Up
				if (arpInput[3] == 0) {
					arpLen=16;
					arpList[0]=arpInput[2]+24;
					arpList[1]=arpInput[1]+24;
					arpList[2]=arpInput[0]+24;
					arpList[3]=arpInput[2]+12;
					arpList[4]=arpInput[1]+12;
					arpList[5]=arpInput[0]+12;
					arpList[6]=arpInput[2];
					arpList[7]=arpInput[1];
					arpList[8]=arpInput[0];
					arpList[9]=arpInput[1];
					arpList[10]=arpInput[2];
					arpList[11]=arpInput[0]+12;
					arpList[12]=arpInput[1]+12;
					arpList[13]=arpInput[2]+12;
					arpList[14]=arpInput[0]+24;
					arpList[15]=arpInput[1]+24;
				} else {
					arpLen=6;
					arpList[0]=arpInput[3]+24;
					arpList[1]=arpInput[2]+24;
					arpList[2]=arpInput[1]+24;
					arpList[3]=arpInput[0]+24;
					arpList[4]=arpInput[3]+12;
					arpList[5]=arpInput[2]+12;
					arpList[6]=arpInput[1]+12;
					arpList[7]=arpInput[0]+12;
					arpList[8]=arpInput[3];
					arpList[9]=arpInput[2];
					arpList[10]=arpInput[1];
					arpList[11]=arpInput[0];
					arpList[12]=arpInput[1];
					arpList[13]=arpInput[2];
					arpList[14]=arpInput[3];
					arpList[15]=arpInput[0]+12;
					arpList[16]=arpInput[1]+12;
					arpList[17]=arpInput[2]+12;
					arpList[18]=arpInput[3]+12;
					arpList[19]=arpInput[0]+24;
					arpList[20]=arpInput[1]+25;
					arpList[21]=arpInput[2]+24;
				}												
				break;
			}
			updateTuning(); // not very good here but still ok
		}
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
		}
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
	if (cmd == 0xCC ||cmd == 0x0C) voiceVol=(float)val/255.0f+0.00000001f;
}
}