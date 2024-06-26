/*
 * tfmx.c
 * File loader and UI for TFMX player.
 * jhp 29Feb96
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <stdio.h>
#include <string.h>
#include "tfmx.h"
#include "tfmx_audio.h"
#include "tfmx_iface.h"
#include "tfmx_player.h"
#include "unsqsh.h"

#define strncasecmp _strnicmp
#define strcasecmp _stricmp

/* structure of of cyb tfmx module (mdat and smpl in one file) */
/* format by my weird friend Alexis 'Cyb' Nasr, back in 1995, yeah ! */
/* FYI, he stopped coding (used asm m68k back then), and is now a doctor ! */
/* values are stored in big endian in the file */
struct CybHeader {
    uint32_t TFhd_head;      /* dc.l "TFHD" for recognition */
    uint32_t TFhd_offset;    /* dc.l	TFhd_sizeof */
    uint8_t TFhd_type;       /* module type :0/1/2/3 (bit 7=FORCED) */
    uint8_t TFhd_version;    /* currently 0 */
    uint32_t _TFhd_mdatsize; /* compiler may align them, use offsets */
    uint32_t _TFhd_smplsize;
    uint16_t _TFhd_sizeof;
};

static inline uint32_t GINT32_FROM_BE(uint32_t v) {
    return ((v >> 24) & 0x000000ff) | ((v >> 8) & 0x0000ff00) | ((v << 8) & 0x00ff0000) | ((v << 16) & 0xff000000);
}


static int tfmx_loader(TfmxState* state, TfmxData* mdata_data, TfmxData* sample_data);

#if 0

/* loading of a single Cyb' TFMX file */
/* return 0 on success */
static int tfmx_cyb_file_load(TfmxState* state, char* fn) {
    FILE* cybf = NULL;
    char* radix = NULL;
    uint8_t* cybmem = NULL;
    long fileSize;
    FILE* mdatf = NULL;
    FILE* smplf = NULL;
    struct CybHeader* cybh = NULL;
    int retval = 1;
    int ulen;
    int mdatsize;
    int smplsize;
    uint32_t offset;

    /* get radix from filename */
    if (!(radix = strrchr(fn, '/')))
        radix = fn;
    else
        radix++;

    /* open the single file */
    cybf = fopen(fn, "rb");

    if (!cybf) {
        return retval;
    }

    /* get length */
    fseek(cybf, 0, SEEK_END);
    fileSize = ftell(cybf);
    rewind(cybf);

    /* alloc mem */
    cybmem = (uint8_t*)malloc(fileSize);
    if (!cybmem)
        goto cleanup;

    /* read it */
    if (fread(cybmem, fileSize, 1, cybf) < 1)
        goto cleanup;
    fclose(cybf);
    cybf = NULL;

    ulen = tfmx_sqsh_get_ulen(cybmem, fileSize);
    if (ulen) {
        uint8_t* dest;

        dest = (uint8_t*)malloc(ulen + 100);
        if (!dest)
            goto cleanup;

        tfmx_sqsh_unpack(cybmem + 16, dest, ulen);

        free(cybmem);
        cybmem = dest;
    }

    if (strncmp((char*)cybmem, "TFHD", 4))
        goto cleanup;

    cybh = (struct CybHeader*)cybmem;

    offset = GINT32_FROM_BE(cybh->TFhd_offset);

    mdatsize = 0;
    mdatsize = cybmem[10];
    mdatsize <<= 8;
    mdatsize |= cybmem[11];
    mdatsize <<= 8;
    mdatsize |= cybmem[12];
    mdatsize <<= 8;
    mdatsize |= cybmem[13];

    smplsize = 0;
    smplsize = cybmem[14];
    smplsize <<= 8;
    smplsize |= cybmem[15];
    smplsize <<= 8;
    smplsize |= cybmem[16];
    smplsize <<= 8;
    smplsize |= cybmem[17];

    /* create temp file names from radix */
    char tmp_mdat[512];
    char tmp_smpl[512];

    sprintf(tmp_mdat, "/tmp/__mdat_%s__", radix);
    sprintf(tmp_smpl, "/tmp/__smpl_%s__", radix);

    /* open and write temp files */
    mdatf = fopen(tmp_mdat, "wb");
    if (!mdatf)
        goto cleanup;
    fwrite(cybmem + offset, mdatsize, 1, mdatf);
    fclose(mdatf);

    smplf = fopen(tmp_smpl, "wb");
    if (!smplf)
        goto cleanup;
    fwrite(cybmem + offset + mdatsize, smplsize, 1, mdatf);
    fclose(smplf);

    /* tfmx loading */
    if (tfmx_loader(state, tmp_mdat, tmp_smpl) == 1) {
        goto cleanup;
    }
    retval = 0;

    /* a kind of poor man exception handling :-/ */
cleanup:
    /* if value for tmpfile => remove it */
    if (mdatf)
        remove(tmp_mdat);
    if (smplf)
        remove(tmp_smpl);
    if (cybmem)
        free(cybmem);
    if (cybf)
        fclose(cybf);
    return retval;
}

#endif

int LoadTFMXFile(TfmxState* state, TfmxData* mdat_data, TfmxData* sample_data) {
    return tfmx_loader(state, mdat_data, sample_data);

/*
    int suffixPos, suffixPosDat, status;
    char *mfn = fName, *sfn, *c;

    if (!fName)
        return 1;

    if (!(sfn = strdup(mfn)))
        return 1;

    if (!(c = strrchr(sfn, '/')))
        c = sfn;
    else
        c++;

    suffixPos = strlen(c) - 4;    // Get filename length
    suffixPosDat = strlen(c) - 5; // Get filename length

    if (strncasecmp(c, "mdat.", 5) == 0) {
        // Case-preserving conversion of "mdat" to "smpl"
        (*c++) ^= 'm' ^ 's';
        (*c++) ^= 'd' ^ 'm';
        (*c++) ^= 'a' ^ 'p';
        (*c++) ^= 't' ^ 'l';
        c -= 4;
    } else if (strncasecmp(c, "tfmx.", 5) == 0) { // Single Cyb' TFMX fil
        free(sfn);
        return tfmx_cyb_file_load(state, fName);
    } else if (suffixPos >= 0 && strncasecmp(c + suffixPos, ".tfx", 4) == 0) {
        // Case-preserving conversion of ".tfx" to ".sam"
        *(c + suffixPos + 1) ^= 't' ^ 's';
        *(c + suffixPos + 2) ^= 'f' ^ 'a';
        *(c + suffixPos + 3) ^= 'x' ^ 'm';
    } else if (suffixPos >= 0 && strncasecmp(c + suffixPosDat, ".mdat", 4) == 0) {
        *(c + suffixPosDat + 1) ^= 'm' ^ 's';
        *(c + suffixPosDat + 2) ^= 'd' ^ 'm';
        *(c + suffixPosDat + 3) ^= 'a' ^ 'p';
        *(c + suffixPosDat + 4) ^= 't' ^ 'l';
    } else {
        TFMXERR(state, "LoadTFMX: Song name prefix / suffix missing ?!");
        free(sfn);
        return 1;
    }

    if ((status = tfmx_loader(state, mfn, sfn)) == 1) {
        // TFMXERR("LoadTFMXFile: Loading of module failed");
        free(sfn);
        return 1;
    } else if (status == 2) {
        // TFMXERR("LoadTFMXFile: Not an MDAT file");
        free(sfn);
        return 1;
    }

    free(sfn);
    return 0;
*/
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int read_data(void* dest, int size, TfmxData* data) {
    if (size + data->read_offset > data->size) {
        size = ((int)data->size) - data->read_offset;
    }

    memcpy(dest, data->data + data->read_offset, size);
    data->read_offset += size;

    return size;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int tfmx_loader(TfmxState* state, TfmxData* mdat_data, TfmxData* smpl_data) {
    /* struct stat s; */
    int x, y, z = 0;
    U16 *sh, *lg;
    U8* buf;

    if (!read_data(&state->mdat_header, sizeof(state->mdat_header), mdat_data)) {
        //TFMXERR(state, "LoadTFMX: Failed to read TFMX header");
        return -1;
    }

    if (strncmp("TFMX-SONG", state->mdat_header.magic, 9)
     && strncmp("TFMX_SONG", state->mdat_header.magic, 9)
     && strncasecmp("TFMXSONG", state->mdat_header.magic, 8)
     && strncasecmp("TFMX ", state->mdat_header.magic, 5)) {
        //printf("%s", "LoadTFMX: Not a TFMX module");
        return -1;
    }

    if (!(x = read_data(&state->mdat_editbuf, MDAT_EDITBUF_LONGS * 4, mdat_data))) {
        //printf("%s", "LoadTFMX: Read error in MDAT file");
        return -1;
    }

    state->mlen = x >> 2;

    state->mdat_editbuf[x >> 2] = -1;
    if (!state->mdat_header.trackstart)
        state->mdat_header.trackstart = 0x180;
    else
        state->mdat_header.trackstart = (_byteswap_ulong(state->mdat_header.trackstart) - 0x200L) >> 2;
    if (!state->mdat_header.pattstart)
        state->mdat_header.pattstart = 0x80;
    else
        state->mdat_header.pattstart = (_byteswap_ulong(state->mdat_header.pattstart) - 0x200L) >> 2;
    if (!state->mdat_header.macrostart)
        state->mdat_header.macrostart = 0x100;
    else
        state->mdat_header.macrostart = (_byteswap_ulong(state->mdat_header.macrostart) - 0x200L) >> 2;

    if (x < 136) {
        return -1;
    }

    for (x = 0; x < 32; x++) {
        state->mdat_header.start[x] = _byteswap_ushort(state->mdat_header.start[x]);
        state->mdat_header.end[x] = _byteswap_ushort(state->mdat_header.end[x]);
        state->mdat_header.tempo[x] = _byteswap_ushort(state->mdat_header.tempo[x]);
    }

    /* Now that we have pointers to most everything, this would be a good time to
       fix everything we can... ntohs tracksteps, convert pointers to array
       indices, ntohl patterns and macros.  We fix the macros first, then the
       patterns, and then the tracksteps (because we have to know when the
       patterns begin to know when the tracksteps end...) */
    z = state->mdat_header.macrostart;
    state->macros = (int*)&(state->mdat_editbuf[z]);

    for (x = 0; x < 128; x++) {
        y = (_byteswap_ulong(state->mdat_editbuf[z]) - 0x200);
        if ((y & 3) || ((y >> 2) > state->mlen)) /* probably not strictly right */
            break;
        state->mdat_editbuf[z++] = y >> 2;
    }
    state->num_mac = x;

    z = state->mdat_header.pattstart;
    state->patterns = (int*)&state->mdat_editbuf[z];
    for (x = 0; x < 128; x++) {
        y = (_byteswap_ulong(state->mdat_editbuf[z]) - 0x200);
        if ((y & 3) || ((y >> 2) > state->mlen))
            break;
        state->mdat_editbuf[z++] = y >> 2;
    }
    state->num_pat = x;

    lg = (U16*)&state->mdat_editbuf[state->patterns[0]];
    sh = (U16*)&state->mdat_editbuf[state->mdat_header.trackstart];
    state->num_ts = (state->patterns[0] - state->mdat_header.trackstart) >> 2;
    y = 0;
    while (sh < lg) {
        x = _byteswap_ushort(*sh);
        *sh++ = x;
    }

    /* Calc the # of subsongs */
    state->nSongs = 0;
    for (x = 0; x < 31; x++) {
        for (y = 0; y < x; y++) {
            if (state->mdat_header.start[x] == state->mdat_header.start[y])
            {
                state->mdat_header.end[x] = 0; /* remove duplicates */
                break;
            }
        }
        /* check if the song is empty */
/* disabled to match with UADE (there will be empty songs)
        sh = (U16*)&state->mdat_editbuf[state->mdat_header.trackstart + (state->mdat_header.start[x] * 4)];
        if (sh[0] == 0xEFFE && sh[1] == 0)
            state->mdat_header.end[x] = 0; / * remove duplicates * /
*/

        if ((state->mdat_header.start[x] <= state->mdat_header.end[x]) && !(x > 0 && state->mdat_header.end[x] == 0L)) {
            state->nSongs++;
        }
    }

    // Now at long last we calc the size of and load the sample file.
    if (!(state->smplbuf = (void*)malloc(smpl_data->size))) {
        //printf("%s", "LoadTFMX: Error allocating samplebuffer");
        return -1;
    }

    state->smplbuf_end = state->smplbuf + smpl_data->size - 1;

    if (!read_data(state->smplbuf, smpl_data->size, smpl_data)) {
        //printf("%s", "LoadTFMX: Error reading SMPL file");
        free(state->smplbuf);
        state->smplbuf = 0;
        return -1;
    }

    /*
    if (plugin_cfg.blend)
        output_chans = 2;
    plugin_cfg.blend &= 1;
    */

    tfmx_calc_sizes(state);
    TFMXRewind(state);

    state->isPro = 1;
    buf = mdat_data->data;
    if ((buf[10] == 'b' && buf[11] == 'y') ||
        (buf[16] == ' ' && buf[17] == ' ') ||
        (buf[16] == '(' && buf[17] == 'E' && buf[18] == 'm' && buf[19] == 'p' && buf[20] == 't' && buf[21] == 'y' && buf[22] == ')') ||
        (buf[16] == 0x30 && buf[17] == 0x3d) || /*lethal Zone*/
        (buf[4] == 0x20)) {
        if (buf[464] == 0x00 && buf[465] == 0x00 && buf[466] == 0x00 && buf[467] == 0x00) {
            if ((buf[14] != 0x0e && buf[15] != 0x60) || /*z-out title */
                (buf[14] == 0x08 && buf[15] == 0x60 && buf[4644] != 0x09 && buf[4645] != 0x0c) || /* metal law */
                (buf[14] == 0x0b && buf[15] == 0x20 && buf[5120] != 0x8c && buf[5121] != 0x26) || /* bug bomber */
                (buf[14] == 0x09 && buf[15] == 0x20 && buf[3876] != 0x93 && buf[3977] != 0x05)) { /* metal preview */
                state->isPro = 0;
            }
        }
    }

    state->hasUnsuportedCommands = 0;

    return 1;

    /* Now the song is fully loaded.  Everything is done but ntohl'ing the actual
       pattern and macro data. The routines that use the data do it for themselves.*/
}

void tfmx_fill_module_info(TfmxState* state, char* t) {
    int x;

    /* Don't print info if there's no song... */
    if (!state->smplbuf) {
        sprintf(t, "No song loaded!");
        return;
    }

    t += sprintf(t, "Module text section:\n\n");
    for (x = 0; x < 6; x++)
        t += sprintf(t, ">%40.40s\n", state->mdat_header.text[x]);

    t += sprintf(t, "\n%d tracksteps at 0x%04d\n", state->num_ts, (state->mdat_header.trackstart << 2) + 0x200);
    t += sprintf(t, "%d patterns at 0x%04dx\n", state->num_pat, (state->mdat_header.pattstart << 2) + 0x200);
    t += sprintf(t, "%d macros at 0x%04dx\n", state->num_mac, (state->mdat_header.macrostart << 2) + 0x200);

    t += sprintf(t, "\nSubsongs:\n\n");
    for (x = 0; x < 31; x++) {
        if ((state->mdat_header.start[x] <= state->mdat_header.end[x]) && !(x > 0 && state->mdat_header.end[x] == 0L)) {
            t += sprintf(t, "Song %2d: start %3x end %3x tempo %d\n", x, _byteswap_ushort(state->mdat_header.start[x]),
                _byteswap_ushort(state->mdat_header.end[x]), state->mdat_header.tempo[x]);
        }
    }
}


void tfmx_fill_text_info(TfmxState* state, char* t) {
    for (int i = 0; i < 6; ++i) {
        t += sprintf(t, "%40.40s\n", state->mdat_header.text[i]);
    }
}
