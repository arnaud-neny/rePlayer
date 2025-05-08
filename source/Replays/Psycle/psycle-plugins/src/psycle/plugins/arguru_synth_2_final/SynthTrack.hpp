#pragma once
#include "filter.hpp"
#include <psycle/helpers/math.hpp>
#include <psycle/helpers/resampler.hpp>

namespace psycle::plugins::arguru_synth_2_final {

///\file SynthTrack.h
///\brief interface for the CSynthTrack class.

#define FILTER_CALC_TIME 64
#define TWOPI            2.0*math::pi

using namespace psycle::helpers;

struct SYNPAR
{
	float *pWave;
	bool wave1noise;
	float *pWave2;
	bool wave2noise;
	float osc2detune;
	float osc2finetune;
	bool osc2sync;
	float amp_env_attack;
	float amp_env_decay;
	int amp_env_sustain;
	float amp_env_release;
	float vcf_env_attack;
	float vcf_env_decay;
	int vcf_env_sustain;
	float vcf_env_release;
	float vcf_lfo_speed;
	int vcf_lfo_amplitude;
	int vcf_cutoff;
	int vcf_resonance;
	int vcf_type;
	int vcf_envmod;
	int osc_mix;
	int out_vol;
	int arp_mod;
	int arp_bpm;
	int arp_cnt;
	int globaldetune;
	int globalfinetune;
	int synthglide;
	int interpolate;
};

class CSynthTrack  
{
public:
	//wtNewACorrection is how much to speedup the wavetable reading due to the change from old 21.5Hz to new 13.75Hz.
	static const float wtNewACorrection;

	CSynthTrack();
	virtual ~CSynthTrack();

	void setSampleRate(int currentSR_, int wavetableSize_, float wavetableCorrection_);
	void setGlobalPar(SYNPAR* globalPar);
	void InitEffect(int cmd,int val);
	void NoteOn(int note);
	void NoteOff(bool stop=false);
	void PerformFx();
	//lfo_freq is the amount to add to the lfo_phase [-2PI..2PI] each 64samples.
	//freq_speed just uses a randomly choosen scale to say the same.
	//It can be converted to Hz with:
	//(freq_speed*0.000005f)*(samplerate/64.0f)/(2.0f*math::pi_f)
	void InitLfo(float freq_speed,int amp);
	void InitEnvelopes(bool force=false);
	inline float GetSample();
	inline float GetSampleOsc1();
	inline float GetSampleOsc2();
	
	int AmpEnvStage;
	int NoteCutTime;

private:
	void ActiveVibrato(int depth,float speed);
	void DisableVibrato();
	void DoGlide();
	void Vibrate();
	inline float GetEnvAmp();
	inline void GetEnvVcf();
	inline void ArpTick(void);
	inline void FilterTick(void);

	static const signed char ArpNote[16][16];
	filter m_filter;
	SYNPAR *syntp;
	//Ok, the explanation of all these:
	//Samplerate, self evident.
	//srCorrection ratio between 44100 and the samplerate.
	//iwavetableSize and fwavetablesize are the amount of samples (in int and float) of one period of the wavetable.
	//wavetableCorrection is just a small correction to fix the period so that its size is integer and even.
	int sampleRate;
	float srCorrection;
	int iwaveTableSize;
	float fwaveTableSize;
	float wavetableCorrection;
	dsp::cubic_resampler resampler;

	int sp_cmd;
	int sp_val;

	// Envelope [Amplitude]
	float AmpEnvValue;
	float AmpEnvCoef;
	float AmpEnvSustainLevel;
	float Stage5AmpVal;
	bool vibrato;
	float OSC1Vol;
	float OSC2Vol;

	// Envelope [Filter]
	int VcfEnvStage;
	float VcfEnvValue;
	float VcfEnvCoef;
	float VcfEnvSustainLevel;

	float VcfEnvMod;
	float VcfCutoff;
	float VcfResonance;

	//LFO
	float lfo_freq;
	float lfo_phase;
	short timetocompute;


	//Oscillator. Target and current speed.
	float OSC1Speed;
	float OSC2Speed;
	float ROSC1Speed;
	float ROSC2Speed;
	
	float OSC1Position;
	float OSC2Position;
	
	float oscglide;

	//Vibrato parameters and value
	float VibratoGr;
	float VibratoSpeed;
	float VibratoDepth;
	float OSCvib;

	// Arpeggiator
public:
	int ArpMode;
protected:
	int Arp_tickcounter;
	int Arp_samplespertick;
	float Arp_basenote;
	unsigned char ArpCounter;
	//unsigned char ArpLimit;


};

inline void CSynthTrack::ArpTick()
{
	Arp_tickcounter=0;

	float note=Arp_basenote+(float)ArpNote[ArpMode-1][ArpCounter];
	OSC1Speed=(float)pow(2.0, note/12.0)*wavetableCorrection;

	float note2=note+
	syntp->osc2finetune+
	syntp->osc2detune;
	OSC2Speed=(float)pow(2.0, note2/12.0)*wavetableCorrection;

	if(++ArpCounter>=syntp->arp_cnt)  ArpCounter=0;

	if (AmpEnvStage<4) {
		InitEnvelopes(true);
	}
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
	float output;

	if(AmpEnvStage)
	{
		if ((ArpMode>0) && (++Arp_tickcounter>Arp_samplespertick)) ArpTick();
	
		if ( syntp->interpolate)  // helper's interpolation method
		{
			if ( syntp->wave1noise) {
				//This assumes MAX_RAND is 0x7fff
				output = (std::rand() - 16384)*OSC1Vol;
			} else {
                output = resampler.work_float(syntp->pWave, OSC1Position, iwaveTableSize,NULL, syntp->pWave, syntp->pWave+iwaveTableSize-1)*OSC1Vol;
			}
			if ( syntp->wave2noise) {
				//This assumes MAX_RAND is 0x7fff
				output += (std::rand() - 16384)*OSC2Vol;
			}
			else {
                output += resampler.work_float(syntp->pWave2, OSC2Position, iwaveTableSize,NULL, syntp->pWave2, syntp->pWave2+iwaveTableSize-1)*OSC2Vol;
			}
		}
		else
		{
			if ( syntp->wave1noise) {
				//This assumes MAX_RAND is 0x7fff
				output = (std::rand() - 16384)*OSC1Vol;
			} else {
				output=syntp->pWave[math::rint<int,float>(OSC1Position)]*OSC1Vol;
			}
			if ( syntp->wave2noise) {
				//This assumes MAX_RAND is 0x7fff
				output += (std::rand() - 16384)*OSC2Vol;
			}
			else {
				output += syntp->pWave2[math::rint<int,float>(OSC2Position)]*OSC2Vol;
			}
		}

		if(vibrato)
		{
			OSC1Position+=ROSC1Speed+OSCvib;
			OSC2Position+=ROSC2Speed+OSCvib;
		}
		else
		{
			OSC1Position+=ROSC1Speed;
			OSC2Position+=ROSC2Speed;
		}
		
		if(OSC1Position>=fwaveTableSize)
		{
			OSC1Position-=fwaveTableSize;
		
			if(syntp->osc2sync)
				OSC2Position=OSC1Position;
		}

		if(OSC2Position>=fwaveTableSize) OSC2Position-=fwaveTableSize;

		GetEnvVcf();

		if(!timetocompute--) FilterTick();
		
		if ( syntp->vcf_type > 9 ) return m_filter.res2(output)*GetEnvAmp();
		else return m_filter.res(output)*GetEnvAmp();
	}
	else return 0;
}

inline float CSynthTrack::GetSampleOsc1()
{
	float output;

	if(AmpEnvStage)
	{
		if ((ArpMode>0) && (++Arp_tickcounter>Arp_samplespertick)) ArpTick();
	
		if ( syntp->wave1noise) {
			//This assumes MAX_RAND is 0x7fff
			output = (std::rand() - 16384)*OSC1Vol;
		} 
		else  if ( syntp->interpolate) { // helper's interpolation method
            output = resampler.work_float(syntp->pWave, OSC1Position, iwaveTableSize,NULL, syntp->pWave, syntp->pWave+iwaveTableSize-1)*OSC1Vol;
		}
		else { output=syntp->pWave[math::rint<int,float>(OSC1Position)]*OSC1Vol; }

		if(vibrato) OSC1Position+=ROSC1Speed+OSCvib;
		else        OSC1Position+=ROSC1Speed;
		
		if(OSC1Position>=fwaveTableSize)  OSC1Position-=fwaveTableSize;
		
		GetEnvVcf();

		if(!timetocompute--) FilterTick();

		if ( syntp->vcf_type > 9 ) return m_filter.res2(output)*GetEnvAmp();
		else return m_filter.res(output)*GetEnvAmp();
	}
	else return 0;
}
inline float CSynthTrack::GetSampleOsc2()
{
	float output;

	if(AmpEnvStage)
	{
		if ((ArpMode>0) && (++Arp_tickcounter>Arp_samplespertick)) ArpTick();
	
		if ( syntp->wave2noise) {
			//This assumes MAX_RAND is 0x7fff
			output = (std::rand() - 16384)*OSC2Vol;
		}
		else if ( syntp->interpolate) { // helper's interpolation method
           output = resampler.work_float(syntp->pWave2, OSC2Position, iwaveTableSize,NULL, syntp->pWave2, syntp->pWave2+iwaveTableSize-1)*OSC2Vol;
		}
		else { output = syntp->pWave2[math::rint<int,float>(OSC2Position)]*OSC2Vol; }

		if(vibrato) OSC2Position+=ROSC2Speed+OSCvib;
		else        OSC2Position+=ROSC2Speed;
		
		if(OSC2Position>=fwaveTableSize) OSC2Position-=fwaveTableSize;
		
		GetEnvVcf();

		if(!timetocompute--) FilterTick();

		if ( syntp->vcf_type > 9 ) return m_filter.res2(output)*GetEnvAmp();
		else return m_filter.res(output)*GetEnvAmp();
	}
	else  return 0;
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
	break;

	case 2: // Decay
		AmpEnvValue-=AmpEnvCoef;
		
		if(AmpEnvValue<AmpEnvSustainLevel)
		{
			if(AmpEnvSustainLevel == 0.0f) {
				AmpEnvValue=0.0f;
				AmpEnvStage=0;
				if (oscglide == 0.0f) {
					ROSC1Speed=0.0f;
					ROSC2Speed=0.0f;
				}
			}
			else {
				AmpEnvValue=AmpEnvSustainLevel;
				AmpEnvStage=3;
			}
		}
	break;

	case 4: // Release
		AmpEnvValue-=AmpEnvCoef;

		if(AmpEnvValue<0.0f)
		{
			AmpEnvValue=0.0f;
			AmpEnvStage=0;
			if (oscglide == 0.0f) {
				ROSC1Speed=0.0f;
				ROSC2Speed=0.0f;
			}
		}
	break;
	
	case 5: // FastRelease
		AmpEnvValue-=AmpEnvCoef;
		Stage5AmpVal+=1.0f/syntp->amp_env_attack;

		if(AmpEnvValue<Stage5AmpVal)
		{
			AmpEnvValue=Stage5AmpVal;
			if (oscglide == 0.0f) {
				ROSC1Speed = OSC1Speed;
				ROSC2Speed = OSC2Speed;
			}
			AmpEnvStage=1;
			AmpEnvCoef=1.0f/syntp->amp_env_attack;
			VcfEnvStage=1;
			VcfEnvCoef=1.0f/syntp->vcf_env_attack;
		}
	break;

	default:
		break;
	}

	return AmpEnvValue;
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
			
			if(!syntp->vcf_env_sustain) VcfEnvStage=0;
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
}