/* (C) 2007-2008 Timothy B. Terriberry
   (C) 2008 Jean-Marc Valin */
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "os_support.h"
#include <stdlib.h>
#include <string.h>
#include "cwrs.h"
#include "mathops.h"
#include "arch.h"

/*Guaranteed to return a conservatively large estimate of the binary logarithm
   with frac bits of fractional precision.
  Tested for all possible 32-bit inputs with frac=4, where the maximum
   overestimation is 0.06254243 bits.*/
int log2_frac_0061(ec_uint32 val, int frac)
{
  int l;
  l=EC_ILOG(val);
  if(val&val-1){
    /*This is (val>>l-16), but guaranteed to round up, even if adding a bias
       before the shift would cause overflow (e.g., for 0xFFFFxxxx).*/
    if(l>16)val=(val>>l-16)+((val&(1<<l-16)-1)+(1<<l-16)-1>>l-16);
    else val<<=16-l;
    l=l-1<<frac;
    /*Note that we always need one iteration, since the rounding up above means
       that we might need to adjust the integer part of the logarithm.*/
    do{
      int b;
      b=(int)(val>>16);
      l+=b<<frac;
      val=val+b>>b;
      val=val*val+0x7FFF>>15;
    }
    while(frac-->0);
    /*If val is not exactly 0x8000, then we have to round up the remainder.*/
    return l+(val>0x8000);
  }
  /*Exact powers of two require no rounding.*/
  else return l-1<<frac;
}

#define MASK32 (0xFFFFFFFF)

/*INV_TABLE[i] holds the multiplicative inverse of (2*i+1) mod 2**32.*/
static const celt_uint32_t INV_TABLE[128]={
  0x00000001,0xAAAAAAAB,0xCCCCCCCD,0xB6DB6DB7,
  0x38E38E39,0xBA2E8BA3,0xC4EC4EC5,0xEEEEEEEF,
  0xF0F0F0F1,0x286BCA1B,0x3CF3CF3D,0xE9BD37A7,
  0xC28F5C29,0x684BDA13,0x4F72C235,0xBDEF7BDF,
  0x3E0F83E1,0x8AF8AF8B,0x914C1BAD,0x96F96F97,
  0xC18F9C19,0x2FA0BE83,0xA4FA4FA5,0x677D46CF,
  0x1A1F58D1,0xFAFAFAFB,0x8C13521D,0x586FB587,
  0xB823EE09,0xA08AD8F3,0xC10C9715,0xBEFBEFBF,
  0xC0FC0FC1,0x07A44C6B,0xA33F128D,0xE327A977,
  0xC7E3F1F9,0x962FC963,0x3F2B3885,0x613716AF,
  0x781948B1,0x2B2E43DB,0xFCFCFCFD,0x6FD0EB67,
  0xFA3F47E9,0xD2FD2FD3,0x3F4FD3F5,0xD4E25B9F,
  0x5F02A3A1,0xBF5A814B,0x7C32B16D,0xD3431B57,
  0xD8FD8FD9,0x8D28AC43,0xDA6C0965,0xDB195E8F,
  0x0FDBC091,0x61F2A4BB,0xDCFDCFDD,0x46FDD947,
  0x56BE69C9,0xEB2FDEB3,0x26E978D5,0xEFDFBF7F,
  0x0FE03F81,0xC9484E2B,0xE133F84D,0xE1A8C537,
  0x077975B9,0x70586723,0xCD29C245,0xFAA11E6F,
  0x0FE3C071,0x08B51D9B,0x8CE2CABD,0xBF937F27,
  0xA8FE53A9,0x592FE593,0x2C0685B5,0x2EB11B5F,
  0xFCD1E361,0x451AB30B,0x72CFE72D,0xDB35A717,
  0xFB74A399,0xE80BFA03,0x0D516325,0x1BCB564F,
  0xE02E4851,0xD962AE7B,0x10F8ED9D,0x95AEDD07,
  0xE9DC0589,0xA18A4473,0xEA53FA95,0xEE936F3F,
  0x90948F41,0xEAFEAFEB,0x3D137E0D,0xEF46C0F7,
  0x028C1979,0x791064E3,0xC04FEC05,0xE115062F,
  0x32385831,0x6E68575B,0xA10D387D,0x6FECF2E7,
  0x3FB47F69,0xED4BFB53,0x74FED775,0xDB43BB1F,
  0x87654321,0x9BA144CB,0x478BBCED,0xBFB912D7,
  0x1FDCD759,0x14B2A7C3,0xCB125CE5,0x437B2E0F,
  0x10FEF011,0xD2B3183B,0x386CAB5D,0xEF6AC0C7,
  0x0E64C149,0x9A020A33,0xE6B41C55,0xFEFEFEFF
};

/*Computes (_a*_b-_c)/(2*_d+1) when the quotient is known to be exact.
  _a, _b, _c, and _d may be arbitrary so long as the arbitrary precision result
   fits in 32 bits, but currently the table for multiplicative inverses is only
   valid for _d<128.*/
static inline celt_uint32_t imusdiv32odd(celt_uint32_t _a,celt_uint32_t _b,
 celt_uint32_t _c,int _d){
  return (_a*_b-_c)*INV_TABLE[_d]&MASK32;
}

/*Computes (_a*_b-_c)/_d when the quotient is known to be exact.
  _d does not actually have to be even, but imusdiv32odd will be faster when
   it's odd, so you should use that instead.
  _a and _d are assumed to be small (e.g., _a*_d fits in 32 bits; currently the
   table for multiplicative inverses is only valid for _d<=256).
  _b and _c may be arbitrary so long as the arbitrary precision reuslt fits in
   32 bits.*/
static inline celt_uint32_t imusdiv32even(celt_uint32_t _a,celt_uint32_t _b,
 celt_uint32_t _c,int _d){
  celt_uint32_t inv;
  int           mask;
  int           shift;
  int           one;
  celt_assert(_d>0);
  shift=EC_ILOG(_d^_d-1);
  celt_assert(_d<=256);
  inv=INV_TABLE[_d-1>>shift];
  shift--;
  one=1<<shift;
  mask=one-1;
  return (_a*(_b>>shift)-(_c>>shift)+
   (_a*(_b&mask)+one-(_c&mask)>>shift)-1)*inv&MASK32;
}

/*Compute floor(sqrt(_val)) with exact arithmetic.
  This has been tested on all possible 32-bit inputs.*/
static unsigned isqrt32(celt_uint32_t _val){
  unsigned b;
  unsigned g;
  int      bshift;
  /*Uses the second method from
     http://www.azillionmonkeys.com/qed/sqroot.html
    The main idea is to search for the largest binary digit b such that
     (g+b)*(g+b) <= _val, and add it to the solution g.*/
  g=0;
  bshift=EC_ILOG(_val)-1>>1;
  b=1U<<bshift;
  do{
    celt_uint32_t t;
    t=((celt_uint32_t)g<<1)+b<<bshift;
    if(t<=_val){
      g+=b;
      _val-=t;
    }
    b>>=1;
    bshift--;
  }
  while(bshift>=0);
  return g;
}

#if 0
/*Compute floor(sqrt(_val)) with exact arithmetic.
  This has been tested on all possible 36-bit inputs.*/
static celt_uint32_t isqrt36(celt_uint64_t _val){
  celt_uint32_t val32;
  celt_uint32_t b;
  celt_uint32_t g;
  int           bshift;
  g=0;
  b=0x20000;
  for(bshift=18;bshift-->13;){
    celt_uint64_t t;
    t=((celt_uint64_t)g<<1)+b<<bshift;
    if(t<=_val){
      g+=b;
      _val-=t;
    }
    b>>=1;
  }
  val32=(celt_uint32_t)_val;
  for(;bshift>=0;bshift--){
    celt_uint32_t t;
    t=(g<<1)+b<<bshift;
    if(t<=val32){
      g+=b;
      val32-=t;
    }
    b>>=1;
  }
  return g;
}
#endif

/*Although derived separately, the pulse vector coding scheme is equivalent to
   a Pyramid Vector Quantizer \cite{Fis86}.
  Some additional notes about an early version appear at
   http://people.xiph.org/~tterribe/notes/cwrs.html, but the codebook ordering
   and the definitions of some terms have evolved since that was written.

  The conversion from a pulse vector to an integer index (encoding) and back
   (decoding) is governed by two related functions, V(N,K) and U(N,K).

  V(N,K) = the number of combinations, with replacement, of N items, taken K
   at a time, when a sign bit is added to each item taken at least once (i.e.,
   the number of N-dimensional unit pulse vectors with K pulses).
  One way to compute this is via
    V(N,K) = K>0 ? sum(k=1...K,2**k*choose(N,k)*choose(K-1,k-1)) : 1,
   where choose() is the binomial function.
  A table of values for N<10 and K<10 looks like:
  V[10][10] = {
    {1,  0,   0,    0,    0,     0,     0,      0,      0,       0},
    {1,  2,   2,    2,    2,     2,     2,      2,      2,       2},
    {1,  4,   8,   12,   16,    20,    24,     28,     32,      36},
    {1,  6,  18,   38,   66,   102,   146,    198,    258,     326},
    {1,  8,  32,   88,  192,   360,   608,    952,   1408,    1992},
    {1, 10,  50,  170,  450,  1002,  1970,   3530,   5890,    9290},
    {1, 12,  72,  292,  912,  2364,  5336,  10836,  20256,   35436},
    {1, 14,  98,  462, 1666,  4942, 12642,  28814,  59906,  115598},
    {1, 16, 128,  688, 2816,  9424, 27008,  68464, 157184,  332688},
    {1, 18, 162,  978, 4482, 16722, 53154, 148626, 374274,  864146}
  };

  U(N,K) = the number of such combinations wherein N-1 objects are taken at
   most K-1 at a time.
  This is given by
    U(N,K) = sum(k=0...K-1,V(N-1,k))
           = K>0 ? (V(N-1,K-1) + V(N,K-1))/2 : 0.
  The latter expression also makes clear that U(N,K) is half the number of such
   combinations wherein the first object is taken at least once.
  Although it may not be clear from either of these definitions, U(N,K) is the
   natural function to work with when enumerating the pulse vector codebooks,
   not V(N,K).
  U(N,K) is not well-defined for N=0, but with the extension
    U(0,K) = K>0 ? 0 : 1,
   the function becomes symmetric: U(N,K) = U(K,N), with a similar table:
  U[10][10] = {
    {1, 0,  0,   0,    0,    0,     0,     0,      0,      0},
    {0, 1,  1,   1,    1,    1,     1,     1,      1,      1},
    {0, 1,  3,   5,    7,    9,    11,    13,     15,     17},
    {0, 1,  5,  13,   25,   41,    61,    85,    113,    145},
    {0, 1,  7,  25,   63,  129,   231,   377,    575,    833},
    {0, 1,  9,  41,  129,  321,   681,  1289,   2241,   3649},
    {0, 1, 11,  61,  231,  681,  1683,  3653,   7183,  13073},
    {0, 1, 13,  85,  377, 1289,  3653,  8989,  19825,  40081},
    {0, 1, 15, 113,  575, 2241,  7183, 19825,  48639, 108545},
    {0, 1, 17, 145,  833, 3649, 13073, 40081, 108545, 265729}
  };

  With this extension, V(N,K) may be written in terms of U(N,K):
    V(N,K) = U(N,K) + U(N,K+1)
   for all N>=0, K>=0.
  Thus U(N,K+1) represents the number of combinations where the first element
   is positive or zero, and U(N,K) represents the number of combinations where
   it is negative.
  With a large enough table of U(N,K) values, we could write O(N) encoding
   and O(min(N*log(K),N+K)) decoding routines, but such a table would be
   prohibitively large for small embedded devices (K may be as large as 32767
   for small N, and N may be as large as 200).

  Both functions obey the same recurrence relation:
    V(N,K) = V(N-1,K) + V(N,K-1) + V(N-1,K-1),
    U(N,K) = U(N-1,K) + U(N,K-1) + U(N-1,K-1),
   for all N>0, K>0, with different initial conditions at N=0 or K=0.
  This allows us to construct a row of one of the tables above given the
   previous row or the next row.
  Thus we can derive O(NK) encoding and decoding routines with O(K) memory
   using only addition and subtraction.

  When encoding, we build up from the U(2,K) row and work our way forwards.
  When decoding, we need to start at the U(N,K) row and work our way backwards,
   which requires a means of computing U(N,K).
  U(N,K) may be computed from two previous values with the same N:
    U(N,K) = ((2*N-1)*U(N,K-1) - U(N,K-2))/(K-1) + U(N,K-2)
   for all N>1, and since U(N,K) is symmetric, a similar relation holds for two
   previous values with the same K:
    U(N,K>1) = ((2*K-1)*U(N-1,K) - U(N-2,K))/(N-1) + U(N-2,K)
   for all K>1.
  This allows us to construct an arbitrary row of the U(N,K) table by starting
   with the first two values, which are constants.
  This saves roughly 2/3 the work in our O(NK) decoding routine, but costs O(K)
   multiplications.
  Similar relations can be derived for V(N,K), but are not used here.

  For N>0 and K>0, U(N,K) and V(N,K) take on the form of an (N-1)-degree
   polynomial for fixed N.
  The first few are
    U(1,K) = 1,
    U(2,K) = 2*K-1,
    U(3,K) = (2*K-2)*K+1,
    U(4,K) = (((4*K-6)*K+8)*K-3)/3,
    U(5,K) = ((((2*K-4)*K+10)*K-8)*K+3)/3,
   and
    V(1,K) = 2,
    V(2,K) = 4*K,
    V(3,K) = 4*K*K+2,
    V(4,K) = 8*(K*K+2)*K/3,
    V(5,K) = ((4*K*K+20)*K*K+6)/3,
   for all K>0.
  This allows us to derive O(N) encoding and O(N*log(K)) decoding routines for
   small N (and indeed decoding is also O(N) for N<3).

  @ARTICLE{Fis86,
    author="Thomas R. Fischer",
    title="A Pyramid Vector Quantizer",
    journal="IEEE Transactions on Information Theory",
    volume="IT-32",
    number=4,
    pages="568--583",
    month=Jul,
    year=1986
  }*/

/*Determines if V(N,K) fits in a 32-bit unsigned integer.
  N and K are themselves limited to 15 bits.*/
static int fits_in32(int _n, int _k)
{
   static const celt_int16_t maxN[15] = {
      32767, 32767, 32767, 1476, 283, 109,  60,  40,
       29,  24,  20,  18,  16,  14,  13};
   static const celt_int16_t maxK[15] = {
      32767, 32767, 32767, 32767, 1172, 238,  95,  53,
       36,  27,  22,  18,  16,  15,  13};
   if (_n>=14)
   {
      if (_k>=14)
         return 0;
      else
         return _n <= maxN[_k];
   } else {
      return _k <= maxK[_n];
   }
}

/*Compute U(1,_k).*/
static inline unsigned ucwrs1(int _k){
  return _k?1:0;
}

/*Compute V(1,_k).*/
static inline unsigned ncwrs1(int _k){
  return _k?2:1;
}

/*Compute U(2,_k).
  Note that this may be called with _k=32768 (maxK[2]+1).*/
static inline unsigned ucwrs2(unsigned _k){
  return _k?_k+(_k-1):0;
}

/*Compute V(2,_k).*/
static inline celt_uint32_t ncwrs2(int _k){
  return _k?4*(celt_uint32_t)_k:1;
}

/*Compute U(3,_k).
  Note that this may be called with _k=32768 (maxK[3]+1).*/
static inline celt_uint32_t ucwrs3(unsigned _k){
  return _k?(2*(celt_uint32_t)_k-2)*_k+1:0;
}

/*Compute V(3,_k).*/
static inline celt_uint32_t ncwrs3(int _k){
  return _k?2*(2*(unsigned)_k*(celt_uint32_t)_k+1):1;
}

/*Compute U(4,_k).*/
static inline celt_uint32_t ucwrs4(int _k){
  return _k?imusdiv32odd(2*_k,(2*_k-3)*(celt_uint32_t)_k+4,3,1):0;
}

/*Compute V(4,_k).*/
static inline celt_uint32_t ncwrs4(int _k){
  return _k?((_k*(celt_uint32_t)_k+2)*_k)/3<<3:1;
}

/*Compute U(5,_k).*/
static inline celt_uint32_t ucwrs5(int _k){
  return _k?(((((_k-2)*(unsigned)_k+5)*(celt_uint32_t)_k-4)*_k)/3<<1)+1:0;
}

/*Compute V(5,_k).*/
static inline celt_uint32_t ncwrs5(int _k){
  return _k?(((_k*(unsigned)_k+5)*(celt_uint32_t)_k*_k)/3<<2)+2:1;
}

/*Computes the next row/column of any recurrence that obeys the relation
   u[i][j]=u[i-1][j]+u[i][j-1]+u[i-1][j-1].
  _ui0 is the base case for the new row/column.*/
static inline void unext(celt_uint32_t *_ui,unsigned _len,celt_uint32_t _ui0){
  celt_uint32_t ui1;
  unsigned      j;
  /*This do-while will overrun the array if we don't have storage for at least
     2 values.*/
  j=1; do {
    ui1=UADD32(UADD32(_ui[j],_ui[j-1]),_ui0);
    _ui[j-1]=_ui0;
    _ui0=ui1;
  } while (++j<_len);
  _ui[j-1]=_ui0;
}

/*Computes the previous row/column of any recurrence that obeys the relation
   u[i-1][j]=u[i][j]-u[i][j-1]-u[i-1][j-1].
  _ui0 is the base case for the new row/column.*/
static inline void uprev(celt_uint32_t *_ui,unsigned _n,celt_uint32_t _ui0){
  celt_uint32_t ui1;
  unsigned      j;
  /*This do-while will overrun the array if we don't have storage for at least
     2 values.*/
  j=1; do {
    ui1=USUB32(USUB32(_ui[j],_ui[j-1]),_ui0);
    _ui[j-1]=_ui0;
    _ui0=ui1;
  } while (++j<_n);
  _ui[j-1]=_ui0;
}

/*Compute V(_n,_k), as well as U(_n,0..._k+1).
  _u: On exit, _u[i] contains U(_n,i) for i in [0..._k+1].*/
static celt_uint32_t ncwrs_urow(unsigned _n,unsigned _k,celt_uint32_t *_u){
  celt_uint32_t um2;
  unsigned      len;
  unsigned      k;
  len=_k+2;
  /*We require storage at least 3 values (e.g., _k>0).*/
  celt_assert(len>=3);
  _u[0]=0;
  _u[1]=um2=1;
  if(_n<=6 || _k>255){
    /*If _n==0, _u[0] should be 1 and the rest should be 0.*/
    /*If _n==1, _u[i] should be 1 for i>1.*/
    celt_assert(_n>=2);
    /*If _k==0, the following do-while loop will overflow the buffer.*/
    celt_assert(_k>0);
    k=2;
    do _u[k]=(k<<1)-1;
    while(++k<len);
    for(k=2;k<_n;k++)unext(_u+1,_k+1,1);
  }
  else{
    celt_uint32_t um1;
    celt_uint32_t n2m1;
    _u[2]=n2m1=um1=(_n<<1)-1;
    for(k=3;k<len;k++){
      /*U(N,K) = ((2*N-1)*U(N,K-1)-U(N,K-2))/(K-1) + U(N,K-2)*/
      _u[k]=um2=imusdiv32even(n2m1,um1,um2,k-1)+um2;
      if(++k>=len)break;
      _u[k]=um1=imusdiv32odd(n2m1,um2,um1,k-1>>1)+um1;
    }
  }
  return _u[_k]+_u[_k+1];
}


/*Returns the _i'th combination of _k elements (at most 32767) chosen from a
   set of size 1 with associated sign bits.
  _y: Returns the vector of pulses.*/
static inline void cwrsi1(int _k,celt_uint32_t _i,int *_y){
  int s;
  s=-(int)_i;
  _y[0]=_k+s^s;
}

/*Returns the _i'th combination of _k elements (at most 32767) chosen from a
   set of size 2 with associated sign bits.
  _y: Returns the vector of pulses.*/
static inline void cwrsi2(int _k,celt_uint32_t _i,int *_y){
  celt_uint32_t p;
  int           s;
  int           yj;
  p=ucwrs2(_k+1U);
  s=-(_i>=p);
  _i-=p&s;
  yj=_k;
  _k=_i+1>>1;
  p=ucwrs2(_k);
  _i-=p;
  yj-=_k;
  _y[0]=yj+s^s;
  cwrsi1(_k,_i,_y+1);
}

/*Returns the _i'th combination of _k elements (at most 32767) chosen from a
   set of size 3 with associated sign bits.
  _y: Returns the vector of pulses.*/
static void cwrsi3(int _k,celt_uint32_t _i,int *_y){
  celt_uint32_t p;
  int           s;
  int           yj;
  p=ucwrs3(_k+1U);
  s=-(_i>=p);
  _i-=p&s;
  yj=_k;
  /*Finds the maximum _k such that ucwrs3(_k)<=_i (tested for all
     _i<2147418113=U(3,32768)).*/
  _k=_i>0?isqrt32(2*_i-1)+1>>1:0;
  p=ucwrs3(_k);
  _i-=p;
  yj-=_k;
  _y[0]=yj+s^s;
  cwrsi2(_k,_i,_y+1);
}

/*Returns the _i'th combination of _k elements (at most 1172) chosen from a set
   of size 4 with associated sign bits.
  _y: Returns the vector of pulses.*/
static void cwrsi4(int _k,celt_uint32_t _i,int *_y){
  celt_uint32_t p;
  int           s;
  int           yj;
  int           kl;
  int           kr;
  p=ucwrs4(_k+1);
  s=-(_i>=p);
  _i-=p&s;
  yj=_k;
  /*We could solve a cubic for k here, but the form of the direct solution does
     not lend itself well to exact integer arithmetic.
    Instead we do a binary search on U(4,K).*/
  kl=0;
  kr=_k;
  for(;;){
    _k=kl+kr>>1;
    p=ucwrs4(_k);
    if(p<_i){
      if(_k>=kr)break;
      kl=_k+1;
    }
    else if(p>_i)kr=_k-1;
    else break;
  }
  _i-=p;
  yj-=_k;
  _y[0]=yj+s^s;
  cwrsi3(_k,_i,_y+1);
}

/*Returns the _i'th combination of _k elements (at most 238) chosen from a set
   of size 5 with associated sign bits.
  _y: Returns the vector of pulses.*/
static void cwrsi5(int _k,celt_uint32_t _i,int *_y){
  celt_uint32_t p;
  int           s;
  int           yj;
  p=ucwrs5(_k+1);
  s=-(_i>=p);
  _i-=p&s;
  yj=_k;
#if 0
  /*Finds the maximum _k such that ucwrs5(_k)<=_i (tested for all
     _i<2157192969=U(5,239)).*/
  if(_i>=0x2AAAAAA9UL)_k=isqrt32(2*isqrt36(10+6*(celt_uint64_t)_i)-7)+1>>1;
  else _k=_i>0?isqrt32(2*(celt_uint32_t)isqrt32(10+6*_i)-7)+1>>1:0;
  p=ucwrs5(_k);
#else 
  /* A binary search on U(5,K) avoids the need for 64-bit arithmetic */
  {
    int kl=0;
    int kr=_k;
    for(;;){
      _k=kl+kr>>1;
      p=ucwrs5(_k);
      if(p<_i){
        if(_k>=kr)break;
        kl=_k+1;
      }
      else if(p>_i)kr=_k-1;
      else break;
    }  
  }
#endif
  _i-=p;
  yj-=_k;
  _y[0]=yj+s^s;
  cwrsi4(_k,_i,_y+1);
}

/*Returns the _i'th combination of _k elements chosen from a set of size _n
   with associated sign bits.
  _y: Returns the vector of pulses.
  _u: Must contain entries [0..._k+1] of row _n of U() on input.
      Its contents will be destructively modified.*/
static void cwrsi(int _n,int _k,celt_uint32_t _i,int *_y,celt_uint32_t *_u){
  int j;
  celt_assert(_n>0);
  j=0;
  do{
    celt_uint32_t p;
    int           s;
    int           yj;
    p=_u[_k+1];
    s=-(_i>=p);
    _i-=p&s;
    yj=_k;
    p=_u[_k];
    while(p>_i)p=_u[--_k];
    _i-=p;
    yj-=_k;
    _y[j]=yj+s^s;
    uprev(_u,_k+2,0);
  }
  while(++j<_n);
}


/*Returns the index of the given combination of K elements chosen from a set
   of size 1 with associated sign bits.
  _y: The vector of pulses, whose sum of absolute values is K.
  _k: Returns K.*/
static inline celt_uint32_t icwrs1(const int *_y,int *_k){
  *_k=abs(_y[0]);
  return _y[0]<0;
}

/*Returns the index of the given combination of K elements chosen from a set
   of size 2 with associated sign bits.
  _y: The vector of pulses, whose sum of absolute values is K.
  _k: Returns K.*/
static inline celt_uint32_t icwrs2(const int *_y,int *_k){
  celt_uint32_t i;
  int           k;
  i=icwrs1(_y+1,&k);
  i+=ucwrs2(k);
  k+=abs(_y[0]);
  if(_y[0]<0)i+=ucwrs2(k+1U);
  *_k=k;
  return i;
}

/*Returns the index of the given combination of K elements chosen from a set
   of size 3 with associated sign bits.
  _y: The vector of pulses, whose sum of absolute values is K.
  _k: Returns K.*/
static inline celt_uint32_t icwrs3(const int *_y,int *_k){
  celt_uint32_t i;
  int           k;
  i=icwrs2(_y+1,&k);
  i+=ucwrs3(k);
  k+=abs(_y[0]);
  if(_y[0]<0)i+=ucwrs3(k+1U);
  *_k=k;
  return i;
}

/*Returns the index of the given combination of K elements chosen from a set
   of size 4 with associated sign bits.
  _y: The vector of pulses, whose sum of absolute values is K.
  _k: Returns K.*/
static inline celt_uint32_t icwrs4(const int *_y,int *_k){
  celt_uint32_t i;
  int           k;
  i=icwrs3(_y+1,&k);
  i+=ucwrs4(k);
  k+=abs(_y[0]);
  if(_y[0]<0)i+=ucwrs4(k+1);
  *_k=k;
  return i;
}

/*Returns the index of the given combination of K elements chosen from a set
   of size 5 with associated sign bits.
  _y: The vector of pulses, whose sum of absolute values is K.
  _k: Returns K.*/
static inline celt_uint32_t icwrs5(const int *_y,int *_k){
  celt_uint32_t i;
  int           k;
  i=icwrs4(_y+1,&k);
  i+=ucwrs5(k);
  k+=abs(_y[0]);
  if(_y[0]<0)i+=ucwrs5(k+1);
  *_k=k;
  return i;
}

/*Returns the index of the given combination of K elements chosen from a set
   of size _n with associated sign bits.
  _y:  The vector of pulses, whose sum of absolute values must be _k.
  _nc: Returns V(_n,_k).*/
static celt_uint32_t icwrs(int _n,int _k,celt_uint32_t *_nc,const int *_y,
 celt_uint32_t *_u){
  celt_uint32_t i;
  int           j;
  int           k;
  /*We can't unroll the first two iterations of the loop unless _n>=2.*/
  celt_assert(_n>=2);
  _u[0]=0;
  for(k=1;k<=_k+1;k++)_u[k]=(k<<1)-1;
  i=icwrs1(_y+_n-1,&k);
  j=_n-2;
  i+=_u[k];
  k+=abs(_y[j]);
  if(_y[j]<0)i+=_u[k+1];
  while(j-->0){
    unext(_u,_k+2,0);
    i+=_u[k];
    k+=abs(_y[j]);
    if(_y[j]<0)i+=_u[k+1];
  }
  *_nc=_u[k]+_u[k+1];
  return i;
}


/*Computes get_required_bits when splitting is required.
  _left_bits and _right_bits must contain the required bits for the left and
   right sides of the split, respectively (which themselves may require
   splitting).*/
static void get_required_split_bits(celt_int16_t *_bits,
 const celt_int16_t *_left_bits,const celt_int16_t *_right_bits,
 int _n,int _maxk,int _frac){
  int k;
  for(k=_maxk;k-->0;){
    /*If we've reached a k where everything fits in 32 bits, evaluate the
       remaining required bits directly.*/
    if(fits_in32(_n,k)){
      get_required_bits_0061(_bits,_n,k+1,_frac);
      break;
    }
    else{
      int worst_bits;
      int i;
      /*Due to potentially recursive splitting, it's difficult to derive an
         analytic expression for the location of the worst-case split index.
        We simply check them all.*/
      worst_bits=0;
      for(i=0;i<=k;i++){
        int split_bits;
        split_bits=_left_bits[i]+_right_bits[k-i];
        if(split_bits>worst_bits)worst_bits=split_bits;
      }
      _bits[k]=log2_frac_0061(k+1,_frac)+worst_bits;
    }
  }
}

/*Computes get_required_bits for a pair of N values.
  _n1 and _n2 must either be equal or two consecutive integers.
  Returns the buffer used to store the required bits for _n2, which is either
   _bits1 if _n1==_n2 or _bits2 if _n1+1==_n2.*/
static celt_int16_t *get_required_bits_pair(celt_int16_t *_bits1,
 celt_int16_t *_bits2,celt_int16_t *_tmp,int _n1,int _n2,int _maxk,int _frac){
  celt_int16_t *tmp2;
  /*If we only need a single set of required bits...*/
  if(_n1==_n2){
    /*Stop recursing if everything fits.*/
    if(fits_in32(_n1,_maxk-1))get_required_bits_0061(_bits1,_n1,_maxk,_frac);
    else{
      _tmp=get_required_bits_pair(_bits2,_tmp,_bits1,
       _n1>>1,_n1+1>>1,_maxk,_frac);
      get_required_split_bits(_bits1,_bits2,_tmp,_n1,_maxk,_frac);
    }
    return _bits1;
  }
  /*Otherwise we need two distinct sets...*/
  celt_assert(_n1+1==_n2);
  /*Stop recursing if everything fits.*/
  if(fits_in32(_n2,_maxk-1)){
    get_required_bits_0061(_bits1,_n1,_maxk,_frac);
    get_required_bits_0061(_bits2,_n2,_maxk,_frac);
  }
  /*Otherwise choose an evaluation order that doesn't require extra buffers.*/
  else if(_n1&1){
    /*This special case isn't really needed, but can save some work.*/
    if(fits_in32(_n1,_maxk-1)){
      tmp2=get_required_bits_pair(_tmp,_bits1,_bits2,
       _n2>>1,_n2>>1,_maxk,_frac);
      get_required_split_bits(_bits2,_tmp,tmp2,_n2,_maxk,_frac);
      get_required_bits_0061(_bits1,_n1,_maxk,_frac);
    }
    else{
      _tmp=get_required_bits_pair(_bits2,_tmp,_bits1,
       _n1>>1,_n1+1>>1,_maxk,_frac);
      get_required_split_bits(_bits1,_bits2,_tmp,_n1,_maxk,_frac);
      get_required_split_bits(_bits2,_tmp,_tmp,_n2,_maxk,_frac);
    }
  }
  else{
    /*There's no need to special case _n1 fitting by itself, since _n2 requires
       us to recurse for both values anyway.*/
    tmp2=get_required_bits_pair(_tmp,_bits1,_bits2,
     _n2>>1,_n2+1>>1,_maxk,_frac);
    get_required_split_bits(_bits2,_tmp,tmp2,_n2,_maxk,_frac);
    get_required_split_bits(_bits1,_tmp,_tmp,_n1,_maxk,_frac);
  }
  return _bits2;
}

void get_required_bits_0061(celt_int16_t *_bits,int _n,int _maxk,int _frac){
  int k;
  /*_maxk==0 => there's nothing to do.*/
  celt_assert(_maxk>0);
  if(fits_in32(_n,_maxk-1)){
    _bits[0]=0;
    if(_maxk>1){
      VARDECL(celt_uint32_t,u);
      SAVE_STACK;
      ALLOC(u,_maxk+1U,celt_uint32_t);
      ncwrs_urow(_n,_maxk-1,u);
      for(k=1;k<_maxk;k++)_bits[k]=log2_frac_0061(u[k]+u[k+1],_frac);
      RESTORE_STACK;
    }
  }
  else{
    VARDECL(celt_int16_t,n1bits);
    VARDECL(celt_int16_t,n2bits_buf);
    celt_int16_t *n2bits;
    SAVE_STACK;
    ALLOC(n1bits,_maxk,celt_int16_t);
    ALLOC(n2bits_buf,_maxk,celt_int16_t);
    n2bits=get_required_bits_pair(n1bits,n2bits_buf,_bits,
     _n>>1,_n+1>>1,_maxk,_frac);
    get_required_split_bits(_bits,n1bits,n2bits,_n,_maxk,_frac);
    RESTORE_STACK;
  }
}


static inline void encode_pulses32(int _n,int _k,const int *_y,ec_enc *_enc){
  celt_uint32_t i;
  switch(_n){
    case 1:{
      i=icwrs1(_y,&_k);
      celt_assert(ncwrs1(_k)==2);
      ec_enc_bits_0061(_enc,i,1);
    }break;
    case 2:{
      i=icwrs2(_y,&_k);
      ec_enc_uint_0061(_enc,i,ncwrs2(_k));
    }break;
    case 3:{
      i=icwrs3(_y,&_k);
      ec_enc_uint_0061(_enc,i,ncwrs3(_k));
    }break;
    case 4:{
      i=icwrs4(_y,&_k);
      ec_enc_uint_0061(_enc,i,ncwrs4(_k));
    }break;
    case 5:{
      i=icwrs5(_y,&_k);
      ec_enc_uint_0061(_enc,i,ncwrs5(_k));
    }break;
    default:{
      VARDECL(celt_uint32_t,u);
      celt_uint32_t nc;
      SAVE_STACK;
      ALLOC(u,_k+2U,celt_uint32_t);
      i=icwrs(_n,_k,&nc,_y,u);
      ec_enc_uint_0061(_enc,i,nc);
      RESTORE_STACK;
    }break;
  }
}

void encode_pulses_0061(int *_y, int N, int K, ec_enc *enc)
{
   if (K==0) {
   } else if(fits_in32(N,K))
   {
      encode_pulses32(N, K, _y, enc);
   } else {
     int i;
     int count=0;
     int split;
     split = (N+1)/2;
     for (i=0;i<split;i++)
        count += abs(_y[i]);
     ec_enc_uint_0061(enc,count,K+1);
     encode_pulses_0061(_y, split, count, enc);
     encode_pulses_0061(_y+split, N-split, K-count, enc);
   }
}

static inline void decode_pulses32(int _n,int _k,int *_y,ec_dec *_dec){
  switch(_n){
    case 1:{
      celt_assert(ncwrs1(_k)==2);
      cwrsi1(_k,ec_dec_bits_0061(_dec,1),_y);
    }break;
    case 2:cwrsi2(_k,ec_dec_uint_0061(_dec,ncwrs2(_k)),_y);break;
    case 3:cwrsi3(_k,ec_dec_uint_0061(_dec,ncwrs3(_k)),_y);break;
    case 4:cwrsi4(_k,ec_dec_uint_0061(_dec,ncwrs4(_k)),_y);break;
    case 5:cwrsi5(_k,ec_dec_uint_0061(_dec,ncwrs5(_k)),_y);break;
    default:{
      VARDECL(celt_uint32_t,u);
      SAVE_STACK;
      ALLOC(u,_k+2U,celt_uint32_t);
      cwrsi(_n,_k,ec_dec_uint_0061(_dec,ncwrs_urow(_n,_k,u)),_y,u);
      RESTORE_STACK;
    }
  }
}

void decode_pulses_0061(int *_y, int N, int K, ec_dec *dec)
{
   if (K==0) {
      int i;
      for (i=0;i<N;i++)
         _y[i] = 0;
   } else if(fits_in32(N,K))
   {
      decode_pulses32(N, K, _y, dec);
   } else {
     int split;
     int count = ec_dec_uint_0061(dec,K+1);
     split = (N+1)/2;
     decode_pulses_0061(_y, split, count, dec);
     decode_pulses_0061(_y+split, N-split, K-count, dec);
   }
}
