///\file
///\brief Alk muter plugin for PSYCLE
#include <psycle/plugin_interface.hpp>
#include <cstdio> // for std::sprintf

namespace psycle { namespace plugins { namespace alk_muter {

using namespace plugin_interface;

CMachineParameter const static mute_parameter = {"Mute","Mute off/on",0,1,MPF_STATE,0};

CMachineParameter const static * const parameters[] = { 
	&mute_parameter
};

CMachineInfo const static machine_info (
	MI_VERSION,
	0x0120,
	EFFECT,
	sizeof parameters / sizeof *parameters,
	parameters,

	"Alk Muter"
	#ifndef NDEBUG
		" (debug build)"
	#endif
	,
	
	"Muter",
	"Alk",
	"About",
	1
);

class machine : public CMachineInterface {
	public:
		machine();
		~machine();
		void Init();
		void SequencerTick();
		void Work(float * left_samples, float * right_samples, int sample_count, int tracks);
		bool DescribeValue(char* text, int const parameter, int const value);
		void Command();
		void ParameterTweak(int parameter, int value);
	private:
		float volume;
		bool change;
		int currentSR;
		float slope;
};

PSYCLE__PLUGIN__INSTANTIATOR("alk-muter", machine, machine_info)

machine::machine():volume(1.0f), change(false) {
	Vals = new int[machine_info.numParameters];
}

machine::~machine() {
	delete[] Vals;
}

void machine::Init() {
	currentSR = pCB->GetSamplingRate();
	slope = currentSR/(44.1f*44100.0f);
}

void machine::SequencerTick() {
	if (currentSR != pCB->GetSamplingRate()) {
		Init();
	}

}

void machine::Command() {
	char text[] = "Made by alk\nThis machine mutes the audio of a path avoiding clicks.\0";
	char caption[] = "Alk's Muter\0";
	pCB->MessBox(text, caption, 0);
}

void machine::ParameterTweak(int parameter, int value) {
	switch(parameter) {
		case 0:
			if (value != Vals[0]) {
				change=true;
			}
			Vals[0] = value;
			break;
		default:
			break;
	}
}

void machine::Work(float * left_samples, float * right_samples, int sample_count, int /*tracks*/) {
	if (!change) {
		if(!Vals[0]) return;
		while(sample_count--) *left_samples++ = *right_samples++ = 0;
	} else if (Vals[0]){
		// mute enabled
		while(volume>0.01f && sample_count--) {
			(*left_samples)*=volume; (*right_samples)*=volume;
			left_samples++; right_samples++;
			volume-=slope;
		}
		sample_count++;
		while(sample_count--) *left_samples++ = *right_samples++ = 0;
		if (volume<=0.01f) {
			change=false;
		}
	} else {
		// mute disabled
		while(volume<0.99f && sample_count--) {
			(*left_samples)*=volume; (*right_samples)*=volume;
			left_samples++; right_samples++;
			volume+=slope;
		}
		if (volume>=0.99f) {
			change=false;
		}
	}
}

bool machine::DescribeValue(char * text, int const parameter, int const value) {
	switch(parameter) {
		case 0:
			switch(value) {
				case 0: std::sprintf(text, "off"); return true;
				case 1: std::sprintf(text, "on"); return true;
				default: return false;
			}
		default: return false;
	}
}
}}}

