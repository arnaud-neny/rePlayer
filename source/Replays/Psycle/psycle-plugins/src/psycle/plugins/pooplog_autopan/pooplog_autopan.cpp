//////////////////////////////////////////////////////////////////////
// pooplog autopan
/////////////////////////////////////////////////////////////////////
/*
Version History

v0.06b
- optimizations

v0.05b
10:55 PM 12/14/2002
- fixed lfo phase knob

v0.04b
1:28 PM 11/21/2002
- works correctly at any sample rate
- fixed inertia

v0.03b
6:35 PM 10/23/2002
- fixed possible crosstalk issue

v0.02b
1:49 PM 10/14/2002
- initial public release

v0.01b
- initial beta release
*/
/////////////////////////////////////////////////////////////////////

#include <psycle/plugin_interface.hpp>
#include <psycle/helpers/math.hpp>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cstdio>
namespace psycle::plugins::pooplog_autopan {
using namespace psycle::plugin_interface;
using namespace psycle::helpers::math;

#define PLUGIN_NAME "Pooplog Autopan 0.06b"

#define FILEVERSION 2
#define MAXSYNCMODES 16
#define FILTER_CALC_TIME				1
#define MAXLFOWAVE 15
#define TWOPI																6.28318530717958647692528676655901f
#define SAMPLE_LENGTH  4096
#define MAXPHASEMIX 9
#define MAXENVTYPE 2
#define MAX_RATE								8192
#define MAXWAVE 17
#define WRAP_AROUND(x) if ((x < 0) || (x >= SAMPLE_LENGTH*2)) x = (x-rint<int>(x))+(rint<int>(x)&((SAMPLE_LENGTH*2)-1));
#define PI 3.14159265358979323846

float SyncAdd[MAXSYNCMODES+1];
float SourceWaveTable[MAXLFOWAVE+1][(SAMPLE_LENGTH*2)+256];

CMachineParameter const paraNULL = {" ", " ", 0, 0, MPF_NULL, 0};
CMachineParameter const paraVCFlfospeed = {"LFO Rate", "LFO Rate", 0, MAX_RATE, MPF_STATE, 6};
CMachineParameter const paraPanlfoamplitude = {"Pan LFO Depth", "Pan LFO Depth", 0, 256, MPF_STATE, 0};
CMachineParameter const paraSmoothing = {"Delta Smoothing", "Delta Smoothing", 0, 256, MPF_STATE, 0};
CMachineParameter const paraVCFlfowave = {"LFO Wave", "LFO Wave", 0, MAXLFOWAVE-1, MPF_STATE, 0};
CMachineParameter const paraVCFlfophase = {"LFO Phase", "LFO Phase", 0, 0xffff, MPF_STATE, 0};
CMachineParameter const paraOUTmix = {"Mix", "Mix", 0, 256, MPF_STATE, 256};
CMachineParameter const paraInputGain = {"Input Gain", "Input Gain", 0, 1024, MPF_STATE, 256};
CMachineParameter const paraInertia = {"Tweak Inertia", "Tweak Inertia", 0,	1024, MPF_STATE, 0};
CMachineParameter const paraPan = {"Panning", "Panning", 0, 512, MPF_STATE, 256};

enum {
	e_paraPan,
	e_paraPanlfoamplitude,
	e_paraSmoothing,
	e_paraVCFlfowave,
	e_paraVCFlfospeed,
	e_paraVCFlfophase,
	e_paraInertia,
	e_paraInputGain,
	e_paraOUTmix
};

CMachineParameter const *pParameters[] = 
{ 
	// global
	&paraPan,
	&paraPanlfoamplitude,
	&paraSmoothing,
	&paraVCFlfowave,
	&paraVCFlfospeed,
	&paraVCFlfophase,
	&paraInertia,
	&paraInputGain,
	&paraOUTmix,
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0006,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	PLUGIN_NAME,
	"Pooplog Autopan",
	"Jeremy Evers",
	"About",
	3
);

struct INERTIA
{
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
	virtual void Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks);
	virtual void SequencerTick();
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
	float wmix;
	float dmix;
	float InGain;
	inline void FilterTick();
	int out_mix;
	unsigned int interpolate;
	int song_sync; 
	int song_freq;
	float *pvcflfowave;
	int				vcflfowave;
	int vcflfospeed;
	int panlfoamplitude;
	float rmix;
	float lmix;
	int pan;
	int smoothing;
	float vcflfophase;
	int oldpan;
};

PSYCLE__PLUGIN__INSTANTIATOR("pooplog-autopan", mi, MacInfo)
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
	inertia = paraInertia.DefValue;
	pLastInertia = pInertia = NULL;
	panlfoamplitude=0;
	vcflfophase=0;
	rmix = 1;
	lmix = 1;
}

inline void mi::FilterTick()
{
	// lfo
	if (vcflfospeed <= MAXSYNCMODES)
	{
		vcflfophase += SyncAdd[vcflfospeed];
	}
	else
	{
		vcflfophase += ((vcflfospeed-MAXSYNCMODES)*(vcflfospeed-MAXSYNCMODES))*0.000030517f*44100.f/song_freq;
	}
	WRAP_AROUND(vcflfophase);
	Vals[e_paraVCFlfophase] = rint<int>(vcflfophase/(SAMPLE_LENGTH*2/65536.0f));
	// vcf
	int newpan = pan;

	if (panlfoamplitude)
	{
		oldpan = pan;
		newpan += rint<int>((pvcflfowave[rint<int>(vcflfophase)])*(panlfoamplitude));

		if (newpan < 0)
		{
			newpan = 0;
		}
		else if (newpan > 512)
		{
			newpan = 512;
		}

		if (newpan > oldpan + smoothing)
		{
			newpan = oldpan + smoothing;
		}
		else if (newpan < oldpan - smoothing)
		{
			newpan = oldpan - smoothing;
		}
		oldpan = newpan;
	}
		// unbalance
	if (newpan <= 256)
	{
		// left
		rmix = newpan/256.0f;
		lmix = 1;
	}
	else
	{
		// right
		lmix = (512-newpan)/256.0f;
		rmix = 1;
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
	}
	else
	{
		*source = dest;
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
				INERTIA* pI2 = pI->next;
				DeleteInertia(pI);
				pI=pI2;
			}
			else if ((pI->add < 0) && (pI->current < pI->dest))
			{
			*(pI->source) = pI->dest;
				INERTIA* pI2 = pI->next;
				DeleteInertia(pI);
				pI=pI2;
			}
			else if (pI->current == pI->dest)
			{
				*(pI->source) = pI->dest;
				INERTIA* pI2 = pI->next;
				DeleteInertia(pI);
				pI=pI2;
			}
			else 
			{
				*pI->source = rint<int>(pI->current);
				pI = pI->next;
			}
		}
	}
	else
	{
		while (pI)
		{
			*(pI->source) = pI->dest;
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
		case e_paraPanlfoamplitude:								NewInertia(&panlfoamplitude, val); break;
		case e_paraVCFlfospeed:												NewInertia(&vcflfospeed, val); break;
		case e_paraVCFlfowave:												pvcflfowave=SourceWaveTable[val%MAXLFOWAVE]; vcflfowave=val%MAXLFOWAVE; break;
		case e_paraOUTmix:
			// mix
			wmix = val/256.0f;
			dmix = 1.0f-wmix;
			break;
		case e_paraPan:								NewInertia(&pan, val); break;
		case e_paraVCFlfophase:
			vcflfophase = val * (SAMPLE_LENGTH*2/65536.0f);
			break;
		case e_paraSmoothing:																NewInertia(&smoothing, (257-val)*2); break;
		case e_paraInertia:				inertia = val; break;
	}
}

void mi::SequencerTick()
{
// Called on each tick while sequencer is playing
	if (song_freq != pCB->GetSamplingRate())  
	{
		song_freq = pCB->GetSamplingRate();
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


// Work... where all is cooked 
void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks)
{
	UpdateInertia();
	do
	{
		FilterTick();

		float sl = *psamplesleft*InGain;
		float sr = *psamplesright*InGain;
		float sol = sl*lmix;
		float sor = sr*rmix;

		*psamplesleft=(sl*dmix)+(sol*wmix);
		*psamplesright=(sr*dmix)+(sor*wmix);

		++psamplesleft;
		++psamplesright;
	} while(--numsamples);
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
	float fv;
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
	case e_paraPan:
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
	case e_paraVCFlfophase:
		sprintf(txt,"%.2f degrees",value * (360/65536.0f));
		return true;
		break;

	case e_paraPanlfoamplitude:
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
	case e_paraSmoothing:
		sprintf(txt,"%.2f %%",100*(value)/256.0f);
		return true;
	}
	return false;
}
}