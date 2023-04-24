#ifndef AUDIOSYS_H__
#define AUDIOSYS_H__

#include "../normalize.h"
#include "../include/nezplug/nezplug.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
   NES_AUDIO_FILTER_NONE,
   NES_AUDIO_FILTER_LOWPASS,
   NES_AUDIO_FILTER_WEIGHTED,
   NES_AUDIO_FILTER_LOWPASS_WEIGHTED
};


PROTECTED void NESAudioRender(NEZ_PLAY*, float *bufp, uint32_t buflen);
PROTECTED void NESAudioHandlerInstall(NEZ_PLAY *, const NEZ_NES_AUDIO_HANDLER * ph);
PROTECTED void NESAudioHandlerTerminate(NEZ_PLAY *);
PROTECTED void NESAudioFrequencySet(NEZ_PLAY *, uint32_t freq);
PROTECTED uint32_t NESAudioFrequencyGet(NEZ_PLAY *);
PROTECTED void NESAudioChannelSet(NEZ_PLAY *, uint32_t ch);
PROTECTED uint32_t NESAudioChannelGet(NEZ_PLAY *);
PROTECTED void NESAudioHandlerInitialize(NEZ_PLAY *);
PROTECTED void NESVolumeHandlerInstall(NEZ_PLAY *, const NEZ_NES_VOLUME_HANDLER * ph);
PROTECTED void NESVolumeHandlerTerminate(NEZ_PLAY *);
PROTECTED void NESVolume(NEZ_PLAY *, uint32_t volume);
PROTECTED void NESAudioFilterSet(NEZ_PLAY*, uint32_t filter);

#ifdef __cplusplus
}
#endif

#endif /* AUDIOSYS_H__ */
