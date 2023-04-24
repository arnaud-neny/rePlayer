#ifndef TRACKINFO_H
#define TRACKINFO_H

#include "../normalize.h"
#include "../include/nezplug/nezplug.h"

#ifdef __cplusplus
extern "C" {
#endif

PROTECTED NEZ_TRACKS *TRACKS_New(uint32_t total);
PROTECTED void TRACKS_Delete(NEZ_TRACKS *);
PROTECTED uint8_t TRACKS_LoadM3U(NEZ_PLAY *, const uint8_t *data, uint32_t length);


#ifdef __cplusplus
}
#endif



#endif
