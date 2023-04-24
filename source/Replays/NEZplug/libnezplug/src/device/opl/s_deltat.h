#ifndef S_DELTAT_H__
#define S_DELTAT_H__

#include "../../include/nezplug/nezplug.h"
#include "../kmsnddev.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	/* MSX-AUDIO   */ YMDELTATPCM_TYPE_Y8950,
	/* OPNA ADPCM  */ YMDELTATPCM_TYPE_YM2608,
	/* OPNB ADPCMB */ YMDELTATPCM_TYPE_YM2610,
	/* PCE-ADPCM   */ MSM5205
};

PROTECTED KMIF_SOUND_DEVICE *YMDELTATPCMSoundAlloc(NEZ_PLAY *pNezPlay, uint32_t ymdeltatpcm_type , uint8_t *pcmbuf);

#ifdef __cplusplus
}
#endif

#endif /* S_DELTAT_H__ */

