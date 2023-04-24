#include "kmsnddev.h"
#include "../common/divfix.h"
#include "logtable.h"
#include "s_sng.h"

#include "../logtable/log_table.h"

#define CPS_SHIFT 18
#define LIN_BITS 7
#define LOG_BITS 12
#define LOG_LIN_BITS 30
#define LOG_KEYOFF (31 << LOG_BITS)

#define FB_WNOISE   0xc000
#define FB_PNOISE   0x8000

#define SN76489AN_PRESET 0x0001
//#define SG76489_PRESET 0x0F35
#define SG76489_PRESET 0x0001

#define RENDERS 7
#define NOISE_RENDERS 2

typedef struct {
	uint32_t cycles;
	uint32_t spd;
	uint32_t vol;
	uint8_t adr;
	uint8_t mute;
	uint8_t pad4[2];
	uint32_t count;
	uint32_t output;
} SNG_SQUARE;

typedef struct {
	uint32_t cycles;
	uint32_t spd;
	uint32_t vol;
	uint32_t rng;
	uint32_t fb;
	uint8_t step1;
	uint8_t step2;
	uint8_t mode;
	uint8_t mute;
	uint8_t pad4[2];
	uint8_t count;
	uint32_t output;
} SNG_NOISE;

typedef struct {
	KMIF_SOUND_DEVICE kmif;
	SNG_SQUARE square[3];
	SNG_NOISE noise;
	struct {
		uint32_t cps;
		uint32_t ncps;
		int32_t mastervolume;
		uint8_t first;
		uint8_t ggs;
	} common;
	uint8_t type;
	uint8_t regs[0x11];
    uint8_t *chmask;
} SNGSOUND;

#define V(a) (((a * (1 << LOG_BITS)) / 3) << 1)
static const uint32_t sng_voltbl[16] = {
	V(0x0), V(0x1), V(0x2),V(0x3),V(0x4), V(0x5), V(0x6),V(0x7),
	V(0x8), V(0x9), V(0xA),V(0xB),V(0xC), V(0xD), V(0xE),LOG_KEYOFF
};
#undef V

__inline static int32_t SNGSoundSquareSynth(SNGSOUND *sndp, SNG_SQUARE *ch)
{
	int32_t outputbuf=0,count=0;
	if (ch->spd < (0x4 << CPS_SHIFT))
	{
		return LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->vol + sndp->common.mastervolume, LOG_LIN_BITS - 21);
	}
	ch->cycles += sndp->common.cps<<RENDERS;
	ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->vol + sndp->common.mastervolume, LOG_LIN_BITS - 21);
	ch->output *= !(ch->adr & 1);
	while (ch->cycles >= ch->spd)
	{
		outputbuf += ch->output;
		count++;

		ch->cycles -= ch->spd;
		ch->count++;
		if(ch->count >= 1<<RENDERS){
			ch->count = 0;
			ch->adr++;
			ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->vol + sndp->common.mastervolume, LOG_LIN_BITS - 21);
			ch->output *= !(ch->adr & 1);
		}
	}
	outputbuf += ch->output;
	count++;
	if (ch->mute) return 0;
	return outputbuf / count;
}

__inline static int32_t SNGSoundNoiseSynth(SNGSOUND *sndp, SNG_NOISE *ch)
{
	int32_t outputbuf=0,count=0;
	//if (ch->spd < (0x1 << (CPS_SHIFT - 1))) return 0;
	ch->cycles += (sndp->common.ncps >> 1) <<NOISE_RENDERS;
	ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->vol + sndp->common.mastervolume, LOG_LIN_BITS - 21);
	ch->output *= !(ch->rng & 1);
	while (ch->cycles >= ch->spd)
	{
		outputbuf += ch->output;
		count++;

		ch->cycles -= ch->spd;
		ch->count++;
		if(ch->count >= 1<<NOISE_RENDERS){
			ch->count = 0;
			if (ch->rng & 1) ch->rng ^= ch->fb;
			//ch->rng += ch->rng + (((ch->rng >> ch->step1)/* ^ (ch->rng >> ch->step2)*/) & 1);
			ch->rng >>= 1;

			ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->vol + sndp->common.mastervolume, LOG_LIN_BITS - 21);
			ch->output *= !(ch->rng & 1);
		}
	}
	outputbuf += ch->output;
	count++;
	if (ch->mute) return 0;
	return outputbuf / count;
}

static void SNGSoundSquareReset(SNG_SQUARE *ch)
{
	XMEMSET(ch, 0, sizeof(SNG_SQUARE));
	ch->vol = LOG_KEYOFF;
}

static void SNGSoundNoiseReset(SNG_NOISE *ch)
{
	XMEMSET(ch, 0, sizeof(SNG_NOISE));
	ch->vol = LOG_KEYOFF;
	//ch->rng = sndp->type == SNG_TYPE_SN76496 ? SN76489AN_PRESET : SG76489_PRESET;
}


static void sng_sndsynth(void *ctx, int32_t *p)
{
	SNGSOUND *sndp = (SNGSOUND*)ctx;
	uint32_t ch;
	int32_t accum = 0;
	for (ch = 0; ch < 3; ch++)
	{
		accum = SNGSoundSquareSynth(sndp, &sndp->square[ch]);
		if (sndp->chmask[NEZ_DEV_SN76489_SQ1 + ch]){
			if ((sndp->common.ggs >> ch) & 0x10) p[0] += accum;
			if ((sndp->common.ggs >> ch) & 0x01) p[1] += accum;
		}
	}
	accum = SNGSoundNoiseSynth(sndp, &sndp->noise) * sndp->chmask[NEZ_DEV_SN76489_NOISE];
	if (sndp->common.ggs & 0x80) p[0] += accum;
	if (sndp->common.ggs & 0x08) p[1] += accum;
}

static void sng_sndvolume(void *ctx, int32_t volume)
{
	SNGSOUND *sndp = (SNGSOUND*)ctx;
	volume = (volume << (LOG_BITS - 8)) << 1;
	sndp->common.mastervolume = volume;
}

static uint32_t sng_sndread(void *ctx, uint32_t a)
{
    (void)ctx;
    (void)a;
	return 0;
}

static void sng_sndwrite(void *ctx, uint32_t a, uint32_t v)
{
	SNGSOUND *sndp = (SNGSOUND*)ctx;
	if ((a & 1) && sndp->type  == SNG_TYPE_GAMEGEAR)
	{
		if (sndp->type == SNG_TYPE_GAMEGEAR) sndp->common.ggs = v;
		sndp->regs[0x10] = v;
	}
	else if (sndp->common.first)
	{
		uint32_t ch = (sndp->common.first >> 5) & 3;
		if (sndp->type == SNG_TYPE_SN76489AN) {
			//0x000が一番低く、0x001が一番高い。
			sndp->square[ch].spd = (((((v & 0x3F) << 4) + (sndp->common.first & 0xF)) + 0x3ff)&0x3ff) +1;
		} else {
			sndp->square[ch].spd = (((v & 0x3F) << 4) + (sndp->common.first & 0xF));
		}
		sndp->regs[ch*2]=sndp->square[ch].spd&0xff;
		sndp->regs[ch*2+1]=(sndp->square[ch].spd>>8) & 0x03;

		sndp->square[ch].spd <<= CPS_SHIFT;
		if (ch == 2 && sndp->noise.mode == 3)
		{
			sndp->noise.spd = ((sndp->square[2].spd ? sndp->square[2].spd : (1<<CPS_SHIFT)) & (0x7ff<<(CPS_SHIFT)));
		}
		sndp->common.first = 0;
	}
	else
	{
		uint32_t ch;
		if(v >= 0x80)sndp->regs[(v & 0xF0)>>4]=v;
		switch (v & 0xF0)
		{
			case 0x80:	case 0xA0:	case 0xC0:
				sndp->common.first = v;
				break;
			case 0x90:	case 0xB0:	case 0xD0:
				ch = (v & 0x60) >> 5;
				sndp->square[ch].vol = sng_voltbl[v & 0xF];
				break;
			case 0xE0:
				//手持ちのSN76489ANが、ここに書いたらリセットしてたので
				sndp->noise.rng = sndp->type == SNG_TYPE_SN76489AN ? SN76489AN_PRESET : SG76489_PRESET;
				sndp->noise.mode = v & 0x3;
				sndp->noise.fb = (v & 4) ? FB_WNOISE : FB_PNOISE;
				//sndp->noise.step1 = (v & 4) ? (14) : (3);
				//sndp->noise.step2 = (v & 4) ? (13) : (3);

				if (sndp->noise.mode == 3)
					sndp->noise.spd = ((sndp->square[2].spd ? sndp->square[2].spd : (1<<CPS_SHIFT)) & (0x7ff<<(CPS_SHIFT)));
				else
					sndp->noise.spd = 1 << (4 + sndp->noise.mode + CPS_SHIFT);
				break;
			case 0xF0:
				sndp->noise.vol = sng_voltbl[v & 0xF];
				break;
		}
	}

}

static void sng_sndreset(void *ctx, uint32_t clock, uint32_t freq)
{
	SNGSOUND *sndp = (SNGSOUND*)ctx;
	XMEMSET(&sndp->common, 0, sizeof(sndp->common));
	sndp->common.cps = DivFix(clock, 16 * freq, CPS_SHIFT);
	sndp->common.ncps = DivFix(clock, (sndp->type == SNG_TYPE_SN76489AN ? 16 : 17) * freq, CPS_SHIFT);
	sndp->common.ggs = 0xff;
	sndp->regs[0x10] = sndp->common.ggs;
	SNGSoundSquareReset(&sndp->square[0]);
	SNGSoundSquareReset(&sndp->square[1]);
	SNGSoundSquareReset(&sndp->square[2]);
	SNGSoundNoiseReset(&sndp->noise);
	sng_sndwrite(sndp, 0, 0xE0);
	sng_sndwrite(sndp, 0, 0x9F);
	sng_sndwrite(sndp, 0, 0xBF);
	sng_sndwrite(sndp, 0, 0xDF);
	sng_sndwrite(sndp, 0, 0xFF);
	sng_sndwrite(sndp, 0, 0x80);
	sng_sndwrite(sndp, 0, 0x00);
	sng_sndwrite(sndp, 0, 0xA0);
	sng_sndwrite(sndp, 0, 0x00);
	sng_sndwrite(sndp, 0, 0xC0);
	sng_sndwrite(sndp, 0, 0x00);
}

static void sng_sndrelease(void *ctx)
{
	SNGSOUND *sndp = (SNGSOUND*)ctx;
	if (sndp) {
		XFREE(sndp);
	}
}

static void sng_setinst(void *ctx, uint32_t n, void *p, uint32_t l)
{
    (void)ctx;
    (void)n;
    (void)p;
    (void)l;
}

PROTECTED KMIF_SOUND_DEVICE *SNGSoundAlloc(NEZ_PLAY *pNezPlay, uint32_t sng_type)
{
	SNGSOUND *sndp;
	sndp = XMALLOC(sizeof(SNGSOUND));
	if (!sndp) return 0;
	XMEMSET(sndp, 0, sizeof(SNGSOUND));

    sndp->chmask = pNezPlay->chmask;
	sndp->type = sng_type;
	sndp->kmif.ctx = sndp;
	sndp->kmif.release = sng_sndrelease;
	sndp->kmif.reset = sng_sndreset;
	sndp->kmif.synth = sng_sndsynth;
	sndp->kmif.volume = sng_sndvolume;
	sndp->kmif.write = sng_sndwrite;
	sndp->kmif.read = sng_sndread;
	sndp->kmif.setinst = sng_setinst;

	return &sndp->kmif;
}

#undef CPS_SHIFT
#undef LIN_BITS
#undef LOG_BITS
#undef LOG_LIN_BITS
#undef LOG_KEYOFF
#undef FB_WNOISE
#undef FB_PNOISE
#undef SN76489AN_PRESET
#undef SG76489_PRESET
#undef RENDERS
#undef NOISE_RENDERS
