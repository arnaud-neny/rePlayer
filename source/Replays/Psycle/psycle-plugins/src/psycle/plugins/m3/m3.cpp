// Here It goes the "mi" declaration. It has been moved to track.hpp due to some compiling requirements.
#include "track.hpp"
#include <psycle/helpers/math.hpp>
#include <cstring>
#include <cstdio>
#include <cmath>
namespace psycle::plugins::m3 {
using namespace psycle::plugin_interface;
using namespace psycle::helpers::math;

// M3 Buzz plugin by MAKK makk@gmx.de
// released on 04-21-99
// Thanks must go to Robert Bristow-Johnson pbjrbj@viconet.com
// a.k.a. robert@audioheads.com for his excellent
// Cookbook formulas for the filters.
// The code is not really speed optimized
// and compiles with many warnings - i'm to lazy to correct
// them all (mostly typecasts).
// Use the source for your own plugins if you want, but don't
// rip the hole machine please.
// Thanks in advance. MAKK

// This Source is Modified by [JAZ] ( jaz@pastnotecut.org ) to
// accomodate the Plugin to Psycle.
// Original Author sources are under:
// http://www.fortunecity.com/skyscraper/rsi/76/plugins.htm

float CTrack::freqTab[12*10];
float CTrack::coefsTab[4 * 128 * 128 * 8];
float CTrack::LFOOscTab[0x10000];
signed short CTrack::WaveTable[5][2100];

/// This value defines the MAX_TRACKS of PSYCLE, not of the Plugin.
/// Leave it like it is. Your Plugin NEEDS TO support it.
/// (Or dynamically allocate them. Check JMDrum's Source for an example)
unsigned int const MAX_PSYCLE_TRACKS = MAX_TRACKS;

CMachineParameter const paraWave1 = {
	"Osc1Wav", "Oscillator 1 Waveform", 0, 5, MPF_STATE, 0
};
CMachineParameter const paraPulseWidth1 = {
	"PulseWidth1", "Oscillator 1 Pulse Width", 0, 127, MPF_STATE, 0x40
};
CMachineParameter const paraWave2 = {
	"Osc2Wav", "Oscillator 2 Waveform", 0, 5, MPF_STATE, 0
};
CMachineParameter const paraPulseWidth2 = {
	"PulseWidth2", "Oscillator 2 Pulse Width", 0, 127, MPF_STATE, 0x40
};
CMachineParameter const paraDetuneSemi = {
	"Semi Detune", "Semi Detune in Halfnotes", 0, 127, MPF_STATE, 0x40
};
CMachineParameter const paraDetuneFine = {
	"Fine Detune", "Fine Detune", 0, 127, MPF_STATE, 0x40
};
CMachineParameter const paraSync = {
	"Oscs Synced", "Sync: Osc2 synced by Osc1", 0, 1, MPF_STATE, 0
};
CMachineParameter const paraMixType = {
	"MixType", "MixType", 0, 8, MPF_STATE, 0
};
CMachineParameter const paraMix = {
	"Osc Mix", "Mix Osc1 <-> Osc2", 0, 127, MPF_STATE, 0x40
};
CMachineParameter const paraSubOscWave = {
	"SubOscWav", "Sub Oscillator Waveform", 0, 4, MPF_STATE, 0
};
CMachineParameter const paraSubOscVol = {
	"SubOscVol", "Sub Oscillator Volume", 0, 127, MPF_STATE, 0x40
};
CMachineParameter const paraPEGAttackTime = {
	"Pitch Env Attack", "Pitch Envelope Attack Time", 0, 127, MPF_STATE, 0
};
CMachineParameter const paraPEGDecayTime = {
	"Pitch Env Release", "Pitch Envelope Release Time", 0, 127, MPF_STATE, 0
};
CMachineParameter const paraPEnvMod = {
	"Pitch Env Mod", "Pitch Envelope Modulation", 0, 127, MPF_STATE, 0x40
};
CMachineParameter const paraGlide = {
	"Glide", "Glide", 0, 127, MPF_STATE, 0
};
CMachineParameter const paraVolume = {
	"Volume", "Volume (Sustain-Level)", 0, 127, MPF_STATE, 0x40
};
CMachineParameter const paraAEGAttackTime = {
	"Amp Env Attack", "Amplitude Envelope Attack Time (ms)", 0, 127, MPF_STATE, 10
};
CMachineParameter const paraAEGSustainTime = {
	"Amp Env Sustain", "Amplitude Envelope Sustain Time (ms)", 0, 127, MPF_STATE, 50
};
CMachineParameter const paraAEGReleaseTime = {
	"Amp Env Release", "Amplitude Envelope Release Time (ms)", 0, 127, MPF_STATE, 30
};
CMachineParameter const paraFilterType = {
	"FilterType", "Filter Type ... 0=LP 1=HP 2=BP 3=BR", 0, 3, MPF_STATE, 0
};
CMachineParameter const paraCutoff = {
	"Cutoff", "Filter Cutoff Frequency", 0, 127, MPF_STATE, 127
};
CMachineParameter const paraResonance = {
	"Res./Bandw.", "Filter Resonance/Bandwidth", 0, 127, MPF_STATE, 32
};
CMachineParameter const paraFEGAttackTime = {
	"Filter Env Attack", "Filter Envelope Attack Time", 0, 127, MPF_STATE, 0
};
CMachineParameter const paraFEGSustainTime = {
	"Filter Env Sustain", "Filter Envelope Sustain Time", 0, 127, MPF_STATE, 0
};
CMachineParameter const paraFEGReleaseTime = {
	"Filter Env Release", "Filter Envelope Release Time", 0, 127, MPF_STATE, 0
};
CMachineParameter const paraFEnvMod = {
	"Filter Env Mod", "Filter Envelope Modulation", 0, 127, MPF_STATE, 0x40
};

// LFOs
CMachineParameter const paraLFO1Dest = {
	"LFO1 Dest", "LFO1 Destination", 0, 15, MPF_STATE, 0
};
CMachineParameter const paraLFO1Wave = {
	"LFO1 Wav", "LFO1 Waveform", 0, 4, MPF_STATE, 0
};
CMachineParameter const paraLFO1Freq = {
	"LFO1 Freq", "LFO1 Frequency", 0, 127, MPF_STATE, 0
};
CMachineParameter const paraLFO1Amount = {
	"LFO1 Amount", "LFO1 Amount", 0, 127, MPF_STATE, 0
};

// lfo2
CMachineParameter const paraLFO2Dest = {
	"LFO2 Dest", "LFO2 Destination", 0, 15, MPF_STATE, 0
};
CMachineParameter const paraLFO2Wave = {
	"LFO2 Wav", "LFO2 Waveform", 0, 4, MPF_STATE, 0
};
CMachineParameter const paraLFO2Freq = {
	"LFO2 Freq", "LFO2 Frequency", 0, 127, MPF_STATE, 0
};
CMachineParameter const paraLFO2Amount = {
	"LFO2 Amount", "LFO2 Amount", 0, 127, MPF_STATE, 0
};

CMachineParameter const *pParameters[] = {
	&paraWave1,
	&paraPulseWidth1,
	&paraWave2,
	&paraPulseWidth2,
	&paraDetuneSemi,
	&paraDetuneFine,
	&paraSync,
	&paraMixType,
	&paraMix,
	&paraSubOscWave,
	&paraSubOscVol,
	&paraPEGAttackTime,
	&paraPEGDecayTime,
	&paraPEnvMod,
	&paraGlide,

	&paraVolume,
	&paraAEGAttackTime,
	&paraAEGSustainTime,
	&paraAEGReleaseTime,

	&paraFilterType,
	&paraCutoff,
	&paraResonance,
	&paraFEGAttackTime,
	&paraFEGSustainTime,
	&paraFEGReleaseTime,
	&paraFEnvMod,

	// LFO 1
	&paraLFO1Dest,
	&paraLFO1Wave,
	&paraLFO1Freq,
	&paraLFO1Amount,
	// LFO 2
	&paraLFO2Dest,
	&paraLFO2Wave,
	&paraLFO2Freq,
	&paraLFO2Amount,
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0120,
	GENERATOR,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"M3 by Makk"
	#ifndef NDEBUG
		 " (debug build)"
	#endif
	,
	"M3",
	"Makk",
	"About",
	5
);

PSYCLE__PLUGIN__INSTANTIATOR("m3", mi, MacInfo)

mi::mi() {
	Vals =new int[MacInfo.numParameters];
	// Generate Oscillator tables
	InitWaveTable();
	// generate frequencyTab
	double freq = 16.35; // c0 to b9
	for(int j = 0; j < 10; ++j)
		for(int i = 0; i < 12; ++i) {
			CTrack::freqTab[j * 12 + i] = float(freq);
			freq *= 1.05946309435929526; // * 2 ^ (1 / 12)
		}
	// generate LFOOscTab
	for(int p = 0; p < 0x10000; ++p) CTrack::LFOOscTab[p] = std::pow(1.00004230724139582, p - 0x8000);
}

mi::~mi() {
	delete[] Vals;
}

void mi::Init() {
	currentSR = pCB->GetSamplingRate();
	TabSizeDivSampleFreq = (float)(2048.0/currentSR);

	for(int i = 0; i < MAX_SIMUL_TRACKS; ++i) {
		Tracks[i].pmi = this;
		Tracks[i].Init();
	}

	// generate coefsTab
	for(int t = 0; t < 4; ++t)
		for(int f = 0; f < 128; ++f)
			for(int r = 0; r < 128; ++r)
				ComputeCoefs(CTrack::coefsTab + (t * 128 * 128 + f * 128 + r) * 8, f, r, t);
}

// Called each tick (i.e.when playing). Note: it goes after ParameterTweak and before SeqTick
void mi::SequencerTick() {
	if(currentSR != pCB->GetSamplingRate()) {
		currentSR = pCB->GetSamplingRate();
		TabSizeDivSampleFreq = (float)(2048.0/currentSR);
		// generate coefsTab
		for(int t = 0; t < 4; ++t)
			for(int f = 0; f < 128; ++f)
				for(int r = 0; r < 128; ++r)
					ComputeCoefs(CTrack::coefsTab + (t * 128 * 128 + f * 128 + r) * 8, f, r, t);

		tvals tmp;
		SetNoValue(tmp);
		tmp.PEGAttackTime = Vals[11];
		tmp.PEGDecayTime = Vals[12]; 
		tmp.AEGAttackTime = Vals[16]; 
		tmp.AEGSustainTime = Vals[17];
		tmp.AEGReleaseTime = Vals[18];
		tmp.FEGAttackTime = Vals[22]; 
		tmp.FEGSustainTime = Vals[23]; 
		tmp.FEGReleaseTime = Vals[24]; 
		tmp.Glide = Vals[14];
		for(int i = 0; i < MAX_SIMUL_TRACKS; ++i)
			Tracks[i].Tick(tmp);
	}
}


void mi::Stop() {
	for(int i = 0; i < MAX_SIMUL_TRACKS; ++i) Tracks[i].Stop();
}

// Called when a parameter is changed by the host app / user gui
void mi::ParameterTweak(int par, int val) {
	Vals[par]=val; // Don't remove this line. Psycle reads this variable when Showing the Parameters' Dialog box.
	tvals tmp;

	SetNoValue(tmp);

	switch(par) {
		case 0:	tmp.Wave1 = val; break;
		case 1: tmp.PulseWidth1 = val; break;
		case 2:	tmp.Wave2 = val; break;
		case 3: tmp.PulseWidth2 = val; break;
		case 4: tmp.DetuneSemi = val; break;
		case 5: tmp.DetuneFine = val; break;
		case 6: tmp.Sync = val; break;
		case 7: tmp.MixType = val; break;
		case 8: tmp.Mix = val; break;
		case 9: tmp.SubOscWave = val; break;
		case 10: tmp.SubOscVol = val; break;
		case 11: tmp.PEGAttackTime = val; break;
		case 12: tmp.PEGDecayTime = val; break;
		case 13: tmp.PEnvMod = val; break;
		case 14: tmp.Glide = val; break;
		case 15: tmp.Volume = val; break;
		case 16: tmp.AEGAttackTime = val; break;
		case 17: tmp.AEGSustainTime = val; break;
		case 18: tmp.AEGReleaseTime = val; break;
		case 19: tmp.FilterType = val; break;
		case 20: tmp.Cutoff = val; break;
		case 21: tmp.Resonance = val; break;
		case 22: tmp.FEGAttackTime = val; break;
		case 23: tmp.FEGSustainTime = val; break;
		case 24: tmp.FEGReleaseTime = val; break;
		case 25: tmp.FEnvMod = val; break;
		case 26: tmp.LFO1Dest = val; break;
		case 27: tmp.LFO1Wave = val; break;
		case 28: tmp.LFO1Freq = val; break;
		case 29: tmp.LFO1Amount = val; break;
		case 30: tmp.LFO2Dest = val; break;
		case 31: tmp.LFO2Wave = val; break;
		case 32: tmp.LFO2Freq = val; break;
		case 33: tmp.LFO2Amount = val; break;
		default: ;
	}

	for(int i = 0; i < MAX_SIMUL_TRACKS; ++i) // It is MUCH better to change the parameter to all
		Tracks[i].Tick(tmp);
}

////////////////////////////////////////////////////////////////////////
// The SeqTick function is where your notes and pattern command handlers
// should be processed.
// It is called by the host sequencer
	
void mi::SeqTick(int channel, int note, int ins, int cmd, int val) {
	// Note Off == 120
	// Empty Note Row == 255
	// Less than note off == NoteON
	// A4 = note 69

	int useVoice = -1;
	tvals tmp;

	SetNoValue(tmp);

	if(note<=NOTE_MAX) { // New Note entering.
		for(int voice = 0; voice < MAX_SIMUL_TRACKS; ++voice) { // Find a voice to apply the new note
			switch(Tracks[voice].AEGState) {
				case EGS_NONE:
					if(useVoice == -1) useVoice = voice;
					else if(Tracks[useVoice]._channel != channel || !Tracks[useVoice].Glide) useVoice = voice;
				break;

				case EGS_RELEASE:
					if(useVoice == -1) useVoice = voice;
				break;
				
				default: ;
			}
			if(Tracks[voice]._channel == channel) { // Does exist a Previous note?
				if(!Tracks[voice].Glide) { // If no Glide , Note Off
					tmp.Note = NOTE_NOTEOFF;
					Tracks[voice].Tick(tmp);
					Tracks[voice]._channel = -1;
					if(useVoice == -1) useVoice = voice;
				}
				else useVoice = voice; // Else, use it.
			}
		}
		if(useVoice == -1) useVoice=0; // No free voices. Assign one!
		tmp.Note = note;
		Tracks[useVoice]._channel = channel;
		Tracks[useVoice].Tick(tmp);
	}
	else if(note == NOTE_NOTEOFF) {
		for(int voice = 0; voice < MAX_SIMUL_TRACKS; ++voice) { // Find the...
			if(
				Tracks[voice]._channel == channel && // ...playing voice on current channel.
				Tracks[voice].AEGState != EGS_NONE &&
				Tracks[voice].AEGState != EGS_RELEASE
			) {
				tmp.Note = note;
				Tracks[voice].Tick(tmp); // Handle Note Off
				Tracks[voice]._channel = -1;
				return;
			}
		}
	}
}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples,int tracks) {
	bool gotSomething=false;
	for( int i = 0; i < MAX_SIMUL_TRACKS; ++i) {
		if(Tracks[i].AEGState) {
			Tracks[i].Work(psamplesleft, numsamples);
			gotSomething = true;
		}
	}
	if(gotSomething) std::memcpy(psamplesright, psamplesleft, numsamples * sizeof(float));
}

void mi::Command() {
	pCB->MessBox("Compiled on " __DATE__ , "M3 Psycle Plugin", 0);
}

// Function that describes the value on client's displaying
bool mi::DescribeValue(char * txt, int const param, int const value) {
	switch(param) {
		case 1: // PW1
		case 3: // PW2
			std::sprintf(txt, "%u : %u", (int)(value * 100.0 / 127), 100 - (int)(value * 100.0 / 127));
			return true;
		case 4: // semi detune
			if(value == 0x40) std::strcpy(txt, "none");
			else if(value > 0x40) std::sprintf(txt, "+%i halfnotes", value - 0x40);
			else std::sprintf(txt, "%i halfnotes", value - 0x40);
			return true;
		case 5: // fine detune
			if(value == 0x40) std::strcpy(txt, "none");
			else if(value > 0x40) std::sprintf(txt, "+%i cents", (int)((value - 0x40) * 100.0 / 63));
			else std::sprintf(txt, "%i cents", (int)((value - 0x40) * 100.0 / 63));
			return true;
		case 6: // Sync
			if(value == 1) std::strcpy(txt, "on");
			else std::strcpy(txt, "off");
			return true;
		case 7: // MixType
			switch(value) {
				case 0: std::strcpy(txt, "add"); break;
				case 1: std::strcpy(txt, "difference"); break;
				case 2: std::strcpy(txt, "mul"); break;
				case 3: std::strcpy(txt, "highest amp"); break;
				case 4: std::strcpy(txt, "lowest amp"); break;
				case 5: std::strcpy(txt, "and"); break;
				case 6: std::strcpy(txt, "or"); break;
				case 7: std::strcpy(txt, "xor"); break;
				case 8: std::strcpy(txt, "random"); break;
				default: std::strcpy(txt, "Invalid!");
			}
			return true;
		case 8: // Mix
			switch(value) {
				case 0: std::strcpy(txt, "Osc1"); break;
				case 127: std::strcpy(txt, "Osc2"); break;
				default: std::sprintf(txt, "%u%% : %u%%", 100 - (int)(value * 100.0 / 127), (int)(value * 100.0 / 127));
			}
			return true;
		case 11: // Pitch Env
		case 12: // Pitch Env
		case 16: // Amp Env
		case 17: // Amp Env
		case 18: // Amp Env
		case 22: // Filter Env
		case 23: // Filter Env
		case 24: // Filter Env
			std::sprintf(txt, "%.4f sec", EnvTime(value) / 1000);
			return true;
		case 13: // PitchEnvMod
		case 25: // Filt ENvMod
			std::sprintf(txt, "%i", value - 0x40);
			return true;
		case 19:
			switch( value) {
				case 0: std::strcpy(txt, "lowpass"); break;
				case 1: std::strcpy(txt, "highpass"); break;
				case 2: std::strcpy(txt, "bandpass"); break;
				case 3: std::strcpy(txt, "bandreject"); break;
				default: std::strcpy(txt, "Invalid!");
			}
			return true;
		case 26: // LFO1Dest
			switch( value) {
				case 0: std::strcpy(txt, "none"); break;
				case 1: std::strcpy(txt, "osc1"); break;
				case 2: std::strcpy(txt, "p.width1"); break;
				case 3: std::strcpy(txt, "volume"); break;
				case 4: std::strcpy(txt, "cutoff"); break;
				case 5: std::strcpy(txt, "osc1+pw1"); break; // 12
				case 6: std::strcpy(txt, "osc1+volume"); break; // 13
				case 7: std::strcpy(txt, "osc1+cutoff"); break; // 14
				case 8: std::strcpy(txt, "pw1+volume"); break; // 23
				case 9: std::strcpy(txt, "pw1+cutoff"); break; // 24
				case 10: std::strcpy(txt, "vol+cutoff"); break; // 34
				case 11: std::strcpy(txt, "o1+pw1+vol"); break; // 123
				case 12: std::strcpy(txt, "o1+pw1+cut"); break; // 124
				case 13: std::strcpy(txt, "o1+vol+cut"); break; // 134
				case 14: std::strcpy(txt, "pw1+vol+cut"); break; // 234
				case 15: std::strcpy(txt, "all");break; // 1234
				default: std::strcpy(txt, "Invalid!");
			}
			return true;
		case 30: // LFO2Dest
			switch( value) {
				case 0: std::strcpy(txt, "none"); break;
				case 1: std::strcpy(txt, "osc2"); break;
				case 2: std::strcpy(txt, "p.width2"); break;
				case 3: std::strcpy(txt, "mix"); break;
				case 4: std::strcpy(txt, "resonance"); break;

				case 5: std::strcpy(txt, "osc2+pw2"); break; // 12
				case 6: std::strcpy(txt, "osc2+mix"); break; // 13
				case 7: std::strcpy(txt, "osc2+res"); break; // 14
				case 8: std::strcpy(txt, "pw2+mix"); break; // 23
				case 9: std::strcpy(txt, "pw2+res"); break; // 24
				case 10: std::strcpy(txt, "mix+res"); break; // 34

				case 11: std::strcpy(txt, "o2+pw2+mix"); break; // 123
				case 12: std::strcpy(txt, "o2+pw2+res"); break; // 124
				case 13: std::strcpy(txt, "o2+mix+res"); break; // 134
				case 14: std::strcpy(txt, "pw2+mix+res"); break; // 234
				case 15: std::strcpy(txt, "all"); break; // 1234
				default: std::strcpy(txt, "Invalid!");
			}
			return true;
		case 9: // SubOscWave
			if(value == 4) { std::strcpy(txt, "random"); return true; break; }
		case 0: // OSC1Wave
		case 2: // OSC2Wave
		case 27: // LFO1Wave
		case 31: // LFO2Wave
			switch(value) {
				case 0: std::strcpy(txt, "sine"); break;
				case 1: std::strcpy(txt, "saw"); break;
				case 2: std::strcpy(txt, "square"); break;
				case 3: std::strcpy(txt, "triangle"); break;
				case 4: std::strcpy(txt, "noise"); break;
				case 5: std::strcpy(txt, "random"); break;
				default: std::strcpy(txt, "Invalid!"); break;
			}
			return true;
		case 28: // LFO1Freq
		case 32: // LFO2Freq
			if(value <= 116) std::sprintf(txt, "%.4f Hz", LFOFreq(value));
			else std::sprintf(txt, "%u ticks", 1 << (value - 117));
			return true;
		default: return false;
	}
}

void mi::SetNoValue(tvals &tv) {
	tv.Note = 0xff;
	tv.Wave1 = 0xff;
	tv.PulseWidth1 = 0xff;
	tv.Wave2 = 0xff;
	tv.PulseWidth2 = 0xff;
	tv.DetuneSemi = 0xff;
	tv.DetuneFine = 0xff;
	tv.Sync = 0xff;
	tv.MixType = 0xff;
	tv.Mix = 0xff;
	tv.SubOscWave = 0xff;
	tv.SubOscVol = 0xff;
	tv.PEGAttackTime = 0xff;
	tv.PEGDecayTime = 0xff;
	tv.PEnvMod = 0xff;
	tv.Glide = 0xff;

	tv.Volume = 0xff;
	tv.AEGAttackTime = 0xff;
	tv.AEGSustainTime = 0xff;
	tv.AEGReleaseTime = 0xff;

	tv.FilterType = 0xff;
	tv.Cutoff = 0xff;
	tv.Resonance = 0xff;
	tv.FEGAttackTime = 0xff;
	tv.FEGSustainTime = 0xff;
	tv.FEGReleaseTime = 0xff;
	tv.FEnvMod = 0xff;

	tv.LFO1Dest = 0xff;
	tv.LFO1Wave = 0xff;
	tv.LFO1Freq = 0xff;
	tv.LFO1Amount = 0xff;
	tv.LFO2Dest = 0xff;
	tv.LFO2Wave = 0xff;
	tv.LFO2Freq = 0xff;
	tv.LFO2Amount = 0xff;
}

void mi::InitWaveTable(){
	for(int c = 0; c < 2100; ++c) {
		double sval = (double) c * 0.00306796157577128245943617517898389;
		CTrack::WaveTable[0][c] = int(std::sin(sval) * 16384.0f);

		if(c < 2048) CTrack::WaveTable[1][c] = (c * 16) - 16384;
		else CTrack::WaveTable[1][c] = ((c - 2048) * 16) - 16384;

		if(c < 1024) CTrack::WaveTable[2][c] = -16384;
		else CTrack::WaveTable[2][c] = 16384;

		if(c < 1024) CTrack::WaveTable[3][c] = (c * 32) - 16384;
		else CTrack::WaveTable[3][c] = 16384 - ((c - 1024) * 32);

		CTrack::WaveTable[4][c] = std::rand();
	}
}


void mi::ComputeCoefs( float *coefs, int freq, int r, int t) {

	float omega = 2 * psycle::plugin_interface::pi * Cutoff(freq) / currentSR;

	float sn, cs;
	sincos(omega, sn, cs);
	//float sn = std::sin(omega);
	//float cs = std::cos(omega);
	
	float alpha;
	if(t<2) alpha = sn / Resonance(r * (freq + 70) / (127.0 + 70));
	else alpha = sn * std::sinh(Bandwidth(r) * omega / sn);

	float a0, a1, a2, b0, b1, b2;

	switch(t) {
		case 0: // LP
				b0 = (1 - cs) / 2;
				b1 = 1 - cs;
				b2 = (1 - cs) / 2;
				a0 = 1 + alpha;
				a1 = -2 * cs;
				a2 = 1 - alpha;
				break;
		case 1: // HP
				b0 = (1 + cs) / 2;
				b1 = -(1 + cs);
				b2 = (1 + cs) / 2;
				a0 = 1 + alpha;
				a1 = -2 * cs;
				a2 = 1 - alpha;
				break;
		case 2: // BP
				b0 = alpha;
				b1 = 0;
				b2 = -alpha;
				a0 = 1 + alpha;
				a1 = -2 * cs;
				a2 = 1 - alpha;
				break;
		case 3: // BR
				b0 = 1;
				b1 = -2 * cs;
				b2 = 1;
				a0 = 1 + alpha;
				a1 = -2 * cs;
				a2 = 1 - alpha;
				break;
		default:
			throw 0;
	}

	coefs[0] = b0 / a0;
	coefs[1] = b1 / a0;
	coefs[2] = b2 / a0;
	coefs[3] = -a1 / a0;
	coefs[4] = -a2 / a0;
}
}