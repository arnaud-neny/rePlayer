#ifndef S_SCC_H__
#define S_SCC_H__

#include "../include/nezplug/nezplug.h"
#include "kmsnddev.h"

#ifdef __cplusplus
extern "C" {
#endif

PROTECTED KMIF_SOUND_DEVICE *SCCSoundAlloc(NEZ_PLAY *pNezPlay);

#ifdef __cplusplus
}
#endif

#endif /* S_SCC_H__ */
