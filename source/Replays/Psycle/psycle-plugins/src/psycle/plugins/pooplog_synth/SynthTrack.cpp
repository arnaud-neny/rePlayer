// SynthTrack.cpp: implementation of the CSynthTrack class.
//
//////////////////////////////////////////////////////////////////////
#include <cmath>
#include "SynthTrack.h"
#include <psycle/plugin_interface.hpp>
#ifdef SYNTH_LIGHT
#ifdef SYNTH_ULTRALIGHT
namespace psycle::plugins::pooplog_synth_ultralight {
#else
namespace psycle::plugins::pooplog_synth_light {
#endif
#else
namespace psycle::plugins::pooplog_synth {
#endif
    //////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSynthTrack::CSynthTrack()
{
	NoteCutTime=0;
	NoteCut=false;
	timetocompute=0;
	note_delay = 0;
	note_delay_counter = 0;
	note = 0;
	TrackerArpeggio[0]=0;
	TrackerArpeggio[1]=0;
	TrackerArpeggio[2]=0;
	TrackerArpeggioIndex = 0;
	TrackerArpeggioCounter = 0;
	TrackerArpeggioRate = 3;
	TFXVol = 1;
#ifndef SYNTH_LIGHT
	tremolo_phase = 0;
	TremoloVol = 1.0f;
	arpflag = 0;
	ArpCounter=0;
	Arp_tickcounter=0;
	Arp_samplespertick=0;
	TremoloEnvValue = 0;
	TremoloEnvCoef = 0;
	TremoloEnvStage = 0;
#endif
#ifndef SYNTH_ULTRALIGHT
	gain_lfo_phase = 0;
	GainEnvStage=-1;
	GainEnvValue=0.0f;
	GainEnvDValue=0.0f;
#endif

	VibratoEnvValue = 0;
	VibratoEnvCoef = 0;
	VibratoEnvStage = 0;

	vibrato_phase = 0;
	PortaCoef = 0;

	Arp_basenote=0;
	Arp_destnote=0;
	LMix = 16384.0f;
	RMix = 16384.0f;
	OutPan = 0x80;

	sp_cmd=0;
	sp_val=0;

	VibratoAmplitude = 0;
	VibratoSpeed = 0;
	AmpEnvStage=0;
	AmpEnvValue=0.0f;

}

CSynthTrack::~CSynthTrack()
{

}

void CSynthTrack::Init(SYNPAR *tspar)
{
	unsigned int i;
	syntp=tspar;
	for (i = 0; i < MAXVCF; i++)
	{
		lVcfv[i].vcflfophase=0;
		lVcfv[i].VcfEnvStage=-1;
		lVcfv[i].VcfEnvValue=0.0f;
		lVcfv[i].VcfEnvDValue=0.0f;
		m_filter[i].init(song_freq);
	}

	for (i = 0; i < MAXOSC; i++)
	{
#ifndef SYNTH_LIGHT
		lOscv[i].oscplfophase = 0;
		lOscv[i].oscwlfophase = 0;
		lOscv[i].oscflfophase = 0;
		lOscv[i].OscpEnvStage=-1;
		lOscv[i].OscpEnvValue=0.0f;
		lOscv[i].OscpEnvDValue=0.0f;
		lOscv[i].OscfEnvStage=-1;
		lOscv[i].OscfEnvValue=0.0f;
		lOscv[i].OscfEnvDValue=0.0f;
		lOscv[i].OscwEnvStage=-1;
		lOscv[i].OscwEnvValue=0.0f;
		lOscv[i].OscwEnvDValue=0.0f;
		lOscv[i].oscphase = 0;
#endif

#ifndef SYNTH_ULTRALIGHT
		lOscv[i].OSCVol[0]=0;
		lOscv[i].OSCVol[1]=0;
		lOscv[i].oscdir = 0;
		lOscv[i].OSCSpeed[0]=0.0f;
		lOscv[i].OSCSpeed[1]=0.0f;
		lOscv[i].Last[0] = 0;
		lOscv[i].Last[1] = 0;
		lOscv[i].rand[0] = 156;
		lOscv[i].rand[1] = 3156;
#else
		lOscv[i].OSCVol=0;
		lOscv[i].OSCSpeed=0.0f;
		lOscv[i].Last = 0;
		lOscv[i].rand = 156;
#endif
		lOscv[i].OSCPosition=0.0f;
	}

}

void CSynthTrack::NoteOn(int note, int cmd, int val)
{

	/*
	Arp_destnote=(float)note
				-24
				+7.415f
				+(float)tspar->globalfinetune*0.0038962f
				+(float)tspar->globaltune;
	*/

	// calculated offset, we will test tuning

	Arp_destnote=(float)note
				-16.51820899f;

	/*
	Arp_basenote=(float)note+
				(log((440.0f)*4096.0f/(44100.0f*12.0f))/(log(2)))
				+4.0f
				+(float)tspar->globalfinetune*0.0038962f
				+(float)tspar->globaldetune;
				*/
#ifndef SYNTH_LIGHT
	if (syntp->arp_mod)
	{
		Arp_tickcounter=0;
		if (syntp->arp_bpm > MAXSYNCMODES)
		{
			Arp_samplespertick = syntp->arp_bpm*(SAMPLE_LENGTH*2*4.0f*(FILTER_CALC_TIME/(44100.0f*60.0f))); // 1/4 note
		}
		ArpCounter=0;
		arpflag = 1;
	}
#endif

	if (cmd != TFX_Portamento) 
	{
		if (syntp->synthporta)
		{
			PortaCoef=freq_mul*(Arp_destnote-Arp_basenote)/(float)syntp->synthporta;
		}
		else
		{
			Arp_basenote=Arp_destnote;
			PortaCoef=0.0f;
			// reset waveform phases
			if (!AmpEnvValue)
			{
				for (unsigned int i = 0; i < MAXOSC; i++)
				{
					lOscv[i].OSCPosition = 0;
#ifndef SYNTH_ULTRALIGHT
					lOscv[i].oscdir = 0;
#endif
				}
			}
		}
		if (cmd != TFX_BypassEnv)
		{
			InitEnvelopes();
		}
	}
	else if (cmd == TFX_Portamento)
	{

		val = 256-val;
	// Init porta
		PortaCoef=freq_mul*(Arp_destnote-Arp_basenote)/(float)(val*(MAX_ENV_TIME/2048));
	}

}

void CSynthTrack::InitEnvelopes()
{
	unsigned int i;
#ifndef SYNTH_LIGHT
	for (i = 0; i < MAXOSC; i++)
	{
		if (syntp->gOscp[i].oscvol[0])
		{
			OSCVALS *pV= &lOscv[i];
			OSCPAR *pG= &syntp->gOscp[i];

			pV->OscpEnvSustainLevel=(float)pG->oscpsustain*0.0039062f;
			pV->OscwEnvSustainLevel=(float)pG->oscwsustain*0.0039062f;
			pV->OscfEnvSustainLevel=(float)pG->oscfsustain*0.0039062f;

			// Init osc1 Envelopes
			if (pG->oscwdelay)
			{
				pV->OscwEnvDValue = 0;
				pV->OscwEnvStage=0;
				pV->OscwEnvCoef=freq_mul/(float)pG->oscwdelay;
			}
			else if (pG->oscwattack)
			{
				pV->OscwEnvStage=1;
				pV->OscwEnvCoef=freq_mul*(1.0f-pV->OscwEnvValue)/(float)pG->oscwattack;
			}
			else if (pG->oscwdecay)
			{
				pV->OscwEnvValue = 1.0f;
				pV->OscwEnvStage=2;
				pV->OscwEnvCoef=freq_mul/(float)pG->oscwdecay;
			}
			else if (pG->oscwsustain)
			{
				pV->OscwEnvValue = pV->OscwEnvSustainLevel;
				pV->OscwEnvStage=3;
			}
			else
			{
				pV->OscwEnvStage=-1;
				pV->OscwEnvCoef=0.0f;
			}
			if (pG->oscpdelay)
			{
				pV->OscpEnvDValue = 0;
				pV->OscpEnvStage=0;
				pV->OscpEnvCoef=freq_mul/(float)pG->oscpdelay;
			}
			else if (pG->oscpattack)
			{
				pV->OscpEnvStage=1;
				pV->OscpEnvCoef=freq_mul*(1.0f-pV->OscpEnvValue)/(float)pG->oscpattack;
			}
			else if (pG->oscpdecay)
			{
				pV->OscpEnvValue = 1.0f;
				pV->OscpEnvStage=2;
				pV->OscpEnvCoef=freq_mul/(float)pG->oscpdecay;
			}
			else if (pG->oscpsustain)
			{
				pV->OscpEnvValue = pV->OscpEnvSustainLevel;
				pV->OscpEnvStage=3;
			}
			else
			{
				pV->OscpEnvStage=-1;
				pV->OscpEnvCoef=0.0f;
			}
			if (pG->oscfdelay)
			{
				pV->OscfEnvDValue = 0;
				pV->OscfEnvStage=0;
				pV->OscfEnvCoef=freq_mul/(float)pG->oscfdelay;
			}
			else if (pG->oscfattack)
			{
				pV->OscfEnvStage=1;
				pV->OscfEnvCoef=freq_mul*(1.0f-pV->OscfEnvValue)/(float)pG->oscfattack;
			}
			else if (pG->oscfdecay)
			{
				pV->OscfEnvValue = 1.0f;
				pV->OscfEnvStage=2;
				pV->OscfEnvCoef=freq_mul/(float)pG->oscfdecay;
			}
			else if (pG->oscfsustain)
			{
				pV->OscfEnvValue = pV->OscfEnvSustainLevel;
				pV->OscfEnvStage=3;
			}
			else
			{
				pV->OscfEnvStage=-1;
				pV->OscfEnvCoef=0.0f;
			}
		}
	}
	if (tremolo_delay)
	{
		TremoloEnvValue = 0;
		TremoloEnvStage=0;
		TremoloEnvCoef=freq_mul/(float)tremolo_delay;
	}
	else
	{
		TremoloEnvStage = 2;
	}
#endif
	if (vibrato_delay)
	{
		VibratoEnvValue = 0;
		VibratoEnvStage=0;
		VibratoEnvCoef=freq_mul/(float)vibrato_delay;
	}
	else
	{
		VibratoEnvStage = 2;
	}
	for (i = 0; i < MAXVCF; i++)
	{
		VCFVALS *pV= &lVcfv[i];
		VCFPAR *pG= &syntp->gVcfp[i];

		pV->VcfCutoff=(float)pG->vcfcutoff;
		pV->VcfEnvSustainLevel=pG->vcfenvsustain*0.0039062f;
		// Init Filter 1Envelope
		if (pG->vcfenvdelay)
		{
			pV->VcfEnvDValue = 0;
			pV->VcfEnvStage=0;
			pV->VcfEnvCoef=freq_mul/pG->vcfenvdelay;
		}
		else if (pG->vcfenvattack)
		{
			pV->VcfEnvStage=1;
			pV->VcfEnvCoef=freq_mul*(1.0f-pV->VcfEnvValue)/(float)pG->vcfenvattack;
		}
		else if (pG->vcfenvdecay)
		{
			pV->VcfEnvValue = 1.0f;
			pV->VcfEnvStage=2;
			pV->VcfEnvCoef=freq_mul/(float)pG->vcfenvdecay;
		}
		else if (pG->vcfenvsustain)
		{
			pV->VcfEnvStage=3;
			pV->VcfEnvValue = pV->VcfEnvSustainLevel;
		}
		else
		{
			pV->VcfEnvStage=-1;
			pV->VcfEnvValue = 0;
		}
	}

	// Init Amplitude Envelope
	AmpEnvSustainLevel=(float)syntp->amp_env_sustain*0.0039062f;

	if (syntp->amp_env_attack)
	{
//												AmpEnvValue = 0;
		AmpEnvStage=1;
		AmpEnvCoef=freq_mul*(1.0f-AmpEnvValue)/(float)(syntp->amp_env_attack*FILTER_CALC_TIME);
	}
	else if (syntp->amp_env_decay)
	{
		AmpEnvValue = 1.0f;
		AmpEnvStage=2;
//								AmpEnvCoef=AmpEnvValue/(float)syntp->amp_env_decay;
		AmpEnvCoef=freq_mul/(float)(syntp->amp_env_decay*FILTER_CALC_TIME);
	}
	else if (syntp->amp_env_sustain)
	{
		AmpEnvValue = AmpEnvSustainLevel;
		AmpEnvStage=3;
	}
	else
	{
		AmpEnvStage=0;
		AmpEnvCoef=0.0f;
	}

#ifndef SYNTH_ULTRALIGHT
	GainEnvSustainLevel=(float)syntp->gain_env_sustain*0.0039062f;
	// Init Gain Envelope
	if (syntp->gain_env_delay)
	{
		GainEnvDValue = 0;
		GainEnvStage=0;
		GainEnvCoef=freq_mul/(float)syntp->gain_env_delay;
	}
	else if (syntp->gain_env_attack)
	{
		GainEnvStage=1;
		GainEnvCoef=freq_mul*(1.0f-GainEnvValue)/(float)syntp->gain_env_attack;
	}
	else if (syntp->gain_env_decay)
	{
		GainEnvValue = 1.0f;
		GainEnvStage=2;
		GainEnvCoef=freq_mul/(float)syntp->gain_env_decay;
	}
	else if (syntp->gain_env_sustain)
	{
		GainEnvStage=3;
		GainEnvValue = GainEnvSustainLevel;
	}
	else
	{
		GainEnvStage=-1;
		GainEnvValue = 0;
	}
#endif
}

void CSynthTrack::NoteOff()
{
	if(AmpEnvStage)
	{
		unsigned int i;
#ifndef SYNTH_LIGHT
		arpflag = 0;
		for (i = 0; i < MAXOSC; i++)
		{
			if (syntp->gOscp[i].oscvol[0])
			{
				OSCVALS *pV= &lOscv[i];
				OSCPAR *pG= &syntp->gOscp[i];
				pV->OscpEnvStage=4;
				pV->OscwEnvStage=4;
				pV->OscfEnvStage=4;
				pV->OscpEnvCoef=freq_mul*pV->OscpEnvValue/(float)pG->oscprelease;
				pV->OscwEnvCoef=freq_mul*pV->OscwEnvValue/(float)pG->oscwrelease;
				pV->OscfEnvCoef=freq_mul*pV->OscfEnvValue/(float)pG->oscfrelease;
				if(pV->OscpEnvCoef<0.00001f)pV->OscpEnvCoef=0.00001f;
				if(pV->OscwEnvCoef<0.00001f)pV->OscwEnvCoef=0.00001f;
				if(pV->OscfEnvCoef<0.00001f)pV->OscfEnvCoef=0.00001f;
			}
		}
#endif
		for (i = 0; i < MAXVCF; i++)
		{
			VCFVALS *pV= &lVcfv[i];
			pV->VcfEnvStage=4;
			pV->VcfEnvCoef=freq_mul*pV->VcfEnvValue/(float)syntp->gVcfp[i].vcfenvrelease;
			if(pV->VcfEnvCoef<0.00001f) pV->VcfEnvCoef=0.00001f;
		}
		AmpEnvStage=4;
		AmpEnvCoef=freq_mul*AmpEnvValue/(float)(syntp->amp_env_release*FILTER_CALC_TIME);
		if(AmpEnvCoef<0.00001f)AmpEnvCoef=0.00001f;
		if(!syntp->amp_env_sustain) AmpEnvStage=0;
#ifndef SYNTH_ULTRALIGHT
		GainEnvStage=4;
		GainEnvCoef=freq_mul*GainEnvValue/(float)syntp->gain_env_release;
		if(GainEnvCoef<0.00001f)GainEnvCoef=0.00001f;
#endif
	}
	if ((sp_cmd == TFX_NoteDelay) || (sp_cmd == TFX_NoteRetrig))
	{
		sp_cmd = 0;
		note_delay=0;
	}
}

void CSynthTrack::PerformFx()
{
	for (unsigned int i = 0; i < MAXOSC; i++)
	{
#ifndef SYNTH_ULTRALIGHT
		if (syntp->gOscp[i].oscvol[0])
		{
			OSCVALS *pV= &lOscv[i];
			OSCPAR *pG= &syntp->gOscp[i];

			pV->OSCVol[0]=(float)pG->oscvol[0]*0.0039062f;
			if (syntp->gOscp[i].oscvol[1])
			{
				pV->OSCVol[1]=(float)pG->oscvol[1]*0.0039062f;
			}
			else
			{
				pV->OSCVol[1]=pV->OSCVol[0];
			}
		}
#else
		if (syntp->gOscp[i].oscvol)
		{
			lOscv[i].OSCVol=(float)syntp->gOscp[i].oscvol*0.0039062f;
		}
#endif
	}

	switch(sp_cmd)
	{
	case TFX_NoteDelay:
		note_delay_counter++;
		if (note_delay_counter >=note_delay)
		{
			if (note<=psycle::plugin_interface::NOTE_MAX)
			{
				NoteOn(note,0,0);
			}
			else
			{
				NoteOff();
			}
			sp_cmd = 0;
		}
		break;
	case TFX_NoteRetrig:
		note_delay_counter++;
		if (note_delay_counter >=note_delay)
		{
			if (note<=psycle::plugin_interface::NOTE_MAX)
			{
				NoteOn(note,0,0);
			}
			else
			{
				NoteOff();
				sp_cmd = 0;
			}
			note_delay_counter = 0;
		}
		break;
	case TFX_TrackerArpeggio:
		TrackerArpeggioCounter++;
		if (TrackerArpeggioCounter > TrackerArpeggioRate)
		{
			TrackerArpeggioCounter = 0;
			TrackerArpeggioIndex++;
			TrackerArpeggioIndex%=3;
		}
		break;
	/* 0x01 : Pitch Up */
	case TFX_PitchUp:
		Arp_basenote +=(float)sp_val*0.001f;
		return;
		break;

	/* 0x02 : Pitch Down */
	case TFX_PitchDown:
		Arp_basenote -=(float)sp_val*0.001f;
		return;
		break;
		// vol up
	case TFX_VolUp: 
		TFXVol+=(float)sp_val*0.0001f;
		if (TFXVol > 1.0f)
		{
			TFXVol = 1.0f;
		}
		return;
		break;
		// vol down
	case TFX_VolDown:
		TFXVol-=(float)sp_val*0.0001f;
		if (TFXVol < 0.0f)
		{
			TFXVol = 0.0f;
		}
		return;
		break;

		// vol fade to dest
	case TFX_VolDest:
		if (TFXVol < (sp_val&0xf0)/240.0f)
		{
			TFXVol+=(float)((sp_val&0x0f)+1)*0.00125f;
			if (TFXVol > (sp_val&0xf0)/240.0f)
			{
				TFXVol = (float)(sp_val&0xf0)/240.0f;
				sp_cmd = TFX_Cancel;
			}
			return;
		}
		else
		if (TFXVol > (sp_val&0xf0)/240.0f)
		{
			TFXVol-=(float)((sp_val&0x0f)+1)*0.00125f;
			if (TFXVol < (sp_val&0xf0)/240.0f)
			{
				TFXVol = (float)(sp_val&0xf0)/240.0f;
				sp_cmd = TFX_Cancel;
			}
			return;
		}
		break;

		// pan left
	case TFX_PanLeft:
		OutPan-=(float)sp_val*0.03125f;
		if (OutPan < 1)
		{
			OutPan = 1.0f;
			sp_cmd = TFX_Cancel;
			return;
		}
		if (OutPan < 0x80)
		{
			LMix=16384.0f;
			RMix=(float)(OutPan-1)*0.007874016f;
			RMix*=RMix*16384;
			return;
		}
		else
		{
			LMix=(float)(0xff-OutPan)*0.007874016f;
			LMix*=LMix*16384;
			RMix=16384.0f;
			return;
		}
		break;
		// pan right
	case TFX_PanRight:
		OutPan+=(float)sp_val*0.03125f;
		if (OutPan > 255)
		{
			OutPan = 255.0f;
			sp_cmd = TFX_Cancel;
			return;
		}
		if (OutPan < 0x80)
		{
			LMix=16384.0f;
			RMix=(float)(OutPan-1)*0.007874016f;
			RMix*=RMix*16384.0f;
			return;
		}
		else
		{
			LMix=(float)(0xff-OutPan)*0.007874016f;
			LMix*=LMix*16384;
			RMix=16384.0f;
			return;
		}
		break;
		// pan to dest
	case TFX_PanDest:
		if (OutPan < (sp_val&0xf0))
		{
			OutPan+=(float)((sp_val&0x0f)+1)*0.125f;
			if (OutPan > (sp_val&0xf0))
			{
				OutPan = (float)(sp_val&0xf0);
				sp_cmd = TFX_Cancel;
				return;
			}
		}
		else
		{
			OutPan-=(float)((sp_val&0x0f)+1)*0.125f;
			if (OutPan < (sp_val&0xf0))
			{
				OutPan = (float)(sp_val&0xf0);
				sp_cmd = TFX_Cancel;
				return;
			}
		}
		if (OutPan < 0x80)
		{
			LMix=16384.0f;
			RMix=(float)(OutPan-1)*0.007874016f;
			RMix*=RMix*16384;
			return;
		}
		else
		{
			LMix=(float)(0xff-OutPan)*0.007874016f;
			LMix*=LMix*16384;
			RMix=16384.0f;
			return;
		}
		break;


	/*  Note Cut */
	case TFX_NoteCut:
		if(NoteCutTime<=0)
		{
			sp_cmd = 0;
			NoteCut=false;
			NoteOff();
		}
		return;
		break;
	}

	unsigned int vcf = (sp_cmd&0x0c)>>2;
	if (vcf < MAXVCF)
	{
		VCFVALS *pV= &lVcfv[vcf];
		switch (sp_cmd & 0xf3)
		{
		// vcf1 CutOff Up 
		case TFX_CutoffUp:
			{
				pV->VcfCutoff+=sp_val*0.03125f;
				if(pV->VcfCutoff>255)pV->VcfCutoff=255;
				return;
			}
			break;

		// vcf1 CutOff Down
		case TFX_CutoffDown:
			pV->VcfCutoff-=sp_val*0.03125f;
			if(pV->VcfCutoff<0)pV->VcfCutoff=0;
			return;
			break;

		case TFX_CutoffDest:
			if (pV->VcfCutoff < (sp_val&0xf0))
			{
				pV->VcfCutoff+=(float)((sp_val&0x0f)+1)*0.125f;
				if (pV->VcfCutoff > (sp_val&0xf0))
				{
					pV->VcfCutoff = (float)(sp_val&0xf0);
					sp_cmd = TFX_Cancel;
				}
				return;
			}
			else
			{
				pV->VcfCutoff-=(float)((sp_val&0x0f)+1)*0.125f;
				if (pV->VcfCutoff < (sp_val&0xf0))
				{
					pV->VcfCutoff = (float)(sp_val&0xf0);
					sp_cmd = TFX_Cancel;
				}
				return;
			}
			break;
		}
	}
}

void CSynthTrack::InitEffect(int cmd, int val)
{
	sp_cmd=cmd;
	sp_val=val;

	if (cmd==TFX_TrackerArpeggio)
	{
		TrackerArpeggio[1]=(val>>4)&0x0f;
		TrackerArpeggio[2]=val&0x0f;
		return;
	}
	else
	{
		TrackerArpeggioIndex = 0;
	}

	// Init vibrato
	if (cmd==TFX_Vibrato)				
	{
		VibratoAmplitude = float((val&0xf0)+1)*2;
		VibratoSpeed = ((val&0x0f)+1) * (MAX_RATE/32.0f);
		return;
	}
	else
	{
		VibratoAmplitude = 0;
	}
	// Note CUT
	if (cmd==TFX_NoteCut)
	{
		if (val>0)
		{
			NoteCutTime=val*32;
			NoteCut=true;
			return;
		}
		else 
		{
			sp_cmd = 0;
			NoteCut=false;
			NoteOff();
		}
	}
	else
	{
		NoteCut=false;
	}

	// do phase syncs

	switch (cmd)
	{
	case TFX_Vol:
		TFXVol=(float)val/255.0f;
		return;
		break;

	case  TFX_Pan: // change pan
		OutPan=(float)val;
		if (OutPan < 1)
		{
			OutPan = 1;
		}
		if (OutPan < 0x80)
		{
			LMix=16384.0f;
			RMix=(float)(OutPan-1)*0.007874016f;
			RMix*=RMix*16384;
			return;
		}
		else
		{
			LMix=(float)(0xff-OutPan)*0.007874016f;
			LMix*=LMix*16384;
			RMix=16384.0f;
			return;
		}
		break;
#ifndef SYNTH_ULTRALIGHT
	case TFX_Gain_LFOPhase:
		gain_lfo_phase = val*SAMPLE_LENGTH/128.0f;
		return;
		break;
#endif
#ifndef SYNTH_LIGHT
	case TFX_Tremolo_LFOPhase:
		tremolo_phase = val*SAMPLE_LENGTH/128.0f;
		return;
		break;
#endif
	case TFX_Vibrato_LFOPhase:
		vibrato_phase = val*SAMPLE_LENGTH/128.0f;
		return;
		break;
	case TFX_TrackerArpeggioRate:
		TrackerArpeggioRate = val;
		return;
		break;
	}

	switch (cmd&0xf0)
	{
	case TFX_OSC_WavePhase:
		{
			int osc = (cmd&0x0f) - 1;
			if (osc < 0)
			{
				for (unsigned int i = 0; i < MAXOSC; i++)
				{
#ifndef SYNTH_ULTRALIGHT
					lOscv[i].OSCPosition = (val&0x7f)*SAMPLE_LENGTH/128.0f;
					lOscv[i].oscdir = (val&0x80) ? 1 : 0;
#else
					lOscv[i].OSCPosition = val*SAMPLE_LENGTH/128.0f;
#endif
				}
			}
			else if (osc < MAXOSC)
			{
#ifndef SYNTH_ULTRALIGHT
				lOscv[osc].OSCPosition = (val&0x7f)*SAMPLE_LENGTH/128.0f;
				lOscv[osc].oscdir = (val&0x80) ? 1 : 0;
#else
				lOscv[osc].OSCPosition = val*SAMPLE_LENGTH/128.0f;
#endif
			}
			return;
		}
		break;
#ifndef SYNTH_LIGHT
	case TFX_OSC_WidthLFOPhase:
		{
			int osc = (cmd&0x0f) - 1;
			if (osc < 0)
			{
				for (unsigned int i = 0; i < MAXOSC; i++)
				{
					lOscv[i].oscwlfophase = val*SAMPLE_LENGTH/128.0f;
				}
			}
			else if (osc < MAXOSC)
			{
				lOscv[osc].oscwlfophase = val*SAMPLE_LENGTH/128.0f;
			}
			return;
		}
		break;
	case TFX_OSC_PhaseLFOPhase:
		{
			int osc = (cmd&0x0f) - 1;
			if (osc < 0)
			{
				for (unsigned int i = 0; i < MAXOSC; i++)
				{
					lOscv[i].oscplfophase = val*SAMPLE_LENGTH/128.0f;
				}
			}
			else if (osc < MAXOSC)
			{
				lOscv[osc].oscplfophase = val*SAMPLE_LENGTH/128.0f;
			}
			return;
		}
		break;
	case TFX_OSC_FrqLFOPhase:
		{
			int osc = (cmd&0x0f) - 1;
			if (osc < 0)
			{
				for (unsigned int i = 0; i < MAXOSC; i++)
				{
					lOscv[i].oscflfophase = val*SAMPLE_LENGTH/128.0f;
				}
			}
			else if (osc < MAXOSC)
			{
				lOscv[osc].oscflfophase = val*SAMPLE_LENGTH/128.0f;
			}
			return;
		}
		break;
#endif
	case TFX_VCF_LFOPhase:
		{
			int vcf = (cmd&0x0f) - 1;
			if (vcf < 0)
			{
				for (unsigned int i = 0; i < MAXVCF; i++)
				{
					lVcfv[i].vcflfophase = val*SAMPLE_LENGTH/128.0f;
				}
			}
			else if (vcf < MAXVCF)
			{
				lVcfv[vcf].vcflfophase = val*SAMPLE_LENGTH/128.0f;
			}
			return;
		}
		break;
	}

	if ((cmd & 0xf3) == TFX_Cutoff)
	{
		unsigned int vcf = (sp_cmd&0x0c)>>2;
		if (vcf < MAXVCF)
		{
			VCFVALS *pV= &lVcfv[vcf];
			pV->VcfCutoff=float(val);
			return;
		}
	}
}

}