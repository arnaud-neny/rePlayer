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
namespace psycle::plugins::jme::gamefxn {
using namespace psycle::helpers;
using namespace universalis::stdlib;

#define FILTER_CALC_TIME 64
#define TWOPI 6.28318530717958647692528676655901f

// Amount to compensate non-Hz based values that work with the wavetable, like the glide parameter.
// 1.56605 = (44100samplespersec/2048samples of old wavetable)/27.5Hz freq of current wavetable
const float CSynthTrack::wtNewACorrection = (44100.f/2048.f)/27.5f;


CSynthTrack::CSynthTrack()
{
	replaycount=1;
	perf_count=1;
	perf_index=1;

	cur_basenote=0;
	cur_realnote=0;
	voicevol=1.0f;
	nextvoicevol=1.0f;
	cur_waveform=0;
	cur_transpose=0;
	add_to_pitch=0.0f;
	cur_option=0;
	cur_command=0;
	cur_parameter=0;
	cur_speed=16;
	cur_pw=512;
	trigger=false;
	allowRetrig=false;
	stopsent=false;
	rcVol=0.0f;

	OSCVol=0;
	oscglide=0;

	sp_cmd=0;
	sp_val=0;

	OSCSpeed=0.0f;
	ROSCSpeed=0.0f;
	OSCPosition=0.0f;

	AmpEnvStage=0;
	AmpEnvValue=0.0f;
	VcfEnvStage=0;
	VcfEnvValue=0.0f;

	fltMode=1;
	aaf1.Setup(16000, AAF16::lowpass, -3.0f, 44100*16);

}

CSynthTrack::~CSynthTrack()
{

}
void CSynthTrack::setGlobalPar(PERFORMANCE* perf)
{
	vpar=perf;
}

void CSynthTrack::setSampleRate(int currentSR_, int wavetableSize_, float wavetableCorrection_) {
	if (fltMode != 0){
		filter.setAlgorithm((eAlgorithm)(fltMode - 1));
		filter.recalculateCoeffs((float)vpar->Cutoff/256.0f, (float)vpar->Resonance/256.0f, currentSR_);
	}
	//the sampling side works oversaples at oversamplesAmt
	aaf1.Setup(16000, AAF16::lowpass, -3.0f, currentSR_*vpar->oversamplesAmt);

	sampleRate = currentSR_;
	srCorrection = 44100.0f / (float)sampleRate;
	waveTableSize = wavetableSize_;
	wavetableCorrection = wavetableCorrection_;
	if (AmpEnvStage) {
		AmpEnvStage = 0;
	}
	cur_pw=waveTableSize*0.125f;
}

void CSynthTrack::NoteOn(int note)
{
	fltMode=1;
	nextNote=note;
	trigger=true;
	allowRetrig=false;
	nextvoicevol=1.0f;
	Retrig();
}

void CSynthTrack::RealNoteOn()
{
	if (trigger){
		trigger=false;
		allowRetrig=true;
		stopsent=false;
		cur_basenote=nextNote;
		voicevol=nextvoicevol;
		if(vpar->StartPos <= vpar->LoopEnd)
			perf_index = vpar->StartPos;
		else
			perf_index = vpar->LoopEnd;
		//Set to 1 so that they are evaluated on next GetSample
		replaycount = 1;
		perf_count = 1;

		if (!vpar->Volume[perf_index]) {
			OSCVol=0.5f;
		}
		if (!vpar->Waveform[perf_index]) {
			cur_waveform = 0;
		}
		if (!vpar->Transpose[perf_index]) {
			cur_transpose = 0;
			add_to_pitch=0.0f;
		}
		if (!vpar->Option[perf_index]) {
			cur_option = 0;
		}
		if (!vpar->Command[perf_index]) {
			cur_command= 0;
		}
		if (!vpar->Speed[perf_index]) {
			cur_speed = 16;
		}
	}
}

void CSynthTrack::Retrig()
{
	// Init Amplitude Envelope
	AmpEnvSustainLevel=(float)vpar->AEGSustain*0.00390625f;
	VcfEnvSustainLevel=(float)vpar->FEGSustain*0.00390625f;

	if(AmpEnvStage==0){
		RealNoteOn();
		AmpEnvStage=1;
		AmpEnvCoef=1.0f/(float)vpar->AEGAttack;
		VcfEnvStage=1;
		VcfEnvCoef=1.0f/(float)vpar->FEGAttack;
	}else{
		AmpEnvStage=5;
		AmpEnvCoef=AmpEnvValue/FAST_RELEASE;
		if(AmpEnvCoef<=0.0f)AmpEnvCoef=0.03125f;
		VcfEnvStage=4;
		VcfEnvCoef=VcfEnvValue/FAST_RELEASE;
		if(VcfEnvCoef<=0.0f)VcfEnvCoef=0.03125f;
	}
}

float CSynthTrack::GetSample()
{
	VcfEnvMod=(float)vpar->EnvMod;
	VcfCutoff=(float)vpar->Cutoff;
	VcfResonance=(float)vpar->Resonance/256.0f;
	replaycount--;
	if (replaycount <= 0){
		replaycount=vpar->ReplaySpeed;
		perf_count--;
		if (perf_count <= 0){
			perf_count = cur_speed;
			if (vpar->Volume[perf_index]){
				//Adapt from the range of PERFORMANCE.Volume 0..256
				OSCVol=(float)vpar->Volume[perf_index]*0.00390625f;
			}
			if (vpar->Waveform[perf_index]) cur_waveform=vpar->Waveform[perf_index]-1;
			if (vpar->Transpose[perf_index]) {
				add_to_pitch=0.0f;
				cur_transpose=vpar->Transpose[perf_index]-1;
				cur_option=vpar->Option[perf_index];
				cur_realnote = cur_transpose;
				if ((cur_option & 1) == 0) cur_realnote+=cur_basenote;
				if ((cur_option & 2) && allowRetrig) Retrig();
			}
			cur_command=vpar->Command[perf_index];
			cur_parameter=vpar->Parameter[perf_index];
			switch(cur_command){
				case 3: cur_realnote=(cur_realnote+cur_parameter)&127; if (cur_realnote>95) cur_realnote-=32; break;
				case 4: cur_realnote=(cur_realnote-cur_parameter)&127; if (cur_realnote>95) cur_realnote-=32; break;
				case 5: cur_pw=cur_parameter*0.00389625f*waveTableSize; break;
				case 6: 
					{
						cur_pw+=cur_parameter*0.00389625f*waveTableSize;
						if (cur_pw>waveTableSize) cur_pw-=waveTableSize;
						break;
					}
				case 7: 
					{
						cur_pw-=cur_parameter*0.00389625f*waveTableSize;
						if(cur_pw<0) cur_pw+=waveTableSize;
						break;
					}
				case 8: OSCPosition=cur_parameter<<4; break;
				case 9: fltMode=0; break;
				case 10: fltMode=1; break;
				case 11: fltMode=2; break;
				case 12: fltMode=3; break;
				case 13: fltMode=4; break;
				case 14: fltMode=5; break;
				case 15: fltMode=6; break;
				case 16: fltMode=7; break;
				case 17: if(stopsent==false) { stopsent=true; NoteOff(); } break;
			}
			if (vpar->Speed[perf_index]) {
				cur_speed=vpar->Speed[perf_index];
				perf_count=cur_speed;
			}
			perf_index++;
			if ((perf_index == vpar->LoopEnd+1) || (perf_index > 15))
			{
				if(vpar->LoopStart <= vpar->LoopEnd)
					perf_index = vpar->LoopStart;
				else
					perf_index = vpar->LoopEnd;
			}
		}
		switch(cur_command){
			case 1: add_to_pitch+=cur_parameter*0.001f; break;
			case 2: add_to_pitch-=cur_parameter*0.001f; break;
		}
		double tmpNote = cur_realnote+add_to_pitch+vpar->Finetune*0.00390625;
		//Limit C-1 to B-7 (8 octaves because sid also has 8)
		if (tmpNote < 12) tmpNote= 0;
		else if(tmpNote > 95) tmpNote = 95;
		OSCSpeed=(float)pow(2.0, tmpNote/12.0)*wavetableCorrection;
		if (cur_waveform == 7) OSCSpeed*=0.25;
	}

	int pos;
	float sample;
	float output=0.0f;

	int const oversampleAmt = vpar->oversamplesAmt;
	switch(cur_waveform)
	{
		case 5:	{	// pulse
					//16x oversample
					for (int i = 0; i<oversampleAmt; i++){
						OSCPosition+=ROSCSpeed;
						if(OSCPosition>=waveTableSize) OSCPosition-=waveTableSize;
						pos = math::rint<int32_t>(OSCPosition-0.5f);
						sample=vpar->Wavetable[cur_waveform][pos+cur_pw];
						output+=aaf1.process((vpar->Wavetable[cur_waveform][pos+cur_pw+1] - sample) * (OSCPosition - (float)pos) + sample);
					}
					break;
				}
		case 8: {	// low noise
					//16x oversample
					for (int i = 0; i<oversampleAmt; i++) output+=vpar->shortnoise;
					break;
				}
		default: {  // static waves
					//16x oversample
					for (int i = 0; i<oversampleAmt; i++){
						OSCPosition+=ROSCSpeed;
						if(OSCPosition>=waveTableSize) OSCPosition-=waveTableSize;
						pos = math::rint<int32_t>(OSCPosition-0.5f);
						sample=vpar->Wavetable[cur_waveform][pos];
						output+=aaf1.process((vpar->Wavetable[cur_waveform][pos+1] - sample) * (OSCPosition - (float)pos) + sample);
					}
					break;
				}
	}
	output*=0.0625f;
	if (cur_waveform > 6) vpar->noiseused=true;
	GetEnvVcf();

	if (fltMode != 0){
		int realcutoff=VcfCutoff+VcfEnvMod*VcfEnvValue;
		if(realcutoff<0.0f)realcutoff=0.0f;
		if(realcutoff>256.0f)realcutoff=256.0f;
		filter.setAlgorithm((eAlgorithm)(fltMode - 1));
		filter.recalculateCoeffs(((float)realcutoff)/256.0f, (float)vpar->Resonance/256.0f, sampleRate);
		filter.process(output); 
	}
	//Anticlick
	rcVol+=(((GetEnvAmp()*OSCVol*voicevol)-rcVol)*RC_VOL_CUTOFF);
	return output*rcVol;
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
			AmpEnvStage=2;
		}
	break;

	case 2: // Decay
		AmpEnvValue -= (AmpEnvValue * AmpEnvCoef * 5.0f)*srCorrection;
		
		//This means -60dB.
		if((AmpEnvValue<AmpEnvSustainLevel) || (AmpEnvValue<0.001f))
		{
			if(!vpar->AEGSustain)  {
				AmpEnvValue=0.0f;
				AmpEnvStage=0;
			}
			else {
				AmpEnvValue=AmpEnvSustainLevel;
				AmpEnvStage=3;
			}
		}
	break;

	case 4: // Release
		AmpEnvValue -= (AmpEnvValue * AmpEnvCoef * 5.0f)*srCorrection;
		//This means -60dB.
		if(AmpEnvValue<0.001f)
		{
			AmpEnvValue=0.0f;
			AmpEnvStage=0;
		}
	break;
	
	case 5: // FastRelease
		AmpEnvValue-=AmpEnvCoef;
		//This means -60dB.
		if(AmpEnvValue<0.001f)
		{
			AmpEnvValue=0.0f;
			RealNoteOn();
			AmpEnvStage=1;
			AmpEnvCoef=1.0f/(float)vpar->AEGAttack;
			VcfEnvStage=1;
			VcfEnvCoef=1.0f/(float)vpar->FEGAttack;
		}
	break;

	default:
		break;
	}

	return AmpEnvValue;
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

	default:
		break;
	}
}

void CSynthTrack::NoteOff()
{
	//control that it isn't too long
	float const unde = 1.0f/(65535.f*srCorrection);

	allowRetrig=false;
	if(AmpEnvStage>0)
	{
		AmpEnvStage=4;
		VcfEnvStage=4;

		AmpEnvCoef=AmpEnvValue/(float)vpar->AEGRelease;
		VcfEnvCoef=VcfEnvValue/(float)vpar->FEGRelease;

		if(AmpEnvCoef<unde)AmpEnvCoef=unde;
		if(VcfEnvCoef<unde)VcfEnvCoef=unde;
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
}

void CSynthTrack::PerformFx()
{
	// Perform tone glide
	DoGlide();
}

void CSynthTrack::InitEffect(int cmd, int val)
{
	sp_cmd=cmd;
	sp_val=val;

	// Init glide
	if(cmd==0x03)
		oscglide=(float)(val*val)*0.0000625f*pow(2.0f,wtNewACorrection*wavetableCorrection*srCorrection);
	else
		oscglide=99999.0f;
	//Vol
	if(sp_cmd==0x0C) nextvoicevol=(float)sp_val/255.0f;
}
}