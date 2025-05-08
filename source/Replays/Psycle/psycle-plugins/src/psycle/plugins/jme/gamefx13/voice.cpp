/*
	GameFX (C)2005 by jme
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
#include <psycle/helpers/math.hpp>
#include <universalis/stdlib/cstdint.hpp>
namespace psycle::plugins::jme::gamefx13 {
using namespace psycle::helpers;
using namespace universalis::stdlib;

#define FILTER_CALC_TIME 64
#define TWOPI 6.28318530717958647692528676655901f

CSynthTrack::CSynthTrack()
{
	replaycount=1;
	perf_count=1;
	perf_index=1;
	timetocompute=FILTER_CALC_TIME;

	cur_basenote=0;
	cur_realnote=0;
	cur_volume=0;
	voicevol=1.0f;
	nextvoicevol=1.0f;
	volmulti=0.0f;
	cur_waveform=0;
	cur_transpose=0;
	add_to_pitch=0.0f;
	cur_option=0;
	cur_command=0;
	cur_parameter=0;
	cur_speed=16;
	cur_pw=512;
	trigger=false;
	keyrelease=true;
	stopsend=false;
	fastRelease=128.0f;
	rcVolCutoff=0.25f;
	rcVol=0.0f;

	minFade=0.008f;

	OSCVol=0;
	oscglide=0;

	sp_cmd=0;
	sp_val=0;

	OSCSpeed=0.0f;
	ROSCSpeed=0.0f;
	OOSCSpeed=0.0f;
	OSCPosition=0.0f;

	AmpEnvStage=0;
	AmpEnvValue=0.0f;
	VcfEnvStage=0;
	VcfEnvValue=0.0f;

	fltMode=1;
	m_filter.init(44100);
}

CSynthTrack::~CSynthTrack()
{

}

void CSynthTrack::NoteOn(int note, PERFORMANCE *perf, int spd)
{
	vpar=perf;
	fltMode=1;
	nextNote=note;
	nextSpd=spd;
	trigger=true;
	keyrelease=true;
	nextvoicevol=1.0f;
	Retrig();
}

void CSynthTrack::RealNoteOn()
{
	if (trigger){
		trigger=false;
		speed=nextSpd;
		if(nextSpd<65) spdcoef=(float)nextSpd*0.015625f; else spdcoef=1.0f;
		volmulti=0.0039062f*spdcoef;
		cur_basenote=nextNote;
		replaycount = 1;
		perf_count = 1;
		perf_index = vpar->StartPos;
		keyrelease=false;
		stopsend=false;
	}
}

void CSynthTrack::Retrig()
{
	// Init Amplitude Envelope
	AmpEnvSustainLevel=(float)vpar->AEGSustain*0.0039062f;
	VcfEnvSustainLevel=(float)vpar->FEGSustain*0.0039062f;
	if(AmpEnvStage==0){
		RealNoteOn();
		AmpEnvStage=1;
		AmpEnvCoef=1.0f/(float)vpar->AEGAttack;
		if (AmpEnvCoef>minFade) AmpEnvCoef=minFade;
	}else{
		AmpEnvStage=5;
		AmpEnvCoef=AmpEnvValue/fastRelease;
		if(AmpEnvCoef<=0.0f)AmpEnvCoef=0.03125f;
	}
	// Init Filter Envelope
	if(VcfEnvStage==0){
		VcfEnvStage=1;
		VcfEnvCoef=1.0f/(float)vpar->FEGAttack;
		if (VcfEnvCoef>minFade) VcfEnvCoef=minFade;
	}else{
		VcfEnvStage=5;
		VcfEnvCoef=VcfEnvValue/fastRelease;
		if(VcfEnvCoef<=0.0f)VcfEnvCoef=0.03125f;
	}
}

float CSynthTrack::GetSample()
{
	VcfEnvMod=(float)vpar->EnvMod;
	VcfCutoff=(float)vpar->Cutoff;
	VcfResonance=(float)vpar->Resonance/256.0f;
	float output=0;
	if (trigger==false)				voicevol=nextvoicevol;
	replaycount--;
	if (replaycount <= 0){
		replaycount=vpar->ReplaySpeed;
		perf_count--;
		if (perf_count <= 0){
			perf_count = cur_speed;
			if (vpar->Volume[perf_index]){
				cur_volume=vpar->Volume[perf_index];
				OSCVol=(float)cur_volume*volmulti;
			}
			if (vpar->Waveform[perf_index]) cur_waveform=vpar->Waveform[perf_index]-1;
			if (vpar->Transpose[perf_index]) {
				add_to_pitch=0.0f;
				cur_transpose=vpar->Transpose[perf_index]-1;
				cur_option=vpar->Option[perf_index];
				cur_realnote = cur_transpose;
				if ((cur_option & 1) == 0) cur_realnote+=cur_basenote;
				if ((cur_option & 2) && !keyrelease) Retrig();
			}
			cur_command=vpar->Command[perf_index];
			cur_parameter=vpar->Parameter[perf_index];
			switch(cur_command){
				case 3: cur_realnote=(cur_realnote+cur_parameter)&127; if (cur_realnote>95) cur_realnote-=32; break;
				case 4: cur_realnote=(cur_realnote-cur_parameter)&127; if (cur_realnote>95) cur_realnote-=32; break;
				case 5: cur_pw=cur_parameter<<2; break;
				case 6: cur_pw=(cur_pw+(cur_parameter<<2))&2047; break;
				case 7: cur_pw=(cur_pw-(cur_parameter<<2))&2047; break;
				case 8: OSCPosition=cur_parameter<<4; break;
				case 9: fltMode=0; break;
				case 10: fltMode=1; break;
				case 11: fltMode=2; break;
				case 15: if(stopsend==false) { stopsend=true; NoteOff(); } break;
			}
			if (vpar->Speed[perf_index]) {
				cur_speed=vpar->Speed[perf_index];
				perf_count=cur_speed;
			}
			perf_index++;
			if ((perf_index == vpar->LoopEnd+1) || (perf_index > 15)) perf_index = vpar->LoopStart;
		}
		switch(cur_command){
			case 1: add_to_pitch+=cur_parameter*0.001f; break;
			case 2: add_to_pitch-=cur_parameter*0.001f; break;
		}
		OSCSpeed=(float)pow(2.0, (55.235+cur_realnote+add_to_pitch+vpar->Finetune*0.0039062f)/12.0)*0.03125;
		if (OSCSpeed > 370) OSCSpeed=370; //Limit C-0 to C-7 (8 octaves because sid also has 8)
		if (cur_waveform > 6) OSCSpeed*=0.25;
		OSCSpeed*=0.0625;
	}

	for (int i = 0; i<16; i++){
		switch(cur_waveform)
		{
			case 5:								output+=vpar->Wavetable[cur_waveform][int(OSCPosition)+cur_pw]; break;
			case 8:								output+=vpar->shortnoise; break;
			default:				output+=vpar->Wavetable[cur_waveform][int(OSCPosition)]; break;
		}
		OSCPosition+=OOSCSpeed;
		if(OSCPosition>=2048.0f) OSCPosition-=2048.0f;
	}
	output*=0.0625;
	if (cur_waveform > 6) vpar->noiseused=true;
	GetEnvVcf();

	if(!timetocompute--)
	{
		if ((fltMode == 1) || (fltMode == 2)){
			int realcutoff=VcfCutoff+VcfEnvMod*VcfEnvValue;
			if(realcutoff<1)realcutoff=1;
			if(realcutoff>250)realcutoff=250;
			m_filter.setfilter(fltMode-1,realcutoff,vpar->Resonance);
			timetocompute=FILTER_CALC_TIME;
		}
	}
	rcVol+=(((GetEnvAmp()*OSCVol*voicevol)-rcVol)*rcVolCutoff);
	if (fltMode) return m_filter.res(output)*rcVol;
	else return output*rcVol;
}

float CSynthTrack::GetEnvAmp()
{
	switch(AmpEnvStage)
	{
	case 1: // Attack
		AmpEnvValue+=AmpEnvCoef;
		
		if(AmpEnvValue>1.0f)
		{
			AmpEnvCoef=(1.0f-AmpEnvSustainLevel)/(float)vpar->AEGDecay;
			if (AmpEnvCoef>minFade) AmpEnvCoef=minFade;
			AmpEnvStage=2;
		}

		return AmpEnvValue;
	break;

	case 2: // Decay
		AmpEnvValue-=AmpEnvCoef;
		
		if(AmpEnvValue<AmpEnvSustainLevel)
		{
			AmpEnvValue=AmpEnvSustainLevel;
			AmpEnvStage=3;

			if(!vpar->AEGSustain) AmpEnvStage=0;
		}

		return AmpEnvValue;
	break;

	case 3:
		return AmpEnvValue;
	break;

	case 4: // Release
		AmpEnvValue-=AmpEnvCoef;

		if(AmpEnvValue<0.0f)
		{
			AmpEnvValue=0.0f;
			AmpEnvStage=0;
		}

		return AmpEnvValue;
	break;
	
	case 5: // FastRelease
		AmpEnvValue-=AmpEnvCoef;

		if(AmpEnvValue<0.0f)
		{
			AmpEnvValue=0.0f;
			RealNoteOn();
			AmpEnvStage=1;
			AmpEnvCoef=1.0f/(float)vpar->AEGAttack;
			if (AmpEnvCoef>minFade) AmpEnvCoef=minFade;
		}

		return AmpEnvValue;
	break;
	
	}

	return 0;
}

void CSynthTrack::GetEnvVcf()
{
	switch(VcfEnvStage)
	{
	case 1: // Attack
		VcfEnvValue+=VcfEnvCoef;
		
		if(VcfEnvValue>1.0f)
		{
			VcfEnvCoef=(1.0f-VcfEnvSustainLevel)/(float)vpar->FEGDecay;
			if (VcfEnvCoef>minFade) VcfEnvCoef=minFade;
			VcfEnvStage=2;
		}
	break;

	case 2: // Decay
		VcfEnvValue-=VcfEnvCoef;
		
		if(VcfEnvValue<VcfEnvSustainLevel)
		{
			VcfEnvValue=VcfEnvSustainLevel;
			VcfEnvStage=3;

			if(!vpar->FEGSustain) VcfEnvStage=0;
		}
	break;

	case 4: // Release
		VcfEnvValue-=VcfEnvCoef;

		if(VcfEnvValue<0.0f)
		{
			VcfEnvValue=0.0f;
			VcfEnvStage=0;
		}
	break;

	case 5: // FastRelease
		VcfEnvValue-=VcfEnvCoef;

		if(VcfEnvValue<0.0f)
		{
			VcfEnvValue=0.0f;
			VcfEnvStage=1;
			VcfEnvCoef=1.0f/(float)vpar->FEGAttack;
			if (VcfEnvCoef>minFade) VcfEnvCoef=minFade;
		}

	break;
	}
}

void CSynthTrack::NoteOff()
{
	keyrelease=true;
	if((AmpEnvStage>0) && (AmpEnvStage<4))
	{
		AmpEnvStage=4;
		AmpEnvCoef=AmpEnvValue/(float)vpar->AEGRelease;
		if (AmpEnvCoef>minFade) AmpEnvCoef=minFade;
	}
	if ((VcfEnvStage>0) && (VcfEnvStage<4)){
		VcfEnvStage=4;
		VcfEnvCoef=VcfEnvValue/(float)vpar->FEGRelease;
		if (VcfEnvCoef>minFade) VcfEnvCoef=minFade;
	}
}

void CSynthTrack::DoGlide()
{
	// Glide Handler
	if(ROSCSpeed<OSCSpeed)
	{
		ROSCSpeed+=oscglide;

		if(ROSCSpeed>OSCSpeed)
			ROSCSpeed=OSCSpeed;
	}
	else
	{
		ROSCSpeed-=oscglide;

		if(ROSCSpeed<OSCSpeed)
			ROSCSpeed=OSCSpeed;
	}
	OOSCSpeed=ROSCSpeed;
}

void CSynthTrack::PerformFx()
{
	// Perform tone glide
	DoGlide();
	//Vol
	if(sp_cmd==12) nextvoicevol=(float)sp_val/255.0f;
}

void CSynthTrack::InitEffect(int cmd, int val)
{
sp_cmd=cmd;
sp_val=val;

// Init glide
if(cmd==3)
oscglide=(float)(val*val)*0.001f;
else
oscglide=99999.0f;
//Vol
if(sp_cmd==12)nextvoicevol=(float)sp_val/255.0f;
}
}