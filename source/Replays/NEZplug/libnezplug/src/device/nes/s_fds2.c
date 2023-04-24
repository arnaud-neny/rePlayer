#ifndef S_FDS2_C__
#define S_FDS2_C__

#include "../../normalize.h"
#include "../kmsnddev.h"
#include "../../format/audiosys.h"
#include "../../format/nsf6502.h"
#include "../../format/m_nsf.h"
#include "s_fds.h"
#include "../../common/divfix.h"

#define NES_BASECYCLES (21477270)
#define CPS_BITS (16)
#define CPF_BITS (12)
#define LOG_BITS (12)

typedef struct {
	uint32_t phase;
	uint32_t count;
	uint8_t spd;			/* fader rate */
	uint8_t disable;		/* fader disable */
	uint8_t direction;	/* fader direction */
	int8_t volume;		/* volume */
} FDS2_FADER;

typedef struct {
	uint32_t cps;		/* cycles per sample */
	uint32_t cpf;		/* cycles per frame */
	uint32_t phase;	/* wave phase */
	uint32_t spd;		/* wave speed */
	uint32_t pt;		/* programmable timer */
	int32_t ofs1;
	int32_t ofs2;
	int32_t ofs3;
	int32_t input;
	FDS2_FADER fd;
	uint8_t fp;		/* frame position; */
	uint8_t lvl;
	uint8_t disable;
	uint8_t disable2;
	int8_t wave[0x40];
} FDS2_FMOP;

typedef struct FDS2_SOUND {
	uint32_t mastervolume;
	FDS2_FMOP op[2];
	uint8_t mute;
	uint8_t reg[0x10];
	uint8_t waveaddr;
} FDS2_SOUND;

static int32_t FDS2SoundOperatorRender(FDS2_FMOP *op)
{
	int32_t spd;
	if (op->disable) return 0;

	if (!op->disable2 && !op->fd.disable)
	{
		uint32_t fdspd;
		op->fd.count += op->cpf;
		fdspd = ((uint32_t)op->fd.spd + 1) << (CPF_BITS + 11);
		if (op->fd.direction)
		{
			while (op->fd.count >= fdspd)
			{
				op->fd.count -= fdspd;
				op->fd.volume += (op->fd.volume < 0x3f);
			}
		}
		else
		{
			while (op->fd.count >= fdspd)
			{
				op->fd.count -= fdspd;
				op->fd.volume -= (op->fd.volume > 0);
			}
		}
	}

	op->pt += op->cps;
	if (op->spd <= 0x20) return 0;
	spd = op->spd + op->input + op->ofs1;
	/* if (spd > (1 << 24)) spd = (1 << 24); */
	op->phase += spd * (op->pt >> CPS_BITS);
	op->pt &= ((1 << CPS_BITS) - 1);

	return op->ofs3 + ((int32_t)op->wave[(op->phase >> 24) & 0x3f] + op->ofs2) * (int32_t)op->fd.volume;
}

static int32_t FDS2SoundRender(NEZ_PLAY *pNezPlay)
{
	FDS2_SOUND *fdssound = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->fdssound;
	fdssound->op[1].input = 0;
	fdssound->op[0].input = FDS2SoundOperatorRender(&fdssound->op[1]) << (11 - fdssound->op[1].lvl);
	return (FDS2SoundOperatorRender(&fdssound->op[0]) << (9 + 2 - fdssound->op[0].lvl));
}

static const NEZ_NES_AUDIO_HANDLER s_fds2_audio_handler[] =
{
	{ 1, FDS2SoundRender, NULL, NULL }, 
	{ 0, 0, NULL, NULL }, 
};

static void FDS2SoundVolume(NEZ_PLAY *pNezPlay, uint32_t volume)
{
	FDS2_SOUND *fdssound = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->fdssound;
	fdssound->mastervolume = (volume << (LOG_BITS - 8)) << 1;
}

static const NEZ_NES_VOLUME_HANDLER s_fds2_volume_handler[] = {
	{ FDS2SoundVolume, NULL }, 
	{ 0, NULL }, 
};

static void FDS2SoundWrite(void *pNezPlay, uint32_t address, uint32_t value)
{
	FDS2_SOUND *fdssound = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->fdssound;
	if (0x4040 <= address && address <= 0x407F)
	{
		fdssound->op[0].wave[address - 0x4040] = ((int32_t)(value & 0x3f)) - 0x20;
	}
	else if (0x4080 <= address && address <= 0x408F)
	{
		int ch = (address >= 0x4084);
		FDS2_FMOP *pop = &fdssound->op[ch];
		fdssound->reg[address - 0x4080] = (uint8_t)value;
		switch (address & 15)
		{
			case 0:	case 4:
				pop->fd.disable = (uint8_t)(value & 0x80);
				if (pop->fd.disable)
				{
					pop->fd.volume = (value & 0x3f);
				}
				else
				{
					pop->fd.direction = (uint8_t)(value & 0x40);
					pop->fd.spd = (uint8_t)(value & 0x3f);
				}
				break;
			case 5:
				{
					int32_t dat;
					if ((value & 0x7f) < 0x60)
						dat = value & 0x7f;
					else
						dat = ((int32_t)(value & 0x7f)) - 0x80;
					switch (((NEZ_PLAY *)pNezPlay)->nes_config.fds_debug_option1)
					{
						default:
						case 1: pop->ofs1 = dat << ((NEZ_PLAY *)pNezPlay)->nes_config.fds_debug_option2; break;
						case 2: pop->ofs2 = dat >> ((NEZ_PLAY *)pNezPlay)->nes_config.fds_debug_option2; break;
						case 3: pop->ofs3 = dat << ((NEZ_PLAY *)pNezPlay)->nes_config.fds_debug_option2; break;
					}
				}
				break;
			case 2:	case 6:
				pop->spd &= 0x000FF0000;
				pop->spd |= (value & 0xFF) << 8;
				break;
			case 3:
				pop->spd &= 0x0000FF00;
				pop->spd |= (value & 0xF) << 16;
				pop->disable = (uint8_t)(value & 0x80);
				break;
			case 7:
				pop->spd &= 0x0000FF00;
				pop->spd |= (value & 0x7F) << 16;
				pop->disable = (uint8_t)(value & 0x80);
				break;
			case 8:
				{
					static int8_t lfotbl[8] = { 0,2,4,6,-8,-6,-4,-2 };
					int8_t v = lfotbl[value & 7];
					fdssound->op[1].wave[fdssound->waveaddr++] = v;
					fdssound->op[1].wave[fdssound->waveaddr++] = v;
					if (fdssound->waveaddr == 0x40) fdssound->waveaddr = 0;
				}
				break;
			case 9:
				fdssound->op[0].lvl = (uint8_t)(value & 3);
				fdssound->op[0].disable2 = (uint8_t)(value & 0x80);
				break;
			case 10:
				fdssound->op[1].lvl = (uint8_t)(value & 3);
				fdssound->op[1].disable2 = (uint8_t)(value & 0x80);
				break;
		}
	}
}

static const NES_WRITE_HANDLER s_fds2_write_handler[] =
{
	{ 0x4040, 0x408F, FDS2SoundWrite, NULL },
	{ 0,      0,      0, NULL },
};

static uint32_t FDS2SoundRead(void *pNezPlay, uint32_t address)
{
	FDS2_SOUND *fdssound = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->fdssound;
	if (0x4090 <= address && address <= 0x409F)
	{
		return fdssound->reg[address - 0x4090];
	}
	return 0;
}

static const NES_READ_HANDLER s_fds2_read_handler[] =
{
	{ 0x4090, 0x409F, FDS2SoundRead, NULL },
	{ 0,      0,      0, NULL },
};

static void FDS2SoundReset(NEZ_PLAY *pNezPlay)
{
	FDS2_SOUND *fdssound = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->fdssound;
	uint32_t i, cps, cpf;
	XMEMSET(fdssound, 0, sizeof(FDS2_SOUND));
	cps = DivFix(NES_BASECYCLES, 12 * NESAudioFrequencyGet(pNezPlay), CPS_BITS);
	cpf = DivFix(NES_BASECYCLES, 12 * NESAudioFrequencyGet(pNezPlay), CPF_BITS);
	fdssound->op[0].cps = fdssound->op[1].cps = cps;
	fdssound->op[0].cpf = fdssound->op[1].cpf = cpf;
	fdssound->op[0].lvl = fdssound->op[1].lvl = 0;
#if 0
	fdssound->op[0].fd.disable = fdssound->op[1].fd.disable = 1;
	fdssound->op[0].disable = fdssound->op[1].disable = 1;
#endif

	for (i = 0; i < 0x40; i++)
	{
		fdssound->op[0].wave[i] = (i < 0x20) ? 0x1f : -0x20;
		fdssound->op[1].wave[i] = 0;
	}
}

static const NEZ_NES_RESET_HANDLER s_fds2_reset_handler[] =
{
	{ NES_RESET_SYS_NOMAL, FDS2SoundReset, NULL }, 
	{ 0,                   0, NULL }, 
};

static void FDS2SoundTerm(NEZ_PLAY *pNezPlay)
{
	FDS2_SOUND *fdssound = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->fdssound;
	if (fdssound)
		XFREE(fdssound);
}

static const NEZ_NES_TERMINATE_HANDLER s_fds2_terminate_handler[] = {
	{ FDS2SoundTerm, NULL },
	{ 0, NULL },
};

PROTECTED void FDS2SoundInstall(NEZ_PLAY *pNezPlay)
{
	FDS2_SOUND *fdssound;
	fdssound = XMALLOC(sizeof(FDS2_SOUND));
	if (!fdssound) return;
	XMEMSET(fdssound, 0, sizeof(FDS2_SOUND));
	((NSFNSF*)pNezPlay->nsf)->fdssound = fdssound;

	NESAudioHandlerInstall(pNezPlay, s_fds2_audio_handler);
	NESVolumeHandlerInstall(pNezPlay, s_fds2_volume_handler);
	NESTerminateHandlerInstall(&pNezPlay->nth, s_fds2_terminate_handler);
	NESReadHandlerInstall(pNezPlay, s_fds2_read_handler);
	NESWriteHandlerInstall(pNezPlay, s_fds2_write_handler);
	NESResetHandlerInstall(pNezPlay->nrh, s_fds2_reset_handler);
}

#undef NES_BASECYCLES
#undef CPS_BITS
#undef CPF_BITS
#undef LOG_BITS

#endif
