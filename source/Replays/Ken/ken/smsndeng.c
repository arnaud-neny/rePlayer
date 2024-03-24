// This file has been modified from Ken Silverman's original release!

// EMSCRIPTEN note: some songs still sound like garbage (see ENTER.SND)
// but the problem does not seem to occur in the original old DOS player..
// overall the impl looks somewhat sketchy (initializing chars with values
// that would require unsigned chars, etc) and probably the old compilers
// just treated some broken scenario differently than the contemporary ones..
// I don't care enough to put extra time in it trying to fix this

#include <fcntl.h>
#include <io.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <stdio.h>
#include <math.h>

#include "ken.h"

	//.SM and .SND player code based off of PPADLIB2.C
	//(PPADLIB2.C is based off of PPADLIB.C)
	//
	//This code must have: "#define KEXTENDEDADLIB 1"
	//defined in ADLIBEMU.C so it can play all channels!
	//
	//Channel mapping:
	//           CT-640:  Adlib:
	//   Track 0:   6  ->  0-5 of synth #0
	//   Track 1:   4  ->  0-3 of synth #1 (9-13)
	//   Track 2:   2  ->  6-8 of synth #0 (drums only!)


#if !defined(max) && !defined(__cplusplus)
#define max(a,b)  (((a) > (b)) ? (a) : (b))
#endif
#if !defined(min) && !defined(__cplusplus)
#define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif


static _inline long scale (long a, long d, long c)
{
	return a * d /c;
}

#define log2(a) (log(a)/log(2))

long smsndsamplerate, smsndnumspeakers = 2, smsndbytespersample = 2;

extern float lvol[18], rvol[18];
extern long lplc[18], rplc[18];
extern void adlibinit(long samplerate, long numspeakers, long bytespersample);
extern void adlib0(long index, long value);
extern void adlibgetsample(void *sndptr, long numsamples);

static char chantrack[18];
static long nownote, waitcnt, startsnd = 27, loopcnt;
static long speed = 240, nspeed, musicstatus = 0, minicnt;
static unsigned char chords[1500][13], inster[3], drumstat;	// EMSCRIPTEN changed to unsigned
static long freq[63] =
{
	0,
	2390,2411,2434,2456,2480,2506,2533,2562,2592,2625,2659,2695,
	3414,3435,3458,3480,3504,3530,3557,3586,3616,3649,3683,3719,
	4438,4459,4482,4504,4528,4554,4581,4610,4640,4673,4707,4743,
	5462,5483,5506,5528,5552,5578,5605,5634,5664,5697,5731,5767,
	6486,6507,6530,6552,6576,6602,6629,6658,6688,6721,6755,6791,
	7510
};
/*
static char inst[30][11] =
{
	1,1,210,124,0,1,25,210,124,0,4,
	32,2,242,51,0,48,14,194,16,0,10,
	209,1,225,195,0,210,30,82,176,0,12,
	177,2,128,255,0,35,41,209,182,0,0,
	32,3,128,255,0,33,20,240,255,0,8,
	33,0,113,174,0,49,22,227,78,0,14,
	97,0,83,62,0,113,22,49,78,0,14,
	97,0,240,255,0,65,45,241,255,0,14,
	193,0,130,255,0,225,30,82,255,0,0,
	33,4,226,0,0,36,208,242,0,0,0,
	21,4,242,114,0,116,79,242,96,0,8,
	0,4,240,248,0,0,16,240,255,0,10,
	145,2,145,255,0,2,18,178,182,0,10,
	49,0,65,45,0,36,84,53,253,0,14,
	0,0,214,191,0,0,13,168,188,0,0,
	65,2,210,124,0,65,25,100,0,0,2,
	129,0,211,255,0,160,35,243,0,0,0,
	193,0,246,181,0,3,6,14,240,0,4,
	1,4,240,255,0,4,44,241,255,0,0,
	161,0,194,31,0,161,28,98,31,0,14,
	97,0,221,22,0,49,26,50,22,0,12,
	97,0,181,46,0,113,30,81,78,0,14,
	33,4,139,14,0,48,15,135,23,0,2,
	65,2,240,255,0,66,38,241,255,0,0,
	1,0,132,255,0,33,60,233,29,0,0,
	177,0,131,255,0,49,25,209,182,0,14,
	192,3,243,248,0,192,18,240,255,0,8,
	65,4,132,255,0,99,30,240,255,0,14,
	33,0,51,49,0,33,16,17,60,0,10,
	32,0,132,255,0,32,12,132,255,0,8
};*/
static char inst[30][11] =					// EMSCRIPTEN seems this is actually signed
{
	1,1,-46,124,0,1,25,-46,124,0,4,
	32,2,-14,51,0,48,14,-62,16,0,10,
	-47,1,-31,-61,0,-46,30,82,-80,0,12,
	-79,2,-128,-1,0,35,41,-47,-74,0,0,
	32,3,-128,-1,0,33,20,-16,-1,0,8,
	33,0,113,-82,0,49,22,-29,78,0,14,
	97,0,83,62,0,113,22,49,78,0,14,
	97,0,-16,-1,0,65,45,-15,-1,0,14,
	-63,0,-126,-1,0,-31,30,82,-1,0,0,
	33,4,-30,0,0,36,-48,-14,0,0,0,
	21,4,-14,114,0,116,79,-14,96,0,8,
	0,4,-16,-8,0,0,16,-16,-1,0,10,
	-111,2,-111,-1,0,2,18,-78,-74,0,10,
	49,0,65,45,0,36,84,53,-3,0,14,
	0,0,-42,-65,0,0,13,-88,-68,0,0,
	65,2,-46,124,0,65,25,100,0,0,2,
	-127,0,-45,-1,0,-96,35,-13,0,0,0,
	-63,0,-10,-75,0,3,6,14,-16,0,4,
	1,4,-16,-1,0,4,44,-15,-1,0,0,
	-95,0,-62,31,0,-95,28,98,31,0,14,
	97,0,-35,22,0,49,26,50,22,0,12,
	97,0,-75,46,0,113,30,81,78,0,14,
	33,4,-117,14,0,48,15,-121,23,0,2,
	65,2,-16,-1,0,66,38,-15,-1,0,0,
	1,0,-124,-1,0,33,60,-23,29,0,0,
	-79,0,-125,-1,0,49,25,-47,-74,0,14,
	-64,3,-13,-8,0,-64,18,-16,-1,0,8,
	65,4,-124,-1,0,99,30,-16,-1,0,14,
	33,0,51,49,0,33,16,17,60,0,10,
	32,0,-124,-1,0,32,12,-124,-1,0,8
};



long kind;
static long chord;

static void setup3812 ()
{
	adlib0(0x1,0);  //clear test stuff
	adlib0(0x4,0);  //reset
	adlib0(0x8,0);  //2-operator synthesis
	adlib0(0xbd,drumstat); //set to rhythm mode
}

static void noteoff (long chan, long freq)
{
	if (chan >= 9) chan += 256-9;
	adlib0(0xa0+chan,freq&255);
	adlib0(0xb0+chan,(freq>>8)&223);
}

static void noteon (long chan, long freq)
{
	if (chan >= 9) chan += 256-9;
	adlib0(0xa0+chan,0);
	adlib0(0xb0+chan,0);
	adlib0(0xa0+chan,freq&255);
	adlib0(0xb0+chan,(freq>>8)|32);
}

static void setinst (long chan, char v0, char v1, char v2, char v3, char v4, char v5,
	char v6, char v7, char v8, char v9, char v10)
{
	long offs;

	if (chan >= 9) chan += 256-9;
	adlib0(0xa0+chan,0);
	adlib0(0xb0+chan,0);
	adlib0(0xc0+chan,v10);
	switch(chan&255)
	{
		case 0: offs = 0; break;
		case 1: offs = 1; break;
		case 2: offs = 2; break;
		case 3: offs = 8; break;
		case 4: offs = 9; break;
		case 5: offs = 10; break;
		case 6: offs = 16; break;
		case 7: offs = 17; break;
		case 8: offs = 18; break;
	}
	if (chan >= 9) offs += 256;
	adlib0(0x20+offs,v5);
	adlib0(0x40+offs,v6);
	adlib0(0x60+offs,v7);
	adlib0(0x80+offs,v8);
	adlib0(0xe0+offs,v9);
	offs += 3;
	adlib0(0x20+offs,v0);
	adlib0(0x40+offs,v1);
	adlib0(0x60+offs,v2);
	adlib0(0x80+offs,v3);
	adlib0(0xe0+offs,v4);
}

static void drum (long drumnum, long freq)
{
	long chan;

	if (drumnum == 16) chan = 6;
	if ((drumnum == 8) || (drumnum == 1)) chan = 7;
	if ((drumnum == 4) || (drumnum == 2)) chan = 8;
	adlib0(0xa0+chan,freq&255);
	adlib0(0xb0+chan,(freq>>8)&223);
	adlib0(0xbd,drumstat & (255-drumnum));
	drumstat |= drumnum;
	adlib0(0xbd,drumstat);
}

void smsndmusicon ()
{
	char ch, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10;
	long i;

	setup3812();
	if (kind == 2) i = startsnd; else i = 0;
	v0 = inst[i][0]; v1 = inst[i][1]; v2 = inst[i][2]; v3 = inst[i][3];
	v4 = inst[i][4]; v5 = inst[i][5]; v6 = inst[i][6]; v7 = inst[i][7];
	v8 = inst[i][8]; v9 = inst[i][9]; v10 = inst[i][10];
	inster[0] = i;
	for(i=0;i<6;i++) setinst(i,v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10);
	for(i=9;i<18;i++) setinst(i,v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10);
	v0 = inst[20][0]; v1 = (inst[20][1]<<1)&63; v2 = inst[20][2]; v3 = inst[20][3];
	v4 = inst[20][4]; v5 = inst[20][5]; v6 = inst[20][6]; v7 = inst[20][7];
	v8 = inst[20][8]; v9 = inst[20][9]; v10 = inst[20][10];
	inster[1] = 20;
	inster[2] = 20;
	setinst(6,0,3,0xd6,0x68,0,0,10,0xd6,0x68,0,4); //bass
	setinst(7,0,2,0xd8,0x4f,0,0,3,0xf8,0xff,0,14); //snare & hihat
	setinst(8,0,63,0xf5,0xc8,0,0,0,0xd6,0x88,0,0); //topsymb & tom
	noteoff(6,600);
	noteoff(7,400);
	noteoff(8,5510);
	musicstatus = 1;
}

void smsndmusicoff ()
{
	long i;

	for(i=0;i<18;i++) noteoff(i,0);
#if 0
	for(i=0;i<9;i++)
	{
		adlib0(0xa0+i,0); adlib0(0xb0+i,0);
		adlib0(0x1a0+i,0); adlib0(0x1b0+i,0);
	}
#endif
	musicstatus = 0;
}

static int watrind, wrand[37] =
{
	0x038c,0x5921,0x27ff,0x3272,0x64ed,0x69d7,0x7518,0x6cd9,
	0x4f4e,0x7f13,0x6595,0x7a13,0x30dc,0x381b,0x2963,0x23fc,
	0x4593,0x3f06,0x08f6,0x02bf,0x029c,0x06e5,0x4606,0x74e0,
	0x5b6d,0x40b0,0x4c0a,0x0c05,0x3c9f,0x32f2,0x6c75,0x7a87,
	0x588e,0x6034,0x22b3,0x4dbc,0x21b3,
};
static int watrand () { watrind++; return(wrand[watrind-1]); }
static void randoinsts ()
{
	long i, j, k;
	float f;

	watrind = 0;
	if ((smsndnumspeakers == 2) && (kind == 1))
	{
		j = (watrand()&2)-1; k = 0;
		for(i=0;i<18;i++)
		{
			if ((i == 0) || (chantrack[i] != chantrack[i-1]))
			{
				f = (float)watrand()/32768.0;
				if (j > 0)
				{
						//lvol[i] < rvol[i]
					lvol[i] = log2(f+1);
					rvol[i] = log2(3-f);
					lplc[i] = watrand()&255;
					rplc[i] = 0;
				}
				else
				{
						//lvol[i] > rvol[i]
					lvol[i] = log2(3-f);
					rvol[i] = log2(f+1);
					lplc[i] = 0;
					rplc[i] = watrand()&255;
				}
				j = -j;
				if (i < 6) k++;
			}
			else
			{
				lvol[i] = lvol[i-1]; rvol[i] = rvol[i-1];
				lplc[i] = lplc[i-1]; rplc[i] = rplc[i-1];
			}
		}
	}
}

static void smnexttic ()
{
	long i, j, k, templong;
	char ch, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10;

	waitcnt--; if (waitcnt > 0) return;

	if (nownote >= chord) { nownote = -1; loopcnt++; }
	nownote++;

	for(k=0;k<18;k++) noteoff(k,0);

	if (kind == 2)
		for(k=0;k<4;k++)
			noteon(k,freq[chords[nownote][k]]);
	if (kind == 1)
	{
		for(k=0;k<12;k++)
		{
			if ((inster[0] != 14) && (k < 6) && (chords[nownote][k] >= 1) && (chords[nownote][k] <= 61))
				noteon(k,freq[chords[nownote][k]]);
			if ((inster[1] != 14) && (k >= 6) && (k < 10) && (chords[nownote][k] >= 1) && (chords[nownote][k] <= 61))
				noteon(k+3,freq[chords[nownote][k]]);
			if (((k < 6) && (inster[0] == 14)) || ((k >= 6) && (k < 10) && (inster[1] == 14)) || ((k >= 10) && (inster[2] == 14)))
			{
				switch(chords[nownote][k]+35)
				{
					case 36: drum(16,600); break;
					case 38: drum(16,800); break;
					case 40: drum(8,400); break;
					case 41: case 72: case 74: drum(8,600); break;
					case 43: drum(8,800); break;
					case 45: case 50: case 52: case 54: case 56: case 58: case 60: case 62: case 61: case 63: drum(2,600); break;
					case 47: case 48: case 66: case 68: case 70: drum(1,600); break;
					case 53: drum(2,900); break;
					case 55: drum(2,850); break;
					case 57: drum(2,800); break;
					case 59: drum(2,750); break;
					case 64: drum(4,5500); break;
					case 65: case 71: case 76: drum(4,5400); break;
					case 67: drum(4,5320); break;
					case 69: drum(4,5280); break;
					case 73: case 75: drum(2,7500); break;
					case 77: case 81: case 91: drum(4,5350); break;
					case 79: drum(4,5300); break;
					case 78: drum(4,5700); break;
					case 80: case 88: drum(4,5550); break;
					case 82: drum(4,5480); break;
					case 83: drum(4,6600); break;
					case 84: drum(4,6500); break;
					case 86: case 93: drum(4,5750); break;
					case 89: drum(4,5450); break;
					case 95: drum(4,7600); break;
					case 96: drum(4,7500); break;
				}
			}
			if (chords[nownote][k] >= 100)
			{
				i = chords[nownote][k]-100;
				v0 = inst[i][0]; v1 = inst[i][1]; v2 = inst[i][2];
				v3 = inst[i][3]; v4 = inst[i][4]; v5 = inst[i][5];
				v6 = inst[i][6]; v7 = inst[i][7]; v8 = inst[i][8];
				v9 = inst[i][9]; v10 = inst[i][10];
				if (k < 6)
				{
					inster[0] = i;
					for(i=0;i<6;i++) setinst(i,v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10);
				}
				if ((k >= 6) && (k < 10))
				{
					inster[1] = i;
					v1 = (v1&192)+((v1+10)&63);
					for(i=9;i<13;i++) setinst(i,v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10);
				}
				if ((k >= 10) && (k < 12)) inster[2] = i;
			}
		}
	}
	waitcnt = chords[nownote][12];
}

long smsndload (Loader* loader)
{
	long i, j, k, l;
	char buffer[15], instr[12];
	const char* filename = loader->GetName(loader);

	adlibinit(smsndsamplerate,smsndnumspeakers,smsndbytespersample);
	nspeed = ((speed*13)>>5);  //This is an ugly hack!
	nownote = -1; waitcnt = 0; drumstat = 32; minicnt = 0; loopcnt = 0;

	kind = 0;
	if (strchr(filename,'.'))
	{
		if (strstr(filename,".sm") || strstr(filename,".SM")) kind = 1;
		if (strstr(filename,".snd") || strstr(filename,".SND")) kind = 2;
	}
	if (kind == 0) return 0;

	memset(chords,0,sizeof(chords));

	if (kind == 1)
	{
		for(i=0;i<6;i++) chantrack[i] = 0;
		for(i=6;i<9;i++) chantrack[i] = 1;
		for(i=9;i<18;i++) chantrack[i] = 2;

		chord = 0;
		while (loader->Read(loader,&buffer,13)==0)
		{
			if (chord == 1500)
				return 0;
			for(i=0;i<12;i++)
			{
				if (buffer[i] >= 100)
				{
					if ((i >= 0) && (i < 6)) for(j=0;j<6;j++) instr[j] = buffer[i]-100;
					if ((i >= 6) && (i < 10)) for(j=6;j<10;j++) instr[j] = buffer[i]-100;
					if ((i >= 10) && (i < 12)) for(j=10;j<12;j++) instr[j] = buffer[i]-100;
				}
				if ((buffer[i] >= 36) && (buffer[i] <= 96)) chords[chord][i] = buffer[i]-35;
				if (buffer[i] >= 100) chords[chord][i] = buffer[i];
			}
			chords[chord][12] = buffer[12];
			chord++;
		}
	}
	if (kind == 2)
	{
		for(i=0;i<18;i++) chantrack[i] = 0;

		chord = 0;
		while (loader->Read(loader,&buffer,7)==0)
		{
			if (chord == 1500)
				return 0;
			chords[chord][0] = buffer[0] - 58;
			chords[chord][1] = buffer[1] - 58;
			chords[chord][2] = buffer[2] - 58;
			chords[chord][3] = buffer[3] - 58;
			chords[chord][4] = buffer[0] - 58;
			chords[chord][5] = buffer[1] - 58;
			chords[chord][6] = buffer[2] - 58;
			chords[chord][7] = buffer[3] - 58;
			chords[chord][8] = buffer[0] - 58;
			chords[chord][9] = buffer[1] - 58;
			chords[chord][10] = buffer[2] - 58;
			chords[chord][11] = buffer[3] - 58;
			chords[chord][12] = (buffer[4] - 48)<<2;
			chord++;
		}
		for(i=0;i<chord;i++)
			for(j=0;j<12;j++)
				if (chords[i][j] > 61) chords[i][j] = 0;
	}
	if (chord==0)
		return 0;

	randoinsts();

	for(i=j=0;i<chord;i++) j += (long)chords[i][12];
	return(scale(j,1000,nspeed));
}

long smsndrendersound (void *dasnd, long numbytestoprocess)
{
	long i, prepcnt;

	prepcnt = numbytestoprocess;
	while (prepcnt > 0)
	{
		i = min(prepcnt,(minicnt/nspeed+4)&~3);
		adlibgetsample(dasnd,i);
		dasnd = (void *)(((uintptr_t)dasnd)+i);
		prepcnt -= i;

		minicnt -= nspeed*i;
		while (minicnt < 0)
		{
			minicnt += smsndsamplerate*smsndnumspeakers*smsndbytespersample;
			smnexttic();
		}
	}
	return(loopcnt);
}

void smsndseek (long seektoms)
{
	long i, j, k, l, templong;
	char v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10;

	smsndmusicoff();

	i = scale(seektoms,nspeed,1000); nownote = j = 0;
	while ((j < i) && (nownote < chord))
		{ j += (long)chords[nownote][12]; nownote++; }
	if (nownote >= chord) nownote = 0;

	if (kind == 1)
		for(l=0;l<nownote;l++)
			for(k=0;k<12;k++)
				if (chords[l][k] >= 100)
				{
					i = chords[l][k]-100;
					v0 = inst[i][0]; v1 = inst[i][1]; v2 = inst[i][2];
					v3 = inst[i][3]; v4 = inst[i][4]; v5 = inst[i][5];
					v6 = inst[i][6]; v7 = inst[i][7]; v8 = inst[i][8];
					v9 = inst[i][9]; v10 = inst[i][10];
					if (k < 6)
					{
						inster[0] = i;
						for(i=0;i<6;i++) setinst(i,v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10);
					}
					if ((k >= 6) && (k < 10))
					{
						inster[1] = i;
						v1 = (v1&192)+((v1+10)&63);
						for(i=9;i<13;i++) setinst(i,v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10);
					}
					if ((k >= 10) && (k < 12)) inster[2] = i;
				}

	waitcnt = 0; minicnt = 0; musicstatus = 1;
}
