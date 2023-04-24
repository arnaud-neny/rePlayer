#include "kmsnddev.h"
#include "../common/divfix.h"
#include "logtable.h"
#include "s_scc.h"

#include "../logtable/log_table.h"

#define CPS_SHIFT 17
#define RENDERS 6
#define LIN_BITS 7
#define LOG_BITS 12
#define LOG_LIN_BITS 30

#define LOG_KEYOFF (31 << (LOG_BITS + 1))

typedef struct {
	uint32_t cycles;
	uint32_t spd;
	uint32_t tone[32];
	uint8_t tonereg[32];
	uint32_t volume;
	uint8_t regs[3];
	uint8_t adr;
	uint8_t mute;
	uint8_t key;
	uint8_t pad4[2];
	uint8_t count;
	uint32_t output;
} SCC_CH;

typedef struct {
	KMIF_SOUND_DEVICE kmif;
	SCC_CH ch[5];
	uint32_t majutushida;
	struct {
		uint32_t cps;
		int32_t mastervolume;
		uint8_t mode;
		uint8_t enable;
	} common;
	uint8_t regs[0x10];
    uint8_t *chmask;
} SCCSOUND;

__inline static int32_t SCCSoundChSynth(SCCSOUND *sndp, SCC_CH *ch)
{
	int32_t outputbuf=0,count=0;
	if (ch->spd <= (9 << CPS_SHIFT)) return 0;

	ch->cycles += sndp->common.cps<<RENDERS;
	ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->volume + sndp->common.mastervolume + ch->tone[ch->adr & 0x1F], LOG_LIN_BITS - LIN_BITS - LIN_BITS - 9);
	while (ch->cycles >= ch->spd)
	{
		outputbuf += ch->output;
		count++;

		ch->cycles -= ch->spd;
		ch->count++;
		if(ch->count >= 1<<RENDERS){
			ch->count = 0;
			ch->adr++;
			ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->volume + sndp->common.mastervolume + ch->tone[ch->adr & 0x1F], LOG_LIN_BITS - LIN_BITS - LIN_BITS - 9);
		}
	}
	outputbuf += ch->output;
	count++;

	if (ch->mute || !ch->key) return 0;
	return outputbuf / count;
}

__inline static void SCCSoundChReset(SCC_CH *ch, uint32_t clock, uint32_t freq)
{
    (void)clock;
    (void)freq;
	XMEMSET(ch, 0, sizeof(SCC_CH));
}

static void sndsynth(void *ctx, int32_t *p)
{
	SCCSOUND *sndp = ctx;
	if (sndp->common.enable)
	{
		uint32_t ch;
		int32_t accum = 0;
		for (ch = 0; ch < 5; ch++) accum += SCCSoundChSynth(sndp, &sndp->ch[ch]) * sndp->chmask[NEZ_DEV_SCC_CH1 + ch];
		accum += LogToLinear((LOG_TABLE *)&log_table_12_7_30, sndp->common.mastervolume + sndp->majutushida, LOG_LIN_BITS - LIN_BITS - 14);
		p[0] += accum;
		p[1] += accum;

	}
}

static void sndvolume(void *ctx, int32_t volume)
{
	SCCSOUND *sndp = ctx;
	volume = (volume << (LOG_BITS - 8)) << 1;
	sndp->common.mastervolume = volume;
}

static uint32_t sndread(void *ctx, uint32_t a)
{
    (void)ctx;
    (void)a;
	return 0;
}

static void sndwrite(void *ctx, uint32_t a, uint32_t v)
{
	SCCSOUND *sndp = ctx;
	if (a == 0xbffe || a == 0xbfff)
	{
		sndp->common.mode = v;
	}
	else if ((0x9800 <= a && a <= 0x985F) || (0xB800 <= a && a <= 0xB89F))
	{
		uint32_t tone = LinearToLog((LOG_TABLE *)&log_table_12_7_30, ((int32_t)(v ^ 0x80)) - 0x80);
		sndp->ch[(a & 0xE0) >> 5].tone[a & 0x1F] = tone;
		sndp->ch[(a & 0xE0) >> 5].tonereg[a & 0x1F] = v;
	}
	else if (0x9860 <= a && a <= 0x987F)
	{
		uint32_t tone = LinearToLog((LOG_TABLE *)&log_table_12_7_30, ((int32_t)(v ^ 0x80)) - 0x80);
		sndp->ch[3].tone[a & 0x1F] = sndp->ch[4].tone[a & 0x1F] = tone;
		sndp->ch[3].tonereg[a & 0x1F] = sndp->ch[4].tonereg[a & 0x1F] = v;
	}
	else if ((0x9880 <= a && a <= 0x988F) || (0xB8A0 <= a && a <= 0xB8AF))
	{
		uint32_t port = a & 0x0F;
		sndp->regs[port]=v;
		if (port <= 0x9)
		{
			SCC_CH *ch = &sndp->ch[port >> 1];
			ch->regs[port & 0x1] = v;
			ch->spd = (((ch->regs[1] & 0x0F) << 8) + ch->regs[0] + 1) << CPS_SHIFT;
		}
		else if (0xA <= port && port <= 0xE)
		{
			SCC_CH *ch = &sndp->ch[port - 0xA];
			ch->regs[2] = v;
			ch->volume = LinearToLog((LOG_TABLE *)&log_table_12_7_30, ch->regs[2] & 0xF);
		}
		else
		{
			uint32_t i;
			if (v & 0x1f) sndp->common.enable = 1;
			for (i = 0; i < 5; i++) sndp->ch[i].key = (v & (1 << i));
		}
	}
	else if (0x5000 <= a && a <= 0x5FFF)
	{
		sndp->common.enable = 1;
		sndp->majutushida = LinearToLog((LOG_TABLE *)&log_table_12_7_30, ((int32_t)(v ^ 0x00)) - 0x80);
	}
}

static void sndreset(void *ctx, uint32_t clock, uint32_t freq)
{
	SCCSOUND *sndp = ctx;
	uint32_t ch;
	XMEMSET(&sndp->common,  0, sizeof(sndp->common));
	sndp->common.cps = DivFix(clock, freq, CPS_SHIFT);
	for (ch = 0; ch < 5; ch++) SCCSoundChReset(&sndp->ch[ch], clock, freq);
	for (ch = 0x9800; ch < 0x988F; ch++) sndwrite(sndp, ch, 0);
	sndp->majutushida = LOG_KEYOFF;
}

static void sndrelease(void *ctx)
{
	SCCSOUND *sndp = ctx;
	if (sndp) {
		XFREE(sndp);
	}
}

static void setinst(void *ctx, uint32_t n, void *p, uint32_t l)
{
    (void)ctx;
    (void)n;
    (void)p;
    (void)l;
}

PROTECTED KMIF_SOUND_DEVICE *SCCSoundAlloc(NEZ_PLAY *pNezPlay)
{
	SCCSOUND *sndp;
	sndp = XMALLOC(sizeof(SCCSOUND));
	if (!sndp) return 0;
	XMEMSET(sndp, 0, sizeof(SCCSOUND));

    sndp->chmask = pNezPlay->chmask;
	sndp->kmif.ctx = sndp;
	sndp->kmif.release = sndrelease;
	sndp->kmif.reset = sndreset;
	sndp->kmif.synth = sndsynth;
	sndp->kmif.volume = sndvolume;
	sndp->kmif.write = sndwrite;
	sndp->kmif.read = sndread;
	sndp->kmif.setinst = setinst;

	return &sndp->kmif;
}

#undef CPS_SHIFT
#undef RENDERS
#undef LIN_BITS
#undef LOG_BITS
#undef LOG_LIN_BITS
#undef LOG_KEYOFF
