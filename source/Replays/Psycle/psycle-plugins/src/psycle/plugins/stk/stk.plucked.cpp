/////////////////////////////////////////////////////////////////////
// Dmitry "Sartorius" Kulikov stk Plucked plugin for PSYCLE
// v0.3
//
// Based on The Synthesis ToolKit in C++ (STK)
// By Perry R. Cook and Gary P. Scavone, 1995-2004.
// http://ccrma.stanford.edu/software/stk/

#include <psycle/plugin_interface.hpp>
#include <stk/Stk.h>
#include <stk/Plucked.h>
#include <stk/ADSR.h>
#include <cmath>
namespace psycle::plugins::stk::plucked {

using namespace psycle::plugin_interface;

using namespace ::stk;

StkFloat const offset(-36.3763165623); // 6 * 12 - 3 - 12 * ln(440) / ln(2)

CMachineParameter const paraVolume = {"Volume", "Volume", 0, 32767, MPF_STATE, 32767};
CMachineParameter const paraAttack = {"Attack", "Attack", 32, 32767, MPF_STATE, 32};
CMachineParameter const paraDecay = {"Decay", "Decay", 32, 32767, MPF_STATE, 32};
CMachineParameter const paraSustain = {"Sustain", "Sustain", 0, 32767, MPF_STATE, 16768};
CMachineParameter const paraRelease = {"Release", "Release", 32, 32767, MPF_STATE, 328};

CMachineParameter const *pParameters[] = 
{ 
	&paraVolume,
	&paraAttack,
	&paraDecay,
	&paraSustain,
	&paraRelease
};


CMachineInfo const MacInfo (
	MI_VERSION,
	0x0100,
	GENERATOR,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
#ifdef _DEBUG
	"stk Plucked (Debug build)",
#else
	"stk Plucked",
#endif
	"stk Plucked",
	"Sartorius, Bohan and STK 4.2.0 developers",
	"Help",
	1
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
	Plucked * track[MAX_TRACKS];
	ADSR				adsr[MAX_TRACKS];
	float				vol_ctrl[MAX_TRACKS];
	StkFloat n2f[NOTE_MAX+1];
	std::vector<int> w_tracks;
	StkFloat samplerate;
};

PSYCLE__PLUGIN__INSTANTIATOR("stk-plucked", mi, MacInfo)

mi::mi()
{
	Vals=new int[MacInfo.numParameters];

	for (int note=0;note<=NOTE_MAX;note++)
	{
		n2f[note]= std::pow(2., (note - offset) / 12);
	}
	
	for(int i = 0; i < MAX_TRACKS; ++i) {
		track[i] = new Plucked(
			#if STK_VERSION != -1
				20 // lowest desired playing frequency
			#endif
		);
	}
}

mi::~mi()
{
	// Destroy dinamically allocated objects/memory here
	delete[] Vals;
	for(int i = 0; i < MAX_TRACKS; ++i) delete track[i];
}

void mi::Init()
{
// Initialize your stuff here
	
	w_tracks.push_back( 0 ); // 
	
	samplerate = (StkFloat)pCB->GetSamplingRate();
	Stk::setSampleRate(samplerate);
	for(int i=0;i<MAX_TRACKS;i++)
	{
		track[i]->clear();
		track[i]->noteOff(0.0);
		adsr[i].setAllTimes(StkFloat(Vals[1]*0.000030517578125),
									StkFloat(Vals[2]*0.000030517578125),
									StkFloat(Vals[3]*0.000030517578125),
									StkFloat(Vals[4]*0.000030517578125));
		vol_ctrl[i] = 1.f;
	}
}

void mi::Stop()
{
	for(int c=0;c<MAX_TRACKS;c++)
	{
		adsr[c].keyOff();
		//track[c]->noteOff(0.0);
		track[c]->clear();
	}
}

void mi::SequencerTick()
{
// Called on each tick while sequencer is playing
	if(samplerate!=(StkFloat)pCB->GetSamplingRate())
	{
		samplerate = (StkFloat)pCB->GetSamplingRate();
		Stk::setSampleRate(samplerate);
		for(int c=0;c<MAX_TRACKS;c++)
		{
			adsr[c].setAllTimes(StkFloat(Vals[1]*0.000030517578125),
									StkFloat(Vals[2]*0.000030517578125),
									StkFloat(Vals[3]*0.000030517578125),
									StkFloat(Vals[4]*0.000030517578125));
		}
	}

}

void mi::ParameterTweak(int par, int val)
{
	// Called when a parameter is changed by the host app / user gui
	Vals[par]=val;
	for(int c=0;c<MAX_TRACKS;c++)
		{
			adsr[c].setAllTimes(StkFloat(Vals[1]*0.000030517578125),
									StkFloat(Vals[2]*0.000030517578125),
									StkFloat(Vals[3]*0.000030517578125),
									StkFloat(Vals[4]*0.000030517578125));
		}
}

void mi::Command()
{
// Called when user presses editor button
// Probably you want to show your custom window here
// or an about button

pCB->MessBox("Simple Plucked","stk Plucked",0);

}

// Work... where all is cooked 
void mi::Work(float *psamplesleft, float *psamplesright , int numsamples,int tracks)
{
	float sl=0;
	float const vol=(float)Vals[0];

	for(std::vector<int>::iterator w_tracks_iter=w_tracks.begin();w_tracks_iter!=w_tracks.end();w_tracks_iter++)
	{
			float *xpsamplesleft=psamplesleft;
			float *xpsamplesright=psamplesright;
			--xpsamplesleft;
			--xpsamplesright;

			int xnumsamples=numsamples;

			Plucked & track = *this->track[*w_tracks_iter];
			ADSR & adsr = this->adsr[*w_tracks_iter];
			do {
				sl = float(adsr.tick() * track.tick()) * vol;
				if(sl < -vol) sl = -vol;
				if(sl > +vol) sl = vol;

				sl *= vol_ctrl[*w_tracks_iter];

				*++xpsamplesleft += sl; // * vol_ctrl[w_tracks_iter];
				*++xpsamplesright += sl; // * vol_ctrl[w_tracks_iter];
			} while(--xnumsamples);
	}

}

// Function that describes value on client's displaying
bool mi::DescribeValue(char* txt,int const param, int const value)
{
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

	bool no_channel(true);

	for(std::vector<int>::iterator w_tracks_iter=w_tracks.begin();w_tracks_iter!=w_tracks.end();w_tracks_iter++)
	{
		if (*w_tracks_iter == channel)
		{
			no_channel = false;
			break;
		}
	}

	if (no_channel) w_tracks.push_back(channel);

	if(note!=NOTE_NONE)
	{
		// Note off
		if(note==NOTE_NOTEOFF)
		{
			adsr[channel].keyOff();
		}
		else 
		// Note on
		{
			adsr[channel].keyOn();
			//StkFloat const offset(-36.3763165623); // 6 * 12 - 3 - 12 * ln(440) / ln(2)
			//StkFloat const frequency = std::pow(2., (note - offset) / 12);
			//track[channel]->noteOff(0.0);
			track[channel]->noteOn(n2f[note],1.0);
		}
		//track[channel]->tick();
	}
	//adsr[channel].tick();
	
	if( cmd == 0x0C) vol_ctrl[channel] = val * .003921568627450980392156862745098f; // 1/255
}
}