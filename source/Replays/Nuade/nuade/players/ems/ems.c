// SPDX-License-Identifier: MIT
// EMS player, translated from the UADE eagleplayer "EMS" (Editeur Musical Sequentiel 1.24 player module, Wanted Team).
//
// EmsModule: byte 1, a version byte, the position count and the highest pattern number (words), a 0x80-byte position
// list (pattern numbers), four bytes, fifteen 12-byte instrument records, then the patterns (64 rows of four 4-byte
// cells = 1024 bytes) and the samples, one after the other.

#include <nuade/players/ems.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define POSITION_COUNT_OFFSET 2
#define PATTERN_MAX_OFFSET 4
#define POSITION_LIST_OFFSET 6
#define INSTRUMENTS_OFFSET 0x86
#define INSTRUMENT_SIZE 12
#define PATTERNS_OFFSET 0x13A
#define CELL_SIZE 4
#define ROW_SIZE (EMS_VOICES * CELL_SIZE)
#define PATTERN_SIZE (EMS_ROWS * ROW_SIZE)
#define PATTERN_LIMIT 0x3F // highest pattern number Check2 accepts
#define VERSION_LIMIT 0x25 // the second header byte

#define DEFAULT_SPEED 6
#define MASTER_VOLUME 64 // DeliTracker's master volume, the replayer scales every volume by it

// A slide stops at these periods.
#define PERIOD_MIN 0x70
#define PERIOD_MAX 0x6B0

// The replayer starts the samples two bytes before the end of the last pattern and clears the first word of each, so it
// plays the data one word early and silences the last word of what precedes it.
#define SAMPLE_START_BIAS 2

// A loop starts up to 0xFFFF words past the start of its sample, so the zeros after the image reach that far before
// the Paula tail of nuade_paula_padded_copy.
#define LOOP_REACH (2 * 0xFFFF)

// The periods Check2 accepts in the first pattern: C-1 down to B-3 plus the notes below.
static const uint16_t k_periods[] = {
    1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960, 906, 856, 808, 762, 720,
    678,  640,  604,  570,  538,  508,  480,  453,  428,  404,  381, 360, 339, 320, 302, 285,
    269,  254,  240,  226,  214,  202,  190,  180,  170,  160,  151, 143, 135, 127, 120, 113,
};

// What Check2 and InitPlayer read from the header.
typedef struct Header {
    int position_count;
    int pattern_count;
    uint64_t image_size; // the file, or what the header says the patterns and samples take if that is more
} Header;

static bool is_known_period(uint16_t period) {
    for (size_t i = 0; i < nuade_sizeof_array(k_periods); i++) {
        if (k_periods[i] == period) {
            return true;
        }
    }
    return false;
}

// One instrument record; the sample pointer is made by the loader.
static EmsInstrument parse_instrument(const uint8_t* data, int index) {
    const uint8_t* r = data + INSTRUMENTS_OFFSET + index * INSTRUMENT_SIZE;
    return (EmsInstrument) { .length = nuade_read_be16(r + 4),
                             .loop_start = nuade_read_be16(r + 6),
                             .loop_length = nuade_read_be16(r + 8),
                             .volume = nuade_read_be16(r + 10) };
}

// Check2. The size InitPlayer wants of the file is not enforced: a file that is short of it plays, and its missing
// samples read as zeros (UADE does the same).
static bool parse_header(const uint8_t* data, size_t size, Header* out) {
    if (data == nullptr || size <= PATTERNS_OFFSET + PATTERN_SIZE) {
        return false;
    }
    const uint16_t position_count = nuade_read_be16(data + POSITION_COUNT_OFFSET);
    const uint16_t pattern_max = nuade_read_be16(data + PATTERN_MAX_OFFSET);
    if (data[0] != 1 || data[1] > VERSION_LIMIT || position_count == 0 || position_count > EMS_POSITION_LIST_SIZE
        || pattern_max > PATTERN_LIMIT) {
        return false;
    }
    for (int i = 0; i < EMS_POSITION_LIST_SIZE; i++) {
        if (data[POSITION_LIST_OFFSET + i] > pattern_max) {
            return false;
        }
    }
    int known_periods = 0;
    for (int i = 0; i < EMS_ROWS * EMS_VOICES; i++) {
        const uint16_t period = nuade_read_be16(data + PATTERNS_OFFSET + i * CELL_SIZE);
        if (period & 0x8000) {
            return false;
        }
        if (period != 0) {
            if (!is_known_period(period)) {
                return false;
            }
            known_periods++;
        }
    }
    if (known_periods == 0) {
        return false;
    }
    uint64_t required = PATTERNS_OFFSET + (uint64_t)(pattern_max + 1) * PATTERN_SIZE;
    for (int i = 0; i < EMS_INSTRUMENTS; i++) {
        required += (uint64_t)parse_instrument(data, i).length * 2;
    }
    *out = (Header) { .position_count = position_count,
                      .pattern_count = pattern_max + 1,
                      .image_size = required > size ? required : size };
    return true;
}

// The pattern at a position of the list, as an offset into the module.
static uint32_t pattern_offset(const EmsModule* m, int position) {
    return PATTERNS_OFFSET + (uint32_t)m->positions[position] * PATTERN_SIZE;
}

// A cell of the pattern at `pattern`. The row is the replayer's 16-bit row number times 16 as a signed word, so a
// break row past the pattern reads whatever follows it, and one past the file reads zeros.
static EmsCell read_cell(const EmsModule* m, uint32_t pattern, uint16_t row, int voice) {
    const int64_t offset = (int64_t)pattern + (int16_t)(uint16_t)(row << 4) + voice * CELL_SIZE;
    if (offset < 0 || offset + CELL_SIZE > m->size) {
        return (EmsCell) { 0 };
    }
    const uint8_t* c = m->data + offset;
    return (EmsCell) { .period = nuade_read_be16(c), .instrument = c[2] >> 4, .effect = c[2] & 0x0F, .param = c[3] };
}

// A volume through the master volume, as 16-bit arithmetic.
static uint16_t scale_volume(uint16_t volume) {
    return (uint16_t)((uint16_t)(volume * MASTER_VOLUME) >> 6);
}

static void write_period(EmsVoice* v, uint16_t period) {
    v->paula.period = period;
}

// The cell's instrument: its sample is started at the end of the row, its volume is set now.
static void select_instrument(EmsVoice* v, const EmsModule* m, int instrument) {
    const EmsInstrument* ins = &m->instruments[instrument - 1];
    v->sample = ins->sample;
    v->length = ins->length;
    v->loop_start = ins->loop_start;
    v->loop_length = ins->loop_length;
    v->volume = scale_volume(ins->volume);
    v->paula.volume = (uint8_t)v->volume;
    v->instrument = instrument;
}

// A cell's effect on its row; the ones that act on every tick are kept for play_tick_effects.
static void play_effect(EmsSong* s, EmsVoice* v, const EmsCell* cell) {
    switch (cell->effect) {
        case EmsEffect_VolumeSlide:
        case EmsEffect_SlideUp:
        case EmsEffect_SlideDown:
        case EmsEffect_Vibrato:
            v->effect = (EmsEffect)cell->effect;
            v->param = cell->param;
            break;
        case EmsEffect_Break:
            s->break_row = cell->param;
            s->row = EMS_ROWS - 1;
            break;
        case EmsEffect_Speed:
            // Parameter 0 is a bug in the replayer: it skips the dispatch with a return address left on the stack and
            // runs on through memory. Not modelled; the command does nothing.
            if (cell->param != 0) {
                s->speed = cell->param;
            }
            break;
        case EmsEffect_Filter:
            // The Amiga's LED filter: UADE renders without it.
            break;
        case EmsEffect_SetVolume:
            v->volume = scale_volume(cell->param);
            v->paula.volume = (uint8_t)v->volume;
            break;
        case EmsEffect_Restart:
            v->loop_length = v->length;
            v->loop_start = 0;
            break;
        default:
            break;
    }
}

static void play_cell(EmsSong* s, EmsVoice* v, const EmsModule* m, const EmsCell* cell) {
    v->sample = nullptr;
    if (cell->instrument != 0) {
        select_instrument(v, m, cell->instrument);
    }
    if (cell->period != 0) {
        write_period(v, cell->period);
        v->period = cell->period;
    }
    play_effect(s, v, cell);
}

// Next row; a pattern ends after its 64th row, and the position list wraps after its last position.
static void advance_row(EmsSong* s, const EmsModule* m) {
    s->row++;
    if (s->row != EMS_ROWS) {
        return;
    }
    s->row = s->break_row;
    s->break_row = 0;
    s->position++;
    if (s->position >= m->position_count) {
        s->position = 0;
        s->looped = true;
    }
    s->pattern_offset = pattern_offset(m, s->position);
}

static void play_row(EmsPlayer* p) {
    EmsSong* s = &p->song;
    const EmsModule* m = &p->module;
    EmsCell cells[EMS_VOICES];
    s->shown_position = s->position;
    s->shown_row = s->row;
    for (int i = 0; i < EMS_VOICES; i++) {
        cells[i] = read_cell(m, s->pattern_offset, s->row, i);
        s->voices[i].effect = EmsEffect_None;
    }
    // A new instrument stops the voice before anything else is written.
    for (int i = 0; i < EMS_VOICES; i++) {
        if (cells[i].instrument != 0) {
            nuade_paula_channel_dma_off(&s->voices[i].paula);
        }
    }
    for (int i = 0; i < EMS_VOICES; i++) {
        play_cell(s, &s->voices[i], m, &cells[i]);
    }
    advance_row(s, m);
    // After a raster wait: the samples of the new instruments, then their DMA. The repeat follows at the next tick.
    for (int i = 0; i < EMS_VOICES; i++) {
        EmsVoice* v = &s->voices[i];
        if (v->sample != nullptr) {
            v->paula.lc = v->sample;
            v->paula.len_words = v->length;
        }
    }
    for (int i = 0; i < EMS_VOICES; i++) {
        if (cells[i].instrument != 0) {
            nuade_paula_channel_dma_on(&s->voices[i].paula);
        }
    }
}

// The tick after a row: the repeat of the samples that row started.
static void write_repeats(EmsSong* s) {
    for (int i = 0; i < EMS_VOICES; i++) {
        EmsVoice* v = &s->voices[i];
        if (v->sample != nullptr) {
            v->paula.lc = v->sample + (size_t)v->loop_start * 2;
            v->paula.len_words = v->loop_length;
            v->sample = nullptr;
        }
    }
}

static void slide_volume(EmsVoice* v) {
    const int volume = (int16_t)v->volume - v->param;
    if (volume > 0) {
        v->volume = (uint16_t)volume;
    } else {
        v->volume = 0;
        v->effect = EmsEffect_None;
    }
    v->paula.volume = (uint8_t)v->volume;
}

// A slide takes the new period only inside the limits. The period stays as the slide left it.
static void slide_period(EmsVoice* v, int new_period) {
    const bool inside = v->effect == EmsEffect_SlideUp ? new_period >= PERIOD_MIN : new_period <= PERIOD_MAX;
    if (inside) {
        v->period = (uint16_t)new_period;
        write_period(v, v->period);
    }
}

static void play_tick_effects(EmsSong* s) {
    for (int i = 0; i < EMS_VOICES; i++) {
        EmsVoice* v = &s->voices[i];
        switch (v->effect) {
            case EmsEffect_VolumeSlide:
                slide_volume(v);
                break;
            case EmsEffect_SlideUp:
                slide_period(v, (int16_t)(v->period - v->param));
                break;
            case EmsEffect_SlideDown:
                slide_period(v, (int16_t)(v->period + v->param));
                break;
            case EmsEffect_Vibrato:
                // Alternates around the period without changing it.
                write_period(v, (uint16_t)(v->period + ((s->tick & 1) ? v->param : -v->param)));
                break;
            default:
                break;
        }
    }
    s->tick++;
}

bool ems_probe(const uint8_t* data, size_t size) {
    Header header;
    return parse_header(data, size, &header);
}

EmsPlayer* ems_create(const uint8_t* data, size_t size) {
    Header header;
    if (!parse_header(data, size, &header)) {
        return nullptr;
    }
    EmsPlayer* p = calloc(1, sizeof(EmsPlayer));
    uint8_t* copy = nuade_paula_padded_copy(nullptr, (size_t)header.image_size + LOOP_REACH);
    if (p == nullptr || copy == nullptr) {
        free(p);
        free(copy);
        return nullptr;
    }
    memcpy(copy, data, size);

    EmsModule* m = &p->module;
    m->data = copy;
    m->size = (uint32_t)size;
    m->position_count = header.position_count;
    memcpy(m->positions, data + POSITION_LIST_OFFSET, EMS_POSITION_LIST_SIZE);
    uint32_t sample_offset = PATTERNS_OFFSET + (uint32_t)header.pattern_count * PATTERN_SIZE - SAMPLE_START_BIAS;
    for (int i = 0; i < EMS_INSTRUMENTS; i++) {
        EmsInstrument* ins = &m->instruments[i];
        *ins = parse_instrument(copy, i);
        if (ins->length != 0) {
            ins->sample = (const int8_t*)copy + sample_offset;
            copy[sample_offset] = 0;
            copy[sample_offset + 1] = 0;
            sample_offset += (uint32_t)ins->length * 2;
        }
    }
    return p;
}

void ems_destroy(EmsPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module.data);
    free(player);
}

// InitSound
bool ems_start(EmsPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    EmsSong* s = &player->song;
    *s = (EmsSong) { .speed = DEFAULT_SPEED, .pattern_offset = pattern_offset(&player->module, 0) };
    for (int i = 0; i < EMS_VOICES; i++) {
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

// Interrupt
EmsTickResult ems_tick(EmsPlayer* player, NuadeVoices* voices) {
    EmsSong* s = &player->song;
    s->looped = false;
    if (s->tick < s->speed) {
        if (s->tick == 0) {
            write_repeats(s);
        }
        play_tick_effects(s);
    } else {
        s->tick = 0;
        play_row(player);
    }
    for (int i = 0; i < EMS_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, (uint32_t)i, voices);
    }
    return (EmsTickResult) { .looped = s->looped };
}

EmsPosition ems_position(const EmsPlayer* player) {
    const EmsSong* s = &player->song;
    return (EmsPosition) { .order = s->shown_position, .row = s->shown_row, .speed = s->speed + 1 };
}

int ems_order_count(const EmsPlayer* player) {
    return player->module.position_count;
}

EmsVoiceState ems_voice_state(const EmsPlayer* player, int voice) {
    const EmsVoice* v = &player->song.voices[voice];
    return (EmsVoiceState) { .period = v->period, .instrument = v->instrument };
}

EmsCell ems_pattern_cell(const EmsPlayer* player, int order, int voice, int row) {
    const EmsModule* m = &player->module;
    return read_cell(m, pattern_offset(m, order), (uint16_t)row, voice);
}
