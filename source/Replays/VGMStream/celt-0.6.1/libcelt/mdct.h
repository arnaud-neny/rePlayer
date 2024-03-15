/* (C) 2008 Jean-Marc Valin, CSIRO
*/
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* This is a simple MDCT implementation that uses a N/4 complex FFT
   to do most of the work. It should be relatively straightforward to
   plug in pretty much and FFT here.
   
   This replaces the Vorbis FFT (and uses the exact same API), which 
   was a bit too messy and that was ending up duplicating code 
   (might as well use the same FFT everywhere).
   
   The algorithm is similar to (and inspired from) Fabrice Bellard's
   MDCT implementation in FFMPEG, but has differences in signs, ordering
   and scaling in many places. 
*/

#ifndef MDCT_H
#define MDCT_H

#include "kiss_fft.h"
#include "arch.h"

typedef struct {
   int n;
   kiss_fft_cfg kfft;
   kiss_twiddle_scalar * restrict trig;
} mdct_lookup;

void mdct_init(mdct_lookup *l,int N);
void mdct_clear(mdct_lookup *l);

/** Compute a forward MDCT and scale by 4/N */
void mdct_forward(const mdct_lookup *l, kiss_fft_scalar *in, kiss_fft_scalar *out, const celt_word16_t *window, int overlap);

/** Compute a backward MDCT (no scaling) and performs weighted overlap-add 
    (scales implicitly by 1/2) */
void mdct_backward(const mdct_lookup *l, kiss_fft_scalar *in, kiss_fft_scalar *out, const celt_word16_t * restrict window, int overlap);

#endif
