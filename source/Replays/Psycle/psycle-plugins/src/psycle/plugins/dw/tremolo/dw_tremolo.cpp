#include <psycle/plugin_interface.hpp>
#include <cmath>
#include <cstdio>
#include <string.h>

namespace psycle::plugins::dw::tremolo {
using namespace psycle::plugins::dw;
using namespace psycle::plugin_interface;

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif

#define LFO_SIZE			720

#define DEPTH_DIV			1000

#define MAX_DEPTH			2000
#define MAX_SPEED			10000
#define MAX_WAVES			3
#define MAX_SKEW			LFO_SIZE
#define MAX_PHASE			LFO_SIZE
#define MAX_GRAVITY			200			// this can't be changed without other updates, be careful

#define MAPSIZE				4096
#define DISPLAY_REFRESH		882


CMachineParameter const paramDepth =	{ "Depth",	"Tremolo Depth",	0,	MAX_DEPTH,	MPF_STATE,MAX_DEPTH/2};
CMachineParameter const paramSpeed =	{ "Speed",	"Tremolo Speed",	16,	MAX_SPEED,	MPF_STATE,MAX_SPEED/6};
CMachineParameter const paramWaveform =	{ "Waveform","lfo waveform",	0,	MAX_WAVES-1,MPF_STATE,0};
CMachineParameter const paramSkew =		{ "P.Width/Skew","lopsidedness",10,	MAX_SKEW-10,MPF_STATE,MAX_SKEW/2};
CMachineParameter const paramGravity =	{ "Gravity","Gravity",			0,	MAX_GRAVITY,MPF_STATE,100};
CMachineParameter const paramGravMode = { "Gravity Mode","Gravity Mode",0,	1,			MPF_STATE,0};
CMachineParameter const paramStereoPhase={ "Stereo Phase","Stereo Phase",0,	MAX_PHASE,	MPF_STATE,MAX_PHASE*3/4	};
CMachineParameter const paramSync		={"Restart LFO","Restart LFO",	0,1,MPF_STATE,0	};
//CMachineParameter const paramNull =	{" "," ",0,0,MPF_NULL,0};

CMachineParameter const *pParameters[] = 
{ 
	//column 1:
	&paramDepth,
	&paramWaveform,
	&paramGravity,
	&paramSkew,
	//column 2:
	&paramSpeed,
	&paramStereoPhase,
	&paramGravMode,
	&paramSync
};

enum
{
	prm_depth=0,
	prm_waveform,
	prm_gravity,
	prm_skew,
	prm_speed,
	prm_stereophase,
	prm_gravmode,
	prm_sync
};

enum
{
	lfo_sine=0,
	lfo_tri,
	lfo_square
};

enum
{				
	grav_updown=0,
	grav_inout
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0002,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
#ifndef NDEBUG
	"dw Tremolo (Debug build)",
#else
	"dw Tremolo",
#endif
	"Tremolo",
	"dw",
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
	void fillLfo();

protected:
	float lfo[LFO_SIZE];
	float lfopos;

	float srMultiplier;

	float inCurve[MAPSIZE+2];
	float outCurve[MAPSIZE+2];
	float lowCurve[MAPSIZE+2];
	float highCurve[MAPSIZE+2];		// later we scale from 0 (not 1) to MAPSIZE, which is why there's +1.. we make it +2 just 
									//  in case (we'll init it to 1.0f later)

	int disp_counter;
};

PSYCLE__PLUGIN__INSTANTIATOR("dw-tremolo", mi, MacInfo)

mi::mi()
{
	Vals = new int[MacInfo.numParameters];
	Vals[prm_skew] = MAX_SKEW/2;
	Vals[prm_gravity] = 100;
}

mi::~mi()
{
	delete[] Vals;
}

void mi::Init()
{
	lfopos=0;
	srMultiplier = 44100.f/(float)pCB->GetSamplingRate();		// this coefficient changes the increment to lfopos..  timing is defined by samples,
															// so without it playing the same song in 22050 will halve the lfo speed.
	float j;
	for(int i=0;i<=MAPSIZE;++i)
	{
		j = i / (float)MAPSIZE;
		highCurve[i] = std::sqrt((-j*j + 2*j));
		lowCurve[MAPSIZE - i] = 1.0f - highCurve[i];
	}
	for(int i=0;i<=MAPSIZE/2;++i)
	{
		j = i / (float)(MAPSIZE / 2);
		inCurve[i] = std::sqrt((-j*j + 2*j)) / 2.0f;
		inCurve[MAPSIZE + 1 - i]				= 1.0f - inCurve[i];
		outCurve[MAPSIZE/2-i]				= .5f - inCurve[i]; 
		outCurve[MAPSIZE/2+i+1]				= .5f + inCurve[i];
	}
	highCurve[MAPSIZE+1] = lowCurve[MAPSIZE+1] = inCurve[MAPSIZE+1] = outCurve[MAPSIZE+1] = 1.0f;

}

// Called on each tick while sequencer is playing
void mi::SequencerTick()
{
	float newsr = 44100.f/(float)pCB->GetSamplingRate();
	if(newsr!=srMultiplier)																																				// it doesn't need to happen this frequently-- plus, it won't register until
		srMultiplier=newsr;																																				// an actual song is playing
}

void mi::Command()
{
// Called when user presses editor button
	pCB->MessBox("dw Tremolo\n\nby d.w. aley","uNF",0);
}

void mi::ParameterTweak(int par, int val)
{
	Vals[par]=val;
	if	(par == prm_skew 
			|| par == prm_waveform
			|| par == prm_gravity
			|| par == prm_gravmode)
		fillLfo();

	if (par == prm_sync && val==1)
	{
		disp_counter=DISPLAY_REFRESH;
		lfopos=0;
	}
}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks)
{
	int rtpos, ltpos;
	int ltSign, rtSign;

	float volMult = Vals[prm_depth] / (float)DEPTH_DIV;
	do
	{
		ltpos=(int)floor(lfopos);
		rtpos=(int)(ltpos+Vals[prm_stereophase]+(MAX_PHASE/2.0f))%LFO_SIZE;				

		ltSign = (*psamplesleft  < 0? -1: +1);
		rtSign = (*psamplesright < 0? -1: +1);
		*psamplesleft  -= lfo[ltpos] * *psamplesleft  * volMult;
		*psamplesright -= lfo[rtpos] * *psamplesright * volMult;

		//when the depth is > 100%, the above multiplication can pass 0.. this next bit makes sure nothing does.
		//if the original sign multiplied by the new value is negative, it means the sign has changed, and the value is zeroed out.
		if(*psamplesleft * ltSign < 0) *psamplesleft = 0.0;								// i'm not sure how cpu-intensive it is to multiply by 1 or -1..
		if(*psamplesright* rtSign < 0) *psamplesright= 0.0;								// but this seems the most terse way to get the result i want.

		lfopos += (Vals[prm_speed]) / (float) (MAX_SPEED*(LFO_SIZE/360.0f)) * srMultiplier;

		if(lfopos>=LFO_SIZE) lfopos-=LFO_SIZE;

		if(Vals[prm_sync]==1)
		{
			if(--disp_counter < 0)
				Vals[prm_sync]=0;
		}

		++psamplesleft;
		++psamplesright;

	} while(--numsamples);
}


bool mi::DescribeValue(char* txt,int const param, int const value)
{
	switch(param)
	{
	case prm_depth: sprintf(txt,"%.1f%%", value/(float)(DEPTH_DIV/100.0f));
		return true;

	case prm_stereophase:
	case prm_skew: sprintf(txt, "%i deg.", (int)((value - MAX_SKEW/2.0f) / (MAX_SKEW/360.0f)));
		return true;
	
		// please, don't ask me to explain how i derived this mess :) (i hate these conversions)
	case prm_speed: sprintf(txt, "%.1f ms", LFO_SIZE/(float)(value/(float)MAX_SPEED)/44.1f * LFO_SIZE/360.0f); 
		return true;

	case prm_gravity: if(Vals[prm_gravmode]==grav_updown)
					  {
						  if(value<100)
							  sprintf(txt, "%i%% (upwards)", value-100);
						  else if(value>100)
							  sprintf(txt, "%i%% (downwards)", value-100);
						  else
							  strcpy(txt, "0%%");
					  } else {
						  if(value<100)
							  sprintf(txt, "%i%% (outwards)", value-100);
						  else if(value>100)
							  sprintf(txt, "%i%% (inwards)", value-100);
						  else
							  strcpy(txt, "0%%");
					  }
		return true;

	case prm_gravmode: if(value == grav_updown)
					   {
						   strcpy(txt, "Up/Down");
					   } else if(value == grav_inout)
					   {
						   strcpy(txt, "In/Out");
					   } else {
						   return false;
					   }
		return true;

	case prm_sync: if(value==1)
					   strcpy(txt, "lfo restarted!");
				   else
					   strcpy(txt, "");
		return true;

	case prm_waveform: switch(value)
					   {
						case lfo_sine: strcpy(txt, "sine");
							break;
						case lfo_tri: strcpy(txt, "triangle");
							break;
						case lfo_square: strcpy(txt, "square");
							break;
					   }
		return true;
	}
	return false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  fillLfo()

// called whenever a parameter is changed that warrants rebuffering the lfo
// just so we're clear, this actually makes -inverted- sine/tri/square waves,
// since these values determine how much is -subtracted- from the signal.
// everything is scaled between 0 and 1

// since there may be a possibility that the user will want to sweep the skew variable, it may be
// in our best interest, efficiency-wise, to reimplement this such that skew controls the increment speed of the
// lfopos variable in Work(), and the buffer keeps an even midpoint. (we could also do something similar with the gravity curves..)

void mi::fillLfo()
{
	int skew = Vals[prm_skew];
	if(skew>LFO_SIZE||skew<1) skew = LFO_SIZE/2;// crashes psycle without this! on creation of the class, this function gets called before
												// Vals[prm_skew] is ever initialized. i'm sure there are more efficient ways to do this..

	int sqSlope;
	float xlop, ylop, temp, x;

	switch(Vals[prm_waveform])
	{
	case lfo_tri:
		for(int i=0;i<skew;++i)
			lfo[i]= 1.0f - (i/(float)skew);
		for(int i=skew;i<LFO_SIZE;++i)
			lfo[i]= ((float)(i-skew) / (float)(LFO_SIZE-skew));
		break;

	case lfo_sine: for(int i=0;i<skew;++i)
					   lfo[i] = 0.5f * cos(i/(float)skew * M_PI) + 0.5f;
		for(int i=skew;i<LFO_SIZE;++i)
			lfo[i] = -0.5f * cos( (i-skew)/(float)(LFO_SIZE-skew) * M_PI) + 0.5f;

		break;

	case lfo_square: sqSlope = (int)(LFO_SIZE/80.0f); // 80.0f is a completely arbitrary constant, feel free to adjust to taste
		if(sqSlope==0) sqSlope=1;						//better safe than sorry..
		if(skew<sqSlope) skew=sqSlope;					// more of a trapezoid wave, i guess.. perfect square makes clicks

		for(int i=0;i<sqSlope;++i)
		{
			lfo[i]=1.0f - (i/(float)sqSlope);
			lfo[i+skew] = (i/(float)sqSlope);
		}
		for(int i=sqSlope;i<skew;++i)
			lfo[i]=0.0f;

		for(int i=skew+sqSlope;i<LFO_SIZE;++i)
			lfo[i]=1.0f;
		break;
	}


	if(Vals[prm_gravity]!=100 && Vals[prm_waveform]!=lfo_square) //we -could- do all this with 0 gravity or a square wave, it just wouldn't change anything..
	{
		ylop=std::fabs(Vals[prm_gravity]-100.0f) / 100.0f; // for weighted average of linear value and curved value

		for(int i=0;i<LFO_SIZE;++i)
		{
			x = lfo[i] * MAPSIZE;

			if(x<0) x=0;			//okay, this really should not be happening, but until i've checked out my equations a bit more,
			if(x>MAPSIZE) x=MAPSIZE; // i don't want to risk it

			xlop=x-floor(x);	// for linear interpolation between points in the curve mapping array


			if(Vals[prm_gravmode]==grav_updown)
			{
				if(Vals[prm_gravity]>100)	// positive gravity, use highcurve (remember! this is being subtracted, so our lfos are umop apisdn)
				{
					temp = (highCurve[(int)(floor(x))] * (1.0f-xlop))	//xlop==0 means 100% floor, xlop==.5 means 50% floor and 50% ceil, etc..
						+ (highCurve[(int)(ceil(x))] * xlop);
				} else {
					temp = (lowCurve[(int)(floor(x))] * (1.0f-xlop)) //now, x should never be more than 1.0f, which means ceil(x) * MAPSIZE should never
						+ (lowCurve[(int)(ceil(x))] * xlop);		// be more than MAPSIZE, but just for safety, the mapping array has one extra element
				}													// at [MAPSIZE+1], initialized to 1.0f.
			} else {
				if(Vals[prm_gravity]>100)
				{
					temp = (inCurve[(int)(floor(x))] * (1.0f-xlop))
						+ (inCurve[(int)(ceil(x))] * xlop);
				} else {
					temp = (outCurve[(int)(floor(x))] * (1.0f-xlop))
						+ (outCurve[(int)(ceil(x))] * xlop);
				}
			}
			lfo[i] = (lfo[i] * (1.0f-ylop))			//ylop==1 means 100% curve map, ylop==.5 means 50% linear and 50% curved, etc.
				+ (temp * ylop);
		}
	}
}
}