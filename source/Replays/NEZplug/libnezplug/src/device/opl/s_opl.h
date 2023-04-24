#ifndef S_OPL_H__
#define S_OPL_H__

#include "../../include/nezplug/nezplug.h"
#include "../kmsnddev.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	OPL_TYPE_OPLL		= 0x10,	/* YAMAHA YM2413 */
	OPL_TYPE_MSXMUSIC	= 0x11,	/* YAMAHA YM2413 */
	OPL_TYPE_SMSFMUNIT	= 0x12,	/* YAMAHA YM2413? */
	OPL_TYPE_VRC7		= 0x13,	/* KONAMI 053982 VRV VII */
	OPL_TYPE_OPL		= 0x20,	/* YAMAHA YM3526 */
	OPL_TYPE_MSXAUDIO	= 0x21,	/* YAMAHA Y8950 */
	OPL_TYPE_OPL2		= 0x22	/* YAMAHA YM3812 */
};

PROTECTED KMIF_SOUND_DEVICE *OPLSoundAlloc(NEZ_PLAY *pNezPlay, uint32_t opl_type);

#ifdef __cplusplus
}
#endif

#endif /* S_OPL_H__ */
