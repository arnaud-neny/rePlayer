#ifndef S_DMG_H__
#define S_DMG_H__

#include "../include/nezplug/nezplug.h"
#include "kmsnddev.h"

#ifdef __cplusplus
extern "C" {
#endif

PROTECTED KMIF_SOUND_DEVICE *DMGSoundAlloc(NEZ_PLAY *pNezPlay);

#ifdef __cplusplus
}
#endif

#endif /* S_DMG_H__ */
