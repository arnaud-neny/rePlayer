// v2eras -- THE behavior-delta ledger of the portable V2 player.
//
// Every known engine-behavior difference between the year-2000 synth (v2m
// format v0, fr08) and the 2004 synth (format v6, synth.asm) is one row
// here: at which format version the OLD behavior flipped to the NEW one, and
// how we know. Engine code never compares versions directly; it asks
// oldBehavior(DELTA_X, version).
//
// Evidence levels:
//   PROVEN   -- verified against a period binary. Three anchors now: the fr08
//               reconstruction proved the v0 side of every row, the 5-song
//               2004-asm A/B proved the v6 side, and the fr-030 candytron
//               final binary (2003-08, kkrunchy-unpacked + the byte-identical
//               genthree/_viruz2a.asm == RG2/ViruzII == RG2/Viewer sources)
//               proved the v5 state of every row (task 6.0e assay,
//               v2m/candytron-extraction/NOTES.md).
//   ANCHORED -- the parameter version tables (sounddef.h) imply the feature's
//               introduction version (params for it appear there).
//   ASSUMED  -- documented guess for the remaining v1..v4 gap. Default
//               policy: flip at v1 unless coupled to an anchored row. When a
//               mid-era binary is analyzed (the follow-up research track),
//               these become data edits here -- never engine redesigns.
//
// The full v0-side characterization lives in v2m/fr08-extraction/DELTA.md;
// row comments cite it. THIS FILE is the living threshold ledger.
//
// NOTE some rows need no engine gate because the loader's canonical defaults
// already make the modern code an exact no-op for old files (marked
// "default-handled"). They are still listed: the ledger must be complete, and
// future evidence may prove a default NOT exactly null on some path.

#ifndef V2ERAS_H_
#define V2ERAS_H_

#include <stdint.h>

// V2_VER_MIN / V2_VER_MAX: the compiled format-version range. Defined by
// v2redux.h, but this header is also pulled in stand-alone by the engine
// (v2core.cpp), so default them here too -- self-contained, no include cycle
// with the public header (which now includes US, for the Era type below).
#ifndef V2_VER_MIN
#define V2_VER_MIN 0
#endif
#ifndef V2_VER_MAX
#define V2_VER_MAX 6
#endif

namespace v2redux {

enum V2Delta {
  // --- engine-structure deltas (all proven at the endpoints) ---------------
  DELTA_ENV_CURVES = 0,    // env attackmul -11/128 + dec/rel via calcfreq x10
                           // (calcfreq2/x11 did not exist in 2000)
  DELTA_FRAME256,          // control frame 256 samples (2004: 128); implies
                           // volramp coeff 1/256 and env/LFO tick rate; LFO
                           // freq without the modern 0.5x compensation
  DELTA_ENV_CLAMP_SUSREL,  // the val<=2^-13 ->OFF clamp only in SUSTAIN/
                           // RELEASE (2004 added the DECAY runout check);
                           // -> v0 envs decay into subnormals while gated
  DELTA_OSC_BOXFILTER,     // tri/saw/pulse = 4x-oversampled numeric box
                           // filter (2004: analytic OSM convolution)
  DELTA_NOISE_LCG_MSVC,    // noise LCG x214013+2531011 (2004: x196314165+
                           // 907633515) + the 2000 16-bit float recurrence
  DELTA_NATIVE_FSIN,       // osc/LFO sine = native fsin (2004: fastsin poly)
  DELTA_NATIVE_FPATAN,     // overdrive waveshaper = native atan (2004:
                           // fastatan rational poly)
  DELTA_OSC_FREQ_CONST,    // osc base freq = baked 3185015.0 (2004: computed
                           // from fcoscbase at runtime; SR-flexible)
  DELTA_NO_DCOFFSET,       // fcdcoffset (2^-18 denormal bias) absent
                           // everywhere (SVF, voice mix, chorus in, delay/
                           // reverb in)
  DELTA_CRUSHER_SPLIT_GAIN1,// bitcrusher renders (in*gain1)*(32768/x) as two
                           // muls; 2004 folds gain1 into crush1 at set
  DELTA_PGMCHANGE_V0,      // PGM change: no same-program early-out + resets
                           // ctl7 (chan volume) to 127
  DELTA_TICK_BEFORE_SET,   // frame control order TICK then SET: sub-objects
                           // step with the PREVIOUS frame's params
  DELTA_SUBFRAME_RENDER,   // render in sub-frame chunks with trailing-edge
                           // control tick (mid-frame note-on voice/chorus
                           // phase + control-edge facets)
  DELTA_NO_VOICE_DCF,      // per-voice DC filter absent (voice = osc->flt->
                           // dist->volramp only)
  DELTA_NO_MASTER_DCF,     // master DC filter absent (mix -> lc/hc EQ direct)
  DELTA_NO_MOOG,           // VCF modes 6/7 (MoogL/H) alias to passthrough
  DELTA_KEYSYNC_OSC_ONLY,  // noteOn keysync has no full-resync path: ANY
                           // keysync != 0 only zeros the osc phase counters
                           // (SYNC_FULL collapses to SYNC_OSC; noise/LFO/
                           // filter state RESUMES on re-trigger)
  DELTA_RVB_E_FULLPREC,    // reverb set: e = sqr(64/(revtime+1)) with the
                           // quotient kept full-precision through the square
                           // (the 2000 set path ran at x87 PC=64) and no
                           // SRfclinfreq factor (2004 SR-flexibility, =1.0
                           // at 44100)
  // --- feature-introduction deltas (anchored by the param tables) ----------
  DELTA_NO_COMP_BOOST,     // channel dcf1/comp/boost/dcf2 + global sum comp
                           // absent (no code in the v0 binary)
  DELTA_NO_AUX_BUSSES,     // AuxA/B busses absent (default-handled: rcv/send
                           // gains canonicalize to 0; lab port verified
                           // bit-exact without an explicit gate)
  DELTA_NO_RVB_LOWCUT,     // reverb low-cut stage absent (ENGINE-GATED, not
                           // default-handled: the canonical global defaults
                           // give pre-v4 files a NONZERO lowcut param, so the
                           // 2004 hpf stage would be a real extra filter --
                           // proven by fr08's reverb tail, DELTA.md)
  DELTA_CC6_HICUT,         // "FAKE 2: Lowcut!" -- a ch15(speech)-only control
                           // hack: MIDI CC6 on channel 15 ALSO sets the master
                           // high-cut freq hcfreq = sqr((val+1)/128), on top of
                           // storing the controller. PROVEN present at v0 (fr08
                           // ProcessControlChange @0x40bded) AND v5 (candytron/
                           // RG2 _viruz2a.asm ".FAKE 2 : Lowcut!"); REMOVED in
                           // 2004 (synth.asm just stores the CC). Drives the
                           // whole-mix master EQ in josie (ch15 sweeps CC6).
  DELTA_RDTSC_SEED,        // osc-noise + LFO-S&H seed from rdtsc (== 0 under the
                           // deterministic pinned-rdtsc convention) -- NOT the
                           // 2004 fixed osc-seed table {0xdeadbeef,0xbaadf00d,
                           // 0xd3adc0de} / libc-rand LFO sequence. PROVEN old at
                           // v0 (fr08 syOscInit/syLFOInit rdtsc @0x40a..) AND v5
                           // (candytron syOscInit @0x41dc99 + syLFOInit rdtsc);
                           // the fixed table is a v6 addition (oscseeds in
                           // synth.asm _OSC_). Decoupled from DELTA_NOISE_LCG_MSVC
                           // (which only picks the LCG constants): at v5 the LCG
                           // is modern but the SEED is still rdtsc->0.
  DELTA_NO_FM_OSC,         // osc mode 5 (FM sine) absent: the 2000 oscjtab
                           // (@0x40a565 in the fr08 image) maps modes 5/6/7
                           // to OFF -- no FM renderer exists in that binary.
                           // Present by v5 (candytron .mode4). Coupled to
                           // DELTA_NATIVE_FSIN for the implementation choice
                           // once present (see v2core renderFMSin_v5).
  DELTA_NO_CHAN_DCF,       // channel dcf1/dcf2 DC filters absent: the v5
                           // syChanProcess (genthree _viruz2a.asm == candytron
                           // binary) is comp -> boost -> dist/chorus -> sends,
                           // with NO DC filter stage; the dcf1 (pre-comp) and
                           // dcf2 (post-dist) one-poles are 2004 additions.
                           // Previously mis-bundled into DELTA_NO_COMP_BOOST
                           // (whose v1 param anchor only covers comp/boost).
                           // Root cause of the josie whole-mix DC-decay
                           // residual (every channel, 2nd sample of first
                           // note).
  DELTA_POLY_16,           // voice pool is 16, not bigger. The pool size is
                           // the first VALUE-typed era difference (16/32/64
                           // across builds), expressed as two composed bool
                           // rows -- see voicePoolSize() below. When a dense
                           // song saturates the period pool, the old engine
                           // STEALS the oldest gate-off voice where a bigger
                           // pool opens a fresh one: allocation choices (and
                           // thus steal-cut release tails + reused-voice
                           // state) diverge. Root cause of the flybye 66.42s
                           // divergence (first >16-voice moment of the song;
                           // alloc trace: 2001 steals slot 10, the 64-voice
                           // port opened voice 16).
  DELTA_POLY_32,           // voice pool is 32, not 64. Both v5 binaries agree
                           // (unlike the osc trio) so the format proxy is
                           // clean at this flip; 64 is the 2004 value.
  DELTA_CHANMOD_NO_VOICE_SRC, // the CHANNEL mod matrix applies ONLY sources < 8
                           // -- velocity (0) + ctl1..7 (1..7). Sources >= 8
                           // (aenv/env2, lfo1/lfo2, note) are voice-PRIVATE and
                           // have no channel-level meaning, so the old store
                           // skips them for channel params. PROVEN old at v3
                           // (fr014 syChanSet @0x40fe39 `cmp al,8; jae`) AND v4
                           // (fr019 @0x429900, same `cmp al,8; jae` after the
                           // dest-range cmp al,0x39/0x52). The modern-core store
                           // (v5 brullwurfel: no such range/source check) applies
                           // ALL sources -- proven by the bit-exact v5/v6 corpus
                           // (pzero/zeitmaschine/debris/kkrieger6 carry source>=8
                           // channel mods and only stay bit-exact if applied).
                           // Build-date proxy at v5 like the osc/player rows
                           // (early-v5 old-core fr-022 would still skip). Root
                           // cause of the fr-014 v3 ch8 ~5x divergence: aenv ->
                           // comp.outgain pushed makeup 98->128 (3.7x extra) plus
                           // lfo1 -> chorus.amount, both wrongly applied.

  DELTA_NO_RONAN_CHANNEL,  // the render-time channel-15 -> Ronan (speech
                           // vocal-tract) routing. OLD (early eras): ch15 is an
                           // ordinary music channel, never passed through Ronan.
                           // The year-2000 fr08 core (v0) inits Ronan but its
                           // render makes NO ronan calls (C1 harness), and a
                           // Ronan-OFF build is bit-exact to the C1 oracle --
                           // fr08's ch15 carries music (an F3 layer entering at
                           // 1:58) that the unconditional routing silenced by
                           // running it through the idle vocal tract. NEW (v5+):
                           // ch15 -> ronanCBProcess, so candytron/kkrieger
                           // speech articulates.

  DELTA_COUNT
};

enum V2Evidence : unsigned char {
  EV_PROVEN = 0,  // period-binary verified threshold
  EV_ANCHORED,    // parameter tables imply it
  EV_ASSUMED      // documented guess (v1..v5 gap)
};

struct V2DeltaRow {
  unsigned char flipsAt;   // OLD behavior applies to versions < flipsAt
  V2Evidence evidence;
};

// The ledger. Order must match the V2Delta enum.
//
// Threshold rationale:
//  - flipsAt 2 + ANCHORED on env/frame rows: kb's own transEnv conversion
//    (v2mconv.cpp:243, disabled) remaps envelopes exactly at the <v2 -> v2
//    boundary, and the frame-size halving is the structural root of that
//    remap (DELTA.md "control-frame size" section). The lab's eraEnvOld()
//    (srcVersion < 2) encoded the same judgment. The candytron binary (v5)
//    has the modern side of both (attackmul -0.09375 @0x41dbd4, frame 128)
//    -- consistent.
//  - flipsAt 1 + ANCHORED on comp/boost: their parameters appear at format
//    v1 (sounddef.h: +10 patch/+9 global params), and the v0 binary has no
//    code for them. Present in the v5 binary -- consistent.
//  - flipsAt 4 + ANCHORED on reverb low-cut: its global param appears at v4.
//    Present in the v5 binary (syReverbSet lowcut + hpf stage) -- consistent.
//  - flipsAt 6 + PROVEN (the 6.0e candytron assay): these rows still show
//    the OLD behavior in the v5 binary and the new one in the 2004 asm, so
//    the flip is pinned at exactly v6. Citations are genthree/_viruz2a.asm
//    (== the shipped binary, 6 signatures spot-verified in the unpacked
//    image) vs v2/synth.asm.
//  - OSC_BOXFILTER is PINNED at flipsAt 3 + PROVEN (the fr-019 v4 render oracle,
//    2026-06-08). It is NOT a build-date row: the tri/saw/pulse box->analytic-OSM
//    rewrite landed at the v1->v3 format step, far earlier than the LCG/freq
//    build-date flip it was wrongly bundled with. PROVEN both sides: flybye (v1)
//    renders the 4x box (`mov cl,4; fldz` @0x40e70a, whole-song bit-exact incl
//    its pulse ch7); fr014 (v3) + fr019 (v4) render the analytic OSM (utof23+
//    fdiv; pulse @0x40e6cf/0x428111, tri/saw @0x40e595/0x427fd9), byte-identical
//    to each other. Once v3/v4 use the OSM the box-vs-OSM divergence is GONE
//    (ch3 pulse corr -1.0->1.0 max|d|=0). NOTE the OSM at v3/v4 still advances the
//    phase at freq<<2 (fr019 `shl esi,2`) because FREQ_CONST is still old there:
//    renderTriSaw/renderPulse scale freq by 4 when old(FREQ_CONST), like
//    renderSin_v0. v2 has no binary -> assumed box like its v1 neighbor.
//    The SAME freq<<2 + a v4 FM modulation-depth of 4.0 (vs v5's 2.0) also fix
//    the v4 FM oscillator (renderFMSin_v5), both gated old(FREQ_CONST) -- with
//    those, fr019 whole-song is 0/341 sec >3%, rms-diff 0.12% (ACCURACY-LOOSE-
//    ENDS.md §9). The leftover 0.12% is a carrier-phase/keysync offset, inaudible.
//  - flipsAt 5 + ASSUMED on the osc-core PAIR (NOISE_LCG_MSVC, OSC_FREQ_CONST):
//    these were WRONGLY flipsAt 1. The fr-013 flybye binary
//    (format v1, plays tpinv2.v2m) PROVES them OLD at v1 -- its synth has the
//    MSVC LCG (214013/2531011 @0x40e7dc) + the baked oscfreq 3185015.0
//    @0x40e5c0 and NO modern LCG anywhere. So v1 is OLD, not NEW.
//    (fr014/fr019 at v3/v4 CONFIRM these stay OLD: fr014 ch8 noise is bit-exact
//    with renderNoise_v0, and the fr014 sine still advances freq<<2 @0x40e7a4 --
//    so only BOXFILTER moved off the proxy, not the LCG/freq pair.)
//    But these do NOT flip at a format boundary: the fr-022 ein.schlag binary
//    embeds a v5 song yet its synth is ALSO old-core (MSVC LCG @0x40da0a,
//    baked 3185015 @0x40d668, no modern LCG) -- while candytron, ALSO v5,
//    is modern-core (LCG 0xbb38435 @0x41dff5, runtime fcoscbase). Two v5
//    binaries, opposite cores: the flip is on the engine BUILD-DATE timeline
//    (between ms2002 fr-022 and Aug-2003 candytron), and format v5 straddles
//    it. flipsAt 5 is the best format-version PROXY: OLD for v0..v4 (proven
//    OLD through early-v5, so the earlier formats are old too) and NEW for
//    v5..v6 -- which keeps the late-v5 oracle corpus (josie/kkrieger6, both
//    Aug-2003+/modern-core) bit-exact, exactly as flipsAt 1 did (v5 is NEW
//    under both). The ONE unrepresentable case is an early-v5 old-core file
//    like fr-022 itself: a format-version model cannot render it correctly.
//    Stays ASSUMED because the value is a proxy, not a real format threshold.
//  - flipsAt 5 + ASSUMED on the player/render rows (PGMCHANGE_V0,
//    TICK_BEFORE_SET, SUBFRAME_RENDER, RVB_E_FULLPREC): also WRONGLY flipsAt 1.
//    Disassembled in flybye (v1) -- its player/render code is byte-for-byte
//    structurally identical to fr08 (v0), so all four are OLD at v1
//    (flybye-extraction/NOTES.md "disassembly" section):
//      * PGMCHANGE_V0   : handler @0x410455 == fr08 @0x40be15 -- no same-prog
//                         early-out + writes ctl7=127 (f3aab07faa @0x410482).
//      * SUBFRAME_RENDER+TICK_BEFORE_SET: render driver @0x40ff83 == fr08
//                         @0x40b95c -- min(todo,framectr) sub-frame chunks +
//                         control TICK at the chunk's trailing edge.
//      * RVB_E_FULLPREC : syReverbSet @0x40f89e == fr08 @0x40b2d0 -- e =
//                         sqr(64/(t+1)), NO SRfclinfreq factor, no PC change.
//    Threshold like the trio: three of the four (TICK/SUBFRAME/RVB_E) are STILL
//    old in early-v5 fr-022 (render driver @0x40f2ae; reverb has no SRfclinfreq
//    @0x40ec88) and only flip in late-v5 candytron (reverb SRfclinfreq fmul
//    @0x41f27d) -- the same BUILD-DATE flip as the trio, so flipsAt 5 is the
//    proxy and early-v5 fr-022 is the unrepresentable case. TICK_BEFORE_SET,
//    SUBFRAME_RENDER, RVB_E_FULLPREC stay flipsAt-5 proxies; the 2026-06-07
//    era-gap sweep CONFIRMED them (old-core through fr-027 v5 2002-07, new at
//    fr-028 brullwurfel 2002-09 -- so fr-027 joins fr-022 as an early-v5
//    old-core unrepresentable case).
//    PGMCHANGE_V0 is NO LONGER in this group: the sweep PINNED it at v4 (v3
//    fr014/fr-022party OLD with ctl7=127, v4 fr019 NEW with the same-program
//    early-out) -> now flipsAt 4 EV_PROVEN, both sides at the one MS2002 party.
//    These keep v5 NEW, so the oracle corpus is unchanged (check.py 3/3 (genuine-only)).
//    Override with Player::open(..., forceBehaviorVersion) when researching.
//  - era-gap sweep (2026-06-07, v2m/era-gap-sweep + brullwurfel-extraction):
//    PINNED two more rows that had no period binary before. NO_FM_OSC: oscjtab
//    mode5 = OFF at v0/v3, fsin FM renderer at v4 (fr019 @0x4282a1) -> flipsAt 4
//    EV_PROVEN (was the last flipsAt-1 ASSUMED). POLY_16: pool 32 PROVEN at v3
//    (fr014 @0x410030), not just early-v5 -> tightened from the flipsAt-5 proxy
//    to flipsAt 3 (the 16->32 growth is v2 or v3; no v2 binary exists, so the
//    exact flip keeps a 1-version gap and the row stays ASSUMED). The brullwurfel
//    (fr-028, 2002-09) v5 oracle re-confirmed the whole v5 row set.
//
// No EV_ASSUMED row remains at flipsAt 1: every v1..v4 row is now either
// PROVEN/ANCHORED or a documented flipsAt-5 build-date proxy.
inline constexpr V2DeltaRow kDeltas[DELTA_COUNT] = {
  /* DELTA_ENV_CURVES         */ { 2, EV_ANCHORED },
  /* DELTA_FRAME256           */ { 2, EV_ANCHORED },
  /* DELTA_ENV_CLAMP_SUSREL   */ { 6, EV_PROVEN   }, // v5 state_dec: no LOWEST
                                                     // runout (binary @0x41e199)
  /* DELTA_OSC_BOXFILTER      */ { 3, EV_PROVEN   }, // tri/saw/pulse OSM rewrite at
                                                     // v3, NOT the build-date flip:
                                                     // fr014/fr019 (v3/v4) render the
                                                     // analytic OSM (utof23+fdiv,
                                                     // pulse @0x40e6cf/0x428111,
                                                     // trisaw @0x40e595) while flybye
                                                     // (v1) is the 4x box (mov cl,4;
                                                     // fldz @0x40e70a). DECOUPLED from
                                                     // NOISE_LCG/FREQ_CONST (those
                                                     // stay old at v3). v2 gap: no
                                                     // binary, box like its v1 neighbor.
  /* DELTA_NOISE_LCG_MSVC     */ { 5, EV_ASSUMED  }, // build-date flip, see below
  /* DELTA_NATIVE_FSIN        */ { 6, EV_PROVEN   }, // v5 osc/LFO/FM = native
                                                     // fsin (binary @0x41dfc3/
                                                     // 0x41e43b/0x41e094)
  /* DELTA_NATIVE_FPATAN      */ { 6, EV_PROVEN   }, // v5 overdrive render =
                                                     // native fpatan @0x41e60b
  /* DELTA_OSC_FREQ_CONST     */ { 5, EV_ASSUMED  }, // build-date flip, see below
  /* DELTA_NO_DCOFFSET        */ { 6, EV_PROVEN   }, // no 2^-18 constant in the
                                                     // whole v5 image/source
  /* DELTA_CRUSHER_SPLIT_GAIN1*/ { 6, EV_PROVEN   }, // v5 render = two muls
                                                     // @0x41e665; set unfolded
  /* DELTA_PGMCHANGE_V0       */ { 4, EV_PROVEN   }, // PINNED v4 (era-gap sweep):
                                                     // v3 OLD ctl7=127 (fr014
                                                     // @0x41045d, fr-022party
                                                     // @0x41019d), v4 NEW no-
                                                     // ctl7 same-prog early-out
                                                     // (fr019 @0x429f00). Both
                                                     // sides proven at the one
                                                     // MS2002 party.
  /* DELTA_TICK_BEFORE_SET    */ { 5, EV_ASSUMED  }, // flybye disasm, see below
  /* DELTA_SUBFRAME_RENDER    */ { 5, EV_ASSUMED  }, // flybye disasm, see below
  /* DELTA_NO_VOICE_DCF       */ { 6, EV_PROVEN   }, // no DCF code in v5 at all
  /* DELTA_NO_MASTER_DCF      */ { 6, EV_PROVEN   }, // v5 master: moddel -> EQ
                                                     // -> comp, no DCF stage
  /* DELTA_NO_MOOG            */ { 6, EV_PROVEN   }, // v5 syFRTab modes 6/7 =
                                                     // bypass; no moog dist
  /* DELTA_KEYSYNC_OSC_ONLY   */ { 6, EV_PROVEN   }, // v5 noteOn: oks!=0 only
                                                     // zeros osc counters, no
                                                     // HARDSYNC path
  /* DELTA_RVB_E_FULLPREC     */ { 5, EV_ASSUMED  }, // flybye disasm, see below; v5
                                                     // set at PC=24
  /* DELTA_NO_COMP_BOOST      */ { 1, EV_ANCHORED },
  /* DELTA_NO_AUX_BUSSES      */ { 6, EV_PROVEN   }, // was ANCHORED; v5 osc
                                                     // jtab modes 6/7 = off, no
                                                     // aux bus code
  /* DELTA_NO_RVB_LOWCUT      */ { 4, EV_ANCHORED },
  /* DELTA_CC6_HICUT          */ { 6, EV_PROVEN   }, // v0 & v5 ch15-CC6 -> master
                                                     // hicut; 2004 dropped it
  /* DELTA_RDTSC_SEED         */ { 6, EV_PROVEN   }, // v0 & v5 rdtsc(->0); the
                                                     // fixed seed table is v6
  /* DELTA_NO_FM_OSC          */ { 4, EV_PROVEN   }, // PINNED v4 (era-gap sweep):
                                                     // oscjtab mode5 = OFF at
                                                     // v0/v3 (fr08, fr014, fr-
                                                     // 022party), = fsin FM
                                                     // renderer at v4 (fr019
                                                     // @0x4282a1). Monotonic
                                                     // (off thru v3) => flybye
                                                     // v1 also no-FM, consistent
                                                     // with its bit-exact oracle
  /* DELTA_NO_CHAN_DCF        */ { 6, EV_PROVEN   }, // v5 syChanProcess has no
                                                     // DC filters (source ==
                                                     // binary); dcf1/dcf2 are
                                                     // 2004/v6 additions
  /* DELTA_POLY_16            */ { 3, EV_ASSUMED  }, // pool 16 PROVEN at v0
                                                     // (fr08 @0x40b9e4) + v1
                                                     // (flybye @0x41000b, steal
                                                     // @0x410393); pool 32
                                                     // PROVEN at v3 (fr014
                                                     // @0x410030, era-gap sweep)
                                                     // -- much earlier than the
                                                     // old flipsAt-5 proxy. Flip
                                                     // is v2 or v3 (no v2 binary
                                                     // exists); set to 3, v2
                                                     // unobserved. Stays ASSUMED
                                                     // for that 1-version gap.
  /* DELTA_POLY_32            */ { 6, EV_PROVEN   }, // pool 32 at BOTH v5
                                                     // binaries (fr-022 cmp
                                                     // dl,0x20 @0x40f322,
                                                     // candytron @0x41f96a);
                                                     // 64 in the 2004 asm
                                                     // (%define POLY 64)
  /* DELTA_CHANMOD_NO_VOICE_SRC */ { 5, EV_ASSUMED }, // v3 fr014 + v4 fr019 SKIP
                                                     // src>=8 in channel mods
                                                     // (`cmp al,8; jae`); modern-
                                                     // core v5+ applies all. v5
                                                     // build-date proxy (early-v5
                                                     // old-core unrepresentable),
                                                     // like the osc/player rows.
  /* DELTA_NO_RONAN_CHANNEL   */ { 5, EV_ASSUMED }, // v0 PROVEN: NO ch15->Ronan
                                                     // routing (fr08 C1 oracle
                                                     // bit-exact with Ronan off;
                                                     // the 2000 render makes no
                                                     // ronan calls). v5 PROVEN:
                                                     // routing present (candytron
                                                     // speech needs it). The flip
                                                     // in the v1..v4 gap is
                                                     // unobserved (no v1-v4 song
                                                     // uses ch15 speech) -> proxy
                                                     // at 5, ASSUMED.
};

// Does the OLD (pre-flip) behavior apply at this behavior version?
//
// The V2_VER_MIN/MAX clamps are what make single-version builds fold every
// gate to a compile-time constant: with MIN==MAX the engine's
// behaviorVersion is itself a constexpr (see v2core), so this whole function
// evaluates at compile time and dead branches vanish.
constexpr bool oldBehavior(V2Delta d, int behaviorVersion)
{
  return (kDeltas[d].flipsAt <= V2_VER_MIN) ? false
       : (kDeltas[d].flipsAt > V2_VER_MAX)  ? true
       : behaviorVersion < (int)kDeltas[d].flipsAt;
}

// The era voice-pool size, composed from the two POLY rows (the ledger's
// first value-typed difference: 16 in 2000/2001, 32 in 2002/2003, 64 in
// 2004). Engine arrays stay sized for 64; the ALLOCATOR never scans past
// this bound, which is provably equivalent to a smaller pool (voices that
// are never allocated keep chanmap == -1, and every other voice loop skips
// those). Constant-folds exactly like the boolean gates.
constexpr int voicePoolSize(int behaviorVersion)
{
  return oldBehavior(DELTA_POLY_16, behaviorVersion) ? 16
       : oldBehavior(DELTA_POLY_32, behaviorVersion) ? 32
       : 64;
}

static_assert(kDeltas[DELTA_POLY_16].flipsAt <= kDeltas[DELTA_POLY_32].flipsAt,
              "voice pool growth must be monotonic (16 -> 32 -> 64)");

// COUPLING: the 2000 oscillator block is one convention, not independent
// rows. The baked freq constant is ~SRfcobasefrq/4 BECAUSE the v0 renderers
// advance the phase counter 4x per output sample (tri/saw/pulse box loop,
// sine freq<<2). If these rows flipped at different versions, pitch would
// shift by two octaves in the gap. Future evidence must move them together
// (or decouple them with code, not just data).
//
// NOTE the sine EVALUATION is decoupled from this convention since the 6.0e
// assay: candytron (v5) evaluates the sine with native fsin (DELTA_NATIVE_FSIN
// old until v6) but advances 1x on the runtime freq. v2core's renderSin_v0
// therefore derives its step from DELTA_OSC_FREQ_CONST, not from the fsin row.
//
// The sine freq-ADVANCE (FREQ_CONST) is ALSO decoupled from the tri/saw/pulse
// box->OSM rewrite (BOXFILTER): the fr014 (v3) oracle disasm shows the analytic
// OSM tri/saw/pulse renderers (BOXFILTER already NEW at v3) ALONGSIDE a sine
// that still advances freq<<2 with native fsin (FREQ_CONST still OLD at v3,
// @0x40e7a4 `shl edx,2`). So BOXFILTER flips at v3 while FREQ_CONST stays on the
// build-date proxy (flipsAt 5). They are no longer one convention.

#if V2_VER_MIN == 0 && V2_VER_MAX == 6
// ledger sanity for the default full-range build. These probe oldBehavior at
// EXPLICIT versions (1/3/...), so they only hold when the compiled range spans
// those versions -- a restricted-range build clamps oldBehavior and the probes
// no longer reflect the pure ledger. Hence the full-range guard (moved inside
// it so single-version builds, e.g. -DV2_VER_MIN=6, still compile).
static_assert(!oldBehavior(DELTA_OSC_BOXFILTER, 3), "v3 tri/saw/pulse are analytic OSM (fr014/fr019)");
static_assert(oldBehavior(DELTA_OSC_BOXFILTER, 1), "v1 tri/saw/pulse are the 4x box (flybye)");
static_assert(oldBehavior(DELTA_OSC_FREQ_CONST, 3), "v3 sine still advances freq<<2 (fr014 @0x40e7a4)");
static_assert(oldBehavior(DELTA_ENV_CURVES, 0), "v0 must get old env curves");
static_assert(oldBehavior(DELTA_ENV_CURVES, 1), "v1 assumed old env curves");
static_assert(!oldBehavior(DELTA_ENV_CURVES, 2), "v2+ gets modern env curves");
static_assert(oldBehavior(DELTA_NO_AUX_BUSSES, 5), "aux busses are v6");
static_assert(!oldBehavior(DELTA_NO_AUX_BUSSES, 6), "v6 has aux busses");
static_assert(!oldBehavior(DELTA_CRUSHER_SPLIT_GAIN1, 6), "v6 folds gain1");
static_assert(oldBehavior(DELTA_CRUSHER_SPLIT_GAIN1, 0), "v0 splits gain1");
// candytron (v5) anchors from the 6.0e assay
static_assert(oldBehavior(DELTA_CRUSHER_SPLIT_GAIN1, 5), "v5 splits gain1");
static_assert(oldBehavior(DELTA_NATIVE_FSIN, 5), "v5 uses native fsin");
static_assert(!oldBehavior(DELTA_NATIVE_FSIN, 6), "v6 uses fastsin");
static_assert(oldBehavior(DELTA_NO_MOOG, 5), "v5 has no moog modes");
static_assert(oldBehavior(DELTA_NO_DCOFFSET, 5), "v5 has no dcoffset");
static_assert(!oldBehavior(DELTA_OSC_BOXFILTER, 5), "v5 osc is OSM already");
static_assert(oldBehavior(DELTA_NO_FM_OSC, 0), "v0 has no FM osc mode");
static_assert(!oldBehavior(DELTA_NO_FM_OSC, 5), "v5 has the FM osc mode");
// voice-pool endpoints (flybye/fr-022/candytron disassembly + 2004 asm)
static_assert(voicePoolSize(0) == 16, "v0 pool is 16 (fr08)");
static_assert(voicePoolSize(1) == 16, "v1 pool is 16 (flybye)");
static_assert(voicePoolSize(5) == 32, "v5 pool is 32 (fr-022 AND candytron)");
static_assert(voicePoolSize(6) == 64, "v6 pool is 64 (synth.asm)");
// channel mod source filter: v3/v4 skip voice-private sources, v5+ apply all
static_assert(oldBehavior(DELTA_CHANMOD_NO_VOICE_SRC, 3), "v3 channel mods skip src>=8 (fr014)");
static_assert(oldBehavior(DELTA_CHANMOD_NO_VOICE_SRC, 4), "v4 channel mods skip src>=8 (fr019)");
static_assert(!oldBehavior(DELTA_CHANMOD_NO_VOICE_SRC, 5), "v5+ channel mods apply all sources");
static_assert(!oldBehavior(DELTA_CHANMOD_NO_VOICE_SRC, 6), "v6 channel mods apply all sources");
#endif

// behavior version a Player resolves to: forced override (research knob) or
// the detected file version; rejects out-of-compiled-range requests.
// With V2_VER_MIN == V2_VER_MAX the only accepted value is that constant,
// which is what lets engine gates constant-fold (see v2core).
inline bool behaviorVersionValid(int v)
{
  return v >= V2_VER_MIN && v <= V2_VER_MAX;
}

// ---------------------------------------------------------------------------
// Era -- the engine-identity coordinate
// ---------------------------------------------------------------------------
//
// The v2m FORMAT VERSION read from a file is a LOSSY proxy for the engine
// build that rendered it: it is the score, not the orchestra. Most ledger
// rows track the format version faithfully, but a few flip mid-version on the
// build-DATE timeline that the format cannot see (the flipsAt-5 ASSUMED
// cluster, and the kkrieger transcendental cluster below). For those, the
// caller must supply the missing coordinate.
//
// An Era is exactly that coordinate: a format-version BASELINE plus a SPARSE
// per-row override of the ledger. The format version detected from a file is
// only the default projection -- Era::v(detected). A caller who knows the
// build provenance names a profile from the `eras::` catalog, or refines one
// with .with(). base < 0 is the auto/modern sentinel (no period gating).
//
// Representation is two 32-bit masks (DELTA_COUNT < 32), so a constexpr Era --
// every `eras::` entry is one -- folds every gate at compile time, even across
// a multi-version compiled range (no longer needs V2_VER_MIN == V2_VER_MAX).
static_assert(DELTA_COUNT <= 32, "Era override masks are 32-bit");

struct Era {
  enum Behavior { Old = 0, New = 1 }; // pre-flip vs post-flip behavior

  int      base;       // format-version baseline 0..6 (-1 = auto/modern)
  uint32_t overridden; // bit d set => row d is forced, ignoring base
  uint32_t forcedNew;  // bit d => the forced value (1 = NEW/post-flip)

  static constexpr Era v(int formatVersion) { return { formatVersion, 0u, 0u }; }
  static constexpr Era Auto() { return { -1, 0u, 0u }; }
  constexpr bool isAuto() const { return base < 0; }

  // Derive a new era with one ledger row pinned to Old/New. Chainable.
  constexpr Era with(V2Delta d, Behavior b) const {
    return { base,
             overridden | (1u << (unsigned)d),
             b == New ? (forcedNew |  (1u << (unsigned)d))
                      : (forcedNew & ~(1u << (unsigned)d)) };
  }

  // Does the OLD (pre-flip) behavior apply for row d? Forced rows answer from
  // the masks; the rest fall back to the format-version proxy.
  constexpr bool old(V2Delta d) const {
    return (overridden >> (unsigned)d & 1u) ? !(forcedNew >> (unsigned)d & 1u)
                                            : oldBehavior(d, base);
  }

  // Era voice-pool size (16/32/64), honoring any POLY overrides.
  constexpr int poolSize() const {
    return old(DELTA_POLY_16) ? 16 : old(DELTA_POLY_32) ? 32 : 64;
  }
};

// The catalog: each entry is ONE disassembled period binary. The clean cases
// are just Era::v(version) (no overrides => byte-identical to the proxy). The
// straddlers carry the rows the format version cannot express.
namespace eras {
  inline constexpr Era fr08      = Era::v(0); // year-2000 (fr08-extraction)
  inline constexpr Era flybye    = Era::v(1); // fr-013 (flybye-extraction)
  inline constexpr Era fr014     = Era::v(3);
  inline constexpr Era fr019     = Era::v(4);
  inline constexpr Era candytron = Era::v(5); // fr-030, late-v5 modern core

  // kkrieger-beta (2004-04): a format-v5 file rendered by a near-v6 engine.
  // PROVEN by disasm of the unpacked binary (image base 0x7c0000):
  //   NATIVE_FSIN   -> New : sine osc AND FM both call the fastsin POLY
  //                          @0x808178 (Horner, double coeffs 0x8080a8/b0/b8,
  //                          no `fsin`). This is what makes the FM render as
  //                          renderFMSin (float scheme) and ends the ch15
  //                          silence -- the proven 30s corr 0.44 -> 0.9995 fix.
  //   NATIVE_FPATAN -> New : per-sample overdrive = the fastatan POLY @0x808100
  //                          (rational, coeffs 0x8080c8..0x808110, `fdiv`, no
  //                          `fpatan`; the two native fpatan in the synth are
  //                          the era-independent pi/4 const + set-time odGain2).
  // Everything else stays v5: NO_DCOFFSET old (no 2^-18 bias in the synth
  // render), POLY_32 old (pool 32). So NOT a v6 engine -- forcing v6 would
  // wrongly inject the DC bias and grow the pool to 64. See ACCURACY-LOOSE-
  // ENDS.md sec.10.
  inline constexpr Era kkrieger2004 = Era::v(5)
                                        .with(DELTA_NATIVE_FSIN,   Era::New)
                                        .with(DELTA_NATIVE_FPATAN, Era::New);

  inline constexpr Era release2004 = Era::v(6); // 2004 synth.asm
}

} // namespace v2redux

#endif // V2ERAS_H_
