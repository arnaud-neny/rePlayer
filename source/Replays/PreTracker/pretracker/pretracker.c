#include "pretracker_internal.h"

#include <string.h>

typedef struct PreSong {
    SongState song;
    PlayerState player;
    MixerState mixer;
    PreSongMetadata metadata;
    PrePlaybackState playback_state;
    f32* sample_buffer;
    u8* prt_data;           // owned copy (WaveInfo pointers reference into it)
    u32 prt_data_size;
    u32 sample_rate;        // default 48000
    i32 solo_channel;       // default -1 (all)
    f32 stereo_mix;         // default 0.0 (full stereo)
    f32 stereo_width_ms;    // Haas delay in ms (0.0 = disabled)
    u8 interp_mode;         // default 0 (BLEP)
    int subsong;            // default 0
    int last_parsed_subsong; // tracks which subsong was last parsed
} PreSong;

static bool fill_metadata(PreSongMetadata* meta, const u8* prt_data, u32 prt_size, const SongState* song) {
    // Song name (20 bytes at offset 0x14) and author (20 bytes at offset 0x28)
    memcpy(meta->song_name, prt_data + 0x14, 20);
    meta->song_name[20] = '\0';
    memcpy(meta->author, prt_data + 0x28, 20);
    meta->author[20] = '\0';

    meta->num_waves = song->num_waves;
    meta->num_steps = song->num_steps;
    meta->num_positions = song->pat_pos_len;
    meta->num_subsongs = song->num_subsongs;

    u8 version = prt_data[3];
    u8 num_inst_names = (version == 0x1E) ? 2 * PRE_MAX_INSTRUMENTS : PRE_MAX_INSTRUMENTS;
    meta->num_instruments = prt_data[0x40];
    if (meta->num_instruments > PRE_MAX_INSTRUMENTS)
        meta->num_instruments = PRE_MAX_INSTRUMENTS;

    // Parse instrument names
    u32 inst_offset = read_be32(prt_data + 0x0C);
    if (inst_offset >= prt_size)
        return false;
    const u8* ptr = prt_data + inst_offset;
    const u8* end = prt_data + prt_size;
    for (int i = 0; i < num_inst_names; i++) {
        char* output = i < PRE_MAX_INSTRUMENTS ? meta->instrument_names[i] : NULL;
        if (!pretracker_read_name_record(&ptr, end, output, PRE_NAME_MAX_LEN))
            return false;
    }

    // Parse wave names
    u32 wave_offset = read_be32(prt_data + 0x10);
    if (wave_offset >= prt_size)
        return false;
    ptr = prt_data + wave_offset;
    for (int i = 0; i < PRE_MAX_WAVES; i++) {
        if (!pretracker_read_name_record(&ptr, end, meta->wave_names[i], PRE_NAME_MAX_LEN))
            return false;
    }
    return true;
}


PreSong* pre_song_create(const u8* data, u32 size) {
    PreSong* ps = (PreSong*)calloc(1, sizeof(PreSong));
    if (!ps) {
        return nullptr;
    }

    // Own a copy of PRT data (WaveInfo pointers reference into it)
    ps->prt_data = (u8*)malloc(size);
    if (!ps->prt_data) {
        free(ps);
        return nullptr;
    }
    memcpy(ps->prt_data, data, size);
    ps->prt_data_size = size;

    // Defaults
    ps->sample_rate = 48000;
    ps->solo_channel = -1;
    ps->stereo_mix = 0.0f;
    ps->subsong = 0;
    ps->last_parsed_subsong = 0;

    // Parse PRT file
    u32 chip_size = pretracker_parse_song(&ps->song, ps->prt_data, size, 0);
    if (chip_size == 0) {
        free(ps->prt_data);
        free(ps);
        return nullptr;
    }

    if (!fill_metadata(&ps->metadata, ps->prt_data, size, &ps->song)) {
        free(ps->prt_data);
        free(ps);
        return nullptr;
    }

    // Allocate sample buffer and generate all samples
    ps->sample_buffer = (f32*)calloc(chip_size, sizeof(f32));
    if (!ps->sample_buffer) {
        free(ps->prt_data);
        free(ps);
        return nullptr;
    }

    pretracker_player_init(&ps->player, ps->sample_buffer, &ps->song);
    pretracker_wavegen_generate(&ps->player);
    return ps;
}


void pre_song_destroy(PreSong* song) {
    if (!song) {
        return;
    }
    free(song->sample_buffer);
    free(song->prt_data);
    free(song);
}

// Public API: configuration setters

void pre_song_set_subsong(PreSong* song, int subsong) {
    song->subsong = subsong;
}


void pre_song_set_sample_rate(PreSong* song, u32 rate) {
    song->sample_rate = rate;
}


void pre_song_set_solo_channel(PreSong* song, i32 channel) {
    song->solo_channel = channel;
}


void pre_song_set_stereo_mix(PreSong* song, f32 mix) {
    song->stereo_mix = mix;
}


void pre_song_set_stereo_width(PreSong* song, f32 delay_ms) {
    song->stereo_width_ms = delay_ms;
}


void pre_song_set_interp_mode(PreSong* song, PreInterpMode mode) {
    song->interp_mode = (u8)mode;
    song->mixer.interp_mode = (u8)mode; // rePlayer
}

static void update_playback_state(PreSong* ps) {
    PrePlaybackState* state = &ps->playback_state;
    SongState* song = &ps->song;
    PlayerState* player = &ps->player;

    state->position = song->curr_pat_pos;
    state->row = player->pat_curr_row;
    state->speed = (player->pat_curr_row & 1) ? player->pat_speed_odd : player->pat_speed_even;
    state->ticks_remaining = player->pat_line_ticks;

    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        PerChannelData* pcd = &player->channeldata[ch];

        if (song->curr_pat_pos < song->pat_pos_len) {
            u16 pos_offset = song->curr_pat_pos * 4 + (u16)ch;
            pos_offset *= 2;
            u8* pos_ptr = song->pos_data_adr + pos_offset;
            state->channels[ch].track_num = pos_ptr[0];
            state->channels[ch].pitch_shift = (i8)pos_ptr[1];
        } else {
            state->channels[ch].track_num = 0;
            state->channels[ch].pitch_shift = 0;
        }

        state->channels[ch].instrument = (u8)(pcd->inst_num4 >> 2);
        state->channels[ch].volume = pcd->pat_vol;
        state->channels[ch].wave = (u8)(pcd->inst_wave_num >> 2);
        state->channels[ch].adsr_phase = (u8)pcd->adsr_phase;
    }
}

// Public API: playback

void pre_song_start(PreSong* song) {
    // Re-apply subsong if changed (only relevant for V1.5 multi-subsong files)
    if (song->subsong != song->last_parsed_subsong && song->song.num_subsongs > 1) {
        if (pretracker_apply_subsong(&song->song, song->prt_data, song->prt_data_size,
                                     song->subsong)) {
            song->metadata.num_positions = song->song.pat_pos_len;
            song->metadata.num_steps = song->song.num_steps;
            pretracker_rebuild_pattern_table(&song->song);
            song->last_parsed_subsong = song->subsong;
        }
    }

    pretracker_mixer_init(&song->mixer, song->sample_rate);
    song->mixer.solo_channel = song->solo_channel;
    song->mixer.stereo_mix = song->stereo_mix;
    song->mixer.interp_mode = song->interp_mode;
    if (song->stereo_width_ms > 0.0f) {
        u32 delay = (u32)(song->stereo_width_ms * 0.001f * (f32)song->sample_rate + 0.5f);
        if (delay > 63)
            delay = 63;
        song->mixer.haas_delay_samples = delay;
        song->mixer.haas_blend = 0.3f;
    } else {
        song->mixer.haas_delay_samples = 0;
        song->mixer.haas_blend = 0.0f;
    }
    pretracker_mixer_start(&song->mixer, &song->player, &song->song);
    pretracker_player_start(&song->player, &song->song);
}


int pre_song_decode(PreSong* song, f32* buffer, int num_frames) {
    int result = pretracker_mixer_render(&song->player, &song->mixer, buffer, num_frames, nullptr, 0);
    update_playback_state(song);
    return result;
}


int pre_song_decode_with_scopes(PreSong* song, f32* buffer, int num_frames, f32** scopes, int num_scopes) {
    int result = pretracker_mixer_render(&song->player, &song->mixer, buffer, num_frames, scopes, num_scopes);
    update_playback_state(song);
    return result;
}

// rePlayer begin
int pre_song_decode_muted(PreSong* song, int num_frames) {
    int result = pretracker_mixer_muted(&song->player, &song->mixer, num_frames);
    update_playback_state(song);
    return result;
}
// rePlayer end


bool pre_song_is_finished(const PreSong* song) {
    return pretracker_player_is_finished(&song->player);
}


const PreSongMetadata* pre_song_get_metadata(const PreSong* ps) {
    return &ps->metadata;
}


// rePlayer begin
int pre_song_get_length(struct PreSong* song) {
    int frames = 0;
    while (!pretracker_player_is_finished(&song->player)) {
        pretracker_player_tick(&song->player);
        frames += song->mixer.samples_per_tick;
    }
    return frames;
}
// rePlayer end


bool pre_song_get_position_entry(const PreSong* ps, u16 position, u8 channel, u8* track_num, i8* pitch_shift) {
    const SongState* song = &ps->song;
    if (position >= song->pat_pos_len || channel >= NUM_CHANNELS) {
        return false;
    }
    u16 pos_offset = position * 4 + channel;
    pos_offset *= 2;
    const u8* pos_ptr = song->pos_data_adr + pos_offset;
    *track_num = pos_ptr[0];
    *pitch_shift = (i8)pos_ptr[1];
    return true;
}


bool pre_song_get_track_cell(const PreSong* ps, u8 track, u8 row, PreTrackCell* cell) {
    const SongState* song = &ps->song;
    if (track == 0 || track > song->num_patterns || row >= song->num_steps ||
        song->pattern_table[track - 1] == NULL) {
        return false;
    }
    const u8* pat_data = song->pattern_table[track - 1] + (u32)row * 3;
    u8 pitch_ctrl = pat_data[0];
    u8 inst_effect = pat_data[1];

    cell->note = pitch_ctrl & PITCH_CTRL_NOTE_MASK;
    cell->has_arpeggio = (pitch_ctrl & PITCH_CTRL_HAS_ARP) != 0;
    cell->instrument = inst_effect >> 4;
    if (pitch_ctrl & PITCH_CTRL_INST_HI) {
        cell->instrument += 16;
    }
    cell->effect_cmd = cell->has_arpeggio ? 0 : (inst_effect & 0x0F);
    cell->effect_data = pat_data[2];
    return true;
}


const PrePlaybackState* pre_song_get_playback_state(const PreSong* ps) {
    return &ps->playback_state;
}
