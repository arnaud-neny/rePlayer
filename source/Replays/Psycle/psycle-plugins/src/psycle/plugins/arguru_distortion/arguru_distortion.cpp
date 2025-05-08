///\file
///\brief Arguru simple distortion/saturator plugin for PSYCLE
#include <psycle/plugin_interface.hpp>
#include <cmath>
#include <cstdio> // for std::sprintf

namespace psycle::plugins::arguru_distortion {

using namespace psycle::plugin_interface;

CMachineParameter const paraThreshold = {"Threshold","Threshold level",1,0x8000,MPF_STATE, 0x200};
CMachineParameter const paraGain = {"Output Gain","Output Gain",1,2048,MPF_STATE,1024};
CMachineParameter const paraInvert = {"Phase inversor","Stereo phase inversor",0,1,MPF_STATE,0};
CMachineParameter const paraMode = {"Mode","Operational mode",0,1,MPF_STATE,0};

CMachineParameter const *pParameters[] = 
{ 
	&paraThreshold,
	&paraGain,
	&paraInvert,
	&paraMode
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0120,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"Arguru Distortion"
#ifndef NDEBUG
	" (debug build)"
#endif
	,
	"Distortion",
	"J. Arguelles",
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

private:
	inline void Clip( float * psamplesleft, float const threshold, float const negthreshold, float const wet );
	inline void Saturate( float * psamplesleft, float const threshold, float const negthreshold, float const wet );
	float leftLim;
	float rightLim;

};

PSYCLE__PLUGIN__INSTANTIATOR("arguru-distortion", mi, MacInfo)

mi::mi():leftLim(1.0f), rightLim(1.0f)
{
	Vals = new int[MacInfo.numParameters];
}

mi::~mi()
{
	delete[] Vals;
}

void mi::Init()
{
}

void mi::SequencerTick()
{
}

void mi::Command()
{
	pCB->MessBox("Made 18/5/2000 by Juan Antonio Arguelles Rius for Psycl3!","-=<([aRgUrU's sAtUrAt0R])>=-",0);
}

void mi::ParameterTweak(int par, int val)
{
	Vals[par]=val;
}

inline void mi::Clip( float * psamplesleft, float const threshold, float const negthreshold, float const wet )
{
	float sl = *psamplesleft;
	if (sl > threshold)sl = threshold;
	if (sl <= negthreshold)sl = negthreshold;
	*psamplesleft=sl*wet;
}
///\todo: minor detail. This implementation will saturate faster at higher sampling rates.
inline void mi::Saturate( float * psamplesleft, float const threshold, float const negthreshold, float const wet )
{
	const float s_in = (*psamplesleft);
	float sl = s_in*leftLim;
	if (sl > threshold) {
		leftLim -= (sl-threshold)*0.1f/s_in;
	} else if (sl <= negthreshold) {
		leftLim -= (sl-negthreshold)*0.1f/s_in; //psamplesleft is already negative, so no need to multiply by -1
	} else if (leftLim > 1.0f){
		leftLim = 1.0f;
	} else if (leftLim < 1.0f) leftLim = 0.01f +(leftLim*0.99f);
	*psamplesleft=sl*wet;
}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks)
{

	float const threshold = (float)(Vals[0]);
	float const negthreshold = -(float)(Vals[0]);

	float const wet = (float)Vals[1]*0.00390625f;
	float const negwet = -(float)Vals[1]*0.00390625f;

	if (Vals[3]==0) {
		if(Vals[2]==0)
		{
			// Clip, No Phase inversion
			do
			{
				Clip(psamplesleft, threshold, negthreshold, wet);
				Clip(psamplesright, threshold, negthreshold, wet);
				++psamplesleft;
				++psamplesright;
			} while(--numsamples);
		}
		else {
			// Clip, Phase inversion
			do
			{
				Clip(psamplesleft, threshold, negthreshold, wet);
				Clip(psamplesright, threshold, negthreshold, negwet);
				++psamplesleft;
				++psamplesright;
			} while(--numsamples);
		}
	}
	else 
	{
		if(Vals[2]==0)
		{
			// Saturate, No Phase inversion
			do
			{
				Saturate(psamplesleft, threshold, negthreshold, wet);
				Saturate(psamplesright, threshold, negthreshold, wet);
				++psamplesleft;
				++psamplesright;
				
			} while(--numsamples);
		}
		else {
			// Saturate, Phase inversion
			do
			{
				Saturate(psamplesleft, threshold, negthreshold, wet);
				Saturate(psamplesright, threshold, negthreshold, negwet);
				++psamplesleft;
				++psamplesright;

			} while(--numsamples);
		} 
	}
}

bool mi::DescribeValue(char* txt,int const param, int const value)
{
	switch(param) {
	case 0:
	{
		if(value > 0)
			std::sprintf(txt,"%.1f dB",20.0f * std::log10(value * 0.000030517578125f));
		else
			std::sprintf(txt,"-Inf. dB");				
		return true;
	}
	case 1:
	{
		if(value > 0)
			std::sprintf(txt,"%.1f dB",20.0f * std::log10(value * 0.00390625f));
		else
			std::sprintf(txt,"-Inf. dB");				
		return true;
	}
	case 2:
	{
		switch(value)
		{
		case 0:std::sprintf(txt,"Off");	break;
		case 1:std::sprintf(txt,"On");	break;
		}
		return true;
	}
	case 3:
	{
		switch(value)
		{
		case 0:std::sprintf(txt,"Clip");	break;
		case 1:std::sprintf(txt,"Saturate");break;
		}
		return true;
	}
	default: return false;
	}
	
}
}