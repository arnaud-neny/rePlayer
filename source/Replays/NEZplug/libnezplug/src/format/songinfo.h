#ifndef SONGINFO_H__
#define SONGINFO_H__

#include "../normalize.h"
#include "../include/nezplug/nezplug.h"

#ifdef __cplusplus
extern "C" {
#endif

PROTECTED NEZ_SONG_INFO* SONGINFO_New();
PROTECTED void SONGINFO_Delete(NEZ_SONG_INFO *info);
PROTECTED uint32_t SONGINFO_GetSongNo(NEZ_SONG_INFO*);
PROTECTED void SONGINFO_SetSongNo(NEZ_SONG_INFO*, uint32_t v);
PROTECTED uint32_t SONGINFO_GetStartSongNo(NEZ_SONG_INFO*);
PROTECTED void SONGINFO_SetStartSongNo(NEZ_SONG_INFO*, uint32_t v);
PROTECTED uint32_t SONGINFO_GetMaxSongNo(NEZ_SONG_INFO*);
PROTECTED void SONGINFO_SetMaxSongNo(NEZ_SONG_INFO*, uint32_t v);
PROTECTED uint32_t SONGINFO_GetExtendDevice(NEZ_SONG_INFO*);
PROTECTED void SONGINFO_SetExtendDevice(NEZ_SONG_INFO*, uint32_t v);
PROTECTED uint32_t SONGINFO_GetInitAddress(NEZ_SONG_INFO*);
PROTECTED void SONGINFO_SetInitAddress(NEZ_SONG_INFO*, uint32_t v);
PROTECTED uint32_t SONGINFO_GetPlayAddress(NEZ_SONG_INFO*);
PROTECTED void SONGINFO_SetPlayAddress(NEZ_SONG_INFO*, uint32_t v);
PROTECTED uint32_t SONGINFO_GetChannel(NEZ_SONG_INFO*);
PROTECTED void SONGINFO_SetChannel(NEZ_SONG_INFO*, uint32_t v);

#ifdef __cplusplus
}
#endif

#endif /* SONGINFO_H__ */
