#include "../include/nezplug/nezplug.h"

#include "handler.h"
#include "audiosys.h"
#include "songinfo.h"
#include "trackinfo.h"

#include "../device/kmsnddev.h"
#include "m_hes.h"
#include "m_gbr.h"
#include "m_zxay.h"
#include "m_nsf.h"
#include "m_kss.h"
#include "m_nsd.h"
#include "m_sgc.h"
#include "../common/util.h"

static Inline uint32_t fix_track(NEZ_PLAY *pNezPlay, uint32_t track)
{
    if(track < 1) track = 1;
    if(track > NEZGetSongMaxAbsolute(pNezPlay)) track = NEZGetSongMaxAbsolute(pNezPlay);
    return track;
}

NEZ_PLAY* NEZNew()
{
	NEZ_PLAY *pNezPlay = (NEZ_PLAY*)XMALLOC(sizeof(NEZ_PLAY));

	if (pNezPlay != NULL) {
		XMEMSET(pNezPlay, 0, sizeof(NEZ_PLAY));
        XMEMSET(pNezPlay->chmask,1,0x80);

		pNezPlay->song = SONGINFO_New();
		if (!pNezPlay->song) {
			XFREE(pNezPlay);
			return NULL;
		}

        pNezPlay->lowpass_filter_level = 8;
        pNezPlay->default_fade = 8000;
        pNezPlay->default_length = 180000;
        pNezPlay->default_loops = 2;
        pNezPlay->gain = 65535 * 256;

	    NESAudioFrequencySet(pNezPlay, 48000);
	    NESAudioChannelSet(pNezPlay, 1);

		pNezPlay->naf_type = NES_AUDIO_FILTER_NONE;

        pNezPlay->nes_config.apu_volume = 64;
        pNezPlay->nes_config.n106_volume = 16;
        pNezPlay->nes_config.n106_realmode = 0;
        pNezPlay->nes_config.fds_realmode = 3;
        pNezPlay->nes_config.realdac = 1;
        pNezPlay->nes_config.noise_random_reset = 0;
        pNezPlay->nes_config.nes2A03type = 1;
        pNezPlay->nes_config.fds_debug_option1 = 1;
        pNezPlay->nes_config.fds_debug_option2 = 0;
#if HES_TONE_DEBUG_OPTION_ENABLE
        pNezPlay->hes_config.tone_debug_option = 0;
#endif
        pNezPlay->hes_config.noise_debug_option1 = 9;
        pNezPlay->hes_config.noise_debug_option2 = 10;
        pNezPlay->hes_config.noise_debug_option3 = 3;
        pNezPlay->hes_config.noise_debug_option4 = 508;
        pNezPlay->kss_config.msx_psg_type = 1;
        pNezPlay->kss_config.msx_psg_volume = 64;
		pNezPlay->naf_prev[0] = pNezPlay->naf_prev[1] = 0x8000;
	}


	return pNezPlay;
}

void NEZDelete(NEZ_PLAY *pNezPlay)
{
	if (pNezPlay != NULL) {
		NESTerminate(pNezPlay);
		NESAudioHandlerTerminate(pNezPlay);
		NESVolumeHandlerTerminate(pNezPlay);
		SONGINFO_Delete(pNezPlay->song);
        TRACKS_Delete(pNezPlay->tracks);
        if(pNezPlay->songinfodata.title != NULL) XFREE(pNezPlay->songinfodata.title);
        if(pNezPlay->songinfodata.artist != NULL) XFREE(pNezPlay->songinfodata.artist);
        if(pNezPlay->songinfodata.copyright != NULL) XFREE(pNezPlay->songinfodata.copyright);
		XFREE(pNezPlay);
	}
}


void NEZSetSongNo(NEZ_PLAY *pNezPlay, uint32_t uSongNo)
{
	if (pNezPlay == 0) return;
    if (uSongNo == 0) uSongNo = 1;
    if (uSongNo > NEZGetSongMaxAbsolute(pNezPlay)) uSongNo = NEZGetSongMaxAbsolute(pNezPlay);

    if (pNezPlay->tracks && pNezPlay->tracks->loaded) {
        pNezPlay->tracks->current = uSongNo;
        uSongNo = pNezPlay->tracks->info[uSongNo - 1].songno + 1;
    }
	SONGINFO_SetSongNo(pNezPlay->song, uSongNo);
}

void NEZSetFade(NEZ_PLAY *pNezPlay, uint32_t fade)
{
	if (pNezPlay == 0) return;
    pNezPlay->default_fade = fade;
}

void NEZSetLoops(NEZ_PLAY *pNezPlay, uint32_t loops)
{
	if (pNezPlay == 0) return;
    pNezPlay->default_loops = loops;
}

void NEZSetLength(NEZ_PLAY *pNezPlay, uint32_t length)
{
	if (pNezPlay == 0) return;
    pNezPlay->default_length = length;
}

void NEZSetFrequency(NEZ_PLAY *pNezPlay, uint32_t freq)
{
	if (pNezPlay == 0) return;
	NESAudioFrequencySet(pNezPlay, freq);
}

void NEZSetChannel(NEZ_PLAY *pNezPlay, uint32_t ch)
{
	if (pNezPlay == 0) return;
	NESAudioChannelSet(pNezPlay, ch);
}

void NEZReset(NEZ_PLAY *pNezPlay)
{
	if (pNezPlay == 0) return;
	NESReset(pNezPlay);
	NESVolume(pNezPlay, pNezPlay->volume);
}

void NEZSetFilter(NEZ_PLAY *pNezPlay, uint32_t filter)
{
	if (pNezPlay == 0) return;
	NESAudioFilterSet(pNezPlay, filter);
}

void NEZGain(NEZ_PLAY *pNezPlay, uint32_t uGain) {
    if(pNezPlay == 0) return;
    pNezPlay->gain = 256.0f * 65535.0f / (1.0f + 7.0f * uGain / 255.0f);
}

void NEZVolume(NEZ_PLAY *pNezPlay, uint32_t uVolume)
{
	if (pNezPlay == 0) return;
	pNezPlay->volume = uVolume;
	NESVolume(pNezPlay, pNezPlay->volume);
}

void NEZAPUVolume(NEZ_PLAY *pNezPlay, int32_t uVolume)
{
	if (pNezPlay == 0) return;
	if (pNezPlay->nsf == 0) return;
	((NSFNSF*)pNezPlay->nsf)->apu_volume = uVolume;
}

void NEZDPCMVolume(NEZ_PLAY *pNezPlay, int32_t uVolume)
{
	if (pNezPlay == 0) return;
	if (pNezPlay->nsf == 0) return;
	((NSFNSF*)pNezPlay->nsf)->dpcm_volume = uVolume;
}

void NEZRender(NEZ_PLAY *pNezPlay, void *bufp, uint32_t buflen)
{
	if (pNezPlay == 0) return;
	NESAudioRender(pNezPlay, (float*)bufp, buflen);
}

uint32_t NEZGetSongNo(NEZ_PLAY *pNezPlay)
{
	if (pNezPlay == 0) return 0;
    return pNezPlay->tracks->current ?
      pNezPlay->tracks->current :
      SONGINFO_GetSongNo(pNezPlay->song);
}

uint32_t NEZGetSongStart(NEZ_PLAY *pNezPlay)
{
	if (pNezPlay == 0) return 0;
	return SONGINFO_GetStartSongNo(pNezPlay->song);
}

uint32_t NEZGetSongMax(NEZ_PLAY *pNezPlay)
{
	if (pNezPlay == 0) return 0;
	return SONGINFO_GetMaxSongNo(pNezPlay->song);
}

/* NSFE and M3U can set the MaxSongNo < the actual MaxSongNo - returns the
 * actual amount of tracks */
uint32_t NEZGetSongMaxAbsolute(NEZ_PLAY *pNezPlay)
{
	if (pNezPlay == 0) return 0;
    if (pNezPlay->tracks && pNezPlay->tracks->total) return pNezPlay->tracks->total;
	return SONGINFO_GetMaxSongNo(pNezPlay->song);
}

uint32_t NEZGetChannel(NEZ_PLAY *pNezPlay)
{
	if (pNezPlay == 0) return 1;
	return SONGINFO_GetChannel(pNezPlay->song);
}

uint32_t NEZGetFrequency(NEZ_PLAY *pNezPlay)
{
	if (pNezPlay == 0) return 1;
	return NESAudioFrequencyGet(pNezPlay);
}

NEZ_TYPE NEZLoad(NEZ_PLAY *pNezPlay, const uint8_t *pData, uint32_t uSize)
{
	uint32_t ret = NEZ_NESERR_NOERROR;

	pNezPlay->songinfodata.detail[0]=0;
    pNezPlay->tracks = NULL;

	while (1)
	{
		if (!pNezPlay || !pData) {
			ret = NEZ_NESERR_PARAMETER;
			break;
		}
		NESTerminate(pNezPlay);
		NESHandlerInitialize(pNezPlay->nrh, pNezPlay->nth);
		NESAudioHandlerInitialize(pNezPlay);

		NEZ_TYPE type = NEZ_TYPE_NONE;
		if (uSize < 8)
		{
			ret = NEZ_NESERR_FORMAT;
			break;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("KSCC"))
		{
			/* KSS */
			ret =  KSSLoad(pNezPlay, pData, uSize);
			if (ret) break;
			type = NEZ_TYPE_KSSC;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("KSSX"))
		{
			/* KSS */
			ret =  KSSLoad(pNezPlay, pData, uSize);
			if (ret) break;
            type = NEZ_TYPE_KSSX;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("HESM"))
		{
			/* HES */
			ret =  HESLoad(pNezPlay, pData, uSize);
			if (ret) break;
            type = NEZ_TYPE_HESM;
		}
		else if (uSize > 0x220 && GetDwordLE(pData + 0x200) == GetDwordLEM("HESM"))
		{
			/* HES(+512byte header) */
			ret =  HESLoad(pNezPlay, pData + 0x200, uSize - 0x200);
			if (ret) break;
            type = NEZ_TYPE_HESM;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("NESM") && pData[4] == 0x1A)
		{
			/* NSF */
			ret = NSFLoad(pNezPlay, pData, uSize);
			if (ret) break;
			type = NEZ_TYPE_NSF;
		}
        else if (GetDwordLE(pData + 0) == GetDwordLEM("NSFE")) {
            /* NSFe */
            ret = NSFELoad(pNezPlay, pData, uSize);
            if (ret) break;
            type = NEZ_TYPE_NSFE;
        }
		else if (GetDwordLE(pData + 0) == GetDwordLEM("ZXAY") && GetDwordLE(pData + 4) == GetDwordLEM("EMUL"))
		{
			/* ZXAY */
			ret =  ZXAYLoad(pNezPlay, pData, uSize);
			if (ret) break;
            type = NEZ_TYPE_AY;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("GBRF"))
		{
			/* GBR */
			ret =  GBRLoad(pNezPlay, pData, uSize);
			if (ret) break;
            type = NEZ_TYPE_GBR;
		}
		else if ((GetDwordLE(pData + 0) & 0x00ffffff) == GetDwordLEM("GBS\x0"))
		{
			/* GBS */
			ret =  GBRLoad(pNezPlay, pData, uSize);
			if (ret) break;
            type = NEZ_TYPE_GBS;
		}
		else if (pData[0] == 0xc3 && uSize > GetWordLE(pData + 1) + 4 && GetWordLE(pData + 1) > 0x70 && (GetDwordLE(pData + GetWordLE(pData + 1) - 0x70) & 0x00ffffff) == GetDwordLEM("GBS\x0"))
		{
			/* GB(GBS player) */
			ret =  GBRLoad(pNezPlay, pData + GetWordLE(pData + 1) - 0x70, uSize - (GetWordLE(pData + 1) - 0x70));
			if (ret) break;
            type = NEZ_TYPE_GBS;
		}
		else if (GetDwordLE(pData + 0) == GetDwordLEM("NESL") && pData[4] == 0x1A)
		{
			/* NSD */
			ret = NSDLoad(pNezPlay, pData, uSize);
			if (ret) break;
            type = NEZ_TYPE_NSD;
		}
		else if ((GetDwordLE(pData + 0) & 0x00ffffff) == GetDwordLEM("SGC"))
		{
			/* SGC */
			ret = SGCLoad(pNezPlay, pData, uSize);
			if (ret) break;
            type = NEZ_TYPE_SGC;
		}
		else
		{
			ret = NEZ_NESERR_FORMAT;
			break;
		}

        /* NSFELoad can create the tracks object */
        if(!pNezPlay->tracks) pNezPlay->tracks = TRACKS_New(NEZGetSongMax(pNezPlay));
        if(!pNezPlay->tracks) {
            ret = NEZ_NESERR_SHORTOFMEMORY;
            break;
        }

		return type;
	}
	NESTerminate(pNezPlay);
	return NEZ_TYPE_NONE;
}

uint32_t NEZLoadM3U(NEZ_PLAY *pNezPlay, const uint8_t *pData, uint32_t uSize) {
	if (!pNezPlay || !pData || !uSize) {
		return NEZ_NESERR_PARAMETER;
    }

    return TRACKS_LoadM3U(pNezPlay,pData,uSize);
}

void NEZMuteChannel(NEZ_PLAY *pNezPlay, int32_t chan)
{
    if(chan < 0) {
        XMEMSET(pNezPlay->chmask,0,sizeof(pNezPlay->chmask));
    } else if ((size_t)chan < sizeof(pNezPlay->chmask)) {
        pNezPlay->chmask[chan] = 0;
    }
}

void NEZUnmuteChannel(NEZ_PLAY *pNezPlay, int32_t chan)
{
    if(chan < 0) {
        XMEMSET(pNezPlay->chmask,1,sizeof(pNezPlay->chmask));
    } else if ((size_t)chan < sizeof(pNezPlay->chmask)) {
        pNezPlay->chmask[chan] = 1;
    }
}

void NEZGBAMode(NEZ_PLAY *pNezPlay, uint8_t m) {
    pNezPlay->gb_config.gbamode = m;
}

const char *NEZGetGameTitle(NEZ_PLAY *pNezPlay) {
    if(pNezPlay->tracks->title) return (const char *)pNezPlay->tracks->title;
    return (const char *)pNezPlay->songinfodata.title;
}

const char *NEZGetGameArtist(NEZ_PLAY *pNezPlay) {
    if(pNezPlay->tracks->artist) return (const char *)pNezPlay->tracks->artist;
    return (const char *)pNezPlay->songinfodata.artist;
}

const char *NEZGetGameCopyright(NEZ_PLAY *pNezPlay) {
    if(pNezPlay->tracks->copyright) return (const char *)pNezPlay->tracks->copyright;
    return (const char *)pNezPlay->songinfodata.copyright;
}

const char *NEZGetGameDetail(NEZ_PLAY *pNezPlay) {
    return (const char *)pNezPlay->songinfodata.detail;
}

const char *NEZGetTrackTitle(NEZ_PLAY *pNezPlay, uint32_t track) {
    track = fix_track(pNezPlay,track);
    if(pNezPlay->tracks && pNezPlay->tracks->loaded) {
        return (const char *)pNezPlay->tracks->info[track - 1].title;
    }
    return NULL;
}

uint32_t NEZGetTrackFade(NEZ_PLAY *pNezPlay, uint32_t track) {
    track = fix_track(pNezPlay,track);
    if(pNezPlay->tracks && pNezPlay->tracks->loaded) {
        if (pNezPlay->tracks->info[track - 1].fade != -1) {
          return (uint32_t)pNezPlay->tracks->info[track - 1].fade;
        }
    }
    return pNezPlay->default_fade;
}

uint32_t NEZGetTrackIntro(NEZ_PLAY *pNezPlay, uint32_t track) {
    track = fix_track(pNezPlay,track);
    if(pNezPlay->tracks && pNezPlay->tracks->loaded) {
        if (pNezPlay->tracks->info[track - 1].intro != -1) {
          return (uint32_t)pNezPlay->tracks->info[track - 1].intro;
        }
    }
    return 0;
}

int32_t NEZGetTrackLoop(NEZ_PLAY *pNezPlay, uint32_t track) {
    track = fix_track(pNezPlay,track);
    if(pNezPlay->tracks && pNezPlay->tracks->loaded) {
        return pNezPlay->tracks->info[track - 1].loops;
    }
    return -1;
}

uint32_t NEZGetTrackLoops(NEZ_PLAY *pNezPlay, uint32_t track) {
    track = fix_track(pNezPlay,track);
    if(pNezPlay->tracks && pNezPlay->tracks->loaded) {
        if (pNezPlay->tracks->info[track - 1].loops != -1) {
          return (uint32_t)pNezPlay->tracks->info[track - 1].loops;
        }
    }
    return pNezPlay->default_loops;
}

uint32_t NEZGetTrackLength(NEZ_PLAY *pNezPlay, uint32_t track) {
    uint32_t ret;
    track = fix_track(pNezPlay,track);
    if(pNezPlay->tracks && pNezPlay->tracks->loaded) {
        if (pNezPlay->tracks->info[track - 1].length != -1) {
          return (uint32_t)pNezPlay->tracks->info[track - 1].length;
        }
        else if(pNezPlay->tracks->info[track - 1].loop != -1) {
            ret = NEZGetTrackIntro(pNezPlay,track);
            ret += pNezPlay->tracks->info[track - 1].loop *
              NEZGetTrackLoops(pNezPlay,track);
            return ret;
        }
    }
    return pNezPlay->default_length;
}
