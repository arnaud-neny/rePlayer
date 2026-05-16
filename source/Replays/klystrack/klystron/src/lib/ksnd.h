#ifndef KSND_DLL_H
#define KSND_DLL_H

/**
 * @file ksnd.h
 * @author  Tero Lindeman <kometbomb@kometbomb.net>
 * @brief This is a wrapper library for simple klystrack music playback.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Song instance created by KSND_LoadSong() and KSND_LoadSongFromMemory()
 *
 * Use KSND_FreeSong() to free.
 */
typedef struct KSong_t KSong;

/**
 * Player context created by KSND_CreatePlayer() and KSND_CreatePlayerUnregistered()
 *
 * Use KSND_FreePlayer() to free.
 */
typedef struct KPlayer_t KPlayer;

/**
 * Song information returned by KSND_GetSongInfo()
 */
typedef struct 
{
	char *song_title; 			/**< Song title as a null-terminated string */
	char *instrument_name[128]; /**< Instrument names as an array */
	int n_instruments;			/**< Number of instruments used by the song */
	int n_channels;				/**< Number of channels used by the song */
} KSongInfo;


#ifdef WIN32 
#ifdef DLLEXPORT
#define KLYSAPI _cdecl __declspec(dllexport)
#else
#define KLYSAPI 
#endif
#else
#define KLYSAPI 
#endif

/**
 * Loads a klystrack song file
 *
 * @param player the @c KPlayer context you will use to play the song
 * @param path path to the klystrack song to be loaded
 * @return @c KSong or @c NULL if there was an error.
 */
KLYSAPI extern KSong * KSND_LoadSong(KPlayer* player, const char *path);

/**
 * Loads a klystrack song from memory
 *
 * @param player the @c KPlayer context you will use to play the song
 * @param[in] data pointer to data containing the song
 * @param data_size size of data pointed by @a data
 * @return @c KSong or @c NULL if there was an error.
 */
KLYSAPI extern KSong * KSND_LoadSongFromMemory(KPlayer* player, void *data, int data_size);

/**
 * Free memory reserved for a @c KSong instance
 *
 * @param song song to be freed
 */
KLYSAPI extern void KSND_FreeSong(KSong *song);

/**
 * Get song length measured in pattern rows
 */
KLYSAPI extern int KSND_GetSongLength(const KSong *song);

/**
 * Get song information from a @c KSong.
 *
 * If @c NULL is passed as the @a data pointer, an internal, thread-unsafe buffer will
 * be used to store the data. This internal buffer will also change every time this 
 * function is called, thus it is preferable to supply your own @c KSongInfo or 
 * otherwise copy the returned data before a new call.
 *
 * If @c song is freed using KSND_FreeSong(), the corresponding @c KSongInfo may be 
 * invalidated and point to unallocated memory.
 *
 * @param song @c KSong whose information is queried
 * @param[out] data pointer to a @c KSongInfo structure, or @c NULL
 * @return pointer to the @c KSongInfo structure passed as @a data or the internal buffer
 */
KLYSAPI extern const KSongInfo * KSND_GetSongInfo(KSong *song, KSongInfo *data);

/**
 * Returns the amount of milliseconds that would need to elapse if the song was played
 * from the beginning to pattern row @a position. 
 *
 * Use this in conjunction with KSND_GetPlayPosition() to find out the current playback 
 * time. Or, use with KSND_GetSongLength() to get the song duration.
 *
 * @param song song whose play time is polled
 * @param position 
 * @return @c position measured in milliseconds
 */
KLYSAPI extern int KSND_GetPlayTime(KSong *song, int position);

/**
 * Create a @c KPlayer context and playback thread.
 *
 * After setting a @c KSong to be played with KSND_PlaySong() the song will start playing
 * automatically.
 *
 * @param sample_rate sample rate in Hz
 * @return @c KPlayer context or @c NULL if there was an error
 */
KLYSAPI extern KPlayer* KSND_CreatePlayer(int sample_rate);

/**
 * Create a @c KPlayer context but don't create a playback thread.
 *
 * You will need to use KSND_FillBuffer() to request audio data manually.
 *
 * @param sample_rate sample rate in Hz
 * @return @c KPlayer context or @c NULL if there was an error
 */
KLYSAPI extern KPlayer* KSND_CreatePlayerUnregistered(int sample_rate);

/**
 * Free a @c KPlayer context and associated memory.
 */
KLYSAPI extern void KSND_FreePlayer(KPlayer *player);

/**
 * Start playing song @c song from position @c start_position (measured in pattern rows)
 */
KLYSAPI extern void KSND_PlaySong(KPlayer *player, KSong *song, int start_position);

/**
 * Stop playback on a player context.
 */
KLYSAPI extern void KSND_Stop(KPlayer* player);

/**
 * Pause or continue playback on a player context.
 *
 * This will pause the actual player thread so it has no effect on players created with KSND_CreatePlayerUnregistered().
 *
 * @param player player context
 * @param state set pause state, value of @c 1 pauses the song and @c 0 continues playback
 *
 */
KLYSAPI extern void KSND_Pause(KPlayer *player, int state);

/**
 * Fill a buffer of 16-bit signed integers with audio. Data will be interleaved for stereo audio.
 *
 * Use only with a @c KPlayer context created with KSND_CreatePlayerUnregistered().
 *
 * @param player player context
 * @param[out] buffer buffer to be filled
 * @param buffer_length size of @a buffer in bytes
 * @return number of samples output
 */
KLYSAPI extern int KSND_FillBuffer(KPlayer *player, short int *buffer, int buffer_length);

/**
 * Set player oversampling quality.
 *
 * Oversample range is [0..4]. 0 implies no oversampling and 4 implies 16-fold oversampling.
 * The less oversampling the less CPU intensive the playback is but the sound quality will suffer 
 * for very high frequencies. Can be adjusted realtime.
 *
 * @param player @c KPlayer context
 * @param oversample oversample rate
 */
KLYSAPI extern void KSND_SetPlayerQuality(KPlayer *player, int oversample);

/**
 * Set playback volume.
 *
 * @param player player context which is currently playing a song set with KSND_PlaySong()
 * @param volume volume [0..128]
 */
KLYSAPI extern void KSND_SetVolume(KPlayer *player, int volume);

/**
 * Enable or disable song looping.
 *
 * If a non-zero value is passed via @c looping, the playback routine will exit KSND_FillBuffer()
 * if it encounters the song end. This is useful if you want to detect song end and avoid extra 
 * audio data in the buffer after the song end. 
 *
 * @param player player context
 * @param looping looping enabled
 */
KLYSAPI extern void KSND_SetLooping(KPlayer *player, int looping);

/**
 * Returns current position in the song.
 *
 * @param player player context which is currently playing a song set with KSND_PlaySong()
 * @return current playback position measured in pattern rows
 */
KLYSAPI extern int KSND_GetPlayPosition(KPlayer* player);

/**
 * Get the current envelope values for each player channel.
 *
 * Envelope values range from 0 (no audio) to 128 (full volume).
 *
 * @param player player context which is currently playing a song set with KSND_PlaySong()
 * @param[out] envelope array of 32-bit signed integers, should contain @c n_channel items
 * @param[in] n_channels get envelope values for @c n_channel first channels
 */
KLYSAPI extern void KSND_GetVUMeters(KPlayer *player, int *envelope, int n_channels);

#ifdef __cplusplus
}
#endif

#endif
