#include <psycle/plugin_interface.hpp>
#include "biquad.hpp"
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <cstdio>

//////////////////////////////////////////////////////////////////////
// KarLKoX "Surround" plugin for PSYCLE
namespace psycle::plugins::surround {
using namespace psycle::plugin_interface;

const short VERNUM=0x120;

CMachineParameter const paraLength = {"Cutoff Frequency", "Cutoff for HighPass Filter", 0, 1000, MPF_STATE, 400};
CMachineParameter const paraMode = {"Work Mode", "Working model for the surround", 0, 1, MPF_STATE, 0};
CMachineParameter const *pParameters[] =
{	// global
	&paraLength,
	&paraMode
};

CMachineInfo const MacInfo (
	MI_VERSION,
	VERNUM,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"KarLKoX Surround "
		#ifndef NDEBUG
			" (Debug build)"
		#endif
		,
	"Surround",
	"Saïd Bougribate",
	"About",
	2
);

class mi : public CMachineInterface
{
public:
	mi();
	virtual ~mi();

	virtual void Init();
	virtual void SequencerTick();
	virtual void Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks);
	virtual bool DescribeValue(char* txt,int const param, int const value);
	virtual void Command();
	virtual void ParameterTweak(int par, int val);
	void Setup();
	
private:
	biquad bqleft, bqright;
	int smprate;
	bool initialized;

};

PSYCLE__PLUGIN__INSTANTIATOR("karlkox-surround", mi, MacInfo)

mi::mi()
{
	Vals = new int[MacInfo.numParameters];
	initialized=false;
}

mi::~mi()
{
	delete [] Vals;
}

void mi::Init()
{
	Setup();
}

void mi::Setup()
{
	smprate=pCB->GetSamplingRate();
	switch( Vals[1]  )
	{
	case 0:
		BiQuad_new(LPF, 0.0f, (float)Vals[0], (float)smprate, 1, &bqleft,!initialized);
		initialized=true;
		break;
	case 1:
		#if 0
			BiQuad_new(HPF, 0.0f, (float)Vals[0], (float)smprate, 1, &bqleft,!initialized);
			BiQuad_new(HPF, 0.0f, (float)Vals[0], (float)smprate, 1, &bqright,!initialized);
			initialized = true;
		#endif
		break;
	}
}

void mi::SequencerTick()
{
	if (pCB->GetSamplingRate() != smprate ) Setup();
}

void mi::Command()
{
	pCB->MessBox("Made 14/12/2001 by Saïd Bougribate for Psycl3!\n\n Some modifications made by [JAZ] on Dec 2002","-=KarLKoX=- [Surround]",0);
}

void mi::ParameterTweak(int par, int val)
{
	Vals[par] = val;
	Setup();
}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks)
{
	if(!initialized) Setup();
	--psamplesleft;
	--psamplesright;
	// over all samples 
	switch(Vals[1])
	{
	case 0:
		do
		{
			float xl = *++psamplesleft;

			*psamplesleft  = -xl + 2*(float)BiQuad(xl, &bqleft); //BQ is a Lowpass

		} while(--numsamples);
		break;
	case 1:
		#if 0
			// This code was meant to make surround just out of the stereo part of a signal
			// concretely it means that it doesn't affect the mono components of the signal.
			do
			{
				float xl = *++psamplesleft;
				float xr = *++psamplesright;
				
				float xtl = (float)BiQuad(xl, &bqleft); // BQ is a HighPass
				float xtr = (float)BiQuad(xr, &bqright); // BQ is a HighPass

				*psamplesleft  = xl+xtl*0.5-xtr*0.5;
				*psamplesright  = xr-xtl*0.5+xtr*0.5;
			} while(--numsamples);
		#endif
		break;
	}
}

bool mi::DescribeValue(char* txt,int const param, int const value)
{
	switch (param)
	{
	case 0:
		sprintf(txt,"%i Hz",value);
		return true;
	case 1:
		switch(value)
		{
		case 0:
			strcpy(txt,"On");
			return true;
		case 1:
			strcpy(txt,"Off");
			return true;
		}
		break;
	}
	return false;
}
}