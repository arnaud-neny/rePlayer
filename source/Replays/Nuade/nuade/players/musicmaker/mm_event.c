// SPDX-License-Identifier: MIT
// MusicMaker V8 player: the events of both replayers, translated from the UADE eagleplayers "MusicMaker-4V" and
// "MusicMaker-8V" (the 8 voice replayer is the 4 voice one with differences in the note start, the tempo command and
// the bends; they are marked MmVariant_8V below).
//
// Events are 3 bytes: the first is the instrument (high nibble) and volume level (low nibble, 0 = rest) of a note, or
// a command from 0xF0; the second is the note (low 6 bits) and flags; the third holds the row length in its low 6 bits.
// A pattern ends with a first byte 0xFF, and the position list names the next one.

#include "mm_core.h"

#define EVENT_REST 0xF1
#define EVENT_PATTERN_DELAY 0xF0
#define EVENT_SKIP 0xF3
#define EVENT_SKIP_EXTRA 6
#define EVENT_END_OF_PATTERN 0xFF
#define COMMAND_VOLUME_BY_NOTE 0xF4
#define COMMAND_CALLBACK 0xF5
#define COMMAND_BEND 0xF6
#define COMMAND_PITCH_SCALE 0xF7
#define COMMAND_PREFIX 0xF8
#define COMMAND_SLIDE 0xF9
#define COMMAND_TABLE 0xFA
#define COMMAND_TEMPO 0xFC
#define COMMAND_NOP 0xFD
#define GLIDE_SECOND 0xF2 // the event after a glide note: the slides of the untimed form
#define GLIDE_SECOND_DOUBLE 0xFB
#define NOTE_MASK 0x3F
#define NOTE_RESTART_FLAG 0x80
#define LENGTH_MASK 0x3F
#define GLIDE_FLAG 0x80
#define GLIDE_TIMED_FLAG 0x40
#define GLIDE_LEVEL_UP_FLAG 0x80  // in the second event of a timed glide: the target volume level is one higher
#define SLIDE_VOLUME_FLAG 0x80    // in the third byte of a 0xF9 event: the slide is the volume's
#define SLIDE_ALTERNATE_FLAG 0x40 // and the slide alternates with the other kind
#define SLOT_SILENT 0x24          // the slot of a rest; with 8 voices a note in it continues the voice's last sample
#define PATTERN_GUARD 4096
#define LIST_GUARD 4096
#define TEMPO_NIBBLE_SHIFT 4

// The position list of a track that has no voice in an 8 voice song: pattern 0, forever.
static const uint8_t s_unused_list_data[4] = { 0x00, 0x00, 0x03, 0xE7 };
static const MmBlock s_unused_list = { .data = s_unused_list_data, .size = 4 };

static uint8_t event_byte(const MmCore* c, uint32_t offset) {
    return mm_u8(&c->song->block, offset);
}

void mm_core_start(MmCore* c, MmVariant variant, const MmSong* song, const MmInstruments* inst, uint32_t voice_count) {
    *c = (MmCore) { .variant = variant,
                    .song = song,
                    .inst = inst,
                    .voice_count = voice_count,
                    .bend_down_step = 0x3F,
                    .bend_up_step = 0x15 };
    mm_core_set_tempo(c, song->tempo);
    for (uint32_t v = 0; v < voice_count; v++) {
        MmVoice* t = &c->voice[v];
        const bool used = song->has_track[v];
        t->list = used ? &song->block : &s_unused_list;
        const uint32_t base = used ? song->track_list[v] : 0;
        const uint32_t start = used ? song->start_index : 0;
        const uint32_t loop = used ? song->loop_index : 0;
        t->list_pos = base + 2u * start;
        t->loop_pos = base + 2u * loop;
        t->event = mm_song_pattern(song, mm_u16(t->list, t->list_pos));
        t->countdown = 1;
        t->pitch_scale = MM_UNIT_SCALE;
        t->note = -1;
        t->instrument = -1;
        t->note_starts = 0;
    }
}

void mm_core_set_tempo(MmCore* c, uint16_t tempo) {
    c->tempo_base = tempo;
}

bool mm_core_poll_song_end(MmCore* c) {
    if (c->song_end_reported) {
        return false;
    }
    for (uint32_t v = 0; v < c->voice_count; v++) {
        if (c->song->has_track[v] && !c->voice[v].ended) {
            return false;
        }
    }
    c->song_end_reported = true;
    return true;
}

void mm_core_start_table_loop(MmCore* c, uint32_t voice, uint16_t bytes) {
    MmVoice* t = &c->voice[voice];
    if (t->table_active) {
        t->table_loop = true;
        t->table_loop_length = bytes;
    }
}

// The next entry of the position list: a 0 entry is a pause of the song's pause_rows, the end marker jumps back to the
// loop point (the track has looped: the voice is cut at its next event). Returns the new pattern's offset and whether
// the list did not wrap.

static bool next_pattern(MmCore* c, MmVoice* t, uint32_t* event) {
    for (int guard = 0; guard < LIST_GUARD; guard++) {
        t->list_pos += 2;
        const uint16_t entry = mm_u16(t->list, t->list_pos);
        if (entry == 0) {
            t->countdown = (uint16_t)(t->countdown + c->song->pause_rows * 2);
            continue;
        }
        if (entry != MM_END_OF_LIST) {
            *event = mm_song_pattern(c->song, entry);
            return true;
        }
        break;
    }
    t->list_pos = t->loop_pos;
    t->looped = true;
    *event = mm_song_pattern(c->song, mm_u16(t->list, t->list_pos));
    return false;
}

// The voice has passed the end of its list and reaches its next event: everything it plays is cut and the note state
// is cleared. The track goes on.

static void cut_voice(MmCore* c, MmVoice* t) {
    t->bend_down = false;
    t->bend_up = false;
    t->volume2 = 0;
    if (c->variant == MmVariant_8V) {
        t->fx.slide = 0;
        t->fx.volume_slide = 0;
    }
    t->period = 0;
    t->note2 = 0;
    t->pitch_scale = MM_UNIT_SCALE;
    t->table = 0;
    t->table_phase = 0;
    t->table_limit = 0;
    t->table_active = false;
    t->table_loop = false;
    t->table_loop_length = 0;
    t->ended = true;
    t->looped = false;
}

// A rest: the voice gets nothing to play and volume 0 (the period stays the note's). With 4 voices that is the silent
// word (and the DMA stops); with 8 a sample of no length.

static MmStep rest_step(const MmCore* c, uint16_t period, bool stop_dma) {
    MmStep step = { .kind = MmStepKind_Note, .has_sample = true, .slot = SLOT_SILENT, .period = period };
    if (c->variant == MmVariant_4V) {
        step.stop_dma = stop_dma;
        step.sample = c->inst->silent;
        step.length = 2;
        step.loop_sample = c->inst->silent;
        step.loop_length = 2;
    }
    return step;
}

// A glide: the note's period and volume go to the targets in the event, over the row length of the event after it.
// With a time (bit 6 of that event's third byte) the target is the note in the low 6 bits of this event's third byte
// and the volume level of its instrument nibble; otherwise the second byte of the second event holds two signed
// nibbles, the slides per tick (the period slide doubled for the 0xFB form).

static void start_glide(const MmCore* c, MmVoice* t, uint32_t at, uint16_t period, uint8_t volume) {
    const uint8_t b0 = event_byte(c, at);
    const uint8_t b2 = event_byte(c, at + 2);
    const uint8_t second = event_byte(c, at + 3);
    const uint8_t b4 = event_byte(c, at + 4);
    const uint8_t b5 = event_byte(c, at + 5);
    MmEffects* fx = &t->fx;
    if ((b5 & GLIDE_TIMED_FLAG) == 0) {
        const int8_t high = (int8_t)b4 >> 4;
        const int8_t low = (int8_t)(b4 << 4) >> 4;
        fx->slide = (int16_t)(second == GLIDE_SECOND_DOUBLE ? low * 2 : low);
        fx->volume_slide = high;
        return;
    }
    const uint32_t duration = (uint32_t)((b5 & LENGTH_MASK) + 1) * 2;
    int32_t delta = (int32_t)mm_period_table[b2 & NOTE_MASK] - period;
    fx->glide_base = period;
    fx->glide_down = delta < 0;
    fx->glide_step = (uint16_t)(((uint32_t)(delta < 0 ? -delta : delta) << 7) / duration);
    // The volume level of the target: the instrument nibble, one more with bit 7 of the second event. Level 16 reads
    // past the table, where the song's tempo word follows.
    const uint32_t level = (uint32_t)(b0 >> 4) + ((b5 & GLIDE_LEVEL_UP_FLAG) != 0 ? 1u : 0u);
    const int32_t target = level < MM_VOLUME_LEVELS ? mm_volume_levels[level] : c->song->tempo;
    delta = target - volume;
    fx->volume_base = volume;
    fx->fade_down = delta < 0;
    fx->fade_step = (uint16_t)(((uint32_t)(delta < 0 ? -delta : delta) << 7) / duration);
    fx->counter = 0;
}

// The sample, first part and loop of a note of instrument `slot`.

static void select_sample(const MmCore* c, bool loops, uint8_t slot, MmStep* step) {
    const MmSampleRecord* record = &c->inst->record[slot];
    const uint32_t sample = c->inst->sample[slot];
    step->sample = sample;
    if (c->variant == MmVariant_4V) {
        if (loops && record->first_length != 0) {
            step->length = record->first_length;
            step->loop_sample = sample + record->loop_offset;
            step->loop_length = (uint32_t)record->loop_length * 2;
        } else {
            step->length = record->length;
            step->loop_sample = c->inst->silent;
            step->loop_length = 2;
        }
        return;
    }
    if (loops && record->loop_length != 0) {
        step->length = record->first_length;
        step->loop_sample = sample + record->loop_offset;
        step->loop_length = (uint32_t)record->loop_length * 2;
    } else {
        step->length = record->length;
    }
}

// The note event at `*at`. A glide takes the event after it too (`*at` moves to it).

static MmStep note_step(MmCore* c, MmVoice* t, uint32_t* at, uint8_t prefix) {
    const uint8_t b0 = event_byte(c, *at);
    const uint8_t b1 = event_byte(c, *at + 1);
    const uint8_t b2 = event_byte(c, *at + 2);
    const uint8_t b3 = event_byte(c, *at + 3);
    const bool is_8v = c->variant == MmVariant_8V;
    MmEffects* fx = &t->fx;
    fx->slide = 0;
    fx->volume_slide = 0;
    fx->mode = 0;
    fx->toggle = false;
    fx->glide_step = 0;
    fx->fade_step = 0;
    const uint8_t note = b1 & NOTE_MASK;
    const uint16_t period = mm_period_table[note];
    t->note = 12 + note;
    const uint8_t level = b0 & 0xF;
    if (b0 == EVENT_REST) {
        if (is_8v) {
            // Remembers the voice's place; a note of the silent slot continues from there. Note 0 is a rest.
            if (note != 0) {
                return (MmStep) { .kind = MmStepKind_Idle, .save_resume = true };
            }
            MmStep step = rest_step(c, period, false);
            step.save_resume = true;
            return step;
        }
        return rest_step(c, period, true);
    }
    if (level == 0) {
        return rest_step(c, period, true);
    }
    const uint8_t volume = (uint8_t)mm_volume_levels[level];
    if (b2 & GLIDE_FLAG) {
        if (b3 == GLIDE_SECOND || b3 == GLIDE_SECOND_DOUBLE) {
            start_glide(c, t, *at, period, volume);
            *at += MM_EVENT_SIZE;
        }
        return (MmStep) { .kind = MmStepKind_Note, .note2 = (uint8_t)(note * 2), .period = period, .volume = volume };
    }
    const uint8_t slot = (uint8_t)((b0 >> 4) + prefix);
    if (is_8v && slot == SLOT_SILENT) {
        return (MmStep) { .kind = MmStepKind_Note,
                          .has_sample = true,
                          .resume = true,
                          .slot = slot,
                          .note2 = (uint8_t)(note * 2),
                          .period = period,
                          .volume = volume };
    }
    t->instrument = slot;
    t->note_starts++;
    MmStep step = { .kind = MmStepKind_Note,
                    .has_sample = true,
                    .stop_dma = !is_8v,
                    .enable_dma = !is_8v,
                    .slot = slot,
                    .note2 = (uint8_t)(note * 2),
                    .period = period,
                    .volume = volume };
    if (slot >= MM_INSTRUMENTS || (!is_8v && c->inst->record[slot].length == 0)) {
        if (is_8v) {
            return step; // a slot past the table: no sample
        }
        MmStep rest = rest_step(c, period, true);
        rest.enable_dma = true;
        return rest;
    }
    select_sample(c, (b1 & NOTE_RESTART_FLAG) != 0, slot, &step);
    return step;
}

// 0xF9: slide values and the mode that makes them alternate.

static void command_slide(MmVoice* t, uint8_t param, uint8_t flags) {
    MmEffects* fx = &t->fx;
    const int16_t value = (int8_t)param;
    if ((flags & SLIDE_VOLUME_FLAG) == 0) {
        fx->slide = value;
        if (fx->mode < 0) {
            fx->mode = 0;
        }
    } else if ((flags & SLIDE_ALTERNATE_FLAG) == 0) {
        fx->volume_slide = value;
        if (fx->mode > 0) {
            fx->mode = 0;
        }
    } else if (value < 0) {
        if (fx->mode < 0) {
            fx->slide = 0;
        }
        fx->volume_slide = (int16_t)-value;
        fx->mode = 1;
        fx->toggle = false;
    } else {
        if (fx->mode > 0) {
            fx->volume_slide = 0;
        }
        fx->slide = value;
        fx->mode = -1;
        fx->toggle = false;
    }
}

// 0xF6: a pitch bend that runs until the voice's next bend command (with 8 voices, until it reaches its limit).

static void command_bend(MmCore* c, MmVoice* t, uint8_t param) {
    t->bend_down = false;
    t->bend_up = false;
    if (param == 0) {
        return;
    }
    if ((int8_t)param > 0) {
        c->bend_up_step = param;
        t->bend_up = true;
    } else {
        c->bend_down_step = (uint8_t)-(int8_t)param;
        t->bend_down = true;
    }
}

// 0xFA: the high nibble of the parameter (plus the prefix) is an instrument slot, the low one one of the tables after
// the samples. The table is used when its first word names the slot; otherwise the slot has none.

static void command_table(MmCore* c, uint8_t param, uint8_t prefix) {
    const uint8_t slot = (uint8_t)((param >> 4) + prefix);
    const uint8_t which = param & 0xF;
    uint32_t table = which != 0 ? c->inst->pitch_table[which - 1] : 0;
    table = table != 0 && mm_u16(&c->inst->block, table) == slot ? table + 2 : 0;
    if (slot < MM_TABLE_SLOTS) {
        c->slot_table[slot] = table;
    }
}

// 0xFC: the tempo as the song's tempo times the high nibble over the low one. A zero divisor is a divide by zero on
// the 68000; the command is ignored. With 4 voices the timer changes at once, with 8 the new tempo (never above the
// song's) takes effect at the start of the next block.

static void command_tempo(MmCore* c, uint8_t param) {
    const uint32_t numerator = param >> TEMPO_NIBBLE_SHIFT;
    const uint32_t divisor = param & 0xF;
    if (divisor == 0) {
        return;
    }
    uint32_t tempo = numerator * c->song->tempo / divisor;
    if (c->variant == MmVariant_4V) {
        tempo = tempo < MM_TEMPO_MIN_4V ? MM_TEMPO_MIN_4V : (tempo > MM_TEMPO_MAX_4V ? MM_TEMPO_MAX_4V : tempo);
        mm_core_set_tempo(c, (uint16_t)tempo);
    } else {
        tempo = tempo < MM_TEMPO_MIN_8V ? MM_TEMPO_MIN_8V : (tempo > c->song->tempo ? c->song->tempo : tempo);
    }
    c->new_tempo = (uint16_t)tempo;
    c->tempo_requested = true;
}

// The event at `at`, which can also be a command. Returns what the replayer has to write.

static MmStep run_event(MmCore* c, MmVoice* t, uint32_t* at) {
    uint8_t prefix = 0;
    uint8_t b0 = event_byte(c, *at);
    uint8_t b1 = event_byte(c, *at + 1);
    switch (b0) {
        case COMMAND_VOLUME_BY_NOTE:
            t->volume_by_note = b1 != 0;
            return (MmStep) { .kind = MmStepKind_Idle };
        case COMMAND_NOP:
        case COMMAND_CALLBACK: // calls the host's callback, which UADE does not set
            return (MmStep) { .kind = MmStepKind_Idle };
        case COMMAND_BEND:
            command_bend(c, t, b1);
            return (MmStep) { .kind = MmStepKind_Idle };
        case COMMAND_PITCH_SCALE:
            return (MmStep) { .kind = MmStepKind_PitchScale,
                              .pitch_scale = b1 != 0 ? MM_UNIT_SCALE : MM_PITCH_SCALE_MAX };
        case COMMAND_PREFIX:
            prefix = b1;
            *at += MM_EVENT_SIZE;
            b0 = event_byte(c, *at);
            b1 = event_byte(c, *at + 1);
            break;
        default:
            break;
    }
    switch (b0) {
        case COMMAND_SLIDE:
            command_slide(t, b1, event_byte(c, *at + 2));
            return (MmStep) { .kind = MmStepKind_Idle };
        case COMMAND_TABLE:
            command_table(c, b1, prefix);
            return (MmStep) { .kind = MmStepKind_Idle };
        case COMMAND_TEMPO:
            command_tempo(c, b1);
            return (MmStep) { .kind = MmStepKind_Idle };
        default:
            return note_step(c, t, at, prefix);
    }
}

// After an event: the next one. A skip event (0xF3) is jumped over, and a pattern's end loads the next pattern of the
// list; a pattern that starts with a delay event adds it to the row.

static void advance(MmCore* c, MmVoice* t, uint32_t at) {
    at += MM_EVENT_SIZE;
    for (int guard = 0; guard < PATTERN_GUARD; guard++) {
        if (event_byte(c, at) == EVENT_SKIP) {
            at += EVENT_SKIP_EXTRA;
        }
        if (event_byte(c, at) != EVENT_END_OF_PATTERN) {
            break;
        }
        const bool wrapped = !next_pattern(c, t, &at);
        if (wrapped || event_byte(c, at) != EVENT_PATTERN_DELAY) {
            break;
        }
        t->countdown = (uint16_t)(t->countdown + ((event_byte(c, at + 2) & LENGTH_MASK) + 1) * 2);
        at += MM_EVENT_SIZE;
    }
    t->event = at;
}

MmStep mm_core_step(MmCore* c, uint32_t voice) {
    MmVoice* t = &c->voice[voice];
    if (--t->countdown != 0) {
        return (MmStep) { .kind = MmStepKind_Idle };
    }
    const bool cut = t->looped;
    if (cut) {
        cut_voice(c, t);
    }
    uint32_t at = t->event;
    const uint8_t b0 = event_byte(c, at);
    const uint8_t b2 = event_byte(c, at + 2);
    const uint8_t b3 = event_byte(c, at + 3);
    const bool pair = b3 == GLIDE_SECOND || b3 == GLIDE_SECOND_DOUBLE || b0 == COMMAND_PREFIX;
    const uint8_t length_byte = pair ? event_byte(c, at + 5) : b2;
    t->countdown = (uint16_t)(((length_byte & LENGTH_MASK) + 1) * 2);
    MmStep step = run_event(c, t, &at);
    advance(c, t, at);
    step.cut = cut;
    return step;
}
