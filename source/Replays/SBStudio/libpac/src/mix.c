/*
 * Copyright (c) 2005 Thomas Pfaff <thomaspfaff@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include "pacP.h"

#define FRAC_BITS 15L /* NOTE: FRAC_BITS > 15 may overflow! */
#define FRAC_MASK ~(~0L << FRAC_BITS)

#define NTSC 7159090.5f
#define PAL 7093789.2f
#define PERIOD_DIV NTSC

#define is_looping(s) (((s)->loopend - (s)->loopstart) > 1)

static long calc_rate (struct pac_module *, struct pac_channel *, long);
static void calc_volume (const struct pac_channel *, int, int *, int *);
static void skip_tick (struct pac_module *, struct pac_channel *, long);

void
pac_mix_tick (struct pac_module *m)
{
   struct pac_sound *s;
   struct pac_channel *c;
   const long *const e = &m->tickbuf[m->ticksize * pac_channels];
   const signed char *s8;
   const short *s16;
   long *d, sn, length;
   int lv, rv;
   int istep;
   long fstep;

   assert (m->tickpos == 0);

   /* Clear the tick (mixing) buffer. */
   if (!m->seeking)
      for (d = m->tickbuf; d < e; d++)
         *d = 0;

   /* Mix at most one tick from all playing channels. */
   for (c = m->channel; c < &m->channel[m->channelcnt]; c++) {
      if (c->playing && (s = &m->sound[m->soundmap[c->instr]])->sample) {
         const long rate = calc_rate (m, c, s->middlec);

         if (m->seeking || c->volume == 0 || m->volume == 0) {
            skip_tick (m, c, rate);
            continue;
         }

         istep = rate / pac_rate;
         fstep = ((rate % pac_rate) << FRAC_BITS) / pac_rate;

         calc_volume (c, m->volume, &lv, &rv);

         if (s->bits == 16)
            s16 = s->sample, s8 = NULL;
         else
            s8 = s->sample, s16 = NULL;

         /* Mix at most one tick to m->tickbuf. */
         length = is_looping (s) ? s->loopend : s->length;
         for (d = m->tickbuf; d < e; ) {
            if (c->offset >= length) {
               if (!is_looping (s)) {
                  c->playing = 0;
                  break;
               }
               c->offset = s->loopstart;
               c->frac = 0;
            }

            if (c->gus_step != 0)
               if (pac_update_gus_ramp (c, 1))
                  calc_volume (c, m->volume, &lv, &rv);

            if (s16 != NULL) {
               sn = s16[c->offset];
               if (pac_mode_flags & PAC_INTERPOLATION) {
                  long sd = s16[c->offset+1] - sn;
                  sn += (sd * c->frac) >> FRAC_BITS;
               }
            }
            else {
               sn = s8[c->offset] * 256;
               if (pac_mode_flags & PAC_INTERPOLATION) {
                  long sd = (s8[c->offset+1] * 256) - sn;
                  sn += (sd * c->frac) >> FRAC_BITS;
               }
            }

            *d++ += sn * lv / PAC_VOLUME_MAX;
            if (pac_channels == 2)
               *d++ += sn * rv / PAC_VOLUME_MAX;

            c->offset += istep;
            c->frac += fstep;
            if (c->frac > FRAC_MASK) {
               c->offset++;
               c->frac &= FRAC_MASK;
            }
         }
      }
   }
}

/* XXX this is not entirely accurate. */
static void
skip_tick (struct pac_module *m, struct pac_channel *c, long rate)
{
   struct pac_sound *s = &m->sound[m->soundmap[c->instr]];
   long length = is_looping (s) ? s->loopend : s->length;
   long step = (rate << FRAC_BITS) / pac_rate;
   long incr = m->ticksize * step;

   c->offset += incr >> FRAC_BITS;
   c->frac += incr - (incr >> FRAC_BITS << FRAC_BITS);
   if (c->frac > FRAC_MASK) {
      c->offset++;
      c->frac &= FRAC_MASK;
   }
   if (c->offset >= length) {
      if (is_looping (s)) {
         long over = c->offset - s->loopstart;
         length = s->loopend - s->loopstart;
         c->offset = s->loopstart + over % length;
         c->frac = (over % length) * step;
         c->frac &= FRAC_MASK;
      }
      else
         c->playing = 0;
   }
   if (c->gus_step != 0)
      pac_update_gus_ramp (c, incr >> FRAC_BITS);
}

static void
calc_volume (const struct pac_channel *c, int m, int *lv, int *rv)
{
   int v;

   v = c->volume + c->dvolume;
   if (v > PAC_VOLUME_MAX)
      v = PAC_VOLUME_MAX;
   else if (v < PAC_VOLUME_MIN)
      v = PAC_VOLUME_MIN;
   v = v * m / PAC_VOLUME_MAX;
   if (pac_mode_flags & PAC_VOLUME_REDUCTION)
      v = v * (100-40) / 100;
   if (pac_channels == 2) {
      *lv = (c->pan > PAC_PAN_CENTER) ?
         v * (PAC_PAN_MAX - c->pan) / PAC_PAN_CENTER : v;
      *rv = (c->pan > PAC_PAN_CENTER) ?
         v : v * c->pan / PAC_PAN_CENTER;
   }
   else
      *lv = *rv = v;
}

static long
calc_rate (struct pac_module *m, struct pac_channel *c, long t)
{
   long p;

   p = c->period + c->dperiod;
   if (p > m->period_max)
      p = m->period_max;
   else if (p < m->period_min)
      p = m->period_min;
   p = PERIOD_DIV / (p * 2);
   if (t != PAC_MIDDLEC_DEFAULT)
      return p * t / PAC_MIDDLEC_DEFAULT;
   return p;
}
