#include "ksnd.h"
#include "snd/music.h"
#include "snd/cyd.h"
#include "macros.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


struct KSong_t 
{
	MusSong song;
	CydWavetableEntry wavetable_entries[CYD_WAVE_MAX_ENTRIES];
};


struct KPlayer_t 
{
	CydEngine cyd;
	MusEngine mus;
	bool cyd_registered;
};


KLYSAPI KSong* KSND_LoadSong(KPlayer* player, const char *path)
{
	KSong *song = calloc(sizeof(*song), 1);
	
	int i = 0;
	for (i = 0 ; i < CYD_WAVE_MAX_ENTRIES ; ++i)
	{
		cyd_wave_entry_init(&song->wavetable_entries[i], NULL, 0, 0, 0, 0, 0);
	}
	
	if (mus_load_song(path, &song->song, song->wavetable_entries))
	{
		return song;
	}
	else
	{
		free(song);
		return NULL;
	}
}

#ifndef USESDL_RWOPS
static int RWread(struct RWops *context, void *ptr, int size, int maxnum)
{
	const int len = my_min(size * maxnum, context->mem.length - context->mem.ptr);
	memcpy(ptr, context->mem.base + context->mem.ptr, len);
	
	context->mem.ptr += len;
	
	return len;
}


static int RWclose(struct RWops *context)
{
	free(context);
	return 1;
}
#endif


KLYSAPI KSong* KSND_LoadSongFromMemory(KPlayer* player, void *data, int data_size)
{
#ifndef USESDL_RWOPS
	RWops *ops = calloc(sizeof(*ops), 1);
	ops->read = RWread;
	ops->close = RWclose;
	ops->mem.base = data;
	ops->mem.length = data_size;
#else
	RWops *ops = SDL_RWFromMem(data, data_size);
#endif
	
	KSong *song = calloc(sizeof(*song), 1);
	
	int i = 0;
	for (i = 0 ; i < CYD_WAVE_MAX_ENTRIES ; ++i)
	{
		cyd_wave_entry_init(&song->wavetable_entries[i], NULL, 0, 0, 0, 0, 0);
	}
	
	if (mus_load_song_RW(ops, &song->song, song->wavetable_entries))
	{
		// rePlayer begin
#ifndef USESDL_RWOPS
		RWclose(ops);
#else
		SDL_RWclose(ops);
#endif // rePlayer end
		return song;
	}
	else
	{
		free(song);
#ifndef USESDL_RWOPS
		RWclose(ops);
#else
		SDL_RWclose(ops);
#endif
		return NULL;
	}
}


KLYSAPI void KSND_FreeSong(KSong *song)
{
	int i = 0;
	for (i = 0 ; i < CYD_WAVE_MAX_ENTRIES ; ++i)
	{
		cyd_wave_entry_init(&song->wavetable_entries[i], NULL, 0, 0, 0, 0, 0);
	}

	mus_free_song(&song->song);
	free(song);
}


KLYSAPI int KSND_GetSongLength(const KSong *song)
{
	return song->song.song_length;
}


KLYSAPI KPlayer* KSND_CreatePlayer(int sample_rate)
{
	KPlayer *player = KSND_CreatePlayerUnregistered(sample_rate);
	
	player->cyd_registered = true;

#ifdef NOSDL_MIXER
	cyd_register(&player->cyd, 4096);
#else
	cyd_register(&player->cyd);
#endif
	
	return player;
}

KLYSAPI KPlayer* KSND_CreatePlayerUnregistered(int sample_rate)
{
	KPlayer *player = malloc(sizeof(*player));
	
	player->cyd_registered = false;
	
	cyd_init(&player->cyd, sample_rate, 1);
	mus_init_engine(&player->mus, &player->cyd);
	
	// Each song has its own wavetable array so let's free this
	free(player->cyd.wavetable_entries); 
	player->cyd.wavetable_entries = NULL;
	
	return player;
}


KLYSAPI void KSND_SetPlayerQuality(KPlayer *player, int oversample)
{
	cyd_set_oversampling(&player->cyd, oversample);
}


KLYSAPI void KSND_FreePlayer(KPlayer *player)
{
	KSND_Stop(player);
	
	if (player->cyd_registered) 
		cyd_unregister(&player->cyd);
		
	cyd_deinit(&player->cyd);
	free(player);
}


KLYSAPI void KSND_PlaySong(KPlayer *player, KSong *song, int start_position)
{
	player->cyd.wavetable_entries = song->wavetable_entries;
	cyd_set_callback(&player->cyd, mus_advance_tick, &player->mus, song->song.song_rate);
	mus_set_fx(&player->mus, &song->song);
	
	if (song->song.num_channels > player->cyd.n_channels)
		cyd_reserve_channels(&player->cyd, song->song.num_channels);
	
	mus_set_song(&player->mus, &song->song, start_position);
}


KLYSAPI int KSND_FillBuffer(KPlayer *player, short int *buffer, int buffer_length)
{
#ifdef NOSDL_MIXER
	cyd_output_buffer_stereo(&player->cyd, (void*)buffer, buffer_length);
#else
	cyd_output_buffer_stereo(0, buffer, buffer_length, &player->cyd);
#endif

	return player->cyd.samples_output;
}


KLYSAPI void KSND_Stop(KPlayer *player)
{
	mus_set_song(&player->mus, NULL, 0);
	cyd_set_callback(&player->cyd, NULL, NULL, 1);
	player->cyd.wavetable_entries = NULL;
}


KLYSAPI void KSND_Pause(KPlayer *player, int state)
{
	cyd_pause(&player->cyd, state);
}


KLYSAPI int KSND_GetPlayPosition(KPlayer *player)
{
	int song_position = 0;
	
	mus_poll_status(&player->mus, &song_position, NULL, NULL, NULL, NULL, NULL, NULL);
	
	return song_position;
}


KLYSAPI void KSND_SetVolume(KPlayer *player, int volume)
{
	cyd_lock(&player->cyd, 1);
	player->mus.volume = volume;
	cyd_lock(&player->cyd, 0);
}


KLYSAPI void KSND_GetVUMeters(KPlayer *player, int *dest, int n_channels)
{
	int temp[MUS_MAX_CHANNELS];
	mus_poll_status(&player->mus, NULL, NULL, NULL, NULL, temp, NULL, NULL);
	memcpy(dest, temp, sizeof(dest[0]) * n_channels);
}


KLYSAPI const KSongInfo * KSND_GetSongInfo(KSong *song, KSongInfo *data)
{
	static KSongInfo buffer;

	if (data == NULL)
		data = &buffer;

	data->song_title = song->song.title;
	data->n_instruments = song->song.num_instruments;
	data->n_channels = song->song.num_channels;
	
	for (int i = 0 ; i < data->n_instruments ; ++i)
		data->instrument_name[i] = song->song.instrument[i].name;
		
	return data;
}


KLYSAPI void KSND_SetLooping(KPlayer *player, int looping)
{
	if (looping)
		player->mus.flags |= MUS_NO_REPEAT;
	else
		player->mus.flags &= ~MUS_NO_REPEAT;
}


KLYSAPI int KSND_GetPlayTime(KSong *song, int position)
{
	return mus_get_playtime_at(&song->song, position);
}
