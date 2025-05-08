//////////////////////////////////////////////////////////////////////
//
//				EQ5
//
//				druttis@darkface.pp.se
//
//////////////////////////////////////////////////////////////////////
#include <psycle/plugin_interface.hpp>
#include "../dsp/Biquad.h"
#include <cstdio>

namespace psycle::plugins::druttis::eq3 {
using namespace psycle::plugins::druttis;
using namespace psycle::plugin_interface;

//////////////////////////////////////////////////////////////////////
//
//				Machine Defs
//
//////////////////////////////////////////////////////////////////////
#define MAC_NAME "EQ-3"
int const MAC_VERSION = 0x0110;
#define MAC_AUTHOR "Druttis"
#define NUM_PARAM_COLS 4
//////////////////////////////////////////////////////////////////////
//
//				Machine Parameters
//
//////////////////////////////////////////////////////////////////////
#define PARAM_LSH_GAIN_LEFT 0
CMachineParameter const param_lsh_gain_left = {"Low Gain Left", "LSH - Gain Left", 0, 256, MPF_STATE, 128};

#define PARAM_LSH_GAIN_RIGHT 1
CMachineParameter const param_lsh_gain_right = {"Low Gain Right", "LSH - Gain Right", -1, 256, MPF_STATE, -1};

#define PARAM_LSH_FREQ 2
CMachineParameter const param_lsh_freq = {"Low Freq", "LSH - Freq", 0, 4096, MPF_STATE, 512};

#define PARAM_PEQ_GAIN_LEFT 3
CMachineParameter const param_peq_gain_left = {"Mid Gain Left", "PEQ - Gain Left", 0, 256, MPF_STATE, 128};

#define PARAM_PEQ_GAIN_RIGHT 4
CMachineParameter const param_peq_gain_right = {"Mid Gain Right", "PEQ - Gain Right", -1, 256, MPF_STATE, -1};

#define PARAM_PEQ_FREQ 5
CMachineParameter const param_peq_freq = {"Mid Freq", "PEQ - Freq", 0, 4096, MPF_STATE, 1024};

#define PARAM_HSH_GAIN_LEFT 6
CMachineParameter const param_hsh_gain_left = {"High Gain Left", "HSH - Gain Left", 0, 256, MPF_STATE, 128};

#define PARAM_HSH_GAIN_RIGHT 7
CMachineParameter const param_hsh_gain_right = {"High Gain Right", "HSH - Gain Right", -1, 256, MPF_STATE, -1};

#define PARAM_HSH_FREQ 8
CMachineParameter const param_hsh_freq = {"High Freq", "HSH - Freq", 0, 4096, MPF_STATE, 2048};

#define PARAM_MASTER_LEFT 9
CMachineParameter const param_master_left = {"Master Gain - Left", "Master - Left", 0, 256, MPF_STATE, 128};

#define PARAM_MASTER_RIGHT 10
CMachineParameter const param_master_right = {"Master Gain - Right", "Master - Right", -1, 256, MPF_STATE, -1};

#define PARAM_BASS_BOOST 11
CMachineParameter const param_bass_boost = {"Bass boost", "Bass boost", 0, 1, MPF_STATE, 0};

//////////////////////////////////////////////////////////////////////
//
//				Machine Parameter List
//
//////////////////////////////////////////////////////////////////////
CMachineParameter const *pParams[] =
{
	&param_lsh_gain_left,
	&param_lsh_gain_right,
	&param_lsh_freq,
	&param_peq_gain_left,
	&param_peq_gain_right,
	&param_peq_freq,
	&param_hsh_gain_left,
	&param_hsh_gain_right,
	&param_hsh_freq,
	&param_master_left,
	&param_master_right,
	&param_bass_boost
};
//////////////////////////////////////////////////////////////////////
//
//				Machine Info
//
//////////////////////////////////////////////////////////////////////
CMachineInfo MacInfo (
	MI_VERSION,
	MAC_VERSION,
	EFFECT,
	sizeof pParams / sizeof *pParams,
	pParams,
#ifdef _DEBUG
	MAC_NAME  " (Debug)",
#else
	MAC_NAME,
#endif
	MAC_NAME,
	MAC_AUTHOR " on " __DATE__,
	"Command Help",
	NUM_PARAM_COLS
);
//////////////////////////////////////////////////////////////////////
//
//				InitFilter Helper Function
//
//////////////////////////////////////////////////////////////////////
void InitFilter(Biquad *b, int type, int gain, int freq, int sr, float bw)
{
	b->Init(
		(Biquad::filter_t) type,
		(float) (gain - 128) / 10.0f,
		12000.0f * (float) freq / 4096.0f + 20.0f,
		sr,
		bw
	);
}

//////////////////////////////////////////////////////////////////////
//
//				Machine Class
//
//////////////////////////////////////////////////////////////////////
class mi : public CMachineInterface
{
	//////////////////////////////////////////////////////////////////
	//
	//				Machine Variables
	//
	//////////////////////////////////////////////////////////////////
private:
	// Static Generic
	static int instances;

	// Instance variables
	Biquad bands[3][2];

	float master[2];
	int currentSR;
	
    bool last_empty;
	//////////////////////////////////////////////////////////////////
	//
	//				Machine Methods
	//
	//////////////////////////////////////////////////////////////////
public:
	// Generic
	mi();
	virtual ~mi();
	virtual void Load();
	virtual void Unload();
	virtual void Command();
	virtual void Init();
	virtual void Stop();
	virtual void ParameterTweak(int par, int val);
	virtual bool DescribeValue(char* txt,int const param, int const value);
	virtual void SequencerTick();
	virtual void Work(float *psamplesleft, float *psamplesright , int numsamples, int numtracks);
	// Custom
	virtual inline bool buffer_has_signals(const float *buffer, int ns);
};
PSYCLE__PLUGIN__INSTANTIATOR("eq3", mi, MacInfo)
//////////////////////////////////////////////////////////////////////
//
//				Initialize Static Machine Variables
//
//////////////////////////////////////////////////////////////////////
// Static Generic
int mi::instances = 0;
//////////////////////////////////////////////////////////////////////
//
//				Machine Constructor
//
//////////////////////////////////////////////////////////////////////
mi::mi()
{
	Vals = new int[MacInfo.numParameters];
	if (mi::instances == 0)
		Load();
	mi::instances++;
	Stop();
}
//////////////////////////////////////////////////////////////////////
//
//				Machine Destructor
//
//////////////////////////////////////////////////////////////////////
mi::~mi()
{
	delete[] Vals;
	mi::instances--;
	if (mi::instances == 0)
		Unload();
}
//////////////////////////////////////////////////////////////////////
//
//				Load
//
//////////////////////////////////////////////////////////////////////
void mi::Load()
{
}
//////////////////////////////////////////////////////////////////////
//
//				load
//
//////////////////////////////////////////////////////////////////////
void mi::Unload()
{
}
//////////////////////////////////////////////////////////////////////
//
//				Command
//
//////////////////////////////////////////////////////////////////////
void mi::Command()
{
	pCB->MessBox(
		"Decription"
		"Equalizer with 3bands\n"
		,
		MAC_AUTHOR " " MAC_NAME
		,
		0
	);
}
//////////////////////////////////////////////////////////////////////
//
//				Init
//
//////////////////////////////////////////////////////////////////////
void mi::Init()
{
	currentSR = pCB->GetSamplingRate();
	Stop();
}
//////////////////////////////////////////////////////////////////////
//
//				Stop
//
//////////////////////////////////////////////////////////////////////
void mi::Stop()
{
	for (int i = 0; i < 3; i++)
	{
		bands[i][0].ResetMemory();
		bands[i][1].ResetMemory();
	}
}

//////////////////////////////////////////////////////////////////////
//
//				ParameterTweak
//
//////////////////////////////////////////////////////////////////////
void mi::ParameterTweak(int par, int val)
{
	Vals[par] = val;

	int left, right, freq;

	switch (par)
	{
	case PARAM_LSH_GAIN_LEFT :
	case PARAM_LSH_GAIN_RIGHT :
	case PARAM_LSH_FREQ :
	case PARAM_BASS_BOOST :
		left = Vals[PARAM_LSH_GAIN_LEFT];
		right = Vals[PARAM_LSH_GAIN_RIGHT];
		freq = Vals[PARAM_LSH_FREQ];
		if (right == -1)
		{
			right = left;
		}
		if (Vals[PARAM_BASS_BOOST])
		{
			InitFilter(&bands[0][0], Biquad::LOWSHELF, left, freq, currentSR, 1.0f);
			InitFilter(&bands[0][1], Biquad::LOWSHELF, right, freq, currentSR, 1.0f);
		}
		else
		{
			InitFilter(&bands[0][0], Biquad::LOWSHELF, left, freq, currentSR, 0.0f);
			InitFilter(&bands[0][1], Biquad::LOWSHELF, right, freq, currentSR, 0.0f);
		}
		break;
	case PARAM_PEQ_GAIN_LEFT :
	case PARAM_PEQ_GAIN_RIGHT :
	case PARAM_PEQ_FREQ :
		left = Vals[PARAM_PEQ_GAIN_LEFT];
		right = Vals[PARAM_PEQ_GAIN_RIGHT];
		freq = Vals[PARAM_PEQ_FREQ];
		if (right == -1)
		{
			right = left;
		}
		InitFilter(&bands[1][0], Biquad::PEAK_EQ, left, freq, currentSR, 0.85f);
		InitFilter(&bands[1][1], Biquad::PEAK_EQ, right, freq, currentSR, 0.85f);
		break;
	case PARAM_HSH_GAIN_LEFT :
	case PARAM_HSH_GAIN_RIGHT :
	case PARAM_HSH_FREQ :
		left = Vals[PARAM_HSH_GAIN_LEFT];
		right = Vals[PARAM_HSH_GAIN_RIGHT];
		freq = Vals[PARAM_HSH_FREQ];
		if (right == -1)
		{
			right = left;
		}
		InitFilter(&bands[2][0], Biquad::HIGHSHELF, left, freq, currentSR, 1.0f);
		InitFilter(&bands[2][1], Biquad::HIGHSHELF, right, freq, currentSR, 1.0f);
		break;
	case PARAM_MASTER_LEFT :
	case PARAM_MASTER_RIGHT :
		left = Vals[PARAM_MASTER_LEFT];
		right = Vals[PARAM_MASTER_RIGHT];
		if (right == -1)
		{
			right = left;
		}
		master[0] = (float) left / 128.0f;
		master[1] = (float) right / 128.0f;
		break;
	}
}
//////////////////////////////////////////////////////////////////////
//
//				DescribeValue
//
//////////////////////////////////////////////////////////////////////
bool mi::DescribeValue(char* txt,int const param, int const value)
{
	switch (param)
	{
	case PARAM_LSH_GAIN_LEFT :
	case PARAM_LSH_GAIN_RIGHT :
	case PARAM_PEQ_GAIN_LEFT :
	case PARAM_PEQ_GAIN_RIGHT :
	case PARAM_HSH_GAIN_LEFT :
	case PARAM_HSH_GAIN_RIGHT :
		if (value > -1) sprintf(txt, "%.2f dB", (float) (value - 128) / 10.0f);
		else sprintf(txt, "(left)");
		return true;
	case PARAM_LSH_FREQ :
	case PARAM_PEQ_FREQ :
	case PARAM_HSH_FREQ :
		sprintf(txt, "%.2f Hz", (float) (value * 12000) / 4096.0f + 20.0f);
		return true;
	case PARAM_MASTER_LEFT :
	case PARAM_MASTER_RIGHT :
		if (value > 0) sprintf(txt, "%.02f dB", 20.0f * std::log10((float) value / 128.0f));
		else if (value > -1) sprintf(txt, "-inf dB");
		else sprintf(txt, "(left)");
		return true;
	case PARAM_BASS_BOOST :
		sprintf(txt, "%s", (value ? "on" : "off"));
		return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////
//
//				SequencerTick
//
//////////////////////////////////////////////////////////////////////
void mi::SequencerTick()
{
	if(currentSR != pCB->GetSamplingRate())
	{
		int left, right, freq;
		currentSR = pCB->GetSamplingRate();
		left = Vals[PARAM_LSH_GAIN_LEFT];
		right = Vals[PARAM_LSH_GAIN_RIGHT];
		freq = Vals[PARAM_LSH_FREQ];
		if (Vals[PARAM_BASS_BOOST])
		{
			InitFilter(&bands[0][0], Biquad::LOWSHELF, left, freq, currentSR, 1.0f);
			InitFilter(&bands[0][1], Biquad::LOWSHELF, right, freq, currentSR, 1.0f);
		}
		else
		{
			InitFilter(&bands[0][0], Biquad::LOWSHELF, left, freq, currentSR, 0.0f);
			InitFilter(&bands[0][1], Biquad::LOWSHELF, right, freq, currentSR, 0.0f);
		}
		left = Vals[PARAM_PEQ_GAIN_LEFT];
		right = Vals[PARAM_PEQ_GAIN_RIGHT];
		freq = Vals[PARAM_PEQ_FREQ];
		InitFilter(&bands[1][0], Biquad::PEAK_EQ, left, freq, currentSR, 0.85f);
		InitFilter(&bands[1][1], Biquad::PEAK_EQ, right, freq, currentSR, 0.85f);
		left = Vals[PARAM_HSH_GAIN_LEFT];
		right = Vals[PARAM_HSH_GAIN_RIGHT];
		freq = Vals[PARAM_HSH_FREQ];
		InitFilter(&bands[2][0], Biquad::HIGHSHELF, left, freq, currentSR, 0.0f);
		InitFilter(&bands[2][1], Biquad::HIGHSHELF, right, freq, currentSR, 0.0f);
	}
}
//////////////////////////////////////////////////////////////////////
//
//				Work
//
//////////////////////////////////////////////////////////////////////

#define SIGNAL_TRESHOLD (0.0000158489f)
 
inline bool mi::buffer_has_signals(const float *buffer, int ns) { // From zzub/plugin.h
    while (ns--) {
        if ((*buffer > SIGNAL_TRESHOLD)||(*buffer < -SIGNAL_TRESHOLD)) {
            return true;
        }
        buffer++;
    }
    return false;
}

void mi::Work(float *psamplesleft, float *psamplesright, int numsamples, int numtracks)
{
    if (last_empty &&
            !buffer_has_signals(psamplesleft, numsamples) &&
            !buffer_has_signals(psamplesright, numsamples)) {
        return;
    }
    int  numsamplesOrig = numsamples;
    
	float il, ir;
	--psamplesleft;
	--psamplesright;
	do
	{
		il = *++psamplesleft;
		*psamplesleft = (bands[0][0].Work(
							bands[1][0].Work(
							bands[2][0].Work(il))))
							* master[0];
		ir = *++psamplesright;
		*psamplesright = (bands[0][1].Work(
							bands[1][1].Work(
							bands[2][1].Work(ir))))
							* master[1];
	}
	while (--numsamples);
	
    if (!buffer_has_signals(++psamplesleft - numsamplesOrig, numsamplesOrig) &&
            !buffer_has_signals(++psamplesright - numsamplesOrig, numsamplesOrig)) {
        last_empty = true;
        Stop();
    } else {
        last_empty = false;
    }
}
}