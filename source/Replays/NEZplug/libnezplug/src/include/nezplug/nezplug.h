#ifndef NEZPLUG_H__
#define NEZPLUG_H__

#include "nezint.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	NEZ_NESERR_NOERROR,
	NEZ_NESERR_SHORTOFMEMORY,
	NEZ_NESERR_FORMAT,
	NEZ_NESERR_PARAMETER
} NEZ_NESERR_CODE;

//チャンネルマスク用
typedef enum{//順番を変えたら恐ろしいことになる
	NEZ_DEV_2A03_SQ1,
	NEZ_DEV_2A03_SQ2,
	NEZ_DEV_2A03_TR,
	NEZ_DEV_2A03_NOISE,
	NEZ_DEV_2A03_DPCM,

	NEZ_DEV_FDS_CH1,

	NEZ_DEV_MMC5_SQ1,
	NEZ_DEV_MMC5_SQ2,
	NEZ_DEV_MMC5_DA,

	NEZ_DEV_VRC6_SQ1,
	NEZ_DEV_VRC6_SQ2,
	NEZ_DEV_VRC6_SAW,

	NEZ_DEV_N106_CH1,
	NEZ_DEV_N106_CH2,
	NEZ_DEV_N106_CH3,
	NEZ_DEV_N106_CH4,
	NEZ_DEV_N106_CH5,
	NEZ_DEV_N106_CH6,
	NEZ_DEV_N106_CH7,
	NEZ_DEV_N106_CH8,

	NEZ_DEV_DMG_SQ1,
	NEZ_DEV_DMG_SQ2,
	NEZ_DEV_DMG_WM,
	NEZ_DEV_DMG_NOISE,

	NEZ_DEV_HUC6230_CH1,
	NEZ_DEV_HUC6230_CH2,
	NEZ_DEV_HUC6230_CH3,
	NEZ_DEV_HUC6230_CH4,
	NEZ_DEV_HUC6230_CH5,
	NEZ_DEV_HUC6230_CH6,

	NEZ_DEV_AY8910_CH1,
	NEZ_DEV_AY8910_CH2,
	NEZ_DEV_AY8910_CH3,

	NEZ_DEV_SN76489_SQ1,
	NEZ_DEV_SN76489_SQ2,
	NEZ_DEV_SN76489_SQ3,
	NEZ_DEV_SN76489_NOISE,

	NEZ_DEV_SCC_CH1,
	NEZ_DEV_SCC_CH2,
	NEZ_DEV_SCC_CH3,
	NEZ_DEV_SCC_CH4,
	NEZ_DEV_SCC_CH5,

	NEZ_DEV_YM2413_CH1,
	NEZ_DEV_YM2413_CH2,
	NEZ_DEV_YM2413_CH3,
	NEZ_DEV_YM2413_CH4,
	NEZ_DEV_YM2413_CH5,
	NEZ_DEV_YM2413_CH6,
	NEZ_DEV_YM2413_CH7,
	NEZ_DEV_YM2413_CH8,
	NEZ_DEV_YM2413_CH9,
	NEZ_DEV_YM2413_BD,
	NEZ_DEV_YM2413_HH,
	NEZ_DEV_YM2413_SD,
	NEZ_DEV_YM2413_TOM,
	NEZ_DEV_YM2413_TCY,

	NEZ_DEV_ADPCM_CH1,

	NEZ_DEV_MSX_DA,

	NEZ_DEV_MAX,
} NEZ_CHANNEL_ID;

typedef enum {
	NEZ_TYPE_NONE,
	NEZ_TYPE_KSSC,
	NEZ_TYPE_KSSX,
	NEZ_TYPE_HESM,
	NEZ_TYPE_NSF,
	NEZ_TYPE_NSFE,
	NEZ_TYPE_AY,
	NEZ_TYPE_GBR,
	NEZ_TYPE_GBS,
	NEZ_TYPE_NSD,
    NEZ_TYPE_SGC
} NEZ_TYPE;

typedef struct NEZ_SONG_INFO_ NEZ_SONG_INFO;
typedef struct NEZ_TRACK_INFO_ NEZ_TRACK_INFO;
typedef struct NEZ_TRACKS_ NEZ_TRACKS;
typedef struct NEZ_NES_AUDIO_HANDLER_ NEZ_NES_AUDIO_HANDLER;
typedef struct NEZ_NES_VOLUME_HANDLER_  NEZ_NES_VOLUME_HANDLER;
typedef struct NEZ_NES_RESET_HANDLER_ NEZ_NES_RESET_HANDLER;
typedef struct NEZ_NES_TERMINATE_HANDLER_ NEZ_NES_TERMINATE_HANDLER;
typedef struct NEZ_PLAY_ NEZ_PLAY;

/* audiohandler function pointers */
typedef int32_t (*NEZ_AUDIOHANDLER)(NEZ_PLAY *);
typedef void (*NEZ_AUDIOHANDLER2)(NEZ_PLAY *, int32_t *p);
typedef void (*NEZ_VOLUMEHANDLER)(NEZ_PLAY *, uint32_t volume);
typedef void (*NEZ_RESETHANDLER)(NEZ_PLAY *);
typedef void (*NEZ_TERMINATEHANDLER)(NEZ_PLAY *);

struct NEZ_SONG_INFO_
{
	uint32_t songno;
	uint32_t maxsongno;
	uint32_t startsongno;
	uint32_t extdevice;
	uint32_t initaddress;
	uint32_t playaddress;
	uint32_t channel;
	uint32_t initlimit;
};

struct NEZ_TRACK_INFO_ {
    uint32_t songno;
    int32_t length;
    int32_t fade;
    int32_t intro;
    int32_t loop;
    int32_t loops;
    int32_t total; /* either length + fade or intro + (loop * loops) + fade */
    char *title;
};

struct NEZ_TRACKS_ {
    uint32_t total;
    uint32_t loaded;
    uint32_t current;
    char *title;
    char *artist;
    char *copyright; /* aka the date */
    char *dumper;
    NEZ_TRACK_INFO *info;
};

struct NEZ_NES_AUDIO_HANDLER_ {
	uint32_t fMode;
	NEZ_AUDIOHANDLER Proc;
	NEZ_AUDIOHANDLER2 Proc2;
	struct NEZ_NES_AUDIO_HANDLER_ *next;
};

struct NEZ_NES_VOLUME_HANDLER_ {
	NEZ_VOLUMEHANDLER Proc;
	struct NEZ_NES_VOLUME_HANDLER_ *next;
};

struct NEZ_NES_RESET_HANDLER_ {
	uint32_t priority;	/* 0(first) - 15(last) */
	NEZ_RESETHANDLER Proc;
	struct NEZ_NES_RESET_HANDLER_ *next;
};

struct NEZ_NES_TERMINATE_HANDLER_ {
	NEZ_TERMINATEHANDLER Proc;
	struct NEZ_NES_TERMINATE_HANDLER_ *next;
};

struct NEZ_PLAY_ {
	NEZ_SONG_INFO *song;
    NEZ_TRACKS *tracks;
	uint32_t volume;
	float gain;
	uint32_t frequency;
	uint32_t channel;
	NEZ_NES_AUDIO_HANDLER *nah;
	NEZ_NES_VOLUME_HANDLER *nvh;
	NEZ_NES_RESET_HANDLER *(nrh[0x10]);
	NEZ_NES_TERMINATE_HANDLER *nth;
	uint32_t naf_type;
	int32_t naf_prev[2];
    uint8_t chmask[0x80];
    int32_t lowpass_filter_level;
    struct {
        uint8_t gbamode;
    } gb_config;
    struct {
        int32_t apu_volume;
        int32_t realdac;
        int32_t noise_random_reset;
        int32_t nes2A03type;
        uint8_t fds_debug_option1;
        uint8_t fds_debug_option2;
        int32_t n106_volume;
        int32_t n106_realmode;
        int32_t fds_realmode;
    } nes_config;
    struct {
#if HES_TONE_DEBUG_OPTION_ENABLE
        uint8_t tone_debug_option;
#endif
        uint8_t noise_debug_option1;
        uint8_t noise_debug_option2;
        int32_t noise_debug_option3;
        int32_t noise_debug_option4;
    } hes_config;
    struct {
        int32_t msx_psg_volume;
        int32_t msx_psg_type;
    } kss_config;
    struct {
        char *coleco_bios_path;
    } sgc_config;
    struct {
    	char* title;
    	char* artist;
    	char* copyright;
    	char detail[1024];
    } songinfodata;
    int32_t output2[2];
    int32_t filter;
    int32_t lowlevel;
    uint32_t default_fade;
    uint32_t default_length;
    uint32_t default_loops;
	void *nsf;
	void *gbrdmg;
	void *heshes;
	void *zxay;
	void *kssseq;
	void *sgcseq;
	void *nsdp;
};

/* NEZ player */
NEZ_PLAY* NEZNew();
void NEZDelete(NEZ_PLAY*);

NEZ_TYPE NEZLoad(NEZ_PLAY*, const uint8_t *, uint32_t);
uint32_t NEZLoadM3U(NEZ_PLAY*, const uint8_t *, uint32_t);
void NEZSetSongNo(NEZ_PLAY*, uint32_t uSongNo);
void NEZSetFrequency(NEZ_PLAY*, uint32_t freq);
void NEZSetFade(NEZ_PLAY*, uint32_t fade);
void NEZSetLength(NEZ_PLAY*, uint32_t length);
void NEZSetLoops(NEZ_PLAY*, uint32_t loops);
void NEZSetChannel(NEZ_PLAY*, uint32_t ch);
void NEZReset(NEZ_PLAY*);
void NEZSetFilter(NEZ_PLAY *, uint32_t filter);
void NEZVolume(NEZ_PLAY*, uint32_t uVolume);
void NEZGain(NEZ_PLAY*, uint32_t uGain);
void NEZAPUVolume(NEZ_PLAY*, int32_t uVolume);
void NEZDPCMVolume(NEZ_PLAY*, int32_t uVolume);
void NEZRender(NEZ_PLAY*, void *bufp, uint32_t buflen);

uint32_t NEZGetSongNo(NEZ_PLAY*);
uint32_t NEZGetSongStart(NEZ_PLAY*);
uint32_t NEZGetSongMax(NEZ_PLAY*);
uint32_t NEZGetSongMaxAbsolute(NEZ_PLAY *);
uint32_t NEZGetChannel(NEZ_PLAY*);
uint32_t NEZGetFrequency(NEZ_PLAY*);

void NEZMuteChannel(NEZ_PLAY *, int32_t chan);
void NEZUnmuteChannel(NEZ_PLAY *, int32_t chan);

void NEZGBAMode(NEZ_PLAY *, uint8_t m);

const char *NEZGetGameTitle(NEZ_PLAY *pNezPlay);
const char *NEZGetGameArtist(NEZ_PLAY *pNezPlay);
const char *NEZGetGameCopyright(NEZ_PLAY *pNezPlay);
const char *NEZGetGameDetail(NEZ_PLAY *pNezPlay);

const char *NEZGetTrackTitle(NEZ_PLAY *pNezPlay, uint32_t track);
uint32_t NEZGetTrackFade(NEZ_PLAY *pNezPlay, uint32_t track);
uint32_t NEZGetTrackIntro(NEZ_PLAY *pNezPlay, uint32_t track);
int32_t NEZGetTrackLoop(NEZ_PLAY *pNezPlay, uint32_t track);
uint32_t NEZGetTrackLength(NEZ_PLAY *pNezPlay, uint32_t track);
uint32_t NEZGetTrackLoops(NEZ_PLAY *pNezPlay, uint32_t track);

#ifdef __cplusplus
}
#endif

#endif /* NEZPLUG_H__ */
