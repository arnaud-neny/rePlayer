/*								Blitz (C)2005-2009 by jme
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

#include "blitz.h"
#include <cstdio>
namespace psycle::plugins::jme::blitz12 {
using namespace psycle::plugin_interface;

#define BLITZ_VERSION "1.2.1"
int const IBLITZ_VERSION =0x0121;

CMachineParameter const paraGlobal = {"Global", "Global", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraGlobalVolume = {"Volume", "Volume", 0, 256, MPF_STATE, 128};
CMachineParameter const paraGlobalCoarse = {"Coarse", "Coarse", -60, 60, MPF_STATE, 0};
CMachineParameter const paraGlobalFine = {"Fine", "Fine", -256, 256, MPF_STATE, 0};
CMachineParameter const paraGlobalGlide = {"Glide", "Glide", 0, 255, MPF_STATE, 0};
CMachineParameter const paraGlobalStereo = {"Stereo Switching", "Stereo Switching", 0, 256, MPF_STATE, 0};

CMachineParameter const paraArpeggiator = {"Arpeggiator", "Arpeggiator", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraArpPattern = {"Pattern", "Pattern", 0, 11, MPF_STATE, 0};
CMachineParameter const paraArpSpeed = {"Speed", "Speed", 1, 256, MPF_STATE, 24};
CMachineParameter const paraArpShuffle = {"Shuffle", "Shuffle", 0, 256, MPF_STATE, 0};
CMachineParameter const paraArpRetrig = { "Retrig", "Retrig", 0, 2, MPF_STATE, 0};

CMachineParameter const paraLfo = {"LFO", "LFO", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraLfoDelay = {"Delay", "Delay", 0, 999, MPF_STATE, 0};
CMachineParameter const paraLfoDepth = {"Depth", "Depth", -999, 999, MPF_STATE, 0};
CMachineParameter const paraLfoSpeed = {"Speed", "Speed", 0, 999, MPF_STATE, 0};
CMachineParameter const paraLfoDestination = {"Destination Oscillators", "Destination Oscillators", 0, 7, MPF_STATE, 0};

CMachineParameter const paraOsc1 = {"Oscillator 1", "Oscillator 1", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc1Volume = {"Volume", "Volume", 0, 256, MPF_STATE, 128};
CMachineParameter const paraOsc1Coarse = {"Coarse", "Coarse", -60, 60, MPF_STATE, 0};
CMachineParameter const paraOsc1Fine = {"Fine", "Fine", -256, 256, MPF_STATE, 0};
CMachineParameter const paraOsc1Waveform = {"Waveform", "Waveform", 0, 33, MPF_STATE, 0};
CMachineParameter const paraOsc1Feedback = {"Feedback", "Feedback", 0, 256, MPF_STATE, 0};
CMachineParameter const paraOsc1Options = {"Options", "Options", 0, 12, MPF_STATE, 0};
CMachineParameter const paraOsc1Func = {"Function", "Function", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc1FuncType = {"Type", "Type", 0, 52, MPF_STATE, 0};
CMachineParameter const paraOsc1FuncSym = {"Symmetry", "Symmetry", 0, 2047, MPF_STATE, 1024};
CMachineParameter const paraOsc1SymDrift = {"Symmetry Drift", "Symmetry Drift", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc1SymDriftRange = {"Range", "Range", -2047, 2047, MPF_STATE, 0};
CMachineParameter const paraOsc1SymDriftSpeed = {"Speed", "Speed", 0, 256, MPF_STATE, 0};
CMachineParameter const paraOsc1SymLfo = {"Symmetry LFO", "Symmetry LFO", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc1SymLfoRange = {"Range", "Range", -2047, 2047, MPF_STATE, 0};
CMachineParameter const paraOsc1SymLfoSpeed = {"Speed", "Speed", 0, 256, MPF_STATE, 0};

CMachineParameter const paraOsc2 = {"Oscillator 2", "Oscillator 2", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc2Volume = {"Volume", "Volume", 0, 256, MPF_STATE, 0};
CMachineParameter const paraOsc2Coarse = {"Coarse", "Coarse", -60, 60, MPF_STATE, 0};
CMachineParameter const paraOsc2Fine = {"Fine", "Fine", -256, 256, MPF_STATE, 0};
CMachineParameter const paraOsc2Waveform = {"Waveform", "Waveform", 0, 33, MPF_STATE, 0};
CMachineParameter const paraOsc2Feedback = {"Feedback", "Feedback", 0, 256, MPF_STATE, 0};
CMachineParameter const paraOsc2Options = {"Options", "Options", 0, 12, MPF_STATE, 0};
CMachineParameter const paraOsc2Func = {"Function", "Function", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc2FuncType = {"Type", "Type", 0, 52, MPF_STATE, 0};
CMachineParameter const paraOsc2FuncSym = {"Symmetry", "Symmetry", 0, 2047, MPF_STATE, 1024};
CMachineParameter const paraOsc2SymDrift = {"Symmetry Drift", "Symmetry Drift", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc2SymDriftRange = {"Range", "Range", -2047, 2047, MPF_STATE, 0};
CMachineParameter const paraOsc2SymDriftSpeed = {"Speed", "Speed", 0, 256, MPF_STATE, 0};
CMachineParameter const paraOsc2SymLfo = {"Symmetry LFO", "Symmetry LFO", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc2SymLfoRange = {"Range", "Range", -2047, 2047, MPF_STATE, 0};
CMachineParameter const paraOsc2SymLfoSpeed = {"Speed", "Speed", 0, 256, MPF_STATE, 0};

CMachineParameter const paraOsc3 = {"Oscillator 3", "Oscillator 3", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc3Volume = {"Volume", "Volume", 0, 256, MPF_STATE, 0};
CMachineParameter const paraOsc3Coarse = {"Coarse", "Coarse", -60, 60, MPF_STATE, 0};
CMachineParameter const paraOsc3Fine = {"Fine", "Fine", -256, 256, MPF_STATE, 0};
CMachineParameter const paraOsc3Waveform = {"Waveform", "Waveform", 0, 33, MPF_STATE, 0};
CMachineParameter const paraOsc3Feedback = {"Feedback", "Feedback", 0, 256, MPF_STATE, 0};
CMachineParameter const paraOsc3Options = {"Options", "Options", 0, 12, MPF_STATE, 0};
CMachineParameter const paraOsc3Func = {"Function", "Function", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc3FuncType = {"Type", "Type", 0, 52, MPF_STATE, 0};
CMachineParameter const paraOsc3FuncSym = {"Symmetry", "Symmetry", 0, 2047, MPF_STATE, 1024};
CMachineParameter const paraOsc3SymDrift = {"Symmetry Drift", "Symmetry Drift", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc3SymDriftRange = {"Range", "Range", -2047, 2047, MPF_STATE, 0};
CMachineParameter const paraOsc3SymDriftSpeed = {"Speed", "Speed", 0, 256, MPF_STATE, 0};
CMachineParameter const paraOsc3SymLfo = {"Symmetry LFO", "Symmetry LFO", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc3SymLfoRange = {"Range", "Range", -2047, 2047, MPF_STATE, 0};
CMachineParameter const paraOsc3SymLfoSpeed = {"Speed", "Speed", 0, 256, MPF_STATE, 0};

CMachineParameter const paraOsc4 = {"Oscillator 4", "Oscillator 4", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc4Volume = {"Volume", "Volume", 0, 256, MPF_STATE, 0};
CMachineParameter const paraOsc4Coarse = {"Coarse", "Coarse", -60, 60, MPF_STATE, 0};
CMachineParameter const paraOsc4Fine = {"Fine", "Fine", -256, 256, MPF_STATE, 0};
CMachineParameter const paraOsc4Waveform = {"Waveform", "Waveform", 0, 33, MPF_STATE, 0};
CMachineParameter const paraOsc4Feedback = {"Feedback", "Feedback", 0, 256, MPF_STATE, 0};
CMachineParameter const paraOsc4Options = {"Options", "Options", 0, 12, MPF_STATE, 0};
CMachineParameter const paraOsc4Func = {"Function", "Function", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc4FuncType = {"Type", "Type", 0, 52, MPF_STATE, 0};
CMachineParameter const paraOsc4FuncSym = {"Symmetry", "Symmetry", 0, 2047, MPF_STATE, 1024};
CMachineParameter const paraOsc4SymDrift = {"Symmetry Drift", "Symmetry Drift", -2047, 2047, MPF_LABEL, 0};
CMachineParameter const paraOsc4SymDriftRange = {"Range", "Range", -2047, 2047, MPF_STATE, 0};
CMachineParameter const paraOsc4SymDriftSpeed = {"Speed", "Speed", 0, 256, MPF_STATE, 0};
CMachineParameter const paraOsc4SymLfo = {"Symmetry LFO", "Symmetry LFO", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraOsc4SymLfoRange = {"Range", "Range", -2047, 2047, MPF_STATE, 0};
CMachineParameter const paraOsc4SymLfoSpeed = {"Speed", "Speed", 0, 256, MPF_STATE, 0};

CMachineParameter const paraRM = {"Ring Modulators", "Ring Modulators", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraRM1 = {"RM 1/2 Volume", "RM 1/2 Volume", 0, 256, MPF_STATE, 0};
CMachineParameter const paraRM2 = {"RM 3/4 Volume", "RM 3/4 Volume", 0, 256, MPF_STATE, 0};
CMachineParameter const paraPitch = {"Pitch Mod", "Pitch Mod", 0, 1, MPF_LABEL, 0};

CMachineParameter const paraModA = {"Envelope Attack", "Envelope Attack", 1, MAX_ENV_TIME>>3, MPF_STATE, 1};
CMachineParameter const paraModD = {"Envelope Decay", "Envelope Decay", 1, MAX_ENV_TIME>>3, MPF_STATE, 1};
CMachineParameter const paraModEnvAmount = {"Envelope Amount", "Envelope Amount", -256, 256, MPF_STATE, 0};
CMachineParameter const paraAmp = {"Amplifier Envelope", "Amplifier Envelope", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraAmpA = {"Attack", "Attack", MIN_ENV_TIME, MAX_ENV_TIME, MPF_STATE, MIN_ENV_TIME};
CMachineParameter const paraAmpD = {"Decay", "Decay", MIN_ENV_TIME, MAX_ENV_TIME, MPF_STATE, 4096};
CMachineParameter const paraAmpS = {"Sustain", "Sustain", 0, 255, MPF_STATE, 224};
CMachineParameter const paraAmpD2 = {"Decay 2", "Decay 2", MIN_ENV_TIME, MAX_ENV_TIME, MPF_STATE, 65536};
CMachineParameter const paraAmpR = {"Release", "Release", MIN_ENV_TIME, MAX_ENV_TIME, MPF_STATE, 640};
CMachineParameter const paraAmpScaling = {"Key Scaling", "Key Scaling", 0, 256, MPF_STATE, 150};
CMachineParameter const paraAmpVelocity = {"Velocity", "Velocity", 0, 256, MPF_STATE, 256};
CMachineParameter const paraAmpTrack = {"Soften High Notes", "Soften High Notes", 0, 256, MPF_STATE, 64};

CMachineParameter const paraFlt = {"Filter", "Filter", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraFltType = {"Type", "Type", 0, 10, MPF_STATE, 0};
CMachineParameter const paraFltCutoff = {"Cutoff", "Cutoff", 0, 256, MPF_STATE, 0};
CMachineParameter const paraFltResonance = {"Resonance", "Resonance", 0, 256, MPF_STATE, 0};
CMachineParameter const paraFltTrack = {"Track", "Track", -64, 64, MPF_STATE, 5};
CMachineParameter const paraFltSweep = {"Sweep", "Sweep", -999, 999, MPF_STATE, 0};
CMachineParameter const paraFltSpeed = {"Speed", "Speed", 0, 999, MPF_STATE, 0};
CMachineParameter const paraFltEnv = {"Filter Envelope", "Filter Envelope", 0, 1, MPF_LABEL, 0};
CMachineParameter const paraFltA = {"Attack", "Attack", MIN_ENV_TIME, MAX_ENV_TIME, MPF_STATE, MIN_ENV_TIME};
CMachineParameter const paraFltD = {"Decay", "Decay", MIN_ENV_TIME, MAX_ENV_TIME, MPF_STATE, 4096};
CMachineParameter const paraFltS = {"Sustain", "Sustain", 0, 255, MPF_STATE, 0};
CMachineParameter const paraFltD2 = {"Decay 2", "Decay 2", MIN_ENV_TIME, MAX_ENV_TIME, MPF_STATE, 65536};
CMachineParameter const paraFltR = {"Release", "Release", MIN_ENV_TIME, MAX_ENV_TIME, MPF_STATE, 3074};
CMachineParameter const paraFltScaling = {"Key Scaling", "Key Scaling", 0, 256, MPF_STATE, 150};
CMachineParameter const paraFltVelocity = {"Velocity", "Velocity", 0, 256, MPF_STATE, 32};
CMachineParameter const paraFltEnvAmount = {"Envelope Amount", "Envelope Amount", -256, 256, MPF_STATE, 150};


CMachineParameter const *pParameters[] = {
	//00
	&paraGlobal,
	&paraGlobalVolume,
	&paraGlobalCoarse,
	&paraGlobalFine,
	//04
	&paraGlobalGlide,
	&paraGlobalStereo,
	&paraArpeggiator,
	&paraArpPattern,
	//08
	&paraArpSpeed,
	&paraArpShuffle,
	&paraArpRetrig,
	&paraLfo,
	//12
	&paraLfoDelay,
	&paraLfoDepth,
	&paraLfoSpeed,
	&paraLfoDestination,
	//16
	&paraOsc1,
	&paraOsc1Volume,
	&paraOsc1Coarse,
	&paraOsc1Fine,
	//20
	&paraOsc1Waveform,
	&paraOsc1Feedback,
	&paraOsc1Options,
	&paraOsc1Func,
	//24
	&paraOsc1FuncType,
	&paraOsc1FuncSym,
	&paraOsc1SymDrift,
	&paraOsc1SymDriftRange,
	//28
	&paraOsc1SymDriftSpeed,
	&paraOsc1SymLfo,
	&paraOsc1SymLfoRange,
	&paraOsc1SymLfoSpeed,
	//32
	&paraOsc2,
	&paraOsc2Volume,
	&paraOsc2Coarse,
	&paraOsc2Fine,
	//36
	&paraOsc2Waveform,
	&paraOsc2Feedback,
	&paraOsc2Options,
	&paraOsc2Func,
	//40
	&paraOsc2FuncType,
	&paraOsc2FuncSym,
	&paraOsc2SymDrift,
	&paraOsc2SymDriftRange,
	//44
	&paraOsc2SymDriftSpeed,
	&paraOsc2SymLfo,
	&paraOsc2SymLfoRange,
	&paraOsc2SymLfoSpeed,
	//48
	&paraOsc3,
	&paraOsc3Volume,
	&paraOsc3Coarse,
	&paraOsc3Fine,
	//52
	&paraOsc3Waveform,
	&paraOsc3Feedback,
	&paraOsc3Options,
	&paraOsc3Func,
	//56
	&paraOsc3FuncType,
	&paraOsc3FuncSym,
	&paraOsc3SymDrift,
	&paraOsc3SymDriftRange,
	//60
	&paraOsc3SymDriftSpeed,
	&paraOsc3SymLfo,
	&paraOsc3SymLfoRange,
	&paraOsc3SymLfoSpeed,
	//64
	&paraOsc4,
	&paraOsc4Volume,
	&paraOsc4Coarse,
	&paraOsc4Fine,
	//68
	&paraOsc4Waveform,
	&paraOsc4Feedback,
	&paraOsc4Options,
	&paraOsc4Func,
	//72
	&paraOsc4FuncType,
	&paraOsc4FuncSym,
	&paraOsc4SymDrift,
	&paraOsc4SymDriftRange,
	//76
	&paraOsc4SymDriftSpeed,
	&paraOsc4SymLfo,
	&paraOsc4SymLfoRange,
	&paraOsc4SymLfoSpeed,
	//80
	&paraRM,
	&paraRM1,
	&paraRM2,
	&paraPitch,
	//84
	&paraModA,
	&paraModD,
	&paraModEnvAmount,
	&paraAmp,
	//88
	&paraAmpA,
	&paraAmpD,
	&paraAmpS,
	&paraAmpD2,
	//92
	&paraAmpR,
	&paraAmpScaling,
	&paraAmpVelocity,
	&paraAmpTrack,
	//96
	&paraFlt,
	&paraFltType,
	&paraFltCutoff,
	&paraFltResonance,
	//100
	&paraFltTrack,
	&paraFltSweep,
	&paraFltSpeed,
	&paraFltEnv,
	//104
	&paraFltA,
	&paraFltD,
	&paraFltS,
	&paraFltD2,
	//108
	&paraFltR,
	&paraFltScaling,
	&paraFltVelocity,
	&paraFltEnvAmount
};

CMachineInfo const MacInfo (
	MI_VERSION,
	IBLITZ_VERSION,
	GENERATOR,								
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"Blitz " BLITZ_VERSION
	#ifndef NDEBUG
		" (Debug build)"
	#endif
	,
	"Blitz",
	"jme",
	"Help",
	7
);

PSYCLE__PLUGIN__INSTANTIATOR("blitz12", mi, MacInfo) // To export DLL functions to host

mi::mi()
{
	Vals=new int[MacInfo.numParameters];
	InitWaveTable();
}

mi::~mi()
{
	delete[] Vals;
}

void mi::Init()
{
	for(int i(0);i<4;++i) globals.oscSymDrift[i]=0;
	globals.restartfx=0;
	globals.stereoPos=0;
	slomo=0;
	InitLoop[0].setRange(globals.oscSymDriftRange[0]);
	InitLoop[0].setSpeed(globals.oscSymDriftSpeed[0]);
	InitLoop[0].reset();
	InitLoop[1].setRange(globals.oscSymDriftRange[1]);
	InitLoop[1].setSpeed(globals.oscSymDriftSpeed[1]);
	InitLoop[1].reset();
	InitLoop[2].setRange(globals.oscSymDriftRange[2]);
	InitLoop[2].setSpeed(globals.oscSymDriftSpeed[2]);
	InitLoop[2].reset();
	InitLoop[3].setRange(globals.oscSymDriftRange[3]);
	InitLoop[3].setSpeed(globals.oscSymDriftSpeed[3]);
	InitLoop[3].reset();
	InitPos[0]=InitLoop[0].getPosition();
	InitPos[1]=InitLoop[1].getPosition();
	InitPos[2]=InitLoop[2].getPosition();
	InitPos[3]=InitLoop[3].getPosition();
	for (int c=0;c<MAX_TRACKS;c++) track[c].InitVoice(&globals);
}

void mi::Stop(){
	for(int c=0;c<MAX_TRACKS;c++) track[c].NoteStop();
}

void mi::updateOsc(int osc){
	for(int c=0;c<MAX_TRACKS;c++){
		if(track[c].ampEnvStage) track[c].calcOneWave(osc);
	}
}

void mi::ParameterTweak(int par, int val){
	// Called when a parameter is changed by the host app / user gui
	Vals[par]=val;

	switch (par){
		case 1: globals.globalVolume=val; break;
		case 2: globals.globalCoarse=val; break;
		case 3: globals.globalFine=val; break;
		case 4: globals.globalGlide=val; break;
		case 5: globals.globalStereo=val; if (globals.globalStereo) globals.stereoLR[0]=1.0f-(globals.globalStereo*0.00390625f); else globals.stereoLR[0]=1.0f; globals.stereoLR[1]=1.0f; break;
		case 7: globals.arpPattern=val; break;
		case 8: globals.arpSpeed=val; break;
		case 9: globals.arpShuffle=val; break;
		case 10: globals.arpRetrig=val; break;
		case 12: globals.lfoDelay=val<<3; break;
		case 13: globals.lfoDepth=val; SyncViber.setLevel(val);
			for(int c=0;c<MAX_TRACKS;c++) track[c].changeLfoDepth(val);
			break;
		case 14: globals.lfoSpeed=val; SyncViber.setSpeed(val);
			for(int c=0;c<MAX_TRACKS;c++) track[c].changeLfoSpeed(val);
			break;
		case 15: globals.lfoDestination=val; break;
		case 17: globals.oscVolume[0]=val; break;
		case 18: globals.oscCoarse[0]=val; break;
		case 19: globals.oscFine[0]=val; break;
		case 20: globals.oscWaveform[0]=val; updateOsc(0); break;
		case 21: globals.oscFeedback[0]=(float)val*0.00025f; break;
		case 22: globals.oscOptions[0]=val; break;
		case 24: globals.oscFuncType[0]=val; updateOsc(0); break;
		case 25: globals.oscFuncSym[0]=val; break;
		case 27: globals.oscSymDriftRange[0]=val; InitLoop[0].setRange(globals.oscSymDriftRange[0]); break;
		case 28: globals.oscSymDriftSpeed[0]=val; InitLoop[0].setSpeed(globals.oscSymDriftSpeed[0]); break;
		case 30: globals.oscSymLfoRange[0]=val; break;
		case 31: globals.oscSymLfoSpeed[0]=val; break;
		case 33: globals.oscVolume[1]=val; break;
		case 34: globals.oscCoarse[1]=val; break;
		case 35: globals.oscFine[1]=val; break;
		case 36: globals.oscWaveform[1]=val; updateOsc(1); break;
		case 37: globals.oscFeedback[1]=(float)val*0.00025f; break;
		case 38: globals.oscOptions[1]=val; break;
		case 40: globals.oscFuncType[1]=val; updateOsc(1); break;
		case 41: globals.oscFuncSym[1]=val;				break;
		case 43: globals.oscSymDriftRange[1]=val; InitLoop[1].setRange(globals.oscSymDriftRange[1]); break;
		case 44: globals.oscSymDriftSpeed[1]=val; InitLoop[1].setSpeed(globals.oscSymDriftSpeed[1]); break;
		case 46: globals.oscSymLfoRange[1]=val; break;
		case 47: globals.oscSymLfoSpeed[1]=val; break;
		case 49: globals.oscVolume[2]=val; break;
		case 50: globals.oscCoarse[2]=val; break;
		case 51: globals.oscFine[2]=val; break;
		case 52: globals.oscWaveform[2]=val; updateOsc(2); break;
		case 53: globals.oscFeedback[2]=(float)val*0.00025f; break;
		case 54: globals.oscOptions[2]=val; break;
		case 56: globals.oscFuncType[2]=val; updateOsc(2); break;
		case 57: globals.oscFuncSym[2]=val;				break;
		case 59: globals.oscSymDriftRange[2]=val; InitLoop[2].setRange(globals.oscSymDriftRange[2]); break;
		case 60: globals.oscSymDriftSpeed[2]=val; InitLoop[2].setSpeed(globals.oscSymDriftSpeed[2]); break;
		case 62: globals.oscSymLfoRange[2]=val; break;
		case 63: globals.oscSymLfoSpeed[2]=val; break;
		case 65: globals.oscVolume[3]=val; break;
		case 66: globals.oscCoarse[3]=val; break;
		case 67: globals.oscFine[3]=val; break;
		case 68: globals.oscWaveform[3]=val; updateOsc(3); break;
		case 69: globals.oscFeedback[3]=(float)val*0.00025f; break;
		case 70: globals.oscOptions[3]=val; break;
		case 72: globals.oscFuncType[3]=val; updateOsc(3); break;
		case 73: globals.oscFuncSym[3]=val; break;
		case 75: globals.oscSymDriftRange[3]=val; InitLoop[3].setRange(globals.oscSymDriftRange[3]); break;
		case 76: globals.oscSymDriftSpeed[3]=val; InitLoop[3].setSpeed(globals.oscSymDriftSpeed[3]); break;
		case 78: globals.oscSymLfoRange[3]=val; break;
		case 79: globals.oscSymLfoSpeed[3]=val; break;
		case 81: globals.rm1=val; break;
		case 82: globals.rm2=val; break;
		case 84: globals.modA=val; break;
		case 85: globals.modD=val; break;
		case 86: globals.modEnvAmount=(float)(val<<5); break;
		case 88: globals.ampA=val<<7; break;
		case 89: globals.ampD=val<<7; break;
		case 90: globals.ampS=val; break;
		case 91: if (val==65536) globals.ampD2=val*2048; else globals.ampD2=val<<7; break;
		case 92: globals.ampR=val<<7; break;
		case 93: globals.ampScaling=val; break;
		case 94: globals.ampVelocity=(float)val*0.00390625f; break;
		case 95: globals.ampTrack=(float)val*0.00390625f; break;
		case 97: globals.fltType=val; break;
		case 98: globals.fltCutoff=(float)val; break;
		case 99: globals.fltResonance=val; break;
		case 100: globals.fltTrack=val; break;
		case 101: globals.fltSweep=val; FiltViber.setLevel(val); break;
		case 102: globals.fltSpeed=val; FiltViber.setSpeed(val); break;
		case 104: globals.fltA=val<<7; break;
		case 105: globals.fltD=val<<7; break;
		case 106: globals.fltS=val; break;
		case 107: if (val==65536) globals.fltD2=val*2048; else globals.fltD2=val<<7; break;
		case 108: globals.fltR=val<<7; break;
		case 109: globals.fltScaling=val; break;
		case 110: globals.fltVelocity=(float)val*0.00390625f; break;
		case 111: globals.fltEnvAmount=(float)val; break;
	}
}

void mi::Command(){
// Called when user presses editor button
// Probably you want to show your custom window here
// or an about button
char buffer[2048];

sprintf(
		buffer,"%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
		"Pattern commands\n",
		"\n0Cxx : Set Volume (new!)",
		"\nC1xx : Slide Up",
		"\nC2xx : Slide Down",
		"\nC3xx : Tone Portamento",
		"\nC4xx : Tone Portamento with Retrig",
		"\nC5xx : Interval (> 128 == minus)",
		"\nC6xx : Touchtaping (> 128 == minus)",
		"\nC7xx : Touchtaping with Retrig",
		"\nC8xx : Init Strobe (+1:Drift, +2:SyncVib, +4:FltVib)",
		"\nCCxx : Set Volume (obsolete)",
		"\nDxyy : Semi Portamento (x tones down with rate yy)",
		"\nExyy : Semi Portamento (x tones up with rate yy)",
		"\nBFFF and lower: Arpeggio (xyzz, x=2nd, y=3rd, zz=optional 4th tone)\n"
		);

	pCB->MessBox(buffer,"Help",0);
}

// Work... where all is cooked
void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks){
	//float sl=0;
	//float sr=0;
	float slr[2];
	for(int c=0;c<tracks;c++){
		if(track[c].ampEnvStage){
			float *xpsamplesleft=psamplesleft;
			float *xpsamplesright=psamplesright;
			int xnumsamples=numsamples;
			--xpsamplesleft;
			--xpsamplesright;
			track[c].PerformFx();
			assert(xnumsamples);
			do{
				track[c].GetSample(slr);
				*++xpsamplesleft+=slr[0];
				*++xpsamplesright+=slr[1];
			}while(--xnumsamples);
		}
	}
	slomo++;
	if (slomo > 10){
		slomo=0;
		InitPos[0]=InitLoop[0].getPosition();
		InitPos[1]=InitLoop[1].getPosition();
		InitPos[2]=InitLoop[2].getPosition();
		InitPos[3]=InitLoop[3].getPosition();
		InitLoop[0].next();
		InitLoop[1].next();
		InitLoop[2].next();
		InitLoop[3].next();
		globals.initposition[0]=InitPos[0];
		globals.initposition[1]=InitPos[1];
		globals.initposition[2]=InitPos[2];
		globals.initposition[3]=InitPos[3];								
	}
	SyncViber.next();
	globals.syncvibe=(float)SyncViber.getPosition();
	FiltViber.next();
	globals.filtvibe=(float)FiltViber.getPosition();
}


// Function that describes value on client's displaying
bool mi::DescribeValue(char* txt,int const param, int const value){

	// Glide, Stereo
	if(param==4||param==5){
		if (value == 0) sprintf(txt,"Off");
		else sprintf(txt, "%3.2f%%", (float)value*100/256);
		return true;
	}

	// ArpShuffle
	if(param==9){
		if (value == 0){ sprintf(txt,"Off"); return true;
		} else return false;
	}

	// Arp Pattern
	if (param==7){
		switch(value){
			case 0:sprintf(txt,"1 oct Up");return true;break;
			case 1:sprintf(txt,"2 oct Up");return true;break;
			case 2:sprintf(txt,"3 oct Up");return true;break;
			case 3:sprintf(txt,"1 oct Down");return true;break;
			case 4:sprintf(txt,"2 oct Down");return true;break;
			case 5:sprintf(txt,"3 oct Down");return true;break;
			case 6:sprintf(txt,"1 oct Up&Down");return true;break;
			case 7:sprintf(txt,"2 oct Up&Down");return true;break;
			case 8:sprintf(txt,"3 oct Up&Down");return true;break;
			case 9:sprintf(txt,"1 oct Down&Up");return true;break;
			case 10:sprintf(txt,"2 oct Down&Up");return true;break;
			case 11:sprintf(txt,"3 oct Down&Up");return true;break;
		}
	}
	// Arp Retrig
	if(param==10){
		switch(value){
			case 0:sprintf(txt,"Off"); return true;break;
			case 1:sprintf(txt,"On");return true;break;
			case 2:sprintf(txt,"Even");return true;break;
		}
	}

	// Lfo Destinations
	if(param==15){
		switch(value){
			case 0:sprintf(txt,"1 2 3 4"); return true;break;
			case 1:sprintf(txt,"- 2 3 4");return true;break;
			case 2:sprintf(txt,"- 2 3 -");return true;break;
			case 3:sprintf(txt,"- 2 - 4");return true;break;
			case 4:sprintf(txt,"- - 3 4");return true;break;
			case 5:sprintf(txt,"- 2 - -");return true;break;
			case 6:sprintf(txt,"- - 3 -");return true;break;
			case 7:sprintf(txt,"- - - 4");return true;break;
		}
	}

	// Volume
	if(param==1||param==0x11||param==0x21||param==0x31||param==0x41||param==0x51||param==0x52){
		if (value > 0) sprintf(txt, "%.02f dB", 20.0f * std::log10((float) value / 128.0f));
		else sprintf(txt, "-inf dB");
		return true;
	}

	// Semitones
	if(param==2||param==0x12||param==0x22||param==0x32||param==0x42){
		sprintf(txt, "%i Semitones", value);
		return true;
	}

	// Finetune
	if(param==3||param==0x13||param==0x23||param==0x33||param==0x43){
		sprintf(txt, "%3.2f Cent", (float)value*100/256);
		return true;
	}


	// DCO base waveform descriptions
	if(param==20||param==36||param==52||param==68){
		switch(value){
			case WAVE_SINE:sprintf(txt,"Sine"); return true;break;
			case WAVE_ADLIB2:sprintf(txt,"Adlib 2");return true;break;
			case WAVE_ADLIB3:sprintf(txt,"Adlib 3");return true;break;
			case WAVE_ADLIB4:sprintf(txt,"Adlib 4");return true;break;
			case WAVE_SINE12:sprintf(txt,"Sine 1&2");return true;break;
			case WAVE_SINECOSINE:sprintf(txt,"Sine&back");return true;break;
			case WAVE_HALFSINE:sprintf(txt,"Half Sine");return true;break;
			case WAVE_TRIANGLE:sprintf(txt,"Triangle");return true;break;
			case WAVE_UPDOWN:sprintf(txt,"Up&Down");return true;break;
			case WAVE_UPDOWNX2:sprintf(txt,"Up&Down High");return true;break;
			case WAVE_SAWTOOTH:sprintf(txt,"Sawtooth");return true;break;
			case WAVE_SAWTOOTHX2:sprintf(txt,"Sawtooth High");return true;break;
			case WAVE_EXPSAW:sprintf(txt,"Exp.Saw"); return true;break;
			case WAVE_MIDSINE:sprintf(txt,"Mid Sine");return true;break;
			case WAVE_STEPS:sprintf(txt,"Steps");return true;break;
			case WAVE_SAWSQUARE:sprintf(txt,"Saw Square");return true;break;
			case WAVE_SQUARE:sprintf(txt,"Square");return true;break;
			case WAVE_SQUAREX2:sprintf(txt,"Square High");return true;break;
			case WAVE_TRISTATE:sprintf(txt,"Tristate");return true;break;
			case WAVE_POLYNEG:sprintf(txt,"Digital 1");return true;break;
			case WAVE_POLYNEG2:sprintf(txt,"Digital 2");return true;break;
			case WAVE_POLYNEG3:sprintf(txt,"Digital 3");return true;break;
			case WAVE_FORMANT2:sprintf(txt,"Formant 2");return true;break;
			case WAVE_FORMANT3:sprintf(txt,"Formant 3");return true;break;
			case WAVE_FORMANT4:sprintf(txt,"Formant 4");return true;break;
			case WAVE_FORMANT5:sprintf(txt,"Formant 5");return true;break;
			case WAVE_FORMANT6:sprintf(txt,"Formant 6");return true;break;
			case WAVE_FORMANT8:sprintf(txt,"Formant 8");return true;break;
			case WAVE_FORMANT10:sprintf(txt,"Formant 10");return true;break;
			case WAVE_RANDOM:sprintf(txt,"Random 1");return true;break;
			case WAVE_RANDOM2:sprintf(txt,"Random 2");return true;break;
			case WAVE_RANDOM3:sprintf(txt,"Random 3");return true;break;
			case WAVE_RANDOM4:sprintf(txt,"Random 4");return true;break;
			case WAVE_RANDOM5:sprintf(txt,"Random 5");return true;break;
		}
	}

	// DCO feedback
	if(param==21||param==37||param==53||param==69){
		sprintf(txt,"%3.2f%%",(float)value*100/256);
		return true;
	}

	// DCO base waveform descriptions
	if(param==22||param==38||param==54||param==70){
		switch(value){
			case 0:sprintf(txt,"Off");return true;break;
			case 1:sprintf(txt,"Sync");return true;break;
			case 2:sprintf(txt,"Arpless Sync");return true;break;
			case 3:sprintf(txt,"Arp Note 1");return true;break;
			case 4:sprintf(txt,"Arp Note 2");return true;break;
			case 5:sprintf(txt,"Arp Note 3");return true;break;
			case 6:sprintf(txt,"Arp Note 4");return true;break;
			case 7:sprintf(txt,"No Track");return true;break;
			case 8:sprintf(txt,"No Track + Sync");return true;break;
			case 9:sprintf(txt,"Phase Reset");return true;break;
			case 10:sprintf(txt,"LFO Oneshot (A/D)");return true;break;
			case 11:sprintf(txt,"LFO Oneshot (A&D)");return true;break;
			case 12:sprintf(txt,"Phase Reset All Osc");return true;break;
		}
	}

	// Filter
	if (param==97)
	{
		switch(value)
		{
		case 0:sprintf(txt,"Off");return true;break;
		case 1:sprintf(txt,"Lowpass");return true;break;
		case 2:sprintf(txt,"Hipass");return true;break;
		case 3:sprintf(txt,"Bandpass");return true;break;
		case 4:sprintf(txt,"Bandreject");return true;break;
		case 5:sprintf(txt,"EQ1+");return true;break;
		case 6:sprintf(txt,"EQ1-");return true;break;
		case 7:sprintf(txt,"EQ2+");return true;break;
		case 8:sprintf(txt,"EQ2-");return true;break;
		case 9:sprintf(txt,"EQ3+");return true;break;
		case 10:sprintf(txt,"EQ3-");return true;break;
		}
	}

	// DCO function descriptions
	if(param==24||param==40||param==56||param==72){
		switch(value){
			case 0:sprintf(txt,"Off");return true;break;
			case 1:sprintf(txt,"Pulsewidth");return true;break;
			case 2:sprintf(txt,"Pulsewidth Medium");return true;break;
			case 3:sprintf(txt,"Pulsewidth Light");return true;break;
			case 4:sprintf(txt,"Dual Squash");return true;break;
			case 5:sprintf(txt,"Muted Sync");return true;break;
			case 6:sprintf(txt,"Sync Up");return true;break;
			case 7:sprintf(txt,"Sync Down");return true;break;
			case 8:sprintf(txt,"Negator");return true;break;
			case 9:sprintf(txt,"Dual Negator");return true;break;
			case 10:sprintf(txt,"Rect Negator");return true;break;
			case 11:sprintf(txt,"Octaver");return true;break;
			case 12:sprintf(txt,"Frequency Shift Keying");return true;break;
			case 13:sprintf(txt,"Mixer");return true;break;
			case 14:sprintf(txt,"Dual Mixer");return true;break;
			case 15:sprintf(txt,"Feedback Mixer");return true;break;
			case 16:sprintf(txt,"Invert Mixer");return true;break;
			case 17:sprintf(txt,"Triangle Mixer");return true;break;
			case 18:sprintf(txt,"Sawtooth Mixer");return true;break;
			case 19:sprintf(txt,"Square Mixer");return true;break;
			case 20:sprintf(txt,"Tremolo");return true;break;
			case 21:sprintf(txt,"Sine PM 1");return true;break;
			case 22:sprintf(txt,"Sine PM 2");return true;break;
			case 23:sprintf(txt,"Sine PM 3");return true;break;
			case 24:sprintf(txt,"Adlib2 PM 1");return true;break;
			case 25:sprintf(txt,"Adlib2 PM 2");return true;break;
			case 26:sprintf(txt,"Adlib2 PM 3");return true;break;
			case 27:sprintf(txt,"Adlib3 PM 1");return true;break;
			case 28:sprintf(txt,"Adlib3 PM 2");return true;break;
			case 29:sprintf(txt,"Adlib3 PM 3");return true;break;
			case 30:sprintf(txt,"Adlib4 PM 1");return true;break;
			case 31:sprintf(txt,"Adlib4 PM 2");return true;break;
			case 32:sprintf(txt,"Adlib4 PM 3");return true;break;
			case 33:sprintf(txt,"Wave PM 1");return true;break;
			case 34:sprintf(txt,"Wave PM 2");return true;break;
			case 35:sprintf(txt,"Wave PM 3");return true;break;
			case 36:sprintf(txt,"Dual Fix PM");return true;break;
			case 37:sprintf(txt,"Multiply");return true;break;
			case 38:sprintf(txt,"AND Gate");return true;break;
			case 39:sprintf(txt,"XOR Gate");return true;break;
			case 40:sprintf(txt,"Boost (Hard Clip)");return true;break;
			case 41:sprintf(txt,"RM to AM (Upright)");return true;break;
			case 42:sprintf(txt,"RM to AM (Flipped)");return true;break;
			case 43:sprintf(txt,"Feedback Control");return true;break;
			case 44:sprintf(txt,"FM next Oscillator (+)");return true;break;
			case 45:sprintf(txt,"FM next Oscillator (-)");return true;break;
			case 46:sprintf(txt,"Filter Modulation");return true;break;
			case 47:sprintf(txt,"Master Saturation");return true;break;
			case 48:sprintf(txt,"FM last Oscillator (+)");return true;break;
			case 49:sprintf(txt,"FM last Oscillator (-)");return true;break;
			case 50:sprintf(txt,"X Rotator");return true;break;
			case 51:sprintf(txt,"Y Rotator");return true;break;
			case 52:sprintf(txt,"Boost II (Wrap)");return true;break;

		}
	}

	//Symmetry
	if(param==25||param==41||param==57||param==73||param==27||param==43||param==59||param==75||param==30||param==46||param==62||param==78){
		float fnord=(float)value;
		if (fnord==1024.0f) fnord=1023.5f;
		sprintf(txt,"%3.2f%%",(float)fnord*100/2047);
		return true;
	}

	//pitchenv, ksc1, velo1, soften, reso, ksc2, velo2, fltenv
	if(param==0x56||param==0x5d||param==0x5e||param==0x05f||param==0x63||param==0x6d||param==0x6e||param==0x6f){
		sprintf(txt,"%3.2f%%",(float)value*100/256);
		return true;
	}

	//pitchLfo, fltLfo
	if (param==0x0d || param==0x65){
		sprintf(txt,"%3.2f%%",(float)value*100/999);
		return true;
	}

	//fltTrack
	if (param==0x64){
		sprintf(txt,"%3.2f%%",(float)value*100/64);
		return true;
	}


	return false;
}

//////////////////////////////////////////////////////////////////////
// The SeqTick function where your notes and pattern command handlers
// should be processed. Called each tick.
// Is called by the host sequencer
	
void mi::SeqTick(int channel, int note, int ins, int cmd, int val){

	if (cmd == 0xC8){
		if (val&1){
			InitLoop[0].reset();
			InitLoop[1].reset();
			InitLoop[2].reset();
			InitLoop[3].reset();
			InitLoop[0].next();
			InitLoop[1].next();
			InitLoop[2].next();
			InitLoop[3].next();
			InitPos[0]=InitLoop[0].getPosition();
			InitPos[1]=InitLoop[1].getPosition();
			InitPos[2]=InitLoop[2].getPosition();
			InitPos[3]=InitLoop[3].getPosition();
			globals.initposition[0]=InitPos[0];
			globals.initposition[1]=InitPos[1];
			globals.initposition[2]=InitPos[2];
			globals.initposition[3]=InitPos[3];
			for (int c=0; c<MAX_TRACKS; c++) track[channel].ResetSym();
		}
		if (val&2){
			SyncViber.reset();
			SyncViber.next();
			globals.syncvibe=(float)SyncViber.getPosition();
		}
		if (val&4){
			FiltViber.reset();
			FiltViber.next();
			globals.filtvibe=(float)FiltViber.getPosition();
		}
	}

	if (channel < MAX_TRACKS){
		float nextVol = 1.0f;
		if ((note<=NOTE_MAX) & ((cmd == 0xCC)||cmd == 0x0C )){
			nextVol=(float)val/255.0f;
			track[channel].InitEffect(cmd,val);
		}
		else track[channel].InitEffect(cmd,val);
		// Note Off												== 120
		// Empty Note Row				== 255
		// Less than note off value??? == NoteON!
		if(note<=NOTE_MAX){ 
			if (cmd != 0xC3) track[channel].NoteOn(note-24,&globals,60,nextVol);
			else {
				if (track[channel].ampEnvStage) track[channel].NoteTie(note-24);
				else track[channel].NoteOn(note-24,&globals,60,nextVol); // retrigger because volume envelope finished
			}
		}
		// Note off
		if(note==NOTE_NOTEOFF) track[channel].NoteOff();
	}
}

void mi::SequencerTick(){
// Called on each tick while sequencer is playing
}


void mi::InitWaveTable(){
	int co = 0;
	int cp = 0;
	float float1 = 16384;
	double quarter = 512*0.00306796157577128245943617517898389;
	for(int c=0;c<2048;c++){
		double sval=(double)c*0.00306796157577128245943617517898389;

		//MidSine
		globals.WaveTable[WAVE_MIDSINE][c]=int(sin((sval*0.5)+quarter)*16384.0f);

		//HalfSine
		globals.WaveTable[WAVE_HALFSINE][c]=int(sin(sval*0.5)*32768.0f -16384);

		//Sine
		globals.WaveTable[WAVE_SINE][c]=int(sin(sval)*16384.0f);

		//Sine 1+2
		globals.WaveTable[WAVE_SINE12][c]=int((sin(sval)*16384.0f)+(sin(sval+sval)*8192.0f) );
		//Up&Down
		if(c<1024) {
			globals.WaveTable[WAVE_UPDOWN][c]=(c*32)-16384;
		} else {
			globals.WaveTable[WAVE_UPDOWN][c]=16384-((c-1024)*32);
		}
		//ExpSaw
		globals.WaveTable[WAVE_EXPSAW][c]=int(sin(sval/4)*32768.0f -16384);

		//TriState
		if(c<1024) {
			if (c<512){
				globals.WaveTable[WAVE_TRISTATE][c]=0;
			}else{
				globals.WaveTable[WAVE_TRISTATE][c]=16384;
			}
		}else{
			if (c<1536){
				globals.WaveTable[WAVE_TRISTATE][c]=0;
			}else{
				globals.WaveTable[WAVE_TRISTATE][c]=-16384;
			}
		}
		//Square
		if(c<1024){
			globals.WaveTable[WAVE_SQUARE][c]=16384;
		}else{
			globals.WaveTable[WAVE_SQUARE][c]=-16384;
		}
		//Steps
		if(c<1024) {
			if (c<512){
				globals.WaveTable[WAVE_STEPS][c]=16384;
			}else{
				globals.WaveTable[WAVE_STEPS][c]=5461;
			}
		}else{
			if (c<1536){
				globals.WaveTable[WAVE_STEPS][c]=-5461;
			}else{
				globals.WaveTable[WAVE_STEPS][c]=-16384;
			}
		}
		//Random
		globals.WaveTable[WAVE_RANDOM][c]=rand()-16384;

		//Formants
		for (co=2;co<7;co++){
			globals.WaveTable[WAVE_FORMANT2+co-2][c] = int(sin(sval*co)*float1);
		}
		globals.WaveTable[WAVE_FORMANT8][c] = int(sin(sval*8)*float1);
		globals.WaveTable[WAVE_FORMANT10][c] = int(sin(sval*10)*float1);
		float1-=8;
	}
	//Sawtooth (Phase+0.5)
	co=16384;
	for(int c=0;c<2048;c++)
	{
		if (co>=32768) co-=32768;
		globals.WaveTable[WAVE_SAWTOOTH][c]=co-16384;
		co+=16;
	}
	//Triangle
	co = 512;
	for(int c=0; c<2048;c++)
	{
		globals.WaveTable[WAVE_TRIANGLE][c]=globals.WaveTable[WAVE_UPDOWN][co];
		co++;
		if (co>=2048) co-=2048;

	}
	//Sawsquare
	for(int c=0; c<1024;c++)
	{
		globals.WaveTable[WAVE_SAWSQUARE][c]=globals.WaveTable[WAVE_SAWTOOTH][c];
	} 
	for(int c=0; c<1024;c++)
	{
		globals.WaveTable[WAVE_SAWSQUARE][c+1024]=0-globals.WaveTable[WAVE_SAWTOOTH][c];
	}
	//Sine Cosine
	for(int c=0; c<1024;c++) {
		globals.WaveTable[WAVE_SINECOSINE][c]=globals.WaveTable[WAVE_SINE][c+c];
		globals.WaveTable[WAVE_SINECOSINE][2047-c]=globals.WaveTable[WAVE_SINE][c+c];
	}

	for(int c=0; c<512;c++) { globals.WaveTable[WAVE_SINECOSINE][c]=globals.WaveTable[WAVE_SINE][c+c]; }
	for(int c=0; c<512;c++) { globals.WaveTable[WAVE_SINECOSINE][c+512]=globals.WaveTable[WAVE_SINE][c+c]; }
	for(int c=0; c<512;c++) { globals.WaveTable[WAVE_SINECOSINE][c+1024]=globals.WaveTable[WAVE_SINE][c+c+1024]; }
	for(int c=0; c<512;c++) { globals.WaveTable[WAVE_SINECOSINE][c+1536]=globals.WaveTable[WAVE_SINE][c+c+1024]; }

	//PolyDIGITAL Sine
	co = 256;
	cp = 0;
	for(int c=0;c<2048;c++)
	{
		if (cp == 1){
			globals.WaveTable[WAVE_POLYNEG][c]=globals.WaveTable[WAVE_SINE][c];
		}else{
			globals.WaveTable[WAVE_POLYNEG][c]=0-globals.WaveTable[WAVE_SINE][c];
		}
		co--;
		if (co==0){
				co = 256;
				cp = 1-cp;
		}
	}
	//PolyDIGITAL Sine 2
	co = 128;
	cp = 0;
	for(int c=0;c<2048;c++)
	{
		if (cp == 1){
			globals.WaveTable[WAVE_POLYNEG2][c]=globals.WaveTable[WAVE_SINE][c];
		}else{
			globals.WaveTable[WAVE_POLYNEG2][c]=0-globals.WaveTable[WAVE_SINE][c];
		}
		co--;
		if (co==0){
				co = 128;
				cp = 1-cp;
		}
	}
	//PolyDIGITAL Sine 3
	co = 64;
	cp = 0;
	for(int c=0;c<2048;c++)
	{
		if (cp == 1){
			globals.WaveTable[WAVE_POLYNEG3][c]=globals.WaveTable[WAVE_SINE][c];
		}else{
			globals.WaveTable[WAVE_POLYNEG3][c]=0-globals.WaveTable[WAVE_SINE][c];
		}
		co--;
		if (co==0){
				co = 64;
				cp = 1-cp;
		}
	}
	for(int c=0;c<2048;c++){
		if(c<1024){ globals.WaveTable[WAVE_ADLIB2][c]=globals.WaveTable[WAVE_SINE][c]*2 -16384; //ADLIB2
		} else {								globals.WaveTable[WAVE_ADLIB2][c]=0-16384;}
		if(c<1024){ globals.WaveTable[WAVE_ADLIB3][c]=globals.WaveTable[WAVE_SINE][c]*2 -16384; //ADLIB3
		} else {								globals.WaveTable[WAVE_ADLIB3][c]=globals.WaveTable[WAVE_SINE][c-1024]*2 -16384;}
		if((c&1023)<512){ globals.WaveTable[WAVE_ADLIB4][c]=globals.WaveTable[WAVE_SINE][c&1023]*2 -16384; //ADLIB4
		} else {								globals.WaveTable[WAVE_ADLIB4][c]=0-16384;}

		//SawtoothX2
		if(c<1024) {
			globals.WaveTable[WAVE_SAWTOOTHX2][c]=globals.WaveTable[WAVE_SAWTOOTH][c+c];
			globals.WaveTable[WAVE_SAWTOOTHX2][c+1024]=globals.WaveTable[WAVE_SAWTOOTH][c+c];
		}
		//Up&DownX2
		if(c<1024) {
			globals.WaveTable[WAVE_UPDOWNX2][c]=globals.WaveTable[WAVE_UPDOWN][c+c];
			globals.WaveTable[WAVE_UPDOWNX2][c+1024]=globals.WaveTable[WAVE_UPDOWN][c+c];
		}
		//SquareX2
		if(c<1024) {
			globals.WaveTable[WAVE_SQUAREX2][c]=globals.WaveTable[WAVE_SQUARE][c+c];
			globals.WaveTable[WAVE_SQUAREX2][c+1024]=globals.WaveTable[WAVE_SQUARE][c+c];
		}

		globals.WaveTable[WAVE_RANDOM2][c]=globals.WaveTable[WAVE_RANDOM][c>>3];
		globals.WaveTable[WAVE_RANDOM3][c]=globals.WaveTable[WAVE_RANDOM][c>>4];
		globals.WaveTable[WAVE_RANDOM4][c]=globals.WaveTable[WAVE_RANDOM][c>>5];
		globals.WaveTable[WAVE_RANDOM5][c]=globals.WaveTable[WAVE_RANDOM][c>>6];
	}
}

}