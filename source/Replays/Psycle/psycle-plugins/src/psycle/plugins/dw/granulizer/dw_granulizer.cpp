#include <psycle/plugin_interface.hpp>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <string.h>
#include <cstdio>

//////////////////////////////////////////////////////////////////////
// dw granulizer plugin
namespace psycle::plugins::dw::granulizer {

using namespace psycle::plugin_interface;

#define TWOPI 6.28318530718
#define NUM_RANDS 4

#define MIN_LENGTH 10
#define MIN_FREQ   1
#define MIN_ATTACK 0
#define MIN_DECAY  0
#define MIN_PITCH  0
#define MIN_LSPEED 10
#define MIN_LAYERS 1

#define MAX_LAYERS 10
#define MAX_LENGTH 16538
#define MAX_FREQ   16538
#define MAX_ATTACK 1800
#define MAX_DECAY  1800
#define MAX_PITCH  1000
#define MAX_THRESH 32768
#define MAX_GAIN   1000
#define MAX_LSPEED 32768

#define LSPEED_MULTIPLYER 30
#define LSPEED_SUBTRACTER MAX_LSPEED + MIN_LSPEED

#define LFO1         0
#define LFO2         1
#define LFO_SINE     0
#define LFO_TRIANGLE 1
#define LFO_SQUARE   2
#define LFO_SAWUP    3
#define LFO_SAWDOWN  4
#define LFO_RANDOM   5

#define GAIN_NORM 100

#define LINK_OFF 0
#define LINK_ON  1

#define SYNC_L1    -1
#define SYNC_L2     1
#define SYNC_NONE   0
#define SYNC_BOTH1 -2
#define SYNC_BOTH2  2

#define SYNC_GRAIN  1

#define LCHAN 0
#define RCHAN 1

#define LIM_AMT_SCALER 1024

#define PRM_SIZE        1
#define PRM_FREQ        2
#define PRM_ATTACK      3
#define PRM_DECAY       4
#define PRM_PSTART      5
#define PRM_PEND        6
#define PRM_PLINK       7
#define PRM_MAXGRAINS   8
#define PRM_RANDSIZE   11
#define PRM_RANDFREQ   12
#define PRM_RANDATTACK 13
#define PRM_RANDDECAY  14
#define PRM_RANDPSTART 15
#define PRM_RANDPEND   16
#define PRM_LFO1SHAPE  18
#define PRM_LFO2SHAPE  19
#define PRM_L1TOSIZE   21
#define PRM_L1TOFREQ   22
#define PRM_L1TOATTACK 23
#define PRM_L1TODECAY  24
#define PRM_L1TOPSTART 25
#define PRM_L1TOPEND   26
#define PRM_LFO1SPEED  28
#define PRM_LFO2SPEED  29
#define PRM_L2TOSIZE   31
#define PRM_L2TOFREQ   32
#define PRM_L2TOATTACK 33
#define PRM_L2TODECAY  34
#define PRM_L2TOPSTART 35
#define PRM_L2TOPEND   36
#define PRM_LFO1PHASE  38
#define PRM_LFO2PHASE  39
#define PRM_THRESH     41
#define PRM_OUTGAIN    42

#define PRM_GRAINSYNC  44
#define PRM_LFOSYNC    45

#define PRM_LIMITAMT   47
#define PRM_DENSITY    48

#define DISPLAY_REFRESH 882 // ~50/sec

#define CUBIC_RESOLUTION 2048 // stolen..


// shortname longname min max flags initval

CMachineParameter const paramNull            = {" "," ",0,0,MPF_NULL,0};
CMachineParameter const paramGrainSize       = { "Size","grain size",MIN_LENGTH,MAX_LENGTH,MPF_STATE,1140};
CMachineParameter const paramGrainFreq       = {"Freq","grain frequency",MIN_FREQ,MAX_FREQ,MPF_STATE,957};
CMachineParameter const paramAttack          = {"Attack","attack time",MIN_ATTACK,MAX_ATTACK,MPF_STATE,200};
CMachineParameter const paramDecay           = {"Decay","decay time",MIN_DECAY,MAX_DECAY,MPF_STATE,200};
CMachineParameter const paramPitchStart      = {"Pitch Start","Start pitch",MIN_PITCH,MAX_PITCH,MPF_STATE,MAX_PITCH};
CMachineParameter const paramPitchEnd        = {"Pitch End","End pitch",MIN_PITCH,MAX_PITCH,MPF_STATE,MAX_PITCH};
CMachineParameter const paramPitchLink       = {"Pitch Link","Link start/end pitch",LINK_OFF,LINK_ON,MPF_STATE,LINK_OFF};
CMachineParameter const paramMaxGrains       = {"Max Grain Layers","Maximum grain layers",MIN_LAYERS,MAX_LAYERS,MPF_STATE,MAX_LAYERS};
CMachineParameter const paramRandSize        = {"Rand% Size","Random size %",0,100,MPF_STATE,0};
CMachineParameter const paramRandFreq        = {"Rand% Freq","Random frequency %",0,100,MPF_STATE,0};
CMachineParameter const paramRandAttack      = {"Rand% Attack","Random attack %",0,100,MPF_STATE,0};
CMachineParameter const paramRandDecay       = {"Rand% Decay","Random decay %",0,100,MPF_STATE,0};
CMachineParameter const paramRandPStart      = {"Rand% PStart","Random start pitch %",0,100,MPF_STATE,0};
CMachineParameter const paramRandPEnd        = {"Rand% PEnd","Random end pitch %",0,100,MPF_STATE,0};
CMachineParameter const paramL1Size          = {"LFO1->Size","LFO1 to grain size",0,200,MPF_STATE,100};
CMachineParameter const paramL1Freq          = {"LFO1->Freq","LFO1 to grain freq",0,200,MPF_STATE,100};
CMachineParameter const paramL1Attack        = {"LFO1->Attack","LFO1 to attack",0,200,MPF_STATE,100};
CMachineParameter const paramL1Decay         = {"LFO1->Decay","LFO1 to decay",0,200,MPF_STATE,100};
CMachineParameter const paramL1PStart        = {"LFO1->PStart","LFO1 to pitch start",0,200,MPF_STATE,100};
CMachineParameter const paramL1PEnd          = {"LFO1->PEnd","LFO1 to pitch end",0,200,MPF_STATE,100};
CMachineParameter const paramL2Size          = {"LFO2->Size","LFO2 to grain size",0,200,MPF_STATE,100};
CMachineParameter const paramL2Freq          = {"LFO2->Freq","LFO2 to grain freq",0,200,MPF_STATE,100};
CMachineParameter const paramL2Attack        = {"LFO2->Attack","LFO2 to attack",0,200,MPF_STATE,100};
CMachineParameter const paramL2Decay         = {"LFO2->Decay","LFO2 to decay",0,200,MPF_STATE,100};
CMachineParameter const paramL2PStart        = {"LFO2->PStart","LFO2 to pitch start",0,200,MPF_STATE,100};
CMachineParameter const paramL2PEnd          = {"LFO2->PEnd","LFO2 to pitch end",0,200,MPF_STATE,100};
CMachineParameter const paramLFO1Shape       = {"LFO1 Shape","LFO1 Shape",0,5,MPF_STATE,0};
CMachineParameter const paramLFO2Shape       = {"LFO2 Shape","LFO2 Shape",0,5,MPF_STATE,0};
CMachineParameter const paramLFO1Speed       = {"LFO1 Frequency","LFO1 Speed",MIN_LSPEED,MAX_LSPEED,MPF_STATE,MAX_LSPEED/2};
CMachineParameter const paramLFO2Speed       = {"LFO2 Frequency","LFO2 Speed",MIN_LSPEED,MAX_LSPEED,MPF_STATE,MAX_LSPEED/2};
CMachineParameter const paramLFO1Phase       = {"LFO1 Phase","LFO1 Phase",0,360,MPF_STATE,0};
CMachineParameter const paramLFO2Phase       = {"LFO2 Phase","LFO2 Phase",0,360,MPF_STATE,0};
CMachineParameter const paramLimitThresh     = {"Limit Threshold","Limiter Threshold",0,MAX_THRESH,MPF_STATE,MAX_THRESH/2};
CMachineParameter const paramLimitAmt        = {"Limit amount","amount limited",0,32768,MPF_STATE,0};
CMachineParameter const paramDensity         = {"Density","grain density",0,10000,MPF_STATE,0};
CMachineParameter const paramOutGain         = {"Out Gain","output gain",0,MAX_GAIN,MPF_STATE,GAIN_NORM};
CMachineParameter const paramLFOSync         = {"LFO Syncer","Synchronize LFOs",-2,2,MPF_STATE,0};
CMachineParameter const paramGrainSync       = {"Grain Syncer","Synchronize Grains",0,1,MPF_STATE,0};

CMachineParameter const paramLabelSync       = {"        -Sync Tools-" , "Synchronization tools", 0,  0, MPF_LABEL, 0};
CMachineParameter const paramLabelDisplay    = {"         -Display-"   , "Display"              , 0,  0, MPF_LABEL, 1};
CMachineParameter const paramLabelGrainShape = {"        -Grain Shape-", "Grain Shape Controls" , 0,  0, MPF_LABEL, 1};
CMachineParameter const paramLabelRandMod    = {"        -Random Mod-" , "Random Modulation"    , 0,  0, MPF_LABEL, 1};
CMachineParameter const paramLabelLFO1Mod    = {"         -LFO1 Mod-"  , "LFO1 Modulation"      , 0,  0, MPF_LABEL, 1};
CMachineParameter const paramLabelLFO2Mod    = {"         -LFO2 Mod-"  , "LFO2 Modulation"      , 0,  0, MPF_LABEL, 1};
CMachineParameter const paramLabelAmpSection = {"        -Amp Section-", "Amp controls"         , 0,  0, MPF_LABEL, 1};
CMachineParameter const paramLabelLFOs       = {"            -LFOs-"   , "LFO Controls"         , 0,  0, MPF_LABEL, 1};
CMachineParameter const paramLabelBlank      = {"              -=-"    , ""                     , 0, -0, MPF_LABEL, 1};


CMachineParameter const *pParameters[] = 
{ 
	// global

	//column 1:
	&paramLabelGrainShape,
	&paramGrainSize,
	&paramGrainFreq,
	&paramAttack,
	&paramDecay,
	&paramPitchStart,
	&paramPitchEnd,
	&paramPitchLink,
	&paramMaxGrains,
	&paramNull,

	//column 2:
	&paramLabelRandMod,
	&paramRandSize,
	&paramRandFreq,
	&paramRandAttack,
	&paramRandDecay,
	&paramRandPStart,
	&paramRandPEnd,
	&paramLabelBlank,
	&paramLFO1Shape,
	&paramLFO2Shape,

	//column 3:
	&paramLabelLFO1Mod,
	&paramL1Size,
	&paramL1Freq,
	&paramL1Attack,
	&paramL1Decay,
	&paramL1PStart,
	&paramL1PEnd,
	&paramLabelLFOs,
	&paramLFO1Speed,
	&paramLFO2Speed,

	//column 4:
	&paramLabelLFO2Mod,
	&paramL2Size,
	&paramL2Freq,
	&paramL2Attack,
	&paramL2Decay,
	&paramL2PStart,
	&paramL2PEnd,
	&paramLabelBlank,
	&paramLFO1Phase,
	&paramLFO2Phase,


	//column 5:
	&paramLabelAmpSection,
	&paramLimitThresh,
	&paramOutGain,
	&paramLabelSync,
	&paramGrainSync,
	&paramLFOSync,
	&paramLabelDisplay,
	&paramLimitAmt,
	&paramDensity,
	&paramNull
};


CMachineInfo const MacInfo (
	MI_VERSION,				
	0x0100,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
	"dw granulizer"
		#ifndef NDEBUG
			" (Debug build)"
		#endif
		,
	"granulizer",
	"dw",
	"About",
	5
);

class GrainLayer {
	public:
		GrainLayer();
		~GrainLayer();
		float process(float samp, int channel);

		int size;
		int position;
		float relPos;
		bool in_use;

		int attack;
		int decay;

		float pitchCurrent;
		float pitchEnd;
		float pitchStep;

		std::vector<float> dataL;
		std::vector<float> dataR;

		std::vector<float> attackBuf;
		std::vector<float> decayBuf;
};

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

		//stolen
		static float SplineInterpolate(float zero, float minusone, float minustwo, unsigned int res);

		int _resolution;
		static float aTable[CUBIC_RESOLUTION];
		static float bTable[CUBIC_RESOLUTION];
		static float cTable[CUBIC_RESOLUTION];
		//stolen

	private:
		void StartLayer();
		void EndLayer(GrainLayer* layertoend);
		double GetLFO(int lfonum); // function wrapper
		double LFOSine(int lfonum);
		double LFOTriangle(int lfonum);
		double LFOSquare(int lfonum);
		double LFOSawUp(int lfonum);
		double LFOSawDown(int lfonum);
		double LFORandom(int lfonum);

		float gain;

		std::vector<GrainLayer*> layers;
		int counter, dispCounter;
		float lfo1counter, lfo2counter;
		float randvals1[NUM_RANDS];
		float randvals2[NUM_RANDS];
};


PSYCLE__PLUGIN__INSTANTIATOR("dw-granulizer", mi, MacInfo)


float mi::aTable[CUBIC_RESOLUTION];												// this feels soooo wrong
float mi::bTable[CUBIC_RESOLUTION];
float mi::cTable[CUBIC_RESOLUTION];


mi::mi()
{
	Vals = new int[MacInfo.numParameters];
	layers.resize(MAX_LAYERS);
	for(int i = 0; i < MAX_LAYERS; ++i)
	{
		layers[i]=new GrainLayer();
		layers[i]->in_use=false;
	}
	counter=0;
	dispCounter=0;
	lfo1counter=0;
	lfo2counter=0;
	for(int i=0;i<NUM_RANDS;++i)
	{
		randvals1[i] = ((rand() % 30000)/15000.0) - 1;
		randvals2[i] = ((rand() % 30000)/15000.0) - 1;
	}

	// stolen section
	_resolution = CUBIC_RESOLUTION;
	// Initialize tables
	for(int i=0; i<_resolution; i++)
	{
		float x = (float)i/(float)_resolution;
		aTable[i] = float(-0.5*x*x*x +     x*x - 0.5*x);
		bTable[i] = float( 1.5*x*x*x - 2.5*x*x         + 1);
		cTable[i] = float(-1.5*x*x*x + 2.0*x*x + 0.5*x);
	//				dTable[i] = float( 0.5*x*x*x - 0.5*x*x);
	}
	// stolen section

}

mi::~mi()
{
	delete[] Vals;
	for(int i = 0; i < MAX_LAYERS; ++i)
		delete layers[i];

}

void mi::Init()
{
	Vals[PRM_LIMITAMT]=0;
}

void mi::SequencerTick()
{
// Called on each tick while sequencer is playing
}

void mi::Command()
{
// Called when user presses editor button
pCB->MessBox("dw granulizer v. 0.1e\n\ninspired by granulab","uNF",0);
}

void mi::ParameterTweak(int par, int val)
{
	if(par != PRM_DENSITY && par != PRM_LIMITAMT)
		Vals[par] = val;

	if(par==PRM_LIMITAMT && val==0)																//avoid init problems, can probably be dealt with better..
		Vals[par]=0;

	if(Vals[PRM_PLINK] == LINK_ON)
	{
		switch(par)
		{
			case PRM_PSTART:				Vals[PRM_PEND] = val;
								break;
			case PRM_PEND:								Vals[PRM_PSTART] = val;
								break;
			case PRM_PLINK:								Vals[PRM_PEND] = Vals[PRM_PSTART];
								break;
		}
	}

	if(par==PRM_LFOSYNC)
	{
		switch(val)
		{
			case SYNC_L1:								lfo1counter=0;
								break;

			case SYNC_L2:								lfo2counter=0;
								break;

			case SYNC_BOTH1:
			case SYNC_BOTH2:				lfo1counter=0;
								lfo2counter=0;
								break;
		}
//								Vals[PRM_LFOSYNC] = SYNC_NONE;
	}

	if(par==PRM_GRAINSYNC && val==SYNC_GRAIN)
		counter=0;
							

	if(par==PRM_OUTGAIN)
	{
		gain=val/GAIN_NORM;
	}
	Vals[PRM_DENSITY] = Vals[PRM_SIZE] * 1000 / (Vals[PRM_FREQ]!=0?Vals[PRM_FREQ]:1);
}

void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks)
{
	int outDataL;
	int outDataR;

	int limitAmtL=0;
	int limitAmtR=0;

	int randFreq;
	int i;
	float srMultiplyer = pCB->GetSamplingRate()/44100.0f;								//this stuff should  maybe be moved somewhere where it's checked a bit less frequently
	float srDivider = 1.0f / (srMultiplyer==0? 1.0f: srMultiplyer);

	do{
		outDataL=0;
		outDataR=0;
		limitAmtL=0;
		limitAmtR=0;

		if(counter<=0)
		{
			randFreq = rand() % (MAX_FREQ-MIN_FREQ);
			counter = Vals[PRM_FREQ]+ (Vals[PRM_RANDFREQ]==0? 0: (randFreq-Vals[PRM_FREQ]) * (Vals[PRM_RANDFREQ]/100.0));

			if(Vals[PRM_L1TOFREQ] != 100)
				{counter += ((MAX_FREQ-MIN_FREQ)/2) * (GetLFO(LFO1) * ((Vals[PRM_L1TOFREQ]-100)/100.0));				}
			if(Vals[PRM_L2TOFREQ] != 100)
				{counter += ((MAX_FREQ-MIN_FREQ)/2) * (GetLFO(LFO2) * ((Vals[PRM_L2TOFREQ]-100)/100.0));				}
			
			if(counter>MAX_FREQ) counter=MAX_FREQ;				
			else if(counter<MIN_FREQ)				counter=MIN_FREQ;

			counter *= srMultiplyer;

			StartLayer();
		}


		//todo: the lfocounters should be floats, and instead of the lfospeed determining their max value, the speed should
		//determine the rate it's incremented towards a fixed max value.  the way it is currently, lfo speed is essentially
		//untweakable, but we need to find a way to switch the implementation over without changing the effective speed..
		if(lfo1counter >= (Vals[PRM_LFO1SPEED]) * LSPEED_MULTIPLYER)
		{
			lfo1counter = 0;
			for(i=0;i<NUM_RANDS;++i)
				randvals1[i] = ((rand() % 30000)/15000.0) - 1;
		}
		
		if(lfo2counter >= (Vals[PRM_LFO2SPEED]) * LSPEED_MULTIPLYER)
		{
			lfo2counter = 0;
			for(i=0;i<NUM_RANDS;++i)
				randvals2[i] = ((rand() % 30000)/15000.0) - 1;
		}
				
		for(int i=0;i<MAX_LAYERS;i++)
			if(layers[i]->in_use)
			{
				if(layers[i]->position < layers[i]->size)
				{
					outDataL += layers[i]->process(*psamplesleft, LCHAN);
					outDataR += layers[i]->process(*psamplesright, RCHAN);

					// these lines were in process(), i moved them here when i went stereo
					layers[i]->relPos += layers[i]->pitchCurrent;
					++layers[i]->position;
					layers[i]->pitchCurrent += layers[i]->pitchStep;
				} else 
				{
					EndLayer(layers[i]);
				}

			}

		
		if(abs(outDataL) > Vals[PRM_THRESH])
		{
			if(Vals[PRM_THRESH] !=0 )												// if threshold is 0, none of this matters (knob output will say 'inf')
			{
				if(dispCounter <= 0)
					limitAmtL = (abs(outDataL) / (float) Vals[PRM_THRESH]) * LIM_AMT_SCALER;
				outDataL = (outDataL > 0 ? Vals[PRM_THRESH] : -Vals[PRM_THRESH]);
			}
			else
				outDataL = 0;
		}

		if(abs(outDataR) > Vals[PRM_THRESH])
		{
			if(Vals[PRM_THRESH] != 0)
			{
				if(dispCounter <= 0)
					limitAmtR = (abs(outDataR) / (float) Vals[PRM_THRESH]) * LIM_AMT_SCALER;
				outDataR = (outDataR > 0 ? Vals[PRM_THRESH] : -Vals[PRM_THRESH]);
			}
			else
				outDataR = 0;
		}

		if(dispCounter<=0)
		{
			dispCounter = DISPLAY_REFRESH;
			Vals[PRM_LIMITAMT] = (limitAmtL > limitAmtR ? limitAmtL : limitAmtR);				


			Vals[PRM_LFOSYNC] = SYNC_NONE;				//<-- moved this here from mi::ParameterTweak() to give the user -some- chance of reading the response
			Vals[PRM_GRAINSYNC] = SYNC_NONE;
		}
			
		outDataL*=gain;
		outDataR*=gain;

		*psamplesleft=outDataL;
		*psamplesright=outDataR;


		++psamplesleft;
		++psamplesright;												
		--counter;
		--dispCounter;
		lfo1counter+=srDivider;
		lfo2counter+=srDivider;

	} while(--numsamples);
	
}

bool mi::DescribeValue(char* txt,int const param, int const value)
{
	double freqinsecs;								// used for PRM_FREQ, it won't compile unless it's declared here

	switch(param)
	{
		case PRM_DENSITY:				sprintf(txt, "%i%%", value / 10);
							return true;

		case PRM_PSTART:
		case PRM_PEND:								sprintf(txt, "%.1f%%", value / (float)MAX_PITCH * 100);
							return true;

		case PRM_PLINK:								if(value == LINK_ON) 
								strcpy(txt, "On");
							else 
								strcpy(txt, "Off");
							return true;

		case PRM_SIZE:
		case PRM_ATTACK:								// fall through..
		case PRM_DECAY:								//sprintf(txt, "%.1fms", value / (pCB->GetSamplingRate() / 1000.0) );
							sprintf(txt, "%.1fms", value / 44.1f );				//compensate for sr elsewhere--speed should be maintained when sr changes.
							return true;

		case PRM_FREQ:								///*float*/ freqinsecs = value / (float) pCB->GetSamplingRate();
							freqinsecs = value / 44100.0f;																//ditto
							if(freqinsecs>0.006)
								sprintf(txt, "1/%.1fms (%.2fgpm)", freqinsecs * 1000, 60.0/freqinsecs );
							else
								sprintf(txt, "1/%.1fms (lots of gpm)", freqinsecs * 1000);
							return true;

		case PRM_L2TOSIZE:
		case PRM_L2TOFREQ:
		case PRM_L2TOATTACK:
		case PRM_L2TODECAY:
		case PRM_L2TOPSTART:
		case PRM_L2TOPEND:
		case PRM_L1TOSIZE:
		case PRM_L1TOFREQ:
		case PRM_L1TOATTACK:
		case PRM_L1TODECAY:
		case PRM_L1TOPSTART:
		case PRM_L1TOPEND:				sprintf(txt, "%i%%", value - 100 );
							return true;

		case PRM_RANDSIZE:
		case PRM_RANDFREQ:
		case PRM_RANDATTACK:
		case PRM_RANDDECAY:
		case PRM_RANDPSTART:
		case PRM_RANDPEND:				sprintf(txt, "%i%%", value);
							return true;
							
		case PRM_OUTGAIN:				if(value!=0)
								sprintf(txt, "%i%% (%.2f dB)", (int)value, 20 * std::log10(value / (float) GAIN_NORM));
							else
								strcpy(txt,"-inf.");
							return true;

		case PRM_LIMITAMT:				if(Vals[PRM_THRESH] == 0)
								strcpy(txt, "inf dB");
							else if(value!=0)
								sprintf(txt, "%.2f dB", 20 * std::log10(value / (float) LIM_AMT_SCALER));
							else
								strcpy(txt,"0 dB");
							return true;

		case PRM_THRESH:				if(value!=0)
								sprintf(txt, "%.2f dB", 20 * std::log10(value / 32768.0));
							else
								strcpy(txt,"0 dB");
							return true;

		case PRM_LFO1SHAPE:
		case PRM_LFO2SHAPE:				switch(value)
							{
								case LFO_SINE:								strcpy(txt,"Sine");
													break;
								case LFO_SQUARE:				strcpy(txt,"Square");
													break;
								case LFO_TRIANGLE:				strcpy(txt,"Triangle");
													break;
								case LFO_SAWUP:								strcpy(txt,"Saw Up");
													break;
								case LFO_SAWDOWN:				strcpy(txt,"Saw Down");
													break;
								case LFO_RANDOM:				strcpy(txt,"Random");
													break;
							}
							return true;

		case PRM_LFO1PHASE:
		case PRM_LFO2PHASE:				sprintf(txt, "%i degrees", value);
							return true;

		case PRM_LFO1SPEED:
		case PRM_LFO2SPEED:				sprintf(txt, "%.3f seconds", (value * LSPEED_MULTIPLYER) / 44100.0f  );
							return true;

		case PRM_LFOSYNC:				switch(value)
							{
								case SYNC_NONE:								strcpy(txt, "<--LFO1  LFO2-->");
													return true;
								case SYNC_L1:								strcpy(txt, "LFO1 Restarted.");
													return true;
								case SYNC_L2:								strcpy(txt, "LFO2 Restarted.");
													return true;
								case SYNC_BOTH1:
								case SYNC_BOTH2:				strcpy(txt, "Both LFOs Restarted.");
													return true;
							}
							break;

		case PRM_GRAINSYNC: switch(value)
							{
								case SYNC_NONE:								strcpy(txt, "");
													return true;
								case SYNC_GRAIN:				strcpy(txt, "Grain Restarted.");
													return true;
							}
							break;
	}
	return false;
}


////////////////////////////////////////////////////////////////////////////////////
//  StartLayer

void mi::StartLayer()
{
	int next;

	for(next = 0; next < Vals[PRM_MAXGRAINS] && layers[next]->in_use; ++next);
	if(next==Vals[PRM_MAXGRAINS]) return;				//out of layers-- just ignore the request, and avoid a big mess

	int i;
	float srMultiplyer = pCB->GetSamplingRate()/44100.0f;
	float splineX;


	float length = Vals[PRM_SIZE];
	float attack = Vals[PRM_ATTACK];
	float decay  = Vals[PRM_DECAY];
	float pstart = Vals[PRM_PSTART];
	float pend   = Vals[PRM_PEND];

	float randLength=rand() % (MAX_LENGTH-MIN_LENGTH)				+ MIN_LENGTH;
	float randAttack=rand() % (MAX_ATTACK-MIN_ATTACK)				+ MIN_ATTACK;
	float randDecay =rand() % (MAX_DECAY-MIN_DECAY)								+ MIN_DECAY;
	float randPStart=rand() % (MAX_PITCH-MIN_PITCH)								+ MIN_PITCH;
	float randPEnd  =rand() % (MAX_PITCH-MIN_PITCH)								+ MIN_PITCH;

	if(Vals[PRM_RANDSIZE] != 0)
		length += (randLength-length)				* (Vals[PRM_RANDSIZE]/100.0);
	if(Vals[PRM_RANDATTACK] != 0)
		attack += (randAttack-attack)				* (Vals[PRM_RANDATTACK]/100.0);
	if(Vals[PRM_RANDDECAY] != 0)
		decay += (randDecay-decay)								* (Vals[PRM_RANDDECAY]/100.0);
	if(Vals[PRM_RANDPSTART] != 0)
		pstart += (randPStart-pstart)				* (Vals[PRM_RANDPSTART]/100.0);
	if(Vals[PRM_RANDPEND] != 0)
		pend += (randPEnd-pend)												* (Vals[PRM_RANDPEND]/100.0);


	if(Vals[PRM_L1TOSIZE] != 100)
		{length += ((MAX_LENGTH-MIN_LENGTH)/2) * (GetLFO(LFO1) * ((Vals[PRM_L1TOSIZE]-100)/100.0));				}
	if(Vals[PRM_L1TOATTACK] != 100)
		{attack += ((MAX_ATTACK-MIN_ATTACK)/2) * (GetLFO(LFO1) * ((Vals[PRM_L1TOATTACK]-100)/100.0));				}
	if(Vals[PRM_L1TODECAY] != 100)
		{decay +=  ((MAX_DECAY-MIN_DECAY)/2)   * (GetLFO(LFO1) * ((Vals[PRM_L1TODECAY]-100)/100.0));				}
	if(Vals[PRM_L1TOPSTART] != 100)
		{pstart += ((MAX_PITCH-MIN_PITCH)/2)   * (GetLFO(LFO1) * ((Vals[PRM_L1TOPSTART]-100)/100.0));				}
	if(Vals[PRM_L1TOPEND] != 100)
		{pend +=   ((MAX_PITCH-MIN_PITCH)/2)   * (GetLFO(LFO1) * ((Vals[PRM_L1TOPEND]-100)/100.0));				}

	if(Vals[PRM_L2TOSIZE] != 100)
		{length += ((MAX_LENGTH-MIN_LENGTH)/2) * (GetLFO(LFO2) * ((Vals[PRM_L2TOSIZE]-100)/100.0));				}
	if(Vals[PRM_L2TOATTACK] != 100)
		{attack += ((MAX_ATTACK-MIN_ATTACK)/2) * (GetLFO(LFO2) * ((Vals[PRM_L2TOATTACK]-100)/100.0));				}
	if(Vals[PRM_L2TODECAY] != 100)
		{decay +=  ((MAX_DECAY-MIN_DECAY)/2)   * (GetLFO(LFO2) * ((Vals[PRM_L2TODECAY]-100)/100.0));				}
	if(Vals[PRM_L2TOPSTART] != 100)
		{pstart += ((MAX_PITCH-MIN_PITCH)/2)   * (GetLFO(LFO2) * ((Vals[PRM_L2TOPSTART]-100)/100.0));				}
	if(Vals[PRM_L2TOPEND] != 100)
		{pend +=   ((MAX_PITCH-MIN_PITCH)/2)   * (GetLFO(LFO2) * ((Vals[PRM_L2TOPEND]-100)/100.0));				}

	
	if(length>MAX_LENGTH) 				length=MAX_LENGTH;				
	else if(length<MIN_LENGTH)				length=MIN_LENGTH;
	if(attack>MAX_ATTACK)				attack=MAX_ATTACK;				
	else if(attack<MIN_ATTACK)				attack=MIN_ATTACK;				
	if(decay>MAX_DECAY)								decay =MAX_DECAY;								
	else if(decay<MIN_DECAY)				decay=MIN_DECAY;				
	if(pstart>MAX_PITCH)				pstart=MAX_PITCH;								
	else if(pstart<MIN_PITCH)				pstart=MIN_PITCH;				
	if(pend>MAX_PITCH)								pend=MAX_PITCH;								
	else if(pend<MIN_PITCH)								pend=MIN_PITCH;								

	length*=srMultiplyer;
	attack*=srMultiplyer;
	decay*=srMultiplyer;

	if(layers[next]->in_use==false)
	{
		layers[next]->dataL.resize((int)length+1);
		layers[next]->dataR.resize((int)length+1);
		layers[next]->size												= (int)(length);
		layers[next]->position								= 0;
		layers[next]->relPos								= 0;
		layers[next]->in_use								= true;
		layers[next]->attack								= (int)(attack);
		layers[next]->decay												= (int)(decay);

		//				generate splines for attack/release envelopes (this function is completely arbitrary, but it sounds much better than linear..)
		layers[next]->attackBuf.clear();
		for(i=0;i<(int)(attack);++i)				
		{
			splineX = (float)i / attack;																																																// 0 <= x <= 1
			if(splineX < 0.4)				layers[next]->attackBuf.push_back(splineX * splineX);																								// x < .4:								x^2
			else if(splineX>0.6)layers[next]->attackBuf.push_back(1 - ((splineX - 1) * (splineX - 1)));				// x > .6:								1 - (x - 1)^2
			else	layers[next]->attackBuf.push_back(3.4 * splineX - 1.2);																				// .4<=x<=.6:				3.4x - 1.2
		}
		layers[next]->decayBuf.clear();
		for(i=0;i<(int)(decay);++i)
		{
			splineX = (float)i / decay;																																																				// 0 <= x <= 1
			if(splineX < 0.4)				layers[next]->decayBuf.push_back(1 - ((splineX) * (splineX)));				// x < .4:								1-(-x)^2 = 1-x^2
			else if(splineX>0.6)layers[next]->decayBuf.push_back((1-splineX) * (1-splineX));								// x > .6:								(1-x)^2
			else	layers[next]->decayBuf.push_back(3.4 * (1-splineX) - 1.2);								// .4<=x<=.6:				3.4(1-x) - 1.2
		}

		layers[next]->pitchCurrent				= (pstart / (float)MAX_PITCH);
		layers[next]->pitchEnd								= (pend				/ (float)MAX_PITCH);
		layers[next]->pitchStep								= (layers[next]->pitchEnd - layers[next]->pitchCurrent) /(length!=0 ? length : 0.0001);
	}
	
}

void mi::EndLayer(GrainLayer* layertoend)
{
	layertoend->in_use=false;
}





///////////////////////////////////////////////////////////////////////////////
//   GrainLayer funcs

GrainLayer::GrainLayer()
{
	attackBuf.reserve(MAX_ATTACK+1);
	decayBuf.reserve(MAX_ATTACK+1);


}

GrainLayer::~GrainLayer()
{}

float GrainLayer::process(float samp, int channel)
{
	float destSamp;
	
	if(channel==LCHAN)
	{
		dataL[position]=samp;

		if(position==0) destSamp=samp;
		else
		{
//												destSamp =( dataL[(int)floor(relPos)] + (dataL[(int)floor(relPos)] -dataL[(int)ceil(relPos)]) * (relPos - floor(relPos)));
			destSamp = mi::SplineInterpolate(				dataL[(int)ceil(relPos)], 
												dataL[(int)floor(relPos)], 
												(floor(relPos-1) >= 0 ? dataL[(int)floor(relPos-1)] : 0), 
												relPos-floor(relPos) * CUBIC_RESOLUTION);
		}
	}
	else
	{
		dataR[position]=samp;

		if(position==0) destSamp=samp;
		else
		{
//												destSamp =( dataR[(int)floor(relPos)] + (dataR[(int)floor(relPos)] -dataR[(int)ceil(relPos)]) * (relPos - floor(relPos)));
			destSamp = mi::SplineInterpolate(				dataR[(int)ceil(relPos)], 
												dataR[(int)floor(relPos)], 
												(floor(relPos-1) >= 0 ? dataR[(int)floor(relPos-1)] : 0), 
												relPos-floor(relPos) * CUBIC_RESOLUTION);
		}
	}

	if(position<attack)
		destSamp*=attackBuf[position];
//								destSamp*=attackStep * position;								//not linear no more

	if(position>size-decay)
		destSamp *= decayBuf[decay-(size-position)];
//								destSamp*=decayStep * (size-position);

/*
	// this bit's been moved to mi::work() to support stereo

	relPos+=pitchCurrent;
	++position;
	pitchCurrent+=pitchStep;
*/
	return destSamp;
}




////////////////////////////////////////////////////////////////////////////////////////
// LFO functions


double mi::GetLFO(int lfonum)																//function wrapper
{
	switch(lfonum)
	{
		case LFO1: switch(Vals[PRM_LFO1SHAPE])
					{
						case LFO_SINE:								return LFOSine(lfonum);
						case LFO_SQUARE:				return LFOSquare(lfonum);
						case LFO_TRIANGLE:				return LFOTriangle(lfonum);
						case LFO_SAWUP:								return LFOSawUp(lfonum);
						case LFO_SAWDOWN:				return LFOSawDown(lfonum);
						case LFO_RANDOM:				return LFORandom(lfonum);
						default:												return 0.0;
					}
					break;												// it feels weird without it.. :)
		case LFO2: switch(Vals[PRM_LFO2SHAPE])
					{
						case LFO_SINE:								return LFOSine(lfonum);
						case LFO_SQUARE:				return LFOSquare(lfonum);
						case LFO_TRIANGLE:				return LFOTriangle(lfonum);
						case LFO_SAWUP:								return LFOSawUp(lfonum);
						case LFO_SAWDOWN:				return LFOSawDown(lfonum);
						case LFO_RANDOM:				return LFORandom(lfonum);
						default:												return 0.0;
					}
					break;
		default:				return 0.0;																//we should never get here, but just in case
	}
}

double mi::LFOSine(int lfonum)
{
	int lfolength;
	int lfopos;
	switch(lfonum)
	{
	case LFO1:				lfolength = Vals[PRM_LFO1SPEED] * LSPEED_MULTIPLYER;
				lfopos=((int)((Vals[PRM_LFO1PHASE] *lfolength/360.0) + lfo1counter)) % lfolength ;
				return sin(TWOPI * lfopos / (double) lfolength);

	case LFO2:				lfolength = (Vals[PRM_LFO2SPEED]) * LSPEED_MULTIPLYER;
				lfopos=((int)((Vals[PRM_LFO2PHASE] *lfolength/360.0) + lfo2counter)) % lfolength ;
				return sin(TWOPI * lfopos / (double) lfolength);

	default:				return 0.0;
	}
	
}

double mi::LFOTriangle(int lfonum)
{
	int lfolength;
	int lfopos;
	int halflen;
	switch(lfonum)
	{
		case LFO1:				lfolength = (Vals[PRM_LFO1SPEED]) * LSPEED_MULTIPLYER;
					halflen = (int) lfolength / 2;
					lfopos = ((int)((Vals[PRM_LFO1PHASE] * lfolength/360.0) + lfo1counter)) % lfolength;
					if(lfopos<=halflen)
						return ((lfopos * 2) / (float) halflen) - 1;
					else
						return 1-(((lfopos-halflen) * 2) / (float) halflen);

		case LFO2:				lfolength = (Vals[PRM_LFO2SPEED]) * LSPEED_MULTIPLYER;
					halflen = (int) lfolength / 2;
					lfopos = ((int)((Vals[PRM_LFO2PHASE] * lfolength/360.0) + lfo2counter)) % lfolength;
					if(lfopos<=halflen)
						return ((lfopos * 2) / (float) halflen) - 1;
					else
						return 1-(((lfopos-halflen) * 2) / (float) halflen);

		default:				return 0.0;
	}

}

double mi::LFOSquare(int lfonum)
{
	int lfolength;
	int lfopos;
	switch(lfonum)
	{
		case LFO1:				lfolength = (Vals[PRM_LFO1SPEED]) * LSPEED_MULTIPLYER;
					lfopos = ((int)((Vals[PRM_LFO1PHASE] * lfolength/360.0) + lfo1counter)) % lfolength;
					if(lfopos < lfolength / 2.0)
						return 1.0;
					else
						return -1.0;

		case LFO2:				lfolength = (Vals[PRM_LFO2SPEED]) * LSPEED_MULTIPLYER;
					lfopos = ((int)((Vals[PRM_LFO2PHASE] * lfolength/360.0) + lfo2counter)) % lfolength;
					if(lfopos < lfolength / 2.0)
						return 1.0;
					else
						return -1.0;

		default:				return 0.0;
	}
}


double mi::LFOSawUp(int lfonum)
{
	int lfolength;
	int lfopos;
	switch(lfonum)
	{
		case LFO1:				lfolength = (Vals[PRM_LFO1SPEED]) * LSPEED_MULTIPLYER;
					lfopos = ((int)((Vals[PRM_LFO1PHASE] * lfolength/360.0) + lfo1counter)) % lfolength;
					return (lfopos * 2.0 / lfolength) - 1;

		case LFO2:				lfolength = (Vals[PRM_LFO2SPEED]) * LSPEED_MULTIPLYER;
					lfopos = ((int)((Vals[PRM_LFO2PHASE] * lfolength/360.0) + lfo2counter)) % lfolength;
					return (lfopos * 2.0 / lfolength) - 1;

		default:				return 0.0;
	}
}

double mi::LFOSawDown(int lfonum)
{
	int lfolength;
	int lfopos;
	switch(lfonum)
	{
		case LFO1:				lfolength = (Vals[PRM_LFO1SPEED]) * LSPEED_MULTIPLYER;
					lfopos = ((int)((Vals[PRM_LFO1PHASE] * lfolength/360.0) + lfo1counter)) % lfolength;
					return 1 - (lfopos * 2.0 / lfolength);

		case LFO2:				lfolength = (Vals[PRM_LFO2SPEED]) * LSPEED_MULTIPLYER;
					lfopos = ((int)((Vals[PRM_LFO2PHASE] * lfolength/360.0) + lfo2counter)) % lfolength;
					return 1 - (lfopos * 2.0 / lfolength);

		default:				return 0.0;
	}
}

double mi::LFORandom(int lfonum)
{
	int lfolength;
	int lfopos;
	switch(lfonum)
	{
		case LFO1:				lfolength = (Vals[PRM_LFO1SPEED]) * LSPEED_MULTIPLYER;
					lfopos = ((int)((Vals[PRM_LFO1PHASE] * lfolength/360.0) + lfo1counter)) % lfolength;
					lfopos = (int)((lfopos / (float) lfolength) * NUM_RANDS);								//lfopos/lfolength returns in range [0, 1).. times NUM_RANDS scales to [0, NUM_RANDS)
					return randvals1[lfopos];																																								// casting to int truncates, and it (hopefully) splits lfolength into NUM_RANDS even-sized slices

		case LFO2:				lfolength = (Vals[PRM_LFO2SPEED]) * LSPEED_MULTIPLYER;
					lfopos = ((int)((Vals[PRM_LFO2PHASE] * lfolength/360.0) + lfo2counter)) % lfolength;
					lfopos = (int)((lfopos / (float) lfolength) * NUM_RANDS);
					return randvals2[lfopos];

		default:				return 0.0;
	}
}


float mi::SplineInterpolate(float zero, float minusone, float minustwo, unsigned int res)
{
	float yo, y0,y1, y2;
	res = res >> 21;


//				yo=(offset==0)?0:*(pData-1);
//				y0=*(pData);
//				y1=(offset+1 == length)?0:*(pData+1);
//				y2=(offset+2 == length)?0:*(pData+2);

//				yo=(offset<=1)?0: *(pData-2);
//				y0=(offset==0)? 0: *(pData-1);
//				y1=*pData;
//				y2=0;

yo=minustwo;
y0=minusone;
y1=zero;
y2=0;


	return (aTable[res]*yo+bTable[res]*y0+cTable[res]*y1  /*+_dTable[res]*y2*/   );
}

			// yo = y[-1] [sample at x-1]
			// y0 = y[0]  [sample at x (input)]
			// y1 = y[1]  [sample at x+1]
			// y2 = y[2]  [sample at x+2]
			
			// res= distance between two neighboughing sample points [y0 and y1] 
			//								,so [0...1.0]. You have to multiply this distance * RESOLUTION used
			//								on the spline conversion table. [2048 by default]
			// If you are using 2048 is asumed you are using 12 bit decimal
			// fixed point offsets for resampling.
			
			// offset = sample offset [info to avoid go out of bounds on sample reading ]
			// length = sample length [info to avoid go out of bounds on sample reading ]
			
			/// Currently is 2048

}