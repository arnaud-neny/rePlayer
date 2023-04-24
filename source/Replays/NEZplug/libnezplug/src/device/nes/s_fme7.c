#include "../../normalize.h"
#include "../kmsnddev.h"
#include "../../format/audiosys.h"
#include "../../format/handler.h"
#include "../../format/nsf6502.h"
#include "../../format/m_nsf.h"
#include "s_fme7.h"
#include "../s_psg.h"

#define BASECYCLES_ZX  (3579545)/*(1773400)*/
#define BASECYCLES_AMSTRAD  (2000000)
#define BASECYCLES_MSX (3579545)
#define BASECYCLES_NES (21477270)
#define FME7_VOL 4/5

typedef struct {
	KMIF_SOUND_DEVICE *psgp;
	uint8_t adr;
} FME7_FME7PSGSOUND;

static int32_t FME7PSGSoundRender(NEZ_PLAY *pNezPlay)
{
	FME7_FME7PSGSOUND *psgs = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->psgs;
	int32_t b[2] = {0, 0};
	psgs->psgp->synth(psgs->psgp->ctx, b);
	return b[0]*FME7_VOL;
}

static const NEZ_NES_AUDIO_HANDLER s_fme7_psg_audio_handler[] = {
	{ 1, FME7PSGSoundRender, NULL, NULL }, 
	{ 0, 0, 0, NULL }, 
};

static void FME7PSGSoundVolume(NEZ_PLAY *pNezPlay, uint32_t volume)
{
	FME7_FME7PSGSOUND *psgs = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->psgs;
	psgs->psgp->volume(psgs->psgp->ctx, volume);
}

static const NEZ_NES_VOLUME_HANDLER s_fme7_psg_volume_handler[] = {
	{ FME7PSGSoundVolume, NULL }, 
	{ 0, NULL }, 
};

static void FME7PSGSoundWrireAddr(void *pNezPlay, uint32_t address, uint32_t value)
{
    (void)address;
	FME7_FME7PSGSOUND *psgs = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->psgs;
	psgs->adr = (uint8_t)value;
	psgs->psgp->write(psgs->psgp->ctx, 0, value);
}
static void FME7PSGSoundWrireData(void *pNezPlay, uint32_t address, uint32_t value)
{
    (void)address;
	FME7_FME7PSGSOUND *psgs = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->psgs;
	psgs->psgp->write(psgs->psgp->ctx, 1, value);
}

static const NES_WRITE_HANDLER s_fme7_write_handler[] =
{
	{ 0xC000, 0xC000, FME7PSGSoundWrireAddr, NULL },
	{ 0xE000, 0xE000, FME7PSGSoundWrireData, NULL },
	{ 0,      0,      0, NULL },
};

static void FME7SoundReset(NEZ_PLAY *pNezPlay)
{
	FME7_FME7PSGSOUND *psgs = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->psgs;
	psgs->psgp->reset(psgs->psgp->ctx, BASECYCLES_NES / 12, NESAudioFrequencyGet(pNezPlay));
}

static const NEZ_NES_RESET_HANDLER s_fme7_reset_handler[] = {
	{ NES_RESET_SYS_NOMAL, FME7SoundReset, NULL }, 
	{ 0,                   0, NULL }, 
};

static void FME7PSGSoundTerm(NEZ_PLAY *pNezPlay)
{
	FME7_FME7PSGSOUND *psgs = ((NSFNSF*)((NEZ_PLAY*)pNezPlay)->nsf)->psgs;
	if (psgs->psgp)
	{
		psgs->psgp->release(psgs->psgp->ctx);
		psgs->psgp = 0;
	}
	XFREE(psgs);
}

static const NEZ_NES_TERMINATE_HANDLER s_fme7_psg_terminate_handler[] = {
	{ FME7PSGSoundTerm, NULL }, 
	{ 0, NULL }, 
};

PROTECTED void FME7SoundInstall(NEZ_PLAY* pNezPlay)
{
	FME7_FME7PSGSOUND *psgs;
	psgs = XMALLOC(sizeof(FME7_FME7PSGSOUND));
	if (!psgs) return;
	XMEMSET(psgs, 0, sizeof(FME7_FME7PSGSOUND));
	((NSFNSF*)pNezPlay->nsf)->psgs = psgs;

	psgs->psgp = PSGSoundAlloc(pNezPlay, PSG_TYPE_YM2149); //エンベロープ31段階あったんでYM2149系でしょう。
	if (!psgs->psgp) return;

	NESTerminateHandlerInstall(&pNezPlay->nth, s_fme7_psg_terminate_handler);
	NESVolumeHandlerInstall(pNezPlay, s_fme7_psg_volume_handler);

	NESAudioHandlerInstall(pNezPlay, s_fme7_psg_audio_handler);
	NESWriteHandlerInstall(pNezPlay, s_fme7_write_handler);
	NESResetHandlerInstall(pNezPlay->nrh, s_fme7_reset_handler);
}

#undef BASECYCLES_ZX
#undef BASECYCLES_AMSTRAD
#undef BASECYCLES_MSX
#undef BASECYCLES_NES
#undef FME7_VOL
