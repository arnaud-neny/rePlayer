/***************************************************************************
                          ssmt.c  -  SoundSmith/MegaTracker specific routines
                             -------------------
    begin                : Wed Dec 8 1999
    copyright            : (C) 1991-2000 by Ian Schmidt
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
#include "tables.h"
#include "ssmt.h"
#include "oss.h"

#define NUM_TRACKS	14

#define MTP_DEBUG 0	// set to 1 for verbose debugging of this module

// local functions

static int8 *SSMT_TypeName(void);
static int8 *SSMT_VerString(void);
static int8 SSMT_IsFile(char *filename);
static int8 SSMT_LoadFile(char *filename);
static void SSMT_PlayStart(void);
static void SSMT_Sequencer(void);
static void SSMT_DoFXTick(void);
static int8 SSMT_GetSongStat(void);
static void SSMT_ResetSongStat(void);
static void SSMT_SetScroll(int8 scval);
static void SSMT_SetOldstyle(int8 scval);
static int8 SSMT_GetNumChannels(void);
static void SSMT_Close(void);

// global variables
MTPInputT ssmt_plugin =
{
	SSMT_TypeName,
	SSMT_VerString,
	SSMT_IsFile,
	SSMT_LoadFile,
	SSMT_PlayStart,
	SSMT_Sequencer,
	SSMT_GetSongStat,
	SSMT_ResetSongStat,
	SSMT_SetScroll,
	SSMT_SetOldstyle,
	NULL,				// no GetCell routine here
	SSMT_GetNumChannels,
	SSMT_Close
};


// local variables
static uint16 	trkVU[NUM_TRACKS];              // track VU height, or 0xffff for no change
static uint16	trkDOC[NUM_TRACKS];             // track DOC freqency
static int16	trkPB[NUM_TRACKS];              // track pitchbend
static uint8	trkFX[NUM_TRACKS];              // track effect
static uint8	trkFXData[NUM_TRACKS];          // track FX data
static uint8	trkPortDir[NUM_TRACKS];         // track portamento direction
static uint16	trkPortAmt[NUM_TRACKS];         // track portamento amount per tick
static uint16	trkPortDest[NUM_TRACKS];        // track portamento destination
static int16	trkPortSpd[NUM_TRACKS];         // portamento speed so you can C00 
static int16	trkVibSpeed[NUM_TRACKS];	// vibrato speed
static int16	trkVibDepth[NUM_TRACKS];	// vibrato depth
static int16	trkVibOffset[NUM_TRACKS];	// vibrato table offset
static int32	trkArpFreq[NUM_TRACKS][4];	// track arpeggiato frequencies
static int16	trkArpBase[NUM_TRACKS];		// track arpeggiato base note
static int16	trkInst[NUM_TRACKS];		// instrument playing on track
static int8	trkNote[NUM_TRACKS];		// note playing on track
static int8	trkSTP[NUM_TRACKS];		// is track STP'd?

static int16	sHand[17];			// inst. handles

static int16 maxInst;

static SSHead header;			// file header
static uint8 *notes, *fx1, *fx2; 	// data blocks
static uint16 *stereodata;		// stereo info
static int8 songstat = 0; 		// song status
static int8 _scroll = 0;		// scrolling notes display
static int8 _oldfx = 0;			// use old (MT IIgs 3.x and earlier) pitchbend semantics

static volatile uint32 blkOfs, lastblkOfs;
static int16 curpos, curline, tempo, tchg, tick;

static int16 _mtBPMs[16] = 
{
	125, 250, 500, 53, 125, 125, 125, 125,
	150, 300, 600, 75, 125, 125, 125, 125
};

// local function protos
int8 _readDOCData(void);
int8 _tryDOCData(int8 *bname, int8 *bpath);
int16 _itsLooping(uint8 modeA, uint8 modeB);
uint32 _LocateChunk(int8 *chkname, uint32 startOfChks, uint32 length);
static uint32 _flipLongEnd(uint32 in);
int8 _chkcmp(int8 *a, int8 *b);
void _loadASIF(char *name, int16 instNum);

static int8 SSMT_GetSongStat(void)
{
	return songstat;
}

static void SSMT_ResetSongStat(void)
{
	if (songstat == 0)
	{
		curpos = curline = 0;
		tempo = header.tempo & 0x0f;
		tick = 0;
		blkOfs = (header.blocklist[curpos] * (14 * 64));
		lastblkOfs = blkOfs;
		songstat = 1;
	}
}

static int8 SSMT_GetNumChannels(void)
{
	return 14;
}

static void SSMT_Close(void)
{
	free(notes);
	free(fx1);
	free(fx2);
	free(stereodata);
}

// start playback

static void SSMT_PlayStart(void)
{
	int16 i;

	curpos = curline = 0;

	// init all tracks
	for (i=0; i<NUM_TRACKS; i++)
	{
		trkInst[i] = -1;
		trkNote[i] = 60;
		trkDOC[i] = 0;
		trkPB[i] = 0;
		trkFX[i] = 0;
		trkFXData[i] = 0;
		trkPortDir[i] = 0;
		trkPortAmt[i] = 0;
		trkPortDest[i] = 0;
		trkPortSpd[i] = 0;
		trkVibSpeed[i] = 0;
		trkVibDepth[i] = 0;
		trkVibOffset[i] = 0;
		trkArpFreq[i][0] = 0;
		trkArpFreq[i][1] = 0;
		trkArpFreq[i][2] = 0;
		trkArpFreq[i][3] = 0;
		trkArpBase[i] = 0;
		trkSTP[i] = 0;
	}

	tempo = header.tempo&0x0f;
	tick = 0;
	blkOfs = (header.blocklist[curpos] * (14*64));
	lastblkOfs = blkOfs;

	songstat = 1;

	// and of course set our sequencer
	GUS_Sequencer = SSMT_Sequencer;

	GUS_SetBPM(125);
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

static void SSMT_Sequencer(void)
{
	int8 track, inst, noteToPlay, arpNote;
	uint8 noteOnVol;
	int16 ihnd, temp, temp2;
	char ntout[6];

	if (!songstat)
	{
		return;
	}	

	tick++;
	if (tick < tempo)
	{
		SSMT_DoFXTick();
		return;
	}

	tick = 0;
	if (_scroll)
	{
		printf("[%02d] ", curline);
	}
	for (track = 0; track < NUM_TRACKS; track++)
	{
		if (_scroll)
		{
			noteToName(notes[blkOfs], ntout);
			printf("%s ", ntout);
			printf("%02x%02x  ", fx1[blkOfs], fx2[blkOfs]);
		}
		
		if (notes[blkOfs] & 0x80)
		{
			// it's a command
			switch(notes[blkOfs] & 0x7f)
			{
				case 0: // STP
					GUS_VoiceSetFrequency(track, 0);
					trkSTP[track] = 1;	// mark track STP'd
					break;

				case 1: // NXT
					curline = 63;
					break;

				default:
					#if MTP_DEBUG
					printf("Unknown command: %x\n", notes[blkOfs]);
					#endif
					break;
			}
		}
		else	// regular old note
		{
			inst = fx1[blkOfs]>>4;
			if (!inst)
			{
				inst = trkInst[track];
			}
			else
			{
				// if in STP mode and inst. changes,
				// STP becomes invalid
				if ((trkSTP[track]) && (trkInst[track] != inst - 1))
				{
					trkSTP[track] = 0;
				}
				trkInst[track] = inst-1;
			}

			if (trkInst[track] != -1)
			{
				ihnd = sHand[trkInst[track]];
				noteOnVol = GUS_GetSampleVolume(ihnd);
			}
			else
			{
				ihnd = 17;	// unused inst
			}

			// invalid sample halts current track on
			// IIgs SS/MT, do it here too :)
			if ((trkInst[track] == -1) || (!GUS_IsInstValid(ihnd)))
			{
				GUS_VoiceSetVolume(track, 0);
				trkVU[track] = 1;
			}

			noteToPlay = notes[blkOfs];
			trkFX[track] = fx1[blkOfs]&0x0f;
			trkFXData[track] = fx2[blkOfs];

			switch(trkFX[track])
			{	
				case FX_ARP:
					if (noteToPlay)
					{
						trkArpBase[track] = noteToPlay;
						arpNote = noteToPlay;
					}
					else
					{
						arpNote = trkArpBase[track];
					}

					if (trkFXData[track] != 0)
					{
						temp = (trkFXData[track]>>4)&0xf;
						temp2 = (trkFXData[track]&0xf);
						trkArpFreq[track][0] = FreqTable[arpNote+temp+temp2]*51;
						trkArpFreq[track][1] = FreqTable[arpNote+temp]*51;
						trkArpFreq[track][2] = FreqTable[arpNote+temp2]*51;
					}
					break;

				case FX_VOL:
					noteOnVol = fx2[blkOfs];
					trkVU[track] = noteOnVol;
					GUS_VoiceSetVolume(track, noteOnVol);
					break;

				case FX_VIB:
					if ((fx2[blkOfs]>>4) != 0)
					{
						trkVibSpeed[track] = (fx2[blkOfs]>>4);
					}
					if ((fx2[blkOfs] & 0x0f) != 0)
					{
						trkVibDepth[track] = ((fx2[blkOfs]&0x0f) * 32);
					}
					break;

				case FX_VOLUP:
					noteOnVol += fx2[blkOfs];
					if (noteOnVol > 0xff)
					{
						noteOnVol = 0xff;
					}
				 	trkVU[track] = noteOnVol;
					break;

				case FX_VOLDN:
					if (fx2[blkOfs] <= noteOnVol)
					{
						noteOnVol -= fx2[blkOfs];
					}
					else
					{
						noteOnVol = 0;
					}
				 	trkVU[track] = noteOnVol;
					break;

				case FX_PORTA:
					temp = FreqTable[trkNote[track]];
					temp2 = FreqTable[noteToPlay];

					if (fx2[blkOfs] != 0)
					{
						trkPortSpd[track] = fx2[blkOfs];
					}

					if (temp2 == temp)	// nowhere to go?
					{
						trkFX[track] = 0;
						trkFXData[track] = 0;
						break;
					}

					if (temp2 > temp)
					{	// pitch going UP
				      		trkPortDir[track] = 1;
				      		trkPortDest[track] = temp2;
				      		noteToPlay = 0;
					}
					else
				    	{	// pitch going DOWN
				      		trkPortDir[track] = 0;
						trkPortDest[track] = temp2;
						noteToPlay = 0;
					}
					break;


				case FX_TEMPO:
					tchg++;
					tempo = fx2[blkOfs]&0x0f;
					GUS_SetBPM(_mtBPMs[fx2[blkOfs]>>4]);
					break;
			}
		
      			if ((trkInst[track] != -1) && (noteToPlay > 0) && (GUS_IsInstValid(ihnd)))
			{
				if (!stereodata[track])
				{
					GUS_VoiceSetPanning(track, 0);
				}
				else
				{
					GUS_VoiceSetPanning(track, 255);
				}

				// now start the new note
				if (noteOnVol > 127)
				{
					noteOnVol = 127;
				}

				// set volume if we didn't already
				if (trkFX[track] != FX_VOL)
				{
					GUS_VoiceSetVolume(track, noteOnVol); //VolumeConversion[noteOnVol]>>2);
				}
			
				if (trkSTP[track])
				{
					trkSTP[track] = 0;
					
					if (trkNote[track] == noteToPlay)
					{	// STP conditions passed, resume voice 
						GUS_VoiceSetFrequency(track, FreqTable[noteToPlay]*51);
					}
					else
					{
						GUS_VoiceSetFrequency(track, FreqTable[noteToPlay]*51);
						GUS_VoicePlay(track, ihnd);
					}
				}
				else
				{
					GUS_VoiceSetFrequency(track, FreqTable[noteToPlay]*51);
					GUS_VoicePlay(track, ihnd);
				}

				trkNote[track] = noteToPlay;
				trkDOC[track] = FreqTable[noteToPlay];
				trkVU[track] = noteOnVol<<1;  // update VUs
				trkPB[track] = 0;          // zero pitchbend
			}
			else
			{
				if (trkInst[track] != -1)
				{
					if (!GUS_IsInstValid(sHand[trkInst[track]]))
					{
						GUS_VoiceSetVolume(track, 0);
					}
				}
				else
				{
					GUS_VoiceSetVolume(track, 0);
				}
			}
		}
		blkOfs++;

	} // big track for loop

	if (_scroll)
	{
		printf("\n");
	}
	curline++;
	if (curline == 64)
	{
		curline = 0;
		curpos++;
			if (_scroll)
			{
				printf("New position: %d\n", curpos);
			}
		if (curpos == header.blocklen)
		{
			songstat = 0;
			return;
		}
		else
		{
			blkOfs = (header.blocklist[curpos] * (14*64));

			if (_scroll)
			{
				printf("New block: %d\n", header.blocklist[curpos]);
			}
		}
	}
}

static void SSMT_DoFXTick(void)
{
	int16 track, temp, vtemp;

	for (track = 0; track < NUM_TRACKS; track++)
	{
		switch(trkFX[track])
		{
			case FX_ARP:
				if (trkFXData[track])
				{
					temp = tick & 0x03;
					if (!temp)
					{
						GUS_VoiceSetFrequency(track, trkArpFreq[track][0]);
					}
					else
					{
						if (temp == 1)
						{
							GUS_VoiceSetFrequency(track, trkArpFreq[track][1]);
						}
						else
						{
							GUS_VoiceSetFrequency(track, trkArpFreq[track][2]);
						}
					}
				}
				break;

			case FX_PITCHUP:
				// prevent overflow
				if (trkDOC[track] < (0xffff-trkFXData[track]))
				{
					trkDOC[track] += trkFXData[track];
					GUS_VoiceSetFrequency(track, trkDOC[track]*51);
					if (_oldfx == 1)	// only do once for MT 3.xx
					{
						trkFX[track] = 0;
						trkFXData[track] = 0;
					}
		      		}
		      		break;

		      	case FX_PITCHDN:
	      			// prevent underflow (FTA.Song)
	      			if (trkDOC[track] > trkFXData[track])
		      		{
		      			trkDOC[track] -= trkFXData[track];
		      			GUS_VoiceSetFrequency(track, trkDOC[track]*51);
					if (_oldfx == 1)	// only do once for MT 3.xx
					{
						trkFX[track] = 0;
						trkFXData[track] = 0;
					}
				}
				break;

			case FX_VIB:
				trkVibOffset[track] += trkVibSpeed[track];
				trkVibOffset[track] &= 0x1f;
				temp = trkVibOffset[track] + trkVibDepth[track];
				vtemp = ((int16)trkDOC[track] + (int16)SineTable[temp]);
				GUS_VoiceSetFrequency(track, vtemp*51);
				break;
				
			case FX_PORTA:
				if (!trkPortDir[track]) 
				{	// portamento down
					trkDOC[track] -= trkPortSpd[track];
					if (trkDOC[track] <= trkPortDest[track])
					{
						trkDOC[track] = trkPortDest[track];
						trkFX[track] = 0;
						trkFXData[track] = 0;
					}
				}
		  		else 	// portamento up
			    	{
					trkDOC[track] += trkPortSpd[track];
					if (trkDOC[track] >= trkPortDest[track])
					{
						trkDOC[track] = trkPortDest[track];
						trkFX[track] = 0;
						trkFXData[track] = 0;
					}
				}

				GUS_VoiceSetFrequency(track, trkDOC[track]*51);
				break;				
		}
	}
}

// ------ loader stuff ----------

// checks if the file is SS/MT

static int8 SSMT_IsFile(char *filename)
{
	unsigned char fhead[16];
	int8 ok;

	ok = 0;

	if (!FILE_Open(filename))
	{
		return 0;
	}

	// get the first 16 bytes for ID
	FILE_Read(16, &fhead[0]);

	// check for SoundSmith "SONGOK" songs.
	// passes if bytes 4 and 5 are "OK".  this is all
	// the checking soundsmith itself does, and some songs
	// actually need this lax checking.
	if ((fhead[4] == 'O') && (fhead[5] == 'K'))
	{
		ok = 1;	
	}

	// check for MegaTracker "IAN92a" songs.
	// note that some subversions of IAN92a won't work with 
	// this player, but since about 5 people on the planet have
	// a recent enough version of MegaTracker to create them
	// I'm not gonna care ;-)
	if ((fhead[0] == 'I') && (fhead[1] == 'A') && (fhead[2] == 'N'))
	{
		ok = 1;
	}
	
	FILE_Close();

	return ok;
}

static int8 SSMT_LoadFile(char *filename)
{
	int32 i, j, temp;
	char barepath[1024], asifpath[1024+32], tempname[32], casename[32];

	// open the file once again
	FILE_Open(filename);

	// read in header, in a portable endian-friendly way
	FILE_Seek(0);

	// read signature
	FILE_Read(6, (uint8 *)&header.signature);

	#if MTP_DEBUG
	strncpy(tempname, &header.signature, 6);
	tempname[6] = '\0';
	printf("signature is [%s]\n", tempname);
	#endif

	// get blocksize and tempo
	header.blocksize = FILE_Readword();
	header.tempo = FILE_Readword();

	#if MTP_DEBUG
	printf("blocksize: %d tempo: %d\n", header.blocksize, header.tempo);
	#endif

	// skip 10 reserved bytes
	FILE_Seekrel(10);

	// do instruments
	for (i = 0; i < 15; i++)
	{
		// get the name
		FILE_Read(22, (char *)tempname);

		// depascalize it
		temp = tempname[0];
		// clamp to avoid bugs
		if (temp < 0)
			temp = 0;
		else if (temp > 21)
			temp = 21;

		tempname[temp + 1] = '\0';

		// and copy it into the structure
		strcpy((int8 *)header.insts[i].name, (char *)&tempname[1]);

		// avoid issues and enable all insts
		if (header.insts[i].name[0] == 0)
		{
			header.insts[i].name[0] = ' ';
            header.insts[i].name[1] = 0;
		}

		FILE_Seekrel(2);	// skip 2 reserved bytes

		// read this instrument's volume
		header.insts[i].volume = FILE_Readword();
		FILE_Seekrel(4);	// skip 5 bytes
	}

	// get the blocklen
	header.blocklen = FILE_Readword();

	#if MTP_DEBUG
	printf("blocklen = %d\n", header.blocklen);
	#endif

	// and the blocklist
	FILE_Read(128, (int8 *)header.blocklist);

	// get the main body of the file
	notes = (uint8 *)malloc(header.blocksize);
	fx1 = (uint8 *)malloc(header.blocksize);
	fx2 = (uint8 *)malloc(header.blocksize);

	// don't forget the stereo data
	stereodata = (uint16 *)malloc(32);

	// let's read the rest in now
	// we shouldn't need this seek, but let's be safe for now
	FILE_Seek(600);
	FILE_Read(header.blocksize, (uint8 *)notes);
	FILE_Read(header.blocksize, (uint8 *)fx1);
	FILE_Read(header.blocksize, (uint8 *)fx2);
	FILE_Read(32, (uint8 *)stereodata);
	FILE_Close();

	// now strip the pathname separate from the filename
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
		barepath[0] = 0;
	}

	// now process the instruments
	if (!_tryDOCData(filename, barepath))
	{
		// handle ASIFs here
		for (i = 0; i < 15; i++)
		{
			if (header.insts[i].name[0] != '\0')
			{
				if (FILE_FindCaseInsensitive(barepath, header.insts[i].name, casename))
				{
					#if MTP_DEBUG
					printf("Found inexact case match for %s: %s\n", header.insts[i].name, casename);
					#endif
					strcpy(asifpath, barepath);
					strncat(asifpath, casename, 32);
				}
				else	// punt
				{
					//printf("Error: unable to match filename %s\n", header.insts[i].name);
					memset(sHand + i, 0, sizeof(sHand[i]));
					continue;
				}

				#if MTP_DEBUG
				printf("Loading ASIF [%s]\n", barepath);
				#endif
				_loadASIF(asifpath, i);
			}
		}
	}

	// now fix up the instrument volumes
	for (i = 0; i < 15; i++)
	{
		if (header.insts[i].name[0] != 0)
	    	{
			// all we need to do is set the instrument's volume
			#if MTP_DEBUG
			printf("inst %d volume %d\n", i, header.insts[i].volume&0xff);
			#endif
			GUS_SetSampleVolume(sHand[i], (header.insts[i].volume&0xff));
	    	}


		else
		{
			// not an instrument, make sure the player knows that
			sHand[i] = 16;		// point this inst to an invalid handle
		}
	}

    	return 1;
}

static void SSMT_SetScroll(int8 scval)
{
	_scroll = scval;
}

static void SSMT_SetOldstyle(int8 scval)
{
	_oldfx = scval;
}

int8 _tryDOCData(int8 *bname, int8 *bpath)
{
	char fname[1024], basename[1024], matchname[32];
	char fullpath[1024];
	int16 i, j, k;

	strncpy(basename, bname, 1024);	// make a copy to mess with

	i = strlen(basename);
	if (basename[i-4] == '.')	// file ends with . and 3 chars!
	{
		if ((toupper(basename[i-3]) == 'M') && (toupper(basename[i-2]) == 'T') && (toupper(basename[i-1]) == 'P'))
		{
			basename[i-4] = '\0';
		}
	}

	// now back up basename and get just the filename without any leading path
	i = strlen(basename)-1;
#ifdef MTP_WIN32
	while ((i > 0) && (basename[i] != '\\'))
#else
#ifndef AMIGA
	while ((i > 0) && (basename[i] != '/'))
#else
	while ((i > 0) && (basename[i] != '/') && (basename[i] != ':'))
#endif
#endif
	{
		i--;
	}

	if (i > 0)
	{
		i++;	// get past the slash again
		k = strlen(basename) - i;
		for (j = 0; j < k; j++)
		{
			basename[j] = basename[i+j];
		}

		basename[k] = '\0';
	}

	// otherwise basename IS a basename with no path, so let it be

	// first form: base.W
	strncpy(fname, basename, 1024);	// copy basename over
	strcat(fname, ".W"); 

	if (FILE_FindCaseInsensitive(bpath, fname, matchname))
	{
		strcpy(fullpath, bpath);
		strncat(fullpath, matchname, 32);
		if (FILE_Open(fullpath))
		{
			return(_readDOCData());
		}
	}

	// second form: base.DOC
	strncpy(fname, basename, 1024);	// copy basename over
	strcat(fname, ".DOC");

	if (FILE_FindCaseInsensitive(bpath, fname, matchname))
	{
		strcpy(fullpath, bpath);
		strncat(fullpath, matchname, 32);
		if (FILE_Open(fullpath))
		{
			return(_readDOCData());
		}
	}

	// third form: base.D
	strncpy(fname, basename, 1024);	// copy basename over
	strcat(fname, ".D");

	if (FILE_FindCaseInsensitive(bpath, fname, matchname))
	{
		strcpy(fullpath, bpath);
		strncat(fullpath, matchname, 32);
		if (FILE_Open(fullpath))
		{
			return(_readDOCData());
		}
	}

	// fourth form: DOC.DATA
	if (FILE_FindCaseInsensitive(bpath, "DOC.DATA", matchname))
	{
		strcpy(fullpath, bpath);
		strncat(fullpath, matchname, 32);
		if (FILE_Open(fullpath))
		{
			return(_readDOCData());
		}
	}

	return(0);     // indicate false, no DOC data
}

int8 _readDOCData(void)
{
	int8 wexit;
	uint16 numInsts, wofs, wlen, realsize, i;
	WaveList wavelists[16][2];
	uint16 wsizes[] = 
	{
		0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000
	};
	uint16 instflags;
	uint8 *wave, *src;

	numInsts = FILE_Readword();
	if (numInsts > 15)
	{
		#if MTP_DEBUG
		printf("Invalid DOC Data file.  Bailing!\n");
		#endif

		return 0;
	}

	#if MTP_DEBUG
	printf("DOC Data file found with %d instruments.\n", numInsts);
	#endif

	// now extract the wavelists
	FILE_Seek(0x10022);	// magic number - offset of first wavelist in a DOC Data file
				// == 64k (doc image) + 2 (header) + 32 (?????)

	// there are 2 6-byte waveilst structures every 0x5c bytes
	// into the file.  snag 'em!
	for (i = 0; i < numInsts; i++)
	{ 
		FILE_Read(6, (uint8 *)&wavelists[i][0]);
		FILE_Read(6, (uint8 *)&wavelists[i][1]);

		// seek to next wavelist
		FILE_Seek(0x10022 + (0x5c*(i+1)));
	}	

	// now munch on the wave data
	for (i = 0; i < numInsts; i++)
	{
		// get offset of waveform (it's in pages)
		wofs = (wavelists[i][0].waveAddress&0xff) << 8;
		wofs += 2;	// skip first word of file (numInsts)

		// get maximum possible length of waveform
		wlen = wsizes[wavelists[i][0].waveSize&0x7];

		// figure out if the wave should loop or not
		if (_itsLooping(wavelists[i][0].docMode&0xfe, wavelists[i][1].docMode&0xfe))
		{
			instflags = SF_LOOP;
		}
		else
		{
			instflags = SF_NONE;
		}

		// allocate memory for the wave
		wave = (uint8 *)malloc(wlen);

		// read the wave data
		FILE_Seek(wofs);
		FILE_Read(wlen, wave);

		// find the true wavesize, by searching for a sample value
		// of zero (on the 5503 zero is the end-of-sample marker)

		src = wave;
		realsize = 0;
		wexit = 0;

		while ((!wexit) && (realsize < wlen))
		{
			if (*src != 0x00)
			{
				*src = *src ^0x80;	// make samples signed 8-bit (the 5503 prefers unsigned)
				src++;
				realsize++;
			}
			else
			{
				wexit = 1;
			}
		}

		#if MTP_DEBUG
		printf("wave %d size is %d out of %d\n", i, realsize, wlen);
		#endif
		
		// ok, let's hand it over to the output driver
		sHand[i] = GUS_Load((char *)wave, realsize, 0, realsize-1, instflags, 0);

		// track the highest inst. # loaded
		maxInst = i;

		// the output driver makes it's own copy of the wave data
		// so we can discard ours now.
		free(wave);
	}

	FILE_Close();
	return 1;
}

// _itsLooping - determines based on docmodes if the instrument specified is looping
int16 _itsLooping(uint8 modeA, uint8 modeB)
{
// there are several sets of docmodes that can cause looping on the 5503,
// this covers the common ones...

	if (
	   ((modeA == 6) && (modeB == 6)) ||   // swap and swap (SSS sound editor)
	   ((modeA == 6) && (modeB == 0)) ||   // swap and freerun (some FTA)
	   ((modeA == 2) && (modeB == 6)) ||   // sync and swap (AZ, some FTA)
	   ((modeA == 0) && (modeB == 0)) )    // freerun and freerun (fun.music)
	{
 		return(1);
	}
   
	return(0);
}

// _chkcmp: compares 2 chunk types

int8 _chkcmp(int8 *a, int8 *b)
{
	if ((a[0] == b[0]) && (a[1] == b[1]) && 
	    (a[2] == b[2]) && (a[3] == b[3]))
	{
		return 0;
	}
	return 1;
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

// _LocateChunk: finds a chunk in an ASIF file
uint32 _LocateChunk(int8 *chkname, uint32 startOfChks, uint32 length)
{
	int32 exit = 0;
	int8 temp[5];
	uint32 clen;

	while ((!exit) && (FILE_GetPos() < length))
	{
		FILE_Read(4, temp);
		if (!_chkcmp(temp, chkname))
		{
			// found it!
			return 1;
		}

		// not found, go on
		// get this chunk's length
		FILE_Read(4, (uint8 *)&clen);	
		
		#ifndef WORDS_BIGENDIAN
		clen = _flipLongEnd(clen);
		#endif

		#if MTP_DEBUG
		temp[4] = '\0';
		printf("Skipping chunk [%s], length %ld\n", temp, clen);
		#endif

		FILE_Seekrel(clen);
	}

	return 0;
}

// _loadASIF - loads an ASIF file and adds it to the instrument list

void _loadASIF(char *name, int16 instNum)
{
	int8 temp[5], slen, awc, bwc, exit;
	uint8 *wavebuf, *wdata;
	int16 instflags;
	uint32 length, topOfChks, clen, reallen;
	WaveList aWaveList, bWaveList;
	WAVEChk wChk;

	if (!FILE_Open(name))
	{
		printf("Unable to open ASIF file %s\n", name);
		FILE_Close();
		return;
	}

	// get header
	FILE_Seek(0);
	FILE_Read(4, temp);

	// make sure it's an IFF file
	if (_chkcmp(temp, "FORM"))
	{
		printf("Error: file %s is not IFF-style\n", name);
		FILE_Close();
		return;
	}
	
	// now get the file's length
	// (this is stored big-endian in the file, so a little hackery happens)

	FILE_Read(4, (uint8 *)&length);	// on big-endian boxes we're all ready to go

	#ifndef WORDS_BIGENDIAN
	length = _flipLongEnd(length);	// on little-endian we need to flip it
	#endif

	#if MTP_DEBUG
	printf("ASIF file length: %ld\n", length);
	#endif
	
	FILE_Read(4, temp);

	// make sure it's an ASIF file
	if (_chkcmp(temp, "ASIF"))
	{
		printf("Error: file %s is not an ASIF file\n", name);
		FILE_Close();
		return;
	}

	// now mark where in the file the start of chunks is
	topOfChks = FILE_GetPos();

	// now let's go chunk-hunting.
	// first, the INST chunk...
	if (!_LocateChunk("INST", topOfChks, length))
	{
		printf("Error: file %s has no INST chunk\n", name);
		FILE_Close();
		return;
	}

	#if MTP_DEBUG
	printf("INST chunk found!  file pos = %ld\n", FILE_GetPos());
	#endif

	// now mine the INST chunk for the info we want
	FILE_Read(4, (uint8 *)&clen);

	#ifndef WORDS_BIGENDIAN
	clen = _flipLongEnd(clen);	// on little-endian we need to flip it
	#endif

	#if MTP_DEBUG
	printf("INST chunk length = %ld\n", clen);
	if (clen != 62)	// standard single-wavelist INST chunks are 62 bytes
	{
		printf("INST chunk length not what I expected, expect problems\n");
	}
	#endif

	// After chunklength is a pascal string with the inst. name.

	// read the string length
	FILE_Read(1, (uint8 *)&slen);
	FILE_Seekrel(slen);

	// now the file pointer is at the start of the INSTChk structure as
	// defined in ssmt.h
	
	FILE_Seekrel(32);	// seek through INSTChk struct to aWaveCount
	FILE_Read(1, (uint8 *)&awc);
	FILE_Read(1, (uint8 *)&bwc);	// get wavecounts

	#if MTP_DEBUG
	printf("wave counts: %d and %d\n", awc, bwc);
	#endif
	
	if ((awc != 1) || (bwc != 1))
	{
		printf("Error: we can only handle ASIFs with 1 wavelist\n");
		FILE_Close();
		return;
	}	

	// now pull in the wave lists
	// we don't use relPitch, so just need first 4 bytes
	FILE_Read(4, (uint8 *)&aWaveList);
	FILE_Seekrel(2);
	FILE_Read(4, (uint8 *)&bWaveList);

	// back to work, now find the WAVE chunk
	FILE_Seek(topOfChks);
	if (!_LocateChunk("WAVE", topOfChks, length))
	{
		printf("Error: file %s has no WAVE chunk\n", name);
		FILE_Close();
		return;
	}

	#if MTP_DEBUG
	printf("WAVE chunk found!  file pos = %ld\n", FILE_GetPos());
	#endif

	// now mine the WAVE chunk for the info we want
	FILE_Read(4, (uint8 *)&clen);

	#ifndef WORDS_BIGENDIAN
	clen = _flipLongEnd(clen);	// on little-endian we need to flip it
	#endif

	#if MTP_DEBUG
	printf("WAVE chunk length = %ld\n", clen);
	#endif

	// After chunklength is a pascal string with the wave name.
	// this is meaningless in most ASIFs

	// read the string length and skip the rest
	FILE_Read(1, (uint8 *)&slen);
	FILE_Seekrel(slen);

	// the file pointer is now aimed at a wave chunk, as per the
	// WAVEChk struct in ssmt.h

	// read in the parts we want
	wChk.waveSize = FILE_Readword();
	wChk.numSamples = FILE_Readword();
	wChk.Location = FILE_Readword();
	wChk.Size = FILE_Readword();
	wChk.origFreq = FILE_Readlong();
	wChk.sampRate = FILE_Readlong();

	#if MTP_DEBUG
	printf("Wave size = %ld\n", wChk.waveSize);
	printf("nSamp: %d, loc: %d, size: %d, orig: %ld, sR: %ld\n",
		wChk.numSamples, wChk.Location, wChk.Size, wChk.origFreq, wChk.sampRate);
	#endif

	// the file pointer is now aimed at the 8-bit unsigned waveform data
	// allocate memory
	wavebuf = malloc(wChk.waveSize);
	// read in the data
	FILE_Read(wChk.waveSize, (uint8 *)wavebuf);

	// now then.  a sample value of 0 on the 5503 ends the wave.
	// this is not the case on all other 8-bit playback hardware
	// (or our software mixer for that matter).  we thus find the
	// wave's true length by looking for the first 0 if any.
	// also we want the data to be signed 8-bit (which is a tad easier
	// to software mix) than unsigned (which the 5503 requires).

	// note: unlike in DOC DATA images, ASIF files typically contain
	// only the actual wave data, so this pass should only 

	reallen = 0;
	wdata = wavebuf;
	exit = 0;

	while ((!exit) && (reallen < wChk.waveSize))
	{
		if (*wdata != 0x00)
		{
			*wdata ^= 0x80;
			wdata++;
			reallen++;
		}
		else
		{
			exit = 1;
		}
	}

	#if MTP_DEBUG
	printf("True wave size is %ld\n", reallen);
	#endif

	// figure out if the wave should loop or not
	if (_itsLooping(aWaveList.docMode&0xfe, bWaveList.docMode&0xfe))
	{
		instflags = SF_LOOP;
	}
	else
	{
		instflags = SF_NONE;
	}

	sHand[instNum] = GUS_Load(wavebuf, reallen, 0, reallen-1, instflags, 0);

	free(wavebuf);

	FILE_Close();
}

static int8 *SSMT_TypeName(void)
{
	return("SoundSmith/MegaTracker");
}

static int8 *SSMT_VerString(void)
{
	return("MTPng SoundSmith/MegaTracker module version 4.3\n"\
	"By Ian Schmidt\n"\
	"Copyright (c) 1991-2000 Ian Schmidt\n"\
	"All Rights Reserved\n");
}
