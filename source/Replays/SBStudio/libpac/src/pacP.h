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

#ifndef H_PACP_INCLUDED
#define H_PACP_INCLUDED

#include "pac.h"

/*
 * PAC format limits.
 */

#define PAC_NOTE_OFF 255
#define PAC_NOTE_MIN 1

#define PAC_TUNE_MIN -8
#define PAC_TUNE_MAX 7
#define PAC_TUNE_DEFAULT 0

#define PAC_SPEED_MIN 0
#define PAC_SPEED_MAX 31

#define PAC_TEMPO_MIN 32
#define PAC_TEMPO_MAX 255

#define PAC_SOUND_MIN 1
#define PAC_SOUND_MAX 255

#define PAC_CHANNEL_MIN 4
#define PAC_CHANNEL_MAX 20

#define PAC_SHEET_MIN 1
#define PAC_SHEET_MAX 255

#define PAC_LENGTH_MIN 1
#define PAC_LENGTH_MAX 256

#define PAC_ROW_MIN 0
#define PAC_ROW_MAX 64

#define PAC_PAN_MIN 0
#define PAC_PAN_MAX 255
#define PAC_PAN_CENTER 127
#define PAC_PAN_DEFAULT PAC_PAN_CENTER

#define PAC_VOLUME_MIN 0
#define PAC_VOLUME_MAX 64

#define PAC_MIDDLEC_MIN 1045L
#define PAC_MIDDLEC_MAX 50178L
#define PAC_MIDDLEC_DEFAULT 8363L

#define PAC_WAVEPOS_MIN 0
#define PAC_WAVEPOS_MAX 255

#define PAC_FORMAT_14 0x0401U
#define PAC_FORMAT_16 0x0601U

/*
 * PAC module structures.
 */

struct pac_channel
{
   int playing;
   int volume;
   int dvolume;
   unsigned long gus_volume;
   unsigned long gus_step;
   int period;
   int dperiod;
   int pan;
   int pan_init;
   int fx9val;
   long offset;
   long frac;
   int note;
   int instr;
   int cmd;
   int arg;
   int volslide;
   int glissando;
   int porta_speed;
   int porta_period;
   int porta_note;
   int tremor_cnt;
   int tremor_arg;
   int sheetloop_row;
   int sheetloop_cnt;
   int vib_wave;
   int vib_pos;
   int vib_speed;
   int vib_depth;
   int trem_wave;
   int trem_pos;
   int trem_speed;
   int trem_depth;
   int delay;
   struct pac_note *delay_note;
};

struct pac_sound
{
   char name[PAC_NAME_MAX+1];
   int number;
   long middlec;
   int tune;
   int volume;
   int bits;
   long loopstart;
   long loopend;
   long length;
   void *sample;
};

struct pac_note
{
   unsigned char number;
   unsigned char instr;
   unsigned char volume;
   unsigned char cmd;
   unsigned char arg;
};

struct pac_sheet
{
   struct pac_note *note;
};

struct pac_module
{
   struct pac_module *next;
   char name[PAC_NAME_MAX+1];
   unsigned fversion;
   unsigned tversion;
   struct pac_sound *sound;
   short soundmap[PAC_SOUND_MAX];
   int soundcnt;
   struct pac_channel *channel;
   int channelcnt;
   struct pac_sheet sheet[PAC_SHEET_MAX];
   int sheetcnt;
   short postbl[PAC_LENGTH_MAX];
   int poscnt;
   int volume;
   int seeking;
   int speed;
   int speed_init;
   int tempo;
   int tempo_init;
   int tempo_min;
   int row;
   int nextrow;
   int pos;
   int nextpos;
   int tick;
   int tickpos;
   long *tickbuf;
   int ticksize;
   int posjmp;
   int sheetbrk;
   int eof;
   int note_max;
   int period_min;
   int period_max;
   int sheetdelay;
   long length;
   long time;
};

extern int pac_framesize;
extern long pac_rate;
extern int pac_bits;
extern int pac_channels;
extern int pac_mode_flags;

#define pac_ticksize(tempo) ((long) (pac_rate / ((tempo) * 0.4) + .5))

void pac_reset (struct pac_module *);
void pac_mix_tick (struct pac_module *);
void pac_update_channels (struct pac_module *);
void pac_update_effects (struct pac_module *);

void pac_start_gus_ramp (struct pac_module *, struct pac_channel *);
int pac_update_gus_ramp (struct pac_channel *, long);
void pac_stop_gus_ramp (struct pac_channel *);

#endif /* H_PACP_INCLUDED */
