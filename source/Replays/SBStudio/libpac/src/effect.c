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
#include <math.h>
#include "pacP.h"

#ifndef M_2PI
#define M_2PI 6.283185f
#endif

#define xarg(n) ((n) >> 4)
#define yarg(n) ((n) & 0xf)
#define isporta(c) ((c)->cmd == FX_PORTA || (c)->cmd == FX_PORTAVOLSLIDE)
#define isdelay(c) ((c)->cmd == FX_EXTENDED && \
                    xarg ((c)->arg) == EFX_DELAYNOTE && \
                    yarg ((c)->arg) > 0)
#define fx_clear(c) ((c)->cmd = (c)->arg = 0)

enum
{
   WAVE_SINE,
   WAVE_RAMPDOWN,
   WAVE_SQUARE
};

static void nop (struct pac_module *, struct pac_channel *);
static int note_to_period (const struct pac_module *, int, int);
static void trigger_note (struct pac_module *, struct pac_channel *,
   const struct pac_note *);

/*
 * Standard effects.
 */

enum
{
   FX_ARPEGGIO,
   FX_SLIDEUP,
   FX_SLIDEDOWN,
   FX_PORTA,
   FX_VIBRATO,
   FX_PORTAVOLSLIDE,
   FX_VIBRATOVOLSLIDE,
   FX_TREMOLO,
   FX_SETPAN,
   FX_SETOFFSET,
   FX_VOLSLIDE,
   FX_POSJUMP,
   FX_NOTEOFF,
   FX_SHEETBREAK,
   FX_EXTENDED,
   FX_SETSPEED,
   FX_TREMOR,
   FX_GVOLUME = 0x15,
   FX_GVOLSLIDE
};

static void fx_arpeggio (struct pac_module *, struct pac_channel *);
static void fx_slideup (struct pac_module *, struct pac_channel *);
static void fx_slidedown (struct pac_module *, struct pac_channel *);
static void fx_porta (struct pac_module *, struct pac_channel *);
static void fx_vibrato (struct pac_module *, struct pac_channel *);
static void fx_portavolslide (struct pac_module *, struct pac_channel *);
static void fx_vibratovolslide (struct pac_module *, struct pac_channel *);
static void fx_tremolo (struct pac_module *, struct pac_channel *);
static void fx_setpan (struct pac_module *, struct pac_channel *);
static void fx_setoffset (struct pac_module *, struct pac_channel *);
static void fx_volslide (struct pac_module *, struct pac_channel *);
static void fx_posjump (struct pac_module *, struct pac_channel *);
static void fx_noteoff (struct pac_module *, struct pac_channel *);
static void fx_sheetbreak (struct pac_module *, struct pac_channel *);
static void fx_extended (struct pac_module *, struct pac_channel *);
static void fx_setspeed (struct pac_module *, struct pac_channel *);
static void fx_tremor (struct pac_module *, struct pac_channel *);
static void fx_gvolume (struct pac_module *, struct pac_channel *);
static void fx_gvolslide (struct pac_module *, struct pac_channel *);

static void (* const fx[]) (struct pac_module *, struct pac_channel *) =
{
   fx_arpeggio,          /* 00xy - Arpeggio. */
   fx_slideup,           /* 01xy - Frequency slide up. */
   fx_slidedown,         /* 02xy - Frequency slide down. */
   fx_porta,             /* 03xy - Tone portamento. */
   fx_vibrato,           /* 04xy - Vibrato. */
   fx_portavolslide,     /* 05xy - Volume slide, continue portamento. */
   fx_vibratovolslide,   /* 06xy - Volume slide, continue vibrato. */
   fx_tremolo,           /* 07xy - Tremolo. */
   fx_setpan,            /* 08xy - Pan control. */
   fx_setoffset,         /* 09xy - Set sample offset. */
   fx_volslide,          /* 0Axy - Volume slide. */
   fx_posjump,           /* 0Bxy - Jump to position. */
   fx_noteoff,           /* 0Cxy - Note off (set volume in fversion 1.6). */
   fx_sheetbreak,        /* 0Dxy - Sheet break. */
   fx_extended,          /* 0Exy - Extended (see below). */
   fx_setspeed,          /* 0Fxy - Song speed. */
   fx_tremor,            /* 10xy - Tremor. */
   nop,                  /* 11xy - Reverb (AWE32/64). */
   nop,                  /* 12xy - Chorus (AWE32/64). */
   nop,                  /* 13xy - Filter (AWE32/64). */
   nop,                  /* 14xy - Resonance (AWE32/64). */
   fx_gvolume,           /* 15xy - Global volume. */
   fx_gvolslide,         /* 16xy - Global volume slide. */
   nop,                  /* 17xy - Modulate filter (AWE32/64). */
   nop,                  /* 18xy - UNUSED. */
   nop,                  /* 19xy - UNUSED. */
   nop,                  /* 1Axy - UNUSED. */
   nop,                  /* 1Bxy - UNUSED. */
   nop,                  /* 1Cxy - UNUSED. */
   nop,                  /* 1Dxy - UNUSED. */
   nop,                  /* 1Exy - UNUSED. */
   nop,                  /* 1Fxy - UNUSED. */
   nop,                  /* 20xy - Set MIDI OUT channel. */
   nop,                  /* 21xy - MIDI program change. */
   nop                   /* 22xy - MIDI pressure. */
};

#define FX_COUNT ((int) (sizeof fx / sizeof *fx))

/*
 * Extended effects.
 */

enum
{
   EFX_FREQTRIMUP = 0x01,
   EFX_FREQTRIMDOWN,
   EFX_GLISSANDO,
   EFX_VIBRATOWAVE,
   EFX_FINETUNE,
   EFX_SHEETLOOP,
   EFX_TREMOLOWAVE,
   EFX_SETPAN,
   EFX_RETRIGNOTE,
   EFX_VOLTRIMUP,
   EFX_VOLTRIMDOWN,
   EFX_CUTNOTE,
   EFX_DELAYNOTE,
   EFX_SHEETDELAY
};

static void efx_freqtrimup (struct pac_module *, struct pac_channel *);
static void efx_freqtrimdown (struct pac_module *, struct pac_channel *);
static void efx_glissando (struct pac_module *, struct pac_channel *);
static void efx_vibratowave (struct pac_module *, struct pac_channel *);
static void efx_finetune (struct pac_module *, struct pac_channel *);
static void efx_sheetloop (struct pac_module *, struct pac_channel *);
static void efx_tremolowave (struct pac_module *, struct pac_channel *);
static void efx_setpan (struct pac_module *, struct pac_channel *);
static void efx_retrignote (struct pac_module *, struct pac_channel *);
static void efx_voltrimup (struct pac_module *, struct pac_channel *);
static void efx_voltrimdown (struct pac_module *, struct pac_channel *);
static void efx_cutnote (struct pac_module *, struct pac_channel *);
static void efx_delaynote (struct pac_module *, struct pac_channel *);
static void efx_sheetdelay (struct pac_module *, struct pac_channel *);

static void (* const efx[]) (struct pac_module *, struct pac_channel *) =
{
   nop,                  /* 0E0y - UNUSED. */
   efx_freqtrimup,       /* 0E1y - Frequency trim up. */
   efx_freqtrimdown,     /* 0E2y - Frequency trim down. */
   efx_glissando,        /* 0E3y - Glissando control. */
   efx_vibratowave,      /* 0E4y - Set vibrato waveform. */
   efx_finetune,         /* 0E5y - Set finetune. */
   efx_sheetloop,        /* 0E6y - Sheet loop. */
   efx_tremolowave,      /* 0E7y - Set tremolo waveform. */
   efx_setpan,           /* 0E8y - Pan control. */
   efx_retrignote,       /* 0E9y - Retrig note. */
   efx_voltrimup,        /* 0EAy - Volume trim up. */
   efx_voltrimdown,      /* 0EBy - Volume trim down. */
   efx_cutnote,          /* 0ECy - Note cut. */
   efx_delaynote,        /* 0EDy - Note delay. */
   efx_sheetdelay,       /* 0EEy - Sheet delay. */
   nop                   /* 0EFy - UNUSED. */
};

#define EFX_COUNT ((int) (sizeof efx / sizeof *efx))

static void
trigger_note (struct pac_module *m, struct pac_channel *c,
              const struct pac_note *n)
{
   const struct pac_sound *s;

   pac_stop_gus_ramp (c);
   c->vib_pos = 0;
   c->trem_pos = 0;
   c->note = n->number;
   s = &m->sound[m->soundmap[c->instr]];
   c->period = note_to_period (m, c->note, s->tune);
   c->offset = 0;
   c->frac = 0;
   c->playing = 1;
}

/* Update all channels for the new row.  This function is called by pac_read
   in read.c at the beginning of every row (at tick 0). */
void
pac_update_channels (struct pac_module *m)
{
   const struct pac_note *n;
   struct pac_channel *c;

   assert (m->tick == 0 && m->tickpos == 0 && m->pos < m->poscnt);

   m->sheetbrk = 0;
   m->posjmp = 0;
   m->sheetdelay = 0;
   n = m->sheet[m->postbl[m->pos]].note + m->row * m->channelcnt;
   for (c = m->channel; c < &m->channel[m->channelcnt]; c++, n++) {
      c->dperiod = 0;
      c->dvolume = 0;
      c->cmd = n->cmd;
      c->arg = n->arg;
      if (!isdelay (n)) {
         if (n->instr > 0) {
            pac_stop_gus_ramp (c);
            c->instr = n->instr;
            c->volume = m->sound[m->soundmap[c->instr]].volume;
         }
         if (n->number > 0 && n->number != PAC_NOTE_OFF) {
            if (isporta (n)) {
               c->porta_note = n->number;
               c->porta_period = c->period;
            }
            else {
               trigger_note (m, c, n);
               c->delay = 0;
               c->delay_note = NULL;
            }
         }
      }
      if (n->volume > 0) {
         pac_stop_gus_ramp (c);
         c->volume = n->volume - 1;
      }
      if (n->number == PAC_NOTE_OFF) {
         assert (m->fversion == PAC_FORMAT_16);
         c->volume = PAC_VOLUME_MIN;
      }
   }
}

/* Update effects on all channels.  This function is called by pac_read in
   read.c every tick (including tick 0 after pac_update_channels). */
void
pac_update_effects (struct pac_module *m)
{
   struct pac_channel *c;

   assert (m->tick < m->speed * (1 + m->sheetdelay) && m->tickpos == 0);

   for (c = m->channel; c < &m->channel[m->channelcnt]; c++) {
      if ((c->cmd > 0 || c->arg > 0) && c->cmd < FX_COUNT)
         fx[c->cmd] (m, c);

      if (c->delay > 0 && --c->delay == 0) {
         const struct pac_note *n;
         trigger_note (m, c, c->delay_note);
         n = c->delay_note;
         if (n->instr > 0) {
            c->instr = n->instr;
            c->volume = m->sound[m->soundmap[c->instr]].volume;
         }
         if (n->volume > 0)
            c->volume = n->volume - 1;
      }
   }
}

/* This function converts a note number and a finetune value to a period
   value used to determine the playback rate of an instrument. */
static int
note_to_period (const struct pac_module *m, int note, int tune)
{
   assert (note >= PAC_NOTE_MIN && note <= m->note_max);
   assert (tune >= PAC_TUNE_MIN && tune <= PAC_TUNE_MAX);

   return m->period_max * pow (2, (-8 * note + 8 - tune) / 96.0) + .5;
}

static void
nop (struct pac_module *m, struct pac_channel *c)
{
#ifndef NDEBUG
   fprintf (stderr, "effect.c: nop: effect `%.2X%.2X'\n", c->cmd, c->arg);
#endif
}

/*
 * Standard effects.
 */

/* 00xy: Arpeggio.  Play note, play note + <x>, play note + <y>, repeat ... */
static void
fx_arpeggio (struct pac_module *m, struct pac_channel *c)
{
   int note, tune;

   assert (c->cmd == FX_ARPEGGIO && c->arg > 0);

   if (m->tick > 0 && c->note > 0) {
      switch (m->tick % 3) {
      case 1:
         note = c->note + xarg (c->arg);
         if (note > m->note_max)
            note = m->note_max;
         break;
      case 2:
         note = c->note + yarg (c->arg);
         if (note > m->note_max)
            note = m->note_max;
         break;
      default:
         note = c->note;
         break;
      }
      tune = m->sound[m->soundmap[c->instr]].tune;
      c->dperiod = note_to_period (m, note, tune) - c->period;
   }
}

/* 01xy: Frequency slide up by <xy>. */
static void
fx_slideup (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_SLIDEUP);

   if (m->tick > 0) {
      c->period -= c->arg;
      if (c->period < m->period_min)
         c->period = m->period_min;
   }
}

/* 02xy: Frequency slide down by <xy>. */
static void
fx_slidedown (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_SLIDEDOWN);

   if (m->tick > 0) {
      c->period += c->arg;
      if (c->period > m->period_max)
         c->period = m->period_max;
   }
}

/* 03xy: Tone portamento.  Slide toward note in <xy> units per tick. */
static void
fx_porta (struct pac_module *m, struct pac_channel *c)
{
#define PREV_NOTE 0.943874f /* pow (2, -1 / 12.0) */
#define NEXT_NOTE 1.059463f /* pow (2,  1 / 12.0) */

   assert (c->cmd == FX_PORTA || c->cmd == FX_PORTAVOLSLIDE);

   if (m->tick == 0) {
      if (c->arg > 0)
         c->porta_speed = c->arg;
   }
   else if (c->note && c->instr && c->porta_speed && c->porta_note) {
      int target = note_to_period (m, c->porta_note,
         m->sound[m->soundmap[c->instr]].tune);
      if (c->porta_period > target) {
         c->porta_period -= c->porta_speed;
         if (c->porta_period < target)
            c->porta_period = target;
      }
      else {
         c->porta_period += c->porta_speed;
         if (c->porta_period > target)
            c->porta_period = target;
      }
      if (c->glissando) {
         if (c->period > target) {
            if (c->porta_period <= (int) (c->period * PREV_NOTE + .5f))
               c->period = c->porta_period;
         }
         else if (c->porta_period >= (int) (c->period * NEXT_NOTE + .5f))
            c->period = c->porta_period;
      }
      else
         c->period = c->porta_period;
      if (c->porta_period == target)
         fx_clear (c);
   }
}

/* 04xy: Vibrato with speed <x> and depth <y>. */
static void
fx_vibrato (struct pac_module *m, struct pac_channel *c)
{
   int d;

   assert (c->cmd == FX_VIBRATO || c->cmd == FX_VIBRATOVOLSLIDE);

   if (m->tick == 0) {
      if (xarg (c->arg) != 0)
         c->vib_speed = xarg (c->arg);
      if (yarg (c->arg) != 0)
         c->vib_depth = yarg (c->arg);
   }
   if (c->vib_speed != 0 && c->vib_depth != 0) {
      switch (c->vib_wave) {
      case WAVE_SINE:
         d = sin (c->vib_pos * M_2PI / PAC_WAVEPOS_MAX) * PAC_VOLUME_MAX;
         break;
      case WAVE_RAMPDOWN:
         d = (c->vib_pos - PAC_WAVEPOS_MAX / 2) / 2;
         break;
      case WAVE_SQUARE:
         d = (c->vib_pos < PAC_WAVEPOS_MAX / 2) ? PAC_VOLUME_MAX :
            -PAC_VOLUME_MAX;
         break;
      default:
         d = 0;
         assert (0);
         break;
      }
      c->dperiod = d * c->vib_depth / 32;
      c->vib_pos += 4 * c->vib_speed;
      if (c->vib_pos > PAC_WAVEPOS_MAX)
         c->vib_pos -= PAC_WAVEPOS_MAX;
   }
}

/* 05xy: Volume slide at speed <xy>, continue portamento. */
static void
fx_portavolslide (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_PORTAVOLSLIDE);

   fx_volslide (m, c);
   if (m->tick > 0)
      fx_porta (m, c);
}

/* 06xy: Volume slide at speed <xy>, continue vibrato. */
static void
fx_vibratovolslide (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_VIBRATOVOLSLIDE);

   fx_volslide (m, c);
   if (m->tick > 0)
      fx_vibrato (m, c);
}

/* 07xy: Tremolo with speed <x> and depth <y>. */
static void
fx_tremolo (struct pac_module *m, struct pac_channel *c)
{
   int d;

   assert (c->cmd == FX_TREMOLO);

   if (m->tick == 0) {
      if (xarg (c->arg))
         c->trem_speed = xarg (c->arg);
      if (yarg (c->arg))
         c->trem_depth = yarg (c->arg);
   }
   if (c->trem_speed != 0 && c->trem_depth != 0) {
      switch (c->trem_wave) {
      case WAVE_SINE:
         d = sin (c->trem_pos * M_2PI / PAC_WAVEPOS_MAX) * PAC_VOLUME_MAX;
         break;
      case WAVE_RAMPDOWN:
         d = (c->trem_pos - PAC_WAVEPOS_MAX / 2) / 2;
         break;
      case WAVE_SQUARE:
         d = (c->trem_pos < PAC_WAVEPOS_MAX / 2) ? PAC_VOLUME_MAX :
            -PAC_VOLUME_MAX;
         break;
      default:
         d = 0;
         assert (0);
         break;
      }
      d = d * c->trem_depth / 16;
      c->dvolume = d;
      c->trem_pos += 4 * c->trem_speed;
      if (c->trem_pos > PAC_WAVEPOS_MAX)
         c->trem_pos -= PAC_WAVEPOS_MAX;
   }
}

/* 08xy: Set pan position to <xy>. */
static void
fx_setpan (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_SETPAN && m->tick == 0);

   c->pan = c->arg;
   fx_clear (c);
}

/* 09xy: Set sample offset to <xy> * 256. */
static void
fx_setoffset (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_SETOFFSET && m->tick == 0);

   if (c->arg != 0)
      c->fx9val = c->arg * 256;
   c->offset = c->fx9val;
   c->frac = 0;
   if (m->sound[m->soundmap[c->instr]].bits == 16)
      c->offset /= sizeof (short);
   fx_clear (c);
}

/* 0Axy: Slide volume up at speed <x> or slide down at <y> if <x> is zero. */
static void
fx_volslide (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_VOLSLIDE ||
           c->cmd == FX_PORTAVOLSLIDE ||
           c->cmd == FX_VIBRATOVOLSLIDE);

   if (m->tick == 0) {
      if (c->arg != 0)
         c->volslide = c->arg;
   }
   else if (c->volslide != 0) {
      if (xarg (c->volslide) != 0) {
         c->volume += xarg (c->volslide);
         if (c->volume > PAC_VOLUME_MAX)
            c->volume = PAC_VOLUME_MAX;
      }
      else {
         c->volume -= yarg (c->volslide);
         if (c->volume < PAC_VOLUME_MIN)
            c->volume = PAC_VOLUME_MIN;
      }
   }
}

/* 0Bxy: Jump to song position <xy> at row 0. */
/* FIXME: some songs (SS.PAC) continue to play forever even when
   PAC_ENDLESS_SONGS is disabled. */
static void
fx_posjump (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_POSJUMP && m->tick == 0);

   m->nextrow = 0;
   m->nextpos = c->arg;
   if (m->nextpos >= m->poscnt) {
      if (pac_mode_flags & PAC_ENDLESS_SONGS)
         m->nextpos = m->time = 0;
      else
         m->eof = 1;
   }
   else if (m->nextpos < m->pos && m->pos == m->poscnt - 1) {
      if (pac_mode_flags & PAC_ENDLESS_SONGS)
         m->time = 0;
      else
         m->eof = 1;
   }
   m->posjmp = 1;
   fx_clear (c);
}

/* 0Cxy: Note off.  In version 1.4 of the PAC format it means note off at
   speed <xy> (argument is only used on the GUS or MIDI).  In 1.6 it sets
   the channel volume to <xy>. */
static void
fx_noteoff (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_NOTEOFF && m->tick == 0);

   if (m->fversion == PAC_FORMAT_14) {
      if (pac_mode_flags & PAC_GUS_EMULATION)
         pac_start_gus_ramp (m, c);
      else
         c->volume = PAC_VOLUME_MIN;
   }
   else {
      c->volume = c->arg;
      if (c->volume > PAC_VOLUME_MAX)
         c->volume = PAC_VOLUME_MAX;
   }
   fx_clear (c);
}

/* 0Dxy: Sheet break.  Jump to the next position and start playing at
   row <xy>.  D10 means start playing next position at row 10.  NOTE:
   SBStudio stores 0D10 as 0x0D10 rather than 0x0D0A. */
static void
fx_sheetbreak (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_SHEETBREAK && m->tick == 0);

   if (!m->sheetbrk) {
      m->nextrow = xarg (c->arg) * 10 + yarg (c->arg);
      if (m->nextrow >= PAC_ROW_MAX)
         m->nextrow = 0;
      if (!m->posjmp) {
         if (++m->nextpos >= m->poscnt) {
            if (pac_mode_flags & PAC_ENDLESS_SONGS) {
               m->nextpos = 0;
               m->time = 0;
            }
            else
               m->eof = 1;
         }
      }
      m->sheetbrk = 1;
   }
   fx_clear (c);
}

/* 0Exy: Extended effect <x> with argument <y>. */
static void
fx_extended (struct pac_module *m, struct pac_channel *c)
{
   int x = xarg (c->arg);

   assert (c->cmd == FX_EXTENDED && x > 0 && x < EFX_COUNT);

   if (x > 0 && x < EFX_COUNT)
      efx[x] (m, c);
}

/* 0Fxy: Set speed if <xy> is in [0,31] or tempo otherwise (<xy> is
   in [32,255]).  If <xy> is 0 then stop the module. */
static void
fx_setspeed (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_SETSPEED && m->tick == 0);

   if (c->arg > PAC_SPEED_MAX) {
      assert (m->tickpos == 0);
      m->tempo = c->arg;
      m->ticksize = pac_ticksize (m->tempo);
      if (m->tempo < m->tempo_min)
         m->tempo_min = m->tempo;
   }
   else {
      m->speed = c->arg;
      if (m->speed == 0)
         m->eof = 1;
   }
   fx_clear (c);
}

/* 10xy: Tremor.  Volume is unchanged for <x> ticks and zero for <y> ticks.
   1000 - use previously set <x> and <y> arguments.
   100y - y > 0, set volume to zero (until next row).
   10x0 - x > 0, do nothing.
   10xy - on for x ticks, off for y ticks.  Turn volume back on next row.  */
/* TODO: slightly messy.  Clean it up. */
static void
fx_tremor (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_TREMOR);

   if (m->tick == 0) {
      if (c->arg != 0)
         c->tremor_arg = c->arg;
      c->tremor_cnt = 0;
   }
   if (c->tremor_arg != 0) {
      int x = xarg (c->tremor_arg);
      int y = yarg (c->tremor_arg);
      if (x == 0 && y > 0) {
         c->dvolume = -c->volume;
         fx_clear (c);
      }
      else if (x > 0 && y == 0)
         fx_clear (c);
      else {
         if (c->dvolume == 0) {
            if (x && ++c->tremor_cnt == x) {
               c->dvolume = -c->volume;
               c->tremor_cnt = 0;
            }
         }
         else if (++c->tremor_cnt >= y) {
            c->dvolume = 0;
            c->tremor_cnt = 0;
         }
      }
   }
}

/* 15xy: Set global volume to <xy>. */
static void
fx_gvolume (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_GVOLUME && m->tick == 0);

   m->volume = c->arg;
   if (m->volume > PAC_VOLUME_MAX)
      m->volume = PAC_VOLUME_MAX;
   fx_clear (c);
}

/* 16xy: Global volume slide down at speed <x> or up at <y> if <x> is zero. */
static void
fx_gvolslide (struct pac_module *m, struct pac_channel *c)
{
   assert (c->cmd == FX_GVOLSLIDE);

   if (m->tick > 0 && c->arg != 0) {
      if (xarg (c->arg) != 0) {
         m->volume += xarg (c->arg);
         if (m->volume > PAC_VOLUME_MAX)
            m->volume = PAC_VOLUME_MAX;
      }
      else {
         m->volume -= yarg (c->arg);
         if (m->volume < PAC_VOLUME_MIN)
            m->volume = PAC_VOLUME_MIN;
      }
   }
}

/*
 * Extended effects.
 */

/* 0E1y: Frequency trim up by <y>. */
static void
efx_freqtrimup (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_FREQTRIMUP && m->tick == 0);

   c->period -= yarg (c->arg);
   if (c->period < m->period_min)
      c->period = m->period_min;
   fx_clear (c);
}

/* 0E2y: Frequency trim down by <y>. */
static void
efx_freqtrimdown (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_FREQTRIMDOWN && m->tick == 0);

   c->period += yarg (c->arg);
   if (c->period > m->period_max)
      c->period = m->period_max;
   fx_clear (c);
}

/* 0E3y: Glissando control.  Enable glissando if <y> is true.  When enabled,
   fx_porta will slide in semi-notes. */
static void
efx_glissando (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_GLISSANDO && m->tick == 0);

   c->glissando = yarg (c->arg);
   fx_clear (c);
}

/* 0E4y: Set vibrato waveform. */
static void
efx_vibratowave (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_VIBRATOWAVE && m->tick == 0);

   if (yarg (c->arg) <= WAVE_SQUARE)
      c->vib_wave = yarg (c->arg);
   fx_clear (c);
}

/* 0E5y: Set finetune to <y>. */
static void
efx_finetune (struct pac_module *m, struct pac_channel *c)
{
   int tune;

   assert (xarg (c->arg) == EFX_FINETUNE && m->tick == 0);

   tune = yarg (c->arg);
   if (tune > 7)
      tune -= 16;
   c->period = note_to_period (m, c->note, tune);
   fx_clear (c);
}

/* 0E6y: Sheet loop.  If <y> is 0, set loop start.  Else loop from current
   row to loop start <y> times, then continue. */
static void
efx_sheetloop (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_SHEETLOOP && m->tick == 0);

   if (yarg (c->arg) == 0)
      c->sheetloop_row = m->row;
   else if (c->sheetloop_row != 255 && ++c->sheetloop_cnt <= yarg (c->arg))
      m->row = c->sheetloop_row - 1;
   else {
      c->sheetloop_cnt = 0;
      c->sheetloop_row = 255;
   }
   fx_clear (c);
}

/* 0E7y: Set tremolo waveform. */
static void
efx_tremolowave (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_TREMOLOWAVE && m->tick == 0);

   if (yarg (c->arg) <= WAVE_SQUARE)
      c->trem_wave = yarg (c->arg);
   fx_clear (c);
}

/* 0E8y: Set pan position to <y>. */
static void
efx_setpan (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_SETPAN && m->tick == 0);

   c->pan = yarg (c->arg) * PAC_PAN_MAX / 15;
   fx_clear (c);
}

/* 0E9y: Retrig note every <y> ticks. */
/* FIXME: SBStudio seems to retrig even if <y> is greater than m->speed. */
static void
efx_retrignote (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_RETRIGNOTE);

   if (yarg (c->arg) > 0 && (m->tick % yarg (c->arg)) == 0)
      c->offset = c->frac = 0;
}

/* 0EAy: Volume trim up by <y>. */
static void
efx_voltrimup (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_VOLTRIMUP && m->tick == 0);

   c->volume += yarg (c->arg);
   if (c->volume > PAC_VOLUME_MAX)
      c->volume = PAC_VOLUME_MAX;
   fx_clear (c);
}

/* 0EBy: Volume trim down by <y>. */
static void
efx_voltrimdown (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_VOLTRIMDOWN && m->tick == 0);

   c->volume -= yarg (c->arg);
   if (c->volume < PAC_VOLUME_MIN)
      c->volume = PAC_VOLUME_MIN;
   fx_clear (c);
}

/* 0ECy: Cut note at tick <y>.  <y> must be less than or equal to the current
   song speed for this to have any effect. */
static void
efx_cutnote (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_CUTNOTE);

   if (m->tick == yarg (c->arg)) {
      c->volume = PAC_VOLUME_MIN;
      fx_clear (c);
   }
}

/* 0EDy: Delay note for <y> ticks.  The current note will continue playing
   until we have delayed for <y> ticks.  The delay will continue into the
   next row if <y> is greater than the current song speed. */
static void
efx_delaynote (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_DELAYNOTE && m->tick == 0);

   c->delay = yarg (c->arg) + 1;
   c->delay_note = m->sheet[m->postbl[m->pos]].note +
      m->row * m->channelcnt + (c - m->channel);
   fx_clear (c);
}

/* 0EEy: Delay sheet for <y> rows. */
static void
efx_sheetdelay (struct pac_module *m, struct pac_channel *c)
{
   assert (xarg (c->arg) == EFX_SHEETDELAY && m->tick == 0);

   m->sheetdelay = yarg (c->arg);
   fx_clear (c);
}
