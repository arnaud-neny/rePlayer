// This file has been modified from Ken Silverman's original release!

// (based on a plain C impl that Ken had kindly supplied to replace
// the old x86 ASM based impl - thanks again :-) )

#ifndef EMSCRIPTEN
#include <basetsd.h>
#include <intrin.h>
#include <io.h>
#define ERR_CODE -1
#else
#include <unistd.h>
#define ERR_CODE 0	/* sticking to old API semantics to match those of the other players */
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <sys\types.h>
#include <sys\stat.h>

#include "ken.h"

#define NUMCHANNELS 16
#define MAXWAVES 256
#define MAXTRACKS 256
#define MAXNOTES 8192
#define MAXEFFECTS 16
#define PI 3.14159265358979323


#ifdef EMSCRIPTEN
#define _inline
#define O_BINARY 0

#define max(a,b)  (((a) > (b)) ? (a) : (b))
#define min(a,b)  (((a) < (b)) ? (a) : (b))

typedef int INT_PTR;

long long __emul(long long a, long b) {
	return a * b;
}
#endif

long scale (long long a, long d, long c) {
	return (a * d)/c;
}

	//Actual sound parameters after initsb was called
int kdmsamplerate;

	//KWV wave variables
static int numwaves;
typedef struct { char instname[16]; int wavleng, repstart, repleng, finetune; } kwv_t;
static kwv_t kwv[MAXWAVES];

	//Other useful wave variables
static int totsndbytes;
static INT_PTR wavoffs[MAXWAVES];

	//Effects array
static int eff[MAXEFFECTS][256];

	//KDM song variables:
static int kdmversionum, numnotes, numtracks;
static unsigned char trinst[MAXTRACKS], trquant[MAXTRACKS];
static unsigned char trvol1[MAXTRACKS], trvol2[MAXTRACKS];
static int nttime[MAXNOTES];
static unsigned char nttrack[MAXNOTES], ntfreq[MAXNOTES];
static unsigned char ntvol1[MAXNOTES], ntvol2[MAXNOTES];
static unsigned char ntfrqeff[MAXNOTES], ntvoleff[MAXNOTES], ntpaneff[MAXNOTES];

	//Other useful song variables:
static int timecount, notecnt, musicstatus, musicrepeat, loopcnt;

	//These shared with K.ASM:
static int kdmasm1, kdmasm3;
static INT_PTR kdmasm2, kdmasm4;
static char qualookup[512*16];

static unsigned char *snd = NULL;
static int kdminited = 0;

#define MAXSAMPLESTOPROCESS 32768
static int stemp[MAXSAMPLESTOPROCESS];

	//Sound reading information
static int splc[NUMCHANNELS], sinc[NUMCHANNELS];
static INT_PTR soff[NUMCHANNELS];
static int svol1[NUMCHANNELS], svol2[NUMCHANNELS];
static int volookup[NUMCHANNELS<<9];
static int swavenum[NUMCHANNELS];
static int frqeff[NUMCHANNELS], frqoff[NUMCHANNELS];
static int voleff[NUMCHANNELS], voloff[NUMCHANNELS];
static int paneff[NUMCHANNELS], panoff[NUMCHANNELS];

static int frqtable[256];
static int ramplookup[64];

	//Interpolation quality:
	//  1: nearest interpolation / 'lo' quality / stereolocomb() in classic KDM player
	//  2: linear  interpolation / 'hi' quality / stereohicomb() in classic KDM player
	//>=3: new higher quality! Suggest: 4 or 8. Above 8 not recommended - slower, but does seem to work up to ~40
#define NUMCOEF 2 //# filter coefficients.

#define LCOEFPREC 5    //number of fractional bits of precision: 5 is ok
#define COEFRANG 32757 //(32757(sic)) DON'T make this > 32757(sic) or else it'll overflow (windowed sinc func goes over 1.f very slightly)
static short coef[NUMCOEF<<LCOEFPREC]; //Sound re-scale filter coefficients
static void initfilters (void)
{
	float f, f2, f3, f4;
	int i, j, k;

		//Generate polynomial filter - fewer divides algo. (See PIANO.C for derivation)
	f4 = 1.0 / (float)(1<<LCOEFPREC);
	for(j=0;j<NUMCOEF;j++)
	{
		for(k=0,f3=1.0;k<j;k++) f3 *= (float)(j-k);
		for(k=j+1;k<NUMCOEF;k++) f3 *= (float)(j-k);
		f3 = COEFRANG / f3; f = ((NUMCOEF-1)>>1);
		for(i=0;i<(1<<LCOEFPREC);i++,f+=f4)
		{
			for(k=0,f2=f3;k<j;k++) f2 *= (f-(float)k);
			for(k=j+1;k<NUMCOEF;k++) f2 *= (f-(float)k);
			coef[i*NUMCOEF+j] = (short)f2;
		}
	}
}

static int stereohicomb (int dummy, int *voloffs, int samps, int sinc, int splc, int *stemp)
{
	unsigned char *ucptr;
	int i, j, k, v[max(NUMCOEF,2)];

	for(i=0;i<samps;i++)
	{
		ucptr = (unsigned char *)(kdmasm4 + (splc>>12));
#if (NUMCOEF == 1)
		j = (int)ucptr[0]; //stereolocomb (nearest sampling, for those who like aliased checkerboards in the sound domain ;P)
#elif (NUMCOEF == 2)
		v[0] = (int)ucptr[0];
		v[1] = (int)ucptr[1]; //technically this sample can be out of bounds. Old KDM player got away with it because the sample buffer is padded w/2 bytes, but would still have clicks.. :/
			//stereohicomb (linear interpolation):
		j = (qualookup[(splc&0xf00)*2 + ((v[0]-v[1])&511)] + v[0])&255; //Simulate exact output of original KDM player: uses only top 4 bits of subsample fraction and weird lookup table
		//j = (((v[1]-v[0])*(splc&4095))>>12) + v[0]; //above, w/o lookup table; negligibly better quality
#else
			//grab next NUMCOEF samples with proper looping..
		int nsplc = splc; INT_PTR nkdmasm4 = kdmasm4;
		for(k=0;k<NUMCOEF;k++)
		{
			v[k] = (int)*(unsigned char *)(nkdmasm4 + (nsplc>>12)); nsplc += 4096;
			if (nsplc >= 0) { if (!kdmasm1) { for(;k<NUMCOEF;k++) v[k] = 128; break; } nkdmasm4 = kdmasm2; nsplc -= kdmasm3; }
		}
			//proper filter!
		short *soef = &coef[((splc&4095)>>(12-LCOEFPREC))*NUMCOEF];
		j = 0; for(k=0;k<NUMCOEF;k++) j += v[k]*(int)soef[k];
		j = min(max(j>>15,0),255);
#endif
		stemp[i*2  ] += voloffs[j*2  ];
		stemp[i*2+1] += voloffs[j*2+1];
		splc += sinc; if (splc < 0) continue;

		if (!kdmasm1) break; //repleng; non-looped sound exits when done
		kdmasm4 = kdmasm2; //wavoffs[daswave]+kwv[daswave].repstart+kwv[daswave].repleng; adjust offset
		splc -= kdmasm3; //kwv[daswave].repleng<<32; adjust splc so ends when >= 0
	}
	return(splc);
}

static void bound2short (int bytespertic, int *stemp, short *sptr)
{
	int i, j;
	for(i=bytespertic*2-1;i>=0;i--)
	{
		j = stemp[i]; stemp[i] = 32768;
		if (j < 0) j = 0;
		if (j > 65535) j = 65535;
		sptr[i] = (short)(j-32768);
	}
}

static _inline void fsin (int *a) { (*a) = (int)(sin((float)(*a)*(PI*1024.f))*16384.f); }

static _inline void calcvolookupstereo (int *lut, int p0, int i0, int p1, int i1)
{
	int i;
	for(i=0;i<256;i++) { lut[i*2] = p0; lut[i*2+1] = p1; p0 += i0; p1 += i1; }
}

static _inline int mulscale16 (int a, int d) { return (int)(__emul(a,d)>>16); }
static _inline int mulscale24 (int a, int d) { return (int)(__emul(a,d)>>24); }
static _inline int mulscale30 (int a, int d) { return (int)(__emul(a,d)>>30); }
static _inline void clearbuf (int *d, int c, int a) { for(c--;c>=0;c--) d[c] = a; }
static _inline void copybuf (int *s, int *d, int c) { for(c--;c>=0;c--) d[c] = s[c]; }

void kdmmusicon (void)
{
	notecnt = 0;
	timecount = nttime[notecnt];
	musicrepeat = 1;
	musicstatus = 1;
}

void kdmmusicoff (void)
{
	int i;

	musicstatus = 0;
	for(i=0;i<NUMCHANNELS;i++) splc[i] = 0;
	musicrepeat = 0;
	timecount = 0;
	notecnt = 0;
}

	//Unlike KDM, this function doesn't suck ALL memory
static int loadwaves (Loader* loader)
{
	int i, j, dawaversionum;

	if (snd) return(1);

	if (!loader) return(0);

	if (loader->Read(loader, &dawaversionum, 4)) goto fail;
	if (dawaversionum != 0) goto fail;

	totsndbytes = 0;
	if (loader->Read(loader, &numwaves, 4)||numwaves<=0||numwaves>256) goto fail;
	if (loader->Read(loader, kwv, numwaves * sizeof(kwv_t))) goto fail;
	memset(&kwv[numwaves],0,(MAXWAVES-numwaves)*sizeof(kwv_t));
	for(i=0;i<MAXWAVES;i++)
		{ wavoffs[i] = totsndbytes; totsndbytes += kwv[i].wavleng; }

	if (!(snd = (char *)malloc(totsndbytes+2))) goto fail;

	for(i=0;i<MAXWAVES;i++) wavoffs[i] += ((INT_PTR)snd);
	if (loader->Read(loader, &snd[0], totsndbytes)) goto fail;
	snd[totsndbytes] = snd[totsndbytes+1] = 128;
	loader->Close(loader);
	return(1);
fail:
	if (snd) free(snd); snd = NULL;
	loader->Close(loader);
	return(0);
}

void initkdmeng (void)
{
	int i, j, k;

	j = (((11025L*2093)/kdmsamplerate)<<13);
	for(i=1;i<76;i++)
	{
		frqtable[i] = j;
		j = mulscale30(j,1137589835);  //(pow(2,1/12)<<30) = 1137589835
	}
	for(i=0;i>=-14;i--) frqtable[i&255] = (frqtable[(i+12)&255]>>1);

	timecount = notecnt = musicstatus = musicrepeat = 0;

	clearbuf(stemp,sizeof(stemp)>>2,32768L);
	for(i=0;i<256;i++)
		for(j=0;j<16;j++)
		{
			qualookup[(j<<9)+i] = (((-i)*j+8)>>4);
			qualookup[(j<<9)+i+256] = (((256-i)*j+8)>>4);
		}
#if (NUMCOEF >= 2)
	initfilters();
#endif
	for(i=0;i<(kdmsamplerate>>11);i++)
	{
		j = 1536 - (i<<10)/(kdmsamplerate>>11);
		fsin(&j);
		ramplookup[i] = ((16384-j)<<1);
	}

	for(i=0;i<256;i++)
	{
		j = i*90; fsin(&j);
		eff[0][i] = 65536+j/9;
		eff[1][i] = min(58386+((i*(65536-58386))/30),65536);
		eff[2][i] = max(69433+((i*(65536-69433))/30),65536);
		j = (i*2048)/120; fsin(&j);
		eff[3][i] = 65536+(j<<2);
		j = (i*2048)/30; fsin(&j);
		eff[4][i] = 65536+j;
		switch((i>>1)%3)
		{
			case 0: eff[5][i] = 65536; break;
			case 1: eff[5][i] = 65536*330/262; break;
			case 2: eff[5][i] = 65536*392/262; break;
		}
		eff[6][i] = min((i<<16)/120,65536);
		eff[7][i] = max(65536-(i<<16)/120,0);
	}

	kdmmusicoff();
}
void freekdmeng(void)
{
	if (snd) free(snd); snd = NULL;
}

static void startwave (int wavnum, int dafreq, int davolume1, int davolume2, int dafrqeff, int davoleff, int dapaneff)
{
	int i, j, chanum;

	if ((davolume1|davolume2) == 0) return;

	chanum = 0;
	for(i=NUMCHANNELS-1;i>0;i--)
		if (splc[i] > splc[chanum])
			chanum = i;

	splc[chanum] = 0;     //Disable channel temporarily for clean switch

	calcvolookupstereo(&volookup[chanum<<9],-(davolume1<<7),davolume1,-(davolume2<<7),davolume2);

	sinc[chanum] = dafreq;
	svol1[chanum] = davolume1;
	svol2[chanum] = davolume2;
	soff[chanum] = wavoffs[wavnum]+kwv[wavnum].wavleng;
	splc[chanum] = -(kwv[wavnum].wavleng<<12);              //splc's modified last
	swavenum[chanum] = wavnum;
	frqeff[chanum] = dafrqeff; frqoff[chanum] = 0;
	voleff[chanum] = davoleff; voloff[chanum] = 0;
	paneff[chanum] = dapaneff; panoff[chanum] = 0;
}

int kdmload (Loader* loader)
{
	int i;

	if (!kdminited)
	{
		initkdmeng();
		{
			char tbuf[2048];
			strcpy(tbuf, loader->GetName(loader));
			int j = -1;
			for(i=0;tbuf[i];i++) if ((tbuf[i] == '/') || (tbuf[i] == '\\')) j = i;
			strcpy(&tbuf[j+1],"waves.kwv");
			if (!loadwaves(loader->Open(loader, tbuf))) return(ERR_CODE);
		}
		kdminited = 1;
	}
	if (!snd) return(ERR_CODE);

	kdmmusicoff();
	if (loader->Read(loader,&kdmversionum, 4)||kdmversionum!=0) return(ERR_CODE);
	if (loader->Read(loader,&numnotes,4)||numnotes<=0||numnotes>8192) return (ERR_CODE);
	if (loader->Read(loader,&numtracks,4)||numtracks<=0||numtracks>256) return (ERR_CODE);
	if (loader->Read(loader,trinst,numtracks)) return (ERR_CODE);
	if (loader->Read(loader,trquant,numtracks)) return (ERR_CODE);
	if (loader->Read(loader,trvol1,numtracks)) return (ERR_CODE);
	if (loader->Read(loader,trvol2,numtracks)) return (ERR_CODE);
	if (loader->Read(loader,nttime,numnotes<<2)) return (ERR_CODE);
	if (loader->Read(loader,nttrack,numnotes)) return (ERR_CODE);
	if (loader->Read(loader,ntfreq,numnotes)) return (ERR_CODE);
	if (loader->Read(loader,ntvol1,numnotes)) return (ERR_CODE);
	if (loader->Read(loader,ntvol2,numnotes)) return (ERR_CODE);
	if (loader->Read(loader,ntfrqeff,numnotes)) return (ERR_CODE);
	if (loader->Read(loader,ntvoleff,numnotes)) return (ERR_CODE);
	if (loader->Read(loader,ntpaneff, numnotes)) memset(ntpaneff, 0, numnotes);

	loopcnt = 0;
	return(scale(nttime[numnotes-1]-nttime[0],1000,120));
}

long kdmrendersound (char *dasnd, int numbytes)
{
	int i, j, k, voloffs1, voloffs2, *volut;
	int daswave, dasinc, dacnt, bytespertic, numsamplestoprocess;
	int ox, oy, x, y;
	int *sndptr, v1, v2;

	numsamplestoprocess = (numbytes>>(2+2-2));		// hardcoded to stereo/16-bit samples
	bytespertic = (kdmsamplerate/120);
	for(dacnt=0;dacnt<numsamplestoprocess;dacnt+=bytespertic)
	{
		if (musicstatus > 0)    //Gets here 120 times/second
		{
			while ((notecnt < numnotes) && (timecount >= nttime[notecnt]))
			{
				j = trinst[nttrack[notecnt]];
				k = mulscale24(frqtable[ntfreq[notecnt]],kwv[j].finetune+748);

				if (ntvol1[notecnt] == 0)   //Note off
				{
					for(i=NUMCHANNELS-1;i>=0;i--)
						if (splc[i] < 0)
							if (swavenum[i] == j)
								if (sinc[i] == k)
									splc[i] = 0;
				}
				else                        //Note on
					startwave(j,k,ntvol1[notecnt],ntvol2[notecnt],ntfrqeff[notecnt],ntvoleff[notecnt],ntpaneff[notecnt]);

				notecnt++;
				if (notecnt >= numnotes) {
					loopcnt++;
					if (musicrepeat > 0)
						notecnt = 0, timecount = nttime[0];
				}
			}
			timecount++;
		}

		for(i=NUMCHANNELS-1;i>=0;i--)
		{
			if (splc[i] >= 0) continue;

			dasinc = sinc[i];

			if (frqeff[i] != 0)
			{
				dasinc = mulscale16(dasinc,eff[frqeff[i]-1][frqoff[i]]);
				frqoff[i]++; if (frqoff[i] >= 256) frqeff[i] = 0;
			}
			if ((voleff[i]) || (paneff[i]))
			{
				voloffs1 = svol1[i];
				voloffs2 = svol2[i];
				if (voleff[i])
				{
					voloffs1 = mulscale16(voloffs1,eff[voleff[i]-1][voloff[i]]);
					voloffs2 = mulscale16(voloffs2,eff[voleff[i]-1][voloff[i]]);
					voloff[i]++; if (voloff[i] >= 256) voleff[i] = 0;
				}

				if (paneff[i])
				{
					voloffs1 = mulscale16(voloffs1,131072-eff[paneff[i]-1][panoff[i]]);
					voloffs2 = mulscale16(voloffs2,eff[paneff[i]-1][panoff[i]]);
					panoff[i]++; if (panoff[i] >= 256) paneff[i] = 0;
				}
				calcvolookupstereo(&volookup[i<<9],-(voloffs1<<7),voloffs1,-(voloffs2<<7),voloffs2);
			}

			daswave = swavenum[i];
			volut = &volookup[i<<9];

			kdmasm1 = kwv[daswave].repleng;
			kdmasm2 = wavoffs[daswave]+kwv[daswave].repstart+kwv[daswave].repleng;
			kdmasm3 = (kwv[daswave].repleng<<12); //repsplcoff
			kdmasm4 = soff[i];
			splc[i] = stereohicomb(0L,volut,bytespertic,dasinc,splc[i],stemp);
			soff[i] = kdmasm4;

			if (splc[i] >= 0) continue;
			stereohicomb(0L,volut,kdmsamplerate>>11,dasinc,splc[i],&stemp[bytespertic<<1]);
		}

		for(i=(kdmsamplerate>>11)-1;i>=0;i--)
		{
			j = (i<<1);
			stemp[j+0] += mulscale16(stemp[j+1024]-stemp[j+0],ramplookup[i]);
			stemp[j+1] += mulscale16(stemp[j+1025]-stemp[j+1],ramplookup[i]);
		}
		j = (bytespertic<<1); k = ((kdmsamplerate>>11)<<1);
		copybuf(&stemp[j],&stemp[1024],k);
		clearbuf(&stemp[j],k,32768);

		bound2short(bytespertic,stemp,(short *)&dasnd[dacnt<<2]);
	}
	return loopcnt;
}

void kdmseek (long seektoms)	// resurrected from old version
{
	long i;

	for(i=0;i<NUMCHANNELS;i++) splc[i] = 0;

	i = scale(seektoms,120,1000)+nttime[0];

	notecnt = 0;
	while ((nttime[notecnt] < i) && (notecnt < numnotes)) notecnt++;
	if (notecnt >= numnotes) notecnt = 0;

	timecount = nttime[notecnt];
}
