#pragma once
#include "filter.hpp"
///\file SynthTrack.h
///\brief interface for the CSynthTrack class.

#define FILTER_CALC_TIME				64
#define TWOPI																6.28318530717958647692528676655901f
namespace psycle::plugins::gzero_synth {
struct SYNPAR
{
	signed short *pWave;
	signed short *pWave2;
	int osc2detune;
	int osc2finetune;
	int osc2sync;
	signed short *pWave3;
	int osc3detune;
	int osc3finetune;
	int osc3sync;
	signed short *pWave4;
	int osc4detune;
	int osc4finetune;
	int osc4sync;

	int amp_env_attack;
	int amp_env_decay;
	int amp_env_sustain;
	int amp_env_release;

	int vcf_env_attack;
	int vcf_env_decay;
	int vcf_env_sustain;
	int vcf_env_release;
	int vcf_lfo_speed;
	int vcf_lfo_amplitude;
	int vcf_cutoff;
	int vcf_resonance;
	int vcf_type;
	int vcf_envmod;

	int osc12_mix;
	int osc34_mix;
	int out_vol;

	int arp_mod;
	int arp_bpm;
	int arp_cnt;

	int custom_arp_step[16];

	int globaldetune;
	int globalfinetune;
	int synthglide;
	int interpolate;
};

class CSynthTrack  
{
public:

	void InitEffect(int cmd,int val);
	void PerformFx();
	void DoGlide();
	void DisableVibrato();
	void ActiveVibrato(int speed,int depth);
	void Vibrate();
	float Filter(float x);
	void NoteOff(bool stop=false);
	float GetEnvAmp();
	void GetEnvVcf();
	float oscglide;
	float GetSample();
	void NoteOn(int note, SYNPAR *tspar,int spd);
	void InitArpeggio();
	
	CSynthTrack();
	virtual ~CSynthTrack();

	int AmpEnvStage;
	int NoteCutTime;
	bool NoteCut;
	filter m_filter;

private:
	float output;
	float lfo_freq;
	float lfo_phase;

	void InitLfo(int freq,int amp);

	short timetocompute;
	void InitEnvelopes(bool force=false);

	float VcfResonance;
	int sp_cmd;
	int sp_val;

	float OSC1Speed;
	float OSC2Speed;
	float OSC3Speed;
	float OSC4Speed;
	float ROSC1Speed;
	float ROSC2Speed;
	float ROSC3Speed;
	float ROSC4Speed;
	
	float OSC1Position;
	float OSC2Position;
	float OSC3Position;
	float OSC4Position;

	float OSCvib;
	float VibratoGr;
	float VibratoSpeed;
	float VibratoDepth;

	// Arpeggiator
	int Arp_tickcounter;
	int Arp_samplespertick;
	float Arp_basenote;
	int ArpMode;
	unsigned char ArpCounter;
	unsigned char ArpLimit;

	signed char ArpNote[16][16];

	bool Arp;

	// Envelope [Amplitude]
	float AmpEnvValue;
	float AmpEnvCoef;
	float AmpEnvSustainLevel;
	float Stage5AmpVal;
	bool vibrato;
	float OSC1Vol;
	float OSC2Vol;
	float OSC3Vol;
	float OSC4Vol;

	// Envelope [Filter]
	float VcfEnvValue;
	float VcfEnvCoef;
	float VcfEnvSustainLevel;
	int VcfEnvStage;
	float Stage5VcfVal;
	float VcfEnvMod;
	float VcfCutoff;
	float synthglide;
	
	SYNPAR *syntp;

	inline void ArpTick(void);
	inline void FilterTick(void);
	inline int f2i(double d)
	{
#ifdef __BIG_ENDIAN__
    return static_cast<int>(d);
#else
		const double magic = 6755399441055744.0; // 2^51 + 2^52
		union tmp_union
		{
			double d;
			int i;
		} tmp;
		tmp.d = (d-0.5) + magic;
		return tmp.i;
#endif
	}

};

inline void CSynthTrack::ArpTick()
{
	Arp_tickcounter=0;

	float note;
	if(syntp->arp_mod == 16)
	{	
		note=Arp_basenote+(float)syntp->custom_arp_step[ArpCounter];
	}
	else
	{
		note=Arp_basenote+(float)ArpNote[syntp->arp_mod-1][ArpCounter];
	}
	OSC1Speed=(float)pow(2.0, note/12.0);

	float note2=note+
	(float)syntp->osc2finetune*0.0039062f+
	(float)syntp->osc2detune;
	OSC2Speed=(float)pow(2.0, note2/12.0);

	float note3=note+
	(float)syntp->osc3finetune*0.0039062f+
	(float)syntp->osc3detune;
	OSC3Speed=(float)pow(2.0, note3/12.0);

	float note4=note+
	(float)syntp->osc4finetune*0.0039062f+
	(float)syntp->osc4detune;
	OSC4Speed=(float)pow(2.0, note4/12.0);

	if(++ArpCounter>=syntp->arp_cnt)  ArpCounter=0;

	InitEnvelopes(AmpEnvStage<4);
	if ( oscglide == 0.0f ) oscglide =256.0f;
}

inline void CSynthTrack::FilterTick()
{
	lfo_phase+=lfo_freq;

	if (lfo_phase>TWOPI) lfo_phase-=TWOPI;
	
	float const VcfLfoVal=sin(lfo_phase)*(float)syntp->vcf_lfo_amplitude;
	int realcutoff=VcfCutoff+VcfLfoVal+VcfEnvMod*VcfEnvValue;

	if (realcutoff<1) realcutoff=1;
	if (realcutoff>240) realcutoff=240;

	m_filter.setfilter(syntp->vcf_type%10,realcutoff,syntp->vcf_resonance);
	timetocompute=FILTER_CALC_TIME;

}

inline float CSynthTrack::GetSample()
{

	if(AmpEnvStage)
	{
		if ((ArpMode>0) && (++Arp_tickcounter>Arp_samplespertick)) ArpTick();
	
		if ( syntp->interpolate )  // Quite Pronounced CPU usage increase...
		{
			int iOsc=f2i(OSC1Position);
			float fractpart=OSC1Position-iOsc;
			float d0=syntp->pWave[iOsc-1];
			float d1=syntp->pWave[iOsc];
			float d2=syntp->pWave[iOsc+1];
			float d3=syntp->pWave[iOsc+2];
			output=((((((((3*(d1-d2))-d0)+d3)*0.5f*fractpart)+((2*d2)+d0)-(((5*d1)+d3)*0.5f))*fractpart)+((d2-d0)*0.5f))*fractpart+d1)*OSC1Vol;


			iOsc=f2i(OSC2Position);
			fractpart=OSC2Position-iOsc;
			d0=syntp->pWave2[iOsc-1];
			d1=syntp->pWave2[iOsc];
			d2=syntp->pWave2[iOsc+1];
			d3=syntp->pWave2[iOsc+2];
			output+=((((((((3*(d1-d2))-d0)+d3)*0.5f*fractpart)+((2*d2)+d0)-(((5*d1)+d3)*0.5f))*fractpart)+((d2-d0)*0.5f))*fractpart+d1)*OSC2Vol;


			iOsc=f2i(OSC3Position);
			fractpart=OSC3Position-iOsc;
			d0=syntp->pWave3[iOsc-1];
			d1=syntp->pWave3[iOsc];
			d2=syntp->pWave3[iOsc+1];
			d3=syntp->pWave3[iOsc+2];
			output+=((((((((3*(d1-d2))-d0)+d3)*0.5f*fractpart)+((2*d2)+d0)-(((5*d1)+d3)*0.5f))*fractpart)+((d2-d0)*0.5f))*fractpart+d1)*OSC3Vol;

			
			iOsc=f2i(OSC4Position);
			fractpart=OSC4Position-iOsc;
			d0=syntp->pWave4[iOsc-1];
			d1=syntp->pWave4[iOsc];
			d2=syntp->pWave4[iOsc+1];
			d3=syntp->pWave4[iOsc+2];
			output+=((((((((3*(d1-d2))-d0)+d3)*0.5f*fractpart)+((2*d2)+d0)-(((5*d1)+d3)*0.5f))*fractpart)+((d2-d0)*0.5f))*fractpart+d1)*OSC4Vol;


		}
		else
		{
			output=syntp->pWave[f2i(OSC1Position)]*OSC1Vol+
					syntp->pWave2[f2i(OSC2Position)]*OSC2Vol+
					syntp->pWave3[f2i(OSC3Position)]*OSC3Vol+
					syntp->pWave4[f2i(OSC4Position)]*OSC4Vol;
		}

		if(vibrato)
		{
			OSC1Position+=ROSC1Speed+OSCvib;
			OSC2Position+=ROSC2Speed+OSCvib;
			OSC3Position+=ROSC3Speed+OSCvib;
			OSC4Position+=ROSC4Speed+OSCvib;
		}
		else
		{
			OSC1Position+=ROSC1Speed;
			OSC2Position+=ROSC2Speed;
			OSC3Position+=ROSC3Speed;
			OSC4Position+=ROSC4Speed;
		}
		
		if(OSC1Position>=2048.0f)
		{
			OSC1Position-=2048.0f;
		
			if(syntp->osc2sync)				OSC2Position=OSC1Position;
			if(syntp->osc3sync)				OSC3Position=OSC1Position;
			if(syntp->osc4sync)				OSC4Position=OSC1Position;
		}

		if(OSC2Position>=2048.0f) OSC2Position-=2048.0f;
		if(OSC3Position>=2048.0f) OSC3Position-=2048.0f;
		if(OSC4Position>=2048.0f) OSC4Position-=2048.0f;


		GetEnvVcf();

		if(!timetocompute--) FilterTick();
		
		if ( syntp->vcf_type > 9 ) return m_filter.res2(output)*GetEnvAmp();
		else return m_filter.res(output)*GetEnvAmp();
	}
	else return 0;
}

inline float CSynthTrack::GetEnvAmp()
{
	switch(AmpEnvStage)
	{
	case 1: // Attack
		AmpEnvValue+=AmpEnvCoef;
		
		if(AmpEnvValue>1.0f)
		{
			AmpEnvCoef=(1.0f-AmpEnvSustainLevel)/(float)syntp->amp_env_decay;
			AmpEnvStage=2;
		}

		return AmpEnvValue;
	break;

	case 2: // Decay
		AmpEnvValue-=AmpEnvCoef;
		
		if(AmpEnvValue<AmpEnvSustainLevel)
		{
			if(AmpEnvSustainLevel == 0.0f)
			{
				AmpEnvStage=0;
				ROSC1Speed=0.0f;
				ROSC2Speed=0.0f;
				ROSC3Speed=0.0f;
				ROSC4Speed=0.0f;
			}
			else
			{
				AmpEnvValue=AmpEnvSustainLevel;
				AmpEnvStage=3;
			}
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
			ROSC1Speed=0.0f;
			ROSC2Speed=0.0f;
			ROSC3Speed=0.0f;
			ROSC4Speed=0.0f;
		}

		return AmpEnvValue;
	break;
	
	case 5: // FastRelease
		AmpEnvValue-=AmpEnvCoef;
		Stage5AmpVal+=AmpEnvCoef;

		if(AmpEnvValue<Stage5AmpVal)
		{
			AmpEnvValue=Stage5AmpVal;
			AmpEnvStage=1;
		}

		return AmpEnvValue;
	break;
	
	}

	return 0;
}

inline void CSynthTrack::GetEnvVcf()
{
	switch(VcfEnvStage)
	{
	case 1: // Attack
		VcfEnvValue+=VcfEnvCoef;
		
		if(VcfEnvValue>1.0f)
		{
			VcfEnvCoef=(1.0f-VcfEnvSustainLevel)/(float)syntp->vcf_env_decay;
			VcfEnvStage=2;
		}
	break;

	case 2: // Decay
		VcfEnvValue-=VcfEnvCoef;
		
		if(VcfEnvValue<VcfEnvSustainLevel)
		{
			VcfEnvValue=VcfEnvSustainLevel;
			VcfEnvStage=3;
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
		Stage5VcfVal+=VcfEnvCoef;

		if(VcfEnvValue<Stage5VcfVal)
		{
			VcfEnvValue=Stage5VcfVal;
			VcfEnvStage=1;
		}

	break;
	}
}
}