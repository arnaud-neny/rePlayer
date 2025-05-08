///\implementation psycle::helpers::dsp::Dither
#include "dither.hpp"
namespace psycle { namespace helpers { namespace dsp {

using namespace universalis::stdlib;

Dither::Dither() : bitdepth(16), pdf(Pdf::triangular), noiseshape(NoiseShape::none) {
	// copied verbatim from mt19937 demo
	uint32_t init[4] = { 0x123, 0x234, 0x345, 0x456 };
	mt.init_by_array(init, sizeof init);
}

void Dither::Process(float * inSamps, unsigned int length) {
	// gaussian rand returns two values, this tells us which we're on
	bool newgauss(true);
	// our random number, and an extra variable for the gaussian distribution
	double gauss(0.0), randval(0.0);
	// quantization error of the last sample, used for noise shaping
	float prevError(0.0f);
	// the amount the sample will eventually be multiplied by for integer quantization
	float bdMultiplier;
	// the inverse.. i.e., the number that will eventually become the quantization interval
	float bdQ;

	switch(bitdepth) {
		case 8:
			bdMultiplier = 1 / 256.0f;
			bdQ = 256.0f;
			break;
		default:
		case 16:
			bdMultiplier = 1.0f;
			bdQ = 1.0f;
			break;
		case 24:
			bdMultiplier = 256.0f;
			bdQ = 1 / 256.0f;
			break;
		case 32:
			bdMultiplier = 65536.0f;
			bdQ = 1 / 65536.0f;
			break;
	}

	for(unsigned int i(0); i < length; ++i) {
		switch(pdf) {
			case Pdf::rectangular:
				randval = mt.genrand_real1()-0.5;
				break;
			case Pdf::gaussian:
				if(newgauss) mt.genrand_gaussian(randval, gauss);
				else randval = gauss;
				newgauss = !newgauss; // mt.genrand_gaussian() has a standard deviation (rms) of 1..
				randval *= 0.5; // we need it to be one-half the quantizing interval (which is 1), so we just halve it
				break;
			case Pdf::triangular:
				randval = (mt.genrand_real1() - 0.5) + (mt.genrand_real1() - 0.5);
				break;
		}

		*(inSamps + i) += randval * bdQ;

		///\todo this seems inefficient.. we're essentially quantizing twice, once for practice to get the error, and again
		///      for real when we write to the wave file.
		if(noiseshape == NoiseShape::highpass) {
			*(inSamps + i) += prevError;
			// The only way to determine the quantization error of rounding to int after scaling by a given factor
			// is to do the actual scaling, and then divide the resulting error by the same factor to keep it in
			// the correct scale (until it's re-multiplied on wave-writing (this is pretty ridiculous))
			prevError = (*(inSamps + i) * bdMultiplier - (int)(*(inSamps + i) * bdMultiplier)) * bdQ;
		}

	}
}

}}}
