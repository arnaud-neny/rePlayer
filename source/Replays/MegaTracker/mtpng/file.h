/***************************************************************************
                          file.h  -  Header for MTPng file handling
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

#ifndef __FILE_H__
#define __FILE_H__

// constants

// function definitions

void FILE_Init(void);
int8 FILE_Open(char *fileName);
void FILE_Close(void);
uint32 FILE_GetLength(void);
void FILE_Seek(uint32 position);
void FILE_Seekrel(uint32 position);
uint32 FILE_GetPos(void);
uint32 FILE_Read(uint32 length, uint8 *where);
uint8 FILE_Readbyte(void);
uint16 FILE_Readword(void);
uint16 FILE_ReadwordRev(void);
uint32 FILE_Readlong(void);
int8 FILE_FindCaseInsensitive(int8 *inpath, int8 *file, int8 *outfile);

#endif
