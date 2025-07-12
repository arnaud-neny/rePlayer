/***************************************************************************
                          baseclass.h  -  contains base classes for
                                          various MTPng objects.
                             -------------------
    begin                : Sat Mar 25 2000
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

#ifndef _MTP_BASECLASS_H_
#define _MTP_BASECLASS_H_

// format type base class

typedef struct 
{
	// functions
	int8 *(*TypeName)(void);	   	// returns file type's name
	int8 *(*VerString)(void);	   	// returns file type's version string
	int8 (*IsFile)(char *filename);		// checks if filename is the type we handle
	int8 (*LoadFile)(char *filename);	// loads file
	void (*PlayStart)(void);		// starts playback
	void (*Sequencer)(void);		// called by mixer depending on BPM set
	int8 (*GetSongStat)(void);		// returns if song is done
	void(*ResetSongStat)(void);		// reset the song if it's done
	void (*SetScroll)(int8 scval);		// sets if plugin should printf scrolly output
	void (*SetOldstyle)(int8 osval);	// sets if plugin should use oldstyle f/x processing
	void (*GetCell)(char *out, int8 track);	// gets cell for the current line specified by "track"
	int8 (*GetNumChannels)(void);		// returns number of channels
	void (*Close)(void);			// close the current plugin
} MTPInputT;

#endif	// _MTP_BASECLASS_H_
