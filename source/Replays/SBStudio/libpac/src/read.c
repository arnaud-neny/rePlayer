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

static void *copy8 (void *, long *, long);
static void *copy16 (void *, long *, long);
static void *copy32 (void *, long *, long);

long
pac_read (struct pac_module *m, void *b, long n)
{
   long t, total = 0;
   void *(*copy) (void *, long *, long);

   if ((n /= pac_framesize) < 1 || m->eof)
      return 0;

   copy = (pac_bits == 32) ? copy32 : (pac_bits == 16) ? copy16 : copy8;

   /* Use buffered sample data from previous call. */
   if (m->tickpos < m->ticksize) {
      if (n < (t = m->ticksize - m->tickpos))
         t = n;
      if (b != NULL)
         b = copy (b, m->tickbuf + m->tickpos * pac_channels, t);
      m->tickpos += t;
      n -= t;
      total += t;
   }

   /* Generate more sample data, one tick at a time. */
   while (n > 0 && !m->eof) {
      assert (m->tickpos == m->ticksize && m->pos < m->poscnt);
      m->tickpos = 0;
      if (++m->tick == m->speed * (1 + m->sheetdelay)) {
         m->tick = 0;
         m->sheetdelay = 0;
         m->row = m->nextrow++;
         if (m->row == PAC_ROW_MAX) {
            m->row = 0;
            m->nextrow = 1;
            m->pos = ++m->nextpos;
            if (m->pos >= m->poscnt) {
               if (pac_mode_flags & PAC_ENDLESS_SONGS) {
                  m->nextpos = 0;
                  m->time = 0;
               }
               else {
                  m->eof = 1;
                  break;
               }
            }
         }
         m->pos = m->nextpos;
         pac_update_channels (m);
      }
      pac_update_effects (m);

      /* Don't mix if calculating length (load.c). */
      if (!(m->seeking && m->length == 0))
         pac_mix_tick (m);

      t = (n > m->ticksize) ? m->ticksize : n;
      if (b != NULL)
         b = copy (b, m->tickbuf, t);
      m->tickpos += t;
      n -= t;
      total += t;
   }
   m->time += total;
   return total * pac_framesize;
}

long
pac_length (const struct pac_module *m)
{
   return m->length;
}

int
pac_eof (const struct pac_module *m)
{
   return m->eof;
}

static void *
copy8 (void *b, long *s, long n)
{
   signed char *d;
   const long *e;

   assert (b != NULL && s != NULL && n > 0);

   for (d = b, e = s + n * pac_channels; s < e; s++) {
      if (*s > 32767)
         *s = 32767;
      else if (*s < -32767)
         *s = -32767;
      *d++ = *s / 256;
   }
   return d;
}

static void *
copy16 (void *b, long *s, long n)
{
   short *d;
   const long *e;

   assert (b != NULL && s != NULL && n > 0);

   for (d = b, e = s + n * pac_channels; s < e; s++) {
      if (*s > 32767)
         *s = 32767;
      else if (*s < -32767)
         *s = -32767;
      *d++ = *s;
   }
   return d;
}

static void *
copy32 (void *b, long *s, long n)
{
   long *d;
   const long *e;

   assert (b != NULL && s != NULL && n > 0);

   for (d = b, e = s + n * pac_channels; s < e; s++)
      *d++ = *s;
   return d;
}
