#include "kmsnddev.h"
#include "../common/divfix.h"
#include "logtable.h"
#include "s_dmg.h"

#include "../logtable/log_table.h"

#define CPS_BITS 11
#define CPFWM_BITS 4

#define NOISE_EDGE 1
#define WAVETABLE_REAL_RW 1 //FIFA Soccer '97のドライバでおかしくなるため、今はまだ無効

#define RESET_MODE 0

#define SQ_RENDERS 6
#define WM_RENDERS 5
#define KAERUNOISE_BITS 2

#define LIN_BITS 7
#define LOG_BITS 12
#define LOG_LIN_BITS 30

/* -------------------- */
/*  DMG INTERNAL SOUND  */
/* -------------------- */

typedef struct {
	uint32_t initial_counter;	/* length initial counter */
	uint32_t counter;			/* length counter */
	uint8_t enable;			/* length counter clock disable */
} LENGTHCOUNTER;
typedef struct {
	uint8_t rate;				/* envelope rate */
	uint8_t direction;		/* sweep direction */
	uint8_t timer;			/* envelope timer */
	uint8_t volume;			/* current volume */
	uint8_t initial_volume;	/* initial volume */
	uint8_t zombie_direction;	/* sweep direction (zombie mode stock)*/
	uint8_t initial_rate;		/* initial envelope rate */
} DMG_ENVELOPEDECAY;
typedef struct {
	uint8_t rate;			/* sweep rate */
	uint8_t direction;	/* sweep direction */
	uint8_t timer;		/* sweep timer */
	uint8_t shifter;		/* sweep shifter */
	uint32_t wl;			/* wave length */
} DMG_SWEEP;
typedef struct {
	LENGTHCOUNTER lc;
	DMG_ENVELOPEDECAY ed;
	DMG_SWEEP sw;
	int32_t mastervolume;
#if RELEASE_SPEED
	uint32_t release;
#endif
	uint32_t cps;		/* cycles per sample */
	uint32_t cpf;	/* cycles per frame (240/192Hz) ($4017.bit7) */
	uint32_t fc;		/* frame counter; */
	uint32_t pt;		/* programmable timer */
	uint32_t wl;		/* wave length */
	uint32_t st;		/* wave step */
	uint32_t output;
	uint8_t fp;		/* frame position; */
	uint8_t duty;		/* frame position; */
	uint8_t key;
	uint8_t mute;
	uint8_t ct;
} DMG_SQUARE;

typedef struct {
	LENGTHCOUNTER lc;
	int32_t mastervolume;
#if RELEASE_SPEED
	uint32_t release;
#endif
	uint32_t cps;		/* cycles per sample */
	uint32_t cpf;		/* cycles per frame (240/192Hz) ($4017.bit7) */
	uint32_t fc;		/* frame counter */
	uint32_t pt;		/* programmable timer */
	uint32_t wl;		/* wave length */
	uint32_t st;		/* wave step */
	uint32_t output;
	uint8_t fp;		/* frame position */
	uint8_t volume;
	uint8_t initial_volume;
	uint8_t on;
	uint8_t key;
	uint8_t mute;
	uint8_t tone[32];
	uint8_t ct;
	uint8_t mccs;		/* Master Channel Control Switch */
} DMG_WAVEMEMORY;

typedef struct {
	LENGTHCOUNTER lc;
	DMG_ENVELOPEDECAY ed;
	int32_t mastervolume;
	uint32_t cps;		/* cycles per sample */
	uint32_t cpf;		/* cycles per frame (240/192Hz) ($4017.bit7) */
	uint32_t fc;		/* frame counter */
	uint32_t pt;		/* programmable timer */
	uint32_t shift;
	uint32_t divratio;
	uint32_t rng;
	uint32_t output;
	uint8_t fp;		/* frame position */
	uint8_t step1;
	uint8_t step2;
	uint8_t key;
	uint8_t mute;
	uint8_t ct;
#if NOISE_EDGE
	uint8_t edge;
	uint8_t edgeout;
	uint8_t rngold;
#endif
} DMG_NOISE;

//typedef struct {
//	uint32_t cps;		/* cycles per sample */
//	uint32_t cpf;		/* cycles per frame (240/192Hz) ($4017.bit7) */
//	uint32_t fc;		/* frame counter */
//	uint32_t enable;
//	uint32_t flag;
//	uint32_t fp;
//	uint32_t halt;
//} DMG_NAZO;

typedef struct {
	KMIF_SOUND_DEVICE kmif;
	DMG_SQUARE square[2];
	DMG_WAVEMEMORY wavememory;
	DMG_NOISE noise;
//	DMG_NAZO nazo;
	struct {
		uint8_t regs[0x30];
		uint8_t enablefg;
	} common;
    uint8_t *GBAMode;
    uint8_t *chmask;
} DMGSOUND;

static const uint8_t dmg_square_duty_table[4][8] = 
/* （単純パターン）こうしないと、アスミッくんワールド２で満足に喋ってくれない */
//	{ {1,0,0,0,0,0,0,0} , {1,1,0,0,0,0,0,0} , {1,1,1,1,0,0,0,0} , {1,1,1,1,1,1,0,0} };

/* GBSOUND.TXT */
	{ {0,0,0,0,1,0,0,0} , {0,0,0,0,1,1,0,0} , {0,0,1,1,1,1,0,0} , {1,1,1,1,0,0,1,1} };
/* GBSOUND.TXTの逆 */
//	{ {1,1,1,1,0,1,1,1} , {1,1,1,1,0,0,1,1} , {1,1,0,0,0,0,1,1} , {0,0,0,0,1,1,0,0} };

static const uint8_t dmg_square_duty_avg[4] = {1,2,4,6};

static const uint8_t dmg_wavememory_volume_table[8] = { 0, 4, 2, 1, 0, 4, 2, 1 };
static const uint8_t dmg_noise_divratio_table[8] = { 1,2,4,6,8,10,12,14 };

static const uint8_t dmg_reset_table[] = 
{
	0x10,0x80,0x11,0xbf,0x12,0x00,0x13,0xff,0x15,0xff,0x16,0x3f,0x17,0x00,0x18,0xff,
	0x1a,0x00,0x1b,0xff,0x1c,0x00,0x1d,0xff,0x1f,0xff,0x20,0xff,0x21,0x00,0x22,0x00,
	0x24,0x77,0x25,0xf3,0x26,0x80,0x30,0x06,0x31,0xfe,0x32,0x0e,0x33,0x7f,0x34,0x00,
	0x35,0xff,0x36,0x58,0x37,0xdf,0x38,0x00,0x39,0xec,0x3a,0x00,0x3b,0xbf,0x3c,0x0c,
	0x3d,0xed,0x3e,0x04,0x3f,0xf7,0x00,0x00,0x54,0x68,0x69,0x73,0x20,0x70,0x72,0x6f,
	0x67,0x72,0x61,0x6d,0x20,0x75,0x73,0x65,0x20,0x4e,0x45,0x5a,0x50,0x6c,0x75,0x67,
	0x2b,0x2b,0x20,0x69,0x6e,0x20,0x47,0x61,0x6d,0x65,0x42,0x6f,0x79,0x2e,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x28,0x6d,0x65,0x73,0x73,0x61,0x67,0x65,0x20,0x62,0x79,
	0x20,0x4f,0x66,0x66,0x47,0x61,0x6f,0x29
};

static const uint32_t dmg_spd_limit_table[8] =
{
	0x7FF, 0x556, 0x667, 0x71D, 
	0x788, 0x7C2, 0x7E1, 0x7F1,
};

//NR30のキーONフラグを立たせると、矩形チャンネルなどの出力が、KEYOFF時に+8される？
#define KEYON_MODE (!sndp->GBAMode[0] && sndp->wavememory.key)

static void LengthCounterStep(LENGTHCOUNTER *lc, uint8_t *key)
{
	if (*key) return;
	if (lc->counter) lc->counter--;
	if (lc->enable && !lc->counter) *key = 1;
}

static void EnvelopeDecayStep(DMG_ENVELOPEDECAY *ed)
{
	if (ed->rate && ++ed->timer >= ed->rate)
	{
		ed->timer = 0;
		if (ed->direction)
		{
			ed->volume += (ed->volume < 0xf);
			//if(ed->volume == 0xf) ed->rate = 0;

		}
		else if (ed->volume)
		{
			ed->volume--;
			//エンペローブ終了したら0クリア（ZOMBIE MODEの為）
			if(!ed->volume) ed->rate = 0;
		}
	}
}

static uint8_t SweepStep(DMG_SWEEP *sw, uint8_t *key)
{
	if (sw->rate && sw->shifter && ++sw->timer >= sw->rate)
	{
		sw->timer = 0;
		if (sw->direction)
		{
			sw->wl -= sw->wl >> sw->shifter;
		}
		else
		{
			sw->wl += sw->wl >> sw->shifter;
			if (sw->wl > dmg_spd_limit_table[sw->shifter]){
				*key = 0;
				sw->wl = 0x7FF;		//「元祖ヤンチャ丸」でのスイープ絡みの強制終了対策
			}
		}
		return 1;
	}
	return 0;
}

//MRN : こんな感じのゾンビらしい
static void DMGZombieMode(DMG_ENVELOPEDECAY *ed, uint32_t *value, uint8_t *key)
{

//	if(!ed->rate || !(*value & 0x07)){
		//エンペローブが下向きで、音量が0ならキーオフ
		if(!(*value & 0xf8)) *key = 0;

		//エンペローブ向きが違っていたら、ボリュームにNOTをかける
		if((*value & 0x08) != ed->zombie_direction){
			ed->zombie_direction = *value & 0x08;
			ed->volume = (ed->volume + ((*value & 0x07) != 0)) & 0x0f;
			ed->volume = ed->volume ^ 0x0f;
		}else{
			ed->volume = (ed->volume + 1 + ((*value & 0x07) != 0)) & 0x0f;
		}
		//*value &= 0xf8;		//エンペローブレート(bit0-2)をクリア
//	}
/*	//MRN : 古いゾンビソース
	if((!sndp->square[ch].lc.enable || !sndp->square[ch].lc.counter)
		&& (sndp->square[0].sw.rate == 0)){
		if(sndp->square[ch].ed.initial_volume != 0)
			sndp->square[ch].ed.volume = (sndp->square[ch].ed.volume + 1) & 0xf;
		else if(sndp->square[ch].ed.rate != 0)
			sndp->square[ch].ed.volume = (sndp->square[ch].ed.volume + 2) & 0xf;
		else
			sndp->square[ch].ed.volume = sndp->square[ch].ed.initial_volume;
	}else{
		sndp->square[ch].ed.volume = sndp->square[ch].ed.initial_volume;
	}
*/
}

static int32_t DMGSoundSquareRender(DMGSOUND *sndp, DMG_SQUARE *ch)
{
	int32_t outputbuf=0,count=0;
	uint32_t wl;
	ch->fc += ch->cps;
	while (ch->fc >= ch->cpf)
	{
		ch->fc -= ch->cpf;
		if (!(ch->fp & 3)) EnvelopeDecayStep(&ch->ed);		/* 64Hz */
		if (!(ch->fp & 1)){
			if (SweepStep(&ch->sw, &ch->mute)){				/* 128Hz */
				ch->wl = ch->sw.wl;
			}
		}
		LengthCounterStep(&ch->lc, &ch->mute);				/* 256Hz */

		ch->fp++;
	}
	wl = (0x800 - ch->wl) << CPS_BITS;
	ch->pt += ch->cps << SQ_RENDERS;

	ch->output = ch->key ? (!ch->mute ? ch->ed.volume * dmg_square_duty_table[ch->duty][ch->st] : 0 ) : (sndp->GBAMode[0] ? 0 : 0x8);
	ch->output = LinearToLog((LOG_TABLE *)&log_table_12_7_30, ch->output) + ch->mastervolume;
	ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->output, log_table_12_7_30.log_lin_bits - log_table_12_7_30.lin_bits - 14);
	
	if (ch->wl > 0x7f9){
		ch->st+= ch->pt / ((wl << CPS_BITS)>>SQ_RENDERS);
		ch->pt%= wl;
		ch->st&= 0x7;

		ch->output = ch->key ? (!ch->mute ? ch->ed.volume * dmg_square_duty_avg[ch->duty] : 0 ) : (sndp->GBAMode[0] ? 0 : 0x20);
		ch->output = LinearToLog((LOG_TABLE *)&log_table_12_7_30, ch->output) + ch->mastervolume;
		ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->output, log_table_12_7_30.log_lin_bits - log_table_12_7_30.lin_bits - 11);
	}
	else 
	while (ch->pt >= wl)
	{
		//---
		outputbuf += ch->output;
		count++;
		//---
		ch->pt -= wl;
		ch->ct++;
		if(ch->ct >= (1 << SQ_RENDERS)){
			ch->ct = 0;
			ch->st++;
			ch->st &= 0x7;

			ch->output = ch->key ? (!ch->mute ? ch->ed.volume * dmg_square_duty_table[ch->duty][ch->st] : 0 ) : (sndp->GBAMode[0] ? 0 : 0x8);
			ch->output = LinearToLog((LOG_TABLE *)&log_table_12_7_30, ch->output) + ch->mastervolume;
			ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->output, log_table_12_7_30.log_lin_bits - log_table_12_7_30.lin_bits - 14);
		}
	}

	//if (ch->mute) return 0;
//	if (!ch->key) return 0;
	//---
	outputbuf += ch->output;
	count++;
	//---

	return outputbuf /count;
}
#define DMG_WAVE_REN \
	ch->output = (ch->on/* && ch->initial_volume*/)\
		 ? (ch->key&&!ch->mute ? (((ch->tone[ch->st] * ch->volume)>>2)<<2) : 0) : (sndp->GBAMode[0] ? (0) : (8<<2));
//		 ? ((ch->tone[ch->st] >> ch->volume)<<2) : (KEYON_MODE ? 0 : (8<<2));

static int32_t DMGSoundWaveMemoryRender(DMGSOUND *sndp, DMG_WAVEMEMORY *ch)
{
	int32_t outputbuf=0,count=0;
	uint32_t wl;
	ch->fc += ch->cps;
	
	while (ch->fc >= ch->cpf)
	{
		ch->fc -= ch->cpf;
		if(ch->key && ch->on)LengthCounterStep(&ch->lc, &ch->mute);				/* 256Hz */
		ch->fp++;
	}

//	if(sndp->wavememory.lc.counter == 0)
//		sndp->wavememory.mccs = 0;

		
	//if (ch->mute) return 0;

	wl = (0x800 - ch->wl) << CPS_BITS;
	ch->pt += ch->cps << WM_RENDERS;

	//ピカチュウの声が0x7FFで回っているため、一応発声しておく
	if (ch->wl > 0x7f9){
		uint32_t total,i;
		for(total=0, i=0; i<0x20; i++)
			total+=ch->tone[i];
		total >>=3;

		ch->st+= ch->pt / ((wl << CPS_BITS)>>WM_RENDERS);
		ch->pt %= wl;
		ch->st &= 0x1f;

		ch->output = (ch->on/* && ch->initial_volume*/) ? (total >> ch->volume) : (sndp->GBAMode[0] ? 0 : (8<<2));
		ch->output = LinearToLog((LOG_TABLE *)&log_table_12_7_30, ch->output) + ch->mastervolume;
		ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->output, log_table_12_7_30.log_lin_bits - log_table_12_7_30.lin_bits - 12);
		outputbuf += ch->output;
		count++;
	}else{
		DMG_WAVE_REN;
		ch->output = LinearToLog((LOG_TABLE *)&log_table_12_7_30, ch->output) + ch->mastervolume;
		ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->output, log_table_12_7_30.log_lin_bits - log_table_12_7_30.lin_bits - 12);
		while (ch->pt >= wl)
		{
			//---
			outputbuf += ch->output;
			count++;
			//---
			ch->pt -= wl;
			ch->ct++;
			if(ch->ct >= (1 << WM_RENDERS)){
				ch->ct = 0;
				if(ch->key)ch->st++;
				ch->st &= 0x1f;

				DMG_WAVE_REN;
				ch->output = LinearToLog((LOG_TABLE *)&log_table_12_7_30, ch->output) + ch->mastervolume;
				ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->output, log_table_12_7_30.log_lin_bits - log_table_12_7_30.lin_bits - 12);
			}
		}
		//---
		outputbuf += ch->output;
		count++;
		//---
	}

	return outputbuf /count;
}

static int32_t DMGSoundNoiseRender(DMGSOUND *sndp, DMG_NOISE *ch)
{
	int32_t outputbuf=0,count=0;

	ch->fc += ch->cps;
	while (ch->fc >= ch->cpf)
	{
		ch->fc -= ch->cpf;
		if (!(ch->fp & 3)) EnvelopeDecayStep(&ch->ed);		/* 64Hz */
		LengthCounterStep(&ch->lc, &ch->mute);				/* 256Hz */
		ch->fp++;

	}

	ch->pt += ch->cps;

	ch->output = ch->key ? (!ch->mute ? ch->ed.volume - (ch->edge * ch->ed.volume) : 0 ) : (sndp->GBAMode[0] ? (0) : (8));
	ch->output = LinearToLog((LOG_TABLE *)&log_table_12_7_30, ch->output) + ch->mastervolume;
	ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->output, log_table_12_7_30.log_lin_bits - log_table_12_7_30.lin_bits - 14);

	while (ch->pt >= (ch->divratio << (CPS_BITS-4)))
	{
		//---
		outputbuf += ch->output;
		count++;
		//---

		/* 音質向上のため */
		ch->ct++;
		if(ch->ct >= 16){ch->ct=0;
			if(ch->rng == 0)ch->rng = 1;
			ch->rng +=  ch->rng + (((ch->rng >> ch->step1) ^ (ch->rng >> ch->step2)) & 1);
			ch->edge ^=/*|=*/ (ch->rng/* ^ ch->rngold*/) & 1;
			ch->rngold = ch->rng;

			ch->output = ch->key ? (!ch->mute ? ch->ed.volume - (ch->edge * ch->ed.volume) : 0 ) : (sndp->GBAMode[0] ? (0) : (8));
			ch->output = LinearToLog((LOG_TABLE *)&log_table_12_7_30, ch->output) + ch->mastervolume;
			ch->output = LogToLinear((LOG_TABLE *)&log_table_12_7_30, ch->output, log_table_12_7_30.log_lin_bits - log_table_12_7_30.lin_bits - 14);
		}
		ch->pt -= (ch->divratio << (CPS_BITS-4));
	}

	//if (ch->mute) return 0;
	//---
	outputbuf += ch->output;
	count++;
	//---

	return outputbuf / count;
}
/*
static int32_t DMGSoundNazoRender(DMGSOUND *sndp, DMG_NAZO *ch)
{
	int32_t outputch = 0;

//	wl =  (0x800 - sndp->wavememory.wl) << (CPS_BITS - 0);
	ch->fc += ch->cps;
#if 1
//	if (ch->fc >= wl)
//	{
//		ch->fc %= wl;
	if (ch->fc >= ch->cpf)
	{
		ch->fc %= ch->cpf;

//		if(ch->enable){
//				sndp->nazo.enable = 0;
//		}
		if(sndp->nazo.enable == 8 && !sndp->wavememory.key){
				sndp->nazo.enable = 0;
		}
//		if(ch->enable == 8)
//			if(!sndp->wavememory.tone[sndp->wavememory.st]
//			 && sndp->wavememory.volume!=6){
//				sndp->nazo.enable = 1;
//			}else
//				sndp->nazo.enable = 8;
	}
//	if(sndp->wavememory.on || sndp->wavememory.volume!=6){
//			sndp->nazo.enable = 0;
//	}
#endif
	return 0;
}
*/
static void dmg_sndvolume(void *ctx, int32_t volume)
{
	DMGSOUND *sndp = ctx;
	volume = (volume << (log_table_12_7_30.log_bits - 8)) << 1;
	sndp->square[0].mastervolume = volume;
	sndp->square[1].mastervolume = volume;
	sndp->wavememory.mastervolume = volume;
	sndp->noise.mastervolume = volume;
}

static void dmg_sndsynth(void *ctx, int32_t *p)
{
	DMGSOUND *sndp = ctx;
	int32_t b[2] = { 0, 0 };
	int32_t outputch,outputidle;
	outputch = LinearToLog((LOG_TABLE *)&log_table_12_7_30, 0x8) + sndp->square[0].mastervolume;
	outputidle = LogToLinear((LOG_TABLE *)&log_table_12_7_30, outputch, log_table_12_7_30.log_lin_bits - log_table_12_7_30.lin_bits - 14);

	if (!(sndp->common.regs[0x16] & 0x80)) {
		b[0] += outputidle * 4;
		b[1] += outputidle * 4;
		return;
	}

	//MRN : GBでは、NR52-7bit目を一旦OFFすると、キーオンされるまでｸﾘｯｸﾉｲｽﾞすら出ない？
	if (!sndp->GBAMode[0] && !sndp->common.enablefg) {
		b[0] += outputidle * 4;
		b[1] += outputidle * 4;
		return;
	}

//	DMGSoundNazoRender(sndp, &sndp->nazo);

	outputch = DMGSoundSquareRender(sndp, &sndp->square[0]);
	if(sndp->chmask[NEZ_DEV_DMG_SQ1]){
		if ((sndp->common.regs[0x15] & 0x10)) b[0] += outputch; else b[0] += outputidle;
		if ((sndp->common.regs[0x15] & 0x01)) b[1] += outputch; else b[1] += outputidle;
	}

	outputch = DMGSoundSquareRender(sndp, &sndp->square[1]);
	if(sndp->chmask[NEZ_DEV_DMG_SQ2]){
		if ((sndp->common.regs[0x15] & 0x20)) b[0] += outputch; else b[0] += outputidle;
		if ((sndp->common.regs[0x15] & 0x02)) b[1] += outputch; else b[1] += outputidle;
	}

	outputch = DMGSoundWaveMemoryRender(sndp, &sndp->wavememory);
	if(sndp->chmask[NEZ_DEV_DMG_WM]){
		if ((sndp->common.regs[0x15] & 0x40)) b[0] += outputch; else b[0] += outputidle;
		if ((sndp->common.regs[0x15] & 0x04)) b[1] += outputch; else b[1] += outputidle;
	}

	outputch = DMGSoundNoiseRender(sndp, &sndp->noise);
	if(sndp->chmask[NEZ_DEV_DMG_NOISE]){
		if ((sndp->common.regs[0x15] & 0x80)) b[0] += outputch; else b[0] += outputidle;
		if ((sndp->common.regs[0x15] & 0x08)) b[1] += outputch; else b[1] += outputidle;
	}
	p[0] += b[0] * ((sndp->common.regs[0x14] & 0x70) >> 4) + 1;
	p[1] += b[1] * (sndp->common.regs[0x14] & 0x07) + 1;
}

static void dmg_sndwrite(void *ctx, uint32_t a, uint32_t v)
{
	DMGSOUND *sndp = ctx;
	uint32_t ch;

#if WAVETABLE_REAL_RW
	if (0xff30 <= a && a <= 0xff3f)
	{
		// 波形メモリ再生中に書くと、
		// GB・GBPは”まれに”変なところに書かさるらしい。
		// GBCは”ときどき”変なところに書かさるらしい。
		// GBAは全く書かれない。
		if(!sndp->GBAMode[0]){//GB では、再生中の波形位置のメモリデータに書くんだと思う。
			if(!sndp->wavememory.key || sndp->wavememory.initial_volume == 0){
				sndp->wavememory.tone[((a - 0xff30) << 1) + 0] = (v >> 4) & 0x0f;
				sndp->wavememory.tone[((a - 0xff30) << 1) + 1] = (v << 0) & 0x0f;
				sndp->common.regs[a - 0xff10] = v;
			}else{
#define WAVE_ADR_CALC ((((a - 0xff30) << 1) + (sndp->wavememory.pt + (9 << SQ_RENDERS)) / (8 << SQ_RENDERS)) & 0x1e)
				if(sndp->wavememory.pt < (50 << SQ_RENDERS)){
					sndp->wavememory.tone[WAVE_ADR_CALC + 0] = (v >> 4) & 0x0f;
					sndp->wavememory.tone[WAVE_ADR_CALC + 1] = (v << 0) & 0x0f;
					sndp->common.regs[(WAVE_ADR_CALC >> 1) + 0x20] = v;
				} 
			}
		}else{//GBA
			if(!sndp->wavememory.key || sndp->wavememory.initial_volume == 0){
				sndp->wavememory.tone[((a - 0xff30) << 1) + 0] = (v >> 4) & 0x0f;
				sndp->wavememory.tone[((a - 0xff30) << 1) + 1] = (v << 0) & 0x0f;
				sndp->common.regs[a - 0xff10] = v;
			}else{
			}
		}
		return;
	}else{
		sndp->common.regs[a - 0xff10] = v;
	}
#else
	if (0xff30 <= a && a <= 0xff3f)
	{
		sndp->wavememory.tone[((a - 0xff30) << 1) + 0] = (v >> 4) & 0x0f;
		sndp->wavememory.tone[((a - 0xff30) << 1) + 1] = (v << 0) & 0x0f;
	}
	sndp->common.regs[a - 0xff10] = v;
#endif

	switch (a)
	{
		case 0xff10:
			sndp->square[0].sw.rate = (v >> 4) & 7;
			sndp->square[0].sw.direction = v & 8;
			sndp->square[0].sw.shifter = v & 7;
			break;
		case 0xff11:
		case 0xff16:
			ch = a >= 0xff16;
			sndp->square[ch].lc.initial_counter = 0x40 - (v & 0x3f);
			sndp->square[ch].duty = v >> 6;
			sndp->square[ch].lc.counter = sndp->square[ch].lc.initial_counter;
			break;
		case 0xff12:
		case 0xff17:
			ch = a >= 0xff16;
			/* ZOMBIE MODE */
			DMGZombieMode(&sndp->square[ch].ed, &v, &sndp->square[ch].key);

			sndp->square[ch].ed.initial_volume = v >> 4;
			sndp->square[ch].ed.direction = v & 8;
			sndp->square[ch].ed.initial_rate = v & 7;
//			if(sndp->square[ch].ed.rate)
//				sndp->square[ch].ed.rate = sndp->square[ch].ed.initial_rate;
			if(sndp->square[ch].ed.initial_rate){
				sndp->square[ch].ed.rate = sndp->square[ch].ed.initial_rate;
				sndp->square[ch].ed.volume = sndp->square[ch].ed.initial_volume;
			}
			break;
		case 0xff13:
		case 0xff18:
			ch = a >= 0xff16;
			sndp->square[ch].wl &= 0x700;
			sndp->square[ch].wl |= v;
			break;
		case 0xff14:
		case 0xff19:
			ch = a >= 0xff16;
			sndp->square[ch].wl &= 0xff;
			sndp->square[ch].wl |= (v << 8) & 0x700;
			sndp->square[ch].lc.enable = v & 0x40;
			
			//MRN : GBでは、NR*4に書き込むたびに、lcカウンタが減算される？
			if(!sndp->GBAMode[0])LengthCounterStep(&sndp->square[ch].lc, &sndp->square[ch].key);
			
			if (v & 0x80)
			{
				if(!sndp->square[ch].key){
					sndp->square[ch].st=0;
				}
				sndp->square[ch].lc.counter = sndp->square[ch].lc.initial_counter;
				sndp->square[ch].ed.volume = sndp->square[ch].ed.initial_volume;
				sndp->square[ch].ed.rate = sndp->square[ch].ed.initial_rate;
				sndp->square[ch].ed.timer = 0;
				sndp->square[ch].sw.timer = 0;
				//sndp->square[ch].fc = 0;
				sndp->square[ch].sw.wl = sndp->square[ch].wl;
				sndp->square[ch].pt=0;
				sndp->square[ch].ct=0;

				sndp->square[ch].mute = 0; 
				sndp->square[ch].key =
					sndp->square[ch].ed.volume | sndp->square[ch].ed.direction ? 1 << ch : 0;

				if (sndp->square[ch].ed.direction 
					? (sndp->square[ch].ed.volume == 0xf) : (!sndp->square[ch].ed.volume))
					sndp->square[ch].ed.rate = 0;
				sndp->square[ch].ed.zombie_direction = sndp->square[ch].ed.direction;

				//MRN : GBでは、NR52-7bit目を一旦OFFすると、キーオンされるまでｸﾘｯｸﾉｲｽﾞすら出ない？
				if((sndp->square[ch].ed.volume)
					|| (sndp->square[ch].ed.direction && sndp->square[ch].ed.initial_rate))
					sndp->common.enablefg = 2;
			}
			break;
		case 0xff1a:
/*			if((sndp->wavememory.on & 0x80 ) != (uint8_t)(v & 0x80)){
				sndp->wavememory.pt = 0;
				sndp->wavememory.ct = 0;
				sndp->wavememory.st = 0;
			}
*/			sndp->wavememory.on = v & 0x80;
			if(!sndp->wavememory.on){
//				sndp->wavememory.lc.counter = 0;
				sndp->wavememory.key = 0;
			}
//			if(!sndp->wavememory.lc.counter)
//				sndp->wavememory.on = 0;
			break;
		case 0xff1b:
			sndp->wavememory.lc.initial_counter = 0x100 - (v & 0xff);
			sndp->wavememory.lc.counter = sndp->wavememory.lc.initial_counter;
			break;
		case 0xff1c:
//			if(!sndp->wavememory.lc.counter && !sndp->common.regs[0])
//				sndp->wavememory.lc.enable = 0x40;
//			if(sndp->wavememory.on)
//				sndp->wavememory.lc.counter = sndp->wavememory.lc.initial_counter;

//			if(v & 0x8)sndp->nazo = (sndp->nazo+1)&1;
//			if(sndp->wavememory.volume == dmg_wavememory_volume_table[(v >> 5) & 3])
//			if(sndp->common.regs[0xc] == v)
//			if(dmg_square_duty_table[sndp->square[1].duty][sndp->square[1].st]==0)
/*			sndp->nazo.enable = 
				(dmg_square_duty_table[sndp->square[0].duty][sndp->square[0].st]==0 ^
				dmg_square_duty_table[sndp->square[1].duty][sndp->square[1].st]==0)||
//				sndp->wavememory.tone[sndp->wavememory.st]==0 &&
				(sndp->wavememory.volume == 6)
				? v & 0x8 : 0;
*/
//			if(1
/*				sndp->square[0].ed.volume == 0 &&
				sndp->square[1].ed.volume == 0 &&
				(sndp->wavememory.volume == 6 || !sndp->wavememory.on) && 
				sndp->noise.ed.volume == 0
*/
//				(dmg_square_duty_table[sndp->square[0].duty][sndp->square[0].st]==0 ^
//				dmg_square_duty_table[sndp->square[1].duty][sndp->square[1].st]==0)||

//			){
//				sndp->nazo.halt = v & 0x8;
//				sndp->wavememory.mccs = v & 0x8;
//				sndp->wavememory.mccs = 1;
//				sndp->nazo.fc %= sndp->nazo.cpf / 2;
//				sndp->nazo.fc = 0;
//			}
			//↑カエルの為〜 の名前入力BGMのクリック音と密接な関係があるようだが…

			//			if(sndp->nazo)sndp->square[0].st=4;
//			if(sndp->nazo)sndp->square[1].st=4;
//			if(sndp->nazo)sndp->wavememory.st = 0x10;
			sndp->wavememory.initial_volume = (v >> 5) & (sndp->GBAMode[0] ? 3 : 7);
			sndp->wavememory.volume = dmg_wavememory_volume_table[sndp->wavememory.initial_volume];
			break;
		case 0xff1d:
			sndp->wavememory.wl &= 0x700;
			sndp->wavememory.wl |= v;
			break;
		case 0xff1e:
			sndp->wavememory.wl &= 0xff;
			sndp->wavememory.wl |= (v << 8) & 0x700;

			if(sndp->wavememory.lc.enable)sndp->wavememory.lc.enable = v & 0x40;

			//MRN : GBでは、NR*4に書き込むたびに、lcカウンタが減算される？
			if(!sndp->GBAMode[0])LengthCounterStep(&sndp->wavememory.lc, &sndp->wavememory.key);

			if (v & 0x80)
			{
				sndp->wavememory.key = 1 << 2;
				sndp->wavememory.mute = 0; 
				sndp->wavememory.lc.counter = sndp->wavememory.lc.initial_counter;
				sndp->wavememory.volume = dmg_wavememory_volume_table[sndp->wavememory.initial_volume];
				//sndp->wavememory.fc = 0;
				//sndp->wavememory.on = 0x80;
				sndp->wavememory.pt = 0;
				sndp->wavememory.ct = 0;
				sndp->wavememory.st = 0;
//				if(sndp->nazo.enable == 8){
//						sndp->nazo.enable = 1;
//				}
				sndp->wavememory.lc.enable = v & 0x40;

				//MRN : GBでは、NR52-7bit目を一旦OFFすると、キーオンされるまでｸﾘｯｸﾉｲｽﾞすら出ない？
				if(sndp->wavememory.initial_volume)
					sndp->common.enablefg = 2;
			}
			break;

		case 0xff20:
			sndp->noise.lc.initial_counter = 0x40 - (v & 0x3f);
			sndp->noise.lc.counter = sndp->noise.lc.initial_counter;
			break;
		case 0xff21:
			/* ZOMBIE MODE */
			DMGZombieMode(&sndp->noise.ed, &v, &sndp->noise.key);

			sndp->noise.ed.initial_volume = v >> 4;
			sndp->noise.ed.direction = v & 8;
			sndp->noise.ed.initial_rate = v & 7;
//			if(sndp->noise.ed.rate)
//				sndp->noise.ed.rate = sndp->noise.ed.initial_rate;
			if(sndp->noise.ed.rate){
				sndp->noise.ed.rate = sndp->noise.ed.initial_rate;
				sndp->noise.ed.volume = sndp->noise.ed.initial_volume;
			}
			break;
		case 0xff22:
			sndp->noise.shift = v >> 4;
			if (sndp->noise.shift > 14) sndp->noise.shift = 14;
#if 1
			/* GBSOUND.TXT */
			sndp->noise.step1 = (v & 8) ? (6) : (14);
			sndp->noise.step2 = (v & 8) ? (5) : (13);
#else
			sndp->noise.step1 = (v & 8) ? (7 + 1) : (15 + 1);
			sndp->noise.step2 = (v & 8) ? (2 + 1) : (10 + 1);
#endif
			sndp->noise.divratio = dmg_noise_divratio_table[v & 7] << sndp->noise.shift;

			//sndp->noise.ed.volume = sndp->noise.ed.initial_volume;
			break;
		case 0xff23:
			sndp->noise.lc.enable = v & 0x40;

			//MRN : GBでは、NR*4に書き込むたびに、lcカウンタが減算される？
			if(!sndp->GBAMode[0])LengthCounterStep(&sndp->noise.lc, &sndp->noise.key);

			if (v & 0x80)
			{
				sndp->noise.ed.volume = sndp->noise.ed.initial_volume;
				sndp->noise.lc.counter = sndp->noise.lc.initial_counter;
				sndp->noise.ed.rate = sndp->noise.ed.initial_rate;
				sndp->noise.ed.timer = 0;
				//sndp->noise.fc = 0;
				sndp->noise.rng = 0xffff;
				sndp->noise.edge = 1;
				sndp->noise.ct = 0;

				sndp->noise.mute = 0; 
				sndp->noise.key = 
					sndp->noise.ed.volume | sndp->noise.ed.direction ? 1 << 3 : 0;
				if (sndp->noise.ed.direction 
					? (sndp->noise.ed.volume == 0xf) : (!sndp->noise.ed.volume))
					sndp->noise.ed.rate = 0;
				sndp->noise.ed.zombie_direction = sndp->noise.ed.direction;

				//MRN : GBでは、NR52-7bit目を一旦OFFすると、キーオンされるまでｸﾘｯｸﾉｲｽﾞすら出ない？
				if((sndp->noise.ed.volume)
					|| (sndp->noise.ed.direction && sndp->noise.ed.initial_rate))
					sndp->common.enablefg = 2;
			}
			break;
		case 0xff26:
			if(!(v & 0x80) && !sndp->GBAMode[0]){
				if(
					(!sndp->square[0].ed.volume||!sndp->square[0].key)
				 && (!sndp->square[1].ed.volume||!sndp->square[1].key)
				 && (!sndp->wavememory.key) && !sndp->wavememory.lc.counter
				 && (!sndp->noise.ed.volume||!sndp->noise.key)
				)
					sndp->common.enablefg = 0;
			}
			break;
	}
}

static uint32_t dmg_sndread(void *ctx, uint32_t a)
{
	DMGSOUND *sndp = ctx;
	switch (a)
	{
		//MRN : こんな感じのマスクらしい
		case 0xff10: return sndp->common.regs[a - 0xff10] | 0x80;
		case 0xff11: return sndp->common.regs[a - 0xff10] | 0x3f;
		case 0xff12: return sndp->common.regs[a - 0xff10] | 0x00;
		case 0xff13: return sndp->common.regs[a - 0xff10] | 0xff;
		case 0xff14: return sndp->common.regs[a - 0xff10] | 0xbf;
		case 0xff15: return sndp->common.regs[a - 0xff10] | 0xff;
		case 0xff16: return sndp->common.regs[a - 0xff10] | 0x3f;
		case 0xff17: return sndp->common.regs[a - 0xff10] | 0x00;
		case 0xff18: return sndp->common.regs[a - 0xff10] | 0xff;
		case 0xff19: return sndp->common.regs[a - 0xff10] | 0xbf;
		case 0xff1a: return sndp->common.regs[a - 0xff10] | 0x7f;
		case 0xff1b: return sndp->common.regs[a - 0xff10] | 0xff;
		case 0xff1c: return sndp->common.regs[a - 0xff10] | 0x9f;
		case 0xff1d: return sndp->common.regs[a - 0xff10] | 0xff;
		case 0xff1e: return sndp->common.regs[a - 0xff10] | 0xbf;
		case 0xff1f: return sndp->common.regs[a - 0xff10] | 0xff;
		case 0xff20: return sndp->common.regs[a - 0xff10] | 0xff;
		case 0xff21: return sndp->common.regs[a - 0xff10] | 0x00;
		case 0xff22: return sndp->common.regs[a - 0xff10] | 0x00;
		case 0xff23: return sndp->common.regs[a - 0xff10] | 0xbf;
		case 0xff24: return sndp->common.regs[a - 0xff10] | 0x00;
		case 0xff25: return sndp->common.regs[a - 0xff10] | 0x00;

		case 0xff26: return((sndp->common.regs[a - 0xff10] & 0x80)|0x70)
			+ (sndp->square[0].key && !sndp->square[0].mute ? 1 : 0) 
			+ (sndp->square[1].key && !sndp->square[1].mute ? 2 : 0) 
			+ (sndp->wavememory.on && sndp->wavememory.key && !sndp->wavememory.mute ? 4 : 0)
			+ (sndp->noise.key && !sndp->noise.mute ? 8 : 0) 
			;
	}

#if WAVETABLE_REAL_RW
	if (0xff30 <= a && a <= 0xff3f)
	{
		//MRN : なんと、GBAではch3再生中に波形メモリを読めない！？
		if(!sndp->GBAMode[0]){//GB では、再生中の波形位置のメモリデータに書くんだと思う。
			if(!sndp->wavememory.key || sndp->wavememory.initial_volume == 0){
				return sndp->common.regs[a - 0xff10];
			}else{
				return sndp->common.regs[(sndp->wavememory.st >> 1)+0x20];
			}
		}else{//GBA
			if(!sndp->wavememory.key || sndp->wavememory.initial_volume == 0){
				return sndp->common.regs[a - 0xff10];
			}else{
				return 0xff;
			}
		}
	}
#endif
	return sndp->common.regs[a - 0xff10];
}


static void DMGSoundSquareReset(DMG_SQUARE *ch, uint32_t clock, uint32_t freq)
{
	XMEMSET(ch, 0, sizeof(DMG_SQUARE));
	ch->cps = DivFix(clock, 4 * freq, CPS_BITS);
	ch->cpf = DivFix(clock, 4 * 256, CPS_BITS);
	ch->duty = 4;
}

static void DMGSoundWaveMemoryReset(DMG_WAVEMEMORY *ch, uint32_t clock, uint32_t freq)
{
	XMEMSET(ch, 0, sizeof(DMG_WAVEMEMORY));
	ch->cps = DivFix(clock, 2 * freq, CPS_BITS);
	ch->cpf = DivFix(clock, 2 * 2, CPFWM_BITS);
	ch->on = 0x80;
}

static void DMGSoundNoiseReset(DMG_NOISE *ch, uint32_t clock, uint32_t freq)
{
	XMEMSET(ch, 0, sizeof(DMG_NOISE));
	ch->cps = DivFix(clock, 8 * freq, CPS_BITS);
	ch->cpf = DivFix(clock, 8 * 256, CPS_BITS);
	ch->rng = 1;
	ch->divratio = 1;
}

/*
static void DMGSoundNazoReset(DMG_NAZO *ch, uint32_t clock, uint32_t freq)
{
	XMEMSET(ch, 0, sizeof(DMG_NOISE));
	ch->cps = DivFix(clock, 2 * freq, CPS_BITS);
	ch->cpf = DivFix(clock, 512 * 256, CPS_BITS);
	ch->enable = 0;
}
*/

static void dmg_sndreset(void *ctx, uint32_t clock, uint32_t freq)
{
	uint32_t p,a;
	DMGSOUND *sndp = ctx;
	XMEMSET(&sndp->common, 0, sizeof(sndp->common));
	DMGSoundSquareReset(&sndp->square[0], clock, freq);
	DMGSoundSquareReset(&sndp->square[1], clock, freq);
	DMGSoundWaveMemoryReset(&sndp->wavememory, clock, freq);
	DMGSoundNoiseReset(&sndp->noise, clock, freq);
//	DMGSoundNazoReset(&sndp->nazo, clock, freq);
	for (p = 0; dmg_reset_table[p] || dmg_reset_table[p+1]; p+=2)
		dmg_sndwrite(sndp, 0xff00 + dmg_reset_table[p], dmg_reset_table[p+1]);
//MRN : GBC・GBAでは、00 FF 00 FF…と、綺麗に初期化される
	if	(sndp->GBAMode[0])
		for (a = 0xff30; a <= 0xff3f; a++) dmg_sndwrite(sndp, a, (a & 1) ? 0xff : 0);
	sndp->common.enablefg = 2;
}

static void dmg_sndrelease(void *ctx)
{
	DMGSOUND *sndp = ctx;
	if (sndp) {
		XFREE(sndp);
	}
}

static void dmg_setinst(void *ctx, uint32_t n, void *p, uint32_t l)
{
    (void)ctx;
    (void)n;
    (void)p;
    (void)l;
}

PROTECTED KMIF_SOUND_DEVICE *DMGSoundAlloc(NEZ_PLAY *pNezPlay)
{
	DMGSOUND *sndp;
	sndp = XMALLOC(sizeof(DMGSOUND));
	if (!sndp) return 0;
	XMEMSET(sndp, 0, sizeof(DMGSOUND));

    sndp->chmask = pNezPlay->chmask;
	sndp->kmif.ctx = sndp;
	sndp->kmif.release = dmg_sndrelease;
	sndp->kmif.reset = dmg_sndreset;
	sndp->kmif.synth = dmg_sndsynth;
	sndp->kmif.volume = dmg_sndvolume;
	sndp->kmif.write = dmg_sndwrite;
	sndp->kmif.read = dmg_sndread;
	sndp->kmif.setinst = dmg_setinst;
    sndp->GBAMode = &(pNezPlay->gb_config.gbamode);

	return &sndp->kmif;
}

#undef CPS_BITS
#undef CPFWM_BITS
#undef NOISE_EDGE
#undef WAVETABLE_REAL_RW
#undef RESET_MODE
#undef SQ_RENDERS
#undef WM_RENDERS
#undef KAERUNOISE_BITS

#undef LIN_BITS
#undef LOG_BITS
#undef LOG_LIN_BITS
#undef KEYON_MODE
