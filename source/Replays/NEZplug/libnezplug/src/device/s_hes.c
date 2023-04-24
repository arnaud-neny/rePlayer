#include "kmsnddev.h"
#include "s_hes.h"
#include "../common/divfix.h"
#include "logtable.h"

#include "../logtable/log_table.h"

#define CPS_SHIFT 16
#define RENDERS 5
#define NOISE_RENDERS 3
#define LIN_BITS 7
#define LOG_BITS 12
#define LOG_LIN_BITS 30
#define LOG_KEYOFF (32 << LOG_BITS)
#define LFO_BASE (0x10 << 8)

typedef struct {
	uint32_t cps;				/* cycles per sample */
	uint32_t pt;				/* programmable timer */
	uint32_t wl;				/* wave length */
	uint32_t npt;				/* noise programmable timer */
	uint32_t nvol;			/* noise volume */
	uint32_t rng;				/* random number generator (seed) */
	uint32_t dda;				/* direct D/A output */
	uint32_t tone[0x20];		/* tone waveform */
	uint8_t tonereg[0x20];	/* tone waveform regs*/
	uint32_t lfooutput;		/* lfo output */
	uint32_t nwl;				/* noise wave length */
	uint8_t regs[8 - 2];		/* registers per channel */
	uint8_t st;				/* wave step */
	uint8_t tonep;			/* tone waveform write pointer*/
	uint8_t mute;
	uint8_t edge;
	uint8_t edgeout;
	uint8_t rngold;
	int32_t output[2];
	uint8_t count;
} HES_WAVEMEMORY;

typedef struct {
	uint32_t cps;			/* cycles per sample */
	uint32_t pt;			/* lfo programmable timer */
	uint32_t wl;			/* lfo wave length */
	uint8_t tone[0x20];	/* lfo waveform */
	uint8_t tonep;		/* lfo waveform write pointer */
	uint8_t st;			/* lfo step */
	uint8_t update;
	uint8_t regs[2];
} HES_LFO;

typedef struct {
	KMIF_SOUND_DEVICE kmif;
	HES_WAVEMEMORY ch[6];
	HES_LFO lfo;
	HES_WAVEMEMORY *cur;
	struct {
		int32_t mastervolume;
		uint8_t sysregs[2];
	} common;
    uint8_t *chmask;
#if HES_TONE_DEBUG_OPTION_ENABLE
    uint8_t *tone_debug_option;
#endif
    uint8_t *noise_debug_option1;
    uint8_t *noise_debug_option2;
    int32_t *noise_debug_option3;
    int32_t *noise_debug_option4;
} HESSOUND;

#define V(a) ((((uint32_t)(0x1F - (a)) * (uint32_t)(1 << LOG_BITS) * (uint32_t)1000) / (uint32_t)3800) << 1)
static const uint32_t hes_voltbl[0x20] = {
	V(0x00), V(0x01), V(0x02),V(0x03),V(0x04), V(0x05), V(0x06),V(0x07),
	V(0x08), V(0x09), V(0x0A),V(0x0B),V(0x0C), V(0x0D), V(0x0E),V(0x0F),
	V(0x10), V(0x11), V(0x12),V(0x13),V(0x14), V(0x15), V(0x16),V(0x17),
	V(0x18), V(0x19), V(0x1A),V(0x1B),V(0x1C), V(0x1D), V(0x1E),V(0x1F),
};

#undef V

static void HESSoundWaveMemoryRender(HESSOUND *sndp, HES_WAVEMEMORY *ch, int32_t *p, uint8_t chn)
{
	uint32_t wl, output, lvol, rvol;
	int32_t outputbf[2]={0,0},count=0;
	if (ch->mute || !(ch->regs[4 - 2] & 0x80)) return;
	lvol = hes_voltbl[(sndp->common.sysregs[1] >> 3) & 0x1E];
	lvol +=	hes_voltbl[(ch->regs[5 - 2] >> 3) & 0x1E];
	lvol += hes_voltbl[ch->regs[4 - 2] & 0x1F];
	rvol = hes_voltbl[(sndp->common.sysregs[1] << 1) & 0x1E];
	rvol += hes_voltbl[(ch->regs[5 - 2] << 1) & 0x1E];
	rvol += hes_voltbl[ch->regs[4 - 2] & 0x1F];

	if (ch->regs[4 - 2] & 0x40)	/* DDA */
	{
		output = ch->dda;
		if(sndp->chmask[NEZ_DEV_HUC6230_CH1+chn]){
			p[0] += LogToLinear((LOG_TABLE *)&log_table_12_7_30, lvol + output + sndp->common.mastervolume, LOG_LIN_BITS - LIN_BITS - 17 - 1);
			p[1] += LogToLinear((LOG_TABLE *)&log_table_12_7_30, rvol + output + sndp->common.mastervolume, LOG_LIN_BITS - LIN_BITS - 17 - 1);
		}
	}
	else if (ch->regs[7 - 2] & 0x80)	/* NOISE */
	{
		ch->npt += ((ch->cps>>16) * ch->nwl)<<NOISE_RENDERS;
		//----------
		output = LinearToLog((LOG_TABLE *)&log_table_12_7_30,(ch->edge * 16)) + (1 << (LOG_BITS + 1)) + 1;
		ch->output[0] = LogToLinear((LOG_TABLE *)&log_table_12_7_30, lvol + output + sndp->common.mastervolume, LOG_LIN_BITS - LIN_BITS - 18 - 1);
		ch->output[1] = LogToLinear((LOG_TABLE *)&log_table_12_7_30, rvol + output + sndp->common.mastervolume, LOG_LIN_BITS - LIN_BITS - 18 - 1);
		//----------
		while (ch->npt > (0x1000 << 6))
		{
			outputbf[0] += ch->output[0];
			outputbf[1] += ch->output[1];
			count++;

			ch->npt = ch->npt - (0x1000 << 6);
			ch->count++;
			if(ch->count >= 1<<NOISE_RENDERS){
				ch->count = 0;

				ch->rng <<= 1;
				ch->rng |= ((ch->rng>>17)&1) ^ ((ch->rng>>14)&1);
				ch->edge = ((ch->rng>>17)&1);
				//----------
				output = LinearToLog((LOG_TABLE *)&log_table_12_7_30,(ch->edge * 16)) + (1 << (LOG_BITS + 1)) + 1;
				ch->output[0] = LogToLinear((LOG_TABLE *)&log_table_12_7_30, lvol + output + sndp->common.mastervolume, LOG_LIN_BITS - LIN_BITS - 18 - 1);
				ch->output[1] = LogToLinear((LOG_TABLE *)&log_table_12_7_30, rvol + output + sndp->common.mastervolume, LOG_LIN_BITS - LIN_BITS - 18 - 1);
				//----------
			}
		}
		outputbf[0] += ch->output[0];
		outputbf[1] += ch->output[1];
		count++;

		if(sndp->chmask[NEZ_DEV_HUC6230_CH1+chn]){
			p[0] += outputbf[0] / count;
			p[1] += outputbf[1] / count;
		}
		
	}
	else
	{
//		wl = ch->wl + ch->lfooutput;
		/* if (wl <= (LFO_BASE + 16)) wl = (LFO_BASE + 16); */
//		if (wl <= (LFO_BASE + 4)) return;
//		wl = (wl - LFO_BASE) << CPS_SHIFT;
		wl = ch->wl + ch->lfooutput;
		wl &= 0xfff;
		/* if (wl <= (LFO_BASE + 16)) wl = (LFO_BASE + 16); */
		if (wl <= 4) return;
		wl = wl << CPS_SHIFT;
		ch->pt += ch->cps << RENDERS;

		//----------
		output = ch->tone[ch->st & 0x1f];
		ch->output[0] = LogToLinear((LOG_TABLE *)&log_table_12_7_30, lvol + output + sndp->common.mastervolume, LOG_LIN_BITS - LIN_BITS - 17 - 1);
		ch->output[1] = LogToLinear((LOG_TABLE *)&log_table_12_7_30, rvol + output + sndp->common.mastervolume, LOG_LIN_BITS - LIN_BITS - 17 - 1);
		//----------
		while (ch->pt >= wl)
		{
			outputbf[0] += ch->output[0];
			outputbf[1] += ch->output[1];
			count++;

			ch->count++;
			ch->pt -= wl;
			if(ch->count >= 1<<RENDERS){
				ch->count = 0;

				ch->st++;
				//----------
				output = ch->tone[ch->st & 0x1f];
				ch->output[0] = LogToLinear((LOG_TABLE *)&log_table_12_7_30, lvol + output + sndp->common.mastervolume, LOG_LIN_BITS - LIN_BITS - 17 - 1);
				ch->output[1] = LogToLinear((LOG_TABLE *)&log_table_12_7_30, rvol + output + sndp->common.mastervolume, LOG_LIN_BITS - LIN_BITS - 17 - 1);
				//----------
			}
		}
		outputbf[0] += ch->output[0];
		outputbf[1] += ch->output[1];
		count++;
		if(sndp->chmask[NEZ_DEV_HUC6230_CH1+chn]){
			p[0] += outputbf[0] / count;
			p[1] += outputbf[1] / count;
		}
	}
}

static void HESSoundLfoStep(HESSOUND *sndp)
{
	if (sndp->lfo.update & 1)
	{
		sndp->lfo.update &= ~1;
		sndp->lfo.wl = sndp->ch[1].regs[2 - 2] + ((sndp->ch[1].regs[3 - 2] & 0xf) << 8);
		sndp->lfo.wl *= sndp->lfo.regs[0];
		sndp->lfo.wl <<= CPS_SHIFT;
	}
	if (sndp->lfo.wl <= (16 << CPS_SHIFT))
	{
//		sndp->ch[0].lfooutput = LFO_BASE;
		sndp->ch[0].lfooutput = 0;
		return;
	}
	sndp->lfo.pt += sndp->lfo.cps;
	while (sndp->lfo.pt >= sndp->lfo.wl)
	{
		sndp->lfo.pt -= sndp->lfo.wl;
		sndp->lfo.st++;
	}
	sndp->ch[0].lfooutput = sndp->lfo.tone[sndp->lfo.st & 0x1f];
	sndp->ch[0].lfooutput -= 0x10;
	switch (sndp->lfo.regs[1] & 3)
	{
		case 0:
//			sndp->ch[0].lfooutput = LFO_BASE;
			sndp->ch[0].lfooutput = 0;
			break;
		case 1:
//			sndp->ch[0].lfooutput += LFO_BASE - (0x10 << 0);
			break;
		case 2:
			sndp->ch[0].lfooutput <<= 4;
//			sndp->ch[0].lfooutput += LFO_BASE - (0x10 << 4);
//			sndp->ch[0].lfooutput += LFO_BASE - (0x10 << 4);
			break;
		case 3:
			sndp->ch[0].lfooutput <<= 8;
			/*sndp->ch[0].lfooutput += LFO_BASE - (0x10 << 8);*/
			break;
	}
}

static void hes_sndsynth(void *ctx, int32_t *p)
{
	HESSOUND *sndp = ctx;
	HESSoundWaveMemoryRender(sndp, &sndp->ch[5], p, 5);
	HESSoundWaveMemoryRender(sndp, &sndp->ch[4], p, 4);
	HESSoundWaveMemoryRender(sndp, &sndp->ch[3], p, 3);
	HESSoundWaveMemoryRender(sndp, &sndp->ch[2], p, 2);
	if (sndp->lfo.regs[1] & 0x80)
	{
		HESSoundWaveMemoryRender(sndp, &sndp->ch[1], p, 1);
//		sndp->ch[0].lfooutput = LFO_BASE;
		sndp->ch[0].lfooutput = 0;
	}
	else
	{
		HESSoundWaveMemoryRender(sndp, &sndp->ch[1], p, 1);
		HESSoundLfoStep(sndp);
	}
	HESSoundWaveMemoryRender(sndp, &sndp->ch[0], p, 0);
}

static void HESSoundChReset(HESSOUND *sndp, HES_WAVEMEMORY *ch, uint32_t clock, uint32_t freq)
{
    (void)sndp;
	int i;
	XMEMSET(ch, 0, sizeof(HES_WAVEMEMORY));
	ch->cps = DivFix(clock, 6 * freq, CPS_SHIFT);
	ch->nvol = LinearToLog((LOG_TABLE *)&log_table_12_7_30, 10);
	ch->dda = LOG_KEYOFF;
	ch->rng = 1;
	ch->edge = 0;
	ch->lfooutput = LFO_BASE;
	for (i = 0; i < 0x20; i++) ch->tone[i] = LOG_KEYOFF;
}

static void HESSoundLfoReset(HES_LFO *ch, uint32_t clock, uint32_t freq)
{
	XMEMSET(ch, 0, sizeof(HES_LFO));
	ch->cps = DivFix(clock, 6 * freq, CPS_SHIFT);
	/* ch->regs[1] = 0x80; */
}


static void hes_sndreset(void *ctx, uint32_t clock, uint32_t freq)
{
	HESSOUND *sndp = ctx;
	uint32_t ch;
	XMEMSET(&sndp->common, 0, sizeof(sndp->common));
	sndp->cur = sndp->ch;
	sndp->common.sysregs[1] = 0xFF;
	for (ch = 0; ch < 6; ch++) HESSoundChReset(sndp, &sndp->ch[ch], clock, freq);
	HESSoundLfoReset(&sndp->lfo, clock, freq);
}

static void hes_sndwrite(void *ctx, uint32_t a, uint32_t v)
{
	HESSOUND *sndp = ctx;
	switch (a & 0xF)
	{
		case 0:	// register select
			sndp->common.sysregs[0] = v & 7;
			if (sndp->common.sysregs[0] <= 5)
				sndp->cur = &sndp->ch[sndp->common.sysregs[0]];
			else
				sndp->cur = 0;
			break;
		case 1:	// main volume
			sndp->common.sysregs[1] = v;
			break;
		case 2:	// frequency low
		case 3:	// frequency high
			if (sndp->cur)
			{
				sndp->cur->regs[a - 2] = v;
				sndp->cur->wl = sndp->cur->regs[2 - 2] + ((sndp->cur->regs[3 - 2] & 0xf) << 8);
				if (sndp->cur == &sndp->ch[1]) sndp->lfo.update |= 1;
			}
			break;
		case 4:	// ON, DDA, AL
/*			if (sndp->cur && !(v & 0x80)) {
					sndp->cur->st = 0;
			}
*/		case 5:	// LAL, RAL
			if (sndp->cur) sndp->cur->regs[a - 2] = v;
			break;
		case 6:	// wave data
			if (sndp->cur)
			{
				uint32_t tone;
				int32_t data = v & 0x1f;
				sndp->cur->regs[6 - 2] = v;
#if HES_TONE_DEBUG_OPTION_ENABLE
				switch (sndp->tone_debug_option[0])
				{
					default:
					case 0:
						tone = LinearToLog((LOG_TABLE *)&log_table_12_7_30, data - 0x10) + (1 << (LOG_BITS + 1));
						//tone =data - 0x10;
						break;
					case 1:
						tone = hes_voltbl2[data];
						break;
					case 2:
						tone = LinearToLog((LOG_TABLE *)&log_table_12_7_30, data) + (1 << (LOG_BITS + 1));
						break;
				}
#else
				tone = LinearToLog((LOG_TABLE *)&log_table_12_7_30, -data) + (1 << (LOG_BITS + 1));
#endif
				if (sndp->cur->regs[4 - 2] & 0x40)
				{
					sndp->cur->dda = tone;
#if 1
					if (sndp->cur == &sndp->ch[1]) sndp->lfo.tonep = 0x0;
					sndp->cur->tonep = 0;
#endif
					//sndp->cur->st = 0;
				}
				if ((sndp->cur->regs[4 - 2] & 0x80) == 0)
				{
					sndp->cur->dda = LOG_KEYOFF;
					sndp->cur->st = 0;
					if (sndp->cur == &sndp->ch[1])
					{
						//sndp->lfo.tone[sndp->lfo.tonep] = data ^ 0x10;
						sndp->lfo.tone[sndp->lfo.tonep] = data;
						if (++sndp->lfo.tonep == 0x20) sndp->lfo.tonep = 0;
					}
					sndp->cur->tone[sndp->cur->tonep] = tone;
					sndp->cur->tonereg[sndp->cur->tonep] = data;
					if (++sndp->cur->tonep == 0x20) sndp->cur->tonep = 0;
				}
			}
			break;
		case 7:	// noise on, noise frq
			if (sndp->cur && sndp->common.sysregs[0] >= 4)
			{
				uint32_t nwl;
				sndp->cur->regs[7 - 2] = v;
				switch (sndp->noise_debug_option1[0])
				{
				case 1:
					/* v0.9.3beta7 old linear frequency */
					/* HES_noise_debug_option1=1 */
					/* sndp->noise_debug_option2[0]=HES_noise_debug_option(default:5) */
					/* sndp->noise_debug_option3[0]=512 */
					/* HES_noise_debug_option5=0 */
					nwl = sndp->noise_debug_option4[0] << (CPS_SHIFT - 10);
					nwl += (((v & 0x1f) << 9) + sndp->noise_debug_option3[0]) << (CPS_SHIFT - 9 + sndp->noise_debug_option2[0]);
					break;
				case 2:
					/* linear frequency */
					nwl = sndp->noise_debug_option4[0] << (CPS_SHIFT - 10);
					nwl += (v & 0x1f) << (CPS_SHIFT - 10 + sndp->noise_debug_option2[0] - sndp->noise_debug_option3[0]);
					break;
				default:
				case 3:
					/* positive logarithmic frequency */
					nwl = sndp->noise_debug_option4[0] << (CPS_SHIFT - 10);
					nwl -= LogToLinear((LOG_TABLE *)&log_table_12_7_30, ((0 & 0x1f) ^ 0x1f) << (LOG_BITS - sndp->noise_debug_option3[0] + 1), LOG_LIN_BITS - CPS_SHIFT - sndp->noise_debug_option2[0]);
					nwl += LogToLinear((LOG_TABLE *)&log_table_12_7_30, ((v & 0x1f) ^ 0x1f) << (LOG_BITS - sndp->noise_debug_option3[0] + 1), LOG_LIN_BITS - CPS_SHIFT - sndp->noise_debug_option2[0]);
					break;
				case 4:
					/* negative logarithmic frequency */
					nwl = sndp->noise_debug_option4[0] << (CPS_SHIFT - 10);
					nwl += LogToLinear((LOG_TABLE *)&log_table_12_7_30, ((0 & 0x1f) ^ 0x00) << (LOG_BITS - sndp->noise_debug_option3[0] + 1), LOG_LIN_BITS - CPS_SHIFT - sndp->noise_debug_option2[0]);
					nwl -= LogToLinear((LOG_TABLE *)&log_table_12_7_30, ((v & 0x1f) ^ 0x00) << (LOG_BITS - sndp->noise_debug_option3[0] + 1), LOG_LIN_BITS - CPS_SHIFT - sndp->noise_debug_option2[0]);
					break;
				case 5:
					/* positive logarithmic frequency (reverse) */
					nwl = sndp->noise_debug_option4[0] << (CPS_SHIFT - 10);
					nwl -= LogToLinear((LOG_TABLE *)&log_table_12_7_30, ((0 & 0x1f) ^ 0x1f) << (LOG_BITS - sndp->noise_debug_option3[0] + 1), LOG_LIN_BITS - CPS_SHIFT - sndp->noise_debug_option2[0]);
					nwl += LogToLinear((LOG_TABLE *)&log_table_12_7_30, ((v & 0x1f) ^ 0x00) << (LOG_BITS - sndp->noise_debug_option3[0] + 1), LOG_LIN_BITS - CPS_SHIFT - sndp->noise_debug_option2[0]);
					break;
				case 6:
					/* negative logarithmic frequency (reverse) */
					nwl = sndp->noise_debug_option4[0] << (CPS_SHIFT - 10);
					nwl += LogToLinear((LOG_TABLE *)&log_table_12_7_30, ((0 & 0x1f) ^ 0x00) << (LOG_BITS - sndp->noise_debug_option3[0] + 1), LOG_LIN_BITS - CPS_SHIFT - sndp->noise_debug_option2[0]);
					nwl -= LogToLinear((LOG_TABLE *)&log_table_12_7_30, (((v & 0x1f) ^ 0x1f)+1) << (LOG_BITS - sndp->noise_debug_option3[0] + 1), LOG_LIN_BITS - CPS_SHIFT - sndp->noise_debug_option2[0]);
					break;
				case 7:
					nwl = sndp->noise_debug_option4[0] << (CPS_SHIFT - 10);
					nwl += LogToLinear((LOG_TABLE *)&log_table_12_7_30, ((v & 0x1f) ^ 0x1f) << (LOG_BITS - sndp->noise_debug_option3[0] + 1), LOG_LIN_BITS - CPS_SHIFT - sndp->noise_debug_option2[0]);
					break;
				case 8:
					nwl = sndp->noise_debug_option4[0];
					nwl += LogToLinear((LOG_TABLE *)&log_table_12_7_30, (v & 0x1f) << (LOG_BITS - sndp->noise_debug_option3[0] + 1), LOG_LIN_BITS - CPS_SHIFT - sndp->noise_debug_option2[0]);
					break;
				case 9:
					if(0x1f - (v & 0x1f))
						nwl = 0x1000 / ((0x1f - (v & 0x1f))) ;
					else
						nwl = 0x4000 ;
					break;
				}
				sndp->cur->npt = 0;
				sndp->cur->nwl = nwl;
			}
			break;
		case 8:	// LFO frequency
			sndp->lfo.regs[a - 8] = v;
			sndp->lfo.update |= 1;
			break;
		case 9:	// LFO on, LFO control
			sndp->lfo.regs[a - 8] = v;
			if(v&0x80){
				//LFO Reset
				sndp->lfo.st = sndp->lfo.pt = 0;
			}
			break;
	}
}

static uint32_t hes_sndread(void *ctx, uint32_t a)
{
	HESSOUND *sndp = ctx;
	a &= 0xF;
	switch (a & 0xF)
	{
		case 0:	case 1:
			return sndp->common.sysregs[a];
		case 2:	case 3:	case 4:	case 5:	case 6:	case 7:
			if (sndp->cur) return sndp->cur->regs[a - 2];
			return 0;
		case 8:	case 9:
			return sndp->lfo.regs[a - 8];
	}
	return 0;
}

static void hes_sndvolume(void *ctx, int32_t volume)
{
	HESSOUND *sndp = ctx;
	volume = (volume << (LOG_BITS - 8)) << 1;
	sndp->common.mastervolume = volume;
}

static void hes_sndrelease(void *ctx)
{
	HESSOUND *sndp = ctx;
	if (sndp) {
		XFREE(sndp);
	}
}

static void hes_setinst(void *ctx, uint32_t n, void *p, uint32_t l)
{
    (void)ctx;
    (void)n;
    (void)p;
    (void)l;

}

PROTECTED KMIF_SOUND_DEVICE *HESSoundAlloc(NEZ_PLAY *pNezPlay)
{
	HESSOUND *sndp;
	sndp = XMALLOC(sizeof(HESSOUND));
	if (!sndp) return 0;
	XMEMSET(sndp, 0, sizeof(HESSOUND));

    sndp->chmask = pNezPlay->chmask;
	sndp->kmif.ctx = sndp;
	sndp->kmif.release = hes_sndrelease;
	sndp->kmif.reset = hes_sndreset;
	sndp->kmif.synth = hes_sndsynth;
	sndp->kmif.volume = hes_sndvolume;
	sndp->kmif.write = hes_sndwrite;
	sndp->kmif.read = hes_sndread;
	sndp->kmif.setinst = hes_setinst;
#if HES_TONE_DEBUG_OPTION_ENABLE
    sndp->tone_debug_option = &pNezPlay->hes_config.tone_debug_option;
#endif
    sndp->noise_debug_option1 = &pNezPlay->hes_config.noise_debug_option1;
    sndp->noise_debug_option2 = &pNezPlay->hes_config.noise_debug_option2;
    sndp->noise_debug_option3 = &pNezPlay->hes_config.noise_debug_option3;
    sndp->noise_debug_option4 = &pNezPlay->hes_config.noise_debug_option4;

	return &sndp->kmif;
}

#undef CPS_SHIFT
#undef RENDERS
#undef NOISE_RENDERS
#undef LOG_KEYOFF
#undef LFO_BASE
#undef LOG_BITS
#undef LIN_BITS
