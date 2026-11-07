// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/gmc.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include <nuade/voice.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Big-endian reader

typedef struct GmcReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} GmcReader;

static void reader_init(GmcReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool reader_eof(const GmcReader* r) {
    return r->pos > r->size;
}

static void reader_seek(GmcReader* r, size_t pos) {
    r->pos = pos;
}

static void reader_skip(GmcReader* r, size_t bytes) {
    r->pos += bytes;
}

static uint8_t reader_read_uint8(GmcReader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static uint16_t reader_read_b_uint16(GmcReader* r) {
    if (r->pos + 2 > r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    uint16_t val = ((uint16_t)r->data[r->pos] << 8) | r->data[r->pos + 1];
    r->pos += 2;
    return val;
}

static uint32_t reader_read_b_uint32(GmcReader* r) {
    if (r->pos + 4 > r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    uint32_t val = ((uint32_t)r->data[r->pos] << 24) | ((uint32_t)r->data[r->pos + 1] << 16)
                   | ((uint32_t)r->data[r->pos + 2] << 8) | r->data[r->pos + 3];
    r->pos += 4;
    return val;
}

static int32_t reader_read_b_int32(GmcReader* r) {
    return (int32_t)reader_read_b_uint32(r);
}

// Position visit tracking

static void clear_visited(GmcModule* m) {
    memset(m->visited, 0, sizeof(m->visited));
}

static bool has_position_been_visited(const GmcModule* m, int pos) {
    if (pos < 0 || pos >= 256)
        return false;
    return m->visited[pos] != 0;
}

static void mark_position_as_visited(GmcModule* m, int pos) {
    if (pos >= 0 && pos < 256)
        m->visited[pos] = 1;
}

// Paula channel registers

static void ch_play_sample(GmcChannel* c, const int8_t* data, uint32_t length) {
    c->sample_data = data;
    c->sample_length = length;
    c->loop_start = 0;
    c->loop_length = 0;
    c->active = true;
    c->play_pending = true;
}

static void ch_set_loop(GmcChannel* c, uint32_t start, uint32_t length) {
    c->loop_start = start;
    c->loop_length = length;
    if (length > 0)
        c->sample_length = start + length;
}

static void ch_set_amiga_volume(GmcChannel* c, uint16_t vol) {
    if (vol > 64)
        vol = 64;
    c->volume = vol;
}

static void ch_set_amiga_period(GmcChannel* c, uint16_t period) {
    c->period = period;
}

static void ch_mute(GmcChannel* c) {
    c->active = false;
}

static void ch_set_volume_256(GmcChannel* c, uint16_t vol) {
    // SetVolume(0-256) maps to 0-64
    ch_set_amiga_volume(c, (uint16_t)(vol >> 2));
}

// Initialize sound

static void initialize_sound(GmcModule* m, int start_position) {
    m->playing_info.current_position = (int16_t)(start_position - 1);
    m->playing_info.current_pattern = 0;
    m->playing_info.song_speed = 0;
    m->playing_info.song_step = 6;
    m->playing_info.pattern_count = 63;
    m->playing_info.shown_position = (uint16_t)start_position;
    m->playing_info.shown_row = 0;

    m->loop_count = 0;

    for (int i = 0; i < 4; i++) {
        m->channel_info[i].slide = 0;
        m->channel_info[i].period = 0;
        m->channel_info[i].volume = 0;
        m->channel_info[i].started_period = 0;
        m->channel_info[i].started_sample = 0;

        memset(&m->channels[i], 0, sizeof(GmcChannel));
    }

    clear_visited(m);
}

// Set instrument

static void set_instrument(GmcChannel* channel, GmcChannelInfo* chan_info, const GmcTrackLine* track_line,
                           const GmcSample* samples) {
    if (track_line->sample != 0) {
        const GmcSample* sample = &samples[track_line->sample - 1];

        chan_info->period = track_line->period;
        chan_info->started_period = track_line->period;
        chan_info->started_sample = track_line->sample;
        chan_info->volume = sample->volume;
        chan_info->slide = 0;

        if (sample->data != nullptr) {
            ch_play_sample(channel, sample->data, sample->length);

            if (sample->loop_length > 0)
                ch_set_loop(channel, sample->loop_start, sample->loop_length);

            ch_set_volume_256(channel, 0);
            ch_set_amiga_period(channel, chan_info->period);
        } else {
            ch_mute(channel);
        }
    }
}

// Set effect

static void set_effect(GmcModule* m, GmcChannelInfo* chan_info, const GmcTrackLine* track_line) {
    uint8_t effect_arg = track_line->effect_arg;

    switch (track_line->effect) {
        case GMC_EFFECT_SLIDE_UP:
            chan_info->slide = -(int32_t)effect_arg;
            break;

        case GMC_EFFECT_SLIDE_DOWN:
            chan_info->slide = (int32_t)effect_arg;
            break;

        case GMC_EFFECT_SET_VOLUME:
            if (effect_arg > 64)
                effect_arg = 64;
            chan_info->volume = effect_arg;
            break;

        case GMC_EFFECT_PATTERN_BREAK:
            m->playing_info.pattern_count = 63;
            break;

        case GMC_EFFECT_POSITION_JUMP:
            m->playing_info.current_position = (int16_t)(effect_arg - 1);
            m->playing_info.pattern_count = 63;
            break;

        case GMC_EFFECT_ENABLE_FILTER:
            // AmigaFilter = true; (no-op in C port)
            break;

        case GMC_EFFECT_DISABLE_FILTER:
            // AmigaFilter = false; (no-op in C port)
            break;

        case GMC_EFFECT_SET_SPEED:
            m->playing_info.song_step = effect_arg;
            break;

        default:
            break;
    }
}

// Update pattern counters

static void update_pattern_counters(GmcModule* m) {
    m->playing_info.pattern_count++;
    if (m->playing_info.pattern_count == 64) {
        m->playing_info.current_position++;
        if (m->playing_info.current_position >= m->num_positions)
            m->playing_info.current_position = 0;

        if (has_position_been_visited(m, m->playing_info.current_position)) {
            m->loop_count++;
            clear_visited(m);
        }

        mark_position_as_visited(m, m->playing_info.current_position);

        m->playing_info.pattern_count = 0;
        m->playing_info.current_pattern = m->position_list[m->playing_info.current_position];
    }
}

// Play pattern row

static void play_pattern_row(GmcModule* m) {
    m->playing_info.shown_position = (uint16_t)m->playing_info.current_position;
    m->playing_info.shown_row = m->playing_info.pattern_count;

    for (int i = 0; i < 4; i++) {
        GmcChannelInfo* chan_info = &m->channel_info[i];
        GmcChannel* channel = &m->channels[i];

        const GmcTrackLine* track_line
            = &m->patterns[m->playing_info.current_pattern].tracks[i][m->playing_info.pattern_count];

        set_instrument(channel, chan_info, track_line, m->samples);
        set_effect(m, chan_info, track_line);
    }
}

// Every tick

static void every_tick(GmcModule* m) {
    for (int i = 0; i < 4; i++) {
        GmcChannelInfo* chan_info = &m->channel_info[i];
        GmcChannel* channel = &m->channels[i];

        chan_info->period = (uint16_t)(chan_info->period + chan_info->slide);

        ch_set_amiga_period(channel, chan_info->period);
        ch_set_amiga_volume(channel, chan_info->volume);
    }
}

// Play tick (main player method)

static void play_tick(GmcModule* m) {
    every_tick(m);

    m->playing_info.song_speed++;
    if (m->playing_info.song_speed >= m->playing_info.song_step) {
        m->playing_info.song_speed = 0;

        update_pattern_counters(m);
        play_pattern_row(m);
    }
}

// Loading

#define GMC_HEADER_SIZE 444
#define GMC_PATTERN_BYTES 1024
#define GMC_MAX_POSITIONS 100

static bool load_module(GmcModule* m, const uint8_t* data, size_t size) {
    GmcReader reader;
    reader_init(&reader, data, size);

    if (size < 444)
        return false;

    // Read sample information
    for (int i = 0; i < 15; i++) {
        uint32_t start = reader_read_b_uint32(&reader);
        m->samples[i].length = (uint16_t)(reader_read_b_uint16(&reader) * 2);
        m->samples[i].volume = reader_read_b_uint16(&reader);
        uint32_t loop_start = reader_read_b_uint32(&reader);
        uint16_t loop_length = reader_read_b_uint16(&reader);

        if ((loop_length != 0) && (loop_length != 2)) {
            m->samples[i].loop_start = (uint16_t)(loop_start - start);
            m->samples[i].loop_length = (uint16_t)(loop_length * 2);
        } else {
            m->samples[i].loop_start = 0;
            m->samples[i].loop_length = 0;
        }

        // A loop must lie inside the sample data, which the voice reads.
        if (m->samples[i].loop_start + m->samples[i].loop_length > m->samples[i].length) {
            m->samples[i].loop_start = 0;
            m->samples[i].loop_length = 0;
        }

        if (reader_eof(&reader))
            return false;

        reader_skip(&reader, 2); // skip 2 bytes
        m->samples[i].data = nullptr;
    }

    // Read position list
    int num_positions = reader_read_b_int32(&reader);
    if (num_positions < 1 || num_positions > GMC_MAX_POSITIONS)
        return false;

    m->position_list = (uint8_t*)calloc(num_positions, sizeof(uint8_t));
    if (!m->position_list)
        return false;

    int actual_positions = num_positions;
    int num_patterns = 0;

    for (int i = 0, cnt = num_positions; i < cnt; i++) {
        uint16_t temp = reader_read_b_uint16(&reader);
        if (temp < 0x8000) {
            m->position_list[i] = (uint8_t)(temp / 1024);

            if (m->position_list[i] > num_patterns)
                num_patterns = m->position_list[i];
        } else {
            actual_positions--;
        }
    }

    if (actual_positions == 0)
        return false;

    m->num_positions = actual_positions;
    num_patterns++;

    if (reader_eof(&reader))
        return false;

    // Read patterns
    reader_seek(&reader, 444);

    m->patterns = (GmcPattern*)calloc(num_patterns, sizeof(GmcPattern));
    m->num_patterns = num_patterns;
    if (!m->patterns)
        return false;

    for (int i = 0; i < num_patterns; i++) {
        for (int j = 0; j < 64; j++) {
            for (int k = 0; k < 4; k++) {
                uint16_t period = reader_read_b_uint16(&reader);
                uint8_t byt3 = reader_read_uint8(&reader);
                uint8_t byt4 = reader_read_uint8(&reader);

                m->patterns[i].tracks[k][j].period = period;
                m->patterns[i].tracks[k][j].sample = (uint8_t)(byt3 >> 4);
                m->patterns[i].tracks[k][j].effect = (GmcEffect)(byt3 & 0x0f);
                m->patterns[i].tracks[k][j].effect_arg = byt4;
            }
        }

        if (reader_eof(&reader))
            return false;
    }

    // Read sample data
    for (int i = 0; i < 15; i++) {
        if (m->samples[i].length > 0) {
            size_t len = m->samples[i].length;
            if (reader.pos + len > reader.size)
                return false;

            m->samples[i].data = (int8_t*)malloc(len);
            if (!m->samples[i].data)
                return false;

            memcpy(m->samples[i].data, reader.data + reader.pos, len);
            reader.pos += len;
        }
    }

    return true;
}

// Probe

bool gmc_probe(const uint8_t* data, size_t size) {
    if (!data || size < GMC_HEADER_SIZE)
        return false;

    GmcReader reader;
    reader_init(&reader, data, size);

    size_t sample_bytes = 0;
    for (int i = 0; i < 15; i++) {
        uint32_t start = reader_read_b_uint32(&reader);
        uint32_t length = (uint32_t)reader_read_b_uint16(&reader) * 2;
        uint16_t volume = reader_read_b_uint16(&reader);
        uint32_t loop_start = reader_read_b_uint32(&reader);
        uint32_t loop_words = reader_read_b_uint16(&reader);
        reader_skip(&reader, 2);

        if (volume > 64)
            return false;
        if (loop_words > 2 && (loop_start < start || loop_start - start + loop_words * 2 > length))
            return false;
        sample_bytes += length;
    }

    int32_t num_positions = reader_read_b_int32(&reader);
    if (num_positions < 1 || num_positions > GMC_MAX_POSITIONS)
        return false;

    int active = 0;
    size_t max_pattern = 0;
    for (int i = 0; i < num_positions; i++) {
        uint16_t entry = reader_read_b_uint16(&reader);
        if (entry >= 0x8000)
            continue;
        if (entry % 1024 != 0)
            return false;
        active++;
        if (entry / 1024u > max_pattern)
            max_pattern = entry / 1024u;
    }
    if (active == 0)
        return false;

    // The header, every pattern up to the highest one used, then the sample data.
    return GMC_HEADER_SIZE + (max_pattern + 1) * GMC_PATTERN_BYTES + sample_bytes <= size;
}

// Paula hand-off

// Hand the registers the tick wrote to the voice. Paula channels 0 and 3 are left, 1 and 2 right.
static void sync_voice(GmcModule* m, uint32_t index) {
    GmcChannel* c = &m->channels[index];

    if (!c->active || c->sample_data == nullptr) {
        if (nuade_voice_is_playing(m->voices, index))
            nuade_voice_stop(m->voices, index);
    } else if (c->play_pending) {
        const NuadeSample sample = { .data = c->sample_data,
                                     .format = NuadeSampleFormat_S8,
                                     .length = c->sample_length,
                                     .loop_start = c->loop_start,
                                     .loop_length = c->loop_length };
        nuade_voice_play(m->voices, index, &sample, 0);
    }
    c->play_pending = false;

    const bool silent = c->muted || c->period == 0;
    nuade_voice_set_pitch(m->voices, index, silent ? 0.0 : nuade_paula_period_hz(c->period));
    nuade_voice_set_volume(m->voices, index, silent ? 0.0f : (float)c->volume / 64.0f);
    nuade_voice_set_pan(m->voices, index, (index == 0 || index == 3) ? -1.0f : 1.0f);
}

bool gmc_tick(GmcModule* module) {
    // A one-shot that ran off its end is already silent; Paula keeps its DMA on.
    for (uint32_t i = 0; i < 4; i++)
        module->channels[i].active = module->channels[i].active && nuade_voice_is_playing(module->voices, i);

    const uint32_t loops = module->loop_count;
    play_tick(module);

    for (uint32_t i = 0; i < 4; i++)
        sync_voice(module, i);
    return module->loop_count != loops;
}

// Public API

GmcModule* gmc_create(const uint8_t* data, size_t size) {
    if (!data || size < 444)
        return nullptr;

    GmcModule* m = (GmcModule*)calloc(1, sizeof(GmcModule));
    if (!m)
        return nullptr;

    if (!load_module(m, data, size)) {
        gmc_destroy(m);
        return nullptr;
    }

    initialize_sound(m, 0);

    return m;
}

void gmc_destroy(GmcModule* module) {
    if (!module)
        return;

    for (int i = 0; i < 15; i++)
        free(module->samples[i].data);

    free(module->patterns);
    free(module->position_list);

    free(module);
}

int gmc_subsong_count(const GmcModule* module) {
    if (!module)
        return 0;
    return 1;
}

bool gmc_start(GmcModule* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong != 0)
        return false;

    module->voices = voices;
    initialize_sound(module, 0);
    return true;
}

int gmc_channel_count(const GmcModule* module) {
    (void)module;
    return 4;
}

void gmc_set_channel_mask(GmcModule* module, uint32_t mask) {
    if (!module)
        return;

    for (int i = 0; i < 4; i++)
        module->channels[i].muted = ((mask >> i) & 1) == 0;
}

// Display

GmcPosition gmc_position(const GmcModule* module) {
    return (GmcPosition) { .position = module->playing_info.shown_position,
                           .row = module->playing_info.shown_row,
                           .speed = module->playing_info.song_step };
}

int gmc_position_count(const GmcModule* module) {
    return module->num_positions;
}

GmcVoiceState gmc_voice_state(const GmcModule* module, int channel) {
    const GmcChannelInfo* info = &module->channel_info[channel];
    return (GmcVoiceState) { .period = info->started_period, .sample = info->started_sample };
}

GmcCell gmc_pattern_cell(const GmcModule* module, int position, int channel, int row) {
    const GmcTrackLine* line = &module->patterns[module->position_list[position]].tracks[channel][row];
    return (GmcCell) {
        .period = line->period, .sample = line->sample, .effect = (uint8_t)line->effect, .effect_arg = line->effect_arg
    };
}
