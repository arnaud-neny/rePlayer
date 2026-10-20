// v2load -- native any-version v2m loading for the portable player.
//
// Faithful reimplementation of the lab's v2mconv.cpp pipeline
// (readfile -> CheckV2MVersion -> ConvertV2M) on top of the transcribed
// format tables in v2load.h, with bounds-checked parsing (the lab trusts its
// inputs; an embeddable loader must reject corrupt data instead of reading
// out of bounds). The upgrade path is byte-replicated from ConvertV2M --
// including its quirks (offsets[0]/4 patch count override, the growing-bound
// mod-destination remap loop, the 4-byte zero tail) -- because task 4.4
// requires byte-equality with conv_v2m output, and fr08 proved that exact
// conversion + era gates = bit-exact audio.

#include "v2load.h"
#include "v2eras.h" // behaviorVersionValid (compiled version range)

#include <string.h>
#include <stdlib.h>
#ifndef NDEBUG
#include <stdio.h>
#endif

namespace v2redux {

namespace {

// little-endian u32 read (the v2m format is little-endian; do not assume
// host endianness or alignment)
inline unsigned readU32(const unsigned char *p)
{
  return (unsigned)p[0] | ((unsigned)p[1] << 8)
       | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

inline void writeU32(unsigned char *p, unsigned v)
{
  p[0] = (unsigned char)(v);
  p[1] = (unsigned char)(v >> 8);
  p[2] = (unsigned char)(v >> 16);
  p[3] = (unsigned char)(v >> 24);
}

// parsed container layout (the lab's _ssbase, with sizes instead of bare
// trusted pointers)
struct Container {
  const unsigned char *start;
  size_t len;

  size_t midiSize;        // header + globals events + 16 channel streams
  struct Chan {
    unsigned noteNum, pcNum, pbNum;
    size_t notePos, pcPos, pbPos;
    struct { unsigned num; size_t pos; } ctl[7];
  } chan[16];

  unsigned globSize;      // byte count of the globals parameter block
  size_t globPos;
  unsigned patchSize;     // byte count of the patch block
  size_t patchPos;
  unsigned spSize;        // speech block (0 if absent)
  size_t spPos;
  bool hasSpeechField;    // whether the file carries the trailing block at all

  int maxp;               // used patch count (pgm-change walk)
};

// bounds-checked cursor
struct Cursor {
  const unsigned char *p;
  size_t pos, len;
  bool ok;

  bool need(size_t n) { if (!ok || len - pos < n) ok = false; return ok; }
  unsigned u32() { if (!need(4)) return 0; unsigned v = readU32(p + pos); pos += 4; return v; }
  bool skip(size_t n) { if (!need(n)) return false; pos += n; return true; }
};

// the lab's readfile(), with every advance bounds-checked
bool parseContainer(const unsigned char *d, size_t len, Container &c)
{
  memset(&c, 0, sizeof(c));
  c.start = d;
  c.len = len;

  Cursor cur = { d, 0, len, true };

  cur.u32();                       // timediv (sequencer consumes it later)
  cur.u32();                       // maxtime
  unsigned gdnum = cur.u32();      // global event count
  if (!cur.ok) return false;
  if (!cur.skip((size_t)10 * gdnum)) return false;

  for (int ch = 0; ch < 16; ch++)
  {
    Container::Chan &cc = c.chan[ch];
    cc.noteNum = cur.u32();
    if (!cur.ok) return false;
    if (!cc.noteNum)
      continue;                    // readfile: empty channel ends after notenum
    cc.notePos = cur.pos;
    if (!cur.skip((size_t)5 * cc.noteNum)) return false;
    cc.pcNum = cur.u32();
    cc.pcPos = cur.pos;
    if (!cur.skip((size_t)4 * cc.pcNum)) return false;
    cc.pbNum = cur.u32();
    cc.pbPos = cur.pos;
    if (!cur.skip((size_t)5 * cc.pbNum)) return false;
    for (int cn = 0; cn < 7; cn++)
    {
      cc.ctl[cn].num = cur.u32();
      cc.ctl[cn].pos = cur.pos;
      if (!cur.skip((size_t)4 * cc.ctl[cn].num)) return false;
    }
  }
  c.midiSize = cur.pos;

  c.globSize = cur.u32();
  if (!cur.ok || c.globSize > 131072) return false;
  c.globPos = cur.pos;
  if (!cur.skip(c.globSize)) return false;

  c.patchSize = cur.u32();
  if (!cur.ok || c.patchSize == 0 || c.patchSize > 1048576) return false;
  c.patchPos = cur.pos;
  if (!cur.skip(c.patchSize)) return false;

  // optional trailing speech block
  if (cur.pos < cur.len)
  {
    c.hasSpeechField = true;
    c.spSize = cur.u32();
    c.spPos = cur.pos;
    if (!cur.ok || c.spSize > 8192 || !cur.skip(c.spSize))
    {
      // the lab zeroes implausible speech sizes instead of failing
      c.spSize = 0;
      c.spPos = 0;
    }
  }
  return true;
}

// the lab's used-patch walk (CheckV2MVersion first half): determine the
// highest used patch from the per-channel program-change streams.
// Event times are stored struct-of-arrays (num lo bytes, num mid, num hi).
int countUsedPatches(const Container &c)
{
  int maxp = 0;
  for (int ch = 0; ch < 16; ch++)
  {
    const Container::Chan &cc = c.chan[ch];
    if (!cc.noteNum)
      continue;

    unsigned char p = 0;
    int pct = 0;
    int np = 0, nt = 0;
    const int nn = (int)cc.noteNum, pcn = (int)cc.pcNum;
    const unsigned char *pcptr = c.start + cc.pcPos;
    const unsigned char *noteptr = c.start + cc.notePos;

    for (int pcp = 0; pcp < pcn; pcp++)
    {
      int td = 0x10000*pcptr[2*pcn+pcp] + 0x100*pcptr[1*pcn+pcp] + pcptr[0*pcn+pcp];
      pct += td;
      if (pct > nt)
      {
        if (p >= maxp) maxp = p + 1;
      }
      p += pcptr[3*pcn+pcp];

      while (np < nn && nt <= pct)
      {
        td = 0x10000*noteptr[2*nn+np] + 0x100*noteptr[1*nn+np] + noteptr[0*nn+np];
        nt += td;
        np++;
      }
    }

    if (np < nn)
    {
      if (p >= maxp) maxp = p + 1;
    }
  }
  return maxp;
}

// CheckV2MVersion second half: match (globSize, per-patch size consistency)
// against the format tables. Returns the detected version or -1.
int detectVersion(const Container &c)
{
  const unsigned char *patchmap = c.start + c.patchPos;
  int best = -1, matches = 0;

  for (int v = 0; v <= kMaxFormatVer; v++)
  {
    if ((int)c.globSize != globalSize(v))
      continue;

    const int parmCount = patchParmCount(v); // == v2vsizes[v]-3*255-1
    int p;
    for (p = 0; p < c.maxp - 1; p++)
    {
      // offsets table at the head of the patch block
      if ((size_t)(p + 2) * 4 > c.patchSize) { p = -1; break; }
      unsigned o0 = readU32(patchmap + 4*p);
      unsigned o1 = readU32(patchmap + 4*(p+1));
      if (o1 <= o0 || o1 > c.patchSize) { p = -1; break; }
      int d = (int)(o1 - o0) - (parmCount + 1);
      if (d < 0 || d % 3) break;
      d /= 3;
      if ((size_t)o0 + parmCount >= c.patchSize) { p = -1; break; }
      if (d != patchmap[o0 + parmCount]) // modnum byte after the params
        break;
    }
    if (p == c.maxp - 1)
    {
      best = v;
      matches++;
    }
  }
  return (matches >= 1) ? best : -1;
}

} // namespace

V2LoadResult v2loadCanonicalize(const void *v2m, size_t length)
{
  V2LoadResult r = { Result::BadFile, -1, nullptr, 0 };
  if (!v2m || length < 16 || length > (size_t)1 << 30)
    return r;

  const unsigned char *in = (const unsigned char *)v2m;

  Container c;
  if (!parseContainer(in, length, c))
    return r;

  c.maxp = countUsedPatches(c);
  if (!c.maxp)
    return r; // "contains no patch data (forgot program changes?)"

  int ver = detectVersion(c);
  if (ver < 0)
    return r; // no structural fingerprint match: not a known v2m layout

  r.version = ver;
  if (!behaviorVersionValid(ver))
  {
    r.result = Result::UnsupportedVersion; // outside the compiled range
    return r;
  }

  if (ver == kMaxFormatVer)
  {
    // canonical form already: identity copy (+ the lab's 4 zero tail bytes)
    unsigned char *out = (unsigned char *)malloc(length + 4);
    if (!out) return r;
    memcpy(out, in, length);
    memset(out + length, 0, 4);
    r.result = Result::OK;
    r.data = out;
    r.size = length + 4;
    return r;
  }

  // ---- upgrade path: ConvertV2M, byte-replicated ---------------------------
  const int vdelta = ver; // the lab's "vdelta" after its double inversion

  const unsigned char *patchmap = in + c.patchPos;

  // ConvertV2M overrides the walked patch count with offsets[0]/4 (the
  // offset table size implies the patch count)
  int maxp = (int)(readU32(patchmap) / 4);
  if (maxp <= 0 || (size_t)maxp * 4 > c.patchSize)
    return r;

  const int gdiff = globalSize(kMaxFormatVer) - globalSize(vdelta);
  const int pdiff = patchSize(kMaxFormatVer) - patchSize(vdelta);
  const size_t newsize = length + gdiff + (size_t)maxp * pdiff;

  unsigned char *out = (unsigned char *)malloc(newsize + 4);
  if (!out) return r;
  memset(out, 0, newsize + 4);
  unsigned char *w = out;
  unsigned char *const wend = out + newsize + 4;

  // sequencer/MIDI part is version-invariant: copy verbatim
  memcpy(w, in, c.midiSize);
  w += c.midiSize;

  // globals: new size prefix, canonical defaults, overwrite present params
  writeU32(w, kNumGlobalParms);
  w += 4;
  memcpy(w, kGlobalDefaults, kNumGlobalParms);
  {
    const unsigned char *oldg = in + c.globPos;
    const unsigned char *oldgEnd = oldg + c.globSize;
    for (int i = 0; i < kNumGlobalParms; i++)
      if (kGlobalParmVer[i] <= vdelta)
      {
        if (oldg >= oldgEnd) { free(out); return r; }
        w[i] = *oldg++;
      }
  }
  w += kNumGlobalParms;

  // patch block: new size prefix, remapped offset table, upgraded patches
  writeU32(w, c.patchSize + (unsigned)(maxp * pdiff));
  w += 4;
  unsigned char *const npm = w; // new patchmap base (offsets are relative to it)

  for (int p = 0; p < maxp; p++)
    writeU32(npm + 4*p, readU32(patchmap + 4*p) + (unsigned)(p * pdiff));
  w += 4 * maxp;

  const unsigned char *const inEnd = in + length;
  for (int p = 0; p < maxp; p++)
  {
    unsigned srcOff = readU32(patchmap + 4*p);
    if ((size_t)srcOff >= c.patchSize) { free(out); return r; }
    const unsigned char *src = patchmap + srcOff;

    // defaults, then overwrite the params the file carries (in table order)
    if (w + kNumPatchParms > wend) { free(out); return r; }
    memcpy(w, kPatchDefaults, kNumPatchParms);
    for (int i = 0; i < kNumPatchParms; i++)
    {
      if (kPatchParmVer[i] <= vdelta)
      {
        if (src >= inEnd) { free(out); return r; }
        *w = *src++;
      }
      w++;
    }

    // mod list: count, then 3 bytes per mod with the destination index
    // remapped to the v6 param order. The remap loop is replicated verbatim
    // from ConvertV2M (its bound re-reads the growing value).
    if (src >= inEnd || w >= wend) { free(out); return r; }
    const int modnum = *w++ = *src++;
    for (int i = 0; i < modnum; i++)
    {
      if (src + 3 > inEnd || w + 3 > wend) { free(out); return r; }
      w[0] = src[0];
      w[1] = src[1];
      w[2] = src[2];
      unsigned char raw_dest = src[2];
      for (int k = 0; k <= w[2]; k++)
        if (k < kNumPatchParms && kPatchParmVer[k] > vdelta) w[2]++;
#ifndef NDEBUG
      if (getenv("V2_MODREMAP"))
        fprintf(stderr, "[modremap] ver=%d src=%d val=%d raw_dest=%d -> v6_dest=%d\n",
                ver, w[0], w[1], raw_dest, w[2]);
#endif
      w += 3;
      src += 3;
    }
  }

  // speech block (length always written, like ConvertV2M)
  if (w + 4 + c.spSize > wend) { free(out); return r; }
  writeU32(w, c.spSize);
  w += 4;
  if (c.spSize)
    memcpy(w, in + c.spPos, c.spSize);
  w += c.spSize;

  r.result = Result::OK;
  r.data = out;
  r.size = newsize + 4;
  return r;
}

} // namespace v2redux
