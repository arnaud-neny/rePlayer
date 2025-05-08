//////////////////////////////////////////////////////////////////////
//Pooplog Filter
/*
/////////////////////////////////////////////////////////////////////
Version History

v0.06b
5/19/2003
- optimizations adjusted for safety
- phaser denormals flop

v0.05b
10:50 PM 12/14/2002
- added many distortion and remapping methods
- fixed a bug no one would ever notice
- renamed overdrive to shape, as it does more than just overdrive now
- fixed lfo phase knob

v0.04b
4:26 AM 11/21/2002
- fixed denormalization
- made it sample-rate independant
- fixed inertia

v0.03b
2:00 AM 10/24/2002
- fixed right/left unbalance text

v0.02b
1:49 PM 10/14/2002
- fixed moog b filters
- added route control
- added lfo phase control
- added od gain lfo control
- added unbalance lfo control
- initial public release

v0.01b
- initial beta release

/////////////////////////////////////////////////////////////////////
	*/
#include <psycle/plugin_interface.hpp>
#include <psycle/helpers/math.hpp>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cstdio>

#include "filter.h"
namespace psycle::plugins::pooplog_filter {
using namespace psycle::plugin_interface;
using namespace psycle::helpers::math;

#define PLUGIN_NAME "Pooplog Filter "
#define PLUGIN_VERSION "0.06b"
int const IPLUGIN_VERSION =0x0006;

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

CMachineParameter const paraNULL = {" ", " ", 0, 0, MPF_NULL, 0};
CMachineParameter const paraVCFcutoff = {"Filter Cutoff", "Filter Cutoff", 0, MAX_VCF_CUTOFF, MPF_STATE, MAX_VCF_CUTOFF/2};
CMachineParameter const paraVCFresonance = {"Filter Resonance", "Filter Resonance", 1, 240, MPF_STATE, 1};
CMachineParameter const paraVCFtype = {"Filter Type", "Filter Type", 0, MAXVCFTYPE, MPF_STATE, 0};
CMachineParameter const paraVCFlfospeed = {"LFO Rate", "LFO Rate", 0, MAX_RATE, MPF_STATE, 6};
CMachineParameter const paraVCFlfoamplitude = {"Cutoff LFO Depth", "Cutoff LFO Depth", 0, MAX_VCF_CUTOFF, MPF_STATE, 0};
CMachineParameter const paraUnbalancelfoamplitude = {"Unbalance LFO Depth", "Unbalance LFO Depth", 0, 256, MPF_STATE, 0};
CMachineParameter const paraGainlfoamplitude = {"Shaper Gain LFO Depth", "Shaper Gain LFO Depth", 0, 256, MPF_STATE, 0};
CMachineParameter const paraVCFlfowave = {"LFO Wave", "LFO Wave", 0, MAXLFOWAVE-1, MPF_STATE, 0};
CMachineParameter const paraVCFlfophase = {"LFO Phase", "LFO Phase", 0, 0xffff, MPF_STATE, 0};
CMachineParameter const paraOUTmix = {"Mix", "Mix", 0, 256, MPF_STATE, 256};
CMachineParameter const paraOUToverdrive = {"Shaper Method", "Shaper Method", 0, MAXOVERDRIVEMETHOD, MPF_STATE, 0};
CMachineParameter const paraRoute = {"Routing", "Routing", 0, 1, MPF_STATE, 0};
CMachineParameter const paraInputGain = {"Input Gain", "Input Gain", 0, 1024, MPF_STATE, 256};
CMachineParameter const paraOUToverdrivegain = {"Shaper Gain/Param", "Shaper Gain/Param", 0, 256, MPF_STATE, 0};
CMachineParameter const paraInertia = {"Tweak Inertia", "Tweak Inertia", 0, 1024, MPF_STATE, 0};
CMachineParameter const paraUnbalance = {"Cutoff Unbalance", "Cutoff Unbalance", 0, 512, MPF_STATE, 256};

enum {
	e_paraVCFtype,
	e_paraVCFcutoff,
	e_paraVCFresonance,
	e_paraVCFlfoamplitude,
	e_paraVCFlfowave,
	e_paraVCFlfospeed,
	e_paraVCFlfophase,
	e_paraUnbalancelfoamplitude,
	e_paraOUToverdrive,
	e_paraOUToverdrivegain,
	e_paraUnbalance,
	e_paraGainlfoamplitude,
	e_paraInputGain,
	e_paraOUTmix,
	e_paraInertia,
	e_paraRoute
};

CMachineParameter const *pParameters[] = 
{ 
	// global
	&paraVCFtype,
	&paraVCFcutoff,
	&paraVCFresonance,
	&paraVCFlfoamplitude,
	&paraVCFlfowave,
	&paraVCFlfospeed,
	&paraVCFlfophase,
	&paraUnbalancelfoamplitude,
	&paraOUToverdrive,
	&paraOUToverdrivegain,
	&paraUnbalance,
	&paraGainlfoamplitude,
	&paraInputGain,
	&paraOUTmix,
	&paraInertia,
	&paraRoute,
};

CMachineInfo const MacInfo (
	MI_VERSION,
	IPLUGIN_VERSION,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	PLUGIN_NAME PLUGIN_VERSION,
	"Pooplog Filter",
	"Jeremy Evers",
	"About",
	4
);

struct INERTIA
{
	bool bCutoff;
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
	void NewInertiaVC(int * source, int dest);
	void DeleteInertia(INERTIA* pI);
	void UpdateInertia();

	void InitWaveTable();

	int inertia;
	struct INERTIA* pInertia;
	struct INERTIA* pLastInertia;

private:
	float * WaveTable[MAXWAVE+1];
	filter l_filter;
	filter r_filter;
	float wmix;
	float dmix;
	float InGain;
	inline void FilterTick();
	int timetocompute;
	float OutGain;
	inline float DoFilterL(float input);
	inline float DoFilterR(float input);
	inline float HandleOverdrive(float input);
	int out_mix;
	int song_sync; 
	int song_freq;
	float max_cutoff;
	int overdrive;
	int overdrivegain;
	float *pvcflfowave;
	int				vcflfowave;
	int vcflfospeed;
	int vcflfoamplitude;
	int vcfcutoff;
	int vcfresonance;
	int vcftype;
	int runbal;
	int lunbal;
	int route;
	int gainlfoamplitude;
	int unbalancelfoamplitude;
	float VcfCutoff;
	float vcflfophase;

	int trackDir;
	int playDir;
//				float trackThresh;
	float nextMin;
	int nextMinCount;
	float thisMin;
	int thisMinCount;
	float nextMax;
	int nextMaxCount;
	float thisMax;
	int thisMaxCount;
	float w1;
	float w2;
	float playPos;
};

PSYCLE__PLUGIN__INSTANTIATOR("pooplog-filter", mi, MacInfo)
//DLL_EXPORTS

mi::mi()
{
	// The constructor zone
	Vals = new int[MacInfo.numParameters];
	InitWaveTable();
	pvcflfowave=SourceWaveTable[0];
	pLastInertia = pInertia = NULL;
}

mi::~mi()
{
	delete[] Vals;
// Destroy dinamically allocated objects/memory here
	INERTIA* pI = pInertia;
	while (pI)
	{
		INERTIA* pI2 = pI->next;
		delete pI;
		pI = pI2;
	}
	pLastInertia = pInertia = NULL;
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
	l_filter.init(song_freq);
	r_filter.init(song_freq);
	inertia = paraInertia.DefValue;
	pLastInertia = pInertia = NULL;
	timetocompute = 1;
	OutGain = 1;
	vcflfoamplitude=0;
	gainlfoamplitude=0;
	unbalancelfoamplitude=0;
	vcflfophase=0;
	runbal = 1;
	lunbal = 1;
	route = 0;

	trackDir = 1;
	playDir = 1;
	nextMin = 0;
	thisMin = 0;
	nextMax = 0;
	thisMax = 0;
	/// Changed from 0 to 1 to avoid a "division by zero" problem
	/// in HandleOverdrive w1 = SAMPLE_LENGTH/(thisMinCount+thisMaxCount-w2);
	/// I don't know what the real solution would be
	nextMinCount = 1;
	thisMinCount = 1;
	/// Changed from 0 to 1 to avoid a "division by zero" problem
	/// in HandleOverdrive() { w1 = SAMPLE_LENGTH/(thisMinCount+thisMaxCount-w2); }
	/// I don't know what the real solution would be
	nextMaxCount = 1;
	thisMaxCount = 1;
	w1 = 1;
	w2 = 1;
	playPos = 0;
//				trackThresh = 0;
}

inline void mi::FilterTick()
{
	timetocompute=FILTER_CALC_TIME-1;
	// lfo
	if (vcflfospeed <= MAXSYNCMODES)
	{
		vcflfophase += SyncAdd[vcflfospeed];
	}
	else
	{
		vcflfophase += ((vcflfospeed-MAXSYNCMODES)*(vcflfospeed-MAXSYNCMODES))*(0.000030517f*44100.0f/song_freq);
	}
	WRAP_AROUND(vcflfophase);
	Vals[e_paraVCFlfophase] = rint<int>(vcflfophase/(SAMPLE_LENGTH*2/65536.0f));
	// vcf
	if (vcftype)
	{
		float outcutoff = VcfCutoff;
		int flunbal = lunbal;
		int frunbal = runbal;

		if (unbalancelfoamplitude)
		{
			int val = rint<int>((pvcflfowave[rint<int>(vcflfophase)])*(unbalancelfoamplitude))+256;
			// unbalance
			if (val <= 256)
			{
				int t;
				// left
				t = 512-(256-val);
				t *= t;
				frunbal = (t+runbal)/2;
				flunbal = (lunbal+(512*512))/2;
			}
			else
			{
				// right
				int t;
				t = 512-(val-256);
				t *= t;
				flunbal = (t+lunbal)/2;
				frunbal = (runbal+(512*512))/2;
			}
		}
		if (vcflfoamplitude)
		{
			outcutoff+= (float(pvcflfowave[rint<int>(vcflfophase)])
							*float(vcflfoamplitude));
		}
		if (outcutoff<0) outcutoff = 0;
		else if (outcutoff>max_cutoff) outcutoff=max_cutoff;

		if (vcftype > MAXFILTER)
		{
			l_filter.setphaser(vcftype-MAXFILTER,outcutoff*(flunbal/(512.0f*512.0f)),vcfresonance);
			r_filter.setphaser(vcftype-MAXFILTER,outcutoff*(frunbal/(512.0f*512.0f)),vcfresonance);
		}
		else if (vcftype > MAXMOOG)
		{
			l_filter.setfilter((vcftype-MAXMOOG-1)/2,outcutoff*(flunbal/(512.0f*512.0f)),vcfresonance);
			r_filter.setfilter((vcftype-MAXMOOG-1)/2,outcutoff*(frunbal/(512.0f*512.0f)),vcfresonance);
		}
		else if (vcftype & 1)
		{
			l_filter.setmoog1((vcftype-1)/2,outcutoff*(flunbal/(512.0f*512.0f)),vcfresonance);
			r_filter.setmoog1((vcftype-1)/2,outcutoff*(frunbal/(512.0f*512.0f)),vcfresonance);
		}
		else
		{
			l_filter.setmoog2((vcftype-1)/2,outcutoff*(flunbal/(512.0f*512.0f)),vcfresonance);
			r_filter.setmoog2((vcftype-1)/2,outcutoff*(frunbal/(512.0f*512.0f)),vcfresonance);
		}
	}
	// overdrive
	if (overdrive)
	{
		if (gainlfoamplitude)
		{
			float p = overdrivegain+(float(pvcflfowave[rint<int>(vcflfophase)])
							*float(gainlfoamplitude));
			if (p < 0) 
			{
				p = 0;
			}
			else if (p > 256)
			{
				p = 256;
			}
			OutGain = 1.0f+(p*p/OVERDRIVEDIVISOR);
		}
		else
		{
			OutGain = 1.0f+(overdrivegain*overdrivegain/OVERDRIVEDIVISOR);
		}
		// gain adsr
		if (OutGain < 1.0f) OutGain = 1.0f;
		else if (OutGain > 65536/OVERDRIVEDIVISOR) OutGain = 65536/OVERDRIVEDIVISOR;
	}
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
		song_sync = pCB->GetBPM();
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
		l_filter.init(song_freq);
		r_filter.init(song_freq);
	}

	else if (song_sync != pCB->GetBPM())  
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

void mi::NewInertiaVC(int * source, int dest)
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
		VcfCutoff=float(dest);
	}
}


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
				if (pI->bCutoff)
				{
					VcfCutoff=float(*pI->source);
				}
				INERTIA* pI2 = pI->next;
				DeleteInertia(pI);
				pI=pI2;
			}
			else if ((pI->add < 0) && (pI->current < pI->dest))
			{
			*(pI->source) = pI->dest;
				if (pI->bCutoff)
				{
					VcfCutoff=float(*pI->source);
				}
				INERTIA* pI2 = pI->next;
				DeleteInertia(pI);
				pI=pI2;
			}
			else if (pI->current == pI->dest)
			{
				*(pI->source) = pI->dest;
				if (pI->bCutoff)
				{
					VcfCutoff=float(*pI->source);
				}
				INERTIA* pI2 = pI->next;
				DeleteInertia(pI);
				pI=pI2;
			}
			else 
			{
				*pI->source = rint<int>(pI->current);
				if (pI->bCutoff)
				{
					VcfCutoff=pI->current;
				}
				pI = pI->next;
			}
		}
	}
	else
	{
		while (pI)
		{
			*(pI->source) = pI->dest;
			if (pI->bCutoff)
			{
				VcfCutoff=float(*pI->source);
			}
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
		case e_paraVCFcutoff:												NewInertiaVC(&vcfcutoff, val); break;
		case e_paraVCFresonance:								NewInertia(&vcfresonance, val); break;
		case e_paraVCFlfoamplitude:								NewInertia(&vcflfoamplitude, val); break;
		case e_paraGainlfoamplitude:								NewInertia(&gainlfoamplitude, val); break;
		case e_paraUnbalancelfoamplitude:								NewInertia(&unbalancelfoamplitude, val); break;
		case e_paraVCFlfospeed:												NewInertia(&vcflfospeed, val); break;
		case e_paraVCFlfowave:												pvcflfowave=SourceWaveTable[val%MAXLFOWAVE]; vcflfowave=val%MAXLFOWAVE; break;
		case e_paraVCFtype: 
			if (vcftype!=val)
			{
				vcftype=val; 
				l_filter.reset();
				r_filter.reset();
				float outcutoff = VcfCutoff;
				if (vcflfoamplitude)
				{
					outcutoff+= (float(pvcflfowave[rint<int>(vcflfophase)])
									*float(vcflfoamplitude));
				}
				if (outcutoff<0) outcutoff = 0;
				else if (outcutoff>max_cutoff) outcutoff=max_cutoff;

				if (vcftype > MAXFILTER)
				{
					l_filter.setphaser(vcftype-MAXFILTER,outcutoff*(lunbal/(512.0f*512.0f)),vcfresonance);
					r_filter.setphaser(vcftype-MAXFILTER,outcutoff*(runbal/(512.0f*512.0f)),vcfresonance);
				}
				else if (vcftype > MAXMOOG)
				{
					l_filter.setfilter((vcftype-MAXMOOG-1)/2,outcutoff*(lunbal/(512.0f*512.0f)),vcfresonance);
					r_filter.setfilter((vcftype-MAXMOOG-1)/2,outcutoff*(runbal/(512.0f*512.0f)),vcfresonance);
				}
				else if (vcftype & 1)
				{
					l_filter.setmoog1((vcftype-1)/2,outcutoff*(lunbal/(512.0f*512.0f)),vcfresonance);
					r_filter.setmoog1((vcftype-1)/2,outcutoff*(runbal/(512.0f*512.0f)),vcfresonance);
				}
				else
				{
					l_filter.setmoog2((vcftype-1)/2,outcutoff*(lunbal/(512.0f*512.0f)),vcfresonance);
					r_filter.setmoog2((vcftype-1)/2,outcutoff*(runbal/(512.0f*512.0f)),vcfresonance);
				}
			}
			break;
		case e_paraOUToverdrive:								overdrive=val; break;
		case e_paraOUToverdrivegain:				NewInertia(&overdrivegain, val); break;
		case e_paraOUTmix:
			// mix
			wmix = val/256.0f;
			dmix = 1.0f-wmix;
			break;
		case e_paraUnbalance:
			// unbalance
			if (val <= 256)
			{
				int t;
				// left
				t = 512-(256-val);
				t *= t;
				NewInertia(&runbal, t);
				NewInertia(&lunbal, 512*512);
			}
			else
			{
				// right
				int t;
				t = 512-(val-256);
				t *= t;
				NewInertia(&lunbal, t);
				NewInertia(&runbal, 512*512);
			}
			break;
		case e_paraRoute:
			route = val;
			break;
		case e_paraVCFlfophase:
			vcflfophase = val * (SAMPLE_LENGTH*2/65536.0f);
			break;
		case e_paraInertia:
			inertia = val;
			break;
	}
}

inline float mi::DoFilterL(float input)
{
	if (vcftype > MAXFILTER)
	{
		return l_filter.phaser(input);
	}
	else if (vcftype > MAXMOOG)
	{
		if (vcftype & 1) return l_filter.res(input);
		return l_filter.res2(input);
	}
	else if (vcftype)
	{
		if (vcftype & 1) return l_filter.moog1(input);
		return l_filter.moog2(input);
	}
	return input;
}

inline float mi::DoFilterR(float input)
{
	if (vcftype > MAXFILTER)
	{
		return r_filter.phaser(input);
	}
	else if (vcftype > MAXMOOG)
	{
		if (vcftype & 1) return r_filter.res(input);
		return r_filter.res2(input);
	}
	else if (vcftype)
	{
		if (vcftype & 1) return r_filter.moog1(input);
		return r_filter.moog2(input);
	}
	return input;
}

inline float mi::HandleOverdrive(float input)
{
#define MAXOVERDRIVEAMP 0.9999f
	// handle Overdrive
	switch (overdrive)
	{
	case 1: // soft clip 1
		return (atanf(input*OutGain)/(float)PI);
		break;
	case 2: // soft clip 2
		input *= OutGain;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								return -1.0f;
		else if (input > 1.0f)				return 1.0f;
		return input;
	case 3: // soft clip 3
		input *= OutGain;
		if (input < -0.75f) return -0.75f + ((input+0.75f)/(1.0f+(powf(2,(input-0.75f)/(1.0f-0.75f)))));
		else if (input < 0.75f) return input;
		else return 0.75f + ((input-0.75f)/(1.0f+(powf(2,(input-0.75f)/(1.0f-0.75f)))));
		break;
	case 4: // hard clip 1
		input *= OutGain;
		if (input < -1.0f)								return -1.0f;
		else if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 5: // hard clip 2
		// bounce off limits
		input *= OutGain;
		if (input < -1.0f)								return input-(rint<int>(input)+1)*(input+1.0f);
		else if (input > 1.0f)				return input-(rint<int>(input)+1)*(input-1.0f);
		return input;
		break;
	case 6: // hard clip 3
		// invert, this one is harsh
		input *= OutGain;
		if (input < -1.0f)								return input + (rint<int>(input/(2.0f)))*(2.0f);
		else if (input > 1.0f)				return input - (rint<int>(input/(2.0f)))*(2.0f);
		return input;
		break;
	case 7: // parabolic distortion
		input = (((input * input)*(OutGain*OutGain)))-1.0f;
		if (input > 1.0f)				return 1.0f;
//												return input*MAXOVERDRIVEAMP*(128.0f/OutGain);
		return input;
		break;
	case 8: // parabolic distortion
		input = (((input * input)*(OutGain*OutGain))*32.0f)-1.0f;
		if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 9: // sin remapper
		return SourceWaveTable[0][rint<int>(input*OutGain*SAMPLE_LENGTH*2)&((SAMPLE_LENGTH*2)-1)];
		break;
	case 10: //
		// good negative partial rectifier
		input *= OutGain;
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
		input *= OutGain;
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
		input = fabsf(input*OutGain);//-1.0f;
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
		input = -fabsf(input*OutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								return -1.0f;
		return input;
		break;
	case 14:
		// good negative full rectifier
		input = -fabsf(input*OutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input < -1.0f)								return -1.0f;
		return input;
		break;

	case 15:
		// good positive full rectifier
		input = fabsf(input*OutGain);//-1.0f;
		input = 1.5f*input/(1.0f+(0.5f*fabsf(input)));
		if (input > 1.0f)				return 1.0f;
		return input;
		break;
	case 16:
		// assymetrical fuzz distortion 1
		if (input < 0.0f)
		{
			return (atanf(input*OutGain*0.5f)/(float)PI);
		}
		else 
		{
			input *= OutGain*4;
			if (input > 1.0f)				return 1.0f;
		}
		return input;
		break;
	case 17:
		// assymetrical fuzz distortion 2
		if (input > 0.0f)
		{
			return (atanf(input*OutGain*0.25f)/(float)PI);
		}
		else 
		{
			input *= OutGain*8;
			if (input < -1.0f)				return -1.0f;
		}
		return input;
		break;
	case 18:
		// assymetrical fuzz distortion 3 
		input *= OutGain;
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
		input *= OutGain;
		if (input < -0.5f) return -0.25f + ((input+0.5f)/(1.0f+(powf(2,(input-0.5f)/(1.0f-0.5f)))));
		else if (input < 0.0f) return input*0.75f;
		input*=2;
		if (input > 0.5f)				return 0.5f;
		break;
	case 20:
		// assymetrical fuzz distortion 1 with hard clip
		if (input < 0.0f)
		{
			input = (atanf(input*OutGain*0.5f)/(float)PI);
			if (input < -0.75f)
			{
				return -0.75f;
			}
		}
		else 
		{
			input *= OutGain*4;
			if (input > 1.0f)				return 1.0f;
		}
		return input;
		break;
	case 21:
		// assymetrical fuzz distortion 2 with full rectifier
		if (input > 0.0f)
		{
			return -(atanf(input*OutGain*0.25f)/(float)PI);
		}
		else 
		{
			input *= OutGain*8;
			if (input < -1.0f)				return -1.0f;
		}
		return input;
		break;
	case 22:
		// assymetrical fuzz distortion 3 with partial rectifier
		input *= OutGain;
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
		input *= OutGain;
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
		if (trackDir < 0)
		{
			// we are tracking negative
//												if (input < trackThresh)
			if (input < 0)
			{
				// track this sample
				if (input < nextMin)
				{
					nextMin = input;
				}
				nextMinCount++;
			}
			else
			{
				thisMin = nextMin;
				thisMinCount = nextMinCount;
				nextMin = 0;
				nextMinCount = 1;
				trackDir = 1;
				// calculate the speeds
				if (overdrivegain < 128-2)
				{
					w1 = (thisMaxCount*(overdrivegain+1)/126.0f);
					w2 = SAMPLE_LENGTH/(thisMinCount+thisMaxCount-w1);
					w1 = SAMPLE_LENGTH/w1;
				}
				else if (overdrivegain > 128+2)
				{
					w2 = (thisMinCount*(255-overdrivegain)/126.0f);
					w1 = SAMPLE_LENGTH/(thisMinCount+thisMaxCount-w2);
					w2 = SAMPLE_LENGTH/w2;
				}
				else 
				{
					w1 = SAMPLE_LENGTH/float(thisMaxCount);
					w2 = SAMPLE_LENGTH/float(thisMinCount);
				}
				if (w1 < 0.00001f)
				{
					w1 = 0.00001f;
				}
				if (w2 < 0.00001f)
				{
					w2 = 0.00001f;
				}
//																trackThresh = (trackThresh+thisMin+((thisMax-thisMin)/2))/2;
			}
		}
		else 
		{
			// we are tracking positive
//												if (input > trackThresh)
			if (input > 0)
			{
				// track this sample
				if (input > nextMax)
				{
					nextMax = input;
				}
				nextMaxCount++;
			}
			else
			{
				thisMax = nextMax;
				thisMaxCount = nextMaxCount;
				nextMax = 0;
				nextMaxCount = 1;
				trackDir = -1;
				// calculate the speeds
				if (overdrivegain < 128)
				{
					w1 = (thisMaxCount*(overdrivegain+1)/128.0f);
					w2 = SAMPLE_LENGTH/(thisMinCount+thisMaxCount-w1);
					w1 = SAMPLE_LENGTH/w1;
				}
				else if (overdrivegain > 128)
				{
					w2 = (thisMinCount*(255-overdrivegain)/128.0f);
					w1 = SAMPLE_LENGTH/(thisMinCount+thisMaxCount-w2);
					w2 = SAMPLE_LENGTH/w2;
				}
				else 
				{
					w1 = SAMPLE_LENGTH/float(thisMaxCount);
					w2 = SAMPLE_LENGTH/float(thisMinCount);
				}
				if (w1 < 0.00001f)
				{
					w1 = 0.00001f;
				}
				if (w2 < 0.00001f)
				{
					w2 = 0.00001f;
				}
//																trackThresh = (trackThresh+thisMin+((thisMax-thisMin)/2))/2;
			}
		}
		// now we just play a waveform
		if (playDir<0)
		{
			playPos += w2;
		}
		else
		{
			playPos += w1;
		}
		if (playPos < 0)
		{
			playPos = 0;
		}
		if (playPos >= SAMPLE_LENGTH)
		{
			int i = rint<int>(playPos/SAMPLE_LENGTH);
			if (i & 1)
			{
				playDir = -playDir;
			}
			playPos = (playPos-rint<int>(playPos))+(rint<int>(playPos)&((SAMPLE_LENGTH)-1));
		}
		switch (overdrive)
		{
		case SYNTH_REMAP_0: 
			if (playDir<0)
			{
				return SourceWaveTable[0][rint<int>(playPos)+SAMPLE_LENGTH]*thisMin*2;
			}
			else
			{
				return SourceWaveTable[0][rint<int>(playPos)+SAMPLE_LENGTH]*thisMax*2;
			}
			break;
		case SYNTH_REMAP_0+1: 
			if (playDir<0)
			{
				return SourceWaveTable[1][rint<int>(playPos)+SAMPLE_LENGTH]*thisMin*2;
			}
			else
			{
				return SourceWaveTable[1][rint<int>(playPos)+SAMPLE_LENGTH]*thisMax*2;
			}
			break;
		case SYNTH_REMAP_0+2: 
			if (playDir<0)
			{
				return SourceWaveTable[10][rint<int>(playPos)+SAMPLE_LENGTH]*thisMin*2;
			}
			else
			{
				return SourceWaveTable[10][rint<int>(playPos)+SAMPLE_LENGTH]*thisMax*2;
			}
			break;
		case SYNTH_REMAP_0+3: 
			if (playDir<0)
			{
				return SourceWaveTable[4][rint<int>(playPos)+SAMPLE_LENGTH]*thisMin*2;
			}
			else
			{
				return SourceWaveTable[4][rint<int>(playPos)+SAMPLE_LENGTH]*thisMax*2;
			}
			break;
		case SYNTH_REMAP_0+4: 
			if (playDir<0)
			{
				return SourceWaveTable[0][rint<int>(playPos)+SAMPLE_LENGTH]*thisMin*2;
			}
			else
			{
				return SourceWaveTable[10][rint<int>(playPos)+SAMPLE_LENGTH]*thisMax*2;
			}
			break;
		case SYNTH_REMAP_0+5: 
			if (playDir<0)
			{
				return SourceWaveTable[1][rint<int>(playPos)+SAMPLE_LENGTH]*thisMin*2;
			}
			else
			{
				return SourceWaveTable[10][rint<int>(playPos)+SAMPLE_LENGTH]*thisMax*2;
			}
			break;
		case SYNTH_REMAP_0+6: 
			if (playDir<0)
			{
				return SourceWaveTable[1][rint<int>(playPos)+SAMPLE_LENGTH]*thisMin*2;
			}
			else
			{
				return SourceWaveTable[4][rint<int>(playPos)+SAMPLE_LENGTH]*thisMax*2;
			}
			break;
		case SYNTH_REMAP_0+7: 
			if (playDir<0)
			{
				return SourceWaveTable[10][rint<int>(playPos)+SAMPLE_LENGTH]*thisMin*2;
			}
			else
			{
				return SourceWaveTable[4][rint<int>(playPos)+SAMPLE_LENGTH]*thisMax*2;
			}
			break;
		}
	}
	return input;
}

// Work... where all is cooked 
void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks)
{
	UpdateInertia();
	if (route)
	{
		do
		{
			if(!timetocompute--) FilterTick();

			float sl = *psamplesleft*InGain;
			float sr = *psamplesright*InGain;
			float sol;
			float sor;

			sol = DoFilterL(HandleOverdrive(sl/32768.0f))*32768.0f;
			sor = DoFilterR(HandleOverdrive(sr/32768.0f))*32768.0f;

			*psamplesleft=(sl*dmix)+(sol*wmix);
			*psamplesright=(sr*dmix)+(sor*wmix);

			++psamplesleft;
			++psamplesright;
		} while(--numsamples);
	}
	else
	{
		do
		{
			if(!timetocompute--) FilterTick();

			float sl = *psamplesleft*InGain;
			float sr = *psamplesright*InGain;
			float sol;
			float sor;

			sol = HandleOverdrive(DoFilterL(sl/32768.0f))*32768.0f;
			sor = HandleOverdrive(DoFilterR(sr/32768.0f))*32768.0f;

			*psamplesleft=(sl*dmix)+(sol*wmix);
			*psamplesright=(sr*dmix)+(sor*wmix);

			++psamplesleft;
			++psamplesright;
		} while(--numsamples);
	}
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
	case e_paraVCFlfospeed:
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
	case e_paraVCFlfowave:
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
	case e_paraVCFtype:
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
	case e_paraVCFcutoff:
		// cutoff
		// vcf type
		if(vcftype)
		{
			if ((vcftype > MAXFILTER) || (vcftype < MAXMOOG))
			{
				fv=CUTOFFCONV(value);
				sprintf(txt,"%.2f hz",fv);
				return true;
				break;
			}
			switch ((vcftype-MAXMOOG-1)/2)
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
	case e_paraVCFresonance:
		// resonance
		sprintf(txt,"%.4f%%",(float)(value-1)*0.418410042f);
		return true;
		break;
	case e_paraVCFlfoamplitude:
		// vcf lfo amplitude
		if ( value == 0 )
		{
			sprintf(txt,"Off");
			return true;
		}
		sprintf(txt,"%.4f%%",(float)value*(100.0f/MAX_VCF_CUTOFF));
		return true;
		break;

	case e_paraOUToverdrive:
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
	case e_paraOUToverdrivegain:
		if (overdrive < SYNTH_REMAP_0)
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
	case e_paraRoute:
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
	case e_paraVCFlfophase:
		sprintf(txt,"%.2f degrees",value * (360/65536.0f));
		return true;
		break;

	case e_paraUnbalancelfoamplitude:
	case e_paraGainlfoamplitude:
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
	}
	return false;
}
}