/*
Copyright (c) 2003-2004, Mark Borgerding
Lots of modifications by Jean-Marc Valin
Copyright (c) 2005-2007, Xiph.Org Foundation
Copyright (c) 2008,      Xiph.Org Foundation, CSIRO

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the author nor the names of any contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef KISS_FFT_H
#define KISS_FFT_H

#include <stdlib.h>
#include <math.h>
#include "arch.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 ATTENTION!
 If you would like a :
 -- a utility that will handle the caching of fft objects
 -- real-only (no imaginary time component ) FFT
 -- a multi-dimensional FFT
 -- a command-line utility to perform ffts
 -- a command-line utility to perform fast-convolution filtering

 Then see kfc.h kiss_fftr.h kiss_fftnd.h fftutil.c kiss_fastfir.c
  in the tools/ directory.
*/

#ifdef USE_SIMD
# include <xmmintrin.h>
# define kiss_fft_scalar __m128
#define KISS_FFT_MALLOC(nbytes) memalign(16,nbytes)
#else	
#define KISS_FFT_MALLOC celt_alloc
#endif	


#ifdef FIXED_POINT
#include "arch.h"	
#define DOUBLE_PRECISION

#ifdef DOUBLE_PRECISION
#  define kiss_fft_scalar celt_int32
#  define kiss_twiddle_scalar celt_int16
#  define KF_SUFFIX _celt_double
#else
#  define kiss_fft_scalar celt_int16
#  define kiss_twiddle_scalar celt_int16
#  define KF_SUFFIX _celt_single
#endif
#else
# ifndef kiss_fft_scalar
/*  default is float */
#   define kiss_fft_scalar float
#   define kiss_twiddle_scalar float
#   define KF_SUFFIX _celt_single
# endif
#endif

#if 0
/* This adds a suffix to all the kiss_fft functions so we
   can easily link with more than one copy of the fft */
#define CAT_SUFFIX(a,b) a ## b
#define SUF(a,b) CAT_SUFFIX(a, b)

#define kiss_fft_alloc_twiddles SUF(kiss_fft_alloc_twiddles,KF_SUFFIX)
#define kiss_fft_alloc SUF(kiss_fft_alloc,KF_SUFFIX)
#define kiss_fft SUF(kiss_fft,KF_SUFFIX)
#define kiss_ifft SUF(kiss_ifft,KF_SUFFIX)
#define kiss_fft_stride SUF(kiss_fft_stride,KF_SUFFIX)
#define kiss_ifft_stride SUF(kiss_ifft_stride,KF_SUFFIX)
#define kiss_fft_free SUF(kiss_fft_free,KF_SUFFIX)

#endif

typedef struct {
    kiss_fft_scalar r;
    kiss_fft_scalar i;
}kiss_fft_cpx;

typedef struct {
   kiss_twiddle_scalar r;
   kiss_twiddle_scalar i;
}kiss_twiddle_cpx;

#define MAXFACTORS 8
/* e.g. an fft of length 128 has 4 factors
 as far as kissfft is concerned
 4*4*4*2
 */

typedef struct kiss_fft_state{
    int nfft;
#ifndef FIXED_POINT
    kiss_fft_scalar scale;
#endif
    int shift;
    celt_int16 factors[2*MAXFACTORS];
    const celt_int16 *bitrev;
    const kiss_twiddle_cpx *twiddles;
} kiss_fft_state;

//typedef struct kiss_fft_state* kiss_fft_cfg;

/** 
 *  kiss_fft_alloc
 *  
 *  Initialize a FFT (or IFFT) algorithm's cfg/state buffer.
 *
 *  typical usage:      kiss_fft_cfg mycfg=kiss_fft_alloc(1024,0,NULL,NULL);
 *
 *  The return value from fft_alloc is a cfg buffer used internally
 *  by the fft routine or NULL.
 *
 *  If lenmem is NULL, then kiss_fft_alloc will allocate a cfg buffer using malloc.
 *  The returned value should be free()d when done to avoid memory leaks.
 *  
 *  The state can be placed in a user supplied buffer 'mem':
 *  If lenmem is not NULL and mem is not NULL and *lenmem is large enough,
 *      then the function places the cfg in mem and the size used in *lenmem
 *      and returns mem.
 *  
 *  If lenmem is not NULL and ( mem is NULL or *lenmem is not large enough),
 *      then the function returns NULL and places the minimum cfg 
 *      buffer size in *lenmem.
 * */

kiss_fft_state *kiss_fft_alloc_twiddles(int nfft,void * mem,size_t * lenmem, const kiss_fft_state *base);

kiss_fft_state *kiss_fft_alloc(int nfft,void * mem,size_t * lenmem);

/**
 * kiss_fft(cfg,in_out_buf)
 *
 * Perform an FFT on a complex input buffer.
 * for a forward FFT,
 * fin should be  f[0] , f[1] , ... ,f[nfft-1]
 * fout will be   F[0] , F[1] , ... ,F[nfft-1]
 * Note that each element is complex and can be accessed like
    f[k].r and f[k].i
 * */
void kiss_fft(const kiss_fft_state *cfg,const kiss_fft_cpx *fin,kiss_fft_cpx *fout);
void kiss_ifft(const kiss_fft_state *cfg,const kiss_fft_cpx *fin,kiss_fft_cpx *fout);

void kiss_fft_free(const kiss_fft_state *cfg);


#ifdef __cplusplus
} 
#endif

#endif
