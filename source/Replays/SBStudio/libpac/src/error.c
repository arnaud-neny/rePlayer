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

#include <errno.h>
#include <string.h>
#include "pacP.h"

#define ERROR_COUNT (PAC_EMAX - PAC_EMIN)

static const struct { int n; const char *s; } error_array[ERROR_COUNT+1] =
{
   { PAC_EALREADY, "Operation already performed" },
   { PAC_ENOTINIT, "Library not initialized" },
   { PAC_EFORMAT, "Invalid format" },
   { PAC_EINVAL, "Invalid argument" },
   { PAC_ENOTSUP, "Not supported" }
};

const char *
pac_strerror (int n)
{
   size_t i;

   if (n > PAC_EMIN && n < PAC_EMAX)
      for (i = 0; i < ERROR_COUNT; i++)
         if (error_array[i].n == n)
            return error_array[i].s;
   return strerror (n);
}

void
pac_perror (const char *s)
{
   if (s != NULL && *s != '\0')
      fprintf (stderr, "%s: ", s);
   fprintf (stderr, "%s\n", pac_strerror (errno));
}
