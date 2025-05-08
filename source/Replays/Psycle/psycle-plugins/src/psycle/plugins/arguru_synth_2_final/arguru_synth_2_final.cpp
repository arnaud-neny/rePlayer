#include "SynthTrack.hpp"
#include <psycle/plugin_interface.hpp>
#include <psycle/helpers/math.hpp>
#include <universalis/stdlib/cstdint.hpp>
#include <cstdlib>
#include <cstdio>

namespace psycle::plugins::arguru_synth_2_final {

using namespace psycle::plugin_interface;
using namespace psycle::helpers;
using namespace psycle::helpers::math;
using namespace universalis::stdlib;

int const WAVETABLES = 6;
int const WAVE_REAL_NOISE = 6;
int const MAX_ENV_TIME = 220500;

CMachineParameter const paraOSC1wave = {"OSC1 Wave", "OSC1 Wave", 0, WAVETABLES, MPF_STATE, 1};
CMachineParameter const paraOSC2wave = {"OSC2 Wave", "OSC2 Wave", 0, WAVETABLES, MPF_STATE, 1};
CMachineParameter const paraOSC2detune = {"OSC2 Detune", "OSC2 Detune", -36, 36, MPF_STATE, 0};
CMachineParameter const paraOSC2finetune = {"OSC2 Finetune", "OSC2 Finetune", 0, 256, MPF_STATE, 27};
CMachineParameter const paraOSC2sync = {"OSC2 Sync", "OSC2 Sync", 0, 1, MPF_STATE, 0};
CMachineParameter const paraVCAattack = {"VCA Attack", "VCA Attack", 32, MAX_ENV_TIME, MPF_STATE, 32};
CMachineParameter const paraVCAdecay = {"VCA Decay", "VCA Decay", 32, MAX_ENV_TIME, MPF_STATE, 6341};
CMachineParameter const paraVCAsustain = {"VCA Sustain", "VCA Sustain level", 0, 256, MPF_STATE, 0};
CMachineParameter const paraVCArelease = {"VCA Release", "VCA Release", 32, MAX_ENV_TIME, MPF_STATE, 2630};
CMachineParameter const paraVCFattack = {"VCF Attack", "VCF Attack", 32, MAX_ENV_TIME, MPF_STATE, 589};
CMachineParameter const paraVCFdecay = {"VCF Decay", "VCF Decay", 32, MAX_ENV_TIME, MPF_STATE, 2630};
CMachineParameter const paraVCFsustain = {"VCF Sustain", "VCF Sustain level", 0, 256, MPF_STATE, 0};
CMachineParameter const paraVCFrelease = {"VCF Release", "VCF Release", 32, MAX_ENV_TIME, MPF_STATE, 2630};
CMachineParameter const paraVCFlfospeed = {"VCF LFO Speed", "VCF LFO Speed", 1, 65536, MPF_STATE, 32};
CMachineParameter const paraVCFlfoamplitude = {"VCF LFO Amplitude", "VCF LFO Amplitude", 0, 240, MPF_STATE, 0};
CMachineParameter const paraVCFcutoff = {"VCF Cutoff", "VCF Cutoff", 0, 240, MPF_STATE, 120};
CMachineParameter const paraVCFresonance = {"VCF Resonance", "VCF Resonance", 1, 240, MPF_STATE, 128};
CMachineParameter const paraVCFtype = {"VCF Type", "VCF Type", 0, 19, MPF_STATE, 0};
CMachineParameter const paraVCFenvmod = {"VCF Envmod", "VCF Envmod", -240, 240, MPF_STATE, 80};
CMachineParameter const paraOSCmix = {"OSC Mix", "OSC Mix", 0, 256, MPF_STATE, 128};
CMachineParameter const paraOUTvol = {"Volume", "Volume", 0, 256, MPF_STATE, 128};
CMachineParameter const paraARPmode = {"Arpeggiator", "Arpeggiator", 0, 16, MPF_STATE, 0};
CMachineParameter const paraARPbpm = {"Arpeggio tempo", "Arp. BPM", 32, 1024, MPF_STATE, 125};
CMachineParameter const paraARPcount = {"Arpeggio Steps", "Arp. Steps", 0, 16, MPF_STATE, 4};
CMachineParameter const paraGlobalDetune = {"Glb. Detune", "Global Detune", -36, 36, MPF_STATE, 1};
// Why is the default tuning +1 +60?
// Answer:
// The original implementation generated a wavetable of 2048 samples.
// At 44100Hz (which Asynth assumed), this is a ~21.5Hz signal.
// A standard tune is for A-5 (i.e, note 69) to be 440Hz, which is
// ~ 20.4335 times the sampled signal.
// log2 of this value is ~ 4.3528.
// Multiply this amount by 12 (notes/octave) to get 52.2344, which stands
// for note 52 and finetune 0.2344.
// There was a compensation in the SeqTick() call which did note-18 ( 69-18 = 51 )
// So to correctly compensate, 1 seminote and fine of 60 is added (0.2344 * 256 ~ 60.01)
//
// With the new implementation where the wavetable is generated depending
// on the sampling rate, this correction is maintained for compatibility, and substracted in parameterTweak.
CMachineParameter const paraGlobalFinetune = {"Gbl. Finetune", "Global Finetune", -256, 256, MPF_STATE, 60};
CMachineParameter const paraGlide = {"Glide Depth", "Glide Depth", 0, 255, MPF_STATE, 0};
CMachineParameter const paraInterpolation = {"Resampling", "Resampling method", 0, 1, MPF_STATE, 0};

CMachineParameter const *pParameters[] = {
	&paraOSC1wave,
	&paraOSC2wave,
	&paraOSC2detune,
	&paraOSC2finetune,
	&paraOSC2sync,
	&paraVCAattack,
	&paraVCAdecay,
	&paraVCAsustain,
	&paraVCArelease,
	&paraVCFattack,
	&paraVCFdecay,
	&paraVCFsustain,
	&paraVCFrelease,
	&paraVCFlfospeed,
	&paraVCFlfoamplitude,
	&paraVCFcutoff,
	&paraVCFresonance,
	&paraVCFtype,
	&paraVCFenvmod,
	&paraOSCmix,
	&paraOUTvol,
	&paraARPmode,
	&paraARPbpm,
	&paraARPcount,
	&paraGlobalDetune,
	&paraGlobalFinetune,
	&paraGlide,
	&paraInterpolation
};


CMachineInfo const MacInfo (
	MI_VERSION,
	0x0250,
	GENERATOR,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"Arguru Synth 2"
		#ifndef NDEBUG
		" (debug build)"
		#endif
		,
	"Arguru Synth",
	"J. Arguelles (arguru) and psycledelics",
	"help",
	4
);

class mi : public CMachineInterface {
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

	private:
		void InitWaveTableSR(bool delArray=false);
		float GetAsFrequency(int top);
		float *WaveTable[WAVETABLES];
		uint32_t waveTableSize;
		float wavetableCorrection;
		CSynthTrack track[MAX_TRACKS];
	
		SYNPAR globalpar;
		bool reinitChannel[MAX_TRACKS];
		uint32_t currentSR;
		int fxsamples;
};

PSYCLE__PLUGIN__INSTANTIATOR("arguru-synth-2f", mi, MacInfo)

mi::mi() {
	Vals = new int[MacInfo.numParameters];
	//Initialize here only those things that don't depend on
	//external values (like sampling rate)
	memset(&globalpar,0,sizeof(SYNPAR));
	for(int i = 0; i < MAX_TRACKS; ++i) {
		track[i].setGlobalPar(&globalpar);
		reinitChannel[i] = false;
	}
	//this prevents a warning in valgrind when calling parameterTweak
	for(int i=0; i < MacInfo.numParameters; i++) {
		Vals[i]=0;
	}
	fxsamples=256;
}

mi::~mi() {
	delete[] Vals;
	// Destroy dinamically allocated objects/memory here
	for (int i=0;i < WAVETABLES; i++) {
		delete[] WaveTable[i];
	}
}

void mi::Init() {
	currentSR=pCB->GetSamplingRate();
	// Initialize your stuff here (you can use pCB here without worries)
	InitWaveTableSR();
	for (int i = 0; i < MAX_TRACKS; ++i) {
		track[i].setSampleRate(currentSR, waveTableSize, wavetableCorrection);
	}
	fxsamples=(256*currentSR)/44100;
}

void mi::Stop() {
	for(int c=0;c<MAX_TRACKS;c++)
		track[c].NoteOff(true);
}

void mi::SequencerTick() {
	if (currentSR != pCB->GetSamplingRate()) {
		Stop();
		currentSR = pCB->GetSamplingRate();
		InitWaveTableSR(true);
		for (int i = 0; i < MAX_TRACKS; ++i) {
			track[i].setSampleRate(currentSR, waveTableSize, wavetableCorrection);
		}
		//force an update of all the parameters.
		ParameterTweak(-1,-1);
		fxsamples=(256*currentSR)/44100;
	}
	for (int i = 0; i < MAX_TRACKS; ++i) reinitChannel[i] = true;
}

void mi::ParameterTweak(int par, int val) {
	// Called when a parameter is changed by the host app / user gui
	if (par >= 0 ) { Vals[par]=val; }
	float multiplier = (float)currentSR/44100.0f;

	if (Vals[0] == WAVE_REAL_NOISE) {
		globalpar.wave1noise=true;
	}
	else {
		globalpar.wave1noise=false;
		globalpar.pWave=WaveTable[Vals[0]];
	}
	if (Vals[1] == WAVE_REAL_NOISE) {
		globalpar.wave2noise=true;
	}
	else {
		globalpar.wave2noise=false;
		globalpar.pWave2=WaveTable[Vals[1]];
	}
	
	globalpar.osc2detune=(float)Vals[2];
	globalpar.osc2finetune=(float)Vals[3]*0.00389625f;
	globalpar.osc2sync=(Vals[4]>0);
	
	//All parameters that are sample rate dependant are corrected here.
	globalpar.amp_env_attack=(float)Vals[5]*multiplier;
	globalpar.amp_env_decay=(float)Vals[6]*multiplier;
	globalpar.amp_env_sustain=Vals[7];
	globalpar.amp_env_release=(float)Vals[8]*multiplier;

	globalpar.vcf_env_attack=(float)Vals[9]*multiplier;
	globalpar.vcf_env_decay=(float)Vals[10]*multiplier;
	globalpar.vcf_env_sustain=Vals[11];
	globalpar.vcf_env_release=(float)Vals[12]*multiplier;
	//in case of lfo_speed it is a division.
	globalpar.vcf_lfo_speed=(float)Vals[13]/multiplier;
	globalpar.vcf_lfo_amplitude=Vals[14];
	if(par == 13 || par == 14 ) {
		for(int channel=0; channel<MAX_TRACKS;channel++) {
			if(track[channel].AmpEnvStage != 0) {
				track[channel].InitLfo(globalpar.vcf_lfo_speed,globalpar.vcf_lfo_amplitude);
			}
		}

	}

	globalpar.vcf_cutoff=Vals[15];
	globalpar.vcf_resonance=Vals[16];
	if(par == 15 || par == 16) {
		for(int channel=0; channel<MAX_TRACKS;channel++) {
			if(track[channel].AmpEnvStage != 0) {
				track[channel].InitEnvelopes(false);
			}
		}
	}
	globalpar.vcf_type=Vals[17];
	globalpar.vcf_envmod=Vals[18];
	globalpar.osc_mix=Vals[19];
	globalpar.out_vol=Vals[20];
	globalpar.arp_mod=Vals[21];
	globalpar.arp_bpm=Vals[22];
	globalpar.arp_cnt=Vals[23];
	//With the new wavetable, we remove the compensation
	globalpar.globaldetune=Vals[24]-1;
	//With the new wavetable, we remove the compensation
	globalpar.globalfinetune=Vals[25]-60;
	if (par == 26 && val > paraGlide.MaxValue) {
		Vals[26] = paraGlide.MaxValue;
	}
	globalpar.synthglide=Vals[26];
	globalpar.interpolate=Vals[27];
}

void mi::Command() {
	// Called when user presses editor button
	// Probably you want to show your custom window here
	// or an about button
	char buffer[2048];

	std::sprintf(
			buffer,"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
			"Pattern commands\n",
			"\n01xx : Pitch slide-up",
			"\n02xx : Pitch slide-down",
			"\n03xx : Pitch glide",
			"\n04xy : Vibrato [x=depth, y=speed]",
			"\n06xy : Change vca sustain",
			"\n07xx : Change vcf env modulation [$00=-128, $80=0, $FF=+128]",
			"\n08xx : Change vcf cutoff frequency",
			"\n09xx : Change vcf resonance amount",
			"\n0Axx : Synth volume",
			"\n0Bxx : Change vca attack (*22 millis)",
			"\n0Cxx : Note volume",
			"\n0Exx : NoteCut in xx/2 milliseconds",
			"\n0Fxx : Change Glide"
			"\n11xx : Vcf cutoff slide-up",
			"\n12xx : Vcf cutoff slide-down\0"
			);

	pCB->MessBox(buffer,"·-=<([aRgUrU's SYNTH 2.5])>=-·",0);
}

// Work... where all is cooked 
void mi::Work(float *psamplesleft, float *psamplesright , int numsamples,int tracks) {
	while (numsamples > 0 ) {
		if(fxsamples == 0) {
			for(int c=0;c<tracks;c++){
				if(track[c].AmpEnvStage){
					track[c].PerformFx();
				}
			}
			fxsamples=(256*currentSR)/44100;
		}

		int minimum = std::min(numsamples,fxsamples);
		for(int c=0;c<tracks;c++) {
			if(track[c].AmpEnvStage) {
				float *xpsamplesleft=psamplesleft;
				float *xpsamplesright=psamplesright;
				--xpsamplesleft;
				--xpsamplesright;
				
				CSynthTrack *ptrack=&track[c];
				if(reinitChannel[c]) {
					ptrack->InitEffect(0,0);
					reinitChannel[c]=false;
				}
				//Allowing it to change while note is active
				if (ptrack->ArpMode!=globalpar.arp_mod) {
					ptrack->ArpMode=globalpar.arp_mod;
				}

				int xnumsamples = minimum;
				if(globalpar.osc_mix == 0) {
					do {
						const float sl=ptrack->GetSampleOsc1();
						*++xpsamplesleft+=sl;
						*++xpsamplesright+=sl;
					} while(--xnumsamples);
				}
				else if(globalpar.osc_mix == 256) {
					do {
						const float sl=ptrack->GetSampleOsc2();
						*++xpsamplesleft+=sl;
						*++xpsamplesright+=sl;
					} while(--xnumsamples);
				}
				else {
					do {
						const float sl=ptrack->GetSample();
						*++xpsamplesleft+=sl;
						*++xpsamplesright+=sl;
					} while(--xnumsamples);
				}
				if(ptrack->NoteCutTime >0) ptrack->NoteCutTime-=minimum;
			}
		}
		fxsamples-=minimum;
		numsamples-=minimum;
		psamplesleft+=minimum;
		psamplesright+=minimum;
	}
}

// Function that describes value on client's displaying

//warning: All sampling rate dependant parameters assume 44.1Khz.
//The correction is done in the parameterTweak method.
bool mi::DescribeValue(char* txt,int const param, int const value) {
	// Oscillators waveform descriptions
	switch(param) {
	case 0: //fallthrough
	case 1:
			switch(value) {
				case 0:std::strcpy(txt,"Sine");return true;
				case 1:std::strcpy(txt,"Sawtooth");return true;
				case 2:std::strcpy(txt,"Square");return true;
				case 3:std::strcpy(txt,"Triangle");return true;
				case 4:std::strcpy(txt,"Sampled noise");return true;
				case 5:std::strcpy(txt,"Low noise");return true;
				case 6:std::strcpy(txt,"White noise");return true;
			}
			break;
	case 2:
			std::sprintf(txt,"%d notes",Vals[param]);
			return true;
	case 3: 
			std::sprintf(txt,"%.03f cts.",Vals[param]*0.390625f);
			return true;
	case 4:	
			std::strcpy(txt,(value==0)?"Off":"On");
			return true;
	case 5: //fallthrough
	case 6: //fallthrough
	case 8: //fallthrough
	case 9: //fallthrough
	case 10: //fallthrough
	case 12:
			std::sprintf(txt,"%.03f ms",Vals[param]*1000.0f/44100.0f);
			return true;
	case 7:
		if (value > 0) sprintf(txt, "%.02f dB", 20.0f * std::log10((float) value / 256.0f));
		else sprintf(txt, "-inf dB");
		return true;
	case 11:
		{
			int top = Vals[15]+(Vals[18]*value*0.00390625f);
			if(top > 240) top = 240;
			if(top < 0) top = 0;
			float result =GetAsFrequency(top);
			std::sprintf(txt,"%.0f Hz", result);
		}
			return true;
	case 13:
			std::sprintf(txt,"%.03f Hz",(value*0.000005f)*(44100.0f/64.0f)/(2.0f*math::pi_f));
			return true;
	case 14:
		{
			int top = Vals[15]+value;
			if(top > 240) top = Vals[15]-value;
			float result;
			if (Vals[17] < 6) {
					result = 264*pow(32.,top/240.) - 264*pow(32.,Vals[15]/240.);
			}
			else if(Vals[17] < 8){
					result = THREESEL((float)top,270.0f,400.0f,800.0f) - THREESEL((float)Vals[15],270.0f,400.0f,800.0f);
			}
			else if(Vals[17] < 10){
					result = THREESEL((float)top,270.0f,400.0f,650.0f) -THREESEL((float)Vals[15],270.0f,400.0f,650.0f);
			}
			else if (Vals[17] < 12) {
					result = 264*pow(32.,top/240.)*0.7f - 264*pow(32.,Vals[15]/240.)*0.7f;
			}
			else if (Vals[17] <14) {
					result = float(top/(1+Vals[16]/240.0)) -float(Vals[15]/(1+Vals[16]/240.0));
			}
			else if (Vals[17] <16) {
					result = float(top/(3.5-2*Vals[16]/240.0)) - float(Vals[15]/(3.5-2*Vals[16]/240.0));
			}
			else if(Vals[17] < 18){
					result = THREESEL((float)top,270.0f,400.0f,800.0f) - THREESEL((float)Vals[15],270.0f,400.0f,800.0f);
			}
			else {
					result = THREESEL((float)top,270.0f,400.0f,650.0f) - THREESEL((float)Vals[15],270.0f,400.0f,650.0f);
			}
			std::sprintf(txt,"%.0f Hz", result);
			return true;
		}
	case 15:
		{
		float result = GetAsFrequency(value);
			if (Vals[17] < 6) {
				std::sprintf(txt,"%.0f Hz",result);
			}
			else if(Vals[17] < 8){
				std::sprintf(txt,"%.0f Hz",result);
			}
			else if(Vals[17] < 10){
				std::sprintf(txt,"%.0f Hz",result);
			}
			else if (Vals[17] < 12) {
				std::sprintf(txt,"%.0f Hz x2",result);
			}
			else if (Vals[17] <14) {
				std::sprintf(txt,"%.0f Hz - %.0f Hz",result, 264*pow(32.,value/240.));
			}
			else if (Vals[17] <16) {
				std::sprintf(txt,"%.03f Hz - %.0f Hz",result, 264*pow(32.,value/240.));
			}
			else if(Vals[17] < 18){
				std::sprintf(txt,"%.0f Hz + %.0f Hz",result, THREESEL((float)value,2140.0f,800.0f,1150.0f));
			}
			else {
				std::sprintf(txt,"%.0f Hz + %.0f Hz",result, THREESEL((float)value,2140.0f,1700.0f,1080.0f));
			}
			}
			return true;
	case 16:
			if (Vals[17] < 2) {
				std::sprintf(txt,"%.03f Q",(float)sqrt(1.01+14*value/240.0));
			}
			else if (Vals[17] < 4) {
				std::sprintf(txt,"%.03f Q",(float)(1.0+value/12.0));
			}
			else if (Vals[17] < 6) {
				std::sprintf(txt,"%.03f Q",8.0f);
			}
			else if(Vals[17] < 8){
				std::sprintf(txt,"%.03f Q",2.0f+value/48.0f);
			}
			else if(Vals[17] < 10){
				std::sprintf(txt,"%.03f Q",2.0f+value/56.0f);
			}
			if (Vals[17] < 12) {
				std::sprintf(txt,"%.03f Q",(float)sqrt(1.01+14*value/240.0));
			}
			else if (Vals[17] < 14) {
				std::sprintf(txt,"%.03f Q",(float)(1.0+value/12.0));
			}
			else if (Vals[17] < 16) {
				std::sprintf(txt,"%.03f Q",8.0f);
			}
			else if(Vals[17] < 18){
				std::sprintf(txt,"%.03f Q",2.0f+value/48.0f);
			}
			else{
				std::sprintf(txt,"%.03f Q",2.0f+value/56.0f);
			}
			return true;
	case 17:
			switch(value) {
			case 0:std::strcpy(txt,"Lowpass A");return true;
			case 1:std::strcpy(txt,"Hipass A");return true;
			case 2:std::strcpy(txt,"Bandpass A");return true;
			case 3:std::strcpy(txt,"Bandreject A");return true;
			case 4:std::strcpy(txt,"ParaEQ1 A");return true;
			case 5:std::strcpy(txt,"InvParaEQ1 A");return true;
			case 6:std::strcpy(txt,"ParaEQ2 A");return true;
			case 7:std::strcpy(txt,"InvParaEQ2 A");return true;
			case 8:std::strcpy(txt,"ParaEQ3 A");return true;
			case 9:std::strcpy(txt,"InvParaEQ3 A");return true;
			case 10:std::strcpy(txt,"Lowpass B");return true;
			case 11:std::strcpy(txt,"Hipass B");return true;
			case 12:std::strcpy(txt,"Bandpass B");return true;
			case 13:std::strcpy(txt,"Bandreject B");return true;
			case 14:std::strcpy(txt,"ParaEQ1 B");return true;
			case 15:std::strcpy(txt,"InvParaEQ1 B");return true;
			case 16:std::strcpy(txt,"ParaEQ2 B");return true;
			case 17:std::strcpy(txt,"InvParaEQ2 B");return true;
			case 18:std::strcpy(txt,"ParaEQ3 B");return true;
			case 19:std::strcpy(txt,"InvParaEQ3 B");return true;
			}
			break;
	case 18:
		{
			int top = Vals[15]+value;
			if(top > 240) top = 240;
			if(top < 0) top = 0;
			float result =GetAsFrequency(top);

			std::sprintf(txt,"%.0f Hz", result);
			return true;
		}
	case 19:
			{
				if ( value == 0 ) std::strcpy(txt,"Osc1");
				else if ( value == 256 ) std::strcpy(txt,"Osc2");
				else std::sprintf(txt,"%.02f dB: %.02f dB",
					20.0f * std::log10((float)(256-value) *0.00390625f),
					20.0f * std::log10((float)value *0.00390625f));
			}
			return true;
	case 20:
			if (value > 0) sprintf(txt, "%.02f dB", 20.0f * std::log10((float) value *0.00390625f));
			else sprintf(txt, "-inf dB");
			return true;
	case 21:
			switch(value) {
			case 0:std::strcpy(txt,"Off");return true;
			case 1:std::strcpy(txt,"Minor1");return true;
			case 2:std::strcpy(txt,"Major1");return true;
			case 3:std::strcpy(txt,"Minor2");return true;
			case 4:std::strcpy(txt,"Major2");return true;
			case 5:std::strcpy(txt,"Riffer1");return true;
			case 6:std::strcpy(txt,"Riffer2");return true;
			case 7:std::strcpy(txt,"Riffer3");return true;
			case 8:std::strcpy(txt,"Minor Bounce");return true;
			case 9:std::strcpy(txt,"Major Bounce");return true;
			}
			break;
	case 22:
		std::sprintf(txt,"%d bpm",value);
		return true;
	case 24:
		//Correct the display for finetune
		std::sprintf(txt,"%d notes",value-1);
		return true;
	case 25:
		//Correct the display for finetune
		std::sprintf(txt,"%.03f cts.",(value-60)*0.390625f);
		return true;
	case 26:
			if (value == 0) {std::strcpy(txt, "Off"); return true;}
			break;
	case 27:
			if (value == 0) {std::strcpy(txt, "Off (Sample hold)"); return true;}
			if (value == 1) {std::strcpy(txt, "Cubic spline"); return true;}
			break;
	default:
			break;
	}
	return false;
}

 
//////////////////////////////////////////////////////////////////////
// The SeqTick function is where your notes and pattern command handlers
// should be processed.
// Is called by the host sequencer
	
void mi::SeqTick(int channel, int note, int ins, int cmd, int val) {
	track[channel].InitEffect(cmd,val);
	reinitChannel[channel]=false;

	// Global scope synth pattern commands
	switch(cmd)
	{
	case 6: // Change sustain
		globalpar.amp_env_sustain=val;
		if(track[channel].AmpEnvStage != 0) {
			track[channel].InitEnvelopes(false);
		}
	break;
	case 7: // Change envmod
		globalpar.vcf_envmod=val-128;
		if(track[channel].AmpEnvStage != 0) {
			track[channel].InitEnvelopes(false);
		}
	break;
	
	case 8: // Change cutoff
		globalpar.vcf_cutoff=val>>1;
		if(track[channel].AmpEnvStage != 0) {
			track[channel].InitEnvelopes(false);
		}
	break;
	
	case 9: // Change reso
		globalpar.vcf_resonance=val>>1;
		if(track[channel].AmpEnvStage != 0) {
			track[channel].InitEnvelopes(false);
		}
	break;
	case 0x0A: // Change volume
		globalpar.out_vol=val/2;
	break;
	case 0x0B: // Change attack
		{
			float multiplier = (float)currentSR/44100.0f;
			globalpar.amp_env_attack=float((val*970.2)+32)*multiplier;
			if(track[channel].AmpEnvStage != 0) {
				track[channel].InitEnvelopes(false);
			}
		}
	break;
	case 0x0F: // Change glide
		{
			globalpar.synthglide=256 - sqrtf(16000.f - val*16000.f/127.f);
		}

	}

	if(note<=NOTE_MAX)
		//Note zero is A_-1 (Which is note 9 in Psycle)
		track[channel].NoteOn(note-9);
	else if(note==NOTE_NOTEOFF)
		track[channel].NoteOff();
}

//New method to generate the wavetables:
//
//Generate a signal of aproximately 13.75Hz for the current sampling rate.
//This frequency is exactly A-1 using the standard tuning of A4=440
//Since it will not be an integer amount of samples, store the difference
//as a factor in wavetableCorrection, so that it can be applied when calculating
//the OSC speed.
void mi::InitWaveTableSR(bool delArray) {
	//Ensure the value is even, we need to divide it by two.
	const uint32_t amount = round<uint32_t>((float)currentSR / 13.75f) & 0xFFFFFFFE;
	const uint32_t half = amount >> 1;
	const uint32_t thirtytwo = amount*32/2048;

	const double sinFraction = 2.0*psycle::plugin_interface::pi/(double)amount;
	const float increase = 32768.0f/(float)amount;
	const float increase2 = 65536.0f/(float)amount;

	//Skipping noise wavetable. It is useless.
	for (uint32_t i=0;i < WAVETABLES; i++) {
		if (delArray) {
			delete[] WaveTable[i];
		}
		//Two more shorts allocated for the interpolation routine.
		WaveTable[i]=new float[amount+2];
	}
	for(uint32_t c=0;c<half;c++) {
		double sval=(double)c*sinFraction;
		WaveTable[0][c]=math::rint<short int,double>(sin(sval)*16384.0);
		WaveTable[1][c]=(c*increase)-16384;
		WaveTable[2][c]=-16384;
		WaveTable[3][c]=(c*increase2)-16384;
		WaveTable[4][c]=rand()-16384;
	}
	for(uint32_t c=half;c<amount;c++) {
		double sval=(double)c*sinFraction;
		WaveTable[0][c]=math::rint<short int,double>(sin(sval)*16384.0);
		WaveTable[1][c]=(c*increase)-16384;
		WaveTable[2][c]=16384;
		WaveTable[3][c]=16384-((c-half)*increase2);
		WaveTable[4][c]=rand()-16384;
	}
	for(uint32_t c=0;c<thirtytwo;c++) {
		WaveTable[5][c]=16384;
	}
	for(uint32_t c=thirtytwo;c<amount;c++) {
		WaveTable[5][c]=0;
	}
	//Two more shorts allocated for the interpolation routine.
	for (uint32_t i=0;i < WAVETABLES; i++) {
		WaveTable[i][amount]=WaveTable[i][0];
		WaveTable[i][amount+1]=WaveTable[i][1];
	}

	waveTableSize = amount;
	wavetableCorrection = (float)amount*13.75f / (float)currentSR;
}

float mi::GetAsFrequency(int top) {
	float result;
	if (Vals[17] < 6) {
		result = 264*pow(32.,top/240.);
	}
	else if(Vals[17] < 8){
		result = THREESEL((float)top,270.0f,400.0f,800.0f);
	}
	else if(Vals[17] < 10){
		result = THREESEL((float)top,270.0f,400.0f,650.0f);
	}
	else if (Vals[17] < 12) {
		result = 264*pow(32.,top/240.)*0.7f;
	}
	else if (Vals[17] <14) {
		result = float(top/(1+Vals[16]/240.0));
	}
	else if (Vals[17] <16) {
		result = float(top/(3.5-2*Vals[16]/240.0));
	}
	else if(Vals[17] < 18){
		result = THREESEL((float)top,270.0f,400.0f,800.0f);
	}
	else {
		result = THREESEL((float)top,270.0f,400.0f,650.0f);
	}
	return result;
}
}