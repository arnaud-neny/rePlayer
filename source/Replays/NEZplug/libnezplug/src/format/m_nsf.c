/* NSF mapper(4KBx8) */

#include "../include/nezplug/nezplug.h"
#include "handler.h"
#include "audiosys.h"
#include "songinfo.h"
#include "nsf6502.h"
#include "../common/util.h"
#include "../device/nes/s_apu.h"
#include "../device/nes/s_mmc5.h"
#include "../device/nes/s_vrc6.h"
#include "../device/nes/s_vrc7.h"
#include "../device/nes/s_fds.h"
#include "../device/nes/s_n106.h"
#include "../device/nes/s_fme7.h"
#include "trackinfo.h"
#include <stdio.h>
#include <string.h>

#define NSF_MAPPER_STATIC 0
#include "m_nsf.h"

#define EXTSOUND_VRC6	(1 << 0)
#define EXTSOUND_VRC7	(1 << 1)
#define EXTSOUND_FDS	(1 << 2)
#define EXTSOUND_MMC5	(1 << 3)
#define EXTSOUND_N106	(1 << 4)
#define EXTSOUND_FME7	(1 << 5)
#define EXTSOUND_J86	(1 << 6)	/* JALECO-86 */

/* RAM area */
static uint32_t ReadRam(void *pNezPlay, uint32_t A)
{
	return ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->ram[A & 0x07FF];
}
static void WriteRam(void *pNezPlay, uint32_t A, uint32_t V)
{
	((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->ram[A & 0x07FF] = (uint8_t)V;
}

/* SRAM area */
static uint32_t ReadStaticArea(void *pNezPlay, uint32_t A)
{
	return ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->static_area[A - 0x6000];
}
static void WriteStaticArea(void *pNezPlay, uint32_t A, uint32_t V)
{
	((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->static_area[A - 0x6000] = (uint8_t)V;
}

#if !NSF_MAPPER_STATIC
static uint32_t ReadRom8000(void *pNezPlay, uint32_t A)
{
	return ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[0][A];
}
static uint32_t ReadRom9000(void *pNezPlay, uint32_t A)
{
	return ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[1][A];
}
static uint32_t ReadRomA000(void *pNezPlay, uint32_t A)
{
	return ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[2][A];
}
static uint32_t ReadRomB000(void *pNezPlay, uint32_t A)
{
	return ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[3][A];
}
static uint32_t ReadRomC000(void *pNezPlay, uint32_t A)
{
	return ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[4][A];
}
static uint32_t ReadRomD000(void *pNezPlay, uint32_t A)
{
	return ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[5][A];
}
static uint32_t ReadRomE000(void *pNezPlay, uint32_t A)
{
	return ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[6][A];
}
static uint32_t ReadRomF000(void *pNezPlay, uint32_t A)
{
	return ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[7][A];
}

// ROMに値を書き込むのはできないはずで不正なのだが、
// これをしないとFDS環境のNSFが動作しないのでしょうがない…

static void WriteRom8000(void *pNezPlay, uint32_t A, uint32_t V)
{
	((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[0][A] = (uint8_t)V;
}
static void WriteRom9000(void *pNezPlay, uint32_t A, uint32_t V)
{
	((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[1][A] = (uint8_t)V;
}
static void WriteRomA000(void *pNezPlay, uint32_t A, uint32_t V)
{
	((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[2][A] = (uint8_t)V;
}
static void WriteRomB000(void *pNezPlay, uint32_t A, uint32_t V)
{
	((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[3][A] = (uint8_t)V;
}
static void WriteRomC000(void *pNezPlay, uint32_t A, uint32_t V)
{
	((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[4][A] = (uint8_t)V;
}
static void WriteRomD000(void *pNezPlay, uint32_t A, uint32_t V)
{
	((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[5][A] = (uint8_t)V;
}
static void WriteRomE000(void *pNezPlay, uint32_t A, uint32_t V)
{
	((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[6][A] = (uint8_t)V;
}
static void WriteRomF000(void *pNezPlay, uint32_t A, uint32_t V)
{
	((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->bank[7][A] = (uint8_t)V;
}
#endif

/* Mapper I/O */
static void WriteMapper(void *pNezPlay, uint32_t A, uint32_t V)
{
	uint32_t bank;
	NSFNSF *nsf = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf);
	bank = A - 0x5FF0;
	if (bank < 6 || bank > 15) return;
#if !NSF_MAPPER_STATIC
	if (bank >= 8)
	{
		if (V < nsf->banknum)
		{
			nsf->bank[bank - 8] = &nsf->bankbase[V << 12] - (bank << 12);
		}
		else
			nsf->bank[bank - 8] = nsf->zero_area - (bank << 12);
		return;
	}
#endif
	if (V < nsf->banknum)
	{
		XMEMCPY(&nsf->static_area[(bank - 6) << 12], &nsf->bankbase[V << 12], 0x1000);
	}
	else
		XMEMSET(&nsf->static_area[(bank - 6) << 12], 0x00, 0x1000);
}

static uint32_t Read2000(void *pNezPlay, uint32_t A)
{
	NSFNSF *nsf = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf);
	switch(A & 0x7){
	case 0:
		return 0xff;
	case 1:
		return 0xff;
	case 2:
		return nsf->counter2002++ ? 0x7f : 0xff;
	case 3:
		return 0xff;
	case 4:
		return 0xff;
	case 5:
		return 0xff;
	case 6:
		return 0xff;
	case 7:
		return 0xff;
	}
	return 0xff;
}


static const NES_READ_HANDLER nsf_mapper_read_handler[] = {
	{ 0x0000,0x0FFF,ReadRam, NULL },
	{ 0x1000,0x1FFF,ReadRam, NULL },
	{ 0x2000,0x2007,Read2000, NULL },
	{ 0x6000,0x6FFF,ReadStaticArea, NULL },
	{ 0x7000,0x7FFF,ReadStaticArea, NULL },
#if !NSF_MAPPER_STATIC
	{ 0x8000,0x8FFF,ReadRom8000, NULL },
	{ 0x9000,0x9FFF,ReadRom9000, NULL },
	{ 0xA000,0xAFFF,ReadRomA000, NULL },
	{ 0xB000,0xBFFF,ReadRomB000, NULL },
	{ 0xC000,0xCFFF,ReadRomC000, NULL },
	{ 0xD000,0xDFFF,ReadRomD000, NULL },
	{ 0xE000,0xEFFF,ReadRomE000, NULL },
	{ 0xF000,0xFFFF,ReadRomF000, NULL },
#else
	{ 0x8000,0x8FFF,ReadStaticArea, NULL },
	{ 0x9000,0x9FFF,ReadStaticArea, NULL },
	{ 0xA000,0xAFFF,ReadStaticArea, NULL },
	{ 0xB000,0xBFFF,ReadStaticArea, NULL },
	{ 0xC000,0xCFFF,ReadStaticArea, NULL },
	{ 0xD000,0xDFFF,ReadStaticArea, NULL },
	{ 0xE000,0xEFFF,ReadStaticArea, NULL },
	{ 0xF000,0xFFFF,ReadStaticArea, NULL },
#endif
	{ 0     ,0     ,0, NULL },
};

static const NES_WRITE_HANDLER nsf_mapper_write_handler[] = {
	{ 0x0000,0x0FFF,WriteRam, NULL },
	{ 0x1000,0x1FFF,WriteRam, NULL },
	{ 0x6000,0x6FFF,WriteStaticArea, NULL },
	{ 0x7000,0x7FFF,WriteStaticArea, NULL },
	{ 0     ,0     ,0, NULL },
};
static const NES_WRITE_HANDLER nsf_mapper_write_handler_fds[] = {
	{ 0x0000,0x0FFF,WriteRam, NULL },
	{ 0x1000,0x1FFF,WriteRam, NULL },
	{ 0x6000,0x6FFF,WriteStaticArea, NULL },
	{ 0x7000,0x7FFF,WriteStaticArea, NULL },
	{ 0x8000,0x8FFF,WriteRom8000, NULL },
	{ 0x9000,0x9FFF,WriteRom9000, NULL },
	{ 0xA000,0xAFFF,WriteRomA000, NULL },
	{ 0xB000,0xBFFF,WriteRomB000, NULL },
	{ 0xC000,0xCFFF,WriteRomC000, NULL },
	{ 0xD000,0xDFFF,WriteRomD000, NULL },
	{ 0xE000,0xEFFF,WriteRomE000, NULL },
	{ 0xF000,0xFFFF,WriteRomF000, NULL },
	{ 0     ,0     ,0, NULL },
};

static const NES_WRITE_HANDLER nsf_mapper_write_handler2[] = {
	{ 0x5FF6,0x5FFF,WriteMapper, NULL },
	{ 0     ,0     ,0, NULL },
};

PROTECTED uint8_t *NSFGetHeader(NEZ_PLAY *pNezPlay)
{
	return ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->head;
}

static void ResetBank(NEZ_PLAY *pNezPlay)
{
	uint32_t i, startbank;
	NSFNSF *nsf = (NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf;
	XMEMSET(nsf->ram, 0, 0x0800);
	startbank = GetWordLE(nsf->head + 8) >> 12;
	for (i = 0; i < 16; i++)
		WriteMapper(pNezPlay, 0x5FF0 + i, 0x10000/* off */);
	if (nsf->banksw)
	{
		for (i = 0; (startbank + i) < 8; i++)
			WriteMapper(pNezPlay, 0x5FF0 + (startbank + i) , i);
		if (nsf->banksw & 2)
		{
			WriteMapper(pNezPlay, 0x5FF6, nsf->head[0x70 + 6]);
			WriteMapper(pNezPlay, 0x5FF7, nsf->head[0x70 + 7]);
		}
		for (i = 8; i < 16; i++)
			WriteMapper(pNezPlay, 0x5FF0 + i, nsf->head[0x70 + i - 8]);
	}
	else
	{
		for (i = startbank; i < 16; i++)
			WriteMapper(pNezPlay, 0x5FF0 + i , i - startbank);
	}
}

static const NEZ_NES_RESET_HANDLER nsf_mapper_reset_handler[] = {
	{ NES_RESET_SYS_FIRST, ResetBank, NULL },
	{ 0,                   0, NULL },
};

static void Terminate(NEZ_PLAY *pNezPlay)
{
	NSFNSF *nsf = (NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf;
	if (nsf)
	{
        NESMemoryHandlerInitialize(pNezPlay); // rePlayer (memory leak)

		if (nsf->bankbase)
		{
			XFREE(nsf->bankbase);
			nsf->bankbase = 0;
		}
		XFREE(nsf);
		((NEZ_PLAY*)pNezPlay)->nsf = 0;
	}
}

static const NEZ_NES_TERMINATE_HANDLER nsf_mapper_terminate_handler[] = {
	{ Terminate, NULL },
    { 0, NULL },
};

static uint32_t NSFMapperInitialize(NEZ_PLAY *pNezPlay, const uint8_t *pData, uint32_t uSize)
{
	uint32_t size = uSize;
	const uint8_t *data = pData;
	NSFNSF *nsf = (NSFNSF*)pNezPlay->nsf;

	size += GetWordLE(nsf->head + 8) & 0x0FFF;
	size  = (size + 0x0FFF) & ~0x0FFF;

	nsf->bankbase = XMALLOC(size + 8);
	if (!nsf->bankbase) return NEZ_NESERR_SHORTOFMEMORY;

	nsf->banknum = size >> 12;
	XMEMSET(nsf->bankbase, 0, size);
	XMEMCPY(nsf->bankbase + (GetWordLE(nsf->head + 8) & 0x0FFF), data, uSize);

	nsf->banksw = nsf->head[0x70] || nsf->head[0x71] || nsf->head[0x72] || nsf->head[0x73] || nsf->head[0x74] || nsf->head[0x75] || nsf->head[0x76] || nsf->head[0x77];
	if (SONGINFO_GetExtendDevice(pNezPlay->song) & EXTSOUND_FDS) nsf->banksw <<= 1;
	return NEZ_NESERR_NOERROR;
}

PROTECTED uint32_t NSFDeviceInitialize(NEZ_PLAY *pNezPlay)
{
	NSFNSF *nsf = (NSFNSF*)pNezPlay->nsf;
	NESResetHandlerInstall(pNezPlay->nrh, nsf->nsf_mapper_reset_handler);
	NESTerminateHandlerInstall(&pNezPlay->nth, nsf->nsf_mapper_terminate_handler);
	NESReadHandlerInstall(pNezPlay, nsf->nsf_mapper_read_handler);
	//FDSのみをサポートしている場合のみ8000-DFFFも書き込み可能-
	if(pNezPlay->song->extdevice == 4){
		NESWriteHandlerInstall(pNezPlay, nsf->nsf_mapper_write_handler_fds);
	}else{
		NESWriteHandlerInstall(pNezPlay, nsf->nsf_mapper_write_handler);
	}

	if (nsf->banksw) NESWriteHandlerInstall(pNezPlay, nsf->nsf_mapper_write_handler2);

	XMEMSET(nsf->ram, 0, 0x0800);
	XMEMSET(nsf->static_area, 0, sizeof(nsf->static_area));
#if !NSF_MAPPER_STATIC
	XMEMSET(nsf->zero_area, 0, sizeof(nsf->zero_area));
#endif

	APUSoundInstall(pNezPlay);
	if (SONGINFO_GetExtendDevice(pNezPlay->song) & EXTSOUND_VRC6) VRC6SoundInstall(pNezPlay);
	if (SONGINFO_GetExtendDevice(pNezPlay->song) & EXTSOUND_VRC7) VRC7SoundInstall(pNezPlay);
	if (SONGINFO_GetExtendDevice(pNezPlay->song) & EXTSOUND_FDS)  FDSSoundInstall(pNezPlay);
	if (SONGINFO_GetExtendDevice(pNezPlay->song) & EXTSOUND_MMC5)
	{
		MMC5SoundInstall(pNezPlay);
		MMC5MutiplierInstall(pNezPlay);
		MMC5ExtendRamInstall(pNezPlay);
	}
	if (SONGINFO_GetExtendDevice(pNezPlay->song) & EXTSOUND_N106) N106SoundInstall(pNezPlay);
	if (SONGINFO_GetExtendDevice(pNezPlay->song) & EXTSOUND_FME7) FME7SoundInstall(pNezPlay);

	nsf->counter2002 = 0;	//暫定

	return NEZ_NESERR_NOERROR;
}


static int32_t NSFMapperSetInfo(NEZ_PLAY *pNezPlay)
{
    uint8_t titlebuffer[0x21];
    uint8_t artistbuffer[0x21];
    uint8_t copyrightbuffer[0x21];
    uint8_t *head = ((NSFNSF*)pNezPlay->nsf)->head;

	SONGINFO_SetStartSongNo(pNezPlay->song, head[0x07]);
	SONGINFO_SetMaxSongNo(pNezPlay->song, head[0x06]);
	SONGINFO_SetExtendDevice(pNezPlay->song, head[0x7B]);
	SONGINFO_SetInitAddress(pNezPlay->song, GetWordLE(head + 0x0A));
	SONGINFO_SetPlayAddress(pNezPlay->song, GetWordLE(head + 0x0C));
	SONGINFO_SetChannel(pNezPlay->song, 1);

	XMEMSET(titlebuffer, 0, 0x21);
	XMEMCPY(titlebuffer, head + 0x000e, 0x20);
    if(pNezPlay->songinfodata.title != NULL) {
        XFREE(pNezPlay->songinfodata.title);
    }
    pNezPlay->songinfodata.title = (char *)XMALLOC(strlen((const char *)titlebuffer)+1);
    if(pNezPlay->songinfodata.title == NULL) {
        return NEZ_NESERR_SHORTOFMEMORY;
    }
    XMEMCPY(pNezPlay->songinfodata.title,titlebuffer,strlen((const char *)titlebuffer)+1);

	XMEMSET(artistbuffer, 0, 0x21);
	XMEMCPY(artistbuffer, head + 0x002e, 0x20);
    if(pNezPlay->songinfodata.artist != NULL) {
        XFREE(pNezPlay->songinfodata.artist);
    }
    pNezPlay->songinfodata.artist = (char *)XMALLOC(strlen((const char *)artistbuffer)+1);
    if(pNezPlay->songinfodata.artist == NULL) {
        return NEZ_NESERR_SHORTOFMEMORY;
    }
    XMEMCPY(pNezPlay->songinfodata.artist,artistbuffer,strlen((const char *)artistbuffer)+1);

	XMEMSET(copyrightbuffer, 0, 0x21);
	XMEMCPY(copyrightbuffer, head + 0x004e, 0x20);
    if(pNezPlay->songinfodata.copyright != NULL) {
        XFREE(pNezPlay->songinfodata.copyright);
    }
    pNezPlay->songinfodata.copyright = (char *)XMALLOC(strlen((const char *)copyrightbuffer)+1);
    if(pNezPlay->songinfodata.copyright == NULL) {
        return NEZ_NESERR_SHORTOFMEMORY;
    }
    XMEMCPY(pNezPlay->songinfodata.copyright,copyrightbuffer,strlen((const char *)copyrightbuffer)+1);

	sprintf(pNezPlay->songinfodata.detail,
"Type          : NSF\r\n\
Song Max      : %d\r\n\
Start Song    : %d\r\n\
Load Address  : %04XH\r\n\
Init Address  : %04XH\r\n\
Play Address  : %04XH\r\n\
NTSC/PAL Mode : %s\r\n\
NTSC Speed    : %04XH (%4.0fHz)\r\n\
PAL Speed     : %04XH (%4.0fHz)\r\n\
Extend Device : %s%s%s%s%s%s%s\r\n\
\r\n\
Set First ROM Bank                    : %d\r\n\
First ROM Bank(8000-8FFF)             : %02XH\r\n\
First ROM Bank(9000-9FFF)             : %02XH\r\n\
First ROM Bank(A000-AFFF)             : %02XH\r\n\
First ROM Bank(B000-BFFF)             : %02XH\r\n\
First ROM Bank(C000-CFFF)             : %02XH\r\n\
First ROM Bank(D000-DFFF)             : %02XH\r\n\
First ROM Bank(E000-EFFF or 6000-6FFF): %02XH\r\n\
First ROM Bank(F000-FFFF or 7000-7FFF): %02XH"
		,head[0x06],head[0x07],GetWordLE(head + 0x08),GetWordLE(head + 0x0A),GetWordLE(head + 0x0C)
		,head[0x7A]&0x02 ? "NTSC + PAL" : head[0x7A]&0x01 ? "PAL" : "NTSC"
		,GetWordLE(head + 0x6E),GetWordLE(head + 0x6E) ? 1000000.0/GetWordLE(head + 0x6E) : 0
		,GetWordLE(head + 0x78),GetWordLE(head + 0x78) ? 1000000.0/GetWordLE(head + 0x78) : 0
		,head[0x7B]&0x01 ? "VRC6 " : ""
		,head[0x7B]&0x02 ? "VRC7 " : ""
		,head[0x7B]&0x04 ? "2C33 " : ""
		,head[0x7B]&0x08 ? "MMC5 " : ""
		,head[0x7B]&0x10 ? "Namco1xx " : ""
		,head[0x7B]&0x20 ? "Sunsoft5B " : ""
		,head[0x7B] ? "" : "None"
		,head[0x70]|head[0x71]|head[0x72]|head[0x73]|head[0x74]|head[0x75]|head[0x76]|head[0x77] ? 1 : 0
		,head[0x70],head[0x71],head[0x72],head[0x73],head[0x74],head[0x75],head[0x76],head[0x77]
	);

    return NEZ_NESERR_NOERROR;
}

PROTECTED uint32_t NSFLoad(NEZ_PLAY *pNezPlay, const uint8_t *pData, uint32_t uSize)
{
	uint32_t ret;
	NSFNSF *THIS_ = (NSFNSF *)XMALLOC(sizeof(NSFNSF));
	if (!THIS_) return NEZ_NESERR_SHORTOFMEMORY;
	XMEMSET(THIS_, 0, sizeof(NSFNSF));
	THIS_->fds_type = 2;
	pNezPlay->nsf = THIS_;

    XMEMCPY(THIS_->nsf_mapper_read_handler,nsf_mapper_read_handler,sizeof(nsf_mapper_read_handler));
    XMEMCPY(THIS_->nsf_mapper_write_handler,nsf_mapper_write_handler,sizeof(nsf_mapper_write_handler));
    XMEMCPY(THIS_->nsf_mapper_write_handler_fds,nsf_mapper_write_handler_fds,sizeof(nsf_mapper_write_handler_fds));
    XMEMCPY(THIS_->nsf_mapper_write_handler2,nsf_mapper_write_handler2,sizeof(nsf_mapper_write_handler2));
    XMEMCPY(THIS_->nsf_mapper_reset_handler,nsf_mapper_reset_handler,sizeof(nsf_mapper_reset_handler));
    XMEMCPY(THIS_->nsf_mapper_terminate_handler,nsf_mapper_terminate_handler,sizeof(nsf_mapper_terminate_handler));
    XMEMCPY(((NSFNSF*)pNezPlay->nsf)->head,pData,0x80);

	NESMemoryHandlerInitialize(pNezPlay);
	ret = NSFMapperSetInfo(pNezPlay);
    if (ret) return ret;
	ret = NSF6502Install(pNezPlay);
	if (ret) return ret;
	ret = NSFMapperInitialize(pNezPlay, pData + 0x80, uSize - 0x80);
	if (ret) return ret;
	ret = NSFDeviceInitialize(pNezPlay);
	if (ret) return ret;
	SONGINFO_SetSongNo(pNezPlay->song, SONGINFO_GetStartSongNo(pNezPlay->song));
	return NEZ_NESERR_NOERROR;
}

static NEZ_TRACK_INFO *NSFFindTrack(NEZ_PLAY *pNezPlay, const uint8_t songNo) {
    uint32_t i = 0;
    while(i < pNezPlay->tracks->loaded) {
        if(pNezPlay->tracks->info[i].songno == (uint32_t)-1)
           return &(pNezPlay->tracks->info[i]);
        if(pNezPlay->tracks->info[i].songno == (uint32_t)songNo)
           return &(pNezPlay->tracks->info[i]);
        i++;
    }
    return &(pNezPlay->tracks->info[i]);
}

PROTECTED uint32_t NSFELoad(NEZ_PLAY *pNezPlay, const uint8_t *pData, uint32_t uSize) {
    uint32_t ret;
    char chunkId[5];
    uint32_t chunkSize;
    const uint8_t *romData;
    uint32_t romSize;
    int32_t iSize;
    uint8_t infoFlag;

    const uint8_t *plst;
    const uint8_t *time;
    const uint8_t *fade;
    const uint8_t *tlbl;
    const uint8_t *auth;

    uint32_t plstSize;
    uint32_t timeSize;
    uint32_t fadeSize;
    uint32_t tlblSize;
    uint32_t authSize;

    NEZ_TRACK_INFO *trackInfo;

	NSFNSF *THIS_ = (NSFNSF *)XMALLOC(sizeof(NSFNSF));
	if (!THIS_) return NEZ_NESERR_SHORTOFMEMORY;
	XMEMSET(THIS_, 0, sizeof(NSFNSF));
	THIS_->fds_type = 2;
	pNezPlay->nsf = THIS_;

    XMEMCPY(THIS_->nsf_mapper_read_handler,nsf_mapper_read_handler,sizeof(nsf_mapper_read_handler));
    XMEMCPY(THIS_->nsf_mapper_write_handler,nsf_mapper_write_handler,sizeof(nsf_mapper_write_handler));
    XMEMCPY(THIS_->nsf_mapper_write_handler_fds,nsf_mapper_write_handler_fds,sizeof(nsf_mapper_write_handler_fds));
    XMEMCPY(THIS_->nsf_mapper_write_handler2,nsf_mapper_write_handler2,sizeof(nsf_mapper_write_handler2));
    XMEMCPY(THIS_->nsf_mapper_reset_handler,nsf_mapper_reset_handler,sizeof(nsf_mapper_reset_handler));
    XMEMCPY(THIS_->nsf_mapper_terminate_handler,nsf_mapper_terminate_handler,sizeof(nsf_mapper_terminate_handler));

    pData += 4;
    uSize -= 4;
    chunkId[4] = 0;
    ret = NEZ_NESERR_NOERROR;
    romData = NULL;
    romSize = 0;
    infoFlag = 0;

    plst = NULL;
    time = NULL;
    fade = NULL;
    tlbl = NULL;
    auth = NULL;
    plstSize = 0;
    timeSize = 0;
    fadeSize = 0;
    tlblSize = 0;
    authSize = 0;

    trackInfo = NULL;

    ((NSFNSF*)pNezPlay->nsf)->head[0x00] = 'N';
    ((NSFNSF*)pNezPlay->nsf)->head[0x01] = 'E';
    ((NSFNSF*)pNezPlay->nsf)->head[0x02] = 'S';
    ((NSFNSF*)pNezPlay->nsf)->head[0x03] = 'M';
    ((NSFNSF*)pNezPlay->nsf)->head[0x04] = 0x1A;
    ((NSFNSF*)pNezPlay->nsf)->head[0x05] = 0x01;
    ((NSFNSF*)pNezPlay->nsf)->head[0x07] = 0x01;
    SetDwordLE( &((NSFNSF*)pNezPlay->nsf)->head[0x6E],16639);
    SetDwordLE( &((NSFNSF*)pNezPlay->nsf)->head[0x78],19997);

    while(uSize) {
        chunkSize = GetDwordLE(pData);
        XMEMCPY(chunkId,&pData[0x04],4);

        uSize -= 8;
        pData += 8;

        if(GetDwordLEM(chunkId) == GetDwordLEM("INFO")) {
            infoFlag = 1;
            XMEMCPY(&((NSFNSF*)pNezPlay->nsf)->head[0x08],&pData[0x00],0x06);
            XMEMCPY(&((NSFNSF*)pNezPlay->nsf)->head[0x7A],&pData[0x06],0x02);
            XMEMCPY(&((NSFNSF*)pNezPlay->nsf)->head[0x06],&pData[0x08],0x01);
            if(chunkSize > 9) {
                ((NSFNSF*)pNezPlay->nsf)->head[0x07] = pData[0x09] + 1;
            }
        }
        else if(GetDwordLEM(chunkId) == GetDwordLEM("BANK")) {
            if(chunkSize >= 8) {
                XMEMCPY(&((NSFNSF*)pNezPlay->nsf)->head[0x70],&pData[0x00],0x08);
            }
        }
        else if(GetDwordLEM(chunkId) == GetDwordLEM("RATE")) {
            XMEMCPY(&((NSFNSF*)pNezPlay->nsf)->head[0x6E],&pData[0x00],0x02);
            if(chunkSize >= 4) XMEMCPY(&((NSFNSF*)pNezPlay->nsf)->head[0x78],&pData[0x02],0x02);
        }
        else if(GetDwordLEM(chunkId) == GetDwordLEM("DATA")) {
            romData = pData;
            romSize = chunkSize;
        }
        else if(GetDwordLEM(chunkId) == GetDwordLEM("NEND")) {
            break;
        }
        else if((uint8_t)chunkId[0] >= 65 && (uint8_t)chunkId[0] <=90) {
            return NEZ_NESERR_FORMAT;
        }
        else if(GetDwordLEM(chunkId) == GetDwordLEM("plst")) {
            plst = pData;
            plstSize = chunkSize;
        }
        else if(GetDwordLEM(chunkId) == GetDwordLEM("time")) {
            time = pData;
            timeSize = chunkSize;
        }
        else if(GetDwordLEM(chunkId) == GetDwordLEM("fade")) {
            fade = pData;
            fadeSize = chunkSize;
        }
        else if(GetDwordLEM(chunkId) == GetDwordLEM("tlbl")) {
            tlbl = pData;
            tlblSize = chunkSize;
        }
        else if(GetDwordLEM(chunkId) == GetDwordLEM("auth")) {
            auth = pData;
            authSize = chunkSize;
        }


        if(chunkSize > uSize) break;
        uSize -= chunkSize;
        pData += chunkSize;
    }
    if(infoFlag == 0 || romData == NULL) return NEZ_NESERR_FORMAT;

	NESMemoryHandlerInitialize(pNezPlay);
	ret = NSFMapperSetInfo(pNezPlay);
    if (ret) return ret;
	ret = NSF6502Install(pNezPlay);
	if (ret) return ret;
	ret = NSFMapperInitialize(pNezPlay, romData, romSize);
	if (ret) return ret;
	ret = NSFDeviceInitialize(pNezPlay);
	if (ret) return ret;
	SONGINFO_SetSongNo(pNezPlay->song, SONGINFO_GetStartSongNo(pNezPlay->song));
    pNezPlay->tracks = TRACKS_New((uint32_t) ((NSFNSF*)pNezPlay->nsf)->head[0x06]);

    while(authSize > 0) {
        romData = auth;
        while(*romData && (size_t)(romData - auth) < authSize) {
            romData++;
        }
        if((size_t)(romData - auth) < authSize && *romData == 0) {
            uSize = strlen((const char *)auth);
            if(uSize) {
                if(pNezPlay->tracks->title == NULL) {
                    pNezPlay->tracks->title = XMALLOC(uSize+1);
                    if(pNezPlay->tracks->title == NULL) return NEZ_NESERR_SHORTOFMEMORY;
                    XMEMCPY(pNezPlay->tracks->title,auth,uSize);
                    pNezPlay->tracks->title[uSize] = 0;
                } else if(pNezPlay->tracks->artist == NULL) {
                    pNezPlay->tracks->artist = XMALLOC(uSize+1);
                    if(pNezPlay->tracks->artist == NULL) return NEZ_NESERR_SHORTOFMEMORY;
                    XMEMCPY(pNezPlay->tracks->artist,auth,uSize);
                    pNezPlay->tracks->artist[uSize] = 0;
                } else if(pNezPlay->tracks->copyright == NULL) {
                    pNezPlay->tracks->copyright = XMALLOC(uSize+1);
                    if(pNezPlay->tracks->copyright == NULL) return NEZ_NESERR_SHORTOFMEMORY;
                    XMEMCPY(pNezPlay->tracks->copyright,auth,uSize);
                    pNezPlay->tracks->copyright[uSize] = 0;
                } else if(pNezPlay->tracks->dumper == NULL) {
                    pNezPlay->tracks->dumper = XMALLOC(uSize+1);
                    if(pNezPlay->tracks->dumper == NULL) return NEZ_NESERR_SHORTOFMEMORY;
                    XMEMCPY(pNezPlay->tracks->dumper,auth,uSize);
                    pNezPlay->tracks->dumper[uSize] = 0;
                } else {
                    goto authdone;
                }
            }
        }
        else {
            goto authdone;
        }
        authSize -= (uSize + 1);
        auth += (uSize + 1);
    }
    authdone:

    if(plstSize > 0) {
        while(plstSize > 0) {
            pNezPlay->tracks->info[pNezPlay->tracks->loaded++].songno = plst[0];
            plst++;
            plstSize--;
	        SONGINFO_SetMaxSongNo(pNezPlay->song, pNezPlay->tracks->loaded);
        }
    }

    infoFlag = 0;
    while(tlblSize > 0) {
        romData = tlbl;
        while(*romData && (size_t)(romData - tlbl) < tlblSize) {
            romData++;
        }
        if((size_t)(romData - tlbl) < tlblSize && *romData == 0) {
            uSize = strlen((const char *)tlbl);
            if(uSize) {
                trackInfo = NSFFindTrack(pNezPlay,infoFlag);
                trackInfo->title = XMALLOC(uSize+1);
                if(trackInfo->title == NULL) return NEZ_NESERR_SHORTOFMEMORY;
                XMEMCPY(trackInfo->title,tlbl,uSize);
                trackInfo->title[uSize] = 0;
                if(trackInfo->songno == (uint32_t)-1) {
                    trackInfo->songno = (uint32_t)infoFlag;
                    pNezPlay->tracks->loaded++;
                }
            }
        }
        else {
            goto tlbldone;
        }
        if(uSize + 1 > tlblSize) goto tlbldone;
        tlblSize -= (uSize + 1);
        tlbl += (uSize + 1);
        infoFlag++;
    }
    tlbldone:

    infoFlag = 0;
    while(timeSize > 0) {
        iSize = (int32_t)GetDwordLE(time);
        trackInfo = NSFFindTrack(pNezPlay,infoFlag);
        if(trackInfo->songno == (uint32_t)-1) {
            trackInfo->songno = (uint32_t)infoFlag;
            pNezPlay->tracks->loaded++;
        }
        if(iSize >= 0) {
            trackInfo->length = iSize;
        }

        if(timeSize < 4) goto timedone;
        timeSize -= 4;
        time += 4;
        infoFlag++;
    }
    timedone:

    infoFlag = 0;
    while(fadeSize > 0) {
        iSize = (int32_t)GetDwordLE(fade);
        trackInfo = NSFFindTrack(pNezPlay,infoFlag);
        if(trackInfo->songno == (uint32_t)-1) {
            trackInfo->songno = (uint32_t)infoFlag;
            pNezPlay->tracks->loaded++;
        }
        if(iSize >= 0) {
            trackInfo->fade = iSize;
        }

        if(fadeSize < 4) goto fadedone;
        fadeSize -= 4;
        fade += 4;
        infoFlag++;
    }
    fadedone:

    /* check that every song has a corresponding "track" */
    for(infoFlag=0;infoFlag<((NSFNSF*)pNezPlay->nsf)->head[0x06];infoFlag++) {
        trackInfo = NSFFindTrack(pNezPlay,infoFlag);
        if(trackInfo->songno == (uint32_t)-1) {
            trackInfo->songno = (uint32_t)infoFlag;
            pNezPlay->tracks->loaded++;
        }
    }

	return NEZ_NESERR_NOERROR;
}

