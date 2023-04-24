#include "../include/nezplug/nezplug.h"
#include "handler.h"
#include "audiosys.h"
#include "songinfo.h"

#include "m_zxay.h"

#include "../device/s_psg.h"
#include "../common/divfix.h"
#include "../common/util.h"

#include "../cpu/kmz80/kmz80.h"

#define IRQ_PATCH 2

#undef SHIFT_CPS
#define SHIFT_CPS 15
#define ZX_BASECYCLES       (3579545)
#define AMSTRAD_BASECYCLES  (2000000)

typedef struct {
	KMZ80_CONTEXT ctx;
	KMIF_SOUND_DEVICE *sndp;
	KMIF_SOUND_DEVICE *amstrad_sndp;
	KMEVENT kme;
	KMEVENT_ITEM_ID vsync_id;

	uint32_t cps;		/* cycles per sample:fixed point */
	uint32_t cpsrem;	/* cycle remain */
	uint32_t cpsgap;	/* cycle gap */
	uint32_t total_cycles;	/* total played cycles */

	uint32_t startsong;
	uint32_t maxsong;

	uint32_t initaddr;
	uint32_t playaddr;
	uint32_t spinit;

	uint8_t ram[0x10004];
	uint8_t *data;
	uint8_t *datalimit;

	uint8_t amstrad_ppi_a;
	uint8_t amstrad_ppi_c;
#ifdef AMSTRAD_PPI_CONNECT_TYPE_B
	uint8_t amstrad_ppi_va;
	uint8_t amstrad_ppi_vd;
#endif
} ZXAY;

static int32_t execute(ZXAY *THIS_)
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

__inline static void synth(ZXAY *THIS_, int32_t *d)
{
	THIS_->sndp->synth(THIS_->sndp->ctx, d);
	THIS_->amstrad_sndp->synth(THIS_->amstrad_sndp->ctx, d);
}

__inline static void volume(ZXAY *THIS_, uint32_t v)
{
	THIS_->sndp->volume(THIS_->sndp->ctx, v);
	THIS_->amstrad_sndp->volume(THIS_->amstrad_sndp->ctx, v);
}


static void vsync_setup(ZXAY *THIS_)
{
	kmevent_settimer(&THIS_->kme, THIS_->vsync_id, 313 * 4 * 342 / 6);
}

static uint32_t read_event(void *ctx, uint32_t a)
{
	ZXAY *THIS_ = ctx;
	return THIS_->ram[a];
}

static uint32_t busread_event(void *ctx, uint32_t a)
{
    (void)ctx;
    (void)a;
	return 0x38;
}

static void write_event(void *ctx, uint32_t a, uint32_t v)
{
	ZXAY *THIS_ = ctx;
	THIS_->ram[a] = (uint8_t)v;
}

static uint32_t ioread_event(void *ctx, uint32_t a)
{
    (void)ctx;
    (void)a;
	return 0xff;
}

static void iowrite_amstrad_ppi_update(ZXAY *THIS_)
{
	switch ((THIS_->amstrad_ppi_c >> 6) & 3)
	{
#ifdef AMSTRAD_PPI_CONNECT_TYPE_B
		case 0:
			THIS_->amstrad_sndp->write(THIS_->amstrad_sndp->ctx, 0, THIS_->amstrad_ppi_va);
			THIS_->amstrad_sndp->write(THIS_->amstrad_sndp->ctx, 1, THIS_->amstrad_ppi_vd);
			break;
		case 2:
			THIS_->amstrad_ppi_vd = THIS_->amstrad_ppi_a;
			break;
		case 3:
			THIS_->amstrad_ppi_va = THIS_->amstrad_ppi_a;
			break;
#else
		case 2:
			THIS_->amstrad_sndp->write(THIS_->amstrad_sndp->ctx, 1, THIS_->amstrad_ppi_a);
			break;
		case 3:
			THIS_->amstrad_sndp->write(THIS_->amstrad_sndp->ctx, 0, THIS_->amstrad_ppi_a);
			break;
#endif
	}
}
static void iowrite_event(void *ctx, uint32_t a, uint32_t v)
{
	ZXAY *THIS_ = ctx;
	if (a == 0xFFFD)					/* ZXspectrum */
		THIS_->sndp->write(THIS_->sndp->ctx, 0, v);
	else if (a == 0xBFFD)				/* ZXspectrum */
		THIS_->sndp->write(THIS_->sndp->ctx, 1, v);
	else if ((a & 0xFF) == 0x00FE)				/* 1 bit D/A */
	{
		THIS_->sndp->write(THIS_->sndp->ctx, 2, (v & 0x10)?1:0);
	}
	else if ((a & 0x0B00) == 0x0000)	/* Amstrad CPC 8255 port A */
	{
		THIS_->amstrad_ppi_a = (uint8_t)v;
		iowrite_amstrad_ppi_update(THIS_);
	}
	else if ((a & 0x0B00) == 0x0200)	/* Amstrad CPC 8255 port C */
	{
		THIS_->amstrad_ppi_c = (uint8_t)v;
		iowrite_amstrad_ppi_update(THIS_);
	}
}

static void vsync_event(KMEVENT *event, KMEVENT_ITEM_ID curid, ZXAY *THIS_)
{
    (void)event;
    (void)curid;
	vsync_setup(THIS_);
#if IRQ_PATCH == 1
	if (THIS_->ctx.pc == 0x0019)
	{
		THIS_->ctx.regs8[REGID_HALTED] = 0;
		THIS_->ctx.pc = 0x0016;
	}
#elif IRQ_PATCH == 2
	if (THIS_->ctx.pc == 0x0018 || THIS_->ctx.pc == 0x0019)
	{
		THIS_->ctx.regs8[REGID_HALTED] = 0;
		THIS_->ctx.pc = 0x0015;
	}
#endif
	THIS_->ctx.regs8[REGID_INTREQ] |= 1;
}

static uint8_t *ZXAYOffset(uint8_t *p)
{
	uint32_t ofs = GetWordBE(p) ^ 0x8000;
	return p + ofs - 0x8000;
}

static void reset(NEZ_PLAY *pNezPlay)
{
	ZXAY *THIS_ = pNezPlay->zxay;
	uint32_t i, freq, song, reginit;
	uint8_t *p, *p2;

	freq = NESAudioFrequencyGet(pNezPlay);
	song = SONGINFO_GetSongNo(pNezPlay->song) - 1;
	if (song >= THIS_->maxsong) song = THIS_->startsong - 1;

	/* sound reset */
	THIS_->sndp->reset(THIS_->sndp->ctx, ZX_BASECYCLES, freq);
	THIS_->amstrad_sndp->reset(THIS_->amstrad_sndp->ctx, AMSTRAD_BASECYCLES, freq);
	for (i = 0x7; i <= 0xa; i++)
	{
		THIS_->sndp->write(THIS_->sndp->ctx, 0, i);
		THIS_->sndp->write(THIS_->sndp->ctx, 1, (i == 0x07) ? 0x38 : 0x08);
		THIS_->amstrad_sndp->write(THIS_->amstrad_sndp->ctx, 0, i);
		THIS_->amstrad_sndp->write(THIS_->amstrad_sndp->ctx, 1, (i == 0x07) ? 0x38 : 0x08);
	}
	THIS_->amstrad_ppi_a = 0;
	THIS_->amstrad_ppi_c = 0;
#ifdef AMSTRAD_PPI_CONNECT_TYPE_B
	THIS_->amstrad_ppi_va = 0;
	THIS_->amstrad_ppi_vd = 0;
#endif

	/* memory reset */
	XMEMSET(THIS_->ram, 0, sizeof(THIS_->ram));
	p = ZXAYOffset(THIS_->data + 18);
	p = ZXAYOffset(p + song * 4 + 2);
	reginit = GetWordBE(p + 8);
	p2 = ZXAYOffset(p + 10);
	THIS_->spinit = GetWordBE(p2);
	THIS_->initaddr = GetWordBE(p2 + 2);
	THIS_->playaddr = GetWordBE(p2 + 4);
	p = ZXAYOffset(p + 12);
	XMEMSET(&THIS_->ram[0x0000], 0xFF, 0x4000);
	XMEMSET(&THIS_->ram[0x4000], 0x00, 0xC000);
#if IRQ_PATCH == 1
	/* NEZplug */
	XMEMCPY(&THIS_->ram[0x0000],
		"\xf3\x00\x00\x21\x00\x00\x7c\xb5"
		"\x20\x08\xed\x57\x67\x2d\x5e\x23"
		"\x56\xeb\x22\x17\x00\x00\xcd\x00"
		"\x00\x18\xfe\xc9", 0x1C);
#elif IRQ_PATCH == 2
	/* NEZplug with HALT */
	XMEMCPY(&THIS_->ram[0x0000],
		"\xf3\x00\x00\x21\x00\x00\x7c\xb5"
		"\x20\x08\xed\x57\x67\x2d\x5e\x23"
		"\x56\xeb\x22\x16\x00\xcd\x00\x00"
		"\x76\x18\xfd\xc9", 0x1C);
#elif IRQ_PATCH == 3
	/* DeliAY with EI */
	XMEMCPY(&THIS_->ram[0x0000],
		"\xf3\x00\xfb\x21\x00\x00\x7c\xb5"
		"\x20\x08\xed\x57\x67\x2d\x5e\x23"
		"\x56\xeb\x22\x17\x00\x00\xcd\x00"
		"\x00\x18\xfe\xc9", 0x1C);
#else
	/* DeliAY */
	XMEMCPY(&THIS_->ram[0x0000],
		"\xf3\x00\x00\x21\x00\x00\x7c\xb5"
		"\x20\x08\xed\x57\x67\x2d\x5e\x23"
		"\x56\xeb\x22\x17\x00\x76\xcd\x00"
		"\x00\x18\xfa\xc9", 0x1C);
#endif
	THIS_->ram[0x0038] = 0xC9;
	if (!THIS_->initaddr) THIS_->initaddr = GetWordBE(p);	/* DEFAULT */
	SetWordLE(&THIS_->ram[0x0001], THIS_->initaddr);
	SONGINFO_SetInitAddress(pNezPlay->song, THIS_->initaddr);
	SONGINFO_SetPlayAddress(pNezPlay->song, THIS_->playaddr);
	SetWordLE(&THIS_->ram[0x0004], THIS_->playaddr);

	do
	{
		uint32_t load,size;
		load = GetWordBE(p);
		size = GetWordBE(p + 2);
		p2 = ZXAYOffset(p + 4);
		if (load + size > 0x10000) size = 0x10000 - load;
		if (p2 + size > THIS_->datalimit) size = THIS_->datalimit - p2;
		XMEMCPY(&THIS_->ram[load], p2, size);
		p += 6;
	} while (GetWordBE(p));

#if	IRQ_PATCH == 3
	/* return address from init */
	THIS_->ram[(THIS_->spinit - 2) & 0xffff] = 0x02;
	THIS_->ram[(THIS_->spinit - 1) & 0xffff] = 0x00;
#else
	THIS_->ram[(THIS_->spinit - 2) & 0xffff] = 0x03;
	THIS_->ram[(THIS_->spinit - 1) & 0xffff] = 0x00;
#endif

	/* cpu reset */
	kmz80_reset(&THIS_->ctx);
	THIS_->ctx.user = THIS_;
	THIS_->ctx.kmevent = &THIS_->kme;
	THIS_->ctx.memread = read_event;
	THIS_->ctx.memwrite = write_event;
	THIS_->ctx.ioread = ioread_event;
	THIS_->ctx.iowrite = iowrite_event;
	THIS_->ctx.busread = busread_event;

	THIS_->ctx.regs8[REGID_A] = THIS_->ctx.regs8[REGID_B] = THIS_->ctx.regs8[REGID_D] = THIS_->ctx.regs8[REGID_H] = THIS_->ctx.regs8[REGID_IXH] = THIS_->ctx.regs8[REGID_IYH] = (uint8_t)((reginit >> 8) & 0xff);
	THIS_->ctx.regs8[REGID_F] = THIS_->ctx.regs8[REGID_C] = THIS_->ctx.regs8[REGID_E] = THIS_->ctx.regs8[REGID_L] = THIS_->ctx.regs8[REGID_IXL] = THIS_->ctx.regs8[REGID_IYL] = (uint8_t)((reginit >> 0) & 0xff);
	THIS_->ctx.saf = THIS_->ctx.sbc = THIS_->ctx.sde = THIS_->ctx.shl = reginit;
	THIS_->ctx.sp = (THIS_->spinit - 2) & 0xffff;
	THIS_->ctx.pc = THIS_->initaddr;

	THIS_->ctx.exflag = 3;	/* ICE/ACI */
	THIS_->ctx.regs8[REGID_M1CYCLE] = 1;
	THIS_->ctx.regs8[REGID_IFF1] = THIS_->ctx.regs8[REGID_IFF2] = 1;
	THIS_->ctx.regs8[REGID_INTREQ] = 0;
#if	IRQ_PATCH == 3
	THIS_->ctx.regs8[REGID_IMODE] = 4;	/* VECTOR INT MODE */
#else
	THIS_->ctx.regs8[REGID_IMODE] = 5;	/* VECTOR CALL MODE */
#endif
	THIS_->ctx.vector[0] = 0x001B;

	/* vsync reset */
	kmevent_init(&THIS_->kme);
	THIS_->vsync_id = kmevent_alloc(&THIS_->kme);
	kmevent_setevent(&THIS_->kme, THIS_->vsync_id, vsync_event, THIS_);
	THIS_->cpsgap = THIS_->cpsrem  = 0;
	THIS_->cps = DivFix(ZX_BASECYCLES, freq, SHIFT_CPS);

#if 0
	{
		/* request execute */
		uint32_t initbreak = 5 << 8; /* 5sec */
		while (THIS_->ctx.pc != THIS_->initsp && --initbreak) kmz80_exec(&THIS_->ctx, ZX_BASECYCLES >> 8);
	}
	vsync_setup(THIS_);
	THIS_->total_cycles = 0;
#else
	vsync_setup(THIS_);
	THIS_->total_cycles = 0;
#endif
}

static void terminate(ZXAY *THIS_)
{
	if (THIS_->sndp) THIS_->sndp->release(THIS_->sndp->ctx);
	if (THIS_->amstrad_sndp) THIS_->amstrad_sndp->release(THIS_->amstrad_sndp->ctx);
	if (THIS_->data) XFREE(THIS_->data);
	XFREE(THIS_);
}

static uint32_t load(NEZ_PLAY *pNezPlay, ZXAY *THIS_, const uint8_t *pData, uint32_t uSize)
{
	XMEMSET(THIS_, 0, sizeof(ZXAY));
	THIS_->sndp = THIS_->amstrad_sndp = 0;
	THIS_->data = 0;

	THIS_->data = (uint8_t*)XMALLOC(uSize);
	if (!THIS_->data) return NEZ_NESERR_SHORTOFMEMORY;
	XMEMCPY(THIS_->data, pData, uSize);
	THIS_->datalimit = THIS_->data + uSize;
	THIS_->maxsong = pData[0x10] + 1;
	THIS_->startsong = pData[0x11] + 1;

	SONGINFO_SetStartSongNo(pNezPlay->song, THIS_->startsong);
	SONGINFO_SetMaxSongNo(pNezPlay->song, THIS_->maxsong);
	SONGINFO_SetExtendDevice(pNezPlay->song, 0);
	SONGINFO_SetChannel(pNezPlay->song, 1);

	THIS_->sndp = PSGSoundAlloc(pNezPlay,PSG_TYPE_AY_3_8910);
	THIS_->amstrad_sndp = PSGSoundAlloc(pNezPlay,PSG_TYPE_YM2149);
	if (!THIS_->sndp || !THIS_->amstrad_sndp) return NEZ_NESERR_SHORTOFMEMORY;
	return NEZ_NESERR_NOERROR;
}



static int32_t ZXAYExecuteZ80CPU(NEZ_PLAY *pNezPlay)
{
	return ((NEZ_PLAY*)pNezPlay)->zxay ? execute((ZXAY*)((NEZ_PLAY*)pNezPlay)->zxay) : 0;
}

static void ZXAYSoundRenderStereo(NEZ_PLAY *pNezPlay, int32_t *d)
{
	synth((ZXAY*)((NEZ_PLAY*)pNezPlay)->zxay, d);
}

static int32_t ZXAYSoundRenderMono(NEZ_PLAY *pNezPlay)
{
	int32_t d[2] = { 0, 0 };
	synth((ZXAY*)((NEZ_PLAY*)pNezPlay)->zxay, d);
#if (((-1) >> 1) == -1)
	return (d[0] + d[1]) >> 1;
#else
	return (d[0] + d[1]) / 2;
#endif
}

static const NEZ_NES_AUDIO_HANDLER zxay_audio_handler[] = {
	{ 0, ZXAYExecuteZ80CPU, 0, NULL },
	{ 3, ZXAYSoundRenderMono, ZXAYSoundRenderStereo, NULL },
	{ 0, 0, 0, NULL },
};

static void ZXAYVolume(NEZ_PLAY *pNezPlay, uint32_t v)
{
	if (((NEZ_PLAY*)pNezPlay)->zxay)
	{
		volume((ZXAY*)((NEZ_PLAY*)pNezPlay)->zxay, v);
	}
}

static const NEZ_NES_VOLUME_HANDLER zxay_volume_handler[] = {
	{ ZXAYVolume, NULL },
	{ 0, NULL },
};

static void ZXAYReset(NEZ_PLAY *pNezPlay)
{
	if (((NEZ_PLAY*)pNezPlay)->zxay) reset((NEZ_PLAY*)pNezPlay);
}

static const NEZ_NES_RESET_HANDLER zxay_reset_handler[] = {
	{ NES_RESET_SYS_LAST, ZXAYReset, NULL },
	{ 0,                  0, NULL },
};

static void ZXAYTerminate(NEZ_PLAY *pNezPlay)
{
	if (((NEZ_PLAY*)pNezPlay)->zxay)
	{
		terminate((ZXAY*)((NEZ_PLAY*)pNezPlay)->zxay);
		((NEZ_PLAY*)pNezPlay)->zxay = 0;
	}
}

static const NEZ_NES_TERMINATE_HANDLER zxay_terminate_handler[] = {
	{ ZXAYTerminate, NULL },
	{ 0, NULL },
};

PROTECTED uint32_t ZXAYLoad(NEZ_PLAY *pNezPlay, const uint8_t *pData, uint32_t uSize)
{
	uint32_t ret;
	ZXAY *THIS_;
	if (pNezPlay->zxay) return NEZ_NESERR_FORMAT; //__builtin_trap();	/* ASSERT */
	THIS_ = (ZXAY *)XMALLOC(sizeof(ZXAY));
	if (!THIS_) return NEZ_NESERR_SHORTOFMEMORY;
	ret = load(pNezPlay, THIS_, pData, uSize);
	if (ret)
	{
		terminate(THIS_);
		return ret;
	}
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
    pNezPlay->songinfodata.detail[0] = 0;

	pNezPlay->zxay = THIS_;
	NESAudioHandlerInstall(pNezPlay, zxay_audio_handler);
	NESVolumeHandlerInstall(pNezPlay, zxay_volume_handler);
	NESResetHandlerInstall(pNezPlay->nrh, zxay_reset_handler);
	NESTerminateHandlerInstall(&pNezPlay->nth, zxay_terminate_handler);
	return ret;
}

#undef IRQ_PATCH
#undef SHIFT_CPS
#undef ZX_BASECYCLES
#undef AMSTRAD_BASECYCLES
