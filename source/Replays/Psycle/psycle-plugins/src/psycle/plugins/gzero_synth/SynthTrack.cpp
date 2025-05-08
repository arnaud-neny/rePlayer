#include "SynthTrack.hpp"
#include <math.h> ///\todo should be <cmath>
///\file arguru_synth_2_final/SynthTrack.cpp
///\brief implementation of the CSynthTrack class.
namespace psycle::plugins::gzero_synth {
CSynthTrack::CSynthTrack()
{
	syntp=0;
	output=0;
	NoteCutTime=0;
	NoteCut=false;

	timetocompute=FILTER_CALC_TIME;

	lfo_phase=0;
	synthglide = 256;

	VibratoSpeed=0;
	VibratoDepth=0;
	OSC1Vol=0;
	OSC2Vol=0;
	OSC3Vol=0;
	OSC4Vol=0;
	ArpCounter=0;
	oscglide=0;
	Arp_tickcounter=0;
	Arp_samplespertick=0;
	Arp_basenote=0;
	ArpMode=0;

	sp_cmd=0;
	sp_val=0;

	OSC1Speed=0.0f;
	OSC2Speed=0.0f;
	ROSC1Speed=0.0f;
	ROSC2Speed=0.0f;
	OSC1Position=0.0f;
	OSC2Position=0.0f;
	OSC3Speed=0.0f;
	OSC4Speed=0.0f;
	ROSC3Speed=0.0f;
	ROSC4Speed=0.0f;
	OSC3Position=0.0f;
	OSC4Position=0.0f;
	
	VibratoGr=0;
	OSCvib=0;
	AmpEnvStage=0;
	AmpEnvValue=0.0f;
	VcfEnvStage=0;
	VcfEnvValue=0.0f;
	Stage5AmpVal=0.0f;
	Stage5VcfVal=0.0f;
	vibrato=false;

	InitArpeggio();
	m_filter.init(44100);
}

CSynthTrack::~CSynthTrack()
{

}

void CSynthTrack::NoteOn(int note, SYNPAR *tspar,int spd)
{
	bool initenvelopes=false;
	syntp=tspar;

	InitLfo(syntp->vcf_lfo_speed,syntp->vcf_lfo_amplitude);

	float nnote=(float)note+
		(float)tspar->globalfinetune*0.0038962f+
		(float)tspar->globaldetune;
	OSC1Speed=(float)pow(2.0, (float)nnote/12.0);

	float note2=nnote+
		(float)tspar->osc2finetune*0.0038962f+
		(float)tspar->osc2detune;
	OSC2Speed=(float)pow(2.0, (float)note2/12.0);

	float note3=nnote+
		(float)tspar->osc3finetune*0.0038962f+
		(float)tspar->osc3detune;
	OSC3Speed=(float)pow(2.0, (float)note3/12.0);
	
	float note4=nnote+
		(float)tspar->osc4finetune*0.0038962f+
		(float)tspar->osc4detune;
	OSC4Speed=(float)pow(2.0, (float)note4/12.0);

	if (oscglide == 0.0f || AmpEnvStage == 0)
	{
		ROSC1Speed = OSC1Speed;
		ROSC2Speed = OSC2Speed;
		ROSC3Speed = OSC3Speed;
		ROSC4Speed = OSC4Speed;
		if (AmpEnvStage == 0)
		{
			if (syntp->osc2sync) OSC2Position = 0;
			else
			{
				OSC2Position-= OSC1Position;
				if (OSC2Position<0) OSC2Position+=2048.0f;
			}
			if (syntp->osc4sync) OSC3Position = 0;
			else
			{
				OSC3Position-= OSC1Position;
				if (OSC3Position<0) OSC3Position+=2048.0f;
			}
			if (syntp->osc4sync) OSC4Position = 0;
			else
			{
				OSC4Position-= OSC1Position;
				if (OSC2Position<0) OSC4Position+=2048.0f;
			}
			OSC1Position = 0;
		}
		initenvelopes =true;
	}
	else if ( oscglide > 0.0f && AmpEnvStage > 3) initenvelopes = true;

	OSC2Vol=(float)syntp->osc12_mix*0.0039062f;
	OSC1Vol=1.0f-OSC2Vol;
	OSC4Vol=(float)syntp->osc34_mix*0.0039062f;
	OSC3Vol=1.0f-OSC4Vol;


	float spdcoef;
	
	if(spd<65)				spdcoef=(float)spd*0.015625f;
	else								spdcoef=1.0f;

	OSC1Vol*=(float)syntp->out_vol*0.0039062f*spdcoef;
	OSC2Vol*=(float)syntp->out_vol*0.0039062f*spdcoef;
	OSC3Vol*=(float)syntp->out_vol*0.0039062f*spdcoef;
	OSC4Vol*=(float)syntp->out_vol*0.0039062f*spdcoef;

	ArpLimit=tspar->arp_cnt;
	ArpMode=syntp->arp_mod;
	Arp_tickcounter=0;
	Arp_basenote=nnote;
	Arp_samplespertick=2646000/(syntp->arp_bpm*4);
	ArpCounter=1;

	InitEnvelopes(initenvelopes);

}

void CSynthTrack::InitEnvelopes(bool force)
{
	VcfEnvMod=(float)syntp->vcf_envmod;
	VcfCutoff=(float)syntp->vcf_cutoff;
	VcfResonance=(float)syntp->vcf_resonance/256.0f;

	// Init Amplitude Envelope
	AmpEnvSustainLevel=(float)syntp->amp_env_sustain*0.0039062f;
	if(AmpEnvStage < 2 || force)
	{
		AmpEnvStage=1;
		AmpEnvCoef=1.0f/(float)syntp->amp_env_attack;
	}
	else if (AmpEnvStage < 4)
	{
		AmpEnvStage=5;
		AmpEnvCoef=1.0f/(float)syntp->amp_env_attack;
		Stage5AmpVal=0.0f;
	}
	
	// Init Filter Envelope
	VcfEnvSustainLevel=(float)syntp->vcf_env_sustain*0.0039062f;
	if (VcfEnvStage < 2 || force)
	{
		VcfEnvStage=1;
		VcfEnvCoef=1.0f/(float)syntp->vcf_env_attack;
	}
	else if (VcfEnvStage < 4)
	{
		VcfEnvStage=5;
		VcfEnvCoef=1.0f/(float)syntp->vcf_env_attack;
		Stage5VcfVal=0.0f;
	}
}

void CSynthTrack::NoteOff(bool fast)
{
	float const unde = 0.00001f;

	if(AmpEnvStage)
	{
		AmpEnvStage=4;
		VcfEnvStage=4;

		AmpEnvCoef=AmpEnvValue/(float)syntp->amp_env_release;
		VcfEnvCoef=VcfEnvValue/(float)syntp->vcf_env_release;

		if(AmpEnvCoef<unde)AmpEnvCoef=unde;
		if(VcfEnvCoef<unde)VcfEnvCoef=unde;

//								if(!syntp->amp_env_sustain) AmpEnvStage=0;
	}
}

void CSynthTrack::Vibrate()
{
	if(vibrato)
	{
		OSCvib=(float)sin(VibratoGr)*VibratoDepth;
		VibratoGr+=VibratoSpeed;

		if (VibratoGr>6.28318530717958647692528676655901f)
			VibratoGr-=6.28318530717958647692528676655901f;
	}
}

void CSynthTrack::ActiveVibrato(int speed,int depth)
{
	VibratoSpeed=(float)depth/16.0f;
	VibratoDepth=(float)speed/16.0f;
	vibrato=true;
}

void CSynthTrack::DisableVibrato()
{
	vibrato=false;
}

void CSynthTrack::InitArpeggio()
{
	ArpNote[0][0]				=0;
	ArpNote[0][1]				=3;
	ArpNote[0][2]				=7;
	ArpNote[0][3]				=12;
	ArpNote[0][4]				=15;
	ArpNote[0][5]				=19;
	ArpNote[0][6]				=24;
	ArpNote[0][7]				=27;
	ArpNote[0][8]				=31;
	ArpNote[0][9]				=36;
	ArpNote[0][10]				=39;
	ArpNote[0][11]				=43;
	ArpNote[0][12]				=48;
	ArpNote[0][13]				=51;
	ArpNote[0][14]				=55;
	ArpNote[0][15]				=60;

	ArpNote[1][0]				=0;
	ArpNote[1][1]				=4;
	ArpNote[1][2]				=7;
	ArpNote[1][3]				=12;
	ArpNote[1][4]				=16;
	ArpNote[1][5]				=19;
	ArpNote[1][6]				=24;
	ArpNote[1][7]				=28;
	ArpNote[1][8]				=31;
	ArpNote[1][9]				=36;
	ArpNote[1][10]				=40;
	ArpNote[1][11]				=43;
	ArpNote[1][12]				=48;
	ArpNote[1][13]				=52;
	ArpNote[1][14]				=55;
	ArpNote[1][15]				=60;

	ArpNote[2][0]				=0;
	ArpNote[2][1]				=3;
	ArpNote[2][2]				=7;
	ArpNote[2][3]				=10;
	ArpNote[2][4]				=12;
	ArpNote[2][5]				=15;
	ArpNote[2][6]				=19;
	ArpNote[2][7]				=22;
	ArpNote[2][8]				=24;
	ArpNote[2][9]				=27;
	ArpNote[2][10]				=31;
	ArpNote[2][11]				=34;
	ArpNote[2][12]				=36;
	ArpNote[2][13]				=39;
	ArpNote[2][14]				=43;
	ArpNote[2][15]				=46;

	ArpNote[3][0]				=0;
	ArpNote[3][1]				=4;
	ArpNote[3][2]				=7;
	ArpNote[3][3]				=10;
	ArpNote[3][4]				=12;
	ArpNote[3][5]				=16;
	ArpNote[3][6]				=19;
	ArpNote[3][7]				=22;
	ArpNote[3][8]				=24;
	ArpNote[3][9]				=28;
	ArpNote[3][10]				=31;
	ArpNote[3][11]				=34;
	ArpNote[3][12]				=36;
	ArpNote[3][13]				=40;
	ArpNote[3][14]				=43;
	ArpNote[3][15]				=46;

	ArpNote[4][0]				=0;
	ArpNote[4][1]				=12;
	ArpNote[4][2]				=0;
	ArpNote[4][3]				=-12;
	ArpNote[4][4]				=12;
	ArpNote[4][5]				=0;
	ArpNote[4][6]				=12;
	ArpNote[4][7]				=0;
	ArpNote[4][8]				=0;
	ArpNote[4][9]				=12;
	ArpNote[4][10]				=-12;
	ArpNote[4][11]				=0;
	ArpNote[4][12]				=12;
	ArpNote[4][13]				=0;
	ArpNote[4][14]				=12;
	ArpNote[4][15]				=-12;

	ArpNote[5][0]				=0;
	ArpNote[5][1]				=12;
	ArpNote[5][2]				=24;
	ArpNote[5][3]				=0;
	ArpNote[5][4]				=12;
	ArpNote[5][5]				=24;
	ArpNote[5][6]				=12;
	ArpNote[5][7]				=0;
	ArpNote[5][8]				=24;
	ArpNote[5][9]				=12;
	ArpNote[5][10]				=0;
	ArpNote[5][11]				=0;
	ArpNote[5][12]				=12;
	ArpNote[5][13]				=0;
	ArpNote[5][14]				=0;
	ArpNote[5][15]				=24;

	ArpNote[6][0]				=0;
	ArpNote[6][1]				=12;
	ArpNote[6][2]				=19;
	ArpNote[6][3]				=0;
	ArpNote[6][4]				=0;
	ArpNote[6][5]				=7;
	ArpNote[6][6]				=0;
	ArpNote[6][7]				=7;
	ArpNote[6][8]				=0;
	ArpNote[6][9]				=12;
	ArpNote[6][10]				=19;
	ArpNote[6][11]				=0;
	ArpNote[6][12]				=12;
	ArpNote[6][13]				=19;
	ArpNote[6][14]				=0;
	ArpNote[6][15]				=-12;

	ArpNote[7][0]				=0;
	ArpNote[7][1]				=3;
	ArpNote[7][2]				=7;
	ArpNote[7][3]				=12;
	ArpNote[7][4]				=15;
	ArpNote[7][5]				=19;
	ArpNote[7][6]				=24;
	ArpNote[7][7]				=27;
	ArpNote[7][8]				=31;
	ArpNote[7][9]				=27;
	ArpNote[7][10]				=24;
	ArpNote[7][11]				=19;
	ArpNote[7][12]				=15;
	ArpNote[7][13]				=12;
	ArpNote[7][14]				=7;
	ArpNote[7][15]				=3;

	ArpNote[8][0]				=0;
	ArpNote[8][1]				=4;
	ArpNote[8][2]				=7;
	ArpNote[8][3]				=12;
	ArpNote[8][4]				=16;
	ArpNote[8][5]				=19;
	ArpNote[8][6]				=24;
	ArpNote[8][7]				=28;
	ArpNote[8][8]				=31;
	ArpNote[8][9]				=28;
	ArpNote[8][10]				=24;
	ArpNote[8][11]				=19;
	ArpNote[8][12]				=16;
	ArpNote[8][13]				=12;
	ArpNote[8][14]				=7;
	ArpNote[8][15]				=4;

	ArpNote[9][0]				=0;
	ArpNote[9][1]				=12;
	ArpNote[9][2]				=0;
	ArpNote[9][3]				=12;
	ArpNote[9][4]				=0;
	ArpNote[9][5]				=12;
	ArpNote[9][6]				=0;
	ArpNote[9][7]				=12;
	ArpNote[9][8]				=0;
	ArpNote[9][9]				=12;
	ArpNote[9][10]				=0;
	ArpNote[9][11]				=12;
	ArpNote[9][12]				=0;
	ArpNote[9][13]				=12;
	ArpNote[9][14]				=0;
	ArpNote[9][15]				=12;

}

void CSynthTrack::DoGlide()
{
	// Glide Handler
	if(ROSC1Speed<OSC1Speed)
	{
		ROSC1Speed+=oscglide;

		if(ROSC1Speed>OSC1Speed) ROSC1Speed=OSC1Speed;
	}
	else
	{
		ROSC1Speed-=oscglide;

		if(ROSC1Speed<OSC1Speed) ROSC1Speed=OSC1Speed;
	}

	if(ROSC2Speed<OSC2Speed)
	{
		ROSC2Speed+=oscglide;

		if(ROSC2Speed>OSC2Speed) ROSC2Speed=OSC2Speed;
	}
	else
	{
		ROSC2Speed-=oscglide;

		if(ROSC2Speed<OSC2Speed) ROSC2Speed=OSC2Speed;
	}

	if(ROSC3Speed<OSC3Speed)
	{
		ROSC3Speed+=oscglide;

		if(ROSC3Speed>OSC3Speed) ROSC3Speed=OSC3Speed;
	}
	else
	{
		ROSC3Speed-=oscglide;

		if(ROSC3Speed<OSC3Speed) ROSC3Speed=OSC3Speed;
	}

	if(ROSC4Speed<OSC4Speed)
	{
		ROSC4Speed+=oscglide;

		if(ROSC4Speed>OSC4Speed) ROSC4Speed=OSC4Speed;
	}
	else
	{
		ROSC4Speed-=oscglide;

		if(ROSC4Speed<OSC4Speed) ROSC4Speed=OSC4Speed;
	}
}

void CSynthTrack::PerformFx()
{
	/* 0x0E : NoteCut Time */
	if(NoteCutTime<0)
	{
		NoteCutTime=0;
		NoteCut=false;
		NoteOff();
	}

	Vibrate();

	float shift;

	switch(sp_cmd)
	{
		/* 0x01 : Pitch Up */
		case 0x01:
			shift=(float)sp_val*0.001f;
			OSC1Speed+=shift;
			OSC2Speed+=shift;
			OSC3Speed+=shift;
			OSC4Speed+=shift;
			break;

		/* 0x02 : Pitch Down */
		case 0x02:
			shift=(float)sp_val*0.001f;
			OSC1Speed-=shift;
			OSC2Speed-=shift;
			OSC3Speed-=shift;
			OSC4Speed-=shift;
			if(OSC1Speed<0)OSC1Speed=0;
			if(OSC2Speed<0)OSC2Speed=0;
			if(OSC3Speed<0)OSC3Speed=0;
			if(OSC4Speed<0)OSC4Speed=0;
			break;

		/* 0x11 : CutOff Up */
		case 0x11:
			VcfCutoff+=sp_val;
			if(VcfCutoff>127)VcfCutoff=127;
			break;

		/* 0x12 : CutOff Down */
		case 0x12:
			VcfCutoff-=sp_val;
			if(VcfCutoff<0)VcfCutoff=0;
			break;
	}
	// Perform tone glide
	DoGlide();
}

void CSynthTrack::InitEffect(int cmd, int val)
{
	sp_cmd=cmd;
	sp_val=val;

	// Init glide
	if (cmd==3) { if ( val != 0 ) oscglide= (float)(val*val)*0.001f; }
	else 
	{
		if ( syntp )
		{
			synthglide = 256-syntp->synthglide;
		}
		if (synthglide < 256.0f) oscglide = (synthglide*synthglide)*0.0000625f;
		else oscglide= 0.0f;
	}

	// Init vibrato
	if (cmd==4)				ActiveVibrato(val>>4,val&0xf);
	else								DisableVibrato();

	// Note CUT
	if (cmd==0x0E && val>0)
	{
		NoteCutTime=val*32;
		NoteCut=true;
	}
	else NoteCut=false;

}

void CSynthTrack::InitLfo(int freq,int amp)
{
	lfo_freq=(float)freq*0.000005f;
}
}