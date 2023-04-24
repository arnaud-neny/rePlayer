#ifndef S_FDS1_C_
#define S_FDS1_C_

#include "../../normalize.h"
#include "../kmsnddev.h"
#include "../../format/audiosys.h"
#include "../../format/nsf6502.h"
#include "../logtable.h"
#include "../../format/m_nsf.h"
#include "s_fds.h"
#include "../../common/divfix.h"
#include "../../logtable/log_table.h"

#define NES_BASECYCLES (21477270)
#define CPS_SHIFT (23)
#define PHASE_SHIFT (23)
#define FADEOUT_SHIFT 11/*(11)*/
#define XXX_SHIFT 1 /* 3 */

typedef struct {
	uint32_t wave[0x40];
	uint32_t envspd;
	int32_t envphase;
	uint32_t envout;
	uint32_t outlvl;

	uint32_t phase;
	uint32_t spd;
	uint32_t volume;
	int32_t sweep;

	uint8_t enable;
	uint8_t envmode;
	uint8_t xxxxx;
	uint8_t xxxxx2;
} FDS1_FMOP;

typedef struct FDS1_SOUND {
	uint32_t cps;
	int32_t cycles;
	uint32_t mastervolume;
	int32_t output;

	FDS1_FMOP op[2];

	uint32_t waveaddr;
	uint8_t mute;
	uint8_t key;
	uint8_t reg[0x10];
} FDS1_SOUND;

static int32_t FDS1SoundRender(NEZ_PLAY *pNezPlay)
{
	FDS1_FMOP *pop;
	FDS1_SOUND *fdssound = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->fdssound;

	for (pop = &fdssound->op[0]; pop < &fdssound->op[2]; pop++)
	{
		uint32_t vol;
		if (pop->envmode)
		{
			pop->envphase -= fdssound->cps >> (FADEOUT_SHIFT - XXX_SHIFT);
			if (pop->envmode & 0x40)
				while (pop->envphase < 0)
				{
					pop->envphase += pop->envspd;
					pop->volume += (pop->volume < 0x1f);
				}
			else
				while (pop->envphase < 0)
				{
					pop->envphase += pop->envspd;
					pop->volume -= (pop->volume > 0x00);
				}
		}
		vol = pop->volume;
		if (vol)
		{
			vol += pop->sweep;
			if (vol > 0x3f)
				vol = 0x3f;
		}
		pop->envout = LinearToLog((LOG_TABLE *)&log_table_12_8_30,vol);
	}
	fdssound->op[1].envout += fdssound->mastervolume;

	fdssound->cycles -= fdssound->cps;
	while (fdssound->cycles < 0)
	{
		fdssound->cycles += 1 << CPS_SHIFT;
		fdssound->output = 0;
		for (pop = &fdssound->op[0]; pop < &fdssound->op[2]; pop++)
		{
			if (!pop->spd || !pop->enable)
			{
				fdssound->output = 0;
				continue;
			}
			pop->phase += pop->spd + fdssound->output;
			fdssound->output = LogToLinear((LOG_TABLE *)&log_table_12_8_30,pop->envout + pop->wave[(pop->phase >> (PHASE_SHIFT - XXX_SHIFT)) & 0x3f], pop->outlvl);
		}
	}
	if (fdssound->mute) return 0;
	return fdssound->output;
}

static const NEZ_NES_AUDIO_HANDLER s_fds1_audio_handler[] =
{
	{ 1, FDS1SoundRender, NULL , NULL}, 
	{ 0, 0, NULL, NULL }, 
};

static void FDS1SoundVolume(NEZ_PLAY *pNezPlay, uint32_t volume)
{
	FDS1_SOUND *fdssound = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->fdssound;
	fdssound->mastervolume = (volume << (log_table_12_8_30.log_bits - 8)) << 1;
}

static const NEZ_NES_VOLUME_HANDLER s_fds1_volume_handler[] = {
	{ FDS1SoundVolume, NULL }, 
	{ 0, NULL }, 
};

static void FDS1SoundWrite(void *pNezPlay, uint32_t address, uint32_t value)
{
	FDS1_SOUND *fdssound = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->fdssound;
	if (0x4040 <= address && address <= 0x407F)
	{
		fdssound->op[1].wave[address - 0x4040] = LinearToLog((LOG_TABLE *)&log_table_12_8_30,((int32_t)value & 0x3f) - 0x20);
	}
	else if (0x4080 <= address && address <= 0x408F)
	{
		int ch = (address < 0x4084);
		FDS1_FMOP *pop = &fdssound->op[ch];
		fdssound->reg[address - 0x4080] = (uint8_t)value;
		switch (address & 15)
		{
			case 0:	case 4:
				if (value & 0x80)
				{
					pop->volume = (value & 0x3f);
					pop->envmode = 0;
				}
				else
				{
					pop->envspd = ((value & 0x3f) + 1) << CPS_SHIFT;
					pop->envmode = (uint8_t)(0x80 | value);
				}
				break;
			case 1:	case 5:
				if (!value) break;
				if ((value & 0x7f) < 0x60)
					pop->sweep = value & 0x7f;
				else
					pop->sweep = ((int32_t)value & 0x7f) - 0x80;
				break;
			case 2:	case 6:
				pop->spd &= 0x00000F00 << 7;
				pop->spd |= (value & 0xFF) << 7;
				break;
			case 3:	case 7:
				pop->spd &= 0x000000FF << 7;
				pop->spd |= (value & 0x0F) << (7 + 8);
				pop->enable = !(value & 0x80);
				break;
			case 8:
				{
					static int8_t lfotbl[8] = { 0,1,2,3,-4,-3,-2,-1 };
					uint32_t v = LinearToLog((LOG_TABLE *)&log_table_12_8_30,lfotbl[value & 7]);
					fdssound->op[0].wave[fdssound->waveaddr++] = v;
					fdssound->op[0].wave[fdssound->waveaddr++] = v;
					if (fdssound->waveaddr == 0x40)
					{
						fdssound->waveaddr = 0;
					}
				}
				break;
			case 9:
				fdssound->op[0].outlvl = log_table_12_8_30.log_lin_bits - log_table_12_8_30.lin_bits - log_table_12_8_30.lin_bits - 10 - (value & 3);
				break;
			case 10:
				fdssound->op[1].outlvl = log_table_12_8_30.log_lin_bits - log_table_12_8_30.lin_bits - log_table_12_8_30.lin_bits - 10 - (value & 3);
				break;
		}
	}
}

static const NES_WRITE_HANDLER s_fds1_write_handler[] =
{
	{ 0x4040, 0x408F, FDS1SoundWrite, NULL },
	{ 0,      0,      0, NULL },
};

static uint32_t FDS1SoundRead(void *pNezPlay, uint32_t address)
{
	FDS1_SOUND *fdssound = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->fdssound;
	if (0x4090 <= address && address <= 0x409F)
	{
		return fdssound->reg[address - 0x4090];
	}
	return 0;
}

static const NES_READ_HANDLER s_fds1_read_handler[] =
{
	{ 0x4090, 0x409F, FDS1SoundRead, NULL },
	{ 0,      0,      0, NULL },
};

static void FDS1SoundReset(NEZ_PLAY *pNezPlay)
{
	FDS1_SOUND *fdssound = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->fdssound;
	int32_t i;
	FDS1_FMOP *pop;
	XMEMSET(fdssound, 0, sizeof(FDS1_SOUND));
	fdssound->cps = DivFix(NES_BASECYCLES, 12 * (1 << XXX_SHIFT) * NESAudioFrequencyGet(pNezPlay), CPS_SHIFT);
	for (pop = &fdssound->op[0]; pop < &fdssound->op[2]; pop++)
	{
		pop->enable = 1;
	}
	fdssound->op[0].outlvl = log_table_12_8_30.log_lin_bits - log_table_12_8_30.lin_bits - log_table_12_8_30.lin_bits - 10;
	fdssound->op[1].outlvl = log_table_12_8_30.log_lin_bits - log_table_12_8_30.lin_bits - log_table_12_8_30.lin_bits - 10;
	for (i = 0; i < 0x40; i++)
	{
		fdssound->op[1].wave[i] = LinearToLog((LOG_TABLE *)&log_table_12_8_30,(i < 0x20)?0x1f:-0x20);
	}
}

static const NEZ_NES_RESET_HANDLER s_fds1_reset_handler[] =
{
	{ NES_RESET_SYS_NOMAL, FDS1SoundReset, NULL }, 
	{ 0,                   0, NULL }, 
};

static void FDS1SoundTerm(NEZ_PLAY *pNezPlay)
{
	FDS1_SOUND *fdssound = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->fdssound;
	if (fdssound) {
		XFREE(fdssound);
    }
}

static const NEZ_NES_TERMINATE_HANDLER s_fds1_terminate_handler[] = {
	{ FDS1SoundTerm, NULL }, 
	{ 0, NULL }, 
};

PROTECTED void FDS1SoundInstall(NEZ_PLAY *pNezPlay)
{
	FDS1_SOUND *fdssound;
	fdssound = XMALLOC(sizeof(FDS1_SOUND));
	if (!fdssound) return;
	XMEMSET(fdssound, 0, sizeof(FDS1_SOUND));
	((NSFNSF*)pNezPlay->nsf)->fdssound = fdssound;

	NESAudioHandlerInstall(pNezPlay, s_fds1_audio_handler);
	NESVolumeHandlerInstall(pNezPlay, s_fds1_volume_handler);
	NESTerminateHandlerInstall(&pNezPlay->nth, s_fds1_terminate_handler);
	NESReadHandlerInstall(pNezPlay, s_fds1_read_handler);
	NESWriteHandlerInstall(pNezPlay, s_fds1_write_handler);
	NESResetHandlerInstall(pNezPlay->nrh, s_fds1_reset_handler);
}

#undef NES_BASECYCLES
#undef CPS_SHIFT
#undef PHASE_SHIFT
#undef FADEOUT_SHIFT
#undef XXX_SHIFT

#endif
