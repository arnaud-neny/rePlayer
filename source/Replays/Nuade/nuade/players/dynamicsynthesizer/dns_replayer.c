// SPDX-License-Identifier: MIT
// Dynamic Synthesizer player: the replayer. One tick runs the sequencer (every `speed` ticks it plays the next step of
// six tracks) and the voices (volume, envelope and DMA of the four Paula channels). The tracks start notes on the
// voices through a voice allocator; the notes are samples with a volume envelope.

#include "dns_internal.h"
#include "nuade_tracker.h"
#include <string.h>

#define NOTE_OFF_WORD 0x8000    // a pattern word: release the track's note
#define PATTERN_END_WORD 0xFFFF // the rows version: the word that follows the last step
#define RELEASED_FLAG 0x8000    // in a voice's age: its note has been released
#define MAX_LEVEL 0x40
#define MIN_PERIOD 0x71           // the eagleplayer's patch keeps the period at 113 or more
#define VOLUME_REGISTER_MASK 0x7F // the eagleplayer's volume patch keeps 7 bits
#define STEP_SIZE 2
#define AGE_GUARD_LIMIT 0x7FFF            // a voice with this age or more has been released or is silent
#define ALLOCATION_STEP 2                 // voices that the starting point of the search moves per note
#define SAMPLE_BASE_IDLE_LONG 0xFFFFFFFFu // what the newer replayers keep in the first long of the samples

// AUDxLC and AUDxLEN of a run of the samples. AUDxLC ignores its lowest bit; a run plays zeros past the file's end. An
// offset past the file is memory the samples do not own: it plays the zeros behind the file, the only memory with room
// for any length.

static void set_run(const DnsSamples* samples, NuadePaulaChannel* paula, uint32_t offset, uint16_t length_words) {
    offset &= ~1u;
    paula->lc = (const int8_t*)(samples->data + (offset <= samples->size ? offset : samples->size));
    paula->len_words = length_words;
}

// The first long of the samples is the silent run that idle channels repeat.

static void set_sample_base_long(DnsSamples* samples, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        samples->data[i] = (uint8_t)(value >> (24 - 8 * i));
    }
}

// The voice that a new note takes: the one with the greatest age. A voice that is silent has the greatest, then the
// ones that were released, then the ones that have played longest. The search starts at a voice that moves by two per
// note and goes down; a voice must be older than the best so far to replace it.

static unsigned allocate_voice(DnsPlayer* player) {
    unsigned best = player->allocation_turn % DNS_VOICES;
    uint16_t best_age = 0;
    for (unsigned i = 0; i < DNS_VOICES; i++) {
        const unsigned voice = (player->allocation_turn - i) % DNS_VOICES;
        if (best_age < player->voices[voice].age) {
            best = voice;
            best_age = player->voices[voice].age;
        }
    }
    player->allocation_turn = (player->allocation_turn + ALLOCATION_STEP) % DNS_VOICES;
    return best;
}

// AUDxVOL from a level: the eagleplayer keeps 7 bits (the channel applies Paula's rule to them).

static uint8_t paula_volume(int16_t level) {
    return (uint8_t)((uint16_t)level & VOLUME_REGISTER_MASK);
}

static int16_t limited_level(int16_t level, uint8_t volume_offset) {
    const int16_t sum = (int16_t)(level + volume_offset);
    return sum > MAX_LEVEL ? MAX_LEVEL : sum;
}

// The period of a note: the table value plus the sample's detune, never below the eagleplayer's minimum.

static uint16_t note_period(const DnsModule* module, uint8_t note, uint16_t detune) {
    const uint16_t period = (uint16_t)(dns_read_period(module, note) + detune);
    return period < MIN_PERIOD ? MIN_PERIOD : period;
}

// A note starts on a voice: DMA stops, the registers get the sample's first run, the envelope starts its attack. The
// volume register is not written here; the next tick's volume update does it.

static void start_voice(DnsPlayer* player, unsigned index, uint8_t sample, uint8_t note, uint8_t volume_offset) {
    DnsVoice* voice = &player->voices[index];
    const DnsInstrument instrument = dns_read_instrument(&player->module, sample);
    nuade_paula_channel_dma_off(&voice->paula);
    set_run(&player->samples, &voice->paula, instrument.start, instrument.start_words);
    voice->instrument = instrument;
    voice->step = instrument.attack_step;
    voice->counter = instrument.interval;
    voice->counter_reload = instrument.interval;
    voice->phase = DnsEnvelopePhase_Attack;
    voice->level = limited_level(instrument.attack_from, volume_offset);
    voice->target = limited_level(instrument.attack_to, volume_offset);
    voice->paula.period = note_period(&player->module, note, instrument.detune);
    voice->dma_state = player->module.version.dma_note_state;
    voice->held = true;
    voice->age = 0;
    voice->display_note = nuade_period_to_note(dns_read_period(&player->module, note));
    voice->display_instrument = sample;
}

// A track plays a note: a voice is chosen and started.

static void note_on(DnsPlayer* player, unsigned track_index, uint16_t word) {
    const DnsVersion* version = &player->module.version;
    DnsTrack* track = &player->tracks[track_index];
    const uint8_t low = (uint8_t)word;
    const uint8_t volume_offset = (uint8_t)((low & version->volume_mask) >> version->volume_shift);
    const uint8_t sample = (uint8_t)((low & 0xF) + track->sample_add);
    const uint8_t note = (uint8_t)((word >> 8) + track->note_add + 1);
    if (version->note_clears_sample_base) {
        set_sample_base_long(&player->samples, 0);
    }
    const unsigned voice = allocate_voice(player);
    if (version->note_unmaps_tracks) {
        for (int i = 0; i < DNS_TRACKS; i++) {
            if (player->tracks[i].voice == voice) {
                player->tracks[i].voice = DNS_NO_VOICE;
            }
        }
    }
    track->voice = (uint16_t)voice;
    start_voice(player, voice, sample, note, volume_offset);
    player->voices_active = true;
}

// The release of a voice's note, if its track has let go: the envelope turns to its release phase.

static void begin_release(DnsVoice* voice) {
    if (voice->held) {
        return;
    }
    if (voice->instrument.release_jumps == 1) {
        voice->level = voice->instrument.sustain;
    }
    voice->target = 0;
    voice->counter = voice->instrument.release_interval;
    voice->counter_reload = voice->instrument.release_interval;
    voice->step = voice->instrument.release_step;
    voice->phase = DnsEnvelopePhase_Release;
}

// A track lets go of its note.

static void note_off(DnsPlayer* player, unsigned track_index) {
    DnsTrack* track = &player->tracks[track_index];
    const DnsNoteOffGuard guard = player->module.version.note_off_guard;
    if (guard == DnsNoteOffGuard_Map && track->voice == DNS_NO_VOICE) {
        return;
    }
    DnsVoice* voice = &player->voices[track->voice & 3];
    if (guard == DnsNoteOffGuard_Age && voice->age >= AGE_GUARD_LIMIT) {
        return;
    }
    if (guard == DnsNoteOffGuard_Map) {
        track->voice = DNS_NO_VOICE;
    }
    voice->held = false;
    voice->age |= RELEASED_FLAG;
    begin_release(voice);
}

// One step of a track's pattern.

static void run_track(DnsPlayer* player, unsigned track_index) {
    DnsTrack* track = &player->tracks[track_index];
    if (track->muted || track->pattern == 0) {
        return;
    }
    const bool rows = player->module.version.sequencer == DnsSequencer_Rows;
    if (rows
        && dns_read_pattern_word(&player->module, track->pattern, (int16_t)(player->step + STEP_SIZE))
               == PATTERN_END_WORD) {
        player->pattern_ended = true;
    }
    const uint16_t word = dns_read_pattern_word(&player->module, track->pattern, (int16_t)player->step);
    if (word == 0) {
        return;
    }
    if (word == NOTE_OFF_WORD) {
        track->sounding = false;
        note_off(player, track_index);
        return;
    }
    if (player->module.version.retrigger_releases && track->sounding) {
        note_off(player, track_index);
    }
    track->sounding = true;
    note_on(player, track_index, word);
}

// The sequencer reaches the end of the song: it loops, and the eagleplayer's SongEnd runs.

static void wrap_position(DnsPlayer* player) {
    player->position = player->loop_position;
    player->looped = true;
}

// The tracks get their patterns for the next position of the song.

static void load_position(DnsPlayer* player) {
    player->step = 0;
    player->pattern_ended = false;
    for (unsigned t = 0; t < DNS_TRACKS; t++) {
        const DnsTrackRow row = dns_read_track_row(&player->module, t, player->position);
        DnsTrack* track = &player->tracks[t];
        track->pattern = dns_pattern_offset(&player->module, row.pattern);
        if (row.pattern != 0) {
            track->note_add = row.note_add;
            track->sample_add = row.sample_add;
        }
    }
    player->position = (uint16_t)(player->position + 1);
    const DnsVersion* version = &player->module.version;
    const bool at_end = version->end_is_exact ? player->end_position == player->position
                                              : (int16_t)player->end_position < (int16_t)player->position;
    if (at_end) {
        wrap_position(player);
    }
}

// The sequencer part of the tick. True when the tick must not update the voices.

static bool run_sequencer(DnsPlayer* player) {
    if (!player->playing) {
        return false;
    }
    player->beat_counter = (uint16_t)(player->beat_counter - 1);
    if (player->beat_counter != 0) {
        return false;
    }
    player->beat_counter = player->speed;
    for (unsigned t = 0; t < DNS_TRACKS; t++) {
        run_track(player, t);
    }
    player->step = (uint16_t)(player->step + STEP_SIZE);
    if (player->module.version.sequencer == DnsSequencer_Rows) {
        if (player->pattern_ended) {
            load_position(player);
        }
        return false;
    }
    if ((int16_t)player->step_limit <= (int16_t)player->step) {
        load_position(player);
        return player->module.version.new_position_skips_voices;
    }
    return false;
}

// A voice's envelope: every `counter_reload` ticks the level steps towards the target; at the target the phase ends.

static void update_envelope(const DnsVersion* version, DnsVoice* voice) {
    if (voice->level == 0) {
        voice->age = version->silent_age;
        return;
    }
    if (!version->age_counts_in_dma_step) {
        voice->age++;
    }
    if (voice->phase == DnsEnvelopePhase_Idle) {
        return;
    }
    if (voice->phase != DnsEnvelopePhase_Release) {
        voice->age &= (uint16_t)~RELEASED_FLAG;
    }
    voice->counter--;
    if (voice->counter != 0) {
        return;
    }
    voice->counter = voice->counter_reload;
    voice->level = (int16_t)(voice->level + voice->step);
    const bool reached = voice->step >= 0 ? voice->level >= voice->target : voice->level <= voice->target;
    if (!reached) {
        return;
    }
    voice->level = voice->target;
    if (voice->phase == DnsEnvelopePhase_Release) {
        voice->phase = DnsEnvelopePhase_Idle;
    } else if (voice->phase == DnsEnvelopePhase_Decay) {
        begin_release(voice);
    } else {
        voice->target = voice->instrument.sustain;
        voice->counter = voice->instrument.decay_interval;
        voice->counter_reload = voice->instrument.decay_interval;
        voice->step = voice->instrument.decay_step;
        voice->phase = DnsEnvelopePhase_Decay;
    }
}

// A voice's DMA sequence after a note: DMA starts one tick later (two with the delay state), the tick after that the
// repeat registers get the sample's loop.

static void step_dma(DnsPlayer* player, DnsVoice* voice) {
    const DnsVersion* version = &player->module.version;
    if (version->age_counts_in_dma_step) {
        voice->age++;
    }
    switch (voice->dma_state) {
        case DnsDmaState_Idle:
            break;
        case DnsDmaState_Delay:
            voice->dma_state = DnsDmaState_Start;
            break;
        case DnsDmaState_Start:
            if (version->age_counts_in_dma_step) {
                voice->age = 0;
            }
            nuade_paula_channel_dma_on(&voice->paula);
            voice->dma_state = DnsDmaState_Repeat;
            break;
        case DnsDmaState_Repeat:
            set_run(&player->samples, &voice->paula, voice->instrument.loop, voice->instrument.loop_words);
            voice->dma_state = DnsDmaState_Idle;
            break;
    }
}

// The voice part of the tick, once a note has played: the volume registers get the levels, then the envelopes and the
// DMA sequences step.

static void update_voices(DnsPlayer* player) {
    if (!player->voices_active) {
        return;
    }
    for (int i = 0; i < DNS_VOICES; i++) {
        player->voices[i].paula.volume = paula_volume(player->voices[i].level);
    }
    for (int i = 0; i < DNS_VOICES; i++) {
        update_envelope(&player->module.version, &player->voices[i]);
    }
    for (int i = 0; i < DNS_VOICES; i++) {
        step_dma(player, &player->voices[i]);
    }
}

void dns_replayer_tick(DnsPlayer* player) {
    if (player->module.version.voices_first) {
        update_voices(player);
        run_sequencer(player);
    } else if (!run_sequencer(player)) {
        update_voices(player);
    }
}

// The start of a song: the replayer's variables begin as the module has them (zero), the song's fields are copied,
// and the first position is loaded.

void dns_replayer_start(DnsPlayer* player, unsigned song_number) {
    const DnsVersion* version = &player->module.version;
    memset(player->voices, 0, sizeof(player->voices));
    memset(player->tracks, 0, sizeof(player->tracks));
    memcpy(player->samples.data, player->samples.head, sizeof(player->samples.head));
    for (int i = 0; i < DNS_VOICES; i++) {
        player->voices[i].display_note = -1;
        player->voices[i].display_instrument = -1;
    }
    player->playing = false;
    player->voices_active = false;
    player->allocation_turn = 0;
    player->looped = false;
    if (version->ages_start_silent) {
        set_sample_base_long(&player->samples, SAMPLE_BASE_IDLE_LONG);
        for (int i = 0; i < DNS_VOICES; i++) {
            player->voices[i].age = 0xFFFF;
        }
    }
    const DnsSong song = dns_read_song(&player->module, song_number);
    player->position = song.position;
    player->loop_position = song.loop_position;
    player->end_position = song.end_position;
    player->speed = song.speed;
    player->step_limit = song.step_limit;
    for (int t = 0; t < DNS_TRACKS; t++) {
        player->tracks[t].muted = song.muted[t];
    }
    load_position(player);
    player->beat_counter = 1;
    player->playing = true;
}
