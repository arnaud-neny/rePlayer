/**********************************************************************
	This file contains two different implementations of the FFT algorithm
	to calculate Power spectrums.
	Function based one is original from Dominic Mazzoni.
	Class based one is made from sources of schismtracker and windows from wikipedia.
	FFTClass is a bit faster, but with a few more spikes.

	Original copyright follows:

	FFT.h

	Dominic Mazzoni

	September 2000

	This file contains a few FFT routines, including a real-FFT
	routine that is almost twice as fast as a normal complex FFT,
	and a power spectrum routine which is more convenient when
	you know you don't care about phase information.  It now also
	contains a few basic windowing functions.

	Some of this code was based on a free implementation of an FFT
	by Don Cross, available on the web at:

	http://www.intersrv.com/~dcross/fft.html

	The basic algorithm for his code was based on Numerical Recipes
	in Fortran.  I optimized his code further by reducing array
	accesses, caching the bit reversal table, and eliminating
	float-to-double conversions, and I added the routines to
	calculate a real FFT and a real power spectrum.

	Note: all of these routines use single-precision floats.
	I have found that in practice, floats work well until you
	get above 8192 samples.  If you need to do a larger FFT,
	you need to use doubles.

**********************************************************************/
#pragma once
#include <cstddef>
#include "resampler.hpp"
namespace psycle { namespace helpers { namespace dsp {
	namespace dmfft {
	/**
	* This is the function you will use the most often.
	* Given an array of floats, this will compute the power
	* spectrum by doing a Real FFT and then computing the
	* sum of the squares of the real and imaginary parts.
	* Note that the output array is half the length PLUS ONE of the
	* input array, and that NumSamples must be a power of two.
		* Understanding In and Out:
		* In is the samples (so, it is amplitude). You need to use a window function
		* on it before calling this method.
		* Out is the total power for each band.
		* To get amplitude from "Out", use sqrt(out[N])/(numSamples>>2)
		* To get dB from "Out", use powerdB(out[N])+db(1/(numSamples>>2)).
		* powerdB is = 10 * log10(in)
		* dB is = 20 * log10(in)
	*/
	void PowerSpectrum(int NumSamples, float *In, float *Out);

	/**
	* Computes an FFT when the input data is real but you still
	* want complex data as output.  The output arrays are half
	* the length of the input, and NumSamples must be a power of
	* two.
	*/
	void RealFFT(int NumSamples,
					float *RealIn, float *RealOut, float *ImagOut);

	/**
	* Computes a FFT of complex input and returns complex output.
	* Currently this is the only function here that supports the
	* inverse transform as well.
	*/
	void FFT(int NumSamples,
				bool InverseTransform,
				float *RealIn, float *ImagIn, float *RealOut, float *ImagOut);

	/**
	* Applies a windowing function to the data in place
	*
	* 0: Rectangular (no window)
	* 1: Bartlett    (triangular)
	* 2: Hamming
	* 3: Hanning
	*/
	void WindowFunc(int whichFunction, int NumSamples, float *data);

	/// Returns the name of the windowing function (for UI display)
	const char * WindowFuncName(int whichFunction);

	/// Returns the number of windowing functions supported
	int NumWindowFuncs();
	}

	typedef enum FftWindowEnum {
		rectangular,
		cosine,
		hann,
		hamming,
		gaussian,
		blackmann,
		blackmannHarris
	} FftWindowType;

	class FFTClass {
	public:
		FFTClass();
		~FFTClass();
		void Setup(FftWindowType type, std::size_t sizeBuf, std::size_t sizeBands);
		/*
		* Understanding In and Out:
		* In is the samples (so, it is amplitude). The window function specified in setup
		* will automatically be applied.
		* Out is the total power for each band.
		* To get amplitude from "Out", use sqrt(out[N])/(sizeBuf>>2)
		* To get dB from "Out", use powerdB(out[N])+db(1/(sizeBuf>>2)).
		* powerdB is = 10 * log10(in)
		* dB is = 20 * log10(in)
		*/
		void CalculateSpectrum(float samplesIn[], float samplesOut[]);
		//This tries to reduce the calculated fft to represent the amount of bands indicated in setup
		void FillBandsFromFFT(float calculatedfftIn[], float banddBOut[]);
		inline int getDCBars() { 
			//This was intended as a DC bar, but it isn't only DC.
			//int DCBar=0; while(fftLog[DCBar] < 1.f) {DCBar++;} return --DCBar; 
			return -1;
		}
	protected:
		void Reset();
		void FillRectangularWindow(float window[], const std::size_t size, const float scale);
		void FillCosineWindow(float window[], const std::size_t size, const float scale);
		void FillHannWindow(float window[], const std::size_t size, const float scale);
		void FillHammingWindow(float window[], const std::size_t size, const float scale);
		void FillGaussianWindow(float window[], const std::size_t size, const float scale);
		void FillBlackmannWindow(float window[], const std::size_t size, const float scale);
		void FillBlackmannHarrisWindow(float window[], const std::size_t size, const float scale);
		std::size_t  Reverse_bits(std::size_t in);
		bool IsPowerOfTwo(size_t x);
		float resample(float const * data, float offset, uint64_t length);
		inline std::size_t getOutputSize() { return outputSize; }
	private:
		std::size_t bufferSize;
		std::size_t bufferSizeLog;
		std::size_t outputSize;
		std::size_t bands;
		/* tables */
		std::size_t *bit_reverse;
		//window to apply to the signal previous to the fft. Can be scaled if using the scale parameter.
		float *window;
		float *precos;
		float *presin;

		/* fft state */
		float *state_real;
		float *state_imag;
		dsp::cubic_resampler resampler;

		//fftLog is initialized in setup, and can be used to scale the frequency, like is done in FillBandsFromFFT:
		float *fftLog;
	};
}}}

// Indentation settings for Vim and Emacs and unique identifier for Arch, a
// version control system. Please do not modify past this point.
//
// Local Variables:
// c-basic-offset: 3
// indent-tabs-mode: nil
// End:
//
// vim: et sts=3 sw=3
// arch-tag: 91e23340-889b-4c2d-89b0-0173315a0b32
