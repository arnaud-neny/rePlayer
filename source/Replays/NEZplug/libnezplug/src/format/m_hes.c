#include "../include/nezplug/nezplug.h"
#include "../normalize.h"
#include "handler.h"
#include "audiosys.h"
#include "songinfo.h"

#include "../device/s_hes.h"
#include "../device/s_hesad.h"
#include "../common/divfix.h"
#include "../common/util.h"

#include "m_hes.h"
#include <stdio.h>

/* -------------------- */
/*  km6502 HuC6280 I/F  */
/* -------------------- */
#undef USE_DIRECT_ZEROPAGE
#undef USE_CALLBACK
#undef USE_INLINEMMC
#undef USE_USERPOINTER

#define USE_DIRECT_ZEROPAGE 0
#define USE_CALLBACK 1
#define USE_INLINEMMC 0
#define USE_USERPOINTER 1

#ifndef External
#define External extern
#endif

#include "../cpu/kmz80/kmevent.h"
#include "../cpu/km6502/km6280w.h"
#include "../cpu/km6502/km6280m.h"

#define SHIFT_CPS 15
#define HES_BASECYCLES (21477270)
#define HES_TIMERCYCLES (1024 * 3)

#if 0

HES
system clock 21477270Hz
CPUH clock 21477270Hz system clock 
CPUL clock 3579545 system clock / 6

FF		I/O
F9-FB	SGX-RAM
F8		RAM
F7		BATTERY RAM
80-87	CD-ROM^2 RAM
00-		ROM

#endif

typedef struct HESHES_TAG HESHES;
typedef uint32_t (*HES_READPROC)(HESHES *THIS_, uint32_t a);
typedef void (*HES_WRITEPROC)(HESHES *THIS_, uint32_t a, uint32_t v);

struct  HESHES_TAG {
	struct K6280_Context ctx;
	KMIF_SOUND_DEVICE *hessnd;
	KMIF_SOUND_DEVICE *hespcm;
	KMEVENT kme;
	KMEVENT_ITEM_ID vsync;
	KMEVENT_ITEM_ID timer;

	uint32_t bp;			/* break point */
	uint32_t breaked;		/* break point flag */

	uint32_t cps;				/* cycles per sample:fixed point */
	uint32_t cpsrem;			/* cycle remain */
	uint32_t cpsgap;			/* cycle gap */
	uint32_t total_cycles;	/* total played cycles */

	uint8_t mpr[0x8];
	uint8_t firstmpr[0x8];
	uint8_t *memmap[0x100];
	uint32_t initaddr;

	uint32_t playerromaddr;
	uint8_t playerrom[0x10];

	uint8_t hestim_RELOAD;	/* IO $C01 ($C00)*/
	uint8_t hestim_COUNTER;	/* IO $C00 */
	uint8_t hestim_START;		/* IO $C01 */
	uint8_t hesvdc_STATUS;
	uint8_t hesvdc_CR;
	uint8_t hesvdc_ADR;
};


static uint32_t hes_km6280_exec(struct K6280_Context *ctx, uint32_t cycles)
{
	HESHES *THIS_ = (HESHES *)ctx->user;
	uint32_t kmecycle;
	kmecycle = ctx->clock = 0;
	while (ctx->clock < cycles)
	{
#if 1
		if (!THIS_->breaked)
#else
		if (1)
#endif
		{
			K6280_Exec(ctx);	/* Execute 1op */
			if (ctx->PC == THIS_->bp)
			{
				if(((THIS_->ctx.iRequest)&(THIS_->ctx.iMask^0x3)&(K6280_INT1|K6280_TIMER))==0)
					THIS_->breaked = 1;
			}
		}
		else
		{
			uint32_t nextcount;
			/* break時は次のイベントまで一度に進める */
			nextcount = THIS_->kme.item[THIS_->kme.item[0].next].count;
			if (kmevent_gettimer(&THIS_->kme, 0, &nextcount))
			{
				/* イベント有り */
				if (ctx->clock + nextcount < cycles)
					ctx->clock += nextcount;	/* 期間中にイベント有り */
				else
					ctx->clock = cycles;		/* 期間中にイベント無し */
			}
			else
			{
				/* イベント無し */
				ctx->clock = cycles;
			}
		}
		/* イベント進行 */
		kmevent_process(&THIS_->kme, ctx->clock - kmecycle);
		kmecycle = ctx->clock;
	}
	ctx->clock = 0;
	return kmecycle;
}

static int32_t hes_execute(HESHES *THIS_)
{
	uint32_t cycles;
	THIS_->cpsrem += THIS_->cps;
	cycles = THIS_->cpsrem >> SHIFT_CPS;
	if (THIS_->cpsgap >= cycles)
		THIS_->cpsgap -= cycles;
	else
	{
		uint32_t excycles = cycles - THIS_->cpsgap;
		THIS_->cpsgap = hes_km6280_exec(&THIS_->ctx, excycles) - excycles;
	}
	THIS_->cpsrem &= (1 << SHIFT_CPS) - 1;
	THIS_->total_cycles += cycles;
	return 0;
}

__inline static void hes_synth(HESHES *THIS_, int32_t *d)
{
	THIS_->hessnd->synth(THIS_->hessnd->ctx, d);
	THIS_->hespcm->synth(THIS_->hespcm->ctx, d);
}

__inline static void hes_volume(HESHES *THIS_, uint32_t v)
{
	THIS_->hessnd->volume(THIS_->hessnd->ctx, v);
	THIS_->hespcm->volume(THIS_->hespcm->ctx, v);
}


static void hes_vsync_setup(HESHES *THIS_)
{
	kmevent_settimer(&THIS_->kme, THIS_->vsync, 4 * 342 * 262);
}

static void hes_timer_setup(HESHES *THIS_)
{
	kmevent_settimer(&THIS_->kme, THIS_->timer, HES_TIMERCYCLES);
}

static void hes_write_6270(HESHES *THIS_, uint32_t a, uint32_t v)
{
	switch (a)
	{
		case 0:
			THIS_->hesvdc_ADR = (uint8_t)v;
			break;
		case 2:
			switch (THIS_->hesvdc_ADR)
			{
				case 5:	/* CR */
					THIS_->hesvdc_CR = (uint8_t)v;
					break;
			}
			break;
		case 3:
			break;
	}
}

static uint32_t hes_read_6270(HESHES *THIS_, uint32_t a)
{
	uint32_t v = 0;
	if (a == 0)
	{
		if (THIS_->hesvdc_STATUS)
		{
			THIS_->hesvdc_STATUS = 0;
			v = 0x20;
		}
		THIS_->ctx.iRequest &= ~K6280_INT1;
#if 0
		v = 0x20;	/* 常にVSYNC期間 */
#endif
	}
	return v;
}

static uint32_t hes_read_io(HESHES *THIS_, uint32_t a)
{
	switch (a >> 10)
	{
		case 0:	/* VDC */	
			return hes_read_6270(THIS_, a & 3);
		case 2:	/* PSG */
			return THIS_->hessnd->read(THIS_->hessnd->ctx, a & 0xf);
		case 3:	/* TIMER */
			if (a & 1)
				return THIS_->hestim_START;
			else
				return THIS_->hestim_COUNTER;
		case 5:	/* IRQ */
			switch (a & 15)
			{
				case 2:
					{
						uint32_t v = 0xf8;
						if (!(THIS_->ctx.iMask & K6280_TIMER)) v |= 4;
						if (!(THIS_->ctx.iMask & K6280_INT1)) v |= 2;
						if (!(THIS_->ctx.iMask & K6280_INT2)) v |= 1;
						return v;
					}
				case 3:
					{
						uint8_t v = 0;
						if (THIS_->ctx.iRequest & K6280_TIMER) v |= 4;
						if (THIS_->ctx.iRequest & K6280_INT1) v |= 2;
						if (THIS_->ctx.iRequest & K6280_INT2) v |= 1;
#if 0
						THIS_->ctx.iRequest &= ~(K6280_TIMER | K6280_INT1 | K6280_INT2);
#endif
						return v;
					}
			}
			return 0x00;
		case 7:
			a -= THIS_->playerromaddr;
			if (a < 0x10) return THIS_->playerrom[a];
			return 0xff;
		case 6:	/* CDROM */
			switch (a & 15)
			{
				case 0x0a:
				case 0x0b:
				case 0x0c:
				case 0x0d:
				case 0x0e://デバッグ用
				case 0x0f://デバッグ用
					return THIS_->hespcm->read(THIS_->hespcm->ctx, a & 0xf);
			} /* fall-through */
		default:
		case 1:	/* VCE */ /* fall-through */
		case 4:	/* PAD */ /* fall-through */
			return 0xff;
	}
}

static void hes_write_io(HESHES *THIS_, uint32_t a, uint32_t v)
{
	switch (a >> 10)
	{
		case 0:	/* VDC */
			hes_write_6270(THIS_, a & 3, v);
			break;
		case 2:	/* PSG */
			THIS_->hessnd->write(THIS_->hessnd->ctx, a & 0xf, v);
			break;
		case 3:	/* TIMER */
			switch (a & 1)
			{
				case 0:
					THIS_->hestim_RELOAD = (uint8_t)(v & 127);
					break;
				case 1:
					v &= 1;
					if (v && !THIS_->hestim_START)
						THIS_->hestim_COUNTER = THIS_->hestim_RELOAD;
					THIS_->hestim_START = (uint8_t)v;
					break;
			}
			break;
		case 5:	/* IRQ */
			switch (a & 15)
			{
				case 2:
					THIS_->ctx.iMask &= ~(K6280_TIMER | K6280_INT1 | K6280_INT2);
					if (!(v & 4)) THIS_->ctx.iMask |= K6280_TIMER;
					if (!(v & 2)) THIS_->ctx.iMask |= K6280_INT1;
					if (!(v & 1)) THIS_->ctx.iMask |= K6280_INT2;
					break;
				case 3:
					THIS_->ctx.iRequest &= ~K6280_TIMER;
					break;
			}
			break;
		case 6:	/* CDROM */
			switch (a & 15)
			{
				case 0x08:
				case 0x09:
				case 0x0a:
				case 0x0b:
				case 0x0d:
				case 0x0e:
				case 0x0f:
					THIS_->hespcm->write(THIS_->hespcm->ctx, a & 0xf, v);
					break;
			}
			break;
		default:
		case 1:	/* VCE */
		case 4:	/* PAD */
		case 7:
			break;
			
	}
}


static uint32_t Callback hes_read_event(void *ctx, Uword a)
{
	HESHES *THIS_ = ctx;
	uint8_t page = THIS_->mpr[a >> 13];
	if (THIS_->memmap[page])
		return THIS_->memmap[page][a & 0x1fff];
	else if (page == 0xff)
		return hes_read_io(THIS_, a & 0x1fff);
	else
		return 0xff;
}

static void Callback hes_write_event(void *ctx, Uword a, Uword v)
{
	HESHES *THIS_ = ctx;
	uint8_t page = THIS_->mpr[a >> 13];
	if (THIS_->memmap[page])
		THIS_->memmap[page][a & 0x1fff] = (uint8_t)v;
	else if (page == 0xff)
		hes_write_io(THIS_, a & 0x1fff, v);
}

static uint32_t Callback hes_readmpr_event(void *ctx, Uword a)
{
	HESHES *THIS_ = ctx;
	uint32_t i;
	for (i = 0; i < 8; i++) if (a & (1 << i)) return THIS_->mpr[i];
	return 0xff;
}

static void Callback hes_writempr_event(void *ctx, Uword a, Uword v)
{
	HESHES *THIS_ = ctx;
	uint32_t i;
	if (v < 0x80 && !THIS_->memmap[v]) return;
	for (i = 0; i < 8; i++) if (a & (1 << i)) THIS_->mpr[i] = (uint8_t)v;
}

static void Callback hes_write6270_event(void *ctx, Uword a, Uword v)
{
	HESHES *THIS_ = ctx;
	hes_write_6270(THIS_, a & 0x1fff, v);
}


static void hes_vsync_event(KMEVENT *event, KMEVENT_ITEM_ID curid, HESHES *THIS_)
{
    (void)event;
    (void)curid;
	hes_vsync_setup(THIS_);
	if (THIS_->hesvdc_CR & 8)
	{
		THIS_->ctx.iRequest |= K6280_INT1;
		THIS_->breaked = 0;
	}
	THIS_->hesvdc_STATUS = 1;
}

static void hes_timer_event(KMEVENT *event, KMEVENT_ITEM_ID curid, HESHES *THIS_)
{
    (void)event;
    (void)curid;
	if (THIS_->hestim_START && THIS_->hestim_COUNTER-- == 0)
	{
		THIS_->hestim_COUNTER = THIS_->hestim_RELOAD;
		THIS_->ctx.iRequest |= K6280_TIMER;
		THIS_->breaked = 0;
	}
	hes_timer_setup(THIS_);
}

static void hes_reset(NEZ_PLAY *pNezPlay)
{
	HESHES *THIS_ = pNezPlay->heshes;
	uint32_t i, initbreak;
	uint32_t freq = NESAudioFrequencyGet(pNezPlay);

	THIS_->hessnd->reset(THIS_->hessnd->ctx, HES_BASECYCLES, freq);
	THIS_->hespcm->reset(THIS_->hespcm->ctx, HES_BASECYCLES, freq);
	kmevent_init(&THIS_->kme);

	/* RAM CLEAR */
	for (i = 0xf8; i <= 0xfb; i++)
		if (THIS_->memmap[i]) XMEMSET(THIS_->memmap[i], 0, 0x2000);

	THIS_->cps = DivFix(HES_BASECYCLES, freq, SHIFT_CPS);
	THIS_->ctx.user = THIS_;
	THIS_->ctx.ReadByte = hes_read_event;
	THIS_->ctx.WriteByte = hes_write_event;
	THIS_->ctx.ReadMPR = hes_readmpr_event;
	THIS_->ctx.WriteMPR = hes_writempr_event;
	THIS_->ctx.Write6270 = hes_write6270_event;

	THIS_->vsync = kmevent_alloc(&THIS_->kme);
	THIS_->timer = kmevent_alloc(&THIS_->kme);
	kmevent_setevent(&THIS_->kme, THIS_->vsync, hes_vsync_event, THIS_);
	kmevent_setevent(&THIS_->kme, THIS_->timer, hes_timer_event, THIS_);

	THIS_->bp = THIS_->playerromaddr + 3;
	for (i = 0; i < 8; i++) THIS_->mpr[i] = THIS_->firstmpr[i];

	THIS_->breaked = 0;
	THIS_->cpsrem = THIS_->cpsgap = THIS_->total_cycles = 0;

	THIS_->ctx.A = (SONGINFO_GetSongNo(pNezPlay->song) - 1) & 0xff;
	THIS_->ctx.P = K6280_Z_FLAG + K6280_I_FLAG;
	THIS_->ctx.X = THIS_->ctx.Y = 0;
	THIS_->ctx.S = 0xFF;
	THIS_->ctx.PC = THIS_->playerromaddr;
	THIS_->ctx.iRequest = 0;
	THIS_->ctx.iMask = ~0;
	THIS_->ctx.lowClockMode = 0;

	THIS_->playerrom[0x00] = 0x20;	/* JSR */
	THIS_->playerrom[0x01] = (uint8_t)((THIS_->initaddr >> 0) & 0xff);
	THIS_->playerrom[0x02] = (uint8_t)((THIS_->initaddr >> 8) & 0xff);
	THIS_->playerrom[0x03] = 0x4c;	/* JMP */
	THIS_->playerrom[0x04] = (uint8_t)(((THIS_->playerromaddr + 3) >> 0) & 0xff);
	THIS_->playerrom[0x05] = (uint8_t)(((THIS_->playerromaddr + 3) >> 8) & 0xff);

	THIS_->hesvdc_STATUS = 0;
	THIS_->hesvdc_CR = 0;
	THIS_->hesvdc_ADR = 0;
	hes_vsync_setup(THIS_);
	THIS_->hestim_RELOAD = THIS_->hestim_COUNTER = THIS_->hestim_START = 0;
	hes_timer_setup(THIS_);

	/* request execute(5sec) */
	initbreak = 5 << 8;
	while (!THIS_->breaked && --initbreak)
		hes_km6280_exec(&THIS_->ctx, HES_BASECYCLES >> 8);

	if (THIS_->breaked)
	{
		THIS_->breaked = 0;
		THIS_->ctx.P &= ~K6280_I_FLAG;
	}

	THIS_->cpsrem = THIS_->cpsgap = THIS_->total_cycles = 0;

}

static void hes_terminate(HESHES *THIS_)
{
	uint32_t i;

	if (THIS_->hessnd) THIS_->hessnd->release(THIS_->hessnd->ctx);
	if (THIS_->hespcm) THIS_->hespcm->release(THIS_->hespcm->ctx);
	for (i = 0; i < 0x100; i++) if (THIS_->memmap[i]) XFREE(THIS_->memmap[i]);
	XFREE(THIS_);
}

static uint32_t hes_alloc_physical_address(HESHES *THIS_, uint32_t a, uint32_t l)
{
	uint8_t page = (uint8_t)(a >> 13);
	uint8_t lastpage = (uint8_t)((a + l - 1) >> 13);
	for (; page <= lastpage; page++)
	{
		if (!THIS_->memmap[page])
		{
			THIS_->memmap[page] = (uint8_t*)XMALLOC(0x2000);
			if (!THIS_->memmap[page]) return 0;
			XMEMSET(THIS_->memmap[page], 0, 0x2000);
		}
	}
	return 1;
}

static void hes_copy_physical_address(HESHES *THIS_, uint32_t a, uint32_t l, const uint8_t *p)
{
	uint8_t page = (uint8_t)(a >> 13);
	uint32_t w;
	if (a & 0x1fff)
	{
		w = 0x2000 - (a & 0x1fff);
		if (w > l) w = l;
		XMEMCPY(THIS_->memmap[page++] + (a & 0x1fff), p, w);
		p += w;
		l -= w;
	}
	while (l)
	{
		w = (l > 0x2000) ? 0x2000 : l;
		XMEMCPY(THIS_->memmap[page++], p, w);
		p += w;
		l -= w;
	}
}


static uint32_t hes_load(NEZ_PLAY *pNezPlay, HESHES *THIS_, const uint8_t *pData, uint32_t uSize)
{
	uint32_t i, p;
	XMEMSET(THIS_, 0, sizeof(HESHES));
	THIS_->hessnd = 0;
	THIS_->hespcm = 0;
	for (i = 0; i < 0x100; i++) THIS_->memmap[i] = 0;

	if (uSize < 0x20) return NEZ_NESERR_FORMAT;
	SONGINFO_SetStartSongNo(pNezPlay->song, pData[5] + 1);
	SONGINFO_SetMaxSongNo(pNezPlay->song, 256);
	SONGINFO_SetChannel(pNezPlay->song, 2);
	SONGINFO_SetExtendDevice(pNezPlay->song, 0);
	for (i = 0; i < 8; i++) THIS_->firstmpr[i] = pData[8 + i];
	THIS_->playerromaddr = 0x1ff0;
	THIS_->initaddr = GetWordLE(pData + 0x06);
	SONGINFO_SetInitAddress(pNezPlay->song, THIS_->initaddr);
	SONGINFO_SetPlayAddress(pNezPlay->song, 0);

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
"Type           : HES\r\n\
Start Song     : %02XH\r\n\
Init Address   : %04XH\r\n\
First Mapper 0 : %02XH\r\n\
First Mapper 1 : %02XH\r\n\
First Mapper 2 : %02XH\r\n\
First Mapper 3 : %02XH\r\n\
First Mapper 4 : %02XH\r\n\
First Mapper 5 : %02XH\r\n\
First Mapper 6 : %02XH\r\n\
First Mapper 7 : %02XH"
		,pData[5],THIS_->initaddr
		,pData[0x8]
		,pData[0x9]
		,pData[0xa]
		,pData[0xb]
		,pData[0xc]
		,pData[0xd]
		,pData[0xe]
		,pData[0xf]
		);

	if (!hes_alloc_physical_address(THIS_, 0xf8 << 13, 0x2000))	/* RAM */
		return NEZ_NESERR_SHORTOFMEMORY;
	if (!hes_alloc_physical_address(THIS_, 0xf9 << 13, 0x2000))	/* SGX-RAM */
		return NEZ_NESERR_SHORTOFMEMORY;
	if (!hes_alloc_physical_address(THIS_, 0xfa << 13, 0x2000))	/* SGX-RAM */
		return NEZ_NESERR_SHORTOFMEMORY;
	if (!hes_alloc_physical_address(THIS_, 0xfb << 13, 0x2000))	/* SGX-RAM */
		return NEZ_NESERR_SHORTOFMEMORY;
	if (!hes_alloc_physical_address(THIS_, 0x00 << 13, 0x2000))	/* IPL-ROM */
		return NEZ_NESERR_SHORTOFMEMORY;
	for (p = 0x10; p + 0x10 < uSize; p += 0x10 + GetDwordLE(pData + p + 4))
	{
		if (GetDwordLE(pData + p) == 0x41544144)	/* 'DATA' */
		{
			uint32_t a, l;
			l = GetDwordLE(pData + p + 4);
			a = GetDwordLE(pData + p + 8);
			if (!hes_alloc_physical_address(THIS_, a, l)) return NEZ_NESERR_SHORTOFMEMORY;
			if (l > uSize - p - 0x10) l = uSize - p - 0x10;
			hes_copy_physical_address(THIS_, a, l, pData + p + 0x10);
		}
	}
	THIS_->hessnd = HESSoundAlloc(pNezPlay);
	if (!THIS_->hessnd) return NEZ_NESERR_SHORTOFMEMORY;
	THIS_->hespcm = HESAdPcmAlloc(pNezPlay);
	if (!THIS_->hespcm) return NEZ_NESERR_SHORTOFMEMORY;

	return NEZ_NESERR_NOERROR;

}


static int32_t ExecuteHES(NEZ_PLAY *pNezPlay)
{
	return ((NEZ_PLAY*)pNezPlay)->heshes ? hes_execute((HESHES*)((NEZ_PLAY*)pNezPlay)->heshes) : 0;
}

static void HESSoundRenderStereo(NEZ_PLAY *pNezPlay, int32_t *d)
{
	hes_synth((HESHES*)((NEZ_PLAY*)pNezPlay)->heshes, d);
}

static int32_t HESSoundRenderMono(NEZ_PLAY *pNezPlay)
{
	int32_t d[2] = { 0,0 } ;
	hes_synth((HESHES*)((NEZ_PLAY*)pNezPlay)->heshes, d);
#if (((-1) >> 1) == -1)
	return (d[0] + d[1]) >> 1;
#else
	return (d[0] + d[1]) / 2;
#endif
}

static const NEZ_NES_AUDIO_HANDLER heshes_audio_handler[] = {
	{ 0, ExecuteHES, 0, NULL },
	{ 3, HESSoundRenderMono, HESSoundRenderStereo, NULL },
	{ 0, 0, 0, NULL },
};

static void HESHESVolume(NEZ_PLAY *pNezPlay, uint32_t v)
{
	if (((NEZ_PLAY*)pNezPlay)->heshes)
	{
		hes_volume((HESHES*)((NEZ_PLAY*)pNezPlay)->heshes, v);
	}
}

static const NEZ_NES_VOLUME_HANDLER heshes_volume_handler[] = {
	{ HESHESVolume, NULL }, 
	{ 0, NULL }, 
};

static void HESHESReset(NEZ_PLAY *pNezPlay)
{
	if (((NEZ_PLAY*)pNezPlay)->heshes) hes_reset((NEZ_PLAY*)pNezPlay);
}

static const NEZ_NES_RESET_HANDLER heshes_reset_handler[] = {
	{ NES_RESET_SYS_LAST, HESHESReset, NULL },
	{ 0,                  0, NULL },
};

static void HESHESTerminate(NEZ_PLAY *pNezPlay)
{
	if (((NEZ_PLAY*)pNezPlay)->heshes)
	{
		hes_terminate((HESHES*)((NEZ_PLAY*)pNezPlay)->heshes);
		((NEZ_PLAY*)pNezPlay)->heshes = 0;
	}
}

static const NEZ_NES_TERMINATE_HANDLER heshes_hes_terminate_handler[] = {
	{ HESHESTerminate, NULL },
	{ 0, NULL },
};

PROTECTED uint32_t HESLoad(NEZ_PLAY *pNezPlay, const uint8_t *pData, uint32_t uSize)
{
	uint32_t ret;
	HESHES *THIS_;
	if (pNezPlay->heshes) return NEZ_NESERR_FORMAT; //__builtin_trap();	/* ASSERT */
	THIS_ = (HESHES *)XMALLOC(sizeof(HESHES));
	if (!THIS_) return NEZ_NESERR_SHORTOFMEMORY;
	ret = hes_load(pNezPlay, THIS_, pData, uSize);
	if (ret)
	{
		hes_terminate(THIS_);
		return ret;
	}
	pNezPlay->heshes = THIS_;
	NESAudioHandlerInstall(pNezPlay, heshes_audio_handler);
	NESVolumeHandlerInstall(pNezPlay, heshes_volume_handler);
	NESResetHandlerInstall(pNezPlay->nrh, heshes_reset_handler);
	NESTerminateHandlerInstall(&pNezPlay->nth, heshes_hes_terminate_handler);
	return ret;
}

#undef USE_DIRECT_ZEROPAGE
#undef USE_CALLBACK
#undef USE_INLINEMMC
#undef USE_USERPOINTER
#undef External
#undef SHIFT_CPS
#undef HES_BASECYCLES
#undef HES_TIMERCYCLES

#include "../cpu/km6502/km6280u.h"
