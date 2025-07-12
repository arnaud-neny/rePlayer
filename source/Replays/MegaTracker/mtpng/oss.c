/***************************************************************************
                          oss.c  -  Open Sound System output driver
                             -------------------
    begin                : Thu Dec 9 1999
    copyright            : (C) 1999 by Ian Schmidt
    email                : ischmidt@cfl.rr.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// #include <stdio.h>
#include <stdlib.h>
// #include <math.h>
// #include <fcntl.h>
// #include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mtptypes.h"
#include "oss.h"

#define NULL 0

// now deal with where autoconf says OSS is
#ifdef HAVE_OSS
#include <sys/ioctl.h>

#ifdef HAVE_MACHINE_SOUNDCARD_H
#include <machine/soundcard.h>
#endif

#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#endif

#ifndef AFMT_S16_NE

#ifdef WORDS_BIGENDIAN
#define AFMT_S16_NE AFMT_S16_BE
#else
#define AFMT_S16_NE AFMT_S16_LE
#endif

#endif // no AFMT_S16_NE

#else // no HAVE_OSS
#ifndef AMIGA
// #error This version of MTP requires the Open Sound System (OSS)!
// #error Please write a driver for your machine! :-)
#endif
#endif

//#define MIXRATE	(44100)			// output sample rate
#define NUM_FRAGS_BROKEN    0x0004
#define NUM_FRAGS_NORMAL    0x0002
static int32 num_frags = NUM_FRAGS_NORMAL;
//#define OSS_FRAGMENT (0x000E | (num_frags<<16));  // 16k fragments (2 * 2^14).

/* Amiga - */
extern char *filename;
extern int frequency;
unsigned long totalsamples = 0;



// local variables

static int32 is_broken_driver;

/* Amiga - */
/*uint32 SPT = (MIXRATE / 50);*/
uint32 SPT;

uint16		curbpm;
volatile	int32 irq_tick;                 // timer tick

/* Amiga - i think this is the ugliest part, i will 
 * add proper memory allocation for samples in init
*/
/*static		int16 samples[MIXRATE *4];*/	// make sure we have enough RAM for worst-case scenario
static		int16 *samples = NULL;

// sequencer function pointer
void		(*GUS_Sequencer)(void);
// voice array

VoiceT		ghld[32];
// instrument array

SampHandT	handles[MAXSAMPLEHANDLES];
#if 0
int		audiofd;
#endif

// filedesc of /dev/dsp

// GUS_SetBPM - set tempo in BPM

void GUS_SetBPM(uint16 bpm)
{	
    curbpm = bpm;
    SPT = (bpm / 2.5);				// make into Hz
    SPT++;					// bump up slightly
/* Amiga - */
/*    SPT = (MIXRATE / SPT);*/
    SPT = (frequency / SPT);
}

// GUS_GetBPM - returns tempo in BPM

uint16 GUS_GetBPM(void)
{	
    return (curbpm);
}

// GUS_Update - timer callback routine: runs sequencer and mixes sound

void GUS_Update(void)
{	
    register double accumL, accumR;
    static	    uint32 index, intindex;
    static	    int32 d1, d2, d3;
    static	    int8 sample;
    int32	    i;

    GUS_Sequencer();
    irq_tick++;
    // by default, assume 50 hz and create SPT stereo
    // samples per clock tick

    index = 0;
    accumL = accumR = 0;

    // now software mix the voices
    while (index < SPT *2)
    {
	for (i = 0; i < 14; i++)
	{
	    if (ghld[i].kick != 0)
	    {
		ghld[i].offset = ghld[i].start << 16;
		ghld[i].kick = 0;
		ghld[i].data = handles[ghld[i].handle].data;
	    }

	    ghld[i].offset += ghld[i].frq;
	    intindex = ghld[i].offset >> 16;

	    if (intindex >= ghld[i].repend)
	    {
			if (ghld[i].flags & SF_LOOP)
			{
			    intindex -= ghld[i].repend; // allow accumulator to work around the loop
			    intindex += ghld[i].reppos;
			}
			else 
			{
			    intindex = ghld[i].repend;
			}
			ghld[i].offset &= 0xffff;	// keep fraction intact for better looping
			ghld[i].offset |= intindex << 16;
	    }

	    // interpolate the sample's value
	    d1 = ghld[i].data[intindex];
	    d2 = ghld[i].data[intindex + (ghld[i].frq >> 16)];
	    d3 = (d1 << 16) + ((d2 - d1) *(ghld[i].offset & 
					   0xffff));

	    sample = (int8)(d3 >> 16);
	    if (ghld[i].pan)
	    {
			accumL += sample *ghld[i].vol;
	    }
	    else 
	    {
			accumR += sample *ghld[i].vol;
	    }
	}
	// (for)

	// now convert the 32-bit samples to 16-bit samples

	samples[index++] = accumL / 4.0;
	samples[index++] = accumR / 4.0;
	accumL = accumR = 0;
    }
    // (while)

    // output the generated samples
#if 0
    write(audiofd, samples, SPT * 4);
#endif
}

/* Amiga - */
void GUS_UpdateFake(void)
{	
    GUS_Sequencer();
    irq_tick++;
    // by default, assume 50 hz and create SPT stereo
    // samples per clock tick

    // now software mix the voices
    totalsamples += SPT;
}
// checks the play position to see if we should trigger another update

void GUS_TimeCheck(void)
{
#ifndef AMIGA
#if 0
	audio_buf_info info;
	fd_set ourset;
	
	if (!is_broken_driver)
	{	// normal case, use the ioctl
    		ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info);
		
//			printf("info.bytes = %d (SPT * 4 = %ld)\n", info.bytes, (SPT*4));
    		if (info.bytes >= (SPT * 4))
    		{
//				printf("GUS_Update: SPT*4 bytes\n");
				fflush(stdout);
				GUS_Update();
//				printf("leaving GUS_Update\n");
				fflush(stdout);
    		}
    		else 
    		{
				usleep(500);
    		}
	}
	else	// use select()
	{
		FD_ZERO(&ourset);
		FD_SET(audiofd, &ourset);
		if (select(audiofd + 1, NULL, &ourset, NULL, NULL) >= 0)
		{
			GUS_Update();
			return;
		}
		usleep(500);	// select bombed, so sleep a little
	}
#endif
#else
	GUS_Update();
/* Amiga - why ?*/
/*	usleep(500);*/
#endif
}

// GUS_Load: sets up a sample for use by the player

int16 GUS_Load(char *samp, uint32 length, uint32 loopstart, uint32
	       loopend, uint16 flags, uint8 volume)
{	
	int16	    handle;
	uint8	    *lpSampMem;

	for (handle = 0; handle < MAXSAMPLEHANDLES; handle++)
	{
		if (handles[handle].inuse == 0)
		{
		    break;				// we found it!
		}
	}
	lpSampMem = malloc(length + 2);
	memcpy(lpSampMem, samp, length);

// set up loops to work properly, given that we linearly
// interpolate the samples

	if (flags & SF_LOOP)
	{
		lpSampMem[length] = lpSampMem[loopstart];
		lpSampMem[length + 1] = lpSampMem[loopstart + 1];
	}
	else 
	{
		lpSampMem[length] = lpSampMem[length - 1];
		lpSampMem[length + 1] = lpSampMem[length];
	}

	handles[handle].data = (unsigned char *) lpSampMem;
	handles[handle].length = length;
	handles[handle].loopstart = loopstart;
	handles[handle].loopend = loopend;
	handles[handle].flags = 0;
	handles[handle].flags |= (flags & (SF_LOOP | SF_16BITS));
	handles[handle].inuse = 1;
	handles[handle].volume = volume;

	return (handle);
}

// GUS_SetSampleVolume - sets the default volume of an already-loaded
// sample.  Useful for backpatching by the loader.

void GUS_SetSampleVolume(int16 handle, uint8 volume)
{
	handles[handle].volume = volume;
}

// GUS_GetSampleVolume - returns the default volume of an already-loaded
// sample.

uint8 GUS_GetSampleVolume(int16 handle)
{
	return (handles[handle].volume);
}

// GUS_UnLoad - unloads a sample, it cannot be used by the player anymore
// do NOT call during playback.  system will crash.

void GUS_UnLoad(int16 handle)
{	
    free(handles[handle].data);
    handles[handle].inuse = 0;
    handles[handle].flags = 0;
}

/* Amiga */
void freesamplesmem(void)
{
  if (samples)
    free(samples);
}

// GUS_Init - inits the output device and our global state

int16 GUS_Init(void)
{	

/* Amiga - */
  SPT = (frequency / 50);

/* Amiga - as i said memory allocation is a must, but 
 * we have to deallocate it as well, so we will use atexit() 
*/
//   if ((atexit(freesamplesmem)))
//   {
// 	perror("atexit failure:");
// 	printf("Cant hookup 'freesamplesmem()' to 'atexit()'!\n");
// 	exit (5);
//   }

  if ((samples = (int16 *)malloc((frequency * 4) * sizeof(int16))) == NULL)
  {
// 	perror("no memory");
// 	printf("There is no memory for samples.\n");
// 	exit (5);
	return 0;
  }


#ifndef AMIGA
#if 0
    int i, format, stereo, rate, fsize;
    audio_buf_info info;

    audiofd = open("/dev/dsp", O_WRONLY, 0);
    if (audiofd == - 1)
    {
			perror("/dev/dsp");
			printf("ERROR: unable to open /dev/dsp.  Aborting.\n");
			exit(-1);
    }

    // reset things
    if (ioctl(audiofd, SNDCTL_DSP_RESET, 0) == -1)
	{
		perror("SNDCTL_DSP_RESET\n");
		exit(-1);
	}

    // check for broken ioctl a la LinuxPPC
/*    if (ioctl(audiofd, SNDCTL_DSP_GETOSPACE, &info) == -1)
    {
			num_frags = NUM_FRAGS_BROKEN;
			printf("Driver doesn't support GETOSPACE. (LinuxPPC?)\n");
			printf("Using select() throttling method instead.\n");
			is_broken_driver = 1;
    }
    else
    {
			num_frags = NUM_FRAGS_NORMAL;
			is_broken_driver = 0;
    }*/
	num_frags = NUM_FRAGS_NORMAL;
	is_broken_driver = 0;

    // set the buffer size we want
    fsize = OSS_FRAGMENT;
    if (ioctl(audiofd, SNDCTL_DSP_SETFRAGMENT, &fsize) == - 1)
    {
			perror("SNDCTL_DSP_SETFRAGMENT");
			exit(-1);
    }

    // set 16-bit output
    format = AFMT_S16_NE;	// 16 bit signed "native"-endian
    if (ioctl(audiofd, SNDCTL_DSP_SETFMT, &format) == - 1)
    {
			perror("SNDCTL_DSP_SETFMT");
			exit(-1);
    }

    // now set stereo
    stereo = 1;
    if (ioctl(audiofd, SNDCTL_DSP_STEREO, &stereo) == - 1)
    {
			perror("SNDCTL_DSP_STEREO");
			exit(-1);
    }

    // and the sample rate
/* Amiga - */
/*    rate = MIXRATE;*/
    rate = frequency;
	printf("rate = %ld\n", rate);
    if (ioctl(audiofd, SNDCTL_DSP_SPEED, &rate) == - 1)
    {
			perror("SNDCTL_DSP_SPEED");
			exit(-1);
    }

    // and make sure that did what we wanted
    ioctl(audiofd, SNDCTL_DSP_GETBLKSIZE, &fsize);
    printf("Fragment size: %d\n", fsize);
    for (i = 0; i < MAXSAMPLEHANDLES; i++)
    {
			handles[i].inuse = 0;
			handles[i].data = NULL;                 // force segfault if used
    }
#endif
#else
/* Amiga - */
        audiofd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 444);
    if (audiofd == - 1)
    {
			perror("Output:");
			printf("ERROR: unable to open output file.  Aborting.\n\n");
			exit(5);
    }
    num_frags = NUM_FRAGS_NORMAL;
#endif
    return (1);
}

void GUS_Exit(void)
{	
    int16	    handle;

    // wait until data is done playing
#ifndef AMIGA
#if 0
    ioctl(audiofd, SNDCTL_DSP_SYNC, 0);
#endif
#endif
#if 0
	close(audiofd);
#endif
    for (handle = 0; handle < MAXSAMPLEHANDLES; handle++)
    {
	if (handles[handle].inuse != 0)
	{
	    GUS_UnLoad(handle);
	}
    }
	freesamplesmem();
}

static uint32 GetMaskFromSize(uint32 len)
//-----------------------------------
{
	uint32 n = 2;

	while (n <= len) n <<= 1;
	return ((n >> 1) - 1);
}

void GUS_PlayStart(void)
{	
    int16	    i;

// init all channels
	for (i = 0; i < 14; i++)
	{
		ghld[i].flags = 0;
		ghld[i].handle = 0;
		ghld[i].kick = 0;
		ghld[i].active = 0;
		ghld[i].frq = 0x10000;
		ghld[i].vol = 64;
	
		if (i & 0x1)
		{
		    ghld[i].pan = 0;
		}
		else 
		{
		    ghld[i].pan = 1;
		}

		ghld[i].data = handles[0].data;
		ghld[i].offset = (handles[0].length - 1) << 16;
	}
	GUS_SetBPM(125);

	// new strategy: try to pre-mix num_frags fragments
#if 0
	for (i = 0; i < num_frags; i++)
	{
		GUS_Update();
	}
#endif
}

void GUS_PlayStop(void)
{	
}

void GUS_VoiceSetVolume(uint8 voice, uint8 vol)
{	
    ghld[voice].vol = vol;
}

uint8 GUS_VoiceGetVolume(uint8 voice)
{	
    return (ghld[voice].vol);
}


void GUS_VoiceSetFrequency(uint8 voice, uint32 frq)
{	
    uint32	    fracFreq;
    double	    intr;

    // use a float to avoid overflow

/* Amiga - */
/*    intr = (frq / (float) MIXRATE) *65536.0;*/
    intr = (frq / (float) frequency) *65536.0;
    fracFreq = (uint32) intr;
    // now we have a nice 16.16 fixed point play rate
    ghld[voice].frq = fracFreq;
    ghld[voice].hzFrq = frq;
}

void GUS_VoiceSetPanning(uint8 voice, uint8 pan)
{	
    ghld[voice].pan = pan;
}

void GUS_VoicePlay(uint8 voice, int16 handle)
{
	GUS_VoicePlayEx(voice, handle, 0, handles[handle].length, 0,
			handles[handle].length-1, handles[handle].flags);
}

void GUS_VoicePlayEx(uint8 voice, int16 handle, uint32 start, uint32 size, 
		   uint32 reppos, uint32 repend, uint16 flags)
{	
    if (start >= size)
    {
	return;
    }
    if (flags & SF_LOOP)
    {
	if (repend > size)
	{
	    repend = size;
	}
    }
    // modify loop and 16-bit flags only, don't touch LOCHW or CLONE
    ghld[voice].flags &= ~(SF_LOOP | SF_16BITS);
    ghld[voice].flags |= (flags & (SF_LOOP | SF_16BITS));
    ghld[voice].handle = handle;
    ghld[voice].start = start;
    ghld[voice].size = size;
    ghld[voice].reppos = reppos;
    ghld[voice].repend = repend;
    ghld[voice].offset = repend << 16;
    ghld[voice].kick = 1;
}

uint16 GUS_IsInstValid(int16 handle)
{
	if (handles[handle].inuse)
	{
		return 1;
	}

	return 0;
}

int16* GUS_GetSamples(void)
{
	return samples;
}