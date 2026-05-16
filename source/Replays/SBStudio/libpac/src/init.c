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
#include <stdio.h>
#include "pacP.h"

const struct pac_channel pac_channel_init =
{
   0,        /* playing */
   64,       /* volume */
   0,        /* dvolume */
   0,        /* gus_volume */
   0,        /* gus_step */
   0,        /* period */
   0,        /* dperiod */
   127,      /* pan */
   127,      /* pan_init */
   0,        /* fx9val */
   0,        /* offset */
   0,        /* frac */
   0,        /* note */
   0,        /* instr */
   0,        /* cmd */
   0,        /* arg */
   0,        /* volslide */
   0,        /* glissando */
   0,        /* porta_speed */
   0,        /* porta_target */
   0,        /* porta_note */
   0,        /* tremor_cnt */
   0,        /* tremor_arg */
   255,      /* sheetloop_row */
   0,        /* sheetloop_cnt */
   0,        /* vib_ctrl */
   0,        /* vib_pos */
   0,        /* vib_speed */
   0,        /* vib_depth */
   0,        /* trem_ctrl */
   0,        /* trem_pos */
   0,        /* trem_speed */
   0,        /* trem_depth */
   0,        /* delay */
   NULL      /* delay_note */
};

const struct pac_sound pac_sound_init =
{
   "",       /* name */
   -1,       /* number */
   8363,     /* middlec */
   0,        /* finetune */
   64,       /* volume */
   0,        /* bits */
   0,        /* loopstart */
   0,        /* loopend */
   0,        /* length */
   NULL      /* sample */
};

const struct pac_note pac_note_init =
{
   0,        /* number */
   0,        /* instr */
   0,        /* volume */
   0,        /* cmd */
   0         /* arg */
};

const struct pac_module pac_module_init =
{
   NULL,     /* next */
   "",       /* name */
   0,        /* fversion */
   0,        /* tversion */
   NULL,     /* sound */
   {0},      /* soundmap */
   0,        /* soundcnt */
   NULL,     /* channel */
   0,        /* channelcnt */
   {{NULL}}, /* sheet */
   0,        /* sheetcnt */
   {0},      /* postbl */
   0,        /* poscnt */
   64,       /* volume */
   0,        /* seeking */
   0,        /* speed */
   0,        /* speed_init */
   0,        /* tempo */
   0,        /* tempo_init */
   0,        /* tempo_min */
   0,        /* row */
   0,        /* nextrow */
   0,        /* pos */
   0,        /* nextpos */
   0,        /* tick */
   0,        /* tickpos */
   NULL,     /* tickbuf */
   0,        /* ticksize */
   0,        /* posjmp */
   0,        /* sheetbrk */
   0,        /* eof */
   0,        /* note_max */
   0,        /* period_min */
   0,        /* period_max */
   0,        /* sheetdelay */
   0,        /* length */
   0         /* time */
};

int pac_framesize;
long pac_rate;
int pac_bits;
int pac_channels;
int pac_initialized;
int pac_mode_flags;

int
pac_init (long rate, int bits, int channels)
{
   if (pac_initialized) {
      errno = PAC_EALREADY;
      return -1;
   }

   if (rate < PAC_RATE_MIN || rate > PAC_RATE_MAX) {
      errno = PAC_EINVAL;
      return -1;
   }

   if (bits != 8 && bits != 16 && bits != 32) {
      errno = PAC_EINVAL;
      return -1;
   }

   if (channels != 1 && channels != 2) {
      errno = PAC_EINVAL;
      return -1;
   }

   pac_bits = bits;
   pac_rate = rate;
   pac_channels = channels;
   pac_framesize = (bits == 32 ? sizeof (long) : bits == 16 ?
      sizeof (short) : 1) * channels;
   pac_mode_flags = PAC_MODE_DEFAULT;
   if (bits == 32)
      pac_mode_flags &= ~PAC_VOLUME_REDUCTION;
   pac_initialized = 1;

#ifndef NDEBUG
   fprintf (stderr, "pac_init: %ld Hz, %d bits, %d channels (framesize %d)\n",
            pac_rate, pac_bits, pac_channels, pac_framesize);
#endif
   return 0;
}

void
pac_exit (void)
{
   extern struct pac_module *pac_module_list;

   while (pac_module_list != NULL)
     pac_close (pac_module_list);
   pac_initialized = 0;
}

void
pac_reset (struct pac_module *m)
{
   int i;

   for (i = 0; i < m->channelcnt; i++) {
      int p = m->channel[i].pan_init;
      m->channel[i] = pac_channel_init;
      m->channel[i].pan_init = p;
      m->channel[i].pan = p;
   }
   m->volume = PAC_VOLUME_MAX;
   m->seeking = 0;
   m->row = 0;
   m->nextrow = 0;
   m->pos = 0;
   m->nextpos = 0;
   m->speed = m->speed_init;
   m->tempo = m->tempo_init;
   m->sheetdelay = 0;
   m->tick = m->speed - 1;
   m->ticksize = pac_ticksize (m->tempo);
   m->tickpos = m->ticksize;
   m->sheetbrk = 0;
   m->posjmp = 0;
   m->time = 0;
   m->eof = 0;
}

void
pac_enable (int f)
{
   pac_mode_flags |= f;
}

int
pac_isenabled (int f)
{
   return pac_mode_flags & f;
}

void
pac_disable (int f)
{
   pac_mode_flags &= ~f;
}
