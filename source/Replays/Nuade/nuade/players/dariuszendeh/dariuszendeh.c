// SPDX-License-Identifier: MIT
// Darius Zendeh player: the replayer, translated from the code every module carries (see dz_locate.c for how the
// tables are found).
//
// A song is a list of steps, each one (pattern, transpose) per voice. A pattern is a table of rows of 4 bytes
// (instrument, note, volume, flags); the voices play the same row number of their patterns, and the first pattern
// decides where the rows end (instrument 0xff). The interrupt alternates between two ticks: the row tick sets the
// registers of every voice from its row and switches the DMA on, the stop tick that follows switches off the voices
// whose row asked for it.

#include "dz_internal.h"
#include "nuade_paula_channel.h"
#include "nuade_tracker.h"
#include <stdlib.h>

#define STEP_BYTES 2 // one list entry: pattern, transpose
#define ROW_BYTES 4
#define END_MARK 0xff

// The module's records

typedef struct StepEntry {
    uint8_t pattern;
    uint8_t transpose;
} StepEntry;

typedef struct Row {
    uint8_t instrument;
    uint8_t note;         // indexes the period table together with the transpose
    bool slides;          // the note byte's bit 7: start from the last period
    uint8_t slide_amount; // the note byte's other bits
    uint8_t volume;
    bool slides_up; // the flags byte's bit 7: the slide raises the period
    bool stops;     // the DMA stop flag of the flags byte
} Row;

typedef struct Instrument {
    uint32_t sample_offset;
    uint16_t length_words;
} Instrument;

static StepEntry read_step_entry(const DzModule* m, uint32_t offset) {
    return (StepEntry) { .pattern = dzm_u8(m, offset), .transpose = dzm_u8(m, offset + 1) };
}

static Row read_row(const DzModule* m, const DzLayout* layout, uint32_t offset) {
    const uint8_t note = dzm_u8(m, offset + 1);
    const uint8_t flags = dzm_u8(m, offset + 3);
    return (Row) {
        .instrument = dzm_u8(m, offset),
        .note = note,
        .slides = (note & 0x80) != 0,
        .slide_amount = note & 0x7f,
        .volume = dzm_u8(m, offset + 2),
        .slides_up = (flags & 0x80) != 0,
        .stops = layout->stop_on_zero_flags ? flags == 0 : (flags & 1) == 0,
    };
}

// An instrument entry is 8 bytes (sample offset long, a word the replayer does not read, length in words) or, in
// version S, 4 bytes (sample offset word, length in words).
static Instrument read_instrument(const DzModule* m, const DzLayout* layout, uint8_t index) {
    const uint32_t entry = layout->instrument_table + (uint32_t)index * (layout->wide_instruments ? 8u : 4u);
    if (layout->wide_instruments) {
        return (Instrument) { .sample_offset = dzm_u32(m, entry), .length_words = dzm_u16(m, entry + 6) };
    }
    return (Instrument) { .sample_offset = dzm_u16(m, entry), .length_words = dzm_u16(m, entry + 2) };
}

bool dz_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size > 0x7fffffffu) {
        return false;
    }
    const DzModule m = { .data = (uint8_t*)data, .size = (uint32_t)size };
    DzLayout layout;
    return dz_locate(&m, &layout);
}

DzPlayer* dz_create(const uint8_t* data, size_t size) {
    if (data == nullptr || size < 16 || size > 0x7fffffffu) {
        return nullptr;
    }
    DzPlayer* p = calloc(1, sizeof(DzPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    p->module.data = nuade_paula_padded_copy(data, size);
    if (p->module.data == nullptr) {
        free(p);
        return nullptr;
    }
    p->module.size = (uint32_t)size;
    if (!dz_locate(&p->module, &p->layout)) {
        dz_destroy(p);
        return nullptr;
    }
    return p;
}

void dz_destroy(DzPlayer* player) {
    if (player != nullptr) {
        free(player->module.data);
        free(player);
    }
}

char dz_version(const DzPlayer* player) {
    switch (player->layout.version) {
        case DzVersion_S:
            return 'S';
        case DzVersion_1:
            return '1';
        default:
            return '2';
    }
}

int dz_subsong_count(const DzPlayer* player) {
    return player->layout.song_count;
}

// The period of a row: the table entry of note + transpose; with a slide, a note with bit 7 set starts from the
// voice's last period and moves by its other bits, up (a bigger period) when bit 7 of the row's flags is set.
static uint16_t row_period(const DzPlayer* p, DzVoice* v, const Row* row, uint8_t transpose) {
    const DzLayout* layout = &p->layout;
    const uint16_t index = (uint16_t)(row->note + transpose);
    uint16_t period = dzm_u16(&p->module, layout->period_table + 2u * index);
    if (layout->slide) {
        if (row->slides) {
            period = row->slides_up ? (uint16_t)(v->last_period + row->slide_amount)
                                    : (uint16_t)(v->last_period - row->slide_amount);
        }
        v->last_period = period;
    }
    return period;
}

// The registers of one voice from its row. AUDxVOL is 7 bits, values above 64 play as 64.
static void set_voice(DzPlayer* p, int index, const StepEntry* entry) {
    DzVoice* v = &p->voices[index];
    const DzModule* m = &p->module;
    const Row row = read_row(m, &p->layout, p->row_pos + (uint32_t)entry->pattern * p->song.record_size);
    const Instrument ins = read_instrument(m, &p->layout, row.instrument);
    v->paula.len_words = ins.length_words;
    v->paula.lc = dzm_sample(m, p->layout.sample_base + ins.sample_offset);
    v->paula.period = row_period(p, v, &row, entry->transpose);
    const uint8_t volume = row.volume > 0 ? row.volume : 1; // the volume minus the fade (0), at least 1
    v->paula.volume = volume;
    v->note = nuade_period_to_note(v->paula.period);
    v->instrument = row.instrument;
    v->stops = row.stops;
}

// Sets the registers of every voice from the current list step and row, then switches the DMA of all voices on. A
// voice that is still playing is not restarted: it takes the new sample at its next repeat.
static void set_row(DzPlayer* p) {
    for (int i = 0; i < p->song.voices; i++) {
        const StepEntry entry = read_step_entry(&p->module, p->list_pos);
        set_voice(p, i, &entry);
        p->list_pos += STEP_BYTES;
    }
    for (int i = 0; i < p->song.voices; i++) {
        nuade_paula_channel_dma_on(&p->voices[i].paula);
    }
}

// To the next row. The pattern ends where pattern 0 has the end mark; then the next list step, and at the end mark of
// the list the end flag is set; a song that does not stop continues with its loop list, a one-shot song sets no
// registers in that tick.
typedef enum Advance {
    Advance_Row,     // set the registers of the next row
    Advance_Wrapped, // the list ran out and the song continues with its loop list
    Advance_RanOut,  // the list ran out and the song sets nothing
} Advance;

static Advance advance_row(DzPlayer* p) {
    const uint32_t step_bytes = (uint32_t)p->song.voices * STEP_BYTES;
    p->row_pos += ROW_BYTES;
    p->list_pos -= step_bytes;
    if (read_row(&p->module, &p->layout, p->row_pos).instrument != END_MARK) {
        return Advance_Row;
    }
    p->row_pos = p->song.pattern_base;
    p->list_pos += step_bytes;
    if (read_step_entry(&p->module, p->list_pos).pattern != END_MARK) {
        return Advance_Row;
    }
    p->end_flag = true;
    if (p->song.one_shot) {
        return Advance_RanOut;
    }
    p->list_pos = p->song.loop_list;
    return Advance_Wrapped;
}

bool dz_start(DzPlayer* player, int subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || subsong >= player->layout.song_count) {
        return false;
    }
    const DzLayout* layout = &player->layout;
    const DzModule* m = &player->module;
    player->song = layout->songs[subsong];
    player->row_tick = dzm_u16(m, layout->phase) != 0;
    player->end_flag = false;
    player->stopped = false;
    for (int i = 0; i < DZ_VOICES; i++) {
        player->voices[i] = (DzVoice) { .note = -1, .instrument = -1 };
        player->voices[i].last_period
            = dzm_u16(m, layout->last_period_table + 2u * (uint32_t)(player->song.voices - 1 - i));
        nuade_voice_stop(voices, (uint32_t)i);
    }
    player->list_pos = player->song.voices == 2 ? player->song.start_list : player->song.loop_list;
    player->row_pos = player->song.pattern_base;
    set_row(player);
    if (layout->silent_init) {
        for (int i = 0; i < DZ_VOICES; i++) {
            player->voices[i].paula.volume = 0;
            nuade_paula_channel_dma_off(&player->voices[i].paula);
        }
    }
    return true;
}

DzTickResult dz_tick(DzPlayer* player, NuadeVoices* voices) {
    DzTickResult result = { 0 };
    if (!player->stopped) {
        if (!player->row_tick) {
            player->row_tick = true;
            for (int i = 0; i < DZ_VOICES; i++) {
                if (player->voices[i].stops) {
                    nuade_paula_channel_dma_off(&player->voices[i].paula);
                }
            }
        } else {
            player->row_tick = false;
            const Advance advance = advance_row(player);
            result.looped = advance != Advance_Row;
            if (advance != Advance_RanOut) {
                set_row(player);
            }
        }
        if (player->song.ends_at_list_end && player->end_flag) {
            player->stopped = true;
        }
        if (player->stopped) {
            for (int i = 0; i < DZ_VOICES; i++) {
                nuade_paula_channel_dma_off(&player->voices[i].paula);
            }
            result.ended = true;
        }
    }
    for (int i = 0; i < DZ_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, (uint32_t)i, voices);
    }
    return result;
}

DzVoiceState dz_voice_state(const DzPlayer* player, int voice) {
    const DzVoice* v = &player->voices[voice];
    return (DzVoiceState) { .note = v->note, .instrument = v->instrument };
}
