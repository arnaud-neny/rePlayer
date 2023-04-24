#include "../include/nezplug/nezplug.h"
#include "handler.h"
#include "audiosys.h"
#include "songinfo.h"
#include "nsf6502.h"
#include "m_nsf.h"
#include "m_nsd.h"
#include "../common/divfix.h"
#include "../common/util.h"

#if 0
PROTECTED uint32_t NSDPlayerGetCycles(void)
{
	return 0;
}
#endif

#define SHIFT_CPS 16
#define NES_BASECYCLES (21477270)

typedef struct
{
	uint8_t isplaying;
	uint8_t sync0;
	uint8_t sync1;
	uint32_t sync2;
	uint32_t cleft;
	uint32_t cps;
	uint32_t cpfc;
	uint32_t cpf;
	uint32_t remainsyncs;
	uint32_t total_cycles;
	uint8_t *top;
	uint8_t *loop;
	uint8_t *current;
} NSDSEQ;

static __inline uint32_t NSDPlayStep(NEZ_PLAY *pNezPlay)
{
	NSDSEQ *nsdplayer = pNezPlay->nsdp;
	while (1)
	{
		uint8_t c;
		uint32_t r;
		c = *nsdplayer->current++;
		switch (c)
		{
			case 0xFF:
				return 1;
			case 0xFE:
				r = 0;
				do
				{
					r <<= 7;
					r |= *nsdplayer->current & 0x7F;
				} while (*nsdplayer->current++ & 0x80);
				r += 3;
				return r;
			case 0xFD:
				if (nsdplayer->loop)
				{
					nsdplayer->current = nsdplayer->loop;
				}
				else
				{
					nsdplayer->isplaying = 0;
					return 0;
				}
				break;
			case 0x1F:
				/* VRC7 */
				NES6502Write(pNezPlay, 0x9010, *nsdplayer->current++);
				NES6502Write(pNezPlay, 0x9030, *nsdplayer->current++);
				break;
			case 0x36:
				/* N106 */
				NES6502Write(pNezPlay, 0xF800, *nsdplayer->current++);
				NES6502Write(pNezPlay, 0x4800, *nsdplayer->current++);
				break;
			case 0x37:
				/* FME7 */
				NES6502Write(pNezPlay, 0xC000, *nsdplayer->current++);
				NES6502Write(pNezPlay, 0xE000, *nsdplayer->current++);
				break;
			case 0x3C:
			case 0x3D:
			case 0x3E:
			case 0x3F:
				NES6502Write(pNezPlay, 0x5FF0 + (c & 0x0F), *nsdplayer->current++);
				break;
			default:
				if ((c <= 0x15) || (0x40 <= c && c <= 0x8F))
				{
					/* APU FDS */
					NES6502Write(pNezPlay, 0x4000 + c, *nsdplayer->current++);
				}
				else if (0x16 <= c && c <= 0x1E)
				{
					/* VRC6 */
					uint32_t adr, ch;
					ch = (c - 0x16) / 3;
					adr = 0x9000 + 0x1000 * ch + (c - 0x16) - 3 * ch;
					NES6502Write(pNezPlay, adr, *nsdplayer->current++);
				}
				else if (0x20 <= c && c <= 0x35)
				{
					/* MMC5 */
					NES6502Write(pNezPlay, 0x5000 + c - 0x20, *nsdplayer->current++);
				}
				break;
		}
	}
}

static void NSDPlayCycles(NEZ_PLAY *pNezPlay, uint32_t syncs)
{
	NSDSEQ *nsdplayer = pNezPlay->nsdp;
	uint32_t r = 0;
	if (!nsdplayer->isplaying) return;
	if (nsdplayer->remainsyncs > syncs)
	{
		nsdplayer->remainsyncs -= syncs;
		return;
	}
	syncs -= nsdplayer->remainsyncs;
	do
	{
		r += NSDPlayStep(pNezPlay);
	} while (nsdplayer->isplaying && r < syncs);
	nsdplayer->remainsyncs = r - syncs;
}

static int32_t ExecuteNSD(NEZ_PLAY *pNezPlay)
{
	NSDSEQ *nsdplayer = ((NEZ_PLAY*)pNezPlay)->nsdp;
	uint32_t cycles;
	nsdplayer->cleft += nsdplayer->cps;
	cycles = nsdplayer->cleft >> SHIFT_CPS;
	if (!nsdplayer->sync0 && cycles) NSDPlayCycles(pNezPlay, cycles);
	nsdplayer->cleft &= (1 << SHIFT_CPS) - 1;
	if (nsdplayer->sync0)
	{
		nsdplayer->cpfc += cycles;
		if (nsdplayer->cpfc > nsdplayer->cpf)
		{
			nsdplayer->cpfc -= nsdplayer->cpf;
			NSDPlayCycles(pNezPlay, 1);
		}
	}
	nsdplayer->total_cycles += cycles;
	return 0;
}

static const NEZ_NES_AUDIO_HANDLER nsdplay_audio_handler[] = {
	{ 0, ExecuteNSD, NULL, NULL },
	{ 0, 0, NULL, NULL },
};

static void NSDPLAYReset(NEZ_PLAY *pNezPlay)
{
	NSDSEQ *nsdplayer = pNezPlay->nsdp;
	uint32_t freq = NESAudioFrequencyGet(pNezPlay);
	nsdplayer->cleft = 0;
	nsdplayer->cpfc = 0;
	nsdplayer->remainsyncs = 0;
	nsdplayer->total_cycles = 0;
	nsdplayer->current = nsdplayer->top;
	if (nsdplayer->sync2)
	{
		nsdplayer->cps = DivFix(nsdplayer->sync2, (256 - nsdplayer->sync1) * freq, SHIFT_CPS);
		nsdplayer->cpf = 0;
		nsdplayer->sync0 = 0;
	}
	else
	{
		nsdplayer->cps = DivFix(NES_BASECYCLES, 12 * freq, SHIFT_CPS);
		switch (nsdplayer->sync1)
		{
		default:
			nsdplayer->cpf = 0;
			nsdplayer->sync0 = 0;
			break;
		case 1:
			nsdplayer->cpf = DivFix(NES_BASECYCLES, 12 * 60, 0);
			nsdplayer->sync0 = 1;
			break;
		case 2:
			nsdplayer->cpf = DivFix(NES_BASECYCLES, 12 * 50, 0);
			nsdplayer->sync0 = 1;
			break;
		}
	}
	nsdplayer->isplaying = 1;
}

static const NEZ_NES_RESET_HANDLER nsdplay_reset_handler[] = {
	{ NES_RESET_SYS_LAST, NSDPLAYReset, NULL },
	{ 0,                  0, NULL },
};

static void NSDPLAYTerminate(NEZ_PLAY *pNezPlay)
{
	NSDSEQ *nsdplayer = ((NEZ_PLAY*)pNezPlay)->nsdp;
	if (nsdplayer)
	{
		if (nsdplayer->top)
		{
			XFREE(nsdplayer->top);
			nsdplayer->top = 0;
		}
		XFREE(nsdplayer);
		((NEZ_PLAY*)pNezPlay)->nsdp = 0;
	}
}

static const NEZ_NES_TERMINATE_HANDLER nsdplay_terminate_handler[] = {
	{ NSDPLAYTerminate, NULL },
	{ 0, NULL },
};

PROTECTED uint32_t NSDPlayerInstall(NEZ_PLAY *pNezPlay, const uint8_t *pData, uint32_t uSize)
{
    (void)uSize;
	NSDSEQ *nsdplayer = pNezPlay->nsdp;
	nsdplayer->sync1 = pData[7];
	nsdplayer->sync2 = GetDwordLE(pData + 0x08);
	nsdplayer->top = XMALLOC(GetDwordLE(pData + 0x30));
	if (!nsdplayer->top) return NEZ_NESERR_SHORTOFMEMORY;

	XMEMCPY(nsdplayer->top, pData + GetDwordLE(pData + 0x38), GetDwordLE(pData + 0x30));
	if (GetDwordLE(pData + 0x3C))
	{
		nsdplayer->loop = nsdplayer->top + GetDwordLE(pData + 0x3C) - GetDwordLE(pData + 0x38);
	}
	else
	{
		nsdplayer->loop = 0;
	}
	NESAudioHandlerInstall(pNezPlay, nsdplay_audio_handler);
	NESResetHandlerInstall(pNezPlay->nrh, nsdplay_reset_handler);
	NESTerminateHandlerInstall(&pNezPlay->nth, nsdplay_terminate_handler);
	NSDPLAYReset(pNezPlay);
	return NEZ_NESERR_NOERROR;
}

PROTECTED uint32_t NSDLoad(NEZ_PLAY *pNezPlay, const uint8_t *pData, uint32_t uSize)
{
	NSFNSF *nsf;
	uint32_t ret;
	pNezPlay->nsf = nsf = (NSFNSF *)XMALLOC(sizeof(NSFNSF));
	if (!pNezPlay->nsf)
		return NEZ_NESERR_SHORTOFMEMORY;
	pNezPlay->nsdp = (NSDSEQ *)XMALLOC(sizeof(NSDSEQ));
	if (!pNezPlay->nsdp) {
		free(pNezPlay->nsf);
		pNezPlay->nsf = 0;
		return NEZ_NESERR_SHORTOFMEMORY;
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
	NESMemoryHandlerInitialize(pNezPlay);
	XMEMSET(nsf->head, 0, 0x80);
	SONGINFO_SetStartSongNo(pNezPlay->song, 1);
	SONGINFO_SetMaxSongNo(pNezPlay->song, 1);
	SONGINFO_SetExtendDevice(pNezPlay->song, pData[0x0C]);
	SONGINFO_SetInitAddress(pNezPlay->song, 0);
	SONGINFO_SetPlayAddress(pNezPlay->song, 0);
	SONGINFO_SetChannel(pNezPlay->song, 1);
	if (GetDwordLE(pData + 0x28))
	{
		const uint8_t *src = pData + GetDwordLE(pData + 0x28);
		uint8_t *des;
		nsf->banksw = 1;				/* bank sw on */
		nsf->banknum = *src++;
		des = nsf->bankbase = XMALLOC((nsf->banknum << 12) + 8);
		if (!nsf->bankbase) return NEZ_NESERR_SHORTOFMEMORY;
		XMEMSET(nsf->bankbase, 0, (nsf->banknum << 12));
		while (*src)
		{
			uint32_t n = 0;
			do
			{
				n <<= 7;
				n |= (*src & 0x7f);
			} while (*src++ & 0x80);
			if (n & 1)
			{
				n >>= 1;
				XMEMCPY(des, src, n);
				src += n;
				des += n;
			}
			else
			{
				n >>= 1;
				des += n;
			}
		}
	}
	else
	{
		nsf->banksw = 0;
		nsf->banknum = 0;
		nsf->bankbase = 0;
	}
	ret = NSDPlayerInstall(pNezPlay, pData, uSize);
	if (ret) return ret;
	ret = NSFDeviceInitialize(pNezPlay);
	if (ret) return ret;
	SONGINFO_SetSongNo(pNezPlay->song, SONGINFO_GetStartSongNo(pNezPlay->song));
	return NEZ_NESERR_NOERROR;
}

#undef SHIFT_CPS
#undef NES_BASECYCLES
