/**********************************************************************
	This file contains two different implementations of the FFT algorithm
	to calculate Power spectrums.
	Function based one is original from Dominic Mazzoni.
	Class based one is made from sources of schismtracker.
	FFTClass is a bit faster, but with a few more spikes.

	Original copyright follows:

	FFT.cpp

	Dominic Mazzoni

	September 2000

	This file contains a few FFT routines, including a real-FFT
	routine that is almost twice as fast as a normal complex FFT,
	and a power spectrum routine when you know you don't care
	about phase information.

	Some of this code was based on a free implementation of an FFT
	by Don Cross, available on the web at:

	http://www.intersrv.com/~dcross/fft.html

	The basic algorithm for his code was based on Numerican Recipes
	in Fortran.  I optimized his code further by reducing array
	accesses, caching the bit reversal table, and eliminating
	float-to-double conversions, and I added the routines to
	calculate a real FFT and a real power spectrum.

**********************************************************************/
#include "fft.hpp"
#include <universalis.hpp>
#include <universalis/os/aligned_alloc.hpp>
#include <psycle/helpers/math.hpp>
#include <psycle/helpers/dsp.hpp>
#include <cstdlib>
#include <cstdio>

namespace psycle { namespace helpers { namespace dsp {
	namespace dmfft {

int **gFFTBitTable = NULL;
//const int MaxFastBits = 16;
const int MaxFastBits = 12;

/* Declare Static functions */
static bool IsPowerOfTwo(int x);
static int NumberOfBitsNeeded(int PowerOfTwo);
static int ReverseBits(int index, int NumBits);
static void InitFFT();

bool IsPowerOfTwo(int x)
{
	return ((x & (x - 1))==0);
}

int NumberOfBitsNeeded(int PowerOfTwo)
{
	int i=0;
	while((PowerOfTwo >>=1)> 0) i++;
	return i;
}

int ReverseBits(int index, int NumBits)
{
	int i, rev;

	for (i = rev = 0; i < NumBits; i++) {
		rev = (rev << 1) | (index & 1);
		index >>= 1;
	}

	return rev;
}

void InitFFT()
{
	gFFTBitTable = new int *[MaxFastBits];

	int len = 2;
	for (int b = 1; b <= MaxFastBits; b++) {

		gFFTBitTable[b - 1] = new int[len];

		for (int i = 0; i < len; i++)
			gFFTBitTable[b - 1][i] = ReverseBits(i, b);

		len <<= 1;
	}
}

/*
	* Complex Fast Fourier Transform
	*/

void FFT(int NumSamples,
			bool InverseTransform,
			float *RealIn, float *ImagIn, float *RealOut, float *ImagOut)
{
	int NumBits;                 /* Number of bits needed to store indices */
	int i, j, k, n;
	int BlockSize, BlockEnd;

	double angle_numerator = 2.0 * math::pi;
	double tr, ti;                /* temp real, temp imaginary */

	if (!IsPowerOfTwo(NumSamples)) {
		std::ostringstream s;
		s << "FFT called with size "<< NumSamples;
		throw universalis::exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
	}

	NumBits = NumberOfBitsNeeded(NumSamples);

	if (!gFFTBitTable)
		InitFFT();

	if (InverseTransform)
		angle_numerator = -angle_numerator;


	/*
	**   Do simultaneous data copy and bit-reversal ordering into outputs...
	*/
	if (NumBits <= MaxFastBits) {
		for (i = 0; i < NumSamples; i++) {
			j = gFFTBitTable[NumBits - 1][i];
			RealOut[j] = RealIn[i];
			ImagOut[j] = (ImagIn == NULL) ? 0.0 : ImagIn[i];
		}
	}
	else {
		for (i = 0; i < NumSamples; i++) {
				j = ReverseBits(i, NumBits);
			RealOut[j] = RealIn[i];
			ImagOut[j] = (ImagIn == NULL) ? 0.0 : ImagIn[i];
		}
	}

	/*
	**   Do the FFT itself...
	*/

	BlockEnd = 1;
	for (BlockSize = 2; BlockSize <= NumSamples; BlockSize <<= 1) {

		double delta_angle = angle_numerator / (double) BlockSize;

		double sm2 = sin(-2 * delta_angle);
		double sm1 = sin(-delta_angle);
		double cm2 = cos(-2 * delta_angle);
		double cm1 = cos(-delta_angle);
		double w = 2 * cm1;
		double ar0, ar1, ar2, ai0, ai1, ai2;

		for (i = 0; i < NumSamples; i += BlockSize) {
			ar2 = cm2;
			ar1 = cm1;

			ai2 = sm2;
			ai1 = sm1;

			for (j = i, n = 0; n < BlockEnd; j++, n++) {
			ar0 = w * ar1 - ar2;
			ar2 = ar1;
			ar1 = ar0;

			ai0 = w * ai1 - ai2;
			ai2 = ai1;
			ai1 = ai0;

			k = j + BlockEnd;
			tr = ar0 * RealOut[k] - ai0 * ImagOut[k];
			ti = ar0 * ImagOut[k] + ai0 * RealOut[k];

			RealOut[k] = RealOut[j] - tr;
			ImagOut[k] = ImagOut[j] - ti;

			RealOut[j] += tr;
			ImagOut[j] += ti;
			}
		}

		BlockEnd = BlockSize;
	}

	/*
		**   Need to normalize if inverse transform...
	*/

	if (InverseTransform) {
		float denom = (float) NumSamples;

		for (i = 0; i < NumSamples; i++) {
			RealOut[i] /= denom;
			ImagOut[i] /= denom;
		}
	}
}

/*
	* Real Fast Fourier Transform
	*
	* This function was based on the code in Numerical Recipes in C.
	* In Num. Rec., the inner loop is based on a single 1-based array
	* of interleaved real and imaginary numbers.  Because we have two
	* separate zero-based arrays, our indices are quite different.
	* Here is the correspondence between Num. Rec. indices and our indices:
	*
	* i1  <->  real[i]
	* i2  <->  imag[i]
	* i3  <->  real[n/2-i]
	* i4  <->  imag[n/2-i]
	*/

void RealFFT(int NumSamples, float *RealIn, float *RealOut, float *ImagOut)
{
	int Half = NumSamples / 2;
	int i;

	float theta = math::pi_f / Half;

	float *tmpReal = new float[Half];
	float *tmpImag = new float[Half];

	for (i = 0; i < Half; i++) {
		tmpReal[i] = RealIn[2 * i];
		tmpImag[i] = RealIn[2 * i + 1];
	}

	FFT(Half, 0, tmpReal, tmpImag, RealOut, ImagOut);

	float wtemp = float (sin(0.5 * theta));

	float wpr = -2.0 * wtemp * wtemp;
	float wpi = float (sin(theta));
	float wr = 1.0 + wpr;
	float wi = wpi;

	int i3;

	float h1r, h1i, h2r, h2i;

	for (i = 1; i < Half / 2; i++) {

		i3 = Half - i;

		h1r = 0.5 * (RealOut[i] + RealOut[i3]);
		h1i = 0.5 * (ImagOut[i] - ImagOut[i3]);
		h2r = 0.5 * (ImagOut[i] + ImagOut[i3]);
		h2i = -0.5 * (RealOut[i] - RealOut[i3]);

		RealOut[i] = h1r + wr * h2r - wi * h2i;
		ImagOut[i] = h1i + wr * h2i + wi * h2r;
		RealOut[i3] = h1r - wr * h2r + wi * h2i;
		ImagOut[i3] = -h1i + wr * h2i + wi * h2r;

		wr = (wtemp = wr) * wpr - wi * wpi + wr;
		wi = wi * wpr + wtemp * wpi + wi;
	}

	RealOut[0] = (h1r = RealOut[0]) + ImagOut[0];
	ImagOut[0] = h1r - ImagOut[0];

	delete[]tmpReal;
	delete[]tmpImag;
}

/*
	* PowerSpectrum
	*
	* This function computes the same as RealFFT, above, but
	* adds the squares of the real and imaginary part of each
	* coefficient, extracting the power and throwing away the
	* phase.
	*
	* For speed, it does not call RealFFT, but duplicates some
	* of its code.
	*/

void PowerSpectrum(int NumSamples, float *In, float *Out)
{
	int Half = NumSamples / 2;
	int i;

	float theta = math::pi_f / Half;

	float *tmpReal = new float[Half];
	float *tmpImag = new float[Half];
	float *RealOut = new float[Half];
	float *ImagOut = new float[Half];

	for (i = 0; i < Half; i++) {
		tmpReal[i] = In[2 * i];
		tmpImag[i] = In[2 * i + 1];
	}

	FFT(Half, 0, tmpReal, tmpImag, RealOut, ImagOut);

	float wtemp = float (sin(0.5 * theta));

	float wpr = -2.0 * wtemp * wtemp;
	float wpi = float (sin(theta));
	float wr = 1.0 + wpr;
	float wi = wpi;

	int i3;

	float h1r, h1i, h2r, h2i, rt, it;

	for (i = 1; i < Half / 2; i++) {

		i3 = Half - i;

		h1r = 0.5 * (RealOut[i] + RealOut[i3]);
		h1i = 0.5 * (ImagOut[i] - ImagOut[i3]);
		h2r = 0.5 * (ImagOut[i] + ImagOut[i3]);
		h2i = -0.5 * (RealOut[i] - RealOut[i3]);

		rt = h1r + wr * h2r - wi * h2i;
		it = h1i + wr * h2i + wi * h2r;

		Out[i] = rt * rt + it * it;

		rt = h1r - wr * h2r + wi * h2i;
		it = -h1i + wr * h2i + wi * h2r;

		Out[i3] = rt * rt + it * it;

		wr = (wtemp = wr) * wpr - wi * wpi + wr;
		wi = wi * wpr + wtemp * wpi + wi;
	}

	rt = (h1r = RealOut[0]) + ImagOut[0];
	it = h1r - ImagOut[0];
	Out[0] = rt * rt + it * it;

	rt = RealOut[Half / 2];
	it = ImagOut[Half / 2];
	Out[Half / 2] = rt * rt + it * it;

	delete[] tmpReal;
	delete[] tmpImag;
	delete[] RealOut;
	delete[] ImagOut;
}

/*
	* Windowing Functions
	*/

int NumWindowFuncs()
{
	return 4;
}

const char* WindowFuncName(int whichFunction)
{
	switch (whichFunction) {
	default:
	case 0:
		return "Rectangular";
	case 1:
		return "Bartlett";
	case 2:
		return "Hamming";
	case 3:
		return "Hanning";
	}
}

void WindowFunc(int whichFunction, int NumSamples, float *in)
{
	int i;

	if (whichFunction == 1) {
		// Bartlett (triangular) window
		for (i = 0; i < NumSamples / 2; i++) {
			in[i] *= (i / (float) (NumSamples / 2));
			in[i + (NumSamples / 2)] *=
				(1.0 - (i / (float) (NumSamples / 2)));
		}
	}

	if (whichFunction == 2) {
		// Hamming
		for (i = 0; i < NumSamples; i++)
			in[i] *= 0.54 - 0.46 * cos(2 * math::pi * i / (NumSamples - 1));
	}

	if (whichFunction == 3) {
		// Hanning
		for (i = 0; i < NumSamples; i++)
			in[i] *= 0.50 - 0.50 * cos(2 * math::pi * i / (NumSamples - 1));
	}
}
}

//
///////////////////////////////////////////////////
//
	FFTClass::FFTClass() :
		bit_reverse(0),
		window(0),
		precos(0),
		presin(0),
		state_real(0),
		state_imag(0),
		fftLog(0)
	{
	}
	FFTClass::~FFTClass()
	{
		Reset();
	}
	void FFTClass::Reset()
	{
		delete[] bit_reverse;
		universalis::os::aligned_memory_dealloc(window);
		universalis::os::aligned_memory_dealloc(precos);
		universalis::os::aligned_memory_dealloc(presin);

		universalis::os::aligned_memory_dealloc(state_real);
		universalis::os::aligned_memory_dealloc(state_imag);

		delete[] fftLog;
	}

	void FFTClass::FillRectangularWindow(float window[], const std::size_t size, const float scale) {
        for (std::size_t n = 0; n < size; n++) {
			window[n] = scale;
        }
	}
	void FFTClass::FillCosineWindow(float window[], const std::size_t size, const float scale) {
		const size_t sizem1 = size-1;
        for (std::size_t n = 0; n < size; n++) {
			window[n] = std::sin(math::pi_f * n/ sizem1) * scale;
        }
	}
	void FFTClass::FillHannWindow(float window[], const std::size_t size, const float scale) {
		const size_t sizem1 = size-1;
		const float twopi = 2.0f*math::pi_f;
        for (std::size_t n = 0; n < size; n++) {
			window[n] = (0.50f - 0.50f * cosf( twopi* n / sizem1)) * scale;
        }
	}
	void FFTClass::FillHammingWindow(float window[], const std::size_t size, const float scale) {
		const size_t sizem1 = size-1;
		const float twopi = 2.0f*math::pi_f;
        for (std::size_t n = 0; n < size; n++) {
			window[n] = (0.54f - 0.46f * cosf(twopi * n / sizem1)) * scale;
        }
	}
	void FFTClass::FillGaussianWindow(float window[], const std::size_t size, const float scale) {
		const size_t sizem1 = size-1;
        for (std::size_t n = 0; n < size; n++) {
			window[n] = powf(math::e,-0.5f *powf((n-sizem1/2.f)/(0.4*sizem1/2.f),2.f)) * scale;
        }
	}
	void FFTClass::FillBlackmannWindow(float window[], const std::size_t size, const float scale) {
		const size_t sizem1 = size-1;
        for (std::size_t n = 0; n < size; n++) {
			window[n] = (0.42659 - 0.49656 * cosf(2.0*math::pi_f * n/ sizem1) + 0.076849 * cosf(4.0*math::pi_f * n /sizem1)) * scale;
        }
	}
	void FFTClass::FillBlackmannHarrisWindow(float window[], const std::size_t size, const float scale) {
		const size_t sizem1 = size-1;
        for (std::size_t n = 0; n < size; n++) {
			window[n] = (0.35875 - 0.48829 * cosf(2.0*math::pi_f * n/ sizem1) + 0.14128 * cosf(4.0*math::pi_f * n /sizem1) - 0.01168 * cosf(6.0*math::pi_f * n /sizem1)) * scale;
        }
	}

	std::size_t FFTClass::Reverse_bits(std::size_t in) {
        std::size_t r = 0, n;
	    for (n = 0; n < bufferSizeLog; n++) {
			r = (r << 1) | (in & 1);
            in >>= 1;
		}
		return r;
	}
	bool FFTClass::IsPowerOfTwo(size_t x)
	{
		return ((x & (x - 1))==0);
	}
	void FFTClass::Setup(FftWindowType type, std::size_t sizeBuf, std::size_t sizeBands)
	{
		if (!IsPowerOfTwo(sizeBuf)) {
			std::ostringstream s;
			s << "FFT called with size "<< sizeBuf;
			throw universalis::exceptions::runtime_error(s.str(), UNIVERSALIS__COMPILER__LOCATION__NO_CLASS);
		}
		bands = sizeBands;
		if (window == NULL || sizeBuf != bufferSize) {
			Reset();
			bufferSize = sizeBuf;
			outputSize = sizeBuf >> 1;
			bit_reverse = new std::size_t[bufferSize];
			universalis::os::aligned_memory_alloc(16, window, bufferSize);
			universalis::os::aligned_memory_alloc(16, precos, outputSize);
			universalis::os::aligned_memory_alloc(16, presin, outputSize);
			universalis::os::aligned_memory_alloc(16, state_real, bufferSize);
			universalis::os::aligned_memory_alloc(16, state_imag, bufferSize);
			fftLog = new float[bands];

			bufferSizeLog = 0;
			for(size_t sizetmp = bufferSize; sizetmp > 1; sizetmp>>=1) { bufferSizeLog++; } 
			for (size_t n = 0; n < bufferSize; n++) {
				bit_reverse[n] = Reverse_bits(n);
			}
			for (size_t n = 0; n < outputSize; n++) {
				float j = 2.0f*helpers::math::pi_f * n / bufferSize;
				precos[n] = cosf(j);
				presin[n] = sinf(j);
			}
		}
		switch(type){
			case rectangular: FillRectangularWindow(window, bufferSize, 1.0f);break;
			case cosine: FillCosineWindow(window, bufferSize, 1.0f);break;
			case hann: FillHannWindow(window, bufferSize, 1.0f);break;
			case hamming: FillHammingWindow(window, bufferSize, 1.0f);break;
			case gaussian: FillGaussianWindow(window, bufferSize, 1.0f);break;
			case blackmann: FillBlackmannWindow(window, bufferSize, 1.0f);break;
			case blackmannHarris: FillBlackmannHarrisWindow(window, bufferSize, 1.0f);break;
			default:break;
		}
		if ( outputSize/bands == 1 ) {
			for (std::size_t n = 0; n < bands; n++ ) {
				fftLog[n]=n;
			}
		}
		else if (outputSize/bands <= 4 ) {
			//exponential.
			const float factor = (float)outputSize/(bands*bands);
			for (std::size_t n = 0; n < bands; n++ ) {
				fftLog[n]=n*n*factor;
			}
		}
		else {
			//constant note scale.
			//factor -> set range from 2^0 to 2^8.
			//factor2 -> scale the result to the FFT output size
			//Note: x^(y*z) = (x^z)^y
			const float factor = 8.f/(float)bands;
			const float factor2 = (float)outputSize/256.f;
			for (std::size_t n = 0; n < bands; n++ ) {
				fftLog[n]=(powf(2.0f,n*factor)-1.f)*factor2;
			}
		}
		resampler.quality(dsp::resampler::quality::linear);
	}
	void FFTClass::CalculateSpectrum(float samplesIn[], float samplesOut[])
	{
        unsigned int n, k, y;
        unsigned int ex, ff;
        float fr, fi;
        float tr, ti;
        int yp;

        /* fft */
        float *rp = state_real;
        float *ip = state_imag;
        for (n = 0; n < bufferSize; n++) {
			const size_t nr = bit_reverse[n];
            *rp++ = samplesIn[ nr ] * window[nr];
            *ip++ = 0;
        }
        ex = 1;
        ff = outputSize;
        for (n = bufferSizeLog; n != 0; n--) {
			for (k = 0; k != ex; k++) {
				fr = precos[k * ff];
				fi = presin[k * ff];
                for (y = k; y < bufferSize; y += ex << 1) {
                   yp = y + ex;
                   tr = fr * state_real[yp] - fi * state_imag[yp];
                   ti = fr * state_imag[yp] + fi * state_real[yp];
                   state_real[yp] = state_real[y] - tr;
                   state_imag[yp] = state_imag[y] - ti;
                   state_real[y] += tr;
                   state_imag[y] += ti;
                }
            }
            ex <<= 1;
            ff >>= 1;
        }

        /* collect fft */
        rp = state_real; rp++;
        ip = state_imag; ip++;
        for (n = 0; n < outputSize; n++) {
            samplesOut[n] = ((*rp) * (*rp)) + ((*ip) * (*ip));
            rp++;ip++;
        }
	}
	void FFTClass::FillBandsFromFFT(float calculatedfftIn[], float banddBOut[])
	{
		helpers::math::erase_all_nans_infinities_and_denormals(calculatedfftIn,outputSize);
		float j=0.0f;
		const float dbinvSamples = psycle::helpers::dsp::dB(1.0f/(bufferSize>>2));
        for (unsigned int h=0, a=0;h<bands;h++)
		{
			float afloat = fftLog[h];
			/*if (afloat < 1.f) {
				//This was intended as a DC bar, but it isn't only DC.
				j = calculatedfftIn[a];
			}
			else*/ if (afloat +1.0f > fftLog[h+1]) {
				j = resampler.work_float(calculatedfftIn,afloat,outputSize, NULL, calculatedfftIn, calculatedfftIn+outputSize-1);
				a = math::round<int,float>(afloat);
			}
			else {
				j = calculatedfftIn[a];
				while(a<=afloat){
					j = std::max(j,calculatedfftIn[a]);
					a++;
				}
			}//+0.0000000001f is -100dB of power. Used to prevent evaluating powerdB(0.0)
			banddBOut[h]=powerdB(j+0.0000000001f)+dbinvSamples;
		}
	}
	float FFTClass::resample(float const * data, float offset, uint64_t length)
	{
		return resampler.work_float(data, offset, length, NULL, data, data+length-1);
	}
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
// arch-tag: 47691958-d393-488c-abc5-81178ea2686e
