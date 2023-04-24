#include "../include/nezplug/nezplug.h"
#include "handler.h"
#include "audiosys.h"
#include "songinfo.h"

#include "m_sgc.h"

#include "../device/s_sng.h"
#include "../device/opl/s_opl.h"
#include "../common/divfix.h"

#include "../cpu/kmz80/kmz80.h"
#include "../common/util.h"

#include <stdio.h>
#include <string.h>

#define SHIFT_CPS 15
#define BASECYCLES       (3579545)

#define EXTDEVICE_SNG		(1 << 1)

#define EXTDEVICE_FMUNIT		(1 << 0)
#define EXTDEVICE_GGSTEREO	(1 << 2)
#define EXTDEVICE_GGRAM		(1 << 3)

#define EXTDEVICE_MSXMUSIC	(1 << 0)
#define EXTDEVICE_MSXRAM	(1 << 2)
#define EXTDEVICE_MSXAUDIO	(1 << 3)
#define EXTDEVICE_MSXSTEREO	(1 << 4)

#define DEVICE_PAL		(1 << 0)

#define SND_SNG 0
#define SND_FMUNIT 1
#define SND_MAX 2


#define SND_VOLUME 0x800

enum{
	SGC_SYNTHMODE_SMS,
	SGC_SYNTHMODE_GG,
	SGC_SYNTHMODE_CV,
	SGC_SYNTHMODE_MAX
};

static const char *sgc_system_name[]={
	"Sega Master System","Game Gear","Coleco Vision","?????"
};

typedef struct SGCSEQ_TAG SGCSEQ;

struct  SGCSEQ_TAG {
	KMZ80_CONTEXT ctx;
	KMIF_SOUND_DEVICE *sndp[SND_MAX];
	uint8_t vola[SND_MAX];
	KMEVENT kme;
	KMEVENT_ITEM_ID vsync_id;

	uint8_t *readmap[8];
	uint8_t *writemap[8];

	uint32_t cps;		/* cycles per sample:fixed point */
	uint32_t cpsrem;	/* cycle remain */
	uint32_t cpsgap;	/* cycle gap */
	uint32_t cpf;		/* cycles per frame:fixed point */
	uint32_t cpfrem;	/* cycle remain */
	uint32_t total_cycles;	/* total played cycles */


	uint32_t clockmode;	// 0x05		0:NTSC 1:PAL
	uint32_t loadaddr;	// 0x08,0x09
	uint32_t initaddr;	// 0x0a,0x0b
	uint32_t playaddr;	// 0x0c,0x0d
	uint32_t stackaddr;	// 0x0e,0x0f
	uint32_t rstaddr[7];	// 0x12-0x1f
	uint8_t mappernum[4];	// 0x20-0x23
	uint32_t startsong;	// 0x24
	uint32_t maxsong;		// 0x25
	uint32_t firstse;	// 0x26
	uint32_t lastse;	// 0x27
	uint32_t systype;		// 0x28		0:SMS 1:GG 2:ColecoVision

	uint32_t tmaxsong;

	uint8_t *data;
	uint32_t datasize;
	uint8_t *bankbase;
	uint32_t bankofs;
	uint32_t banknum;
	uint32_t banksize;

	uint8_t ram[0x6000];
	uint8_t bios[0x2000];
};


static int32_t sgc_execute(SGCSEQ *THIS_)
{
	uint32_t cycles;
	THIS_->cpsrem += THIS_->cps;
	cycles = THIS_->cpsrem >> SHIFT_CPS;
	if (THIS_->cpsgap >= cycles)
		THIS_->cpsgap -= cycles;
	else
	{
		uint32_t excycles = cycles - THIS_->cpsgap;
		THIS_->cpsgap = kmz80_exec(&THIS_->ctx, excycles) - excycles;
	}
	THIS_->cpsrem &= (1 << SHIFT_CPS) - 1;
	THIS_->total_cycles += cycles;
	return 0;
}

__inline static void sgc_synth(SGCSEQ *THIS_, int32_t *d)
{
	switch (THIS_->systype)
	{
		default:
			NEVER_REACH
		case SGC_SYNTHMODE_SMS:
			THIS_->sndp[SND_SNG]->synth(THIS_->sndp[SND_SNG]->ctx, d);
			THIS_->sndp[SND_FMUNIT]->synth(THIS_->sndp[SND_FMUNIT]->ctx, d);
			break;
		case SGC_SYNTHMODE_GG:
		case SGC_SYNTHMODE_CV:
			THIS_->sndp[SND_SNG]->synth(THIS_->sndp[SND_SNG]->ctx, d);
			break;
	}
}

__inline static void sgc_volume(SGCSEQ *THIS_, uint32_t v)
{
	v += 256;
	switch (THIS_->systype)
	{
		default:
			NEVER_REACH
		case SGC_SYNTHMODE_SMS:
			THIS_->sndp[SND_SNG]->volume(THIS_->sndp[SND_SNG]->ctx, v + (THIS_->vola[SND_SNG] << 4) - SND_VOLUME);
			THIS_->sndp[SND_FMUNIT]->volume(THIS_->sndp[SND_FMUNIT]->ctx, v + (THIS_->vola[SND_FMUNIT] << 4) - SND_VOLUME);
			break;
		case SGC_SYNTHMODE_GG:
		case SGC_SYNTHMODE_CV:
			THIS_->sndp[SND_SNG]->volume(THIS_->sndp[SND_SNG]->ctx, v + (THIS_->vola[SND_SNG] << 4) - SND_VOLUME);
			break;
	}
}

static void sgc_vsync_setup(SGCSEQ *THIS_)
{
	int32_t cycles;
	THIS_->cpfrem += THIS_->cpf;
	cycles = THIS_->cpfrem >> SHIFT_CPS;
	THIS_->cpfrem &= (1 << SHIFT_CPS) - 1;
	kmevent_settimer(&THIS_->kme, THIS_->vsync_id, cycles);
}

static void sgc_play_setup(SGCSEQ *THIS_, uint32_t pc)
{
	THIS_->bios[0x0]=0xcd;
	THIS_->bios[0x1]=pc&0xff;
	THIS_->bios[0x2]=pc>>8;
	THIS_->bios[0x3]=0x76;
	THIS_->bios[0x4]=0x18;
	THIS_->bios[0x5]=0xfe;
	THIS_->ctx.pc = 0x0000;
	THIS_->ctx.regs8[REGID_HALTED] = 0;
}

static uint32_t sgc_busread_event(void *ctx, uint32_t a)
{
    (void)ctx;
    (void)a;
	return 0x38;
}

static uint32_t sgc_read_event(void *ctx, uint32_t a)
{
	SGCSEQ *THIS_ = ctx;
	if(a<0x400)
	{
		return THIS_->bios[a];
	}
	else if(a>=0xfffc && THIS_->systype <= SGC_SYNTHMODE_GG)
	{
		return THIS_->mappernum[a&0x3];
	}
	else if (THIS_->readmap[a >> 13])
	{
		return THIS_->readmap[a >> 13][a&0x1fff];
	}
	return 0x00;
}


static void sgc_write_event(void *ctx, uint32_t a, uint32_t v)
{
	SGCSEQ *THIS_ = ctx;
	uint32_t page = a >> 13;
	if (a >= 0xfffd && THIS_->systype <= SGC_SYNTHMODE_GG)
	{
		uint8_t b = (a & 0x3);
		THIS_->mappernum[b] = v;
		if(THIS_->readmap[4]==THIS_->ram && a==0xffff)return;
		THIS_->readmap[b*2-2] = THIS_->data + THIS_->mappernum[b] * 0x4000;
		THIS_->readmap[b*2-1] = THIS_->data + THIS_->mappernum[b] * 0x4000 + 0x2000;
		THIS_->writemap[b*2-2] = 0;
		THIS_->writemap[b*2-1] = 0;

	}
	else if (a == 0xfffc && THIS_->systype <= SGC_SYNTHMODE_GG)
	{
		if(v&0x8){
			//RAM
			THIS_->readmap [4] = THIS_->ram;
			THIS_->writemap[4] = THIS_->ram;
			THIS_->readmap [5] = THIS_->ram;
			THIS_->writemap[5] = THIS_->ram;
		}else{
			//ROM
			THIS_->readmap [4] = THIS_->data + THIS_->mappernum[3] * 0x4000;
			THIS_->writemap[4] = 0;
			THIS_->readmap [5] = THIS_->data + THIS_->mappernum[3] * 0x4000 + 0x2000;
			THIS_->writemap[5] = 0;
		}

	}
	else if (THIS_->writemap[page])
	{
		THIS_->writemap[page][a&0x1fff] = (uint8_t)(v & 0xff);
	}
}

static uint32_t sgc_ioread_eventSMS(void *ctx, uint32_t a)
{
    (void)ctx;
    (void)a;
	return 0xff;
}

static void sgc_iowrite_eventSMS(void *ctx, uint32_t a, uint32_t v)
{
	SGCSEQ *THIS_ = ctx;
	a &= 0xff;
	if ((a & 0xfe) == 0x7e)
		THIS_->sndp[SND_SNG]->write(THIS_->sndp[SND_SNG]->ctx, 0, v);
	else if (a == 0x06)
		THIS_->sndp[SND_SNG]->write(THIS_->sndp[SND_SNG]->ctx, 1, v);
	else if (THIS_->sndp[SND_FMUNIT] && (a & 0xfe) == 0xf0)
		THIS_->sndp[SND_FMUNIT]->write(THIS_->sndp[SND_FMUNIT]->ctx, a & 1, v);

}

static uint32_t sgc_ioread_eventCV(void *ctx, uint32_t a)
{
    (void)ctx;
    (void)a;
	return 0xff;
}

static void sgc_iowrite_eventCV(void *ctx, uint32_t a, uint32_t v)
{
	SGCSEQ *THIS_ = ctx;
	a &= 0xff;
	if ((a & 0xe0) == 0xe0)
		THIS_->sndp[SND_SNG]->write(THIS_->sndp[SND_SNG]->ctx, a & 1, v);
}

static void sgc_vsync_event(KMEVENT *event, KMEVENT_ITEM_ID curid, SGCSEQ *THIS_)
{
    (void)event;
    (void)curid;
	sgc_vsync_setup(THIS_);
	if (THIS_->ctx.regs8[REGID_HALTED]) sgc_play_setup(THIS_, THIS_->playaddr);
}


static void sgc_reset(NEZ_PLAY *pNezPlay)
{
	SGCSEQ *THIS_ = pNezPlay->sgcseq;
	uint32_t i, freq, song;

	freq = NESAudioFrequencyGet(pNezPlay);
	song = SONGINFO_GetSongNo(pNezPlay->song) - 1;
	if (song >= THIS_->tmaxsong) song = THIS_->startsong;
	if(song >= THIS_->maxsong) song += THIS_->firstse - THIS_->maxsong;

	/* sound reset */
	for (i = 0; i < SND_MAX; i++)
	{
		if (THIS_->sndp[i]) THIS_->sndp[i]->reset(THIS_->sndp[i]->ctx, BASECYCLES, freq);
	}

	/* memory reset */
	XMEMSET(THIS_->ram, 0, sizeof(THIS_->ram));
	XMEMSET(THIS_->bios, 0, sizeof(THIS_->bios));
	switch(THIS_->systype){
	case SGC_SYNTHMODE_SMS:
	case SGC_SYNTHMODE_GG:
		//0400-3FFF : ROM
		THIS_->readmap [0] = THIS_->data + THIS_->mappernum[1] * 0x4000;
		THIS_->writemap[0] = 0;
		THIS_->readmap [1] = THIS_->data + THIS_->mappernum[1] * 0x4000 + 0x2000;
		THIS_->writemap[1] = 0;
		//4000-7FFF : ROM
		THIS_->readmap [2] = THIS_->data + THIS_->mappernum[2] * 0x4000;
		THIS_->writemap[2] = 0;
		THIS_->readmap [3] = THIS_->data + THIS_->mappernum[2] * 0x4000 + 0x2000;
		THIS_->writemap[3] = 0;
		//8000-BFFF : ROM
		THIS_->readmap [4] = THIS_->data + THIS_->mappernum[3] * 0x4000;
		THIS_->writemap[4] = 0;
		THIS_->readmap [5] = THIS_->data + THIS_->mappernum[3] * 0x4000 + 0x2000;
		THIS_->writemap[5] = 0;
		//C000-FFFF : RAM
		THIS_->readmap [6] = THIS_->ram;
		THIS_->writemap[6] = THIS_->ram;
		THIS_->readmap [7] = THIS_->ram;
		THIS_->writemap[7] = THIS_->ram;
		//0000-03FF : FREE
		for (i = 1; i < 8; i++)
		{
			THIS_->bios[i*8]=0xc3;
			THIS_->bios[i*8+1]=THIS_->rstaddr[i-1]&0xff;
			THIS_->bios[i*8+2]=THIS_->rstaddr[i-1]>>8;
		}
		break;
	case SGC_SYNTHMODE_CV:
		//0000-1FFF : BIOS
		THIS_->readmap [0] = THIS_->bios;
		THIS_->writemap[0] = 0;
		//2000-5FFF : FREE
		THIS_->readmap [1] = 0;
		THIS_->writemap[1] = 0;
		THIS_->readmap [2] = 0;
		THIS_->writemap[2] = 0;
		//6000-7FFF : RAM
		THIS_->readmap [3] = THIS_->ram;
		THIS_->writemap[3] = THIS_->ram;
		//8000-FFFF : ROM
		THIS_->readmap [4] = THIS_->data + 0x0000;
		THIS_->writemap[4] = 0;
		THIS_->readmap [5] = THIS_->data + 0x2000;
		THIS_->writemap[5] = 0;
		THIS_->readmap [6] = THIS_->data + 0x4000;
		THIS_->writemap[6] = 0;
		THIS_->readmap [7] = THIS_->data + 0x6000;
		THIS_->writemap[7] = 0;

		XMEMSET(THIS_->bios, 0xc9, sizeof(THIS_->bios));

		{	//BIOSのロード
			static FILE *fp = NULL;
			fp = fopen(pNezPlay->sgc_config.coleco_bios_path, "rb");
			if (fp){
				if(fread(THIS_->bios,0x01,0x2000,fp) <= 0) {
                    XMEMSET(THIS_->bios,0,0x2000);
                }
				fclose(fp);
			}
		}
		break;
	default:
		break;
	}

	/* cpu reset */
	kmz80_reset(&THIS_->ctx);
	THIS_->ctx.user = THIS_;
	THIS_->ctx.kmevent = &THIS_->kme;
	THIS_->ctx.memread = sgc_read_event;
	THIS_->ctx.memwrite = sgc_write_event;
	switch(THIS_->systype){
	case SGC_SYNTHMODE_SMS:
	case SGC_SYNTHMODE_GG:
		THIS_->ctx.ioread = sgc_ioread_eventSMS;
		THIS_->ctx.iowrite = sgc_iowrite_eventSMS;
		THIS_->ctx.regs8[REGID_M1CYCLE] = 1;
		break;
	case SGC_SYNTHMODE_CV:
		THIS_->ctx.ioread = sgc_ioread_eventCV;
		THIS_->ctx.iowrite = sgc_iowrite_eventCV;
		THIS_->ctx.regs8[REGID_M1CYCLE] = 1;
		break;
	default:
		break;
	}
	THIS_->ctx.busread = sgc_busread_event;

	THIS_->ctx.regs8[REGID_A] = (uint8_t)((song >> 0) & 0xff);
	THIS_->ctx.regs8[REGID_H] = (uint8_t)((song >> 8) & 0xff);
	THIS_->ctx.sp = THIS_->stackaddr;
	sgc_play_setup(THIS_, THIS_->initaddr);

	THIS_->ctx.exflag = 3;	/* ICE/ACI */
	THIS_->ctx.regs8[REGID_IFF1] = THIS_->ctx.regs8[REGID_IFF2] = 0;
	THIS_->ctx.regs8[REGID_INTREQ] = 0;
	THIS_->ctx.regs8[REGID_IMODE] = 1;

	/* vsync reset */
	kmevent_init(&THIS_->kme);
	THIS_->vsync_id = kmevent_alloc(&THIS_->kme);
	kmevent_setevent(&THIS_->kme, THIS_->vsync_id, sgc_vsync_event, THIS_);
	THIS_->cpsgap = THIS_->cpsrem  = 0;
	THIS_->cpfrem  = 0;
	THIS_->cps = DivFix(BASECYCLES, freq, SHIFT_CPS);
	if (THIS_->clockmode&0x1)
		THIS_->cpf = (4 * 342 * 313 / 6) << SHIFT_CPS;
	else
		THIS_->cpf = (4 * 342 * 262 / 6) << SHIFT_CPS;

	{
		/* request execute */
		uint32_t initbreak = 5 << 8; /* 5sec */
		while (!THIS_->ctx.regs8[REGID_HALTED] && --initbreak) kmz80_exec(&THIS_->ctx, BASECYCLES >> 8);
	}
	sgc_vsync_setup(THIS_);
	if (THIS_->ctx.regs8[REGID_HALTED])
	{
		sgc_play_setup(THIS_, THIS_->playaddr);
	}
	THIS_->total_cycles = 0;

}

static void sgc_terminate(SGCSEQ *THIS_)
{
	uint32_t i;

	for (i = 0; i < SND_MAX; i++)
	{
		if (THIS_->sndp[i]) THIS_->sndp[i]->release(THIS_->sndp[i]->ctx);
	}
	if (THIS_->data) XFREE(THIS_->data);
	XFREE(THIS_);
}


static uint32_t sgc_load(NEZ_PLAY *pNezPlay, SGCSEQ *THIS_, const uint8_t *pData, uint32_t uSize)
{
	uint32_t i, headersize;
    uint8_t titlebuffer[0x21];
    uint8_t artistbuffer[0x21];
    uint8_t copyrightbuffer[0x21];
	XMEMSET(THIS_, 0, sizeof(SGCSEQ));
	for (i = 0; i < SND_MAX; i++) THIS_->sndp[i] = 0;
	for (i = 0; i < SND_MAX; i++) THIS_->vola[i] = 0x80;
	THIS_->data = 0;
	if ((GetDwordLE(pData + 0) & 0x00ffffff) == GetDwordLEM("SGC"))
	{
		headersize = 0xa0;
		THIS_->clockmode = pData[0x05];
		THIS_->loadaddr = GetWordLE(pData + 0x08);
		THIS_->initaddr = GetWordLE(pData + 0x0a);
		THIS_->playaddr = GetWordLE(pData + 0x0c);
		THIS_->stackaddr = GetWordLE(pData + 0x0e);
		for(i=0;i<7;i++)THIS_->rstaddr[i] = GetWordLE(pData + 0x12 + i*2);
		for(i=0;i<4;i++)THIS_->mappernum[i] = pData[0x20+i];
		THIS_->startsong = pData[0x24];
		THIS_->maxsong = pData[0x25];
		THIS_->firstse = pData[0x26];
		THIS_->lastse = pData[0x27];
		THIS_->systype = pData[0x28];
		if(THIS_->systype>=SGC_SYNTHMODE_MAX)return NEZ_NESERR_PARAMETER;
		if(THIS_->firstse < THIS_->maxsong
		|| THIS_->firstse > THIS_->lastse
		|| THIS_->firstse == 0){
			THIS_->firstse = THIS_->lastse = 0;
		}
		THIS_->tmaxsong = THIS_->maxsong + THIS_->lastse - THIS_->firstse + (THIS_->firstse?1:0);
	}
	else
	{
		return NEZ_NESERR_FORMAT;
	}
	SONGINFO_SetStartSongNo(pNezPlay->song, THIS_->startsong+1);
	SONGINFO_SetMaxSongNo(pNezPlay->song, THIS_->tmaxsong);
	SONGINFO_SetInitAddress(pNezPlay->song, THIS_->initaddr);
	SONGINFO_SetPlayAddress(pNezPlay->song, THIS_->playaddr);
//	SONGINFO_SetExtendDevice(pNezPlay->song, THIS_->extdevice << 8);

	sprintf(pNezPlay->songinfodata.detail,
"Type          : SGC\r\n\
System        : %s\r\n\
Song Max      : %d\r\n\
Start Song    : %d\r\n\
First SE      : %d\r\n\
Last SE       : %d\r\n\
Load Address  : %04XH\r\n\
Init Address  : %04XH\r\n\
Play Address  : %04XH\r\n\
Stack Address : %04XH\r\n\
Clock Mode    : %s\r\n\
\r\n\
RST 08H ADR   : %04XH\r\n\
RST 10H ADR   : %04XH\r\n\
RST 18H ADR   : %04XH\r\n\
RST 20H ADR   : %04XH\r\n\
RST 28H ADR   : %04XH\r\n\
RST 30H ADR   : %04XH\r\n\
RST 38H ADR   : %04XH\r\n\
\r\n\
First ROM Bank(0400-3FFF): %02XH\r\n\
First ROM Bank(4000-7FFF): %02XH\r\n\
First ROM Bank(8000-BFFF): %02XH\r\n\
RAM Bank      (8000-BFFF): %d\r\n\
"
		,sgc_system_name[THIS_->systype]
		,THIS_->maxsong
		,THIS_->startsong
		,THIS_->firstse
		,THIS_->lastse
		,THIS_->loadaddr,THIS_->initaddr,THIS_->playaddr,THIS_->stackaddr
		,THIS_->clockmode&0x01 ? "PAL" : "NTSC"
		,THIS_->rstaddr[0]
		,THIS_->rstaddr[1]
		,THIS_->rstaddr[2]
		,THIS_->rstaddr[3]
		,THIS_->rstaddr[4]
		,THIS_->rstaddr[5]
		,THIS_->rstaddr[6]
		,THIS_->mappernum[1]
		,THIS_->mappernum[2]
		,THIS_->mappernum[3]
		,THIS_->mappernum[0]&0x8 ? 1:0
	);

	XMEMSET(titlebuffer, 0, 0x21);
	XMEMCPY(titlebuffer, pData + 0x0040, 0x20);

    if(pNezPlay->songinfodata.title != NULL) {
        XFREE(pNezPlay->songinfodata.title);
    }
    pNezPlay->songinfodata.title = (char *)XMALLOC(strlen((const char *)titlebuffer)+1);
    if(pNezPlay->songinfodata.title == NULL) {
        return NEZ_NESERR_SHORTOFMEMORY;
    }
    XMEMCPY(pNezPlay->songinfodata.title,titlebuffer,strlen((const char *)titlebuffer)+1);

	XMEMSET(artistbuffer, 0, 0x21);
	XMEMCPY(artistbuffer, pData + 0x0060, 0x20);

    if(pNezPlay->songinfodata.artist != NULL) {
        XFREE(pNezPlay->songinfodata.artist);
    }
    pNezPlay->songinfodata.artist = (char *)XMALLOC(strlen((const char *)artistbuffer)+1);
    if(pNezPlay->songinfodata.artist == NULL) {
        return NEZ_NESERR_SHORTOFMEMORY;
    }
    XMEMCPY(pNezPlay->songinfodata.artist,artistbuffer,strlen((const char *)artistbuffer)+1);

	XMEMSET(copyrightbuffer, 0, 0x21);
	XMEMCPY(copyrightbuffer, pData + 0x0080, 0x20);

    if(pNezPlay->songinfodata.copyright != NULL) {
        XFREE(pNezPlay->songinfodata.copyright);
    }
    pNezPlay->songinfodata.copyright = (char *)XMALLOC(strlen((const char *)copyrightbuffer)+1);
    if(pNezPlay->songinfodata.copyright == NULL) {
        return NEZ_NESERR_SHORTOFMEMORY;
    }
    XMEMCPY(pNezPlay->songinfodata.copyright,copyrightbuffer,strlen((const char *)copyrightbuffer)+1);

	switch(THIS_->systype){
	case SGC_SYNTHMODE_SMS:
		THIS_->sndp[SND_SNG] = SNGSoundAlloc(pNezPlay,SNG_TYPE_SEGAMKIII);
		THIS_->sndp[SND_FMUNIT] = OPLSoundAlloc(pNezPlay,OPL_TYPE_SMSFMUNIT);
		if (!THIS_->sndp[SND_FMUNIT]) return NEZ_NESERR_SHORTOFMEMORY;
		SONGINFO_SetChannel(pNezPlay->song, 1);
		THIS_->datasize = uSize - headersize;
		THIS_->banknum = ((THIS_->datasize+THIS_->loadaddr)>>14)+1;
		if(THIS_->banknum < 3)THIS_->banknum = 3;
		THIS_->banksize = 0x4000;
		if(THIS_->mappernum[1] >= THIS_->banknum)return NEZ_NESERR_PARAMETER;
		if(THIS_->mappernum[2] >= THIS_->banknum)return NEZ_NESERR_PARAMETER;
		if(THIS_->mappernum[3] >= THIS_->banknum)return NEZ_NESERR_PARAMETER;

		THIS_->data = XMALLOC(THIS_->banksize * THIS_->banknum + 8);
		if (!THIS_->data) return NEZ_NESERR_SHORTOFMEMORY;

		XMEMSET(THIS_->data, 0, THIS_->banknum * THIS_->banksize);

		XMEMCPY(THIS_->data + THIS_->loadaddr, pData + headersize, THIS_->datasize);

		break;
	case SGC_SYNTHMODE_GG:
		THIS_->sndp[SND_SNG] = SNGSoundAlloc(pNezPlay,SNG_TYPE_GAMEGEAR);
		SONGINFO_SetChannel(pNezPlay->song, 2);
		THIS_->datasize = uSize - headersize;
		THIS_->banknum = ((THIS_->datasize+THIS_->loadaddr)>>14)+1;
		if(THIS_->banknum < 3)THIS_->banknum = 3;
		THIS_->banksize = 0x4000;
		if(THIS_->mappernum[1] >= THIS_->banknum)return NEZ_NESERR_PARAMETER;
		if(THIS_->mappernum[2] >= THIS_->banknum)return NEZ_NESERR_PARAMETER;
		if(THIS_->mappernum[3] >= THIS_->banknum)return NEZ_NESERR_PARAMETER;

		THIS_->data = XMALLOC(THIS_->banksize * THIS_->banknum + 8);
		if (!THIS_->data) return NEZ_NESERR_SHORTOFMEMORY;

		XMEMSET(THIS_->data, 0, THIS_->banknum * THIS_->banksize);

		XMEMCPY(THIS_->data + THIS_->loadaddr, pData + headersize, THIS_->datasize);

		break;
	case SGC_SYNTHMODE_CV:
		THIS_->sndp[SND_SNG] = SNGSoundAlloc(pNezPlay,SNG_TYPE_SN76489AN);
		SONGINFO_SetChannel(pNezPlay->song, 1);
		THIS_->banknum = 4;
		THIS_->banksize = 0x2000;
		THIS_->datasize = uSize - headersize;
		if (THIS_->datasize > 0x8000)
			THIS_->datasize = 0x10000 - THIS_->loadaddr;
		THIS_->data = XMALLOC(THIS_->banksize * THIS_->banknum + 8);
		if (!THIS_->data) return NEZ_NESERR_SHORTOFMEMORY;

		XMEMSET(THIS_->data, 0, THIS_->banknum * THIS_->banksize);

		XMEMCPY(THIS_->data + THIS_->loadaddr - 0x8000, pData + headersize, THIS_->datasize);

		break;
	default:
		break;
	}

	return NEZ_NESERR_NOERROR;
}

static int32_t SGCSEQExecuteZ80CPU(NEZ_PLAY *pNezPlay)
{
	sgc_execute(((NEZ_PLAY*)pNezPlay)->sgcseq);
	return 0;
}

static void SGCSEQSoundRenderStereo(NEZ_PLAY *pNezPlay, int32_t *d)
{
	sgc_synth(((NEZ_PLAY*)pNezPlay)->sgcseq, d);
}

static int32_t SGCSEQSoundRenderMono(NEZ_PLAY *pNezPlay)
{
	int32_t d[2] = { 0, 0 };
	sgc_synth(((NEZ_PLAY*)pNezPlay)->sgcseq, d);
#if (((-1) >> 1) == -1)
	return (d[0] + d[1]) >> 1;
#else
	return (d[0] + d[1]) / 2;
#endif
}

static const NEZ_NES_AUDIO_HANDLER sgcseq_audio_handler[] = {
	{ 0, SGCSEQExecuteZ80CPU, 0, NULL },
	{ 3, SGCSEQSoundRenderMono, SGCSEQSoundRenderStereo, NULL },
	{ 0, 0, 0, NULL },
};

static void SGCSEQVolume(NEZ_PLAY *pNezPlay, uint32_t v)
{
	if (((NEZ_PLAY*)pNezPlay)->sgcseq)
	{
		sgc_volume(((NEZ_PLAY*)pNezPlay)->sgcseq, v);
	}
}

static const NEZ_NES_VOLUME_HANDLER sgcseq_volume_handler[] = {
	{ SGCSEQVolume, NULL },
	{ 0, NULL },
};

static void SGCSEQReset(NEZ_PLAY *pNezPlay)
{
	if (((NEZ_PLAY*)pNezPlay)->sgcseq) sgc_reset((NEZ_PLAY*)pNezPlay);
}

static const NEZ_NES_RESET_HANDLER sgcseq_reset_handler[] = {
	{ NES_RESET_SYS_LAST, SGCSEQReset, NULL },
	{ 0,                  0, NULL },
};

static void SGCSEQTerminate(NEZ_PLAY *pNezPlay)
{
	if (((NEZ_PLAY*)pNezPlay)->sgcseq)
	{
		sgc_terminate(((NEZ_PLAY*)pNezPlay)->sgcseq);
		((NEZ_PLAY*)pNezPlay)->sgcseq = 0;
	}
}

static const NEZ_NES_TERMINATE_HANDLER sgcseq_terminate_handler[] = {
	{ SGCSEQTerminate, NULL },
	{ 0, NULL },
};

PROTECTED uint32_t SGCLoad(NEZ_PLAY *pNezPlay, const uint8_t *pData, uint32_t uSize)
{
	uint32_t ret;
	SGCSEQ *THIS_;
	if (pNezPlay->sgcseq) return NEZ_NESERR_FORMAT; //__builtin_trap();	/* ASSERT */
	THIS_ = XMALLOC(sizeof(SGCSEQ));
	if (!THIS_) return NEZ_NESERR_SHORTOFMEMORY;
	ret = sgc_load(pNezPlay, THIS_, pData, uSize);
	if (ret)
	{
		sgc_terminate(THIS_);
		return ret;
	}
	pNezPlay->sgcseq = THIS_;
	NESAudioHandlerInstall(pNezPlay, sgcseq_audio_handler);
	NESVolumeHandlerInstall(pNezPlay, sgcseq_volume_handler);
	NESResetHandlerInstall(pNezPlay->nrh, sgcseq_reset_handler);
	NESTerminateHandlerInstall(&pNezPlay->nth, sgcseq_terminate_handler);
	return ret;
}
#undef SHIFT_CPS
#undef BASECYCLES
#undef EXTDEVICE_SNG
#undef EXTDEVICE_FMUNIT
#undef EXTDEVICE_GGSTEREO
#undef EXTDEVICE_GGRAM
#undef EXTDEVICE_MSXMUSIC
#undef EXTDEVICE_MSXRAM
#undef EXTDEVICE_MSXAUDIO
#undef EXTDEVICE_MSXSTEREO
#undef DEVICE_PAL
#undef SND_SNG
#undef SND_FMUNIT
#undef SND_MAX
#undef SND_VOLUME
