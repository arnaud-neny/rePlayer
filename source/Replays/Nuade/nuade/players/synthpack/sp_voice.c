// SPDX-License-Identifier: MIT
// Synth Pack: what the voices and the sequencers of both layouts share.

#include "sp_internal.h"

uint16_t sp_period(unsigned note, int8_t tuning) {
    const uint16_t period = note < SP_PERIOD_COUNT ? sp_periods[note] : 0;
    return (uint16_t)(period - tuning);
}

SpLfoSettings sp_lfo_settings(const SpMemory* set, uint32_t at) {
    return (SpLfoSettings) { .delay = sp_u8(set, at),
                             .speed = sp_u8(set, at + 1),
                             .depth = sp_u8(set, at + 2),
                             .wave = sp_u8(set, at + 3),
                             .after_release = sp_u8(set, at + 4) != 0 };
}

SpWave sp_wave_read(const SpMemory* set, uint32_t header, uint32_t data) {
    return (SpWave) { .header = header,
                      .data = data,
                      .size = sp_u16(set, header + 2),
                      .loop_start = sp_u16(set, header + 4),
                      .loop_end = sp_u16(set, header + 6) };
}

static bool sp_wave_has_loop(const SpWave* wave) {
    return wave->loop_start != 0 || wave->loop_end != 0;
}

bool sp_wave_scanned(const SpWave* wave) {
    return (int16_t)wave->size <= SP_MORPH_MAX_SIZE;
}

// The first run of a note: up to the loop's end for a looped waveform, else the whole waveform.

uint16_t sp_wave_first_length(const SpWave* wave) {
    return (uint16_t)((sp_wave_has_loop(wave) ? wave->loop_end : wave->size) >> 1);
}

static uint16_t sp_wave_loop_length(const SpWave* wave) {
    return (uint16_t)((uint16_t)(wave->loop_end - wave->loop_start) >> 1);
}

uint16_t sp_wave_repeat_length(const SpWave* wave) {
    return sp_wave_has_loop(wave) ? sp_wave_loop_length(wave) : 1;
}

static uint32_t sp_wave_loop_offset(const SpWave* wave) {
    return wave->header + (uint16_t)(wave->data + wave->loop_start);
}

uint32_t sp_wave_scan_offset(const SpWave* wave, unsigned position) {
    return wave->header + wave->data + (position & ~1u) + ((position & 1) != 0 ? SP_MORPH_ALTERNATE : 0);
}

bool sp_wave_in_set(const SpMemory* set, const SpWave* wave) {
    const uint16_t first_length = sp_wave_first_length(wave);
    if (first_length == 0 || !sp_memory_has(set, (wave->header + wave->data) & ~1u, 2u * first_length)) {
        return false;
    }
    if (!sp_wave_has_loop(wave)) {
        return true;
    }
    const uint16_t loop_length = sp_wave_loop_length(wave);
    return loop_length != 0 && sp_memory_has(set, sp_wave_loop_offset(wave) & ~1u, 2u * loop_length);
}

bool sp_wave_scan_in_set(const SpMemory* set, const SpWave* wave, unsigned last_position, uint16_t length) {
    for (unsigned position = 0; position <= last_position; position++) {
        if (!sp_memory_has(set, sp_wave_scan_offset(wave, position) & ~1u, 2u * length)) {
            return false;
        }
    }
    return true;
}

SpVoice sp_voice_new(bool released) {
    return (SpVoice) { .released = released, .adsr = { .phase = SpAdsrPhase_Idle }, .note = -1 };
}

void sp_voice_release(SpVoice* voice) {
    voice->released = true;
    sp_adsr_release(&voice->adsr);
}

void sp_voice_note_on(SpVoice* voice, unsigned note, int8_t tuning, const SpSlide* slide) {
    voice->arp_note = (uint16_t)note;
    voice->note = (int)note - 12; // note 24 (period 856) is C-1, semitone 12
    voice->base_period = sp_period(note, tuning);
    voice->period = voice->base_period;
    voice->released = false;
    sp_adsr_start(&voice->adsr);
    voice->level = 0;
    voice->slide = slide->start;
    voice->restart = SpRestart_Latch;
}

bool sp_voice_silent(const SpVoice* voice) {
    return voice->released
           && (voice->adsr.phase == SpAdsrPhase_Idle
               || (voice->adsr.phase == SpAdsrPhase_Release && voice->adsr.level == 0));
}

void sp_voice_slide(SpVoice* voice, const SpSlide* slide) {
    if (!voice->released) {
        if (voice->slide == 0) {
            return;
        }
        voice->slide = (int16_t)(voice->slide + (voice->slide > 0 ? -slide->step : slide->step));
    } else {
        const int16_t distance = (int16_t)(slide->target - voice->slide);
        if (distance != 0) {
            const int16_t step = distance < 0 ? (int16_t)-slide->release_step : slide->release_step;
            voice->slide = (int16_t)(voice->slide + step);
        }
    }
    voice->period = (uint16_t)(voice->period - voice->slide);
}

// The second tick of a note: the loop is written after the DMA started.

static void sp_voice_repeat(SpVoice* voice, const SpWave* wave, SpChannel* channel, const SpMemory* set) {
    voice->restart = SpRestart_None;
    channel->paula.len_words = sp_wave_repeat_length(wave);
    if (sp_wave_has_loop(wave)) {
        sp_channel_set_lc(channel, set, sp_wave_loop_offset(wave));
    }
}

// The first tick of a note: the sample is written and the DMA stops until the next tick.

static void sp_voice_latch(SpVoice* voice, const SpWave* wave, SpChannel* channel, const SpMemory* set) {
    voice->restart = SpRestart_Repeat;
    channel->latched = true;
    sp_channel_set_sample(channel, set, wave->header + wave->data, sp_wave_first_length(wave));
}

void sp_voice_restart(SpVoice* voice, const SpWave* wave, SpChannel* channel, const SpMemory* set) {
    if (voice->restart == SpRestart_Repeat) {
        sp_voice_repeat(voice, wave, channel, set);
    }
    if (voice->restart == SpRestart_Latch) {
        sp_voice_latch(voice, wave, channel, set);
    }
}

void sp_sequencer_start(SpSequencer* sequencer, unsigned position, unsigned loop_point) {
    *sequencer = (SpSequencer) { .running = true,
                                 .position = position,
                                 .loop_point = loop_point,
                                 .tick = SP_START_TICK,
                                 .speed = SP_DEFAULT_SPEED };
}

bool sp_sequencer_tick(SpSequencer* sequencer) {
    sequencer->tick++;
    const bool row_due = !((int16_t)sequencer->speed > (int16_t)sequencer->tick);
    if (row_due) {
        sequencer->tick = 0;
    }
    return row_due;
}

static void sp_sequencer_next_position(SpSequencer* sequencer) {
    sequencer->repeat = 0;
    sequencer->position = (sequencer->position + 1) % SP_POSITION_COUNT;
}

// After the last row of the position: play it again, or go on, or act on its control.

static void sp_sequencer_position_end(SpSequencer* sequencer, SpSongEvents* events) {
    const uint8_t control = sequencer->entry.control;
    sequencer->row = 0;
    if (control < 0x80) {
        if (control <= sequencer->repeat) {
            sp_sequencer_next_position(sequencer);
        } else {
            sequencer->repeat++;
        }
        return;
    }
    switch (control) {
        case SpControl_SongEnd:
            events->song_end = true;
            break;
        case SpControl_Stop:
            sequencer->running = false;
            events->song_end = true;
            events->stopped = true;
            break;
        case SpControl_SetLoop:
            sequencer->loop_point = sequencer->position;
            sp_sequencer_next_position(sequencer);
            break;
        case SpControl_Loop:
            if (sequencer->loop_point == SP_NO_LOOP_POINT) {
                sp_sequencer_next_position(sequencer);
            } else {
                sequencer->repeat = 0;
                sequencer->position = sequencer->loop_point;
                events->song_end = true;
            }
            break;
        default:
            break;
    }
}

void sp_sequencer_row_end(SpSequencer* sequencer, SpSongEvents* events) {
    sequencer->row++;
    if (sequencer->entry.rows > sequencer->row) {
        return;
    }
    sp_sequencer_position_end(sequencer, events);
}
