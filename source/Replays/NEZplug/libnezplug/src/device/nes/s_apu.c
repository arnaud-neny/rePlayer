#include "../../include/nezplug/nezplug.h"
#include "../../format/audiosys.h"
#include "../../normalize.h"
#include "../kmsnddev.h"
#include "../../format/handler.h"
#include "../../format/nsf6502.h"
#include "../../format/m_nsf.h"
#include "s_apu.h"

#ifndef NO_STDLIB
#include <time.h>
#include <math.h>
#endif

#include "../../common/divfix.h"

#define NES_BASECYCLES (21477270)

/* 31 - log2(NES_BASECYCLES/(12*MIN_FREQ)) > CPS_BITS  */
/* MIN_FREQ:11025 23.6 > CPS_BITS */
/* 32-12(max spd) > CPS_BITS */

#define CPS_BITS 11

#define SQUARE_RENDERS 6
#define TRIANGLE_RENDERS 6
#define NOISE_RENDERS 4
#define DPCM_RENDERS 4

#define SQ_VOL_BIT 10
#define TR_VOL ((1<<9)*4)
#define NOISE_VOL ((1<<8)*4)
#define DPCM_VOL ((1<<6)*5)


#define AMPTML_BITS 14
#define AMPTML_MAX (1 << (AMPTML_BITS))

#define VOL_SHIFT 0 /* 後期型 */

typedef struct {
	uint32_t counter;				/* length counter */
	uint8_t clock_disable;	/* length counter clock disable */
} APU_LENGTHCOUNTER;

typedef struct {
	uint8_t disable;			/* envelope decay disable */
	uint8_t counter;			/* envelope decay counter */
	uint8_t rate;				/* envelope decay rate */
	uint8_t timer;			/* envelope decay timer */
	uint8_t looping_enable;	/* envelope decay looping enable */
	uint8_t volume;			/* volume */
} APU_ENVELOPEDECAY;

typedef struct {
	uint8_t ch;				/* sweep channel */
	uint8_t active;			/* sweep active */
	uint8_t rate;				/* sweep rate */
	uint8_t timer;			/* sweep timer */
	uint8_t direction;		/* sweep direction */
	uint8_t shifter;			/* sweep shifter */
} APU_SWEEP;

typedef struct {
	APU_LENGTHCOUNTER lc;
	APU_ENVELOPEDECAY ed;
	APU_SWEEP sw;
	uint32_t mastervolume;
	uint32_t cps;		/* cycles per sample */
	uint32_t *cpf;	/* cycles per frame (240/192Hz) ($4017.bit7) */
	uint32_t fc;		/* frame counter; */
	uint32_t wl;		/* wave length */
	uint32_t pt;		/* programmable timer */
	uint32_t st;		/* wave step */
	uint32_t duty;		/* duty rate */
	uint32_t output;
	uint8_t fp;		/* frame position */
	uint8_t key;
	uint8_t mute;
	uint32_t ct;
} NESAPU_SQUARE;

typedef struct {
	uint32_t *cpf;			/* cycles per frame (240Hz fix) */
	uint32_t fc;			/* frame counter; */
	uint8_t load;			/* length counter load register */
	uint8_t start;		/* length counter start */
	uint8_t counter;		/* length counter */
	uint8_t startb;		/* length counter start */
	uint8_t counterb;		/* length counter */
	uint8_t tocount;		/* length counter go to count mode */
	uint8_t mode;			/* length counter mode load(0) count(1) */
	uint8_t clock_disable;	/* length counter clock disable */
} APU_LINEARCOUNTER;

typedef struct {
	APU_LENGTHCOUNTER lc;
	APU_LINEARCOUNTER li;
	uint32_t mastervolume;
	uint32_t cps;		/* cycles per sample */
	uint32_t *cpf;	/* cycles per frame (240/192Hz) ($4017.bit7) */
	uint32_t cpb180;	/* cycles per base/180 */
	uint32_t fc;		/* frame counter; */
	uint32_t b180c;	/* base/180 counter; */
	uint32_t wl;		/* wave length */
	uint32_t wlb;		/* wave length base*/
	uint32_t pt;		/* programmable timer */
	uint32_t st;		/* wave step */
	uint32_t output;
	uint8_t fp;		/* frame position; */
	uint8_t key;
	uint8_t mute;
	int32_t ct;
	uint8_t ct2;
	uint32_t dpcmout;
} NESAPU_TRIANGLE;

typedef struct {
	APU_LENGTHCOUNTER lc;
	APU_LINEARCOUNTER li;
	APU_ENVELOPEDECAY ed;
	uint32_t mastervolume;
	uint32_t cps;		/* cycles per sample */
	uint32_t *cpf;	/* cycles per frame (240/192Hz) ($4017.bit7) */
	uint32_t fc;		/* frame counter; */
	uint32_t wl;		/* wave length */
	uint32_t pt;		/* programmable timer */
	uint32_t rng;
	uint32_t rngcount;
	uint32_t output;
	uint8_t rngshort;
	uint8_t fp;		/* frame position; */
	uint8_t key;
	uint8_t mute;
	uint8_t ct;
	uint32_t dpcmout;
} NESAPU_NOISE;

typedef struct {
	uint32_t cps;		/* cycles per sample */
	uint32_t wl;		/* wave length */
	uint32_t pt;		/* programmable timer */
	uint32_t length;	/* bit length */
	uint32_t mastervolume;
	uint32_t adr;		/* current address */
	int32_t dacout;
	int32_t dacout0;

	uint8_t start_length;
	uint8_t start_adr;
	uint8_t loop_enable;
	uint8_t irq_enable;
	uint8_t irq_report;
	uint8_t input;	/* 8bit input buffer */
	uint8_t first;
	uint8_t dacbase;
	uint8_t key;
	uint8_t mute;

	uint32_t output;
	uint8_t ct;
	uint32_t dactbl[128];

} NESAPU_DPCM;

typedef struct {
	NESAPU_SQUARE square[2];
	NESAPU_TRIANGLE triangle;
	NESAPU_NOISE noise;
	NESAPU_DPCM dpcm;
	uint32_t cpf[4];	/* cycles per frame (240/192Hz) ($4017.bit7) */
	uint8_t regs[0x20];
} APUSOUND;


/* ------------------------- */
/*  NES INTERNAL SOUND(APU)  */
/* ------------------------- */

/* GBSOUND.TXT */
static const uint8_t apu_square_duty_table[][8] = 
{
	{0,0,0,1,0,0,0,0} , {0,0,0,1,1,0,0,0} , {0,0,0,1,1,1,1,0} , {1,1,1,0,0,1,1,1} ,
	{1,0,0,0,0,0,0,0} , {1,1,1,1,0,0,0,0} , {1,1,0,0,0,0,0,0} , {1,1,1,1,1,1,0,0}  //クソ互換機のDuty比のひっくり返ってるやつ 
};
//	{ {0,1,0,0,0,0,0,0} , {0,1,1,0,0,0,0,0} , {0,1,1,1,1,0,0,0} , {0,1,1,1,1,1,1,0} };

static const uint8_t apu_vbl_length_table[96] = {
	0x05, 0x7f, 0x0a, 0x01, 0x14, 0x02, 0x28, 0x03,
	0x50, 0x04, 0x1e, 0x05, 0x07, 0x06, 0x0d, 0x07,
	0x06, 0x08, 0x0c, 0x09, 0x18, 0x0a, 0x30, 0x0b,
	0x60, 0x0c, 0x24, 0x0d, 0x08, 0x0e, 0x10, 0x0f,
	0x54, 0x68, 0x69, 0x73, 0x20, 0x70, 0x72, 0x6f,
	0x67, 0x72, 0x61, 0x6d, 0x20, 0x75, 0x73, 0x65, 
	0x20, 0x4e, 0x45, 0x5a, 0x50, 0x6c, 0x75, 0x67,
	0x2b, 0x2b, 0x20, 0x69, 0x6e, 0x20, 0x46, 0x61, 
	0x6d, 0x69, 0x63, 0x6f, 0x6e, 0x2e, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x28, 0x6d, 0x65, 
	0x73, 0x73, 0x61, 0x67, 0x65, 0x20, 0x62, 0x79,
	0x20, 0x4f, 0x66, 0x66, 0x47, 0x61, 0x6f, 0x29

};

static const uint32_t apu_wavelength_converter_table[16] = {
	0x002, 0x004, 0x008, 0x010, 0x020, 0x030, 0x040, 0x050,
	0x065, 0x07f, 0x0be, 0x0fe, 0x17d, 0x1fc, 0x3f9, 0x7f2
};

static const uint32_t apu_spd_limit_table[8] =
{
	0x3FF, 0x555, 0x666, 0x71C, 
	0x787, 0x7C1, 0x7E0, 0x7F0,
};

static const uint32_t apu_dpcm_freq_table[16] =
{
	428, 380, 340, 320,
	286, 254, 226, 214,
	190, 160, 142, 128,
	106,  85,  72,  54,
};

#ifdef NO_STDLIB
static Inline uint32_t abs_int32(int32_t v) {
    int32_t const mask = v >> ((sizeof(int32_t) * CHAR_BIT) - 1);
    return (uint32_t)(v + mask) ^ mask;
}
#endif

__inline static void ApuLengthCounterStep(APU_LENGTHCOUNTER *lc)
{
	if (lc->counter && !lc->clock_disable) lc->counter--;
}

/*
・4008のMSBが0の場合に400Bに書くと、以後は4008に何の値を書いても無視される。
・4008のMSBが1の場合に400Bに書くと、以後4008に80で消音・81-FFで発声が可能。
　ただし、00-7Fの値を書いた場合、その瞬間に各種カウンタが有効・そのカウント値でカウント開始となり、
　それ以後の4008書き込みは、キーオフカウンタは有効無効以外無視となる。
・4008の値変更による消音・発声のタイミングは、三角波音長カウント時（つまり、4017-MSBが0の時は240Hz周期）。
・400B書き込みによるキーオンは、カウント周期が来たあとに発声される。
・周波数は書いて瞬時に反映される。そのため、8-10bit目をまたぐ周波数変更は、
　変更の合間の周波数の値によってはプチノイズがときどき出る。
*/
__inline static void ApuLinearCounterStep(APU_LINEARCOUNTER *li/*, uint32_t cps*/)
{
//	li->fc += cps;
//	while (li->fc > li->cpf[0])
//	{
//		li->fc -= li->cpf[0];
		if (li->start)
		{
			//li->start = 0;
			li->counter = li->load;
			li->start = li->clock_disable = li->tocount;
		}
		else{
			if(li->counter){
				li->counter--;
			}
		}
//		if (!li->tocount && li->counter)
//		{
//			li->start = 0;
//		}


		
//	}
}

__inline static void ApuEnvelopeDecayStep(APU_ENVELOPEDECAY *ed)
{
//	if (!ed->disable)
	if((ed->timer & 0x1f) == 0)
	{
		ed->timer = ed->rate;
		if (ed->counter || ed->looping_enable){
			ed->counter = (ed->counter - 1) & 0xF;
		}
	}else
		ed->timer--;
}

__inline static void ApuSweepStep(APU_SWEEP *sw, uint32_t *wl)
{
	if (sw->active && sw->shifter && --sw->timer > 7)
	{
		sw->timer = sw->rate;
		if (sw->direction)
		{
			if(!sw->ch){
				if (*wl > 0)*wl += ~(*wl >> sw->shifter);
			}else{
				*wl -= (*wl >> sw->shifter);
			}
		}
		else
		{
			*wl += (*wl >> sw->shifter);
//			if (*wl < 0x7ff && !sw->ch) *wl+=1;
		}
	}
}

static void NESAPUSoundSquareCount(NESAPU_SQUARE *ch){

	ch->fc += ch->cps;
	while (ch->fc >= ch->cpf[0])
	{
		ch->fc -= ch->cpf[0];
		if (!(ch->fp & 4)){
			if (!(ch->fp & 1)) ApuLengthCounterStep(&ch->lc);	/* 120Hz */
			if (!(ch->fp & 1)) ApuSweepStep(&ch->sw, &ch->wl);	/* 120Hz */
			ApuEnvelopeDecayStep(&ch->ed);	/* 240Hz */
		}
		ch->fp++;if(ch->fp >= 4+ch->cpf[2])ch->fp=0;
	}

}


static int32_t NESAPUSoundSquareRender(NESAPU_SQUARE *ch)
{
	int32_t outputbuf=0,count=0;
	NESAPUSoundSquareCount(ch);
	if (!ch->key || !ch->lc.counter)
	{
		return 0;
	}
	else
	{
		if (!ch->sw.direction && ch->wl > apu_spd_limit_table[ch->sw.shifter])
		{
#if 1
			return 0;
#endif
		}
		else if (ch->wl <= 7 || 0x7ff < ch->wl)
		{
#if 1
			return 0;
#endif
		}
		else
		{
			ch->pt += ch->cps << SQUARE_RENDERS;

			ch->output = (apu_square_duty_table[ch->duty][ch->st]) 
				? (ch->ed.disable ? ch->ed.volume : ch->ed.counter)<<1 : 0;
			ch->output <<= SQ_VOL_BIT; 
				while (ch->pt >= ((ch->wl + 1) << CPS_BITS))
				{
					outputbuf += ch->output;
					count++;

					ch->pt -= ((ch->wl + 1) << CPS_BITS);

					ch->ct++;
					if(ch->ct >= (1<<(SQUARE_RENDERS+1))){
						ch->ct = 0;
						ch->st = (ch->st + 1) & 0x7;

						ch->output = (apu_square_duty_table[ch->duty][ch->st]) 
							? (ch->ed.disable ? ch->ed.volume : ch->ed.counter)<<1 : 0;
						ch->output <<= SQ_VOL_BIT; 
					}
				}
		}
	}
	if (ch->mute) return 0;

	outputbuf += ch->output;
	count++;
	return outputbuf /count;
}

static void NESAPUSoundTriangleCount(NESAPU_TRIANGLE *ch)
{
	/*
	ch->b180c += ch->cps;
	if(ch->b180c >= ch->cpb180){
		ch->wl = ch->wlb;
		if (ch->li.startb == 0xc0){
			ch->li.counter = ch->li.counterb;
			ch->li.start = ch->li.startb = 0x80;
		}
		ch->b180c %= ch->cpb180;
	}
	*/
	ch->fc += ch->cps;
	//if (!(ch->fp & 4)){
	//}
	while (ch->fc >= ch->cpf[0])
	{
		ch->fc -= ch->cpf[0];
		if (!(ch->fp & 4)){
			if (!(ch->fp & 1)) ApuLengthCounterStep(&ch->lc);	/* 120Hz */
			ApuLinearCounterStep(&ch->li);
		}
		ch->fp++;if(ch->fp >= 4+ch->cpf[2])ch->fp=0;
	}

}
static int32_t NESAPUSoundTriangleRender(NESAPU_TRIANGLE *ch)
{
	int32_t outputbuf=0,count=0;
	//int32_t output;
	NESAPUSoundTriangleCount(ch);

	ch->output = (ch->st & 0x0f);
	if (ch->st & 0x10) ch->output = ch->output ^ 0xf ;
	ch->output *= TR_VOL;

	if (ch->key && (ch->li.clock_disable ? ch->li.load : ch->li.counter ) &&  ch->lc.counter) {
		/* 古いタイプ 
		ch->pt += ch->cps << TRIANGLE_RENDERS;
		if (ch->wl <= 4){
			ch->st += ch->pt / (((ch->wl + 1) << CPS_BITS)>>TRIANGLE_RENDERS);
			ch->pt %= ((ch->wl + 1) << CPS_BITS);
			ch->output = 8;
			ch->output *= TR_VOL;
		}else{
			while (ch->pt >= ((ch->wl + 1) << CPS_BITS))
			{
				outputbuf += ch->output;
				count++;

				ch->pt -= ((ch->wl + 1) << CPS_BITS);
				ch->ct++;
				if(ch->ct >= (1<<TRIANGLE_RENDERS)){
					ch->ct = 0;
					ch->st++;

					ch->output = (ch->st & 0x0f);
					if (ch->st & 0x10) ch->output = ch->output ^ 0xf ;
					ch->output *= TR_VOL;
				}
			}
		}
		*/
		//新しいタイプ。レンダー毎に一定の減算を行い、アンダーフロー時に周波数レジスタ値で
		//カウントリセットする方式。
		//9bit目をまたぐときのプチノイズの乗り方的に、たぶん実機の動作はこれかと。
		if(ch->wlb < 4){
			//周波数レジスタが極端に小さいとき。ここは適当で良いかぁ。
			/*//これちと重い
			ch->pt += ch->cps << TRIANGLE_RENDERS;

			ch->ct -= ch->pt / (1<<CPS_BITS);
			ch->pt %= (1<<CPS_BITS);

			while (ch->ct < 0)//カウンタがアンダーフローした場合
			{
				ch->ct += ch->wlb;
				ch->ct2++;

				if(ch->ct2 >= (1 << TRIANGLE_RENDERS)){
					ch->wlb = ch->wl +1;
					ch->ct += ch->wlb;
					ch->ct2 = 0;
					ch->st++;
				}
			}*/
			ch->wlb = ch->wl +1;

			ch->output = 8;
			ch->output *= TR_VOL;
		}else{
			ch->pt += ch->cps << TRIANGLE_RENDERS;

			ch->ct -= ch->pt / (1<<CPS_BITS);
			ch->pt %= (1<<CPS_BITS);

			while (ch->ct < 0)//カウンタがアンダーフローした場合
			{
				ch->ct += ch->wlb;
				ch->ct2++;

				outputbuf += ch->output;
				count++;

				if(ch->ct2 >= (1 << TRIANGLE_RENDERS)){
					ch->wlb = ch->wl +1;
					ch->ct2 = 0;
					ch->st++;

					ch->output = (ch->st & 0x0f);
					if (ch->st & 0x10) ch->output = ch->output ^ 0xf ;
					ch->output *= TR_VOL;
				}
			}
		}
	}
	if (ch->mute) return 0;
	outputbuf += ch->output;
	count++;

	outputbuf = outputbuf / count;
	return outputbuf;

}

static void NESAPUSoundNoiseCount(NESAPU_NOISE *ch){

	ch->fc += ch->cps;
	while (ch->fc >= ch->cpf[0])
	{
		ch->fc -= ch->cpf[0];
		if (!(ch->fp & 4)){
			if (!(ch->fp & 1)) ApuLengthCounterStep(&ch->lc);	/* 120Hz */
			ApuEnvelopeDecayStep(&ch->ed);						/* 240Hz */
		}
		ch->fp++;if(ch->fp >= 4+ch->cpf[2])ch->fp=0;
	}

}
static int32_t NESAPUSoundNoiseRender(NEZ_PLAY *pNezPlay, NESAPU_NOISE *ch)
{
	int32_t outputbuf=0,count=0;

	NESAPUSoundNoiseCount(ch);
	if (!ch->key || !ch->lc.counter){
		return 0;
	}
	if (!ch->wl) return 0;

	ch->output = (ch->rng & 1) * (ch->ed.disable ? ch->ed.volume : ch->ed.counter);
	ch->output *= NOISE_VOL;
	//クソ互換機は、やたらとノイズがでかい。
	if(pNezPlay->nes_config.nes2A03type==2)ch->output *= 2;

	ch->pt += ch->cps << NOISE_RENDERS;
	while (ch->pt >= ch->wl << (CPS_BITS + 1))
	{
		outputbuf += ch->output;
		count++;

		ch->pt -= ch->wl << (CPS_BITS + 1);

		/* 音質向上のため */
		ch->rngcount++;
		if( ch->rngcount >= (1<<NOISE_RENDERS)){
			ch->rngcount = 0;
			ch->rng >>= 1;
			ch->rng |= ((ch->rng ^ (ch->rng >> (ch->rngshort ? 6 : 1))) & 1) << 15;

			ch->output = (ch->rng & 1) * (ch->ed.disable ? ch->ed.volume : ch->ed.counter);
			ch->output *= NOISE_VOL;
			//クソ互換機は、やたらとノイズがでかい。
			if(pNezPlay->nes_config.nes2A03type==2)ch->output *= 2;
		}
	}
	outputbuf += ch->output;
	count++;
	if (ch->mute) return 0;

	outputbuf = outputbuf / count;
	return outputbuf;
}

__inline static void NESAPUSoundDpcmRead(NEZ_PLAY *pNezPlay, NESAPU_DPCM *ch)
{
	ch->input = (uint8_t)NES6502ReadDma(pNezPlay, ch->adr);
	if(++ch->adr > 0xffff)ch->adr = 0x8000;
	
}

static void NESAPUSoundDpcmStart(NEZ_PLAY *pNezPlay, NESAPU_DPCM *ch)
{
	ch->adr = 0xC000 + ((uint32_t)ch->start_adr << 6);
	ch->length = (((uint32_t)ch->start_length << 4) + 1) << 3;
	ch->irq_report = 0;
/*
	if (ch->irq_enable && !ch->loop_enable){
		//割り込みがかかる条件の場合
		NES6502SetIrqCount((NEZ_PLAY*)pNezPlay, ch->length * ch->wl);
	}
*/
	NESAPUSoundDpcmRead(pNezPlay, ch);
}

static int32_t NESAPUSoundDpcmRender(NEZ_PLAY *pNezPlay)
{
#define ch (&((APUSOUND*)((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->apu)->dpcm)
#define DPCM_OUT ch->dactbl[((ch->dacout<<1) + ch->dacout0)] * DPCM_VOL;

	int32_t outputbuf=0,count=0;

	ch->output = DPCM_OUT;
	if (ch->first)
	{
		ch->first = 0;
		ch->dacbase = (uint8_t)ch->dacout;
	}
	if (ch->key && ch->length)
	{

		ch->pt += ch->cps << DPCM_RENDERS;
		while (ch->pt >= ((ch->wl + 0) << CPS_BITS))
		{
			outputbuf += ch->output;
			count++;

			ch->pt -= ((ch->wl + 0) << CPS_BITS);
			ch->ct++;
			if(ch->ct >= (1<<DPCM_RENDERS)){
				ch->ct = 0;
				if (ch->length == 0) continue;
				if (ch->input & 1)
					ch->dacout += (ch->dacout < +0x3f);
				else
					ch->dacout -= (ch->dacout > 0);
				ch->input >>= 1;

				if (--ch->length == 0)
				{
					if (ch->loop_enable)
					{
						NESAPUSoundDpcmStart((NEZ_PLAY*)pNezPlay, ch);	/*loop */
					}
					else
					{
						if (ch->irq_enable)
						{
							NES6502Irq((NEZ_PLAY*)pNezPlay);	// irq gen
							ch->irq_report = 0x80;
						}
						
						ch->length = 0;
						ch->key = 0;
					}
				}
				else if ((ch->length & 7) == 0)
				{
					NESAPUSoundDpcmRead((NEZ_PLAY*)pNezPlay, ch);
				}
				ch->output = DPCM_OUT;
			}
		}
	}
	if (ch->mute) return 0;
	outputbuf += ch->output;
	count++;
	outputbuf /= count;
	return	outputbuf;
#undef ch
#undef DPCM_OUT
}

#define DAC_SQ_DOWN (1<<12)
#define DAC_SQ_BIT 32
#define DAC_TND_DOWN (1<<12)
#define DAC_TND_BIT 46

static int32_t APUSoundRender(NEZ_PLAY *pNezPlay)
{
	APUSOUND *apu = ((NSFNSF*)pNezPlay->nsf)->apu;
	int32_t accum = 0 , sqout = 0, tndout = 0;
	sqout += NESAPUSoundSquareRender(&apu->square[0]) * pNezPlay->chmask[NEZ_DEV_2A03_SQ1];
	sqout += NESAPUSoundSquareRender(&apu->square[1]) * pNezPlay->chmask[NEZ_DEV_2A03_SQ2];
	sqout >>= 1;
    if (pNezPlay->nes_config.realdac) {
#ifdef NO_STDLIB
		sqout = sqout * (DAC_SQ_DOWN - (abs_int32(sqout) / DAC_SQ_BIT)) / DAC_SQ_DOWN;
#else
		sqout = sqout * (DAC_SQ_DOWN - (abs(sqout) / DAC_SQ_BIT)) / DAC_SQ_DOWN;
#endif
    }
	accum += sqout * apu->square[0].mastervolume / 20/*20kΩ*/;
	tndout += NESAPUSoundDpcmRender(pNezPlay) * pNezPlay->chmask[NEZ_DEV_2A03_DPCM];
	tndout += NESAPUSoundTriangleRender(&apu->triangle) * pNezPlay->chmask[NEZ_DEV_2A03_TR];
	tndout += NESAPUSoundNoiseRender(pNezPlay,&apu->noise) * pNezPlay->chmask[NEZ_DEV_2A03_NOISE];
	tndout >>= 1;
    if (pNezPlay->nes_config.realdac) {
#ifdef NO_STDLIB
		tndout = tndout * (DAC_TND_DOWN - (abs_int32(tndout) / DAC_TND_BIT)) / DAC_TND_DOWN;
#else
		tndout = tndout * (DAC_TND_DOWN - (abs(tndout) / DAC_TND_BIT)) / DAC_TND_DOWN;
#endif
    }
	accum += tndout * apu->triangle.mastervolume / 12/*12kΩ*/;
	//accum = apu->amptbl[tndout >> (26 - AMPTML_BITS)];
	accum -= 0x60000;
	return accum * pNezPlay->nes_config.apu_volume / 8;
}

static const NEZ_NES_AUDIO_HANDLER s_apu_audio_handler[] = {
	{ 1, APUSoundRender, NULL, NULL }, 
	{ 0, 0, NULL, NULL }, 
};

static void APUSoundVolume(NEZ_PLAY *pNezPlay, uint32_t volume)
{
	APUSOUND *apu = ((NSFNSF*)pNezPlay->nsf)->apu;
	
	if(volume > 255) volume = 255;
	volume = 256 - volume;
	/* SND1 */
	apu->square[0].mastervolume = volume;
	apu->square[1].mastervolume = volume;

	/* SND2 */
	apu->triangle.mastervolume = volume;
	apu->noise.mastervolume = volume;
	apu->dpcm.mastervolume = volume;
}

static const NEZ_NES_VOLUME_HANDLER s_apu_volume_handler[] = {
	{ APUSoundVolume, NULL },
	{ 0, NULL }, 
};

static void APUSoundWrite(void *pNezPlay, uint32_t address, uint32_t value)
{
	APUSOUND *apu = ((NSFNSF*)((NEZ_PLAY *)pNezPlay)->nsf)->apu;
	if (0x4000 <= address && address <= 0x4017)
	{
		apu->regs[address - 0x4000] = (uint8_t)value;
		switch (address)
		{
			case 0x4000:	case 0x4004:
				{
					int ch = address >= 0x4004;
					if (value & 0x10)
						apu->square[ch].ed.volume = (uint8_t)(value & 0x0f);
					apu->square[ch].ed.rate   = (uint8_t)(value & 0x0f);
					apu->square[ch].ed.disable = (uint8_t)(value & 0x10);
					apu->square[ch].lc.clock_disable = (uint8_t)(value & 0x20);
					apu->square[ch].ed.looping_enable = (uint8_t)(value & 0x20);
					//クソ互換機のへんてこDuty比にするのを、ここでやる。
					if(((NEZ_PLAY *)pNezPlay)->nes_config.nes2A03type==2){
						apu->square[ch].duty = ((value >> 6) & 3)|4;
					}else{
						apu->square[ch].duty = (value >> 6) & 3;
					}
				}
				break;
			case 0x4001:	case 0x4005:
				{
					int ch = address >= 0x4004;
					apu->square[ch].sw.shifter = (uint8_t)(value & 7);
					apu->square[ch].sw.direction = (uint8_t)(value & 8);
					apu->square[ch].sw.rate = (uint8_t)((value >> 4) & 7);
					apu->square[ch].sw.active = (uint8_t)(value & 0x80);
					apu->square[ch].sw.timer = apu->square[ch].sw.rate;
				}
				break;
			case 0x4002:	case 0x4006:
				{
					int ch = address >= 0x4004;
					apu->square[ch].wl &= 0xffffff00;
					apu->square[ch].wl += value;
				}
				break;
			case 0x4003:	case 0x4007:
				{
					int ch = address >= 0x4004;
					//apu->square[ch].pt = 0;
#if 1
					apu->square[ch].st = 0x0;
					apu->square[ch].ct = 0x30;
#endif
					apu->square[ch].wl &= 0x0ff;
					apu->square[ch].wl += (value & 7) << 8;
					apu->square[ch].lc.counter = apu_vbl_length_table[value >> 3] * 2;
					apu->square[ch].ed.counter = 0xf;
					apu->square[ch].ed.timer = apu->square[ch].ed.rate + 1;
				}
				break;
			case 0x4008:
				apu->triangle.li.tocount = (uint8_t)(value & 0x80);
				apu->triangle.li.load = (uint8_t)(value & 0x7f);
				apu->triangle.lc.clock_disable = (uint8_t)(value & 0x80);
				/*
				if(apu->triangle.li.clock_disable){
					apu->triangle.li.counter = apu->triangle.li.load;
					apu->triangle.li.clock_disable = (uint8_t)(value & 0x80);
				}*/
				//if(apu->triangle.li.clock_disable)
				//	apu->triangle.li.mode=1;

				//apu->triangle.li.start = (uint8_t)(value & 0x80);
				break;
			case 0x400a:
				apu->triangle.wl &= 0x700;
				apu->triangle.wl += value;
//				if(apu->triangle.wlb == 0){
//					apu->triangle.wl = apu->triangle.wlb;
//				}
				//apu->triangle.wl = apu->triangle.wlb;
				//apu->triangle.b180c = 0;
				break;
			case 0x400b:
				apu->triangle.wl &= 0x0ff;
				apu->triangle.wl += (value & 7) << 8;
/*				if (!apu->triangle.lc.counter
				 || !apu->triangle.li.counter
				 || !apu->triangle.li.mode)
*///				apu->triangle.wl = apu->triangle.wlb;
				apu->triangle.lc.counter = apu_vbl_length_table[value >> 3] * 2;
				//apu->triangle.li.mode = apu->triangle.li.clock_disable;
				//apu->triangle.li.counter = apu->triangle.li.load;

				//apu->triangle.li.clock_disable = apu->triangle.li.tocount;
				apu->triangle.lc.clock_disable = apu->triangle.li.tocount;

				apu->triangle.li.start = 0x80;
				//apu->triangle.wl = apu->triangle.wlb;
				//apu->triangle.b180c = 0;
				break;
			case 0x400c:
				if (value & 0x10)
					apu->noise.ed.volume = (uint8_t)(value & 0x0f);
				else
				{
					apu->noise.ed.rate = (uint8_t)(value & 0x0f);
				}
				apu->noise.ed.disable = (uint8_t)(value & 0x10);
				apu->noise.lc.clock_disable = (uint8_t)(value & 0x20);
				apu->noise.ed.looping_enable = (uint8_t)(value & 0x20);
				break;
			case 0x400e:
				//初代2A03では、ノイズ15段階＆短周期無しなので、それをレジスタいじりで再現。
				if(((NEZ_PLAY *)pNezPlay)->nes_config.nes2A03type==0){
					apu->noise.wl = apu_wavelength_converter_table[(value & 0x0f)==0xf ? (0xe) : (value & 0x0f)];
					apu->noise.rngshort = 0;
				}else{
					apu->noise.wl = apu_wavelength_converter_table[value & 0x0f];
					apu->noise.rngshort = (uint8_t)(value & 0x80);
				}
				break;
			case 0x400f:
				// apu.noise.rng = 0x8000;
				apu->noise.ed.counter = 0xf;
				apu->noise.lc.counter = apu_vbl_length_table[value >> 3] * 2;
				apu->noise.ed.timer = apu->noise.ed.rate + 1;
				break;

			case 0x4010:
				apu->dpcm.wl = apu_dpcm_freq_table[value & 0x0F];
				apu->dpcm.loop_enable = (uint8_t)(value & 0x40);
				apu->dpcm.irq_enable = (uint8_t)(value & 0x80);
				if (!apu->dpcm.irq_enable) apu->dpcm.irq_report = 0;
				break;
			case 0x4011:
#if 0
				if (apu->dpcm.first && (value & 0x7f))
				{
					apu->dpcm.first = 0;
					apu->dpcm.dacbase = value & 0x7f;
				}
#endif
				apu->dpcm.dacout = (value >> 1) & 0x3f;
				apu->dpcm.dacout0 = value & 1;
				break;
			case 0x4012:
				apu->dpcm.start_adr = (uint8_t)value;
				break;
			case 0x4013:
				apu->dpcm.start_length = (uint8_t)value;
				break;

			case 0x4015:
				if (value & 1)
					apu->square[0].key = 1;
				else
				{
					apu->square[0].key = 0;
					apu->square[0].lc.counter = 0;
				}
				if (value & 2)
					apu->square[1].key = 1;
				else
				{
					apu->square[1].key = 0;
					apu->square[1].lc.counter = 0;
				}
				if (value & 4)
					apu->triangle.key = 1;
				else
				{
					apu->triangle.key = 0;
					apu->triangle.lc.counter = 0;
					apu->triangle.li.counter = 0;
					apu->triangle.li.start = 0;
				}
				if (value & 8)
					apu->noise.key = 1;
				else
				{
					apu->noise.key = 0;
					apu->noise.lc.counter = 0;
				}
				if (value & 16)
				{
					if (!apu->dpcm.key)
					{
						apu->dpcm.key = 1;
						NESAPUSoundDpcmStart(pNezPlay, &apu->dpcm);
					}
				}
				else
				{
					apu->dpcm.key = 0;
				}
				break;
			case 0x4017:
					apu->square[0].fp = 0;
					apu->square[1].fp = 0;
					apu->triangle.fp = 0;
					apu->noise.fp = 0;


					apu->square[0].fc = apu->cpf[0] - apu->square[0].cps;
					apu->square[1].fc = apu->cpf[0] - apu->square[1].cps;
					apu->triangle.fc = apu->cpf[0] - apu->triangle.cps;
					apu->noise.fc = apu->cpf[0] - apu->noise.cps;
					
					apu->triangle.b180c = apu->triangle.cpb180;

					NESAPUSoundSquareCount(&apu->square[0]);
					NESAPUSoundSquareCount(&apu->square[1]);
					NESAPUSoundTriangleCount(&apu->triangle);
					NESAPUSoundNoiseCount(&apu->noise);
				if (value & 0x80){
					//80フラグ
					apu->cpf[2] = 1;

				}else{
					apu->cpf[2] = 0;

				}
				break;
		}
	}
}

static const NES_WRITE_HANDLER s_apu_write_handler[] =
{
	{ 0x4000, 0x4017, APUSoundWrite, NULL },
	{ 0,      0,      0, NULL },
};

static uint32_t APUSoundRead(void *pNezPlay, uint32_t address)
{
	APUSOUND *apu = ((NSFNSF*)((NEZ_PLAY *)pNezPlay)->nsf)->apu;
	if (0x4015 == address)
	{
		int key = 0;
		if (apu->square[0].key && apu->square[0].lc.counter) key |= 1;
		if (apu->square[1].key && apu->square[1].lc.counter) key |= 2;
		if (apu->triangle.key && apu->triangle.lc.counter) key |= 4;
		if (apu->noise.key && apu->noise.lc.counter) key |= 8;
		if (apu->dpcm.length) key |= 16;
		key |= apu->dpcm.irq_report;
		key |= ((NSFNSF*)((NEZ_PLAY *)pNezPlay)->nsf)->vsyncirq_fg;
		apu->dpcm.irq_report = 0;
		((NSFNSF*)((NEZ_PLAY *)pNezPlay)->nsf)->vsyncirq_fg = 0;
		return key;
	}
	if (0x4000 <= address && address <= 0x4017)
		return 0x40; //$4000〜$4014を読んでも、全部$40が返ってくる（ファミベ調べ） 
	return 0xFF;
}

static const NES_READ_HANDLER s_apu_read_handler[] =
{
	{ 0x4000, 0x4017, APUSoundRead, NULL },
	{ 0,      0,      0, NULL },
};

static uint32_t GetNTSCPAL(NEZ_PLAY *pNezPlay)
{
	uint8_t *nsfhead = NSFGetHeader(pNezPlay);
	if (nsfhead[0x7a] & 1){
		return 13;
	}
	else
	{
		return 12;
	}

}

static void NESAPUSoundSquareReset(NEZ_PLAY *pNezPlay, NESAPU_SQUARE *ch)
{
	XMEMSET(ch, 0, sizeof(NESAPU_SQUARE));
	ch->cps = DivFix(NES_BASECYCLES, GetNTSCPAL(pNezPlay) * NESAudioFrequencyGet(pNezPlay), CPS_BITS);
}
static void NESAPUSoundTriangleReset(NEZ_PLAY *pNezPlay, NESAPU_TRIANGLE *ch)
{
	XMEMSET(ch, 0, sizeof(NESAPU_TRIANGLE));
	ch->cps = DivFix(NES_BASECYCLES, GetNTSCPAL(pNezPlay) * NESAudioFrequencyGet(pNezPlay), CPS_BITS);
	ch->st=0x8;
}
static void NESAPUSoundNoiseReset(NEZ_PLAY *pNezPlay, NESAPU_NOISE *ch)
{
	XMEMSET(ch, 0, sizeof(NESAPU_NOISE));
	ch->cps = DivFix(NES_BASECYCLES, GetNTSCPAL(pNezPlay) * NESAudioFrequencyGet(pNezPlay), CPS_BITS);

	//ノイズ音のランダム初期化
#ifndef NO_STDLIB
	if(pNezPlay->nes_config.noise_random_reset){
		srand(time(NULL) + clock());
		ch->rng = rand() + (rand()<<16);
	}else{
#endif
		ch->rng = 0x8000;
#ifndef NO_STDLIB
	}
#endif
	ch->rngcount = 0;
}


static void NESAPUSoundDpcmReset(NEZ_PLAY *pNezPlay, NESAPU_DPCM *ch)
{
	XMEMSET(ch, 0, sizeof(NESAPU_DPCM));
	ch->cps = DivFix(NES_BASECYCLES, 12 * NESAudioFrequencyGet(pNezPlay), CPS_BITS);
}

static void APUSoundReset(NEZ_PLAY *pNezPlay)
{
	APUSOUND *apu = ((NSFNSF*)pNezPlay->nsf)->apu;
	uint32_t i;
	NESAPUSoundSquareReset(pNezPlay, &apu->square[0]);
	NESAPUSoundSquareReset(pNezPlay, &apu->square[1]);
	NESAPUSoundTriangleReset(pNezPlay, &apu->triangle);
	NESAPUSoundNoiseReset(pNezPlay, &apu->noise);
	NESAPUSoundDpcmReset(pNezPlay, &apu->dpcm);
	apu->cpf[1] = DivFix(NES_BASECYCLES, 12 * 240, CPS_BITS);
//		apu->cpf[2] = DivFix(NES_BASECYCLES, 12 * 240 * 5 / 6, CPS_BITS);

	apu->cpf[0] = apu->cpf[1];
	apu->cpf[2] = 0;
	apu->square[0].sw.ch = 0;
	apu->square[1].sw.ch = 1;
	apu->square[0].cpf = apu->cpf;
	apu->square[1].cpf = apu->cpf;
	apu->triangle.cpf = apu->cpf;
	apu->noise.cpf = apu->cpf;
	apu->triangle.li.cpf = apu->cpf;

	apu->triangle.cpb180 = DivFix(NES_BASECYCLES, 12 * NES_BASECYCLES / 180/8, CPS_BITS);//暫定

	for (i = 0; i <= 0x17; i++)
	{
		APUSoundWrite(pNezPlay, 0x4000 + i, (i == 0x10) ? 0x10 : 0x00);
	}
	APUSoundWrite(pNezPlay, 0x4015, 0x0f);

	for (i = 0; i < 128; i++)
	{
		//DPCMのDAC部分は、0F->10、2F->30、3F->40、4F->50、6F->70 の出力差は、他の時と比べて1.5倍くらい幅がある。
		//さらに、1F->20、5F->60 の出力差は、他の時と比べて2倍くらい幅がある。
		apu->dpcm.dactbl[i] = i * 2 + (i >> 4) + ((i + 0x20) >> 5);
	}

#if 1
	apu->dpcm.first = 1;
#endif
}

static const NEZ_NES_RESET_HANDLER s_apu_reset_handler[] = {
	{ NES_RESET_SYS_NOMAL, APUSoundReset, NULL }, 
	{ 0,                   0, NULL }, 
};

static void APUSoundTerm(NEZ_PLAY *pNezPlay)
{
	APUSOUND *apu = ((NSFNSF*)pNezPlay->nsf)->apu;
	if (apu)
		XFREE(apu);
}

static const NEZ_NES_TERMINATE_HANDLER s_apu_terminate_handler[] = {
	{ APUSoundTerm, NULL }, 
	{ 0, NULL }, 
};

PROTECTED void APUSoundInstall(NEZ_PLAY *pNezPlay)
{
	APUSOUND *apu;
	apu = XMALLOC(sizeof(APUSOUND));
	if (!apu) return;
	XMEMSET(apu, 0, sizeof(APUSOUND));
	((NSFNSF*)pNezPlay->nsf)->apu = apu;

	NESAudioHandlerInstall(pNezPlay, s_apu_audio_handler);
	NESVolumeHandlerInstall(pNezPlay, s_apu_volume_handler);
	NESTerminateHandlerInstall(&pNezPlay->nth, s_apu_terminate_handler);
	NESReadHandlerInstall(pNezPlay, s_apu_read_handler);
	NESWriteHandlerInstall(pNezPlay, s_apu_write_handler);
	NESResetHandlerInstall(pNezPlay->nrh, s_apu_reset_handler);
}

#undef NES_BASECYCLES
#undef CPS_BITS
#undef SQUARE_RENDERS
#undef TRIANGLE_RENDERS
#undef NOISE_RENDERS
#undef DPCM_RENDERS
#undef SQ_VOL_BIT
#undef TR_VOL
#undef NOISE_VOL
#undef DPCM_VOL
#undef DPCM_VOL_DOWN
#undef AMPTML_BITS
#undef AMPTML_MAX
#undef VOL_SHIFT

