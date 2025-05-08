// CTrack Definition file
#include "track.hpp"
#include <cassert>
namespace psycle::plugins::m3 {
using namespace psycle::plugin_interface;

void CTrack::Stop()
{
	AEGState = EGS_NONE;
}

void CTrack::Init()
{
	_channel = -1;
	Note = 0;
	pwavetab1=pwavetab2=pwavetabsub=pwavetabLFO1=pwavetabLFO2= WaveTable[0];
	SubOscVol = 64;
	noise1 = noise2 = Sync = false;
	Bal1 = 63;
	Bal2 = 64;
	MixType = 0;
	Phase1 = Phase2 = PhaseSub = 0; // Osc starten neu
	Ph1=Ph2=1.0;
	currentcenter1=currentcenter2=64.f;
	Center1 = Center2 = 64/127;
	PhScale1A = PhScale1B = PhScale2A = PhScale2B = 1.0;
	PhaseAdd1=PhaseAdd2=0;
	Frequency = FrequencyFrom = 0.0;
	DetuneSemi = DetuneFine = 1;
	Glide =  GlideActive = false;
	GlideMul = GlideFactor = 0.f;
	GlideTime = GlideCount = 0;
	PitchMod = PitchModActive = false;
	PEGState = EGS_NONE;
	PEGAttackTime = MSToSamples( pmi->EnvTime( 0));
	PEGDecayTime = MSToSamples( pmi->EnvTime( 0));
	PEGCount = PEGAttackTime;
	PitchMul = PitchFactor = 1.f;
	PEnvMod = 0;
	r1=26474; r2=13075; r3=18376; r4=31291; // randomGenerator
	OldOut = 0;
	Volume = (float)(64/245.0);
	AEGState = EGS_NONE;
	AEGAttackTime = MSToSamples( pmi->EnvTime( 10));
	AEGSustainTime = MSToSamples( pmi->EnvTime( 50));
	AEGReleaseTime = MSToSamples( pmi->EnvTime( 30));
	AEGCount = AEGAttackTime;
	Amp=1.f;
	AmpAdd = 0.f;
	coefsTabOffs = coefsTab; // lp
	Cutoff = 127;
	Resonance = 32;
	x1 = x2 = y1 = y2 = 0;
	FEnvMod = 0;
	FEGState = EGS_NONE;
	FEGAttackTime = MSToSamples( pmi->EnvTime( 0));
	FEGSustainTime = MSToSamples( pmi->EnvTime( 0));
	FEGReleaseTime = MSToSamples( pmi->EnvTime( 0));
	FEGCount = 0;
	Cut = 0.f;
	CutAdd = 0.f;
	LFO_Osc1 = LFO_PW1 = LFO_Amp = LFO_Cut = false;				
	LFO_Osc2 = LFO_PW2 = LFO_Mix = LFO_Reso = false;
	LFO1Noise = LFO2Noise = LFO1Synced = LFO2Synced = false;
	LFO1Amount = LFO2Amount = 0;
	PhaseLFO1 = PhaseLFO2 = PhaseAddLFO1 = PhaseAddLFO2 = noisePhase = 0;
	LFO1Freq=LFO2Freq=0;
	pnoise = WaveTable[4];
	RandomMixType = RandomWave1 = RandomWave2 = RandomWaveSub = false;
}

void CTrack::Tick( tvals const &tv)
{
	// Filter
	if( tv.FilterType != 0xff)
	{
		assert(tv.FilterType<4);
		coefsTabOffs = coefsTab + (int)tv.FilterType*128*128*8;
	}
	if( tv.Cutoff != 0xff)
	{
		Cutoff = tv.Cutoff;
	}
	if( tv.Resonance != 0xff)
	{
		Resonance = tv.Resonance;
	}

	// FEG
	if( tv.FEGAttackTime != 0xff)
	{
		FEGAttackTime = MSToSamples( pmi->EnvTime( tv.FEGAttackTime));
	}
	if( tv.FEGSustainTime != 0xff)
	{
		FEGSustainTime = MSToSamples( pmi->EnvTime( tv.FEGSustainTime));
	}
	if( tv.FEGReleaseTime != 0xff)
	{
		FEGReleaseTime = MSToSamples( pmi->EnvTime( tv.FEGReleaseTime));
	}
	if( tv.FEnvMod != 0xff)
	{
		FEnvMod = (tv.FEnvMod - 0x40)<<1;
	}


	// PEG
	if( tv.PEGAttackTime != 0xff)
	{
		PEGAttackTime = MSToSamples( pmi->EnvTime( tv.PEGAttackTime));
	}
	if( tv.PEGDecayTime != 0xff)
	{
		PEGDecayTime = MSToSamples( pmi->EnvTime( tv.PEGDecayTime));
	}
	if( tv.PEnvMod != 0xff)
	{
		if( tv.PEnvMod - 0x40 != 0)
			PitchMod = true;
		else
		{
			PitchMod = false;
			PitchModActive = false;
		}
		PEnvMod = tv.PEnvMod - 0x40;
	}

	if( tv.Mix != 0xff)
	{
		Bal1 = 127-tv.Mix;
		Bal2 = tv.Mix;
	}

	if( tv.Glide != 0xff)
	{
		if( tv.Glide == 0)
		{
			Glide = false;
			GlideActive = false;
		}
		else
		{
			Glide = true;
			GlideTime = tv.Glide*10000000/44100;
		}
	}

	// SubOsc
	if( tv.SubOscWave != 0xff)
	{
		if( tv.SubOscWave == 4) // random
			RandomWaveSub = true;
		else
		{
			assert(tv.SubOscWave < 4);
			pwavetabsub = WaveTable[tv.SubOscWave];
			RandomWaveSub = false;
		}
	}
	if( tv.SubOscVol != 0xff)
	{
		SubOscVol = tv.SubOscVol;
	}

	// PW
	if( tv.PulseWidth1 != 0xff)
	{
		Center1 = tv.PulseWidth1/127.0;
	}
	if( tv.PulseWidth2 != 0xff)
	{
		Center2 = tv.PulseWidth2/127.0;
	}

	// Detune
	if( tv.DetuneSemi != 0xff)
	{
		DetuneSemi = (float)pow( 1.05946309435929526, tv.DetuneSemi-0x40);
	}
	if( tv.DetuneFine != 0xff)
	{
		DetuneFine = (float)pow( 1.00091728179580156, tv.DetuneFine-0x40);
	}
	if( tv.Sync != 0xff)
	{
		Sync = tv.Sync?true:false;
	}

	if( tv.MixType != 0xff)
	{
		if( tv.MixType == 8) // random
			RandomMixType = true;
		else
		{
			MixType = tv.MixType;
			RandomMixType = false;
		}
	}

	if( tv.Wave1 != 0xff) // neuer wert
	{
		RandomWave1 = noise1 = false;

		if( tv.Wave1 == 4) // noise
		{
			pwavetab1 = NULL;
			noise1 = true;
		}
		else if( tv.Wave1 == 5) // random
		{
			RandomWave1 = true;
		}
		else 
		{
			assert(tv.Wave1 < 4 );
			pwavetab1 =  WaveTable[tv.Wave1];
		}

	}

	if( tv.Wave2 != 0xff) // neuer wert
	{
		RandomWave2 = noise2 = false;

		if( tv.Wave2 == 4) // noise
		{
			noise2 = true;
			pwavetab2 = NULL;
		}
		else if( tv.Wave2 == 5) // random
		{
			RandomWave2 = true;
		}
		else 
		{				
			assert(tv.Wave2 < 4 );
			pwavetab2 =  WaveTable[tv.Wave2];
		}

	}

	if( tv.AEGAttackTime != 0xff)
	{
		AEGAttackTime = MSToSamples( pmi->EnvTime( tv.AEGAttackTime));
	}
	if( tv.AEGSustainTime != 0xff)
	{
		AEGSustainTime = MSToSamples( pmi->EnvTime( tv.AEGSustainTime));
	}
	if( tv.AEGReleaseTime != 0xff)
	{
		AEGReleaseTime = MSToSamples( pmi->EnvTime( tv.AEGReleaseTime));
	}
	if( tv.Volume != 0xff)
	{
		Volume = (float)(tv.Volume/245.0);
	}


	if( tv.Note != NOTE_NONE) // neuer wert
	{
		Note = tv.Note;
		if(Note <= NOTE_MAX) // neue note gesetzt
		{
			FrequencyFrom = Frequency;
			Frequency = freqTab[Note];


			// RANDOMS
			if( RandomMixType)
			{
				MixType = (unsigned)pnoise[noisePhase++] % 8;
				noisePhase &= 0x7ff;
			}
			if( RandomWaveSub)
			{
				pwavetabsub = WaveTable[(unsigned)pnoise[noisePhase++] % 4];
				noisePhase &= 0x7ff;
			}
			if( RandomWave1)
			{
				pwavetab1 = WaveTable[(unsigned)pnoise[noisePhase++] % 4];
				noisePhase &= 0x7ff;
			}
			if( RandomWave2)
			{
				pwavetab2 = WaveTable[(unsigned)pnoise[noisePhase++] % 4];
				noisePhase &= 0x7ff;
			}

			if( Glide)
			{
				GlideActive = true;
				if( Frequency > FrequencyFrom)
				{				GlideMul = std::pow(2., 1. / GlideTime); }
				else
				{				GlideMul = std::pow(0.5, 1. / GlideTime); }
				GlideFactor = 1;
				GlideCount = (int)(log( Frequency/FrequencyFrom)/log(GlideMul))* (pmi->currentSR/44100);
			}
			else
			{				GlideActive = false; }

			// trigger envelopes neu an...
			// Amp
			AEGState = EGS_ATTACK;
			AEGCount = AEGAttackTime;
			AmpAdd = Volume/AEGAttackTime;
			Amp = 0; //AmpAdd; // fange bei 0 an
			// Pitch
			if( PitchMod)
			{
				PitchModActive = true;
				PEGState = EGS_ATTACK;
				PEGCount = PEGAttackTime;
				PitchMul = pow( pow( 1.01, PEnvMod), 1.0/PEGAttackTime);
				PitchFactor = 1.0;
			}
			else
			{				PitchModActive = false;				}

			// Filter
			FEGState = EGS_ATTACK;
			FEGCount = FEGAttackTime;
			CutAdd = ((float)FEnvMod)/FEGAttackTime;
			Cut = 0.0; // fange bei 0 an
			NewPhases();

		}
		else if( tv.Note == NOTE_NOTEOFF)
		{
			AEGState = EGS_SUSTAIN; // Prepare Amp Osci to enter in Release mode.
			AEGCount = 0;
		}
	}

	// ..........LFO............

	// LFO1
	if( tv.LFO1Dest != 0xff)
	{
		LFO_Osc1 = LFO_PW1 = LFO_Amp = LFO_Cut = false;
		switch( tv.LFO1Dest)
		{
//		case 0: ...none
		case 1: LFO_Osc1 = true;	break;
		case 2: LFO_PW1 = true;		break;
		case 3: LFO_Amp = true;		break;
		case 4: LFO_Cut = true;		break;

		case 5: // 12
			LFO_Osc1 = true;	LFO_PW1 = true;		break;
		case 6: // 13
			LFO_Osc1 = true;	LFO_Amp = true;		break;
		case 7: // 14
			LFO_Osc1 = true;	LFO_Cut = true;		break;
		case 8: // 23
			LFO_PW1 = true;		LFO_Amp = true;		break;
		case 9: // 24
			LFO_PW1 = true;		LFO_Cut = true;		break;
		case 10: // 34
			LFO_Amp = true;		LFO_Cut = true;		break;

		case 11: // 123
			LFO_Osc1 = true;	LFO_PW1 = true;		LFO_Amp = true;	break;
		case 12: // 124
			LFO_Osc1 = true;	LFO_PW1 = true;		LFO_Cut = true; break;
		case 13: // 134
			LFO_Osc1 = true;	LFO_Amp = true;		LFO_Cut = true; break;
		case 14: // 234
			LFO_PW1 = true;		LFO_Amp = true;		LFO_Cut = true;	break;
		case 15: // 1234
			LFO_Osc1 = true;
			LFO_PW1 = true;
			LFO_Amp = true;
			LFO_Cut = true;
			break;
		}
	}
	if( tv.LFO1Wave != 0xff)
	{
		if( tv.LFO1Wave == 4)
		{				LFO1Noise = true;
		}
		else
		{				LFO1Noise = false;
		}
		assert(tv.LFO1Wave < 5);
		pwavetabLFO1 = WaveTable[tv.LFO1Wave];
	}

	if( tv.LFO1Freq != 0xff)
	{
		if( tv.LFO1Freq>116)
		{
			LFO1Synced = true;
			LFO1Freq = tv.LFO1Freq - 117;
		}
		else
		{
			LFO1Synced = false;
			LFO1Freq = tv.LFO1Freq;
		}
	}
	if( tv.LFO1Amount != 0xff)
	{
		LFO1Amount = tv.LFO1Amount;
	}

	if( LFO1Synced)
	{
		if( LFO1Noise) // sample & hold
		{				PhaseAddLFO1 = (int)(0x200000/(pmi->pCB->GetTickLength()<<LFO1Freq)); }
		else
		{				PhaseAddLFO1 = (int)((double)0x200000*2048/(pmi->pCB->GetTickLength()<<LFO1Freq)); }
	}
	else
	{
		if( LFO1Noise) // sample & hold
		{				PhaseAddLFO1 = (int)(pmi->LFOFreq( LFO1Freq)/pmi->currentSR*0x200000); }
		else
		{				PhaseAddLFO1 = (int)(pmi->LFOFreq( LFO1Freq)*pmi->TabSizeDivSampleFreq*0x200000); }
	}

	// LFO2
	if( tv.LFO2Dest != 0xff)
	{
		LFO_Osc2 = LFO_PW2 = LFO_Mix = LFO_Reso = false;
		switch( tv.LFO2Dest)
		{
//		case 0: ...none
		case 1:	LFO_Osc2 = true;	break;
		case 2: LFO_PW2 = true;		break;
		case 3:	LFO_Mix = true;		break;
		case 4:	LFO_Reso = true;	break;

		case 5: // 12
			LFO_Osc2 = true;	LFO_PW2 = true;		break;
		case 6: // 13
			LFO_Osc2 = true;	LFO_Mix = true;		break;
		case 7: // 14
			LFO_Osc2 = true;	LFO_Reso = true;	break;
		case 8: // 23
			LFO_PW2 = true;		LFO_Mix = true;		break;
		case 9: // 24
			LFO_PW2 = true;		LFO_Reso = true;	break;
		case 10: // 34
			LFO_Mix = true;		LFO_Reso = true;	break;

		case 11: // 123
			LFO_Osc2 = true;	LFO_PW2 = true;		LFO_Mix = true;		break;
		case 12: // 124
			LFO_Osc2 = true;	LFO_PW2 = true;		LFO_Reso = true;	break;
		case 13: // 134
			LFO_Osc2 = true;	LFO_Mix = true;		LFO_Reso = true;	break;
		case 14: // 234
			LFO_PW2 = true;		LFO_Mix = true;		LFO_Reso = true;	break;

		case 15: // 1234
			LFO_Osc2 = true;
			LFO_PW2 = true;
			LFO_Mix = true;
			LFO_Reso = true;
			break;
		}
	}
	if( tv.LFO2Wave != 0xff)
	{
		if( tv.LFO2Wave == 4)
		{				LFO2Noise = true;
		}
		else
		{				LFO2Noise = false;
		}
		assert(tv.LFO2Wave < 5);
		pwavetabLFO2 = WaveTable[tv.LFO2Wave];
	}

	if( tv.LFO2Freq != 0xff)
	{
		if( tv.LFO2Freq>116)
		{
			LFO2Synced = true;
			LFO2Freq = tv.LFO2Freq - 117;
		}
		else
		{
			LFO2Synced = false;
			LFO2Freq = tv.LFO2Freq;
		}
	}

	if( tv.LFO2Amount != 0xff)
	{
		LFO2Amount = tv.LFO2Amount;
	}

	if( LFO2Synced)
	{
		if( LFO2Noise) // sample & hold
		{				PhaseAddLFO2 = (int)(0x200000/(pmi->pCB->GetTickLength()<<LFO2Freq)); }
		else
		{				PhaseAddLFO2 = (int)((double)0x200000*2048/(pmi->pCB->GetTickLength()<<LFO2Freq)); }
	}
	else
	{
		if( LFO2Noise) // sample & hold
		{				PhaseAddLFO2 = (int)(pmi->LFOFreq( LFO2Freq)/pmi->currentSR*0x200000); }
		else
		{				PhaseAddLFO2 = (int)(pmi->LFOFreq( LFO2Freq)*pmi->TabSizeDivSampleFreq*0x200000); }
	}


	if( GlideActive)
	{
		PhaseAdd1 = (int)(FrequencyFrom*pmi->TabSizeDivSampleFreq*0x10000);
		PhaseAdd2 = (int)(FrequencyFrom*DetuneSemi*DetuneFine*pmi->TabSizeDivSampleFreq*0x10000);
	}
	else
	{
		PhaseAdd1 = (int)(Frequency*pmi->TabSizeDivSampleFreq*0x10000);
		PhaseAdd2 = (int)(Frequency*DetuneSemi*DetuneFine*pmi->TabSizeDivSampleFreq*0x10000);
	}

}



void CTrack::Work( float *psamples, int numsamples)
{
	for( int i=0; i<numsamples; i++)
	{
		if( AEGState)
		{
			float o = Osc()*VCA();
			*(psamples++) += Filter( OldOut + o); // anti knack
			OldOut = o;
		}
		NewPhases();
	}
}
}