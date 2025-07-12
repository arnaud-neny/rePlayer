/***************************************************************************
                          ntgs.h - protos for the NoiseTracker GS "object"
                             -------------------
    begin                : Sat Jan 1 2000
    copyright            : (C) 2000 by Ian Schmidt
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

#ifndef __NTGS_H__
#define __NTGS_H__

// NTGS 1.x header (not as stored in file)

typedef struct
{
	int16 version;
	char signature[14];		// "FTA MODULEFILE"
	uint16 tempo;
	uint16 loopmode;
	uint16 rawmode;
	uint16 numpos;
	uint16 numblks;
	uint16 numDOCinsts;
	uint16 numRAMinsts;
	uint16 mode4k;
	uint16 docpgsize;
	uint16 rampgsize;
	uint16 numramtrks;
	uint16 blksize;
	uint16 totaltrks;
	uint16 musicsizeL;		// more gymnastics due to packing
	uint16 musicsizeH;
	uint16 reserved2;
	uint8 padding[16];
	uint16 stereo[15];
	uint16 blklist[256];
} NTGS1Head;

// NTGS 1.x instrument (not as stored in file)

typedef struct
{
	char name[16];			// inst. name
	uint32 instsize;		// inst. size
	uint32 instpos;			// inst. wave offset in the file
	uint16 instvol;			// more ugh
	uint16 insttype;		// ...
	uint8 instleft[2];		// DOC modes for left
	uint8 instright[2];		// DOC modes for right
	uint16 instSMode;
	uint32 instWaveOfs;		// offset in file of our wave data
} NTGS1Inst;				

// global variables

extern MTPInputT ntgs_plugin;

#endif
