/////////////////////////////////////////////////////////////////////
// Dmitry "Sartorius" Kulikov stk Shakers plugin for PSYCLE
// v0.3a
//
// Based on The Synthesis ToolKit in C++ (STK)
// By Perry R. Cook and Gary P. Scavone, 1995-2004.
// http://ccrma.stanford.edu/software/stk/

#include <psycle/plugin_interface.hpp>
#include <cstdio>
#include <cmath>
#include <stk/Stk.h>
#include <stk/Shakers.h>
namespace psycle::plugins::stk::shakers {
using namespace psycle::plugin_interface;

using namespace ::stk;

//In shakers 4.5.0, notes where changed
const int shakers_old_to_new[] = {0,1,2,19,21,5,3,4,8,9,20,6,7,12,13,14,15,16,17,18,10,11,22};


CMachineParameter const paraShakeEnergy = {"Shake Energy", "Shake Energy", 0, 128, MPF_STATE, 64};
CMachineParameter const paraDecay = {"Decay", "Decay", 0, 128, MPF_STATE, 64};
CMachineParameter const paraObjects = {"Objects", "Objects", 1, 128, MPF_STATE, 10};
CMachineParameter const paraResonanceFrequency = {"Resonance Frequency", "Resonance Frequency", 1, 128, MPF_STATE, 64};
CMachineParameter const paraShakeEnergy2 = {"Shake Energy2", "Shake Energy 2", 1, 128, MPF_STATE, 64};
CMachineParameter const paraVolume = {"Volume", "Volume", 0, 32767, MPF_STATE, 32767};

CMachineParameter const *pParameters[] = 
{ 
	&paraShakeEnergy,
	&paraDecay,
	&paraObjects,
	&paraResonanceFrequency,
	&paraShakeEnergy2,
	&paraVolume
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0100,
	GENERATOR,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
#ifdef _DEBUG
	"stk Shakers (Debug build)",
#else
	"stk Shakers",
#endif
	"Shakers",
	"Sartorius, bohan and STK 4.5.0 developers",
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

	Shakers track[MAX_TRACKS];
	bool noteonoff[MAX_TRACKS];
	float				vol_ctrl[MAX_TRACKS];
	StkFloat samplerate;
};

PSYCLE__PLUGIN__INSTANTIATOR("stk-shakers", mi, MacInfo)

mi::mi()
{
	Vals=new int[MacInfo.numParameters];
}

mi::~mi()
{
	delete[] Vals;

// Destroy dinamically allocated objects/memory here
}

void mi::Init()
{
// Initialize your stuff here
	samplerate = (StkFloat)pCB->GetSamplingRate();
	Stk::setSampleRate(samplerate);
	for(int i=0;i<MAX_TRACKS;i++)
	{
		noteonoff[i]=false;
		vol_ctrl[i]=1.f;
	}

}

void mi::Stop()
{
	for(int c=0;c<MAX_TRACKS;c++)
	{
		track[c].noteOff(0.0f);
		noteonoff[c]=false;
	}

}

void mi::SequencerTick()
{
// Called on each tick while sequencer is playing
	if(samplerate!=(StkFloat)pCB->GetSamplingRate())
	{
		samplerate = (StkFloat)pCB->GetSamplingRate();
		Stk::setSampleRate(samplerate);
	}
}

void mi::ParameterTweak(int par, int val)
{
	// Called when a parameter is changed by the host app / user gui
	Vals[par]=val;
	int contpar=0;
	switch(par)
	{
	case 0:contpar=2;break;
	case 1:contpar=4;break;
	case 2:contpar=11;break;
	case 3:contpar=1;break;
	case 4:contpar=128;break;
	}
	if (contpar!=0)
		for(int c=0;c<MAX_TRACKS;c++) track[c].controlChange(contpar,(StkFloat)val);
}

void mi::Command()
{
// Called when user presses editor button
// Probably you want to show your custom window here
// or an about button
char buffer[2048];

sprintf(
		buffer,"%s%s%s%s%s%s%s%s%s%s%s%s%s",
		"Note  : Instrument\n",
		"\nC-4 : Maraca, C#4 : Cabasa",
		"\nD-4 : Sekere, D#4 : Guiro",
		"\nE-4 : Water Drops, F-4 : Bamboo Chimes",
		"\nF#4 : Tambourine, G-4 : Sleigh Bells",
		"\nG#4 : Sticks, A-4 : Crunch",
		"\nA#4 : Wrench, B-4 : Sand Paper",
		"\nC-5 : Coke Can, C#5 : Next Mug",
		"\nD-5 : Penny + Mug, D#5 : Nickle + Mug",
		"\nE-5 : Dime + Mug, F-5 : Quarter + Mug",
		"\nF#5 : Franc + Mug, G-5 : Peso + Mug",
		"\nG#5 : Big Rocks, A-5 : Little Rocks",
		"\nA#5 : Tuned Bamboo Chimes"
		);

pCB->MessBox(buffer,"stk Shakers",0);

}

// Work... where all is cooked 
void mi::Work(float *psamplesleft, float *psamplesright , int numsamples,int tracks)
{
	float sl=0;
//				float sr=0;
	float const vol=(float)Vals[5];
	for(int c=0;c<tracks;c++)
	{
		if(noteonoff[c])
		{
			float *xpsamplesleft=psamplesleft;
			float *xpsamplesright=psamplesright;
			--xpsamplesleft;
			--xpsamplesright;
			
			int xnumsamples=numsamples;
		
			Shakers *ptrack=&track[c];

			do
				{
					sl=(float)ptrack->tick()*vol;
					if (sl<-vol)sl=-vol;
					if (sl>vol)sl=vol;

					sl*=vol_ctrl[c];

					*++xpsamplesleft+=sl;
					*++xpsamplesright+=sl;
				} while(--xnumsamples);
		}
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
	// Note Off						== 120
	// Empty Note Row				== 255
	// Less than note off value??? == NoteON!
	
	if((note>=48) && (note< 71))
	{
		// change the instrument index from old indexes to new indexes
		int notechange = shakers_old_to_new[note-48];
		//Convert it to frequency, in a way that shakers understand.
		float freq= 220.f*pow(2.f, ((float)notechange+7.f)/12.f);
		track[channel].noteOn(freq,10.f);
		noteonoff[channel]=true;
	}

	// Note off
	if(note==NOTE_NOTEOFF)
	{
		track[channel].noteOff(0.0);
		noteonoff[channel]=false;
	}

	//track[channel].tick();

	if( cmd == 0x0C) vol_ctrl[channel] = val * .003921568627450980392156862745098f; // 1/255
}
}