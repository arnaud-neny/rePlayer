// SPDX-License-Identifier: MIT
// Ashley Hogg player, new family ("Amiga Audio Driver": Cosmic Spacehead, Fantastic Dizzy).
//
// The driver has seven entry points; the eagleplayer calls the init, the "play song" with the subsong number, and the
// tick (50 Hz). The samples are in a separate file of 16 byte records (start, loop start, attack and loop length,
// volume, period row) followed by the sample data.
//
// A song is an order list of pattern numbers; a pattern is four byte offsets, one event stream per voice. A step is
// `phase_limit` ticks. The tick that is phase 1 of the step reads the events of voices 0 to 2, the one of phase 2 those
// of voice 3 and starts the notes (DMA off, attack registers, DMA on, loop registers: the driver waits for the DMA to
// stop and to start in between); every other tick only runs the effects and writes volumes and periods.
//
// Not translated, because the eagleplayer never calls them: the sound effect entry, the fade, pause and continue
// entries. The vibrato routine in the effect table is not reachable from any effect number. The eagleplayer calls
// SongEnd when the order list wraps (the driver's chaining of one Fantastic Dizzy song into another is patched out).
//
// Not modelled: the 0xFF event increments the order position as a word, which can carry into the high byte; the driver
// keeps the high byte in the pattern number it indexes with, and the jump event keeps it too. The port drops it, which
// differs only for positions past 255 (crafted files only).

#include "ah_internal.h"
#include "nuade_tracker.h"

#define PHASE_READ_FIRST 1 // reads the events of voices 0 to 2
#define PHASE_READ_LAST 2  // reads the events of voice 3 and starts the notes
#define DEFAULT_PHASE (NEW_DEFAULT_PHASE_LIMIT - 1)
#define DEFAULT_VOLUME 0x40
#define PATTERN_RECORD_SIZE 8
#define SONG_ENTRY_SIZE 4
#define MAX_EVENTS 1024 // per call of read_event: an iteration guard against streams that never end a note
// per voice and step: the same for streams that only branch (pattern changes, jumps, loops)
#define MAX_STREAM_RESTARTS 1024

// Event bytes, in the order the driver tests them
#define EVENT_NEXT_PATTERN 0xFF
#define EVENT_JUMP 0xFE
#define EVENT_SPEED 0xFD
#define EVENT_INSTRUMENT 0xF1 // .. 0xFC: instrument 1 .. 12
#define EVENT_EFFECT 0xD9     // .. 0xF0: effect type and ticks, parameter in the next byte
#define EVENT_VOLUME 0xB8     // .. 0xD8: set volume
#define EVENT_LOOP 0x94       // .. 0xB7: repeat the last events
#define EVENT_LOOP_BASE 0x8E  // the repeat count is the event minus this
#define LOOP_BACK_EXTRA 8     // the loop starts this many bytes before the distance the event gives
#define NOTE_SHIFT 2
#define NOTE_MASK 0x3F
#define NOTE_LONG 3 // value of the two low bits of a note event that has a second byte
#define DURATION_MASK 0x3F
#define EFFECT_TYPE_SHIFT 2
#define EFFECT_LOW_MASK 3    // the low two bits of the effect length, from the effect event
#define EFFECT_HIGH_MASK 0xC // the high two bits, from the second byte of a note
#define STEP_SHIFT 6
#define STEP_MASK 3 // the effect parameter's two high bits, minus 1, are its step per tick
#define FORM_MASK 3 // the two low bits of a note event: its form
#define EFFECT_PARAM_MASK 0x3F

// $2E2: every voice's event stream is the pattern the order list names at the current position.

static void load_voices(AhNewSong* s, const AhMemory* m) {
    const uint8_t pattern = ah_u8(m, s->order_list + 1u + s->order_pos);
    const uint32_t record = s->pattern_table + (uint32_t)(int32_t)(int16_t)(pattern * PATTERN_RECORD_SIZE);
    for (int i = 0; i < AH_VOICES; i++) {
        AhNewVoice* v = &s->voices[i];
        v->seq = s->pattern_table + (uint32_t)(int32_t)ah_s16(m, record + (uint32_t)i * 2);
        v->repeating = false;
        v->repeat_count = 0;
    }
}

// $7AA: the next event byte. While a loop counts, every byte read counts; when it is used up the stream continues
// after the loop event.

static uint8_t fetch(const AhMemory* m, AhNewVoice* v, uint32_t* at) {
    const uint8_t value = ah_u8(m, (*at)++);
    if (v->repeating) {
        v->repeat_count--;
        if (v->repeat_count == 0) {
            *at = v->loop_return;
            v->seq = *at;
            v->repeating = false;
        }
    }
    return value;
}

// The period of a note in a row of the period table.

static uint16_t period_of(const AhNewTables* t, const AhMemory* m, const AhNewVoice* v, int note_index) {
    const uint32_t row = t->periods + (uint32_t)(int32_t)(int16_t)(v->sample.period_row * NEW_PERIOD_ROW_SIZE);
    return ah_u16(m, row + (uint32_t)(int32_t)(int16_t)(note_index * 2));
}

// The transposition of the song, in notes.

static int song_transposition(const AhNewTables* t, const AhNewSong* s, const AhMemory* m) {
    return ah_s16(m, t->transpose + (uint32_t)s->subsong * 2);
}

// $7C6: a note with an instrument: the sample's record is copied into the voice, the note starts at the next phase 2.

static void note_on(AhNew* modern, const AhMemory* m, AhNewVoice* v) {
    const AhNewTables* t = &modern->tables;
    if (v->note == 0 || v->instrument == 0) {
        return;
    }
    const uint32_t remap = t->remap + (uint32_t)(uint16_t)(modern->song.subsong << 4);
    const uint8_t sample = ah_u8(m, remap + v->instrument);
    v->pending_sample = (uint16_t)(sample + 1);
    v->sample = ah_new_read_sample(&modern->samples, sample);
    v->display_instrument = sample;
    v->display_note = nuade_period_to_note(period_of(t, m, v, v->note + song_transposition(t, &modern->song, m)));
}

// $962: a changing effect parameter moves by its step until its ticks are used up.

static void run_effect_parameter(AhNewVoice* v) {
    if (v->effect_ticks == 0) {
        return;
    }
    v->effect_param = (uint16_t)(v->effect_param + v->effect_step);
    v->effect_ticks--;
    if (v->effect_ticks == 0) {
        v->effect = AhNewEffect_None;
        v->effect_step = 0;
        v->effect_new = false;
    }
}

// A note's second byte (and the effect it carries): duration and the effect's length.

static void read_long_duration(const AhMemory* m, AhNewVoice* v, uint32_t* at) {
    const uint8_t byte = fetch(m, v, at);
    v->duration = (uint8_t)((byte & DURATION_MASK) - 1);
    if (v->effect_new) {
        const uint8_t high = (uint8_t)((byte >> 4) & EFFECT_HIGH_MASK);
        v->effect_ticks = (uint8_t)(((v->effect_ticks & EFFECT_LOW_MASK) | high) + 1);
    }
}

// $5DE: one event. Returns true when the voice's stream has been replaced and has to be read from its start.

static bool read_event(AhNew* modern, const AhMemory* m, AhNewVoice* v) {
    AhNewSong* s = &modern->song;
    if (v->seq == 0) {
        return false;
    }
    uint32_t at = v->seq;
    v->effect_new = false;
    for (int guard = 0; guard < MAX_EVENTS; guard++) {
        const uint8_t event = fetch(m, v, &at);
        if (event == EVENT_NEXT_PATTERN) {
            s->order_pos++;
            if (s->order_pos == s->order_len) {
                s->order_pos = 0;
                s->looped = true;
            }
            load_voices(s, m);
            return true;
        } else if (event == EVENT_JUMP) {
            s->order_pos = fetch(m, v, &at);
            load_voices(s, m);
            return true;
        } else if (event == EVENT_SPEED) {
            s->phase_limit = fetch(m, v, &at);
        } else if (event >= EVENT_INSTRUMENT) {
            v->instrument = (uint8_t)(event - (EVENT_INSTRUMENT - 1));
            v->retrigger = true;
        } else if (event >= EVENT_EFFECT) {
            const uint8_t kind = (uint8_t)(event - EVENT_EFFECT);
            v->effect_ticks = (uint8_t)((v->effect_ticks & EFFECT_HIGH_MASK) | (kind & EFFECT_LOW_MASK));
            const uint8_t parameter = fetch(m, v, &at);
            v->effect = (AhNewEffect)((kind >> EFFECT_TYPE_SHIFT) + 1);
            v->effect_step = (int8_t)(((parameter >> STEP_SHIFT) & STEP_MASK) - 1);
            v->effect_param = parameter & EFFECT_PARAM_MASK;
            v->effect_new = true;
            if (v->effect == AhNewEffect_SetVolume) {
                v->volume_locked = true;
            }
        } else if (event >= EVENT_VOLUME) {
            v->effect = AhNewEffect_SetVolume;
            v->effect_param = (uint16_t)((event - EVENT_VOLUME) & NOTE_MASK);
            v->effect_ticks = 0;
            v->volume_locked = true;
        } else if (event >= EVENT_LOOP) {
            const uint8_t distance = ah_u8(m, at++);
            v->repeat_count = (uint16_t)(event - EVENT_LOOP_BASE);
            v->loop_return = at;
            v->seq = at - LOOP_BACK_EXTRA - distance;
            v->repeating = true;
            return true;
        } else {
            const uint8_t form = event & FORM_MASK;
            const uint8_t note = (event >> NOTE_SHIFT) & NOTE_MASK;
            if (form == NOTE_LONG) {
                if (note != 0) {
                    v->note = note;
                }
                read_long_duration(m, v, &at);
            } else {
                if (v->effect_new) {
                    v->effect_ticks = 1;
                    v->effect_step = 0;
                }
                v->duration = ah_u8(m, modern->tables.durations + form);
                if (note != 0) {
                    v->note = note;
                }
            }
            if (note != 0) {
                note_on(modern, m, v);
            }
            v->seq = at;
            return false;
        }
    }
    return false;
}

// $5C2: a voice's sequencer step. A stream that keeps replacing itself without reaching a note is cut off.

static void run_sequencer(AhNew* modern, const AhMemory* m, int index) {
    AhNewVoice* v = &modern->song.voices[index];
    run_effect_parameter(v);
    if (v->duration != 0) {
        v->duration--;
        return;
    }
    for (int restarts = 0; restarts < MAX_STREAM_RESTARTS && read_event(modern, m, v); restarts++) {}
}

// $A4E: the parameter's high nibble raises the volume, else its low nibble lowers it.

static void slide_volume(AhNewVoice* v) {
    const int up = (v->effect_param >> 4) & 0xF;
    const int down = v->effect_param & 0xF;
    int volume = v->volume & AH_VOLUME_MASK;
    if (up != 0) {
        volume = volume + up > AH_MAX_VOLUME ? AH_MAX_VOLUME : volume + up;
    } else {
        volume = volume - down < 0 ? 0 : volume - down;
    }
    v->volume = (uint8_t)volume;
    v->volume_locked = false;
}

// $920: the voice's effect, except on the tick after it was defined.

static void run_effect(AhNewVoice* v) {
    if (v->effect == AhNewEffect_None) {
        return;
    }
    if (v->effect_new) {
        v->effect_new = false;
        return;
    }
    switch (v->effect) {
        case AhNewEffect_SlideUp:
            v->period_offset = (int16_t)(v->period_offset - (int16_t)v->effect_param);
            break;
        case AhNewEffect_SlideDown:
            v->period_offset = (int16_t)(v->period_offset + (int16_t)v->effect_param);
            break;
        case AhNewEffect_VolumeSlide:
        case AhNewEffect_VolumeSlide2:
            slide_volume(v);
            break;
        case AhNewEffect_SetVolume:
            v->volume = (uint8_t)v->effect_param;
            v->volume_locked = false;
            break;
        case AhNewEffect_None:
        case AhNewEffect_Nothing:
            break;
    }
}

// $82E: effect, volume and period of a voice.

static void update_voice(AhNew* modern, const AhMemory* m, int index) {
    AhNewVoice* v = &modern->song.voices[index];
    NuadePaulaChannel* paula = &modern->song.paula[index];
    run_effect(v);
    paula->volume = ah_paula_volume(v->volume);
    paula->period
        = (uint16_t)(period_of(&modern->tables, m, v, v->note + song_transposition(&modern->tables, &modern->song, m))
                     + v->period_offset);
}

// $4A4: the volume a note starts with is the sample's, unless a volume command has set it.

static bool loads_sample_volume(AhNewVoice* v) {
    if (v->retrigger) {
        v->retrigger = false;
        if (v->volume_locked) {
            return false;
        }
        if (v->effect == AhNewEffect_SetVolume) {
            v->effect = AhNewEffect_None;
        }
        return true;
    }
    return v->effect != AhNewEffect_SetVolume;
}

// $47E: the voices with a new note stop their DMA; their attack registers are written, the DMA starts, and then the
// loop registers (the driver's two interrupts, with a wait for the DMA in front of each).

static void start_notes(AhNew* modern) {
    AhNewSong* s = &modern->song;
    const AhMemory* samples = &modern->samples;
    bool started[AH_VOICES] = { false };
    for (int i = 0; i < AH_VOICES; i++) {
        AhNewVoice* v = &s->voices[i];
        if (v->pending_sample == 0) {
            continue;
        }
        started[i] = true;
        nuade_paula_channel_dma_off(&s->paula[i]);
        if (loads_sample_volume(v)) {
            v->volume = ah_new_read_sample(samples, (uint8_t)(v->pending_sample - 1)).volume;
        }
        v->volume_locked = false;
        v->pending_sample = 0;
        v->effect_new = false;
    }
    for (int i = 0; i < AH_VOICES; i++) {
        if (started[i]) {
            ah_set_sample(samples, &s->paula[i], s->voices[i].sample.start, s->voices[i].sample.start_length);
            nuade_paula_channel_dma_on(&s->paula[i]);
        }
    }
    for (int i = 0; i < AH_VOICES; i++) {
        if (started[i]) {
            s->voices[i].period_offset = 0;
            ah_set_sample(samples, &s->paula[i], s->voices[i].sample.loop_start, s->voices[i].sample.loop_length);
        }
    }
}

// $1E0: start a song (1-based number): all DMA off, the voices cleared, the first pattern loaded.

static void start_song(AhNew* modern, const AhMemory* m, int number) {
    AhNewSong* s = &modern->song;
    const AhNewTables* t = &modern->tables;
    for (int i = 0; i < AH_VOICES; i++) {
        s->voices[i] = (AhNewVoice) { .volume = DEFAULT_VOLUME, .display_note = -1, .display_instrument = -1 };
        nuade_paula_channel_dma_off(&s->paula[i]);
    }
    s->subsong = number - 1;
    s->phase_limit = NEW_DEFAULT_PHASE_LIMIT;
    s->phase = DEFAULT_PHASE;
    const uint32_t entry = t->songs + (uint32_t)s->subsong * SONG_ENTRY_SIZE;
    s->order_list = t->songs + (uint32_t)(int32_t)ah_s16(m, entry);
    s->pattern_table = t->songs + (uint32_t)(int32_t)ah_s16(m, entry + 2);
    s->order_len = ah_u8(m, s->order_list);
    s->order_pos = 0;
    load_voices(s, m);
}

// $378: the tick.

void ah_new_tick(AhNew* modern, const AhMemory* module) {
    AhNewSong* s = &modern->song;
    s->looped = false;
    if (s->pending_song >= 0) {
        start_song(modern, module, s->pending_song);
        s->pending_song = -1;
        return;
    }
    s->phase++;
    if (s->phase == PHASE_READ_FIRST) {
        for (int i = 0; i < AH_LAST_VOICE; i++) {
            run_sequencer(modern, module, i);
        }
        update_voice(modern, module, AH_LAST_VOICE);
        return;
    }
    if (s->phase == PHASE_READ_LAST) {
        run_sequencer(modern, module, AH_LAST_VOICE);
        start_notes(modern);
    } else if (s->phase == s->phase_limit) {
        s->phase = 0;
    }
    for (int i = 0; i < AH_VOICES; i++) {
        update_voice(modern, module, i);
    }
}

// The init and "play song" entries, then the tick that the eagleplayer's InitSound runs, which starts the song.

void ah_new_start(AhNew* modern, const AhMemory* module, int subsong) {
    modern->song = (AhNewSong) { .pending_song = subsong + 1 };
    ah_new_tick(modern, module);
}
