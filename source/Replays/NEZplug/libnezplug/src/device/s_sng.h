#ifndef S_SNG_H__
#define S_SNG_H__

#include "../include/nezplug/nezplug.h"
#include "kmsnddev.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	SNG_TYPE_SN76489AN,	/* TI SN76489AN */
	SNG_TYPE_SEGAMKIII,	/* SEGA custom VDP (Mono)      */
	SNG_TYPE_GAMEGEAR	/* SEGA custom VDP (Stereo)    */
};

PROTECTED KMIF_SOUND_DEVICE *SNGSoundAlloc(NEZ_PLAY *pNezPlay, uint32_t sng_type);

#ifdef __cplusplus
}
#endif

#endif /* S_SNG_H__ */
