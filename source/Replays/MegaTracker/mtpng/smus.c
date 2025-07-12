/***************************************************************************
                          smus.c  -  Will Harvey modified SMUS player
                             -------------------
    begin                : Mon Feb 21 2000
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// #include <stdio.h>
#include <stdlib.h>
// #include <string.h>
// #include <ctype.h>

#include "mtptypes.h"
#include "file.h"
#include "smus.h"
#include "oss.h"
#include "tabprot.h"

#define MTP_DEBUG 0	// set to 1 for verbose debugging of this module

uint16 baseticks[64] = 
{    // wh   hlf  qt   ei  sx  32  64t 128th
	384, 192, 96,  48, 24, 12, 6,  3, 	// normal duration
	576, 288, 144, 72, 36, 18, 9,  5,	// dotted (3/2ths normal duration)
	0,   0,   0,   0,  0,  0,  0,  0,	// 2/3rds
	384, 192, 96,  48, 24, 12, 6,  3, 	// dotted 2/3rd (normal duration)
	0,   0,   0,   0,  0,  0,  0,  0,	// 5/6ths
	0,   0,   0,   0,  0,  0,  0,  0,	// dotted 5/6ths (15/12ths)
	0,   0,   0,   0,  0,  0,  0,  0,	// 6/7ths
	0,   0,   0,   0,  0,  0,  0,  0,	// dotted 6/7ths (9/7ths)
};

static SMUSInfo sminfo;				// file info
static int16 songstat;
static int16 playerbpm;	// BPM tempo that sequencer wants
static uint8 track, trkVU[16];
static int16 handles[128];	// inst. handles
static int8 _scroll;

// local function protos

static int8 *SMUS_TypeName(void);
static int8 *SMUS_VerString(void);
static int8 SMUS_IsFile(char *filename);
static int8 SMUS_LoadFile(char *filename);
static void SMUS_PlayStart(void);
static void SMUS_Sequencer(void);
static void SMUS_DoFXTick(void);
static int8 SMUS_GetSongStat(void);
static void SMUS_ResetSongStat(void);
static void SMUS_SetScroll(int8 scval);
static void SMUS_SetOldstyle(int8 scval);
static int8 SMUS_GetNumChannels(void);
static void SMUS_Close(void);

static uint32 _flipLongEnd(uint32 in);

// global variables
MTPInputT smus_plugin =
{
	SMUS_TypeName,
	SMUS_VerString,
	SMUS_IsFile,
	SMUS_LoadFile,
	SMUS_PlayStart,
	SMUS_Sequencer,
	SMUS_GetSongStat,
	SMUS_ResetSongStat,
	SMUS_SetScroll,
	SMUS_SetOldstyle,
	NULL,				// no GetCell routine here
	SMUS_GetNumChannels,
	SMUS_Close
};

// extern we use (ugh, clean me!)
void _FlipData(uint8 *input, uint32 size);

// global functions

static int8 SMUS_GetSongStat(void)
{
	return songstat;
}

static void SMUS_ResetSongStat(void)
{
	if (songstat == 0)
	{
		SMUS_PlayStart();
	}
}

static int8 SMUS_GetNumChannels(void)
{
	return sminfo.tracks;
}

static void SMUS_Close(void)
{
	int32 i;
	for (i = 0; i < sminfo.realtrks; i++)
		free(sminfo.trkdata[i]);
}

// start playback

static void SMUS_PlayStart(void)
{
	int32 i;
	float a, b;

	printf("smus play start\n");

	for (i=0; i<sminfo.tracks; i++)
	{
		sminfo.trkofs[i] = 0;
		sminfo.trkinst[i] = i;
		sminfo.trkvol[i] = 0x7f;
		sminfo.trkdelta[i] = 0;
	}

	// generate tick table based on "normal" duration notes
	for (i=0; i<8; i++)
	{
		a = (float)baseticks[i];

		// dotted (3/2ths)
		b = (a*3.0)/2.0;
		baseticks[8+i] = (uint16)b;

		// 2/3rds
		b = (a*2.0)/3.0;
		baseticks[16+i] = (uint16)b;

		// dotted 2/3rds
		b = (b*3.0)/2.0;
		baseticks[24+i] = (uint16)b;

		// 5/6ths
		b = (a*5.0)/6.0;
		baseticks[32+i] = (uint16)b;

		// dotted 5/6ths
		b = (b*3.0)/2.0;
		baseticks[40+i] = (uint16)b;

		// 6/7ths
		b = (a*6.0)/7.0;
		baseticks[48+i] = (uint16)b;

		// dotted 6/7ths
		b = (b*3.0)/2.0;
		baseticks[56+i] = (uint16)b;
	}

	// and of course set our sequencer
	GUS_Sequencer = SMUS_Sequencer;

	printf("initial tempo = %ld\n", sminfo.tempo/32);
	playerbpm = (sminfo.tempo/32);	// heh, good luck windoze
	GUS_SetBPM(playerbpm);
	songstat = 1;
}

static void noteToName(int16 note, char *out)
{
	char oct[4];

	switch (note)
	{
		case 0:
			strcpy(out, "---");
			break;
	        case 0x81:
			strcpy(out, "NXT");
			break;
	        case 0x80:
			strcpy(out, "STP");
			break;
		default:
			switch((note-1)%12)
			{
				case 0:
					strcpy(out, "C-");
					break;
				case 1:
					strcpy(out, "C#");
					break;
				case 2:
					strcpy(out, "D-");
					break;
				case 3:
					strcpy(out, "D#");
					break;
				case 4:
					strcpy(out, "E-");
					break;
				case 5:
					strcpy(out, "F-");
					break;
				case 6:
					strcpy(out, "F#");
					break;
				case 7:
					strcpy(out, "G-");
					break;
				case 8:
					strcpy(out, "G#");
					break;
				case 9:
					strcpy(out, "A-");
					break;
				case 10:
					strcpy(out, "A#");
					break;
				case 11:
					strcpy(out, "B-");
					break;
			}
			sprintf(oct, "%d", ((note-1)/12));
			strcat(out, oct);
			break;
	}
}

static void SMUS_Sequencer(void)
{
	uint8 cmd, data, cnotes;
	uint16 voice, tno, pf, tp;
	char nn[8];

	if (GUS_GetBPM() != playerbpm)
	{
		GUS_SetBPM(playerbpm);
	}

	tno = 0;
	pf = 0;
	tp = -1;
	for (track = 0; track<sminfo.tracks; track++)
	{
		tno++;
		cnotes = 0;
		if ((sminfo.trkdelta[track] == 0) && (sminfo.trkofs[track] < sminfo.trklen[track]))
		{
chord:
			cmd = sminfo.trkdata[track][sminfo.trkofs[track]];
			data = sminfo.trkdata[track][sminfo.trkofs[track]+1];

//			printf("[%01d/%04d] cmd: %x data: %x\n", track, sminfo.trkofs[track], cmd, data);
//			fflush(stdout);

			sminfo.trkofs[track] += 2;

			if (cmd >= 0x80)
			{
				switch (cmd)
				{
					case 0x80:	// rest
//						GUS_VoiceSetVolume(track*4, 0);	// turn off track's volume
//						GUS_VoiceSetVolume((track*4)+1, 0);	// turn off track's volume
//						GUS_VoiceSetVolume((track*4)+2, 0);	// turn off track's volume
//						GUS_VoiceSetVolume((track*4)+3, 0);	// turn off track's volume
						sminfo.trkdelta[track] = baseticks[data&0x3f];
						break;

					case 0x81:	// set instrument
						if (data != 0)
						{
							sminfo.trkinst[track] = data-1;
							printf("track %d to inst %d\n", track, sminfo.trkinst[track]);
						}
						goto chord;
						break;

					case 0x84:	// set volume
						sminfo.trkvol[track] = data;
												// no break intentional
					case 0x82:	// set time signature
					case 0x83:	// set key signature
					case 0x85:	// set MIDI channel
					case 0x86:	// set MIDI program
					default:	// DMCS GS uses unknown commands, beware
						goto chord;	// non-rest meta events have no duration
						break;
				}
			}
			else	// regular note
			{
				// save the last voice for the "important" note
				if ((cnotes == 3) && (data & 0x80))
				{
					goto chord;
				}

				voice = (track*4) + cnotes;

				GUS_VoiceSetVolume(voice, sminfo.trkvol[track]);
				GUS_VoiceSetPanning(voice, 64);
				GUS_VoiceSetFrequency(voice, FreqTable[cmd]*51);
				GUS_VoicePlay(voice, sminfo.trkinst[track]);
				
				pf = 1;
				noteToName(cmd, nn);
				if (tp != track)
				{
					tp = track;
					printf("t%02d: ", track);
				}
				printf("%s", nn);

				trkVU[track] = sminfo.trkvol[track];

				sminfo.trkdelta[track] = baseticks[data&0x3f];

				// check if this note is part of a chord
				if (data & 0x80)
				{
					printf("c ");
					cnotes++;
					goto chord;
				}
				else
				{
					printf(" ");
					cnotes = 0;
				}
			}
		}
		else	// nonzero delta or track done, subtract 1 tick
		{
			if (sminfo.trkofs[track] < sminfo.trklen[track])
			{
				sminfo.trkdelta[track]--;
			}
		}
	}

	if (pf)
	{
		printf("\n");
	}

	// if all tracks are done, end the song
	if (!tno)
	{
		songstat = 0;
	}


}		

static int8 SMUS_IsFile(char *filename)
{
	unsigned char fhead[32];
	int8 ok;
	int32 instofs, wavofs, docofs, fullsize;
	char *instptr, IDsave, *iwaveptr, *smuptr;
	char ibase[10], ipath[512];

	ok = 0;

	if (!FILE_Open(filename))
	{
		return 0;
	}

	// get the first 32 bytes (more than enough)
	FILE_Seek(0);
	FILE_Read(32, &fhead[0]);

	if ((fhead[0] == 'F') && (fhead[1] == 'O') && (fhead[2] == 'R') && (fhead[3] == 'M'))
	{
		// it's IFF at least, check formtype at offset 8
		if ((fhead[8] == 'S') && (fhead[9] == 'M') && (fhead[10] == 'U') && (fhead[11] == 'S'))
		{
			ok = 1;
		}
	}

	FILE_Close();

	return ok;
}

static int8 SMUS_LoadFile(char *filename)
{
	IFFHead chk; 
	uint32 ctop;
	char name[64], barepath[512+32], casename[512+32], ibase[32], obase[32];
	char fullname[1024];
	int32 i, j, exit = 0;
	uint8 *instdata;
	
	// open the file once again
	FILE_Open(filename);

	// get first chunk pointer
	FILE_Seek(12);
		   
	FILE_Read(4, (uint8 *)&chk.id);
	#ifdef WORDS_BIG_ENDIAN
	chk.len = FILE_Readlong();
	#else
	chk.len = _flipLongEnd(FILE_Readlong());
	#endif
	if (chk.len & 1)
	{
		chk.len++;
	}

	sminfo.realtrks = 0;

	ctop = 12;
	while (!exit)
	{
		if ((chk.id[0] == 'S') && (chk.id[1] == 'H') && (chk.id[2] == 'D') && (chk.id[3] == 'R'))
		{
			// got SHDR chunk
			sminfo.tempo = ((FILE_Readbyte())<<8 | FILE_Readbyte());
			sminfo.volume = FILE_Readbyte();
			sminfo.tracks = FILE_Readbyte();

			printf("tempo: %d volume: %d tracks: %d\n", sminfo.tempo, sminfo.volume, sminfo.tracks);
		}
		else
		{
			if ((chk.id[0] == 'T') && (chk.id[1] == 'R') && (chk.id[2] == 'A') && (chk.id[3] == 'K'))
			{
				printf("Got TRK chunk %d\n", sminfo.realtrks+1);
				sminfo.trkdata[sminfo.realtrks] = malloc(chk.len);
				FILE_Read(chk.len, sminfo.trkdata[sminfo.realtrks]);
				sminfo.trklen[sminfo.realtrks] = chk.len;
				sminfo.realtrks++;
			}
			else
			{
					if ((chk.id[0] == 'N') && (chk.id[1] == 'A') && (chk.id[2] == 'M') && (chk.id[3] == 'E'))
					{
						// NAME chunk
						FILE_Read(64, name);
						name[63] = '\0';
						printf("Score name is '%s'\n", name);
					}
			}
		}

		// get the next chunk
		if (ctop+chk.len+8 < FILE_GetLength())
		{
			FILE_Seek(ctop+chk.len+8);
			ctop += chk.len+8;
		
			FILE_Read(4, (uint8 *)&chk.id);
			#ifdef WORDS_BIG_ENDIAN
			chk.len = FILE_Readlong();
			#else
			chk.len = _flipLongEnd(FILE_Readlong());
			#endif
			if (chk.len & 1)
			{
				chk.len++;
			}

			printf("Loaded new chunk [%c%c%c%c]\n", chk.id[0], chk.id[1], chk.id[2], chk.id[3]);
		}
		else
		{
			exit = 1;
		}

	}

	FILE_Close();

	printf("Attempting to load Sandcastle format instruments\n");

	strcpy(ibase, "INST0");		// set up first inst. name
	strcpy(obase, "iout0");

	strncpy(barepath, filename, 512);
	j = strlen(barepath) - 1;

	// strip filename off the path
	// if it's not a path we can handle that too
#ifdef MTP_WIN32
	while ((j > 0) && (barepath[j] != '\\'))
#else
#ifndef AMIGA
	while ((j > 0) && (barepath[j] != '/'))
#else
	while ((j > 0) && (barepath[j] != '/') && (barepath[j] != ':'))
#endif
#endif
	{
		j--;
	}

	if (j > 0)
	{
		barepath[j+1] = '\0';
	}
	else
	{
		strcpy(barepath, "./");
	}

	exit = i = 0;

	while (!exit)
	{
		if (FILE_FindCaseInsensitive(barepath, ibase, casename))
		{
			// found it
			printf("found inst [%s] in path [%s]\n", casename, barepath);

			strcpy(fullname, barepath);
			strcat(fullname, casename);
			FILE_Open(fullname);
			FILE_Seek(0x29c);	// DATA chunk is at 29c in all known Sandcastle instruments

			FILE_Read(4, (uint8 *)&chk.id);
			#ifdef WORDS_BIG_ENDIAN
			chk.len = FILE_Readlong();
			#else
			chk.len = _flipLongEnd(FILE_Readlong());
			#endif

			if ((chk.id[0] == 'D') && (chk.id[1] == 'A') && (chk.id[2] == 'T') && (chk.id[3] == 'A'))
			{
				// got it
				FILE_Seek(1);	// skip first byte of inst, it's always zero :/
				instdata = (uint8 *)malloc(chk.len-1);
				FILE_Read(chk.len-1, instdata);

				_FlipData(instdata, chk.len-1);

				GUS_Load(instdata, chk.len-1, 0, chk.len-2, SF_NONE, 0);

/*				{
					FILE *of;

					of = fopen(obase, "wb");
					fwrite(instdata, chk.len-1, 1, of);
					fclose(of);
				}		   */

				free(instdata);
				FILE_Close();

				if (ibase[4] != '9')
				{
					ibase[4]++;
				}
				else
				{
					ibase[4] = 'A';
				}
				if (obase[4] != '9')
				{
					obase[4]++;
				}
				else
				{
					obase[4] = 'A';
				}
			}
			else
			{
				printf("invalid Sandcastle instrument, stopping load\n");
				exit = 1;
			}

		}
		else
		{
			exit = 1;
		}
	}
}

//  _flipLongEnd - flips the specified word's endianness

static uint32 _flipLongEnd(uint32 in)
{
	uint32 out;
	uint8 a, b, c, d;

	a = (in & 0xff);
	b = (in & 0xff00)>>8;
	c = (in & 0xff0000)>>16;
	d = (in & 0xff000000)>>24;
	out = (d | c<<8 | b<<16 | a<<24);

	return out;
}

static int8 *SMUS_TypeName(void)
{
	return("Will Harvey/SMUS");
}

static int8 *SMUS_VerString(void)
{
	return("MTPng Will Harvey/SMUS module version 0.1.1\n"\
	"By Ian Schmidt\n"\
	"Copyright (c) 1999-2000 Ian Schmidt\n"\
	"All Rights Reserved\n");
}

static void SMUS_SetScroll(int8 scval)
{
	_scroll = scval;
}

static void SMUS_SetOldstyle(int8 scval)
{
	if (scval)
	{
		printf("NTGS: unsupported option\n");
	}
}

