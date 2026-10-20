// v2redux -- portable, version-native V2M player.
//
// Self-contained: no dependency on the rest of v2/ (the x87-faithful lab) or
// any library beyond the C/C++ standard library. Plays v2m files of any
// format version (0..6) natively: the loader detects the version by
// structural fingerprint, canonicalizes the patch data, and the engine
// switches its behavior deltas on the source version (see v2eras.h).
//
// Determinism contract: same file + same seed + same compiled version range
// => bit-identical float32 output on every supported host. All
// transcendentals are project-owned (v2math.h); ordinary arithmetic is
// strict IEEE-754 binary32, matching the original x87 PC=24 rounding
// op-for-op. Build policy: NO -ffast-math, -ffp-contract=off, subnormals
// must not be flushed (open() verifies this at runtime).
//
// Compile-time configuration:
//   V2_VER_MIN / V2_VER_MAX  -- supported format version range (default 0/6).
//                               Out-of-range gates constant-fold away.
//   V2_RONAN                 -- 1 (default): include the Ronan speech synth.
//
// OpenSpec change: portable-version-native-player.

#ifndef V2REDUX_H_
#define V2REDUX_H_

#include <stddef.h>
#include <stdint.h>

#ifndef V2_VER_MIN
#define V2_VER_MIN 0
#endif
#ifndef V2_VER_MAX
#define V2_VER_MAX 6
#endif
// Ronan (speech synth): the port is verified against the candytron v5 oracle
// (task 6.1, user-confirmed by listening: josie speech corr 0.99966). Default
// is ON -- speech songs (candytron, josie, .kkrieger) route channel 15 through
// the Ronan vocal tract, and with Ronan OFF that channel's raw glottal source
// leaks to the mix as noise. Opt OUT with -DV2_RONAN=0 for a speech-silent
// build (drops the phoneme tables; changes those songs' determinism hashes).
#ifndef V2_RONAN
#define V2_RONAN 1
#endif

// Era + the eras:: catalog (the engine-identity coordinate the caller supplies
// when the file's format version under-resolves the build; see v2eras.h).
// Included AFTER the V2_VER_MIN/MAX defaults above so the ledger sees them.
#include "v2eras.h"

namespace v2redux {

enum class Result {
  OK = 0,
  BadFile,            // structurally invalid / not a v2m
  UnsupportedVersion, // detected version outside [V2_VER_MIN, V2_VER_MAX]
  FpEnvBroken,        // FP environment flushes subnormals (e.g. -ffast-math host)
};

struct PlayerImpl; // engine + sequencer + loaded song (one heap allocation)

class Player {
public:
  Player();
  ~Player();
  Player(const Player &) = delete;
  Player &operator=(const Player &) = delete;

  // Detect the file's format version, canonicalize, and prepare for playback.
  // The data is copied; the caller's buffer may be freed after open().
  //
  // era: which engine identity to render with (see v2eras.h).
  //   Era::Auto()  (default) -- use the detected format version. Correct for
  //                every clean case; for an ambiguous version (v5) it picks
  //                the documented newest-in-range default.
  //   eras::xxx    -- a named build profile, e.g. eras::kkrieger2004, when the
  //                file's format version cannot express the build that wrote it.
  //   Era::v(n)/.with(..) -- a raw version or a refined research override.
  // The resolved base must lie within the compiled [V2_VER_MIN, V2_VER_MAX].
  Result open(const void *v2mData, size_t length, const Era &era = Era::Auto());

  // Back-compat overload: -1 = detected version (== Era::Auto()); 0..6 = force
  // that format-version baseline (== Era::v(n)). Prefer the Era form above.
  Result open(const void *v2mData, size_t length, int forceBehaviorVersion);

  // Format version (0..6) detected by the last successful open().
  // After a failed open() with UnsupportedVersion, still reports what was
  // detected. -1 if nothing has been detected yet.
  int fileVersion() const;

  // Start playback at the given song position. Implies a full synth reset.
  void play(uint32_t fromMs = 0);
  bool isPlaying() const; // false once the song end was reached (tail may ring)

  // Pull-render interleaved stereo float32 at 44100 Hz. Renders silence when
  // nothing is open/playing. Never allocates. Chunk-size invariant: any
  // partition of N frames produces identical bits.
  void render(float *stereoInterleaved, uint32_t frames);

  // Replaces the historical rdtsc noise/S&H/dist seeding. Default 0 -- the
  // deterministic reference (matches the pinned-rdtsc C1 oracle convention).
  // Takes effect at the next play().
  void setSeed(uint64_t seed);

  // ---- interactive control + display monitors (NOT part of the determinism
  // contract; the default all-audible mask leaves the reference render exact) --

  // Per-channel mute: bit ch set = channel audible (default: all 16 audible).
  // A muted channel still renders its voices + FX (state advances exactly as the
  // reference) but feeds no output bus -- so muting/unmuting is instant and
  // in-phase, notes already sounding cut immediately, and nothing hangs or
  // desyncs. Persists across play()/seek; takes effect on the next render().
  void setChannelMask(uint32_t mask);

  // Active voice count per channel into dest[0..15]; returns the total across
  // all channels. dest must hold 16 ints. Display-only.
  int getChannelPoly(int *dest16) const;

  // Monotonic per-channel note-on counters into dest[0..15] (dest holds 16).
  // A rise in dest[ch] between two polls means a note was struck on channel ch
  // (counts even muted channels). Display-only.
  void getChannelNoteOns(uint32_t *dest16) const;

  // Song length in milliseconds: the loop point where isPlaying() goes false,
  // derived from the sequence timing WITHOUT rendering audio (fast). Implies a
  // synth reset -- call play() before the next render(). 0 if nothing is open.
  long long lengthMs() const;

  // Per-channel output peak (0..~1+) into dest[0..15], measured since the last
  // call and then cleared (read-and-clear). Reflects each channel's loudness in
  // the mix, independent of its mute state. Display-only.
  void getChannelLevels(float *dest16);

private:
  PlayerImpl *impl_;
  int detectedVersion_;
};

// One-time FP environment self-check (also run by open()): verifies subnormal
// arithmetic is not flushed to zero. Exposed for embedders that want to fail
// fast at startup.
bool fpEnvironmentOk();

} // namespace v2redux

#endif // V2REDUX_H_
