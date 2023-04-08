#include "types.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "sounddef.h"
#include "v2mconv.h"

const char *v2sources[] =
{
    "Velocity",
    "Modulation",
    "Breath",
    "Ctl #3",
    "Ctl #4",
    "Ctl #5",
    "Ctl #6",
    "Volume",
    "Amp EG",
    "EG 2",
    "LFO 1",
    "LFO 2",
    "Note",
};
const int v2nsources = sizeof(v2sources)/sizeof(char *);

// lr: in case you miss the arrays: look in sounddef.h
static unsigned char v2clipboard[v2soundsize];
static char v2clipname[256];

unsigned char *soundmem;
long          *patchoffsets;
unsigned char *editmem;
char          patchnames[128][32];
char          globals[v2ngparms];
int           v2version;
int           *v2vsizes;
int           *v2gsizes;
int           *v2topics2;
int           *v2gtopics2;
int           v2curpatch;

#if 1//def RONAN
char          speech[64][256];
char          *speechptrs[64];
#endif

void sdInit()
{
    soundmem = new unsigned char [smsize + v2soundsize];
    patchoffsets = (long *)soundmem;
    unsigned char *sptr = soundmem + 128*4;

    // printf("sound size: %d\n",v2nparms);
    char s[256];

    for (int i = 0; i < 129; i++)
    {
        if (i < 128)
        {
            patchoffsets[i] = (long)(sptr - soundmem);
            sprintf_s(s, "Init Patch #%03d", i);
            strcpy_s(patchnames[i], s);
        } else
            editmem = sptr;
        memcpy(sptr, v2initsnd, v2soundsize);
        sptr += v2soundsize;
    }
    for (int i = 0; i < v2ngparms; i++)
        globals[i] = v2initglobs[i];

    memcpy(v2clipboard, v2initsnd, v2soundsize);
    sprintf_s(v2clipname, "Init Patch");

    // init version control
    v2version = 0;
    for (int i = 0; i < v2nparms; i++)
        if (v2parms[i].version > v2version)
            v2version = v2parms[i].version;
    for (int i = 0; i < v2ngparms; i++)
        if (v2gparms[i].version > v2version)
            v2version = v2gparms[i].version;

    v2vsizes = new int[v2version + 1];
    v2gsizes = new int[v2version + 1];
    memset(v2vsizes, 0, (v2version + 1)*sizeof(int));
    memset(v2gsizes, 0, (v2version + 1)*sizeof(int));
    for (int i = 0; i < v2nparms; i++)
    {
        const V2PARAM &p = v2parms[i];
//        ATLASSERT(p.version<=v2version);
        for (int j = v2version; j >= p.version; j--)
            v2vsizes[j]++;
    }
    for (int i = 0; i < v2ngparms; i++)
    {
        const V2PARAM &p = v2gparms[i];
        //ATLASSERT(p.version<=v2version);
        for (int j = v2version; j >= p.version; j--)
            v2gsizes[j]++;
    }
//ATLASSERT(v2vsizes[v2version]==v2nparms);

    for (int i = 0; i <= v2version; i++)
    {
        // printf("size of version %d sound bank: %d params, %d globals\n",i,v2vsizes[i],v2gsizes[i]);
        v2vsizes[i] += 1 + 255*3;
    }

    v2topics2 = new int[v2ntopics];
    int p = 0;
    for (int i = 0; i < v2ntopics; i++)
    {
        v2topics2[i] = p;
        p += v2topics[i].no;
    }

    v2gtopics2 = new int[v2ngtopics];
    p = 0;
    for (int i = 0; i < v2ngtopics; i++)
    {
        v2gtopics2[i] = p;
        p += v2gtopics[i].no;
    }

#if 1//def RONAN
    memset(speech, 0, 64*256);
    for (int i = 0; i < 64; i++)
        speechptrs[i] = speech[i];

    strcpy_s(speech[0], "!DHAX_ !kwIH_k !br4AH_UHn !fAA_ks !jAH_mps !OW!vER_ !DHAX_ !lEY!zIY_ !dAA_g ");
#endif
}

void sdClose()
{
    delete[] soundmem;
    delete[] v2vsizes;
    delete[] v2gsizes;
    delete[] v2topics2;
    delete[] v2gtopics2;
}

void sdCopyPatch()
{
    memcpy(v2clipboard, soundmem + 128*4 + v2curpatch*v2soundsize, v2soundsize);
    strcpy_s(v2clipname, patchnames[v2curpatch]);
}

void sdPastePatch()
{
    memcpy(soundmem + 128*4 + v2curpatch*v2soundsize, v2clipboard, v2soundsize);
    strcpy_s(patchnames[v2curpatch], v2clipname);
}

void sdInitPatch()
{
    memcpy(soundmem + 128*4 + v2curpatch*v2soundsize, v2initsnd, v2soundsize);
    sprintf_s(patchnames[v2curpatch], "Init Patch #%03d", v2curpatch);
}
