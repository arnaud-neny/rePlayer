//============================================================================
//
//			Biquad (2 pole, 2 zero) filter
//
//	Implementation based on:
//
//			Cookbook formulae for audio EQ biquad filter coefficients
//----------------------------------------------------------------------------
//			by Robert Bristow-Johnson  <rbj@audioimagination.com>
//			http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
//			With correction by druttis@darkface.pp.se
//============================================================================
#pragma once

#include <psycle/helpers/math/erase_all_nans_infinities_and_denormals.hpp>

namespace psycle::plugins::druttis {

using namespace psycle::helpers::math;

class Biquad
{
public:
	typedef enum {
		LOWPASS=0,
		HIGHPASS,
		BANDPASS_PEAK,
		BANDPASS_0DB,
		NOTCHBAND,
		ANALOGPEAKFILTER,
		PEAK_EQ,
		LOWSHELF,
		HIGHSHELF
	}filter_t;
public:
	Biquad();
	~Biquad();
	void Init();
	void Init(filter_t type,float freqGain,float freq, float srate, float q);
	void ResetMemory();
	void SetFilterType(filter_t type);
	void SetFreqAndQ(float freq, float q, float srate);
	// gain in dB applied to the band for EQ or shelf types.
	void SetFreqGain(float freqGain);
	void ChangeSampleRate(float srate);
	void SetRawCoeffs(double b0, double b1, double b2, double a0, double a1, double a2);
	inline float Work(float sample);
public:
	void SetLowPass(double cosw, double sinw);
	void SetHighPass(double cosw, double sinw);
	void SetBandPassPeak(double cosw, double sinw, double w0);
	void SetBandPass0dB(double cosw, double sinw, double w0);
	void SetNotchBand(double cosw, double sinw, double w0);
	void SetAnalogPeakFilter(double cosw, double sinw, double w0);
	void SetPeakingEQ(double cosw, double sinw, double w0);
	void SetLowShelf(double cosw, double sinw, double w0);
	void SetHighShelf(double cosw, double sinw, double w0);
	// Memory for input values
	float				inputs[2];
	// Memory for output values
	float				outputs[2];
	// gain in dB applied to the band of EQ or shelf.
	float				freqgain;
	// calculated coeffs for input signal
	float				inCoeffs[3];
	// calculated coeffs for output signal
	float				outCoeffs[2];
	// sampling rate of operation
	float				srate;
	// frequency of the filter
	float				freq;
	// Q (low/highpass), or bandwidth(bandpass/notchband/eq) or shelf slope (low/highshelf)
	float				q;
	// type of filter
	filter_t			type;
};

//------------------------------------------------------------------------
//				tick
//------------------------------------------------------------------------
inline float Biquad::Work(float sample)
{
	erase_all_nans_infinities_and_denormals(sample);
	float lastOutput = 
		  sample * inCoeffs[0]
		+ inputs[0] * inCoeffs[1]
		+ inputs[1] * inCoeffs[2]
		- outputs[0] * outCoeffs[0]
		- outputs[1] * outCoeffs[1];
	erase_all_nans_infinities_and_denormals(lastOutput);

	inputs[1] = inputs[0];
	inputs[0] = sample;
	outputs[1] = outputs[0];
	outputs[0] = lastOutput;
	return lastOutput;
}

}