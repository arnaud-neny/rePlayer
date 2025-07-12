/***************************************************************************
                          oss.h  -  description
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

#ifndef _OSS_H_
#define _OSS_H_

// sample and track flags

#define SF_NONE			0				// no flags
#define SF_LOOP			1				// sample is looping
#define SF_16BITS		2				// sample is 16 bits, otherwise 8

// voice structure

typedef struct
{
	uint8 kick;			// =1 -> sample has to be restarted
	uint8 active;		// =1 -> sample is playing
	uint16 flags; 		// 16/8 bits looping/one-shot
	int16 handle;		// identifies the sample
	uint32 start;		// start index
	uint32 size;			// samplesize
	uint32 reppos;		// loop start
	uint32 repend;		// loop end
	uint32 frq;			// current frequency, in fixed point 16.16
	uint32 hzFrq;		// current frequency, in Hz.
	uint8 vol;			// current volume
	uint8 dvol;			// destination volume
	uint8 pan;			// current panning position
	uint32 offset;		// 16.16 offset
	int8 *data;			// sample data we're currently playing
} VoiceT;

// sample info struct

typedef struct
{
	unsigned char *data;	// pointer to sample data
	uint32 length;			// length of sample
	uint32 loopstart;		// offset to loopstart
	uint32 loopend;			// offset of loopend
	uint16 flags;			// flags
	uint8 volume;
  	uint8 inuse;
} SampHandT;

// global defs

extern void (*GUS_Sequencer)(void);

extern uint16 dskick;

// max number of instruments a driver should support

#define MAXSAMPLEHANDLES 128

// function protos

void GUS_Update(void);
void GUS_UpdateFake(void);
int16 GUS_Load(char *samp, uint32 length, uint32 loopstart, uint32 loopend, uint16 flags, uint8 volume);
void GUS_UnLoad(int16 handle);
void GUS_SetSampleVolume(int16 handle, uint8 volume);
uint8 GUS_GetSampleVolume(int16 handle);
int16 GUS_Init(void);
void GUS_Exit(void);
void GUS_PlayStart(void);
void GUS_PlayStop(void);
int16 GUS_IsThere(void);
void GUS_VoiceSetVolume(uint8 voice, uint8 vol);
uint8 GUS_VoiceGetVolume(uint8 voice);
void GUS_VoiceSetFrequency(uint8 voice, uint32 frq);
void GUS_VoiceSetPanning(uint8 voice, uint8 pan);
void GUS_VoicePlay(uint8 voice, int16 handle);
void GUS_VoicePlayEx(uint8 voice, int16 handle, uint32 start, uint32 size, uint32 reppos, uint32 repend, uint16 flags);
void GUS_TimeCheck(void);
void GUS_SetBPM(uint16 bpm);
uint16 GUS_GetBPM(void);
uint16 GUS_IsInstValid(int16 handle);
int16* GUS_GetSamples(void);
#endif
