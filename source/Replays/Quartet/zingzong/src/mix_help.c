/**
 * @file   mix_help.c
 * @author Benjamin Gerard AKA Ben/OVR
 * @date   2017-07-14
 * @brief  Mixer help functions.
 */

#define ZZ_DBG_PREFIX "(flt) "
#include "zz_private.h"

/* clipped int16_t */
static inline int16_t
i16_clip(i32_t v)
{
  if ( unlikely(v < -32768 )) {
    dmsg("i16: %i\n", (int) v);
    v = -32768;
  } else if ( unlikely(v > 32767) ) {
    dmsg("i16: %i\n", (int) v);
    v = 32767;
  }
  return v;
}

int16_t zz_i16_clip(i32_t v)
{
  return i16_clip(v);
}

#ifndef NO_FLOAT

#include <math.h>

/* int8_t to float sample conversion table */
static float i8tofl_lut[256];

/* Currently let the compiler do its work. */
static inline i32_t
fast_ftoi(const float f)
{
  return (i32_t) f;
}

i32_t zz_ftoi(const float f)
{
  return fast_ftoi(f);
}

/* float to clipped int16_t */
static inline int16_t
flt_to_i16(const float f)
{
  return i16_clip(fast_ftoi(f));
}

/* Convert int8_t PCM buffer to float PCM */
void
i8tofl(float * restrict d, const uint8_t * restrict s, int n)
{
  static int init = 0;

  if (unlikely(!init)) {
    const float sc = 1.0 / 128.0;
    int i; float * lut = i8tofl_lut;
    init = 1;
    for ( i = -128; i < 128; ++i ) {
      *lut++ = sc * (float)i;
      zz_assert( lut[-1] >= -1.0 );
      zz_assert( lut[-1] <   1.0 );
    }
  }
  if (n > 0) {
    int i = 0;
    do {
      d[i] = i8tofl_lut[s[i]];
    } while (++i<n);
  }
}

/* Convert normalized float PCM buffer to in16_t PCM */
void
fltoi16(int16_t * restrict d, const float * restrict s, int n)
{
  const float sc = 32768.0; int i;
  for (i=0; i<n; ++i)
    d[i] = i16_clip(s[i] * sc);
}

void
map_flt_to_i16(int16_t * restrict d,
               const float * restrict va, const float * restrict vb,
               const float * restrict vc, const float * restrict vd,
               const float sc1, const float sc2, const int n)
{
  int i;
  for ( i=0; i<n; ++i ) {
    const float ab = *va ++ + *vb ++;
    const float cd = *vc ++ + *vd ++;

    *d++ = flt_to_i16( ab * sc1 + cd * sc2 );
    *d++ = flt_to_i16( ab * sc2 + cd * sc1 );
  }
}

#endif /* #ifndef NO_FLOAT */

void
map_i16_to_i16(int16_t * restrict d,
               const i16_t * restrict va, const i16_t * restrict vb,
               const i16_t * restrict vc, const i16_t * restrict vd,
               const i16_t sc1, const i16_t sc2, const int n)
{
  int i;
  for ( i=0; i<n; ++i ) {
    const i32_t ab = (i32_t) *va ++ + (i32_t) *vb ++;
    const i32_t cd = (i32_t) *vc ++ + (i32_t) *vd ++;

    *d++ = i16_clip( (ab * sc1 + cd * sc2) >> 9 );
    *d++ = i16_clip( (ab * sc2 + cd * sc1) >> 9 );
  }
}
