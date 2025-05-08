///\interface psycle::helpers::dsp::Dither

#pragma once
#include <psycle/helpers/mersennetwister.hpp>
namespace psycle { namespace helpers { namespace dsp {

class Dither {
	public:
		Dither();
		virtual ~Dither() {}
		//Psycle uses -32768.0..32768.0 range for insamples. This Process is done with that in mind.
		// Modify the bdMultiplier and bdQ values in the switch case for other use cases.
		void Process(float * inSamps, unsigned int length);

		struct Pdf {
			enum type {
				triangular = 0,
				rectangular,
				gaussian
			};
		};
		
		struct NoiseShape {
			enum type {
				none = 0,
				highpass
			};
		};

		void SetBitDepth(unsigned int newdepth) { bitdepth = newdepth; }
		void SetPdf(Pdf::type newpdf) { pdf = newpdf; }
		void SetNoiseShaping(NoiseShape::type newns) { noiseshape = newns; }

	private:
		unsigned int bitdepth;
		Pdf::type pdf;
		NoiseShape::type noiseshape;

		MersenneTwister mt;
};

}}}
