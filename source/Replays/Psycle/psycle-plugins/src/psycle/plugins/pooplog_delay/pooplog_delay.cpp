//////////////////////////////////////////////////////////////////////
//Pooplog Delay
/*
/////////////////////////////////////////////////////////////////////
Version History

v0.04b
5/19/2003
- more anti-denormal code

v0.03b
- made it so track tempo defaults to on

v0.02b
- initial release version

v0.01b
- initial test beta

/////////////////////////////////////////////////////////////////////
	*/
#include <psycle/plugin_interface.hpp>
#include <psycle/helpers/math.hpp>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cstdio>

#include "filter.h"
#ifdef LIGHT
namespace psycle::plugins::pooplog_delay_light {
#else
namespace psycle::plugins::pooplog_delay {
#endif
    using namespace psycle::plugin_interface;
using namespace psycle::helpers::math;

#ifndef LIGHT
#define MAX_BUF 1024*1024*4
#define PLUGIN_NAME "Pooplog Delay 0.04b"
#else
#define MAX_BUF 1024*1024*2
#define PLUGIN_NAME "Pooplog Delay Light 0.04b"
#endif


#define SYNTH_REMAP_0 24
#define FILEVERSION 2
#define MAXSYNCMODES 16
#define OVERDRIVEDIVISOR 512.0f
#define FILTER_CALC_TIME				32
#define MAXLFOWAVE 15
#define MAX_VCF_CUTOFF 301
#define MAX_VCF_CUTOFF_22050 263
#define TWOPI																6.28318530717958647692528676655901f
#define SAMPLE_LENGTH  4096
#define MAXPHASEMIX 9
#define MAXOVERDRIVEMETHOD SYNTH_REMAP_0+7
#define MAXENVTYPE 2
#define MAXVCFTYPE 42
#define MAXMOOG 6
#define MAXFILTER 26
#define MAX_RATE								4096
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


CMachineParameter const paraLengthL = {"Left Delay Length", "Left Delay Length", 0, NUM_BUFF-1, MPF_STATE, 21};
CMachineParameter const paraDelayPanL = {"Left Delay Pan", "Left Delay Pan", 0, 512, MPF_STATE, 0};
CMachineParameter const paraFeedbackL = {"Left Feedback", "Left Feedback", 0, 256, MPF_STATE, 64};
CMachineParameter const paraFeedbackPanL = {"Left Feedback Pan", "Left Feedback Pan", 0, 512, MPF_STATE, 0};
CMachineParameter const paraLengthR = {"Right Delay Length", "Right Delay Length", 0, NUM_BUFF-1, MPF_STATE, 21};
CMachineParameter const paraDelayPanR = {"Right Delay Pan", "Right Delay Pan", 0, 512, MPF_STATE, 512};
CMachineParameter const paraFeedbackR = {"Right Feedback", "Right Feedback", 0, 256, MPF_STATE, 64};
CMachineParameter const paraFeedbackPanR = {"Right Feedback Pan", "Right Feedback Pan", 0, 512, MPF_STATE, 512};
CMachineParameter const paraNULL = {" ", " ", 0, 0, MPF_NULL, 0};
CMachineParameter const paraDLVCFcutoff = {"Delay Cutoff", "Delay Cutoff", 0, MAX_VCF_CUTOFF, MPF_STATE, MAX_VCF_CUTOFF/2};
CMachineParameter const paraDLVCFresonance = {"Delay Resonance", "Delay Resonance", 1, 240, MPF_STATE, 1};
CMachineParameter const paraDLVCFtype = {"Delay Filter Type", "Delay Filter Type", 0, MAXVCFTYPE, MPF_STATE, 0};
CMachineParameter const paraDLVCFlfospeed = {"Delay LFO Rate", "Delay LFO Rate", 0, MAX_RATE, MPF_STATE, 0};
CMachineParameter const paraDLVCFlfoamplitude = {"Cutoff LFO Depth", "Cutoff LFO Depth", 0, MAX_VCF_CUTOFF, MPF_STATE, 0};
CMachineParameter const paraDLUnbalancelfoamplitude = {"Unbalance LFO Depth", "Unbalance LFO Depth", 0, 256, MPF_STATE, 0};
CMachineParameter const paraDLGainlfoamplitude = {"Gain LFO Depth", "Gain LFO Depth", 0, 256, MPF_STATE, 0};
CMachineParameter const paraDLVCFlfowave = {"Delay LFO Wave", "Delay LFO Wave", 0, MAXLFOWAVE-1, MPF_STATE, 0};
CMachineParameter const paraDLVCFlfophase = {"Delay LFO Phase", "Delay LFO Phase", 0, 0xffff, MPF_STATE, 0};
CMachineParameter const paraDLOUToverdrive = {"Delay Shaper", "Delay Shaper", 0, MAXOVERDRIVEMETHOD, MPF_STATE, 0};
CMachineParameter const paraDLOUToverdrivegain = {"Shaper Gain/Param", "Shaper Gain/Param", 0, 256, MPF_STATE, 0};
CMachineParameter const paraDLUnbalance = {"Cutoff Unbalance", "Cutoff Unbalance", 0, 512, MPF_STATE, 256};
CMachineParameter const paraDLRoute = {"Delay Routing", "Delay Routing", 0, 1, MPF_STATE, 0};


#ifndef LIGHT
CMachineParameter const paraFBVCFcutoff = {"Feedback Cutoff", "Feedback Cutoff", 0, MAX_VCF_CUTOFF, MPF_STATE, MAX_VCF_CUTOFF/2};
CMachineParameter const paraFBVCFresonance = {"Feedback Resonance", "Feedback Resonance", 1, 240, MPF_STATE, 1};
CMachineParameter const paraFBVCFtype = {"Feedback Filter Type", "Feedback Filter Type", 0, MAXVCFTYPE, MPF_STATE, 0};
CMachineParameter const paraFBVCFlfospeed = {"Feedback LFO Rate", "Feedback LFO Rate", 0, MAX_RATE, MPF_STATE, 0};
CMachineParameter const paraFBVCFlfoamplitude = {"Cutoff LFO Depth", "Cutoff LFO Depth", 0, MAX_VCF_CUTOFF, MPF_STATE, 0};
CMachineParameter const paraFBUnbalancelfoamplitude = {"Unbalance LFO Depth", "Unbalance LFO Depth", 0, 256, MPF_STATE, 0};
CMachineParameter const paraFBGainlfoamplitude = {"Gain LFO Depth", "Gain LFO Depth", 0, 256, MPF_STATE, 0};
CMachineParameter const paraFBVCFlfowave = {"Feedback LFO Wave", "Feedback LFO Wave", 0, MAXLFOWAVE-1, MPF_STATE, 0};
CMachineParameter const paraFBVCFlfophase = {"Feedback LFO Phase", "Feedback LFO Phase", 0, 0xffff, MPF_STATE, 0};
CMachineParameter const paraFBOUToverdrive = {"Feedback Shaper", "Feedback Shaper", 0, MAXOVERDRIVEMETHOD, MPF_STATE, 0};
CMachineParameter const paraFBRoute = {"Feedback Routing", "Feedback Routing", 0, 1, MPF_STATE, 0};
CMachineParameter const paraFBOUToverdrivegain = {"Shaper Gain/Param", "Shaper Gain/Param", 0, 256, MPF_STATE, 0};
CMachineParameter const paraFBUnbalance = {"Cutoff Unbalance", "Cutoff Unbalance", 0, 512, MPF_STATE, 256};
CMachineParameter const paraOUToverdrivegain = {"Final Gain/Param", "Final Gain/Param", 0, 256, MPF_STATE, 0};
CMachineParameter const paraOUToverdrive = {"Final Shaper", "Final Shaper", 0, MAXOVERDRIVEMETHOD, MPF_STATE, 0};
#endif

CMachineParameter const paraInputGain = {"Input Gain", "Input Gain", 0, 1024, MPF_STATE, 256};
CMachineParameter const paraOUTmix = {"Mix", "Mix", 0, 256, MPF_STATE, 128};
CMachineParameter const paraInertia = {"Tweak Inertia", "Tweak Inertia", 0, 1024, MPF_STATE, 0};
CMachineParameter const paraTRACKTempo = {"Follow Tempo", "Follow Tempo", 0, 1, MPF_STATE, 1};


#ifndef LIGHT
enum {
	e_paraLengthL, 
	e_paraLengthR, 
	e_paraDelayPanL, 
	e_paraDelayPanR, 
	e_paraFeedbackL, 
	e_paraFeedbackR, 
	e_paraFeedbackPanL, 
	e_paraFeedbackPanR,
	e_paraNULL00,
	e_paraDLVCFtype,
	e_paraDLVCFcutoff,
	e_paraDLVCFresonance,
	e_paraDLUnbalance,
	e_paraDLVCFlfospeed,
	e_paraDLVCFlfoamplitude,
	e_paraDLUnbalancelfoamplitude,
	e_paraDLGainlfoamplitude,
	e_paraDLVCFlfowave,
	e_paraDLVCFlfophase,
	e_paraDLOUToverdrive,
	e_paraDLOUToverdrivegain,
	e_paraDLRoute,
	e_paraNULL01,
	e_paraFBVCFtype,
	e_paraFBVCFcutoff,
	e_paraFBVCFresonance,
	e_paraFBUnbalance,
	e_paraFBVCFlfospeed,
	e_paraFBVCFlfoamplitude,
	e_paraFBUnbalancelfoamplitude,
	e_paraFBGainlfoamplitude,
	e_paraFBVCFlfowave,
	e_paraFBVCFlfophase,
	e_paraFBOUToverdrive,
	e_paraFBOUToverdrivegain,
	e_paraFBRoute,
	e_paraOUToverdrive,
	e_paraOUToverdrivegain,
	e_paraInputGain,
	e_paraOUTmix,
	e_paraInertia,
	e_paraNULL02,
	e_paraTRACKTempo
};

CMachineParameter const *pParameters[] = 
{ 
	// global
	&paraLengthL, 
	&paraLengthR, 
	&paraDelayPanL, 
	&paraDelayPanR, 
	&paraFeedbackL, 
	&paraFeedbackR, 
	&paraFeedbackPanL, 
	&paraFeedbackPanR,
	&paraNULL,
	&paraDLVCFtype,
	&paraDLVCFcutoff,
	&paraDLVCFresonance,
	&paraDLUnbalance,
	&paraDLVCFlfospeed,
	&paraDLVCFlfoamplitude,
	&paraDLUnbalancelfoamplitude,
	&paraDLGainlfoamplitude,
	&paraDLVCFlfowave,
	&paraDLVCFlfophase,
	&paraDLOUToverdrive,
	&paraDLOUToverdrivegain,
	&paraDLRoute,
	&paraNULL,
	&paraFBVCFtype,
	&paraFBVCFcutoff,
	&paraFBVCFresonance,
	&paraFBUnbalance,
	&paraFBVCFlfospeed,
	&paraFBVCFlfoamplitude,
	&paraFBUnbalancelfoamplitude,
	&paraFBGainlfoamplitude,
	&paraFBVCFlfowave,
	&paraFBVCFlfophase,
	&paraFBOUToverdrive,
	&paraFBOUToverdrivegain,
	&paraFBRoute,
	&paraOUToverdrive,
	&paraOUToverdrivegain,
	&paraInputGain,
	&paraOUTmix,
	&paraInertia,
	&paraNULL,
	&paraTRACKTempo
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0004,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	PLUGIN_NAME,
	"Pooplog Delay",
	"Jeremy Evers",
	"About",
	5
);

#else

enum {
	e_paraLengthL, 
	e_paraLengthR, 
	e_paraDelayPanL, 
	e_paraDelayPanR, 
	e_paraFeedbackL, 
	e_paraFeedbackR, 
	e_paraNULL00a,
	e_paraFeedbackPanL, 
	e_paraFeedbackPanR,
	e_paraNULL00,
	e_paraDLVCFtype,
	e_paraDLVCFcutoff,
	e_paraDLVCFresonance,
	e_paraNULL0b,
	e_paraDLUnbalance,
	e_paraDLVCFlfospeed,
	e_paraDLVCFlfoamplitude,
	e_paraDLUnbalancelfoamplitude,
	e_paraDLGainlfoamplitude,
	e_paraDLVCFlfowave,
	e_paraDLVCFlfophase,
	e_paraDLOUToverdrive,
	e_paraDLOUToverdrivegain,
	e_paraDLRoute,
	e_paraTRACKTempo,
	e_paraInputGain,
	e_paraOUTmix,
	e_paraInertia
};

CMachineParameter const *pParameters[] = 
{ 
	// global
	&paraLengthL, 
	&paraLengthR, 
	&paraDelayPanL, 
	&paraDelayPanR, 
	&paraFeedbackL, 
	&paraFeedbackR, 
	&paraNULL,
	&paraFeedbackPanL, 
	&paraFeedbackPanR,
	&paraNULL,
	&paraDLVCFtype,
	&paraDLVCFcutoff,
	&paraDLVCFresonance,
	&paraNULL,
	&paraDLUnbalance,
	&paraDLVCFlfospeed,
	&paraDLVCFlfoamplitude,
	&paraDLUnbalancelfoamplitude,
	&paraDLGainlfoamplitude,
	&paraDLVCFlfowave,
	&paraDLVCFlfophase,
	&paraDLOUToverdrive,
	&paraDLOUToverdrivegain,
	&paraDLRoute,
	&paraTRACKTempo,
	&paraInputGain,
	&paraOUTmix,
	&paraInertia,
};

CMachineInfo const MacInfo (
	MI_VERSION,				
	0x0004,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,																																							// numParameters
	pParameters,																												// Pointer to parameters
	PLUGIN_NAME,
	"Pooplog Delay L",																												// short name
	"Jeremy Evers",																												// author
	"About",																																// A command, that could be use for open an editor, etc...
	4
);

#endif

struct INERTIA
{
	int bCutoff;
	float current;
	float add;
	int * source;
	int dest;
	struct INERTIA* next;
	struct INERTIA* prev;
};

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
	void NewInertia(int * source, int dest);
	void NewInertiaDLVC(int * source, int dest);
#ifndef LIGHT
	void NewInertiaFBVC(int * source, int dest);
#endif
	void DeleteInertia(INERTIA* pI);
	void UpdateInertia();

	void InitWaveTable();

	int inertia;
	struct INERTIA* pInertia;
	struct INERTIA* pLastInertia;

private:
	float * WaveTable[MAXWAVE+1];
	filter l_dl_filter;
	filter r_dl_filter;
#ifndef LIGHT
	filter l_fb_filter;
	filter r_fb_filter;
	inline float DoFBFilterL(float input);
	inline float DoFBFilterR(float input);
	inline float HandleFBOverdrive(float input);
	inline float HandleFNOverdrive(float input);

	float FBOutGain;
	int FBoverdrive;
	int FBoverdrivegain;
	int FBtrackDir;
	int FBplayDir;
//				float FBtrackThresh;
	float FBnextMin;
	int FBnextMinCount;
	float FBthisMin;
	int FBthisMinCount;
	float FBnextMax;
	int FBnextMaxCount;
	float FBthisMax;
	int FBthisMaxCount;
	float FBw1;
	float FBw2;
	float FBplayPos;

	float *pFBvcflfowave;
	int				FBvcflfowave;
	int FBvcflfospeed;
	int FBvcflfoamplitude;
	int FBvcfcutoff;
	int FBvcfresonance;
	int FBvcftype;
	int FBrunbal;
	int FBlunbal;
	int FBroute;
	int FBgainlfoamplitude;
	int FBunbalancelfoamplitude;
	float FBVcfCutoff;
	float FBvcflfophase;

	float FNOutGain;
	int FNoverdrive;
	int FNoverdrivegain;
	int FNtrackDir;
	int FNplayDir;
//				float FNtrackThresh;
	float FNnextMin;
	int FNnextMinCount;
	float FNthisMin;
	int FNthisMinCount;
	float FNnextMax;
	int FNnextMaxCount;
	float FNthisMax;
	int FNthisMaxCount;
	float FNw1;
	float FNw2;
	float FNplayPos;
#endif
	float wmix;
	float dmix;
	float InGain;
	inline void FilterTick();
	int timetocompute;
	inline float DoDLFilterL(float input);
	inline float DoDLFilterR(float input);
	inline float HandleDLOverdrive(float input);
	int out_mix;
	int song_sync; 
	int song_freq;
	float max_cutoff;

	float DLOutGain;
	int DLoverdrive;
	int DLoverdrivegain;
	int DLtrackDir;
	int DLplayDir;
//				float DLtrackThresh;
	float DLnextMin;
	int DLnextMinCount;
	float DLthisMin;
	int DLthisMinCount;
	float DLnextMax;
	int DLnextMaxCount;
	float DLthisMax;
	int DLthisMaxCount;
	float DLw1;
	float DLw2;
	float DLplayPos;


	float *pDLvcflfowave;
	int				DLvcflfowave;
	int DLvcflfospeed;
	int DLvcflfoamplitude;
	int DLvcfcutoff;
	int DLvcfresonance;
	int DLvcftype;
	int DLrunbal;
	int DLlunbal;
	int DLroute;
	int DLgainlfoamplitude;
	int DLunbalancelfoamplitude;
	float DLVcfCutoff;
	float DLvcflfophase;

	float *pBufferL;
	float *pBufferR;
	int Lbufsize;
	int Rbufsize;
	int Lindex;
	int Rindex;

	int LDLpan;
	int RDLpan;
	int LFB;
	int RFB;
	int LFBpan;
	int RFBpan;

	bool trackBPM;
};

#ifdef LIGHT
PSYCLE__PLUGIN__INSTANTIATOR("pooplog-delay-light", mi, MacInfo)
#else
PSYCLE__PLUGIN__INSTANTIATOR("pooplog-delay", mi, MacInfo)
#endif
//DLL_EXPORTS

mi::mi()
{
	// The constructor zone
	Vals = new int[MacInfo.numParameters];
	InitWaveTable();
	pDLvcflfowave=SourceWaveTable[0];
#ifndef LIGHT
	pFBvcflfowave=SourceWaveTable[0];
#endif
	pLastInertia = pInertia = NULL;
	pBufferL = NULL;
	pBufferR = NULL;
}

mi::~mi()
{
	delete[] Vals;
// Destroy dynamically allocated objects/memory here
	INERTIA* pI = pInertia;
	while (pI)
	{
		INERTIA* pI2 = pI->next;
		delete pI;
		pI = pI2;
	}
	pLastInertia = pInertia = NULL;
	if (pBufferL)
	{
		delete[] pBufferL;
	}
	if (pBufferR)
	{
		delete[] pBufferR;
	}
}


void mi::Init()
{
// Initialize your stuff here
	song_sync = pCB->GetBPM();
	song_freq = pCB->GetSamplingRate();
	if (song_freq < 44100)
	{
		max_cutoff = MAX_VCF_CUTOFF_22050;
	}
	else
	{
		max_cutoff = MAX_VCF_CUTOFF;
	}
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
#ifndef LIGHT
	FBvcftype = 0;
	l_fb_filter.init(song_freq);
	r_fb_filter.init(song_freq);
	FBvcflfoamplitude=0;
	FBgainlfoamplitude=0;
	FBunbalancelfoamplitude=0;
	FBvcflfophase=0;
	FBrunbal = 1;
	FBlunbal = 1;
	FBroute = 0;

	FBOutGain = 1;
	FBtrackDir = 1;
	FBplayDir = 1;
	FBnextMin = 0;
	FBthisMin = 0;
	FBnextMax = 0;
	FBthisMax = 0;
	FBnextMinCount = 0;
	FBthisMinCount = 1;
	FBnextMaxCount = 0;
	FBthisMaxCount = 1;
	FBw1 = 1;
	FBw2 = 1;
	FBplayPos = 0;
//				FBtrackThresh = 0;

	FNOutGain = 1;
	FNtrackDir = 1;
	FNplayDir = 1;
	FNnextMin = 0;
	FNthisMin = 0;
	FNnextMax = 0;
	FNthisMax = 0;
	FNnextMinCount = 0;
	FNthisMinCount = 1;
	FNnextMaxCount = 0;
	FNthisMaxCount = 1;
	FNw1 = 1;
	FNw2 = 1;
	FNplayPos = 0;
//				FNtrackThresh = 0;

#endif
	DLvcftype = 0;
	l_dl_filter.init(song_freq);
	r_dl_filter.init(song_freq);
	inertia = paraInertia.DefValue;
	pLastInertia = pInertia = NULL;
	timetocompute = 1;

	DLvcflfoamplitude=0;
	DLgainlfoamplitude=0;
	DLunbalancelfoamplitude=0;
	DLvcflfophase=0;
	DLrunbal = 1;
	DLlunbal = 1;
	DLroute = 0;

	DLOutGain = 1;
	DLtrackDir = 1;
	DLplayDir = 1;
	DLnextMin = 0;
	DLthisMin = 0;
	DLnextMax = 0;
	DLthisMax = 0;
	DLnextMinCount = 0;
	DLthisMinCount = 1;
	DLnextMaxCount = 0;
	DLthisMaxCount = 1;
	DLw1 = 1;
	DLw2 = 1;
	DLplayPos = 0;
//				DLtrackThresh = 0;

	Lbufsize = int(((buffindex[paraLengthL.DefValue]*song_freq*60.0f)/song_sync)+0.5f);
	Rbufsize = int(((buffindex[paraLengthR.DefValue]*song_freq*60.0f)/song_sync)+0.5f);
	Lindex = 0;
	Rindex = 0;

	LDLpan=0;
	RDLpan=512;
	LFB=64;
	RFB=64;
	LFBpan=0;
	RFBpan=512;

	trackBPM=false;
}

inline void mi::FilterTick()
{
	timetocompute=FILTER_CALC_TIME-1;
	// lfos
	if (DLvcflfospeed <= MAXSYNCMODES)
	{
		DLvcflfophase += SyncAdd[DLvcflfospeed];
	}
	else
	{
		DLvcflfophase += ((DLvcflfospeed-MAXSYNCMODES)*(DLvcflfospeed-MAXSYNCMODES))*(0.000030517f*44100.0f/song_freq);
	}
	WRAP_AROUND(DLvcflfophase);
	Vals[e_paraDLVCFlfophase] = rint<int>(DLvcflfophase/(SAMPLE_LENGTH*2/65536.0f));
	// vcf
	if (DLvcftype)
	{
		float outcutoff = DLVcfCutoff;
		int flunbal = DLlunbal;
		int frunbal = DLrunbal;

		if (DLunbalancelfoamplitude)
		{
			int val = rint<int>((pDLvcflfowave[rint<int>(DLvcflfophase)])*(DLunbalancelfoamplitude))+256;
			// unbalance
			if (val <= 256)
			{
				int t;
				// left
				t = 512-(256-val);
				t *= t;
				frunbal = (t+DLrunbal)/2;
				flunbal = (DLlunbal+(512*512))/2;
			}
			else
			{
				// right
				int t;
				t = 512-(val-256);
				t *= t;
				flunbal = (t+DLlunbal)/2;
				frunbal = (DLrunbal+(512*512))/2;
			}
		}
		if (DLvcflfoamplitude)
		{
			outcutoff+= (float(pDLvcflfowave[rint<int>(DLvcflfophase)])
							*float(DLvcflfoamplitude));
		}
		if (outcutoff<0) outcutoff = 0;
		else if (outcutoff>max_cutoff) outcutoff=max_cutoff;

		if (DLvcftype > MAXFILTER)
		{
			l_dl_filter.setphaser(DLvcftype-MAXFILTER,outcutoff*(flunbal/(512.0f*512.0f)),DLvcfresonance);
			r_dl_filter.setphaser(DLvcftype-MAXFILTER,outcutoff*(frunbal/(512.0f*512.0f)),DLvcfresonance);
		}
		else if (DLvcftype > MAXMOOG)
		{
			l_dl_filter.setfilter((DLvcftype-MAXMOOG-1)/2,outcutoff*(flunbal/(512.0f*512.0f)),DLvcfresonance);
			r_dl_filter.setfilter((DLvcftype-MAXMOOG-1)/2,outcutoff*(frunbal/(512.0f*512.0f)),DLvcfresonance);
		}
		else if (DLvcftype & 1)
		{
			l_dl_filter.setmoog1((DLvcftype-1)/2,outcutoff*(flunbal/(512.0f*512.0f)),DLvcfresonance);
			r_dl_filter.setmoog1((DLvcftype-1)/2,outcutoff*(frunbal/(512.0f*512.0f)),DLvcfresonance);
		}
		else
		{
			l_dl_filter.setmoog2((DLvcftype-1)/2,outcutoff*(flunbal/(512.0f*512.0f)),DLvcfresonance);
			r_dl_filter.setmoog2((DLvcftype-1)/2,outcutoff*(frunbal/(512.0f*512.0f)),DLvcfresonance);
		}
	}
	// overdrive
	if (DLoverdrive)
	{
		if (DLgainlfoamplitude)
		{
			float p = DLoverdrivegain+(float(pDLvcflfowave[rint<int>(DLvcflfophase)])
							*float(DLgainlfoamplitude));
			if (p < 0) 
			{
				p = 0;
			}
			else if (p > 256)
			{
				p = 256;
			}
			DLOutGain = 1.0f+(p*p/OVERDRIVEDIVISOR);
		}
		else
		{
			DLOutGain = 1.0f+(DLoverdrivegain*DLoverdrivegain/OVERDRIVEDIVISOR);
		}
		// gain adsr
		if (DLOutGain < 1.0f) DLOutGain = 1.0f;
		else if (DLOutGain > 65536/OVERDRIVEDIVISOR) DLOutGain = 65536/OVERDRIVEDIVISOR;
	}
#ifndef LIGHT
	// lfos
	if (FBvcflfospeed <= MAXSYNCMODES)
	{
		FBvcflfophase += SyncAdd[FBvcflfospeed];
	}
	else
	{
		FBvcflfophase += ((FBvcflfospeed-MAXSYNCMODES)*(FBvcflfospeed-MAXSYNCMODES))*(0.000030517f*44100.0f/song_freq);
	}
	WRAP_AROUND(FBvcflfophase);
	Vals[e_paraFBVCFlfophase] = rint<int>(FBvcflfophase/(SAMPLE_LENGTH*2/65536.0f));
	// vcf
	if (FBvcftype)
	{
		float outcutoff = FBVcfCutoff;
		int flunbal = FBlunbal;
		int frunbal = FBrunbal;

		if (FBunbalancelfoamplitude)
		{
			int val = rint<int>((pFBvcflfowave[rint<int>(FBvcflfophase)])*(FBunbalancelfoamplitude))+256;
			// unbalance
			if (val <= 256)
			{
				int t;
				// left
				t = 512-(256-val);
				t *= t;
				frunbal = (t+FBrunbal)/2;
				flunbal = (FBlunbal+(512*512))/2;
			}
			else
			{
				// right
				int t;
				t = 512-(val-256);
				t *= t;
				flunbal = (t+FBlunbal)/2;
				frunbal = (FBrunbal+(512*512))/2;
			}
		}
		if (FBvcflfoamplitude)
		{
			outcutoff+= (float(pFBvcflfowave[rint<int>(FBvcflfophase)])
							*float(FBvcflfoamplitude));
		}
		if (outcutoff<0) outcutoff = 0;
		else if (outcutoff>max_cutoff) outcutoff=max_cutoff;

		if (FBvcftype > MAXFILTER)
		{
			l_fb_filter.setphaser(FBvcftype-MAXFILTER,outcutoff*(flunbal/(512.0f*512.0f)),FBvcfresonance);
			r_fb_filter.setphaser(FBvcftype-MAXFILTER,outcutoff*(frunbal/(512.0f*512.0f)),FBvcfresonance);
		}
		else if (FBvcftype > MAXMOOG)
		{
			l_fb_filter.setfilter((FBvcftype-MAXMOOG-1)/2,outcutoff*(flunbal/(512.0f*512.0f)),FBvcfresonance);
			r_fb_filter.setfilter((FBvcftype-MAXMOOG-1)/2,outcutoff*(frunbal/(512.0f*512.0f)),FBvcfresonance);
		}
		else if (FBvcftype & 1)
		{
			l_fb_filter.setmoog1((FBvcftype-1)/2,outcutoff*(flunbal/(512.0f*512.0f)),FBvcfresonance);
			r_fb_filter.setmoog1((FBvcftype-1)/2,outcutoff*(frunbal/(512.0f*512.0f)),FBvcfresonance);
		}
		else
		{
			l_fb_filter.setmoog2((FBvcftype-1)/2,outcutoff*(flunbal/(512.0f*512.0f)),FBvcfresonance);
			r_fb_filter.setmoog2((FBvcftype-1)/2,outcutoff*(frunbal/(512.0f*512.0f)),FBvcfresonance);
		}
	}
	// overdrive
	if (FBoverdrive)
	{
		if (FBgainlfoamplitude)
		{
			float p = FBoverdrivegain+(float(pFBvcflfowave[rint<int>(FBvcflfophase)])
							*float(FBgainlfoamplitude));
			if (p < 0) 
			{
				p = 0;
			}
			else if (p > 256)
			{
				p = 256;
			}
			FBOutGain = 1.0f+(p*p/OVERDRIVEDIVISOR);
		}
		else
		{
			FBOutGain = 1.0f+(FBoverdrivegain*FBoverdrivegain/OVERDRIVEDIVISOR);
		}
		// gain adsr
		if (FBOutGain < 1.0f) FBOutGain = 1.0f;
		else if (FBOutGain > 65536/OVERDRIVEDIVISOR) FBOutGain = 65536/OVERDRIVEDIVISOR;
	}
#endif
}

void mi::SequencerTick()
{
// Called on each tick while sequencer is playing
	if (song_freq != pCB->GetSamplingRate())
	{
		song_freq = pCB->GetSamplingRate();
		if (song_freq < 44100)
		{
			max_cutoff = MAX_VCF_CUTOFF_22050;
		}
		else
		{
			max_cutoff = MAX_VCF_CUTOFF;
		}
		if (trackBPM)
		{
			song_sync = pCB->GetBPM();
		}
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
		l_dl_filter.init(song_freq);
		r_dl_filter.init(song_freq);
#ifndef LIGHT
		l_fb_filter.init(song_freq);
		r_fb_filter.init(song_freq);
#endif
		Lbufsize = int(((buffindex[Vals[e_paraLengthL]]*song_freq*60.0f)/song_sync)+0.5f);
		Rbufsize = int(((buffindex[Vals[e_paraLengthR]]*song_freq*60.0f)/song_sync)+0.5f);
		if (Lbufsize < 4)
		{
			Lbufsize = 4;
		}
		else if (Lbufsize > MAX_BUF)
		{
			Lbufsize = MAX_BUF;
		}
		if (Rbufsize < 4)
		{
			Rbufsize = 4;
		}
		else if (Rbufsize > MAX_BUF)
		{
			Rbufsize = MAX_BUF;
		}
		Lindex %= Lbufsize;
		Rindex %= Rbufsize;
	}

	else if (trackBPM)
	{
		if (song_sync != pCB->GetBPM())  
		{
			song_sync = pCB->GetBPM();
			// recalculate song sync rate
			// optimizations coming here soon
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
			Lbufsize = int(((buffindex[Vals[e_paraLengthL]]*song_freq*60.0f)/song_sync)+0.5f);
			Rbufsize = int(((buffindex[Vals[e_paraLengthR]]*song_freq*60.0f)/song_sync)+0.5f);
			if (Lbufsize < 4)
			{
				Lbufsize = 4;
			}
			else if (Lbufsize > MAX_BUF)
			{
				Lbufsize = MAX_BUF;
			}
			if (Rbufsize < 4)
			{
				Rbufsize = 4;
			}
			else if (Rbufsize > MAX_BUF)
			{
				Rbufsize = MAX_BUF;
			}
			Lindex %= Lbufsize;
			Rindex %= Rbufsize;
		}
	}
}

void mi::Command()
{
// Called when user presses editor button
// Probably you want to show your custom window here
// or an about button
	pCB->MessBox("Jeremy Evers\r\nnegspect@hotmail.com",PLUGIN_NAME,0);
}

void mi::NewInertia(int * source, int dest)
{
	if (inertia)
	{
		INERTIA* pI;
		pI = pInertia;
		while (pI)
		{
			if (pI->source == source)
			{
				break;
			}
			pI = pI->next;
		}
		if (*source == dest)
		{
			if (pI)
			{
				DeleteInertia(pI);
			}
			return;
		}
		if (!pI)
		{
			pI = new INERTIA;
			if (pLastInertia)
			{
				pLastInertia->next = pI;
				pI->next = NULL;
				pI->prev = pLastInertia;
				pLastInertia = pI;
			}
			else
			{
				pI->next = NULL;
				pI->prev = NULL;
				pInertia = pI;
				pLastInertia = pI;
			}
		}
		pI->current = float(*source);
		pI->source = source;
		pI->dest = dest;
		pI->add = (dest-pI->current)/inertia*(44100.0f/song_freq);
		pI->bCutoff = 0;
	}
	else
	{
		*source = dest;
	}
}

void mi::NewInertiaDLVC(int * source, int dest)
{
	if (inertia)
	{
		INERTIA* pI;
		pI = pInertia;
		while (pI)
		{
			if (pI->source == source)
			{
				break;
			}
			pI = pI->next;
		}
		if (*source == dest)
		{
			if (pI)
			{
				DeleteInertia(pI);
			}
			return;
		}
		if (!pI)
		{
			pI = new INERTIA;
			if (pLastInertia)
			{
				pLastInertia->next = pI;
				pI->next = NULL;
				pI->prev = pLastInertia;
				pLastInertia = pI;
			}
			else
			{
				pI->next = NULL;
				pI->prev = NULL;
				pInertia = pI;
				pLastInertia = pI;
			}
		}
		pI->current = float(*source);
		pI->source = source;
		pI->dest = dest;
		pI->add = (dest-pI->current)/inertia*(44100.0f/song_freq);
		pI->bCutoff = 1;
	}
	else
	{
		*source = dest;
		DLVcfCutoff=float(dest);
	}
}

#ifndef LIGHT
void mi::NewInertiaFBVC(int * source, int dest)
{
	if (inertia)
	{
		INERTIA* pI;
		pI = pInertia;
		while (pI)
		{
			if (pI->source == source)
			{
				break;
			}
			pI = pI->next;
		}
		if (*source == dest)
		{
			if (pI)
			{
				DeleteInertia(pI);
			}
			return;
		}
		if (!pI)
		{
			pI = new INERTIA;
			if (pLastInertia)
			{
				pLastInertia->next = pI;
				pI->next = NULL;
				pI->prev = pLastInertia;
				pLastInertia = pI;
			}
			else
			{
				pI->next = NULL;
				pI->prev = NULL;
				pInertia = pI;
				pLastInertia = pI;
			}
		}
		pI->current = float(*source);
		pI->source = source;
		pI->dest = dest;
		pI->add = (dest-pI->current)/inertia*(44100.0f/song_freq);
		pI->bCutoff = 2;
	}
	else
	{
		*source = dest;
		FBVcfCutoff=float(dest);
	}
}
#endif

void mi::DeleteInertia(INERTIA* pI)
{
	if (pInertia == pI)
	{
		pInertia = pI->next;
	}
	if (pLastInertia == pI)
	{
		pLastInertia = pI->prev;
	}
	if (pI->next)
	{
		pI->next->prev = pI->prev;
	}
	if (pI->prev)
	{
		pI->prev->next = pI->next;
	}
	delete pI;
}

void mi::UpdateInertia()
{
	INERTIA* pI;
	pI = pInertia;
	if (inertia)
	{
		while (pI)
		{
			pI->current += pI->add;
			if ((pI->add > 0) && (pI->current > pI->dest))
			{
				*(pI->source) = pI->dest;
				if (pI->bCutoff == 1)
				{
					DLVcfCutoff=float(*pI->source);
				}
#ifndef LIGHT
				else if (pI->bCutoff == 2)
				{
					FBVcfCutoff=float(*pI->source);
				}
#endif
				INERTIA* pI2 = pI->next;
				DeleteInertia(pI);
				pI=pI2;
			}
			else if ((pI->add < 0) && (pI->current < pI->dest))
			{
				*(pI->source) = pI->dest;
				if (pI->bCutoff == 1)
				{
					DLVcfCutoff=float(*pI->source);
				}
#ifndef LIGHT
				else if (pI->bCutoff == 2)
				{
					FBVcfCutoff=float(*pI->source);
				}
#endif
				INERTIA* pI2 = pI->next;
				DeleteInertia(pI);
				pI=pI2;
			}
			else if (pI->current == pI->dest)
			{
				*(pI->source) = pI->dest;
				if (pI->bCutoff == 1)
				{
					DLVcfCutoff=float(*pI->source);
				}
#ifndef LIGHT
				else if (pI->bCutoff == 2)
				{
					FBVcfCutoff=float(*pI->source);
				}
#endif
				INERTIA* pI2 = pI->next;
				DeleteInertia(pI);
				pI=pI2;
			}
			else 
			{
				*pI->source = rint<int>(pI->current);
				if (pI->bCutoff == 1)
				{
					DLVcfCutoff=pI->current;
				}
#ifndef LIGHT
				else if (pI->bCutoff == 2)
				{
					FBVcfCutoff=pI->current;
				}
#endif
				pI = pI->next;
			}
		}
	}
	else
	{
		while (pI)
		{
			*(pI->source) = pI->dest;
			if (pI->bCutoff == 1)
			{
				DLVcfCutoff=float(*pI->source);
			}
#ifndef LIGHT
			else if (pI->bCutoff == 2)
			{
				FBVcfCutoff=float(*pI->source);
			}
#endif
			INERTIA* pI2 = pI->next;
			DeleteInertia(pI);
			pI=pI2;
		}
	}
}

void mi::ParameterTweak(int par, int val)
{
	Vals[par]=val;
	switch (par)
	{
		case e_paraInputGain:
			InGain = val/256.0f;
			break;
		case e_paraInertia:
			inertia = val;
			break;
		case e_paraOUTmix:
			// mix
			wmix = val/256.0f;
			dmix = 1.0f-wmix;
			break;
#ifndef LIGHT
		case e_paraOUToverdrive:								FNoverdrive=val; break;
		case e_paraOUToverdrivegain:				NewInertia(&FNoverdrivegain, val); break;
#endif

		case e_paraLengthL:
			Lbufsize = int(((buffindex[val]*song_freq*60.0f)/song_sync)+0.5f);
			if (Lbufsize < 4)
			{
				Lbufsize = 4;
			}
			else if (Lbufsize > MAX_BUF)
			{
				Lbufsize = MAX_BUF;
			}
			Lindex %= Lbufsize;
			break;
		case e_paraLengthR:
			Rbufsize = int(((buffindex[val]*song_freq*60.0f)/song_sync)+0.5f);
			if (Rbufsize < 4)
			{
				Rbufsize = 4;
			}
			else if (Rbufsize > MAX_BUF)
			{
				Rbufsize = MAX_BUF;
			}
			Rindex %= Rbufsize;
			break;
		case e_paraDelayPanL:				NewInertia(&LDLpan, val); break;
		case e_paraDelayPanR:				NewInertia(&RDLpan, val); break;
		case e_paraFeedbackL:								NewInertia(&LFB, val); break;
		case e_paraFeedbackR:								NewInertia(&RFB, val); break;
		case e_paraFeedbackPanL:				NewInertia(&LFBpan, val); break;
		case e_paraFeedbackPanR:				NewInertia(&RFBpan, val); break;

		case e_paraDLVCFcutoff:												NewInertiaDLVC(&DLvcfcutoff, val); break;
		case e_paraDLVCFresonance:								NewInertia(&DLvcfresonance, val); break;
		case e_paraDLVCFlfoamplitude:								NewInertia(&DLvcflfoamplitude, val); break;
		case e_paraDLGainlfoamplitude:								NewInertia(&DLgainlfoamplitude, val); break;
		case e_paraDLUnbalancelfoamplitude:								NewInertia(&DLunbalancelfoamplitude, val); break;
		case e_paraDLVCFlfospeed:												NewInertia(&DLvcflfospeed, val); break;
		case e_paraDLVCFlfowave:												DLvcflfowave=val%MAXLFOWAVE; pDLvcflfowave=SourceWaveTable[DLvcflfowave]; break;
		case e_paraDLVCFtype: 
			if (DLvcftype!=val)
			{
				DLvcftype=val; 
				l_dl_filter.reset();
				r_dl_filter.reset();
				float outcutoff = DLVcfCutoff;
				if (DLvcflfoamplitude)
				{
					outcutoff+= (float(pDLvcflfowave[rint<int>(DLvcflfophase)])
									*float(DLvcflfoamplitude));
				}
				if (outcutoff<0) outcutoff = 0;
				else if (outcutoff>max_cutoff) outcutoff=max_cutoff;

				if (DLvcftype > MAXFILTER)
				{
					l_dl_filter.setphaser(DLvcftype-MAXFILTER,outcutoff*(DLlunbal/(512.0f*512.0f)),DLvcfresonance);
					r_dl_filter.setphaser(DLvcftype-MAXFILTER,outcutoff*(DLrunbal/(512.0f*512.0f)),DLvcfresonance);
				}
				else if (DLvcftype > MAXMOOG)
				{
					l_dl_filter.setfilter((DLvcftype-MAXMOOG-1)/2,outcutoff*(DLlunbal/(512.0f*512.0f)),DLvcfresonance);
					r_dl_filter.setfilter((DLvcftype-MAXMOOG-1)/2,outcutoff*(DLrunbal/(512.0f*512.0f)),DLvcfresonance);
				}
				else if (DLvcftype & 1)
				{
					l_dl_filter.setmoog1((DLvcftype-1)/2,outcutoff*(DLlunbal/(512.0f*512.0f)),DLvcfresonance);
					r_dl_filter.setmoog1((DLvcftype-1)/2,outcutoff*(DLrunbal/(512.0f*512.0f)),DLvcfresonance);
				}
				else
				{
					l_dl_filter.setmoog2((DLvcftype-1)/2,outcutoff*(DLlunbal/(512.0f*512.0f)),DLvcfresonance);
					r_dl_filter.setmoog2((DLvcftype-1)/2,outcutoff*(DLrunbal/(512.0f*512.0f)),DLvcfresonance);
				}
			}
			break;
		case e_paraDLOUToverdrive:								DLoverdrive=val; break;
		case e_paraDLOUToverdrivegain:				NewInertia(&DLoverdrivegain, val); break;
		case e_paraDLUnbalance:
			// unbalance
			if (val <= 256)
			{
				int t;
				// left
				t = 512-(256-val);
				t *= t;
				NewInertia(&DLrunbal, t);
				NewInertia(&DLlunbal, 512*512);
			}
			else
			{
				// right
				int t;
				t = 512-(val-256);
				t *= t;
				NewInertia(&DLlunbal, t);
				NewInertia(&DLrunbal, 512*512);
			}
			break;
		case e_paraDLRoute:
			DLroute = val;
			break;
		case e_paraDLVCFlfophase:
			DLvcflfophase = val * (SAMPLE_LENGTH*2/65536.0f);
			break;

#ifndef LIGHT
		case e_paraFBVCFcutoff:												NewInertiaFBVC(&FBvcfcutoff, val); break;
		case e_paraFBVCFresonance:								NewInertia(&FBvcfresonance, val); break;
		case e_paraFBVCFlfoamplitude:								NewInertia(&FBvcflfoamplitude, val); break;
		case e_paraFBGainlfoamplitude:								NewInertia(&FBgainlfoamplitude, val); break;
		case e_paraFBUnbalancelfoamplitude:								NewInertia(&FBunbalancelfoamplitude, val); break;
		case e_paraFBVCFlfospeed:												NewInertia(&FBvcflfospeed, val); break;
		case e_paraFBVCFlfowave:												FBvcflfowave=val%MAXLFOWAVE; pFBvcflfowave=SourceWaveTable[FBvcflfowave]; break;
		case e_paraFBVCFtype: 
			if (FBvcftype!=val)
			{
				FBvcftype=val; 
				l_fb_filter.reset();
				r_fb_filter.reset();
				float outcutoff = FBVcfCutoff;
				if (FBvcflfoamplitude)
				{
					outcutoff+= (float(pFBvcflfowave[rint<int>(FBvcflfophase)])
									*float(FBvcflfoamplitude));
				}
				if (outcutoff<0) outcutoff = 0;
				else if (outcutoff>max_cutoff) outcutoff=max_cutoff;

				if (FBvcftype > MAXFILTER)
				{
					l_fb_filter.setphaser(FBvcftype-MAXFILTER,outcutoff*(FBlunbal/(512.0f*512.0f)),FBvcfresonance);
					r_fb_filter.setphaser(FBvcftype-MAXFILTER,outcutoff*(FBrunbal/(512.0f*512.0f)),FBvcfresonance);
				}
				else if (FBvcftype > MAXMOOG)
				{
					l_fb_filter.setfilter((FBvcftype-MAXMOOG-1)/2,outcutoff*(FBlunbal/(512.0f*512.0f)),FBvcfresonance);
					r_fb_filter.setfilter((FBvcftype-MAXMOOG-1)/2,outcutoff*(FBrunbal/(512.0f*512.0f)),FBvcfresonance);
				}
				else if (FBvcftype & 1)
				{
					l_fb_filter.setmoog1((FBvcftype-1)/2,outcutoff*(FBlunbal/(512.0f*512.0f)),FBvcfresonance);
					r_fb_filter.setmoog1((FBvcftype-1)/2,outcutoff*(FBrunbal/(512.0f*512.0f)),FBvcfresonance);
				}
				else
				{
					l_fb_filter.setmoog2((FBvcftype-1)/2,outcutoff*(FBlunbal/(512.0f*512.0f)),FBvcfresonance);
					r_fb_filter.setmoog2((FBvcftype-1)/2,outcutoff*(FBrunbal/(512.0f*512.0f)),FBvcfresonance);
				}
			}
			break;
		case e_paraFBOUToverdrive:								FBoverdrive=val; break;
		case e_paraFBOUToverdrivegain:				NewInertia(&FBoverdrivegain, val); break;
		case e_paraFBUnbalance:
			// unbalance
			if (val <= 256)
			{
				int t;
				// left
				t = 512-(256-val);
				t *= t;
				NewInertia(&FBrunbal, t);
				NewInertia(&FBlunbal, 512*512);
			}
			else
			{
				// right
				int t;
				t = 512-(val-256);
				t *= t;
				NewInertia(&FBlunbal, t);
				NewInertia(&FBrunbal, 512*512);
			}
			break;
		case e_paraFBRoute:
			FBroute = val;
			break;
		case e_paraFBVCFlfophase:
			FBvcflfophase = val * (SAMPLE_LENGTH*2/65536.0f);
			break;
#endif
		case e_paraTRACKTempo:
			trackBPM = val!=0;
			if (trackBPM)
			{
				if (song_sync != pCB->GetBPM())  
				{
					song_sync = pCB->GetBPM();
					// recalculate song sync rate
					// optimizations coming here soon
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
					Lbufsize = int(((buffindex[Vals[e_paraLengthL]]*song_freq*60.0f)/song_sync)+0.5f);
					Rbufsize = int(((buffindex[Vals[e_paraLengthR]]*song_freq*60.0f)/song_sync)+0.5f);
					if (Lbufsize < 4)
					{
						Lbufsize = 4;
					}
					else if (Lbufsize > MAX_BUF)
					{
						Lbufsize = MAX_BUF;
					}
					if (Rbufsize < 4)
					{
						Rbufsize = 4;
					}
					else if (Rbufsize > MAX_BUF)
					{
						Rbufsize = MAX_BUF;
					}
					Lindex %= Lbufsize;
					Rindex %= Rbufsize;
				}
			}
			break;
	}
}

inline float mi::DoDLFilterL(float input)
{
	if (DLvcftype > MAXFILTER)
	{
		return l_dl_filter.phaser(input);
	}
	else if (DLvcftype > MAXMOOG)
	{
		if (DLvcftype & 1) return l_dl_filter.res(input);
		return l_dl_filter.res2(input);
	}
	else if (DLvcftype)
	{
		if (DLvcftype & 1) return l_dl_filter.moog1(input);
		return l_dl_filter.moog2(input);
	}
	return input;
}

inline float mi::DoDLFilterR(float input)
{
	if (DLvcftype > MAXFILTER)
	{
		return r_dl_filter.phaser(input);
	}
	else if (DLvcftype > MAXMOOG)
	{
		if (DLvcftype & 1) return r_dl_filter.res(input);
		return r_dl_filter.res2(input);
	}
	else if (DLvcftype)
	{
		if (DLvcftype & 1) return r_dl_filter.moog1(input);
		return r_dl_filter.moog2(input);
	}
	return input;
}

#ifndef LIGHT
inline float mi::DoFBFilterL(float input)
{
	if (FBvcftype > MAXFILTER)
	{
		return l_fb_filter.phaser(input);
	}
	else if (FBvcftype > MAXMOOG)
	{
		if (FBvcftype & 1) return l_fb_filter.res(input);
		return l_fb_filter.res2(input);
	}
	else if (FBvcftype)
	{
		if (FBvcftype & 1) return l_fb_filter.moog1(input);
		return l_fb_filter.moog2(input);
	}
	return input;
}

inline float mi::DoFBFilterR(float input)
{
	if (FBvcftype > MAXFILTER)
	{
		return r_fb_filter.phaser(input);
	}
	else if (FBvcftype > MAXMOOG)
	{
		if (FBvcftype & 1) return r_fb_filter.res(input);
		return r_fb_filter.res2(input);
	}
	else if (FBvcftype)
	{
		if (FBvcftype & 1) return r_fb_filter.moog1(input);
		return r_fb_filter.moog2(input);
	}
	return input;
}
#endif

#ifndef LIGHT
inline float mi::HandleFNOverdrive(float input)
{
	// handle Overdrive
	switch (FNoverdrive)
	{
	case 1: // soft clip 1
		return (atanf(input*FNOutGain)/(float)PI);
		break;
	case 2: // soft clip 2
		input *= FNOutGain;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								return -1.0f;
		else if (input > 1.0f)				return 1.0f;
		return input;
	case 3: // soft clip 3
		input *= FNOutGain;
		if (input < -0.75f) return -0.75f + ((input+0.75f)/(1.0f+(powf(2,(input-0.75f)/(1.0f-0.75f)))));
		else if (input < 0.75f) return input;
		else return 0.75f + ((input-0.75f)/(1.0f+(powf(2,(input-0.75f)/(1.0f-0.75f)))));
		break;
	case 4: // hard clip 1
		input *= FNOutGain;
		if (input < -1.0f)								return -1.0f;
		else if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 5: // hard clip 2
		// bounce off limits
		input *= FNOutGain;
		if (input < -1.0f)								return input-(rint<int>(input)+1)*(input+1.0f);
		else if (input > 1.0f)				return input-(rint<int>(input)+1)*(input-1.0f);
		return input;
		break;
	case 6: // hard clip 3
		// invert, this one is harsh
		input *= FNOutGain;
		if (input < -1.0f)								return input + (rint<int>(input/(2.0f)))*(2.0f);
		else if (input > 1.0f)				return input - (rint<int>(input/(2.0f)))*(2.0f);
		return input;
		break;
	case 7: // parabolic distortion
		input = (((input * input)*(FNOutGain*FNOutGain)))-1.0f;
		if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 8: // parabolic distortion
		input = (((input * input)*(FNOutGain*FNOutGain))*32.0f)-1.0f;
		if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 9: // sin remapper
		return SourceWaveTable[0][rint<int>(input*FNOutGain*SAMPLE_LENGTH*2)&((SAMPLE_LENGTH*2)-1)];
		break;
	case 10: //
		// good negative partial rectifier
		input *= FNOutGain;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								input= -1.0f;
		else if (input > 1.0f)				input= 1.0f;
		if (input < -0.333f)
		{
			input = -0.666f - input;
		}
		return input;
		break;
	case 11:
		// good positive partial rectifier
		input *= FNOutGain;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								input= -1.0f;
		else if (input > 1.0f)				input= 1.0f;
		if (input > 0.333f)
		{
			input = 0.666f - input;
		}
		return input;
		break;
	case 12:
		// good positive half rectifier
		if (input <= 0)
		{
			return 0;
		}
		input = fabsf(input*FNOutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input > 1.0f)								return 1.0f;
		return input;
		break;
	case 13:
		// good negative half rectifier
		if (input >= 0)
		{
			return 0;
		}
		input = -fabsf(input*FNOutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								return -1.0f;
		return input;
		break;
	case 14:
		// good negative full rectifier
		input = -fabsf(input*FNOutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								return -1.0f;
		return input;
		break;

	case 15:
		// good positive full rectifier
		input = fabsf(input*FNOutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 16:
		// assymetrical fuzz distortion 1
		if (input < 0.0f)
		{
			return (atanf(input*FNOutGain*0.5f)/(float)PI);
		}
		else 
		{
			input *= FNOutGain*4;
			if (input > 1.0f)				return 1.0f;
		}
		return input;
		break;
	case 17:
		// assymetrical fuzz distortion 2
		if (input > 0.0f)
		{
			return (atanf(input*FNOutGain*0.25f)/(float)PI);
		}
		else 
		{
			input *= FNOutGain*8;
			if (input < -1.0f)				return -1.0f;
		}
		return input;
		break;
	case 18:
		// assymetrical fuzz distortion 3 
		input *= FNOutGain;
		if (input > 0.1f)
		{
			return (atanf(input*0.5f)/(float)PI);
		}
		else 
		{
			if (input < -1.0f)				return -1.0f;
		}
		return input;
		break;
	case 19:
		// assymetrical fuzz 4
		input *= FNOutGain;
		if (input < -0.5f) return -0.25f + ((input+0.5f)/(1.0f+(powf(2,(input-0.5f)/(1.0f-0.5f)))));
		else if (input < 0.0f) return input*0.75f;
		input*=2;
		if (input > 0.5f)				return 0.5f;
		break;
	case 20:
		// assymetrical fuzz distortion 1 with hard clip
		if (input < 0.0f)
		{
			input = (atanf(input*FNOutGain*0.5f)/(float)PI);
			if (input < -0.75f)
			{
				return -0.75f;
			}
		}
		else 
		{
			input *= FNOutGain*4;
			if (input > 1.0f)				return 1.0f;
		}
		return input;
		break;
	case 21:
		// assymetrical fuzz distortion 2 with full rectifier
		if (input > 0.0f)
		{
			return -(atanf(input*FNOutGain*0.25f)/(float)PI);
		}
		else 
		{
			input *= FNOutGain*8;
			if (input < -1.0f)				return -1.0f;
		}
		return input;
		break;
	case 22:
		// assymetrical fuzz distortion 3 with partial rectifier
		input *= FNOutGain;
		if (input > 0.1f)
		{
			return (atanf(input*0.5f)/(float)PI);
		}
		else 
		{
			if (input < -0.25f) return 0.5f-input;
		}
		return input;
		break;
	case 23:
		// assymetrical fuzz 4+partial rectifier
		input *= FNOutGain;
		if (input < -0.5f) return -0.25f + ((input+0.5f)/(1.0f+(powf(2,(input-0.5f)/(1.0f-0.5f)))));
		else if (input < 0.0f) return input*0.75f;
		input*=2;
		if (input > 0.5f)				return 1.0f-input;
		break;

	case SYNTH_REMAP_0: 
	case SYNTH_REMAP_0+1: 
	case SYNTH_REMAP_0+2: 
	case SYNTH_REMAP_0+3: 
	case SYNTH_REMAP_0+4: 
	case SYNTH_REMAP_0+5: 
	case SYNTH_REMAP_0+6: 
	case SYNTH_REMAP_0+7: 
		// synth effect
		// ok track current
		if (FNtrackDir < 0)
		{
			// we are tracking negative
//												if (input < trackThresh)
			if (input < 0)
			{
				// track this sample
				if (input < FNnextMin)
				{
					FNnextMin = input;
				}
				FNnextMinCount++;
			}
			else
			{
				FNthisMin = FNnextMin;
				FNthisMinCount = FNnextMinCount;
				FNnextMin = 0;
				FNnextMinCount = 1;
				FNtrackDir = 1;
				// calculate the speeds
				if (FNoverdrivegain < 128-2)
				{
					FNw1 = (FNthisMaxCount*(FNoverdrivegain+1)/126.0f);
					FNw2 = SAMPLE_LENGTH/(FNthisMinCount+FNthisMaxCount-FNw1);
					FNw1 = SAMPLE_LENGTH/FNw1;
				}
				else if (FNoverdrivegain > 128+2)
				{
					FNw2 = (FNthisMinCount*(255-FNoverdrivegain)/126.0f);
					FNw1 = SAMPLE_LENGTH/(FNthisMinCount+FNthisMaxCount-FNw2);
					FNw2 = SAMPLE_LENGTH/FNw2;
				}
				else 
				{
					FNw1 = SAMPLE_LENGTH/float(FNthisMaxCount);
					FNw2 = SAMPLE_LENGTH/float(FNthisMinCount);
				}
				if (FNw1 < 0.00001f)
				{
					FNw1 = 0.00001f;
				}
				if (FNw2 < 0.00001f)
				{
					FNw2 = 0.00001f;
				}
//																FNtrackThresh = (FNtrackThresh+FNthisMin+((FNthisMax-FNthisMin)/2))/2;
			}
		}
		else 
		{
			// we are tracking positive
//												if (input > FNtrackThresh)
			if (input > 0)
			{
				// track this sample
				if (input > FNnextMax)
				{
					FNnextMax = input;
				}
				FNnextMaxCount++;
			}
			else
			{
				FNthisMax = FNnextMax;
				FNthisMaxCount = FNnextMaxCount;
				FNnextMax = 0;
				FNnextMaxCount = 1;
				FNtrackDir = -1;
				// calculate the speeds
				if (FNoverdrivegain < 128)
				{
					FNw1 = (FNthisMaxCount*(FNoverdrivegain+1)/128.0f);
					FNw2 = SAMPLE_LENGTH/(FNthisMinCount+FNthisMaxCount-FNw1);
					FNw1 = SAMPLE_LENGTH/FNw1;
				}
				else if (FNoverdrivegain > 128)
				{
					FNw2 = (FNthisMinCount*(255-FNoverdrivegain)/128.0f);
					FNw1 = SAMPLE_LENGTH/(FNthisMinCount+FNthisMaxCount-FNw2);
					FNw2 = SAMPLE_LENGTH/FNw2;
				}
				else 
				{
					FNw1 = SAMPLE_LENGTH/float(FNthisMaxCount);
					FNw2 = SAMPLE_LENGTH/float(FNthisMinCount);
				}
				if (FNw1 < 0.00001f)
				{
					FNw1 = 0.00001f;
				}
				if (FNw2 < 0.00001f)
				{
					FNw2 = 0.00001f;
				}
//																FNtrackThresh = (FNtrackThresh+FNthisMin+((FNthisMax-FNthisMin)/2))/2;
			}
		}
		// now we just play a waveform
		if (FNplayDir<0)
		{
			FNplayPos += FNw2;
		}
		else
		{
			FNplayPos += FNw1;
		}
		if (FNplayPos < 0)
		{
			FNplayPos = 0;
		}
		if (FNplayPos >= SAMPLE_LENGTH)
		{
			int i = rint<int>(FNplayPos/SAMPLE_LENGTH);
			if (i & 1)
			{
				FNplayDir = -FNplayDir;
			}
			FNplayPos = (FNplayPos-rint<int>(FNplayPos))+(rint<int>(FNplayPos)&((SAMPLE_LENGTH)-1));
		}
		switch (FNoverdrive)
		{
		case SYNTH_REMAP_0: 
			if (FNplayDir<0)
			{
				return SourceWaveTable[0][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMin*2;
			}
			else
			{
				return SourceWaveTable[0][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+1: 
			if (FNplayDir<0)
			{
				return SourceWaveTable[1][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMin*2;
			}
			else
			{
				return SourceWaveTable[1][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+2: 
			if (FNplayDir<0)
			{
				return SourceWaveTable[10][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMin*2;
			}
			else
			{
				return SourceWaveTable[10][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+3: 
			if (FNplayDir<0)
			{
				return SourceWaveTable[4][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMin*2;
			}
			else
			{
				return SourceWaveTable[4][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+4: 
			if (FNplayDir<0)
			{
				return SourceWaveTable[0][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMin*2;
			}
			else
			{
				return SourceWaveTable[10][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+5: 
			if (FNplayDir<0)
			{
				return SourceWaveTable[1][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMin*2;
			}
			else
			{
				return SourceWaveTable[10][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+6: 
			if (FNplayDir<0)
			{
				return SourceWaveTable[1][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMin*2;
			}
			else
			{
				return SourceWaveTable[4][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+7: 
			if (FNplayDir<0)
			{
				return SourceWaveTable[10][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMin*2;
			}
			else
			{
				return SourceWaveTable[4][rint<int>(FNplayPos)+SAMPLE_LENGTH]*FNthisMax*2;
			}
			break;
		}
	}
	return input;
}
#endif

inline float mi::HandleDLOverdrive(float input)
{
	// handle Overdrive
	switch (DLoverdrive)
	{
	case 1: // soft clip 1
		return (atanf(input*DLOutGain)/(float)PI);
		break;
	case 2: // soft clip 2
		input *= DLOutGain;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								return -1.0f;
		else if (input > 1.0f)				return 1.0f;
		return input;
	case 3: // soft clip 3
		input *= DLOutGain;
		if (input < -0.75f) return -0.75f + ((input+0.75f)/(1.0f+(powf(2,(input-0.75f)/(1.0f-0.75f)))));
		else if (input < 0.75f) return input;
		else return 0.75f + ((input-0.75f)/(1.0f+(powf(2,(input-0.75f)/(1.0f-0.75f)))));
		break;
	case 4: // hard clip 1
		input *= DLOutGain;
		if (input < -1.0f)								return -1.0f;
		else if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 5: // hard clip 2
		// bounce off limits
		input *= DLOutGain;
		if (input < -1.0f)								return input-(rint<int>(input)+1)*(input+1.0f);
		else if (input > 1.0f)				return input-(rint<int>(input)+1)*(input-1.0f);
		return input;
		break;
	case 6: // hard clip 3
		// invert, this one is harsh
		input *= DLOutGain;
		if (input < -1.0f)								return input + (rint<int>(input/(2.0f)))*(2.0f);
		else if (input > 1.0f)				return input - (rint<int>(input/(2.0f)))*(2.0f);
		return input;
		break;
	case 7: // parabolic distortion
		input = (((input * input)*(DLOutGain*DLOutGain)))-1.0f;
		if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 8: // parabolic distortion
		input = (((input * input)*(DLOutGain*DLOutGain))*32.0f)-1.0f;
		if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 9: // sin remapper
		return SourceWaveTable[0][rint<int>(input*DLOutGain*SAMPLE_LENGTH*2)&((SAMPLE_LENGTH*2)-1)];
		break;
	case 10: //
		// good negative partial rectifier
		input *= DLOutGain;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								input= -1.0f;
		else if (input > 1.0f)				input= 1.0f;
		if (input < -0.333f)
		{
			input = -0.666f - input;
		}
		return input;
		break;
	case 11:
		// good positive partial rectifier
		input *= DLOutGain;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								input= -1.0f;
		else if (input > 1.0f)				input= 1.0f;
		if (input > 0.333f)
		{
			input = 0.666f - input;
		}
		return input;
		break;
	case 12:
		// good positive half rectifier
		if (input <= 0)
		{
			return 0;
		}
		input = fabsf(input*DLOutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input > 1.0f)								return 1.0f;
		return input;
		break;
	case 13:
		// good negative half rectifier
		if (input >= 0)
		{
			return 0;
		}
		input = -fabsf(input*DLOutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								return -1.0f;
		return input;
		break;
	case 14:
		// good negative full rectifier
		input = -fabsf(input*DLOutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								return -1.0f;
		return input;
		break;

	case 15:
		// good positive full rectifier
		input = fabsf(input*DLOutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 16:
		// assymetrical fuzz distortion 1
		if (input < 0.0f)
		{
			return (atanf(input*DLOutGain*0.5f)/(float)PI);
		}
		else 
		{
			input *= DLOutGain*4;
			if (input > 1.0f)				return 1.0f;
		}
		return input;
		break;
	case 17:
		// assymetrical fuzz distortion 2
		if (input > 0.0f)
		{
			return (atanf(input*DLOutGain*0.25f)/(float)PI);
		}
		else 
		{
			input *= DLOutGain*8;
			if (input < -1.0f)				return -1.0f;
		}
		return input;
		break;
	case 18:
		// assymetrical fuzz distortion 3 
		input *= DLOutGain;
		if (input > 0.1f)
		{
			return (atanf(input*0.5f)/(float)PI);
		}
		else 
		{
			if (input < -1.0f)				return -1.0f;
		}
		return input;
		break;
	case 19:
		// assymetrical fuzz 4
		input *= DLOutGain;
		if (input < -0.5f) return -0.25f + ((input+0.5f)/(1.0f+(powf(2,(input-0.5f)/(1.0f-0.5f)))));
		else if (input < 0.0f) return input*0.75f;
		input*=2;
		if (input > 0.5f)				return 0.5f;
		break;
	case 20:
		// assymetrical fuzz distortion 1 with hard clip
		if (input < 0.0f)
		{
			input = (atanf(input*DLOutGain*0.5f)/(float)PI);
			if (input < -0.75f)
			{
				return -0.75f;
			}
		}
		else 
		{
			input *= DLOutGain*4;
			if (input > 1.0f)				return 1.0f;
		}
		return input;
		break;
	case 21:
		// assymetrical fuzz distortion 2 with full rectifier
		if (input > 0.0f)
		{
			return -(atanf(input*DLOutGain*0.25f)/(float)PI);
		}
		else 
		{
			input *= DLOutGain*8;
			if (input < -1.0f)				return -1.0f;
		}
		return input;
		break;
	case 22:
		// assymetrical fuzz distortion 3 with partial rectifier
		input *= DLOutGain;
		if (input > 0.1f)
		{
			return (atanf(input*0.5f)/(float)PI);
		}
		else 
		{
			if (input < -0.25f) return 0.5f-input;
		}
		return input;
		break;
	case 23:
		// assymetrical fuzz 4+partial rectifier
		input *= DLOutGain;
		if (input < -0.5f) return -0.25f + ((input+0.5f)/(1.0f+(powf(2,(input-0.5f)/(1.0f-0.5f)))));
		else if (input < 0.0f) return input*0.75f;
		input*=2;
		if (input > 0.5f)				return 1.0f-input;
		break;

	case SYNTH_REMAP_0: 
	case SYNTH_REMAP_0+1: 
	case SYNTH_REMAP_0+2: 
	case SYNTH_REMAP_0+3: 
	case SYNTH_REMAP_0+4: 
	case SYNTH_REMAP_0+5: 
	case SYNTH_REMAP_0+6: 
	case SYNTH_REMAP_0+7: 
		// synth effect
		// ok track current
		if (DLtrackDir < 0)
		{
			// we are tracking negative
//												if (input < trackThresh)
			if (input < 0)
			{
				// track this sample
				if (input < DLnextMin)
				{
					DLnextMin = input;
				}
				DLnextMinCount++;
			}
			else
			{
				DLthisMin = DLnextMin;
				DLthisMinCount = DLnextMinCount;
				DLnextMin = 0;
				DLnextMinCount = 1;
				DLtrackDir = 1;
				// calculate the speeds
				if (DLoverdrivegain < 128-2)
				{
					DLw1 = (DLthisMaxCount*(DLoverdrivegain+1)/126.0f);
					DLw2 = SAMPLE_LENGTH/(DLthisMinCount+DLthisMaxCount-DLw1);
					DLw1 = SAMPLE_LENGTH/DLw1;
				}
				else if (DLoverdrivegain > 128+2)
				{
					DLw2 = (DLthisMinCount*(255-DLoverdrivegain)/126.0f);
					DLw1 = SAMPLE_LENGTH/(DLthisMinCount+DLthisMaxCount-DLw2);
					DLw2 = SAMPLE_LENGTH/DLw2;
				}
				else 
				{
					DLw1 = SAMPLE_LENGTH/float(DLthisMaxCount);
					DLw2 = SAMPLE_LENGTH/float(DLthisMinCount);
				}
				if (DLw1 < 0.00001f)
				{
					DLw1 = 0.00001f;
				}
				if (DLw2 < 0.00001f)
				{
					DLw2 = 0.00001f;
				}
//																DLtrackThresh = (DLtrackThresh+DLthisMin+((DLthisMax-DLthisMin)/2))/2;
			}
		}
		else 
		{
			// we are tracking positive
//												if (input > DLtrackThresh)
			if (input > 0)
			{
				// track this sample
				if (input > DLnextMax)
				{
					DLnextMax = input;
				}
				DLnextMaxCount++;
			}
			else
			{
				DLthisMax = DLnextMax;
				DLthisMaxCount = DLnextMaxCount;
				DLnextMax = 0;
				DLnextMaxCount = 1;
				DLtrackDir = -1;
				// calculate the speeds
				if (DLoverdrivegain < 128)
				{
					DLw1 = (DLthisMaxCount*(DLoverdrivegain+1)/128.0f);
					DLw2 = SAMPLE_LENGTH/(DLthisMinCount+DLthisMaxCount-DLw1);
					DLw1 = SAMPLE_LENGTH/DLw1;
				}
				else if (DLoverdrivegain > 128)
				{
					DLw2 = (DLthisMinCount*(255-DLoverdrivegain)/128.0f);
					DLw1 = SAMPLE_LENGTH/(DLthisMinCount+DLthisMaxCount-DLw2);
					DLw2 = SAMPLE_LENGTH/DLw2;
				}
				else 
				{
					DLw1 = SAMPLE_LENGTH/float(DLthisMaxCount);
					DLw2 = SAMPLE_LENGTH/float(DLthisMinCount);
				}
				if (DLw1 < 0.00001f)
				{
					DLw1 = 0.00001f;
				}
				if (DLw2 < 0.00001f)
				{
					DLw2 = 0.00001f;
				}
//																DLtrackThresh = (DLtrackThresh+DLthisMin+((DLthisMax-DLthisMin)/2))/2;
			}
		}
		// now we just play a waveform
		if (DLplayDir<0)
		{
			DLplayPos += DLw2;
		}
		else
		{
			DLplayPos += DLw1;
		}
		if (DLplayPos < 0)
		{
			DLplayPos = 0;
		}
		if (DLplayPos >= SAMPLE_LENGTH)
		{
			int i = rint<int>(DLplayPos/SAMPLE_LENGTH);
			if (i & 1)
			{
				DLplayDir = -DLplayDir;
			}
			DLplayPos = (DLplayPos-rint<int>(DLplayPos))+(rint<int>(DLplayPos)&((SAMPLE_LENGTH)-1));
		}
		switch (DLoverdrive)
		{
		case SYNTH_REMAP_0: 
			if (DLplayDir<0)
			{
				return SourceWaveTable[0][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMin*2;
			}
			else
			{
				return SourceWaveTable[0][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+1: 
			if (DLplayDir<0)
			{
				return SourceWaveTable[1][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMin*2;
			}
			else
			{
				return SourceWaveTable[1][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+2: 
			if (DLplayDir<0)
			{
				return SourceWaveTable[10][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMin*2;
			}
			else
			{
				return SourceWaveTable[10][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+3: 
			if (DLplayDir<0)
			{
				return SourceWaveTable[4][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMin*2;
			}
			else
			{
				return SourceWaveTable[4][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+4: 
			if (DLplayDir<0)
			{
				return SourceWaveTable[0][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMin*2;
			}
			else
			{
				return SourceWaveTable[10][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+5: 
			if (DLplayDir<0)
			{
				return SourceWaveTable[1][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMin*2;
			}
			else
			{
				return SourceWaveTable[10][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+6: 
			if (DLplayDir<0)
			{
				return SourceWaveTable[1][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMin*2;
			}
			else
			{
				return SourceWaveTable[4][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+7: 
			if (DLplayDir<0)
			{
				return SourceWaveTable[10][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMin*2;
			}
			else
			{
				return SourceWaveTable[4][rint<int>(DLplayPos)+SAMPLE_LENGTH]*DLthisMax*2;
			}
			break;
		}
	}
	return input;
}

#ifndef LIGHT
inline float mi::HandleFBOverdrive(float input)
{
	// handle Overdrive
	switch (FBoverdrive)
	{
	case 1: // soft clip 1
		return (atanf(input*FBOutGain)/(float)PI);
		break;
	case 2: // soft clip 2
		input *= FBOutGain;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								return -1.0f;
		else if (input > 1.0f)				return 1.0f;
		return input;
	case 3: // soft clip 3
		input *= FBOutGain;
		if (input < -0.75f) return -0.75f + ((input+0.75f)/(1.0f+(powf(2,(input-0.75f)/(1.0f-0.75f)))));
		else if (input < 0.75f) return input;
		else return 0.75f + ((input-0.75f)/(1.0f+(powf(2,(input-0.75f)/(1.0f-0.75f)))));
		break;
	case 4: // hard clip 1
		input *= FBOutGain;
		if (input < -1.0f)								return -1.0f;
		else if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 5: // hard clip 2
		// bounce off limits
		input *= FBOutGain;
		if (input < -1.0f)								return input-(rint<int>(input)+1)*(input+1.0f);
		else if (input > 1.0f)				return input-(rint<int>(input)+1)*(input-1.0f);
		return input;
		break;
	case 6: // hard clip 3
		// invert, this one is harsh
		input *= FBOutGain;
		if (input < -1.0f)								return input + (rint<int>(input/(2.0f)))*(2.0f);
		else if (input > 1.0f)				return input - (rint<int>(input/(2.0f)))*(2.0f);
		return input;
		break;
	case 7: // parabolic distortion
		input = (((input * input)*(FBOutGain*FBOutGain)))-1.0f;
		if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 8: // parabolic distortion
		input = (((input * input)*(FBOutGain*FBOutGain))*32.0f)-1.0f;
		if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 9: // sin remapper
		return SourceWaveTable[0][rint<int>(input*FBOutGain*SAMPLE_LENGTH*2)&((SAMPLE_LENGTH*2)-1)];
		break;
	case 10: //
		// good negative partial rectifier
		input *= FBOutGain;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								input= -1.0f;
		else if (input > 1.0f)				input= 1.0f;
		if (input < -0.333f)
		{
			input = -0.666f - input;
		}
		return input;
		break;
	case 11:
		// good positive partial rectifier
		input *= FBOutGain;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								input= -1.0f;
		else if (input > 1.0f)				input= 1.0f;
		if (input > 0.333f)
		{
			input = 0.666f - input;
		}
		return input;
		break;
	case 12:
		// good positive half rectifier
		if (input <= 0)
		{
			return 0;
		}
		input = fabsf(input*FBOutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input > 1.0f)								return 1.0f;
		return input;
		break;
	case 13:
		// good negative half rectifier
		if (input >= 0)
		{
			return 0;
		}
		input = -fabsf(input*FBOutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								return -1.0f;
		return input;
		break;
	case 14:
		// good negative full rectifier
		input = -fabsf(input*FBOutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								return -1.0f;
		return input;
		break;

	case 15:
		// good positive full rectifier
		input = fabsf(input*FBOutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 16:
		// assymetrical fuzz distortion 1
		if (input < 0.0f)
		{
			return (atanf(input*FBOutGain*0.5f)/(float)PI);
		}
		else 
		{
			input *= FBOutGain*4;
			if (input > 1.0f)				return 1.0f;
		}
		return input;
		break;
	case 17:
		// assymetrical fuzz distortion 2
		if (input > 0.0f)
		{
			return (atanf(input*FBOutGain*0.25f)/(float)PI);
		}
		else 
		{
			input *= FBOutGain*8;
			if (input < -1.0f)				return -1.0f;
		}
		return input;
		break;
	case 18:
		// assymetrical fuzz distortion 3 
		input *= FBOutGain;
		if (input > 0.1f)
		{
			return (atanf(input*0.5f)/(float)PI);
		}
		else 
		{
			if (input < -1.0f)				return -1.0f;
		}
		return input;
		break;
	case 19:
		// assymetrical fuzz 4
		input *= FBOutGain;
		if (input < -0.5f) return -0.25f + ((input+0.5f)/(1.0f+(powf(2,(input-0.5f)/(1.0f-0.5f)))));
		else if (input < 0.0f) return input*0.75f;
		input*=2;
		if (input > 0.5f)				return 0.5f;
		break;
	case 20:
		// assymetrical fuzz distortion 1 with hard clip
		if (input < 0.0f)
		{
			input = (atanf(input*FBOutGain*0.5f)/(float)PI);
			if (input < -0.75f)
			{
				return -0.75f;
			}
		}
		else 
		{
			input *= FBOutGain*4;
			if (input > 1.0f)				return 1.0f;
		}
		return input;
		break;
	case 21:
		// assymetrical fuzz distortion 2 with full rectifier
		if (input > 0.0f)
		{
			return -(atanf(input*FBOutGain*0.25f)/(float)PI);
		}
		else 
		{
			input *= FBOutGain*8;
			if (input < -1.0f)				return -1.0f;
		}
		return input;
		break;
	case 22:
		// assymetrical fuzz distortion 3 with partial rectifier
		input *= FBOutGain;
		if (input > 0.1f)
		{
			return (atanf(input*0.5f)/(float)PI);
		}
		else 
		{
			if (input < -0.25f) return 0.5f-input;
		}
		return input;
		break;
	case 23:
		// assymetrical fuzz 4+partial rectifier
		input *= FBOutGain;
		if (input < -0.5f) return -0.25f + ((input+0.5f)/(1.0f+(powf(2,(input-0.5f)/(1.0f-0.5f)))));
		else if (input < 0.0f) return input*0.75f;
		input*=2;
		if (input > 0.5f)				return 1.0f-input;
		break;

	case SYNTH_REMAP_0: 
	case SYNTH_REMAP_0+1: 
	case SYNTH_REMAP_0+2: 
	case SYNTH_REMAP_0+3: 
	case SYNTH_REMAP_0+4: 
	case SYNTH_REMAP_0+5: 
	case SYNTH_REMAP_0+6: 
	case SYNTH_REMAP_0+7: 
		// synth effect
		// ok track current
		if (FBtrackDir < 0)
		{
			// we are tracking negative
//												if (input < trackThresh)
			if (input < 0)
			{
				// track this sample
				if (input < FBnextMin)
				{
					FBnextMin = input;
				}
				FBnextMinCount++;
			}
			else
			{
				FBthisMin = FBnextMin;
				FBthisMinCount = FBnextMinCount;
				FBnextMin = 0;
				FBnextMinCount = 1;
				FBtrackDir = 1;
				// calculate the speeds
				if (FBoverdrivegain < 128-2)
				{
					FBw1 = (FBthisMaxCount*(FBoverdrivegain+1)/126.0f);
					FBw2 = SAMPLE_LENGTH/(FBthisMinCount+FBthisMaxCount-FBw1);
					FBw1 = SAMPLE_LENGTH/FBw1;
				}
				else if (FBoverdrivegain > 128+2)
				{
					FBw2 = (FBthisMinCount*(255-FBoverdrivegain)/126.0f);
					FBw1 = SAMPLE_LENGTH/(FBthisMinCount+FBthisMaxCount-FBw2);
					FBw2 = SAMPLE_LENGTH/FBw2;
				}
				else 
				{
					FBw1 = SAMPLE_LENGTH/float(FBthisMaxCount);
					FBw2 = SAMPLE_LENGTH/float(FBthisMinCount);
				}
				if (FBw1 < 0.00001f)
				{
					FBw1 = 0.00001f;
				}
				if (FBw2 < 0.00001f)
				{
					FBw2 = 0.00001f;
				}
//																FBtrackThresh = (FBtrackThresh+FBthisMin+((FBthisMax-FBthisMin)/2))/2;
			}
		}
		else 
		{
			// we are tracking positive
//												if (input > FBtrackThresh)
			if (input > 0)
			{
				// track this sample
				if (input > FBnextMax)
				{
					FBnextMax = input;
				}
				FBnextMaxCount++;
			}
			else
			{
				FBthisMax = FBnextMax;
				FBthisMaxCount = FBnextMaxCount;
				FBnextMax = 0;
				FBnextMaxCount = 1;
				FBtrackDir = -1;
				// calculate the speeds
				if (FBoverdrivegain < 128)
				{
					FBw1 = (FBthisMaxCount*(FBoverdrivegain+1)/128.0f);
					FBw2 = SAMPLE_LENGTH/(FBthisMinCount+FBthisMaxCount-FBw1);
					FBw1 = SAMPLE_LENGTH/FBw1;
				}
				else if (FBoverdrivegain > 128)
				{
					FBw2 = (FBthisMinCount*(255-FBoverdrivegain)/128.0f);
					FBw1 = SAMPLE_LENGTH/(FBthisMinCount+FBthisMaxCount-FBw2);
					FBw2 = SAMPLE_LENGTH/FBw2;
				}
				else 
				{
					FBw1 = SAMPLE_LENGTH/float(FBthisMaxCount);
					FBw2 = SAMPLE_LENGTH/float(FBthisMinCount);
				}
				if (FBw1 < 0.00001f)
				{
					FBw1 = 0.00001f;
				}
				if (FBw2 < 0.00001f)
				{
					FBw2 = 0.00001f;
				}
//																FBtrackThresh = (FBtrackThresh+FBthisMin+((FBthisMax-FBthisMin)/2))/2;
			}
		}
		// now we just play a waveform
		if (FBplayDir<0)
		{
			FBplayPos += FBw2;
		}
		else
		{
			FBplayPos += FBw1;
		}
		if (FBplayPos < 0)
		{
			FBplayPos = 0;
		}
		if (FBplayPos >= SAMPLE_LENGTH)
		{
			int i = rint<int>(FBplayPos/SAMPLE_LENGTH);
			if (i & 1)
			{
				FBplayDir = -FBplayDir;
			}
			FBplayPos = (FBplayPos-rint<int>(FBplayPos))+(rint<int>(FBplayPos)&((SAMPLE_LENGTH)-1));
		}
		switch (FBoverdrive)
		{
		case SYNTH_REMAP_0: 
			if (FBplayDir<0)
			{
				return SourceWaveTable[0][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMin*2;
			}
			else
			{
				return SourceWaveTable[0][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+1: 
			if (FBplayDir<0)
			{
				return SourceWaveTable[1][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMin*2;
			}
			else
			{
				return SourceWaveTable[1][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+2: 
			if (FBplayDir<0)
			{
				return SourceWaveTable[10][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMin*2;
			}
			else
			{
				return SourceWaveTable[10][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+3: 
			if (FBplayDir<0)
			{
				return SourceWaveTable[4][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMin*2;
			}
			else
			{
				return SourceWaveTable[4][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+4: 
			if (FBplayDir<0)
			{
				return SourceWaveTable[0][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMin*2;
			}
			else
			{
				return SourceWaveTable[10][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+5: 
			if (FBplayDir<0)
			{
				return SourceWaveTable[1][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMin*2;
			}
			else
			{
				return SourceWaveTable[10][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+6: 
			if (FBplayDir<0)
			{
				return SourceWaveTable[1][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMin*2;
			}
			else
			{
				return SourceWaveTable[4][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMax*2;
			}
			break;
		case SYNTH_REMAP_0+7: 
			if (FBplayDir<0)
			{
				return SourceWaveTable[10][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMin*2;
			}
			else
			{
				return SourceWaveTable[4][rint<int>(FBplayPos)+SAMPLE_LENGTH]*FBthisMax*2;
			}
			break;
		}
	}
	return input;
}
#endif

// Work... where all is cooked 
void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks)
{
	UpdateInertia();

	float DLmixLL;
	float DLmixLR;
	float DLmixRL;
	float DLmixRR;

	float FBL = LFB/256.0f;
	float FBR = RFB/256.0f;
	float FBmixLL;
	float FBmixLR;
	float FBmixRL;
	float FBmixRR;

	if (LDLpan <= 256)
	{
		// left
		DLmixLR = LDLpan/256.0f;
		DLmixLL = 1;
	}
	else
	{
		// right
		DLmixLL = (512-LDLpan)/256.0f;
		DLmixLR = 1;
	}

	if (RDLpan <= 256)
	{
		// left
		DLmixRR = RDLpan/256.0f;
		DLmixRL = 1;
	}
	else
	{
		// right
		DLmixRL = (512-RDLpan)/256.0f;
		DLmixRR = 1;
	}

	if (LFBpan <= 256)
	{
		// left
		FBmixLR = LFBpan/256.0f;
		FBmixLL = 1;
	}
	else
	{
		// right
		FBmixLL = (512-LFBpan)/256.0f;
		FBmixLR = 1;
	}

	if (RFBpan <= 256)
	{
		// left
		FBmixRR = RFBpan/256.0f;
		FBmixRL = 1;
	}
	else
	{
		// right
		FBmixRL = (512-RFBpan)/256.0f;
		FBmixRR = 1;
	}

#ifndef LIGHT
	// ok here is the order of events:
	if (DLroute)
	{
		if (FBroute)
		{
			do
			{
				if(!timetocompute--) FilterTick();

				// - get incoming sample 
				// - filter & waveshape & pan buffered sample (delay)
				// - mix with clean input -> keep mixed for feedback
				// - waveshape
				// - output
				// - filter & waveshape & pan mixed sample (delay)
				// - output

				float sl = (*psamplesleft*InGain)/32768.0f;
				float sr = (*psamplesright*InGain)/32768.0f;

				float sol = DoDLFilterL(HandleDLOverdrive(pBufferL[Lindex]));
				float sor = DoDLFilterR(HandleDLOverdrive(pBufferR[Rindex]));

				*psamplesleft++ = HandleFNOverdrive((sol*wmix)+(sl*dmix))*32768.0f; // this is what we will be outputting
				*psamplesright++ = HandleFNOverdrive((sor*wmix)+(sr*dmix))*32768.0f;

				sol = pBufferL[Lindex];
				sor = pBufferR[Rindex];

				pBufferL[Lindex++] = (DoFBFilterL(HandleFBOverdrive((sol*FBmixLL)+(sor*FBmixLR)+l_dl_filter.denormal))*FBL)+((sl*DLmixLL)+(sr*DLmixLR)+l_dl_filter.denormal);
				pBufferR[Rindex++] = (DoFBFilterR(HandleFBOverdrive((sor*FBmixRL)+(sor*FBmixRR)+l_dl_filter.denormal))*FBR)+((sl*DLmixRL)+(sr*DLmixRR)+l_dl_filter.denormal);

				if (Lindex >= Lbufsize)
				{
					Lindex = 0;
				}
				if (Rindex >= Rbufsize)
				{
					Rindex = 0;
				}
			} while(--numsamples);
		}
		else
		{
			do
			{
				if(!timetocompute--) FilterTick();

				// - get incoming sample 
				// - filter & waveshape & pan buffered sample (delay)
				// - mix with clean input -> keep mixed for feedback
				// - waveshape
				// - output
				// - filter & waveshape & pan mixed sample (delay)
				// - output

				float sl = (*psamplesleft*InGain)/32768.0f;
				float sr = (*psamplesright*InGain)/32768.0f;

				float sol = DoDLFilterL(HandleDLOverdrive(pBufferL[Lindex]));
				float sor = DoDLFilterR(HandleDLOverdrive(pBufferR[Rindex]));

				*psamplesleft++ = HandleFNOverdrive((sol*wmix)+(sl*dmix))*32768.0f; // this is what we will be outputting
				*psamplesright++ = HandleFNOverdrive((sor*wmix)+(sr*dmix))*32768.0f;

				sol = pBufferL[Lindex];
				sor = pBufferR[Rindex];

				pBufferL[Lindex++] = (HandleFBOverdrive(DoFBFilterL((sol*FBmixLL)+(sor*FBmixLR)+l_dl_filter.denormal))*FBL)+((sl*DLmixLL)+(sr*DLmixLR)+l_dl_filter.denormal);
				pBufferR[Rindex++] = (HandleFBOverdrive(DoFBFilterR((sor*FBmixRL)+(sor*FBmixRR)+l_dl_filter.denormal))*FBR)+((sl*DLmixRL)+(sr*DLmixRR)+l_dl_filter.denormal);

				if (Lindex >= Lbufsize)
				{
					Lindex = 0;
				}
				if (Rindex >= Rbufsize)
				{
					Rindex = 0;
				}
			} while(--numsamples);
		}
	}
	else
	{
		if (FBroute)
		{
			do
			{
				if(!timetocompute--) FilterTick();

				// - get incoming sample 
				// - filter & waveshape & pan buffered sample (delay)
				// - mix with clean input -> keep mixed for feedback
				// - waveshape
				// - output
				// - filter & waveshape & pan mixed sample (delay)
				// - output

				float sl = (*psamplesleft*InGain)/32768.0f;
				float sr = (*psamplesright*InGain)/32768.0f;

				float sol = HandleDLOverdrive(DoDLFilterL(pBufferL[Lindex]));
				float sor = HandleDLOverdrive(DoDLFilterR(pBufferR[Rindex]));

				*psamplesleft++ = HandleFNOverdrive((sol*wmix)+(sl*dmix))*32768.0f; // this is what we will be outputting
				*psamplesright++ = HandleFNOverdrive((sor*wmix)+(sr*dmix))*32768.0f;

				sol = pBufferL[Lindex];
				sor = pBufferR[Rindex];

				pBufferL[Lindex++] = (DoFBFilterL(HandleFBOverdrive((sol*FBmixLL)+(sor*FBmixLR)+l_dl_filter.denormal))*FBL)+((sl*DLmixLL)+(sr*DLmixLR)+l_dl_filter.denormal);
				pBufferR[Rindex++] = (DoFBFilterR(HandleFBOverdrive((sor*FBmixRL)+(sor*FBmixRR)+l_dl_filter.denormal))*FBR)+((sl*DLmixRL)+(sr*DLmixRR)+l_dl_filter.denormal);

				if (Lindex >= Lbufsize)
				{
					Lindex = 0;
				}
				if (Rindex >= Rbufsize)
				{
					Rindex = 0;
				}
			} while(--numsamples);
		}
		else
		{
			do
			{
				if(!timetocompute--) FilterTick();

				// - get incoming sample 
				// - filter & waveshape & pan buffered sample (delay)
				// - mix with clean input -> keep mixed for feedback
				// - waveshape
				// - output
				// - filter & waveshape & pan mixed sample (delay)
				// - output

				float sl = (*psamplesleft*InGain)/32768.0f;
				float sr = (*psamplesright*InGain)/32768.0f;

				float sol = HandleDLOverdrive(DoDLFilterL(pBufferL[Lindex]));
				float sor = HandleDLOverdrive(DoDLFilterR(pBufferR[Rindex]));

				*psamplesleft++ = HandleFNOverdrive((sol*wmix)+(sl*dmix))*32768.0f; // this is what we will be outputting
				*psamplesright++ = HandleFNOverdrive((sor*wmix)+(sr*dmix))*32768.0f;

				sol = pBufferL[Lindex];
				sor = pBufferR[Rindex];

				pBufferL[Lindex++] = (HandleFBOverdrive(DoFBFilterL((sol*FBmixLL)+(sor*FBmixLR)+l_dl_filter.denormal))*FBL)+((sl*DLmixLL)+(sr*DLmixLR)+l_dl_filter.denormal);
				pBufferR[Rindex++] = (HandleFBOverdrive(DoFBFilterR((sor*FBmixRL)+(sor*FBmixRR)+l_dl_filter.denormal))*FBR)+((sl*DLmixRL)+(sr*DLmixRR)+l_dl_filter.denormal);

				if (Lindex >= Lbufsize)
				{
					Lindex = 0;
				}
				if (Rindex >= Rbufsize)
				{
					Rindex = 0;
				}
			} while(--numsamples);
		}
	}
#else
	if (DLroute)
	{
		do
		{
			if(!timetocompute--) FilterTick();

			// - get incoming sample 
			// - filter & waveshape & pan buffered sample (delay)
			// - mix with clean input -> keep mixed for feedback
			// - waveshape
			// - output
			// - filter & waveshape & pan mixed sample (delay)
			// - output

			float sl = (*psamplesleft*InGain)/32768.0f;
			float sr = (*psamplesright*InGain)/32768.0f;

			float sol = DoDLFilterL(HandleDLOverdrive(pBufferL[Lindex]));
			float sor = DoDLFilterR(HandleDLOverdrive(pBufferR[Rindex]));

			*psamplesleft++ = ((sol*wmix)+(sl*dmix))*32768.0f; // this is what we will be outputting
			*psamplesright++ = ((sor*wmix)+(sr*dmix))*32768.0f;

			sol = pBufferL[Lindex];
			sor = pBufferR[Rindex];

			pBufferL[Lindex++] = (((sol*FBmixLL)+(sor*FBmixLR))*FBL)+((sl*DLmixLL)+(sr*DLmixLR))+l_dl_filter.denormal;
			pBufferR[Rindex++] = (((sor*FBmixRL)+(sor*FBmixRR))*FBR)+((sl*DLmixRL)+(sr*DLmixRR))+l_dl_filter.denormal;

			if (Lindex >= Lbufsize)
			{
				Lindex = 0;
			}
			if (Rindex >= Rbufsize)
			{
				Rindex = 0;
			}
		} while(--numsamples);
	}
	else
	{
		do
		{
			if(!timetocompute--) FilterTick();

			// - get incoming sample 
			// - filter & waveshape & pan buffered sample (delay)
			// - mix with clean input -> keep mixed for feedback
			// - waveshape
			// - output
			// - filter & waveshape & pan mixed sample (delay)
			// - output

			float sl = (*psamplesleft*InGain)/32768.0f;
			float sr = (*psamplesright*InGain)/32768.0f;

			float sol = HandleDLOverdrive(DoDLFilterL(pBufferL[Lindex]));
			float sor = HandleDLOverdrive(DoDLFilterR(pBufferR[Rindex]));

			*psamplesleft++ = ((sol*wmix)+(sl*dmix))*32768.0f; // this is what we will be outputting
			*psamplesright++ = ((sor*wmix)+(sr*dmix))*32768.0f;

			sol = pBufferL[Lindex];
			sor = pBufferR[Rindex];

			pBufferL[Lindex++] = (((sol*FBmixLL)+(sor*FBmixLR))*FBL)+((sl*DLmixLL)+(sr*DLmixLR))+l_dl_filter.denormal;
			pBufferR[Rindex++] = (((sor*FBmixRL)+(sor*FBmixRR))*FBR)+((sl*DLmixRL)+(sr*DLmixRR))+l_dl_filter.denormal;

			if (Lindex >= Lbufsize)
			{
				Lindex = 0;
			}
			if (Rindex >= Rbufsize)
			{
				Rindex = 0;
			}
		} while(--numsamples);
	}
#endif
}



void mi::InitWaveTable()
{
	int rand = 156;
	for(int c=0;c<(SAMPLE_LENGTH*2)+256;c++)
	{
		int c2 = c & ((SAMPLE_LENGTH*2)-1);
		double sval=(double)(c*0.00306796157577128245943617517898389)/(SAMPLE_LENGTH/1024);

		// sine
		SourceWaveTable[0][c]=float(sin(sval));

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
		SourceWaveTable[5][c] = float((sin(sval)*0.75f) + (sin(sval*2.0)*0.5f) + (sin(sval*4.0)*0.25f) + (sin(sval*16.0)*0.125f));
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
			SourceWaveTable[6][c] = float((1.0-sin(sval+PI/2)));
		}
		else if (c2<SAMPLE_LENGTH)				
		{
			// phase 2
			SourceWaveTable[6][c] = float((1.0-sin(sval-PI/2)));
		}
		else if (c2<SAMPLE_LENGTH*3/2)				
		{
			// phase 3
			SourceWaveTable[6][c] = float((-1.0-sin(sval+PI/2)));
		}
		else 
		{
			// phase 4
			SourceWaveTable[6][c] = float((-1.0-sin(sval-PI/2)));
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
		SourceWaveTable[8][c] = float(sin((PI/4)+(double)(c2*0.00306796157577128245943617517898389)/(SAMPLE_LENGTH/768)));

		// R soft saw
		SourceWaveTable[9][c] = float(sin((PI*5/4)+(double)(c2*0.00306796157577128245943617517898389)/(SAMPLE_LENGTH/768)));


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
			SourceWaveTable[11][c] = float((sin(sval+PI/2)));
		}
		else if (c2<SAMPLE_LENGTH)				
		{
			// phase 2
			SourceWaveTable[11][c] = float((sin(sval-PI/2)));
		}
		else if (c2<SAMPLE_LENGTH*3/2)				
		{
			// phase 3
			SourceWaveTable[11][c] = float((sin(sval+PI/2)));
		}
		else 
		{
			// phase 4
			SourceWaveTable[11][c] = float((sin(sval-PI/2)));
		}

		// racer
		if (c2<SAMPLE_LENGTH/2)				
		{
			// phase 1
			SourceWaveTable[12][c] = float((sin(sval+PI/2)));
		}
		else if (c2<SAMPLE_LENGTH)				
		{
			// phase 2
			SourceWaveTable[12][c] = float((sin(sval*2)));
		}
		else if (c2<SAMPLE_LENGTH*3/2)				
		{
			// phase 3
			SourceWaveTable[12][c] = float((sin(sval+PI/2)));
		}
		else 
		{
			// phase 4
			SourceWaveTable[12][c] = float((sin(sval*2+PI)));
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
	float fv,fv2;
	switch (param)
	{
	case e_paraDLVCFlfospeed:
#ifndef LIGHT
	case e_paraFBVCFlfospeed:
#endif
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
	case e_paraDLVCFlfowave:
#ifndef LIGHT
	case e_paraFBVCFlfowave:
#endif
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
	case e_paraDLVCFtype:
#ifndef LIGHT
	case e_paraFBVCFtype:
#endif
		// vcf type
		if (value > MAXFILTER)
		{
			sprintf(txt,"%d Stage Phaser",value-MAXFILTER);
			return true;
		}
		switch(value)
		{
			case 0 :sprintf(txt,"Off");return true;break;
			case 1 :sprintf(txt,"Moog Lowpass A");return true;break;
			case 2 :sprintf(txt,"Moog Lowpass B");return true;break;
			case 3 :sprintf(txt,"Moog Hipass A");return true;break;
			case 4 :sprintf(txt,"Moog Hipass B");return true;break;
			case 5 :sprintf(txt,"Moog Bandpass A");return true;break;
			case 6 :sprintf(txt,"Moog Bandpass B");return true;break;
			case 7 :sprintf(txt,"Lowpass A");return true;break;
			case 8 :sprintf(txt,"Lowpass B");return true;break;
			case 9 :sprintf(txt,"Hipass A");return true;break;
			case 10:sprintf(txt,"Hipass B");return true;break;
			case 11:sprintf(txt,"Bandpass A");return true;break;
			case 12:sprintf(txt,"Bandpass B");return true;break;
			case 13:sprintf(txt,"Bandreject A");return true;break;
			case 14:sprintf(txt,"Bandreject B");return true;break;
			case 15:sprintf(txt,"ParaEQ1 A");return true;break;
			case 16:sprintf(txt,"ParaEQ1 B");return true;break;
			case 17:sprintf(txt,"InvParaEQ1 A");return true;break;
			case 18:sprintf(txt,"InvParaEQ1 B");return true;break;
			case 19:sprintf(txt,"ParaEQ2 A");return true;break;
			case 20:sprintf(txt,"ParaEQ2 B");return true;break;
			case 21:sprintf(txt,"InvParaEQ2 A");return true;break;
			case 22:sprintf(txt,"InvParaEQ2 B");return true;break;
			case 23:sprintf(txt,"ParaEQ3 A");return true;break;
			case 24:sprintf(txt,"ParaEQ3 B");return true;break;
			case 25:sprintf(txt,"InvParaEQ3 A");return true;break;
			case 26:sprintf(txt,"InvParaEQ3 B");return true;break;
		}
		break;
	case e_paraDLVCFcutoff:
		// cutoff
		// vcf type
		if(DLvcftype)
		{
			if ((DLvcftype > MAXFILTER) || (DLvcftype < MAXMOOG))
			{
				fv=CUTOFFCONV(value);
				sprintf(txt,"%.2f hz",fv);
				return true;
				break;
			}
			switch ((DLvcftype-MAXMOOG-1)/2)
			{
			case 0: 
			case 1: 
			case 2: 
			case 3: 
			case 4: 
			case 5: 
					fv=CUTOFFCONV(value);
					sprintf(txt,"%.2f hz",fv);
					return true;
					break;
			case 6: 
			case 7: 
					fv=THREESEL((float)value,270,400,800);
					fv2=THREESEL((float)value,2140,800,1150);
					sprintf(txt,"%.2f, %.2f hz",fv, fv2);
					return true;
					break;
			case 8: 
			case 9: 
					fv=THREESEL((float)value,270,400,650);
					fv2=THREESEL((float)value,2140,1700,1080);
					sprintf(txt,"%.2f, %.2f hz",fv, fv2);
					return true;
					break;
			}
		}
		else
		{
			fv=CUTOFFCONV(value);
			sprintf(txt,"%.2f hz",fv);
			return true;
			break;
		}
		break;
#ifndef LIGHT
	case e_paraFBVCFcutoff:
		// cutoff
		// vcf type
		if(FBvcftype)
		{
			if ((FBvcftype > MAXFILTER) || (FBvcftype < MAXMOOG))
			{
				fv=CUTOFFCONV(value);
				sprintf(txt,"%.2f hz",fv);
				return true;
				break;
			}
			switch ((FBvcftype-MAXMOOG-1)/2)
			{
			case 0: 
			case 1: 
			case 2: 
			case 3: 
			case 4: 
			case 5: 
					fv=CUTOFFCONV(value);
					sprintf(txt,"%.2f hz",fv);
					return true;
					break;
			case 6: 
			case 7: 
					fv=THREESEL((float)value,270,400,800);
					fv2=THREESEL((float)value,2140,800,1150);
					sprintf(txt,"%.2f, %.2f hz",fv, fv2);
					return true;
					break;
			case 8: 
			case 9: 
					fv=THREESEL((float)value,270,400,650);
					fv2=THREESEL((float)value,2140,1700,1080);
					sprintf(txt,"%.2f, %.2f hz",fv, fv2);
					return true;
					break;
			}
		}
		else
		{
			fv=CUTOFFCONV(value);
			sprintf(txt,"%.2f hz",fv);
			return true;
			break;
		}
		break;
#endif
	case e_paraDLVCFresonance:
#ifndef LIGHT
	case e_paraFBVCFresonance:
#endif
		// resonance
		sprintf(txt,"%.4f%%",(float)(value-1)*0.418410042f);
		return true;
		break;
	case e_paraDLVCFlfoamplitude:
#ifndef LIGHT
	case e_paraFBVCFlfoamplitude:
#endif
		// vcf lfo amplitude
		if ( value == 0 )
		{
			sprintf(txt,"Off");
			return true;
		}
		sprintf(txt,"%.4f%%",(float)value*(100.0f/MAX_VCF_CUTOFF));
		return true;
		break;

	case e_paraDLOUToverdrive:
#ifndef LIGHT
	case e_paraOUToverdrive:
	case e_paraFBOUToverdrive:
#endif
		switch(value)
		{
			case 0 :sprintf(txt,"Off");return true;break;
			case 1 :sprintf(txt,"Soft Clip 1");return true;break;
			case 2 :sprintf(txt,"Soft Clip 2");return true;break;
			case 3 :sprintf(txt,"Soft Clip 3");return true;break;
			case 4 :sprintf(txt,"Hard Clip 1");return true;break;
			case 5 :sprintf(txt,"Hard Clip 2");return true;break;
			case 6 :sprintf(txt,"Hard Clip 3");return true;break;
			case 7 :sprintf(txt,"Parabolic Distortion 1");return true;break;
			case 8 :sprintf(txt,"Parabolic Distortion 2");return true;break;
			case 9 :sprintf(txt,"Nutty Remapper");return true;break;
			case 10 :sprintf(txt,"-Partial Rectifier");return true;break;
			case 11 :sprintf(txt,"+Partial Rectifier");return true;break;
			case 12 :sprintf(txt,"-Half Rectifier");return true;break;
			case 13 :sprintf(txt,"+Half Rectifier");return true;break;
			case 14 :sprintf(txt,"-Full Rectifier");return true;break;
			case 15 :sprintf(txt,"+Full Rectifier");return true;break;
			case 16 :sprintf(txt,"Fuzz 1");return true;break;
			case 17 :sprintf(txt,"Fuzz 2");return true;break;
			case 18 :sprintf(txt,"Fuzz 3");return true;break;
			case 19 :sprintf(txt,"Fuzz 4");return true;break;
			case 20 :sprintf(txt,"Fuzz 1+Hard Clip");return true;break;
			case 21 :sprintf(txt,"Fuzz 2+Full Rectifier");return true;break;
			case 22 :sprintf(txt,"Fuzz 3+Partial Rectifier");return true;break;
			case 23 :sprintf(txt,"Fuzz 4+Partial Rectifier");return true;break;
			case SYNTH_REMAP_0 :sprintf(txt,"Sin Remap");return true;break;
			case SYNTH_REMAP_0+1 :sprintf(txt,"Triangle Remap");return true;break;
			case SYNTH_REMAP_0+2 :sprintf(txt,"Soft Square Remap");return true;break;
			case SYNTH_REMAP_0+3 :sprintf(txt,"Square Remap");return true;break;
			case SYNTH_REMAP_0+4 :sprintf(txt,"Sin/Soft Square Remap");return true;break;
			case SYNTH_REMAP_0+5 :sprintf(txt,"Triangle/Soft Square Remap");return true;break;
			case SYNTH_REMAP_0+6 :sprintf(txt,"Triangle/Square Remap");return true;break;
			case SYNTH_REMAP_0+7 :sprintf(txt,"Soft Square/Square Remap");return true;break;
		}
		break;
#ifndef LIGHT
	case e_paraOUToverdrivegain:
		if (FNoverdrive < SYNTH_REMAP_0)
		{
			fv=value*value*100.0f/OVERDRIVEDIVISOR;
			sprintf(txt,"%.4f%%",fv);
		}
		else
		{
			fv = 100*value/256.0f;
			sprintf(txt,"%.2f%% : %.2f%%",fv,100-fv);
		}
		return true;
		break;
#endif
	case e_paraDLOUToverdrivegain:
		if (DLoverdrive < SYNTH_REMAP_0)
		{
			fv=value*value*100.0f/OVERDRIVEDIVISOR;
			sprintf(txt,"%.4f%%",fv);
		}
		else
		{
			fv = 100*value/256.0f;
			sprintf(txt,"%.2f%% : %.2f%%",fv,100-fv);
		}
		return true;
		break;
#ifndef LIGHT
	case e_paraFBOUToverdrivegain:
		if (FBoverdrive < SYNTH_REMAP_0)
		{
			fv=value*value*100.0f/OVERDRIVEDIVISOR;
			sprintf(txt,"%.4f%%",fv);
		}
		else
		{
			fv = 100*value/256.0f;
			sprintf(txt,"%.2f%% : %.2f%%",fv,100-fv);
		}
		return true;
		break;
#endif
	case e_paraInertia:
		if (value == 0)
		{
			sprintf(txt,"Off");
			return true;
		}
		fv=(float)(value*0.022676f*256);
		sprintf(txt,"%.4f ms",fv);
		return true;
		break;
	case e_paraDLUnbalance:
#ifndef LIGHT
	case e_paraFBUnbalance:
#endif
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
	case e_paraDLRoute:
#ifndef LIGHT
	case e_paraFBRoute:
#endif
		if (value)
		{
			sprintf(txt,"Shaper->Filter");
			return true;
		}
		else
		{
			sprintf(txt,"Filter->Shaper");
			return true;
		}
		break;
	case e_paraDLVCFlfophase:
#ifndef LIGHT
	case e_paraFBVCFlfophase:
#endif
		sprintf(txt,"%.2f degrees",value * (360/65536.0f));
		return true;
		break;

	case e_paraDLUnbalancelfoamplitude:
	case e_paraDLGainlfoamplitude:
#ifndef LIGHT
	case e_paraFBUnbalancelfoamplitude:
	case e_paraFBGainlfoamplitude:
#endif
		if ( value == 0 )
		{
			sprintf(txt,"Off");
			return true;
		}
		sprintf(txt,"%.2f %%",100*(value)/256.0f);
		return true;
	case e_paraInputGain:
		// mix
		sprintf(txt,"%.2f %%",100*(value)/256.0f);
		return true;
	case e_paraOUTmix:
		// mix
		sprintf(txt,"%.2f %% Wet",100*(value)/256.0f);
		return true;
	case e_paraLengthL:
	case e_paraLengthR:
		// buffer length
		sprintf(txt, "%.6f beats",buffindex[value]);
		return true;
		break;
	case e_paraDelayPanL:
	case e_paraDelayPanR:
	case e_paraFeedbackPanL:
	case e_paraFeedbackPanR:
		// unbalance
		if (value < 256)
		{
			sprintf(txt,"%.2f %% left",100-(100*(value/256.0f)));
			return true;
		}
		else if (value > 256)
		{
			sprintf(txt,"%.2f %% right",100-(100*(512-value)/256.0f));
			return true;
		}
		else 
		{
			sprintf(txt,"center");
			return true;
		}
		break;
	case e_paraFeedbackL:
	case e_paraFeedbackR:
		// mix
		sprintf(txt,"%.2f %%",100*(value)/256.0f);
		return true;
	case e_paraTRACKTempo:
		if (value)
		{
			sprintf(txt,"On");
		}
		else
		{
			sprintf(txt,"Off");
		}
		return true;
	}
	return false;
}
}