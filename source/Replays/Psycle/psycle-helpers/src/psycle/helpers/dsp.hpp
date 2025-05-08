///\file
///\brief various signal processing utility functions and classes, psycle::helpers::dsp::Cubic amongst others.
#pragma once
#include "math/erase_all_nans_infinities_and_denormals.hpp"
#if defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS 
	#include <xmmintrin.h>
#endif
#include <cmath>
#include <cstring>
#include <cstddef> // for std::ptrdiff_t

#if defined BOOST_AUTO_TEST_CASE
	#include <universalis/os/aligned_alloc.hpp>
	#include <universalis/os/clocks.hpp>
	#include <sstream>
#endif
namespace psycle { namespace helpers { /** various signal processing utility functions. */ namespace dsp {

using namespace universalis::stdlib;

	/// linear -> deciBell
	/// amplitude normalized to 1.0f.
	float inline UNIVERSALIS__COMPILER__CONST_FUNCTION
	dB(float amplitude)
	{
		///\todo merge with psycle::helpers::math::linear_to_deci_bell
		return 20.0f * std::log10(amplitude);
	}

	/// deciBell -> linear
	float inline UNIVERSALIS__COMPILER__CONST_FUNCTION
	dB2Amp(float db)
	{
		///\todo merge with psycle::helpers::math::deci_bell_to_linear
		return std::pow(10.0f, db / 20.0f);
	}
	/// linear -> deciBell
	/// power normalized to 1.0f.
	float inline UNIVERSALIS__COMPILER__CONST_FUNCTION
	powerdB(float power)
	{
		return 10.0f * std::log10(power);
	}

	/// deciBell -> linear
	float inline UNIVERSALIS__COMPILER__CONST_FUNCTION
	dB2power(float db)
	{
		return std::pow(10.0f, db / 10.0f);
	}
	//These two are used for UI in some places, so that sliders get 0dB in the middle.
	float inline UNIVERSALIS__COMPILER__CONST_FUNCTION
	SliderToAmountVert(int nPos) {
		return ((1024-nPos)*(1024-nPos))/(16384.f*4.f*4.f);
	}

	int inline UNIVERSALIS__COMPILER__CONST_FUNCTION
	AmountToSliderVert(float amount) {
		int t = (int)std::sqrt(amount*16384.f*4.f*4.f);
		return 1024-t;
	}
	//These two are used for UI in some places, so that sliders get 0dB in the middle.
	float inline UNIVERSALIS__COMPILER__CONST_FUNCTION
	SliderToAmountHoriz(int nPos) {
		return (nPos*nPos)/(16384.f*4.f*4.f);
	}

	int inline UNIVERSALIS__COMPILER__CONST_FUNCTION
	AmountToSliderHoriz(float amount) {
		int t = (int)std::sqrt(amount*16384.f*4.f*4.f);
		return t;
	}
	/// undenormalize (renormalize) samples in a signal buffer.
	///\todo make a template version that accept both float and doubles
	inline void Undenormalize(float * UNIVERSALIS__COMPILER__RESTRICT pSamplesL,float * UNIVERSALIS__COMPILER__RESTRICT pSamplesR, int numsamples)
	{
		math::erase_all_nans_infinities_and_denormals(pSamplesL, numsamples);
		math::erase_all_nans_infinities_and_denormals(pSamplesR, numsamples);
	}

	/****************************************************************************/

  inline void SimpleAdd(const float * UNIVERSALIS__COMPILER__RESTRICT pSrcSamples, float * UNIVERSALIS__COMPILER__RESTRICT pDstSamples, int numSamples)
  {
    #if defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS    
		const __m128* psrc = (const __m128*)pSrcSamples;
		__m128* pdst = (__m128*)pDstSamples;
		numSamples += 3;
		numSamples >>= 2;
		for(int i = 0; i < numSamples; ++i) pdst[i] = _mm_add_ps(pdst[i], psrc[i]);
    #else
    for(int i = 0; i < numSamples; ++i) pDstSamples[i] += pSrcSamples[i];
		#endif
  }

  inline void SimpleSub(const float * UNIVERSALIS__COMPILER__RESTRICT pSrcSamples, float * UNIVERSALIS__COMPILER__RESTRICT pDstSamples, int numSamples)
  {
    #if defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS    
		const __m128* psrc = (const __m128*)pSrcSamples;
		__m128* pdst = (__m128*)pDstSamples;
		numSamples += 3;
		numSamples >>= 2;
		for(int i = 0; i < numSamples; ++i) pdst[i] = _mm_sub_ps(pdst[i], psrc[i]);
    #else
    for(int i = 0; i < numSamples; ++i) pDstSamples[i] -= pSrcSamples[i];
		#endif
  }

	/// mixes two signals. memory should be aligned by 16 in optimized paths.
	inline void Add(const float * UNIVERSALIS__COMPILER__RESTRICT pSrcSamples, float * UNIVERSALIS__COMPILER__RESTRICT pDstSamples, int numSamples, float vol)
	{
		#if defined DIVERSALIS__COMPILER__GNU
			numSamples += 3;
			numSamples >>= 2;
			typedef float vec __attribute__((vector_size(4 * sizeof(float))));
			const vec vol_vec = { vol, vol, vol, vol };
			const vec* src = reinterpret_cast<const vec*>(pSrcSamples);
			vec* dst = reinterpret_cast<vec*>(pDstSamples);
			#pragma omp parallel for // with gcc, build with the -fopenmp flag
			for(int i = 0; i < numSamples; ++i) dst[i] += src[i] * vol_vec;
		#elif defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS
			const __m128 volps = _mm_set_ps1(vol);
			const __m128* psrc = (const __m128*)pSrcSamples;
			__m128* pdst = (__m128*)pDstSamples;
			numSamples += 3;
			numSamples >>= 2;
			for(int i = 0; i < numSamples; ++i) pdst[i] = _mm_add_ps(pdst[i], _mm_mul_ps(psrc[i], volps));
		#elif defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__ASSEMBLER__INTEL
			__asm
			{
					movss xmm2, vol
					shufps xmm2, xmm2, 0H
					mov esi, pSrcSamples
					mov edi, pDstSamples
					mov eax, [numSamples]
				LOOPSTART:
					cmp eax, 0
					jle END
					movaps xmm0, [esi]
					movaps xmm1, [edi]
					mulps xmm0, xmm2
					addps xmm0, xmm1
					movaps [edi], xmm0
					add esi, 10H
					add edi, 10H
					sub eax, 4
					jmp LOOPSTART
				END:
			}
		#else
			for(int i = 0; i < numSamples; ++i) pDstSamples[i] += pSrcSamples[i] * vol;
		#endif
	}

	/// multiply a signal by a ratio, inplace. memory should be aligned by 16 in optimized paths.
	///\see MovMul()
	inline void Mul(float * UNIVERSALIS__COMPILER__RESTRICT pDstSamples, int numSamples, float multi)
	{
		#if defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS
			const __m128 volps = _mm_set_ps1(multi);
			__m128 *pdst = (__m128*)pDstSamples;
			while(numSamples>0)
			{
				*pdst = _mm_mul_ps(*pdst,volps);
				pdst++;
				numSamples-=4;
			}
		#elif defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__ASSEMBLER__INTEL
			// This code assumes aligned memory (to 16) and assigned by powers of 4!
			__asm
			{
					movss xmm2, multi
					shufps xmm2, xmm2, 0H
					mov edi, pDstSamples
					mov eax, [numSamples]
				LOOPSTART:
					cmp eax, 0
					jle END
					movaps xmm0, [edi]
					mulps xmm0, xmm2
					movaps [edi], xmm0
					add edi, 10H
					sub eax, 4
					jmp LOOPSTART
				END:
			}
		#else
			for(int i = 0; i < numSamples; ++i) pDstSamples[i] *= multi;
		#endif
	}

		/// multiply a signal by a ratio and store in another. pSrcSamples need to be aligned by 16 in optimized paths. pDstSamples can be unaligned
	///\see Mul()
	inline void MovMul(const float * UNIVERSALIS__COMPILER__RESTRICT pSrcSamples, float * UNIVERSALIS__COMPILER__RESTRICT pDstSamples, int numSamples, float multi)
	{
		#if defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS
			const __m128 volps = _mm_set_ps1(multi);
			const __m128 *psrc = (__m128*)pSrcSamples;
			if (reinterpret_cast<std::ptrdiff_t>(pDstSamples)&0xF) {
				while(numSamples>3)
				{
					_mm_storeu_ps(pDstSamples,_mm_mul_ps(*psrc,volps));
					psrc++;
					pDstSamples+=4;
					numSamples-=4;
				}
				if (numSamples>0) {
					for(int i = 0; i < numSamples; ++i) pDstSamples[i] = pSrcSamples[i] * multi;
				}
			}
			else {
				while(numSamples>0)
				{
					_mm_store_ps(pDstSamples,_mm_mul_ps(*psrc,volps));
					psrc++;
					pDstSamples+=4;
					numSamples-=4;
				}
			}
		#elif defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__ASSEMBLER__INTEL
#if DIVERSALIS__CPU__SIZEOF_POINTER == 8
			if (reinterpret_cast<__int64>(pDstSamples)&0xF) {
#else
			if (reinterpret_cast<__int32>(pDstSamples)&0xF) {
#endif
				__asm
				{
					movss xmm2, multi
					shufps xmm2, xmm2, 0H
					mov esi, pSrcSamples
					mov edi, pDstSamples
					mov eax, [numSamples]
				LOOPSTART:
					cmp eax, 3
					jle END
					movaps xmm0, [esi]
					mulps xmm0, xmm2
					movups [edi], xmm0
					add esi, 10H
					add edi, 10H
					sub eax, 4
					jmp LOOPSTART
				END:
				}
				if (numSamples>0) {
					for(int i = 0; i < numSamples; ++i) pDstSamples[i] = pSrcSamples[i] * multi;
				}
			}
			else{
				__asm
				{
					movss xmm2, multi
					shufps xmm2, xmm2, 0H
					mov esi, pSrcSamples
					mov edi, pDstSamples
					mov eax, [numSamples]
				LOOPSTART:
					cmp eax, 0
					jle END
					movaps xmm0, [esi]
					mulps xmm0, xmm2
					movaps [edi], xmm0
					add esi, 10H
					add edi, 10H
					sub eax, 4
					jmp LOOPSTART
				END:
				}
			}
		#else
			for(int i = 0; i < numSamples; ++i) pDstSamples[i] = pSrcSamples[i] * multi;
		#endif
	}

	//Copy a signal into another. pSrcSamples need to be aligned by 16 in optimized paths. pDstSamples can be unaligned
	inline void Mov(const float * UNIVERSALIS__COMPILER__RESTRICT pSrcSamples, float * UNIVERSALIS__COMPILER__RESTRICT pDstSamples, int numSamples)
	{
		#if defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS
			if (reinterpret_cast<std::ptrdiff_t>(pDstSamples)&0xF) {
				while(numSamples>3)
				{
					_mm_storeu_ps(pDstSamples,_mm_load_ps(pSrcSamples));
					pSrcSamples+=4;
					pDstSamples+=4;
					numSamples-=4;
				}
				if (numSamples>0) { //copy last samples.
					std::memcpy(pDstSamples, pSrcSamples, numSamples * sizeof *pSrcSamples);
				}
			}
			else {
				while(numSamples>0)
				{
					_mm_store_ps(pDstSamples,_mm_load_ps(pSrcSamples));
					pSrcSamples+=4;
					pDstSamples+=4;
					numSamples-=4;
				}
			}
		#elif defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__ASSEMBLER__INTEL
#if DIVERSALIS__CPU__SIZEOF_POINTER == 8
			if (reinterpret_cast<__int64>(pDstSamples)&0xF) {
#else
			if (reinterpret_cast<__int32>(pDstSamples)&0xF) {
#endif
				__asm
				{
					mov esi, pSrcSamples
					mov edi, pDstSamples
					mov eax, [numSamples]
				LOOPSTART:
					cmp eax, 3
					jle END
					movaps xmm0, [esi]
					movups [edi], xmm0
					add esi, 10H
					add edi, 10H
					sub eax, 4
					jmp LOOPSTART
				END:
					mov pSrcSamples, esi
					mov pDstSamples, edi
					mov numSamples, eax
				}
				if (numSamples>0) {
					std::memcpy(pDstSamples, pSrcSamples, numSamples * sizeof *pSrcSamples);
				}
			}
			else {
				__asm
				{
					mov esi, pSrcSamples
					mov edi, pDstSamples
					mov eax, [numSamples]
				LOOPSTART:
					cmp eax, 0
					jle END
					movaps xmm0, [esi]
					movaps [edi], xmm0
					add esi, 10H
					add edi, 10H
					sub eax, 4
					jmp LOOPSTART
				END:
				}
			}
		#else
			std::memcpy(pDstSamples, pSrcSamples, numSamples * sizeof *pSrcSamples);
		#endif
	}


	/// zero-out a signal buffer. samples need to be aligned by 16 in optimized paths.
	inline void Clear(float *pDstSamples, int numSamples)
	{
		#if defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS
			const __m128 zeroval = _mm_set_ps1(0.0f);
			while(numSamples>0)
			{
				_mm_store_ps(pDstSamples,zeroval);
				pDstSamples+=4;
				numSamples-=4;
			}
		#elif defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__ASSEMBLER__INTEL
			// This code assumes aligned memory (to 16) and assigned by powers of 4!
			__asm
			{
					xorps xmm0, xmm0
					mov edi, pDstSamples
					mov eax, [numSamples]
				LOOPSTART:
					cmp eax, 0
					jle END
					movaps [edi], xmm0
					add edi, 10H
					sub eax, 4
					jmp LOOPSTART
				END:
			}
		#else
			std::memset(pDstSamples, 0, numSamples * sizeof *pDstSamples);
		#endif
	}

	extern int numRMSSamples;
	struct RMSData {
		int count;
		double AccumLeft, AccumRight;
		float previousLeft, previousRight;
	};

	//helper method for GetRMSVol. samples need to be aligned by 16 in optimized paths.
	inline void accumulateRMS(RMSData &rms,const float * UNIVERSALIS__COMPILER__RESTRICT pSamplesL, const float * UNIVERSALIS__COMPILER__RESTRICT pSamplesR,  int count)
	{
#if defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS
		__m128 acleftvec = _mm_set_ps1(0.0f);
		__m128 acrightvec = _mm_set_ps1(0.0f);
		const __m128 *psrcl = (const __m128*)pSamplesL;
		const __m128 *psrcr = (const __m128*)pSamplesR;
		while (count > 3){
			acleftvec = _mm_add_ps(acleftvec,_mm_mul_ps(*psrcl,*psrcl));
			acrightvec = _mm_add_ps(acrightvec,_mm_mul_ps(*psrcr,*psrcr));
			psrcl++;
			psrcr++;
			count-=4;
		}
		float result;
		acleftvec = _mm_add_ps(acleftvec,_mm_movehl_ps(acleftvec, acleftvec)); // add (0= 0+2 and 1=1+3 thanks to the move)
		acleftvec = _mm_add_ss(acleftvec,_mm_shuffle_ps(acleftvec,acleftvec,0x11)); // add (0 = 0+1 thanks to the shuffle)
		_mm_store_ss(&result, acleftvec); //get the position 0
		rms.AccumLeft += result;
		acrightvec = _mm_add_ps(acrightvec,_mm_movehl_ps(acrightvec, acrightvec)); // add (0= 0+2 and 1=1+3 thanks to the move)
		acrightvec = _mm_add_ss(acrightvec,_mm_shuffle_ps(acrightvec,acrightvec,0x11)); // add (0 = 0+1 thanks to the shuffle)
		_mm_store_ss(&result, acrightvec); //get the position 0
		rms.AccumRight += result;
		if (count > 0 ) {
			const float* pL = (const float*)psrcl;
			const float* pR= (const float*)psrcr;
			while (count--) {
				rms.AccumLeft += (*pL * *pL); pL++;
				rms.AccumRight += (*pR * *pR); pR++;
			}
		}
#else
		double acleft(rms.AccumLeft),acright(rms.AccumRight);
		--pSamplesL; --pSamplesR;
		while (count--) {
			++pSamplesL; acleft  += *pSamplesL * *pSamplesL;
			++pSamplesR; acright += *pSamplesR * *pSamplesR;
		};
		rms.AccumLeft = acleft;
		rms.AccumRight = acright;
#endif
	}

	/// finds the RMS volume value in a signal buffer. samples need to be aligned by 16 in optimized paths.
	/// Note: Values are accumulated since the standard calculation requires 50ms of data.
	inline float GetRMSVol(RMSData &rms,const float * UNIVERSALIS__COMPILER__RESTRICT pSamplesL, const float * UNIVERSALIS__COMPILER__RESTRICT pSamplesR, int numSamples)
	{
		const float * pL = pSamplesL;
		const float * pR = pSamplesR;

		int ns = numSamples;
		int count(numRMSSamples - rms.count);
		if ( ns >= count)
		{
			// count can be negative when changing the samplerate.
			if (count >= 0) {
				accumulateRMS(rms, pSamplesL, pSamplesR, count);
#if defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS
				//small workaround for 16byte boundary (it makes it slightly incorrect, but hopefully just a bit).
				ns -= count&0x3;
				count = count&~0x3;
#endif
				ns -= count;
				pL+=count; pR+=count;
			}
			rms.previousLeft  = std::sqrt(rms.AccumLeft  / numRMSSamples);
			rms.previousRight = std::sqrt(rms.AccumRight / numRMSSamples);
			rms.AccumLeft = 0;
			rms.AccumRight = 0;
			rms.count = 0;
		}
		rms.count+=ns;
		accumulateRMS(rms, pL, pR, ns);
		return rms.previousLeft > rms.previousRight ? rms.previousLeft : rms.previousRight;
	}

	/// finds the maximum amplitude in a signal buffer. samples need to be aligned by 16 in optimized paths.
	inline float GetMaxVol(const float * UNIVERSALIS__COMPILER__RESTRICT pSamplesL, const float * UNIVERSALIS__COMPILER__RESTRICT pSamplesR, int numSamples)
	{
		#if defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS
			__m128 minVol = _mm_set_ps1(0.0f);
			__m128 maxVol = _mm_set_ps1(0.0f);
			const __m128 *psrcl = (const __m128*)pSamplesL;
			const __m128 *psrcr = (const __m128*)pSamplesR;
			while(numSamples > 0) {
				maxVol =  _mm_max_ps(maxVol,*psrcl);
				maxVol =  _mm_max_ps(maxVol,*psrcr);
				minVol =  _mm_min_ps(minVol,*psrcl);
				minVol =  _mm_min_ps(minVol,*psrcr);
				psrcl++;
				psrcr++;
				numSamples-=4;
			}
			__m128 highTmp = _mm_movehl_ps(maxVol, maxVol);
			maxVol =  _mm_max_ps(maxVol,highTmp);
			highTmp = _mm_move_ss(highTmp,maxVol);
			maxVol = _mm_shuffle_ps(maxVol, highTmp, 0x11);
			maxVol =  _mm_max_ps(maxVol,highTmp);
			
			__m128 lowTmp = _mm_movehl_ps(minVol, minVol);
			minVol =  _mm_max_ps(minVol,lowTmp);
			lowTmp = _mm_move_ss(lowTmp,minVol);
			minVol = _mm_shuffle_ps(minVol, lowTmp, 0x11);
			minVol =  _mm_max_ps(minVol,lowTmp);

			__m128 minus1 = _mm_set_ps1(-1.0f);
			minVol = _mm_mul_ss(minVol, minus1);
			//beware: -0.0f and 0.0f are supposed to be the same number, -0.0f > 0.0f returns false, just because 0.0f = 0.0f returns true
			maxVol = _mm_max_ps(minVol,maxVol);
			float result;
			_mm_store_ss(&result, maxVol);
			return result;
		#elif defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__ASSEMBLER__INTEL
			// If anyone knows better assembler than me improve this variable utilization:
			float volmax = 0.0f, volmin = 0.0f;
			float *volmaxb = &volmax, *volminb = &volmin;
			__asm
			{
					// we store the max in xmm0 and the min in xmm1
					xorps xmm0, xmm0
					xorps xmm1, xmm1
					mov esi, [pSamplesL]
					mov edi, [pSamplesR]
					mov eax, [numSamples]
					// Loop does: get the 4 max values and 4 min values in xmm0 and xmm1 respct.
				LOOPSTART:
					cmp eax, 0
					jle END
					maxps xmm0,[esi]
					maxps xmm0,[edi]
					minps xmm1,[esi]
					minps xmm1,[edi]
					add esi, 10H
					add edi, 10H
					sub eax, 4
					jmp LOOPSTART
				END:
					// to finish, get the max value of each of the four values.
					// put 02 and 03 to 20 and 21
					movhlps xmm2, xmm0
					// find max of 00 and 20 (02) and of 01 and 21 (03)
					maxps xmm0, xmm2
					// put 00 (result of max(00,02)) to 20
					movss xmm2, xmm0
					// put 01 (result of max(01,03)) into 00 (that's the only one we care about)
					shufps xmm0, xmm2, 11H
					// and find max of 00 (01) and 20 (00)
					maxps xmm0, xmm2

					movhlps xmm2, xmm1
					minps xmm1, xmm2
					movss xmm2, xmm1
					shufps xmm1, xmm2, 11H
					minps xmm1, xmm2

					mov edi, volmaxb
					movss [edi], xmm0
					mov edi, volminb
					movss [edi], xmm1
			}
			volmin*=-1.0f;
			//beware: -0.0f and 0.0f are supposed to be the same number, -0.0f > 0.0f returns false, just because 0.0f = 0.0f returns true
			return (volmin>volmax)?volmin:volmax;
		#else
			--pSamplesL; --pSamplesR;
			float vol = 0.0f;
			do { /// not all waves are symmetrical
				const float volL = std::fabs(*++pSamplesL);
				const float volR = std::fabs(*++pSamplesR);
				if (volL > vol) vol = volL;
				if (volR > vol) vol = volR;
			} while (--numSamples);
			return vol;
		#endif
	}
	//same method, for a single buffer (allowing to calculate max for each buffer). samples need to be aligned by 16 in optimized paths.
	inline float GetMaxVol(const float * UNIVERSALIS__COMPILER__RESTRICT pSamples, int numSamples)
	{
		#if defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__FEATURE__XMM_INTRINSICS
			__m128 minVol = _mm_set_ps1(0.0f);
			__m128 maxVol = _mm_set_ps1(0.0f);
			const __m128 *psrc = (const __m128*)pSamples;
			while(numSamples > 0) {
				maxVol =  _mm_max_ps(maxVol,*psrc);
				minVol =  _mm_min_ps(minVol,*psrc);
				psrc++;
				numSamples-=4;
			}
			__m128 highTmp = _mm_movehl_ps(maxVol, maxVol);
			maxVol =  _mm_max_ps(maxVol,highTmp);
			highTmp = _mm_move_ss(highTmp,maxVol);
			maxVol = _mm_shuffle_ps(maxVol, highTmp, 0x11);
			maxVol =  _mm_max_ps(maxVol,highTmp);
			
			__m128 lowTmp = _mm_movehl_ps(minVol, minVol);
			minVol =  _mm_max_ps(minVol,lowTmp);
			lowTmp = _mm_move_ss(lowTmp,minVol);
			minVol = _mm_shuffle_ps(minVol, lowTmp, 0x11);
			minVol =  _mm_max_ps(minVol,lowTmp);

			__m128 minus1 = _mm_set_ps1(-1.0f);
			minVol = _mm_mul_ss(minVol, minus1);
			//beware: -0.0f and 0.0f are supposed to be the same number, -0.0f > 0.0f returns false, just because 0.0f == 0.0f returns true
			maxVol = _mm_max_ps(minVol,maxVol);
			float result;
			_mm_store_ss(&result, maxVol);
			return result;
		#elif defined DIVERSALIS__CPU__X86__SSE && defined DIVERSALIS__COMPILER__ASSEMBLER__INTEL
			// If anyone knows better assembler than me improve this variable utilization:
			float volmax = 0.0f, volmin = 0.0f;
			float *volmaxb = &volmax, *volminb = &volmin;
			__asm
			{
					// we store the max in xmm0 and the min in xmm1
					xorps xmm0, xmm0
					xorps xmm1, xmm1
					mov esi, [pSamples]
					mov eax, [numSamples]
					// Loop does: get the 4 max values and 4 min values in xmm0 and xmm1 respct.
				LOOPSTART:
					cmp eax, 0
					jle END
					maxps xmm0,[esi]
					minps xmm1,[esi]
					add esi, 10H
					sub eax, 4
					jmp LOOPSTART
				END:
					// to finish, get the max and of each of the four values.
					// put 02 and 03 to 20 and 21
					movhlps xmm2, xmm0
					// find max of 00 and 20 (02) and of 01 and 21 (03)
					maxps xmm0, xmm2
					// put 00 (result of max(00,02)) to 20
					movss xmm2, xmm0
					// put 01 (result of max(01,03)) into 00 (that's the only one we care about)
					shufps xmm0, xmm2, 11H
					// and find max of 00 (01) and 20 (00)
					maxps xmm0, xmm2

					movhlps xmm2, xmm1
					minps xmm1, xmm2
					movss xmm2, xmm1
					shufps xmm1, xmm2, 11H
					minps xmm1, xmm2

					mov edi, volmaxb
					movss [edi], xmm0
					mov edi, volminb
					movss [edi], xmm1
			}
			volmin*=-1.0f;
			//beware: -0.0f and 0.0f are supposed to be the same number, -0.0f > 0.0f returns false, just because 0.0f == 0.0f returns true
			return (volmin>volmax)?volmin:volmax;
		#else
			--pSamples;
			float vol = 0.0f;
			do { /// not all waves are symmetrical
				const float nVol = std::fabs(*++pSamples);
				if (nVol > vol) vol = nVol;
			} while (--numSamples);
			return vol;
		#endif
	}


	#if defined BOOST_AUTO_TEST_CASE
		BOOST_AUTO_TEST_CASE(dsp_test) {
			std::size_t const alignment = 16;
			std::vector<float, universalis::os::aligned_allocator<float, alignment> > v1, v2;
			for(std::size_t s = 0; s < 10000; ++s) {
				v1.push_back(s);
				v2.push_back(s);
			}
			typedef universalis::os::clocks::steady clock;
			int const iterations = 10000;
			float const vol = 0.5;

			{ // add
				std::size_t const count = v1.size() >> 2 << 2; // stop at multiple of four
				clock::time_point const t1(clock::now());
				for(int i(0); i < iterations; ++i) Add(&v1[0], &v2[0], count, vol);
				clock::time_point const t2(clock::now());
				for(int i(0); i < iterations; ++i) for(std::size_t j(0); j < count; ++j) v2[j] += v1[j] * vol;
				clock::time_point const t3(clock::now());
				{
					std::ostringstream s;
					using namespace universalis::stdlib::chrono;
					s << "add: " << nanoseconds(t2 - t1).count() * 1e-9 << "s < " << nanoseconds(t3 - t2).count() * 1e-9 << "s";
					BOOST_MESSAGE(s.str());
				}
				BOOST_CHECK(t2 - t1 < t3 - t2);
			}
		}
	#endif

}}}
