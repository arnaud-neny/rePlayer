// v2load -- native v2m loading: per-version format tables + (4.2) version
// detection + (4.3) canonicalization to the v6 patch layout.
//
// PROVENANCE: the tables below are transcribed from the parameter version
// annotations in v2/sounddef.h (v2parms[].version, v2gparms[].version) and
// the canonical defaults (v2initsnd, v2initglobs). That file is the single
// source of truth: kb's sdInit() derives the per-version patch/global sizes
// from exactly these annotations, and v2mconv's CheckV2MVersion/ConvertV2M
// detect and upgrade files with them. test/tablecheck.cpp includes the lab's
// sounddef.h read-only and asserts every row here matches -- run it whenever
// sounddef.h changes (it can't change: it shipped in 2004 -- the assert
// guards OUR transcription).
//
// Format-version vocabulary (same as the lab):
//   patch  = [params present at <=ver] [modnum:1] [modnum * 3 bytes]
//   patchSize(ver)  = paramCount + 1 + 255*3   (sdInit's v2vsizes[] -- the
//                     MAX size; actual stored patches use their real modnum)
//   globalSize(ver) = global param count        (sdInit's v2gsizes[])
// Version detection needs BOTH sizes: v3 and v4 have equal patch sizes (the
// v4 addition is the reverb lowcut GLOBAL) and differ only in globalSize.

#ifndef V2LOAD_H_
#define V2LOAD_H_

#include "v2redux.h" // Result, V2_VER_MIN/MAX
#include <stddef.h>

namespace v2redux {

enum {
  kNumPatchParms  = 89,  // sounddef.h v2nparms
  kNumGlobalParms = 23,  // sounddef.h v2ngparms
  kMaxMods        = 255, // mod list length byte
  kPatchSlack     = 1 + kMaxMods*3, // modnum byte + max mod data
  kMaxFormatVer   = 6,   // sounddef.h: max version annotation (= v2version)
};

// sounddef.h v2parms[i].version, in table order. A param is present in a
// format-v file iff its entry is <= v.
inline constexpr unsigned char kPatchParmVer[kNumPatchParms] = {
  // Voice: Panning, Txpose(v2!)
  0, 2,
  // Osc 1: Mode, Ringmod(v2), Txpose, Detune, Color, Volume
  0, 2, 0, 0, 0, 0,
  // Osc 2: same shape
  0, 2, 0, 0, 0, 0,
  // Osc 3: same shape
  0, 2, 0, 0, 0, 0,
  // VCF 1: Mode, Cutoff, Reso
  0, 0, 0,
  // VCF 2: Mode, Cutoff, Reso
  0, 0, 0,
  // Routing, Balance(v3)
  0, 3,
  // Voice Dist: Mode, InGain, Param1, Param2
  0, 0, 0, 0,
  // Amp EG: Attack, Decay, Sustain, SusTime, Release, Amplify
  0, 0, 0, 0, 0, 0,
  // EG 2: same shape
  0, 0, 0, 0, 0, 0,
  // LFO 1: Mode, KeySync, EnvMode, Rate, Phase, Polarity(v5), Amplify
  0, 0, 0, 0, 0, 5, 0,
  // LFO 2: same shape
  0, 0, 0, 0, 0, 5, 0,
  // Globals: KeySync, ChanVol, AuxARecv(v6), AuxBRecv(v6), AuxASend(v6),
  //          AuxBSend(v6), Reverb, Delay, FXRoute, Boost(v1)
  0, 0, 6, 6, 6, 6, 0, 0, 0, 1,
  // Channel Dist: Mode, InGain, Param1, Param2
  0, 0, 0, 0,
  // Chorus/Flanger: Amount, FeedBk, DelayL, DelayR, MRate, MDepth, MPhase
  0, 0, 0, 0, 0, 0, 0,
  // Compressor (all v1): Mode, Couple, AutoGain, LkAhead, Threshd, Ratio,
  //                      Attack, Release, OutGain
  1, 1, 1, 1, 1, 1, 1, 1, 1,
  // MaxPoly
  0,
};

// sounddef.h v2gparms[i].version, in table order.
inline constexpr unsigned char kGlobalParmVer[kNumGlobalParms] = {
  // Reverb: Time, HighCut, LowCut(v4!), Volume
  0, 0, 4, 0,
  // Stereo Delay: Volume, FeedBk, DelayL, DelayR, MRate, MDepth, MPhase
  0, 0, 0, 0, 0, 0, 0,
  // Post Filters: LowCut, HighCut
  0, 0,
  // Sum Compressor (all v1)
  1, 1, 1, 1, 1, 1, 1, 1, 1,
  // Gui Features (v6, deprecated "we dont go to ravenholm" byte)
  6,
};

// sounddef.h v2initsnd[0..88] -- the canonical defaults inserted for params
// a pre-v6 file doesn't carry (value-preserving upgrade, proven by fr08).
inline constexpr unsigned char kPatchDefaults[kNumPatchParms] = {
  64, 64,                            // Panning, Transpose
  1, 0, 64, 64, 0, 127,              // osc1: simple sawtooth
  0, 0, 64, 64, 32, 127,             // osc2: off
  0, 0, 64, 64, 32, 127,             // osc3: off
  1, 127, 0,                         // vcf1: low pass, open
  0, 64, 0,                          // vcf2: off
  0,                                 // routing: single
  64,                                // filter balance: mid
  0, 32, 0, 64,                      // VoiceDist: off
  0, 64, 127, 64, 80, 0,             // env1: pling
  0, 64, 127, 64, 80, 64,            // env2: pling
  1, 1, 0, 64, 2, 0, 0,              // lfo1
  1, 1, 0, 64, 2, 0, 127,            // lfo2
  0,                                 // oscsync
  0,                                 // chanvol
  0, 0,                              // aux a/b recv
  0, 0,                              // aux a/b send
  0, 0,                              // aux1/2 sends (reverb/delay)
  0,                                 // FXRoute
  0,                                 // Boost
  0, 32, 100, 64,                    // ChanDist: off
  64, 64, 32, 32, 0, 0, 64,          // Chorus/Flanger: off
  0, 0, 1, 2, 90, 32, 20, 64, 64,    // Compressor: off
  1,                                 // maxpoly
};

// sounddef.h v2initglobs[] -- canonical global defaults. NOTE the reverb
// LowCut default is 32 (nonzero): a pre-v4 file canonicalized with this
// value MUST NOT run the 2004 reverb lowcut stage -- that is exactly the
// DELTA_NO_RVB_LOWCUT engine gate (v2eras.h).
inline constexpr unsigned char kGlobalDefaults[kNumGlobalParms] = {
  64, 64, 32, 127,                   // Reverb (Time, HighCut, LowCut, Volume)
  100, 80, 64, 64, 0, 0, 64,         // Delay
  0, 127,                            // lc/hc post filters
  0, 0, 1, 2, 90, 32, 20, 64, 64,    // Sum compressor: off
  0,                                 // gui color
};

// param counts per format version -- the same computation as sdInit()
// (count of annotations <= ver), evaluated at compile time.
constexpr int patchParmCount(int ver)
{
  int n = 0;
  for (int i = 0; i < kNumPatchParms; i++)
    if (kPatchParmVer[i] <= ver) n++;
  return n;
}

constexpr int globalSize(int ver) // == sdInit's v2gsizes[ver]
{
  int n = 0;
  for (int i = 0; i < kNumGlobalParms; i++)
    if (kGlobalParmVer[i] <= ver) n++;
  return n;
}

constexpr int patchSize(int ver) // == sdInit's v2vsizes[ver] (max size)
{
  return patchParmCount(ver) + kPatchSlack;
}

// derived table sanity (hand-checked against sdInit's printf output shape;
// v3/v4 patch sizes are EQUAL -- globalSize disambiguates)
static_assert(patchParmCount(0) == 68 && globalSize(0) == 12, "v0 sizes");
static_assert(patchParmCount(1) == 78 && globalSize(1) == 21, "v1 sizes");
static_assert(patchParmCount(2) == 82 && globalSize(2) == 21, "v2 sizes");
static_assert(patchParmCount(3) == 83 && globalSize(3) == 21, "v3 sizes");
static_assert(patchParmCount(4) == 83 && globalSize(4) == 22, "v4 sizes");
static_assert(patchParmCount(5) == 85 && globalSize(5) == 22, "v5 sizes");
static_assert(patchParmCount(6) == 89 && globalSize(6) == 23, "v6 sizes");
static_assert(patchSize(6) == 89 + 1 + 255*3, "v6 patch size = v2soundsize");

// ---------------------------------------------------------------------------
// loader API (v2load.cpp)
// ---------------------------------------------------------------------------

struct V2LoadResult {
  Result result;       // OK / BadFile / UnsupportedVersion
  int version;         // detected format version (0..6); -1 if undeterminable.
                       // On UnsupportedVersion this still reports what was
                       // detected (outside the compiled [V2_VER_MIN,V2_VER_MAX]).
  unsigned char *data; // canonicalized NEWEST-layout v2m (malloc'd; caller
                       // frees). Non-null only when result == OK.
  size_t size;
};

// Detect the v2m format version by structural fingerprint (globals size +
// per-patch size consistency, the CheckV2MVersion method) and canonicalize
// value-preservingly to the v6 layout (the ConvertV2M method: era defaults
// inserted for missing params, mod destinations remapped, MIDI verbatim).
// v6 input canonicalizes to an identical byte image (plus the lab's 4-byte
// zero tail, which ConvertV2M also appends).
V2LoadResult v2loadCanonicalize(const void *v2m, size_t length);

} // namespace v2redux

#endif // V2LOAD_H_
