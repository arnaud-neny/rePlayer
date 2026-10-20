#include "v2core.h"
#include "v2eras.h"
#include "v2math.h"
#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h> // V2_STEAL voice-allocation tap (debug)

// the behavior-delta ledger (V2Delta ids + oldBehavior); see v2eras.h
using namespace v2redux;

// TODO:
// - VU meters?

// Ye olde original V2 bugs you can turn on and off :)
// Each is 1 = ASM-faithful (reproduces the shipping demo audio), 0 = fixed.
//   BUG_V2_FM_RANGE   - broken sine range reduction for FM oscis
//   BUG_V2_ATAN_TABLE - fastatan picks the wrong rational table for |x|>=2
//                       (cmovge vs cmovae on the exponent byte); affects the
//                       distortion "overdrive" mode
//   BUG_V2_COMP_OLDMODE - syCompInit only sets mode=2 and leaves oldmode at 0
//                       (zeroed instance) — but PEAK|MONO|ON also encodes to
//                       0, so a compressor that is peak/mono/on from its very
//                       first set() never takes the mode-change reset and
//                       starts with curgain = 0.0 instead of 1.0 (the
//                       channel's first note fades in over ~25ms instead of
//                       starting at full gain).
#define BUG_V2_FM_RANGE   1
#ifndef BUG_V2_ATAN_TABLE     // overridable from the build (-DBUG_V2_ATAN_TABLE=0)
#define BUG_V2_ATAN_TABLE 1
#endif
#ifndef BUG_V2_COMP_OLDMODE   // overridable from the build (-DBUG_V2_COMP_OLDMODE=0)
#define BUG_V2_COMP_OLDMODE 1
#endif

// Debugging tools
#define DEBUGSCOPES 0
#define COVERAGE    0

// --------------------------------------------------------------------------
// Debug scopes
// --------------------------------------------------------------------------

#if DEBUGSCOPES
#include "scope.h"
#define DEBUG_PLOT_OPEN(which, title, rate, w, h) scopeOpen((which), (title), (rate), (w), (h))
#define DEBUG_PLOT_VAL(which, value) do { float t=value; scopeSubmit((which), &t, 1); } while(0)
#define DEBUG_PLOT(which, data, nsamples) scopeSubmit((which), (data), (nsamples))
#define DEBUG_PLOT_STRIDED(which, data, stride, nsamples) scopeSubmitStrided((which), (data), (stride), (nsamples))
#define DEBUG_PLOT_UPDATE() scopeUpdateAll()
#else
#define DEBUG_PLOT_OPEN(which, title, rate, w, h)
#define DEBUG_PLOT_VAL(which, value)
#define DEBUG_PLOT(which, data, nsamples)
#define DEBUG_PLOT_STRIDED(which, data, stride, nsamples)
#define DEBUG_PLOT_UPDATE()
#endif

#define DEBUG_PLOT_CHAN(which, ch) ((unsigned char *)(which)+(ch))
#define DEBUG_PLOT_STEREO(which, data, nsamples) \
  DEBUG_PLOT_STRIDED(DEBUG_PLOT_CHAN(which, 0), &(data)->l, 2, (nsamples)); \
  DEBUG_PLOT_STRIDED(DEBUG_PLOT_CHAN(which, 1), &(data)->r, 2, (nsamples))

// --------------------------------------------------------------------------
// Code coverage
// --------------------------------------------------------------------------

#if COVERAGE
#include <stdio.h>

static const char *code_coverage[500];
#define COVER(desc) code_coverage[__COUNTER__] = (desc)

static sInt synthGetNumCoverage();
#else
#define COVER(desc)
#endif

// --------------------------------------------------------------------------
// Constants.
// --------------------------------------------------------------------------

// Natural constants
static const sF32 fclowest = 1.220703125e-4f; // 2^(-13) - clamp EGs to 0 below this (their nominal range is 0..128) 
static const sF32 fcpi_2  = 1.5707963267948966192313216916398f;
static const sF32 fcpi    = 3.1415926535897932384626433832795f;
static const sF32 fc1p5pi = 4.7123889803846898576939650749193f;
static const sF32 fc2pi   = 6.28318530717958647692528676655901f;
static const sF32 fc32bit = 2147483648.0f; // 2^31 (original code has (2^31)-1, but this ends up rounding up to 2^31 anyway)

// Synth constants
static const sF32 fcoscbase   = 261.6255653f; // Oscillator base freq
// era <v1 (fr08): the 2000 osc multiplies the pitch-pow2 by this baked constant
// (0x4a4265dc) instead of the SR-computed SRfcobasefrq. = fcoscbase·2²⁹/44100;
// ≈ SRfcobasefrq/4 (the 2000 render advances cnt 4× per sample). DELTA.md d2.
static const sF32 fcoscbase_v0 = 3185015.0f;
static const sF32 fcsrbase    = 44100.0f;     // Base sampling rate
static const sF32 fcboostfreq = 150.0f;       // Bass boost cut-off freq
static const sF32 fcframebase = 128.0f;       // size of a frame in samples
static const sF32 fcdcflt     = 126.0f;
static const sF32 fccfframe   = 11.0f;

static const sF32 fcfmmax     = 2.0f;
static const sF32 fcattackmul = -0.09375f; // -0.0859375
static const sF32 fcattackadd = 7.0f;
// era <v2 (fr08): the original attack multiplier -- kb's comment above IS this
// value; the change to -12/128 came with format v2 (fr08-extraction/DELTA.md).
static const sF32 fcattackmul_v0 = -0.0859375f;
static const sF32 fcsusmul    = 0.0019375f;
static const sF32 fcgain      = 0.6f;
static const sF32 fcgainh     = 0.6f;
static const sF32 fcmdlfomul  = 1973915.49f;
static const sF32 fcrms8192 = 0.0110485434560398050687631931578883f; // asm fci8192 = 1/sqrt(8192), 24-bit
static const sF32 fcmoogsixth = 1.0f/6.0f; // asm fci6 (24-bit float). As a BARE
  // literal inside the moog clip expression gcc/x87 promotes it to an ~exact
  // 80-bit fldt (FLT_EVAL_METHOD=2 constant-promotion bug) — 1 ULP off the
  // asm's fmul dword [fci6] for ~25% of inputs. A static const sF32 loads flds.
static const sF32 fccpdfalloff = 0.9998f; // @@@BUG this should probably depend on sampling rate.

static const sF32 fcdcoffset  = 3.814697265625e-6f; // 2^-18

// --------------------------------------------------------------------------
// General helper functions. 
// --------------------------------------------------------------------------

#define COUNTOF(x)    (sizeof(x)/sizeof(*(x)))

// Float bitcasts. Union-based type punning to maximize compiler
// compatibility.
union FloatBits
{
  sF32 f;
  sU32 u;
};

static sF32 bits2float(sU32 u)
{
  FloatBits x;
  x.u = u;
  return x.f;
}

// Fast arctangent
static sF32 fastatan(sF32 x)
{
  // extract sign
  sF32 sign = 1.0f;
  if (x < 0.0f)
  {
    sign = -1.0f;
    x = -x;
  }

  // we have two rational approximations: one for |x| < 1.0 and one for
  // |x| >= 1.0, both of the general form
  //   r(x) = (cx1*x + cx3*x^3) / (cxm0 + cxm2*x^2 + cxm4*x^4) + bias
  // PORTING FIX (not an ASM bug): the ASM loads these coefficients as DOUBLES
  // (fmul/fadd qword [fcatan*], synth.asm:108-113) — narrowing them to sF32
  // flips the 24-bit rounding of the products/sums for ~28% of inputs (1 ULP).
  // In particular the |x|>=1 bias is the full double pi/2, not 1.57079633f.
  // Values copied verbatim from synth.asm.
  // PORTING FIX (not an ASM bug): cxm2 (x^2 denom coeff) is SHARED across both
  // tables in the ASM (0.76443945); only cxm0/cxm4 are per-table. The original
  // C++ port made cxm2 per-table and swapped it with cxm4. Corrected here.
  static const sF64 coeffs[2][6] = {
    //         cx1          cx3        cxm0        cxm2        cxm4   bias
    {          1.0, 0.43157974,        1.0, 0.76443945, 0.05831938,   0.0 },
    { -0.431597974,       -1.0, 0.05831938, 0.76443945,        1.0,
      1.5707963267948966192313216916398 },
  };
#if BUG_V2_ATAN_TABLE
  // ASM bug: the |x|>=1 table is selected only when the biased exponent == 0x7f
  // (cmovge vs cmovae), i.e. x in [1,2); for x >= 2 it uses the |x|<1 table.
  const sF64 *c = coeffs[x >= 1.0f && x < 2.0f];
#else
  const sF64 *c = coeffs[x >= 1.0f]; // corrected: |x|>=1 table for all x >= 1
#endif
  // asm fastatan at PC=24: double (qword) coefficients, every op rounded to
  // 24 bits. Mirror per op (see fastsin note); plain double expressions skip
  // roundings and diverge the overdrive path.
  sF32 x2 = x*x;                       // fmul st0,st0
  sF32 A = (sF32)((sF64)x2 * c[1]);    // fmul qword [fcatanx3]
  sF32 B = (sF32)((sF64)x2 * c[4]);    // fmul qword [fcatanxm4]
  A = (sF32)((sF64)A + c[0]);          // fadd qword [fcatanx1]
  B = (sF32)((sF64)B + c[3]);          // fadd qword [fcatanxm2]
  sF32 num = A * x;                    // fmulp st3,st0
  sF32 den = B * x2;                   // fmulp st1,st0
  den = (sF32)((sF64)den + c[2]);      // fadd qword [fcatanxm0]
  sF32 r = num / den;                  // fdivp st1,st0
  r = (sF32)((sF64)r + c[5]);          // fadd qword [fcatanadd]
  return r * sign;                     // fmulp st1,st0
}

// Fast sine for x in [-pi/2, pi/2]
// This is a single-precision odd Minimax polynomial, not a Taylor series!
// Coefficients courtesy of Robin Green.
static sF32 fastsin(sF32 x)
{
  sF32 x2 = x*x;
  // asm fastsin loads the polynomial coeffs as qword DOUBLES (fcsinx3/5/7 =
  // `dq`) and evaluates on the x87 at PC=24: the CONSTANTS are double values
  // but EVERY operation rounds to a 24-bit significand. Mirror that per op:
  // each step is float(double_const OP float) -- a single-rounding-equivalent
  // of the asm op (the double intermediate only double-rounds at razor ties).
  // Computing the whole expression in double (as a plain C double expression
  // would) is NOT the asm: it skips five roundings and diverges every SIN
  // LFO / sin / FM-sin osc (found by the portable-engine corpus A/B).
  sF32 t = (sF32)(-0.00018542 * (sF64)x2);   // fmul qword [fcsinx7]
  t = (sF32)((sF64)t + 0.0083143);           // fadd qword [fcsinx5]
  t = t * x2;                                // fmul st0,st1
  t = (sF32)((sF64)t - 0.16666);             // fadd qword [fcsinx3]
  t = t * x2;                                // fmulp st1,st0
  t = 1.0f + t;                              // fld1; faddp
  return t * x;                              // fmulp st1,st0
}

// Fast sine with range check (for x >= 0)
// Applies symmetries, then funnels into fastsin.
static sF32 fastsinrc(sF32 x)
{
  // @@@BUG
  // NB this range reduction really only works for values >=0,
  // yet FM-sine oscillators will also pass in negative values.
  // This is not a good idea. At all. But it's what the original
  // V2 code does. :)

  // first range reduction: mod with 2pi
  x = fmodf(x, fc2pi);
  // now x in [-2pi,2pi]

#if !BUG_V2_FM_RANGE
  if (x < 0.0f)
    x += fc2pi;
#endif

  // need to reduce to [-pi/2, pi/2] to call fastsin
  if (x > fc1p5pi) // x in (3pi/2,2pi]
    x -= fc2pi; // sin(x) = sin(x-2pi)
  else if (x > fcpi_2) // x in (pi/2,3pi/2]
  {
    // asm fastsinrc reflects about `fldpi` -- the x87's 80-bit pi -- NOT a 32-bit
    // float `fcpi`. At PC=24 the float-pi result rounds to a different 24-bit
    // value (1 ULP), and that ULP, via SIN-LFO -> osc pitch -> freq fistp
    // boundary, accumulates into audible osc phase drift over the song. Use the
    // x87 pi to match. Portable: double pi (vs the asm's 80-bit fldpi --
    // differs only at razor ties after the 24-bit output rounding).
    x = (sF32)(3.14159265358979323846264 - (double)x);
  }

  return fastsin(x);
}

// Transcendental kernels: project-owned portable implementations (v2math.h)
// that mirror the asm pow2/pow/fistp/fsin/fpatan kernels' ROUNDING STRUCTURE
// (high-precision core + float32 rounding exactly where the x87 rounds at
// PC=24). Verified bit-identical against the genuine x87 sequences at PC=24
// by test/mathcheck (0 mismatches in 25M+ points incl. the fistp-tie
// sensitive oscfreq path); see CONVENTIONS.md.
static inline sF32 v2_pow2(sF32 y)              { return v2redux::vm::pow2f(y); }
static inline sF32 v2_pow(sF32 base, sF32 e)    { return v2redux::vm::powf24(base, e); }
static inline sInt v2_fistp(sF32 v)             { return v2redux::vm::fistp(v); }
static inline sInt v2_oscfreq(sF32 pno, sF32 base) { return v2redux::vm::oscfreqi(pno, base); }
static inline sF32 v2_overdrive_gain2(sF32 p1g, sF32 gain1) { return v2redux::vm::odGain2(p1g, gain1); }
static inline sF32 v2_fsin(sF32 x)              { return v2redux::vm::sinf24(x); }
static inline sF32 v2_fcos(sF32 x)              { return v2redux::vm::cosf24(x); }
static inline sF32 v2_atan(sF32 x)              { return v2redux::vm::atanf24(x); }

// 2^x and base^e: faithful x87 kernels in the validation build, libm otherwise.
// Routing ALL transcendentals through these makes every coefficient (env, lfo,
// filter, distortion, reverb, mod-delay, compressor) match the asm in the
// faithful build, not just the oscillator frequency.
static inline sF32 v2_exp2(sF32 x)
{
  return v2_pow2(x);
}
static inline sF32 v2_sin(sF32 x)
{
  return v2_fsin(x);
}
static inline sF32 v2_atanf(sF32 x)  // native atan for the eraV0 overdrive
{
  return v2_atan(x);
}
static inline sF32 v2_powf(sF32 base, sF32 e)
{
  return v2_pow(base, e);
}

static sF32 calcfreq(sF32 x)  { return v2_exp2((x - 1.0f) * 10.0f); }
static sF32 calcfreq2(sF32 x) { return v2_exp2((x - 1.0f) * fccfframe); }

// tri/saw box-filter working type. The "hard" cases (b/d/e/f) have a
// catastrophic cancellation amplified by rcpf=1/f. The asm computes this on the
// x87 at 24-bit mantissa / 15-bit exponent. The faithful build runs x87 at
// PC=24 (-mpc32) so plain `float` reproduces that exactly. The portable build
// uses SSE single (8-bit exponent), which would overflow the cancellation at
// low frequencies, so it uses `double` as an approximation of the asm.
// PORTABLE NOTE: the faithful x87 build used `float` here (PC=24 gives the
// 24-bit significand AND a 15-bit register exponent that survives the
// rcpf-amplified cancellation). Plain float32 would overflow that range at
// low frequencies; double is the portable approximation (range-correct,
// precision a superset of PC=24). One of the two documented epsilon sources
// vs the x87 oracles (the other: flcalc below).
typedef float trisaw_flt; // EXPERIMENT

// square
static inline sF32 sqr(sF32 x)
{
  return x*x;
}

template<typename T>
static inline T min(T a, T b)
{
  return a < b ? a : b;
}

template<typename T>
static inline T max(T a, T b)
{
  return a > b ? a : b;
}

template<typename T>
static inline T clamp(T x, T min, T max)
{
  return (x < min) ? min : (x > max) ? max : x;
}

// uniform randon number generator
// just a linear congruential generator, nothing fancy.
static inline sU32 urandom(sU32 *seed)
{
  *seed = *seed * 196314165 + 907633515;
  return *seed;
}

// uniform random float in [-1,1)
static inline sF32 frandom(sU32 *seed)
{
  sU32 bits = urandom(seed); // random 32-bit value
  sF32 f = bits2float((bits >> 9) | 0x40000000); // random float in [2,4)
  return f - 3.0f; // uniform random float in [-1,1)
}

// 32-bit value into float with 23 bits percision
static inline sF32 utof23(sU32 x)
{
  sF32 f = bits2float((x >> 9) | 0x3f800000); // 1 + x/(2^32)
  return f - 1.0f;
}

// float from [0,1) into 0.32 unsigned fixed-point
// this loses a bit, but that's what V2 does.
static inline sU32 ftou32(sF32 v)
{
  // asm computes these as `fistp; shl x,1` = 2*round_to_nearest(v*fc32bit). The
  // port truncated via (sInt); that off-by-one (when frac>=0.5) desyncs every
  // ftou32-derived integer phase/breakpoint (osc brpt, lfo cphase, dist dfreq,
  // moddel mphase). Match the asm round in the faithful build.
  return 2u * (sU32)v2_fistp(v * fc32bit);
}

// linear interpolation between a and b using t.
static inline sF32 lerp(sF32 a, sF32 b, sF32 t)
{
  return a + t * (b-a);
}

// --------------------------------------------------------------------------
// Building blocks
// --------------------------------------------------------------------------

union StereoSample
{
  struct
  {
    sF32 l, r;
  };
  sF32 ch[2];
};

// LRC filter.
// The state variables are 'l' and 'b'. The time series for l and b
// correspond to a resonant low-pass and band-pass respectively, hence
// the name. 'step' returns 'h', which is just the "missing" resonant
// high-pass.
//
// Note that 'freq' here isn't actually a frequency at all, it's actually
// 2*(1 - cos(2pi*freq/SR)), but V2 calls this "frequency" anyway :)
struct V2LRC
{
  sF32 l, b;

  void init()
  {
    l = b = 0.0f;
  }

  // Single step
  sF32 step(sF32 in, sF32 freq, sF32 reso)
  {
    l += freq * b;
    sF32 h = in - b*reso - l;
    b += freq * h;
    return h;
  }

  // 2x oversampled step (the good stuff)
  sF32 step_2x(sF32 in, sF32 freq, sF32 reso)
  {
    // the filters get slightly biased inputs to avoid the state variables
    // getting too close to 0 for prolonged periods of time (which would
    // cause denormals to appear)
    in += fcdcoffset;

    // step 1
    l += freq * b - fcdcoffset; // undo bias here (1 sample delay)
    b += freq * (in - b*reso - l);

    // step 2
    l += freq * b;
    sF32 h = in - b*reso - l;
    b += freq * h;

    return h;
  }
};

// VCF block-working type. The asm (syFltRender) keeps the SVF state l,b (and coeffs
// f,r) on the x87 stack across the WHOLE block -- 15-bit exponent, mantissa at PC=24
// (24-bit). The port's V2LRC.l/b are 32-bit floats (8-bit exponent), stored back each
// sample; the high-pass output h = in - b*reso - l is a cancellation, so the narrower
// exponent of an intermediate diverges in HIGH/BAND serial chains. Mirror the asm:
// keep the block state at register width, narrowing to 32-bit only on store
// between blocks (the asm's fstp dword).
// PORTABLE NOTE: the x87 register is 24-bit significand (PC=24) + 15-bit
// exponent -- inexpressible in portable C++. double is the approximation
// (wider significand, sufficient exponent). Filter-state recurrences may
// drift vs the x87 oracles at the sub-ULP level; documented epsilon source.
typedef float flcalc; // EXPERIMENT: per-op 24-bit rounding like the asm

// 2x-oversampled SVF step on register-width working state (mirrors V2LRC::step_2x /
// the asm .process). Updates l,b in place; returns the high-pass output h.
// dco = the denormal-prevention DC bias (fcdcoffset normally; 0 for era <v1,
// where the 2000 syFltRender injects no bias -- DELTA.md delta 6).
static inline flcalc lrc_step_2x(flcalc &l, flcalc &b, flcalc in, flcalc freq, flcalc reso, flcalc dco)
{
  in += dco;
  l += freq * b - dco;
  b += freq * (in - b*reso - l);
  l += freq * b;
  flcalc h = in - b*reso - l;
  b += freq * h;
  return h;
}

// Moog filter state
struct V2Moog
{
  sF32 b[5]; // filter state

  void init()
  {
    b[0] = b[1] = b[2] = b[3] = b[4] = 0.0f;
  }

  sF32 step(sF32 realin, sF32 f, sF32 p, sF32 q)
  {
    sF32 in = realin + fcdcoffset; // again, biased in
    sF32 t1, t2, t3, b4;

    in -= q * b[4]; // feedback
    t1 = b[1]; b[1] = (in + b[0]) * p + b[1] * f;
    t2 = b[2]; b[2] = (t1 + b[1]) * p + b[2] * f;
    t3 = b[3]; b[3] = (t2 + b[2]) * p + b[3] * f;
               b4   = (t3 + b[3]) * p + b[4] * f;

    b4 -= b4*b4*b4 * fcmoogsixth; // clipping (see fcmoogsixth: do NOT inline the literal)
    b4 -= fcdcoffset; // un-bias
    b[4] = b4; // feedback state keeps the once-unbiased value (matches ASM)
    b[0] = realin;

    return b4;
  }
};

// DC filter state. Just a highpass used with a very low cut-off
// to remove DC offsets from a signal.
struct V2DCF
{
  sF32 xm1; // x(n-1)
  sF32 ym1; // y(n-1)

  void init()
  {
    xm1 = ym1 = 0.0f;
  }

  sF32 step(sF32 in, sF32 R)
  {
    // y(n) = x(n) - x(n-1) + R*y(n-1)
    // asm syDCFRenderStereo order: ((R*ym1 - xm1 + in) + dc) - dc -- the denormal-
    // avoidance bias `dc` is added AFTER the main sum, not before. Writing it as
    // `(dc + R*ym1 - xm1 + in) - dc` rounds the recurrence differently each sample;
    // with the pole R~0.997 that ~1 ULP error integrates (1/(1-R)~333x) into audible
    // channel drift. Match the asm operand order exactly.
    sF32 y = (R*ym1 - xm1 + in + fcdcoffset) - fcdcoffset;
    xm1 = in;
    ym1 = y;
    return y;
  }
};

// Constant-length delay line
class V2Delay
{
  sU32 pos, len;
  sF32 *buf;

public:
  V2Delay()
    : pos(0), len(0), buf(0)
  {
  }

  void init(sF32 *buf, sU32 len)
  {
    this->buf = buf;
    this->len = len;
    reset();
  }

  template<sU32 N>
  void init(sF32 (&buf)[N])
  {
    init(buf, N);
  }

  void reset()
  {
    memset(buf, 0, sizeof(*buf)*len);
    pos = 0;
  }

  inline sF32 fetch() const
  {
    return buf[pos];
  }

  inline void feed(sF32 v)
  {
    buf[pos] = v;
    if (++pos == len)
      pos = 0;
  }
};

// debug stuff
static void checkRange(const sF32 *src, sInt nsamples)
{
  for (sInt i=0; i < nsamples; i++)
    assert(src[i] >= -1.5f && src[i] < 1.5f);
}

static void checkRange(const StereoSample *src, sInt nsamples)
{
  checkRange(&src[0].l, nsamples * 2);
}

// --------------------------------------------------------------------------
// V2 Instance
// --------------------------------------------------------------------------

// Deterministic libc-rand replacement for the modern LFO S&H seed path. The
// original code calls rand() ("not really, but close enough...") -- host's
// libc, breaking cross-host determinism. This replicates glibc's TYPE_3
// additive generator at its default seed, so renders stay bit-identical to
// the lab oracles on glibc hosts AND identical on every other platform.
struct V2Rand {
  // ring of the last 31 values of the additive recurrence r[i]=r[i-31]+r[i-3]
  // (mod 2^32); outputs are (uint32)r[i] >> 1 starting at i=344 (glibc seeds
  // r[0..33] then discards 310 outputs). Verified against the glibc sequence:
  // seed 1 -> 1804289383, 846930886, 1681692777, ...
  sU32 ring[31];
  sInt head;
  void init(sU32 seed)
  {
    if (seed == 0) seed = 1;                  // glibc: srand(0) == srand(1)
    sU32 r[344];
    r[0] = seed;
    for (sInt i = 1; i < 31; i++) {
      sS64 v = (sS64)16807 * (sS32)r[i-1] % 2147483647;
      if (v < 0) v += 2147483647;             // glibc's signed mod trick
      r[i] = (sU32)v;
    }
    for (sInt i = 31; i < 34; i++)  r[i] = r[i-31];
    for (sInt i = 34; i < 344; i++) r[i] = r[i-31] + r[i-3];
    for (sInt j = 0; j < 31; j++) ring[j] = r[313 + j];
    head = 0;
  }
  sU32 next()
  {
    sU32 v = ring[head] + ring[(head + 28) % 31]; // r[i-31] + r[i-3]
    ring[head] = v;
    head = (head + 1) % 31;
    return v >> 1;
  }
};

// Player-provided seed variation (synthSetSeed; replaces the historical rdtsc
// non-determinism with an explicit knob). 0 (default) = the deterministic
// reference: the asm's fixed table for modern, all-zero for eraV0 (the C1
// pinned-rdtsc convention). Nonzero = decorrelated per-index variation.
static inline sU32 seedMix(sU64 userSeed, sU32 base, sInt idx)
{
  if (!userSeed) return base;
  sU64 z = userSeed + (sU64)(idx + 1) * 0x9e3779b97f4a7c15ull; // splitmix64
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return base ^ (sU32)(z >> 32);
}


struct V2Instance
{
  static const int MAX_FRAME_SIZE = 280; // in samples

  // Era compat (fr08-extraction/DELTA.md): the v2m format version the song
  // was ORIGINALLY authored as, before any canonicalization upgrade.
  // SRCVER_MODERN (= the 2004 core, format v6) by default;
  // synthSetSourceVersion() lowers it for period files. Engine code never
  // compares srcVersion directly: every behavior question goes through the
  // v2eras.h ledger -- old(DELTA_X), ONE delta id per call site. The ledger
  // row carries the flip version + evidence; oldBehavior() constant-folds
  // every gate in single-version builds (V2_VER_MIN == V2_VER_MAX).
  static const sInt SRCVER_MODERN = 6;
  // The era coordinate (format-version base + sparse ledger overrides, see
  // v2eras.h Era). Defaults to modern; synthSetSourceVersion/synthSetEra lower
  // it for period files. Engine code never inspects it directly: every
  // behavior question goes through old(DELTA_X), ONE delta id per call site.
  Era era;
  bool old(V2Delta d) const {
#if V2_VER_MIN == V2_VER_MAX
    // Single-version build: oldBehavior folds to a compile-time constant and
    // cross-era overrides are unrepresentable anyway (the other version's code
    // paths aren't compiled), so bypass the masks entirely -- every gate folds.
    return oldBehavior(d, V2_VER_MIN);
#else
    return era.old(d);
#endif
  }
  // era voice-pool bound (16/32/64, DELTA_POLY_16/32): the ALLOCATOR never
  // scans voices past this, which is provably equivalent to the period
  // engine's smaller pool -- unallocated voices keep chanmap == -1 and every
  // other voice loop skips those. Arrays stay sized for the 2004 POLY = 64.
  sInt voicePool() const {
#if V2_VER_MIN == V2_VER_MAX
    return voicePoolSize(V2_VER_MIN);
#else
    return era.poolSize();
#endif
  }

  // Stuff that depends on the sample rate
  sF32 SRfcsamplesperms;
  sF32 SRfcobasefrq;
  sF32 SRfclinfreq;
  sF32 SRfcBoostCos, SRfcBoostSin;
  sF32 SRfcdcfilter;

  sInt SRcFrameSize;
  sF32 SRfciframe;

  // determinism (design D6): the libc-rand replacement stream + the player
  // seed knob (0 = deterministic reference)
  V2Rand rng;
  sU64 userSeed;

  // interactive channel mute (display/playback knob, NOT part of the determinism
  // contract). bit ch set = muted: the channel still renders its voices + FX
  // (all state advances identically to the reference) but contributes nothing to
  // any output bus -- so muting/unmuting is instant, in-phase, and click-free,
  // and never perturbs the rendered bits of the other channels' dry signal.
  // Default 0 (synthInit zeroes the whole instance) = every channel audible.
  sU32 chanMute;

  // per-channel output peak meter (display-only, read-and-cleared by the host).
  // Peak of each channel's post-FX mix contribution, max-accumulated across the
  // chunks rendered since the last read. Written during render but only READ
  // from chan[]/written to this side buffer -- never alters the rendered bits.
  sF32 chanPeak[16];   // 16 == V2Synth::CHANS (not in scope here)

  // buffers
  sF32 vcebuf[MAX_FRAME_SIZE];
  sF32 vcebuf2[MAX_FRAME_SIZE];
  StereoSample levelbuf[MAX_FRAME_SIZE]; // original V2 overlaps level buffer with voice buffers
  StereoSample chanbuf[MAX_FRAME_SIZE];
  sF32 aux1buf[MAX_FRAME_SIZE];
  sF32 aux2buf[MAX_FRAME_SIZE];
  StereoSample mixbuf[MAX_FRAME_SIZE];
  StereoSample auxabuf[MAX_FRAME_SIZE];
  StereoSample auxbbuf[MAX_FRAME_SIZE];

  void calcNewSampleRate(sInt samplerate)
  {
    sF32 sr = (sF32)samplerate;

    // asm calcNewSampleRate computes the reciprocal 1/sr ONCE (fld1/fdiv) and
    // MULTIPLIES by it; `a/sr` and `a*(1/sr)` round differently (1 ULP). That ULP
    // in SRfcobasefrq flips the osc freq fistp on some notes -> phase drift. Match
    // the asm: a single reciprocal, multiply, in the asm's operand order.
    sF32 recip = 1.0f / sr;

    SRfcsamplesperms = sr / 1000.0f; // (not computed by the asm; sr/1000 is fine)
    SRfcobasefrq = (fcoscbase * fc32bit) * recip;
    SRfclinfreq = fcsrbase * recip;
    SRfcdcfilter = 1.0f - fcdcflt * recip;

    // frame size
    SRcFrameSize = (sInt)(fcframebase * sr / fcsrbase + 0.5f);
    SRfciframe = 1.0f / (sF32)SRcFrameSize;

    assert(SRcFrameSize <= MAX_FRAME_SIZE);

    // low shelving EQ (asm order: (1/sr)*fc2pi*fcboostfreq)
    sF32 boost = recip * fc2pi * fcboostfreq;
    SRfcBoostCos = v2_fcos(boost);   // deterministic; libm cos differs per arch
    SRfcBoostSin = v2_fsin(boost);
  }
};


// --------------------------------------------------------------------------
// Oscillator
// --------------------------------------------------------------------------

struct syVOsc
{
  sF32 mode;    // OSC_* (as float. it's all floats in here)
  sF32 ring;
  sF32 pitch;
  sF32 detune;
  sF32 color;
  sF32 gain;
};

struct V2Osc
{
  enum Mode
  {
    OSC_OFF     = 0,
    OSC_TRI_SAW = 1,
    OSC_PULSE   = 2,
    OSC_SIN     = 3,
    OSC_NOISE   = 4,
    OSC_FM_SIN  = 5,
    OSC_AUXA    = 6,
    OSC_AUXB    = 7,
  };

  sInt mode;          // OSC_*
  bool ring;          // ring modulation on/off
  sU32 cnt;           // wave counter
  sInt freq;          // wave counter inc (8x/sample)
  sU32 brpt;          // break point for tri/pulse wave
  sF32 nffrq, nfres;  // noise filter freq/resonance
  sU32 nseed;         // noise random seed
  sF32 gain;          // output gain
  V2LRC nf;           // noise filter
  sF32 note;
  sF32 pitch;

  // era <v1 (fr08): the 2000 osc renders tri/saw + pulse as a 4x linearly-
  // oversampled numeric box filter (syOscRender @0x40a585), not the 2004
  // analytic convolution. These are the per-segment line coeffs syOscSet
  // @0x40a4d2 precomputes; see fr08-extraction/DELTA.md.
  sF32 v0_dn_k, v0_dn_o;  // cnt<brpt segment: val = p*dn_k + dn_o
  sF32 v0_up_k, v0_up_o;  // cnt>=brpt segment: val = p*up_k + up_o

  V2Instance *inst;   // V2 instance we belong to.

  void init(V2Instance *instance, sInt idx)
  {
    static const sU32 seeds[] = { 0xdeadbeefu, 0xbaadf00du, 0xd3adc0deu };
    assert(idx < COUNTOF(seeds));

    cnt = 0;
    nf.init();
    // era <v1 (fr08): the 2000 syOscInit seeds nseed from rdtsc; the C1 ground
    // truth pins rdtsc=0, so the matched-seed A/B (DELTA.md D6) needs nseed=0
    // here too. (At the first synthInit srcVersion is still MODERN; the era
    // setter re-seeds those voices. keysync noteOns re-init and hit this path
    // with the real srcVersion.)
    // SEED source (DELTA_RDTSC_SEED, flips at v6) is independent of the LCG
    // CONSTANTS (DELTA_NOISE_LCG_MSVC): at v5 the LCG is modern but the seed is
    // still rdtsc (== 0 here). The fixed seeds[] table is a v6 addition.
    nseed = seedMix(instance->userSeed,
                    instance->old(DELTA_RDTSC_SEED) ? 0u : seeds[idx], idx);
    inst = instance;
  }

  void noteOn()
  {
    chgPitch();
  }

  void chgPitch()
  {
    // asm syOscChgPitch (synth.asm:488-502): x87 at PC=24. fci128=0.0078125
    // (=1/128, exact), fci12=0.083333333333 (rounded 1/12, a MULTIPLY not a
    // divide), pow2 via f2xm1, freq stored with fistp (round-to-nearest).
    nffrq = inst->SRfclinfreq * calcfreq((pitch + 64.0f) * 0.0078125f);
    // whole freq tail (·fci12, pow2, ·base, fistp) as one x87 sequence with a
    // 24-bit fci12 -- bit-exact with the asm (no 80-bit constant / excess precision).
    // era <v1 (fr08): the 2000 synth multiplies by a baked constant 3185015.0
    // (= fcoscbase·2²⁹/44100 ≈ SRfcobasefrq/4) and advances cnt 4× per sample
    // in the oversampled render; 2004 computes SRfcobasefrq at runtime and
    // advances once. Same pitch, but the baked constant rounds differently.
    freq = v2_oscfreq(pitch + note - 60.0f,
                      inst->old(DELTA_OSC_FREQ_CONST) ? fcoscbase_v0
                                                      : inst->SRfcobasefrq);
  }

  void set(const syVOsc *para)
  {
    mode = (sInt)para->mode;
    ring = (((sInt)para->ring) & 1) != 0;

    pitch = (para->pitch - 64.0f) + (para->detune - 64.0f) / 128.0f;
    chgPitch();
    gain = para->gain / 128.0f;

    sF32 col = para->color / 128.0f;
    brpt = ftou32(col);
    nfres = 1.0f - sqrtf(col);

    if (inst->old(DELTA_OSC_BOXFILTER))
    {
      // 2000 syOscSet @0x40a4d2 box-segment coeffs (x87 PC=24, faithful build).
      // down (cnt<brpt, ramp -1->+1):  val = p*(2/col) + (-1-2/col)
      // up   (cnt>=brpt, ramp +1->-1): val = p*(-2/(1-col)) + (4/(1-col)-1)
      v0_dn_k = 2.0f / col;
      v0_dn_o = -1.0f - 2.0f / col;
      v0_up_k = -2.0f / (1.0f - col);
      v0_up_o = 4.0f / (1.0f - col) - 1.0f;
    }
  }

  void render(sF32 *dest, sInt nsamples)
  {
    // Per-mode era gates, one delta id per renderer. The _v0 renderers are
    // faithful ports of the 2000 syOscRender @0x40a585: tri/saw + pulse are
    // 4x-oversampled box filters, sine uses native fsin (freq<<2 advance when
    // DELTA_OSC_FREQ_CONST is old, 1x at v5 -- see renderSin_v0), noise uses
    // the MSVC LCG + 16-bit float gen. FM doesn't exist at v0 (oscjtab mode 5
    // = off, DELTA_NO_FM_OSC); once present it is native-fsin/integer-mod
    // through v5 (renderFMSin_v5) and fastsinrc/float-mod at v6.
    // AUXA/AUXB don't exist before v6 (DELTA_NO_AUX_BUSSES -> silence); ring
    // is default-off in canonicalized period files.
    switch (mode & 7)
    {
    case OSC_OFF:
      break;
    case OSC_TRI_SAW:
      if (inst->old(DELTA_OSC_BOXFILTER)) renderTriSaw_v0(dest, nsamples);
      else                                renderTriSaw(dest, nsamples);
      break;
    case OSC_PULSE:
      if (inst->old(DELTA_OSC_BOXFILTER)) renderPulse_v0(dest, nsamples);
      else                                renderPulse(dest, nsamples);
      break;
    case OSC_SIN:
      if (inst->old(DELTA_NATIVE_FSIN)) renderSin_v0(dest, nsamples);
      else                              renderSin(dest, nsamples);
      break;
    case OSC_NOISE:
      if (inst->old(DELTA_NOISE_LCG_MSVC)) renderNoise_v0(dest, nsamples);
      else                                 renderNoise(dest, nsamples);
      break;
    case OSC_FM_SIN:
      if (inst->old(DELTA_NO_FM_OSC))
        break; // 2000 oscjtab maps mode 5 to off (no FM renderer in v0)
      if (inst->old(DELTA_NATIVE_FSIN)) renderFMSin_v5(dest, nsamples);
      else                              renderFMSin(dest, nsamples);
      break;
    case OSC_AUXA:
      if (!inst->old(DELTA_NO_AUX_BUSSES))
        renderAux(dest, inst->auxabuf, nsamples);
      break;
    case OSC_AUXB:
      if (!inst->old(DELTA_NO_AUX_BUSSES))
        renderAux(dest, inst->auxbbuf, nsamples);
      break;
    }

    DEBUG_PLOT(this, dest, nsamples);
  }

private:
  // ---- year-2000 (era <v1) oscillator renderers (DELTA.md delta 2-4) --------
  // Faithful ports of syOscRender @0x40a585. The faithful build runs x87 at
  // PC=24 (-mpc32), so plain float arithmetic reproduces the 2000 rounding;
  // only the [1,2)-from-counter bit trick, the fistp, and fsin are special.

  // counter top bits -> float in [1,2): asm `shr eax,9; or 0x3f800000`.
  static inline sF32 v0_cnt2f(sU32 c)
  {
    return bits2float((c >> 9) | 0x3f800000);
  }

  void renderTriSaw_v0(sF32 *dest, sInt nsamples)
  {
    COVER("Osc v0 trisaw");
    sU32 c = cnt;
    sF32 bg = gain * 0.25f; // box gain = (gain/128)*0.25  (gain already /128)
    for (sInt i=0; i < nsamples; i++)
    {
      sF32 acc = 0.0f;
      for (sInt s=0; s < 4; s++) // 4x oversample + box average
      {
        sF32 p = v0_cnt2f(c);
        acc += (c < brpt) ? (p * v0_dn_k + v0_dn_o) : (p * v0_up_k + v0_up_o);
        c += (sU32)freq;
      }
      output(dest + i, acc * bg);
    }
    cnt = c;
  }

  void renderPulse_v0(sF32 *dest, sInt nsamples)
  {
    COVER("Osc v0 pulse");
    sU32 c = cnt;
    sF32 bg = gain * 0.25f;
    for (sInt i=0; i < nsamples; i++)
    {
      sF32 acc = 0.0f;
      for (sInt s=0; s < 4; s++)
      {
        if (c >= brpt) acc += 2.0f; // each high sub-sample contributes +2
        c += (sU32)freq;
      }
      output(dest + i, (acc - 4.0f) * bg); // {-4..+4} * box gain
    }
    cnt = c;
  }

  void renderSin_v0(sF32 *dest, sInt nsamples)
  {
    COVER("Osc v0 sin");
    sU32 c = cnt;
    // v0 advances 4*freq per output sample (the 4x-oversample convention
    // coupled to the baked freq constant); v5 (candytron .mode2) keeps the
    // SAME native-fsin evaluation but advances 1x on the runtime-fcoscbase
    // freq -- the eval and the advance are decoupled (6.0e assay).
    sU32 step = inst->old(DELTA_OSC_FREQ_CONST) ? ((sU32)freq << 2)
                                                : (sU32)freq;
    for (sInt i=0; i < nsamples; i++)
    {
      sF32 p = v0_cnt2f(c);
      c += step;
      output(dest + i, v2_sin(p * fc2pi) * gain); // native fsin, gain=gain/128
    }
    cnt = c;
  }

  void renderNoise_v0(sF32 *dest, sInt nsamples)
  {
    COVER("Osc v0 noise");
    // Exact 2000 noise + resonant LRC, traced from syOscRender @0x40a659. Note
    // this is a DIFFERENT recurrence/output from the 2004 V2LRC.step (and uses
    // the MSVC LCG + a 16-bit float gen). nf.{l,b} hold the two filter states.
#ifndef NDEBUG
    { static int dbgn=0; if (getenv("V2_NSEED") && dbgn<6) { dbgn++;
        sU32 fb,rb; memcpy(&fb,&nffrq,4); memcpy(&rb,&nfres,4);
        fprintf(stderr, "[nseed_v0] nseed=%u nffrq=%.9g[%08x] nfres=%.9g[%08x]\n", nseed, nffrq, fb, nfres, rb); } }
#endif
    sF32 sl = nf.l, sb = nf.b, f = nffrq, r = nfres;
    sU32 seed = nseed;
    for (sInt i=0; i < nsamples; i++)
    {
      seed = seed * 214013 + 2531011;          // MSVC LCG (0x343fd/0x269ec3)
      // 2000 float gen: (seed&0xffff)<<7 | 0x40000000 -> [2,4); n = that - 3
      sF32 n = bits2float(((seed & 0xffff) << 7) | 0x40000000) - 3.0f;
      sF32 bb = sb + sl * f;                    // b' = b + l*nffrq
      sF32 h  = (n - sl) * r - bb;              // h  = (n-l)*nfres - b'
      sF32 ll = sl + h * f;                     // l' = l + h*nffrq
      output(dest + i, gain * (h + bb + ll));   // out = (h + b' + l') * gain
      sl = ll; sb = bb;
    }
    nf.l = sl; nf.b = sb;
    nseed = seed;
  }

  inline void output(sF32 *dest, sF32 x)
  {
    if (ring)
      *dest *= x;
    else
      *dest += x;
  }

  // Oscillator state machine (read description of renderTriSaw for context)
  //
  // We keep track of whether the current sample is in the up or down phase,
  // whether the previous sample was, and if the waveform counter wrapped
  // around on the transition. This allows us to figure out which of
  // the cases above we fall into. Note this code uses a different bit ordering
  // from the ASM version that is hopefully a bit easier to understand.
  //
  // For reference: our bits map to the ASM version as follows (MSB->LSB order)
  //   (o)ld_up
  //   (c)arry
  //   (n)ew_up

  enum OSMTransitionCode    // carry:old_up:new_up
  {
    OSMTC_DOWN = 0,         // old=down, new=down, no carry
    // 1 is an invalid configuration
    OSMTC_UP_DOWN = 2,      // old=up, new=down, no carry
    OSMTC_UP = 3,           // old=up, new=up, no carry
    OSMTC_DOWN_UP_DOWN = 4, // old=down, new=down, carry
    OSMTC_DOWN_UP = 5,      // old=down, new=up, carry
    // 6 is an invalid configuration
    OSMTC_UP_DOWN_UP = 7    // old=up, new=up, carry
  };

  inline sU32 osm_init() // our state field: old_up:new_up
  {
    return (cnt - freq) < brpt ? 3 : 0;
  }

  inline sU32 osm_tick(sU32 &state) // returns transition code
  {
    // old_up = new_up, new_up = (cnt < brpt)
    state = ((state << 1) | (cnt < brpt)) & 3;

    // we added freq to cnt going from the previous sample to the current one.
    // so if cnt is less than freq, we carried.
    sU32 transition_code = state | (cnt < (sU32)freq ? 4 : 0); 

    // finally, tick the oscillator
    cnt += freq;

    return transition_code;
  }

  void renderTriSaw(sF32 *dest, sInt nsamples)
  {
    // Okay, so here's the general idea: instead of the classical sawtooth
    // or triangle waves, V2 uses a generalized triangle wave that looks like
    // this:
    //
    //       /\                  /\
    //      /   \               /   \
    //     /      \            /      \
    // ---/---------\---------/---------\> t
    //   /            \      /
    //  /               \   /
    // /                  \/
    // [-----1 period-----]
    // [-----] "break point" (brpt)
    //
    // If brpt=1/2 (ignoring fixed-point scaling), you get a regular triangle
    // wave. The example shows brpt=1/3, which gives an asymmetrical triangle
    // wave. At the extremes, brpt=0 gives a pure saw-down wave, and brpt=1
    // (if that was a possible value, which it isn't) gives a pure saw-up wave.
    //
    // Purely point-sampling this (or any other) waveform would cause notable
    // aliasing. The standard ways to avoid this issue are to either:
    // 1) Over-sample by a certain amount and then use a low-pass filter to
    //    (hopefully) get rid of the frequencies that would alias, or
    // 2) Generate waveforms from a Fourier series that's cut off below the
    //    Nyquist frequency, ensuring there's no aliasing to begin with.
    // V2 does neither. Instead it computes the convolution of the continuous
    // waveform with an analytical low-pass filter. The ideal low-pass in
    // terms of frequency response would be a sinc filter, which unfortunately
    // has infinite support. Instead, V2 just uses a simple box filter. This
    // doesn't exactly have favorable frequency-domain characteristics, but
    // it's still much better than point sampling and has the advantage that
    // it's fairly simple analytically. It boils down to computing the average
    // value of the waveform over the interval [t,t+h], where t is the current
    // time and h = 1/SR (SR=sampling rate), which is in turn:
    //
    //    f_box(t) = 1/h * (integrate(x=t..t+h) f(x) dx)
    //
    // Now there's a bunch of cases for these intervals [t,t+h] that we need to
    // consider. Bringing up the diagram again, and adding some intervals at the
    // bottom:
    //
    //       /\                  /\
    //      /   \               /   \
    //     /      \            /      \
    // ---/---------\---------/---------\> t
    //   /            \      /
    //  /               \   /
    // /                  \/
    // [-a-]      [-c]
    //     [--b--]       [-d--]
    //   [-----------e-----------]
    //          [-----------f-----------]
    //
    // a) is purely in the saw-up region,
    // b) starts in the saw-up region and ends in saw-down,
    // c) is purely in the saw-down region,
    // d) starts during saw-down and ends in saw-up.
    // e) starts during saw-up and ends in saw-up, but passes through saw-down
    // f) starts saw-down, ends saw-down, passes through saw-up.
    //
    // For simplicity here, I draw different-sized intervals sampling a fixed-
    // frequency wave, even though in practice it's the other way round, but
    // this way it's easier to put it all into a single picture.
    //
    // The original assembly code goes through a few gyrations to encode all
    // these possible cases into a bitmask and then does a single switch.
    // In practice, for all but very high-frequency waves, we're hitting the
    // "easy" cases a) and c) almost all the time.
    COVER("Osc tri/saw");

    // FREQ_CONST: at v3/v4 the analytic OSM renderer is present (BOXFILTER new
    // at v3) but the freq is still the baked 4x-oversample convention (FREQ_CONST
    // old) -- fr019's OSM tri/saw advances freq<<2 (`shl esi,2` @0x427fd9), like
    // renderSin_v0's step. Scale the phase freq for this render (osm_init/osm_tick
    // + utof23(freq) all read the member); restore after. v5/v6 advance 1x.
    sInt freq_save = freq;
    if (inst->old(DELTA_OSC_FREQ_CONST)) freq <<= 2;

    // calc helper values.
    // PORTING NOTE: the "hard" cases below (b/d/e/f) have a catastrophic
    // cancellation amplified by rcpf=1/f. The asm computes this on the x87 at
    // 24-bit mantissa / 15-bit exponent. `trisaw_flt` is `float` in the faithful
    // build (x87 PC=24 reproduces the asm exactly) and `double` in the portable
    // build (SSE single's 8-bit exponent would overflow at low frequencies).
    trisaw_flt f = utof23(freq);
    trisaw_flt omf = 1.0 - f;
    trisaw_flt rcpf = 1.0 / f;
    trisaw_flt col = utof23(brpt);

    // m1 = 2/col = slope of saw-up wave
    // m2 = -2/(1-col) = slope of saw-down wave
    // c1 = gain/2*m1 = gain/col = scaled integration constant
    // c2 = gain/2*m2 = -gain/(1-col) = scaled integration constant
    trisaw_flt c1 = gain / col;
    trisaw_flt c2 = -gain / (1.0 - col);

    sU32 state = osm_init();

    for (sInt i=0; i < nsamples; i++)
    {
      trisaw_flt p = utof23(cnt) - col;
      trisaw_flt y = 0.0;

      // state machine action
      switch (osm_tick(state))
      {
      case OSMTC_UP: // case a)
        // average of linear function = just sample in the middle
        y = c1 * (p + p - f);
        break;

      case OSMTC_DOWN: // case c)
        // again, average of a linear function
        y = c2 * (p + p - f);
        break;
        
      case OSMTC_UP_DOWN: // case b)
        y = rcpf * (c2 * (p*p) - c1 * ((p-f)*(p-f))); // trisaw_flt, not float sqr()
        break;

      case OSMTC_DOWN_UP: // case d)
        // asm m0c21: y = -((c2*(p+1-f)^2 - c1*p^2) + g) * (1/f). The asm forms
        // the square base as (p+1)-f INLINE (fld1/fadd/fsub), not p+(1-f); at
        // PC=24 those round differently. Also the 3-term sum is (c2.. - c1..)+g,
        // not g + c2.. - c1.. . Match the asm op order exactly (1-ULP fidelity).
        {
          trisaw_flt t = p + (trisaw_flt)1.0 - f; // (p+1) - f, in the asm association
          y = -((c2*(t*t) - c1*(p*p)) + gain) * rcpf;
        }
        break;

      case OSMTC_UP_DOWN_UP: // case e)
        // asm m0c121: y = -((c1*(omf*(2p+omf))) + g) * (1/f). The asm multiplies
        // omf*(2p+omf) FIRST, then by c1 (c1*(omf*X)), not (c1*omf)*X.
        y = -((c1*(omf*(p + p + omf))) + gain) * rcpf;
        break;

      case OSMTC_DOWN_UP_DOWN: // case f)
        // asm m0c212: same as case e with c2: y = -((c2*(omf*(2p+omf))) + g)*(1/f).
        y = -((c2*(omf*(p + p + omf))) + gain) * rcpf;
        break;

      // INVALID CASES
      default:
        assert(false);
        break;
      }

      output(dest + i, y + gain);
    }
    freq = freq_save;
  }

  void renderPulse(sF32 *dest, sInt nsamples)
  {
    // This follows the same general pattern as renderTriSaw above, except
    // this time the waveform is a pulse wave with variable pulse width,
    // which means we get very simple integrals. The state machine works
    // the exact same way, see above for description.
    COVER("Osc pulse");

    // FREQ_CONST: v3/v4 advance the OSM pulse at freq<<2 (fr019 @0x428111 does
    // `shl esi,2`); v5/v6 advance 1x. Mirror renderTriSaw -- see the note there.
    sInt freq_save = freq;
    if (inst->old(DELTA_OSC_FREQ_CONST)) freq <<= 2;

    // calc helper values
    sF32 f = utof23(freq);
    sF32 gdf = gain / f;
    sF32 col = utof23(brpt);

    sF32 cc121 = gdf * 2.0f * (col - 1.0f) + gain;
    sF32 cc212 = gdf * 2.0f * col - gain;

    sU32 state = osm_init();

    for (sInt i=0; i < nsamples; i++)
    {
      sF32 p = utof23(cnt);
      sF32 out = 0.0f;

      switch (osm_tick(state))
      {
      case OSMTC_UP:
        out = gain;
        break;

      case OSMTC_DOWN:
        out = -gain;
        break;

      case OSMTC_UP_DOWN:
        out = gdf * 2.0f * (col - p) + gain;
        break;

      case OSMTC_DOWN_UP:
        out = gdf * 2.0f * p - gain;
        break;

      case OSMTC_UP_DOWN_UP:
        out = cc121;
        break;

      case OSMTC_DOWN_UP_DOWN:
        out = cc212;
        break;

      // INVALID CASES
      default:
        assert(false);
        break;
      }

      output(dest + i, out);
    }
    freq = freq_save;
  }

  void renderSin(sF32 *dest, sInt nsamples)
  {
    COVER("Osc sin");

    // Sine is already a perfectly bandlimited waveform, so we needn't
    // worry about aliasing here.
    for (sInt i=0; i < nsamples; i++)
    {
      // Brace yourselves: The name is a lie! It's actually a cosine wave!
      sU32 phase = cnt + 0x40000000; // quarter-turn (pi/2) phase offset
      cnt += freq; // step the oscillator

      // range reduce to [0,pi]
      if (phase & 0x80000000) // Symmetry: cos(x) = cos(-x)
        phase = ~phase; // V2 uses ~ not - which is slightly off but who cares

      // convert to t in [1,2)
      sF32 t = bits2float((phase >> 8) | 0x3f800000); // 1.0f + (phase / (2^31))

      // and then to t in [-pi/2,pi/2)
      // i know the V2 ASM code says "scale/move to (-pi/4 .. pi/4)".
      // trust me, it's lying.
      t = t * fcpi - fc1p5pi;

      output(dest + i, gain * fastsin(t));
    }
  }

  void renderNoise(sF32 *dest, sInt nsamples)
  {
    COVER("Osc noise");

    V2LRC flt = nf;
    sU32 seed = nseed;

    for (sInt i=0; i < nsamples; i++)
    {
      // uniform random value (noise)
      sF32 n = frandom(&seed);

      // filter
      sF32 h = flt.step(n, nffrq, nfres);
      sF32 x = nfres*(flt.l + h) + flt.b;

      output(dest + i, gain * x);
    }

    // Persist the working filter state + seed back to the members. The asm
    // (syOsc .mode3, synth.asm:964-968) stores nfb/nfl/nseed every block so the
    // resonant noise filter rings continuously. The original port wrote `flt =
    // nf` here (backwards) — nf never updated, so the LRC restarted from l=b=0
    // each 1024-sample block, decorrelating the filtered noise from the asm.
    nf = flt;
    nseed = seed;
  }

  void renderFMSin(sF32 *dest, sInt nsamples)
  {
    COVER("Osc FM");

    // V2's take on FM is a bit unconventional but fairly slick and flexible.
    // The carrier wave is always a sine, but the modulator is whatever happens
    // to be in the voice buffer at that point - which is the output of the
    // previous oscillators. So you can use all the oscillator waveforms
    // (including noise, other FMs and the aux bus oscillators!) as modulator
    // if you are so inclined.
    //
    // And it's very little code too :)
    for (sInt i=0; i < nsamples; i++)
    {
      sF32 mod = dest[i] * fcfmmax;
      // The asm (syOsc .mode4, synth.asm:981-986) builds the phase float as
      // (cnt>>9)|0x3f800000 -- a value in [1,2) -- and does NOT subtract 1.0 the
      // way utof23() does. For a real sine the extra 1.0 (= 2pi after the *fc2pi
      // scale) would be irrelevant, but V2's fastsinrc range reduction is
      // deliberately broken (see its @@@BUG), so fastsinrc(t) != fastsinrc(t+2pi):
      // with a negative modulator the offset moves t across the pi/2 / 3pi/2
      // reflection boundaries and the minimax poly is evaluated out of range.
      // Match the asm's [1,2) phase (and its `mod + phase` operand order). This
      // path is FM-sine only (ch7) -- never exercised before 17.8s.
      sF32 t = (mod + bits2float((cnt >> 9) | 0x3f800000)) * fc2pi;
      cnt += freq;

      sF32 out = gain * fastsinrc(t);
      if (ring)
        dest[i] *= out;
      else
        dest[i] = out;
    }
  }

  void renderFMSin_v5(sF32 *dest, sInt nsamples)
  {
    COVER("Osc v5 FM");

    // candytron-era FM (genthree _viruz2a.asm .mode4, binary @~0x41e07x; same
    // numbering as 2004 but a different scheme): the modulator is scaled by
    // fcfmmax*fc32bit and CONVERTED TO AN INTEGER with fistp, then added to a
    // copy of the phase counter with 32-bit wraparound; the carrier is the
    // native-fsin sine on the [1,2)*2pi phase (same evaluation as
    // renderSin_v0). 2004 instead keeps the modulator in float and feeds
    // fastsinrc. Non-ring REPLACES the buffer (asm stores without fadd),
    // exactly like the 2004 FM.
    // FREQ_CONST: v4 advances the FM carrier at freq<<2 (fr019 @0x4282a7 does
    // `shl edx,2; add eax,edx`), exactly like renderSin_v0 / tri-saw / pulse;
    // v5/v6 advance 1x. (The modulator add uses the un-advanced cnt either way.)
    const sInt cstep = inst->old(DELTA_OSC_FREQ_CONST) ? (freq << 2) : freq;
    // The integer-scheme FM (v4 AND v5) scales the modulator by 4.0, NOT the 2.0
    // fcfmmax -- proven from BOTH binaries: fr019 @0x427e7c = 4.0 and candytron
    // @0x41dbd0 = 4.0. (fcfmmax=2.0 is the 2004 synth.asm value @line 85, but the
    // v6 renderFMSin uses a different FLOAT scheme; the 2.0 was wrongly reused
    // here.) With 2.0 the FM timbre is wrong (magnitude-spectrum corr 0.87); with
    // 4.0 it matches (0.998). The carrier freq<<2 stays v4-only (candytron's FM
    // @0x41e060 has no `shl edx,2`).
    const sF32 fmdepth = 4.0f;
    for (sInt i=0; i < nsamples; i++)
    {
      sInt modi = v2_fistp(dest[i] * fmdepth * fc32bit);
      sF32 p = v0_cnt2f(cnt + (sU32)modi);
      cnt += cstep;

      sF32 out = gain * v2_sin(p * fc2pi);
      if (ring)
        dest[i] *= out;
      else
        dest[i] = out;
    }
  }

  void renderAux(sF32 *dest, const StereoSample *src, sInt nsamples)
  {
    COVER("Osc aux");

    // PORTING FIX (not an ASM bug): the asm (.auxa/.auxb, synth.asm) computes
    // ((l+r) * gain) * fcgain per sample; pre-folding gain*fcgain rounds
    // differently (1 ULP on a fraction of samples). The aux oscs read the
    // cross-channel feedback busses, so the noise only appears in full-mix
    // context — invisible to leaf tests and channel-solo bisection
    // (debris_ost residual).
    for (sInt i=0; i < nsamples; i++)
    {
      sF32 aux = (src[i].l + src[i].r) * gain * fcgain;
      if (ring)
        aux *= dest[i];
      dest[i] = aux;
    }
  }
};

// --------------------------------------------------------------------------
// Envelope Generator
// --------------------------------------------------------------------------

struct syVEnv
{
  sF32 ar;  // attack rate
  sF32 dr;  // decay rate
  sF32 sl;  // sustain level
  sF32 sr;  // sustain rate
  sF32 rr;  // release rate
  sF32 vol; // volume
};

struct V2Env
{
  // Slightly different state ordering here than in V2 code, to make
  // things simpler.
  enum State
  {
    OFF,
    RELEASE,

    ATTACK,
    DECAY,
    SUSTAIN,
  };

  sF32 out;
  State state;
  sF32 val;   // output value (0.0-128.0)
  sF32 atd;   // attack delta (added every frame in stAttack, transition ->stDecay at 128.0)
  sF32 dcf;   // decay factor (mul'd every frame in stDecay, transition ->stSustain at sul)
  sF32 sul;   // sustain level (defines stDecay->stSustain transition point)
  sF32 suf;   // sustain factor (mul'd every frame in stSustain, transition ->stRelease at gate off or ->stOff at 0.0)
  sF32 ref;   // release factor (mul'd every frame in stRelease, transition ->stOff at 0.0)
  sF32 gain;

  V2Instance *inst;

  void init(V2Instance *instance)
  {
    state = OFF;
    inst = instance;
  }

  void set(const syVEnv *para)
  {
    if (inst->old(DELTA_ENV_CURVES))
    {
      // era <v2 (fr08): attack = 2^(7 - ar*11/128), and decay/release go
      // through calcfreq (x10 range) -- calcfreq2 (x11) didn't exist yet.
      // This is exactly the delta kb's disabled transEnv (v2mconv.cpp:243)
      // tries to approximate on the data side; gating the code is bit-exact.
      // Evidence: 2000 syEnvSet @0x40a6d1 (fr08-extraction/DELTA.md).
      atd = v2_exp2(para->ar * fcattackmul_v0 + fcattackadd);
      dcf = 1.0f - calcfreq(1.0f - para->dr / 128.0f);
      ref = 1.0f - calcfreq(1.0f - para->rr / 128.0f);
    }
    else
    {
      // ar: 2^7 (128) to 2^-4 (0.03, ca. 10 secs at 344frames/sec)
      atd = v2_exp2(para->ar * fcattackmul + fcattackadd);

      // dcf: 0 (5msecs thanks to volramping) up to almost 1
      dcf = 1.0f - calcfreq2(1.0f - para->dr / 128.0f);

      // ref: 0 (5ms thanks to volramping) up to almost 1
      ref = 1.0f - calcfreq2(1.0f - para->rr / 128.0f);
    }

    // sul: 0..127 is fine already
    sul = para->sl;

    // suf: 1/128 (15ms till it's gone) up to 128 (15ms till it's fully there)
    suf = v2_exp2(fcsusmul * (para->sr - 64.0f));

    gain = para->vol / 128.0f;
  }

  void tick(bool gate)
  {
    // process immediate gate transition
    if (state <= RELEASE && gate) // gate on
      state = ATTACK;
    else if (state >= ATTACK && !gate) // gate off
      state = RELEASE;

    // process current state
    switch (state)
    {
    case OFF:
      COVER("EG off");
      val = 0.0f;
      break;

    case ATTACK:
      COVER("EG atk");
      val += atd;
      if (val >= 128.0f)
      {
        val = 128.0f;
        state = DECAY;
      }
      break;

    case DECAY:
      COVER("EG dcy");
      val *= dcf;
      if (val <= sul)
      {
        val = sul;
        state = SUSTAIN;
      }
      break;

    case SUSTAIN:
      COVER("EG sus");
      val *= suf;
      if (val > 128.0f)
        val = 128.0f;
      break;

    case RELEASE:
      COVER("EG rel");
      val *= ref;
      break;
    }

    // avoid underflow to denormals
    // era <v1 (fr08): the 2000 tick @0x40a759 has the LOWEST->OFF runout check
    // ONLY in its SUSTAIN and RELEASE handlers; the 2004 `.s4checkrunout` jump
    // from DECAY (synth.asm:1178) did not exist yet. So a gate-held env whose
    // sustain level is below 2^-13 NEVER leaves DECAY in the 2000 -- val keeps
    // decaying into denormals and out stays (tiny) nonzero. Audible when such
    // an env drives a modmatrix dest: fr08 ch2 (pgm 3) mods env2->osc1/osc2
    // pitch; the residual eps shifts the integer osc freq by ~2 -> slow phase
    // drift vs a port that clamped env2 to exact 0. (ATTACK is unreachable
    // here either way: atd >= 2^-4 > fclowest.) Gate: skip the clamp in
    // DECAY/ATTACK under eraV0. (DELTA.md "syEnvTick clamp placement")
    if (val <= fclowest
        && !(inst->old(DELTA_ENV_CLAMP_SUSREL) && (state == DECAY || state == ATTACK)))
    {
      val = 0.0f;
      state = OFF;
    }

    out = val * gain;
    DEBUG_PLOT_VAL(this, out / 128.0f);
  }
};

// --------------------------------------------------------------------------
// Filter
// --------------------------------------------------------------------------

struct syVFlt
{
  sF32 mode;
  sF32 cutoff;
  sF32 reso;
};

struct V2Flt
{
  enum Mode
  {
    BYPASS,
    LOW,
    BAND,
    HIGH,
    NOTCH,
    ALL,
    MOOGL,
    MOOGH
  };

  sInt mode;
  sF32 cfreq;
  sF32 res;
  sF32 moogf, moogp, moogq; // moog filter coeffs
  V2LRC lrc;
  V2Moog moog;

  V2Instance *inst;

  void init(V2Instance *instance)
  {
    lrc.init();
    moog.init();
    inst = instance;
  }

  void set(const syVFlt *para)
  {
    mode = (sInt)para->mode;
    sF32 f = calcfreq(para->cutoff / 128.0f) * inst->SRfclinfreq;
    sF32 r = para->reso / 128.0f;

    if (mode < MOOGL)
    {
      COVER("VCF set regular");
      res = 1.0f - r;
      cfreq = f;
    }
    else
    {
      COVER("VCF set moog");

      // @@@BUG? V2 code for this part looks suspicious.
      // Match syFltSet's moog operand order EXACTLY: the moog is a 4-pole
      // recursive filter, so a 1-ULP coeff error drifts/amplifies over the song.
      // 0.8/5.6 as static const sF32 -> loaded 32-bit like the asm fc0p8/fc5p6
      // (a bare literal in this live x87 expression promotes to 80-bit, != asm).
      static const sF32 fc0p8 = 0.8f, fc5p6 = 5.6f;
      f *= 0.25f;
      sF32 t = 1.0f - f;

      moogp = f + fc0p8 * (f * t);              // asm: f + 0.8*(t*f)
      moogf = 1.0f - (moogp + moogp);           // asm: 1 - (p+p), not (1-p)-p
      sF32 qx = fc5p6 * (t * t) + (1.0f - t);   // asm: 5.6*t^2 + (1-t)
      sF32 qb = r * (1.0f + 0.5f * (t * qx));   // asm: r' * (1 + 0.5*(t*qx))
      sF32 q2 = qb + qb;                        // asm: *2
      moogq = q2 + q2;                          // asm: *2 again (= *4)
    }
  }

  void render(sF32 *dest, const sF32 *src, sInt nsamples, sInt step=1)
  {
    V2LRC flt;
    V2Moog m;
    // era <v1 (fr08): the 2000 SVF render injects no denormal DC bias.
    const flcalc dco = inst->old(DELTA_NO_DCOFFSET) ? (flcalc)0 : (flcalc)fcdcoffset;

    sInt m7 = mode & 7;
    // era: VCF modes 6/7 (MoogL/H) post-date the 2000 engine -- alias to
    // passthrough (DELTA_NO_MOOG). Unreachable from genuine period data (no
    // period authoring path emits 6/7); gated for ledger completeness and the
    // forceBehaviorVersion research knob.
    if (m7 >= MOOGL && inst->old(DELTA_NO_MOOG))
    {
      if (dest != src)
        for (sInt i=0; i < nsamples; i++)
          dest[i*step] = src[i*step];
      DEBUG_PLOT_STRIDED(this, dest, step, nsamples);
      return;
    }

    switch (m7)
    {
    case BYPASS:
      COVER("VCF bypass");
      // @@@BUG ignores step? this is wrong but I suppose this
      // never gets hit for stereo case.
      if (dest != src)
        memmove(dest, src, nsamples * sizeof(sF32));
      break;

    case LOW:
      COVER("VCF low");
      { flcalc l = lrc.l, b = lrc.b;
        for (sInt i=0; i < nsamples; i++)
        {
          lrc_step_2x(l, b, src[i*step], cfreq, res, dco);
          dest[i*step] = (sF32)l;
        }
        lrc.l = (sF32)l; lrc.b = (sF32)b; }
      break;

    case BAND:
      COVER("VCF band");
      { flcalc l = lrc.l, b = lrc.b;
        for (sInt i=0; i < nsamples; i++)
        {
          lrc_step_2x(l, b, src[i*step], cfreq, res, dco);
          dest[i*step] = (sF32)b;
        }
        lrc.l = (sF32)l; lrc.b = (sF32)b; }
      break;

    case HIGH:
      COVER("VCF high");
      { flcalc l = lrc.l, b = lrc.b;
        for (sInt i=0; i < nsamples; i++)
        {
          flcalc h = lrc_step_2x(l, b, src[i*step], cfreq, res, dco);
          dest[i*step] = (sF32)h;
        }
        lrc.l = (sF32)l; lrc.b = (sF32)b; }
      break;

    case NOTCH:
      COVER("VCF notch");
      { flcalc l = lrc.l, b = lrc.b;
        for (sInt i=0; i < nsamples; i++)
        {
          flcalc h = lrc_step_2x(l, b, src[i*step], cfreq, res, dco);
          dest[i*step] = (sF32)(l + h);
        }
        lrc.l = (sF32)l; lrc.b = (sF32)b; }
      break;

    case ALL:
      COVER("VCF all");
      // PORTING FIX (not an ASM bug): the asm (.mode5, synth.asm) sums the
      // allpass output as (h + b) + l; (l + b) + h rounds differently for ~a
      // ULP on some samples. The allpass output never feeds the l/b state, so
      // the filter's own ledger state stays exact — the 1-ULP output noise
      // only surfaces in DOWNSTREAM stateful blocks (kkrieger6: voice ALL vcf
      // -> channel dist's embedded allpass accumulated it).
      { flcalc l = lrc.l, b = lrc.b;
        for (sInt i=0; i < nsamples; i++)
        {
          flcalc h = lrc_step_2x(l, b, src[i*step], cfreq, res, dco);
          dest[i*step] = (sF32)(h + b + l);
        }
        lrc.l = (sF32)l; lrc.b = (sF32)b; }
      break;

    case MOOGL:
      COVER("VCF moog low");
      // Moog filters are 2x oversampled, so run filter twice.
      m = moog;
      for (sInt i=0; i < nsamples; i++)
      {
        sF32 in = src[i*step];
        m.step(in, moogf, moogp, moogq);
        dest[i*step] = m.step(in, moogf, moogp, moogq);
      }
      moog = m;
      break;

    case MOOGH:
      COVER("VCF moog high");
      m = moog;
      for (sInt i=0; i < nsamples; i++)
      {
        sF32 in = src[i*step];
        m.step(in, moogf, moogp, moogq);
        dest[i*step] = in - m.step(in, moogf, moogp, moogq);
      }
      moog = m;
      break;
    }


    DEBUG_PLOT_STRIDED(this, dest, step, nsamples);
  }
};

// --------------------------------------------------------------------------
// Low Frequency Oscillator
// --------------------------------------------------------------------------

struct syVLFO
{
  sF32 mode;    // 0=saw, 1=tri, 2=pulse, 3=sin, 4=s&h
  sF32 sync;    // 0=free, 1=in sync with keyon
  sF32 egmode;  // 0=continuous 1=one-shot (EG mode)
  sF32 rate;    // rate (0Hz..~43Hz)
  sF32 phase;   // start phase shift
  sF32 pol;     // polarity: +, -, +/-
  sF32 amp;     // amplification (0..1)
};

struct V2LFO
{
  enum Mode
  {
    SAW,
    TRI,
    PULSE,
    SIN,
    S_H
  };

  sF32 out;
  sInt mode;    // mode
  bool sync;    // sync mode
  bool eg;      // envelope generator mode
  sInt freq;    // frequency
  sU32 cntr;    // counter
  sU32 cphase;  // counter sync phase
  sF32 gain;    // output gain
  sF32 dc;      // output dc
  sU32 nseed;   // random seed
  sU32 last;    // last counter value (for s&h transition)
  V2Instance *inst;

  void init(V2Instance *instance)
  {
    cntr = last = 0;
    inst = instance;
    // pre-v6 (fr08 AND candytron v5): LFO S&H seed is rdtsc (== 0 under the
    // pinned-rdtsc convention). v6 switched to the libc-rand sequence.
    // (Re-seeded by the era setter for the synth-init-time voices, as
    // srcVersion isn't set yet at first init.)
    if (instance->old(DELTA_RDTSC_SEED)) { nseed = 0u; return; }
    // deterministic replacement for the original's libc rand() (the lab
    // matched the asm only because both sides shared one glibc sequence);
    // V2Rand replicates that sequence portably (design D6).
    nseed = seedMix(instance->userSeed, instance->rng.next(), 1);
  }

  void set(const syVLFO *para)
  {
    mode = (sInt)para->mode;
    sync = (sInt)para->sync != 0;
    eg = (sInt)para->egmode != 0;
    // asm syLFOSet (synth.asm): freq stored with fistp (round-to-nearest), NOT a
    // truncating cast. fci128=0.0078125 (=1/128, exact), order matches the asm
    // (calcfreq * fc32bit * 0.5). The truncating (sInt) below is off-by-one
    // whenever the frac >= 0.5, which slowly desyncs the integer phase counter
    // and (via LFO->amp-env modulation) drifts curvol over the whole song.
    // era <v1 (fr08): syLFOSet @0x40a965 uses calcfreq*2^31 (no *0.5). The 0.5
    // compensates for the modern 128-sample control frame; under the period
    // 256-frame it must be dropped or the LFO runs at half speed (modulating
    // filter cutoff/pitch/amp -> diverges). per-sample rate stays invariant.
    sF32 lfomul = inst->old(DELTA_FRAME256) ? fc32bit : (fc32bit * 0.5f);
    freq = v2_fistp(calcfreq(para->rate * 0.0078125f) * lfomul);
    cphase = ftou32(para->phase / 128.0f);

    switch ((sInt)para->pol)
    {
    case 0: // +
      COVER("LFO pol +");
      gain = para->amp;
      dc = 0.0f;
      break;

    case 1: // -
      COVER("LFO pol -");
      gain = -para->amp;
      dc = 0.0f;
      break;

    case 2: // +/-
      COVER("LFO pol +/-");
      gain = para->amp;
      dc = -0.5f * para->amp;
      break;
    }
  }

  void keyOn()
  {
    if (sync)
    {
      COVER("LFO sync");
      cntr = cphase;
      last = ~0u;
    }
  }

  void tick()
  {
    sF32 v;
    sU32 x;

    switch (mode & 7)
    {
    case SAW:
    default:
      COVER("LFO saw");
      v = utof23(cntr);
      break;

    case TRI:
      COVER("LFO tri");
      x = (cntr << 1) ^ (sS32(cntr) >> 31);
      v = utof23(x);
      break;

    case PULSE:
      COVER("LFO pulse");
      x = sS32(cntr) >> 31;
      v = utof23(x);
      break;

    case SIN:
      COVER("LFO sin");
      v = utof23(cntr);
      // era <v1 (fr08): native fsin (syLFO sine @0x40aa28), not the fastsinrc
      // polynomial (same delta as the osc sine; DELTA.md delta 3).
      v = (inst->old(DELTA_NATIVE_FSIN) ? v2_sin(v * fc2pi)
                                        : fastsinrc(v * fc2pi)) * 0.5f + 0.5f;
      break;

    case S_H:
      COVER("LFO sample+hold");
      if (inst->old(DELTA_NOISE_LCG_MSVC))
      {
        // era <v1: S&H uses the MSVC LCG (214013/2531011) and the low-16-bit
        // value extraction (nseed&0xffff)<<16 (syLFO S&H @0x40aa37), not the
        // 2004 urandom + full-nseed.
        if (cntr < last)
          nseed = nseed * 214013 + 2531011;
        last = cntr;
        v = utof23((nseed & 0xffff) << 16);
      }
      else
      {
        if (cntr < last)
          nseed = urandom(&nseed);
        last = cntr;
        v = utof23(nseed);
      }
      break;
    }

    out = v * gain + dc;
    cntr += freq;
    if (cntr < (sU32)freq && eg) // in one-shot mode, clamp at wrap-around
      cntr = ~0u;

    DEBUG_PLOT_VAL(this, out / 128.0f);
  }
};

// --------------------------------------------------------------------------
// Distortion
// --------------------------------------------------------------------------

struct syVDist
{
  sF32 mode;    // see below
  sF32 ingain;  // -12dB .. 36dB
  sF32 param1;  // outgain / crush / outfreq / cutoff
  sF32 param2;  // offset / xor / jitter / reso
};

struct V2Dist
{
  enum Mode
  {
    OFF = 0,
    OVERDRIVE,
    CLIP,
    BITCRUSHER,
    DECIMATOR,

    FLT_BASE  = DECIMATOR,
    FLT_LOW   = FLT_BASE + V2Flt::LOW,
    FLT_BAND  = FLT_BASE + V2Flt::BAND,
    FLT_HIGH  = FLT_BASE + V2Flt::HIGH,
    FLT_NOTCH = FLT_BASE + V2Flt::NOTCH,
    FLT_ALL   = FLT_BASE + V2Flt::ALL,
    FLT_MOOGL = FLT_BASE + V2Flt::MOOGL,
    FLT_MOOGH = FLT_BASE + V2Flt::MOOGH,
  };

  sInt mode;
  sF32 gain1;     // input gain for all fx
  sF32 gain2;     // output gain for od/clip
  sF32 offs;      // offs for od/clip
  sF32 crush1;    // 1/crush_factor
  sInt crush2;    // crush_factor
  sInt crxor;     // xor value for crush
  sU32 dcount;    // decimator counter
  sU32 dfreq;     // decimator frequency
  sF32 dvall;     // last decimator value (mono/left)
  sF32 dvalr;     // last decimator value (mono/right)
  V2Flt fltl;     // filter mono/left
  V2Flt fltr;     // filter right
  V2Instance *inst;

  void init(V2Instance *instance)
  {
    dcount = 0;
    dvall = dvalr = 0.0f;
    fltl.init(instance);
    fltr.init(instance);
    inst = instance;
  }

  void set(const syVDist *para)
  {
    sF32 x;

    mode = (sInt)para->mode;
    gain1 = v2_exp2((para->ingain - 32.0f) / 16.0f);

    switch (mode)
    {
    case OFF:
      break;

    case OVERDRIVE:
      // /128.0f is a power of two (exact), same value as the asm's fmul fci128.
      gain2 = v2_overdrive_gain2(para->param1 / 128.0f, gain1);
      offs = gain1 * 2.0f * ((para->param2 / 128.0f) - 0.5f);
      break;

    case CLIP:
      gain2 = para->param1 / 128.0f;
      offs = gain1 * 2.0f * ((para->param2 / 128.0f) - 0.5f);
      break;

    case BITCRUSHER:
      x = para->param1 * 256.0f + 1.0f;
      crush2 = (sInt)x;
      // era <v1 (fr08): the 2000 crusher set (@0x40ab0b) stores crush1 =
      // 32768/x WITHOUT gain1 -- gain1 is a separate per-sample multiply in
      // its render (@0x40abef: fmul gain1; fmul crush1). 2004 folds gain1
      // into crush1 at set time (synth.asm:1858-1860; one render multiply).
      // Same product, different association: near a quantizer tie the 1-ULP
      // difference flips the fistp (fr08 ch1 @271.64s, t=-1 vs -2).
      if (inst->old(DELTA_CRUSHER_SPLIT_GAIN1))
        crush1 = 32768.0f / x;
      else
        crush1 = gain1 * (32768.0f / x);
      crxor = ((sInt)para->param2) << 9;
      break;

    case DECIMATOR:
      dfreq = ftou32(calcfreq(para->param1 / 128.0f));
      break;

    default: // filters
      {
        syVFlt setup;
        setup.cutoff = para->param1;
        setup.reso = para->param2;
        setup.mode = (sF32)(mode - FLT_BASE);
        fltl.set(&setup);
        fltr.set(&setup);
      }
      break;
    }
  }

  void renderMono(sF32 *dest, const sF32 *src, sInt nsamples)
  {
    switch (mode)
    {
    case OFF:
      if (dest != src)
        memmove(dest, src, nsamples * sizeof(sF32));
      break;

    case OVERDRIVE:
      COVER("DIST overdrive");
      for (sInt i=0; i < nsamples; i++)
        dest[i] = overdrive(src[i]);
      break;

    case CLIP:
      COVER("DIST clip");
      for (sInt i=0; i < nsamples; i++)
        dest[i] = clip(src[i]);
      break;

    case BITCRUSHER:
      COVER("DIST bitcrusher");
      for (sInt i=0; i < nsamples; i++)
        dest[i] = bitcrusher(src[i]);
      break;

    case DECIMATOR:
      COVER("DIST mono decimator");
      for (sInt i=0; i < nsamples; i++)
      {
        decimator_tick(src[i], 0.0f);
        dest[i] = dvall;
      }
      break;

    default: // filters
      COVER("DIST mono filter");
      fltl.render(dest, src, nsamples);
      break;
    }

    DEBUG_PLOT(this, dest, nsamples);
  }

  void renderStereo(StereoSample *dest, const StereoSample *src, sInt nsamples)
  {
    // @@@BUG this matches the original V2 code, but frankly I have my doubts
    // that always running the Moog filters in Mono mode is intentional... 
    switch (mode)
    {
    case DECIMATOR:
      COVER("DIST stereo decimator");
      for (sInt i=0; i < nsamples; i++)
      {
        decimator_tick(src[i].l, src[i].r);
        dest[i].l = dvall;
        dest[i].r = dvalr;
      }
      break;

    case FLT_LOW:
    case FLT_BAND:
    case FLT_HIGH:
    case FLT_NOTCH:
    case FLT_ALL:
      COVER("DIST stereo filter");
      fltl.render(&dest[0].l, &src[0].l, nsamples, 2);
      fltr.render(&dest[0].r, &src[0].r, nsamples, 2);
      break;

    default:
      // everything else we presume to be stateless and just pass through the
      // mono version.
      renderMono(&dest[0].l, &src[0].l, nsamples*2);
    }

    DEBUG_PLOT_STEREO(this, dest, nsamples);
  }

private:
  inline sF32 overdrive(sF32 in)
  {
    // era <v1 (fr08): native x87 atan (fpatan), not the fastatan polynomial
    // (syDistRenderMono @0x40ab88; DELTA.md delta -- fastatan is post-2000).
    if (inst->old(DELTA_NATIVE_FPATAN))
      return gain2 * v2_atanf(in * gain1 + offs);
    return gain2 * fastatan(in * gain1 + offs);
  }

  inline sF32 clip(sF32 in)
  {
    return gain2 * clamp(in * gain1 + offs, -1.0f, 1.0f);
  }

  inline sF32 bitcrusher(sF32 in)
  {
    // era <v1 (fr08): (in*gain1)*crush1 -- two separate 24-bit-rounded
    // multiplies like the 2000 render; crush1 excludes gain1 there (see set).
    sF32 scaled = inst->old(DELTA_CRUSHER_SPLIT_GAIN1) ? (in * gain1 * crush1)
                                                       : (in * crush1);
    sInt t = (sInt)lrintf(scaled); // ASM uses fistp (round-to-nearest), not truncation
    t = clamp(t * crush2, -0x7fff, 0x7fff) ^ crxor;
    return (sF32)t / 32768.0f;
  }

  inline void decimator_tick(sF32 l, sF32 r)
  {
    dcount += dfreq;
    if (dcount < dfreq) // carry
    {
      dvall = l;
      dvalr = r;
    }
  }
};

// --------------------------------------------------------------------------
// DC filter
// --------------------------------------------------------------------------

// This is just a high-pass with very low cut-off to remove DC offsets from
// the signal.
struct V2DCFilter
{
  V2DCF fl;         // left/mono filter state
  V2DCF fr;         // right filter state
  V2Instance *inst;

  void init(V2Instance *instance)
  {
    inst = instance;
    fl.init();
    fr.init();
  }

  void renderMono(sF32 *dest, const sF32 *src, sInt nsamples)
  {
    sF32 R = inst->SRfcdcfilter;

    V2DCF l = fl;
    for (sInt i=0; i < nsamples; i++)
      dest[i] = l.step(src[i], R);
    fl = l;
  }

  void renderStereo(StereoSample *dest, const StereoSample *src, sInt nsamples)
  {
    sF32 R = inst->SRfcdcfilter;

    V2DCF l = fl;
    V2DCF r = fr;
    for (sInt i=0; i < nsamples; i++)
    {
      dest[i].l = l.step(src[i].l, R);
      dest[i].r = r.step(src[i].r, R);
    }
    fl = l;
    fr = r;
  }
};

// --------------------------------------------------------------------------
// V2 Voice
// --------------------------------------------------------------------------

struct syVV2
{
  // Voice parameters.
  // changing any of these *will* invalidate all V2M files!
  static const sInt NOSC = 3; // number of oscillators
  static const sInt NFLT = 2; // number of filters (NB: changing this would complicate routing too)
  static const sInt NENV = 2; // number of envelope generators
  static const sInt NLFO = 2; // number of LFOs

  sF32 panning;
  sF32 transp;   // transpose
  syVOsc osc[NOSC];
  syVFlt flt[NFLT];
  sF32 routing; // 0: single 1: serial 2: parallel
  sF32 fltbal;  // parallel filter balance
  syVDist dist;
  syVEnv env[NENV];
  syVLFO lfo[NLFO];
  sF32 oscsync; // 0: none 1: osc 2: full
};

#ifndef NDEBUG
// dev-only: report per-stage voice-chain peaks (V2_VCETAP set). Use under a
// channel solo so only the target channel's voices render. (osc/flt/dist/dcf)
static inline void vcetap_snap(const char *stage, const sF32 *buf, sInt n)
{
  static int on = -1; if (on < 0) { on = getenv("V2_VCETAP") ? 1 : 0; }
  if (!on) return;
  sF32 mx = 0.0f;
  for (sInt i = 0; i < n; i++) { sF32 a = buf[i] < 0 ? -buf[i] : buf[i]; if (a > mx) mx = a; }
  if (mx > 0.0f) fprintf(stderr, "[vce] %-4s %.5f\n", stage, mx);
}
#define VCETAP_SNAP(stage, buf, n) vcetap_snap(#stage, buf, n)
#else
#define VCETAP_SNAP(stage, buf, n) ((void)0)
#endif

struct V2Voice
{
  enum FilterRouting
  {
    FLTR_SINGLE = 0,
    FLTR_SERIAL,
    FLTR_PARALLEL
  };

  enum KeySync
  {
    SYNC_NONE = 0,
    SYNC_OSC,
    SYNC_FULL
  };

  sInt note;
  sF32 velo;
  bool gate;

  sF32 curvol;
  sF32 volramp;

  sF32 xpose;     // transpose
  sInt fmode;     // FLTR_*
  sF32 lvol;      // left volume
  sF32 rvol;      // right volume
  sF32 f1gain;    // filter 1 gain
  sF32 f2gain;    // filter 2 gain

  sInt keysync;

  V2Osc osc[syVV2::NOSC];
  V2Flt vcf[syVV2::NFLT];
  V2Env env[syVV2::NENV];
  V2LFO lfo[syVV2::NLFO];
  V2Dist dist;    // distorter
  V2DCFilter dcf; // post DC filter

  V2Instance *inst;

  void init(V2Instance *instance)
  {
    for (sInt i=0; i < syVV2::NOSC; i++)
      osc[i].init(instance, i);
    for (sInt i=0; i < syVV2::NFLT; i++)
      vcf[i].init(instance);
    for (sInt i=0; i < syVV2::NENV; i++)
      env[i].init(instance);
    for (sInt i=0; i < syVV2::NLFO; i++)
      lfo[i].init(instance);
    dist.init(instance);
    dcf.init(instance);
    inst = instance;
  }

  void tick()
  {
    for (sInt i=0; i < syVV2::NENV; i++)
      env[i].tick(gate);

    for (sInt i=0; i < syVV2::NLFO; i++)
      lfo[i].tick();


    // volume ramping slope
    volramp = (env[0].out / 128.0f - curvol) * inst->SRfciframe;
    DEBUG_PLOT_VAL(&curvol, curvol);
  }

  void render(StereoSample *dest, sInt nsamples)
  {
    assert(nsamples <= V2Instance::MAX_FRAME_SIZE);

    // clear voice buffer
    sF32 *voice = inst->vcebuf;
    sF32 *voice2 = inst->vcebuf2;
    memset(voice, 0, nsamples * sizeof(*voice));

    // oscillators -> voice buffer
    for (sInt i=0; i < syVV2::NOSC; i++)
      osc[i].render(voice, nsamples);
    VCETAP_SNAP(osc, voice, nsamples);
#ifndef NDEBUG
    { static int osh=0; if (getenv("V2_OSCONSET") && osh<2) {
        int any=0; for (sInt k=0;k<16 && k<nsamples;k++) if (voice[k]!=0.0f) any=1;
        if (any) { osh++;
          fprintf(stderr,"[oscbuf] first16:");
          for (sInt k=0;k<16 && k<nsamples;k++) fprintf(stderr," %.5f", voice[k]);
          fprintf(stderr,"\n"); } } }
#endif

    // voice buffer -> filters -> voice buffer
    switch (fmode)
    {
    case FLTR_SINGLE:
      COVER("VOICE filter single");
      vcf[0].render(voice, voice, nsamples);
      break;

    case FLTR_SERIAL:
    default:
      COVER("VOICE filter serial");
      vcf[0].render(voice, voice, nsamples);
      vcf[1].render(voice, voice, nsamples);
      break;

    case FLTR_PARALLEL:
      COVER("VOICE filter parallel");
      vcf[1].render(voice2, voice, nsamples);
      vcf[0].render(voice, voice, nsamples);
      for (sInt i=0; i < nsamples; i++)
        voice[i] = voice[i]*f1gain + voice2[i]*f2gain;
      break;
    }
    VCETAP_SNAP(flt, voice, nsamples);

    // voice buffer -> distortion -> voice buffer
    dist.renderMono(voice, voice, nsamples);
    VCETAP_SNAP(dist, voice, nsamples);

    // voice buffer -> dc filter -> voice buffer
    // era <v1 (fr08): syV2Render @0x40ad4d has NO per-voice DC filter (osc ->
    // flt -> dist -> volramp); the post-voice dcf is post-2000. (DELTA.md)
    if (!inst->old(DELTA_NO_VOICE_DCF))
    {
      dcf.renderMono(voice, voice, nsamples);
      VCETAP_SNAP(dcf, voice, nsamples);
    }

    DEBUG_PLOT(this, voice, nsamples);

    // voice buffer (mono) -> +=output buffer (stereo)
    // original ASM code has chan buffer hardwired as output here. era <v1
    // injects no fcdcoffset in the voice->channel mix (the 2000 loop adds none).
    const sF32 dco = inst->old(DELTA_NO_DCOFFSET) ? 0.0f : fcdcoffset;
#ifndef NDEBUG
    { static int vn=0; if (getenv("V2_VOL") && vn<8) { vn++;
        fprintf(stderr, "[vol] curvol=%.5f flt0.cfreq=%.5f flt0.res=%.4f flt1.cfreq=%.5f env2.out=%.4f env2.val=%.4f aenv.out=%.4f\n",
                curvol, vcf[0].cfreq, vcf[0].res, vcf[1].cfreq, env[1].out, env[1].val, env[0].out); } }
#endif
    sF32 cv = curvol;
    for (sInt i=0; i < nsamples; i++)
    {
      sF32 out = voice[i] * cv;
      cv += volramp;

      dest[i].l += lvol * out + dco;
      dest[i].r += rvol * out + dco;
    }

    curvol = cv;
  }

  void set(const syVV2 *para)
  {
    xpose = para->transp - 64.0f;
    updateNote();

    fmode = (sInt)para->routing;
    keysync = (sInt)para->oscsync;

    // equal power panning
    sF32 p = para->panning / 128.0f;
    lvol = sqrtf(1.0f - p);
    rvol = sqrtf(p);

    // filter balance for parallel
    sF32 x = (para->fltbal - 64.0f) / 64.0f;
    // The asm (syV2Set, synth.asm:2468-2481) picks the branch on the sign of
    // `fist(fltbal-64)` -- the ROUND-TO-NEAREST INTEGER -- not the float sign of x.
    // For a modulated fltbal in (63.5, 64) the rounded int is 0, so the asm takes
    // the >=0 branch and computes f1gain = 1-x > 1 (a slight BOOST of filter 1),
    // where the float-sign test wrongly attenuated filter 2 instead. This was the
    // last whole-song divergence source (via the parallel filter combine).
    sInt xi = v2_fistp(para->fltbal - 64.0f);
    if (xi >= 0)
    {
      f2gain = 1.0f;
      f1gain = 1.0f - x;
    }
    else
    {
      f1gain = 1.0f;
      f2gain = 1.0f + x;
    }

    // subsections
    for (sInt i=0; i < syVV2::NOSC; i++)
      osc[i].set(&para->osc[i]);

    for (sInt i=0; i < syVV2::NENV; i++)
      env[i].set(&para->env[i]);

    for (sInt i=0; i < syVV2::NFLT; i++)
      vcf[i].set(&para->flt[i]);

    for (sInt i=0; i < syVV2::NLFO; i++)
      lfo[i].set(&para->lfo[i]);

    dist.set(&para->dist);
  }

  void noteOn(sInt note, sInt vel)
  {
    this->note = note;
    updateNote();

    velo = (sF32)vel;
    gate = true;

    // reset EGs
    for (sInt i=0; i < syVV2::NENV; i++)
      env[i].state = V2Env::ATTACK;

    // process sync
    // era <v1 (fr08): the 2000 noteOn @0x40aef7 has NO full-resync path. Its
    // only keysync branch (`mov eax,[ebp+0x20]; or eax; je`) zeros the three
    // osc phase counters [+0x28]/[+0x70]/[+0xb8] (= osc.cnt) and nothing else
    // -- i.e. SYNC_OSC for ANY keysync != 0. syOscInit (the rdtsc noise reseed)
    // is called only at voice INIT, never on noteOn, so a re-triggered voice
    // RESUMES its evolved noise/LFO/filter state. The modern SYNC_FULL re-inits
    // osc/vcf/dist (reseeding noise to 0 in the old era) + zeros env.val/
    // curvol; that decorrelated every re-triggered noise voice from the 2000
    // (ch5: rel-rms ~1.41, the equal-power-uncorrelated signature). Collapse
    // FULL->OSC (DELTA_KEYSYNC_OSC_ONLY). (DELTA.md)
    sInt ks = (inst->old(DELTA_KEYSYNC_OSC_ONLY) && keysync == SYNC_FULL)
            ? SYNC_OSC : keysync;
    switch (ks)
    {
    case SYNC_FULL:
      COVER("VOICE noteOn sync full");
      for (sInt i=0; i < syVV2::NENV; i++)
        env[i].val = 0.0f;
      curvol = 0.0f;

      for (sInt i=0; i < syVV2::NOSC; i++)
        osc[i].init(inst, i);

      for (sInt i=0; i < syVV2::NFLT; i++)
        vcf[i].init(inst);

      dist.init(inst);
      // fall-through

    case SYNC_OSC:
      COVER("VOICE noteOn sync osc");
      for (sInt i=0; i < syVV2::NOSC; i++)
        osc[i].cnt = 0;
      // fall-through

    case SYNC_NONE:
    default:
      break;
    }

    for (sInt i=0; i < syVV2::NOSC; i++)
      osc[i].chgPitch();

    for (sInt i=0; i < syVV2::NLFO; i++)
      lfo[i].keyOn();

    dcf.init(inst);
  }

  void noteOff()
  {
    gate = false;
  }

private:
  void updateNote()
  {
    sF32 n = xpose + (sF32)note;
    for (sInt i=0; i < syVV2::NOSC; i++)
      osc[i].note = n;
  }
};

// --------------------------------------------------------------------------
// Bass boost (fixed low shelving EQ)
// --------------------------------------------------------------------------

struct syVBoost
{
  sF32 amount;    // boost in dB (0..18)
};

struct V2Boost
{
  bool enabled;
  sF32 a1, a2;      // normalized filter coeffs
  sF32 b0, b1, b2;
  sF32 x1[2];       // state variables
  sF32 x2[2];
  sF32 y1[2];
  sF32 y2[2];

  V2Instance *inst;

  void init(V2Instance *instance)
  {
    inst = instance;
  }

  void set(const syVBoost *para)
  {
    enabled = ((sInt)para->amount) != 0;
    if (!enabled)
      return;

    COVER("BOOST set");

    // A = 10^(dBgain/40), or a rough approximation anyway
    sF32 A = v2_exp2(para->amount / 128.0f);

    // V2 code computes beta = sqrt((A^2 + 1) - (A-1)^2), which is algebraically
    // just sqrt(2A) — but NOT bit-equal: the asm (syBoostSet, synth.asm:2790-
    // 2800) rounds A*A, A*A+1, (A-1)^2 and the difference to single precision
    // step by step, and for 21 of the 127 possible amounts the result differs
    // from sqrt(2A) by 1 ULP. Keep the asm's stepwise form; the "redundant"
    // shape is load-bearing for bit-fidelity.
    sF32 beta = sqrtf((A * A + 1.0f) - (A - 1.0f) * (A - 1.0f));

    // temp vars
    sF32 bs = beta * inst->SRfcBoostSin;
    sF32 Am1 = A - 1.0f;
    sF32 Ap1 = A + 1.0f;
    sF32 cAm1 = Am1 * inst->SRfcBoostCos;
    sF32 cAp1 = Ap1 * inst->SRfcBoostCos;

    // a0 = (A+1) + (A-1)*cos + beta*sin, summed in the asm's ORDER:
    // (bs + cAm1) + Ap1 (syBoostSet, synth.asm:2818-2823 — fadd st1, fadd st3).
    // FP addition is not associative: (Ap1 + cAm1) + bs rounds differently for
    // some amounts (e.g. 92 — pzero's 9.677s ch7 patch), shifting ia0 and ALL
    // FIVE coefficients by 1 ULP and shocking the biquad state. The association
    // below is load-bearing; do not "clean it up".
    sF32 ia0 = 1.0f / ((bs + cAm1) + Ap1);

    b1 = 2.0f * A * (Am1 - cAp1) * ia0;
    a1 = -2.0f * (Am1 + cAp1) * ia0;
    a2 = (Ap1 + cAm1 - bs) * ia0;
    // PORT FIX: syBoostSet forms A*ia0 ONCE, then b0/b2 = (Ap1-cAm1 ± bs)*(A*ia0)
    // (synth.asm:2854-2862). The port wrote A*(...)*ia0, i.e. (A*(...))* ia0 --
    // FP multiply isn't associative, so b0/b2 came out 1 ULP off the asm, and the
    // chorus feedback comb + sum compressor amplified that into the whole-song
    // residual. Group to match the asm exactly.
    sF32 Aia0 = A * ia0;
    b0 = (Ap1 - cAm1 + bs) * Aia0;
    b2 = (Ap1 - cAm1 - bs) * Aia0;
  }

  void render(StereoSample *buf, sInt nsamples)
  {
    if (!enabled)
      return;

    COVER("BOOST render");

    for (sInt ch=0; ch < 2; ch++)
    {
      sF32 xm1 = x1[ch], xm2 = x2[ch];
      sF32 ym1 = y1[ch], ym2 = y2[ch];

      for (sInt i=0; i < nsamples; i++)
      {
        sF32 x = buf[i].ch[ch] + fcdcoffset;

        // Second-order IIR filter. Match syBoostProcChan's accumulation grouping
        // EXACTLY: b0*x + ((b1*x1 - a1*y1) + (b2*x2 - a2*y2)). The biquad is
        // recursive, so a different operand order rounds each sample differently
        // and the poles integrate it into drift (the sequential form below drifts).
        sF32 y = b0*x + ((b1*xm1 - a1*ym1) + (b2*xm2 - a2*ym2));
        ym2 = ym1; ym1 = y;
        xm2 = xm1; xm1 = x;

        buf[i].ch[ch] = y;
      }

      x1[ch] = xm1; x2[ch] = xm2;
      y1[ch] = ym1; y2[ch] = ym2;
    }
  }
};

// --------------------------------------------------------------------------
// Modulating delay
// --------------------------------------------------------------------------

struct syVModDel
{
  sF32 amount;    // dry/wet value (0=-wet, 64=dry, 127=wet)
  sF32 fb;        // feedback (0=-100%, 64=0%, 127=~100%)
  sF32 llength;   // length of left delay
  sF32 rlength;   // length of right delay
  sF32 mrate;     // modulation rate
  sF32 mdepth;    // modulation depth
  sF32 mphase;    // modulation stereo phase (0=-180deg, 64=0deg, 127=180deg)
};


struct V2ModDel
{
  sF32 *db[2];    // left/right delay buffer
  sU32 dbufmask;  // delay buffer mask (size-1, must be pow2)

  sU32 dbptr;     // buffer write pos
  sU32 dboffs[2]; // buffer read offset
  
  sU32 mcnt;      // mod counter
  sInt mfreq;     // mod freq
  sU32 mphase;    // mod phase
  sU32 mmaxoffs;  // mod max offs (2048samples*depth)

  sF32 fbval;     // feedback val
  sF32 dryout;
  sF32 wetout;

  V2Instance *inst;

  void init(V2Instance *instance, sF32 *buf1, sF32 *buf2, sInt buflen)
  {
    assert(buflen != 0 && (buflen & (buflen - 1)) == 0);
    db[0] = buf1;
    db[1] = buf2;
    dbufmask = buflen - 1;
    inst = instance;

    reset();
  }

  void reset()
  {
    dbptr = 0;
    mcnt = 0;

    memset(db[0], 0, (dbufmask + 1) * sizeof(sF32));
    memset(db[1], 0, (dbufmask + 1) * sizeof(sF32));
  }

  void set(const syVModDel *para)
  {
    wetout = (para->amount - 64.0f) / 64.0f;
    dryout = 1.0f - fabsf(wetout);
    fbval = (para->fb - 64.0f) / 64.0f;

    // asm syModDelSet: every int conversion is fistp (round-to-nearest), NOT a
    // truncating cast. fci128=0.0078125 (=1/128, exact); operation order matches
    // the asm. The truncating (sInt)/ftou32 below are off-by-one when the frac
    // >= 0.5; mfreq drives the integer mod-counter (mcnt += mfreq) so its error
    // slowly drifts the chorus, diverging the channel output over the song.
    sF32 lenscale = ((sF32)dbufmask - 1023.0f) * 0.0078125f;
    dboffs[0] = v2_fistp(para->llength * lenscale);
    dboffs[1] = v2_fistp(para->rlength * lenscale);

    mfreq = v2_fistp(calcfreq(para->mrate * 0.0078125f) * fcmdlfomul * inst->SRfclinfreq);
    mmaxoffs = v2_fistp(para->mdepth * 0.0078125f * 1023.0f);
    mphase = 2u * (sU32)v2_fistp((para->mphase - 64.0f) * 0.0078125f * fc32bit);
  }

  void renderAux2Main(StereoSample *dest, sInt nsamples)
  {
    if (!wetout)
      return;

    COVER("MODDEL aux->main");

    for (sInt i=0; i < nsamples; i++)
    {
      StereoSample x;

      sF32 in = inst->aux2buf[i]
              + (inst->old(DELTA_NO_DCOFFSET) ? 0.0f : fcdcoffset); // era: no DC bias
      processSample(&x, in, in, 0.0f);

      dest[i].l += x.l;
      dest[i].r += x.r;
    }
  }

  void renderChan(StereoSample *chanbuf, sInt nsamples)
  {
    if (!wetout)
      return;

    COVER("MODDEL chan");

    // era <v1 (fr08): the 2000 chorus render @0x40b268 feeds the channel buffer
    // straight in (fld [esi]/[esi+4]) with NO denormal bias; 2004 adds
    // fcdcoffset. Through the feedback comb this accumulates, so it must be
    // gated for the matched A/B. (DELTA.md delta 6, channel level.)
    const sF32 dco = inst->old(DELTA_NO_DCOFFSET) ? 0.0f : fcdcoffset;
    sF32 dry = dryout;
    for (sInt i=0; i < nsamples; i++)
      processSample(&chanbuf[i], chanbuf[i].l + dco, chanbuf[i].r + dco, dry);
  }

private:
  inline sF32 processChanSample(sF32 in, sInt ch, sF32 dry)
  {
    // modulation is a triangle wave
    sU32 counter = mcnt + (ch ? mphase : 0);
    counter = (counter < 0x80000000u) ? counter*2 : 0xffffffffu - counter*2;
    
    // determine effective offset
    sU64 offs32_32 = (sU64)counter * mmaxoffs; // 32.32 fixed point
    sU32 offs_int = sU32(offs32_32 >> 32) + dboffs[ch];
    // PORT FIX: asm syModDelProcessSample reads in1=db[idx], in2=db[idx+1] and
    // interpolates in1 + (1-frac)*(in2-in1) (asm x = fc2-(1+frac) = 1-frac). The
    // RIGHT channel (ch=1, the "rechtes dingens") has an extra `dec ebx`
    // (synth.asm:3067) so idx = dbptr-offs-1; the LEFT channel does not. The port
    // read {index, index-1} for BOTH channels -- correct for the right, but the
    // LEFT channel then interpolated the wrong neighbor pair (a one-sample tap
    // error the feedback comb amplified into the dominant chain divergence).
    sU32 idx = dbptr - offs_int - (ch ? 1u : 0u);

    // linear interpolation using low-order bits of offs32_32.
    sF32 *delaybuf = db[ch];
    sF32 x = utof23((sU32)(offs32_32 & 0xffffffffu));
    sF32 in1 = delaybuf[idx & dbufmask];
    sF32 in2 = delaybuf[(idx + 1) & dbufmask];
    sF32 delayed = in1 + (1.0f - x) * (in2 - in1);


    // mix and output
    delaybuf[dbptr] = in + delayed*fbval;
    return in*dry + delayed*wetout;
  }

  inline void processSample(StereoSample *out, sF32 l, sF32 r, sF32 dry)
  {
    out->l = processChanSample(l, 0, dry);
    out->r = processChanSample(r, 1, dry);

    // tick
    mcnt += mfreq;
    dbptr = (dbptr + 1) & dbufmask;
  }
};

// --------------------------------------------------------------------------
// Stereo Compressor
// --------------------------------------------------------------------------

struct syVComp
{
  sF32 mode;      // 0=off, 1=Peak, 2=RMS
  sF32 stereo;    // 0=mono, 1=stereo
  sF32 autogain;  // 0=off, 1=on
  sF32 lookahead; // lookahead in ms
  sF32 threshold; // threshold (-54dB .. 6dB)
  sF32 ratio;     // (0=1:1 .. 127=1:inf)
  sF32 attack;    // attack value
  sF32 release;   // release value
  sF32 outgain;   // output gain
};

struct V2Comp
{
  static const int COMPDLEN = 5700;
  static const int RMSLEN = 8192; // must be a power of 2

  enum Mode
  {
    MODE_OFF = 0,
    MODE_PEAK,
    MODE_RMS,
  };

  enum ModeBits
  {
    MODE_BIT_PEAK   = 0,
    MODE_BIT_RMS    = 1,
    MODE_BIT_MONO   = 0,
    MODE_BIT_STEREO = 2,
    MODE_BIT_ON     = 0,
    MODE_BIT_OFF    = 4,
  };

  sInt mode;      // bit 0: Peak/RMS, bit 1: Stereo, bit 2: off
  sInt oldmode;   // last mode set() saw (asm syWComp.oldmode; drives the reset)

  sF32 invol;     // input gain (1/threshold, internal threshold is always 0dB)
  sF32 ratio;
  sF32 outvol;    // output gain (outgain * threshold)
  sF32 attack;    // attack (lpf coeff, 0..1)
  sF32 release;   // release (lpf coeff, 0..1)

  sU32 dblen;     // lookahead buffer length
  sU32 dbcnt;     // lookahead buffer offset

  sF32 curgain[2]; // current gain
  sF32 peakval[2]; // peak value
  sF32 rmsval[2];  // rms current value
  sU32 rmscnt;     // rms counter

  StereoSample dbuf[COMPDLEN]; // lookahead delay buffer
  StereoSample rmsbuf[RMSLEN]; // RMS ring buffer

  V2Instance *inst;

  void init(V2Instance *instance)
  {
    mode = MODE_BIT_STEREO;
    memset(dbuf, 0, sizeof(dbuf));
    inst = instance;

#if BUG_V2_COMP_OLDMODE
    // ASM-faithful: syCompInit only sets mode=2; oldmode/curgain/peak/rms stay
    // at the zeroed-instance values. curgain=1 only ever arrives via the
    // mode-change reset in set() — which a comp whose first set() computes
    // mode==0 (peak|mono|on) never takes, so it starts at curgain=0.
    oldmode = 0;
    for (sInt i=0; i < 2; i++)
    {
      peakval[i] = 0.0f;
      rmsval[i] = 0.0f;
      curgain[i] = 0.0f;
    }
    memset(rmsbuf, 0, sizeof(rmsbuf));
    rmscnt = 0;
#else
    oldmode = mode;
    reset();
#endif
  }

  void reset()
  {
    for (sInt i=0; i < 2; i++)
    {
      peakval[i] = 0.0f;
      rmsval[i] = 0.0f;
      curgain[i] = 1.0f;
    }
    memset(rmsbuf, 0, sizeof(rmsbuf));
    rmscnt = 0;
  }

  void set(const syVComp *para)
  {
    switch ((sInt)para->mode)
    {
    case MODE_OFF:  mode = MODE_BIT_OFF; break;
    case MODE_PEAK: mode = MODE_BIT_PEAK | MODE_BIT_ON; break;
    case MODE_RMS:  mode = MODE_BIT_RMS | MODE_BIT_ON; break;
    default:        assert(false);
    }

    if (para->stereo != 0.0f)
      mode |= MODE_BIT_STEREO;

    // asm syCompSet compares against a PERSISTENT oldmode field (not last
    // frame's mode): the reset fires only on a transition set() itself sees.
    // (Encoding note: asm encodes OFF as 5/7 vs our 4/6 — both injective and
    // both map peak|mono|on to 0, so transition decisions are identical.)
    if (mode != oldmode)
    {
      oldmode = mode;
      reset();
    }

    // @@@BUG: original V2 code uses "fcsamplesperms" here which is
    // hard-coded to 44.1kHz
    // asm syCompSet stores dblen via fistp (round-to-nearest); the port truncated.
    dblen = (sU32)v2_fistp(para->lookahead * inst->SRfcsamplesperms);

    sF32 thresh = 8.0f * calcfreq(para->threshold / 128.0f);
    invol = 1.0f / thresh;
    if (para->autogain != 0.0f)
      thresh = 1.0f;
    outvol = thresh * v2_exp2((para->outgain - 64.0f) / 16.0f);
    ratio = para->ratio / 128.0f;
    
    // attack: 0 (!) ... 200ms (5Hz)
    attack = v2_exp2(-para->attack * 12.0f / 128.0f);
    // release: 5ms .. 5s
    release = v2_exp2(-para->release * 16.0f / 128.0f);
#ifndef NDEBUG
    if (getenv("V2_COMPDUMP"))
      fprintf(stderr, "[comp] mode=%.0f stereo=%.0f thresh=%.0f ratio=%.0f attack=%.0f release=%.0f outgain=%.0f autogain=%.0f -> invol=%.4f outvol=%.4f\n",
              para->mode, para->stereo, para->threshold, para->ratio, para->attack, para->release, para->outgain, para->autogain, invol, outvol);
#endif
  }

  void render(StereoSample *buf, sInt nsamples)
  {
    if (mode & MODE_BIT_OFF)
      return;

    COVER("COMP render");

    // Step 1: level detect (fills LD buffers)
    StereoSample *levels = inst->levelbuf;
    switch (mode & (MODE_BIT_RMS | MODE_BIT_STEREO))
    {
    case MODE_BIT_PEAK | MODE_BIT_MONO:
      COVER("COMP level peak mono");
      for (sInt i=0; i < nsamples; i++)
        levels[i].l = levels[i].r = invol * doPeak(0.5f * (buf[i].l + buf[i].r), 0);
      break;

    case MODE_BIT_RMS | MODE_BIT_MONO:
      COVER("COMP level rms mono");
      for (sInt i=0; i < nsamples; i++)
      {
        // mono: dcoffset on the input (asm syCompLDMonoRMS), not the accumulator
        levels[i].l = levels[i].r = invol * doRMS(0.5f * (buf[i].l + buf[i].r) + fcdcoffset, 0, 0.0f);
        rmscnt = (rmscnt + 1) & (RMSLEN - 1);
      }
      break;

    case MODE_BIT_PEAK | MODE_BIT_STEREO:
      COVER("COMP level peak stereo");
      for (sInt i=0; i < nsamples; i++)
      {
        levels[i].l = invol * doPeak(buf[i].l, 0);
        levels[i].r = invol * doPeak(buf[i].r, 1);
      }
      break;

    case MODE_BIT_RMS | MODE_BIT_STEREO:
      COVER("COMP level rms stereo");
      for (sInt i=0; i < nsamples; i++)
      {
        levels[i].l = invol * doRMS(buf[i].l, 0, fcdcoffset);
        levels[i].r = invol * doRMS(buf[i].r, 1, fcdcoffset);
        rmscnt = (rmscnt + 1) & (RMSLEN - 1);
      }
      break;
    }

    // Step 2: compress!
    for (sInt ch=0; ch < 2; ch++)
    {
      sF32 gain = curgain[ch];
      sU32 dbind = dbcnt;

      for (sInt i=0; i < nsamples; i++)
      {
        // lookahead delay line
        sF32 v = outvol * dbuf[dbind].ch[ch];
        dbuf[dbind].ch[ch] = invol * buf[i].ch[ch];
        // PORT FIX: ASM (syCompProcChannel) wraps with "inc/cmp dblen/jbe" i.e.
        // resets only when the index EXCEEDS dblen -> ring length dblen+1. The
        // port used ">= dblen" (ring length dblen), making the lookahead one
        // sample short. Match the ASM.
        if (++dbind > dblen)
          dbind = 0;

        // determine dest gain
        sF32 dgain = 1.0f;
        sF32 lvl = levels[i].ch[ch];
        if (lvl >= 1.0f)
          dgain = 1.0f / (1.0f + ratio * (lvl - 1.0f));

        // and compress
        gain += (dgain < gain ? attack : release) * (dgain - gain);
        buf[i].ch[ch] = v * gain;
      }

      curgain[ch] = gain;
      if (ch == 1)
        dbcnt = dbind;
    }
  }
  
private:
  // level detection variants
  inline sF32 doPeak(sF32 in, sInt ch)
  {
    peakval[ch] = max(peakval[ch] * fccpdfalloff + fcdcoffset, fabsf(in));
    return peakval[ch];
  }

  // PORTING FIX (not an ASM bug) — op-for-op port of syCompLD{Mono,Stereo}RMS:
  // - the oldest sample is subtracted from the accumulator FIRST (its own
  //   rounding), not fused into "+= insq - oldest";
  // - the FIXDENORMALS dcoffset goes on the ACCUMULATOR in stereo mode but on
  //   the INPUT in mono mode (asm inconsistency, replicated via the callers);
  // - the output is sqrt(rv) * fci8192 (the asm's 24-bit 1/sqrt(8192)
  //   constant), NOT sqrt(rv/8192) — a different rounding sequence (1 ULP on
  //   most samples; was the whole-song residual driver via the sum compressor).
  inline sF32 doRMS(sF32 in, sInt ch, sF32 accumdc)
  {
    rmsval[ch] -= rmsbuf[rmscnt].ch[ch]; // remove oldest
    rmsval[ch] += accumdc;               // denormal fix (stereo: on accumulator)
    sF32 insq = sqr(in);
    rmsval[ch] += insq;                  // add new sample
    rmsbuf[rmscnt].ch[ch] = insq;        // keep track of value we added
    return sqrtf(rmsval[ch]) * fcrms8192;
  }
};

// --------------------------------------------------------------------------
// Stereo reverb
// --------------------------------------------------------------------------

struct syVReverb
{
  sF32 revtime;
  sF32 highcut;
  sF32 lowcut;
  sF32 vol;
};

struct V2Reverb
{
  sF32 gainc[4];  // feedback gain for comb filter delays 0-3
  sF32 gaina[2];  // feedback gain for allpas delays 0-1
  sF32 gainin;    // input gain
  sF32 damp;      // high cut (1-val^2)
  sF32 lowcut;    // low cut (val^2)

  V2Delay combd[2][4];  // left/right comb filter delay lines
  sF32 combl[2][4];     // left/right comb delay filter buffers
  V2Delay alld[2][2];   // left/right allpass filters
  sF32 hpf[2];          // memory for low cut filters

  V2Instance *inst;

  // delay line buffers
  sF32 bcombl0[1309];
  sF32 bcombl1[1635];
  sF32 bcombl2[1811];
  sF32 bcombl3[1926];
  sF32 balll0[220];
  sF32 balll1[74];
  sF32 bcombr0[1327];
  sF32 bcombr1[1631];
  sF32 bcombr2[1833];
  sF32 bcombr3[1901];
  sF32 ballr0[205];
  sF32 ballr1[77];

  void init(V2Instance *instance)
  {
    // init filters
    combd[0][0].init(bcombl0);
    combd[0][1].init(bcombl1);
    combd[0][2].init(bcombl2);
    combd[0][3].init(bcombl3);
    alld[0][0].init(balll0);
    alld[0][1].init(balll1);
    combd[1][0].init(bcombr0);
    combd[1][1].init(bcombr1);
    combd[1][2].init(bcombr2);
    combd[1][3].init(bcombr3);
    alld[1][0].init(ballr0);
    alld[1][1].init(ballr1);

    inst = instance;
    reset();
  }

  void reset()
  {
    for (sInt ch=0; ch < 2; ch++)
    {
      // comb
      for (sInt i=0; i < 4; i++)
      {
        combd[ch][i].reset();
        combl[ch][i] = 0.0f;
      }

      // allpass
      for (sInt i=0; i < 2; i++)
        alld[ch][i].reset();

      // low cut
      hpf[ch] = 0.0f;
    }
  }

  void set(const syVReverb *para)
  {
    static const sF32 gaincdef[4] = {
      0.966384599f, 0.958186359f, 0.953783929f, 0.950933178f
    };
    static const sF32 gainadef[2] = {
      0.994260075f, 0.998044717f
    };

    // era <v1 (fr08): the 2000 syReverbSet @0x40b2d0 computes e = sqr(64/(revtime
    // +1)) with NO SRfclinfreq factor (a 2004 SR-flexibility addition, =1.0 at
    // 44100) AND at the DEFAULT x87 precision (PC=64), not the PC=24 the render
    // path uses -- so the 64/(revtime+1) quotient is kept full-precision and
    // squared, then rounded once (square-exact-quotient). The modern C path
    // rounds the quotient to 24 bits first (round-then-square), giving e one ULP
    // lower, which flips the near-tie comb feedback gain gainc[2]
    // (3f6a7efb -> 3f6a7efc) and seeds the reverb-tail residual. Reproduce the
    // 2000 by computing the quotient+square in double, no SRfclinfreq. (DELTA.md)
    sF32 e;
    if (inst->old(DELTA_RVB_E_FULLPREC))
    {
      // The build/render runs x87 at PC=24 (-mpc32), but the 2000 reverb set
      // ran at the default PC=64, so the 64/(revtime+1) quotient is kept
      // full-precision and squared (square-exact-quotient). Temporarily raise
      // PC to 64 so GCC keeps the quotient in the register at full precision
      // through the square. (round-then-square at PC=24 is one ULP low.)
      // Portable: same structure in double (53-bit core vs the x87's 64-bit
      // -- can differ only at razor ties, same stance as every v2math
      // kernel): full-precision quotient, squared, rounded ONCE to float.
      sF32 rt1 = para->revtime + 1.0f; // (exact: revtime int + 1)
      double q = 64.0 / (double)rt1;
      e = (sF32)(q * q);
    }
    else
      e = inst->SRfclinfreq * sqr(64.0f / (para->revtime + 1.0f));
    for (sInt i=0; i < 4; i++)
      gainc[i] = v2_powf(gaincdef[i], e);

    for (sInt i=0; i < 2; i++)
      gaina[i] = v2_powf(gainadef[i], e);

    damp = inst->SRfclinfreq * (para->highcut / 128.0f);
    gainin = para->vol / 128.0f;
    lowcut = inst->SRfclinfreq * sqr(sqr(para->lowcut / 128.0f));
  }

  void render(StereoSample *dest, sInt nsamples)
  {
    const sF32 *inbuf = inst->aux1buf;

    COVER("RVB render");

    for (sInt i=0; i < nsamples; i++)
    {
      sF32 in = inbuf[i] * gainin
              + (inst->old(DELTA_NO_DCOFFSET) ? 0.0f : fcdcoffset); // era: no DC bias

      for (sInt ch=0; ch < 2; ch++)
      {
        // parallel comb filters
        sF32 cur = 0.0f;
        for (sInt j=0; j < 4; j++)
        {
          sF32 dv = gainc[j] * combd[ch][j].fetch();
          sF32 nv = (j & 1) ? (dv - in) : (dv + in); // alternate phase on combs
          sF32 lp = combl[ch][j] + damp * (nv - combl[ch][j]);
          // PORT FIX: persist the comb lowpass state. The asm stores the filtered
          // result to BOTH the lowpass memory and the delay line (synth.asm:3883
          // `fst lpfcl0` / `fst linecl0`); the port fed only the delay line and
          // never wrote combl back, so it stayed 0 and the one-pole lowpass
          // degenerated into a fixed `damp*nv` scale -- losing each comb's HF
          // damping, which diverged through the comb feedback (reverb-only; this
          // block is excluded from the leaf oracle, so it went uncaught).
          combl[ch][j] = lp;
          combd[ch][j].feed(lp);
          cur += lp;
        }

        // serial allpass filters
        for (sInt j=0; j < 2; j++)
        {
          sF32 dv = alld[ch][j].fetch();
          sF32 dz = cur + gaina[j] * dv;
          alld[ch][j].feed(dz);
          cur = dv - gaina[j] * dz;
        }

        // low cut and output. era <v4: the 2000 reverb render @0x40b435
        // outputs `dest += cur` with NO low-cut high-pass stage (the lowcut
        // param/stage is v4-added per sounddef.h). fr08's canonicalized lowcut
        // param is nonzero, so the hpf would be an EXTRA high-pass not in the
        // period binary -- it diverges the reverb tail. (DELTA.md)
        if (inst->old(DELTA_NO_RVB_LOWCUT))
          dest[i].ch[ch] += cur;
        else
        {
          hpf[ch] += lowcut * (cur - hpf[ch]);
          dest[i].ch[ch] += cur - hpf[ch];
        }
      }
    }
  }
};

// --------------------------------------------------------------------------
// Channel
// --------------------------------------------------------------------------

struct syVChan
{
  sF32 chanvol;
  sF32 auxarcv; // aux a receive
  sF32 auxbrcv; // aux b receive
  sF32 auxasnd; // aux a send
  sF32 auxbsnd; // aux b send
  sF32 aux1;
  sF32 aux2;
  sF32 fxroute;
  syVBoost boost;
  syVDist dist;
  syVModDel chorus;
  syVComp comp;
};

#ifndef NDEBUG
// dev-only: per-channel-FX-stage peak (V2_MIXTAP set), localizes a channel-chain
// divergence (dcf1/comp/boost/dist/chorus) under a channel solo.
static inline void chtap_snap(const char *stage, const StereoSample *chan, sInt n)
{
  static int on = -1; if (on < 0) on = getenv("V2_MIXTAP") ? 1 : 0;
  if (!on) return;
  sF32 mx = 0.0f;
  for (sInt i=0; i<n; i++) { sF32 a=fabsf(chan[i].l), b=fabsf(chan[i].r); if(a>mx)mx=a; if(b>mx)mx=b; }
  if (mx > 0.5f) fprintf(stderr, "[ch] %-6s %.5f\n", stage, mx);
}
#define CHTAP_SNAP(stage, chan, n) chtap_snap(#stage, chan, n)
#else
#define CHTAP_SNAP(stage, chan, n) ((void)0)
#endif

struct V2Chan
{
  enum FXRouting
  {
    FXR_DIST_THEN_CHORUS = 0,
    FXR_CHORUS_THEN_DIST,
  };

  sF32 chgain;    // channel gain
  sF32 a1gain;    // aux1 gain
  sF32 a2gain;    // aux2 gain
  sF32 aasnd;     // aux a send gain
  sF32 absnd;     // aux b send gain
  sF32 aarcv;     // aux a receive gain
  sF32 abrcv;     // aux b receive gain
  sInt fxr;
  V2DCFilter dcf1;
  V2Boost boost;
  V2Dist dist;
  V2DCFilter dcf2;
  V2ModDel chorus;
  V2Comp comp;

  V2Instance *inst;

  void init(V2Instance *instance, sF32 *delbuf1, sF32 *delbuf2, sInt buflen)
  {
    inst = instance;
    dcf1.init(inst);
    boost.init(inst);
    dist.init(inst);
    dcf2.init(inst);
    chorus.init(inst, delbuf1, delbuf2, buflen);
    comp.init(inst);
  }

  void set(const syVChan *para)
  {
    aarcv = para->auxarcv / 128.0f;
    abrcv = para->auxbrcv / 128.0f;
    aasnd = fcgain * (para->auxasnd / 128.0f);
    absnd = fcgain * (para->auxbsnd / 128.0f);
    chgain = fcgain * (para->chanvol / 128.0f);
    // PORTING FIX (not an ASM bug): the ASM (syChanSet, synth.asm:4198-4209)
    // computes ((aux/128) * fcgainh) * chgain — aux first, chgain LAST.
    // Multiplication is non-associative: grouping chgain*fcgainh first came out
    // 1 ULP off for some volumes, seeding the aux sends (reverb input).
    a1gain = (para->aux1 / 128.0f) * fcgainh * chgain;
    a2gain = (para->aux2 / 128.0f) * fcgainh * chgain;
    fxr = (sInt)para->fxroute;
    dist.set(&para->dist);
    chorus.set(&para->chorus);
    // comp/boost state is only ever read by their renders, which process()
    // gates on the same row -- skipping set here is unobservable for old-era
    // files and lets single-version builds drop the comp/boost code entirely
    // (version-build-subsetting spec, code elimination).
    if (!inst->old(DELTA_NO_COMP_BOOST))
    {
      comp.set(&para->comp);
      boost.set(&para->boost);
    }
  }

  void process(sInt nsamples, bool muted = false, sInt chanIndex = -1)
  {
    StereoSample *chan = inst->chanbuf;
    CHTAP_SNAP(entry, chan, nsamples);

    // AuxA/B receive (stereo)
    accumulate(chan, inst->auxabuf, nsamples, aarcv);
    accumulate(chan, inst->auxbbuf, nsamples, abrcv);
    CHTAP_SNAP(rxaux, chan, nsamples);

    // Filters. era <v1 (fr08): the 2000 channel chain @0x40b5cc is ONLY
    // dist + chorus (in fxr order) -- comp/boost are v1 features with no code
    // in v0 (DELTA_NO_COMP_BOOST, anchored at v1 by the param tables). The
    // dcf1/dcf2 DC filters are gated SEPARATELY: the v5 syChanProcess
    // (genthree _viruz2a.asm == candytron binary) is comp -> boost ->
    // dist/chorus with NO DC filter stage; dcf1/dcf2 are 2004/v6 additions
    // (DELTA_NO_CHAN_DCF, proven by the candytron oracle). (DELTA.md)
    const bool nochain = inst->old(DELTA_NO_COMP_BOOST);
    const bool nodcf   = inst->old(DELTA_NO_CHAN_DCF);
    if (!nochain)
    {
      if (!nodcf)
      {
        dcf1.renderStereo(chan, chan, nsamples);
        CHTAP_SNAP(dcf1, chan, nsamples);
        DEBUG_PLOT_STEREO(&dcf1, chan, nsamples);
      }
#ifndef NDEBUG
      if (!getenv("V2_NOCOMP"))
#endif
      comp.render(chan, nsamples);
      CHTAP_SNAP(comp, chan, nsamples);
      boost.render(chan, nsamples);
      CHTAP_SNAP(boost, chan, nsamples);
    }
    if (fxr == FXR_DIST_THEN_CHORUS)
    {
      dist.renderStereo(chan, chan, nsamples);
      CHTAP_SNAP(dist, chan, nsamples);
      if (!nochain && !nodcf) { dcf2.renderStereo(chan, chan, nsamples); CHTAP_SNAP(dcf2, chan, nsamples); }
#ifndef NDEBUG
      if (!getenv("V2_NOCHORUS"))
#endif
      chorus.renderChan(chan, nsamples);
      CHTAP_SNAP(chorus, chan, nsamples);
    }
    else // FXR_CHORUS_THEN_DIST
    {
      chorus.renderChan(chan, nsamples);
      CHTAP_SNAP(chorus, chan, nsamples);
      dist.renderStereo(chan, chan, nsamples);
      CHTAP_SNAP(dist, chan, nsamples);
      if (!nochain && !nodcf) { dcf2.renderStereo(chan, chan, nsamples); CHTAP_SNAP(dcf2, chan, nsamples); }
    }

    // per-channel output meter (display-only): peak of this channel's mix
    // contribution (post-FX signal * channel gain), measured regardless of mute
    // so a silenced-but-active channel still reads its level. Max-accumulated;
    // the host reads-and-clears. Reads chan[]/writes the side buffer only.
    if (chanIndex >= 0)
    {
      sF32 pk = 0.0f;
      for (sInt i = 0; i < nsamples; i++)
      {
        sF32 a = chan[i].l; if (a < 0.0f) a = -a;
        sF32 b = chan[i].r; if (b < 0.0f) b = -b;
        sF32 m = a > b ? a : b;
        if (m > pk) pk = m;
      }
      pk *= chgain;
      if (pk > inst->chanPeak[chanIndex]) inst->chanPeak[chanIndex] = pk;
    }

    // Muted channels render fully (above) but feed NO output bus -- not the main
    // mix, and not the reverb/delay/aux sends (skipping only the main mix would
    // leave the channel's reverb/delay tail audible). The dry FX state has
    // already advanced, so unmuting resumes seamlessly and in-phase.
    if (!muted)
    {
      // Aux1/2 send (mono)
      accumulateMonoMix(inst->aux1buf, chan, nsamples, a1gain);
      accumulateMonoMix(inst->aux2buf, chan, nsamples, a2gain);

      // AuxA/B send (stereo)
      accumulate(inst->auxabuf, chan, nsamples, aasnd);
      accumulate(inst->auxbbuf, chan, nsamples, absnd);

      // Channel buffer to mix buffer (stereo)
      accumulate(inst->mixbuf, chan, nsamples, chgain);
    }

    DEBUG_PLOT_STEREO(this, chan, nsamples);
  }

private:
  void accumulate(StereoSample *dest, const StereoSample *src, sInt nsamples, sF32 gain)
  {
    if (gain == 0.0f)
      return;

    for (sInt i=0; i < nsamples; i++)
    {
      dest[i].l += gain * src[i].l;
      dest[i].r += gain * src[i].r;
    }
  }

  void accumulateMonoMix(sF32 *dest, const StereoSample *src, sInt nsamples, sF32 gain)
  {
    if (gain == 0.0f)
      return;

    for (sInt i=0; i < nsamples; i++)
      dest[i] += gain * (src[i].l + src[i].r);
  }
};

// --------------------------------------------------------------------------
// Sound definitions
// --------------------------------------------------------------------------

struct V2Mod
{
  sU8 source;   // source: vel/ctl1-7/aenv/env2/lfo1/lfo2
  sU8 val;      // 0=-1 .. 128=1
  sU8 dest;     // destination (index into V2Sound)
};

struct V2Sound
{
  sU8 voice[sizeof(syVV2) / sizeof(sF32)];
  sU8 chan[sizeof(syVChan) / sizeof(sF32)];
  sU8 maxpoly;
  sU8 modnum;
  V2Mod modmatrix[1]; // actually modnum entries!
};

union V2PatchMap
{
  sU32 offsets[128];    // offsets into raw_data[]
  sU8 raw_data[1];      // variable size
};

// --------------------------------------------------------------------------
// Ronan
// --------------------------------------------------------------------------

struct syWRonan
{
  sU8 mem[64*1024]; // "that should be enough" --synth.asm. :)
};

#if V2_RONAN // ronan.cpp (speech synth), independent compile flag (design D5)

extern "C"
{
  void __stdcall ronanCBInit(syWRonan *pthis);
  void __stdcall ronanCBTick(syWRonan *pthis);
  void __stdcall ronanCBNoteOn(syWRonan *pthis);
  void __stdcall ronanCBNoteOff(syWRonan *pthis);
  void __stdcall ronanCBSetCtl(syWRonan *pthis, sU32 ctl, sU32 val);
  void __stdcall ronanCBProcess(syWRonan *pthis, sF32 *buf, sU32 len);
  void __stdcall ronanCBSetSR(syWRonan *pthis, sInt samplerate);
}

#else

static inline void ronanCBInit(syWRonan *) {}
static inline void ronanCBTick(syWRonan *) {}
static inline void ronanCBNoteOn(syWRonan *) {}
static inline void ronanCBNoteOff(syWRonan *) {}
static inline void ronanCBSetCtl(syWRonan *, sU32, sU32) {}
static inline void ronanCBProcess(syWRonan *, sF32 *, sU32) {}
static inline void ronanCBSetSR(syWRonan *, sInt) {}

extern "C" void __stdcall synthSetLyrics(void *, const char **) {}

#endif

// --------------------------------------------------------------------------
// Synth
// --------------------------------------------------------------------------

struct V2ChanInfo
{
  sU8 pgm;    // program
  sU8 ctl[7]; // controllers
};

// V2Synth holds a V2Instance.
// In the original code these are one and the same struct (SYN) but that
// would turn out fairly awkward in this C++ version, hence the split.
#ifndef NDEBUG
// dev-only: per-master-stage peak (V2_MIXTAP set). Tracks the running max of a
// stereo mix buffer at each global-FX boundary (premix/reverb/delay/dcf/lchc/
// compr) so a channel solo localizes a master-stage divergence vs an oracle.
static inline void mixtap_snap(const char *stage, const StereoSample *mix, sInt n)
{
  static int on = -1; if (on < 0) on = getenv("V2_MIXTAP") ? 1 : 0;
  if (!on) return;
  sF32 mx = 0.0f;
  for (sInt i=0; i<n; i++) {
    sF32 a = fabsf(mix[i].l), b = fabsf(mix[i].r);
    if (a>mx) mx=a; if (b>mx) mx=b;
  }
  if (mx > 0.0f) fprintf(stderr, "[mix] %-7s %.5f\n", stage, mx);
}
#define MIXTAP_SNAP(stage, mix, n) mixtap_snap(#stage, mix, n)
#else
#define MIXTAP_SNAP(stage, mix, n) ((void)0)
#endif

struct V2Synth
{
  static const sInt POLY = 64;
  static const sInt CHANS = 16;

  const V2PatchMap *patchmap;
  sU32 mrstat;          // running status in MIDI decoding
  sU32 curalloc;
  sInt samplerate;
  sInt chanmap[POLY];   // voice -> chan
  sU32 allocpos[POLY];
  sInt voicemap[CHANS]; // chan -> choice
  sInt tickd;           // number of finished samples left in mix buffer (modern path)
  sInt subRemain;       // eraV0 sub-frame path: samples left in the current control frame

  // voice-allocation tap (V2_STEAL env): mirror the oracle's [binS] tap so the
  // portable's chanmap alloc/free/steal events can be diffed against candytron.
  sU32 dbgsmpl;
  void pollSteal(const char *tag)
  {
    static int on = -1;
    if (on < 0) { const char *e = getenv("V2_STEAL"); on = e ? 1 : 0; }
    if (!on) return;
    static int prev[POLY]; static int init = 0;
    if (!init) { for (sInt v=0; v<POLY; v++) prev[v]=chanmap[v]; init=1; return; }
    for (sInt v=0; v<POLY; v++) if (chanmap[v]!=prev[v]) {
      fprintf(stderr,"[ptbS] smpl=%u %s v=%d chan %d->%d alloc=%u\n",
              dbgsmpl,tag,v,prev[v],chanmap[v],allocpos[v]);
      prev[v]=chanmap[v];
    }
  }

  V2ChanInfo chans[CHANS];
  syVV2 voicesv[POLY];
  V2Voice voicesw[POLY];
  syVChan chansv[CHANS];
  V2Chan chansw[CHANS];

  struct Globals
  {
    syVReverb rvbparm;
    syVModDel delparm;
    sF32 vlowcut;
    sF32 vhighcut;
    syVComp cprparm;
    sU8 guicolor;
    sU8 _pad[3];
  } globals;

  V2Reverb reverb;
  V2ModDel delay;
  V2DCFilter dcf;
  V2Comp compr;
  sF32 lcfreq;    // low cut freq
  sF32 lcbuf[2];  // low cut buf l/r
  sF32 hcfreq;    // high cut freq
  sF32 hcbuf[2];  // high cut buf l/r

  bool initialized;

  // delay buffers
  sF32 maindelbuf[2][32768];
  sF32 chandelbuf[CHANS][2][2048];

  V2Instance instance;

  syWRonan ronan;

  void init(const void *patchmap, sInt samplerate)
  {
    // Ahem, so this is somewhat dubious, but we don't use
    // virtual functions or anything so it should be fine. Ahem.
    // Look away please :)
    //
    // PORT FIX: the original used sizeof(this) (== 4, a pointer!) so this
    // memset cleared only 4 bytes and the port silently relied on the caller
    // handing it zeroed memory. The ASM _synthInit zeroes the WHOLE instance
    // ("mov ecx, SYN.size / rep stosb"). Match it: sizeof(*this).
    memset((void *)this, 0, sizeof(*this)); // (see PORT FIX above)

    // era compat: the memset above would leave the era zeroed (base 0 = fr08
    // era!); default to modern. A period file must opt in via
    // synthSetSourceVersion / synthSetEra AFTER synthInit (mirrors the
    // synthSetGlobals call order in the player).
    instance.era = Era::v(V2Instance::SRCVER_MODERN);
    instance.rng.init(1);     // glibc-compatible deterministic stream
    instance.userSeed = 0;    // player seed knob (synthSetSeed)

    // set sampling rate
    this->samplerate = samplerate;
    instance.calcNewSampleRate(samplerate);
    ronanCBSetSR(&ronan, samplerate);

    // patch map
    this->patchmap = (const V2PatchMap*)patchmap;

    // init voices
    for (sInt i=0; i < POLY; i++)
    {
      chanmap[i] = -1;
      voicesw[i].init(&instance);
    }

    // init channels
    for (sInt i=0; i < CHANS; i++)
    {
      chans[i].ctl[6] = 0x7f;
      chansw[i].init(&instance, chandelbuf[i][0], chandelbuf[i][1], COUNTOF(chandelbuf[i][0]));
    }

    // global filters
    reverb.init(&instance);
    delay.init(&instance, maindelbuf[0], maindelbuf[1], COUNTOF(maindelbuf[0]));
    ronanCBInit(&ronan);
    compr.init(&instance);
    dcf.init(&instance);

    // debug plots (uncomment the ones you want)
    sInt sr_plot = 44100/10; // plot rate
    sInt sr_lfo = 800;
    sInt w = 800, h = 150;

    //DEBUG_PLOT_OPEN(&voicesw[1].osc[0], "Voice 1 VCO 0", sr_plot, w, h);
    //DEBUG_PLOT_OPEN(&voicesw[1].osc[1], "Voice 1 VCO 1", sr_plot, w, h);
    //DEBUG_PLOT_OPEN(&voicesw[1].vcf[0], "Voice 1 VCF 0", sr_plot, w, h);
    DEBUG_PLOT_OPEN(&voicesw[1].env[0], "Voice 1 Env 0", sr_lfo, w, h);
    //DEBUG_PLOT_OPEN(&voicesw[1].lfo[0], "Voice 1 LFO 0", sr_lfo, w, h);
    //DEBUG_PLOT_OPEN(&voicesw[1].dist, "Voice 1 Dist", sr_plot, w, h);
    //DEBUG_PLOT_OPEN(&voicesw[1], "Voice 1 final", sr_plot, w, h);
    //DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[0].dcf1, 0), "Chan 0 DCF1 L", sr_plot, w, h);
    //DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[0].dcf1, 1), "Chan 0 DCF1 R", sr_plot, w, h);
    //DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[0], 0), "Channel 0 L", sr_plot, w, h);
    //DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[0], 1), "Channel 0 R", sr_plot, w, h);
    //DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&instance.mixbuf, 0), "Mix L", sr_plot, w, h);
    //DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&instance.mixbuf, 1), "Mix R", sr_plot, w, h);

    initialized = true;
  }

  void render(sF32 *buf, sInt nsamples, sF32 *buf2, bool add)
  {
    sInt todo = nsamples;


    // era <v1 (fr08): the 2000 driver renders in sub-frame chunks with a
    // trailing-edge control tick. Modern keeps the whole-frame path below.
    if (instance.old(DELTA_SUBFRAME_RENDER))
    {
      renderSubFrame(buf, nsamples, buf2, add);
      DEBUG_PLOT_UPDATE();
      return;
    }

    // fragment loop - chunk everything into frames.
    while (todo)
    {
      // do we need to render a new frame?
      if (!tickd)
      {
        tick();
        pollSteal("REND"); // voice frees happen in tick()
      }

      // copy to dest buffer(s)
      const StereoSample *src = &instance.mixbuf[instance.SRcFrameSize - tickd];
      sInt nread = min(todo, tickd);
      if (!buf2) // interleaved samples
      {
        if (!add)
        {
          COVER("OUT interleaved set");
          memcpy(buf, src, nread * sizeof(StereoSample));
        }
        else
        {
          COVER("OUT interleaved add");
          for (sInt i=0; i < nread; i++)
          {
            buf[i*2+0] += src[i].l;
            buf[i*2+1] += src[i].r;
          }
        }

        buf += 2*nread;
      }
      else // buf = left, buf2 = right
      {
        if (!add)
        {
          COVER("OUT separate set");
          for (sInt i=0; i < nread; i++)
          {
            buf[i] = src[i].l;
            buf2[i] = src[i].r;
          }
        }
        else
        {
          COVER("OUT separate add");
          for (sInt i=0; i < nread; i++)
          {
            buf[i] += src[i].l;
            buf2[i] += src[i].r;
          }
        }

        buf += nread;
        buf2 += nread;
      }

      todo -= nread;
      tickd -= nread;
      dbgsmpl += nread;
    }

    DEBUG_PLOT_UPDATE();
  }

  void processMIDI(const sU8 *cmd)
  {
    while (*cmd != 0xfd) // until end of stream
    {
      if (*cmd & 0x80) // start of message
        mrstat = *cmd++;

      if (mrstat < 0x80) // we don't have a current message? uhm...
        break;

      sInt chan = mrstat & 0xf;
      switch ((mrstat >> 4) & 7)
      {
      case 1: // Note on
        if (cmd[1] != 0) // velocity==0 is actually a note off
        {
          COVER("MIDI note on");
          if (chan == CHANS-1)
            ronanCBNoteOn(&ronan);

          // calculate current polyphony for this channel
          const V2Sound *sound = getpatch(chans[chan].pgm);
          // era pool bound (DELTA_POLY_16/32): the 2000/2001 engines scan 16
          // voices, the 2002/2003 builds 32, the 2004 asm 64. When a dense
          // song saturates the period pool, the original STEALS where a
          // 64-voice scan would open a fresh voice -- allocation choices
          // diverge (proven by the flybye 66.42s alloc-trace hunt,
          // flybye-extraction/NOTES.md).
          const sInt pool = instance.voicePool();
          sInt npoly = 0;
          for (sInt i=0; i < pool; i++)
            npoly += (chanmap[i] == chan);


          // voice allocation. this is equivalent to the original V2 code,
          // but hopefully simpler to follow.
          sInt usevoice = -1;
          sInt chanmask, chanfind;

          if (!npoly || npoly < sound->maxpoly) // even if maxpoly is 0, allow at least 1.
          {
            // if we haven't reached polyphony limit yet, try to find a free voice
            // first.
            for (sInt i=0; i < pool; i++)
            {
              if (chanmap[i] < 0)
              {
                usevoice = i;
                break;
              }
            }

            // okay, need to find a free voice. we'll take any channel.
            COVER("SYN find voice any");
            chanmask = 0;
            chanfind = 0;
          }
          else
          {
            // if we're at polyphony limit, we know there's at least one voice
            // used by this channel, so we can limit ourselves to killing
            // voices from our own chan.
            // PORTING FIX (not an ASM bug): the asm steal loops (.killvoice,
            // synth.asm:5512/5535) compare the FULL chanmap entry against the
            // channel. Masking with 0xf made FREE voices (chanmap == -1,
            // -1 & 0xf == 15) eligible as "channel 15's own voices", so at the
            // poly limit on the Ronan channel the port ALLOCATED fresh voices
            // instead of stealing — polyphony grew unbounded (kkrieger6's
            // velocity-ramp swells on chan 15).
            COVER("SYN find voice channel");
            chanmask = ~0;
            chanfind = chan;
          }

          // don't have a voice yet? kill oldest eligible one with gate off.
          if (usevoice < 0)
          {
            COVER("SYN replace voice gate off");
            sU32 oldest = curalloc;
            for (sInt i=0; i < pool; i++)
            {
              if ((chanmap[i] & chanmask) == chanfind && !voicesw[i].gate && allocpos[i] < oldest)
              {
                oldest = allocpos[i];
                usevoice = i;
              }
            }
          }

          // still no voice? okay, just take the oldest one we can find, period.
          if (usevoice < 0)
          {
            COVER("SYN replace voice oldest");
            sU32 oldest = curalloc;
            for (sInt i=0; i < pool; i++)
            {
              if ((chanmap[i] & chanmask) == chanfind && allocpos[i] < oldest)
              {
                oldest = allocpos[i];
                usevoice = i;
              }
            }
          }

          // we have our voice - assign it!
          assert(usevoice >= 0);
          chanmap[usevoice] = chan;
          voicemap[chan] = usevoice;
          allocpos[usevoice] = curalloc++;

          // and note on!
          storeV2Values(usevoice);
          voicesw[usevoice].noteOn(cmd[0], cmd[1]);
          // (era <v1 mid-frame note-on voice/chorus phase is now handled
          // structurally by renderSubFrame -- the partial chunk after this
          // event renders the new voice + its channel FX naturally. The earlier
          // per-event scratch-advance hacks are subsumed and removed.)
          cmd += 2;
          break;
        }
        // fall-through (for when we had a note off)

      case 0: // Note off
        COVER("MIDI note off");
        if (chan == CHANS-1)
          ronanCBNoteOff(&ronan);

        for (sInt i=0; i < POLY; i++)
        {
          if (chanmap[i] != chan)
            continue;

          V2Voice *voice = &voicesw[i];
          if (voice->note == cmd[0] && voice->gate)
          {
            voice->noteOff();
            // The asm (ProcessNoteOff, synth.asm:5398-5400) stops at the FIRST
            // matching voice (`jmp .end`). The port looped over all POLY voices and
            // released EVERY voice holding this note, so when a note is held on more
            // than one voice of a channel (overlapping/retriggered notes) the port
            // released the extra voice(s) the asm keeps sustaining -- a slowly
            // accumulating whole-song divergence via the modulation envelopes.
            break;
          }
        }
        cmd += 2;
        break;

      case 2: // Aftertouch
        COVER("MIDI aftertouch");
        cmd++; // ignored
        break;

      case 3: // Control change
        COVER("MIDI controller change");
        {
          sInt ctrl = cmd[0];
          sU8 val = cmd[1];
          if (ctrl >= 1 && ctrl <= 7)
          {
            chans[chan].ctl[ctrl - 1] = val;
            if (chan == CHANS-1)
            {
              ronanCBSetCtl(&ronan, ctrl, val);
              // era <v6 ("FAKE 2: Lowcut!"): CC6 on the speech channel ALSO
              // drives the master high-cut, on top of storing the controller.
              // fr08 @0x40bded + candytron/RG2 _viruz2a.asm; 2004 removed it.
              // Same form as the global highcut in setGlobals. DELTA_CC6_HICUT;
              // ch15-only, so invisible to per-channel solos but it sweeps the
              // whole-mix master EQ in josie. (fr08 v0 stays exact -- it sends
              // no ch15/CC6.)
              if (ctrl == 6 && instance.old(DELTA_CC6_HICUT))
                hcfreq = sqr((val + 1.0f) / 128.0f);
            }
          }
          else if (ctrl == 120) // CC #120: all sound off
          {
            COVER("MIDI CC all sound off");
            for (sInt i=0; i < POLY; i++)
            {
              if (chanmap[i] != chan)
                continue;

              voicesw[i].init(&instance);
              chanmap[i] = -1;
            }
          }
          else if (ctrl == 123) // CC #123: all notes off
          {
            COVER("MIDI CC all notes off");
            if (chan == CHANS-1)
              ronanCBNoteOff(&ronan);

            for (sInt i=0; i < POLY; i++)
            {
              if (chanmap[i] == chan)
                voicesw[i].noteOff();
            }
          }
        }
        cmd += 2;
        break;

      case 4: // Program change
        COVER("MIDI program change");
        {
          sU8 pgm = *cmd++ & 0x7f;
          // era <v1 (fr08): the 2000 handler (@0x40be15) has NO same-program
          // check (every PGM event kills the channel's voices and resets the
          // controllers, even a reload of the same pgm), and its controller
          // reset zeroes ctl1-6 then sets ctl7 = 127 (volume back to max).
          // 2004 (synth.asm ProcessProgramChange:5685) added the same-pgm
          // early-out and dropped the ctl7 write (volume preserved). Proven
          // by fr08 @190.12s: pgm2 on ch1 resets ctl7 109->127 in the 2000,
          // feeding a ctl7->chanvol channel mod => chgain delta => constant
          // DC offset in the mix from the next ch1 note-on (192.086 s).
          // (DELTA.md)
          // did the program actually change? (era <v1: no such check)
          if (instance.old(DELTA_PGMCHANGE_V0) || chans[chan].pgm != pgm)
          {
            COVER("MIDI program change real");
            chans[chan].pgm = pgm;

            // need to turn all voices on this channel off.
            for (sInt i=0; i < POLY; i++)
            {
              if (chanmap[i] == chan)
                chanmap[i] = -1;
            }
          }

          // either way, reset controllers
          for (sInt i=0; i < 6; i++)
            chans[chan].ctl[i] = 0;
          if (instance.old(DELTA_PGMCHANGE_V0))
            chans[chan].ctl[6] = 127; // era <v1: ctl7 (volume) reset to max
        }
        break;

      case 5: // Pitch bend
        COVER("MIDI pitch bend");
        cmd += 2; // ignored
        break;

      case 6: // Poly Aftertouch
        COVER("MIDI poly aftertouch");
        cmd += 2; // ignored
        break;

      case 7: // System
        COVER("MIDI system exclusive");
        if (chan == 0xf) // Reset
          init(patchmap, samplerate);
        break; // rest ignored
      }
    }
    pollSteal("MIDI"); // voice steals/allocs happen in note-on handling above
  }

  void setGlobals(const sU8 *para)
  {
    if (!initialized)
      return;

    // copy over
    sF32 *globf = (sF32 *)&globals;
    for (sInt i=0; i < sizeof(globals)/sizeof(sF32); i++)
      globf[i] = para[i];

    // set
    reverb.set(&globals.rvbparm);
    delay.set(&globals.delparm);
    // sum-comp state is only read by its render (gated on the same row in
    // renderFrame); see V2Chan::set for the elimination rationale
    if (!instance.old(DELTA_NO_COMP_BOOST))
      compr.set(&globals.cprparm);
    lcfreq = sqr((globals.vlowcut + 1.0f) / 128.0f);
    hcfreq = sqr((globals.vhighcut + 1.0f) / 128.0f);
  }

  void getPoly(sInt *dest)
  {
    for (sInt i=0; i <= CHANS; i++)
      dest[i] = 0;

    for (sInt i=0; i < POLY; i++)
    {
      sInt chan = chanmap[i];
      if (chan < 0)
        continue;

      dest[chan]++;
      dest[CHANS]++;
    }
  }

  void getPgm(sInt *dest)
  {
    for (sInt i=0; i < CHANS; i++)
      dest[i] = chans[i].pgm;
  }

private:
  const V2Sound *getpatch(sInt pgm) const
  {
    assert(pgm >= 0 && pgm < 128);
    return (const V2Sound *)&patchmap->raw_data[patchmap->offsets[pgm]];
  }

  sF32 getmodsource(const V2Voice *voice, sInt chan, sInt source) const
  {
    sF32 in = 0.0f;

    switch (source)
    {
    case 0: // velocity
      COVER("MOD src vel");
      in = voice->velo;
      break;

    case 1: case 2: case 3: case 4: case 5: case 6: case 7: // controller value
      COVER("MOD src ctl");
      in = chans[chan].ctl[source-1];
      break;

    case 8: case 9: // EG output
      COVER("MOD src EG");
      in = voice->env[source-8].out;
      break;

    case 10: case 11: // LFO output
      COVER("MOD src LFO");
      in = voice->lfo[source-10].out;
      break;

    default: // note
      COVER("MOD src note");
      in = 2.0f * (voice->note - 48.0f);
      break;
    }

    return in;
  }

  void storeV2Values(sInt vind)
  {
    assert(vind >= 0 && vind < POLY);
    sInt chan = chanmap[vind];
    if (chan < 0)
      return;

    // get patch definition
    const V2Sound *patch = getpatch(chans[chan].pgm);

    // voice data
    syVV2 *vpara = &voicesv[vind];
    sF32 *vparaf = (sF32 *)vpara;
    V2Voice *voice = &voicesw[vind];
    
    // copy voice dependent data
    for (sInt i=0; i < COUNTOF(patch->voice); i++)
      vparaf[i] = (sF32)patch->voice[i];

    // modulation matrix
    for (sInt i=0; i < patch->modnum; i++)
    {
      const V2Mod *mod = &patch->modmatrix[i];
      if (mod->dest >= COUNTOF(patch->voice))
        continue;

      sF32 scale = (mod->val - 64.0f) / 64.0f;
      vparaf[mod->dest] = clamp(vparaf[mod->dest] + scale*getmodsource(voice, chan, mod->source), 0.0f, 128.0f);
    }

    voice->set(vpara);

#ifndef NDEBUG
    // dev-only: dump a channel's post-modulation voice config once (V2_PATCHDUMP=ch)
    { static int pd=-2; if(pd==-2){const char*e=getenv("V2_PATCHDUMP"); pd=e?atoi(e):-1;}
      static int shown[16]={0};
      if(pd>=0 && chan==pd && chan<16 && !shown[chan]){ shown[chan]=1;
        fprintf(stderr,"[patch ch%d pgm%d] routing=%.0f fltbal=%.0f oscsync=%.0f panning=%.0f transp=%.0f\n",
                chan, chans[chan].pgm, vpara->routing, vpara->fltbal, vpara->oscsync, vpara->panning, vpara->transp);
        for(int i=0;i<syVV2::NOSC;i++) fprintf(stderr,"  osc%d mode=%.0f ring=%.0f pitch=%.1f detune=%.1f color=%.0f gain=%.0f\n",
                i, vpara->osc[i].mode, vpara->osc[i].ring, vpara->osc[i].pitch, vpara->osc[i].detune, vpara->osc[i].color, vpara->osc[i].gain);
        for(int i=0;i<syVV2::NFLT;i++) fprintf(stderr,"  flt%d mode=%.0f cutoff=%.0f reso=%.0f\n",
                i, vpara->flt[i].mode, vpara->flt[i].cutoff, vpara->flt[i].reso);
        fprintf(stderr,"  dist mode=%.0f ingain=%.0f p1=%.0f p2=%.0f\n",
                vpara->dist.mode, vpara->dist.ingain, vpara->dist.param1, vpara->dist.param2);
        fprintf(stderr,"  modnum=%d (dest indices: osc0.pitch=4 osc1.pitch=10 osc2.pitch=16; flt0.cut=21 flt1.cut=24)\n", patch->modnum);
        for(int i=0;i<patch->modnum;i++){ const V2Mod*m=&patch->modmatrix[i];
          fprintf(stderr,"    mod%d src=%d val=%d dest=%d\n", i, m->source, m->val, m->dest); }
      } }
#endif
  }

  void storeChanValues(sInt chan)
  {
    assert(chan >= 0 && chan < CHANS);

    // get patch definition
    const V2Sound *patch = getpatch(chans[chan].pgm);

    // chan data
    syVChan *cpara = &chansv[chan];
    sF32 *cparaf = (sF32 *)cpara;
    V2Chan *cwork = &chansw[chan];
    V2Voice *voice = &voicesw[voicemap[chan]];

    // copy channel dependent data
    for (sInt i=0; i < COUNTOF(patch->chan); i++)
      cparaf[i] = (sF32)patch->chan[i];

    // modulation matrix
    for (sInt i=0; i < patch->modnum; i++)
    {
      const V2Mod *mod = &patch->modmatrix[i];

      // era <v5: the CHANNEL mod loop only applies sources < 8 -- velocity (0) +
      // ctl1..7 (1..7). Sources >= 8 (aenv/env2, lfo1/lfo2, note) are voice-
      // PRIVATE and have no channel-level meaning, so the v3/v4 store skips them
      // for channel params (fr014 syChanSet @0x40fe39 `cmp al,8; jae`; fr019 v4
      // @0x429900). The modern-core v5+ store applies ALL sources. Without this
      // gate, fr-014 ch8 (pgm7) wrongly let `aenv -> comp.outgain` push outgain
      // 98 -> 128 (clamp) for ~3.7x extra compressor makeup, plus `lfo1 ->
      // chorus.amount` -- the ~5x ch8 divergence vs the v3 render oracle.
      // (DELTA_CHANMOD_NO_VOICE_SRC, flipsAt 5; the v5/v6 corpus carries such
      // mods and stays bit-exact only because v5+ applies them.)
      if (instance.old(DELTA_CHANMOD_NO_VOICE_SRC) && mod->source >= 8)
        continue;

      sInt dest = mod->dest - COUNTOF(patch->voice);
      if (dest < 0 || dest >= COUNTOF(patch->chan))
        continue;

      sF32 scale = (mod->val - 64.0f) / 64.0f;
      cparaf[dest] = clamp(cparaf[dest] + scale*getmodsource(voice, chan, mod->source), 0.0f, 128.0f);
    }

#ifndef NDEBUG
    // V2_CHANPARM=<ch>: dump that channel's POST-MOD param array (cparaf, the
    // storeChanValues output == the asm 0x510384+ch*0x64 value array) + the
    // decoded comp config, printed whenever pgm or comp.outgain changes. The
    // tap that localized the fr-014 ch8 divergence to aenv->comp.outgain.
    if (getenv("V2_CHANPARM") && chan == atoi(getenv("V2_CHANPARM"))) {
      static int lastpgm=-99; static float lastog=-99;
      if (chans[chan].pgm != lastpgm || cpara->comp.outgain != lastog) {
        lastpgm = chans[chan].pgm; lastog = cpara->comp.outgain;
        fprintf(stderr,"[chanparm] ch%d pgm=%d COUNTOF(chan)=%d | comp: mode=%.0f stereo=%.0f auto=%.0f lkah=%.0f thr=%.0f rat=%.0f atk=%.0f rel=%.0f outg=%.0f\n",
                chan, chans[chan].pgm, (int)COUNTOF(patch->chan),
                cpara->comp.mode, cpara->comp.stereo, cpara->comp.autogain, cpara->comp.lookahead,
                cpara->comp.threshold, cpara->comp.ratio, cpara->comp.attack, cpara->comp.release, cpara->comp.outgain);
        fprintf(stderr,"[chanparm] ch%d raw cparaf:", chan);
        for (int i=0;i<(int)COUNTOF(patch->chan);i++) fprintf(stderr," %.0f", cparaf[i]);
        fprintf(stderr,"\n");
      }
    }
#endif
    cwork->set(cpara);
  }

  void tick()
  {
    // voices
    for (sInt i=0; i < POLY; i++)
    {
      if (chanmap[i] < 0)
        continue;

      if (instance.old(DELTA_TICK_BEFORE_SET))
      {
        // era <v1 (fr08): the 2000 frame tick runs the voice TICK (env/lfo
        // step + volramp, @0x40ad06) BEFORE the voice SET (@0x40af88). So a
        // tick always steps with the params of the PREVIOUS set, and
        // modsource changes (velocity at noteon! ctls, env/lfo-sourced mods)
        // reach the sub-objects one frame later than modern. Concretely:
        // fr08 ch10 has env-gain = velocity-mod with base 0, so the first
        // tick after noteon still sees gain 0 -> the note's first frame is
        // SILENT in the 2000 binary (proven by the C1 tick log: out=0 st=1
        // at tick#1, out=2*a*g at tick#2). Modern order made it ramp one
        // frame early -- this was the residual voice-level delta. (DELTA.md)
        voicesw[i].tick();
        if (voicesw[i].env[0].state == V2Env::OFF)
        {
          chanmap[i] = -1;
          continue;
        }
        storeV2Values(i);
        continue;
      }

      storeV2Values(i);
      voicesw[i].tick();

      // if EG1 finished, turn off voice
      if (voicesw[i].env[0].state == V2Env::OFF)
        chanmap[i] = -1;
    }

    // chans
    for (sInt i=0; i < CHANS; i++)
      storeChanValues(i);

    ronanCBTick(&ronan);
    tickd = instance.SRcFrameSize;
    renderFrame(instance.SRcFrameSize);

#if COVERAGE
    // print coverage updates as they happen
    static int cur_frame;
    static const char *old_coverage[COUNTOF(code_coverage)];
    sInt ccount = synthGetNumCoverage();
    for (sInt i=0; i < ccount; i++)
    {
      if (old_coverage[i] != code_coverage[i])
      {
        old_coverage[i] = code_coverage[i];
        printf("[%5d,%3d] %s\n", cur_frame, i, code_coverage[i]);
      }
    }
    cur_frame++;
#endif
  }

  // era <v1 (fr08): the per-frame CONTROL update only (env/lfo step + volramp +
  // store, channel store, ronan), WITHOUT renderFrame. The 2000 driver
  // @0x40b95c runs this when the control-frame counter hits 0 (the TRAILING
  // edge of the render that filled the frame), then renders the frame's samples
  // in sub-frame chunks @0x40ba10. Splitting control-tick from render is what
  // makes the period sub-frame timing exact (see renderSubFrame). (DELTA.md)
  void controlTick()
  {
    for (sInt i=0; i < POLY; i++)
    {
      if (chanmap[i] < 0)
        continue;
      // eraV0 order: TICK (env/lfo/volramp) then SET (modmatrix), so a tick
      // steps with the previous frame's params (DELTA.md TICK-before-SET).
      voicesw[i].tick();
      if (voicesw[i].env[0].state == V2Env::OFF)
      {
        chanmap[i] = -1;
        continue;
      }
      storeV2Values(i);
    }

    for (sInt i=0; i < CHANS; i++)
      storeChanValues(i);

    ronanCBTick(&ronan);
  }

  // era <v1 sub-frame render: render `todo` samples to buf in chunks that never
  // cross a 256-control-frame boundary, doing controlTick() at each boundary
  // (and at the trailing edge before returning, so the next call's render --
  // and any ProcessMIDI the player runs in between -- sees the pre-event tick).
  // This is the unifying fix for every "sub-frame" era delta: mid-frame note-on
  // voice/chorus phase AND frame-aligned control-tick edge. Mirrors the 2000
  // driver @0x40b95c (frame counter) + @0x40ba10 (per-chunk voices+channels+
  // global FX). Modern (>=v1) keeps the whole-frame path in render().
  void renderSubFrame(sF32 *buf, sInt nsamples, sF32 *buf2, bool add)
  {
    sInt todo = nsamples;
    while (todo)
    {
      if (subRemain == 0)
      {
        controlTick();
        subRemain = instance.SRcFrameSize;
      }
      sInt chunk = min(todo, subRemain);

      renderFrame(chunk); // renders `chunk` samples (channels + global FX) into mixbuf[0..chunk)

      const StereoSample *src = instance.mixbuf;
      if (!buf2) // interleaved
      {
        if (!add)
          memcpy(buf, src, chunk * sizeof(StereoSample));
        else
          for (sInt i=0; i < chunk; i++) { buf[i*2+0] += src[i].l; buf[i*2+1] += src[i].r; }
        buf += 2*chunk;
      }
      else // separate L/R
      {
        if (!add)
          for (sInt i=0; i < chunk; i++) { buf[i] = src[i].l; buf2[i] = src[i].r; }
        else
          for (sInt i=0; i < chunk; i++) { buf[i] += src[i].l; buf2[i] += src[i].r; }
        buf += chunk; buf2 += chunk;
      }

      todo -= chunk;
      subRemain -= chunk;
    }
    // trailing-edge control tick (matches the 2000: tick + reload happen before
    // the driver returns, so a boundary-aligned event the player processes next
    // lands AFTER this tick -- the frame that just closed used pre-event ctls).
    if (subRemain == 0)
    {
      controlTick();
      subRemain = instance.SRcFrameSize;
    }
  }

  void renderFrame(sInt nsamples)
  {
    // (eraV0 sub-frame path passes a partial count; modern passes SRcFrameSize)


    // clear output buffer
    memset(instance.mixbuf, 0, nsamples * sizeof(StereoSample));

    // clear aux buffers
    memset(instance.aux1buf, 0, nsamples * sizeof(sF32));
    memset(instance.aux2buf, 0, nsamples * sizeof(sF32));
    memset(instance.auxabuf, 0, nsamples * sizeof(StereoSample));
    memset(instance.auxbbuf, 0, nsamples * sizeof(StereoSample));


    // process all channels
    for (sInt chan=0; chan < CHANS; chan++)
    {
      // check if any voices are active on this channel
      sInt voice = 0;
      while (voice < POLY && chanmap[voice] != chan)
        voice++;

      if (voice == POLY)
        continue;

#ifndef NDEBUG
      if (getenv("KK_MUTECH") && (atoi(getenv("KK_MUTECH")) == chan))
        continue;
#endif

      // clear channel buffer
      memset(instance.chanbuf, 0, nsamples * sizeof(StereoSample));

      // render all voices on this channel
      for (; voice < POLY; voice++)
      {
        if (chanmap[voice] != chan)
          continue;

        voicesw[voice].render(instance.chanbuf, nsamples);
      }
#ifndef NDEBUG
      if (getenv("V2_MIXTAP")) {
        sF32 mx=0.0f; for (sInt i=0;i<nsamples;i++){ sF32 a=fabsf(instance.chanbuf[i].l),b=fabsf(instance.chanbuf[i].r); if(a>mx)mx=a; if(b>mx)mx=b; }
        sInt nv=0; for (sInt v=0; v<POLY; v++) if (chanmap[v]==chan) nv++;
        if (mx>0.5f) fprintf(stderr,"[chan%d] voicesum=%.5f nvoices=%d\n", chan, mx, nv);
      }
#endif

      // channel 15 -> Ronan (speech vocal tract). The render-time routing did
      // not exist in the early core: at v0 (fr08) ch15 is an ordinary music
      // channel and the 2000 render makes no ronan calls -- routing it through
      // the idle vocal tract silenced fr08's ch15 layer (proven: Ronan-off is
      // bit-exact to the C1 oracle). Gate so only v5+ articulates speech here.
      if (chan == CHANS-1 && !instance.old(DELTA_NO_RONAN_CHANNEL))
        ronanCBProcess(&ronan, &instance.chanbuf[0].l, nsamples);

#ifndef NDEBUG
      // V2_CHANTRACE: per-channel pre/post-process peak + the channel comp's
      // runtime mode/invol/outvol/net/curgain + fxr. Pairs with V2_CHANPARM
      // (post-mod param array) to localize a channel-FX-chain divergence.
      sF32 ctin = 0.0f;
      if (getenv("V2_CHANTRACE")) { for (sInt i=0;i<nsamples;i++){ sF32 a=fabsf(instance.chanbuf[i].l),b=fabsf(instance.chanbuf[i].r); if(a>ctin)ctin=a; if(b>ctin)ctin=b; } }
#endif
      chansw[chan].process(nsamples, ((instance.chanMute >> chan) & 1) != 0, chan);
#ifndef NDEBUG
      if (getenv("V2_CHANTRACE")) {
        sF32 cto=0.0f; for (sInt i=0;i<nsamples;i++){ sF32 a=fabsf(instance.chanbuf[i].l),b=fabsf(instance.chanbuf[i].r); if(a>cto)cto=a; if(b>cto)cto=b; }
        if (ctin>0.4f || cto>0.4f) {
          V2Comp &cp = chansw[chan].comp;
          fprintf(stderr,"[chantrace] ch%d in=%.4f out=%.4f | comp mode=%d invol=%.4f outvol=%.4f net=%.4f curgain=%.3f fxr=%d\n",
                  chan, ctin, cto, cp.mode, cp.invol, cp.outvol, cp.invol*cp.outvol, cp.curgain[0], chansw[chan].fxr);
        }
      }
#endif
    }

    // global filters
    StereoSample *mix = instance.mixbuf;
    MIXTAP_SNAP(premix, mix, nsamples);   // dry channel sum, before any global FX
    reverb.render(mix, nsamples);
    MIXTAP_SNAP(reverb, mix, nsamples);
    delay.renderAux2Main(mix, nsamples);
    MIXTAP_SNAP(delay, mix, nsamples);
    // era <v1 (fr08): NO master DC filter -- fcdcflt (126.0f) appears nowhere
    // in the 2000 image, and the 2000 mix @0x40b7ba goes straight to the
    // parametric lc/hc EQ. The ungated ~20Hz one-pole here was the dominant
    // ch10 residual: +13deg phase / -0.22dB at the 88Hz bass fundamental
    // (the per-harmonic phase fit is exactly 1-126/SR). (DELTA.md)
    if (!instance.old(DELTA_NO_MASTER_DCF))
    {
      dcf.renderStereo(mix, mix, nsamples);
      MIXTAP_SNAP(dcf, mix, nsamples);
    }

    // low cut/high cut
    sF32 lcf = lcfreq, hcf = hcfreq;
    for (sInt i=0; i < nsamples; i++)
    {
      for (sInt ch=0; ch < 2; ch++)
      {
        // low cut
        sF32 x = mix[i].ch[ch] - lcbuf[ch];
        lcbuf[ch] += lcf * x;

        // high cut
        if (hcf != 1.0f)
        {
          hcbuf[ch] += hcf * (x - hcbuf[ch]);
          x = hcbuf[ch];
        }

        mix[i].ch[ch] = x;
      }
    }

    MIXTAP_SNAP(lchc, mix, nsamples);

    // sum compressor. era <v1 (fr08): the 2000 mix path @0x40baf8 ends right
    // after the lc/hc EQ (`ret` @0x40bb8b) -- there is NO global sum compressor
    // (it's a v1+ feature with no code in the v0 binary; conv2m defaults it
    // Off). The port's compressor is a near-no-op when off, but its envelope
    // follower still drifts by ~1 ULP on louder material, leaving a decaying
    // transient (ch10: a ctl7 swell trips it -> ~8e-4 ringing down over the
    // release). Gate it out under eraV0. (DELTA.md)
    if (!instance.old(DELTA_NO_COMP_BOOST))
    {
      compr.render(mix, nsamples);
      MIXTAP_SNAP(compr, mix, nsamples);
    }

    DEBUG_PLOT_STEREO(mix, mix, nsamples);

  }
};

// --------------------------------------------------------------------------
// C-style interface
// --------------------------------------------------------------------------

unsigned int __stdcall synthGetSize()
{
  return sizeof(V2Synth);
}

void __stdcall synthInit(void *pthis, const void *patchmap, int samplerate)
{
  ((V2Synth *)pthis)->init(patchmap, samplerate);
}

void __stdcall synthRender(void *pthis, void *buf, int smp, void *buf2, int add)
{
  ((V2Synth *)pthis)->render((sF32 *)buf, smp, (sF32 *)buf2, add != 0);
}

void __stdcall synthProcessMIDI(void *pthis, const void *ptr)
{
  ((V2Synth *)pthis)->processMIDI((const sU8 *)ptr);
}

void __stdcall synthSetGlobals(void *pthis, const void *ptr)
{
  ((V2Synth *)pthis)->setGlobals((const sU8 *)ptr);
}


// Apply the side effects of a just-set era (the inst.era field is already
// updated). Shared by synthSetSourceVersion and synthSetEra. Call after
// synthInit (which sets the modern 128-sample frame); we recompute the
// control-frame size and re-seed here.
static void synthApplyEra(V2Synth *syn)
{
  V2Instance &inst = syn->instance;
  // ROOT delta (fr08-extraction/DELTA.md): the year-2000 synth ran the control
  // rate at a 256-sample frame (driver resets the frame counter to 0x100), vs
  // 128 in 2004. This single change is the root of the envelope decay/release
  // shaping (calcfreq x10 over 256 == ~calcfreq2 x11 over 128, kb's transEnv
  // sqrt relationship) AND the volume-ramp coeff (1/256 vs 1/128 = 1/frame).
  // Voices read SRcFrameSize / SRfciframe live, so overriding here (post-init,
  // pre-render) retimes env/LFO/volramp without touching the SR constants.
  if (inst.old(DELTA_FRAME256))
  {
    inst.SRcFrameSize = 256;
    inst.SRfciframe   = 1.0f / 256.0f;
  }
  // Matched-seed A/B (DELTA.md D6): the C1 ground truth pins rdtsc=0, so all
  // osc-noise / LFO-S&H seeds start at 0. The voices were init'd during
  // synthInit (before the era was set), so re-seed them here; keysync
  // noteOns re-init through the era-gated init path and seed 0 on their own.
  if (inst.old(DELTA_RDTSC_SEED))
    for (sInt v=0; v < V2Synth::POLY; v++)
    {
      for (sInt o=0; o < syVV2::NOSC; o++) syn->voicesw[v].osc[o].nseed = 0u;
      for (sInt l=0; l < syVV2::NLFO; l++) syn->voicesw[v].lfo[l].nseed = 0u;
    }
}

void __stdcall synthSetSourceVersion(void *pthis, int srcver)
{
  // era compat (see V2Instance::era). Shorthand: a plain format-version base
  // with no ledger overrides.
  V2Synth *syn = (V2Synth *)pthis;
  syn->instance.era = Era::v(srcver);
  synthApplyEra(syn);
}

void __stdcall synthSetEra(void *pthis, int base,
                           unsigned int overridden, unsigned int forcedNew)
{
  // era compat (richer): the full Era coordinate, base + override masks.
  V2Synth *syn = (V2Synth *)pthis;
  syn->instance.era = Era{ base, (uint32_t)overridden, (uint32_t)forcedNew };
  synthApplyEra(syn);
}

// determinism knob (design D6): replaces the historical rdtsc seeding. 0 =
// the deterministic reference (asm seed table / C1 pinned-rdtsc convention).
// Call after synthInit, before rendering.
void __stdcall synthSetSeed(void *pthis, unsigned long long seed)
{
  ((V2Synth *)pthis)->instance.userSeed = (sU64)seed;
}

void __stdcall synthSetChanMute(void *pthis, unsigned int muteMask)
{
  ((V2Synth *)pthis)->instance.chanMute = (sU32)muteMask;
}

void __stdcall synthGetPoly(void *pthis, void *dest)
{
  ((V2Synth *)pthis)->getPoly((sInt*)dest);
}

void __stdcall synthGetChannelPeaks(void *pthis, float *dest16)
{
  V2Synth *s = (V2Synth *)pthis;
  for (int i = 0; i < 16; i++) { dest16[i] = s->instance.chanPeak[i]; s->instance.chanPeak[i] = 0.0f; }
}

void __stdcall synthGetPgm(void *pthis, void *dest)
{
  ((V2Synth *)pthis)->getPgm((sInt*)dest);
}

void __stdcall synthSetVUMode(void *, int)
{
  // nyi
}

void __stdcall synthGetChannelVU(void *, int, float *, float *)
{
  // nyi
}

void __stdcall synthGetMainVU(void *, float *, float *)
{
  // nyi
}

long __stdcall synthGetFrameSize(void *pthis)
{
  return ((V2Synth *)pthis)->instance.SRcFrameSize;
}


extern "C" void * __stdcall synthGetSpeechMem(void *pthis)
{
  return &((V2Synth *)pthis)->ronan;
}

#if COVERAGE

void synthPrintCoverage()
{
  int end = synthGetNumCoverage();
  printf("synth coverage:\n");
  for (int i=0; i < end; i++)
    printf("[%3d] %s\n", i, code_coverage[i] ? code_coverage[i] : "<not hit>");
}

static int synthGetNumCoverage()
{
  return __COUNTER__;
}

#endif

// vim: sw=2:sts=2:et:cino=\:0l1g0(0
