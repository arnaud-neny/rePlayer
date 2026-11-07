#include "nuade_viz.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct NuadeSlot {
    u64 frame;
    NuadePosition position;
    NuadeVoiceView* voices; // display_voices entries
} NuadeSlot;

struct NuadeViz {
    u32 voice_count;    // the mixer's: meters
    u32 display_voices; // snapshots
    NuadeSlot ring[NUADE_SNAPSHOT_RING];
    u32 head;             // next slot to write
    u32 count;            // valid slots
    NuadeVoiceInfo* info; // scratch for the player's voice_info call

    bool meters;
    f32* peak;  // voice_count
    f32* scope; // voice_count * NUADE_SCOPE_FRAMES
    u32 scope_head;
    u32 scope_count;
};

NuadeViz* nuade_viz_create(u32 voice_count, u32 display_voices) {
    NuadeViz* viz = (NuadeViz*)calloc(1, sizeof(NuadeViz));
    if (viz == nullptr) {
        return nullptr;
    }
    viz->voice_count = voice_count;
    viz->display_voices = display_voices;
    const size_t n = display_voices ? display_voices : 1;
    NuadeVoiceView* views = (NuadeVoiceView*)calloc((size_t)NUADE_SNAPSHOT_RING * n, sizeof(NuadeVoiceView));
    viz->info = (NuadeVoiceInfo*)calloc(n, sizeof(NuadeVoiceInfo));
    if (views == nullptr || viz->info == nullptr) {
        free(views);
        free(viz->info);
        free(viz);
        return nullptr;
    }
    for (u32 i = 0; i < NUADE_SNAPSHOT_RING; i++) {
        viz->ring[i].voices = views + (size_t)i * display_voices;
    }
    return viz;
}

void nuade_viz_destroy(NuadeViz* viz) {
    if (viz == nullptr) {
        return;
    }
    free(viz->ring[0].voices); // all slots share one allocation
    free(viz->info);
    free(viz->peak);
    free(viz->scope);
    free(viz);
}

void nuade_viz_reset(NuadeViz* viz) {
    viz->head = 0;
    viz->count = 0;
    nuade_viz_clear_meters(viz);
}

void nuade_viz_record(NuadeViz* viz, u64 frame, const NuadePlayer* player, void* user_data, const NuadeVoices* voices,
                      NuadeGridClock clock) {
    NuadeSlot* slot = &viz->ring[viz->head];
    slot->frame = frame;
    slot->position = (NuadePosition) { 0 };
    if (player->position != nullptr) {
        const NuadePositionResult position = player->position(user_data);
        if (position.valid) {
            slot->position = position.position;
        }
    }
    u32 grid_row = 0;
    if (clock.ticks_per_row != 0 && clock.ticks_run != 0) {
        const u32 last = clock.ticks_run - 1;
        grid_row = last / clock.ticks_per_row;
        slot->position = (NuadePosition) {
            .mode = NuadeScroll_Rows, .row = grid_row, .tick = last % clock.ticks_per_row, .speed = clock.ticks_per_row
        };
    }

    u32 filled = 0;
    if (player->voice_info != nullptr) {
        filled = player->voice_info(user_data, viz->info, viz->display_voices);
        if (filled > viz->display_voices) {
            filled = viz->display_voices;
        }
    }
    for (u32 v = 0; v < viz->display_voices; v++) {
        NuadeVoiceView* view = &slot->voices[v];
        *view = (NuadeVoiceView) { .info = { .note = -1, .instrument = -1 } };
        if (v < filled) {
            view->info = viz->info[v];
            if (clock.ticks_per_row != 0) {
                view->info.order = 0;
                view->info.row = grid_row;
            }
        }
        if (voices != nullptr) {
            NuadeVoiceState state;
            nuade_voice_get_state(voices, v, &state);
            view->active = state.playing;
            view->pitch_hz = (f32)state.pitch_hz;
            view->volume = state.volume;
            view->pan = state.pan;
            view->sample_pos = (f32)state.position;
        }
    }

    viz->head = (viz->head + 1) % NUADE_SNAPSHOT_RING;
    if (viz->count < NUADE_SNAPSHOT_RING) {
        viz->count++;
    }
}

NuadeSnapshotResult nuade_viz_find(const NuadeViz* viz, u64 frame, NuadeVoiceView* voices, u32 cap) {
    // Newest first: frames never decrease along the ring.
    for (u32 i = 1; i <= viz->count; i++) {
        const NuadeSlot* slot = &viz->ring[(viz->head + NUADE_SNAPSHOT_RING - i) % NUADE_SNAPSHOT_RING];
        if (slot->frame > frame) {
            continue;
        }
        if (voices != nullptr) {
            const u32 n = cap < viz->display_voices ? cap : viz->display_voices;
            memcpy(voices, slot->voices, n * sizeof(NuadeVoiceView));
        }
        return (NuadeSnapshotResult) {
            .valid = true,
            .snapshot = { .frame = slot->frame, .position = slot->position, .voice_count = viz->display_voices }
        };
    }
    return (NuadeSnapshotResult) { 0 };
}

bool nuade_viz_meters_on(const NuadeViz* viz) {
    return viz->meters;
}

void nuade_viz_enable_meters(NuadeViz* viz) {
    if (viz->meters) {
        return;
    }
    const size_t n = viz->voice_count ? viz->voice_count : 1;
    viz->peak = (f32*)calloc(n, sizeof(f32));
    viz->scope = (f32*)calloc(n * NUADE_SCOPE_FRAMES, sizeof(f32));
    if (viz->peak == nullptr || viz->scope == nullptr) {
        free(viz->peak);
        free(viz->scope);
        viz->peak = nullptr;
        viz->scope = nullptr;
        return;
    }
    viz->meters = true;
}

void nuade_viz_clear_meters(NuadeViz* viz) {
    viz->scope_head = 0;
    viz->scope_count = 0;
    if (viz->peak != nullptr) {
        memset(viz->peak, 0, viz->voice_count * sizeof(f32));
    }
}

void nuade_viz_feed(NuadeViz* viz, float* const* voices, u32 frames) {
    for (u32 v = 0; v < viz->voice_count; v++) {
        f32 peak = viz->peak[v];
        f32* ring = viz->scope + (size_t)v * NUADE_SCOPE_FRAMES;
        u32 head = viz->scope_head;
        for (u32 i = 0; i < frames; i++) {
            const f32 x = voices[v][i];
            peak = fmaxf(peak, fabsf(x));
            ring[head] = x;
            head = (head + 1) % NUADE_SCOPE_FRAMES;
        }
        viz->peak[v] = peak;
    }
    const u64 total = (u64)viz->scope_count + frames;
    viz->scope_count = total < NUADE_SCOPE_FRAMES ? (u32)total : NUADE_SCOPE_FRAMES;
    viz->scope_head = (u32)(((u64)viz->scope_head + frames) % NUADE_SCOPE_FRAMES);
}

u32 nuade_viz_vu(NuadeViz* viz, float* out, u32 cap) {
    const u32 n = cap < viz->voice_count ? cap : viz->voice_count;
    for (u32 v = 0; v < n; v++) {
        out[v] = viz->peak[v];
        viz->peak[v] = 0.0f;
    }
    return n;
}

u32 nuade_viz_scope(const NuadeViz* viz, u32 voice, float* out, u32 cap) {
    if (voice >= viz->voice_count) {
        return 0;
    }
    const u32 n = cap < viz->scope_count ? cap : viz->scope_count;
    const f32* ring = viz->scope + (size_t)voice * NUADE_SCOPE_FRAMES;
    u32 pos = (viz->scope_head + NUADE_SCOPE_FRAMES - n) % NUADE_SCOPE_FRAMES;
    for (u32 i = 0; i < n; i++) {
        out[i] = ring[pos];
        pos = (pos + 1) % NUADE_SCOPE_FRAMES;
    }
    return n;
}
