// v2math -- project-owned transcendentals for the portable V2 player.
//
// WHY OWNED: libm differs per OS/version, so any libm call in the audio path
// makes renders host-tinted. These implementations are fixed C++ with
// hardcoded constants (provenance: test/gen_constants.py), giving
// bit-identical results on every IEEE-754 host.
//
// WHY THIS SHAPE: the original synths run the x87 with PC=24 (24-bit
// significand rounding on every arithmetic op) but compute transcendentals
// (f2xm1/fyl2x/fsin/fpatan) at ~64-bit internal precision. Each function here
// mirrors one asm kernel's ROUNDING STRUCTURE: high-precision (double) core,
// with explicit float32 roundings exactly where the asm rounds at PC=24.
// Mirrored kernels (synth.asm:355-408 + the year-2000 binary, see DELTA.md):
//   pow2f    <- asm pow2: fprem | f2xm1 | faddp(24-bit) | fscale(exact)
//   powf24   <- asm pow:  fyl2x(64-bit) feeding the pow2 tail
//   oscfreqi <- syOscChgPitch freq tail: fmul fci12 | pow2 | fmul base | fistp
//   atanf24 / odGain2 <- fld1;fpatan (64-bit) [+ fdiv 24-bit]
//   sinf24   <- fsin (v0 sine osc/LFO; host x87 fsin was vendor-dependent --
//               this is the one place "bit-exact to history" never existed)
//
// Accuracy: double cores are accurate to ~2^-55 (vs the x87's ~2^-64), so the
// float32-rounded results match the x87's in all but ties closer than 2^-30
// ULP -- expected a handful of 1-ULP flips per song, identical on every host,
// covered by the published epsilon (design D4).
//
// Allowed <math.h> usage (exact / correctly-rounded BY DEFINITION, therefore
// deterministic everywhere): ldexp(f), frexp, trunc(f), floor(f), fmod(f),
// sqrt(f), fabs(f), lrint(f). No approximating libm calls (exp/log/sin/...).

#ifndef V2MATH_H_
#define V2MATH_H_

#include <math.h>
#include <stdint.h>

namespace v2redux {
namespace vm {

// ---------------------------------------------------------------------------
// generated constants (test/gen_constants.py, decimal 60-digit)
// ---------------------------------------------------------------------------

// 2^(k/32), k=0..31, double-double
static const double kExp2Tab[32][2] = {
  { 1.0, 0.0 },
  { 1.0218971486541166, 5.109225028973444e-17 },
  { 1.0442737824274138, 8.551889705537965e-17 },
  { 1.0671404006768237, -7.899853966841582e-17 },
  { 1.0905077326652577, -3.046782079812471e-17 },
  { 1.1143867425958924, 1.0410278456845571e-16 },
  { 1.1387886347566916, 8.912812676025408e-17 },
  { 1.1637248587775775, 3.8292048369240935e-17 },
  { 1.189207115002721, 3.982015231465646e-17 },
  { 1.215247359980469, -7.712630692681488e-17 },
  { 1.241857812073484, 4.658027591836937e-17 },
  { 1.2690509571917332, 2.667932131342186e-18 },
  { 1.2968395546510096, 2.5382502794888315e-17 },
  { 1.3252366431597413, -2.8587312100388614e-17 },
  { 1.3542555469368927, 7.70094837980299e-17 },
  { 1.383909881963832, -6.770511658794786e-17 },
  { 1.4142135623730951, -9.667293313452913e-17 },
  { 1.4451808069770467, -3.0237581349939873e-17 },
  { 1.4768261459394993, -3.483994556892796e-17 },
  { 1.5091644275934228, -1.016455327754295e-16 },
  { 1.5422108254079407, 7.949834809697621e-17 },
  { 1.5759808451078865, -1.0136916471278304e-17 },
  { 1.6104903319492543, 2.4707192569797888e-17 },
  { 1.645755478153965, -1.0125679913674773e-16 },
  { 1.681792830507429, 8.199010020581497e-17 },
  { 1.718619298122478, -1.851380418263111e-17 },
  { 1.7562521603732995, 2.960140695448873e-17 },
  { 1.7947090750031072, 1.8227458427912087e-17 },
  { 1.8340080864093424, 3.283107224245627e-17 },
  { 1.8741676341103, -6.122763413004143e-17 },
  { 1.9152065613971474, -1.0619946056195963e-16 },
  { 1.9571441241754002, 8.960767791036668e-17 },
};

// 2^t - 1 = sum c[n] t^n  (c[n] = ln2^n / n!), |t| <= 1/64
static const double kExp2m1C[7] = {
  0.6931471805599453,    // ln2^1/1!
  0.24022650695910072,   // ln2^2/2!
  0.05550410866482158,   // ln2^3/3!
  0.009618129107628477,  // ln2^4/4!
  0.0013333558146428443, // ln2^5/5!
  0.0001540353039338161, // ln2^6/6!
  1.5252733804059841e-05,// ln2^7/7!
};

// log2(1 + j/32), j=0..32, double-double
static const double kLog2Tab[33][2] = {
  { 0.0, 0.0 },
  { 0.044394119358453436, 1.3338680039226223e-18 },
  { 0.0874628412503394, 6.765321226991275e-18 },
  { 0.12928301694496647, -1.147571414337692e-17 },
  { 0.16992500144231237, -1.0448980122780218e-17 },
  { 0.20945336562894978, -1.747801539116594e-18 },
  { 0.2479275134435855, 3.8662183541602335e-18 },
  { 0.28540221886224837, -2.726283638197372e-17 },
  { 0.32192809488736235, -3.717019964142682e-19 },
  { 0.3575520046180837, 1.8984820907705057e-17 },
  { 0.3923174227787603, -1.6328502208352762e-17 },
  { 0.42626475470209796, -1.9932012137193316e-17 },
  { 0.45943161863729726, -3.8053583859449705e-19 },
  { 0.4918530963296747, -1.0820682119194486e-17 },
  { 0.5235619560570128, 3.838472289082233e-17 },
  { 0.5545888516776374, -1.2269989151629687e-17 },
  { 0.5849625007211562, -5.224490061390109e-18 },
  { 0.6147098441152082, -2.2208024293925304e-17 },
  { 0.6438561897747247, -7.434039928285364e-19 },
  { 0.6724253419714956, -2.6214744450027748e-17 },
  { 0.7004397181410922, -2.2038346320583612e-17 },
  { 0.7279204545631992, -2.476475356878588e-17 },
  { 0.7548875021634686, -1.5673470184170328e-17 },
  { 0.7813597135246596, -7.522378350087652e-19 },
  { 0.8073549220576041, 4.4407139084295174e-17 },
  { 0.8328900141647416, 5.415287952402795e-17 },
  { 0.8579809951275721, 3.2653869625311436e-17 },
  { 0.8826430493618412, 2.2296523086165164e-17 },
  { 0.9068905956085185, 4.991495917345345e-17 },
  { 0.9307373375628862, 4.094087911381388e-17 },
  { 0.9541963103868752, -3.7239566747188146e-17 },
  { 0.9772799234999164, 3.395815896151496e-17 },
  { 1.0, 0.0 },
};

static const double kTwoOverLn2 = 2.8853900817779268;

// Taylor coeffs for |t| <= pi/4: sin t = t + t^3*(S0 + S1 t^2 + ...),
// cos t = 1 + t^2*(C0 + C1 t^2 + ...)
static const double kSinC[8] = {
  -0.16666666666666666,   //-1/3!
  0.008333333333333333,   // 1/5!
  -0.0001984126984126984, //-1/7!
  2.7557319223985893e-06, // 1/9!
  -2.505210838544172e-08, //-1/11!
  1.6059043836821613e-10, // 1/13!
  -7.647163731819816e-13, //-1/15!
  2.8114572543455206e-15, // 1/17!
};
static const double kCosC[8] = {
  -0.5,                   //-1/2!
  0.041666666666666664,   // 1/4!
  -0.001388888888888889,  //-1/6!
  2.48015873015873e-05,   // 1/8!
  -2.755731922398589e-07, //-1/10!
  2.08767569878681e-09,   // 1/12!
  -1.1470745597729725e-11,//-1/14!
  4.779477332387385e-14,  // 1/16!
};

// atan(j/8), j=0..8, double-double
static const double kAtanTab[9][2] = {
  { 0.0, 0.0 },
  { 0.12435499454676144, -3.1253241424539383e-18 },
  { 0.24497866312686414, 1.0698755618734451e-17 },
  { 0.35877067027057225, -2.4623815582638635e-17 },
  { 0.4636476090008061, 2.2698777452961687e-17 },
  { 0.5585993153435624, -5.4556305485916264e-18 },
  { 0.6435011087932844, 1.5834785051444286e-17 },
  { 0.7188299996216245, -2.1478388444456983e-17 },
  { 0.7853981633974483, 3.061616997868383e-17 },
};

static const double kPiHi = 3.141592653589793;
static const double kPiO2Hi = 1.5707963267948966, kPiO2Lo = 6.123233995736766e-17;
// Cody-Waite 3-part pi/2 (33+33+rest bits) for sin range reduction
static const double kPio2A = 1.5707963267341256;
static const double kPio2B = 6.077100506303966e-11;
static const double kPio2C = 2.0222662487959506e-21;
static const double kTwoOverPi = 0.6366197723675814;

// fci12 -- the asm's `dd 0.083333333333` (a 24-bit float that is NOT 1/12;
// see synth_core.cpp v2_fci12). Must be float so oscfreqi multiplies the
// same value the asm loads with `fmul dword`.
static const float kFci12 = 0.083333333333f;

// ---------------------------------------------------------------------------
// double-precision cores (~2^-55)
// ---------------------------------------------------------------------------

// 2^r for r in (-1, 1): table 2^(k/32) (double-double) * poly(2^t-1), |t|<=1/64
inline double exp2Core(double r)
{
  int k = (int)lrint(r * 32.0);            // -32..32
  double t = r - (double)k * 0.03125;      // exact (Sterbenz)
  double q = t * (kExp2m1C[0] + t * (kExp2m1C[1] + t * (kExp2m1C[2] +
             t * (kExp2m1C[3] + t * (kExp2m1C[4] + t * (kExp2m1C[5] +
             t * kExp2m1C[6]))))));        // 2^t - 1
  int kk = k + 32;                         // 0..64
  const double *T = kExp2Tab[kk & 31];
  double m = T[0] + (T[0] * q + (T[1] + T[1] * q));
  return ldexp(m, (kk >> 5) - 1);          // exact scale
}

// log2(x) for finite float x > 0, absolute error ~2^-55
inline double log2Core(float xf)
{
  int ex;
  double m = frexp((double)xf, &ex);       // m in [0.5, 1)
  m *= 2.0;
  ex -= 1;                                 // m in [1, 2)
  int j = (int)lrint((m - 1.0) * 32.0);    // 0..32
  double Tj = 1.0 + (double)j * 0.03125;   // exact
  double s = (m - Tj) / (m + Tj);          // |s| <= ~1/128
  double s2 = s * s;
  double p = kTwoOverLn2 * s * (1.0 + s2 * (1.0 / 3.0 + s2 * (1.0 / 5.0 +
             s2 * (1.0 / 7.0))));          // 2*atanh(s)/ln2
  return (double)ex + (kLog2Tab[j][0] + (p + kLog2Tab[j][1]));
}

inline double sinPoly(double t)            // |t| <= pi/4
{
  double w = t * t;
  return t + t * w * (kSinC[0] + w * (kSinC[1] + w * (kSinC[2] + w * (kSinC[3] +
         w * (kSinC[4] + w * (kSinC[5] + w * (kSinC[6] + w * kSinC[7])))))));
}
inline double cosPoly(double t)            // |t| <= pi/4
{
  double w = t * t;
  return 1.0 + w * (kCosC[0] + w * (kCosC[1] + w * (kCosC[2] + w * (kCosC[3] +
         w * (kCosC[4] + w * (kCosC[5] + w * (kCosC[6] + w * kCosC[7])))))));
}

// sin(x) for float-origin x (synth feeds |x| < ~few*2pi; reduction good to
// |x| ~ 1e6), absolute error ~2^-55
inline double sinCore(double x)
{
  int n = (int)lrint(x * kTwoOverPi);
  double t = x - (double)n * kPio2A;
  t -= (double)n * kPio2B;
  t -= (double)n * kPio2C;                 // x - n*pi/2, |t| <= pi/4
  switch (n & 3) {
  default:
  case 0: return sinPoly(t);
  case 1: return cosPoly(t);
  case 2: return -sinPoly(t);
  case 3: return -cosPoly(t);
  }
}

// cos(x) = sin(x + pi/2): same reduction, quadrant shifted by one. Computed
// directly (no pi/2 add) so it shares sinCore's determinism / accuracy.
inline double cosCore(double x)
{
  int n = (int)lrint(x * kTwoOverPi);
  double t = x - (double)n * kPio2A;
  t -= (double)n * kPio2B;
  t -= (double)n * kPio2C;                 // x - n*pi/2, |t| <= pi/4
  switch ((n + 1) & 3) {
  default:
  case 0: return sinPoly(t);
  case 1: return cosPoly(t);
  case 2: return -sinPoly(t);
  case 3: return -cosPoly(t);
  }
}

inline double atan01(double a)             // 0 <= a <= 1
{
  int j = (int)lrint(a * 8.0);             // 0..8
  double c = (double)j * 0.125;            // exact
  double t = (a - c) / (1.0 + a * c);      // |t| <= 1/16
  double w = t * t;
  double p = t + t * w * (-1.0 / 3.0 + w * (1.0 / 5.0 + w * (-1.0 / 7.0 +
             w * (1.0 / 9.0 + w * (-1.0 / 11.0)))));
  return kAtanTab[j][0] + (p + kAtanTab[j][1]);
}

inline double atanCore(double x)           // any finite x
{
  double a = fabs(x);
  double r;
  if (a > 1.0)
    r = kPiO2Hi - (atan01(1.0 / a) - kPiO2Lo);
  else
    r = atan01(a);
  return x < 0.0 ? -r : r;
}

// ---------------------------------------------------------------------------
// asm-kernel mirrors (float32 roundings exactly where the x87 rounds at PC=24)
// ---------------------------------------------------------------------------

// asm pow2 (synth.asm:388): 2^y. fprem splits y; f2xm1+faddp produce the
// 24-bit-rounded mantissa; fscale is an exact power-of-2 scale.
inline float pow2f(float y)
{
  float yi = truncf(y);                    // fprem/fscale integer part
  double r = (double)y - (double)yi;       // exact, in (-1, 1)
  float m24 = (float)exp2Core(r);          // the faddp PC=24 rounding
  return ldexpf(m24, (int)yi);             // exact (single rounding if subnormal)
}

// asm pow (synth.asm:399): base^e via fyl2x (64-bit) feeding the pow2 tail.
inline float powf24(float base, float e)
{
  double t = (double)e * log2Core(base);   // fyl2x
  double ti = trunc(t);
  float m24 = (float)exp2Core(t - ti);
  return ldexpf(m24, (int)ti);
}

// fistp (round-to-nearest-even, the x87 default mode). Out-of-range/NaN
// stores the INTEGER INDEFINITE 0x80000000 on the x87 -- lrintf is UB there,
// so mirror the exact semantics explicitly (extreme pitches CAN overflow the
// 32-bit osc freq; determinism requires the same defined result everywhere).
inline int32_t fistp(float x)
{
  float r = rintf(x);                      // RNE, exact
  if (!(r >= -2147483648.0f && r < 2147483648.0f))
    return (int32_t)0x80000000;            // x87 integer indefinite (also NaN)
  return (int32_t)r;
}

// syOscChgPitch freq tail (one x87 sequence in the asm; see synth_core.cpp
// v2_oscfreq): round( 2^((pitch+note-60) * fci12) * basefreq )
inline int32_t oscfreqi(float pno, float base)
{
  float t = pno * kFci12;                  // fmul dword (24-bit)
  float m = pow2f(t);                      // pow2 tail
  return fistp(m * base);                  // fmul (24-bit) + fistp
}

// fld1; fpatan -- the year-2000 native atan (overdrive waveshaper)
inline float atanf24(float x) { return (float)atanCore((double)x); }

// fsin -- the year-2000 native sine (osc sine / LFO). Host fsin was
// CPU-vendor-dependent; this fixed implementation is the portable reference.
inline float sinf24(float x) { return (float)sinCore((double)x); }

// fcos -- deterministic stand-in for the set-time x87 fcos (boost-EQ coeff);
// libm cos is not correctly-rounded, so it differs per arch/vendor.
inline float cosf24(float x) { return (float)cosCore((double)x); }

// dist OVERDRIVE setup tail (asm .mode1, synth.asm:1831-1838):
// gain2 = (param1/128) / atan(gain1); fpatan full precision, fdiv rounds.
inline float odGain2(float p1g, float gain1)
{
  return (float)((double)p1g / atanCore((double)gain1));
}

} // namespace vm
} // namespace v2redux

#endif // V2MATH_H_
