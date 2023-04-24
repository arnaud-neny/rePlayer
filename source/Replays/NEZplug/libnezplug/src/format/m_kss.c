#include "../include/nezplug/nezplug.h"
#include "handler.h"
#include "audiosys.h"
#include "songinfo.h"

#include "m_kss.h"

#include "../device/s_psg.h"
#include "../device/s_scc.h"
#include "../device/s_sng.h"
#include "../device/opl/s_opl.h"
#include "../common/divfix.h"
#include "../common/util.h"

#include "../cpu/kmz80/kmz80.h"

#include <stdio.h>

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

#define EXTDEVICE_PAL		(1 << 6)
#define EXTDEVICE_EXRAM		(1 << 7)

#define SND_PSG 0
#define SND_SCC 1
#define SND_MSXMUSIC 2
#define SND_MSXAUDIO 3
#define SND_MAX 4

#define SND_SNG 0
#define SND_FMUNIT 2

#define SND_VOLUME 0x800


typedef struct KSSSEQ_TAG KSSSEQ;

struct  KSSSEQ_TAG {
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

	uint32_t startsong;
	uint32_t maxsong;

	uint32_t initaddr;
	uint32_t playaddr;

	uint8_t *data;
	uint32_t dataaddr;
	uint32_t datasize;
	uint8_t *bankbase;
	uint32_t bankofs;
	uint32_t banknum;
	uint32_t banksize;
    int32_t *msx_psg_volume;
    int32_t *msx_psg_type;
	enum
	{
		KSS_BANK_OFF,
		KSS_16K_MAPPER,
		KSS_8K_MAPPER
	} bankmode;
	enum
	{
		KSS_SYNTHMODE_SMS,
		KSS_SYNTHMODE_MSX,
		KSS_SYNTHMODE_MSXSTEREO
	} synthmode;
	uint8_t sccenable;
	uint8_t rammode;
	uint8_t extdevice;
	uint8_t majutushimode;

	uint8_t ram[0x10000];
	uint8_t usertone_enable[2];
	uint8_t usertone[2][16 * 19];
};

static int32_t kss_execute(KSSSEQ *THIS_)
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

__inline static void kss_synth(KSSSEQ *THIS_, int32_t *d)
{
	switch (THIS_->synthmode)
	{
		default:
			NEVER_REACH
		case KSS_SYNTHMODE_SMS:
			THIS_->sndp[SND_SNG]->synth(THIS_->sndp[SND_SNG]->ctx, d);
			if (THIS_->sndp[SND_FMUNIT]) THIS_->sndp[SND_FMUNIT]->synth(THIS_->sndp[SND_FMUNIT]->ctx, d);
			break;
		case KSS_SYNTHMODE_MSX:
			{
				int32_t b[2];
				b[0] = 0;
				THIS_->sndp[SND_PSG]->synth(THIS_->sndp[SND_PSG]->ctx, b);
				b[0] = b[0] * THIS_->msx_psg_volume[0] / 64;
				d[0] += b[0];
				d[1] += b[0];
				THIS_->sndp[SND_SCC]->synth(THIS_->sndp[SND_SCC]->ctx, d);
				if (THIS_->sndp[SND_MSXMUSIC]) THIS_->sndp[SND_MSXMUSIC]->synth(THIS_->sndp[SND_MSXMUSIC]->ctx, d);
				if (THIS_->sndp[SND_MSXAUDIO]) THIS_->sndp[SND_MSXAUDIO]->synth(THIS_->sndp[SND_MSXAUDIO]->ctx, d);
			}
			break;
		case KSS_SYNTHMODE_MSXSTEREO:
			{
				int32_t b[3];
				b[0] = b[1] = b[2] = 0;
				THIS_->sndp[SND_PSG]->synth(THIS_->sndp[SND_PSG]->ctx, &b[0]);
				b[0] = b[1] = b[0] * THIS_->msx_psg_volume[0] / 64;
				THIS_->sndp[SND_SCC]->synth(THIS_->sndp[SND_SCC]->ctx, &b[0]);
				if (THIS_->sndp[SND_MSXMUSIC]) THIS_->sndp[SND_MSXMUSIC]->synth(THIS_->sndp[SND_MSXMUSIC]->ctx, &b[0]);
				if (THIS_->sndp[SND_MSXAUDIO]) THIS_->sndp[SND_MSXAUDIO]->synth(THIS_->sndp[SND_MSXAUDIO]->ctx, &b[1]);
				d[0] += b[1];
				d[1] += b[2] + b[2];
			}
			break;
	}
}

__inline static void kss_volume(KSSSEQ *THIS_, uint32_t v)
{
	v += 256;
	switch (THIS_->synthmode)
	{
		default:
			NEVER_REACH
		case KSS_SYNTHMODE_SMS:
			THIS_->sndp[SND_SNG]->volume(THIS_->sndp[SND_SNG]->ctx, v + (THIS_->vola[SND_SNG] << 4) - SND_VOLUME);
			if (THIS_->sndp[SND_FMUNIT]) THIS_->sndp[SND_FMUNIT]->volume(THIS_->sndp[SND_FMUNIT]->ctx, v + (THIS_->vola[SND_FMUNIT] << 4) - SND_VOLUME);
			break;
		case KSS_SYNTHMODE_MSX:
		case KSS_SYNTHMODE_MSXSTEREO:
			THIS_->sndp[SND_PSG]->volume(THIS_->sndp[SND_PSG]->ctx, v + (THIS_->vola[SND_PSG] << 4) - SND_VOLUME);
			THIS_->sndp[SND_SCC]->volume(THIS_->sndp[SND_SCC]->ctx, v + (THIS_->vola[SND_SCC] << 4) - SND_VOLUME);
			if (THIS_->sndp[SND_MSXMUSIC]) THIS_->sndp[SND_MSXMUSIC]->volume(THIS_->sndp[SND_MSXMUSIC]->ctx, v + (THIS_->vola[SND_MSXMUSIC] << 4) - SND_VOLUME);
			if (THIS_->sndp[SND_MSXAUDIO]) THIS_->sndp[SND_MSXAUDIO]->volume(THIS_->sndp[SND_MSXAUDIO]->ctx, v + (THIS_->vola[SND_MSXAUDIO] << 4) - SND_VOLUME);
			break;
	}
}

static void kss_vsync_setup(KSSSEQ *THIS_)
{
	int32_t cycles;
	THIS_->cpfrem += THIS_->cpf;
	cycles = THIS_->cpfrem >> SHIFT_CPS;
	THIS_->cpfrem &= (1 << SHIFT_CPS) - 1;
	kmevent_settimer(&THIS_->kme, THIS_->vsync_id, cycles);
}

static void kss_play_setup(KSSSEQ *THIS_, uint32_t pc)
{
	uint32_t sp = 0xF380, rp;
	THIS_->ram[--sp] = 0;
	THIS_->ram[--sp] = 0xfe;
	THIS_->ram[--sp] = 0x18;	/* JR +0 */
	THIS_->ram[--sp] = 0x76;	/* HALT */
	rp = sp;
	THIS_->ram[--sp] = (uint8_t)(rp >> 8);
	THIS_->ram[--sp] = (uint8_t)(rp & 0xff);
	THIS_->ctx.sp = sp;
	THIS_->ctx.pc = pc;
	THIS_->ctx.regs8[REGID_HALTED] = 0;
}

static uint32_t kss_busread_event(void *ctx, uint32_t a)
{
    (void)ctx;
    (void)a;
	return 0x38;
}

static uint32_t kss_read_event(void *ctx, uint32_t a)
{
	KSSSEQ *THIS_ = ctx;
	return THIS_->readmap[a >> 13][a];
}



static void KSSMaper8KWrite(KSSSEQ *THIS_, uint32_t a, uint32_t v)
{
	uint32_t page = a >> 13;
	v -= THIS_->bankofs;
	if (v < THIS_->banknum)
	{
		THIS_->readmap[page] = THIS_->bankbase + THIS_->banksize * v - a;
	}
	else
	{
		THIS_->readmap[page] = THIS_->ram;
	}
}

static void KSSMaper16KWrite(KSSSEQ *THIS_, uint32_t a, uint32_t v)
{
	uint32_t page = a >> 13;
	v -= THIS_->bankofs;
	if (v < THIS_->banknum)
	{
		THIS_->readmap[page] = THIS_->readmap[page + 1] = THIS_->bankbase + THIS_->banksize * v - a;
		if (!(THIS_->extdevice & EXTDEVICE_SNG)) THIS_->sccenable = 1;
	}
	else
	{
		THIS_->readmap[page] = THIS_->readmap[page + 1] = THIS_->ram;
		if (!(THIS_->extdevice & EXTDEVICE_SNG)) THIS_->sccenable = !THIS_->rammode;
	}
}

static void kss_write_event(void *ctx, uint32_t a, uint32_t v)
{
	KSSSEQ *THIS_ = ctx;
	uint32_t page = a >> 13;
	if (THIS_->writemap[page])
	{
		THIS_->writemap[page][a] = (uint8_t)(v & 0xff);
	}
	else if (THIS_->bankmode == KSS_8K_MAPPER)
	{
		if (a == 0x9000)
			KSSMaper8KWrite(THIS_, 0x8000, v);
		else if (a == 0xb000)
			KSSMaper8KWrite(THIS_, 0xa000, v);
		else if (THIS_->sccenable)
			THIS_->sndp[SND_SCC]->write(THIS_->sndp[SND_SCC]->ctx, a, v);
	}
	else
	{
		if (THIS_->sccenable)
			THIS_->sndp[SND_SCC]->write(THIS_->sndp[SND_SCC]->ctx, a, v);
		else if (THIS_->rammode)
			THIS_->ram[a] = (uint8_t)(v & 0xff);
	}
}

static uint32_t kss_ioread_eventSMS(void *ctx, uint32_t a)
{
    (void)ctx;
    (void)a;
	return 0xff;
}
static uint32_t kss_ioread_eventMSX(void *ctx, uint32_t a)
{
	KSSSEQ *THIS_ = ctx;
	a &= 0xff;
	if (a == 0xa2)
		return THIS_->sndp[SND_PSG]->read(THIS_->sndp[SND_PSG]->ctx, 0);
	if (THIS_->sndp[SND_MSXAUDIO] && (a & 0xfe) == 0xc0)
		return THIS_->sndp[SND_MSXAUDIO]->read(THIS_->sndp[SND_MSXAUDIO]->ctx, a & 1);
	return 0xff;
}

static void kss_iowrite_eventSMS(void *ctx, uint32_t a, uint32_t v)
{
	KSSSEQ *THIS_ = ctx;
	a &= 0xff;
	if ((a & 0xfe) == 0x7e)
		THIS_->sndp[SND_SNG]->write(THIS_->sndp[SND_SNG]->ctx, 0, v);
	else if (a == 0x06)
		THIS_->sndp[SND_SNG]->write(THIS_->sndp[SND_SNG]->ctx, 1, v);
	else if (THIS_->sndp[SND_FMUNIT] && (a & 0xfe) == 0xf0)
		THIS_->sndp[SND_FMUNIT]->write(THIS_->sndp[SND_FMUNIT]->ctx, a & 1, v);
	else if (a == 0xfe && THIS_->bankmode == KSS_16K_MAPPER)
		KSSMaper16KWrite(THIS_, 0x8000, v);
}
static void kss_iowrite_eventMSX(void *ctx, uint32_t a, uint32_t v)
{
	KSSSEQ *THIS_ = ctx;
	a &= 0xff;
	if ((a & 0xfe) == 0xa0)
		THIS_->sndp[SND_PSG]->write(THIS_->sndp[SND_PSG]->ctx, a & 1, v);
	else if (a == 0xab)
		THIS_->sndp[SND_PSG]->write(THIS_->sndp[SND_PSG]->ctx, 2, v & 1);
	else if (THIS_->sndp[SND_MSXMUSIC] && (a & 0xfe) == 0x7c)
		THIS_->sndp[SND_MSXMUSIC]->write(THIS_->sndp[SND_MSXMUSIC]->ctx, a & 1, v);
	else if (THIS_->sndp[SND_MSXAUDIO] && (a & 0xfe) == 0xc0)
		THIS_->sndp[SND_MSXAUDIO]->write(THIS_->sndp[SND_MSXAUDIO]->ctx, a & 1, v);
	else if (a == 0xfe && THIS_->bankmode == KSS_16K_MAPPER)
		KSSMaper16KWrite(THIS_, 0x8000, v);
}

static void kss_vsync_event(KMEVENT *event, KMEVENT_ITEM_ID curid, KSSSEQ *THIS_)
{
    (void)event;
    (void)curid;
	kss_vsync_setup(THIS_);
	if (THIS_->ctx.regs8[REGID_HALTED]) kss_play_setup(THIS_, THIS_->playaddr);
}

static void kss_reset(NEZ_PLAY *pNezPlay)
{
	KSSSEQ *THIS_ = pNezPlay->kssseq;
	uint32_t i, freq, song;

	freq = NESAudioFrequencyGet(pNezPlay);
	song = SONGINFO_GetSongNo(pNezPlay->song) - 1;
	if (song >= THIS_->maxsong) song = THIS_->startsong - 1;

	/* sound reset */
	if (THIS_->extdevice & EXTDEVICE_SNG)
	{
		if (THIS_->sndp[SND_FMUNIT] && THIS_->usertone_enable[1]) THIS_->sndp[SND_FMUNIT]->setinst(THIS_->sndp[SND_FMUNIT]->ctx, 0, THIS_->usertone[1], 16 * 19);
	}
	else
	{
		if (THIS_->sndp[SND_MSXMUSIC] && THIS_->usertone_enable[0]) THIS_->sndp[SND_MSXMUSIC]->setinst(THIS_->sndp[SND_MSXMUSIC]->ctx, 0, THIS_->usertone[0], 16 * 19);
	}
	for (i = 0; i < SND_MAX; i++)
	{
		if (THIS_->sndp[i]) THIS_->sndp[i]->reset(THIS_->sndp[i]->ctx, BASECYCLES, freq);
	}

	/* memory reset */
	XMEMSET(THIS_->ram, 0xC9, 0x4000);
	XMEMCPY(THIS_->ram + 0x93, "\xC3\x01\x00\xC3\x09\x00", 6);
	XMEMCPY(THIS_->ram + 0x01, "\xD3\xA0\xF5\x7B\xD3\xA1\xF1\xC9", 8);
	XMEMCPY(THIS_->ram + 0x09, "\xD3\xA0\xDB\xA2\xC9", 5);
	XMEMSET(THIS_->ram + 0x4000, 0, 0xC000);
	if (THIS_->datasize)
	{
		if (THIS_->dataaddr + THIS_->datasize > 0x10000)
			XMEMCPY(THIS_->ram + THIS_->dataaddr, THIS_->data, 0x10000 - THIS_->dataaddr);
		else
			XMEMCPY(THIS_->ram + THIS_->dataaddr, THIS_->data, THIS_->datasize);
	}
	for (i = 0; i < 8; i++)
	{
		THIS_->readmap[i] = THIS_->ram;
		THIS_->writemap[i] = THIS_->ram;
	}
	THIS_->writemap[4] = THIS_->writemap[5] = 0;
	if (THIS_->majutushimode)
	{
		THIS_->writemap[2] = 0;
		THIS_->writemap[3] = 0;
	}
	if (!(THIS_->extdevice & EXTDEVICE_SNG)) THIS_->sccenable = !THIS_->rammode;

	/* cpu reset */
	kmz80_reset(&THIS_->ctx);
	THIS_->ctx.user = THIS_;
	THIS_->ctx.kmevent = &THIS_->kme;
	THIS_->ctx.memread = kss_read_event;
	THIS_->ctx.memwrite = kss_write_event;
	if (THIS_->extdevice & EXTDEVICE_SNG)
	{
		THIS_->ctx.ioread = kss_ioread_eventSMS;
		THIS_->ctx.iowrite = kss_iowrite_eventSMS;
		THIS_->ctx.regs8[REGID_M1CYCLE] = 1;
	}
	else
	{
		THIS_->ctx.ioread = kss_ioread_eventMSX;
		THIS_->ctx.iowrite = kss_iowrite_eventMSX;
		THIS_->ctx.regs8[REGID_M1CYCLE] = 2;
	}
	THIS_->ctx.busread = kss_busread_event;

	THIS_->ctx.regs8[REGID_A] = (uint8_t)((song >> 0) & 0xff);
	THIS_->ctx.regs8[REGID_H] = (uint8_t)((song >> 8) & 0xff);
	kss_play_setup(THIS_, THIS_->initaddr);

	THIS_->ctx.exflag = 3;	/* ICE/ACI */
	THIS_->ctx.regs8[REGID_IFF1] = THIS_->ctx.regs8[REGID_IFF2] = 0;
	THIS_->ctx.regs8[REGID_INTREQ] = 0;
	THIS_->ctx.regs8[REGID_IMODE] = 1;

	/* vsync reset */
	kmevent_init(&THIS_->kme);
	THIS_->vsync_id = kmevent_alloc(&THIS_->kme);
	kmevent_setevent(&THIS_->kme, THIS_->vsync_id, kss_vsync_event, THIS_);
	THIS_->cpsgap = THIS_->cpsrem  = 0;
	THIS_->cpfrem  = 0;
	THIS_->cps = DivFix(BASECYCLES, freq, SHIFT_CPS);
	if (THIS_->extdevice & EXTDEVICE_PAL)
		THIS_->cpf = (4 * 342 * 313 / 6) << SHIFT_CPS;
	else
		THIS_->cpf = (4 * 342 * 262 / 6) << SHIFT_CPS;

	{
		/* request execute */
		uint32_t initbreak = 5 << 8; /* 5sec */
		while (!THIS_->ctx.regs8[REGID_HALTED] && --initbreak) kmz80_exec(&THIS_->ctx, BASECYCLES >> 8);
	}
	kss_vsync_setup(THIS_);
	if (THIS_->ctx.regs8[REGID_HALTED])
	{
		kss_play_setup(THIS_, THIS_->playaddr);
	}
	THIS_->total_cycles = 0;
}

static void kss_terminate(KSSSEQ *THIS_)
{
	uint32_t i;

	for (i = 0; i < SND_MAX; i++)
	{
		if (THIS_->sndp[i]) THIS_->sndp[i]->release(THIS_->sndp[i]->ctx);
	}
	if (THIS_->data) XFREE(THIS_->data);
	XFREE(THIS_);
}

static uint32_t kss_load(NEZ_PLAY *pNezPlay, KSSSEQ *THIS_, const uint8_t *pData, uint32_t uSize)
{
	uint32_t i, headersize;
	XMEMSET(THIS_, 0, sizeof(KSSSEQ));
	for (i = 0; i < SND_MAX; i++) THIS_->sndp[i] = 0;
	for (i = 0; i < SND_MAX; i++) THIS_->vola[i] = 0x80;
	THIS_->data = 0;

	if (GetDwordLE(pData) == GetDwordLEM("KSCC"))
	{
		headersize = 0x10;
		THIS_->startsong = 1;
		THIS_->maxsong = 256;
	}
	else if (GetDwordLE(pData) == GetDwordLEM("KSSX"))
	{
		headersize = 0x10 + pData[0x0e];
		if (pData[0x0e] >= 0x0c)
		{
			THIS_->startsong = GetWordLE(pData + 0x18) + 1;
			THIS_->maxsong = GetWordLE(pData + 0x1a) + 1;
		}
		else
		{
			THIS_->startsong = 1;
			THIS_->maxsong = 256;
		}
		if (pData[0x0e] >= 0x10)
		{
			THIS_->vola[SND_PSG] = 0x100 - (pData[0x1c] ^ 0x80);
			THIS_->vola[SND_SCC] = 0x100 - (pData[0x1d] ^ 0x80);
			THIS_->vola[SND_MSXMUSIC] = 0x100 - (pData[0x1e] ^ 0x80);
			THIS_->vola[SND_MSXAUDIO] = 0x100 - (pData[0x1f] ^ 0x80);
		}
	}
	else
	{
		return NEZ_NESERR_FORMAT;
	}
	THIS_->dataaddr = GetWordLE(pData + 0x04);
	THIS_->datasize = GetWordLE(pData + 0x06);
	THIS_->initaddr = GetWordLE(pData + 0x08);
	THIS_->playaddr = GetWordLE(pData + 0x0A);
	THIS_->bankofs = pData[0x0C];
	THIS_->banknum = pData[0x0D];
	THIS_->extdevice = pData[0x0F];
	SONGINFO_SetStartSongNo(pNezPlay->song, THIS_->startsong);
	SONGINFO_SetMaxSongNo(pNezPlay->song, THIS_->maxsong);
	SONGINFO_SetInitAddress(pNezPlay->song, THIS_->initaddr);
	SONGINFO_SetPlayAddress(pNezPlay->song, THIS_->playaddr);
	SONGINFO_SetExtendDevice(pNezPlay->song, THIS_->extdevice << 8);

    if(pNezPlay->songinfodata.title != NULL) {
        XFREE(pNezPlay->songinfodata.title);
        pNezPlay->songinfodata.title = NULL;
    }

    if(pNezPlay->songinfodata.artist != NULL) {
        XFREE(pNezPlay->songinfodata.artist);
        pNezPlay->songinfodata.artist = NULL;
    }

    if(pNezPlay->songinfodata.copyright != NULL) {
        XFREE(pNezPlay->songinfodata.copyright);
        pNezPlay->songinfodata.copyright = NULL;
    }

	sprintf(pNezPlay->songinfodata.detail,
"Type         : KS%c%c\r\n\
Load Address : %04XH\r\n\
Load Size    : %04XH\r\n\
Init Address : %04XH\r\n\
Play Address : %04XH\r\n\
Extra Device : %s%s%s%s%s"
		,pData[0x02],pData[0x03]
		,THIS_->dataaddr,THIS_->datasize,THIS_->initaddr,THIS_->playaddr
		,pData[0x0F]&0x01 ? (pData[0x0F]&0x02 ? "FMUNIT " : "FMPAC ") : ""
		,pData[0x0F]&0x02 ? "SN76489 " : ""
		,pData[0x0F]&0x04 ? (pData[0x0F]&0x02 ? "GGstereo " : "RAM ") : ""
		,pData[0x0F]&0x08 ? (pData[0x0F]&0x02 ? "RAM " : "MSX-AUDIO ") : ""
		,pData[0x0F] ? "" : "None"
	);

	if (THIS_->banknum & 0x80)
	{
		THIS_->banknum &= 0x7f;
		THIS_->bankmode = KSS_8K_MAPPER;
		THIS_->banksize = 0x2000;
	}
	else if (THIS_->banknum)
	{
		THIS_->bankmode = KSS_16K_MAPPER;
		THIS_->banksize = 0x4000;
	}
	else
	{
		THIS_->bankmode = KSS_BANK_OFF;
	}
	THIS_->data = XMALLOC(THIS_->datasize + THIS_->banksize * THIS_->banknum + 8);
	if (!THIS_->data) return NEZ_NESERR_SHORTOFMEMORY;
	THIS_->bankbase = THIS_->data + THIS_->datasize;
	if (uSize > 0x10 && THIS_->datasize)
	{
		uint32_t size;
		XMEMSET(THIS_->data, 0, THIS_->datasize);
		if (THIS_->datasize < uSize - headersize)
			size = THIS_->datasize;
		else
			size = uSize - headersize;
		XMEMCPY(THIS_->data, pData + headersize, size);
	}
	else
	{
		THIS_->datasize = 0;
	}
	if (uSize > (headersize + THIS_->datasize) && THIS_->bankmode != KSS_BANK_OFF)
	{
		uint32_t size;
		XMEMSET(THIS_->bankbase, 0, THIS_->banksize * THIS_->banknum);
		if (THIS_->banksize * THIS_->banknum < uSize - (headersize + THIS_->datasize))
			size = THIS_->banksize * THIS_->banknum;
		else
			size = uSize - (headersize + THIS_->datasize);
		XMEMCPY(THIS_->bankbase, pData + (headersize + THIS_->datasize), size);
	}
	else
	{
		THIS_->banknum = 0;
		THIS_->bankmode = KSS_BANK_OFF;
	}


	THIS_->majutushimode = 0;
	if (THIS_->extdevice & EXTDEVICE_SNG)
	{
		THIS_->synthmode = KSS_SYNTHMODE_SMS;
		if (THIS_->extdevice & EXTDEVICE_GGSTEREO)
		{
			THIS_->sndp[SND_SNG] = SNGSoundAlloc(pNezPlay,SNG_TYPE_GAMEGEAR);
			SONGINFO_SetChannel(pNezPlay->song, 2);
		}
		else
		{
			THIS_->sndp[SND_SNG] = SNGSoundAlloc(pNezPlay,SNG_TYPE_SEGAMKIII);
			SONGINFO_SetChannel(pNezPlay->song, 1);
		}
		if (!THIS_->sndp[SND_SNG]) return NEZ_NESERR_SHORTOFMEMORY;
		if (THIS_->extdevice & EXTDEVICE_FMUNIT)
		{
			THIS_->sndp[SND_FMUNIT] = OPLSoundAlloc(pNezPlay,OPL_TYPE_SMSFMUNIT);
			if (!THIS_->sndp[SND_FMUNIT]) return NEZ_NESERR_SHORTOFMEMORY;
		}
		if (THIS_->extdevice & (EXTDEVICE_EXRAM | EXTDEVICE_GGRAM))
		{
			THIS_->rammode = 1;
		}
		else
		{
			THIS_->rammode = 0;
		}
		THIS_->sccenable = 0;
	}
	else
	{
		if (THIS_->extdevice & EXTDEVICE_MSXSTEREO)
		{
			if (THIS_->extdevice & EXTDEVICE_MSXAUDIO)
			{
				SONGINFO_SetChannel(pNezPlay->song, 2);
				THIS_->synthmode = KSS_SYNTHMODE_MSXSTEREO;
			}
			else
			{
				SONGINFO_SetChannel(pNezPlay->song, 1);
				THIS_->synthmode = KSS_SYNTHMODE_MSX;
				THIS_->majutushimode = 1;
			}
		}
		else
		{
			SONGINFO_SetChannel(pNezPlay->song, 1);
			THIS_->synthmode = KSS_SYNTHMODE_MSX;
		}
		if(pNezPlay->kss_config.msx_psg_type){
			THIS_->sndp[SND_PSG] = PSGSoundAlloc(pNezPlay,PSG_TYPE_YM2149);
		}else{
			THIS_->sndp[SND_PSG] = PSGSoundAlloc(pNezPlay,PSG_TYPE_AY_3_8910);
		}
		if (!THIS_->sndp[SND_PSG]) return NEZ_NESERR_SHORTOFMEMORY;
		if (THIS_->extdevice & EXTDEVICE_MSXMUSIC)
		{
			THIS_->sndp[SND_MSXMUSIC] = OPLSoundAlloc(pNezPlay,OPL_TYPE_MSXMUSIC);
			if (!THIS_->sndp[SND_MSXMUSIC]) return NEZ_NESERR_SHORTOFMEMORY;
		}
		if (THIS_->extdevice & EXTDEVICE_MSXAUDIO)
		{
			THIS_->sndp[SND_MSXAUDIO] = OPLSoundAlloc(pNezPlay,OPL_TYPE_MSXAUDIO);
			if (!THIS_->sndp[SND_MSXAUDIO]) return NEZ_NESERR_SHORTOFMEMORY;
		}
		if (THIS_->extdevice & EXTDEVICE_EXRAM)
		{
			THIS_->rammode = 1;
			THIS_->sccenable = 0;
		}
		else
		{
			THIS_->sndp[SND_SCC] = SCCSoundAlloc(pNezPlay);
			if (!THIS_->sndp[SND_SCC]) return NEZ_NESERR_SHORTOFMEMORY;
			THIS_->rammode = (THIS_->extdevice & EXTDEVICE_MSXRAM) != 0;
			THIS_->sccenable = !THIS_->rammode;
		}
	}
	return NEZ_NESERR_NOERROR;
}

static int32_t KSSSEQExecuteZ80CPU(NEZ_PLAY *pNezPlay)
{
	kss_execute(((NEZ_PLAY*)pNezPlay)->kssseq);
	return 0;
}

static void KSSSEQSoundRenderStereo(NEZ_PLAY *pNezPlay, int32_t *d)
{
	kss_synth(((NEZ_PLAY*)pNezPlay)->kssseq, d);
}

static int32_t KSSSEQSoundRenderMono(NEZ_PLAY *pNezPlay)
{
	int32_t d[2] = { 0, 0 };
	kss_synth(((NEZ_PLAY*)pNezPlay)->kssseq, d);
#if (((-1) >> 1) == -1)
	return (d[0] + d[1]) >> 1;
#else
	return (d[0] + d[1]) / 2;
#endif
}

static const NEZ_NES_AUDIO_HANDLER kssseq_audio_handler[] = {
	{ 0, KSSSEQExecuteZ80CPU, 0, NULL },
	{ 3, KSSSEQSoundRenderMono, KSSSEQSoundRenderStereo, NULL },
	{ 0, 0, 0, NULL },
};

static void KSSSEQVolume(NEZ_PLAY *pNezPlay, uint32_t v)
{
	if (((NEZ_PLAY*)pNezPlay)->kssseq)
	{
		kss_volume(((NEZ_PLAY*)pNezPlay)->kssseq, v);
	}
}

static const NEZ_NES_VOLUME_HANDLER kssseq_volume_handler[] = {
	{ KSSSEQVolume, NULL },
	{ 0, NULL },
};

static void KSSSEQReset(NEZ_PLAY *pNezPlay)
{
	if (((NEZ_PLAY*)pNezPlay)->kssseq) kss_reset((NEZ_PLAY*)pNezPlay);
}

static const NEZ_NES_RESET_HANDLER kssseq_reset_handler[] = {
	{ NES_RESET_SYS_LAST, KSSSEQReset, NULL },
	{ 0,                  0, NULL },
};

static void KSSSEQTerminate(NEZ_PLAY *pNezPlay)
{
	if (((NEZ_PLAY*)pNezPlay)->kssseq)
	{
		kss_terminate(((NEZ_PLAY*)pNezPlay)->kssseq);
		((NEZ_PLAY*)pNezPlay)->kssseq = 0;
	}
}

static const NEZ_NES_TERMINATE_HANDLER kssseq_terminate_handler[] = {
	{ KSSSEQTerminate, NULL },
	{ 0, NULL },
};

PROTECTED uint32_t KSSLoad(NEZ_PLAY *pNezPlay, const uint8_t *pData, uint32_t uSize)
{
	uint32_t ret;
	KSSSEQ *THIS_;
	if (pNezPlay->kssseq) return NEZ_NESERR_FORMAT; //__builtin_trap();	/* ASSERT */
	THIS_ = XMALLOC(sizeof(KSSSEQ));
	if (!THIS_) return NEZ_NESERR_SHORTOFMEMORY;
	ret = kss_load(pNezPlay, THIS_, pData, uSize);
	if (ret)
	{
		kss_terminate(THIS_);
		return ret;
	}
	pNezPlay->kssseq = THIS_;
    THIS_->msx_psg_type = &(pNezPlay->kss_config.msx_psg_type);
    THIS_->msx_psg_volume = &(pNezPlay->kss_config.msx_psg_volume);
	NESAudioHandlerInstall(pNezPlay, kssseq_audio_handler);
	NESVolumeHandlerInstall(pNezPlay, kssseq_volume_handler);
	NESResetHandlerInstall(pNezPlay->nrh, kssseq_reset_handler);
	NESTerminateHandlerInstall(&pNezPlay->nth, kssseq_terminate_handler);
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
#undef EXTDEVICE_PAL
#undef EXTDEVICE_EXRAM

#undef SND_PSG
#undef SND_SCC
#undef SND_MSXMUSIC
#undef SND_MSXAUDIO
#undef SND_MAX

#undef SND_SNG
#undef SND_FMUNIT

#undef SND_VOLUME
