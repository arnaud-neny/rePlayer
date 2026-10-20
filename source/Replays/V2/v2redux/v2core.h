// v2core -- portable V2 synth engine (fork of the lab's synth_core.cpp with
// the proven faithful-branch semantics; see the change design D1).
// Provides the type aliases the engine source uses (64-bit-clean stdint
// versions of the original types.h) and the engine entry points.

#ifndef V2CORE_H_
#define V2CORE_H_

#include <stdint.h>

// the legacy headers guard their own typedef blocks with V2TYPES
#define V2TYPES

// the engine source carries the original __stdcall annotations; they are
// Windows-ABI noise everywhere else
#ifndef _WIN32
#define __stdcall
#endif

typedef int       sInt;
typedef unsigned  sUInt;
typedef sInt      sBool;
typedef char      sChar;

typedef int8_t    sS8;
typedef int16_t   sS16;
typedef int32_t   sS32;   // original: signed long (32-bit only in -m32 labs)
typedef int64_t   sS64;
typedef uint8_t   sU8;
typedef uint16_t  sU16;
typedef uint32_t  sU32;   // original: unsigned long; MUST be 32-bit (wraparound semantics)
typedef uint64_t  sU64;

typedef float     sF32;
typedef double    sF64;

#define sTRUE  1
#define sFALSE 0

template<class T> inline T sMin(const T a, const T b) { return (a < b) ? a : b; }
template<class T> inline T sMax(const T a, const T b) { return (a > b) ? a : b; }
template<class T> inline T sClamp(const T x, const T min, const T max) { return sMax(min, sMin(max, x)); }

// ---------------------------------------------------------------------------
// engine entry points (the original synth.h C API, per-instance via pthis;
// pthis = caller-provided zero-init buffer of synthGetSize() bytes)
// ---------------------------------------------------------------------------
extern "C"
{
  unsigned int __stdcall synthGetSize();

  void __stdcall synthInit(void *pthis, const void *patchmap, int samplerate = 44100);
  void __stdcall synthRender(void *pthis, void *buf, int smp, void *buf2 = 0, int add = 0);
  void __stdcall synthProcessMIDI(void *pthis, const void *ptr);
  void __stdcall synthSetGlobals(void *pthis, const void *ptr);

  // era compat: the v2m FORMAT VERSION the song data was originally authored
  // as (0..6); gates period DSP behaviors via the v2eras.h ledger. Call after
  // synthInit (init resets to modern). Shorthand for synthSetEra(pthis, ver,0,0).
  void __stdcall synthSetSourceVersion(void *pthis, int srcver);

  // era compat (richer): set the full Era coordinate -- a format-version base
  // plus the v2eras.h Era override masks (overridden/forcedNew). Lets a caller
  // pin a build that the format version cannot express (e.g. eras::kkrieger2004
  // = base 5 with NATIVE_FSIN/FPATAN forced New). Call after synthInit.
  void __stdcall synthSetEra(void *pthis, int base,
                             unsigned int overridden, unsigned int forcedNew);

  // determinism knob: replaces the historical rdtsc seeding (0 = reference)
  void __stdcall synthSetSeed(void *pthis, unsigned long long seed);

  // interactive channel mute (display/playback knob; NOT part of the determinism
  // contract). bit ch set = muted -- the channel still renders but feeds no
  // output bus. Default (and synthInit) = 0 (all audible). Re-apply after
  // synthInit, like the seed.
  void __stdcall synthSetChanMute(void *pthis, unsigned int muteMask);

  void __stdcall synthGetPoly(void *pthis, void *dest);
  // per-channel output peak meter (display-only): copies 16 floats into dest
  // (peak since the last call) and clears the accumulators. Read-and-clear.
  void __stdcall synthGetChannelPeaks(void *pthis, float *dest16);
  void __stdcall synthGetPgm(void *pthis, void *dest);
  void __stdcall synthSetVUMode(void *pthis, int mode);
  void __stdcall synthGetChannelVU(void *pthis, int ch, float *l, float *r);
  void __stdcall synthGetMainVU(void *pthis, float *l, float *r);
  long __stdcall synthGetFrameSize(void *pthis);
  void *__stdcall synthGetSpeechMem(void *pthis);
  void __stdcall synthSetLyrics(void *pthis, const char **ptr); // no-op without RONAN
}

#endif // V2CORE_H_
