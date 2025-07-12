/***************************************************************************
                          main.c  -  Main program (traditional C main())
                             -------------------
    begin                : Wed Dec  8 23:53:24 EST 1999
    copyright            : (C) 1999-2000 by Ian Schmidt
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtptypes.h"
#include "file.h"
#include "ssmt.h"
#include "ntgs.h"
#include "smus.h"
#include "oss.h"

// local protos
void _ShowUsage(void);

// static variables

// input plugin table - add your input plugin to this table
// and it will automatically be "hooked up"
static MTPInputT *input_plugins[] =
{
	&ssmt_plugin,		// soundsmith/megatracker
	&ntgs_plugin,		// noisetracker GS
	&smus_plugin,		// will harvey SMUS
	(MTPInputT *)NULL	
};


/* Amiga - */
char *filename = "AUDIO:B/16/F/44100/C/2/BUFFER/352800";
int frequency = 44100;
char typeofoutput = 0;
extern unsigned long totalsamples;
int infoonly = 0;
char aif=0;
unsigned long tmp;
extern int audiofd;
char hed[54];
char br=0; 

typedef struct {
        unsigned short  exponent;               // Exponent, bit #15 is sign bit for mantissa
        unsigned long   mantissa[2];            // 64 bit mantissa
} extended;


void ulong2extended (unsigned long in, extended *ex)
{
  ex->exponent=31+16383;
  ex->mantissa[1]=0;
  while(!(in & 0x80000000))
  {
    ex->exponent--;
    in<<=1;
  }
  ex->mantissa[0]=in;
}

extended ext;


// functions
int main(int argc, char *argv[])
{
	int16 arg, ftype;
	int8 scroll = 0;
	int8 oldstyle = 0;

	// print startup info	
	printf("\nMegaTracker Player for %s v4.3.1 [27-OCT-2000]\n", PLAT_NAME);
	printf("Written by and copyright (c) 1996-2000 Ian Schmidt.\n");
	printf("This software comes with ABSOLUTELY NO WARRANTY.\n");
	printf("Refer to the file COPYING for details.\n\n");

	// we need at least one argument
	if (argc < 2)
	{
		_ShowUsage();
		exit(5);
	}
	
	// now scan for arguments.  yes there's ways to do this, but none
	// are portable.
	arg = 1;

	// note: following line uses short-circuit evaluation.
	// the order of tests is important lest a segfault happen.
	while ((arg < argc) && (argv[arg][0] == '-') && (br==0))	// get all minus options
	{
		switch (argv[arg][1])
		{
			case 's':
			case 'S':
				scroll = 1;
				break;

			case 'o':
			case 'O':
			  	oldstyle = 1;
				break;	

			case 'v':
			case 'V':
				ftype = 0;
				printf("================================\n");
				while (input_plugins[ftype] != (MTPInputT *)NULL)
				{
					printf("Plugin %d is: \n%s", ftype+1, input_plugins[ftype]->VerString());
					printf("================================\n");
					ftype++;
				}
				exit(0);
				break;

/* Amiga - */
			case 'r':
                                typeofoutput = 0;
                                filename = &argv[arg][2];
				break;
			case 'f':
                                frequency = atoi(&argv[arg][2]);
				break;
			case 'i':
                                infoonly = 1;
				break;
                   case 'a':
                                aif=1;
                                typeofoutput = 0;
                                filename = &argv[arg][2];
                         break;
		   case 't':
                            br=1;
                            break;
			default:
				printf("Unknown switch -%c.\n", argv[arg][1]);
				break;
		}

		arg++;
	}

	if (argc <= arg)
	{
		_ShowUsage();
		exit(5);
	}

/* Amiga - */
	if (strlen(filename) == NULL)
	{
		printf("Dude, i dont know where to output to, give me filename!\n\n");
		exit (5);
	}

	if ((frequency < 4000) || (frequency > 48000))
	{
		printf("Frequency must be between 4000 and 48000 Hz!\n\n");
		exit (5);
	}


	// now ask each of our filetype "objects" if they can handle this
	ftype = 0;

	while (input_plugins[ftype] != (MTPInputT *)NULL)
	{
		if (input_plugins[ftype]->IsFile(argv[arg]))
		{
			printf("File identified: %s\n", input_plugins[ftype]->TypeName());
			break;
		}
		else
		{
			ftype++;
		}
	}

	// did we find one?
	if (input_plugins[ftype] == (MTPInputT *)NULL)
	{
		printf("Sorry, I don't know how to handle this type of file.\n\n");
		exit(5);
	}
	
	// init output driver (must be before xx_LoadFile call to init inst. structs)
	if (GUS_Init())
	{
		printf("Output driver initialized\n");
	}
	else
	{
		printf("Error opening output driver\n\n");
		exit(5);
	}

	// now load the file
	input_plugins[ftype]->LoadFile(argv[arg]);

	// start playback
	input_plugins[ftype]->SetOldstyle(oldstyle);
	input_plugins[ftype]->SetScroll(scroll);
	input_plugins[ftype]->PlayStart();

	// start the output plugin
	// GUS_PlayStart();

	// main playback loop (should thread this off)
	while (input_plugins[ftype]->GetSongStat())
	{
/* Amiga - */
/*		GUS_TimeCheck();*/
		GUS_UpdateFake();
	}
	printf("Projected raw size:  %lu\n", totalsamples * 4);
	printf("Song duration:  %lu seconds\n", totalsamples / frequency);



/* Amiga - playback is here */
	if (infoonly == 0)
	{
             if (aif==1) {
                hed[0]='F'; hed[1]='O'; hed[2]='R'; hed[3]='M';
                tmp=54+(totalsamples*4)-8;
                memcpy(&hed[4], &tmp, 4);
                hed[8]='A'; hed[9]='I'; hed[10]='F'; hed[11]='F';
                hed[12]='C'; hed[13]='O'; hed[14]='M'; hed[15]='M';
                hed[16]=0; hed[17]=0; hed[18]=0; hed[19]=18;
                hed[20]=0; hed[21]=2;  // chans
                memcpy(&hed[22], &totalsamples, 4);
                hed[26]=0; hed[27]=16;  // bits
                ulong2extended(frequency, &ext);
                memcpy(&hed[28], &ext, 10);
                hed[38]='S'; hed[39]='S'; hed[40]='N'; hed[41]='D';
                tmp=(totalsamples*4)+8;
                memcpy(&hed[42], &tmp, 4);
                hed[46]=0; hed[47]=0; hed[48]=0; hed[49]=0;
                hed[50]=0; hed[51]=0; hed[52]=0; hed[53]=0;
                write(audiofd, &hed, 54);
             }
		input_plugins[ftype]->SetOldstyle(oldstyle);
		input_plugins[ftype]->SetScroll(scroll);
		input_plugins[ftype]->PlayStart();
		GUS_PlayStart();
		while (input_plugins[ftype]->GetSongStat())
		{
			GUS_Update();
		}
	}

	GUS_Exit();
	
	return EXIT_SUCCESS;
}

void _ShowUsage(void)
{
	printf("Usage: mtp [switches] file\n");
	printf("file is a MTPNG compatible song.\n");
	printf("\n Switches:\n");
	printf("   -o: (SS/MT only): use old pitchslides\n");
	printf("   -s: show scrolling display\n");
	printf("   -v: show versions of MTPng modules\n");

/* Amiga - */
	printf("   -r<file>: output as RAW\n");
      printf("   -a<file>: output as AIFF\n");
	printf("   -f<freq>: set mixing frequency\n");
	printf("   -i: info only, dont play\n");
        printf("   -t: terminate switches\n\n");
	exit(5);
}
