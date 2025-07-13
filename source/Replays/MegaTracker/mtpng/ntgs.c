/***************************************************************************
                          ntgs.c  -  NoiseTracker GS filetype "object"
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// #include <stdio.h>
#include <stdlib.h>
// #include <string.h>
// #include <ctype.h>

#include "mtptypes.h"
#include "file.h"
#include "ntgs.h"
#include "oss.h"
#include "tabprot.h"

#define printf(...)
#define sprintf(...)

#define MTP_DEBUG 0	// set to 1 for verbose debugging of this module

static int16	trkInst[16];
static uint8	trkFX[16];              // track effect
static uint8	trkFXData[16];          // track FX data

static NTGS1Head header;	// current song's header
static NTGS1Inst ihead[128];	// max insts should be enforced!

static int16	sHand[128];	// inst. handles

static uint8 	*blockdata;	// actual music sequence data

static int8	songstat;	// song status

static int16	playerbpm;	// BPM tempo that sequencer wants

static int8 	_scroll = 1;	// scroll status

static volatile uint32 blkOfs, lastblkOfs;
static int16 curpos, curline;

// local function protos

static void _noteToName(int16 note, char *out);
void _FlipData(uint8 *input, uint32 size);

static int8 *NTGS_TypeName(void);
static int8 *NTGS_VerString(void);
static int8 NTGS_IsFile(char *filename);
static int8 NTGS_LoadFile(char *filename);
static void NTGS_PlayStart(void);
static void NTGS_Sequencer(void);
static void NTGS_DoFXTick(void);
static int8 NTGS_GetSongStat(void);
static void NTGS_ResetSongStat(void);
static void NTGS_SetScroll(int8 scval);
static void NTGS_SetOldstyle(int8 scval);
static int8 NTGS_GetNumChannels(void);
static void NTGS_Close(void);

// global variables
MTPInputT ntgs_plugin =
{
	NTGS_TypeName,
	NTGS_VerString,
	NTGS_IsFile,
	NTGS_LoadFile,
	NTGS_PlayStart,
	NTGS_Sequencer,
	NTGS_GetSongStat,
	NTGS_ResetSongStat,
	NTGS_SetScroll,
	NTGS_SetOldstyle,
	NULL,				// no GetCell routine here
	NTGS_GetNumChannels,
	NTGS_Close
};

// global functions

static int8 NTGS_GetSongStat(void)
{
	return songstat;
}

static void NTGS_ResetSongStat(void)
{
	if (songstat == 0)
	{
		curpos = curline = 0;

		blkOfs = (header.blklist[0] * header.blksize);

		songstat = 1;
	}
}

static int8 NTGS_GetNumChannels(void)
{
	return header.totaltrks;
}

static void NTGS_Close(void)
{
	free(blockdata);
}

// start playback

static void NTGS_PlayStart(void)
{
	int16 i;
	float hz, bpm;

	curpos = curline = 0;

	// init all tracks
	for (i=0; i<16; i++)
	{
		trkInst[i] = -1;
		trkFX[i] = 0;
		trkFXData[i] = 0;
	}

	blkOfs = (header.blklist[0] * header.blksize);

	songstat = 1;

	// and of course set our sequencer
	GUS_Sequencer = NTGS_Sequencer;

	// finally get the tempo right
	hz = (float) ((header.tempo * 51) / 255);
	bpm = hz * 2.5;
	playerbpm = (uint16) bpm + 2;	// slight adjustment to make songs right speed
	GUS_SetBPM(playerbpm);
	#if MTP_DEBUG
	printf("starting tempo: %ld BPM\n", playerbpm);
	#endif
}

static void NTGS_Sequencer(void)
{
	int8 track, inst, noteToPlay;
	uint8 noteOnVol;
	int16 ihnd, temp, temp2;
	char ntout[6];
	float hz, bpm;

	if (!songstat)
	{
		return;
	}

	if (GUS_GetBPM() != playerbpm)
	{
		GUS_SetBPM(playerbpm);
	}

	if (_scroll)
	{
		printf("[%02d] ", curline);
	}

	for (track = 0; track < header.totaltrks; track++)
	{
		if (_scroll)
	      	{
			_noteToName (blockdata[blkOfs], ntout);
			printf ("%s %02x %02x ", ntout, blockdata[blkOfs + 1] & 0xff,
				blockdata[blkOfs + 2] & 0xff);
		}
	 	if (blockdata[blkOfs] >= 0x54)	
		{
			// special command
			switch (blockdata[blkOfs])
			{
				case 0x54:	// NXT
					curline = 63;
					break;

				case 0x55:	// STP
					GUS_VoiceSetVolume(track, 0);
					break;
			}
		}
		else
		{
			// real note, handle it

			inst = blockdata[blkOfs + 1] & 0x3f;
			if (!inst)
			{
				inst = trkInst[track];
			}
			else
			{
				trkInst[track] = inst - 1;
			}

			if (track >= header.numramtrks)
			{	// DOC-based instrument, add offset
				ihnd = sHand[trkInst[track] + header.numRAMinsts];
			}
			else
			{	// RAM-based instrument, number is OK as-is
				ihnd = sHand[trkInst[track]];
			}

			if (!GUS_IsInstValid(ihnd))
			{
				GUS_VoiceSetVolume(track, 0);
			}

			noteOnVol = GUS_GetSampleVolume(ihnd);
			noteToPlay = blockdata[blkOfs];
			trkFX[track] = (blockdata[blkOfs + 1] & 0xc0);
			trkFXData[track] = blockdata[blkOfs + 2];

			// handle NTGS's crippled effects
			switch (trkFX[track])
			{
				case 0x40:	// set volume
					noteOnVol = trkFXData[track];
					GUS_VoiceSetVolume(track, noteOnVol);
					break;

				case 0x80:	// tempo change
					hz = (float) ((trkFXData[track] * 51) / 255);
					bpm = hz * 2.5;
					playerbpm = (uint16) bpm + 2;
					GUS_SetBPM(playerbpm);
					break;

				default:
					break;
			}

			// and see if we have a note to play
			if ((trkInst[track] != -1) && (noteToPlay != 0) && (GUS_IsInstValid(ihnd)))
			{
				// check stereo
				if (header.stereo[track] == 0)
				{
					GUS_VoiceSetPanning(track, 0);
				}
				else
				{
					GUS_VoiceSetPanning(track, 255);
				}

				// and start the new note
				if (noteOnVol > 127)
				{
					noteOnVol = 127;
				}

				GUS_VoiceSetVolume(track, noteOnVol);

				GUS_VoiceSetFrequency(track, FreqTable[noteToPlay] * 51);

				GUS_VoicePlay(track, ihnd);
			}
		}

		blkOfs += 3;	
	}	// big for (track = 0 ...) 

	if (_scroll)
	{
		printf("\n");
	}

	curline++;
	if (curline == 64)
	{
		curline = 0;
		curpos++;
		if (curpos == header.numpos)
		{
			songstat = 0;
			return;
		}
		else
		{
			blkOfs = (header.blklist[curpos] * header.blksize);
		}
	}
}



// ------ loader stuff ----------

// checks if the file is an NTGS module

static int8 NTGS_IsFile(char *filename)
{
	unsigned char fhead[14];
	int8 ok;

	ok = 0;

	if (!FILE_Open(filename))
	{
		return 0;
	}

	// get 16 bytes starting at offset 2 for ID
	FILE_Seek(2);
	FILE_Read(14, &fhead[0]);

	// NTGS 1.x modules start with the text "FTA MODULEFILE"
	// (2.x are FTA MODULEFIL2 'cuz of lack of imagination by Dave :-)

	if ((fhead[0] == 'F') && (fhead[1] == 'T') && (fhead[2] == 'A') && 
	    (fhead[4] == 'M') && (fhead[5] == 'O') && (fhead[6] == 'D') &&
	    (fhead[13] == 'E'))
	{
		ok = 1;	
	}

	FILE_Close();

	return ok;
}

static int8 NTGS_LoadFile(char *filename)
{
	int32 i, j, flags;
	int32 instofs, ramofs, docofs, wavofs, fullsize;
	uint8 *wdata;

	// open the file once again
	FILE_Open(filename);

	// read in header, in a portable endian-friendly way
	FILE_Seek(0);

	header.version = FILE_Readword();
	FILE_Read(14, (uint8 *)&header.signature);
	header.tempo = FILE_Readword();
	header.loopmode = FILE_Readword();
	header.rawmode = FILE_Readword();
	header.numpos = FILE_Readword();
	header.numblks = FILE_Readword();
	header.numDOCinsts = FILE_Readword();
	header.numRAMinsts = FILE_Readword();
	header.mode4k = FILE_Readword();
	header.docpgsize = FILE_Readword();
	header.rampgsize = FILE_Readword();
	header.numramtrks = FILE_Readword();
	header.blksize = FILE_Readword();
	header.totaltrks = FILE_Readword();
	header.musicsizeL = FILE_Readword();
	header.musicsizeH = FILE_Readword();

	FILE_Seekrel(18);	// skip reserved bytes
	
	// read stereo info
	for (i = 0; i < 15; i++)
	{
		header.stereo[i] = FILE_Readword();
	}

	// read blocklist
	for (i = 0; i < 256; i++)
	{
		header.blklist[i] = FILE_Readword();
	}
	
	#if MTP_DEBUG
	printf("NoiseTracker GS 1.x module with %d tracks and %d instruments.\n", header.totaltrks, header.numDOCinsts+header.numRAMinsts);
	printf("-------------------\n");
	printf("tempo: %d, lpmode: %d, rawmode: %d\n", header.tempo, header.loopmode, header.rawmode);
	printf("numpos: %d numblks: %d numDOC: %d numRAM: %d\n", header.numpos, header.numblks, header.numDOCinsts, header.numRAMinsts);
	printf("mode4K: %d docpgsz: %d rampgsz: %d\n", header.mode4k, header.docpgsize, header.rampgsize);
	printf("ramtrks: %d tottrks: %d blksize: %d musicsize: %ld\n", header.numramtrks, header.totaltrks, header.blksize, ((header.musicsizeH<<16)|(header.musicsizeL)));

	printf("---------------\n");
	#endif

	// compute offset to instruments
	instofs = ((header.musicsizeH<<16)|(header.musicsizeL)) + 606;

	// and offset to RAM-based waves
	ramofs = ((header.musicsizeH<<16)|(header.musicsizeL)) + 606 + ((header.numDOCinsts+header.numRAMinsts) * 32);
	if (ramofs & 0xff)
	{
		// not already page aligned

		ramofs &= 0xffffff00;
		ramofs += 0x100;		
	}

	// ditto for DOC-based waves
	docofs = ramofs + (header.rampgsize * 256);

	// now let's deal with the instruments
	for (i = 0; i < (header.numDOCinsts+header.numRAMinsts); i++)
	{
		FILE_Seek(instofs);	// seek to first instrument
		 
		FILE_Read(16, (uint8 *)&ihead[i].name[0]);

		// instsize is a *3* byte little-endian field.
		// die Goguel die!
		ihead[i].instsize = FILE_Readbyte();
		ihead[i].instsize |= (FILE_Readbyte() << 8);
		ihead[i].instsize |= (FILE_Readbyte() << 16);

		// inst. position is stored as a 2-byte number of pages
		ihead[i].instpos = FILE_Readword()*256;

		// the rest of this stuff is relatively easy
		ihead[i].instvol = FILE_Readbyte();
		FILE_Seekrel(1);	// skip other byte
		ihead[i].insttype = FILE_Readword();
		ihead[i].instleft[0] = FILE_Readbyte();
		ihead[i].instleft[1] = FILE_Readbyte();
		ihead[i].instright[0] = FILE_Readbyte();
		ihead[i].instright[1] = FILE_Readbyte();
		ihead[i].instSMode = FILE_Readword();

		ihead[i].name[14] = '\0';	// terminate instrument name

		#if MTP_DEBUG
		printf("Inst %02d: [%s] size: %ld\n", i, ihead[i].name, ihead[i].instsize);
		printf("vol = %d, type = %d, left = %02x%02x, right = %02x%02x, smode = %d\n",
			ihead[i].instvol, ihead[i].insttype, ihead[i].instleft[0], ihead[i].instleft[1],
			ihead[i].instright[0], ihead[i].instright[1], ihead[i].instSMode);
		#endif

		if (i >= header.numRAMinsts)
		{
			// DOC based
			ihead[i].instWaveOfs = docofs + ihead[i].instpos;
		}
		else
		{
			// RAM based
			ihead[i].instWaveOfs = ramofs + ihead[i].instpos;
		}

		instofs += 32;		// inst. records are 32 bytes
	}

	// ok, pre-processing is done, now load the instruments
	// (this is a separate loop to cut down on 
	for (i = 0; i < (header.numDOCinsts+header.numRAMinsts); i++)
	{
		// seek to the waveform
		FILE_Seek(ihead[i].instWaveOfs);

		// allocate RAM for the wave
		fullsize = ihead[i].instsize;
		wdata = (uint8 *)malloc(fullsize);

		// read it in
		FILE_Read(fullsize, (uint8 *)wdata);
		
		// convert to unsigned 8-bit
		_FlipData(wdata, fullsize);

		// and add it to the driver
		// use doc mode to figure things out as follows:
		// mode 0 is a swap (ram-based) inst, 2 is oneshot DOC based,
		// and 6&7 are looping DOC based.
		if (ihead[i].instleft[1] <= 2)
		{
		 	flags = SF_NONE;
		}
		else
		{
			flags = SF_LOOP;
		}

		// add the wave to the output driver
	 	sHand[i] = GUS_Load((char *)wdata, fullsize, 0, fullsize-1, flags, 0);

		// and set the volume
		GUS_SetSampleVolume(sHand[i], 127); //ihead[i].instvol);

		// ok, the output driver has to make it's own copy, so we
		// must free the wave 
		free(wdata);		
	}

	// alright, now let's grab the one missing piece: the music sequence data
	FILE_Seek(606);		// seek to the magic end-of-header offset
	fullsize = ((header.musicsizeH<<16)|(header.musicsizeL));
	blockdata = (uint8 *)malloc(fullsize);
	FILE_Read(fullsize, blockdata);

	// now we're done
	FILE_Close();
}
	
static void _noteToName(int16 note, char *out)
{
	char oct[4];

	switch (note)
	{
		case 0:
			strcpy(out, "---");
			break;
	        case 0x54:
			strcpy(out, "NXT");
			break;
	        case 0x55:
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

void _FlipData(uint8 *input, uint32 size)
{
	uint8 *data, lnz;
	uint32 i;

	data = input;

	lnz = 0;

	for (i=0; i<size; i++)
	{
		*data ^= 0x80;
		if (*data == 0x80)	// was a zero!
		{
			*data = lnz;
		}
		else
		{
			lnz = *data;
		}

		data++;
	}
}

static int8 *NTGS_TypeName(void)
{
	return("NoiseTracker GS");
}

static int8 *NTGS_VerString(void)
{
	return("MTPng NoiseTracker GS module version 1.0.1\n"\
	"By Ian Schmidt\n"\
	"Copyright (c) 1998-2000 Ian Schmidt\n"\
	"All Rights Reserved\n");
}

static void NTGS_SetScroll(int8 scval)
{
	_scroll = scval;
}

static void NTGS_SetOldstyle(int8 scval)
{
	if (scval)
	{
		printf("NTGS: unsupported option\n");
	}
}

