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

#pragma once
#include "atlantisfilter.h"
#include "aaf.h"
namespace psycle::plugins::jme::gamefxn {
#define WAVETABLES 8
static float const FAST_RELEASE = 128.0f;
static float const RC_VOL_CUTOFF=0.25f;

struct PERFORMANCE
{
//Parameters
	int Volume[16];
	int Waveform[16];
	int Transpose[16];
	int Option[16];
	int Command[16];
	int Parameter[16];
	int Speed[16];
	// which of the 16 positions is the first when note starts
	int StartPos;
	// which of the 16 positions is the loop start point
	int LoopStart;
	// which of the 16 positions is the loop end point
	int LoopEnd;
	// Speed (in samples) at which the updates to OSCSpeed and perf_count happen
	float ReplaySpeed;
	// Amplitude envelope attack, decay sustain and release. ADR in samples, S in 0..256
	float AEGAttack;
	float AEGDecay;
	int AEGSustain;
	float AEGRelease;
	// Filter cutoff in 0..255
	int Cutoff;
	// Filter ressonance in 1..240
	int Resonance;
	// Filter envelope attack, decay sustain and release. ADR in samples, S in 0..256
	float FEGAttack;
	float FEGDecay;
	int FEGSustain;
	float FEGRelease;
	// Filter envelope amount 0..255
	int EnvMod;
	// Note finetune 1/256th of a note.
	int Finetune;
//Wavetables and noise generator
	signed short* Wavetable[WAVETABLES];
	//Value of the noise sample in this round (short) (used directly as low noise osci).
	signed short shortnoise;
	// Switch used to indicate if the noise wavetable needs updating by the main Work call.
	bool noiseused;
	int oversamplesAmt;
};

class CSynthTrack  
{
public:
	//wtNewACorrection is how much to speedup the wavetable reading due to the change from old 21.5Hz to new 13.75Hz.
	static const float wtNewACorrection;

	CSynthTrack();
	virtual ~CSynthTrack();

	void InitEffect(int cmd,int val);
	void PerformFx();
	void NoteOff();
	float GetSample();
	void NoteOn(int note);
	void setGlobalPar(PERFORMANCE*perf);
	void setSampleRate(int currentSR_, int wavetableSize_, float wavetableCorrection_);
	//Stage of the amplitude envelope (used to detect note active)
	int AmpEnvStage;
private:
	void Retrig();
	void DoGlide();
	float Filter(float x);
	float GetEnvAmp();
	void GetEnvVcf();
	void RealNoteOn();

	CSIDFilter filter;
	AAF16 aaf1;
	PERFORMANCE *vpar;
	int sampleRate;
	float srCorrection;
	//in float since it is compared with OSCPosition
	float waveTableSize;
	float wavetableCorrection;

	// Note
	int nextNote;
	float nextvoicevol;
	// base note (note received)
	int cur_basenote;
	// real note,after applying effects (note used to calculate OSCSpeed)
	int cur_realnote;
	// Index inside the waveforms.
	float OSCPosition;
	// (target) OSC speed (samples to advance inside wavetables)
	float OSCSpeed;
	// (running) OSC speed (samples to advance inside wavetables)
	float ROSCSpeed;
	// Amount of glide, if any (amount of samples to increase ROSCSpeed with).
	float oscglide;

	// Envelope [Amplitude]
	float AmpEnvValue;
	float AmpEnvCoef;
	float AmpEnvSustainLevel;
	// Osc volume in 0..1 range (converted from PERFORMANCE.Volume)
	float OSCVol;
	// Volume for this voice  in 0..1 range(contains the 0Cxx command)
	float voicevol;

	// Filter
	int fltMode;
	float VcfCutoff;
	float VcfResonance;

	// Envelope [Filter]
	float VcfEnvValue;
	float VcfEnvCoef;
	float VcfEnvSustainLevel;
	// Stage of the filter envelope
	int VcfEnvStage;
	// filter envelope amount in 0..1 range.
	float VcfEnvMod;

	// Command
	int sp_cmd;
	int sp_val;

	// counter (in samples) until next OSCSpeed and perf_count updates.
	int replaycount;
	// Counter (in samples) until jumping to next state (Note: total amount is perf_count * replaycount)
	int perf_count;
	// Index of which of the 16 states is playing
	int perf_index;

	// (Innecessary?) Controls that RealNoteOn is not called multiple times
	//(this would happen on retrig, if note has stopped or also in fastrelease)
	bool trigger;
	// Controls if retrig option (if selected) should be executed
	bool allowRetrig;
	// Controls if the stop command (gate off) has been executed
	bool stopsent;

	// The index of the waveform table that is in use right now.
	int	cur_waveform;
	// The transpose value in use right now
	int	cur_transpose;
	// The option in use right now
	int	cur_option;
	// The command in use right now
	int	cur_command;
	// The paramter in use right now
	int	cur_parameter;
	// Pitch variation (Inc/dec pitch command)
	float add_to_pitch;
	// The speed in use right now
	int	cur_speed;
	// The current pulse width in use right now
	int cur_pw;
	// final volume, used for anticlick.
	float rcVol;
};
}