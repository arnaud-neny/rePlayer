////implementation for dwfilter class
#include "dw_filter.hpp"
#include <psycle/helpers/math/erase_all_nans_infinities_and_denormals.hpp>
namespace psycle::plugins::dw {
const double dwfilter::PI																= 3.141592653589793238;
const double dwfilter::TWO_PI												= 2 * dwfilter::PI;
const double dwfilter::PI_EIGHTHS								= dwfilter::PI/8.0f;
const double dwfilter::PI_FOURTHS								= dwfilter::PI/4.0f;
const double dwfilter::PI_THIRDS								= dwfilter::PI/3.0f;
const double dwfilter::PI_HALVES								= dwfilter::PI/2.0f;
const int   dwfilter::FILT_MIN_FREQ								= 15;
const float dwfilter::FILT_MIN_GAIN								= 0.125f;
const float dwfilter::FILT_MAX_GAIN								= 8.0f;
const float dwfilter::FILT_MIN_BW								= 0.01f;
const float dwfilter::FILT_MAX_BW								= float(dwfilter::PI_THIRDS);
const float dwfilter::FILT_MIN_SLOPE				= 0.01f;
const float dwfilter::FILT_MAX_SLOPE				= 1.0f;
const float dwfilter::FILT_MIN_Q								= 0.7071f;
const float dwfilter::FILT_MAX_Q								= 0.98f;

dwfilter::dwfilter() 
{
	zeroize();
	nyquist=22050;
}
dwfilter::dwfilter(int samprate)
{
	zeroize();
	if(samprate>=11025 && samprate<=192000)
		nyquist=samprate/2;
	else
		nyquist=22050;
}

dwfilter::dwfilter(const dwfilter& rhs)
{
	nyquist=rhs.nyquist;
	SetMode(rhs.mode);
	SetFreq(rhs.freq);
	SetGain(rhs.gain);
	SetSlope(rhs.slope);
	SetQ(rhs.q);
	SetBW(rhs.bandwidth);
}
dwfilter& dwfilter::operator=(const dwfilter& rhs)
{
	nyquist=rhs.nyquist;
	SetMode(rhs.mode);
	SetFreq(rhs.freq);
	SetGain(rhs.gain);
	SetSlope(rhs.slope);
	SetQ(rhs.q);
	SetBW(rhs.bandwidth);
	return *this;
}

void dwfilter::zeroize()
{
	beta=0.0;
	sintheta=0.0;
	costhetaRho=0.0;
	sinthetaBeta=0.0;
	thetac = 0;
	bwtan = 0;
	costheta = 0;
	sintheta = 0;
	coefa0 = 1.0;
	coefa1 = 0.0;
	coefa2 = 0.0;
	coefb1 = 0.0;
	coefb2 = 0.0;
	gain=1.0;
	freq=0.0;
	bandwidth=0.0;
	mode=nop;

	emptybuffers();

	slope=1.0f;																// for now..
}

void dwfilter::emptybuffers()
{
	for(int i=0;i<FILT_MAX_CHANS;++i)
	z0[i] = z1[i] = z2[i] = 0;
}

bool dwfilter::SetMode(int _mode)
{
	if(_mode<0 || _mode>=NUM_MODES)
		return false;
	else if(mode==_mode)
		return false;
	mode = (filtmode)_mode;
	CoefUpdate();
	return true;
}

bool dwfilter::SetFreq(double _freq)
{
	if(_freq>=0.0 && _freq<=nyquist)
	{
		if(_freq!=freq)
		{
			int dif=freq-_freq;
			freq = _freq;
			CoefUpdate();				

			if(dif > +800 || dif < -800) // empty buffers every freq change, and twses crackle..
				emptybuffers();      // don't empty buffers ever, and large jumps pop.
			return true;                 // this compromise appears to work well enough..
		}
	} 
	return false; 
}

bool dwfilter::SetGain(double _gain)
{
	if(_gain<FILT_MIN_GAIN) _gain = FILT_MIN_GAIN;
	if(_gain>FILT_MAX_GAIN) _gain = FILT_MAX_GAIN;
	if(gain!=_gain)
	{
		gain = _gain;
		CoefUpdate();
		return true;
	}
	return false; 
}

bool dwfilter::SetBW(double _bw)
{
	if(_bw>=FILT_MIN_BW && _bw<=FILT_MAX_BW)
	{
		if(bandwidth!=_bw)
		{
			bandwidth = _bw;
			CoefUpdate();
			return true; 
		}
	}
	return false;
}

bool dwfilter::SetSlope(double _slope)
{
	if(_slope>=FILT_MIN_SLOPE && _slope<=FILT_MAX_SLOPE)
	{
		if(slope!=_slope)
		{
			slope = _slope;				
			CoefUpdate();				
			return true; 
		}
	}
	return false; 
}

bool dwfilter::SetQ(double _q)
{
	if(_q>=FILT_MIN_Q && _q<=FILT_MAX_Q)
	{
		if(q!=_q)
		{
			q = _q;
			CoefUpdate();
			return true;
		}
	}
	return false;
}

bool dwfilter::SetSampleRate(int samprate)
{
	if(samprate>=11025 && samprate<=192000)								/* who knows, maybe some day... */
	{
		int newNyq=samprate/2.0;
		if(nyquist!=newNyq)
		{
			nyquist = newNyq;
			CoefUpdate();
			return true;
		}
	}
	return false;
}


void dwfilter::CoefUpdate()
{
	switch(mode)
	{
	case eq_parametric:				thetac = freq/(float)nyquist * PI;
						costheta = cosf(thetac);
						bwtan = tanf(bandwidth/2.0f);
						coefa0 = (float)(1.0f + gain * bwtan) / (float)(1.0f + bwtan);
						coefa1 = coefb1 = (float) (-2.0f * costheta) / (float) (1.0f + bwtan);
						coefa2 = (float)(1.0f - gain * bwtan) / (float)(1.0f + bwtan);
						coefb2 = (float)(1.0f - bwtan) / (float)(1.0f + bwtan);
						break;

	case eq_loshelf:				thetac = freq/(float)nyquist * PI;
						rho = sqrt(gain);
						costheta = cosf(thetac);
						sintheta = sinf(thetac);
						//slope = 1.0f;
						beta = sqrt((rho*rho + 1)/slope - ((rho-1)*(rho-1)));
						costhetaRho = costheta * rho;
						sinthetaBeta = sintheta * beta;

						coefb0 =        ((rho+1) + costhetaRho - costheta + sinthetaBeta); 

						coefa0 = (rho * ((rho+1) - costhetaRho + costheta + sinthetaBeta) / coefb0);
						coefa1 = (2*rho*((rho-1) - costhetaRho - costheta																) / coefb0);
						coefa2 = (rho * ((rho+1) - costhetaRho + costheta - sinthetaBeta) / coefb0);
						coefb1 = ( -2 * ((rho-1) + costhetaRho + costheta																) / coefb0);
						coefb2 =       (((rho+1) + costhetaRho - costheta - sinthetaBeta) / coefb0);
						break;

	case eq_hishelf:				thetac = freq/(float)nyquist * PI;
						rho = sqrt(gain);
						costheta = cosf(thetac);
						sintheta = sinf(thetac);
						//slope = 1.0f;
						beta = sqrt((rho*rho + 1)/slope - ((rho-1)*(rho-1)));
						costhetaRho = costheta * rho;
						sinthetaBeta = sintheta * beta;

						coefb0 =       ((rho+1) - costhetaRho + costheta + sinthetaBeta );

						coefa0 = ( rho * ((rho+1) + costhetaRho - costheta + sinthetaBeta) / coefb0);
						coefa1 = (-2*rho*((rho-1) + costhetaRho + costheta																 ) / coefb0);
						coefa2 = ( rho * ((rho+1) + costhetaRho - costheta - sinthetaBeta) / coefb0);
						coefb1 = (   2 * ((rho-1) - costhetaRho - costheta																 ) / coefb0);
						coefb2 = (       ((rho+1) - costhetaRho + costheta - sinthetaBeta) / coefb0);
						break;

	}
}



float dwfilter::Process(const float xn, const int chan)
{
	float yn=0.0;

	if (chan<0 || chan>=FILT_MAX_CHANS)
		return 0.0;

/*				switch(mode)								//unnecessary for now, everything is biquad
	{
	case eq_hishelf:
	case eq_loshelf:
	case eq_parametric:
*/				

	z0[chan] =				  xn
				- coefb1 * z1[chan]
				- coefb2 * z2[chan];

	yn =								  coefa0 * z0[chan]
				+ coefa1 * z1[chan]
				+ coefa2 * z2[chan];


/*																												break;

	}
*/				

	// it might save a bit on cpu to just pre-init a buffer of random values to add to the input
	#if 0
		if(isdenormal(z0[chan]))
			z0[chan]=0.0f;
	#else
		psycle::helpers::math::erase_all_nans_infinities_and_denormals(z0[chan]);
	#endif

	z2[chan] = z1[chan];
	z1[chan] = z0[chan];
	return yn;

}

#if 0
	bool dwfilter::isdenormal(float num)								
	{
		if(  (   ( *(uint32_t*) & num) & 0x7f800000) == 0)
			return true;
		return false;
	}
#endif
}