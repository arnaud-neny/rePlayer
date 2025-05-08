/////////////////////////////////////////////////////////////////////
// Dmitry "Sartorius" Kulikov FluidSynth plugin for PSYCLE
//

#include <psycle/plugin_interface.hpp>

#include <diversalis.hpp>
#include <fluidsynth.h>

#include <string>
#include <cstring>
#include <cstdio>

namespace psycle::plugins::dfs_sf2_player {
using namespace psycle::plugin_interface;

#define MAX_PATH_A_LA_CON (1 << 12) // TODO total shit.. paths are loaded/saved with this length!
#define FILEVERSION 3 // TODO fix endianess and max path and upgrade the version
#define MAXINSTR 64

struct INSTR {
	int bank;
	int prog;
	int pitch;
	int wheel;
};

struct SYNPAR {
	int version;
	char SFPathName[MAX_PATH_A_LA_CON];

	INSTR instr[MAXINSTR];
	int curChannel;

	int reverb_on;
	int roomsize;
	int damping;
	int width;
	int r_level;

	int chorus_on;
	int nr;
	int c_level;
	int speed;
	int depth_ms;
	int type;

	int polyphony;
	int interpolation;
	int gain;
};

CMachineParameter const paraInstr = {
	"Channel / Instrument", "Channel", 0, 0, MPF_LABEL, 0
};

CMachineParameter const paraChannel = {
	"Midi Channel (aux col)", "Midi channel. Select with aux column", 0, MAXINSTR-1, MPF_STATE, 0
};

CMachineParameter const paraBank = {
	"Bank", "Bank", 0, 255, MPF_STATE, 0
};

CMachineParameter const paraProgram = {
	"Program", "Program", 0, 255, MPF_STATE, 0
};

CMachineParameter const paraPitchBend = {
	"Pitch bend amount", "Max Pitch bend", 0, 16384, MPF_STATE, 8192
};

CMachineParameter const paraWheelSens = {
	"Pitch bend Range", "Pitch Wheel sensitivity range", 0, 72, MPF_STATE, 2
};

CMachineParameter const paraReverb = {
	"Reverb", "Reverb", 0, 0, MPF_LABEL, 0
};

CMachineParameter const paraReverbOn = {
	"On/Off", "On/Off", 0, 1, MPF_STATE, 0
};

CMachineParameter const paraReverbRoom = {
	"Room", "Room", 0, 100, MPF_STATE, 30
};

CMachineParameter const paraReverbDamp = {
	"Damp", "Damp", 0, 100, MPF_STATE, 35
};

CMachineParameter const paraReverbWidth = {
	"Width", "Width", 0, 100, MPF_STATE, 70
};

CMachineParameter const paraReverbLevel = {
	"Level", "Level", 0, 1000, MPF_STATE, 350
};

CMachineParameter const paraChorus = {
	"Chorus", "Chorus", 0, 0, MPF_LABEL, 0
};

CMachineParameter const paraChorusOn = {
	"On/Off", "On/Off", 0, 1, MPF_STATE, 0
};

CMachineParameter const paraChorusNr = {
	"No", "No", 1, 99, MPF_STATE, FLUID_CHORUS_DEFAULT_N
};

CMachineParameter const paraChorusLevel = {
	"Level", "Level", 1, 10000, MPF_STATE, 512
};

CMachineParameter const paraChorusSpeed = {
	"Speed", "Speed", 3, 50, MPF_STATE, 3
};

CMachineParameter const paraChorusDepth = {
	"Depth", "Depth", 0, 1000, MPF_STATE, 8
};

CMachineParameter const paraChorusType = {
	"Mode", "Mode", FLUID_CHORUS_MOD_SINE, FLUID_CHORUS_MOD_TRIANGLE, MPF_STATE, FLUID_CHORUS_MOD_SINE
};

CMachineParameter const paraPolyphony = {
	"Polyphony", "Polyphony", 1, 256, MPF_STATE, 128
};

CMachineParameter const paraInter = {
	"Interpolation", "Interpolation", FLUID_INTERP_NONE, FLUID_INTERP_HIGHEST+3, MPF_STATE, FLUID_INTERP_DEFAULT
};

CMachineParameter const paraGain = {
	"Global Gain", "Global Gain", 0, 256, MPF_STATE, 48
};

CMachineParameter const paraNull = {
	"", "", 0, 0, MPF_NULL, 0
};

CMachineParameter const *pParameters[] = { 
	&paraInstr,
	&paraChannel,
	&paraBank,
	&paraProgram,
	&paraPitchBend,
	&paraWheelSens,
	&paraNull,
	&paraPolyphony,

	&paraReverb,
	&paraReverbOn,
	&paraReverbRoom,
	&paraReverbDamp,
	&paraReverbWidth,
	&paraReverbLevel,

	&paraNull,
	&paraInter,

	&paraChorus,
	&paraChorusOn,
	&paraChorusNr,
	&paraChorusLevel,
	&paraChorusSpeed,
	&paraChorusDepth,
	&paraChorusType,

	&paraGain
};

enum {
	e_paraInstr,
	e_paraChannel,
	e_paraBank,
	e_paraProgram,
	e_paraPitchBend,
	e_paraWheelSens,
	e_paraNull1,

	e_paraPolyphony,

	e_paraReverb,
	e_paraReverbOn,
	e_paraReverbRoom,
	e_paraReverbDamp,
	e_paraReverbWidth,
	e_paraReverbLevel,
	e_paraNull2,

	e_paraInter,

	e_paraChorus,
	e_paraChorusOn,
	e_paraChorusNr,
	e_paraChorusLevel,
	e_paraChorusSpeed,
	e_paraChorusDepth,
	e_paraChorusType,
	e_paraGain,

	// NUMPARAMETERS
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0101,
	GENERATOR,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"FluidSynth SF2 player"
		#ifndef NDEBUG
			" (Debug build)"
		#endif
		,
	"FluidSynth",
	"Peter Hanappe and others http://fluidsynth.org Ver." FLUIDSYNTH_VERSION "\n"
		"\n"
		"SoundFont(R) is a registered trademark of E-mu Systems, Inc.\n"
		"\n"
		"Usage:\n"
		"\n"
		"C-4 02 03 0C20\tC4 - note\n"
		"\t\t02 - instrument preset (required)\n"
		"\t\t03 - machine number\n"
		"\t\t0Cxx - (optional) volume at note on (00-7F)\n"
		"\n"
		"off 02 03\t\tnote-off (the instrument preset is required!)\n"
		"mcm command is supported\n"
		"\n"
		"ported by Sartorius",
	"Load SoundFont",
	3
);

class mi : public CMachineInterface {
	public:
		mi();
		virtual ~mi();

		virtual void Init();
		virtual void SequencerTick();
		virtual void ParameterTweak(int par, int val);
		virtual void Work(float *psamplesleft, float* psamplesright, int numsamples,int tracks);
		virtual void Stop();
		virtual void PutData(void * pData);
		virtual void GetData(void * pData);
		virtual int GetDataSize() { return sizeof SYNPAR; }
		virtual void Command();
		virtual void MidiEvent(int channel, int midievent, int value);
		virtual bool DescribeValue(char* txt,int const param, int const value);
		virtual bool HostEvent(const int eventNr, int const val1, float const val2);
		virtual void SeqTick(int channel, int note, int ins, int cmd, int val);
		
		SYNPAR globalpar;

	private:
		void SetParams();
		bool LoadSF(const char * sffile);

		fluid_settings_t* settings;
		fluid_synth_t* synth;
		fluid_preset_t* preset;

		int sf_id;
		int banks[129],progs[129];
		
		int max_bank_index;

		int lastnote[MAXINSTR][MAX_TRACKS];

		//reverb
		double roomsize, damping, width, r_level;
		//chorus
		double nr, c_level, speed, depth_ms, type;

		bool new_sf;
		int samplerate;
};

PSYCLE__PLUGIN__INSTANTIATOR("fs-sf2-player", mi, MacInfo)

mi::mi() {
	Vals = new int[MacInfo.numParameters];
	new_sf = true;
}


mi::~mi() {
	delete[] Vals;
	if(synth) delete_fluid_synth(synth);
	if(settings) delete_fluid_settings(settings);
}

void mi::Init() {
	max_bank_index = -1;
	sf_id = 0;

	globalpar.version = FILEVERSION;
	std::memset(&globalpar.SFPathName, 0, MAX_PATH_A_LA_CON);

	for(int i = 0; i < MAXINSTR; ++i) {
		globalpar.instr[i].bank  = paraBank.DefValue;
		globalpar.instr[i].prog  = paraProgram.DefValue;
		globalpar.instr[i].pitch = paraPitchBend.DefValue;
		globalpar.instr[i].wheel = paraWheelSens.DefValue;
	}
	globalpar.curChannel = paraChannel.DefValue;

	globalpar.reverb_on = paraReverbOn.DefValue;
	globalpar.roomsize  = paraReverbRoom.DefValue;      
	globalpar.damping   = paraReverb.DefValue;          
	globalpar.width     = paraReverbWidth.DefValue;             
	globalpar.r_level   = paraReverbLevel.DefValue;     

	roomsize = (double) globalpar.roomsize * .01;
	damping  = (double) globalpar.damping * .01;
	width    = (double) globalpar.width * .01;
	r_level  = (double) globalpar.r_level * .001;


	globalpar.chorus_on = paraChorusOn.DefValue;
	globalpar.nr        = nr = FLUID_CHORUS_DEFAULT_N;
	globalpar.c_level   = paraChorusLevel.DefValue;     
	globalpar.speed     = paraChorusSpeed.DefValue;             
	globalpar.depth_ms  = paraChorusDepth.DefValue;     
	globalpar.type      = type = FLUID_CHORUS_DEFAULT_TYPE;

	c_level  = (double) globalpar.c_level * .001;
	speed    = (double) globalpar.speed * .1;
	depth_ms = (double) globalpar.depth_ms;

	
	globalpar.polyphony = 128;
	globalpar.interpolation = FLUID_INTERP_DEFAULT;
	globalpar.gain = 64;

	settings = new_fluid_settings();
	
	#ifndef NDEBUG
		fluid_settings_setstr(settings, "synth.verbose", "yes");
	#endif
	samplerate=pCB->GetSamplingRate();
	fluid_settings_setnum(settings, "synth.sample-rate", samplerate);
	fluid_settings_setint(settings, "synth.polyphony", globalpar.polyphony);
	fluid_settings_setint(settings, "synth.interpolation", globalpar.interpolation);
	fluid_settings_setint(settings, "synth.midi-channels", MAXINSTR);
	fluid_settings_setint(settings, "synth.threadsafe-api", 0);
	fluid_settings_setint(settings, "synth.parallel-render", 0);
	synth = new_fluid_synth(settings);
	SetParams();
}

void mi::SetParams() {
	fluid_synth_set_interp_method(synth, -1, globalpar.interpolation);
	fluid_synth_set_polyphony(synth, globalpar.polyphony);
	fluid_synth_set_reverb_on(synth, globalpar.reverb_on);
	if(globalpar.reverb_on) {
		fluid_synth_set_reverb(synth, roomsize, damping, width, r_level);
	}
	fluid_synth_set_chorus_on(synth, globalpar.chorus_on);
	if(globalpar.chorus_on) {
		fluid_synth_set_chorus(synth, nr, c_level, speed, depth_ms, type);
	}
	fluid_synth_set_gain(synth,globalpar.gain*(1.0/128.0));

	//fluid_synth_system_reset(synth);
	
	for(int i = 0; i < MAXINSTR; ++i)
		for(int y = 0; y < MAX_TRACKS; ++y)
			lastnote[i][y]  =255;
}

void mi::SequencerTick() {
	if(samplerate!=pCB->GetSamplingRate()) {
		fluid_settings_setnum(settings, "synth.sample-rate", samplerate);
		fluid_synth_set_sample_rate(synth, samplerate);
	}
}
void mi::PutData(void* pData) {
	sf_id = 0;
	int loadingVersion = ((SYNPAR*)pData)->version;
	if (pData == NULL || loadingVersion > FILEVERSION) {
		pCB->MessBox("WARNING!\nThis fileversion does not match current plugin's fileversion.\nYour settings are probably fucked.","FluidSynth",0);
		return;
	}

	DIVERSALIS__WARNING("Reading/writting structured data as a raw bunch of bytes leads to endianess issues!");

	size_t readsize = GetDataSize();
	if (loadingVersion == 2) {
		readsize -= sizeof(globalpar.gain);
		globalpar.gain = 25;
	}
	std::memcpy(&globalpar, pData, readsize);
	globalpar.version = FILEVERSION;

	new_sf = false;

	if(LoadSF(globalpar.SFPathName)) {
		Vals[e_paraChannel] = globalpar.curChannel;
		for(int i = 0; i < MAXINSTR; ++i) {
			fluid_synth_bank_select(synth, i, globalpar.instr[i].bank);
			fluid_synth_program_change(synth, i, globalpar.instr[i].prog);
			fluid_synth_pitch_bend(synth, i, globalpar.instr[i].pitch);
			fluid_synth_pitch_wheel_sens(synth, i, globalpar.instr[i].wheel);
		}
		Vals[e_paraBank]      = globalpar.instr[globalpar.curChannel].bank;
		Vals[e_paraProgram]   = globalpar.instr[globalpar.curChannel].prog;
		Vals[e_paraPitchBend] = globalpar.instr[globalpar.curChannel].pitch;
		Vals[e_paraWheelSens] = globalpar.instr[globalpar.curChannel].wheel;

		Vals[e_paraReverbOn]    = globalpar.reverb_on;
		Vals[e_paraReverbRoom]  = globalpar.roomsize;
		Vals[e_paraReverbDamp]  = globalpar.damping;
		Vals[e_paraReverbWidth] = globalpar.width;
		Vals[e_paraReverbLevel] = globalpar.r_level;
		
		roomsize = (double) globalpar.roomsize * .01;
		damping  = (double) globalpar.damping * .01;
		width    = (double) globalpar.width * .01;
		r_level  = (double) globalpar.r_level * .001;

		Vals[e_paraChorusOn]    = globalpar.chorus_on;
		Vals[e_paraChorusNr]    = nr = globalpar.nr;
		Vals[e_paraChorusLevel] = globalpar.c_level;
		Vals[e_paraChorusSpeed] = globalpar.speed;
		Vals[e_paraChorusDepth] = globalpar.depth_ms;
		Vals[e_paraChorusType]  = type = globalpar.type;

		c_level  = (double) globalpar.c_level * .001;
		speed    = (double) globalpar.speed * .1;
		depth_ms = (double) globalpar.depth_ms;

		Vals[e_paraPolyphony] = globalpar.polyphony;
		Vals[e_paraInter]     = globalpar.interpolation;
		Vals[e_paraGain]	  = globalpar.gain;

		SetParams();
	} else {
		pCB->MessBox("WARNING!\nSomething wrong...","FluidSynth",0);
		return;
	}
	new_sf = true;
}

void mi::GetData(void* pData) {
	if(pData) std::memcpy(pData, &globalpar, GetDataSize());
}

void mi::Stop() {
	for(int chan = 0; chan < MAX_TRACKS; ++chan) {
		fluid_synth_all_notes_off(synth,chan);
		fluid_synth_all_sounds_off(synth,chan);
		for(int i = 0; i < MAXINSTR; ++i)
			lastnote[i][chan] = 255;
	}
}

void mi::ParameterTweak(int par, int val) {
	Vals[par]=val;
				
	switch(par) {
		case e_paraChannel:
			globalpar.curChannel = val;
			preset = fluid_synth_get_channel_preset(synth, globalpar.curChannel);
			if(preset != NULL && max_bank_index != -1) {
				for(int b = 0; b <= max_bank_index; ++b)
					if(banks[b] == (*(preset)->get_banknum)(preset)) {
						Vals[e_paraBank] = b;
						break;
					}
				//banks[Vals[e_paraBank]] = (*(preset)->get_banknum)(preset);
				globalpar.instr[globalpar.curChannel].bank = (*(preset)->get_banknum)(preset);
				Vals[e_paraProgram] = globalpar.instr[globalpar.curChannel].prog = (*(preset)->get_num)(preset);
			} else {
				Vals[e_paraProgram] = paraProgram.MaxValue;
			}
			Vals[e_paraPitchBend] = globalpar.instr[globalpar.curChannel].pitch;
			fluid_synth_pitch_bend(synth, globalpar.curChannel, globalpar.instr[globalpar.curChannel].pitch);
			Vals[e_paraWheelSens] = globalpar.instr[globalpar.curChannel].wheel;
			fluid_synth_pitch_wheel_sens(synth, globalpar.curChannel, globalpar.instr[globalpar.curChannel].wheel);
			break;
		case e_paraBank:
			if(max_bank_index != 1) {
				if (Vals[e_paraBank]>max_bank_index) {
					Vals[e_paraBank] = max_bank_index;
				}
				globalpar.instr[globalpar.curChannel].bank = banks[Vals[e_paraBank]];
				globalpar.instr[globalpar.curChannel].prog = Vals[e_paraProgram] = progs[globalpar.instr[globalpar.curChannel].bank];

				fluid_synth_bank_select(synth, globalpar.curChannel, globalpar.instr[globalpar.curChannel].bank);
				fluid_synth_program_change(synth, globalpar.curChannel, globalpar.instr[globalpar.curChannel].prog);
			}
			break;
		case e_paraProgram:
			globalpar.instr[globalpar.curChannel].prog = val;
			fluid_synth_program_change(synth, globalpar.curChannel, globalpar.instr[globalpar.curChannel].prog);
			break;
		case e_paraPitchBend:
			globalpar.instr[globalpar.curChannel].pitch = val;
			fluid_synth_pitch_bend(synth, globalpar.curChannel, globalpar.instr[globalpar.curChannel].pitch);
			break;
		case e_paraWheelSens:
			globalpar.instr[globalpar.curChannel].wheel = val;
			fluid_synth_pitch_wheel_sens(synth, globalpar.curChannel, globalpar.instr[globalpar.curChannel].wheel);
			break;
		case e_paraReverbOn:
			globalpar.reverb_on = val;
			fluid_synth_set_reverb_on(synth, globalpar.reverb_on);
			if(globalpar.reverb_on) {
				fluid_synth_set_reverb(synth, roomsize, damping, width, r_level);
			}
			break;
		case e_paraReverbRoom:
			globalpar.roomsize = val;
			roomsize = globalpar.roomsize * .01;
			if(globalpar.reverb_on) {
				fluid_synth_set_reverb(synth, roomsize, damping, width, r_level);
			}
			break;
		case e_paraReverbDamp:
			globalpar.damping = val;
			damping = globalpar.damping * .01;
			if(globalpar.reverb_on) {
				fluid_synth_set_reverb(synth, roomsize, damping, width, r_level);
			}
			break;
		case e_paraReverbWidth:
			globalpar.width = val;
			width = globalpar.width * .01;
			if(globalpar.reverb_on) {
				fluid_synth_set_reverb(synth, roomsize, damping, width, r_level);
			}
			break;
		case e_paraReverbLevel:
			globalpar.r_level = val;
			r_level = globalpar.r_level * .001;
			if(globalpar.reverb_on) {
				fluid_synth_set_reverb(synth, roomsize, damping, width, r_level);
			}
			break;
		case e_paraChorusOn:
			globalpar.chorus_on = val;
			fluid_synth_set_chorus_on(synth, globalpar.chorus_on);
			if(globalpar.chorus_on) {
				fluid_synth_set_chorus(synth, nr, c_level, speed, depth_ms, type);
			}
			break;
		case e_paraChorusNr:
			globalpar.nr = nr = val;
			if(globalpar.chorus_on) {
				fluid_synth_set_chorus(synth, nr, c_level, speed, depth_ms, type);
			}
			break;
		case e_paraChorusLevel:
			globalpar.c_level = val; 
			c_level = (double)globalpar.c_level*.001;
			if(globalpar.chorus_on) {
				fluid_synth_set_chorus(synth, nr, c_level, speed, depth_ms, type);
			}
			break;
		case e_paraChorusSpeed:
			globalpar.speed = val; 
			speed = (double)globalpar.speed *.1;
			if(globalpar.chorus_on) {
				fluid_synth_set_chorus(synth, nr, c_level, speed, depth_ms, type);
			}
			break;
		case e_paraChorusDepth:
			globalpar.depth_ms = val; 
			depth_ms = (double)globalpar.depth_ms;
			if(globalpar.chorus_on) {
				fluid_synth_set_chorus(synth, nr, c_level, speed, depth_ms, type);
			}
			break;
		case e_paraChorusType:
			globalpar.type = type = val;
			if(globalpar.chorus_on) {
				fluid_synth_set_chorus(synth, nr, c_level, speed, depth_ms, type);
			}
			break;
		case e_paraPolyphony:
			globalpar.polyphony = val;
			fluid_synth_set_polyphony(synth, globalpar.polyphony);
			break;
		case e_paraInter:
			if(val < FLUID_INTERP_LINEAR) {
				globalpar.interpolation = FLUID_INTERP_NONE;
			} else if(val < FLUID_INTERP_4THORDER){
				globalpar.interpolation = FLUID_INTERP_LINEAR;
			} else if(val < FLUID_INTERP_7THORDER){
				globalpar.interpolation = FLUID_INTERP_4THORDER;
			} else {
				globalpar.interpolation = FLUID_INTERP_7THORDER;
			}
			fluid_synth_set_interp_method(synth, -1, globalpar.interpolation);
			break;
		case e_paraGain:
			globalpar.gain = val;
			fluid_synth_set_gain(synth,val*(1.0/128.0));
			break;
		}
}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples,int tracks) {
	if(sf_id > 0) {
		float *xpl, *xpr;
		xpl = psamplesleft;
		xpr = psamplesright;
		--xpl;
		--xpr;
		
		fluid_synth_write_float(synth, numsamples, psamplesleft, 0, 1, psamplesright, 0, 1);
		
		for(int i = 0; i < numsamples; ++i) {
			*++xpl *= 32767.f;
			*++xpr *= 32767.f;
		}
	}
}

bool mi::DescribeValue(char* txt,int const param, int const value) {
	switch(param) {
		case e_paraChannel:
			if (value==9) std::sprintf(txt,"%02X (Drums)",value+1);
			else std::sprintf(txt,"%02X (Melody)",value+1);
			return true;
		case e_paraBank:
			std::sprintf(txt,"%i",banks[value]);
			return true;
		case e_paraProgram:
			preset = fluid_synth_get_channel_preset(synth, globalpar.curChannel);
			if(preset != NULL) {
				std::sprintf(txt,"%d: %s",preset->get_num(preset)+1,preset->get_name(preset));
			} else {
				std::sprintf(txt,"(none)");
			}
			return true;
		case e_paraPitchBend:
			std::sprintf(txt,"%.2f semis", Vals[e_paraWheelSens]*(value-8192)/8192.f);
			return true;
		case e_paraWheelSens:
			std::sprintf(txt,"%.d semis", value);
			return true;
		case e_paraReverbOn:
		case e_paraChorusOn:
			std::sprintf(txt,value?"On":"Off");
			return true;
		case e_paraReverbRoom:
			std::sprintf(txt,"%.2f",(float)roomsize);
			return true;
		case e_paraReverbDamp:
			std::sprintf(txt,"%.2f",(float)damping);
			return true;
		case e_paraReverbWidth:
			std::sprintf(txt,"%.2f",(float)width);
			return true;
		case e_paraReverbLevel:
			std::sprintf(txt,"%.3f",(float)r_level);
			return true;
		case e_paraChorusNr:
			std::sprintf(txt,"%i",value);
			return true;
		case e_paraChorusLevel:
			std::sprintf(txt,"%.2f",(float)c_level);
			return true;
		case e_paraChorusSpeed:
			std::sprintf(txt,"%.1f",(float)speed);
			return true;
		case e_paraChorusDepth:
			std::sprintf(txt,"%i ms",value);
			return true;
		case e_paraChorusType:
			std::sprintf(txt,value?"Triangle":"Sine");
			return true;
		case e_paraPolyphony:
			std::sprintf(txt,"%i",value);
			return true;
		case e_paraInter:
			if(value < FLUID_INTERP_LINEAR) {
				std::sprintf(txt,"None");
			} else if(value < FLUID_INTERP_4THORDER){
				std::sprintf(txt,"Linear");
			} else if(value < FLUID_INTERP_7THORDER){
				std::sprintf(txt,"4th order");
			} else {
				std::sprintf(txt,"7th order");
			}
			return true;
		case e_paraGain:
			std::sprintf(txt,"%.2f%%",(float)value*(1.0/1.28));
			return true;
		default:
			return false;
	}
}

void mi::SeqTick(int channel, int note, int ins, int cmd, int val) {
	if(ins >= MAXINSTR || channel >= MAX_TRACKS)
	{
		pCB->MessBox("Outside range","outside range",1);
		return;
	}
	if(note<=NOTE_MAX) {
		if(lastnote[ins][channel] != 255) fluid_synth_noteoff(synth, ins, lastnote[ins][channel]);
		lastnote[ins][channel] = note;
		if(cmd == 0xC) {
			val >>=1;
			fluid_synth_cc(synth, ins, 0x07, 127);
			fluid_synth_noteon(synth, ins, note, val);
		} else {
			fluid_synth_cc(synth, ins, 0x07, 127);
			fluid_synth_noteon(synth, ins, note, 127);
		}
	}
	else if(note==NOTE_NOTEOFF) {
		fluid_synth_noteoff(synth, ins, lastnote[ins][channel]);
		lastnote[ins][channel] = 255;
	}
	else if(note==NOTE_NONE && lastnote[ins][channel] != 255) 
	{
		if(cmd == 0x0C) {
			fluid_synth_cc(synth, ins, 0x07, val>>1);
		}
	}
}

void mi::Command() {
	char name[1024];
	name[0]='\0';
	bool open=pCB->FileBox(true,"SF2 (*.sf2)|*.sf2|All Files (*.*)|*.*||",name);
	if (open) {
		LoadSF(name);
	}
}

bool mi::LoadSF(const char * sf_file) {
	std::string loadfile = sf_file;
	if(!fluid_is_soundfont(sf_file)) {
		char buffer[1024];
		std::sprintf(buffer, "It's not a SoundFont file or file %s not found! Please, locate it.", sf_file);
		pCB->MessBox(buffer, "SF2 Loader",0);
		buffer[0]='\0';
		bool open=pCB->FileBox(true,"SF2 (*.sf2)|*.sf2|All Files (*.*)|*.*||",buffer);
		if (!open || !fluid_is_soundfont(buffer)) {
			return false;
		}
		loadfile = buffer;
	}
	if(sf_id) {
		Stop();
		fluid_synth_sfunload(synth,sf_id, 1);
	}
	sf_id = fluid_synth_sfload(synth, loadfile.c_str(), 1);
	if(sf_id == -1) {
		pCB->MessBox("Error loading!","SF2 Loader",0);
		sf_id = 0;
		return false;
	}

	fluid_synth_system_reset(synth);

	int cur_bank(-1);

	max_bank_index = -1;

	for(int i = 0; i < 129; ++i) {
		banks[i] = 0;
		progs[i] = 0;
	}
	
	const int midi_chan = fluid_synth_count_midi_channels(synth);
	for(int i = 0; i < midi_chan; ++i) {
		preset = fluid_synth_get_channel_preset(synth, i);
		if(preset != NULL) {
			if(cur_bank != (*(preset)->get_banknum)(preset)) {
				cur_bank = (*(preset)->get_banknum)(preset);
				++max_bank_index;
				banks[max_bank_index] = cur_bank;
				progs[cur_bank] = (*(preset)->get_num)(preset);
				if(new_sf) {
					globalpar.instr[i].bank = banks[max_bank_index];
					globalpar.instr[i].prog = progs[banks[max_bank_index]];
				}
			}
		}
	}
	// set channel to the first available instrument
	if(new_sf)
		for(int i = 0; i < midi_chan; ++i) {
			preset = fluid_synth_get_channel_preset(synth, i);
			if(preset != NULL) {
				Vals[e_paraChannel] = globalpar.curChannel = i;
				Vals[e_paraBank] = globalpar.instr[globalpar.curChannel].bank;
				Vals[e_paraProgram] = globalpar.instr[globalpar.curChannel].prog;
				break;
			}
		}

	std::sprintf(globalpar.SFPathName, loadfile.c_str());

	return true;
}
void mi::MidiEvent(int channel, int midievent, int value) {
	switch(midievent) {
		case 0x80: fluid_synth_noteoff(synth, channel, value>>8);break;
		case 0x90: fluid_synth_noteon(synth, channel, value>>8, value&0xFF);break;
		case 0xB0: fluid_synth_cc(synth,channel, value>>8,value&0xFF);break;
		case 0xC0: {
			int val = value>>8;
			if(channel == Vals[globalpar.curChannel]) {
				Vals[e_paraProgram] = val;
			}
			globalpar.instr[globalpar.curChannel].prog = val;
			fluid_synth_program_change(synth, channel, val);
			break;
			}
		case 0xD0: fluid_synth_channel_pressure(synth, channel, value>>8);break;
		case 0xE0: {
			int val = ((value&0x7F)<<7) + ((value&0x7F00)>>8);
			if(channel == Vals[globalpar.curChannel]) {
				Vals[e_paraPitchBend] = val;
			}
			globalpar.instr[globalpar.curChannel].pitch = val;
			fluid_synth_pitch_bend(synth, channel, val);
			break;
			}
		default:break;
	}
}
bool mi::HostEvent(int const eventNr, int const val1, float const val2) {
	if (eventNr == HE_NEEDS_AUX_COLUMN) {
		return true;
	}
	else {
		return false;
	}
}
}