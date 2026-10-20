# V2 Redux — a faithful, deterministic V2M player

A self-contained C++17 player for **V2M** files — the music format of Farbrausch's
**V2 synthesizer** (Tammo "kb" Hinrichs, 2000–2008), the engine behind demos like
*.the .product*, *candytron*, and *.kkrieger*.

It is a clean-room C++ port of the original V2 engine (`synth.asm` / `libv2` /
the v2mplayer) with one obsession: **be honest to the original**. Not "sounds
close" — bit-for-bit faithful to how the genuine engine actually rendered each
song, reproduced from disassembly of the original release binaries.

## What makes it different

- **Version-native.** Plays every V2M format version (0–6) at its *own* era's
  behaviour — no lossy "convert to latest" step. The synth had real behavioural
  changes over the years (oscillator rewrites, the FM scheme, polyphony, the
  noise generator); this player reproduces each.
- **Oracle-proven fidelity.** Songs were carved from the original demo
  executables and rendered through *their own* engine, then matched
  sample-for-sample. Multiple eras are **whole-song bit-exact** against the
  genuine binary (v0 *.the .product*, v1 *flybye*, …); the rest sit at a
  measured, documented ε-floor (sub-perceptual transcendental ULP). The full
  story is in [`docs/ACCURACY.md`](docs/ACCURACY.md).
- **Deterministic on every host.** Identical float32 output bit-for-bit across
  compilers, optimization levels, and **architectures** — verified equal on
  x86_64 and aarch64. The audio path uses the engine's own polynomial
  transcendentals (no platform libm `sin`/`cos`/`pow`) over strict IEEE-754
  binary32, matching the original x87 `PC=24` rounding op-for-op.
- **Self-contained.** No dependencies beyond the C++ standard library. No inline
  asm, no x87/SSE assumptions, 32/64-bit clean, builds on GCC / Clang / MSVC.
- **Speech.** The Ronan phoneme/speech channel is ported, working, and **on by
  default** (build `-DV2_RONAN=0` for a speech-silent variant).

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build           # runs the test suite (see below)
```

The CMake build pins the determinism contract per compiler (no FMA contraction,
never fast-math, SSE2 not x87).

## Render a song

```sh
build/v2dump song.v2m out.wav            # whole song to WAV, tail trimmed
build/v2dump song.v2m out.f32 60         # first 60 s as raw float32 stereo
```

## Using the library

```cpp
#include "v2redux.h"
using namespace v2redux;

Player p;
if (p.open(v2mBytes, v2mLength) == Result::OK) {
    StereoSample buf[4096];
    p.render(buf, 4096);                  // 44100 Hz, interleaved float32 L/R
}
```

One song per `Player`; instances are independent and may run concurrently. When
the provenance of a file is known to need a specific engine *build* (e.g. the
`.kkrieger` beta, which ships the float-FM scheme inside a v5 file), pass an
`Era` from the `eras::` catalog — see [`docs/ACCURACY.md`](docs/ACCURACY.md).

## Tests

`ctest` runs: math-kernel checks, a table check against the original `sounddef.h`,
multi-instance independence, and **`corpus_hashes`** — the cross-host determinism
gate. It renders the bundled corpus and compares SHA-256 against the checked-in
`test/baselines.sha256`; these hashes are identical on every conforming host.

The test corpus is genuine songs carved from the original demo releases — see
**[`corpus/`](corpus/)** and its provenance/licensing note below.

## Repository layout

```
src/        the engine — library sources, headers, the v2dump CLI
test/       the test suite (ctest) + the determinism baselines
corpus/     genuine test songs carved from the demos (see licensing)
toolkit/    the carve/unpack/oracle tooling that reproduces the corpus
docs/       REFERENCE, ACCURACY, CONVENTIONS
```

## Reproducing the corpus

The `corpus/` songs are not just bundled — they are **reproducible** from the
original demo releases with the tooling in [`toolkit/`](toolkit/):

```sh
toolkit/era unpack <demo.exe> image.bin           # depack the release binary
toolkit/era carve  image.bin --extract 0 --out song.v2m
```

Each song's source release and SHA-256 are in
[`corpus/embedded/PROVENANCE.md`](corpus/embedded/PROVENANCE.md), so the corpus
can be regenerated rather than taken on trust. (The hard unpackers want
`pip install capstone unicorn`; the common ones are pure Python.)

## Documentation

- **[`docs/REFERENCE.md`](docs/REFERENCE.md)** — the C++ API, the version & era
  support model, the determinism contract, and the measured-fidelity (ε) table.
- **[`docs/ACCURACY.md`](docs/ACCURACY.md)** — the fidelity record: how each era
  was validated against its genuine engine, and the residuals that remain.
- **[`docs/CONVENTIONS.md`](docs/CONVENTIONS.md)** — the coding rules that keep
  the output bit-exact (float-only audio path, no fast-math, etc.).

## Licensing — please read

- **This port and all original work in this repo** (the C++ engine port, the
  test/oracle harness, the docs) is released into the **public domain (CC0)**.
- **The V2 engine** it derives from was placed in the public domain by its
  author, Tammo "kb" Hinrichs; that notice is preserved in [`LICENSE`](LICENSE).
- **The songs in [`corpus/`](corpus/) are NOT public domain.** They are
  compositions © their original authors (Farbrausch / .theprodukkt), carved from
  freely-distributed demo releases and bundled **only as test vectors**, with
  full provenance. They are **not** relicensed. See
  [`corpus/LICENSE`](corpus/LICENSE). If you are a rights holder and want a song
  removed, please open an issue — it will be removed immediately.

See [`LICENSE`](LICENSE) and [`corpus/LICENSE`](corpus/LICENSE) for the details.

## Credits

V2 synthesizer © Tammo "kb" Hinrichs / Farbrausch. This is an independent,
fidelity-focused C++ port. Reverse-engineering it has been a privilege.
