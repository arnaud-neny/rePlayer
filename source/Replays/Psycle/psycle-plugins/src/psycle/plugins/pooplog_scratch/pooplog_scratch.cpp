//////////////////////////////////////////////////////////////////////
// pooplog scratch...

/*
/////////////////////////////////////////////////////////////////////
Version History

v0.06b
5/19/2003
- added some anti-denormal code

v0.05b
- fixed buffer length to be accurate (sorry if you have to change your settings)

v0.04b
- optimizations

v0.03b
1:42 PM 11/21/2002
- fixed to work at any sample rate

v0.02b
7:06 PM 10/23/2002
- fixed buffer issues
- fixed crosstalk issues
- fixed unbalance right/left text
- added more buffer lengths in the "fun range"
- added input gain

v0.01b
- initial release

// some sort of smoothing flag for wrap, or even entire interpolation mode
// lfo
// a wet/dry for signal coming in to buffer - this will work like resonance
/////////////////////////////////////////////////////////////////////
	*/
#include <psycle/plugin_interface.hpp>
#include <psycle/helpers/math.hpp>
#include <cstring>
#include <cstdlib>
#include <cstdio>
namespace psycle::plugins::pooplog_scratch {
using namespace psycle::plugin_interface;
using namespace psycle::helpers::math;

#define PLUGIN_NAME "Pooplog Scratch Master 0.06b"

#define DRAG_MAX 4096
#define DRAG_MAX_f 4096.0f
#define SPEED_MAX 4096
#define SPEED_ONE 1024.0f
#define MAX_BUF 1024*1024*4


#define NUM_BUFF 89
const static float buffindex[NUM_BUFF]={
	1/256.0f,
	1/192.0f,
	1/128.0f,
	1/96.0f,
		1/64.0f,
		1/48.0f,
		1/32.0f,
		1/25.0f,
		1/24.0f,
		1/20.0f,
		1/16.0f,
		1/12.0f,
		1/10.0f,
		1/8.0f,
		1/6.0f,
		1/5.0f,
		1/4.0f,
		1/3.0f,
		1/2.0f,
		2/3.0f,
		3/4.0f,
		1.0f,
		1.25f,
		1.5f,
		1.75f,
		2.0f,
		2.5f,
		3.0f,
		4.0f,
		5.0f,
		6.0f,
		7.0f,
		8.0f,
		10.0f,
		12.0f,
		14.0f,
		16.0f,
		18.0f,
		20.0f,
		22.0f,
		24.0f,
		26.0f,
		28.0f,
		30.0f,
		32.0f,
		34.0f,
		36.0f,
		38.0f,
		40.0f,
		42.0f,
		44.0f,
		46.0f,
		48.0f,
		50.0f,
		52.0f,
		54.0f,
		56.0f,
		58.0f,
		60.0f,
		62.0f,
		64.0f,
		68.0f,
		72.0f,
		76.0f,
		80.0f,
		84.0f,
		88.0f,
		92.0f,
		96.0f,
	104.0f,
	112.0f,
	120.0f,
	128.0f,
	136.0f,
	144.0f,
	152.0f,
	160.0f,
	168.0f,
	176.0f,
	184.0f,
	192.0f,
	200.0f,
	208.0f,
	216.0f,
	224.0f,
	232.0f,
	240.0f,
	248.0f,
	256.0f};

CMachineParameter const paraLength = {"Buffer Length", "Buffer Length", 0, NUM_BUFF-1, MPF_STATE, 25};
CMachineParameter const paraSpeed = {"Scratch Speed", "Scratch Speed", int(-SPEED_ONE*2), SPEED_MAX, MPF_STATE, int(SPEED_ONE)};
CMachineParameter const paraDragL = {"Left Drag Delay", "Left Drag Delay", 0, DRAG_MAX, MPF_STATE, DRAG_MAX/2};
CMachineParameter const paraDragR = {"Right Drag Delay", "Right Drag Delay", 0, DRAG_MAX, MPF_STATE, DRAG_MAX/2};
CMachineParameter const paraUnbalance = {"Speed Unbalance", "Speed Unbalance", 0, 512, MPF_STATE, 256};
CMachineParameter const paraFeedback = {"Feedback", "Feedback", 0, 512, MPF_STATE, 256};
CMachineParameter const paraMix = {"Mix", "Mix", 0, 256, MPF_STATE, 256};
CMachineParameter const paraInputGain = {"Input Gain", "Input Gain", 0, 1024, MPF_STATE, 256};

enum
{ 
	// global
	e_paraLength,
	e_paraSpeed,
	e_paraDragL,
	e_paraDragR,
	e_paraUnbalance,
	e_paraFeedback,
	e_paraInputGain,
	e_paraMix
};

CMachineParameter const *pParameters[] = 
{ 
	// global
	&paraLength,
	&paraSpeed,
	&paraDragL,
	&paraDragR,
	&paraUnbalance,
	&paraFeedback,
	&paraInputGain,
	&paraMix,
};


CMachineInfo const MacInfo (
	MI_VERSION,
	0x0006,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	PLUGIN_NAME,
	"Pooplog Scratch",
	"Jeremy Evers",
	"About",
	4
);


class mi : public CMachineInterface
{
public:
	float denormal;
	mi();
	virtual ~mi();

	virtual void Init();
	virtual void SequencerTick();
	virtual void Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks);
	virtual bool DescribeValue(char* txt,int const param, int const value);
	virtual void Command();
	virtual void ParameterTweak(int par, int val);

private:
	void RebuildBuffers();

	float InGain;
	float *pBufferL;
	float *pBufferR;
	int bufsize;
	float rdelta;
	float ldelta;
	float ddelta;
	float rdrag;
	float ldrag;
	float rlindex;
	float rrindex;
	float dmix;
	float wmix;
	float feedback;
	float runbal;
	float lunbal;
	int windex;
	int song_sync;
	int song_freq;
};

PSYCLE__PLUGIN__INSTANTIATOR("pooplog-scratch-master", mi, MacInfo)
//DLL_EXPORTS

mi::mi()
{
	denormal = (float)1.0E-18;
	// The constructor zone
	Vals = new int[MacInfo.numParameters];
	Vals[e_paraLength] = 17;
}

mi::~mi()
{
	delete[] Vals;
	if (pBufferL)
	{
		delete[] pBufferL;
	}
	if (pBufferR)
	{
		delete[] pBufferR;
	}
// Destroy dinamically allocated objects/memory here
}

void mi::Init()
{
// Initialize your stuff here
	pBufferL = NULL;
	pBufferR = NULL;
	bufsize = 256;
	rdelta = 1;
	ldelta = 1;
	ddelta = 1;
	rdrag = 1;
	ldrag = 1;
	rlindex = 0;
	rrindex = 0;
	windex = 1;
	pBufferL = new float[MAX_BUF];
	if (pBufferL)
	{
		memset(pBufferL,0,sizeof(float)*(MAX_BUF));
	}
	pBufferR = new float[MAX_BUF];
	if (pBufferR)
	{
		memset(pBufferR,0,sizeof(float)*(MAX_BUF));
	}
	song_sync = pCB->GetBPM();
	song_freq = pCB->GetSamplingRate();
	InGain = 1;
}

void mi::SequencerTick()
{
// Called on each tick while sequencer is playing
	if (pCB->GetBPM() != song_sync)
	{
		RebuildBuffers();
	}
	else if (pCB->GetSamplingRate() != song_freq)
	{
		RebuildBuffers();
	}
}

void mi::Command()
{
// Called when user presses editor button
// Probably you want to show your custom window here
// or an about button
pCB->MessBox("Jeremy Evers\r\nnegspect@hotmail.com",PLUGIN_NAME,0);
}

void mi::RebuildBuffers()
{
	song_freq = pCB->GetSamplingRate();
	song_sync = pCB->GetBPM();
	bufsize = int(((buffindex[Vals[0]]*song_freq*60.0f)/song_sync)+0.5f);
	if (bufsize < 4)
	{
		bufsize = 4;
	}
	else if (bufsize > MAX_BUF)
	{
		bufsize = MAX_BUF;
	}
	while(rrindex >= bufsize)
	{
		rrindex -= bufsize;
	}
	while(rlindex >= bufsize)
	{
		rlindex -= bufsize;
	}
	while(windex >= bufsize)
	{
		windex -= bufsize;
	}
}

void mi::ParameterTweak(int par, int val)
{
	Vals[par]=val;
	switch (par)
	{
		case e_paraLength:
			// buffer size
			RebuildBuffers();
			break;
		case e_paraSpeed:
			// speed
			if (val >= 0)
			{
				ddelta = (val/SPEED_ONE)*(val/SPEED_ONE);
			}
			else
			{
				ddelta = -(val/SPEED_ONE)*(val/SPEED_ONE);
			}
			break;
		case e_paraDragL:
			// drag left
			ldrag = (DRAG_MAX-val)/DRAG_MAX_f;
			break;
		case e_paraDragR:
			// drag right
			rdrag = (DRAG_MAX-val)/DRAG_MAX_f;
			break;
		case e_paraUnbalance:
			// unbalance
			if (val <= 256)
			{
				// left
				runbal = 1-((256-val)/512.0f);
				runbal *= runbal;
				lunbal = 1;
			}
			else
			{
				// right
				runbal = 1;
				lunbal = 1-((val-256)/512.0f);
				lunbal *= lunbal;
			}
			break;
		case e_paraFeedback:
			// feedback
			feedback = (val-256)/256.0f;
			break;
		case e_paraMix:
			wmix = val/256.0f;
			dmix = 1.0f-wmix;
			break;
		case e_paraInputGain:
			InGain = (val/256.0f)*(val/256.0f);
			break;
	}
}

// Work... where all is cooked 
void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks)
{
	do
	{
		float sl = *psamplesleft*InGain;
		float sr = *psamplesright*InGain;
		float sol = 0.f;
		float sor = 0.f;

		if (pBufferL)
		{
			int i = rint<int>(rlindex);
			if (i < (bufsize-1))
			{
				sol = (pBufferL[i]*(1.0f-(rlindex-i)))+(pBufferL[i+1]*(rlindex-i))+denormal;
			}
			else
			{
				sol = (pBufferL[i]*(1.0f-(rlindex-i)))+(pBufferL[0]*(rlindex-i))+denormal;
			}
			pBufferL[windex] = sl+(sol*feedback)+denormal;
		}
		if (pBufferR)
		{
			int i = rint<int>(rrindex);
			if (i < (bufsize-1))
			{
				sor = (pBufferR[i]*(1.0f-(rrindex-i)))+(pBufferR[i+1]*(rrindex-i))+denormal;
			}
			else
			{
				sor = (pBufferR[i]*(1.0f-(rrindex-i)))+(pBufferR[0]*(rrindex-i))+denormal;
			}
			pBufferR[windex] = sr+(sor*feedback)+denormal;
		}

		rrindex += rdelta;
		rlindex += ldelta;
		while(rrindex < 0)
		{
			rrindex += bufsize;
		}
		while(rrindex >= bufsize)
		{
			rrindex -= bufsize;
		}
		while(rlindex < 0)
		{
			rlindex += bufsize;
		}
		while(rlindex >= bufsize)
		{
			rlindex -= bufsize;
		}
		windex++;
		while(windex >= bufsize)
		{
			windex -= bufsize;
		}

		*psamplesleft=(sl*dmix)+(sol*wmix);
		*psamplesright=(sr*dmix)+(sor*wmix);

		++psamplesleft;
		++psamplesright;
	} while(--numsamples);
	rdelta += (((ddelta*runbal) - rdelta)*rdrag);
	ldelta += (((ddelta*lunbal) - ldelta)*ldrag);

	denormal =- denormal;
}

// Function that describes value on client's displaying
bool mi::DescribeValue(char* txt,int const param, int const value)
{
	switch (param)
	{
	case e_paraLength:
		// buffer length
		sprintf(txt, "%.6f beats",buffindex[value]);
		return true;
		break;
	case e_paraSpeed:
		// speed
		if (value >= 0)
		{
			sprintf(txt,"%.2f x",(value/SPEED_ONE)*(value/SPEED_ONE));
		}
		else
		{
			sprintf(txt,"-%.2f x",(value/SPEED_ONE)*(value/SPEED_ONE));
		}
		return true;
		break;
	case e_paraDragL:
		// drag left
		case e_paraDragR:
		// drag right
		sprintf(txt,"%.2f %%",100*(value)/DRAG_MAX_f);
		return true;
		break;
	case e_paraUnbalance:
		// unbalance
		if (value < 256)
		{
			sprintf(txt,"%.2f %% right",-(100*((value-256)/512.0f)*((value-256)/512.0f)));
			return true;
		}
		else if (value > 256)
		{
			sprintf(txt,"%.2f %% left",-(100*((256-value)/512.0f)*((256-value)/512.0f)));
			return true;
		}
		else 
		{
			sprintf(txt,"center");
			return true;
		}
		break;
	case e_paraFeedback:
		// feedback
		sprintf(txt,"%.2f %%",100*(value-256)/256.0f);
		return true;
	case e_paraMix:
		// mix
		sprintf(txt,"%.2f %%",100*(value)/256.0f);
		return true;
	case e_paraInputGain:
		// mix
		sprintf(txt,"%.2f %%",100*(value/256.0f)*(value/256.0f));
		return true;
	}
	return false;
}
}