/******************************************************************************/
/* Mednafen - Multi-system Emulator                                           */
/******************************************************************************/
/* sound.cpp - WonderSwan Sound Emulation
**  Copyright (C) 2007-2017 Mednafen Team
**  Copyright (C) 2016 Alex 'trap15' Marshall - http://daifukkat.su/
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "wswan.h"
#include "sound.h"
#include "v30mz.h"
#include "memory.h"

#include "../sound/Blip_Buffer.h"

namespace MDFN_IEN_WSWAN
{
#ifdef WSRP_REMOVED
static Blip_Synth<blip_good_quality, 4096> WaveSynth;
#else
static Blip_Synth<blip_high_quality, 4096> WaveSynth;
static Blip_Synth<blip_med_quality, 4096, 1> NoiseSynth;
static const double wave_synth_treble_db = -8.0;
#endif
static Blip_Buffer *sbuf[2] = { NULL };

static uint16 period[4];
static uint8 volume[4]; // left volume in upper 4 bits, right in lower 4 bits
static uint8 voice_volume;

static uint8 sweep_step, sweep_value;
static uint8 noise_control;
static uint8 control;
static uint8 output_control;

static int32 sweep_8192_divider;
static uint8 sweep_counter;
static uint8 SampleRAMPos;

static int32 sample_cache[4][2];

static int32 last_v_val;

static uint8 HyperVoice;
static int32 last_hv_val[2];
static uint8 HVoiceCtrl, HVoiceChanCtrl;

static int32 period_counter[4];
static int32 last_val[4][2]; // Last outputted value, l&r
static uint8 sample_pos[4];
static uint16 nreg;
static uint32 last_ts;

#ifdef WSRP_REMOVED
#else
static uint32 channel_mute = 0;
static Silent_Blip_Buffer sbuf_dummy;

void WSwan_SetChannelMute(uint32 mute)
{
	channel_mute = mute;
}

#define SYNCSAMPLE_MUTE(wt)	\
   {	\
    int32 left = sample_cache[ch][0], right = sample_cache[ch][1];	\
    WaveSynth.offset_inline(wt, left - last_val[ch][0], &sbuf_dummy);	\
    WaveSynth.offset_inline(wt, right - last_val[ch][1], &sbuf_dummy);	\
    last_val[ch][0] = left;	\
    last_val[ch][1] = right;	\
   }

#define SYNCSAMPLE_NOISE_MUTE(wt) \
	{	\
    int32 left = sample_cache[ch][0], right = sample_cache[ch][1];	\
    NoiseSynth.offset_inline(wt, left - last_val[ch][0], &sbuf_dummy);	\
    NoiseSynth.offset_inline(wt, right - last_val[ch][1], &sbuf_dummy);	\
    last_val[ch][0] = left;	\
    last_val[ch][1] = right;	\
   }
#endif

#define MK_SAMPLE_CACHE	\
   {    \
    int sample; \
    sample = (((wsRAM[((SampleRAMPos << 6) + (sample_pos[ch] >> 1) + (ch << 4)) ] >> ((sample_pos[ch] & 1) ? 4 : 0)) & 0x0F)); \
    sample_cache[ch][0] = sample * ((volume[ch] >> 4) & 0x0F); \
    sample_cache[ch][1] = sample * ((volume[ch] >> 0) & 0x0F); \
   }

#define MK_SAMPLE_CACHE_NOISE \
   {    \
    int sample; \
    sample = ((nreg & 1) ? 0xF : 0x0);  \
    sample_cache[ch][0] = sample * ((volume[ch] >> 4) & 0x0F); \
    sample_cache[ch][1] = sample * ((volume[ch] >> 0) & 0x0F); \
   }

#define MK_SAMPLE_CACHE_VOICE \
   {    \
    int sample, half; \
    sample = volume[ch]; \
    half = sample >> 1; \
    sample_cache[ch][0] = (voice_volume & 4) ? sample : (voice_volume & 8) ? half : 0; \
    sample_cache[ch][1] = (voice_volume & 1) ? sample : (voice_volume & 2) ? half : 0; \
   }


#define SYNCSAMPLE(wt)	\
   {	\
    int32 left = sample_cache[ch][0], right = sample_cache[ch][1];	\
    WaveSynth.offset_inline(wt, left - last_val[ch][0], sbuf[0]);	\
    WaveSynth.offset_inline(wt, right - last_val[ch][1], sbuf[1]);	\
    last_val[ch][0] = left;	\
    last_val[ch][1] = right;	\
   }
#ifdef WSRP_REMOVED
#define SYNCSAMPLE_NOISE(wt) SYNCSAMPLE(wt)
#else
#define SYNCSAMPLE_NOISE(wt) \
	{	\
    int32 left = sample_cache[ch][0], right = sample_cache[ch][1];	\
    NoiseSynth.offset_inline(wt, left - last_val[ch][0], sbuf[0]);	\
    NoiseSynth.offset_inline(wt, right - last_val[ch][1], sbuf[1]);	\
    last_val[ch][0] = left;	\
    last_val[ch][1] = right;	\
   }
#endif

void WSwan_SoundUpdate(void)
{
 int32 run_time;

 //printf("%d\n", v30mz_timestamp);
 //printf("%02x %02x\n", control, noise_control);
 run_time = v30mz_timestamp - last_ts;

 for(unsigned int ch = 0; ch < 4; ch++)
 {
  // Channel is disabled?
  if(!(control & (1 << ch)))
   continue;

  if(ch == 1 && (control & 0x20)) // Direct D/A mode?
  {
   MK_SAMPLE_CACHE_VOICE;
#ifdef WSRP_REMOVED
   SYNCSAMPLE(v30mz_timestamp);
#else
   if (!(channel_mute & (1 << ch)))
    SYNCSAMPLE(v30mz_timestamp)
   else
    SYNCSAMPLE_MUTE(v30mz_timestamp)
#endif
  }
  else if(ch == 2 && (control & 0x40) && sweep_value) // Sweep
  {
   uint32 tmp_pt = 2048 - period[ch];
   uint32 meow_timestamp = v30mz_timestamp - run_time;
   uint32 tmp_run_time = run_time;

   while(tmp_run_time)
   {
    int32 sub_run_time = tmp_run_time;

    if(sub_run_time > sweep_8192_divider)
     sub_run_time = sweep_8192_divider;

    sweep_8192_divider -= sub_run_time;
    if(sweep_8192_divider <= 0)
    {
     sweep_8192_divider += 8192;
     sweep_counter--;
     if(sweep_counter <= 0)
     {
      sweep_counter = sweep_step + 1;
      period[ch] = (period[ch] + (int8)sweep_value) & 0x7FF;
#ifdef WSRP_REMOVED
#else
	  tmp_pt = 2048 - period[ch];
#endif
     }
    }

    meow_timestamp += sub_run_time;
    if(tmp_pt > 4)
    {
     period_counter[ch] -= sub_run_time;
     while(period_counter[ch] <= 0)
     {
      sample_pos[ch] = (sample_pos[ch] + 1) & 0x1F;

      MK_SAMPLE_CACHE;
#ifdef WSRP_REMOVED
      SYNCSAMPLE(meow_timestamp + period_counter[ch]);
#else
      if (!(channel_mute & (1 << ch)))
        SYNCSAMPLE(meow_timestamp + period_counter[ch])
      else
        SYNCSAMPLE_MUTE(meow_timestamp + period_counter[ch])
#endif
      period_counter[ch] += tmp_pt;
     }
    }
    tmp_run_time -= sub_run_time;
   }
  }
  else if(ch == 3 && (control & 0x80) && (noise_control & 0x10)) // Noise
  {
   uint32 tmp_pt = 2048 - period[ch];

   period_counter[ch] -= run_time;
   while(period_counter[ch] <= 0)
   {
    static const uint8 stab[8] = { 14, 10, 13, 4, 8, 6, 9, 11 };

    nreg = ((nreg << 1) | ((1 ^ (nreg >> 7) ^ (nreg >> stab[noise_control & 0x7])) & 1)) & 0x7FFF;

    if(control & 0x80)
    {
     MK_SAMPLE_CACHE_NOISE;
#ifdef WSRP_REMOVED
     SYNCSAMPLE_NOISE(v30mz_timestamp + period_counter[ch]);
#else
     if (!(channel_mute & (1 << ch)))
        SYNCSAMPLE_NOISE(v30mz_timestamp + period_counter[ch])
     else
        SYNCSAMPLE_NOISE_MUTE(v30mz_timestamp + period_counter[ch])
#endif
    }
    else if(tmp_pt > 4)
    {
     sample_pos[ch] = (sample_pos[ch] + 1) & 0x1F;
     MK_SAMPLE_CACHE;
#ifdef WSRP_REMOVED
     SYNCSAMPLE(v30mz_timestamp + period_counter[ch]);
#else
     if (!(channel_mute & (1 << ch)))
        SYNCSAMPLE(v30mz_timestamp + period_counter[ch])
     else
        SYNCSAMPLE_MUTE(v30mz_timestamp + period_counter[ch])
#endif
    }
    period_counter[ch] += tmp_pt;
   }
  }
  else
  {
   uint32 tmp_pt = 2048 - period[ch];

   if(tmp_pt > 4)
   {
    period_counter[ch] -= run_time;
    while(period_counter[ch] <= 0)
    {
     sample_pos[ch] = (sample_pos[ch] + 1) & 0x1F;

     MK_SAMPLE_CACHE;
#ifdef WSRP_REMOVED
     SYNCSAMPLE(v30mz_timestamp + period_counter[ch]); // - period_counter[ch]);
#else
     if (!(channel_mute & (1 << ch)))
         SYNCSAMPLE(v30mz_timestamp + period_counter[ch]) // - period_counter[ch]);
     else
         SYNCSAMPLE_MUTE(v30mz_timestamp + period_counter[ch])
#endif
     period_counter[ch] += tmp_pt;
    }
   }
  }
 }

 if(HVoiceCtrl & 0x80)
 {
  int16 sample = (uint8)HyperVoice;

  switch(HVoiceCtrl & 0xC)
  {
   case 0x0: sample = (uint16)sample << (8 - (HVoiceCtrl & 3)); break;
   case 0x4: sample = (uint16)(sample | -0x100) << (8 - (HVoiceCtrl & 3)); break;
   case 0x8: sample = (uint16)((int8)sample) << (8 - (HVoiceCtrl & 3)); break;
   case 0xC: sample = (uint16)sample << 8; break;
  }
  // bring back to 11bit, keeping signedness
  sample >>= 5;

  int32 left, right;
  left  = (HVoiceChanCtrl & 0x40) ? sample : 0;
  right = (HVoiceChanCtrl & 0x20) ? sample : 0;
#ifdef WSRP_REMOVED
#else
 if (!(channel_mute & (1 << 1)))
 {
#endif
  WaveSynth.offset_inline(v30mz_timestamp, left - last_hv_val[0], sbuf[0]);
  WaveSynth.offset_inline(v30mz_timestamp, right - last_hv_val[1], sbuf[1]);
#ifdef WSRP_REMOVED
#else
 }
 else
 {
  WaveSynth.offset_inline(v30mz_timestamp, left - last_hv_val[0], &sbuf_dummy);
  WaveSynth.offset_inline(v30mz_timestamp, right - last_hv_val[1], &sbuf_dummy);
 }
#endif
  last_hv_val[0] = left;
  last_hv_val[1] = right;
 }
 last_ts = v30mz_timestamp;
}

void WSwan_SoundWrite(uint32 A, uint8 V)
{
 WSwan_SoundUpdate();

 if(A >= 0x80 && A <= 0x87)
 {
  int ch = (A - 0x80) >> 1;

  if(A & 1)
   period[ch] = (period[ch] & 0x00FF) | ((V & 0x07) << 8);
  else
   period[ch] = (period[ch] & 0x0700) | ((V & 0xFF) << 0);

  //printf("Period %d: 0x%04x --- %f\n", ch, period[ch], 3072000.0 / (2048 - period[ch]));
 }
 else if(A >= 0x88 && A <= 0x8B)
 {
  volume[A - 0x88] = V;
 }
 else if(A == 0x8C)
  sweep_value = V;
 else if(A == 0x8D)
 {
  sweep_step = V;
  sweep_counter = sweep_step + 1;
  sweep_8192_divider = 8192;
 }
 else if(A == 0x8E)
 {
  //printf("NOISECONTROL: %02x\n", V);
  if(V & 0x8)
   nreg = 0;

  noise_control = V & 0x17;
 }
 else if(A == 0x90)
 {
  for(int n = 0; n < 4; n++)
  {
   if(!(control & (1 << n)) && (V & (1 << n)))
   {
    period_counter[n] = 1;
    sample_pos[n] = 0x1F;
   }
  }
#ifdef WSRP_REMOVED
#else
  if (V & 0x20)
  {
    if (!(control & 0x20))
       volume[0x89 - 0x88] = 0x80;  //silence
  }
#endif
  control = V;
  //printf("Sound Control: %02x\n", V);
 }
 else if(A == 0x91)
 {
  output_control = V & 0xF;
  //printf("%02x, %02x\n", V, (V >> 1) & 3);
 }
 else if(A == 0x92)
  nreg = (nreg & 0xFF00) | (V << 0);
 else if(A == 0x93)
  nreg = (nreg & 0x00FF) | ((V & 0x7F) << 8);  
 else if(A == 0x94)
 {
  voice_volume = V & 0xF;
  //printf("%02x\n", V);
 }
 else switch(A)
 {
  case 0x6A: HVoiceCtrl = V; break;
  case 0x6B: HVoiceChanCtrl = V & 0x6F; break;
  case 0x8F: SampleRAMPos = V; break;
  case 0x95: HyperVoice = V; break; // Pick a port, any port?!
  //default: printf("%04x:%02x\n", A, V); break;
 }
 WSwan_SoundUpdate();
}

uint8 WSwan_SoundRead(uint32 A)
{
 WSwan_SoundUpdate();

 if(A >= 0x80 && A <= 0x87)
 {
  int ch = (A - 0x80) >> 1;

  if(A & 1)
   return(period[ch] >> 8);
  else
   return(period[ch]);
 }
 else if(A >= 0x88 && A <= 0x8B)
  return(volume[A - 0x88]);
 else switch(A)
 {
  default:  /*printf("SoundRead: %04x\n", A);*/ return(0);
  case 0x6A: return(HVoiceCtrl);
  case 0x6B: return(HVoiceChanCtrl);
  case 0x8C: return(sweep_value);
  case 0x8D: return(sweep_step);
  case 0x8E: return(noise_control);
  case 0x8F: return(SampleRAMPos);
  case 0x90: return(control);
  case 0x91: return(output_control | 0x80);
  case 0x92: return((nreg >> 0) & 0xFF);
  case 0x93: return((nreg >> 8) & 0xFF);
  case 0x94: return(voice_volume);
 }
}


int32 WSwan_SoundFlush(int16 *SoundBuf, const int32 MaxSoundFrames)
{
	int32 FrameCount = 0;

	WSwan_SoundUpdate();

	if(SoundBuf)
	{
	 for(int y = 0; y < 2; y++)
	 {
	  sbuf[y]->end_frame(v30mz_timestamp);
 	  FrameCount = sbuf[y]->read_samples(SoundBuf + y, MaxSoundFrames, true);
	 }
	}

	last_ts = 0;

	return(FrameCount);
}

// Call before wsRAM is updated
void WSwan_SoundCheckRAMWrite(uint32 A)
{
 if((A >> 6) == SampleRAMPos)
  WSwan_SoundUpdate();
}

static void RedoVolume(void)
{
 WaveSynth.volume(2.5);
#ifdef WSRP_REMOVED
#else
 NoiseSynth.volume(2.5);
#endif
}

void WSwan_SoundInit(void)
{
 for(int i = 0; i < 2; i++)
 {
  sbuf[i] = new Blip_Buffer();

  sbuf[i]->set_sample_rate(0 ? 0 : 44100, 60);
  sbuf[i]->clock_rate((long)(3072000));
  sbuf[i]->bass_freq(20);
 }

 RedoVolume();
#ifdef WSRP_REMOVED
#else
 WaveSynth.treble_eq(blip_eq_t(wave_synth_treble_db, 0, 44100, 44100/2));
 NoiseSynth.treble_eq(blip_eq_t(0, 0, 44100, 44100 / 2));
#endif
}

void WSwan_SoundKill(void)
{
 for(int i = 0; i < 2; i++)
 {
  if(sbuf[i])
  {
   delete sbuf[i];
   sbuf[i] = NULL;
  }
 }

}

bool WSwan_SetSoundRate(uint32 rate)
{
 for(int i = 0; i < 2; i++)
  sbuf[i]->set_sample_rate(rate?rate:44100, 60);

#ifdef WSRP_REMOVED
#else
 WaveSynth.treble_eq(blip_eq_t(wave_synth_treble_db, 0, rate ? rate : 44100, rate ? rate / 2 : 44100 / 2));
 NoiseSynth.treble_eq(blip_eq_t(0, 0, rate ? rate : 44100, rate ? rate / 2 : 44100 / 2));
#endif

 return(true);
}

#ifdef WSRP_REMOVED
void WSwan_SoundStateAction(StateMem *sm, const unsigned load, const bool data_only)
{
 SFORMAT StateRegs[] =
 {
  SFVAR(period),
  SFVAR(volume),
  SFVAR(voice_volume),
  SFVAR(sweep_step),
  SFVAR(sweep_value),
  SFVAR(noise_control),
  SFVAR(control),
  SFVAR(output_control),
  SFVAR(HVoiceCtrl),
  SFVAR(HVoiceChanCtrl),

  SFVAR(sweep_8192_divider),
  SFVAR(sweep_counter),
  SFVAR(SampleRAMPos),
  SFVAR(period_counter),
  SFVAR(sample_pos),
  SFVAR(nreg),
  SFEND
 };

 MDFNSS_StateAction(sm, load, data_only, StateRegs, "PSG");

 if(load)
 {
  if(sweep_8192_divider < 1)
   sweep_8192_divider = 1;

  for(unsigned ch = 0; ch < 4; ch++)
  {
   period[ch] &= 0x7FF;

   if(period_counter[ch] < 1)
    period_counter[ch] = 1;

   sample_pos[ch] &= 0x1F;
  }
 }
}
#endif

void WSwan_SoundReset(void)
{
 memset(period, 0, sizeof(period));
 memset(volume, 0, sizeof(volume));
 voice_volume = 0;
 sweep_step = 0;
 sweep_value = 0;
 noise_control = 0;
 control = 0;
 output_control = 0;

 sweep_8192_divider = 8192;
 sweep_counter = 0;
 SampleRAMPos = 0;

 for(unsigned ch = 0; ch < 4; ch++)
  period_counter[ch] = 1;

 memset(sample_pos, 0, sizeof(sample_pos));
 nreg = 0;

 memset(sample_cache, 0, sizeof(sample_cache));
 memset(last_val, 0, sizeof(last_val));
 last_v_val = 0;

 HyperVoice = 0;
 last_hv_val[0] = last_hv_val[1] = 0;
 HVoiceCtrl = 0;
 HVoiceChanCtrl = 0;

 for(int y = 0; y < 2; y++)
  sbuf[y]->clear();
}

}
