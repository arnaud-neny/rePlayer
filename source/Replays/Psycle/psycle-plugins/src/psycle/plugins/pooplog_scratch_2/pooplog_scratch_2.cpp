//////////////////////////////////////////////////////////////////////
// pooplog scratch...

/*
/////////////////////////////////////////////////////////////////////
Version History

v0.06b
- added some anti-denormal code

v0.05b
- fixed buffer length to be accurate (sorry if you have to change your settings)

v0.04b
10:55 PM 12/14/2002
- fixed lfo phase knobs

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
/////////////////////////////////////////////////////////////////////
	*/

#include <psycle/plugin_interface.hpp>
#include <psycle/helpers/math.hpp>
#include <cstring>
#include <cstdlib>
#include <cstdio>
namespace psycle::plugins::pooplog_scratch_2 {
using namespace psycle::plugin_interface;
using namespace psycle::helpers::math;

#define PLUGIN_NAME "Pooplog Scratch Master 2 0.06b"

#define DRAG_MAX 4096
#define DRAG_MAX_f 4096.0f
#define SPEED_MAX 4096
#define SPEED_ONE 1024.0f
#define MAX_RATE								8192
#define MAX_BUF 1024*1024*4

#define MAXLFOWAVE 15
#define TWOPI																6.28318530717958647692528676655901f
#define SAMPLE_LENGTH  4096
#define MAXSYNCMODES 16

#define FILTER_CALC_TIME				32
#define MAXWAVE 17
#define WRAP_AROUND(x) if ((x < 0) || (x >= SAMPLE_LENGTH*2)) x = (x-rint<int>(x))+(rint<int>(x)&((SAMPLE_LENGTH*2)-1));
#define PI 3.14159265358979323846

float SyncAdd[MAXSYNCMODES+1];
float SourceWaveTable[MAXLFOWAVE+1][(SAMPLE_LENGTH*2)+256];

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
CMachineParameter const paraBufThru = {"Buffer Through", "Buffer Through", 0, 512, MPF_STATE, 256};
CMachineParameter const paraMix = {"Mix (Xfade)", "Mix (Xfade)", 0, 256, MPF_STATE, 256};
CMachineParameter const paraInputGain = {"Input Gain", "Input Gain", 0, 1024, MPF_STATE, 256};
CMachineParameter const paraSPEEDlfospeed = {"Speed LFO Rate", "Speed LFO Rate", 0, MAX_RATE, MPF_STATE, 6};
CMachineParameter const paraSPEEDlfoamplitude = {"Speed LFO Depth", "Speed LFO Depth", 0, 256, MPF_STATE, 0};
CMachineParameter const paraSPEEDlfowave = {"Speed LFO Wave", "Speed LFO Wave", 0, MAXLFOWAVE-1, MPF_STATE, 0};
CMachineParameter const paraMIXlfospeed = {"Mix LFO Rate", "Mix LFO Rate", 0, MAX_RATE, MPF_STATE, 6};
CMachineParameter const paraMIXlfowave = {"Mix LFO Wave", "Mix LFO Wave", 0, MAXLFOWAVE-1, MPF_STATE, 0};
CMachineParameter const paraMIXlfoamplitude = {"Mix LFO Depth", "Mix LFO Depth", 0, 256, MPF_STATE, 0};
CMachineParameter const paraSmoothing = {"Delta Smoothing", "Delta Smoothing", 0, 255, MPF_STATE, 0};
CMachineParameter const paraSPEEDlfophase = {"Speed LFO Phase", "Speed LFO Phase", 0, 0xffff, MPF_STATE, 0};
CMachineParameter const paraMIXlfophase = {"Mix LFO Phase", "Mix LFO Phase", 0, 0xffff, MPF_STATE, 0};
CMachineParameter const paraNULL = {" ", " ", 0, 0, MPF_NULL, 0};

enum
{ 
	// global
	e_paraLength,
	e_paraSpeed,
	e_paraSPEEDlfospeed,
	e_paraSPEEDlfoamplitude,
	e_paraSPEEDlfowave,

	e_paraDragL,
	e_paraDragR,
	e_paraMIXlfospeed,
	e_paraMIXlfoamplitude,
	e_paraMIXlfowave,

	e_paraUnbalance,
	e_paraFeedback,
	e_paraBufThru,
	e_paraSPEEDlfophase,
	e_paraMIXlfophase,

	e_paraSmoothing,
	e_paraInputGain,
	e_paraMix
};

CMachineParameter const *pParameters[] = 
{ 
	// global
	&paraLength,
	&paraSpeed,
	&paraSPEEDlfospeed,
	&paraSPEEDlfoamplitude,
	&paraSPEEDlfowave,
	&paraDragL,
	&paraDragR,
	&paraMIXlfospeed,
	&paraMIXlfoamplitude,
	&paraMIXlfowave,
	&paraUnbalance,
	&paraFeedback,
	&paraBufThru,
	&paraSPEEDlfophase,
	&paraMIXlfophase,
	&paraSmoothing,
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
	"Scratch 2",
	"Jeremy Evers",
	"About",
	4
);


class mi : public CMachineInterface
{
public:
	mi();
	virtual ~mi();
	float denormal;

	virtual void Init();
	virtual void SequencerTick();
	virtual void Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks);
	virtual bool DescribeValue(char* txt,int const param, int const value);
	virtual void Command();
	virtual void ParameterTweak(int par, int val);

private:
	void RebuildBuffers();
	void InitWaveTable();
	void FilterTick();


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
	float bufthru;
	float runbal;
	float lunbal;
	int windex;
	int song_sync;
	int song_freq;

	float * WaveTable[MAXWAVE+1];

	float *pspeedlfowave;
	int				speedlfowave;
	int speedlfospeed;
	float speedlfoamplitude;
	float speedlfophase;

	float *pmixlfowave;
	int				mixlfowave;
	int mixlfospeed;
	float mixlfoamplitude;
	float mixlfophase;

	float smoothing;

	float oldl;
	float oldr;
	int timetocompute;
};

PSYCLE__PLUGIN__INSTANTIATOR("pooplog-scratch-master-2", mi, MacInfo)
//DLL_EXPORTS

mi::mi()
{
	denormal = (float)10E-18;
	// The constructor zone
	Vals = new int[MacInfo.numParameters];
	Vals[e_paraLength] = 17;
	InitWaveTable();
	pspeedlfowave=SourceWaveTable[0];
	pmixlfowave=SourceWaveTable[0];
	oldl = 0;
	oldr = 0;
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
	timetocompute = 1;
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
	InGain = 1;
	speedlfoamplitude = 0;
	mixlfoamplitude = 0;

	song_sync = pCB->GetBPM();
	song_freq = pCB->GetSamplingRate();
	SyncAdd[0] = 0;  // 512 beats
	SyncAdd[1] = song_sync*(SAMPLE_LENGTH*2*0.015625f*0.125f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 512 beats
	SyncAdd[2] = song_sync*(SAMPLE_LENGTH*2*0.015625f*0.25f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 256 beats
	SyncAdd[3] = song_sync*(SAMPLE_LENGTH*2*0.015625f*0.5f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 128 beats
	SyncAdd[4] = song_sync*(SAMPLE_LENGTH*2*0.015625f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 64 beats
	SyncAdd[5] = song_sync*(SAMPLE_LENGTH*2*0.03125f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 32 beats
	SyncAdd[6] = song_sync*(SAMPLE_LENGTH*2*0.0625f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 16 beats
	SyncAdd[7] = song_sync*(SAMPLE_LENGTH*2*0.125f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 8 beats
	SyncAdd[8] = song_sync*(SAMPLE_LENGTH*2*0.25f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 4 beats
	SyncAdd[9] = song_sync*(SAMPLE_LENGTH*2*0.5f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 2 beats 
	SyncAdd[10] = song_sync*(SAMPLE_LENGTH*2*1.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // whole note
	SyncAdd[11] = song_sync*(SAMPLE_LENGTH*2*2.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 1/2 note
	SyncAdd[12] = song_sync*(SAMPLE_LENGTH*2*4.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 1/4 note
	SyncAdd[13] = song_sync*(SAMPLE_LENGTH*2*8.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 1/8 note
	SyncAdd[14] = song_sync*(SAMPLE_LENGTH*2*16.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 1/16 note
	SyncAdd[15] = song_sync*(SAMPLE_LENGTH*2*32.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 1/32 note
	SyncAdd[16] = song_sync*(SAMPLE_LENGTH*2*64.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 1/64 note

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
	SyncAdd[0] = 0;  // 512 beats
	SyncAdd[1] = song_sync*(SAMPLE_LENGTH*2*0.015625f*0.125f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 512 beats
	SyncAdd[2] = song_sync*(SAMPLE_LENGTH*2*0.015625f*0.25f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 256 beats
	SyncAdd[3] = song_sync*(SAMPLE_LENGTH*2*0.015625f*0.5f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 128 beats
	SyncAdd[4] = song_sync*(SAMPLE_LENGTH*2*0.015625f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 64 beats
	SyncAdd[5] = song_sync*(SAMPLE_LENGTH*2*0.03125f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 32 beats
	SyncAdd[6] = song_sync*(SAMPLE_LENGTH*2*0.0625f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 16 beats
	SyncAdd[7] = song_sync*(SAMPLE_LENGTH*2*0.125f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 8 beats
	SyncAdd[8] = song_sync*(SAMPLE_LENGTH*2*0.25f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 4 beats
	SyncAdd[9] = song_sync*(SAMPLE_LENGTH*2*0.5f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 2 beats 
	SyncAdd[10] = song_sync*(SAMPLE_LENGTH*2*1.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // whole note
	SyncAdd[11] = song_sync*(SAMPLE_LENGTH*2*2.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 1/2 note
	SyncAdd[12] = song_sync*(SAMPLE_LENGTH*2*4.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 1/4 note
	SyncAdd[13] = song_sync*(SAMPLE_LENGTH*2*8.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 1/8 note
	SyncAdd[14] = song_sync*(SAMPLE_LENGTH*2*16.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 1/16 note
	SyncAdd[15] = song_sync*(SAMPLE_LENGTH*2*32.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 1/32 note
	SyncAdd[16] = song_sync*(SAMPLE_LENGTH*2*64.0f*(FILTER_CALC_TIME/(song_freq*60.0f)));  // 1/64 note
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
		case e_paraBufThru:
			// feedback
			bufthru = (val-256)/256.0f;
			break;
		case e_paraMix:
			wmix = val/256.0f;
			dmix = 1.0f-wmix;
			break;
		case e_paraInputGain:
			InGain = (val/256.0f)*(val/256.0f);
			break;

		case e_paraSPEEDlfoamplitude:								
			speedlfoamplitude = val/256.0f; 
			break;
		case e_paraSPEEDlfospeed:												
			speedlfospeed = val; 
			break;
		case e_paraSPEEDlfowave:												
			pspeedlfowave=SourceWaveTable[val%MAXLFOWAVE]; 
			speedlfowave=val%MAXLFOWAVE; 
			break;
		case e_paraSPEEDlfophase:
			speedlfophase = val * (SAMPLE_LENGTH*2/65536.0f);
			break;

		case e_paraMIXlfoamplitude:								
			mixlfoamplitude = val/256.0f; 
			break;
		case e_paraMIXlfospeed:												
			mixlfospeed = val; 
			break;
		case e_paraMIXlfowave:												
			pmixlfowave=SourceWaveTable[val%MAXLFOWAVE]; 
			mixlfowave=val%MAXLFOWAVE; 
			break;
		case e_paraMIXlfophase:
			mixlfophase = val * (SAMPLE_LENGTH*2/65536.0f);
			break;

		case e_paraSmoothing:																
			smoothing = float((256-val)*(256-val))/2.0f; 
			break;

	}
}

inline void mi::FilterTick()
{
	timetocompute=FILTER_CALC_TIME-1;
	// lfo
	if (speedlfospeed <= MAXSYNCMODES)
	{
		speedlfophase += SyncAdd[speedlfospeed];
	}
	else
	{
		speedlfophase += ((speedlfospeed-MAXSYNCMODES)*(speedlfospeed-MAXSYNCMODES))*(0.000030517f*44100.0f/song_freq);
	}
	WRAP_AROUND(speedlfophase);
	Vals[e_paraSPEEDlfophase] = rint<int>(speedlfophase/(SAMPLE_LENGTH*2/65536.0f));

	if (speedlfoamplitude)
	{
		float speedmod = (float(pspeedlfowave[rint<int>(speedlfophase)])*speedlfoamplitude);
		rdelta += (((ddelta*runbal)+(ddelta*runbal*speedmod) - rdelta)*rdrag);
		ldelta += (((ddelta*lunbal)+(ddelta*lunbal*speedmod) - ldelta)*ldrag);
	}
	else
	{
		rdelta += (((ddelta*runbal) - rdelta)*rdrag);
		ldelta += (((ddelta*lunbal) - ldelta)*ldrag);
	}

	if (mixlfospeed <= MAXSYNCMODES)
	{
		mixlfophase += SyncAdd[mixlfospeed];
	}
	else
	{
		mixlfophase += ((mixlfospeed-MAXSYNCMODES)*(mixlfospeed-MAXSYNCMODES))*(0.000030517f*44100.0f/song_freq);
	}
	WRAP_AROUND(mixlfophase);
	Vals[e_paraMIXlfophase] = rint<int>(mixlfophase/(SAMPLE_LENGTH*2/65536.0f));
	if (mixlfoamplitude)
	{
		wmix = Vals[e_paraMix]/256.0f+(float(pmixlfowave[rint<int>(mixlfophase)])*mixlfoamplitude);
		dmix = 1.0f-wmix;
	}
	else
	{
		wmix = Vals[e_paraMix]/256.0f;
		dmix = 1.0f-wmix;
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

		if(!timetocompute--) FilterTick();

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
			pBufferL[windex] = (pBufferL[windex]*bufthru)+sl+(sol*feedback)+denormal;
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
			pBufferR[windex] = (pBufferR[windex]*bufthru)+sr+(sor*feedback)+denormal;
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
		if (*psamplesleft > oldl + smoothing)
		{
			*psamplesleft = oldl + smoothing;
		}
		else if (*psamplesleft < oldl - smoothing)
		{
			*psamplesleft = oldl - smoothing;
		}
		oldl= *psamplesleft++;
		*psamplesright=(sr*dmix)+(sor*wmix);
		if (*psamplesright > oldr + smoothing)
		{
			*psamplesright = oldr + smoothing;
		}
		else if (*psamplesright < oldr - smoothing)
		{
			*psamplesright = oldr - smoothing;
		}
		oldr= *psamplesright++;
	} while(--numsamples);
	denormal = -denormal;
}

void mi::InitWaveTable()
{
	int rand = 156;
	for(int c=0;c<(SAMPLE_LENGTH*2)+256;c++)
	{
		int c2 = c & ((SAMPLE_LENGTH*2)-1);
		double sval=(double)(c*0.00306796157577128245943617517898389)/(SAMPLE_LENGTH/1024);

		// sine
		SourceWaveTable[0][c]=float(std::sin(sval));

		// triangle
		if (c2<SAMPLE_LENGTH/2)				
		{
			SourceWaveTable[1][c]=(c2*(1.0f/(SAMPLE_LENGTH/2)));
		}
		else if (c2<(SAMPLE_LENGTH*3)/2) 
		{
			SourceWaveTable[1][c]=1.0f-((c2-(SAMPLE_LENGTH/2))*(1.0f/(SAMPLE_LENGTH/2)));
		}
		else
		{
			SourceWaveTable[1][c]=((c2-((SAMPLE_LENGTH*3)/2))*(1.0f/(SAMPLE_LENGTH/2)))-1.0f;
		}

		// left saw
		SourceWaveTable[2][c]=(1.0f/SAMPLE_LENGTH)*((SAMPLE_LENGTH*2)-c2)-1.0f;

		// right saw
		SourceWaveTable[3][c]=(1.0f/SAMPLE_LENGTH)*(c2)-1.0f;

		// square
		if (c2 < SAMPLE_LENGTH)
		{
			SourceWaveTable[4][c]=1.0f;
		}
		else
		{
			SourceWaveTable[4][c]=-1.0f;
		}

		// harmonic distortion sin 1
		SourceWaveTable[5][c] = float((std::sin(sval)*0.75f) + (std::sin(sval*2.0)*0.5f) + (std::sin(sval*4.0)*0.25f) + (std::sin(sval*16.0)*0.125f));
		// bounce off limits
		if (SourceWaveTable[5][c] > 1.0f)
		{
			SourceWaveTable[5][c] -= 2*(SourceWaveTable[5][c]-1.0f);
		}
		else if (SourceWaveTable[7][c] < -1.0f)
		{
			SourceWaveTable[5][c] -= 2*(SourceWaveTable[5][c]+1.0f);
		}

		// inv sin
		if (c2<SAMPLE_LENGTH/2)				
		{
			// phase 1
			SourceWaveTable[6][c] = float((1.0-std::sin(sval+PI/2)));
		}
		else if (c2<SAMPLE_LENGTH)				
		{
			// phase 2
			SourceWaveTable[6][c] = float((1.0-std::sin(sval-PI/2)));
		}
		else if (c2<SAMPLE_LENGTH*3/2)				
		{
			// phase 3
			SourceWaveTable[6][c] = float((-1.0-std::sin(sval+PI/2)));
		}
		else 
		{
			// phase 4
			SourceWaveTable[6][c] = float((-1.0-std::sin(sval-PI/2)));
		}


		// inv tri
		SourceWaveTable[7][c] = ((SourceWaveTable[4][c]*0.5f)+(SourceWaveTable[0][c])-(SourceWaveTable[1][c]*0.5f)) ;
		// bounce off limits
		if (SourceWaveTable[7][c] > 1.0f)
		{
			SourceWaveTable[7][c] -= 2*(SourceWaveTable[7][c]-1.0f);
		}
		else if (SourceWaveTable[7][c] < -1.0f)
		{
			SourceWaveTable[7][c] -= 2*(SourceWaveTable[7][c]+1.0f);
		}

		// L soft saw
		SourceWaveTable[8][c] = float(std::sin((PI/4)+(double)(c2*0.00306796157577128245943617517898389)/(SAMPLE_LENGTH/768)));

		// R soft saw
		SourceWaveTable[9][c] = float(std::sin((PI*5/4)+(double)(c2*0.00306796157577128245943617517898389)/(SAMPLE_LENGTH/768)));


		// soft square
		if (c2<SAMPLE_LENGTH/2)				
		{
			SourceWaveTable[10][c] = ((SourceWaveTable[0][c]*0.5f) + (SourceWaveTable[4][c]*0.5f));// - fabs(sin(sval*32.0f)*0.2f*SourceWaveTable[5][c]));
		}
		else
		{
			SourceWaveTable[10][c] = ((SourceWaveTable[0][c]*0.5f) + (SourceWaveTable[4][c]*0.5f));// + fabs(sin(sval*32.0f)*0.2f*SourceWaveTable[5][c]));
		}

		// super mw
		if (c2<SAMPLE_LENGTH/2)				
		{
			// phase 1
			SourceWaveTable[11][c] = float((std::sin(sval+PI/2)));
		}
		else if (c2<SAMPLE_LENGTH)				
		{
			// phase 2
			SourceWaveTable[11][c] = float((std::sin(sval-PI/2)));
		}
		else if (c2<SAMPLE_LENGTH*3/2)				
		{
			// phase 3
			SourceWaveTable[11][c] = float((std::sin(sval+PI/2)));
		}
		else 
		{
			// phase 4
			SourceWaveTable[11][c] = float((std::sin(sval-PI/2)));
		}

		// racer
		if (c2<SAMPLE_LENGTH/2)				
		{
			// phase 1
			SourceWaveTable[12][c] = float((std::sin(sval+PI/2)));
		}
		else if (c2<SAMPLE_LENGTH)				
		{
			// phase 2
			SourceWaveTable[12][c] = float((std::sin(sval*2)));
		}
		else if (c2<SAMPLE_LENGTH*3/2)				
		{
			// phase 3
			SourceWaveTable[12][c] = float((std::sin(sval+PI/2)));
		}
		else 
		{
			// phase 4
			SourceWaveTable[12][c] = float((std::sin(sval*2+PI)));
		}

		// white noise
		rand = (rand*171)+145;
		SourceWaveTable[13][c]=((rand&0xffff)-0x8000)/32768.0f;

		// brown noise
		if (c > 0)
		{
			SourceWaveTable[14][c] = SourceWaveTable[14][c-1] + (SourceWaveTable[13][c]*0.5f);  
			// bounce off limits
			if (SourceWaveTable[14][c] > 1.0f)
			{
				SourceWaveTable[14][c] -= 2*(SourceWaveTable[14][c]-1.0f);
			}
			else if (SourceWaveTable[14][c] < -1.0f)
			{
				SourceWaveTable[14][c] -= 2*(SourceWaveTable[14][c]+1.0f);
			}
		}
		else
		{
			SourceWaveTable[14][0] = 0;
		}
		// none
		SourceWaveTable[15][c] = 0;
	}

	/*
		case 0:sprintf(txt,"Sine +");return true;break;
		case 1:sprintf(txt,"Sine -");return true;break;
		case 2:sprintf(txt,"Triangle +");return true;break;
		case 3:sprintf(txt,"Triangle -");return true;break;
		case 4:sprintf(txt,"L Saw +");return true;break;
		case 5:sprintf(txt,"L Saw -");return true;break;
		case 6:sprintf(txt,"R Saw +");return true;break;
		case 7:sprintf(txt,"R Saw -");return true;break;
		case 8:sprintf(txt,"Square +");return true;break;
		case 9:sprintf(txt,"Square -");return true;break;
		case 10:sprintf(txt,"Harmonic Sine +");return true;break;
		case 11:sprintf(txt,"Harmonic Sine -");return true;break;
		case 12:sprintf(txt,"Inverted Sine +");return true;break;
		case 13:sprintf(txt,"Inverted Sine -");return true;break;
		case 14:sprintf(txt,"Inv Triangle +");return true;break;
		case 15:sprintf(txt,"Inv Triangle -");return true;break;
		case 16:sprintf(txt,"Soft L Saw +");return true;break;
		case 17:sprintf(txt,"Soft L Saw -");return true;break;
		case 18:sprintf(txt,"Soft R Saw +");return true;break;
		case 19:sprintf(txt,"Soft R Saw -");return true;break;
		case 20:sprintf(txt,"Soft Square +");return true;break;
		case 21:sprintf(txt,"Soft Square -");return true;break;
		case 22:sprintf(txt,"White Noise 1");return true;break;
		case 23:sprintf(txt,"White Noise 2");return true;break;
		case 24:sprintf(txt,"Brown Noise 1");return true;break;
		case 25:sprintf(txt,"Brown Noise 2");return true;break;
		case 26:sprintf(txt,"Silence");return true;break;
	*/
	WaveTable[0] = &SourceWaveTable[0][0];
	WaveTable[1] = &SourceWaveTable[1][0];
	WaveTable[2] = &SourceWaveTable[2][0];
	WaveTable[3] = &SourceWaveTable[3][0];
	WaveTable[4] = &SourceWaveTable[4][0];
	WaveTable[5] = &SourceWaveTable[5][0];
	WaveTable[6] = &SourceWaveTable[6][0];
	WaveTable[7] = &SourceWaveTable[7][0];
	WaveTable[8] = &SourceWaveTable[8][0];
	WaveTable[9] = &SourceWaveTable[9][0];
	WaveTable[10] = &SourceWaveTable[10][0];
	WaveTable[11] = &SourceWaveTable[11][0];
	WaveTable[12] = &SourceWaveTable[12][0];
	WaveTable[13] = &SourceWaveTable[13][0];
	WaveTable[14] = &SourceWaveTable[13][0];
	WaveTable[15] = &SourceWaveTable[14][0];
	WaveTable[16] = &SourceWaveTable[14][0];
	WaveTable[17] = &SourceWaveTable[15][0];
}

// Function that describes value on client's displaying
bool mi::DescribeValue(char* txt,int const param, int const value)
{
	float fv;
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
	case e_paraBufThru:
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
	case e_paraSmoothing:
		sprintf(txt,"%.2f %%",100*(value)/256.0f);
		return true;
	case e_paraSPEEDlfospeed:
	case e_paraMIXlfospeed:
		switch (value)
		{
			case 0: sprintf(txt,"Paused"); return true; break;
			case 1: sprintf(txt,"Sync 512 beats"); return true; break;
			case 2: sprintf(txt,"Sync 256 beats"); return true; break;
			case 3: sprintf(txt,"Sync 128 beats"); return true; break;
			case 4: sprintf(txt,"Sync 64 beats"); return true; break;
			case 5: sprintf(txt,"Sync 32 beats"); return true; break;
			case 6: sprintf(txt,"Sync 16 beats"); return true; break;
			case 7: sprintf(txt,"Sync 8 beats"); return true; break;
			case 8: sprintf(txt,"Sync 4 beats"); return true; break;
			case 9: sprintf(txt,"Sync 2 beats"); return true; break;
			case 10: sprintf(txt,"Sync whole note"); return true; break;
			case 11: sprintf(txt,"Sync 1/2 note"); return true; break;
			case 12: sprintf(txt,"Sync 1/4 note"); return true; break;
			case 13: sprintf(txt,"Sync 1/8 note"); return true; break;
			case 14: sprintf(txt,"Sync 1/16 note"); return true; break;
			case 15: sprintf(txt,"Sync 1/32 note"); return true; break;
			case 16: sprintf(txt,"Sync 1/64 note"); return true; break;
		}
		// filter lfo rates
		fv=44100.f*((value-MAXSYNCMODES)*(value-MAXSYNCMODES)*0.000030517f)/(SAMPLE_LENGTH*2*FILTER_CALC_TIME);
		sprintf(txt,"%.4f hz",fv);
		return true;
		break;
	case e_paraSPEEDlfowave:
	case e_paraMIXlfowave:
		// lfo waveform
		switch(value)
		{
			case 0:sprintf(txt,"Sine");return true;break;
			case 1:sprintf(txt,"Triangle");return true;break;
			case 2:sprintf(txt,"L Saw");return true;break;
			case 3:sprintf(txt,"R Saw");return true;break;
			case 4:sprintf(txt,"Square");return true;break;
			case 5:sprintf(txt,"Harmonic Sine");return true;break;
			case 6:sprintf(txt,"Inverted Sine");return true;break;
			case 7:sprintf(txt,"Inv Triangle");return true;break;
			case 8:sprintf(txt,"Soft L Saw");return true;break;
			case 9:sprintf(txt,"Soft R Saw");return true;break;
			case 10:sprintf(txt,"Soft Square");return true;break;
			case 11:sprintf(txt,"Super MW");return true;break;
			case 12:sprintf(txt,"Racer");return true;break;
			case 13:sprintf(txt,"White Noise");return true;break;
			case 14:sprintf(txt,"Brown Noise");return true;break;
			case 15:sprintf(txt,"Silence");return true;break;
		}
		break;
	case e_paraSPEEDlfophase:
	case e_paraMIXlfophase:
		sprintf(txt,"%.2f degrees",value * (360/65536.0f));
		return true;
		break;

	case e_paraSPEEDlfoamplitude:
	case e_paraMIXlfoamplitude:
		if ( value == 0 )
		{
			sprintf(txt,"Off");
			return true;
		}
		sprintf(txt,"%.2f %%",100*(value)/256.0f);
		return true;

	}
	return false;
}

}