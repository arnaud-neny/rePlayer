// CTrack Declaration file

#pragma once
#include <psycle/plugin_interface.hpp>
#include <universalis/stdlib/cstdint.hpp>
#include <cstdlib>
#include <cmath>
namespace psycle::plugins::m3 {
using namespace universalis::stdlib;

unsigned int const MAX_SIMUL_TRACKS = 8;

class mi;

enum {
	EGS_NONE=0,
	EGS_ATTACK,
	EGS_SUSTAIN,
	EGS_RELEASE
};

class tvals {
public:
	uint8_t Note;
	uint8_t Wave1;
	uint8_t PulseWidth1;
	uint8_t Wave2;
	uint8_t PulseWidth2;
	uint8_t DetuneSemi;
	uint8_t DetuneFine;
	uint8_t Sync;
	uint8_t MixType;
	uint8_t Mix;
	uint8_t SubOscWave;
	uint8_t SubOscVol;
	uint8_t PEGAttackTime;
	uint8_t PEGDecayTime;
	uint8_t PEnvMod;
	uint8_t Glide;

	uint8_t Volume;
	uint8_t AEGAttackTime;
	uint8_t AEGSustainTime;
	uint8_t AEGReleaseTime;

	uint8_t FilterType;
	uint8_t Cutoff;
	uint8_t Resonance;
	uint8_t FEGAttackTime;
	uint8_t FEGSustainTime;
	uint8_t FEGReleaseTime;
	uint8_t FEnvMod;

	uint8_t LFO1Dest;
	uint8_t LFO1Wave;
	uint8_t LFO1Freq;
	uint8_t LFO1Amount;
	uint8_t LFO2Dest;
	uint8_t LFO2Wave;
	uint8_t LFO2Freq;
	uint8_t LFO2Amount;
};

class CTrack {
	public:
		void Tick(tvals const &tv);
		void Stop();
		void Init();
		void Work(float *psamples, int numsamples);
		inline float Osc();
		inline float Filter( float x);
		inline float VCA();
		void NewPhases();
		int MSToSamples(double const ms);

	public:
		static float freqTab[120];
		static float coefsTab[4*128*128*8];
		static float LFOOscTab[0x10000];
		static signed short WaveTable[5][2100];

		int _channel;

		// ......Osc......
		uint8_t Note;
		const short *pwavetab1, *pwavetab2, *pwavetabsub;
		int SubOscVol;
		bool noise1, noise2;
		int Bal1, Bal2;
		int MixType;
		int Phase1, Phase2, PhaseSub;
		int Ph1, Ph2;
		float currentcenter1, currentcenter2;
		float Center1, Center2;
		float PhScale1A, PhScale1B;
		float PhScale2A, PhScale2B;
		int PhaseAdd1, PhaseAdd2;
		float Frequency, FrequencyFrom;
		// Detune
		float DetuneSemi, DetuneFine;
		bool Sync;
		// Glide
		bool Glide, GlideActive;
		float GlideMul, GlideFactor;
		int GlideTime, GlideCount;
		// PitchEnvMod
		bool PitchMod, PitchModActive;
		// PEG ... AD-Hüllkurve
		int PEGState;
		int PEGAttackTime;
		int PEGDecayTime;
		int PEGCount;
		float PitchMul, PitchFactor;
		int PEnvMod;
		// random generator... rauschen
		short r1, r2, r3, r4;

		float OldOut; // gegen extreme Knackser/Wertesprünge

		// .........AEG........ ASR-Hüllkurve
		float Volume;
		int AEGState;
		int AEGAttackTime;
		int AEGSustainTime;
		int AEGReleaseTime;
		int AEGCount;
		float Amp;
		float AmpAdd;

		// ........Filter..........
		float *coefsTabOffs; // abhängig vom FilterTyp
		int Cutoff, Resonance;
		float x1, x2, y1, y2;
		// FEG ... ASR-Hüllkurve
		int FEnvMod;
		int FEGState;
		int FEGAttackTime;
		int FEGSustainTime;
		int FEGReleaseTime;
		int FEGCount;
		float Cut;
		float CutAdd;

		// .........LFOs...........
		// 1
		bool LFO_Osc1;
		bool LFO_PW1;
		bool LFO_Amp;
		bool LFO_Cut;
		// 2
		bool LFO_Osc2;
		bool LFO_PW2;
		bool LFO_Mix;
		bool LFO_Reso;

		bool LFO1Noise, LFO2Noise; // andere Frequenz
		bool LFO1Synced,LFO2Synced; // zum Songtempo
		const short *pwavetabLFO1, *pwavetabLFO2;
		int LFO1Amount, LFO2Amount;
		int PhaseLFO1, PhaseLFO2, PhaseAddLFO1, PhaseAddLFO2;
		int LFO1Freq, LFO2Freq;



		// RANDOMS
		const short *pnoise;
		int noisePhase;
		bool RandomMixType;
		bool RandomWave1;
		bool RandomWave2;
		bool RandomWaveSub;

		mi *pmi; // ptr to MachineInterface

};

class mi : public psycle::plugin_interface::CMachineInterface {
	public:
		mi();
		virtual ~mi();

		virtual void Init();
		virtual void SequencerTick();
		virtual void Work(float *psamplesleft, float* psamplesright, int numsamples,int tracks);
		virtual bool DescribeValue(char* txt,int const param, int const value);
		virtual void Command();
		virtual void ParameterTweak(int par, int val);
		virtual void SeqTick(int channel, int note, int ins, int cmd, int val);
		virtual void Stop();

		inline static float Cutoff(int v);
		inline static float Resonance(float v);
		inline static float Bandwidth(int v);
		inline static float LFOFreq( int v);
		inline static float EnvTime( int v);

		float TabSizeDivSampleFreq;
		int currentSR;
	private:
		void SetNoValue(tvals &tv);
		void ComputeCoefs( float *coefs, int f, int r, int t);
		void InitWaveTable();
		// skalefuncs

		
		CTrack Tracks[MAX_SIMUL_TRACKS];
};

// scale functions

float mi::Cutoff(int v) {
	return std::pow((v + 5.0) / (127.0 + 5.0), 1.7) * 13000.0 + 30.0;
}

float mi::Resonance(float v) {
	return std::pow(v / 127.0, 4.0) * 150.0 + 0.1;
}

float mi::Bandwidth(int v) {
	return std::pow(v / 127.0, 4.0) * 4.0 + 0.1;
}

float mi::LFOFreq(int v) {
	return (std::pow((v + 8.0) / (116.0 + 8.0), 4.0) - 0.000017324998565270) * 40.00072;
}

float mi::EnvTime(int v) {
	return std::pow((v + 2.0) / (127.0 + 2.0), 3.0) * 10000;
}
////////////////////////////////
////////////////////////////////
////////////////////////////////

inline int CTrack::MSToSamples(double const ms) {
	return (int)(pmi->currentSR * ms * (1.0 / 1000.0)) + 1; // +1 wg. div durch 0
}


inline float CTrack::Osc() {
	float o, o2;
	int B1, B2;
	if(LFO_Mix) {
		B2 = Bal2 + ((pwavetabLFO2[((unsigned int) PhaseLFO2) >> 21] * LFO2Amount) >> 15);
		if(B2 < 0) B2 = 0;
		else if(B2 > 127) B2 = 127;
		B1 = 127-B2;
	} else {
		B1 = Bal1;
		B2 = Bal2;
	}

	// osc1
	if(noise1) {
		short t = (r1 + r2 + r3 + r4) & 0xFFFF;
		r1 = r2; r2 = r3; r3 = r4; r4 = t;
		o = float((t * B1) >> 7);
	} else
		o = float((pwavetab1[Ph1 >> 16] * B1) >> 7);

	// osc2
	if(noise2) {
		short u = (r1 + r2 + r3 + r4) & 0xFFFF;
		r1=r2; r2=r3; r3=r4; r4=u;
		o2 = (u * B2) >> 7;
	}
	else
		o2 = (pwavetab2[Ph2 >> 16] * B2) >> 7;


	switch( MixType) {
		case 0: //ADD
			o += o2;
		break;
		case 1: // ABS
			o = fabs(o-o2)*2-0x8000;
		break;
		case 2: // MUL
			o *= o2*(1.0/0x4000);
		break;
		case 3: // highest amp
			if(std::fabs(o) < std::fabs(o2)) o = o2;
		break;
		case 4: // lowest amp
			if(std::fabs(o) > std::fabs(o2)) o = o2;
		break;
		case 5: // AND
			o = (int)o & (int)o2;
		break;
		case 6: // OR
			o = (int)o | (int)o2;
		break;
		case 7: // XOR
			o = (int)o ^ (int)o2;
		break;
	}
	return o + ((pwavetabsub[PhaseSub >> 16] * SubOscVol) >> 7);
}

inline float CTrack::VCA() {
	// EG...
	if(!AEGCount--)
		switch( ++AEGState) {
			case EGS_SUSTAIN:
				AEGCount = AEGSustainTime;
				Amp = Volume;
				AmpAdd = 0.0;
			break;
			case EGS_RELEASE:
				AEGCount = AEGReleaseTime-1;
				AmpAdd = -Amp/AEGReleaseTime;
			break;
			case EGS_RELEASE + 1:
				AEGState = EGS_NONE;
				AEGCount = -1;
				AmpAdd = 0.0;
				Amp = 0.0;
			break;
		}

	Amp += AmpAdd;
	if(LFO_Amp) {
		float a = Amp + (pwavetabLFO1[((unsigned)PhaseLFO1)>>21]*LFO1Amount)/(127.0*0x8000);
		if(a < 0) a = 0;
		return a;
	} else return Amp;
}

inline float CTrack::Filter( float x) {
	float y;

	// Envelope
	if(FEGState) {
		if(!FEGCount--)
			switch( ++FEGState) {
				case EGS_SUSTAIN:
					FEGCount = FEGSustainTime;
					Cut = (float)FEnvMod;
					CutAdd = 0.0;
				break;
				case EGS_RELEASE:
					FEGCount = FEGReleaseTime;
					CutAdd = ((float)-FEnvMod)/FEGReleaseTime;
				break;
				case EGS_RELEASE + 1:
					FEGState = EGS_NONE; // false
					FEGCount = -1;
					Cut = 0.0;
					CutAdd = 0.0;
				break;
			}
			Cut += CutAdd;
	}

	// LFO
	// Cut
	int c, r;
	if(LFO_Cut) {
		c = Cutoff + Cut + // Cut = EnvMod
		((pwavetabLFO1[((unsigned)PhaseLFO1) >> 21] * LFO1Amount) >> (7 + 8));
	}
	else c = Cutoff + Cut; // Cut = EnvMod

	if(c < 0) c = 0;
	else if(c > 127) c = 127;

	// Reso
	if(LFO_Reso) {
		r = Resonance +
		((pwavetabLFO2[((unsigned)PhaseLFO2) >> 21] * LFO2Amount) >> (7 + 8));
	} else r = Resonance;

	if(r < 0) r = 0;
	else if(r > 127) r = 127;

	int ofs = ((c << 7) + r) << 3;
	y =
		coefsTabOffs[ofs    ] * x +
		coefsTabOffs[ofs + 1] * x1 +
		coefsTabOffs[ofs + 2] * x2 +
		coefsTabOffs[ofs + 3] * y1 +
		coefsTabOffs[ofs + 4] * y2;

	y2 = y1; y1 = y;
	x2 = x1; x1 = x;
	///\fixme
	return y;
	//return x;
}

inline void CTrack::NewPhases() {
	if(PitchModActive) {
		if(GlideActive) {
			if(LFO_Osc1) {
				float pf = LFOOscTab[(pwavetabLFO1[((unsigned)PhaseLFO1) >> 21] * LFO1Amount >> 7) + 0x8000];
				Phase1 += PhaseAdd1 * GlideFactor*PitchFactor * pf;
				PhaseSub += PhaseAdd1 * GlideFactor*PitchFactor * pf / 2;
			} else {
				Phase1 += PhaseAdd1 * GlideFactor * PitchFactor;
				PhaseSub += PhaseAdd1 * GlideFactor * PitchFactor / 2;
			}
			if(LFO_Osc2) {
				Phase2 += PhaseAdd2 * GlideFactor * PitchFactor
				*LFOOscTab[(pwavetabLFO2[((unsigned)PhaseLFO2) >> 21] * LFO2Amount >> 7) + 0x8000];
			} else Phase2 += PhaseAdd2 * GlideFactor * PitchFactor;

			GlideFactor *= GlideMul;
			if(!GlideCount--) {
				GlideActive = false;
				PhaseAdd1 = (int)(Frequency * pmi->TabSizeDivSampleFreq * 0x10000);
				PhaseAdd2 = (int)(Frequency * DetuneSemi * DetuneFine * pmi->TabSizeDivSampleFreq * 0x10000);
			}
		} else { // no glide
			if(LFO_Osc1) {
				float pf = LFOOscTab[(pwavetabLFO1[((unsigned)PhaseLFO1) >> 21] * LFO1Amount >> 7) + 0x8000];
				Phase1 += PhaseAdd1 * PitchFactor * pf;
				PhaseSub += PhaseAdd1 * PitchFactor * pf / 2;
			} else {
				Phase1 += PhaseAdd1 * PitchFactor;
				PhaseSub += PhaseAdd1 * PitchFactor / 2;
			}
			if(LFO_Osc2) Phase2 += PhaseAdd2 * PitchFactor * LFOOscTab[(pwavetabLFO2[((unsigned)PhaseLFO2) >> 21] * LFO2Amount >> 7) + 0x8000];
			else Phase2 += PhaseAdd2 * PitchFactor;
		}

		PitchFactor *= PitchMul;

		if(!PEGCount--)
			if(++PEGState == 2) { // DECAY-PHASE begin
				PEGCount = PEGDecayTime;
				PitchMul = std::pow(std::pow(1 / 1.01, PEnvMod), 1.0 / PEGDecayTime);
			} else // AD-curve is at end
				PitchModActive = false;
	} else { // no pitch mod
		if(GlideActive) {
			if(LFO_Osc1) {
				float pf = LFOOscTab[(pwavetabLFO1[((unsigned)PhaseLFO1) >> 21] * LFO1Amount >> 7) + 0x8000];
				Phase1 += PhaseAdd1 * GlideFactor * pf;
				PhaseSub += PhaseAdd1 * GlideFactor * pf / 2;
			} else {
				Phase1 += PhaseAdd1 * GlideFactor;
				PhaseSub += PhaseAdd1 * GlideFactor / 2;
			}
			if(LFO_Osc2) {
				Phase2 += PhaseAdd2 * GlideFactor * LFOOscTab[(pwavetabLFO2[((unsigned)PhaseLFO2) >> 21] * LFO2Amount >> 7) + 0x8000];
			} else Phase2 += PhaseAdd2*GlideFactor;

			GlideFactor *= GlideMul;
			if(!GlideCount--) {
				GlideActive = false;
				PhaseAdd1 = (int)(Frequency * pmi->TabSizeDivSampleFreq * 0x10000);
				PhaseAdd2 = (int)(Frequency * DetuneSemi * DetuneFine * pmi->TabSizeDivSampleFreq * 0x10000);
			}
		} else {
			if(LFO_Osc1) {
					float pf = LFOOscTab[(pwavetabLFO1[((unsigned)PhaseLFO1) >> 21] * LFO1Amount >> 7) + 0x8000];
					Phase1 += PhaseAdd1 * pf;
					PhaseSub += PhaseAdd1 * pf / 2;
			} else {
					Phase1 += PhaseAdd1;
					PhaseSub += PhaseAdd1 / 2;
			}
			if(LFO_Osc2) Phase2 += PhaseAdd2 * LFOOscTab[(pwavetabLFO2[((unsigned)PhaseLFO2) >> 21] * LFO2Amount >> 7) + 0x8000];
			else Phase2 += PhaseAdd2;
		}
	}

	if(Phase1 & 0xf8000000) { // new durchlauf ??
		// PW1
		if(LFO_PW1) { // LFO_PW_Mod
			currentcenter1 = Center1 + (float)pwavetabLFO1[((unsigned)PhaseLFO1) >> 21] * LFO1Amount / (127.0 * 0x8000);
			if(currentcenter1 <= 0) currentcenter1 = 0.00001f;
			else if(currentcenter1 >= 1) currentcenter1 = 0.99999f;
		} else {// No LFO
			currentcenter1 = Center1;
		}
		PhScale1A = 0.5 / currentcenter1;
		PhScale1B = 0.5 / (1 - currentcenter1);
		currentcenter1 *= 0x8000000;
		// PW2
		if(LFO_PW2) { //LFO_PW_Mod
			currentcenter2 = Center2 + (float)pwavetabLFO2[((unsigned)PhaseLFO2) >> 21] * LFO2Amount / (127.0 * 0x8000);
			if(currentcenter2 <= 0) currentcenter2 = 0.00001f;
			else if(currentcenter2 >= 1) currentcenter2 = 0.99999f;
		} else { // No LFO
			currentcenter2 = Center2;
		}
		PhScale2A = 0.5 / currentcenter2;
		PhScale2B = 0.5 / (1 - currentcenter2);
		currentcenter2 *= 0x8000000;

		// SYNC
		if(Sync) Phase2 = Phase1; // !!!!!
	}

	Phase1 &= 0x7ffffff;
	Phase2 &= 0x7ffffff;
	PhaseSub &= 0x7ffffff;

	if(Phase1 < currentcenter1) Ph1 = Phase1*PhScale1A;
	else Ph1 = (Phase1 - currentcenter1) * PhScale1B + 0x4000000;

	if(Phase2 < currentcenter2) Ph2 = Phase2 * PhScale2A;
	else Ph2 = (Phase2 - currentcenter2) * PhScale2B + 0x4000000;

	// LFOs
	PhaseLFO1 += PhaseAddLFO1;
	PhaseLFO2 += PhaseAddLFO2;
}
}