#include "songinfo.h"
#include "../normalize.h"

PROTECTED NEZ_SONG_INFO* SONGINFO_New()
{
	NEZ_SONG_INFO *info = (NEZ_SONG_INFO*)XMALLOC(sizeof(NEZ_SONG_INFO));

	if (info != NULL) {
		info->songno = 1;
		info->maxsongno = info->startsongno = 0;
		info->extdevice = 0;
		info->initaddress = info->playaddress = 0;
		info->channel = 1;
		info->initlimit = 0;
	}

	return info;
}

PROTECTED void SONGINFO_Delete(NEZ_SONG_INFO *info)
{
	free (info);
}

PROTECTED uint32_t SONGINFO_GetSongNo(NEZ_SONG_INFO *info)
{
	return info->songno;
}

PROTECTED void SONGINFO_SetSongNo(NEZ_SONG_INFO *info, uint32_t v)
{
	if (info->maxsongno && v > info->maxsongno) v = info->maxsongno;
	if (v == 0) v++;
	info->songno = v;
}

PROTECTED uint32_t SONGINFO_GetStartSongNo(NEZ_SONG_INFO *info)
{
	return info->startsongno;
}

PROTECTED void SONGINFO_SetStartSongNo(NEZ_SONG_INFO *info, uint32_t v)
{
	info->startsongno = v;
}

PROTECTED uint32_t SONGINFO_GetMaxSongNo(NEZ_SONG_INFO *info)
{
	return info->maxsongno;
}

PROTECTED void SONGINFO_SetMaxSongNo(NEZ_SONG_INFO* info, uint32_t v)
{
	info->maxsongno = v;
}

PROTECTED uint32_t SONGINFO_GetExtendDevice(NEZ_SONG_INFO *info)
{
	return info->extdevice;
}

PROTECTED void SONGINFO_SetExtendDevice(NEZ_SONG_INFO *info, uint32_t v)
{
	info->extdevice = v;
}

PROTECTED uint32_t SONGINFO_GetInitAddress(NEZ_SONG_INFO *info)
{
	return info->initaddress;
}

PROTECTED void SONGINFO_SetInitAddress(NEZ_SONG_INFO *info, uint32_t v)
{
	info->initaddress = v;
}

PROTECTED uint32_t SONGINFO_GetPlayAddress(NEZ_SONG_INFO *info)
{
	return info->playaddress;
}

PROTECTED void SONGINFO_SetPlayAddress(NEZ_SONG_INFO *info, uint32_t v)
{
	info->playaddress = v;
}

PROTECTED uint32_t SONGINFO_GetChannel(NEZ_SONG_INFO *info)
{
	return info->channel;
}

PROTECTED void SONGINFO_SetChannel(NEZ_SONG_INFO *info, uint32_t v)
{
	info->channel = v;
}
