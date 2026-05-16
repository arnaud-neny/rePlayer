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
#include <errno.h>
#include "pacP.h"

long
pac_seek (struct pac_module *m, long offset, int whence)
{
   long tick_max;

   if (m->length == 0) {
      errno = PAC_ENOTSUP;
      return -1;
   }

   switch (whence) {
   case SEEK_SET:
      break;
   case SEEK_CUR:
      if (offset > 0)
         if ((unsigned long) m->time + offset > (unsigned long) m->length) {
            errno = PAC_EINVAL;
            return -1;
         }
      offset += m->time;
      break;
   case SEEK_END:
      if (offset > 0) {
         errno = PAC_EINVAL;
         return -1;
      }
      offset += m->length;
      break;
   default:
      errno = PAC_EINVAL;
      return -1;
   }

   if (offset < 0 || offset > m->length) {
      errno = PAC_EINVAL;
      return -1;
   }

   if (offset == 0) {
      pac_reset (m);
      return 0;
   }

   if (offset < m->time)
      pac_reset (m);
   else
      offset -= m->time;

   tick_max = pac_ticksize (m->tempo_min);
   if ((offset - tick_max) >= tick_max) {
      m->seeking = 1;
      pac_read (m, NULL, (offset - tick_max) * pac_framesize);
      m->seeking = 0;
      offset = tick_max;
   }
   pac_read (m, NULL, offset * pac_framesize);
   return m->time;
}

long
pac_tell (const struct pac_module *m)
{
   return m->time;
}
