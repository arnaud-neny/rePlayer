//////////////////////////////////////////////////////////////////////
/* pooplog lofi
/////////////////////////////////////////////////////////////////////
Version History

v0.04b
- optimizations adjusted for safety

v0.03b
1:52 PM 11/21/2002
- fixed to work at any sample rate

v0.02b
6:33 PM 10/23/2002
- fixed crosstalk issue
- added input gain knob

v0.01b
- initial release

/////////////////////////////////////////////////////////////////////
	*/
#include <psycle/plugin_interface.hpp>
#include <psycle/helpers/math.hpp>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cstdio>
namespace psycle::plugins::pooplog_lofi {
using namespace psycle::plugin_interface;
using namespace psycle::helpers::math;

#define PLUGIN_NAME "Pooplog Lofi Processor 0.04b"


CMachineParameter const paraFrequency = {"Resample Frequency", "Resample Frequency", 0, 4096, MPF_STATE, 0};
CMachineParameter const paraBits = {"Resample Bits", "Resample Bits", 0, 32, MPF_STATE, 0};
CMachineParameter const paraInputGain = {"Input Gain", "Input Gain", 0, 1408, MPF_STATE, 256};
CMachineParameter const paraUnbalance = {"Frequency Unbalance", "Frequency Unbalance", 0, 512, MPF_STATE, 256};

enum
{ 
	// global
	e_paraFrequency,
	e_paraBits,
	e_paraUnbalance,
	e_paraInputGain
};


CMachineParameter const *pParameters[] = 
{ 
	// global
	&paraFrequency,
	&paraBits,
	&paraUnbalance,
	&paraInputGain,
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0004,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	PLUGIN_NAME,
	"Pooplog Lofi",
	"Jeremy Evers",
	"About",
	4
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

private:
	float InGain;
	float lincrement;
	float lcounter;
	float rincrement;
	float rcounter;
	float totalleft;
	float totalright;
	int lsamplesavg;
	int rsamplesavg;
	float lastleft;
	float lastright;
	int song_freq;
};

PSYCLE__PLUGIN__INSTANTIATOR("pooplog-lofi-processor", mi, MacInfo)
//DLL_EXPORTS

mi::mi()
{
	// The constructor zone
	Vals = new int[MacInfo.numParameters];
}

mi::~mi()
{
	delete[] Vals;
// Destroy dinamically allocated objects/memory here
}

void mi::Init()
{
// Initialize your stuff here
	Vals[e_paraFrequency] = 0;
	Vals[e_paraBits] = 0;
	lcounter=0;
	rcounter=0;
	totalleft=0;
	totalright=0;
	lsamplesavg=0;
	rsamplesavg=0;
	lastleft=0;
	lastright=0;
	song_freq = pCB->GetSamplingRate();
}

void mi::SequencerTick()
{
// Called on each tick while sequencer is playing
	if (song_freq != pCB->GetSamplingRate())
	{
		song_freq = pCB->GetSamplingRate();

		float lunbal = 1.0f;
		float runbal = 1.0f;
		if (Vals[e_paraUnbalance] <= 256)
		{
			// left
			runbal = ((512-(256-Vals[e_paraUnbalance]))*(512-(256-Vals[e_paraUnbalance])))/(512.0f*512.0f);
		}
		else
		{
			// right
			lunbal = ((512-(Vals[e_paraUnbalance]-256))*(512-(Vals[e_paraUnbalance]-256)))/(512.0f*512.0f);
		}
		lincrement = (((4096*4096)-(Vals[e_paraFrequency]*Vals[e_paraFrequency]))/(4096*4096.0f))*lunbal*lunbal*44100.0f/song_freq;
		rincrement = (((4096*4096)-(Vals[e_paraFrequency]*Vals[e_paraFrequency]))/(4096*4096.0f))*runbal*runbal*44100.0f/song_freq;
		if (lincrement > 1.0f)
		{
			lincrement = 1.0f;
		}
		if (rincrement > 1.0f)
		{
			rincrement = 1.0f;
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

void mi::ParameterTweak(int par, int val)
{
	Vals[par]=val;
	switch(par)
	{
	case e_paraFrequency:
	case e_paraUnbalance:
		// unbalance
		{
			float lunbal = 1.0f;
			float runbal = 1.0f;
			if (Vals[e_paraUnbalance] <= 256)
			{
				// left
				runbal = ((512-(256-Vals[e_paraUnbalance]))*(512-(256-Vals[e_paraUnbalance])))/(512.0f*512.0f);
			}
			else
			{
				// right
				lunbal = ((512-(Vals[e_paraUnbalance]-256))*(512-(Vals[e_paraUnbalance]-256)))/(512.0f*512.0f);
			}
			lincrement = (((4096*4096)-(Vals[e_paraFrequency]*Vals[e_paraFrequency]))/(4096*4096.0f))*lunbal*lunbal*44100.0f/song_freq;
			rincrement = (((4096*4096)-(Vals[e_paraFrequency]*Vals[e_paraFrequency]))/(4096*4096.0f))*runbal*runbal*44100.0f/song_freq;
			if (lincrement > 1.0f)
			{
				lincrement = 1.0f;
			}
			if (rincrement > 1.0f)
			{
				rincrement = 1.0f;
			}
			break;
		}
	case e_paraInputGain:
		InGain = (val/256.0f)*(val/256.0f)*(val/256.0f);
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

	if (Vals[e_paraFrequency])
	{
		// do resample
		totalleft += sl;
		lsamplesavg++;
		lcounter+=lincrement;
		if (lcounter >= 1.0f)
		{
			lcounter-=1;
			lastleft=totalleft/lsamplesavg;
			totalleft = 0;
			lsamplesavg = 0;
		}
		sl = lastleft;

		// do resample
		totalright += sr;
		rsamplesavg++;
		rcounter+=rincrement;
		if (rcounter >= 1.0f)
		{
			rcounter-=1;
			lastright=totalright/rsamplesavg;
			totalright = 0;
			rsamplesavg = 0;
		}
		sr = lastright;
	}

	if (Vals[e_paraBits]==32)
	{
		if (sl > 0)
		{
			sl = 12288.0f; 
		}
		else
		{
			sl = -12288.0f; 
		}
		if (sr > 0)
		{
			sr = 12288.0f; 
		}
		else
		{
			sr = -12288.0f; 
		}
	}
	else if (Vals[e_paraBits])
	{
		const unsigned int bits[] = {
			0xffffffff,
			0xfffffffe,
			0xfffffffc,
			0xfffffff8,
			0xfffffff0,
			0xffffffe0,
			0xffffffc0,
			0xffffff80,
			0xffffff00,
			0xfffffe00,
			0xfffffc00,
			0xfffff800,
			0xfffff000,
			0xffffe000,
			0xffffc000,
			0xffff8000,
			0xffff0000,
			0xfffe0000,
			0xfffc0000,
			0xfff80000,
			0xfff00000,
			0xffe00000,
			0xffc00000,
			0xff800000,
			0xff000000,
			0xfe000000,
			0xfc000000,
			0xf8000000,
			0xf0000000,
			0xe0000000,
			0xc0000000,
			0x80000000,
			0x00000000,
		};

		int i;
		// do bits
		if (fabs(sl) < 16384)
		{
			i = abs(rint<int>(sl*65536*2));
		}
		else
		{
			i = 0x7fffffff;
		}
		if (sl > 0)
		{
			sl = (i & bits[Vals[1]-1])/(65536*2.0f);
		}
		else
		{
			sl = (i & bits[Vals[1]-1])/(-65536*2.0f);
		}
		if (fabs(sr) < 16384)
		{
			i = abs(rint<int>(sr*65536*2));
		}
		else
		{
			i = 0x7fffffff;
		}
		if (sr > 0)
		{
			sr = (i & bits[Vals[1]-1])/(65536*2.0f);
		}
		else
		{
			sr = (i & bits[Vals[1]-1])/(-65536*2.0f);
		}
	}

	*psamplesleft=sl;
	*psamplesright=sr;

	++psamplesleft;
	++psamplesright;
		
	} while(--numsamples);

}

// Function that describes value on client's displaying
bool mi::DescribeValue(char* txt,int const param, int const value)
{
	switch (param)
	{
	case e_paraFrequency:
		// freq
		if (value == 0)
		{
			sprintf(txt,"Off");
			return true;
		}
		sprintf(txt,"%.4f hz",((4096*4096)-(value*value)) * (44100.0f/(4096*4096.0f)));
		return true;
		break;
	case e_paraBits:
		// bits
		if (value == 0)
		{
			sprintf(txt,"Off");
			return true;
		}
		else if (value == 32)
		{
			sprintf(txt,"1 bit square");
			return true;
		}
		sprintf(txt,"%d bits",33-value);
		return true;
		break;
	case e_paraInputGain:
		// mix
		sprintf(txt,"%.2f %%",100*(value/256.0f)*(value/256.0f)*(value/256.0f));
		return true;
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
	default:
		return false;
		break;
	}
}
}