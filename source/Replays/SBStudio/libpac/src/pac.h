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

#ifndef H_PAC_INCLUDED
#define H_PAC_INCLUDED

#include <limits.h>
#include <stdio.h>

#define PAC_VERSION "0.9.0"

#define PAC_NAME_MAX 40
#define PAC_RATE_MIN 4000L
#define PAC_RATE_MAX 48000L

/* libpac option flags. */
#define PAC_GUS_EMULATION 001
#define PAC_ENDLESS_SONGS 002
#define PAC_INTERPOLATION 004
#define PAC_STRICT_FORMAT 010
#define PAC_NOSOUNDS 020
#define PAC_VOLUME_REDUCTION 040
#define PAC_MODE_DEFAULT (PAC_GUS_EMULATION|\
                          PAC_INTERPOLATION|\
                          PAC_STRICT_FORMAT|\
                          PAC_VOLUME_REDUCTION)

/* libpac error codes. */
enum
{
   PAC_EMIN = INT_MAX / 2,
   PAC_EFORMAT,
   PAC_EALREADY,
   PAC_ENOTINIT,
   PAC_EINVAL,
   PAC_ENOTSUP,
   PAC_EMAX
};

struct pac_module;

int pac_init (long, int, int);
void pac_exit (void);

int pac_test (const char *);
struct pac_module *pac_open (const char *);
void pac_close (struct pac_module *);

long pac_read (struct pac_module *, void *, long);
long pac_seek (struct pac_module *, long, int);
long pac_tell (const struct pac_module *);
int pac_eof (const struct pac_module *);

void pac_enable (int);
int pac_isenabled (int);
void pac_disable (int);

long pac_length (const struct pac_module *);
const char *pac_title (const struct pac_module *);

void pac_perror (const char *);
const char *pac_strerror (int);

#endif /* H_PAC_INCLUDED */
