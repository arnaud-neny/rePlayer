// v2play -- native real-time player + visualizer for the portable V2 synth.
//
//   v2play <file.v2m> [--era N]
//
// Audio: sokol_audio (ALSA / CoreAudio / WASAPI) pulls float32 stereo straight
// from v2redux::Player::render() on its own thread. Graphics: sokol_gfx +
// sokol_gl draw an oscilloscope (time domain), a log-frequency spectrum
// analyzer, and L/R VU bars from a snapshot of the same audio.
//
// The synth core keeps its strict determinism contract; everything in THIS
// file (FFT, scope trigger, drawing) is display-only and deliberately outside
// that contract -- it never feeds back into the rendered signal.
//
// Keys:  space = pause   r = restart   l = loop toggle   q/esc = quit
//        left/right = seek +-5s   up/down = seek +-30s
//        1..9,0 = toggle-mute channels 1..10   ctrl+1..6 = channels 11..16
//        shift+digit = solo that channel (toggle)   ` = un-mute all
// Mouse: left-click a channel cell = toggle its mute; right-click = solo it
//        (mute all but that one); click/drag the bottom timeline = seek.
//
// A 16-cell channel strip across the top: each cell is split into two bars --
// LEFT = output AMPLITUDE (height + green->yellow->red VU color,
// synthGetChannelPeaks), RIGHT = POLYPHONY (height = active voice count, blue,
// synthGetPoly). Both show regardless of mute. The channel NUMBER below is the
// on/off indicator (green = audible, red = muted, yellow = soloed). Muting
// happens at the synth mix stage (a muted channel still renders but feeds no
// bus -- instant, in-phase, no hung notes) and is outside the determinism
// contract: the default all-audible mask renders the exact reference bits. A
// timeline slider along the bottom shows the playhead (song length measured at
// load from the sequence timing, no full render).

#include "v2redux.h"

#include "third_party/sokol/sokol_config.h"
#include "third_party/sokol/sokol_log.h"
#include "third_party/sokol/sokol_gfx.h"
#include "third_party/sokol/sokol_app.h"
#include "third_party/sokol/sokol_glue.h"
#include "third_party/sokol/sokol_gl.h"
#include "third_party/sokol/sokol_audio.h"

#include <atomic>
#include <mutex>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Optional compile-time-embedded song (CMake -DV2_EMBED_SONG=<file.v2m>): the
// generated translation unit defines these with C linkage. When present and no
// file argument is given, v2play plays this song -- a self-contained demo exe.
#ifdef V2PLAY_EMBEDDED
extern "C" const unsigned char v2play_song[];
extern "C" const unsigned long v2play_song_len;
#ifndef V2PLAY_EMBED_NAME
#define V2PLAY_EMBED_NAME "embedded song"
#endif
#endif

// ---------------------------------------------------------------------------
// shared audio<->gui state
// ---------------------------------------------------------------------------

static const int   SAMPLE_RATE = 44100;
static const int   RING        = 1 << 15;   // 32768 mono samples
static const int   RING_MASK   = RING - 1;
static const int   SNAP        = 8192;      // window pulled per frame
static const int   FFT_N       = 8192;      // <= SNAP (~186ms -> ~5.4Hz bins)

static v2redux::Player g_player;
static bool               g_opened = false;

static std::mutex         g_mtx;            // guards the ring + block peaks
static float              g_ring[RING];
static uint32_t           g_widx = 0;
static float              g_blockPeakL = 0.0f, g_blockPeakR = 0.0f;

static std::atomic<bool>  g_paused{false};
static std::atomic<bool>  g_restart{false};
static std::atomic<bool>  g_loop{true};

// channel mute/solo. The GUI thread owns g_muted/g_soloCh (set in event_cb) and
// publishes the effective audible mask (bit ch = audible) into g_chanMask; the
// audio thread reads it and hands it to the player before each render block.
static std::atomic<uint32_t> g_chanMask{0xffff};
static uint16_t           g_muted  = 0;   // bit ch set = user-muted (GUI thread)
static int                g_soloCh = -1;  // soloed channel, or -1 (GUI thread)

// timeline: g_lenMs is the song's loop length (where isPlaying() goes false),
// measured once at load; g_scrubbing tracks a timeline drag (GUI thread only).
static long long          g_lenMs = 0;    // 0 = unknown
static bool               g_scrubbing = false;

// playhead + seeking. The audio thread owns g_posFrames and publishes the
// position in ms; the GUI thread reads g_playMs and posts a target into
// g_seekMs (-1 = none) which the audio thread consumes via play(targetMs).
static uint64_t           g_posFrames = 0;       // audio-thread owned
static std::atomic<long long> g_playMs{0};       // published position (ms)
static std::atomic<long long> g_seekMs{-1};      // requested seek target (ms)

// ---------------------------------------------------------------------------
// audio thread: render + mirror a mono sum into the ring
// ---------------------------------------------------------------------------

static void audio_cb(float *buffer, int nframes, int nchan)
{
  if (!g_opened) {
    memset(buffer, 0, sizeof(float) * (size_t)nframes * nchan);
    return;
  }

  // position changes take effect even while paused (reposition, stay paused)
  if (g_restart.exchange(false)) {
    g_player.play(0); g_posFrames = 0; g_playMs.store(0);
  }
  long long seek = g_seekMs.exchange(-1);
  if (seek >= 0) {
    g_player.play((uint32_t)seek);
    g_posFrames = (uint64_t)seek * SAMPLE_RATE / 1000;
    g_playMs.store(seek);
  }

  if (g_paused.load(std::memory_order_relaxed)) {
    memset(buffer, 0, sizeof(float) * (size_t)nframes * nchan);
    return;
  }

  // device buffer is interleaved stereo float32 -- exactly render()'s format
  g_player.setChannelMask(g_chanMask.load(std::memory_order_relaxed));
  g_player.render(buffer, (uint32_t)nframes);

  if (g_loop.load(std::memory_order_relaxed) && !g_player.isPlaying()) {
    g_player.play(0); g_posFrames = 0; // loop the demo (cuts the ring-out tail)
  }
  g_posFrames += (uint64_t)nframes;
  g_playMs.store((long long)(g_posFrames * 1000 / SAMPLE_RATE));

  float pL = 0.0f, pR = 0.0f;
  std::lock_guard<std::mutex> lock(g_mtx);
  for (int i = 0; i < nframes; i++) {
    float l = buffer[2 * i], r = buffer[2 * i + 1];
    g_ring[g_widx & RING_MASK] = 0.5f * (l + r);
    g_widx++;
    float al = fabsf(l), ar = fabsf(r);
    if (al > pL) pL = al;
    if (ar > pR) pR = ar;
  }
  g_blockPeakL = pL;
  g_blockPeakR = pR;
}

// ---------------------------------------------------------------------------
// tiny display-only radix-2 FFT (in place)
// ---------------------------------------------------------------------------

static void fft(float *re, float *im, int n)
{
  for (int i = 1, j = 0; i < n; i++) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      float tr = re[i]; re[i] = re[j]; re[j] = tr;
      float ti = im[i]; im[i] = im[j]; im[j] = ti;
    }
  }
  for (int len = 2; len <= n; len <<= 1) {
    float ang = -2.0f * 3.14159265358979f / (float)len;
    float wr = cosf(ang), wi = sinf(ang);
    for (int i = 0; i < n; i += len) {
      float cwr = 1.0f, cwi = 0.0f;
      for (int k = 0; k < len / 2; k++) {
        int a = i + k, b = a + len / 2;
        float vr = re[b] * cwr - im[b] * cwi;
        float vi = re[b] * cwi + im[b] * cwr;
        float ur = re[a], ui = im[a];
        re[a] = ur + vr; im[a] = ui + vi;
        re[b] = ur - vr; im[b] = ui - vi;
        float n2 = cwr * wr - cwi * wi;
        cwi = cwr * wi + cwi * wr; cwr = n2;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// drawing helpers (NDC, sokol-gl default identity matrices)
// ---------------------------------------------------------------------------

// region: channel strip pinned at the very top, oscilloscope on the top half,
// spectrum on the bottom half
static const float SCOPE_Y0 = 0.05f, SCOPE_Y1 = 0.95f;   // -> upper band below
static const float SCOPE_CY = 0.45f, SCOPE_H = 0.36f;    // center / half-height
static const float SPEC_Y0  = -0.95f, SPEC_H = 0.85f;    // bars grow up from y0
static const float CHAN_Y0  = 0.895f, CHAN_Y1 = 0.985f;  // 16-cell channel strip
static const float CHAN_X0  = -0.97f, CHAN_X1 = 0.97f;   // strip horizontal span
static const float SEEK_X0  = -0.94f, SEEK_X1 = 0.94f;   // timeline slider span
static const float SEEK_Y0  = -0.986f, SEEK_Y1 = -0.957f;// (bottom margin, below spectrum)

static void draw_grid()
{
  sgl_begin_lines();
  sgl_c4f(0.16f, 0.18f, 0.20f, 1.0f);
  // scope zero line
  sgl_v2f(-0.97f, SCOPE_CY); sgl_v2f(0.97f, SCOPE_CY);
  // divider
  sgl_v2f(-0.97f, 0.0f);     sgl_v2f(0.97f, 0.0f);
  // spectrum baseline
  sgl_v2f(-0.97f, SPEC_Y0);  sgl_v2f(0.97f, SPEC_Y0);
  sgl_end();
  (void)SCOPE_Y0; (void)SCOPE_Y1;
}

// minimal 7-segment digit. Each segment is a solid filled QUAD (not a 1px line)
// so the strokes stay crisp under MSAA instead of going thin/fuzzy. bit0=a (top)
// .. bit6=g (middle). Display-only; avoids pulling in a font/atlas.
static void draw_digit(float left, float bot, float dw, float dh, int d)
{
  static const unsigned char SEG[10] =
    { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
  if (d < 0 || d > 9) return;
  unsigned char s = SEG[d];
  float right = left + dw, top = bot + dh, mid = bot + dh * 0.5f;
  float tx = dw * 0.30f, ty = dh * 0.16f;   // stroke thickness (x for verticals, y for horizontals)
  sgl_begin_quads();
#define SEGQ(x0, y0, x1, y1) do { sgl_v2f(x0, y0); sgl_v2f(x1, y0); sgl_v2f(x1, y1); sgl_v2f(x0, y1); } while (0)
  if (s & 0x01) SEGQ(left,       top - ty,      right,     top);            // a (top)
  if (s & 0x40) SEGQ(left,       mid - ty*0.5f, right,     mid + ty*0.5f);  // g (middle)
  if (s & 0x08) SEGQ(left,       bot,           right,     bot + ty);       // d (bottom)
  if (s & 0x20) SEGQ(left,       mid,           left + tx, top);            // f (top-left)
  if (s & 0x02) SEGQ(right - tx, mid,           right,     top);            // b (top-right)
  if (s & 0x10) SEGQ(left,       bot,           left + tx, mid);            // e (bottom-left)
  if (s & 0x04) SEGQ(right - tx, bot,           right,     mid);            // c (bottom-right)
#undef SEGQ
  sgl_end();
}

// 1- or 2-digit number centered horizontally on cx, sitting on baseline baseY
static void draw_number(float cx, float baseY, int n, float r, float g, float b)
{
  const float dw = 0.015f, dh = 0.032f, gap = 0.007f;
  int digits[2], nd = 0;
  if (n >= 10) digits[nd++] = (n / 10) % 10;
  digits[nd++] = n % 10;
  float total = nd * dw + (nd - 1) * gap;
  float x = cx - total * 0.5f;
  sgl_c4f(r, g, b, 1.0f);
  for (int i = 0; i < nd; i++) {
    draw_digit(x, baseY, dw, dh, digits[i]);
    x += dw + gap;
  }
}

// level -> VU color ramp: green (quiet) -> yellow -> red (hot)
static void vu_ramp(float h, float *r, float *g, float *b)
{
  if (h < 0.0f) h = 0.0f; if (h > 1.0f) h = 1.0f;
  *r = h < 0.5f ? 0.15f + h * 1.6f : 1.0f;
  *g = h < 0.85f ? 0.95f : 1.0f - (h - 0.85f) * 5.0f;
  if (*g < 0.0f) *g = 0.0f;
  *b = 0.22f;
}

// 16-cell channel strip, each cell split into two bars:
//   LEFT  = output AMPLITUDE (height + VU color green->yellow->red)
//   RIGHT = POLYPHONY        (height = active voice count, cool blue)
// Both shown regardless of mute. The channel NUMBER below is the on/off
// indicator (green = audible, red = muted, yellow = soloed).
static void draw_channels(const float *level, const int *poly,
                          uint16_t muted, int soloCh)
{
  const float x0 = CHAN_X0, x1 = CHAN_X1;
  const float w  = (x1 - x0) / 16.0f;
  const float H  = CHAN_Y1 - CHAN_Y0;
  const float POLYREF = 8.0f;   // voice count that reads as a full-height bar

  for (int c = 0; c < 16; c++) {
    float cx0 = x0 + w * c + 0.004f;
    float cx1 = x0 + w * (c + 1) - 0.004f;
    float mid = 0.5f * (cx0 + cx1);
    float gap = 0.0015f;
    bool audible = soloCh >= 0 ? (c == soloCh) : !((muted >> c) & 1);

    // left half: amplitude (height + VU color)
    float lvl = level[c]; if (lvl < 0.0f) lvl = 0.0f; if (lvl > 1.0f) lvl = 1.0f;
    if (lvl > 0.001f) {
      float r, g, b; vu_ramp(lvl, &r, &g, &b);
      sgl_begin_quads();
      sgl_c4f(r, g, b, 1.0f);
      sgl_v2f(cx0, CHAN_Y0); sgl_v2f(mid - gap, CHAN_Y0);
      sgl_v2f(mid - gap, CHAN_Y0 + H * lvl); sgl_v2f(cx0, CHAN_Y0 + H * lvl);
      sgl_end();
    }

    // right half: polyphony (height = voice count, blue)
    float pol = (float)poly[c] / POLYREF; if (pol > 1.0f) pol = 1.0f;
    if (pol > 0.0f) {
      sgl_begin_quads();
      sgl_c4f(0.30f, 0.58f, 0.95f, 1.0f);
      sgl_v2f(mid + gap, CHAN_Y0); sgl_v2f(cx1, CHAN_Y0);
      sgl_v2f(cx1, CHAN_Y0 + H * pol); sgl_v2f(mid + gap, CHAN_Y0 + H * pol);
      sgl_end();
    }

    // cell frame + center divider (brighter outline for the soloed channel)
    sgl_begin_lines();
    if (soloCh == c) sgl_c4f(0.95f, 0.9f, 0.2f, 1.0f);
    else             sgl_c4f(0.22f, 0.24f, 0.28f, 1.0f);
    sgl_v2f(cx0, CHAN_Y0); sgl_v2f(cx1, CHAN_Y0);
    sgl_v2f(cx1, CHAN_Y0); sgl_v2f(cx1, CHAN_Y1);
    sgl_v2f(cx1, CHAN_Y1); sgl_v2f(cx0, CHAN_Y1);
    sgl_v2f(cx0, CHAN_Y1); sgl_v2f(cx0, CHAN_Y0);
    sgl_c4f(0.18f, 0.19f, 0.22f, 1.0f);
    sgl_v2f(mid, CHAN_Y0); sgl_v2f(mid, CHAN_Y1);   // half divider
    sgl_end();

    // channel number = ON/OFF indicator (green = on, red = off, yellow = solo)
    float lr, lg, lb;
    if (soloCh == c)  { lr = 0.95f; lg = 0.9f;  lb = 0.25f; }
    else if (audible) { lr = 0.35f; lg = 0.92f; lb = 0.45f; }
    else              { lr = 0.85f; lg = 0.16f; lb = 0.16f; }
    draw_number(mid, CHAN_Y0 - 0.052f, c + 1, lr, lg, lb);
  }
}

// timeline slider in the bottom margin: track + filled-to-playhead + thumb.
static void draw_seekbar(long long playMs, long long lenMs)
{
  const float y0 = SEEK_Y0, y1 = SEEK_Y1;
  float frac = lenMs > 0 ? (float)((double)playMs / (double)lenMs) : 0.0f;
  if (frac < 0.0f) frac = 0.0f; if (frac > 1.0f) frac = 1.0f;
  float px = SEEK_X0 + (SEEK_X1 - SEEK_X0) * frac;

  // track
  sgl_begin_quads();
  sgl_c4f(0.13f, 0.14f, 0.17f, 1.0f);
  sgl_v2f(SEEK_X0, y0); sgl_v2f(SEEK_X1, y0); sgl_v2f(SEEK_X1, y1); sgl_v2f(SEEK_X0, y1);
  // elapsed fill
  sgl_c4f(0.22f, 0.52f, 0.9f, 1.0f);
  sgl_v2f(SEEK_X0, y0); sgl_v2f(px, y0); sgl_v2f(px, y1); sgl_v2f(SEEK_X0, y1);
  // thumb (a touch taller than the track)
  float hw = 0.006f, over = 0.007f;
  sgl_c4f(0.92f, 0.95f, 1.0f, 1.0f);
  sgl_v2f(px - hw, y0 - over); sgl_v2f(px + hw, y0 - over);
  sgl_v2f(px + hw, y1 + over); sgl_v2f(px - hw, y1 + over);
  sgl_end();

  // frame
  sgl_begin_line_strip();
  sgl_c4f(0.30f, 0.32f, 0.36f, 1.0f);
  sgl_v2f(SEEK_X0, y0); sgl_v2f(SEEK_X1, y0);
  sgl_v2f(SEEK_X1, y1); sgl_v2f(SEEK_X0, y1); sgl_v2f(SEEK_X0, y0);
  sgl_end();
}

static void draw_scope(const float *snap)
{
  // draw from the TAIL so the scope shows the most recent audio (the FFT uses
  // the whole, longer window; the scope must not inherit its latency).
  const int N = 1024;                 // samples drawn
  const int searchLo = SNAP - N - 2048;

  // hysteresis trigger: arm on a dip below -h, then fire on the next rising
  // zero crossing. Rejects near-zero noise wiggles so the trace stands still.
  float peak = 1e-6f;
  for (int i = searchLo; i < SNAP; i++) {
    float a = fabsf(snap[i]); if (a > peak) peak = a;
  }
  float h = 0.10f * peak;
  static int lastStart = searchLo;
  int start = -1;
  bool armed = false;
  for (int i = searchLo + 1; i < SNAP - N; i++) {
    if (snap[i] < -h) armed = true;
    if (armed && snap[i - 1] <= 0.0f && snap[i] > 0.0f) { start = i; break; }
  }
  if (start < 0) start = lastStart;   // silence/no edge: hold the last position
  else lastStart = start;
  if (start < searchLo) start = searchLo;
  if (start > SNAP - N) start = SNAP - N;

  sgl_begin_line_strip();
  sgl_c4f(0.25f, 1.0f, 0.45f, 1.0f);
  for (int i = 0; i < N; i++) {
    float s = snap[start + i];
    float x = -0.97f + 1.94f * ((float)i / (float)(N - 1));
    float y = SCOPE_CY + s * SCOPE_H;
    if (y > SCOPE_CY + SCOPE_H) y = SCOPE_CY + SCOPE_H;
    if (y < SCOPE_CY - SCOPE_H) y = SCOPE_CY - SCOPE_H;
    sgl_v2f(x, y);
  }
  sgl_end();
}

static void draw_spectrum(const float *snap)
{
  static float re[FFT_N], im[FFT_N];
  const float *src = snap + (SNAP - FFT_N);     // most recent FFT_N samples
  for (int i = 0; i < FFT_N; i++) {
    float w = 0.5f - 0.5f * cosf(2.0f * 3.14159265358979f * i / (FFT_N - 1)); // Hann
    re[i] = src[i] * w;
    im[i] = 0.0f;
  }
  fft(re, im, FFT_N);

  const int   NBARS  = 120;
  const float binHz  = (float)SAMPLE_RATE / FFT_N;
  const float fmin   = 35.0f, fmax = 18000.0f;
  const float dbMin  = -70.0f, dbMax = 0.0f;
  const float gain   = 3.5f;                      // visual headroom fudge

  auto magAt = [&](int k) -> float {
    if (k < 1) k = 1;
    if (k > FFT_N / 2) k = FFT_N / 2;
    return sqrtf(re[k] * re[k] + im[k] * im[k]) * (2.0f / FFT_N) * gain;
  };

  sgl_begin_quads();
  for (int b = 0; b < NBARS; b++) {
    float f0 = fmin * powf(fmax / fmin, (float)b / NBARS);
    float f1 = fmin * powf(fmax / fmin, (float)(b + 1) / NBARS);
    int i0 = (int)(f0 / binHz), i1 = (int)(f1 / binHz);
    float mag;
    if (i1 > i0) {
      // band spans >=1 full bin: take the peak across the covered bins
      mag = 0.0f;
      for (int k = (i0 < 1 ? 1 : i0); k <= i1; k++) {
        float m = magAt(k);
        if (m > mag) mag = m;
      }
    } else {
      // sub-bin band (the low end): linearly interpolate at the band center
      float fc = sqrtf(f0 * f1), kf = fc / binHz;
      int k = (int)kf; float fr = kf - k;
      mag = magAt(k) * (1.0f - fr) + magAt(k + 1) * fr;
    }
    float db = 20.0f * log10f(mag + 1e-9f);
    float h  = (db - dbMin) / (dbMax - dbMin);
    if (h < 0.0f) h = 0.0f; if (h > 1.0f) h = 1.0f;

    float x0 = -0.97f + 1.94f * ((float)b / NBARS);
    float x1 = -0.97f + 1.94f * ((float)(b + 1) / NBARS) - 0.004f;
    float y0 = SPEC_Y0;
    float y1 = SPEC_Y0 + h * SPEC_H;
    // green -> yellow -> red by height
    float r = h < 0.5f ? h * 2.0f : 1.0f;
    float g = h < 0.5f ? 1.0f : 1.0f - (h - 0.5f) * 1.6f;
    sgl_c4f(r * 0.9f + 0.05f, g, 0.25f, 1.0f);
    sgl_v2f(x0, y0); sgl_v2f(x1, y0); sgl_v2f(x1, y1); sgl_v2f(x0, y1);
  }
  sgl_end();
}

static void vu_color(float h)
{
  float r = h > 0.6f ? 1.0f : h * 1.4f;
  float g = h > 0.85f ? 1.0f - (h - 0.85f) * 4.0f : 1.0f;
  if (g < 0.0f) g = 0.0f;
  sgl_c4f(r, g, 0.20f, 1.0f);
}

static void draw_vu(float pL, float pR)
{
  // two thin vertical bars hugging the left/right edges, full window height
  auto bar = [](float x0, float x1, float level) {
    float y0 = -0.97f, y1 = -0.97f + 1.94f * level;
    sgl_begin_quads();
    vu_color(level);
    sgl_v2f(x0, y0); sgl_v2f(x1, y0); sgl_v2f(x1, y1); sgl_v2f(x0, y1);
    sgl_end();
  };
  bar(-0.995f, -0.975f, pL);
  bar(0.975f, 0.995f, pR);
}

// ---------------------------------------------------------------------------
// sokol-app lifecycle
// ---------------------------------------------------------------------------

static void init_cb(void)
{
  sg_desc gd = {};
  gd.environment = sglue_environment();
  gd.logger.func = slog_func;
  sg_setup(&gd);

  sgl_desc_t sd = {};
  sd.logger.func = slog_func;
  sgl_setup(&sd);

  // Song length for the timeline scale: derived from the sequence timing only
  // (no DSP), so it's instant. lengthMs() implies a synth reset, so play(0)
  // after it to (re)start clean. Done before saudio_setup (single-threaded).
  if (g_opened) {
    g_lenMs = g_player.lengthMs();
    fprintf(stderr, "song length: %lld:%02lld\n", g_lenMs / 60000, (g_lenMs / 1000) % 60);
    g_player.play(0);
  }

  saudio_desc ad = {};
  ad.sample_rate   = SAMPLE_RATE;
  ad.num_channels  = 2;
  ad.buffer_frames = 1024;
  ad.stream_cb     = audio_cb;
  ad.logger.func   = slog_func;
  saudio_setup(&ad);

  if (saudio_sample_rate() != SAMPLE_RATE)
    fprintf(stderr, "warning: audio device runs at %d Hz, song is %d Hz -- "
            "pitch will be off\n", saudio_sample_rate(), SAMPLE_RATE);
}

static void frame_cb(void)
{
  static float snap[SNAP];
  float pkL, pkR;
  {
    std::lock_guard<std::mutex> lock(g_mtx);
    uint32_t w = g_widx;
    for (int i = 0; i < SNAP; i++)
      snap[i] = g_ring[(w - SNAP + i) & RING_MASK];
    pkL = g_blockPeakL;
    pkR = g_blockPeakR;
  }

  // VU ballistics: instant attack, timed release
  static float vuL = 0.0f, vuR = 0.0f;
  float dt = (float)sapp_frame_duration();
  float rel = dt * 1.8f;
  vuL = pkL > vuL ? pkL : vuL - rel; if (vuL < 0.0f) vuL = 0.0f;
  vuR = pkR > vuR ? pkR : vuR - rel; if (vuR < 0.0f) vuR = 0.0f;

  // per-channel meters (display-only, benign reads of synth state the audio
  // thread is updating): amplitude peak (read-and-clear, with VU ballistics --
  // instant attack, timed release) and the active voice count (polyphony).
  static float chLvl[16] = {0};
  float chRaw[16];
  int   poly[16];
  g_player.getChannelLevels(chRaw);
  g_player.getChannelPoly(poly);
  float chRel = dt * 2.2f;
  for (int c = 0; c < 16; c++) {
    float v = chRaw[c]; if (v > 1.0f) v = 1.0f;
    chLvl[c] = v > chLvl[c] ? v : chLvl[c] - chRel;
    if (chLvl[c] < 0.0f) chLvl[c] = 0.0f;
  }

  sgl_defaults();
  draw_grid();
  draw_spectrum(snap);
  draw_scope(snap);
  draw_channels(chLvl, poly, g_muted, g_soloCh);
  draw_seekbar(g_playMs.load(), g_lenMs);
  draw_vu(vuL > 1.0f ? 1.0f : vuL, vuR > 1.0f ? 1.0f : vuR);

  sg_pass pass = {};
  pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
  pass.action.colors[0].clear_value = { 0.03f, 0.03f, 0.045f, 1.0f };
  pass.swapchain = sglue_swapchain();
  sg_begin_pass(&pass);
  sgl_draw();
  sg_end_pass();
  sg_commit();
}

// post a relative seek from the current published position (lower-clamped at
// 0; no upper clamp -- the song length isn't exposed by the player API)
static void seek_by(long long deltaMs)
{
  long long t = g_playMs.load() + deltaMs;
  if (t < 0) t = 0;
  g_seekMs.store(t);
  fprintf(stderr, "seek -> %lld:%02lld\n", t / 60000, (t / 1000) % 60);
}

// recompute the audible mask from the GUI-thread mute/solo state and publish it
static void publish_mask(void)
{
  uint32_t m = g_soloCh >= 0 ? (1u << g_soloCh)
                             : (~(uint32_t)g_muted) & 0xffffu;
  g_chanMask.store(m, std::memory_order_relaxed);
}

// shared mute/solo toggles (used by both the number keys and the mouse)
static void toggle_mute(int ch)
{
  g_muted ^= (uint16_t)(1u << ch);   // flip this channel, drop any active solo
  g_soloCh = -1;
  publish_mask();
}
static void toggle_solo(int ch)
{
  g_soloCh = (g_soloCh == ch) ? -1 : ch;   // solo this channel (or un-solo)
  publish_mask();
}

// digit -> channel toggle. Ctrl selects the upper bank (channels 11..16); Shift
// solos instead of muting. Returns true if the key was a channel key.
static bool handle_channel_key(int keycode, uint32_t mods)
{
  int digit;
  if (keycode >= SAPP_KEYCODE_1 && keycode <= SAPP_KEYCODE_9)
    digit = keycode - SAPP_KEYCODE_1 + 1;   // 1..9
  else if (keycode == SAPP_KEYCODE_0)
    digit = 10;
  else
    return false;

  int ch;
  if (mods & SAPP_MODIFIER_CTRL) {
    if (digit < 1 || digit > 6) return true; // upper bank is channels 11..16
    ch = 10 + (digit - 1);
  } else {
    ch = digit - 1;                          // 0..9
  }

  if (mods & SAPP_MODIFIER_SHIFT) toggle_solo(ch);
  else                            toggle_mute(ch);
  return true;
}

// mouse pixel -> NDC (sokol mouse_x/y are framebuffer pixels; y is top-down)
static void mouse_ndc(const sapp_event *e, float *nx, float *ny)
{
  float w = (float)sapp_width(), h = (float)sapp_height();
  *nx = (w > 0) ? (e->mouse_x / w) * 2.0f - 1.0f : 0.0f;
  *ny = (h > 0) ? 1.0f - (e->mouse_y / h) * 2.0f : 0.0f;
}

// which channel cell is under (nx,ny)? -1 if not over the strip (the hit band
// extends down to include the labels printed just below the cells).
static int hit_channel(float nx, float ny)
{
  if (ny > CHAN_Y1 || ny < CHAN_Y0 - 0.06f) return -1;
  if (nx < CHAN_X0 || nx > CHAN_X1) return -1;
  int ch = (int)((nx - CHAN_X0) / ((CHAN_X1 - CHAN_X0) / 16.0f));
  return (ch < 0 || ch > 15) ? -1 : ch;
}

// is (nx,ny) over the timeline? returns fraction 0..1, or -1 if not. The hit
// band is padded vertically so the thin bar is easy to grab.
static float hit_seek(float nx, float ny)
{
  if (ny < SEEK_Y0 - 0.02f || ny > SEEK_Y1 + 0.02f) return -1.0f;
  float f = (nx - SEEK_X0) / (SEEK_X1 - SEEK_X0);
  if (f < 0.0f) f = 0.0f; if (f > 1.0f) f = 1.0f;
  return f;
}

// post a seek to a fraction of the song (uses g_lenMs; lower-clamped at 0)
static void seek_to_fraction(float f)
{
  if (g_lenMs <= 0) return;
  long long t = (long long)(f * (double)g_lenMs);
  if (t < 0) t = 0;
  if (t >= g_lenMs) t = g_lenMs - 1;
  g_seekMs.store(t);
}

static void event_cb(const sapp_event *e)
{
  switch (e->type) {
  case SAPP_EVENTTYPE_MOUSE_DOWN: {
    float nx, ny; mouse_ndc(e, &nx, &ny);
    if (e->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
      float f = hit_seek(nx, ny);
      if (f >= 0.0f) { g_scrubbing = true; seek_to_fraction(f); break; }
      int ch = hit_channel(nx, ny);            // left-click a cell = mute toggle
      if (ch >= 0) toggle_mute(ch);
    } else if (e->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
      int ch = hit_channel(nx, ny);            // right-click a cell = solo (mute rest)
      if (ch >= 0) toggle_solo(ch);
    }
    break;
  }
  case SAPP_EVENTTYPE_MOUSE_MOVE:
    if (g_scrubbing) {                          // dragging the timeline thumb
      float nx, ny; mouse_ndc(e, &nx, &ny);
      float f = (nx - SEEK_X0) / (SEEK_X1 - SEEK_X0);
      if (f < 0.0f) f = 0.0f; if (f > 1.0f) f = 1.0f;
      seek_to_fraction(f);
    }
    break;
  case SAPP_EVENTTYPE_MOUSE_UP:
    g_scrubbing = false;
    break;
  case SAPP_EVENTTYPE_KEY_DOWN:
    if (handle_channel_key(e->key_code, e->modifiers))
      return;
    switch (e->key_code) {
    case SAPP_KEYCODE_GRAVE_ACCENT:
      g_muted = 0; g_soloCh = -1; publish_mask(); break;
    case SAPP_KEYCODE_ESCAPE:
    case SAPP_KEYCODE_Q: sapp_request_quit(); break;
    case SAPP_KEYCODE_SPACE:
      g_paused.store(!g_paused.load()); break;
    case SAPP_KEYCODE_R: g_restart.store(true); break;
    case SAPP_KEYCODE_L: g_loop.store(!g_loop.load()); break;
    case SAPP_KEYCODE_LEFT:  seek_by(-5000);  break;
    case SAPP_KEYCODE_RIGHT: seek_by(+5000);  break;
    case SAPP_KEYCODE_DOWN:  seek_by(-30000); break;
    case SAPP_KEYCODE_UP:    seek_by(+30000); break;
    default: break;
    }
    break;
  default: break;
  }
}

static void cleanup_cb(void)
{
  saudio_shutdown();
  sgl_shutdown();
  sg_shutdown();
}

// ---------------------------------------------------------------------------
// load the song, then hand off to sokol-app
// ---------------------------------------------------------------------------

static bool open_song(const void *data, size_t len, int era, const char *label)
{
  v2redux::Result r = era < 0 ? g_player.open(data, len)
                              : g_player.open(data, len, era);
  if (r != v2redux::Result::OK) {
    fprintf(stderr, "%s: open failed (result %d, detected v%d)\n",
            label, (int)r, g_player.fileVersion());
    return false;
  }
  fprintf(stderr, "%s: format v%d\n", label, g_player.fileVersion());
  return true;
}

static bool load_song_file(const char *path, int era)
{
  FILE *f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "cannot open %s\n", path); return false; }
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  void *data = malloc((size_t)len);
  bool ok = data && fread(data, 1, (size_t)len, f) == (size_t)len;
  fclose(f);
  if (!ok) { fprintf(stderr, "short read on %s\n", path); free(data); return false; }
  ok = open_song(data, (size_t)len, era, path);
  free(data);
  return ok;
}

sapp_desc sokol_main(int argc, char *argv[])
{
  const char *path = nullptr;
  int era = -1;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--era") == 0 && i + 1 < argc)
      era = atoi(argv[++i]);
    else if (!path)
      path = argv[i];
  }
  if (path) {
    g_opened = load_song_file(path, era);
  } else {
#ifdef V2PLAY_EMBEDDED
    g_opened = open_song(v2play_song, (size_t)v2play_song_len, era,
                         V2PLAY_EMBED_NAME);
#else
    fprintf(stderr, "usage: v2play <file.v2m> [--era N]\n"
                    "  (or rebuild with -DV2_EMBED_SONG=<file.v2m> to bake one in)\n");
#endif
  }

  // Nothing to play (no file arg, load/open failed, or no embedded song): don't
  // bring up a silent window -- a blank GUI with a frozen playhead reads as a
  // hang. Bail out so the failure is obvious.
  if (!g_opened) {
    fprintf(stderr, "v2play: no song to play -- exiting\n");
    exit(1);
  }

  sapp_desc d = {};
  d.init_cb     = init_cb;
  d.frame_cb    = frame_cb;
  d.cleanup_cb  = cleanup_cb;
  d.event_cb    = event_cb;
  d.width       = 960;
  d.height      = 600;
  d.sample_count = 4;   // 4x MSAA: smooths the scope line + spectrum bar edges
  d.window_title = "v2play";
  d.logger.func = slog_func;
  return d;
}
