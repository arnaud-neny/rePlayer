/***************************************************************************
                          ssmt.h  -  description
                             -------------------
    begin                : Sat Dec 18 1999
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

#ifndef __SSMT_H__
#define __SSMT_H__

typedef struct
{
  int8 name[23];
  uint16 volume;
} SSInst;

typedef struct
{
  int8 signature[6];      // SONGOK or IAN92a signature
  uint16 blocksize;
  uint16 tempo;
  SSInst insts[15];       // instruments
  uint16 blocklen;
  int8 blocklist[128];
  uint16 stereo[15];
} SSHead;


// various structures used to load instruments, etc.

// instSeg

typedef struct
{
  int8 breakpoint;
  int8 inc[2];
} InstSeg; 

// wavelist

typedef struct
{
  int8 topkey;
  uint8 waveAddress;
  uint8 waveSize;
  int8 docMode;
  int16 relPitch;
} WaveList;

// INST chunk, after the IName

typedef struct
{
  uint16 samplenum;	// (offset 0)
  int8 env[3*8];	// (offset 2)
  int8 releaseSeg;	// (offset 26)
  int8 priorityInc;	// (27)
  int8 pbRange;		// (28)
  int8 vibDepth;	// (29)
  int8 vibSpeed;	// (30)
  int8 updateRate;	// (31)
  int8 aWaveCount;	// (32)
  int8 bWaveCount;	// (33)
  WaveList aWaveList;	// (offset 34)
  WaveList bWaveList;
} INSTChk;

// WAVE chunk, after the wave name

typedef struct
{
  int16 waveSize;
  int16 numSamples;
  int16 Location;
  int16 Size;
  uint32 origFreq;
  uint32 sampRate;
  int8 wavedata[1];
} WAVEChk;

// effects
#define FX_ARP		0	// arpeggiato
#define FX_PITCHUP 	1	// pitchslide
#define FX_PITCHDN 	2	// pitchslide
#define FX_VOL		3	// volume set
#define FX_VIB		4	// vibrato
#define FX_VOLUP	5	// volume up
#define FX_VOLDN	6	// volume down
#define FX_PORTA	12	// portamento
#define FX_TEMPO	15	// set tempo

// Global variables

extern MTPInputT ssmt_plugin;

#endif
