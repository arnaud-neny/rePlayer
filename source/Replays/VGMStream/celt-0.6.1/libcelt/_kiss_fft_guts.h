/*
Copyright (c) 2003-2004, Mark Borgerding

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the author nor the names of any contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef KISS_FFT_GUTS_H
#define KISS_FFT_GUTS_H

#define MIN(a,b) ((a)<(b) ? (a):(b))
#define MAX(a,b) ((a)>(b) ? (a):(b))

/* kiss_fft.h
   defines kiss_fft_scalar as either short or a float type
   and defines
   typedef struct { kiss_fft_scalar r; kiss_fft_scalar i; }kiss_fft_cpx; */
#include "kiss_fft.h"

#define MAXFACTORS 32
/* e.g. an fft of length 128 has 4 factors 
 as far as kissfft is concerned
 4*4*4*2
 */

struct kiss_fft_state{
    int nfft;
#ifndef FIXED_POINT
    kiss_fft_scalar scale;
#endif
    int factors[2*MAXFACTORS];
    int *bitrev;
    kiss_twiddle_cpx twiddles[1];
};

/*
  Explanation of macros dealing with complex math:

   C_MUL(m,a,b)         : m = a*b
   C_FIXDIV( c , div )  : if a fixed point impl., c /= div. noop otherwise
   C_SUB( res, a,b)     : res = a - b
   C_SUBFROM( res , a)  : res -= a
   C_ADDTO( res , a)    : res += a
 * */
#ifdef FIXED_POINT
#include "arch.h"

#ifdef DOUBLE_PRECISION

# define FRACBITS 31
# define SAMPPROD celt_int64_t 
#define SAMP_MAX 2147483647
#ifdef MIXED_PRECISION
#define TWID_MAX 32767
#define TRIG_UPSCALE 1
#else
#define TRIG_UPSCALE 65536
#define TWID_MAX 2147483647
#endif
#define EXT32(a) (a)

#else /* DOUBLE_PRECISION */

# define FRACBITS 15
# define SAMPPROD celt_int32_t 
#define SAMP_MAX 32767
#define TRIG_UPSCALE 1
#define EXT32(a) EXTEND32(a)

#endif /* !DOUBLE_PRECISION */

#define SAMP_MIN -SAMP_MAX

#if defined(CHECK_OVERFLOW)
#  define CHECK_OVERFLOW_OP(a,op,b)  \
	if ( (SAMPPROD)(a) op (SAMPPROD)(b) > SAMP_MAX || (SAMPPROD)(a) op (SAMPPROD)(b) < SAMP_MIN ) { \
		fprintf(stderr,"WARNING:overflow @ " __FILE__ "(%d): (%d " #op" %d) = %ld\n",__LINE__,(a),(b),(SAMPPROD)(a) op (SAMPPROD)(b) );  }
#endif

#   define smul(a,b) ( (SAMPPROD)(a)*(b) )
#   define sround( x )  (kiss_fft_scalar)( ( (x) + ((SAMPPROD)1<<(FRACBITS-1)) ) >> FRACBITS )

#ifdef MIXED_PRECISION

#   define S_MUL(a,b) MULT16_32_Q15(b, a)

#   define C_MUL(m,a,b) \
      do{ (m).r = SUB32(S_MUL((a).r,(b).r) , S_MUL((a).i,(b).i)); \
          (m).i = ADD32(S_MUL((a).r,(b).i) , S_MUL((a).i,(b).r)); }while(0)

#   define C_MULC(m,a,b) \
      do{ (m).r = ADD32(S_MUL((a).r,(b).r) , S_MUL((a).i,(b).i)); \
          (m).i = SUB32(S_MUL((a).i,(b).r) , S_MUL((a).r,(b).i)); }while(0)

#   define C_MUL4(m,a,b) \
      do{ (m).r = SHR(SUB32(S_MUL((a).r,(b).r) , S_MUL((a).i,(b).i)),2); \
          (m).i = SHR(ADD32(S_MUL((a).r,(b).i) , S_MUL((a).i,(b).r)),2); }while(0)

#   define C_MULBYSCALAR( c, s ) \
      do{ (c).r =  S_MUL( (c).r , s ) ;\
          (c).i =  S_MUL( (c).i , s ) ; }while(0)

#   define DIVSCALAR(x,k) \
        (x) = S_MUL(  x, (TWID_MAX-((k)>>1))/(k)+1 )

#   define C_FIXDIV(c,div) \
        do {    DIVSCALAR( (c).r , div);  \
                DIVSCALAR( (c).i  , div); }while (0)

#define  C_ADD( res, a,b)\
    do {(res).r=ADD32((a).r,(b).r);  (res).i=ADD32((a).i,(b).i); \
    }while(0)
#define  C_SUB( res, a,b)\
    do {(res).r=SUB32((a).r,(b).r);  (res).i=SUB32((a).i,(b).i); \
    }while(0)
#define C_ADDTO( res , a)\
    do {(res).r = ADD32((res).r, (a).r);  (res).i = ADD32((res).i,(a).i);\
    }while(0)

#define C_SUBFROM( res , a)\
    do {(res).r = ADD32((res).r,(a).r);  (res).i = SUB32((res).i,(a).i); \
    }while(0)

#else /* MIXED_PRECISION */
#   define sround4( x )  (kiss_fft_scalar)( ( (x) + ((SAMPPROD)1<<(FRACBITS-1)) ) >> (FRACBITS+2) )

#   define S_MUL(a,b) sround( smul(a,b) )

#   define C_MUL(m,a,b) \
      do{ (m).r = sround( smul((a).r,(b).r) - smul((a).i,(b).i) ); \
          (m).i = sround( smul((a).r,(b).i) + smul((a).i,(b).r) ); }while(0)
#   define C_MULC(m,a,b) \
      do{ (m).r = sround( smul((a).r,(b).r) + smul((a).i,(b).i) ); \
          (m).i = sround( smul((a).i,(b).r) - smul((a).r,(b).i) ); }while(0)

#   define C_MUL4(m,a,b) \
               do{ (m).r = sround4( smul((a).r,(b).r) - smul((a).i,(b).i) ); \
               (m).i = sround4( smul((a).r,(b).i) + smul((a).i,(b).r) ); }while(0)

#   define C_MULBYSCALAR( c, s ) \
               do{ (c).r =  sround( smul( (c).r , s ) ) ;\
               (c).i =  sround( smul( (c).i , s ) ) ; }while(0)

#   define DIVSCALAR(x,k) \
	(x) = sround( smul(  x, SAMP_MAX/k ) )

#   define C_FIXDIV(c,div) \
	do {    DIVSCALAR( (c).r , div);  \
		DIVSCALAR( (c).i  , div); }while (0)

#endif /* !MIXED_PRECISION */



#else  /* not FIXED_POINT*/

#define EXT32(a) (a)

#   define S_MUL(a,b) ( (a)*(b) )
#define C_MUL(m,a,b) \
    do{ (m).r = (a).r*(b).r - (a).i*(b).i;\
        (m).i = (a).r*(b).i + (a).i*(b).r; }while(0)
#define C_MULC(m,a,b) \
    do{ (m).r = (a).r*(b).r + (a).i*(b).i;\
        (m).i = (a).i*(b).r - (a).r*(b).i; }while(0)

#define C_MUL4(m,a,b) C_MUL(m,a,b)

#   define C_FIXDIV(c,div) /* NOOP */
#   define C_MULBYSCALAR( c, s ) \
    do{ (c).r *= (s);\
        (c).i *= (s); }while(0)
#endif



#ifndef CHECK_OVERFLOW_OP
#  define CHECK_OVERFLOW_OP(a,op,b) /* noop */
#endif

#ifndef C_ADD
#define  C_ADD( res, a,b)\
    do { \
	    CHECK_OVERFLOW_OP((a).r,+,(b).r)\
	    CHECK_OVERFLOW_OP((a).i,+,(b).i)\
	    (res).r=(a).r+(b).r;  (res).i=(a).i+(b).i; \
    }while(0)
#define  C_SUB( res, a,b)\
    do { \
	    CHECK_OVERFLOW_OP((a).r,-,(b).r)\
	    CHECK_OVERFLOW_OP((a).i,-,(b).i)\
	    (res).r=(a).r-(b).r;  (res).i=(a).i-(b).i; \
    }while(0)
#define C_ADDTO( res , a)\
    do { \
	    CHECK_OVERFLOW_OP((res).r,+,(a).r)\
	    CHECK_OVERFLOW_OP((res).i,+,(a).i)\
	    (res).r += (a).r;  (res).i += (a).i;\
    }while(0)

#define C_SUBFROM( res , a)\
    do {\
	    CHECK_OVERFLOW_OP((res).r,-,(a).r)\
	    CHECK_OVERFLOW_OP((res).i,-,(a).i)\
	    (res).r -= (a).r;  (res).i -= (a).i; \
    }while(0)
#endif /* C_ADD defined */

#ifdef FIXED_POINT
/*#  define KISS_FFT_COS(phase)  TRIG_UPSCALE*floor(MIN(32767,MAX(-32767,.5+32768 * cos (phase))))
#  define KISS_FFT_SIN(phase)  TRIG_UPSCALE*floor(MIN(32767,MAX(-32767,.5+32768 * sin (phase))))*/
#  define KISS_FFT_COS(phase)  floor(.5+TWID_MAX*cos (phase))
#  define KISS_FFT_SIN(phase)  floor(.5+TWID_MAX*sin (phase))
#  define HALF_OF(x) ((x)>>1)
#elif defined(USE_SIMD)
#  define KISS_FFT_COS(phase) _mm_set1_ps( cos(phase) )
#  define KISS_FFT_SIN(phase) _mm_set1_ps( sin(phase) )
#  define HALF_OF(x) ((x)*_mm_set1_ps(.5))
#else
#  define KISS_FFT_COS(phase) (kiss_fft_scalar) cos(phase)
#  define KISS_FFT_SIN(phase) (kiss_fft_scalar) sin(phase)
#  define HALF_OF(x) ((x)*.5)
#endif

#define  kf_cexp(x,phase) \
	do{ \
		(x)->r = KISS_FFT_COS(phase);\
		(x)->i = KISS_FFT_SIN(phase);\
	}while(0)
   
#define  kf_cexp2(x,phase) \
   do{ \
      (x)->r = TRIG_UPSCALE*celt_cos_norm((phase));\
      (x)->i = TRIG_UPSCALE*celt_cos_norm((phase)-32768);\
}while(0)


#endif /* KISS_FFT_GUTS_H */
