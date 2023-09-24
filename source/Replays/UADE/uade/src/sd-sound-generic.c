/* 
 * UADE
 * 
 * Support for sound
 * 
 * Copyright 2000 - 2005 Heikki Orsila <heikki.orsila@iki.fi>
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "memory.h"
#include "custom.h"
#include "gensound.h"
#include "sd-sound.h"
#include "audio.h"
#include "uadectl.h"
#include <uade/uadeconstants.h>

uae_u16 sndbuffer[MAX_SOUND_BUF_SIZE / 2];
uae_u16 *sndbufpt;

int sound_bytes_per_second;

void close_sound (void)
{
}

/* Try to determine whether sound is available.  */
int setup_sound (void)
{
   sound_available = 1;
   return 1;
}


void set_sound_freq(int x)
{
  /* Validation is done later in init_sound() */
  currprefs.sound_freq = x;
  init_sound();
}


void init_sound (void)
{
  int channels;
  int dspbits;
  unsigned int rate;
  
  dspbits = currprefs.sound_bits;
  rate    = currprefs.sound_freq;
  channels = currprefs.stereo ? 2 : 1;

  if (dspbits != 16) {
    assert(0 && "Only 16 bit sounds supported");
    currprefs.sound_bits = dspbits = 16;
  }
  if (rate < 1 || rate > SOUNDTICKS_NTSC) {
    assert(0 &&"Too small or high a rate");
    currprefs.sound_freq = rate = SOUNDTICKS_PAL;
  }
  if (channels != 2) {
    assert(0 && "Only stereo supported");
    currprefs.stereo = channels = 2;
  }

  sound_bytes_per_second = (dspbits / 8) *  channels * rate;

  audio_set_rate(rate);

  sound_available = 1;
  
  sndbufpt = sndbuffer;
}

/* this should be called between subsongs when remote slave changes subsong */
void flush_sound (void)
{
  sndbufpt = sndbuffer;
}
