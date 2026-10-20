// v2dump -- offline render driver for the portable player:
//   v2dump <file.v2m> <out.{wav,f32}> [seconds] [chunkframes]
//
// Output format by extension:
//   .wav      IEEE-float32 stereo 44100 WAV (lossless; same bits as .f32)
//   anything  raw interleaved stereo float32 (the lab's compare.py format)
// For mp3/ogg/etc. encode the WAV externally (e.g. ffmpeg) -- the portable
// tools stay dependency-free by design. (A live ALSA player is planned as a
// separate tool.)

#include "v2redux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define ftruncate _chsize_s
#define fileno _fileno
#else
#include <unistd.h> // ftruncate (trim the auto-mode silence tail)
#endif

// minimal RIFF/WAVE header, format 3 (IEEE float), patched on close
struct WavHeader {
  char riff[4]; uint32_t riffLen; char wave[4];
  char fmt[4]; uint32_t fmtLen;
  uint16_t tag, channels; uint32_t rate, bytesPerSec;
  uint16_t blockAlign, bits;
  char data[4]; uint32_t dataLen;
};

static void wavWriteHeader(FILE *f, uint32_t dataBytes)
{
  WavHeader h;
  memcpy(h.riff, "RIFF", 4); memcpy(h.wave, "WAVE", 4);
  memcpy(h.fmt, "fmt ", 4);  memcpy(h.data, "data", 4);
  h.riffLen = 36 + dataBytes;
  h.fmtLen = 16;
  h.tag = 3;                       // WAVE_FORMAT_IEEE_FLOAT
  h.channels = 2;
  h.rate = 44100;
  h.bits = 32;
  h.blockAlign = (uint16_t)(h.channels * h.bits / 8);
  h.bytesPerSec = h.rate * h.blockAlign;
  h.dataLen = dataBytes;
  fseek(f, 0, SEEK_SET);
  fwrite(&h, sizeof(h), 1, f);
}

static bool hasExt(const char *path, const char *ext)
{
  size_t lp = strlen(path), le = strlen(ext);
  if (lp < le) return false;
  for (size_t i = 0; i < le; i++) {
    char a = path[lp - le + i], b = ext[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (a != b) return false;
  }
  return true;
}

int main(int argc, char **argv)
{
  if (argc < 3) {
    fprintf(stderr, "usage: %s <file.v2m> <out.{wav,f32}> [seconds] [chunkframes]\n"
                    "  seconds omitted (or \"auto\"): render the whole song, let the\n"
                    "  reverb/delay tail ring out to silence, trim it\n", argv[0]);
    return 1;
  }
  const char *inPath = argv[1], *outPath = argv[2];
  bool autoLen = (argc <= 3) || (strcmp(argv[3], "auto") == 0);
  double seconds = autoLen ? 0.0 : atof(argv[3]);
  uint32_t chunk = (argc > 4) ? (uint32_t)atoi(argv[4]) : 4096;
  bool wav = hasExt(outPath, ".wav");

  FILE *f = fopen(inPath, "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", inPath); return 1; }
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  void *data = malloc((size_t)len);
  if (fread(data, 1, (size_t)len, f) != (size_t)len) {
    fprintf(stderr, "short read\n");
    return 1;
  }
  fclose(f);

  v2redux::Player p;
  v2redux::Result r = p.open(data, (size_t)len);
  free(data);
  switch (r) {
  case v2redux::Result::OK:
    break;
  case v2redux::Result::BadFile:
    fprintf(stderr, "%s: not a (supported) v2m file\n", inPath);
    return 2;
  case v2redux::Result::UnsupportedVersion:
    fprintf(stderr, "%s: format v%d outside compiled range [%d..%d]\n",
            inPath, p.fileVersion(), V2_VER_MIN, V2_VER_MAX);
    return 3;
  case v2redux::Result::FpEnvBroken:
    fprintf(stderr, "FP environment flushes subnormals (fast-math build?)\n");
    return 4;
  }
  fprintf(stderr, "%s: format v%d\n", inPath, p.fileVersion());

  FILE *o = fopen(outPath, "wb");
  if (!o) { fprintf(stderr, "cannot write %s\n", outPath); return 1; }
  if (wav)
    wavWriteHeader(o, 0); // placeholder, patched after render

  p.play(0);
  enum { CHUNKMAX = 4096 };
  static float buf[2 * CHUNKMAX];
  if (chunk < 1 || chunk > CHUNKMAX) chunk = 4096;

  uint64_t total;
  if (!autoLen) {
    // fixed length (deterministic; use for A/B comparisons)
    total = (uint64_t)(seconds * 44100.0);
    for (uint64_t done = 0; done < total; ) {
      uint32_t n = (uint32_t)((total - done < (uint64_t)chunk) ? (total - done)
                                                               : (uint64_t)chunk);
      p.render(buf, n);
      fwrite(buf, 2 * sizeof(float), n, o);
      done += n;
    }
  } else {
    // whole song + tail (for listening; same policy as the lab harness's
    // "auto": render while events remain, then ring the reverb/delay tail
    // out to ~1s below -90 dBFS -- or the caps -- and trim the silence)
    const float  kSilence    = 3.0e-5f;             // ~ -90 dBFS
    const uint64_t kSongCap  = 20ull * 60 * 44100;  // runaway/looping songs
    const uint64_t kTailCap  = 60ull * 44100;       // max tail after song end
    const uint64_t kQuietNeed = 44100;              // ~1s of quiet = done

    uint64_t song = 0, tail = 0, quiet = 0;
    total = 0;
    while (p.isPlaying() && song < kSongCap) {
      p.render(buf, chunk);
      fwrite(buf, 2 * sizeof(float), chunk, o);
      song += chunk;
    }
    total = song;
    while (quiet < kQuietNeed && tail < kTailCap) {
      p.render(buf, chunk);
      fwrite(buf, 2 * sizeof(float), chunk, o);
      tail += chunk;
      total += chunk;
      float peak = 0.0f;
      for (uint32_t i = 0; i < 2 * chunk; i++) {
        float a = buf[i] < 0 ? -buf[i] : buf[i];
        if (a > peak) peak = a;
      }
      quiet = (peak < kSilence) ? quiet + chunk : 0;
    }
    if (quiet) { // trim the detected trailing silence
      total -= quiet;
      fflush(o);
      if (ftruncate(fileno(o), (off_t)((wav ? sizeof(WavHeader) : 0)
                                       + total * 2 * sizeof(float))) != 0)
        fprintf(stderr, "warning: could not trim trailing silence\n");
    }
    fprintf(stderr, "auto length: song %.1fs + tail %.1fs\n",
            (double)song / 44100.0, (double)(total - song) / 44100.0);
  }

  if (wav)
    wavWriteHeader(o, (uint32_t)(total * 2 * sizeof(float)));
  fclose(o);
  fprintf(stderr, "wrote %.1fs -> %s%s\n", (double)total / 44100.0, outPath,
          wav ? " (float32 WAV)" : " (raw f32)");
  return 0;
}
