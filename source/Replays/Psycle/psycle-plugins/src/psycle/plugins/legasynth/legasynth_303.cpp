/////////////////////////////////////////////////////////////////////
// Dmitry "Sartorius" Kulikov LegaSynth TB303 plugin for PSYCLE
// v0.2 beta
//

#include "./303/303_voice.h"
#include "./lib/chorus.h"
#include <psycle/plugin_interface.hpp>
#include <psycle/helpers/math/clip.hpp>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>
namespace psycle::plugins::legasynth {
using namespace psycle::plugin_interface;
using namespace universalis::stdlib;

CMachineParameter const paraCoarse = {"Coarse", "Coarse", -24, 24, MPF_STATE, 0};
CMachineParameter const paraFine = {"Fine", "Fine", -100, 100, MPF_STATE, 0};
CMachineParameter const paraPB = {"Pitch bend", "Pitch bend", 0, 0x4000, MPF_STATE, 0x2000};
CMachineParameter const paraPD = {"Pitch bend depth", "Pitch bend depth", 0, 12, MPF_STATE, 12};
CMachineParameter const paraExpression = {"Main Volume", "Main Volume", 0, 127, MPF_STATE, 127};
CMachineParameter const paraDist = {"Distortion", "Distortion", 0, 100, MPF_STATE, 0};
CMachineParameter const paraFilter = {"Envelope&Filter", "Envelope&Filter", 0, 0, MPF_LABEL, 0};
CMachineParameter const paraCutOff = {"Cutoff", "Cutoff", 0, 0xFFFF, MPF_STATE, 0x7FFF};
CMachineParameter const paraCutSweep = {"Cutoff sweep", "Cutoff sweep", -127, 127, MPF_STATE, 0};
CMachineParameter const paraCutLFOSpeed = {"Cutoff LFO speed", "Cutoff", 0, 0xFF, MPF_STATE, 0};
CMachineParameter const paraCutLFODepth = {"Cutoff LFO depth","Cutoff LFO depth", 0, 0x2000, MPF_STATE, 0};
CMachineParameter const paraResonance = {"Resonance", "Resonance", 0, 0xFFFF, MPF_STATE, 0};
CMachineParameter const paraMod = {"Modulation", "Modulation", 0, 0xFFFF, MPF_STATE, 0};
CMachineParameter const paraDecay = {"Decay", "Modulation", 0, 0xFFFF, MPF_STATE, 0};
CMachineParameter const paraLFO = {"Vibrato&Sweep", "Vibrato&Sweep", 0, 0, MPF_LABEL, 0};
CMachineParameter const paraSpeed = {"Vibrato Speed", "Vibrato Speed", 0, 0xFF, MPF_STATE, 0};
CMachineParameter const paraDepth = {"Vibrato Depth", "Vibrato Depth", 0, 0xFF, MPF_STATE, 0};
CMachineParameter const paraType = {"LFO Type", "LFO Type", 0, 2, MPF_STATE, 0};
CMachineParameter const paraDelay = {"Vibrato attack", "Vibrato attack", 0, 0xFF, MPF_STATE, 0};
CMachineParameter const paraSweepDelay = {"sweep delay", "sweep delay", 0, 0xFF, MPF_STATE, 0};
CMachineParameter const paraSweepValue = {"sweep amount", "sweep amount",	-127, 127, MPF_STATE, 0};
CMachineParameter const paraChorus = {"Chorus", "Chorus", 0, 0, MPF_LABEL, 0};
CMachineParameter const paraChorusOnOff = {"On/Off", "On/Off", 0, 1, MPF_STATE, 0};
CMachineParameter const paraChorusDelay = {"Delay", "Delay", 0, 127, MPF_STATE, 3};
CMachineParameter const paraChorusLFOSpeed = {"LFO Speed", "LFO Speed", 0, 127, MPF_STATE, 16};
CMachineParameter const paraChorusLFODepth = {"LFO Depth", "LFO Depth", 0, 127, MPF_STATE, 3};
CMachineParameter const paraChorusFB = {"Amount", "Amoun t", 1, 100, MPF_STATE, 64};
CMachineParameter const paraChorusWidth = {"Stereo Width", "Stereo Width", -127, 127, MPF_STATE, -10};

CMachineParameter const *pParameters[] = 
{ 
	&paraCoarse,
	&paraFine,
	&paraPB,
	&paraPD,
	&paraExpression,
	&paraDist,
	&paraFilter,
	&paraCutOff,
	&paraCutSweep,
	&paraCutLFOSpeed,
	&paraCutLFODepth,
	&paraResonance,
	&paraMod,
	&paraDecay,
	&paraLFO,
	&paraSpeed,
	&paraDepth,
	&paraType,
	&paraDelay,
	&paraSweepDelay,
	&paraSweepValue,
	&paraChorus,
	&paraChorusOnOff,
	&paraChorusDelay,
	&paraChorusLFOSpeed,
	&paraChorusLFODepth,
	&paraChorusFB,
	&paraChorusWidth
};


CMachineInfo const MacInfo (
	MI_VERSION,
	0x0020,
	GENERATOR,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
#ifdef _DEBUG
	"LegaSynth TB303 (Debug build)",
#else
	"LegaSynth TB303",
#endif
	"TB303",
	"Juan Linietsky, ported by Sartorius",
	"Help",
	2
);

class mi : public CMachineInterface
{
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

	TB303_Voice track[MAX_TRACKS];
	TB303_Voice::Data track_data;
	Chorus				chorus;
	int samplerate;
};

PSYCLE__PLUGIN__INSTANTIATOR("legasynth-303", mi, MacInfo)

mi::mi()
{
	Vals=new int[MacInfo.numParameters];
}

mi::~mi()
{
	// Destroy dinamically allocated objects/memory here
	delete[] Vals;
}

void mi::Init()
{
	// Initialize your stuff here
	samplerate = pCB->GetSamplingRate();
	track_data.LFO_speed=0;
	track_data.LFO_depth=0;
	track_data.LFO_type=0; // 0 - sine / 1 - saw / 2- pulse
	track_data.LFO_delay=0; 
	
	track_data.sweep_delay=0;
	track_data.sweep_value=0;
	
	track_data.coarse = 0;
	track_data.fine=0; //coarse -100 to 100
	track_data.envelope_cutoff=0x7FFF; //"Cut", "Filter envelope cutoff 0 - 0xFFFF"
	track_data.resonance=0; //"Res", "Filter resonance - 0 - 0xFFFF 
	track_data.envelope_mod=0; //"Env", "Filter envelope modulation"
	track_data.envelope_decay=0; // "Filter envelope decay" - 0 - 0xFFFF

	track_data.cutoff_sweep=0;
	track_data.cutoff_lfo_speed=0;
	track_data.cutoff_lfo_depth=0;


	for(int i=0;i<MAX_TRACKS;i++)
	{
		track[i].set_data(track_data);

		track[i].set_mix_frequency(samplerate);
		track[i].set_note_off(0);
		track[i].set_sustain(false);
	}
	chorus.set_mixfreq(samplerate);
}

void mi::Stop()
{
	for(int c=0;c<MAX_TRACKS;c++)
	{
		track[c].set_note_off(0);
	}
}

void mi::SequencerTick()
{
	// Called on each tick while sequencer is playing
	if(samplerate!=pCB->GetSamplingRate())
	{
		samplerate=pCB->GetSamplingRate();
		chorus.set_mixfreq(samplerate);
		for(int i=0;i<MAX_TRACKS;i++)
			track[i].set_mix_frequency(samplerate);
	}

}

void mi::ParameterTweak(int par, int val)
{
	// Called when a parameter is changed by the host app / user gui
	int pb = Vals[2];
	int pd = Vals[3];
	char expr = (Vals[4]&0xFF);
	Vals[par]=val;
	switch (par) {
		case 0:track_data.coarse = val; break;
		case 1:track_data.fine = val; break;
		case 2:pb = val; break;
		case 3:pd = val; break;
		case 4:expr = val; break;
		case 5: break;
		case 6: break;
		case 7:track_data.envelope_cutoff = val; break;
		case 8:track_data.cutoff_sweep = val; break;
		case 9:track_data.cutoff_lfo_speed = val; break;
		case 10:track_data.cutoff_lfo_depth = val; break;
		case 11:track_data.resonance = val; break;
		case 12:track_data.envelope_mod = val; break;
		case 13:track_data.envelope_decay = val;break;
		case 14:break;
		case 15:track_data.LFO_speed = val;break;
		case 16:track_data.LFO_depth = val;break;
		case 17:track_data.LFO_type = val;break;
		case 18:track_data.LFO_delay = val;break;
		case 19:track_data.sweep_delay = val;break;
		case 20:track_data.sweep_value = val;break;
		case 21:break;
		case 22:break;
		case 23:chorus.set_delay(val);break;
		case 24:chorus.set_lfo_speed(val*0.125f);break;
		case 25:chorus.set_lfo_depth(val*0.1f);break;
		case 26:chorus.set_feedback(val);break;
		case 27:chorus.set_width(val/1.27f);break;
	}
	if (par>1 && par<5){
		for(int c=0;c<MAX_TRACKS;c++) {
			track[c].set_pitch_bend(pb);
			track[c].set_pitch_depth(pd);
			track[c].set_main_volume(expr);
		}
	} else if (par>14 && par<21) {
		for(int c=0;c<MAX_TRACKS;c++) {
			track[c].set_data(track_data);
		}
	}
}

void mi::Command()
{
	// Called when user presses editor button
	// Probably you want to show your custom window here
	// or an about button
	char buffer[2048];

	sprintf(
			buffer,"%s",
			"LegaSynth TB303\n"
			);

	pCB->MessBox(buffer,"LegaSynth TB303",0);

}

// Work... where all is cooked 
void mi::Work(float *psamplesleft, float *psamplesright , int numsamples,int tracks)
{
	double const range = 0.0000152587890625; // change range from 32bits to 16bits (TODO ?)
	float const dist = (float) Vals[5] * 0.005f;
	for(int c=0;c<tracks;c++)
	{
		TB303_Voice *ptrack=&track[c];
		Voice::Status status = ptrack->get_status();
		if (status != Voice::DEAD) {
			ptrack->mix(numsamples,psamplesleft,psamplesright);
		}
	}
	if(Vals[5]>0) {
		float *xpsamplesleft = psamplesleft;
		float *xpsamplesright = psamplesright;
		--xpsamplesleft;
		--xpsamplesright;
		for(int k = 0; k < numsamples; ++k)
		{
			const float fl = *++xpsamplesleft;
			const double il = fl * dist;
			*xpsamplesleft = psycle::helpers::math::clip<16>(fl - range * (il * il - il * il * il));
			const float fr = *++xpsamplesright;
			const double ir = fr * dist;
			*xpsamplesright = psycle::helpers::math::clip<16>(fr - range * (ir * ir - ir * ir * ir));
		}
	}

	if (Vals[22]==1){
		chorus.process(psamplesleft,psamplesright,numsamples);
	}
}

// Function that describes value on client's displaying
bool mi::DescribeValue(char* txt,int const param, int const value)
{
	switch (param)
	{
	case 0://fallthrough
	case 3:
		sprintf(txt,"%d notes",value);
		return true;
	case 1:
		sprintf(txt,"%d cents",value);
		return true;
	case 2:
		sprintf(txt,"%.02f%% (%.02f)",(float)(value-8192.f)/81.92f, Vals[3]*(float)(value-8192.f)/8192.f);
		return true;
	case 5:
		sprintf(txt,"%i %%",value);
		return true;
	case 8:
		sprintf(txt,"%.02f%%/s",value*16*Voice::UPDATES_PER_SECOND/655.35f);
		return true;
	case 9:
		sprintf(txt,"%.02fHz",(value+1)*Voice::UPDATES_PER_SECOND*(0.1f/256.f));
		return true;
	case 10:
		sprintf(txt,"%.02f%%", value*(1.0f/81.92f));
		return true;
	case 13:
		sprintf(txt,"%.02fms",50 + (2450*value*0.000015259021896696421759365224689097f)); /*/65535.0;*/
		return true;
	case 15:
		sprintf(txt,"%.02fHz",value*(Voice::UPDATES_PER_SECOND/819.2f));
		return true;
	case 16:
		sprintf(txt,"%.02f notes",value*0.1f);
		return true;
	case 17:
		switch(value)
		{
		case 0:sprintf(txt,"Sine");return true;break;
		case 1:sprintf(txt,"Sawtooth");return true;break;
		case 2:sprintf(txt,"Square");return true;break;
		}
	case 18: //fallthrough
	case 19:
		sprintf(txt,"%.02fms",value*(1000.f/Voice::UPDATES_PER_SECOND));
		return true;
	case 20:
		sprintf(txt,"%.02f notes/s", (std::pow(std::abs(value),1.6)*0.0025*Voice::UPDATES_PER_SECOND)*((value<0)?-1.0:1.0));
		return true;
	case 22:
		sprintf(txt,value?"On":"Off");
		return true;
	case 23:
		sprintf(txt,"%d ms",value);
		return true;
	case 24:
		sprintf(txt,"L%.02f Hz, R%.02f Hz",value*0.125f, value*0.15625f);
		return true;
	case 25:
		sprintf(txt,"%.01f ms",value*0.1f);
		return true;
	case 26:
		sprintf(txt,"%d %%",value);
		return true;
	case 27:
		if(value<0) {
			sprintf(txt,"%.0f%% - %.0f%%", 100.f-(value/1.27f), value/1.27f);
		}
		else if(value == 0) { sprintf(txt,"Left - Right"); }
		else if(value == 64) { sprintf(txt,"Center"); }
		else if(value== 127) { sprintf(txt,"Right - Left"); }
		else { sprintf(txt,"%.0f%% - %.0f%%", 100.f-(value/1.27f), value/1.27f); }
		
		return true;
	default:
		break;
	};
	return false;
}

//////////////////////////////////////////////////////////////////////
// The SeqTick function where your notes and pattern command handlers
// should be processed. Called each tick.
// Is called by the host sequencer
	
void mi::SeqTick(int channel, int note, int ins, int cmd, int val)
{
	// Note Off												== 120
	// Empty Note Row				== 255
	// Less than note off value??? == NoteON!
	
	// Note off
	if(note==NOTE_NOTEOFF)
	{
		track[channel].set_note_off(0);
	}
	else if (note<=NOTE_MAX)
	// Note on
	{
		track[channel].set_note(note,Vals[4]);
	}

	if( (note < 121 || note == 255 ) && (cmd == 0x0C)) track[channel].set_velocity(val /2);
	
}
}