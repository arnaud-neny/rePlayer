// SPDX-License-Identifier: MIT
// Leggless Music Editor: the eagleplayer's Check2 and InitPlayer checks, the walk that checks everything the replayer
// can reach, and the typed readers of the module records.

#include "lme_internal.h"
#include "nuade_base.h"
#include "nuade_utils.h"
#include <stdlib.h>

#define LME_MARK 0x4C4D4500u // "LME\0"
#define LME_ZERO_LONG_OFFSET 0x24u
// The tables are counted from here.
#define LME_TABLE_BASE 0x28u
// The first entry of the subsong table: instruments, pattern table, pattern data and sample base.
#define LME_SUBSONG_ENTRY_BYTES 16u
#define LME_INSTRUMENT_BYTES 0x3Au
#define LME_POSITION_END 0xFFFFu
#define LME_POSITION_HAS_TRANSPOSE 0x8000u
#define LME_FLAGS_END 0x8000u
#define LME_MIN_ARPEGGIO_COUNT 2u
// Events the probe's walk may read in all (the three real songs need under 10,000).
#define LME_WALK_EVENT_BUDGET 1000000u
#define LME_MAX_WAVE_WORDS (LME_WAVE_BYTES / 2)
// InitPlayer's size check starts its running sample offset here and adds this to the sample base.
#define LME_INIT_MIN_SAMPLE_OFFSET 3u
#define LME_INIT_SAMPLE_SLACK 4u

// The flags word of an event.
#define LME_FLAG_STOP 0x01u
#define LME_FLAG_INSTRUMENT 0x02u
#define LME_FLAG_SLIDE 0x04u
#define LME_FLAG_LEGATO 0x08u
#define LME_FLAG_ARPEGGIO 0x10u

static const uint16_t k_periods[LME_NOTE_COUNT]
    = { 0x358, 0x328, 0x2FA, 0x2D0, 0x2A6, 0x280, 0x25C, 0x23A, 0x21A, 0x1FC, 0x1E0, 0x1C5, 0x1AC,
        0x194, 0x17D, 0x168, 0x153, 0x140, 0x12E, 0x11D, 0x10D, 0x0FE, 0x0F0, 0x0E2, 0x0D6, 0x0CA,
        0x0BE, 0x0B4, 0x0AA, 0x0A0, 0x097, 0x08F, 0x087, 0x07F, 0x078, 0x071, 0x06B };

static bool fetch_u16(const LmeModule* module, uint32_t* pos, uint16_t* value) {
    if (*pos > module->size - 2) {
        return false;
    }
    *value = nuade_read_be16(module->bytes + *pos);
    *pos += 2;
    return true;
}

static uint32_t table_long(const LmeModule* module, uint32_t offset) {
    return nuade_read_be32(module->bytes + offset);
}

// The subsong table entry of `subsong` is the one after the shared first entry.

static uint32_t list_entry(uint32_t subsong, uint32_t voice) {
    return LME_TABLE_BASE + LME_SUBSONG_ENTRY_BYTES * (subsong + 1) + 4 * voice;
}

// InitPlayer adds up the lengths of the samples (a record counts when its sample offset is above all before it, from 3)
// and refuses the song when that and the sample base do not fit in the file.

static bool init_player_size_ok(const LmeModule* module, uint32_t sample_base) {
    const uint32_t first_list = table_long(module, list_entry(0, 0));
    const uint32_t records
        = (uint32_t)((first_list - (module->instruments - LME_TABLE_BASE)) / LME_INSTRUMENT_BYTES) & 0xFFFFu;
    if (records == 0) {
        return false;
    }
    uint64_t total = 0;
    uint32_t highest = LME_INIT_MIN_SAMPLE_OFFSET;
    for (uint32_t i = 0; i < records; i++) {
        const LmeInstrument instrument = lme_instrument(module, i);
        if (instrument.sample_offset > highest) {
            highest = instrument.sample_offset;
            total += (uint64_t)instrument.length_words * 2;
        }
    }
    return total + LME_TABLE_BASE + sample_base + LME_INIT_SAMPLE_SLACK <= module->size;
}

bool lme_module_parse(LmeModule* module, const uint8_t* bytes, size_t size) {
    *module = (LmeModule) { .bytes = bytes, .size = size };
    if (size < LME_TABLE_BASE + 2 * LME_SUBSONG_ENTRY_BYTES || nuade_read_be32(bytes) != LME_MARK
        || nuade_read_be32(bytes + LME_ZERO_LONG_OFFSET) != 0) {
        return false;
    }
    const uint32_t instruments = nuade_read_be32(bytes + LME_TABLE_BASE);
    const uint32_t patterns = nuade_read_be32(bytes + LME_TABLE_BASE + 4);
    const uint32_t pattern_data = nuade_read_be32(bytes + LME_TABLE_BASE + 8);
    const uint32_t samples = nuade_read_be32(bytes + LME_TABLE_BASE + 12);
    if (instruments < 2 * LME_SUBSONG_ENTRY_BYTES || instruments % LME_SUBSONG_ENTRY_BYTES != 0
        || instruments > patterns || patterns > pattern_data || pattern_data > samples
        || (uint64_t)LME_TABLE_BASE + samples > size || patterns % 2 != 0 || pattern_data % 2 != 0
        || samples % 2 != 0) {
        return false;
    }
    module->subsong_count = instruments / LME_SUBSONG_ENTRY_BYTES - 1;
    module->instruments = LME_TABLE_BASE + instruments;
    module->pattern_table = LME_TABLE_BASE + patterns;
    module->pattern_count = (pattern_data - patterns) / 4;
    if (module->pattern_count > LME_MAX_PATTERNS) {
        module->pattern_count = LME_MAX_PATTERNS;
    }
    module->pattern_data = LME_TABLE_BASE + pattern_data;
    module->samples = LME_TABLE_BASE + samples;

    // The position lists sit between the instruments and the pattern table; the instruments end at the first.
    uint32_t first_list = patterns;
    for (uint32_t subsong = 0; subsong < module->subsong_count; subsong++) {
        for (uint32_t voice = 0; voice < LME_VOICES; voice++) {
            const uint32_t list = nuade_read_be32(bytes + list_entry(subsong, voice));
            if (list < instruments || list >= patterns || list % 2 != 0) {
                return false;
            }
            if (list < first_list) {
                first_list = list;
            }
        }
    }
    module->instrument_count = (first_list - instruments) / LME_INSTRUMENT_BYTES;
    if (module->instrument_count == 0 || module->pattern_count == 0) {
        return false;
    }
    if (module->instrument_count > LME_MAX_INSTRUMENTS) {
        module->instrument_count = LME_MAX_INSTRUMENTS;
    }
    return init_player_size_ok(module, samples);
}

uint32_t lme_position_list(const LmeModule* module, uint32_t subsong, uint32_t voice) {
    return LME_TABLE_BASE + table_long(module, list_entry(subsong, voice));
}

bool lme_read_position(const LmeModule* module, uint32_t* list_pos, LmePosition* position) {
    uint16_t word;
    *position = (LmePosition) { 0 };
    if (!fetch_u16(module, list_pos, &word)) {
        return false;
    }
    if (word == LME_POSITION_END) {
        position->end = true;
        return true;
    }
    if ((word & LME_POSITION_HAS_TRANSPOSE) != 0) {
        uint16_t transpose;
        if (!fetch_u16(module, list_pos, &transpose)) {
            return false;
        }
        position->transpose = (int16_t)transpose;
        word &= (uint16_t)~LME_POSITION_HAS_TRANSPOSE;
    }
    if (word >= module->pattern_count) {
        return false;
    }
    const uint32_t offset = table_long(module, module->pattern_table + 4u * word);
    if (offset > module->size || module->pattern_data > module->size - offset || offset % 2 != 0) {
        return false;
    }
    position->pattern_start = module->pattern_data + offset;
    return true;
}

bool lme_note_period(uint16_t note, int16_t transpose, uint16_t* period) {
    const uint16_t index = (uint16_t)(note + (uint16_t)transpose);
    if (index >= LME_NOTE_COUNT) {
        return false;
    }
    *period = k_periods[index];
    return true;
}

// The arpeggio command: the slot count (the note's own period is the first), the ticks between slots and the others.

static bool read_arpeggio(const LmeModule* module, uint32_t* pos, LmeArpeggio* arpeggio) {
    if (!fetch_u16(module, pos, &arpeggio->count) || !fetch_u16(module, pos, &arpeggio->reload)
        || arpeggio->count < LME_MIN_ARPEGGIO_COUNT || arpeggio->count > LME_ARPEGGIO_SLOTS) {
        return false;
    }
    for (uint32_t i = 0; i + 1 < arpeggio->count; i++) {
        if (!fetch_u16(module, pos, &arpeggio->notes[i])) {
            return false;
        }
    }
    return true;
}

LmeRead lme_read_event(const LmeModule* module, uint32_t* pos, bool first_of_pattern, LmeEvent* event) {
    uint16_t flags;
    *event = (LmeEvent) { 0 };
    if (!fetch_u16(module, pos, &flags)) {
        return LmeRead_Invalid;
    }
    if ((flags & LME_FLAGS_END) != 0 && !first_of_pattern) {
        return LmeRead_PatternEnd;
    }
    event->stop = (flags & LME_FLAG_STOP) != 0;
    if (event->stop) {
        return fetch_u16(module, pos, &event->duration) ? LmeRead_Event : LmeRead_Invalid;
    }
    event->legato = (flags & LME_FLAG_LEGATO) != 0;
    event->has_instrument = (flags & LME_FLAG_INSTRUMENT) != 0;
    event->has_arpeggio = (flags & LME_FLAG_ARPEGGIO) != 0;
    event->has_slide = (flags & LME_FLAG_SLIDE) != 0;
    if ((event->has_instrument && !fetch_u16(module, pos, &event->instrument))
        || (event->has_arpeggio && !read_arpeggio(module, pos, &event->arpeggio))) {
        return LmeRead_Invalid;
    }
    if (event->has_slide) {
        uint16_t steps;
        if (!fetch_u16(module, pos, &event->slide.note) || !fetch_u16(module, pos, &steps)
            || !fetch_u16(module, pos, &event->slide.delay)) {
            return LmeRead_Invalid;
        }
        event->slide.steps = (int16_t)steps;
    }
    return fetch_u16(module, pos, &event->duration) && fetch_u16(module, pos, &event->note) ? LmeRead_Event
                                                                                            : LmeRead_Invalid;
}

LmeInstrument lme_instrument(const LmeModule* module, uint32_t index) {
    const uint8_t* r = module->bytes + module->instruments + (uint64_t)index * LME_INSTRUMENT_BYTES;
    return (LmeInstrument) {
        .sample_offset = nuade_read_be32(r),
        .length_words = nuade_read_be16(r + 0x04),
        .volume = nuade_read_be16(r + 0x06),
        .loop_offset = nuade_read_be32(r + 0x08),
        .loop_words = nuade_read_be16(r + 0x0C),
        .vibrato = { .offset = (int16_t)nuade_read_be16(r + 0x0E),
                     .step = (int16_t)nuade_read_be16(r + 0x10),
                     .direction = nuade_read_be16(r + 0x12),
                     .max = (int16_t)nuade_read_be16(r + 0x14),
                     .min = (int16_t)nuade_read_be16(r + 0x16),
                     .delay = (int16_t)nuade_read_be16(r + 0x18) },
        .envelope = { .phase = (LmeEnvelopePhase)nuade_read_be16(r + 0x1A),
                      .attack_step = (int16_t)nuade_read_be16(r + 0x1C),
                      .attack_max = (int16_t)nuade_read_be16(r + 0x1E),
                      .hold = (int16_t)nuade_read_be16(r + 0x20),
                      .decay_step = (int16_t)nuade_read_be16(r + 0x22),
                      .decay_min = (int16_t)nuade_read_be16(r + 0x24) },
        .wave = { .type = (LmeWaveType)nuade_read_be16(r + 0x26),
                  .width = (int16_t)nuade_read_be16(r + 0x28),
                  .grow_step = (int16_t)nuade_read_be16(r + 0x2A),
                  .max = (int16_t)nuade_read_be16(r + 0x2C),
                  .shrink_step = (int16_t)nuade_read_be16(r + 0x2E),
                  .min = (int16_t)nuade_read_be16(r + 0x30),
                  .value_a = (int16_t)nuade_read_be16(r + 0x32),
                  .value_b = (int16_t)nuade_read_be16(r + 0x34) },
        .retrigger = { .counter = nuade_read_be16(r + 0x36), .period = nuade_read_be16(r + 0x38) },
    };
}

const int8_t* lme_sample_at(const LmeModule* module, uint32_t offset) {
    return (const int8_t*)(module->bytes + module->samples + (offset & ~1u));
}

// The pulse width grows by one step per tick up to the maximum (the test is the sign of the difference, as in the
// replayer), then shrinks to the minimum and grows again.

void lme_wave_step(LmeWave* wave) {
    if (wave->type != LmeWaveType_Shrinking) {
        wave->width = (int16_t)(wave->width + wave->grow_step);
        if (lme_difference_not_negative(wave->width, wave->max)) {
            wave->width = wave->max;
            wave->type = LmeWaveType_Shrinking;
        }
        return;
    }
    wave->width = (int16_t)(wave->width - wave->shrink_step);
    if (wave->width <= wave->min) {
        wave->width = wave->min;
        wave->type = LmeWaveType_Growing;
    }
}

// A sample must lie inside the file: Paula would read whatever the player's memory holds behind it. AUDxLC bit 0 does
// not exist, so a sample at an odd offset is read from the even address below it.

static bool sample_in_file(const LmeModule* module, uint32_t offset, uint32_t words) {
    const uint64_t end = (uint64_t)module->samples + (offset & ~1u) + (uint64_t)words * 2;
    return words != 0 && end <= module->size;
}

// A synth wave writes `width - 1` bytes of one value, then the rest of the buffer; every width the pulse can take must
// keep that inside the buffer. The (type, width) pair repeats after at most 2^17 steps.

static bool wave_widths_ok(const LmeInstrument* instrument) {
    if (instrument->wave.type > LmeWaveType_Shrinking || instrument->length_words == 0
        || instrument->length_words > LME_MAX_WAVE_WORDS) {
        return false;
    }
    const int32_t most = 2 * (int32_t)instrument->length_words + 1;
    uint8_t* seen = calloc(2u * 65536u / 8u, 1);
    if (seen == nullptr) {
        return false;
    }
    LmeWave wave = instrument->wave;
    bool ok = true;
    for (;;) {
        lme_wave_step(&wave);
        if (wave.width < 1 || wave.width > most || wave.type > LmeWaveType_Shrinking) {
            ok = false;
            break;
        }
        const uint32_t state = (uint32_t)(wave.type - LmeWaveType_Growing) * 65536u + (uint16_t)wave.width;
        if ((seen[state / 8] & (1u << (state % 8))) != 0) {
            break;
        }
        seen[state / 8] = (uint8_t)(seen[state / 8] | (1u << (state % 8)));
    }
    free(seen);
    return ok;
}

static bool instrument_ok(const LmeModule* module, uint32_t index) {
    const LmeInstrument instrument = lme_instrument(module, index);
    if (instrument.wave.type != LmeWaveType_Sample) {
        return wave_widths_ok(&instrument);
    }
    return sample_in_file(module, instrument.sample_offset, instrument.length_words)
           && sample_in_file(module, instrument.loop_offset, instrument.loop_words);
}

static uint32_t gcd(uint32_t a, uint32_t b) {
    while (b != 0) {
        const uint32_t rest = a % b;
        a = b;
        b = rest;
    }
    return a;
}

typedef struct Walk {
    const LmeModule* module;
    bool* instrument_checked; // per instrument: the check passed
    uint32_t grid;
    uint64_t events_left; // the probe refuses a song whose position lists make it read more events than this
} Walk;

typedef struct WalkVoice {
    int16_t transpose;
    bool instrument_selected;
} WalkVoice;

// Checks what one event makes the replayer read.

static bool walk_event(Walk* walk, WalkVoice* voice, const LmeEvent* event) {
    uint16_t period;
    walk->grid = gcd(walk->grid, event->duration);
    if (event->stop) {
        return true;
    }
    if (event->has_instrument) {
        if (event->instrument >= walk->module->instrument_count) {
            return false;
        }
        if (!walk->instrument_checked[event->instrument]) {
            if (!instrument_ok(walk->module, event->instrument)) {
                return false;
            }
            walk->instrument_checked[event->instrument] = true;
        }
        voice->instrument_selected = true;
    }
    if (event->has_arpeggio) {
        for (uint32_t i = 0; i + 1 < event->arpeggio.count; i++) {
            if (!lme_note_period(event->arpeggio.notes[i], voice->transpose, &period)) {
                return false;
            }
        }
    }
    if (event->has_slide
        && (event->slide.steps < 1 || !lme_note_period(event->slide.note, voice->transpose, &period))) {
        return false;
    }
    return lme_note_period(event->note, voice->transpose, &period) && (event->legato || voice->instrument_selected);
}

// One pattern, event by event, to its end mark.

static bool walk_pattern(Walk* walk, WalkVoice* voice, uint32_t pos) {
    for (bool first = true;; first = false) {
        LmeEvent event;
        const LmeRead read = lme_read_event(walk->module, &pos, first, &event);
        if (walk->events_left == 0) {
            return false;
        }
        walk->events_left--;
        if (read == LmeRead_PatternEnd) {
            return true;
        }
        if (read != LmeRead_Event || !walk_event(walk, voice, &event)) {
            return false;
        }
    }
}

// One position list to its end mark; it must name at least one pattern.

static bool walk_list(Walk* walk, uint32_t list_pos) {
    WalkVoice voice = { 0 };
    for (bool first = true;; first = false) {
        LmePosition position;
        if (!lme_read_position(walk->module, &list_pos, &position)) {
            return false;
        }
        if (position.end) {
            return !first;
        }
        voice.transpose = position.transpose;
        if (!walk_pattern(walk, &voice, position.pattern_start)) {
            return false;
        }
    }
}

bool lme_module_validate(LmeModule* module) {
    Walk walk = { .module = module,
                  .events_left = LME_WALK_EVENT_BUDGET,
                  .instrument_checked = calloc(module->instrument_count, sizeof(bool)) };
    if (walk.instrument_checked == nullptr) {
        return false;
    }
    bool ok = true;
    for (uint32_t subsong = 0; ok && subsong < module->subsong_count; subsong++) {
        for (uint32_t voice = 0; ok && voice < LME_VOICES; voice++) {
            ok = walk_list(&walk, lme_position_list(module, subsong, voice));
        }
    }
    free(walk.instrument_checked);
    module->grid_ticks = walk.grid != 0 ? walk.grid : 1;
    return ok;
}
