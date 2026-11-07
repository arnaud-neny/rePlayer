// SPDX-License-Identifier: MIT
// Maniacs of Noise player: the song level. The start routine sets the song's volume, tempo and the voices' sequences;
// one interrupt steps the voices from the last to the first and then the fade of the song volume. The sequencer, which
// the voice replayers of all versions share, is here too.

#include "mon_internal.h"
#include <stdlib.h>
#include <string.h>

#define MASTER_VOLUME 64 // DeliTracker's volume, which the eagleplayer's Volume routine writes after the start

bool mon_sequence_next(MonPlayer* p, int voice, bool repeating) {
    const MonModule* m = &p->module;
    MonVoice* v = &p->voices[voice];
    if (repeating) {
        v->repeat--;
        if (v->repeat != 0) {
            v->pattern = v->pattern_start;
            return true;
        }
        v->repeat = 1;
    }
    uint32_t at = v->sequence;
    for (int guard = 0; guard < MON_COMMAND_GUARD; guard++) {
        const uint16_t word = mon_u16(m, at);
        at += 2;
        if (mon_sequence_is_pattern(word)) {
            v->pattern_start = mon_song_address(m, word);
            v->pattern = v->pattern_start;
            v->sequence = at;
            return true;
        }
        const uint8_t parameter = mon_sequence_parameter(word);
        switch (mon_sequence_command(word)) {
            case MonSequenceCommand_Transpose:
                v->transpose = parameter;
                break;
            case MonSequenceCommand_Repeat:
                v->repeat = parameter;
                break;
            case MonSequenceCommand_FadeIn:
                p->fade = (MonFade) {
                    .direction = 1, .reload = parameter, .counter = parameter, .target = p->start_volume
                };
                p->master = 0;
                break;
            case MonSequenceCommand_FadeOut:
                p->fade = (MonFade) { .direction = -1, .reload = parameter, .counter = parameter, .target = 0 };
                break;
            case MonSequenceCommand_Tempo:
                p->tempo = parameter;
                break;
            case MonSequenceCommand_Jump:
                at = v->track + (uint32_t)parameter * 2;
                break;
            case MonSequenceCommand_Stop:
            case MonSequenceCommand_Unknown: // the walk of the songs has refused a module with one
                p->playing = false;
                return false;
        }
    }
    p->playing = false;
    return false;
}

bool mon_probe(const uint8_t* data, size_t size) {
    MonModule module;
    return mon_locate(data, size, &module);
}

MonPlayer* mon_create(const uint8_t* data, size_t size) {
    MonPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    player->chip = nuade_paula_padded_copy(data, size);
    if (player->chip == nullptr || !mon_locate(player->chip, size, &player->module)) {
        free(player->chip);
        free(player);
        return nullptr;
    }
    return player;
}

void mon_destroy(MonPlayer* player) {
    if (player != nullptr) {
        free(player->chip);
        free(player);
    }
}

int mon_subsong_count(const MonPlayer* player) {
    return (int)player->module.subsong_count;
}

// The init routine leaves several fields of the voice records as the image has them: the high byte of the voice volume,
// the fixed period, the swing and the slide that a note-on does not reset. (The instrument pointer is an address of the
// run that made the image; the songs select an instrument before their first note.)

static void load_voice_record(const MonModule* m, int voice, MonVoice* v) {
    const MonLayout* layout = m->layout;
    const uint32_t record = m->base + layout->voice_records + (uint32_t)voice * layout->voice_stride;
    v->volume_high = mon_u8(m, record + layout->record_volume_high);
    v->slide_length = mon_u16(m, record + layout->record_slide_length);
    v->slide_from = mon_u16(m, record + layout->record_slide_from);
    if (layout->record_period_fixed != 0) {
        v->period_fixed = mon_u16(m, record + layout->record_period_fixed);
    }
    if (layout->record_swing != 0) {
        v->swing = mon_u8(m, record + layout->record_swing);
    }
}

bool mon_start(MonPlayer* player, int subsong, NuadeVoices* voices) {
    const MonModule* m = &player->module;
    MonSong song;
    if (voices == nullptr || subsong < 0 || !mon_read_song(m, (uint32_t)subsong, &song)) {
        return false;
    }
    memset(player->voices, 0, sizeof(player->voices));
    memset(player->paula, 0, sizeof(player->paula));
    const uint8_t envelope_mask = mon_u8(m, m->base + m->layout->envelope_mask);
    for (int i = 0; i < MON_VOICES; i++) {
        MonVoice* v = &player->voices[i];
        load_voice_record(m, i, v);
        v->running = song.tracks[i] != 0;
        v->envelope_on = (envelope_mask >> i & 1) != 0;
        v->track = mon_song_address(m, song.tracks[i]);
        v->sequence = v->track;
        v->instrument = MON_NO_ADDRESS;
        v->repeat = 1;
        v->restart = true;
        v->display_note = -1;
        v->display_instrument = -1;
        nuade_voice_stop(voices, (uint32_t)i);
    }
    player->instrument_register = MON_NO_ADDRESS;
    player->master = MASTER_VOLUME;
    player->tempo = song.tempo;
    player->start_volume = song.master_volume;
    player->fade = (MonFade) { 0 };
    player->playing = true;
    return true;
}

// The fade of the song volume, one step every `reload` ticks. A fade-out that reaches 0 switches the voices off.

static bool run_fade(MonPlayer* p) {
    MonFade* fade = &p->fade;
    if (fade->direction == 0) {
        return false;
    }
    fade->counter--;
    if (fade->counter != 0) {
        return false;
    }
    fade->counter = fade->reload;
    p->master = (uint8_t)(p->master + fade->direction);
    if (p->master != fade->target) {
        return false;
    }
    fade->direction = 0;
    if (p->master != 0) {
        return false;
    }
    for (int i = 0; i < MON_VOICES; i++) {
        if (p->voices[i].running) {
            nuade_paula_channel_dma_off(&p->paula[i]);
        }
    }
    p->playing = false;
    return true;
}

// The interrupt: the voices from the last to the first, then the fade. True when the fade ended the song.

static bool run_interrupt(MonPlayer* p) {
    for (int voice = MON_VOICES - 1; voice >= 0; voice--) {
        if (p->voices[voice].running && !mon_step_voice(p, voice)) {
            return false;
        }
    }
    return run_fade(p);
}

MonTickResult mon_tick(MonPlayer* player, NuadeVoices* voices) {
    bool ended = false;
    if (player->playing) {
        ended = run_interrupt(player);
    }
    for (int i = 0; i < MON_VOICES; i++) {
        nuade_paula_channel_sync(&player->paula[i], (uint32_t)i, voices);
    }
    return (MonTickResult) { .ended = ended };
}

MonVoiceState mon_voice_state(const MonPlayer* player, int voice) {
    if (voice < 0 || voice >= MON_VOICES) {
        return (MonVoiceState) { .note = -1, .instrument = -1 };
    }
    const MonVoice* v = &player->voices[voice];
    return (MonVoiceState) { .note = v->display_note, .instrument = v->display_instrument };
}
