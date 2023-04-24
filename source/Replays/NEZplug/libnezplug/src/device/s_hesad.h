#ifndef S_HESAD_H__
#define S_HESAD_H__

#include "../include/nezplug/nezplug.h"
#include "kmsnddev.h"

#ifdef __cplusplus
extern "C" {
#endif

PROTECTED KMIF_SOUND_DEVICE *HESAdPcmAlloc(NEZ_PLAY *pNezPlay);

#ifdef __cplusplus
}
#endif

#endif /* S_HESAD_H__ */
