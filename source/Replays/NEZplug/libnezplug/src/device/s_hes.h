#ifndef S_HES_H__
#define S_HES_H__

#include "../include/nezplug/nezplug.h"
#include "kmsnddev.h"

#ifdef __cplusplus
extern "C" {
#endif

PROTECTED KMIF_SOUND_DEVICE *HESSoundAlloc(NEZ_PLAY *pNezPlay);

#ifdef __cplusplus
}
#endif

#endif /* S_HES_H__ */
