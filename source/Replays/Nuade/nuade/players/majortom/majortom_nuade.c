// SPDX-License-Identifier: MIT
// Major Tom as a nuade player.

#include <nuade/player.h>

#include "nuade_base.h"
#include "nuade_tracker.h"
#include <nuade/players/majortom.h>
#include <stdio.h>

// A note byte is (octave + 1) * 16 + semitone; the period table starts at the C two octaves below ProTracker's C-1.
#define MT_NOTE_FIRST 0x10
#define MT_NOTE_OCTAVE_SHIFT (-1)

static NuadeProbe majortom_probe(const uint8_t* data, size_t size, const char* filename) {
    (void)filename;
    return mt_probe(data, size) ? NuadeProbe_Supported : NuadeProbe_Unsupported;
}

static NuadePlayerOpenResult majortom_open(const NuadeSongSource* source) {
    NuadePlayerOpenResult result = { 0 };
    MtPlayer* player = mt_create(source->data, source->size);
    if (player == nullptr) {
        result.error.status = NuadeStatus_InvalidData;
        snprintf(result.error.message, sizeof(result.error.message), "not a Major Tom song");
        return result;
    }
    result.user_data = player;
    result.info = (NuadeSongInfo) {
        .voice_count = MT_VOICES, .subsong_count = 1, .default_subsong = 0, .timebase_hz = MT_TIMEBASE_HZ
    };
    return result;
}

static void majortom_close(void* user_data) {
    mt_destroy((MtPlayer*)user_data);
}

static bool majortom_start(void* user_data, uint32_t subsong, uint32_t sample_rate, NuadeVoices* voices) {
    (void)sample_rate;
    return subsong == 0 && mt_start((MtPlayer*)user_data, voices);
}

static NuadeTickResult majortom_tick(void* user_data, NuadeVoices* voices) {
    const MtTickResult tick = mt_tick((MtPlayer*)user_data, voices);
    uint32_t flags = NuadeTick_None;
    if (tick.looped) {
        flags |= NuadeTick_Looped;
    }
    if (tick.ended) {
        flags |= NuadeTick_Ended;
    }
    return (NuadeTickResult) { .length = MT_TICK_LENGTH, .flags = flags };
}

static void majortom_metadata(void* user_data, const NuadeMetadataSink* sink) {
    (void)user_data;
    sink->set_tag(sink->ctx, NUADE_META_FORMAT, "Major Tom");
}

static NuadePositionResult majortom_position(void* user_data) {
    const MtPosition p = mt_position((const MtPlayer*)user_data);
    return (NuadePositionResult) { .valid = true,
                                   .position = { .mode = NuadeScroll_Rows,
                                                 .order = (uint32_t)p.order,
                                                 .row = (uint32_t)p.row,
                                                 .rows = MT_ROWS,
                                                 .speed = (uint32_t)p.speed } };
}

static int32_t majortom_note(uint8_t note) {
    if (note == 0) {
        return NUADE_CELL_EMPTY;
    }
    return ((note - MT_NOTE_FIRST) / 16) * 12 + (note & 0xf);
}

static uint32_t majortom_voice_info(void* user_data, NuadeVoiceInfo* out, uint32_t cap) {
    const MtPlayer* player = (const MtPlayer*)user_data;
    const MtPosition p = mt_position(player);
    const uint32_t n = cap < MT_VOICES ? cap : MT_VOICES;
    for (uint32_t i = 0; i < n; i++) {
        const MtVoiceState v = mt_voice_state(player, (int)i);
        out[i] = (NuadeVoiceInfo) { .note = v.note >= MT_NOTE_FIRST ? majortom_note((uint8_t)v.note) : -1,
                                    .instrument = v.instrument > 0 ? v.instrument - 1 : -1,
                                    .order = (uint32_t)p.order,
                                    .row = (uint32_t)p.row };
    }
    return n;
}

static NuadeTrackerInfoResult majortom_tracker_info(void* user_data, const NuadePosition* at) {
    const MtPlayer* player = (const MtPlayer*)user_data;
    if ((int)at->order >= mt_order_count(player)) {
        return (NuadeTrackerInfoResult) { 0 };
    }
    return (NuadeTrackerInfoResult) {
        .valid = true,
        .info = { .channels = MT_VOICES,
                  .rows = MT_ROWS,
                  .column_count = 4,
                  .columns = { { "note", NuadeColumn_Note, 3, MT_NOTE_OCTAVE_SHIFT },
                               { "inst", NuadeColumn_Hex, 2, 0 },
                               { "fx", NuadeColumn_Hex, 1, 0 },
                               { "arg", NuadeColumn_Hex, 2, 0 } } },
    };
}

static NuadePatternCell majortom_cell(const void* ctx, uint32_t order, uint32_t channel, uint32_t row) {
    NuadePatternCell cell = nuade_cell_empty();
    const MtCell c = mt_pattern_cell((const MtPlayer*)ctx, (int)order, (int)channel, (int)row);
    cell.value[0] = majortom_note(c.note);
    if (c.instrument != 0) {
        cell.value[1] = c.instrument;
    }
    if (c.effect != 0 || c.param != 0) {
        cell.value[2] = c.effect;
        cell.value[3] = c.param;
    }
    return cell;
}

static uint32_t majortom_tracker_cells(void* user_data, const NuadePosition* at, uint32_t channel, int32_t row_lo,
                                       int32_t row_hi, NuadePatternCell* out, uint32_t cap) {
    const MtPlayer* player = (const MtPlayer*)user_data;
    if ((int)at->order >= mt_order_count(player)) {
        return 0;
    }
    return nuade_tracker_fill(at, MT_VOICES, MT_ROWS, channel, row_lo, row_hi, out, cap, majortom_cell, user_data);
}

const NuadePlayer nuade_player_majortom = {
    .api_version = NUADE_PLAYER_API_VERSION,
    .id = "majortom",
    .name = "Major Tom",
    .version = "1",
    .extensions = "hn,arp", // rePlayer
    .probe = majortom_probe,
    .open = majortom_open,
    .close = majortom_close,
    .start = majortom_start,
    .tick = majortom_tick,
    .metadata = majortom_metadata,
    .position = majortom_position,
    .voice_info = majortom_voice_info,
    .tracker_info = majortom_tracker_info,
    .tracker_cells = majortom_tracker_cells,
};

MtPlayer* mt_from_nuade(NuadeSong* song) {
    if (nuade_player(song) != &nuade_player_majortom) {
        return nullptr;
    }
    return (MtPlayer*)nuade_player_data(song);
}
