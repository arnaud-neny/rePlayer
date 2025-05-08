//============================================================================
//
//				Biquad (2 pole, 2 zero) filter
//
//
//	Implementation based on:
//
//			Cookbook formulae for audio EQ biquad filter coefficients
//----------------------------------------------------------------------------
//			by Robert Bristow-Johnson  <rbj@audioimagination.com>
//			http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
//			With correction by druttis@darkface.pp.se
//============================================================================
#include "Biquad.h"
#include <psycle/helpers/math.hpp>

namespace psycle::plugins::druttis {

const double ln2half = 0.34657359027997265470861606072909; // ln(2)/2

//============================================================================
//				Constructor/Destructor
//============================================================================
Biquad::Biquad()
{
}
Biquad::~Biquad()
{
}
//============================================================================
//				Init
//============================================================================
void Biquad::Init()
{
	freqgain = 0.0f;
	freq = 500.f;
	q = 0.000001f;
	srate = 44100.f;
	type = LOWPASS;
	inCoeffs[0] = 0.0f;
	inCoeffs[1] = 0.0f;
	inCoeffs[2] = 0.0f;
	outCoeffs[0] = 0.0f;
	outCoeffs[1] = 0.0f;
	ResetMemory();
}
void Biquad::Init(filter_t intype,float infreqGain,float infreq, float insrate, float inq) {
	type = intype;
	freqgain = infreqGain;
	ResetMemory();
	SetFreqAndQ(infreq,inq,insrate);	
}
//============================================================================
//				ResetMemory
//============================================================================
void Biquad::ResetMemory()
{
	inputs[0] = 0.0f;
	inputs[1] = 0.0f;
	outputs[0] = 0.0f;
	outputs[1] = 0.0f;
}
//============================================================================
//				SetFilterType
//============================================================================
void Biquad::SetFilterType(filter_t intype)
{
	type = intype;
	SetFreqAndQ(freq,q,srate);
}
//============================================================================
//				SetFreqGain
//============================================================================
void Biquad::SetFreqGain(float newGain)
{
	freqgain = newGain;
	SetFreqAndQ(freq,q,srate);
}
//============================================================================
//				ChangeSampleRate
//============================================================================
void Biquad::ChangeSampleRate(float srate)
{
	SetFreqAndQ(freq,q,srate);
}

//============================================================================
//				SetFreqAndQ
//============================================================================
void Biquad::SetFreqAndQ(float infreq, float inq, float insrate)
{
	freq = infreq;
	q = inq;
	srate = insrate;
	double w0 = 2*pi * freq/srate;
	double cosw = cos(w0);
	double sinw = sin(w0);

	switch(type)
	{
	case LOWPASS: SetLowPass(cosw,sinw);break;
	case HIGHPASS: SetHighPass(cosw,sinw);break;
	case BANDPASS_PEAK:SetBandPassPeak(cosw,sinw,w0);break;
	case BANDPASS_0DB:SetBandPass0dB(cosw,sinw,w0);break;
	case NOTCHBAND:SetNotchBand(cosw,sinw,w0);break;
	case ANALOGPEAKFILTER:SetAnalogPeakFilter(cosw,sinw,w0);break;
	case PEAK_EQ:SetPeakingEQ(cosw,sinw,w0);break;
	case LOWSHELF:SetLowShelf(cosw,sinw,w0);break;
	case HIGHSHELF:SetHighShelf(cosw,sinw,w0);break;
	default:break;
	}
}

void Biquad::SetLowPass(double cosw, double sinw)
{
	double alpha = sinw/(2.0*q);
	double a0 = (1.0 + alpha);
	
	inCoeffs[0] = (1.0 - cosw)/(2.0*a0);
	inCoeffs[1] = (1.0 - cosw)/a0;
	inCoeffs[2] = (1.0 - cosw)/(2.0*a0);

	outCoeffs[0] = (-2.0*cosw)/a0;
	outCoeffs[1] = (1.0 - alpha)/a0;
}

void Biquad::SetHighPass(double cosw, double sinw)
{
	double alpha = sinw/(2.0*q);
	double a0 = (1.0 + alpha);
	
	inCoeffs[0] = (1.0 + cosw)/(2.0*a0);
	inCoeffs[1] = -(1.0 + cosw)/a0;
	inCoeffs[2] = (1.0 + cosw)/(2.0*a0);

	outCoeffs[0] = (-2.0*cosw)/a0;
	outCoeffs[1] = (1.0 - alpha)/a0;
}

void Biquad::SetBandPassPeak(double cosw, double sinw, double w0)
{
	double alpha = sinw*sinh( ln2half * q * w0/sinw );
	double a0 = (1.0 + alpha);
	
	inCoeffs[0] = q*alpha/a0;
	inCoeffs[1] = 0;
	inCoeffs[2] = -q*alpha/a0;

	outCoeffs[0] = (-2.0*cosw)/a0;
	outCoeffs[1] = (1.0 - alpha)/a0;
}

void Biquad::SetBandPass0dB(double cosw, double sinw, double w0)
{
	double alpha = sinw*sinh( ln2half * q * w0/sinw );
	double a0 = (1.0 + alpha);
	
	inCoeffs[0] = alpha/a0;
	inCoeffs[1] = 0.0;
	inCoeffs[2] = -alpha/a0;

	outCoeffs[0] = (-2.0*cosw)/a0;
	outCoeffs[1] = (1.0 - alpha)/a0;
}
void Biquad::SetNotchBand(double cosw, double sinw, double w0)
{
	double alpha = sinw*sinh( ln2half * q * w0/sinw );
	double a0 = (1.0 + alpha);
	
	inCoeffs[0] = 1.0/a0;
	inCoeffs[1] = (-2.0*cosw)/a0;
	inCoeffs[2] = 1.0/a0;

	outCoeffs[0] = (-2.0*cosw)/a0;
	outCoeffs[1] = (1.0 - alpha)/a0;
}

void Biquad::SetAnalogPeakFilter(double cosw, double sinw, double w0)
{
	double alpha = sinw*sinh( ln2half * q * w0/sinw );
	double a0 = (1.0 + alpha);
	
	inCoeffs[0] = (1.0 - alpha)/a0;
	inCoeffs[1] = (-2.0*cosw)/a0;
	inCoeffs[2] = (1.0 - alpha)/a0;

	outCoeffs[0] = (-2.0*cosw)/a0;
	outCoeffs[1] = (1.0 - alpha)/a0;
}

void Biquad::SetPeakingEQ(double cosw, double sinw, double w0)
{
	double A = pow(10.0,freqgain/40.0);
	double alpha = sinw*sinh( ln2half * q * w0/sinw );
	double a0 = (1.0 + alpha/A);
	
	inCoeffs[0] = (1.0 + alpha*A)/a0;
	inCoeffs[1] = (-2.0*cosw)/a0;
	inCoeffs[2] = (1.0 - alpha*A)/a0;

	outCoeffs[0] = (-2.0*cosw)/a0;
	outCoeffs[1] = (1.0 - alpha/A)/a0;
}

void Biquad::SetLowShelf(double cosw, double sinw, double w0)
{
	double A = pow(10.0,freqgain/40.0);
	double beta;
	if (q != 0.0f)
	{
		beta = sqrt(A * A + 1.0) / (double) q - ((A - 1.0) * (A - 1.0));
	}
	else
	{
		beta = sqrt(A + A);
	}
	double amonecos = (A-1.0)*cosw;
	double a0 = (A+1.0) + amonecos + beta *sinw;

	inCoeffs[0] = (A*( (A+1.0) - amonecos + beta *sinw ))/a0;
	inCoeffs[1] = (2.0*A*( (A-1.0) - (A+1.0)*cosw) )/a0;
	inCoeffs[2] = (A*( (A+1.0) - amonecos - beta *sinw ))/a0;

	outCoeffs[0] = (-2.0*( (A-1.0) + (A+1.0)*cosw))/a0;
	outCoeffs[1] = ((A+1) + amonecos - beta *sinw)/a0;
}

void Biquad::SetHighShelf(double cosw, double sinw, double w0)
{
	double A = pow(10.0,freqgain/40.0);
	double beta;
	if (q != 0.0f)
	{
		beta = sqrt(A * A + 1.0) / (double) q - ((A - 1.0) * (A - 1.0));
	}
	else
	{
		beta = sqrt(A + A);
	}
	double amonecos = (A-1.0)*cosw;
	double a0 = (A+1.0) - amonecos + beta *sinw;

	inCoeffs[0] = (A*( (A+1.0) + amonecos + beta *sinw ))/a0;
	inCoeffs[1] = (-2.0*A*( (A-1.0) + (A+1.0)*cosw) )/a0;
	inCoeffs[2] = (A*( (A+1.0) + amonecos -beta *sinw))/a0;

	outCoeffs[0] = (2.0*( (A-1.0) - (A+1.0)*cosw))/a0;
	outCoeffs[1] = ((A+1) - amonecos - beta *sinw)/a0;
}

void Biquad::SetRawCoeffs(double b0, double b1, double b2, double a0, double a1, double a2)
{
	inCoeffs[0] = b0/a0;
	inCoeffs[1] = b1/a0;
	inCoeffs[2] = b2/a0;

	outCoeffs[0] = a1/a0;
	outCoeffs[1] = a2/a0;
}
}