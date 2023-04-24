#include "../kmsnddev.h"
#include "../../common/divfix.h"
#include "../logtable.h"
#include "s_deltat.h"
#include "../../logtable/log_table.h"

#define CPS_SHIFT 16
#define PHASE_SHIFT 16 /* 16(fix) */
#define LIN_BITS 7
#define LOG_BITS 12
#define LOG_LIN_BITS 30

typedef struct {
	KMIF_SOUND_DEVICE kmif;
	struct YMDELTATPCMSOUND_COMMON_TAG {
		int32_t mastervolume;
		int32_t step;
		int32_t output;
		uint32_t cnt;
		uint32_t cps;
		uint32_t phase;
		uint32_t deltan;
		int32_t scale;
		uint32_t mem;
		uint32_t play;
		uint32_t start;
		uint32_t stop;
		uint32_t level32;
		uint8_t key;
		uint8_t level;
		uint8_t granuality;
		uint8_t pad4_3;
		uint8_t regs[0x10];
	} common;
	uint8_t *romrambuf;
	uint32_t romrammask;
	uint8_t *rambuf;
	uint32_t rammask;
	uint8_t *rombuf;
	uint32_t rommask;
	uint8_t ymdeltatpcm_type;
	uint8_t memshift;
	uint32_t ram_size;
    uint8_t *chmask;
} YMDELTATPCMSOUND;

static const int8_t deltat_table_step[16] =
{
	1,	3,	5,	7,	9,	11,	13,	15,
	-1,	-1,	-1,	-1,	2,	4,	6,	8
};
static const uint8_t deltat_table_scale[16] =
{
	 57,  57,  57,  57,  77, 102, 128, 153,
	 57,  57,  57,  57,  77, 102, 128, 153,
};

static const int32_t deltat_scaletable[49*16]={
    2,    6,   10,   14,   18,   22,   26,   30,   -2,   -6,  -10,  -14,  -18,  -22,  -26,  -30,
    2,    6,   10,   14,   19,   23,   27,   31,   -2,   -6,  -10,  -14,  -19,  -23,  -27,  -31,
    2,    6,   11,   15,   21,   25,   30,   34,   -2,   -6,  -11,  -15,  -21,  -25,  -30,  -34,
    2,    7,   12,   17,   23,   28,   33,   38,   -2,   -7,  -12,  -17,  -23,  -28,  -33,  -38,
    2,    7,   13,   18,   25,   30,   36,   41,   -2,   -7,  -13,  -18,  -25,  -30,  -36,  -41,
    3,    9,   15,   21,   28,   34,   40,   46,   -3,   -9,  -15,  -21,  -28,  -34,  -40,  -46,
    3,   10,   17,   24,   31,   38,   45,   52,   -3,  -10,  -17,  -24,  -31,  -38,  -45,  -52,
    3,   10,   18,   25,   34,   41,   49,   56,   -3,  -10,  -18,  -25,  -34,  -41,  -49,  -56,
    4,   12,   21,   29,   38,   46,   55,   63,   -4,  -12,  -21,  -29,  -38,  -46,  -55,  -63,
    4,   13,   22,   31,   41,   50,   59,   68,   -4,  -13,  -22,  -31,  -41,  -50,  -59,  -68,
    5,   15,   25,   35,   46,   56,   66,   76,   -5,  -15,  -25,  -35,  -46,  -56,  -66,  -76,
    5,   16,   27,   38,   50,   61,   72,   83,   -5,  -16,  -27,  -38,  -50,  -61,  -72,  -83,
    6,   18,   31,   43,   56,   68,   81,   93,   -6,  -18,  -31,  -43,  -56,  -68,  -81,  -93,
    6,   19,   33,   46,   61,   74,   88,  101,   -6,  -19,  -33,  -46,  -61,  -74,  -88, -101,
    7,   22,   37,   52,   67,   82,   97,  112,   -7,  -22,  -37,  -52,  -67,  -82,  -97, -112,
    8,   24,   41,   57,   74,   90,  107,  123,   -8,  -24,  -41,  -57,  -74,  -90, -107, -123,
    9,   27,   45,   63,   82,  100,  118,  136,   -9,  -27,  -45,  -63,  -82, -100, -118, -136,
   10,   30,   50,   70,   90,  110,  130,  150,  -10,  -30,  -50,  -70,  -90, -110, -130, -150,
   11,   33,   55,   77,   99,  121,  143,  165,  -11,  -33,  -55,  -77,  -99, -121, -143, -165,
   12,   36,   60,   84,  109,  133,  157,  181,  -12,  -36,  -60,  -84, -109, -133, -157, -181,
   13,   39,   66,   92,  120,  146,  173,  199,  -13,  -39,  -66,  -92, -120, -146, -173, -199,
   14,   43,   73,  102,  132,  161,  191,  220,  -14,  -43,  -73, -102, -132, -161, -191, -220,
   16,   48,   81,  113,  146,  178,  211,  243,  -16,  -48,  -81, -113, -146, -178, -211, -243,
   17,   52,   88,  123,  160,  195,  231,  266,  -17,  -52,  -88, -123, -160, -195, -231, -266,
   19,   58,   97,  136,  176,  215,  254,  293,  -19,  -58,  -97, -136, -176, -215, -254, -293,
   21,   64,  107,  150,  194,  237,  280,  323,  -21,  -64, -107, -150, -194, -237, -280, -323,
   23,   70,  118,  165,  213,  260,  308,  355,  -23,  -70, -118, -165, -213, -260, -308, -355,
   26,   78,  130,  182,  235,  287,  339,  391,  -26,  -78, -130, -182, -235, -287, -339, -391,
   28,   85,  143,  200,  258,  315,  373,  430,  -28,  -85, -143, -200, -258, -315, -373, -430,
   31,   94,  157,  220,  284,  347,  410,  473,  -31,  -94, -157, -220, -284, -347, -410, -473,
   34,  103,  173,  242,  313,  382,  452,  521,  -34, -103, -173, -242, -313, -382, -452, -521,
   38,  114,  191,  267,  345,  421,  498,  574,  -38, -114, -191, -267, -345, -421, -498, -574,
   42,  126,  210,  294,  379,  463,  547,  631,  -42, -126, -210, -294, -379, -463, -547, -631,
   46,  138,  231,  323,  417,  509,  602,  694,  -46, -138, -231, -323, -417, -509, -602, -694,
   51,  153,  255,  357,  459,  561,  663,  765,  -51, -153, -255, -357, -459, -561, -663, -765,
   56,  168,  280,  392,  505,  617,  729,  841,  -56, -168, -280, -392, -505, -617, -729, -841,
   61,  184,  308,  431,  555,  678,  802,  925,  -61, -184, -308, -431, -555, -678, -802, -925,
   68,  204,  340,  476,  612,  748,  884, 1020,  -68, -204, -340, -476, -612, -748, -884,-1020,
   74,  223,  373,  522,  672,  821,  971, 1120,  -74, -223, -373, -522, -672, -821, -971,-1120,
   82,  246,  411,  575,  740,  904, 1069, 1233,  -82, -246, -411, -575, -740, -904,-1069,-1233,
   90,  271,  452,  633,  814,  995, 1176, 1357,  -90, -271, -452, -633, -814, -995,-1176,-1357,
   99,  298,  497,  696,  895, 1094, 1293, 1492,  -99, -298, -497, -696, -895,-1094,-1293,-1492,
  109,  328,  547,  766,  985, 1204, 1423, 1642, -109, -328, -547, -766, -985,-1204,-1423,-1642,
  120,  360,  601,  841, 1083, 1323, 1564, 1804, -120, -360, -601, -841,-1083,-1323,-1564,-1804,
  132,  397,  662,  927, 1192, 1457, 1722, 1987, -132, -397, -662, -927,-1192,-1457,-1722,-1987,
  145,  436,  728, 1019, 1311, 1602, 1894, 2185, -145, -436, -728,-1019,-1311,-1602,-1894,-2185,
  160,  480,  801, 1121, 1442, 1762, 2083, 2403, -160, -480, -801,-1121,-1442,-1762,-2083,-2403,
  176,  528,  881, 1233, 1587, 1939, 2292, 2644, -176, -528, -881,-1233,-1587,-1939,-2292,-2644,
  194,  582,  970, 1358, 1746, 2134, 2522, 2910, -194, -582, -970,-1358,-1746,-2134,-2522,-2910
};


__inline static void deltat_writeram(YMDELTATPCMSOUND *sndp, uint32_t v)
{
	sndp->rambuf[(sndp->common.mem >> 1) & sndp->rammask] = (uint8_t)v;
	sndp->common.mem += 1 << 1;
}

__inline static uint32_t deltat_readram(YMDELTATPCMSOUND *sndp)
{
	uint32_t v;
	v = sndp->romrambuf[(sndp->common.play >> 1) & sndp->romrammask];
	if (sndp->common.play & 1)
		v &= 0x0f;
	else
		v >>= 4;
	sndp->common.play += 1;
	if (sndp->common.play >= sndp->common.stop)
	{
		if (sndp->common.regs[0] & 0x10)
		{
			sndp->common.play = sndp->common.start;
			sndp->common.step = 0;
			if(sndp->ymdeltatpcm_type==MSM5205){
				sndp->common.scale = 0;
			}else{
				sndp->common.scale = 127;
			}
		}
		else
		{
			sndp->common.key = 0;
		}
	}
	return v;
}

__inline static void DelrtatStep(YMDELTATPCMSOUND *sndp, uint32_t data)
{
	if(sndp->ymdeltatpcm_type==MSM5205){
		sndp->common.scale = sndp->common.scale + deltat_scaletable[(sndp->common.step << 4) + (data & 0xf)];
		if (sndp->common.scale >  2047) sndp->common.scale = 2047;
		if (sndp->common.scale < -2048) sndp->common.scale = -2048;

		sndp->common.step += deltat_table_step[(data & 7) + 8];
		if (sndp->common.step > 48) sndp->common.step = 48;
		if (sndp->common.step < 0) sndp->common.step = 0;
	}else{
		if (data & 8)
			sndp->common.step -= (deltat_table_step[data & 7] * sndp->common.scale) >> 3;
		else
			sndp->common.step += (deltat_table_step[data & 7] * sndp->common.scale) >> 3;
		if (sndp->common.step > ((1 << 15) - 1)) sndp->common.step = ((1 << 15) - 1);
		if (sndp->common.step < -(1 << 15)) sndp->common.step = -(1 << 15);
		sndp->common.scale = (sndp->common.scale * deltat_table_scale[data]) >> 6;
		if (sndp->common.scale > 24576) sndp->common.scale = 24576;
		if (sndp->common.scale < 127) sndp->common.scale = 127;
	}
}

#if (((-1) >> 1) == -1)
#define SSR(x, y) (((int32_t)x) >> (y))
#else
#define SSR(x, y) (((x) >= 0) ? ((x) >> (y)) : (-((-(x) - 1) >> (y)) - 1))
#endif


static void deltat_sndsynth(void *ctx, int32_t *p)
{
    YMDELTATPCMSOUND *sndp = (YMDELTATPCMSOUND *)ctx;
	if (sndp->common.key)
	{
		uint32_t step;
		sndp->common.cnt += sndp->common.cps;
		step = sndp->common.cnt >> CPS_SHIFT;
		sndp->common.cnt &= (1 << CPS_SHIFT) - 1;
		sndp->common.phase += step * sndp->common.deltan;
		step = sndp->common.phase >> PHASE_SHIFT;
		sndp->common.phase &= (1 << PHASE_SHIFT) - 1;
		if (step)
		{
			do
			{
				DelrtatStep(sndp, deltat_readram(sndp));
			} while (--step);
			if(sndp->ymdeltatpcm_type==MSM5205){
				sndp->common.output = sndp->common.scale * sndp->common.level32;
			}else{
				sndp->common.output = sndp->common.step * sndp->common.level32;
			}
			sndp->common.output = SSR(sndp->common.output, 8 + 2);
		}
		if(sndp->chmask[NEZ_DEV_ADPCM_CH1]){
			p[0] += sndp->common.output;
			p[1] += sndp->common.output;
		}
	}
}



static void deltad_sndwrite(void *ctx, uint32_t a, uint32_t v)
{
    YMDELTATPCMSOUND *sndp = (YMDELTATPCMSOUND *)ctx;
	sndp->common.regs[a] = (uint8_t)v;
	switch (a)
	{
		/* START,REC,MEMDATA,REPEAT,SPOFF,--,--,RESET */
		case 0x00:	/* Control Register 1 */
			if ((v & 0x80) && !sndp->common.key)
			{
				sndp->common.key = 1;
				sndp->common.play = sndp->common.start;
				sndp->common.step = 0;
				if(sndp->ymdeltatpcm_type==MSM5205){
					sndp->common.scale = 0;
				}else{
					sndp->common.scale = 127;
				}
			}
			if (v & 1) sndp->common.key = 0;
			break;
		/* L,R,-,-,SAMPLE,DA/AD,RAMTYPE,ROM */
		case 0x01:	/* Control Register 2 */	//MSX-AUDIOにADPCM用ROMは無いはずなので無効化
//			sndp->romrambuf  = (sndp->common.regs[1] & 1) ? sndp->rombuf  : sndp->rambuf;
//			sndp->romrammask = (sndp->common.regs[1] & 1) ? sndp->rommask : sndp->rammask;
			break;
		case 0x02:	/* Start Address L */
		case 0x03:	/* Start Address H */
			sndp->common.granuality = (v & 2) ? 1 : 4;
			sndp->common.start = ((sndp->common.regs[3] << 8) + sndp->common.regs[2]) << (sndp->memshift + 1);
			sndp->common.mem = sndp->common.start;
			break;
		case 0x04:	/* Stop Address L */
		case 0x05:	/* Stop Address H */
			sndp->common.stop = ((sndp->common.regs[5] << 8) + sndp->common.regs[4]) << (sndp->memshift + 1);
			break;
		case 0x06:	/* Prescale L */
		case 0x07:	/* Prescale H */
			break;
		case 0x08:	/* Data */
			if ((sndp->common.regs[0] & 0x60) == 0x60) deltat_writeram(sndp, v);
			break;
		case 0x09:	/* Delta-N L */
		case 0x0a:	/* Delta-N H */
			sndp->common.deltan = (sndp->common.regs[0xa] << 8) + sndp->common.regs[0x9];
			if (sndp->common.deltan < 0x100) sndp->common.deltan = 0x100;
			break;
		case 0x0b:	/* Level Control */
			sndp->common.level = (uint8_t)v;
			sndp->common.level32 = ((int32_t)(sndp->common.level * LogToLinear((LOG_TABLE *)&log_table_12_7_30, sndp->common.mastervolume, LOG_LIN_BITS - 15))) >> 7;
			if(sndp->ymdeltatpcm_type==MSM5205){
				sndp->common.output = sndp->common.scale * sndp->common.level32;
			}else{
				sndp->common.output = sndp->common.step * sndp->common.level32;
			}
			sndp->common.output = SSR(sndp->common.output, 8 + 2);
			break;
	}
}

static uint32_t deltat_sndread(void *ctx, uint32_t a)
{
    (void)ctx;
    (void)a;
	return 0;
}

static void deltat_sndreset(void *ctx, uint32_t clock, uint32_t freq)
{
    YMDELTATPCMSOUND *sndp = (YMDELTATPCMSOUND *)ctx;
	XMEMSET(&sndp->common, 0, sizeof(sndp->common));
	sndp->common.cps = DivFix(clock, 72 * freq, CPS_SHIFT);
	sndp->romrambuf  = (sndp->common.regs[1] & 1) ? sndp->rombuf  : sndp->rambuf;
	sndp->romrammask = (sndp->common.regs[1] & 1) ? sndp->rommask : sndp->rammask;
	sndp->common.granuality = 4;
}

static void deltat_sndvolume(void *ctx, int32_t volume)
{
    YMDELTATPCMSOUND *sndp = (YMDELTATPCMSOUND *)ctx;
	volume = (volume << (LOG_BITS - 8)) << 1;
	sndp->common.mastervolume = volume;
	sndp->common.level32 = ((int32_t)(sndp->common.level * LogToLinear((LOG_TABLE *)&log_table_12_7_30, sndp->common.mastervolume, LOG_LIN_BITS - 15))) >> 7;
	sndp->common.output = sndp->common.step * sndp->common.level32;
	sndp->common.output = SSR(sndp->common.output, 8 + 2);
}

static void deltat_sndrelease(void *ctx)
{
    YMDELTATPCMSOUND *sndp = (YMDELTATPCMSOUND *)ctx;
	if (sndp) {
		XFREE(sndp);
	}
}

static void deltat_setinst(void *ctx, uint32_t n, void *p, uint32_t l)
{
    YMDELTATPCMSOUND *sndp = (YMDELTATPCMSOUND *)ctx;
	if (n) return;
	if (p)
	{
		sndp->rombuf  = p;
		sndp->rommask = l - 1;
		sndp->romrambuf  = (sndp->common.regs[1] & 1) ? sndp->rombuf  : sndp->rambuf;
		sndp->romrammask = (sndp->common.regs[1] & 1) ? sndp->rommask : sndp->rammask;
	}
	else
	{
		sndp->rombuf  = 0;
		sndp->rommask = 0;
	}

}

PROTECTED KMIF_SOUND_DEVICE *YMDELTATPCMSoundAlloc(NEZ_PLAY *pNezPlay, uint32_t ymdeltatpcm_type , uint8_t *pcmbuf)
{
	uint32_t ram_size;
	YMDELTATPCMSOUND *sndp;
	switch (ymdeltatpcm_type)
	{
		case YMDELTATPCM_TYPE_Y8950:
			ram_size = 32 * 1024;
			break;
		case YMDELTATPCM_TYPE_YM2608:
			ram_size = 256 * 1024;
			break;
		case MSM5205:
			ram_size = 256 * 256;
			break;
		default:
			ram_size = 0;
			break;
	}
	sndp = XMALLOC(sizeof(YMDELTATPCMSOUND) + ram_size);
	if (!sndp) return 0;

    sndp->chmask = pNezPlay->chmask;
	sndp->ram_size = ram_size;
	sndp->ymdeltatpcm_type = (uint8_t)ymdeltatpcm_type;
	switch (ymdeltatpcm_type)
	{
		case YMDELTATPCM_TYPE_Y8950:
			sndp->memshift = 2;
			break;
		case YMDELTATPCM_TYPE_YM2608:
			/* OPNA */
			sndp->memshift = 6;
			break;
		case YMDELTATPCM_TYPE_YM2610:
			sndp->memshift = 9;
			break;
		case MSM5205:
			sndp->memshift = 0;
			break;
	}
	sndp->kmif.ctx = sndp;
	sndp->kmif.release = deltat_sndrelease;
	sndp->kmif.synth = deltat_sndsynth;
	sndp->kmif.volume = deltat_sndvolume;
	sndp->kmif.reset = deltat_sndreset;
	sndp->kmif.write = deltad_sndwrite;
	sndp->kmif.read = deltat_sndread;
	sndp->kmif.setinst = deltat_setinst;
	/* RAM */
	if(pcmbuf != NULL)
		sndp->rambuf = pcmbuf;
	else
		sndp->rambuf = ram_size ? (uint8_t *)(sndp + 1) : 0;
	sndp->rammask = ram_size ? (ram_size - 1) : 0;
	/* ROM */
	sndp->rombuf = 0;
	sndp->rommask = 0;

	return &sndp->kmif;
}

#undef CPS_SHIFT
#undef PHASE_SHIFT
#undef LIN_BITS
#undef LOG_BITS
#undef LOG_LIN_BITS
#undef SSR
