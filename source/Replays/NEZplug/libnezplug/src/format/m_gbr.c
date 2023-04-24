#include "../include/nezplug/nezplug.h"
#include "handler.h"
#include "audiosys.h"
#include "songinfo.h"

#include "m_gbr.h"
#include "../device/s_dmg.h"
#include "../common/divfix.h"
#include "../common/util.h"

#include "../cpu/kmz80/kmz80.h"
#include <stdio.h>
#include <string.h>
#define SHIFT_CPS 15

#define DMG_BASECYCLES (4096 * 1024)

#define TEKKA_PATCH_ENABLE 0
#define MARIO2_PATCH_ENABLE 0
#define GBS2GB_EMULATION 0
#if 0

DMG
system clock 4194304Hz
hsync clock 9198Hz (456 cycles)
vsync clock 59.73Hz (456*154cycles)
timer0 clock 4096Hz   (system clock / 1024)
timer1 clock 262144Hz (system clock / 16)
timer2 clock 65536Hz  (system clock / 64)
timer3 clock 16384Hz  (system clock / 256)

sound length counter 256Hz
sound length(ch3) counter 2Hz
sweep 128Hz
envelope 64Hz

h/v = 154 = 144(vdisp)+10(vbrank) lines

FF80-FFFF high RAM
FF00-FF4B I/O
E000-FE00 (Mirror of 8kB int32_ternal RAM)
D000-DFFF 4kB int32_ternal RAM / 4kB switchable CGB int32_ternal RAM bank
C000-CFFF 4kB int32_ternal RAM
A000-BFFF 8kB External switchable RAM bank
8000-9FFF 8kB VRAM
6000-7FFF 16kB External switchable ROM bank / MBC1 ROM/RAM Select
4000-5FFF ...                               / RAM Bank Select
2000-3FFF 16kB External ROM bank #0         / ROM Bank Select
0000-1FFF ...                               / RAM Bank enable

#endif

static const uint32_t gbr_timer_clock_table[4] = {
	10, 4, 6, 8,
};

typedef struct GBRDMG_TAG GBRDMG;
typedef uint32_t (*GBR_READPROC)(GBRDMG *THIS_, uint32_t a);
typedef void (*GBR_WRITEPROC)(GBRDMG *THIS_, uint32_t a, uint32_t v);

struct  GBRDMG_TAG {
	KMZ80_CONTEXT ctx;
	KMIF_SOUND_DEVICE *dmgsnd;
	KMEVENT kme;
	KMEVENT_ITEM_ID vsync;
	KMEVENT_ITEM_ID timer;

	uint32_t cps;		/* cycles per sample:fixed point */
	uint32_t cpsrem;	/* cycle remain */
	uint32_t cpsgap;	/* cycle gap */
	uint32_t total_cycles;	/* total played cycles */

	uint32_t startsong;
	uint32_t maxsong;

	uint32_t loadaddr;
	uint32_t initaddr;
	uint32_t vsyncaddr;
	uint32_t timeraddr;
	//---+ [changes_rough.txt]
	uint32_t playaddr;
	//---+
	uint32_t stackaddr;

	uint8_t *bankrom;
	uint8_t *playerrom;
	uint32_t playerromioaddr;

	uint8_t *readmap[16];
	GBR_READPROC readproc[16];
	uint8_t *writemap[16];
	GBR_WRITEPROC writeproc[16];

	uint8_t highram[0x80];
	uint8_t io[0x180];
	uint8_t ram[8 * 0x1000];
	uint8_t vram[0x2000];
	uint8_t bankram[0x10 * 0x2000];
#if USE_DUMMYRAM
	uint8_t dummyram[0x2000];
#endif

	uint8_t isgbr;
	uint8_t bankromnum;
	uint8_t bankromfirst[2];
	uint8_t gb_DIV;
	uint8_t gb_TIMA;
	uint8_t gb_TMA;
	uint8_t gb_TMC;
	uint8_t gb_IE;
	uint8_t gb_IF;
	uint8_t timerflag;
	uint8_t firstTMA;
	uint8_t firstTMC;
	uint8_t isCGB;
	//---+ [changes_rough.txt]
	uint8_t useINT;
	//---+
	uint8_t rambankenable;
	uint8_t rombankselect;
	uint8_t rombankselecthi;
	uint8_t rambankselect;
	uint8_t romramselect;
	enum {
		GB_MAPPER_ROM,
		GB_MAPPER_MBC1,
		GB_MAPPER_MBC2,
		GB_MAPPER_MBC3,
		GB_MAPPER_MBC5,
		GB_MAPPER_SSC,
		GB_MAPPER_GBR
	} mapper_type;
};


static int32_t gbr_execute(GBRDMG *THIS_)
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

__inline static void gbr_snyth(GBRDMG *THIS_, int32_t *d)
{
	THIS_->dmgsnd->synth(THIS_->dmgsnd->ctx, d);
}

__inline static void gbr_volume(GBRDMG *THIS_, uint32_t v)
{
	THIS_->dmgsnd->volume(THIS_->dmgsnd->ctx, v);
}

static void gbr_vsync_setup(GBRDMG *THIS_)
{
	kmevent_settimer(&THIS_->kme, THIS_->vsync, THIS_->isCGB ? (154 * 456 * 2) : (154 * 456));
}

static void gbr_timer_setup(GBRDMG *THIS_)
{
	if (THIS_->gb_TMC & 0x4)
	{
		kmevent_settimer(&THIS_->kme, THIS_->timer, (1 << gbr_timer_clock_table[THIS_->gb_TMC & 3]) * (256 - THIS_->gb_TIMA));
	}
	else
	{
		kmevent_settimer(&THIS_->kme, THIS_->timer, 0);
	}
}

static void gbr_timer_update_TIMA(GBRDMG *THIS_)
{
	uint32_t cnt;
	/* タイマ動作中なら現カウンタを取得 */
	if (kmevent_gettimer(&THIS_->kme, THIS_->timer, &cnt))
	{
		cnt >>= gbr_timer_clock_table[THIS_->gb_TMC & 3];
		THIS_->gb_TIMA = (uint8_t)((256 - cnt) & 0xff);
	}
}

static void gbr_map_read_proc8k(GBRDMG *THIS_, uint32_t a, GBR_READPROC proc)
{
	uint32_t page = a >> 12;
	THIS_->readproc[page] = THIS_->readproc[page + 1] = proc;
}
static void gbr_map_read_mem4k(GBRDMG *THIS_, uint32_t a, uint8_t *p)
{
	uint32_t page = a >> 12;
	THIS_->readproc[page] = 0;
	THIS_->readmap[page] = p - (page << 12);
}
static void gbr_map_read_mem8k(GBRDMG *THIS_, uint32_t a, uint8_t *p)
{
	uint32_t page = a >> 12;
	THIS_->readproc[page] = THIS_->readproc[page + 1] = 0;
	THIS_->readmap[page] = THIS_->readmap[page + 1] = p - (page << 12);
}
static void gbr_map_write_proc8k(GBRDMG *THIS_, uint32_t a, GBR_WRITEPROC proc)
{
	uint32_t page = a >> 12;
	THIS_->writeproc[page] = THIS_->writeproc[page + 1] = proc;
}
static void gbr_map_write_mem4k(GBRDMG *THIS_, uint32_t a, uint8_t *p)
{
	uint32_t page = a >> 12;
	THIS_->writeproc[page] = 0;
	THIS_->writemap[page] = p - (page << 12);
}
static void gbr_map_write_mem8k(GBRDMG *THIS_, uint32_t a, uint8_t *p)
{
	uint32_t page = a >> 12;
	THIS_->writeproc[page] = THIS_->writeproc[page + 1] = 0;
	THIS_->writemap[page] = THIS_->writemap[page + 1] = p - (page << 12);
}

static uint32_t gbr_read_null(GBRDMG *THIS_, uint32_t a)
{
    (void)THIS_;
    (void)a;
	return 0xFF;
}
static void gbr_write_null(GBRDMG *THIS_, uint32_t a, uint32_t v)
{
    (void)THIS_;
    (void)a;
    (void)v;
}

static uint32_t gbr_read_E000(GBRDMG *THIS_, uint32_t a)
{
	if (a >= 0xff80 && a != 0xffff)
		return THIS_->highram[a & 0x7f];
	else if (a < 0xfe00)
		return THIS_->ram[a & 0x1fff];
	else if (0xff10 <= a &&  a <= 0xff3f)
		return THIS_->dmgsnd->read(THIS_->dmgsnd->ctx, a);
	else {
		switch (a)
		{
			case 0xff04:	/* DIV */
				return (THIS_->gb_DIV + ((THIS_->total_cycles + THIS_->ctx.cycle) >> 8)) & 0xff;
			case 0xff05:	/* TIMA */
				gbr_timer_update_TIMA(THIS_);
				return THIS_->gb_TIMA;
			case 0xff06:	/* TMA */
				return THIS_->gb_TMA;
			case 0xff07:	/* TMC */
				return THIS_->gb_TMC;
			case 0xff0f:	/* IF */
				return THIS_->gb_IF;
//---+ [ff4d_fix.txt]
			case 0xff4d:	/* Speed Switch */
				if (THIS_->isCGB == 0) {
				return (uint8_t)0x7e;
				} else {
				return THIS_->io[a & 0x1ff] | 0x80;
				}
//---+
			case 0xffff:	/* IE */
				return THIS_->gb_IE;
		}
		return THIS_->io[a & 0x1ff];
	}
}
static void gbr_write_E000(GBRDMG *THIS_, uint32_t a, uint32_t v)
{
	if (a >= 0xff80 && a != 0xffff)
		THIS_->highram[a & 0x7f] = (uint8_t)v;
	else if (a < 0xfe00)
		THIS_->ram[a & 0x1fff] = (uint8_t)v;
	else if (0xff10 <= a &&  a <= 0xff3f)
		THIS_->dmgsnd->write(THIS_->dmgsnd->ctx, a, v);
	else {
		THIS_->io[a & 0x1ff] = (uint8_t)v;
		switch (a)
		{
			case 0xff04:	/* DIV */
				THIS_->gb_DIV = (v - ((THIS_->total_cycles + THIS_->ctx.cycle) >> 8)) & 0xff;
				break;
			case 0xff05:	/* TIMA */
				THIS_->gb_TIMA = (uint8_t)v;
				gbr_timer_setup(THIS_);
				break;
			case 0xff06:	/* TMA */
				THIS_->gb_TMA = (uint8_t)v;
				break;
			case 0xff07:	/* TMC */
				{
					uint32_t nextcount;
					double cnt;
                    nextcount = 0;
					kmevent_gettimer(&THIS_->kme, THIS_->timer, &nextcount);
					cnt = ((double)nextcount) / (1 << gbr_timer_clock_table[THIS_->gb_TMC & 3]);
					THIS_->gb_TMC = (uint8_t)v;
					cnt = cnt * (1 << gbr_timer_clock_table[THIS_->gb_TMC & 3]);
					if (THIS_->gb_TMC & 0x4)
					{
						kmevent_settimer(&THIS_->kme, THIS_->timer, (uint32_t)cnt);
					}
					else
					{
						kmevent_settimer(&THIS_->kme, THIS_->timer, 0);
					}
				}
				//THIS_->gb_TIMA = THIS_->gb_TMA;
				//timer_setup(THIS_);
				break;
			case 0xff0f:	/* IF */
				THIS_->ctx.regs8[REGID_INTREQ] = THIS_->gb_IF = (uint8_t)v;
				break;
//---+ [ff4d_fix.txt]
			case 0xff4d:	/* Speed Switch */
				/* The XOR(^) is normally done during an OP_STOP command */
				if (THIS_->isCGB == 0) {
				THIS_->io[a & 0x1ff] = (uint8_t)0x7e;
				} else {
				if (((uint8_t)v & 1) == 1) {
				THIS_->io[a & 0x1ff] = (uint8_t)0xfe;
				}
				}
				break;
//---+
			case 0xff70:	/* SVBK */
				{
					uint32_t rampage = v & 7;
					if (!rampage) rampage = 1;
					gbr_map_read_mem4k(THIS_, 0xd000, &THIS_->ram[rampage << 12]);
					gbr_map_write_mem4k(THIS_, 0xd000, &THIS_->ram[rampage << 12]);
				}
				break;
			case 0xffff:	/* IE */
				THIS_->gb_IE = (uint8_t)v;
				THIS_->ctx.regs8[REGID_INTMASK] = THIS_->gb_IE & 0x5;
				break;
		}
	}
}

static void gbr_force_rombank(GBRDMG *THIS_, uint32_t bank, uint32_t data)
{
	uint32_t base = bank << 14;
	if (data >= THIS_->bankromnum)
	{
#if 1
		/* 無視してみる */
#else
		/* bank #0 を割り当ててみる */
		map_read_mem8k(THIS_, base + 0x0000, THIS_->bankrom);
		map_read_mem8k(THIS_, base + 0x2000, THIS_->bankrom + 0x2000);
		/* 無効領域マップしてみる */
		map_read_proc8k(THIS_, base + 0x0000, read_null);
		map_read_proc8k(THIS_, base + 0x2000, read_null);
#endif
	}
	else
	{
		gbr_map_read_mem8k(THIS_, base + 0x0000, THIS_->bankrom + (data << 14));
		gbr_map_read_mem8k(THIS_, base + 0x2000, THIS_->bankrom + (data << 14) + 0x2000);
	}
}

static void gbr_update_banks(GBRDMG *THIS_)
{
	uint32_t rambankenable = 0;
	uint32_t rambank = 0;
	uint32_t rombank;
	rombank = THIS_->rombankselect;
	switch (THIS_->mapper_type)
	{
		case GB_MAPPER_ROM:
			rombank = 1;
			rambankenable = 0;
			rambank = 0;
			break;
		case GB_MAPPER_MBC1:
		case GB_MAPPER_SSC:
			if (THIS_->romramselect)
			{
				rambank = THIS_->rambankselect;
			}
			else
			{
				rambank = 0;
				rombank += (THIS_->rambankselect << 5);
			}
			rambankenable = (THIS_->mapper_type == GB_MAPPER_SSC) || (THIS_->rambankenable == 0xa);
			if (rombank == 0) rombank = 1;
			break;
		case GB_MAPPER_MBC2:
			rambankenable = (THIS_->rambankenable == 0xa);
			rambank = 0;
			if (rombank == 0) rombank = 1;
			break;
		case GB_MAPPER_MBC3:
			rambank = THIS_->rambankselect;
			rambankenable = (rambank < 4) || (THIS_->rambankenable == 0xa);
			if (rombank == 0) rombank = 1;
			break;
		case GB_MAPPER_MBC5:
			rambank = THIS_->rambankselect;
			rombank += THIS_->rombankselecthi << 8;
			rambankenable = (THIS_->rambankenable == 0xa);
			break;
		case GB_MAPPER_GBR:
			rambank = THIS_->rambankselect;
			rombank += THIS_->rombankselecthi << 8;
			rambankenable = (THIS_->rambankenable == 0xa);
			if (rombank == 0) rombank = 1;
			break;
	}
	if (rambankenable)
	{
		gbr_map_write_mem8k(THIS_, 0xA000, THIS_->bankram + (rambank << 13));
		gbr_map_read_mem8k(THIS_, 0xA000, THIS_->bankram + (rambank << 13));
	}
	else
	{
#if USE_DUMMYRAM
		gbr_map_write_mem8k(THIS_, 0xA000, THIS_->dummyram);
		gbr_map_read_mem8k(THIS_, 0xA000, THIS_->dummyram);
#else
		gbr_map_write_proc8k(THIS_, 0xA000, gbr_write_null);
		gbr_map_read_proc8k(THIS_, 0xA000, gbr_read_null);
#endif
	}
	gbr_force_rombank(THIS_, 1, rombank);
}

static void gbr_write_rambankenable(GBRDMG *THIS_, uint32_t a, uint32_t v)
{
	switch (THIS_->mapper_type)
	{
		case GB_MAPPER_MBC1:
		case GB_MAPPER_SSC:
		case GB_MAPPER_MBC3:
		case GB_MAPPER_MBC5:
			THIS_->rambankenable = (uint8_t)(v & 0x0f);
			gbr_update_banks(THIS_);
			break;
		case GB_MAPPER_MBC2:
			if (!(a & 0x0100))
			{
				THIS_->rambankenable = (uint8_t)(v & 0x0f);
				gbr_update_banks(THIS_);
			}
			break;
		default:
			break;
	}
}


static void gbr_write_rombankselect(GBRDMG *THIS_, uint32_t a, uint32_t v)
{
	switch (THIS_->mapper_type)
	{
		case GB_MAPPER_MBC1:
		case GB_MAPPER_SSC:
			THIS_->rombankselect = (uint8_t)(v & 0x1f);
			gbr_update_banks(THIS_);
			break;
		case GB_MAPPER_MBC2:
			if (a & 0x0100)
			{
				THIS_->rombankselect = (uint8_t)(v & 0x0f);
				gbr_update_banks(THIS_);
			}
			break;
		case GB_MAPPER_MBC3:
			THIS_->rombankselect = (uint8_t)(v & 0x7f);
			gbr_update_banks(THIS_);
			break;
		case GB_MAPPER_MBC5:
		case GB_MAPPER_GBR:
			if (a & 0x01000)
				THIS_->rombankselecthi = (uint8_t)(v & 1);
			else
				THIS_->rombankselect = (uint8_t)(v & 0xff);
			gbr_update_banks(THIS_);
			break;
		default:
			break;
	}
}
static void gbr_write_rambankselect(GBRDMG *THIS_, uint32_t a, uint32_t v)
{
    (void)a;
	switch (THIS_->mapper_type)
	{
		case GB_MAPPER_MBC1:
		case GB_MAPPER_SSC:
			THIS_->rambankselect = (uint8_t)(v & 0x03);
			gbr_update_banks(THIS_);
			break;
		case GB_MAPPER_MBC3:
		case GB_MAPPER_MBC5:
		case GB_MAPPER_GBR:
			THIS_->rambankselect = (uint8_t)(v & 0x0f);
			gbr_update_banks(THIS_);
			break;
		default:
			break;
	}
}

static void gbr_write_romramselect(GBRDMG *THIS_, uint32_t a, uint32_t v)
{
    (void)a;
	switch (THIS_->mapper_type)
	{
		case GB_MAPPER_MBC1:
		case GB_MAPPER_SSC:
			THIS_->romramselect = (uint8_t)(v & 0x1);
			gbr_update_banks(THIS_);
			break;
		default:
			break;
	}
}


static uint32_t gbr_read_event(void *ctx, uint32_t a)
{
	GBRDMG *THIS_ = ctx;
	uint32_t page = a >> 12;
	if (THIS_->readproc[page])
		return THIS_->readproc[page](THIS_, a);
	else
		return THIS_->readmap[page][a];
}

static void gbr_write_event(void *ctx, uint32_t a, uint32_t v)
{
	GBRDMG *THIS_ = ctx;
	uint32_t page = a >> 12;
	if (THIS_->writeproc[page])
		THIS_->writeproc[page](THIS_, a, v);
	else
		THIS_->writemap[page][a] = (uint8_t)v;
}

static void gbr_vsync_event(KMEVENT *event, KMEVENT_ITEM_ID curid, GBRDMG *THIS_)
{
    (void)event;
    (void)curid;

	gbr_vsync_setup(THIS_);
	THIS_->gb_IF |= 1;
	THIS_->ctx.regs8[REGID_INTREQ] |= 1;
	//---+ [changes_rough.txt]
	if (THIS_->useINT) THIS_->ctx.playflag |= 0x41;
	//---+
	if (THIS_->gb_IE & 1)
	{
	}//else
	//	vsync_setup2(THIS_);
}

static void gbr_timer_event(KMEVENT *event, KMEVENT_ITEM_ID curid, GBRDMG *THIS_)
{
    (void)event;
    (void)curid;
	THIS_->gb_TIMA = THIS_->gb_TMA;
	gbr_timer_setup(THIS_);
	THIS_->gb_IF |= 4;
	THIS_->ctx.regs8[REGID_INTREQ] |= 4;
	//---+ [changes_rough.txt]
	if (THIS_->useINT) THIS_->ctx.playflag |= 0x44;
	//---+
	if (THIS_->gb_IE & 4)
	{
	}//else
	//	timer_setup2(THIS_);

}

static void gbr_reset(NEZ_PLAY *pNezPlay)
{
	GBRDMG *THIS_ = pNezPlay->gbrdmg;
	uint32_t song, initbreak;
	uint32_t freq = NESAudioFrequencyGet(pNezPlay);
	song = SONGINFO_GetSongNo(pNezPlay->song) - 1;
	if (song >= THIS_->maxsong) song = THIS_->startsong - 1;

	THIS_->playerromioaddr = 0x9fc0;
	THIS_->playerrom = &THIS_->vram[THIS_->playerromioaddr & 0x1fff];
	kmdmg_reset(&THIS_->ctx);
	XMEMSET(THIS_->highram, 0, 0x80);
	XMEMSET(THIS_->io, 0, 0x180);
	XMEMSET(THIS_->ram, 0, 8 * 0x1000);
	XMEMSET(THIS_->vram, 0, 0x2000);
	XMEMSET(THIS_->bankram, 0, 0x10 * 0x2000);
#if USE_DUMMYRAM
	XMEMSET(THIS_->dummyram, 0, 0x2000);
#endif
	THIS_->dmgsnd->reset(THIS_->dmgsnd->ctx, DMG_BASECYCLES, freq);
	kmevent_init(&THIS_->kme);
	THIS_->cpsrem = THIS_->cpsgap = 0;
	THIS_->cps = DivFix(DMG_BASECYCLES, freq, SHIFT_CPS + THIS_->isCGB);
	THIS_->ctx.user = THIS_;
	THIS_->ctx.kmevent = &THIS_->kme;
	THIS_->ctx.memread = gbr_read_event;
	THIS_->ctx.memwrite = gbr_write_event;
	THIS_->vsync = kmevent_alloc(&THIS_->kme);
	THIS_->timer = kmevent_alloc(&THIS_->kme);
	kmevent_setevent(&THIS_->kme, THIS_->vsync, gbr_vsync_event, THIS_);
	kmevent_setevent(&THIS_->kme, THIS_->timer, gbr_timer_event, THIS_);

	THIS_->rambankenable = 0xa/*THIS_->isgbr ? 0xa : 0*/;
	THIS_->rambankselect = 0;
	THIS_->romramselect =  0;
	THIS_->rombankselecthi = 0;
	gbr_force_rombank(THIS_, 0, 0);
	gbr_force_rombank(THIS_, 1, 0);
	gbr_force_rombank(THIS_, 0, THIS_->bankromfirst[0]);
	THIS_->rombankselect = THIS_->bankromfirst[1];
	gbr_map_write_proc8k(THIS_, 0x0000, gbr_write_rambankenable);
	gbr_map_write_proc8k(THIS_, 0x2000, gbr_write_rombankselect);
	gbr_map_write_proc8k(THIS_, 0x4000, gbr_write_rambankselect);
	gbr_map_write_proc8k(THIS_, 0x6000, gbr_write_romramselect);
	gbr_map_read_mem8k(THIS_, 0x8000, THIS_->vram);
	gbr_map_write_mem8k(THIS_, 0x8000, THIS_->vram);
	gbr_map_read_mem8k(THIS_, 0xC000, THIS_->ram);
	gbr_map_write_mem8k(THIS_, 0xC000, THIS_->ram);
	gbr_map_read_proc8k(THIS_, 0xE000, gbr_read_E000);
	gbr_map_write_proc8k(THIS_, 0xE000, gbr_write_E000);
	gbr_update_banks(THIS_);

	THIS_->ctx.regs8[REGID_A] = (uint8_t)(song & 0xff);
#if 1
	THIS_->ctx.regs8[REGID_IMODE] = 4;	/* VECTOR IRQ MODE */
#else
	THIS_->ctx.regs8[REGID_IMODE] = 5;	/* VECTOR CALL MODE */
#endif
	THIS_->ctx.exflag = 2;	/* AUTOIRQCLAR */
	THIS_->ctx.vector[0] = THIS_->vsyncaddr;
	THIS_->ctx.vector[2] = THIS_->timeraddr;
	//---+ [changes_rough.txt]
	THIS_->ctx.playbase = THIS_->playaddr;
	THIS_->ctx.playflag = 0;
	//---+

	/* 9fc0 DMA */
	THIS_->playerrom[0x00] = 0xCD;	/* CALL */
	THIS_->playerrom[0x01] = (THIS_->initaddr >> 0) & 0xFF;
	THIS_->playerrom[0x02] = (THIS_->initaddr >> 8) & 0xFF;
	THIS_->playerrom[0x03] = 0x18;	/* JR */
	THIS_->playerrom[0x04] = 0xfe;	/* -2 */
#if TEKKA_PATCH_ENABLE
#if 0
	/*
		TEKKA.GBR(TEKKAMAN BLADE)対策 (PLAYからのRET前に余分なPOP) Ver.1(廃止)
		  こちらの方がわかりやすい？Wiz外伝で異常。
	*/
	THIS_->playerrom[0x05] = 0xcd;	/* CALL */
	THIS_->playerrom[0x06] = ((THIS_->playerromioaddr + 0x0a) >> 0) & 0xFF;
	THIS_->playerrom[0x07] = ((THIS_->playerromioaddr + 0x0a) >> 8) & 0xFF;
	THIS_->playerrom[0x08] = 0x18;	/* JR */
	THIS_->playerrom[0x09] = 0xfb;	/* -5 */
	THIS_->playerrom[0x0a] = 0xcd;	/* CALL */
	THIS_->playerrom[0x0b] = ((THIS_->playerromioaddr + 0x0f) >> 0) & 0xFF;
	THIS_->playerrom[0x0c] = ((THIS_->playerromioaddr + 0x0f) >> 8) & 0xFF;
	THIS_->playerrom[0x0d] = 0x18;	/* JR */
	THIS_->playerrom[0x0e] = 0xfb;	/* -5 */
	THIS_->playerrom[0x0f] = 0xcd;	/* CALL */
	THIS_->playerrom[0x10] = ((THIS_->playerromioaddr + 0x14) >> 0) & 0xFF;
	THIS_->playerrom[0x11] = ((THIS_->playerromioaddr + 0x14) >> 8) & 0xFF;
	THIS_->playerrom[0x12] = 0x18;	/* JR */
	THIS_->playerrom[0x13] = 0xfb;	/* -5 */
	THIS_->playerrom[0x14] = 0xcd;	/* CALL */
	THIS_->playerrom[0x15] = ((THIS_->playerromioaddr + 0x18) >> 0) & 0xFF;
	THIS_->playerrom[0x16] = ((THIS_->playerromioaddr + 0x18) >> 8) & 0xFF;
	THIS_->playerrom[0x17] = 0x18;	/* JR */
	THIS_->playerrom[0x18] = 0xfb;	/* -5 & EI */
	THIS_->playerrom[0x19] = 0x00;	/* NOP */
	THIS_->playerrom[0x1a] = 0x76;	/* HALT */
	THIS_->playerrom[0x1b] = 0x18;	/* JR */
	THIS_->playerrom[0x1c] = 0xfb;	/* -5 */
#else
	/*
		TEKKA.GBR対策 (PLAYからのRET前に余分なPOP) Ver.2
		  割り込み禁止(Wiz外伝対策)
	*/
	THIS_->playerrom[0x05] = 0xf3;	/* DI */
	THIS_->playerrom[0x06] = 0xcd;	/* CALL */
	THIS_->playerrom[0x07] = ((THIS_->playerromioaddr + 0x0e) >> 0) & 0xFF;
	THIS_->playerrom[0x08] = ((THIS_->playerromioaddr + 0x0e) >> 8) & 0xFF;
	THIS_->playerrom[0x09] = 0x31;	/* LD SP,nn */
	THIS_->playerrom[0x0a] = (THIS_->stackaddr >> 0) & 0xFF;
	THIS_->playerrom[0x0b] = (THIS_->stackaddr >> 8) & 0xFF;
	THIS_->playerrom[0x0c] = 0x18;	/* JR */
	THIS_->playerrom[0x0d] = 0xf7;	/* -9 */
	THIS_->playerrom[0x0e] = 0xf5;	/* PUSH AF */
	THIS_->playerrom[0x0f] = 0x7d;	/* LD A,L */
	THIS_->playerrom[0x10] = 0xea;	/* LD (nn),A */
	THIS_->playerrom[0x11] = ((THIS_->playerromioaddr + 0x1e) >> 0) & 0xFF;
	THIS_->playerrom[0x12] = ((THIS_->playerromioaddr + 0x1e) >> 8) & 0xFF;
	THIS_->playerrom[0x13] = 0x7c;	/* LD A,H */
	THIS_->playerrom[0x14] = 0xea;	/* LD (nn),A */
	THIS_->playerrom[0x15] = ((THIS_->playerromioaddr + 0x20) >> 0) & 0xFF;
	THIS_->playerrom[0x16] = ((THIS_->playerromioaddr + 0x20) >> 8) & 0xFF;
	THIS_->playerrom[0x17] = 0xf1;	/* POP AF */
	THIS_->playerrom[0x18] = 0xe1;	/* POP HL */
	THIS_->playerrom[0x19] = 0xe5;	/* PUSH HL */
	THIS_->playerrom[0x1a] = 0xe5;	/* PUSH HL */
	THIS_->playerrom[0x1b] = 0xe5;	/* PUSH HL */
	THIS_->playerrom[0x1c] = 0xe5;	/* PUSH HL */
	THIS_->playerrom[0x1d] = 0x2e;	/* LD L,n */
	THIS_->playerrom[0x1e] = 0x00;
	THIS_->playerrom[0x1f] = 0x26;	/* LD H,n */
	THIS_->playerrom[0x20] = 0x00;
	THIS_->playerrom[0x21] = 0xfb;	/* EI */
	THIS_->playerrom[0x22] = 0x00;	/* NOP */
	THIS_->playerrom[0x23] = 0x76;	/* HALT */
	THIS_->playerrom[0x24] = 0x18;	/* JR */
	THIS_->playerrom[0x25] = 0xfb;	/* -5 */
#endif
#else
#if GBS2GB_EMULATION
	/*
		再生アドレスに飛んだときのレジスタの値を、GBS2GBに合わせるテスト
	*/
	THIS_->playerrom[0x05] = 0x21;	/* LD HL,0c03 */
	THIS_->playerrom[0x06] = 0x03;	
	THIS_->playerrom[0x07] = 0x0c;	
	THIS_->playerrom[0x08] = 0x11;	/* LD DE,0001 */
	THIS_->playerrom[0x09] = 0x01;	
	THIS_->playerrom[0x0A] = 0x00;	
	THIS_->playerrom[0x0B] = 0x01;	/* LD BC,0080 */
	THIS_->playerrom[0x0C] = 0x80;	
	THIS_->playerrom[0x0D] = 0x00;	
	THIS_->playerrom[0x0E] = 0x3e;	/* LD A,3 */
	THIS_->playerrom[0x0F] = 0x03;	
	THIS_->playerrom[0x10] = 0xFB;	/* EI */
	THIS_->playerrom[0x11] = 0x00;	/* NOP */
	THIS_->playerrom[0x12] = 0x76;	/* HALT */
	THIS_->playerrom[0x13] = 0x18;	/* JR */
	THIS_->playerrom[0x14] = 0xf0;	/* -5 */
#else
	THIS_->playerrom[0x05] = 0xFB;	/* EI */
//	THIS_->playerrom[0x06] = 0x00;	/* NOP */
	THIS_->playerrom[0x06] = 0x76;	/* HALT */
	THIS_->playerrom[0x07] = 0x18;	/* JR */
	THIS_->playerrom[0x08] = 0xfc;	/* -5 */
	THIS_->playerrom[0x09] = 0xc9;	/* -5 */
#endif
#endif
	THIS_->ctx.pc = THIS_->playerromioaddr;
	THIS_->ctx.sp = THIS_->stackaddr;
	THIS_->ctx.regs8[REGID_IFF1] = THIS_->ctx.regs8[REGID_IFF2] = 0;

	THIS_->ctx.regs8[REGID_INTREQ] = THIS_->gb_IF = 0;
	THIS_->ctx.rstbase = THIS_->loadaddr;

	THIS_->ctx.regs8[REGID_INTMASK] = THIS_->gb_IE = THIS_->timerflag;
	THIS_->gb_TMA = THIS_->firstTMA;
	THIS_->gb_TMC = THIS_->firstTMC;
	THIS_->gb_TIMA = THIS_->gb_TMA;


	/* request execute */
	initbreak = 5 << 8;
	while (THIS_->ctx.pc != THIS_->playerromioaddr + 3 && --initbreak) kmz80_exec(&THIS_->ctx, DMG_BASECYCLES >> 8);
	THIS_->playerrom[4] = 0x00;	/* -2 */

	/* timer enable */
	/* nemesis1/2, last bible1/2 */
	if (THIS_->timerflag & 4) THIS_->gb_TMC |= 4;

	gbr_vsync_setup(THIS_);
	gbr_timer_setup(THIS_);
	/* THIS_->gb_TIMA = THIS_->gb_TMA; */
	/* THIS_->gb_IE = THIS_->timerflag; */

	THIS_->total_cycles = 0;

}

static void gbr_terminate(GBRDMG *THIS_)
{
	//ここまでダンプ設定
	if (THIS_->dmgsnd) THIS_->dmgsnd->release(THIS_->dmgsnd->ctx);
	if (THIS_->bankrom) XFREE(THIS_->bankrom);
	XFREE(THIS_);
}

static uint32_t gbr_load(NEZ_PLAY *pNezPlay, GBRDMG *THIS_, const uint8_t *pData, uint32_t uSize)
{
	uint8_t titlebuffer[0x21];
	uint8_t artistbuffer[0x21];
	uint8_t copyrightbuffer[0x21];

	XMEMSET(THIS_, 0, sizeof(GBRDMG));
	THIS_->dmgsnd = 0;
	THIS_->bankrom = 0;
	if (pData[0] != 0x47 || pData[1] != 0x42) return NEZ_NESERR_FORMAT;
	if (pData[2] == 0x52)
	{
		if (pData[3] != 0x46) return NEZ_NESERR_FORMAT;
		THIS_->isgbr = 1;	/* 'GBRF' GameBoy Ripped sound Format */
	}
	else if (pData[2] == 0x53)
		THIS_->isgbr = 0;	/* 'GBS' GameBoy Sound format */
	else
		return NEZ_NESERR_FORMAT;

	if (THIS_->isgbr)
	{
		if (uSize < 0x20) return NEZ_NESERR_FORMAT;
		THIS_->maxsong = 256;
		THIS_->startsong = 1;
		SONGINFO_SetStartSongNo(pNezPlay->song, THIS_->startsong);
		SONGINFO_SetMaxSongNo(pNezPlay->song, THIS_->maxsong);
		THIS_->bankromnum = pData[4];
		if (!THIS_->bankromnum) return NEZ_NESERR_FORMAT;
		THIS_->bankromfirst[0] = pData[5];
		THIS_->bankromfirst[1] = pData[6];
		THIS_->timerflag = 0;
		if (pData[7] & 1) THIS_->timerflag |= 1;
		if (pData[7] & 2) THIS_->timerflag |= 4;
		THIS_->loadaddr = 0;
		THIS_->initaddr = GetWordLE(pData + 0x08);
		THIS_->vsyncaddr = GetWordLE(pData + 0x0a);
		THIS_->timeraddr = GetWordLE(pData + 0x0c);
#if 1
		THIS_->stackaddr = 0x9fc0;	/* vram */
#else
		/*  Dr.Mario 再生不可 */
		/*  JUDGE RED 再生不可 */
		THIS_->stackaddr = 0xe000;	/* intrnal ram */
#endif
		THIS_->firstTMA = pData[0x0e];
		THIS_->firstTMC = pData[0x0f] & 0x7f;
		//ヘッダ0FHの最上位ビットが立っていて、その上位４ビットが全部立っていないならGBC
		//上位４ビットが全部立っている = タイマー未使用ビットを全部立たせてる = GBCとは限らない
		THIS_->isCGB = (pData[0x0f] & 0x80) && (pData[0x0f] & 0xf0) != 0xf0 ? 1 : 0;
		THIS_->mapper_type = GB_MAPPER_GBR;
		uSize -= 0x20;
		pData += 0x20;
		if (uSize > ((uint32_t)THIS_->bankromnum) << 14)
			uSize = ((uint32_t)THIS_->bankromnum) << 14;

		XMEMSET(titlebuffer, 0, 0x21);
		XMEMCPY(titlebuffer, pData + 0x0134, 0x10);
        if(pNezPlay->songinfodata.title) {
            XFREE(pNezPlay->songinfodata.title);
        }
        pNezPlay->songinfodata.title = (char *)XMALLOC(strlen((const char *)titlebuffer)+1);
        if(pNezPlay->songinfodata.title == NULL) {
            return NEZ_NESERR_SHORTOFMEMORY;
        }
        XMEMCPY(pNezPlay->songinfodata.title,titlebuffer,strlen((const char *)titlebuffer)+1);

		XMEMSET(artistbuffer, 0, 0x21);
        if(pNezPlay->songinfodata.artist) {
            XFREE(pNezPlay->songinfodata.artist);
            pNezPlay->songinfodata.artist = NULL;
        }

		XMEMSET(copyrightbuffer, 0, 0x21);
        if(pNezPlay->songinfodata.copyright) {
            XFREE(pNezPlay->songinfodata.copyright);
            pNezPlay->songinfodata.copyright = NULL;
        }

		sprintf(pNezPlay->songinfodata.detail,
"Type               : GBRF\r\n\
Bank ROM Num       : %XH\r\n\
Bank ROM(0000-3FFF): %XH\r\n\
Bank ROM(4000-7FFF): %XH\r\n\
Init Address       : %04XH\r\n\
VSync Address      : %04XH\r\n\
Timer Address      : %04XH\r\n\
int32_terrupt Flag     : %s%s\r\n\
GBC Flag(2x Speed) : %d\r\n\
First TMA          : %02XH\r\n\
First TMC          : %02XH"
			,THIS_->bankromnum,THIS_->bankromfirst[0],THIS_->bankromfirst[1],THIS_->initaddr
			,THIS_->vsyncaddr,THIS_->timeraddr
			,(THIS_->timerflag&1) ? "VSync " : "" , (THIS_->timerflag>>2) ? "Timer " : ""
			,THIS_->isCGB,THIS_->firstTMA,THIS_->firstTMC);
		
	}
	else
	{
		uint32_t size;
		if (uSize < 0x70) return NEZ_NESERR_FORMAT;
		THIS_->maxsong = pData[0x04];
		THIS_->startsong = pData[0x05];
		SONGINFO_SetStartSongNo(pNezPlay->song, THIS_->startsong);
		SONGINFO_SetMaxSongNo(pNezPlay->song, THIS_->maxsong);
		THIS_->bankromfirst[0] = 0;
		THIS_->bankromfirst[1] = 1;
		THIS_->loadaddr = GetWordLE(pData + 0x06);
		THIS_->initaddr = GetWordLE(pData + 0x08);
		//--- - [changes_rough.txt]
		//THIS_->vsyncaddr = GetWordLE(pData + 0x0a);
		//THIS_->timeraddr = GetWordLE(pData + 0x0a);
		//THIS_->stackaddr = GetWordLE(pData + 0x0c);
		//THIS_->timerflag = (pData[0x0f] & 4) ? 4 : 1;
		//--- -

		//---+ [changes_rough.txt]
		
		//なんか心配な時用。上位４ビットが全部立っていないなら有効としておく。（ときメモ辺りとか）
		//THIS_->useINT = ((pData[0x0f] & 0x44) == 0x44 && (pData[0x0f] & 0xf0) != 0xf0)? 1 : 0;

		//言われるがままにソースを修正したが、一部のGBSが再生されなくなったのはこの部分のせい
		THIS_->useINT = ((pData[0x0f] & 0x44) == 0x44 )? 1 : 0;
		
		if (THIS_->useINT & 1) {
			THIS_->timerflag = 0x05; /* Timer + VBlank */
			THIS_->vsyncaddr = THIS_->loadaddr + 0x40; //GetWordLE(pData + 0x0a);
			THIS_->timeraddr = THIS_->loadaddr + 0x48; //GetWordLE(pData + 0x0a);
			THIS_->playaddr = GetWordLE(pData + 0x0a);
		}
		else{
			THIS_->timerflag = (pData[0x0f] & 4) ? 4 : 1;
			THIS_->vsyncaddr = (pData[0x0f] & 4) ? 0x9fc9 : GetWordLE(pData + 0x0a);
			THIS_->timeraddr = (pData[0x0f] & 4) ? GetWordLE(pData + 0x0a) : 0x9fc9;
		}
		THIS_->stackaddr = GetWordLE(pData + 0x0c);
		//---+
		THIS_->firstTMA = pData[0x0e];
		THIS_->firstTMC = pData[0x0f] & 7;
		THIS_->isCGB = (pData[0x0f] & 0x80) ? 1 : 0;
		THIS_->mapper_type = GB_MAPPER_MBC1;

		XMEMSET(titlebuffer, 0, 0x21);
		XMEMCPY(titlebuffer, pData + 0x0010, 0x20);

        if(pNezPlay->songinfodata.title != NULL) {
            XFREE(pNezPlay->songinfodata.title);
        }
        pNezPlay->songinfodata.title = (char *)XMALLOC(strlen((const char *)titlebuffer)+1);
        if(pNezPlay->songinfodata.title == NULL) {
            return NEZ_NESERR_SHORTOFMEMORY;
        }
        XMEMCPY(pNezPlay->songinfodata.title,titlebuffer,strlen((const char *)titlebuffer)+1);

		XMEMSET(artistbuffer, 0, 0x21);
		XMEMCPY(artistbuffer, pData + 0x0030, 0x20);

        if(pNezPlay->songinfodata.artist != NULL) {
            XFREE(pNezPlay->songinfodata.artist);
        }
        pNezPlay->songinfodata.artist = (char *)XMALLOC(strlen((const char *)artistbuffer)+1);
        if(pNezPlay->songinfodata.artist == NULL) {
            return NEZ_NESERR_SHORTOFMEMORY;
        }
        XMEMCPY(pNezPlay->songinfodata.artist,artistbuffer,strlen((const char *)artistbuffer)+1);

		XMEMSET(copyrightbuffer, 0, 0x21);
		XMEMCPY(copyrightbuffer, pData + 0x0050, 0x20);

        if(pNezPlay->songinfodata.copyright != NULL) {
            XFREE(pNezPlay->songinfodata.copyright);
        }
        pNezPlay->songinfodata.copyright = (char *)XMALLOC(strlen((const char *)copyrightbuffer)+1);
        if(pNezPlay->songinfodata.copyright == NULL) {
            return NEZ_NESERR_SHORTOFMEMORY;
        }
        XMEMCPY(pNezPlay->songinfodata.copyright,copyrightbuffer,strlen((const char *)copyrightbuffer)+1);

		sprintf(pNezPlay->songinfodata.detail,
"Type              : GBS\r\n\
Song Max          : %d\r\n\
Start Song        : %d\r\n\
Load Address      : %04XH\r\n\
Init Address      : %04XH\r\n\
Play Address      : %04XH\r\n\
Stack Address     : %04XH\r\n\
Timer Mode        : %d\r\n\
First TMA         : %02XH\r\n\
First TMC         : %02XH\r\n\
GBC Mode(2x Speed): %d\r\n\
Use INT           : %d"
			,THIS_->maxsong,THIS_->startsong,THIS_->loadaddr,THIS_->initaddr
			,THIS_->vsyncaddr,THIS_->stackaddr,THIS_->timerflag>>2,THIS_->firstTMA
			,THIS_->firstTMC,THIS_->isCGB,THIS_->useINT);
		
		uSize -= 0x70;
		pData += 0x70;
		size = uSize + THIS_->loadaddr;
		if (size < 0x8000) size = 0x8000;
		size = (size + 0x3fff) & ~ 0x3fff;
		THIS_->bankromnum = (uint8_t)(size >> 14);
		
	}
	SONGINFO_SetChannel(pNezPlay->song, 2);
	SONGINFO_SetExtendDevice(pNezPlay->song, 0);
	SONGINFO_SetInitAddress(pNezPlay->song, THIS_->initaddr);
	//---+
	if (THIS_->useINT == 1)
	SONGINFO_SetPlayAddress(pNezPlay->song, THIS_->playaddr);
	else
	//---+
	SONGINFO_SetPlayAddress(pNezPlay->song, (THIS_->timerflag & 1) ? THIS_->vsyncaddr : THIS_->timeraddr);
	THIS_->bankrom = (uint8_t *)XMALLOC(THIS_->bankromnum << 14);
	if (!THIS_->bankrom) return NEZ_NESERR_SHORTOFMEMORY;
	XMEMSET(THIS_->bankrom, 0, THIS_->bankromnum << 14);
#if MARIO2_PATCH_ENABLE
	/*
		MARIO2.GBR(SUPER MARIO LAND)対策 (ファイルサイズ縮小のため#0と#1を交換)
	*/
	if (THIS_->bankromnum == 2 && THIS_->bankromfirst[0] == 1 && THIS_->bankromfirst[1] == 0)
	{
		/* 4000-7FFFには#0は選択できないので本来は不正 */
		if (uSize > 0x4000) XMEMCPY(THIS_->bankrom + 0x0000, pData + 0x4000, uSize - 0x4000);
		XMEMCPY(THIS_->bankrom + 0x4000, pData + 0x0000, (uSize > 0x4000) ? 0x4000 : uSize);
		THIS_->bankromfirst[0] = 0;
		THIS_->bankromfirst[1] = 1;
	}
	else
	{
		XMEMCPY(THIS_->bankrom + THIS_->loadaddr, pData, uSize);
	}
#else
	XMEMCPY(THIS_->bankrom + THIS_->loadaddr, pData, uSize);
#endif
	THIS_->dmgsnd = DMGSoundAlloc(pNezPlay);
	if (!THIS_->dmgsnd) return NEZ_NESERR_SHORTOFMEMORY;
	return NEZ_NESERR_NOERROR;
}

static int32_t ExecuteDMGCPU(NEZ_PLAY *pNezPlay)
{
	return ((NEZ_PLAY*)pNezPlay)->gbrdmg ? gbr_execute((GBRDMG*)((NEZ_PLAY*)pNezPlay)->gbrdmg) : 0;
}

static void DMGSoundRenderStereo(NEZ_PLAY *pNezPlay, int32_t *d)
{
	gbr_snyth((GBRDMG*)((NEZ_PLAY*)pNezPlay)->gbrdmg, d);
}

static int32_t DMGSoundRenderMono(NEZ_PLAY *pNezPlay)
{
	int32_t d[2] = { 0,0 } ;
	gbr_snyth((GBRDMG*)((NEZ_PLAY*)pNezPlay)->gbrdmg, d);
#if (((-1) >> 1) == -1)
	return (d[0] + d[1]) >> 1;
#else
	return (d[0] + d[1]) / 2;
#endif
}

static const NEZ_NES_AUDIO_HANDLER gbrdmg_audio_handler[] = {
	{ 0, ExecuteDMGCPU, 0, NULL },
	{ 3, DMGSoundRenderMono, DMGSoundRenderStereo, NULL },
	{ 0, 0, 0, NULL },
};

static void GBRDMGVolume(NEZ_PLAY *pNezPlay, uint32_t v)
{
	if (((NEZ_PLAY*)pNezPlay)->gbrdmg)
	{
		gbr_volume((GBRDMG*)((NEZ_PLAY*)pNezPlay)->gbrdmg, v);
	}
}

static const NEZ_NES_VOLUME_HANDLER gbrgbr_volume_handler[] = {
	{ GBRDMGVolume, NULL }, 
	{ 0, NULL }, 
};

static void GBRDMGCPUReset(NEZ_PLAY *pNezPlay)
{
	if (((NEZ_PLAY*)pNezPlay)->gbrdmg) gbr_reset((NEZ_PLAY*)pNezPlay);
}

static const NEZ_NES_RESET_HANDLER gbrdmg_reset_handler[] = {
	{ NES_RESET_SYS_LAST, GBRDMGCPUReset, NULL },
	{ 0,                  0, NULL },
};

static void GBRDMGCPUTerminate(NEZ_PLAY *pNezPlay)
{
	if (((NEZ_PLAY*)pNezPlay)->gbrdmg)
	{
		gbr_terminate((GBRDMG*)((NEZ_PLAY*)pNezPlay)->gbrdmg);
		((NEZ_PLAY*)pNezPlay)->gbrdmg = 0;
	}
}

static const NEZ_NES_TERMINATE_HANDLER gbrdmg_terminate_handler[] = {
	{ GBRDMGCPUTerminate, NULL },
	{ 0, NULL },
};

PROTECTED uint32_t GBRLoad(NEZ_PLAY *pNezPlay, const uint8_t *pData, uint32_t uSize)
{
	uint32_t ret;
	GBRDMG *THIS_;
	if (pNezPlay->gbrdmg) return NEZ_NESERR_FORMAT;// __builtin_trap();	/* ASSERT */
	THIS_ = (GBRDMG *)XMALLOC(sizeof(GBRDMG));
	if (!THIS_) return NEZ_NESERR_SHORTOFMEMORY;
	ret = gbr_load(pNezPlay, THIS_, pData, uSize);
	if (ret)
	{
		gbr_terminate(THIS_);
		return ret;
	}
	pNezPlay->gbrdmg = THIS_;
	NESAudioHandlerInstall(pNezPlay, gbrdmg_audio_handler);
	NESVolumeHandlerInstall(pNezPlay, gbrgbr_volume_handler);
	NESResetHandlerInstall(pNezPlay->nrh, gbrdmg_reset_handler);
	NESTerminateHandlerInstall(&pNezPlay->nth, gbrdmg_terminate_handler);
	return ret;
}

#undef SHIFT_CPS
#undef DMG_BASECYCLES
#undef TEKKA_PATCH_ENABLE
#undef MARIO2_PATCH_ENABLE
#undef GBS2GB_EMULATION
