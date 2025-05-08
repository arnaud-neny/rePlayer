//////////////////////////////////////////////////////////////////////
// MoreAmp EQ effect plugin for PSYCLE by Sartorius
//
//   Original

/*
	* MoreAmp
	* Copyright (C) 2004-2005 pmisteli
	*
	* This program is free software; you can redistribute it and/or modify
	* it under the terms of the GNU General Public License as published by
	* the Free Software Foundation; either version 2 of the License, or
	* (at your option) any later version.
	*
	* This program is distributed in the hope that it will be useful,
	* but WITHOUT ANY WARRANTY; without even the implied warranty of
	* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	* GNU General Public License for more details.
	*
	* You should have received a copy of the GNU General Public License
	* along with this program; if not, write to the Free Software
	* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
	*
	*/

/*
	*   PCM time-domain equalizer
	*
	*   Copyright (C) 2002-2006  Felipe Rivera <liebremx at users.sourceforge.net>
	*
	*   This program is free software; you can redistribute it and/or modify
	*   it under the terms of the GNU General Public License as published by
	*   the Free Software Foundation; either version 2 of the License, or
	*   (at your option) any later version.
	*
	*   This program is distributed in the hope that it will be useful,
	*   but WITHOUT ANY WARRANTY; without even the implied warranty of
	*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	*   GNU General Public License for more details.
	*
	*   You should have received a copy of the GNU General Public License
	*   along with this program; if not, write to the Free Software
	*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
	*
	*   $Id: moreamp_eq.cpp 3616 2006-12-17 20:15:14Z johan-boule $$
	*/
#include <psycle/plugin_interface.hpp>
#include "maEqualizer.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
namespace psycle::plugins::moreamp_eq {
using namespace psycle::plugin_interface;

static char buffer[] = 
		"Extra filtering:"\
		"\nSet this to enable extra-filtering,"\
		"\non either 31-band or 10-band equalization."\
		"\nNote that this doubles the processor time for filtering."\
		"\n"\
		"\nLink:"\
		"\nIf this is on and a knob with a (*) is moved,"\
		"\nthen the 2 knobs on either side will be linked to it and move as well."\
		"\nThis means that you can set a fairly smooth"\
		"\n31-point curve by moving only ten knobs."\
		"\nNote that 31 knobs are displayed even when the Bands is 10,"\
		"\nin this case only the ten knobs with a (*) are active."\
		"\n";
static bool equalizerInitialized = false;

static sIIRCoefficients MSVC_ALIGN iir_cf10[EQBANDS10] GNU_ALIGN;

static sIIRCoefficients MSVC_ALIGN iir_cf31[EQBANDS31] GNU_ALIGN;

static const double band_f010[] = { 31.,62.,125.,250.,500.,1000.,2000.,4000.,8000.,16000. };

static const double band_f031[] = { 20.,25.,31.5,40.,50.,63.,80.,100.,125.,160.,200.,250.,315.,400.,500.,630.,800.,1000.,1250.,1600.,2000.,2500.,3150.,4000.,5000.,6300.,8000.,10000.,12500.,16000.,20000. };

band bands[] =
{
	{ iir_cf10,	band_f010, 1.0, 10 },
	{ iir_cf31,	band_f031, 1.0/3.0, 31 },
	{ 0 }
};

static const float slidertodb[] =
{
	-16.0, -15.5, -15.0, -14.5, -14.0, -13.5, -13.0, -12.5, -12.0, -11.5, -11.0, -10.5, -10.0,  -9.5, -9.0, -8.5,
	-8.0,  -7.5,  -7.0,  -6.5,  -6.0,  -5.5,  -5.0,  -4.5,  -4.0,  -3.5,  -3.0,  -2.5,  -2.0,  -1.5, -1.0, -0.5,
	0.0,   0.5,   1.0,   1.5,   2.0,   2.5,   3.0,   3.5,   4.0,   4.5,   5.0,   5.5,   6.0,   6.5,  7.0,  7.5,
	8.0,   8.5,   9.0,   9.5,  10.0,  10.5,  11.0,  11.5,  12.0,  12.5,  13.0,  13.5,  14.0,  14.5, 15.0, 15.5,
	16.0
};

inline float DBfromScaleGain(int value)
{
	if(value >= EQSLIDERMIN && value <= EQSLIDERMAX)
			return slidertodb[value];
	return 0.0f;
}

CMachineParameter const paraB20 = { "20 Hz","20 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB25 = { "25 Hz","25 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB31 = { "31.5 Hz *","* 31.5 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB40 = { "40 Hz","40 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB50 = { "50 Hz","50 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB62 = { "62.5 Hz *","* 62.5 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB80 = { "80 Hz","80 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB100 = { "100 Hz","100 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB125 = { "125 Hz *","* 125 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB160 = { "160 Hz","160 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB200 = { "200 Hz","200 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB250 = { "250 Hz *","* 250 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB315 = { "315 Hz","315 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB400 = { "400 Hz","400 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB500 = { "500 Hz *","* 500 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB630 = { "630 Hz","630 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB800 = { "800 Hz","800 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB1000 = { "1000 Hz *","* 1000 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB1250 = { "1250 Hz","1250 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32};
CMachineParameter const paraB1600 = { "1600 Hz","1600 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB2000 = { "2000 Hz *","* 2000 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB2500 = { "2500 Hz","2500 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32, };
CMachineParameter const paraB3150 = { "3150 Hz","3150 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB4000 = { "4000 Hz *","* 4000 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB5000 = { "5000 Hz","5000 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB6300 = { "6300 Hz","6300 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB8000 = { "8000 Hz *","* 8000 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB10000 = { "10000 Hz","10000 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB12500 = { "12500 Hz","12500 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB16000 = { "16000 Hz *","* 16000 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };
CMachineParameter const paraB20000 = { "20000 Hz","20000 Hz",EQSLIDERMIN,EQSLIDERMAX,MPF_STATE,32 };

CMachineParameter const paraDiv = { "Parametrization","Parametrization",0,0,MPF_LABEL,0 };
CMachineParameter const paraPreAmp = { "Preamp","Preamp",-16,16,MPF_STATE,0 };
CMachineParameter const paraBands = { "Bands","Bands",0,1,MPF_STATE,0 };
CMachineParameter const paraExtra = { "Extra filtering","Extra filtering",0,1,MPF_STATE,0 };
CMachineParameter const paraLink = { "Link *","Link",0,1,MPF_STATE,0 };

CMachineParameter const *pParameters[] = 
{ 
	// global
		&paraB20,
		&paraB25,
		&paraB31,
		&paraB40,
		&paraB50,
		&paraB62,
		&paraB80,
		&paraB100,
		&paraB125,
		&paraB160,
		&paraB200,
		&paraB250,
		&paraB315,
		&paraB400,
		&paraB500,
		&paraB630,
		&paraB800,
		&paraB1000,
		&paraB1250,
		&paraB1600,
		&paraB2000,
		&paraB2500,
		&paraB3150,
		&paraB4000,
		&paraB5000,
		&paraB6300,
		&paraB8000,
		&paraB10000,
		&paraB12500,
		&paraB16000,
		&paraB20000,
		&paraDiv,
		&paraPreAmp,
		&paraBands,
		&paraExtra,
		&paraLink
};

CMachineInfo const MacInfo (
	MI_VERSION,
	0x0100,
	EFFECT,
	sizeof pParameters / sizeof *pParameters,
	pParameters,
#ifdef _DEBUG
	"MoreAmp EQ (Debug build)",
#else
	"MoreAmp EQ",
#endif
	"maEQ",
	"Felipe Rivera/pmisteli/Sartorius",
	"Help",
	3
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
	void maEqualizerReset();
	void maEqualizerSetLevels();
	void maEqualizerSetPreamp();

	void worksse(float *psamplesleft, float *psamplesright, int numSamples);

	void calc_coeffs();
	inline void find_f1_and_f2(double f0, double octave_percent, double *f1, double *f2);
	inline int find_root(double a, double b, double c, double *x0);
	void clean_history();

	void load_coeffssse(int);
	void set_gainsse(int,int,float);
	void setup_gainsse(int);
	void clean_historysse();

	sIIRCoefficients *iir_cf;
	
	/* History for two filters */
	sXYData MSVC_ALIGN data_history[EQ_MAX_BANDS][EQ_CHANNELS] GNU_ALIGN;
	sXYData MSVC_ALIGN data_history2[EQ_MAX_BANDS][EQ_CHANNELS] GNU_ALIGN;

	float gains31[EQBANDS31];
	float gains10[EQBANDS31];

	float *gains;
	float preamp;

	float dither[MAX_BUFFER_LENGTH];

	int g_eqi, g_eqj, g_eqk;
	short bnds;
	int di;
	int srate;
};


PSYCLE__PLUGIN__INSTANTIATOR("maeq", mi, MacInfo)

mi::mi()
{
	// The constructor zone
	Vals = new int[MacInfo.numParameters];
	gains = this->gains10;
	//iir_cf = new sIIRCoefficients;
	iir_cf = NULL;
	srate= -1;
}

mi::~mi()
{
	delete [] Vals;
}

void mi::Init()
{
// Initialize your stuff here

	bool forceupdate = ( srate != pCB->GetSamplingRate() );
	srate = pCB->GetSamplingRate();

	bnds = EQBANDS10;

	if (!equalizerInitialized || forceupdate ) {
		calc_coeffs();

		clean_history();
	}

	maEqualizerSetLevels();

	maEqualizerReset();

}

void mi::SequencerTick()
{
// Called on each tick while sequencer is playing
	if(srate != pCB->GetSamplingRate())
	{
		srate = pCB->GetSamplingRate();
		calc_coeffs();
		clean_history();
	}
}

void mi::Command()
{
	// Called when user presses editor button
	// Probably you want to show your custom window here
	// or an about button
	pCB->MessBox(&buffer[0],"MoreAmp EQ",0);
}

void mi::ParameterTweak(int par, int val)
{
	int delta;
	Vals[par]=val;

	// terrible construction... 
	if (Vals[LINK]==1)
	{
		switch(par)
		{
			case B31:
				
			Vals[B20] = Vals[B25] = val;
					
			delta = (val - Vals[B62]) / 3;
			
			Vals[B40] = Vals[B62] + delta + delta;
						
			Vals[B50] = Vals[B62] + delta;								
			break;

			case B62:
			case B125:
			case B250:
			case B500:
			case B1000:
			case B2000:
			case B4000:
			case B8000:
				
				delta = (Vals[par] - Vals[par - 3]) / 3;

				Vals[par-2] = Vals[par - 3] + delta;								
				
				Vals[par - 1] = Vals[par - 3] + delta + delta;
							
				delta = (Vals[par] - Vals[par + 3]) / 3;
				
				Vals[par+1] = Vals[par + 3] + delta + delta;
							
				Vals[par+2] = Vals[par + 3] + delta;								
			break;

			case B16000:

				delta = (Vals[B16000] - Vals[B8000]) / 3;
				
				Vals[B10000] = Vals[B8000] + delta;
							
				Vals[B12500] = Vals[B8000] + delta + delta;
					
				Vals[B20000]= val;								
			break;

			default:
				break;
		}
	}
	if (par<_DIV)
	{
		this->maEqualizerSetLevels();
	} else 
	{
		switch(par)
		{
			case _DIV: break;
			case PRE_AMP: maEqualizerSetPreamp(); break;
			case BANDS: bnds = (val==0?EQBANDS10:EQBANDS31); this->maEqualizerReset(); break;
			//case EXTRA: extra = (val==1); break;
			//case LINK: lnk = val; break;
			default:
				break;
		}
	}
	
}

// Work... where all is cooked 
void mi::Work(float *psamplesleft, float *psamplesright , int numsamples, int tracks)
{
	short index, band;
	float out_l,out_r;

	int const extra = Vals[EXTRA];

	/**
		* IIR filter equation is
		* y[n] = 2 * (alpha*(x[n]-x[n-2]) + gamma*y[n-1] - beta*y[n-2])
		*
		* NOTE: The 2 factor was introduced in the coefficients to save
		* 												a multiplication
		*
		* This algorithm cascades two filters to get nice filtering
		* at the expense of extra CPU cycles
		*/
	/* 16bit, 2 bytes per sample, so divide by two the length of
		* the buffer (length is in bytes)
		*/

	if(extra)
	{
			for(index = 0; index < numsamples; index++)
			{
					out_l = out_r = 0;
					
			// denormalization via random noise
			float const leftsample = (*psamplesleft*preamp); 
			float const rightsample = (*psamplesright*preamp);
			float const leftsampledither = leftsample + dither[di];
			float const rightsampledither = rightsample + dither[di];
					/* For each band */
					for(band = 0; band < bnds; band++)
					{
						// left ch

						/* Store Xi(n) */
				data_history[band][0].x[g_eqi] = leftsampledither;
						/* Calculate and store Yi(n) */
			
						data_history[band][0].y[g_eqi] =
						(
						/* 								= alpha * [x(n)-x(n-2)] */
								iir_cf[band].alpha * ( data_history[band][0].x[g_eqi]
						- data_history[band][0].x[g_eqk])
						/* 								+ gamma * y(n-1) */
						+ iir_cf[band].gamma * data_history[band][0].y[g_eqj]
						/* 								- beta * y(n-2) */
						- iir_cf[band].beta * data_history[band][0].y[g_eqk]
								);

						/* Apply the gain  */
				out_l +=  data_history[band][0].y[g_eqi] * gains[band];

						// right ch
						/* Store Xi(n) */
				data_history[band][1].x[g_eqi] = rightsampledither;
						/* Calculate and store Yi(n) */
			
						data_history[band][1].y[g_eqi] =
							(
						/* 								= alpha * [x(n)-x(n-2)] */
								iir_cf[band].alpha * ( data_history[band][1].x[g_eqi]
								-  data_history[band][1].x[g_eqk])
						/* 								+ gamma * y(n-1) */
							+ iir_cf[band].gamma * data_history[band][1].y[g_eqj]
						/* 								- beta * y(n-2) */
							- iir_cf[band].beta * data_history[band][1].y[g_eqk]
				);

						/* Apply the gain  */
				out_r +=  data_history[band][1].y[g_eqi] * gains[band];

					} /* For each band */

					/* Filter the sample again */
					for(band = 0; band < bnds; band++)
					{
						// left ch
						/* Store Xi(n) */
						data_history2[band][0].x[g_eqi] = out_l;

						/* Calculate and store Yi(n) */
						data_history2[band][0].y[g_eqi] = 
						(
						/* y(n) = alpha * [x(n)-x(n-2)] */
								iir_cf[band].alpha * (data_history2[band][0].x[g_eqi]
							-  data_history2[band][0].x[g_eqk])
						/*     + gamma * y(n-1) */
								+ iir_cf[band].gamma * data_history2[band][0].y[g_eqj]
							/* 								- beta * y(n-2) */
							- iir_cf[band].beta * data_history2[band][0].y[g_eqk]
								);

						/* Apply the gain */
						out_l +=  data_history2[band][0].y[g_eqi]*gains[band];

						// right ch
						/* Store Xi(n) */
						data_history2[band][1].x[g_eqi] = out_r;

						/* Calculate and store Yi(n) */
						data_history2[band][1].y[g_eqi] = 
						(
						/* y(n) = alpha * [x(n)-x(n-2)] */
								iir_cf[band].alpha * (data_history2[band][1].x[g_eqi]
							-  data_history2[band][1].x[g_eqk])
						/* 				    + gamma * y(n-1) */
						+ iir_cf[band].gamma * data_history2[band][1].y[g_eqj]
						/* 								- beta * y(n-2) */
								- iir_cf[band].beta * data_history2[band][1].y[g_eqk]
								);

						/* Apply the gain */
						out_r +=  data_history2[band][1].y[g_eqi]*gains[band];

					} /* For each band */
					/* Volume stuff
					Scale down original PCM sample and add it to the filters 
					output. This substitutes the multiplication by 0.25
					*/

			*psamplesleft = out_l + (leftsample*0.25f);
			*psamplesright = out_r + (rightsample*0.25f);

				g_eqi = (g_eqi + 1) % 3;
				g_eqj = (g_eqj + 1) % 3;
				g_eqk = (g_eqk + 1) % 3;

				di = (di + 1) % MAX_BUFFER_LENGTH;

				++psamplesleft;
				++psamplesright;
				
			}/* For each pair of samples */
	} else {
			for(index = 0; index < numsamples; index++)
			{
					out_l = out_r = 0;
			// denormalization via random noise
			float const leftsample = (*psamplesleft*preamp); 
			float const rightsample = (*psamplesright*preamp);
			float const leftsampledither = leftsample + dither[di];
			float const rightsampledither = rightsample + dither[di];
					/* For each band */
					for(band = 0; band < bnds; band++)
					{
						// left ch
						/* Store Xi(n) */
				data_history[band][0].x[g_eqi] = leftsampledither;

						/* Calculate and store Yi(n) */
						data_history[band][0].y[g_eqi] =
						(
						/* 								= alpha * [x(n)-x(n-2)] */
								iir_cf[band].alpha * ( data_history[band][0].x[g_eqi]
								-  data_history[band][0].x[g_eqk])
						/* 								+ gamma * y(n-1) */
								+ iir_cf[band].gamma * data_history[band][0].y[g_eqj]
						/* 								- beta * y(n-2) */
								- iir_cf[band].beta * data_history[band][0].y[g_eqk]
								);

						/* Apply the gain  */
				out_l +=  data_history[band][0].y[g_eqi] * gains[band];

						// right ch
						/* Store Xi(n) */
				data_history[band][1].x[g_eqi] = rightsampledither;
						/* Calculate and store Yi(n) */

						data_history[band][1].y[g_eqi] =
							(
						/*								= alpha * [x(n)-x(n-2)] */
								iir_cf[band].alpha * ( data_history[band][1].x[g_eqi]
								-  data_history[band][1].x[g_eqk])
						/* 								+ gamma * y(n-1) */
								+ iir_cf[band].gamma * data_history[band][1].y[g_eqj]
						/* 								- beta * y(n-2) */
								- iir_cf[band].beta * data_history[band][1].y[g_eqk]
								);

						/* Apply the gain  */
				out_r +=  data_history[band][1].y[g_eqi] * gains[band];

					} /* For each band */

			*psamplesleft = out_l + (leftsample*0.25f);
			*psamplesright = out_r + (rightsample*0.25f);

				g_eqi = (g_eqi + 1) % 3;
				g_eqj = (g_eqj + 1) % 3;
				g_eqk = (g_eqk + 1) % 3;
				
				di = (di + 1) % MAX_BUFFER_LENGTH;

				++psamplesleft;
				++psamplesright;
			}/* For each pair of samples */
	}
}

// Function that describes value on client's displaying
bool mi::DescribeValue(char* txt,int const param, int const value)
{

	switch(param)
	{
		case B20:
		case B25:
		case B40:
		case B50:
		case B80:
		case B100:
		case B160:
		case B200:
		case B315:
		case B400:
		case B630:
		case B800:
		case B1250:
		case B1600:
		case B2500:
		case B3150:
		case B5000:
		case B6300:
		case B10000:
		case B12500:
		case B20000:
			if (bnds == EQBANDS10) {
				std::strcpy(txt,"--");
				return true;
			}
			
		case B31:
		case B62:
		case B125:
		case B250:
		case B500:
		case B1000:
		case B2000:
		case B4000:
		case B8000:
		case B16000:
			std::sprintf(txt,"%.1f dB",DBfromScaleGain(value));
			return true;
		case _DIV: return false;
		case PRE_AMP:
			std::sprintf(txt,"%i dB",value);
			return true;
		case BANDS:
			std::sprintf(txt,"%i",(value==0?EQBANDS10:EQBANDS31));
			return true;
		case EXTRA:
		case LINK:
			std::sprintf(txt,value?"On":"Off");
			return true;
		default:
			return false;
	}
}

void mi::maEqualizerReset()
{
	if(bnds == EQBANDS31)
	{
		iir_cf = iir_cf31;
		gains = gains31;
	}
	else
	{
		iir_cf = iir_cf10;
		gains = gains10;
	}
}

void mi::maEqualizerSetPreamp() {
	preamp = (9.9999946497217584440165E-01 *
	        exp(6.9314738656671842642609E-02 * (float)(Vals[PRE_AMP]+20))
	        + 3.7119444716771825623636E-07);
}

void mi::maEqualizerSetLevels()
{
	maEqualizerSetPreamp();
	for(int i=0;i<EQBANDS31;i++){
		gains31[i] = (2.5220207857061455181125E-01 *
		        exp(8.0178361802353992349168E-02 * DBfromScaleGain(Vals[i]))
		        - 2.5220207852836562523180E-01);
	}
	for(int i=0;i<EQBANDS10;i++){
		gains10[i] = (2.5220207857061455181125E-01 *
		        exp(8.0178361802353992349168E-02 * DBfromScaleGain(Vals[3*i+2]))
		        - 2.5220207852836562523180E-01);
	}
}

/* Get the freqs at both sides of F0. These will be cut at -3dB */
inline void mi::find_f1_and_f2(double f0, double octave_percent, double *f1, double *f2)
{
	double octave_factor = std::pow(2.0, octave_percent/2.0);
	*f1 = f0/octave_factor;
	*f2 = f0*octave_factor;
}

/* Find the quadratic root
	* Always return the smallest root */
inline int mi::find_root(double a, double b, double c, double *x0) {
	double k = c-((b*b)/(4.*a));
	double h = -(b/(2.*a));
	double x1 = 0.;
	if (-(k/a) < 0.)
		return -1; 
	*x0 = h - std::sqrt(-(k/a));
	x1 = h + std::sqrt(-(k/a));
	if (x1 < *x0)
		*x0 = x1;
	return 0;
}

/* Calculate all the coefficients as specified in the bands[] array */
void mi::calc_coeffs()
{
	int i, n;
	double f1, f2;
	double x0;

	n = 0;
	for (; bands[n].cfs; n++)
	{
		double *freqs = (double *)bands[n].cfs;
		for (i=0; i<bands[n].band_count; i++)
		{
			if(freqs[i]<=srate/2)
			{

				/* Find -3dB frequencies for the center freq */
				find_f1_and_f2(freqs[i], bands[n].octave, &f1, &f2);
				/* Find Beta */
				if ( find_root(
					BETA2(TETA(freqs[i]), TETA(f1)), 
					BETA1(TETA(freqs[i]), TETA(f1)), 
					BETA0(TETA(freqs[i]), TETA(f1)), 
					&x0) == 0)
				{
					/* Got a solution, now calculate the rest of the factors */
					/* Take the smallest root always (find_root returns the smallest one)
					*
					* NOTE: The IIR equation is
					*				y[n] = 2 * (alpha*(x[n]-x[n-2]) + gamma*y[n-1] - beta*y[n-2])
					*  Now the 2 factor has been distributed in the coefficients
					*/
					/* Now store the coefficients */
					bands[n].coeffs[i].beta = 2.0 * x0;
					bands[n].coeffs[i].alpha = 2.0 * ALPHA(x0);
					bands[n].coeffs[i].gamma = 2.0 * GAMMA(x0, TETA(freqs[i]));
				} else {
					/* Shouldn't happen */
					bands[n].coeffs[i].beta = 0.;
					bands[n].coeffs[i].alpha = 0.;
					bands[n].coeffs[i].gamma = 0.;
				}
			} else {
				bands[n].coeffs[i].beta = 0.;
				bands[n].coeffs[i].alpha = 0.;
				bands[n].coeffs[i].gamma = 0.;
			}
		}// for i
	}//for n
}

void mi::clean_history()
{
	/* Zero the history arrays */
	std::memset(data_history, 0, sizeof(sXYData) * EQ_MAX_BANDS * EQ_CHANNELS);
	std::memset(data_history2, 0, sizeof(sXYData) * EQ_MAX_BANDS * EQ_CHANNELS);
	/* this is only needed if we use fpu code and there's no other place for
	the moment to init the dither array*/
	for (int n = 0; n < MAX_BUFFER_LENGTH; n++) {
		dither[n] = (rand() / float(RAND_MAX));
	}
	di = 0;
	g_eqi = 0; g_eqj = 2; g_eqk = 1;
	//g_eqi = 2; g_eqj = 1; g_eqk = 0;
}
}