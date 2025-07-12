/***************************************************************************
                          file.c  -  file I/O handling and utilities
                             -------------------
    begin                : Wed Dec 8 1999
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

// headers

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mtptypes.h"
#include "file.h"

// globals and statics

static int8 _IsOpen;
static FILE *_file;

// functions

void FILE_Init(void)
{
	_IsOpen = 0;
}

int8 FILE_Open(char *fileName)
{
//	printf("FILE_Open('%s')\n", fileName);

	_file = fopen(fileName, "rb");	// read-only, binary
	
	if (_file == (FILE *)NULL)
	{
		return(0);
	}
	
	_IsOpen = 1;
	return(1);	// indicate success
}

uint32 FILE_GetLength(void)
{
	uint32 curPos, length;
	
	if (!_IsOpen)	// if no file open, invalid operation
	{
		return(0);
	}
	
    curPos = ftell(_file);          // save current position	
    fseek(_file, 0, SEEK_END);      // seek to end
    length = ftell(_file);          // find end's position
    fseek(_file, curPos, SEEK_SET); // restore original position

    return(length);
}

void FILE_Seek(uint32 position)
{

	if (!_IsOpen)	// if no file open, invalid operation
	{
		return;
	}

	fseek(_file, position, SEEK_SET);
}


void FILE_Seekrel(uint32 position)
{
	fseek(_file, position, SEEK_CUR);
}


uint32 FILE_GetPos(void)
{
	return ftell(_file);
}

uint32 FILE_Read(uint32 length, uint8 *where)
{
	if (!_IsOpen)	// if no file open, invalid operation
	{
		return(0);	
	}
	
	return(fread(where, length, 1, _file));
}

uint8 FILE_Readbyte(void)
{
	uint8 inbyte;

	if (!_IsOpen)	// if no file open, invalid operation
	{
		return(0);	
	}	

	fread(&inbyte, 1, 1, _file);
	
	return(inbyte);
}

uint16 FILE_Readword(void)
{
	uint16 inword;
	#ifdef WORDS_BIGENDIAN
	uint8 a, b;
	#endif

	fread(&inword, 2, 1, _file);
	
	#ifdef WORDS_BIGENDIAN
	a = inword>>8;
	b = inword & 0xff;
	inword = (a | (b<<8));
	#endif
	
	return(inword);
}

uint16 FILE_ReadwordRev(void)
{
	uint16 inword;
	#ifndef WORDS_BIGENDIAN
	uint8 a, b;
	#endif

	fread(&inword, 2, 1, _file);
	
	#ifndef WORDS_BIGENDIAN
	a = inword>>8;
	b = inword & 0xff;
	inword = (a | (b<<8));
	#endif
	
	return(inword);
}

uint32 FILE_Readlong(void)
{
	uint32 inlong;
	#ifdef WORDS_BIGENDIAN
	uint8 a, b, c, d;
	#endif
	
	fread(&inlong, 4, 1, _file);
	
	#ifdef WORDS_BIGENDIAN
	a = (inlong & 0xff);
	b = (inlong & 0xff00)>>8;
	c = (inlong & 0xff0000)>>16;
	d = (inlong & 0xff000000)>>24;
	inlong = (d | c<<8 | b<<16 | a<<24);
	#endif
	
	return(inlong);
}

void FILE_Close(void)
{
	if (_IsOpen)
	{
		fclose(_file);
	}
}

int8 FILE_FindCaseInsensitive(int8 *inpath, int8 *file, int8 *outfile)
{
	DIR *dir;
	struct dirent *ent;

//	printf("FILE_FindCaseInsensitive: looking for [%s] in path [%s]\n", file, inpath);
	
	if (!(dir = opendir(inpath)))
	{
		// if can't open directory, we're done
		return 0;
	}

	while ((ent = readdir(dir)))
	{
//		printf("comparing: [%s] vs [%s]\n", ent->d_name, file);
		if (!strcasecmp(ent->d_name, file))
		{
			// got it!
//			printf("found match: %s\n", ent->d_name);
	 		strncpy(outfile, ent->d_name, 32);
			closedir(dir);
			return 1;
		}
	}

	// ran out of directory
	closedir(dir);
	return 0;
}
