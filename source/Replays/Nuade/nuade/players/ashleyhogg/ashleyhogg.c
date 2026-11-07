// SPDX-License-Identifier: MIT
// Ashley Hogg player: the entry points of the two driver families (ah_old.c, ah_new.c) and the helpers they share; the
// module checks and the table locating are in ah_module.c.

#include "ah_internal.h"

// AUDxLC ignores its lowest bit, so a sample at an odd offset is fetched from the byte before it.

void ah_set_sample(const AhMemory* m, NuadePaulaChannel* paula, uint32_t offset, uint16_t length_words) {
    offset &= ~1u;
    paula->lc = offset <= m->capacity ? (const int8_t*)(m->data + offset) : nullptr;
    paula->len_words = length_words;
}

uint8_t ah_paula_volume(unsigned volume) {
    return (uint8_t)(volume & AH_VOLUME_MASK);
}

static void sync_channels(NuadePaulaChannel* channels, NuadeVoices* voices) {
    for (int i = 0; i < AH_VOICES; i++) {
        nuade_paula_channel_sync(&channels[i], (uint32_t)i, voices);
    }
}

bool ah_start(AhPlayer* player, int subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || subsong >= ah_subsong_count(player)) {
        return false;
    }
    if (player->family == AhFamily_New) {
        if (subsong >= NEW_MAX_SUBSONG) {
            return false;
        }
        ah_new_start(&player->new_family, &player->module, subsong);
    } else {
        ah_old_start(&player->old_family, &player->module, subsong);
    }
    for (int i = 0; i < AH_VOICES; i++) {
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

AhTickResult ah_tick(AhPlayer* player, NuadeVoices* voices) {
    if (player->family == AhFamily_New) {
        AhNewSong* song = &player->new_family.song;
        ah_new_tick(&player->new_family, &player->module);
        sync_channels(song->paula, voices);
        return (AhTickResult) { .looped = song->looped, .ended = false };
    }
    AhOldSong* song = &player->old_family.song;
    ah_old_tick(&player->old_family, &player->module);
    sync_channels(song->paula, voices);
    return (AhTickResult) { .looped = song->looped, .ended = song->ended };
}

uint32_t ah_grid_ticks(const AhPlayer* player, int subsong) {
    if (subsong < 0 || subsong >= ah_subsong_count(player)) {
        return 0;
    }
    if (player->family == AhFamily_New) {
        return NEW_DEFAULT_PHASE_LIMIT;
    }
    return ah_u8(&player->module, player->old_family.tables.speeds + (uint32_t)subsong);
}

AhVoiceState ah_voice_state(const AhPlayer* player, int voice) {
    if (voice < 0 || voice >= AH_VOICES) {
        return (AhVoiceState) { .note = -1, .instrument = -1 };
    }
    if (player->family == AhFamily_New) {
        const AhNewVoice* v = &player->new_family.song.voices[voice];
        return (AhVoiceState) { .note = v->display_note, .instrument = v->display_instrument };
    }
    const AhOldVoice* v = &player->old_family.song.voices[voice];
    return (AhVoiceState) { .note = v->display_note, .instrument = v->display_instrument };
}
