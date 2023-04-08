#pragma once

#include <stdint.h>

struct ssbase
{
    const uint8_t   *patchmap;
    const uint8_t   *globals;
    uint32_t    timediv;
    uint32_t    timediv2;
    uint32_t    maxtime;
    const uint8_t   *gptr;
    uint32_t  gdnum;
    struct _basech
    {
        uint32_t    notenum;
        const uint8_t      *noteptr;
        uint32_t    pcnum;
        const uint8_t      *pcptr;
        uint32_t    pbnum;
        const uint8_t      *pbptr;
        struct _bcctl
        {
            uint32_t       ccnum;
            const uint8_t  *ccptr;
        } ctl[7];
    } chan[16];

    const uint8_t *mididata;
    int midisize;
    int patchsize;
    int globsize;
    int maxp;
    const uint8_t *newpatchmap;

    const uint8_t *speechdata;
    int spsize;

};

// gives version deltas
int CheckV2MVersion(const unsigned char *inptr, const int inlen, ssbase& base);

// converts V2M to newest version
void ConvertV2M(const unsigned char *inptr, const int inlen, unsigned char **outptr, int *outlen);

extern const char * const v2mconv_errors[];
