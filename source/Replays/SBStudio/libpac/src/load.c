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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "pacP.h"

/* Sheet packing codes. */
#define END_OF_NOTE 0xFDU
#define END_OF_ROW 0xFEU
#define END_OF_SHEET 0xFFU

/* Miscellaneous flags. */
#define SHEET_PACKED 0x01U
#define SOUND_16BIT 0x02U
#define SOUND_MIDDLEC 0x08U

/* File block IDs (x86 endian). */
#define ID_PACG 0x47434150UL
#define ID_PAOR 0x524f4150UL
#define ID_PAIN 0x4e494150UL
#define ID_SOIN 0x4e494f53UL
#define ID_SONA 0x414e4f53UL
#define ID_SONG 0x474e4f53UL
#define ID_SOOR 0x524f4f53UL
#define ID_SOCS 0x53434f53UL
#define ID_SOCN 0x4e434f53UL
#define ID_SOSH 0x48534f53UL
#define ID_SND  0x20444e53UL
#define ID_SNNA 0x414e4e53UL
#define ID_SNIN 0x4e494e53UL
#define ID_SNDT 0x54444e53UL
#define ID_END  0x20444e45UL

/* PAC file format types. */
typedef uint8_t BYTE;
typedef uint16_t INT;
typedef uint32_t LONG;

/* XXX convert from LE to BE. */
#ifdef CPU_IS_BIG_ENDIAN
#define LE_INT(n) (n)
#define LE_LONG(n) (n)
#else
#define LE_INT(n) (n)
#define LE_LONG(n) (n)
#endif

/* Used by pac_exit for cleaning up (init.c). */
struct pac_module *pac_module_list;

static int read_sheet (struct pac_module *, int, FILE *);
static int read_sound (struct pac_module *, int, FILE *);
static LONG next_block (FILE *, LONG *);
static long find_block (FILE *, LONG, int);
static int scan_module (struct pac_module *);

/* Return true if NAME seems to be a PAC file. */
int
pac_test (const char *name)
{
   FILE *fp;
   LONG id = 0;

   if ((fp = fopen (name, "rb")) == NULL)
      return 0;
   fread (&id, sizeof id, 1, fp);
   fclose (fp);
   if (id == ID_PACG)
      return 1;
   errno = PAC_EFORMAT;
   return 0;
}

struct pac_module *
pac_open (const char *filename)
{
   extern int pac_initialized;
   extern const struct pac_module pac_module_init;
   extern const struct pac_sound pac_sound_init;
   extern const struct pac_channel pac_channel_init;
   BYTE pb;
   INT pi;
   LONG pl;
   FILE *fp;
   long size;
   int i;
   struct pac_module *m;

   if (!pac_initialized) {
      errno = PAC_ENOTINIT;
      return NULL;
   }

   if ((fp = fopen (filename, "rb")) == NULL)
      return NULL;

   fread (&pl, sizeof pl, 1, fp);
   if (pl != ID_PACG) {
      fclose (fp);
      errno = PAC_EFORMAT;
      return NULL;
   }

   /* Allocate and initialize module storage. */
   if ((m = malloc (sizeof *m)) == NULL) {
      fclose (fp);
      return NULL;
   }
   *m = pac_module_init;

   /* Locate and read the PAIN block (required). */
   errno = 0;
   if ((size = find_block (fp, ID_PAIN, 1)) < 6)
      goto error;
   fread (&pi, 1, sizeof pi, fp);
   m->fversion = pi;
   fread (&pi, 1, sizeof pi, fp);
   m->tversion = pi;
   fread (&pi, 1, sizeof pi, fp);
   m->soundcnt = LE_INT (pi);
   if (m->soundcnt > PAC_SOUND_MAX)
      goto error;

   /* Set minimum and maximum period values.  File version 1.4 has 4 octaves
      (48 notes) while file version 1.6 has 6 octaves (72 notes). */
   switch (m->fversion) {
   case PAC_FORMAT_14:
      m->note_max = 48;
      m->period_min = 54;
      m->period_max = 856;
      break;
   case PAC_FORMAT_16:
      m->note_max = 72;
      m->period_min = 27;
      m->period_max = 1712;
      break;
   default:
      goto error;
   }

   /* Find the SONG block (required). */
   if ((size = find_block (fp, ID_SONG, 1)) != 0)
      goto error;

   /* Read the SONA block (optional). */
   if ((size = find_block (fp, ID_SONA, 1)) > 0) {
      if (size > PAC_NAME_MAX)
         size = PAC_NAME_MAX;
      fread (m->name, 1, size, fp);
      m->name[size] = '\0';
   }

   /* Read the SOIN block (required). */
   if ((size = find_block (fp, ID_SOIN, 1)) < 8)
      goto error;
   fread (&pb, sizeof pb, 1, fp);
   m->speed_init = pb;
   if (m->speed_init < PAC_SPEED_MIN + 1 ||
       m->speed_init > PAC_SPEED_MAX)
      goto error;
   fread (&pb, sizeof pb, 1, fp);
   m->tempo_init = pb;
   if (m->tempo_init < PAC_TEMPO_MIN ||
       m->tempo_init > PAC_TEMPO_MAX)
      goto error;
   fread (&pi, sizeof pi, 1, fp);
   m->sheetcnt = LE_INT (pi);
   if (m->sheetcnt < PAC_SHEET_MIN ||
       m->sheetcnt > PAC_SHEET_MAX)
      goto error;
   fread (&pb, sizeof pb, 1, fp);
   m->channelcnt = pb;
   if (m->channelcnt < PAC_CHANNEL_MIN ||
       m->channelcnt > PAC_CHANNEL_MAX)
      goto error;
   fseek (fp, 3, SEEK_CUR);

   /* Allocate and initialize the channels. */
   m->channel = calloc (m->channelcnt, sizeof *m->channel);
   if (m->channel == NULL)
      goto error;
   for (i = 0; i < m->channelcnt; i++)
      m->channel[i] = pac_channel_init;

   /* Read channel pan positions. */
   if (m->fversion == PAC_FORMAT_14) {
      if ((size - 8) != m->channelcnt)
         goto error;
      for (i = 0; i < m->channelcnt; i++) {
         fread (&pb, sizeof pb, 1, fp);
         m->channel[i].pan_init = pb * PAC_PAN_MAX / 15;
      }
   }
   else {
      for (i = 0; (size = find_block (fp, ID_SOCS, !i)) >= 6; i++) {
         fread (&pb, sizeof pb, 1, fp);
         if (pb < m->channelcnt) {
            struct pac_channel *c = &m->channel[pb];
            fread (&pb, sizeof pb, 1, fp);
            c->pan_init = pb;
            fseek (fp, size - 2, SEEK_CUR);
         }
         else
            fseek (fp, size - 1, SEEK_CUR);
      }
      if (i != m->channelcnt && (pac_mode_flags & PAC_STRICT_FORMAT))
         goto error;
   }

   /* Read the SOOR block (required). */
   if ((size = find_block (fp, ID_SOOR, 1)) > 0) {
      size /= 2;
      if (size < PAC_LENGTH_MIN || size > PAC_LENGTH_MAX)
         goto error;
      for (m->poscnt = 0; m->poscnt < size; m->poscnt++) {
         fread (&pi, sizeof pi, 1, fp);
         m->postbl[m->poscnt] = LE_INT (pi);
         if (m->postbl[m->poscnt] >= m->sheetcnt)
            goto error;
      }
      if (feof (fp) || ferror (fp))
         goto error;
   }

   /* Allocate and initialize the sounds. */
   m->sound = calloc (m->soundcnt + 1, sizeof *m->sound);
   if (m->sound == NULL)
      goto error;
   for (i = 0; i < m->soundcnt + 1; i++)
      m->sound[i] = pac_sound_init;

   /* Read the sounds.  One SND block for every sound. */
   if (!(pac_mode_flags & PAC_NOSOUNDS)) {
      for (i = 0; find_block (fp, ID_SND, !i) == 0; i++)
         if (read_sound (m, i, fp) != 0)
            if (pac_mode_flags & PAC_STRICT_FORMAT)
               goto error;
      if (i != m->soundcnt && (pac_mode_flags & PAC_STRICT_FORMAT))
         goto error;
   }

   /* Read the sheets.  One SOSH block for every sheet. */
   for (i = 0; find_block (fp, ID_SOSH, !i) > 0; i++)
      if (read_sheet (m, i, fp) != 0)
         goto error;
   if (i != m->sheetcnt)
      goto error;

   /* Find module length and allocate tick buffer. */
   size = scan_module (m);
/* XXX check for possible overflow */
   m->tickbuf = malloc (size * pac_channels * sizeof *m->tickbuf);
   if (m->tickbuf == NULL)
      goto error;

   fclose (fp);
   pac_reset (m);
   m->next = pac_module_list;
   pac_module_list = m;
   return m;

error:
   if (errno == 0)
      errno = PAC_EFORMAT;
   fclose (fp);
   pac_close (m);
   return NULL;
}

void
pac_close (struct pac_module *m)
{
   int i;
   struct pac_module **n;

   if (m == NULL)
      return;

   for (i = 0; i < m->sheetcnt; i++)
      free (m->sheet[i].note);
   if (m->sound != NULL) {
      for (i = 0; i < m->soundcnt + 1; i++)
         free (m->sound[i].sample);
      free (m->sound);
   }
   free (m->channel);
   free (m->tickbuf);

   for (n = &pac_module_list; *n != NULL && *n != m; n = &(*n)->next)
      ;
   *n = m->next;
   free (m);
}

const char *
pac_title (const struct pac_module *m)
{
   return m->name;
}

static int
read_sheet (struct pac_module *m, int i, FILE *fp)
{
   extern const struct pac_note pac_note_init;
   struct pac_sheet *s;
   struct pac_note *n;
   int row;

   assert (m != NULL && i >= 0 && fp != NULL);

   s = &m->sheet[i];
/* XXX check for possible overflow */
   s->note = malloc (PAC_ROW_MAX * m->channelcnt * sizeof *s->note);
   if (s->note == NULL)
      return -1;

   for (n = s->note; n < s->note + PAC_ROW_MAX * m->channelcnt; n++)
      *n = pac_note_init;

   for (row = 0; row < PAC_ROW_MAX; row++) {
      n = s->note + row * m->channelcnt;
      for (; n < s->note + (row + 1) * m->channelcnt; n++) {
         BYTE pb;
         fread (&pb, sizeof pb, 1, fp);
         if (pb >= END_OF_NOTE) {
            if (pb == END_OF_NOTE)
               continue;
            if (pb == END_OF_ROW)
               break;
            if (pb == END_OF_SHEET)
               return 0;
         }
         if (m->fversion == PAC_FORMAT_16) {
            assert (pb == 0 || pb == 2 || (pb >= 3 && pb <= 74));
            if (pb == 2)
               pb = PAC_NOTE_OFF;
            else if (pb != 0)
               pb -= 2;
         }
         else {
            assert (pb == 0 || (pb >= 2 && pb <= 49));
            if (pb != 0)
               pb -= 1;
         }
         n->number = pb;

         fread (&pb, sizeof pb, 1, fp);
         n->instr = pb;

         fread (&pb, sizeof pb, 1, fp);
         if (pb >= END_OF_NOTE) {
            if (pb == END_OF_NOTE)
               continue;
            if (pb == END_OF_ROW)
               break;
            if (pb == END_OF_SHEET)
               return 0;
         }
         if (pb > PAC_VOLUME_MAX + 1)
            pb = PAC_VOLUME_MAX + 1;
         n->volume = pb;

         fread (&pb, sizeof pb, 1, fp);
         n->cmd = pb;

         fread (&pb, sizeof pb, 1, fp);
         n->arg = pb;
      }
   }
   return 0;
}

static int
read_sound (struct pac_module *m, int i, FILE *fp)
{
   extern const struct pac_sound pac_sound_init;
   struct pac_sound s = pac_sound_init;
   int done, snna, sndt, snin;
   BYTE pb;
   INT pi;
   LONG pl, size;

   for (done = snna = sndt = snin = 0; !done; ) {
      long skip = 0, org = ftell (fp);
      switch (next_block (fp, &size)) {
      case ID_SNNA:
         if (snna)
            return -1;
#if 1 /* Skip reading sound name. */
         if (size > PAC_NAME_MAX) {
            skip = size - PAC_NAME_MAX;
            size = PAC_NAME_MAX;
         }
         fread (s.name, 1, size, fp);
         s.name[size] = '\0';
         fseek (fp, skip, SEEK_CUR);
#else
         fseek (fp, size, SEEK_CUR);
#endif
         if (feof (fp) || ferror (fp))
            return -1;
         snna = 1;
         break;
      case ID_SNIN:
         if (snin)
            return -1;
         fread (&pi, sizeof pi, 1, fp);
         s.number = LE_INT (pi);
         fread (&pi, sizeof pi, 1, fp);
         s.middlec = LE_INT (pi);
         fread (&pb, sizeof pb, 1, fp);
         s.tune = (pb > PAC_TUNE_MAX) ? (int) pb - 16 : pb;
         fread (&pi, sizeof pi, 1, fp);
         s.volume = LE_INT (pi) * PAC_VOLUME_MAX / 16384;
         fread (&pi, sizeof pi, 1, fp);
         pi = LE_INT (pi);
         s.bits = (pi & SOUND_16BIT) ? 16 : 8;
         if ((pi & SOUND_MIDDLEC) == 0)
            s.middlec = PAC_MIDDLEC_DEFAULT;
         fread (&pl, sizeof pl, 1, fp);
         s.loopstart = LE_LONG (pl);
         fread (&pl, sizeof pl, 1, fp);
         s.loopend = LE_LONG (pl);
         fseek (fp, 1, SEEK_CUR);
         if (feof (fp) || ferror (fp))
            return -1;
         snin = 1;
         break;
      case ID_SNDT:
         if (sndt)
            return -1;
         s.length = size;
         if (s.length > 0) {
            /* Allocate one extra sample for interpolation. */
            s.sample = malloc (s.length + sizeof (short));
            if (s.sample == NULL)
               return -1;
            if (fread (s.sample, 1, size, fp) < size)
               return -1;
         }
         if (feof (fp) || ferror (fp))
            return -1;
         sndt = 1;
         break;
      default:
         fseek (fp, org, SEEK_SET);
         if (!snin)
            return -1;
         done = 1;
         break;
      }
   }

   if (s.bits == 16) {
      s.length /= sizeof (short);
      s.loopstart /= sizeof (short);
      s.loopend /= sizeof (short);
      if (s.length > 0)
         *((short *) s.sample + s.length) = 0;
#ifdef CPU_IS_BIG_ENDIAN
      {
         short *p, *e;

         for (p = s.sample, e = p + s.length; p < e; p++)
            *p = LE_INT (*p);
      }
#endif
   }
   else if (s.length > 0)
      *((signed char *) s.sample + s.length) = 0;

   if (s.loopstart > s.loopend || s.loopend > s.length) {
      if (pac_mode_flags & PAC_STRICT_FORMAT)
         return -1;
      s.loopstart = s.loopend = 0;
   }
   m->soundmap[s.number+1] = i+1;
   m->sound[i+1] = s;
   return 0;
}

static LONG
next_block (FILE *fp, LONG *size)
{
   LONG name;

   if (fread (&name, sizeof name, 1, fp) == 1)
      if (fread (size, sizeof *size, 1, fp) == 1)
         return name;
   *size = 0;
   return ID_END;
}

static long
find_block (FILE *fp, LONG id, int restart)
{
   LONG name, size;

   if (restart)
      fseek (fp, 8, SEEK_SET);
   while ((name = next_block (fp, &size)) != id) {
      if (name == ID_END)
         return -1;
      fseek (fp, LE_LONG (size), SEEK_CUR);
   }
   return LE_LONG (size);
}

/* Scan module 'm' to obtain the minimum tempo value and the length, in frames,
   of the module.  This is done by reading the entire module with the mixers
   disabled (m->seeking).  A lower tempo value means a larger tick buffer (more
   samples per tick). */
static int
scan_module (struct pac_module *m)
{
   int mode;

   mode = pac_mode_flags;
   pac_disable (PAC_ENDLESS_SONGS);
   m->tempo_min = m->tempo_init;
   pac_reset (m);
   m->seeking = 1;
   if (pac_read (m, NULL, LONG_MAX / 8 * 8) < (LONG_MAX / 8 * 8))
      m->length = m->time;
   m->seeking = 0;
   pac_mode_flags = mode;
   return pac_ticksize (m->tempo_min);
}
