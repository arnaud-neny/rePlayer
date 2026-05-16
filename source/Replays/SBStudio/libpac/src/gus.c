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

/* NOTE: see the file libpac/doc/gus-ramping.txt for information on
   how the GUS does hardware volume ramping.  I'm not sure how accurate
   this implementation is. */

#include <assert.h>
#include "pacP.h"

#define GUS_RAMP_MIN 0x3f
#define GUS_RAMP_MAX 0xc1
#define GUS_VOLUME_MAX 4096L
#define FRAC_BITS 24

#define GUS_RATE(n) ((n) >> 6)
#define GUS_DECR(n) ((n) & 0x3fu)

void
pac_start_gus_ramp (struct pac_module *m, struct pac_channel *c)
{
   const int fur_div[] = { 1, 8, 64, 512 };
   double fur, time, step;
   int dist;

   assert (c->arg >= GUS_RAMP_MIN && c->arg <= GUS_RAMP_MAX);

   fur = 1.0 / (1.6 * m->channelcnt) / fur_div[GUS_RATE (c->arg)];
   dist = c->volume * GUS_VOLUME_MAX / PAC_VOLUME_MAX;
   time = dist / (fur * 1000.0) / GUS_DECR (c->arg);
   c->gus_volume = c->volume * (1UL << FRAC_BITS);
   step = c->volume / (time / 1000.0 * pac_rate);
   c->gus_step = step * (1UL << FRAC_BITS);
}

void
pac_stop_gus_ramp (struct pac_channel *c)
{
   c->gus_volume = 0;
   c->gus_step = 0;
}

int
pac_update_gus_ramp (struct pac_channel *c, long n)
{
   long i;

   assert (c->gus_step != 0 && (pac_mode_flags & PAC_GUS_EMULATION));

   for (i = 0; i < n && c->gus_volume != 0; i++) {
      if (c->gus_volume > c->gus_step)
         c->gus_volume -= c->gus_step;
      else
         c->gus_volume = 0;
   }
   c->volume = c->gus_volume >> FRAC_BITS;
   return i;
}
