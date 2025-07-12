/***************************************************************************
                          smus.h - protos for the SMUS "object"
                             -------------------
    begin                : Mon Feb 11 2000
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

#ifndef __SMUS_H__
#define __SMUS_H__

// Generic IFF chunk header

typedef struct
{
	char id[4];	// chunk ID
	uint32 len;	// chunk length
	char data[1];	// chunk data
} IFFHead;

// SMUS info struct

typedef struct
{
	uint16 tempo;			// tempo, in 128ths of a quarter note / minute
	uint8 volume;			// global volume
	uint8 tracks;			// header # of tracks
	uint16 realtrks;		// # of trk chunks found
	uint8 *trkdata[32];		// track data pointer
	uint16 trkofs[32];		// trk. offset
	uint32 trklen[32];		// trk. length
	uint16 trkvol[32];		// track volumes
	uint16 trkdelta[32];		// # of ticks for 'delta' in tracks
	uint16 trkinst[32];		// inst. # for track
} SMUSInfo;

// global variables

extern MTPInputT smus_plugin;

#endif
