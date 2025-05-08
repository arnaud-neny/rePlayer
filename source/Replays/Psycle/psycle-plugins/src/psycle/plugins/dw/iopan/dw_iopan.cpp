#include <psycle/plugin_interface.hpp>
#include <cstdio>
namespace psycle::plugins::dw::iopan {
using namespace psycle::plugins::dw;

using namespace psycle::plugin_interface;

#define LCHAN 0
#define RCHAN 2
#define MAINTAIN 1
#define FLIP 3

#define FL_NO 0
#define FL_YES 1

#define MT_WIDTH 0
#define MT_NONE 1
#define MT_CENTER 2

CMachineParameter const leftExtent = {"Left", "Left pan extent", 0, 128, MPF_STATE, 0};
CMachineParameter const rightExtent = {"Right", "Right pan extent", 0, 128, MPF_STATE, 128};
CMachineParameter const maintain = {"Maintain", "Maintain", 0, 2, MPF_STATE, MT_NONE};
CMachineParameter const allowFlip = {"Allow Flip", "Allow channel flipping", 0, 1, MPF_STATE, FL_YES};

CMachineParameter const *pParameters[] = 
{ 
	// global
	&leftExtent,
	&maintain,
	&rightExtent,
	&allowFlip
};

CMachineInfo const MacInfo (
	MI_VERSION,				
	0x0001,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"dw IoPan"
		#ifndef NDEBUG
			" (Debug build)"
		#endif
		,
	"IoPan",
	"dw",
	"About",
	2
);


class mi : public CMachineInterface {
	public:
		mi();
		virtual ~mi();
		virtual void Init();
		virtual void SequencerTick();
		virtual void Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks);
		virtual bool DescribeValue(char* txt,int const param, int const value);
		virtual void Command();
		virtual void ParameterTweak(int par, int val);
};

PSYCLE__PLUGIN__INSTANTIATOR("dw-iopan", mi, MacInfo)

mi::mi() {
	Vals = new int[MacInfo.numParameters];
}

mi::~mi() {
	delete[] Vals;
}

void mi::Init() {
}

void mi::SequencerTick() {
	// Called on each tick while sequencer is playing
}

void mi::Command() {
	// Called when user presses editor button
	pCB->MessBox("IoPan v. 0.1b\n\npan scaler for psycle\nby d.w. aley","uNF",0);
}



void mi::ParameterTweak(int par, int val) {

////Variable center code
//
//				int center = (Vals[RCHAN] + Vals[LCHAN]) / 2;
//				if(center>128) center=64;								// dodge some weird init errors
//
//				int maxPan, minPan;
//				if(center>64) 
//				{
//								maxPan=128;
//								minPan=128-(2*(128-center));
//				}
//				else
//				{
//								minPan=0;
//								maxPan=2*center;
//				}

	int width = Vals[RCHAN] - Vals[LCHAN];


	if(Vals[FLIP] == FL_NO)												// Flip not allowed
	{
		if(par == LCHAN && val > Vals[RCHAN]) 
		{
			if(Vals[MAINTAIN] == MT_CENTER)								// This could probably be done better..
				val = 64;																												// I'm trying to avoid issues with flip off and center on.
			else																																
				Vals[RCHAN] = val;
		}
		if(par == RCHAN && val < Vals[LCHAN]) 
		{
			if(Vals[MAINTAIN] == MT_CENTER)
				val = 64;
			else
				Vals[LCHAN] = val;
		}
	}								

	if(par == FLIP && val == FL_NO)
	{
		if(Vals[MAINTAIN] != MT_CENTER && Vals[LCHAN] > Vals[RCHAN])
			Vals[RCHAN] = Vals[LCHAN] = ((Vals[RCHAN]+Vals[LCHAN])/2);
	}


//my mother would cry if she saw this mess.  please, please, fix me.

	if(Vals[MAINTAIN]==MT_CENTER || (par==MAINTAIN && val==MT_CENTER))
	{
		switch(par)
		{
			case FLIP:								//fall through..
			case MAINTAIN:				if(Vals[FLIP] == FL_NO)																												
							{
								if(Vals[RCHAN] > 64)
									Vals[LCHAN] = 128 - Vals[RCHAN];				//again, this is all to prevent conflicts
								else if(Vals[LCHAN] < 64)																//between centering and no flipping.
									Vals[RCHAN] = 128 - Vals[LCHAN];
								else
									Vals[LCHAN] = Vals[RCHAN] = 64;
							}
							else if(par==MAINTAIN && val==MT_CENTER)
								Vals[RCHAN]=128-Vals[LCHAN];
							break;				

////variable center code, we may still use this one day (?)
//
//								if(par==RCHAN || par==LCHAN)
//								{
//												if(val > maxPan) val=maxPan;
//												else if(val < minPan) val=minPan;
//								}

			case LCHAN:								//Vals[RCHAN]=center+(center-val); // <-- variable center code
							Vals[RCHAN] = 128 - val;
							break;
			case RCHAN:								//Vals[LCHAN]=center+(center-val);  // <-- variable center code
							Vals[LCHAN] = 128 - val;
							break;
		}
	}
	if(Vals[MAINTAIN]==MT_WIDTH)
	{
		switch(par)
		{
			case LCHAN:								if(val + width > 128)				
							{
								val = 128 - width;
							}
							else if(val + width < 0)								
							{
								val = 0 - width;
							}
							Vals[RCHAN] = val + width;
							
							break;

			case RCHAN:								if(val - width > 128)
							{
								val = 128 + width;
							}
							else if(val - width < 0)
							{
								val = width;
							}
							Vals[LCHAN] = val - width;
							
							break;
		}
	}
	Vals[par]=val;

}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks)
{
	if(Vals[LCHAN]!=0 || Vals[RCHAN] !=128)
	{
		float lCoefR = Vals[LCHAN] / 128.0;								//how much of the LEFT channel goes to the RIGHT channel
		float lCoefL = 1.0 - lCoefR;												//how much of the LEFT channel stays on the LEFT channel
		float rCoefR = Vals[RCHAN] / 128.0;								//how much of the RIGHT channel stays on the RIGHT channel
		float rCoefL = 1.0 - rCoefR;												//how much of the RIGHT channel goes to the LEFT channel
	
		float rbuf;
		float lbuf;
	
		do
		{												
			lbuf = rCoefL * *psamplesright;
			*psamplesright *= rCoefR;
	
			rbuf = lCoefR * *psamplesleft;
			*psamplesleft *= lCoefL;
	
			*psamplesright = rbuf + *psamplesright;
			*psamplesleft  = lbuf + *psamplesleft;
	
			++psamplesleft;
			++psamplesright;
	
		} while(--numsamples);
	}
}
bool mi::DescribeValue(char* txt,int const param, int const value)
{
	switch(param)
	{
		case MAINTAIN:
			if(value == MT_NONE)
				sprintf(txt,"nothing");
			else if(value == MT_CENTER) 
				sprintf(txt,"center");
			else 
				sprintf(txt,"current width");
			return true;
	
		case FLIP:
			if(value == FL_NO) 
				sprintf(txt,"no");
			else 
				sprintf(txt,"yes");
			return true;
	}
	return false;
}
}