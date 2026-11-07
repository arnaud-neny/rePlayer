// SPDX-License-Identifier: MIT
// Fashion Tracker player, translated from the UADE eagleplayer "Fashion Tracker player module V1.0" (Wanted Team).
//
// The module is the Amiga executable of the original player (code, then data), with its relocations applied against a
// load address the player recovers from the code. InitPlayer scans the first 0x3E8 bytes of that code for three
// instruction patterns and reads the table addresses out of their operands: the sample tables, the instrument table,
// the pattern block and the order list. Patterns are 64 rows of 4 voices of 4 bytes: a period word (0 = no note), one
// byte instrument (high nibble) and effect (low nibble), one parameter byte. A row lasts 6 ticks.

#include <nuade/players/fashiontracker.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define SCAN_SIZE 0x3E8
#define PATTERN_SIZE 0x400
#define ROW_SIZE 16
#define TICKS_PER_ROW 6

// Opcodes the InitPlayer scan looks for.
#define OP_MOVE_L_A1_IND_TO_ABS 0x23D1u // move.l (a1),abs.l : the sample and instrument table operands
#define OP_MOVE_L_ABS_TO_ABS 0x2379u    // move.l abs.l,abs.l : its preceding long is the code's link address
#define OP_MULU_W_400_D0 0xC0FC0400u    // mulu.w #$400,d0 : the pattern block address follows
#define OP_CMPI_W_400_ABS 0x0C790400u   // cmpi.w #$400,abs.l : the order list address and length follow

// The module's check2 (first instructions of the original player).
#define CHECK_LONG_0 0x13FC0040u
#define CHECK_LONG_8 0x4E710439u
#define CHECK_WORD_C 0x0001u
#define CHECK_LONG_12 0x66F44E75u
#define CHECK_LONG_16 0x48E7FFFEu

// The row counter value after the last row: the next row tick starts a new pattern. The counter reaches it by playing
// the row after row 63, which is the first row of the pattern that follows in memory.
#define ROWS_DONE (FT_ROWS)

#define PERIOD_COUNT 36
#define NOTE_BASE 12 // semitones from C-0 of table entry 0 (856 = C-1)

#define SILENCE_WORDS 4

// Room past the file for the sample starts its tables name: the sample count is derived from two table addresses, so it
// is capped, and a start beyond the 68000 24-bit address space is no sample (it would only make the allocation huge).
#define MAX_PADDED_SAMPLES 256
#define ADDRESS_LIMIT 0x1000000u

static const uint16_t k_periods[PERIOD_COUNT] = {
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320,
    302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
};

// What a sample plays after its one pass: the player's 8 byte BSS hunk, all zero.
static const int8_t k_silence[SILENCE_WORDS * 2] = { 0 };

// FtModule reads; the original reads whatever memory follows the module, here it is zero.
static uint8_t module_u8(const FtModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static uint16_t module_u16(const FtModule* m, uint32_t offset) {
    return (uint16_t)(module_u8(m, offset) << 8 | module_u8(m, offset + 1));
}

static uint32_t module_u32(const FtModule* m, uint32_t offset) {
    return (uint32_t)module_u16(m, offset) << 16 | module_u16(m, offset + 2);
}

bool ft_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size < SCAN_SIZE) {
        return false;
    }
    return nuade_read_be32(data) == CHECK_LONG_0 && nuade_read_be32(data + 8) == CHECK_LONG_8
           && nuade_read_be16(data + 12) == CHECK_WORD_C && nuade_read_be32(data + 0x12) == CHECK_LONG_12
           && nuade_read_be32(data + 0x16) == CHECK_LONG_16;
}

// The offset in the module of a table address as the code holds it.
static uint32_t table_offset(const FtModule* m, uint32_t address) {
    return address - m->link_base;
}

// The two scans of InitPlayer. A pattern must be found exactly as often as the original's counter allows: it starts at
// 1, a sample table match subtracts 6, a pattern block match adds 3, an order list match adds 2, and anything but 0 at
// the end is an error.

static bool scan_tables(FtModule* m) {
    uint32_t i = 0;
    while (i < SCAN_SIZE && module_u16(m, i) != OP_MOVE_L_ABS_TO_ABS) {
        i += 2;
    }
    if (i == SCAN_SIZE) {
        return false;
    }
    m->link_base = module_u32(m, i - 4);

    int counter = 1;
    for (i = 0; i < SCAN_SIZE; i += 2) {
        if (module_u16(m, i) == OP_MOVE_L_A1_IND_TO_ABS) {
            m->sample_lengths = table_offset(m, module_u32(m, i + 8));
            m->sample_starts = table_offset(m, module_u32(m, i - 6));
            m->sample_loop_flags = table_offset(m, module_u32(m, i + 0x18));
            m->instruments = table_offset(m, module_u32(m, i - 0x1E));
            counter -= 6;
        } else if (module_u32(m, i) == OP_MULU_W_400_D0) {
            m->patterns = table_offset(m, module_u32(m, i + 6));
            counter += 3;
        } else if (module_u32(m, i) == OP_CMPI_W_400_ABS) {
            m->order_list = table_offset(m, module_u32(m, i + 0xC));
            m->order_count = module_u32(m, i + 0x22);
            counter += 2;
        }
    }
    return counter == 0;
}

// Zero pad the module so that every sample the tables name starts inside `data`; the Paula tail covers its length.

static bool load_module(FtModule* m, const uint8_t* data, size_t size) {
    uint32_t end = (uint32_t)size;
    m->data = nuade_paula_padded_copy(data, size);
    if (m->data == nullptr) {
        return false;
    }
    m->size = (uint32_t)size;
    if (!scan_tables(m) || m->order_count == 0) {
        return false;
    }
    const uint32_t samples = (m->sample_starts - m->sample_lengths) / 4;
    for (uint32_t i = 0; i < samples && i < MAX_PADDED_SAMPLES; i++) {
        const uint32_t start = table_offset(m, module_u32(m, m->sample_starts + i * 4));
        if (start < ADDRESS_LIMIT && start > end) {
            end = start;
        }
    }
    if (end > m->size) {
        uint8_t* padded = nuade_paula_padded_copy(nullptr, end);
        if (padded == nullptr) {
            return false;
        }
        memcpy(padded, m->data, m->size);
        free(m->data);
        m->data = padded;
        m->size = end;
    }
    return true;
}

FtPlayer* ft_create(const uint8_t* data, size_t size) {
    if (!ft_probe(data, size)) {
        return nullptr;
    }
    FtPlayer* p = calloc(1, sizeof(FtPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    if (!load_module(&p->module, data, size)) {
        ft_destroy(p);
        return nullptr;
    }
    return p;
}

void ft_destroy(FtPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module.data);
    free(player);
}

static FtCell cell_at(const FtModule* m, uint32_t pattern, uint32_t voice, uint32_t row) {
    const uint32_t offset = m->patterns + pattern * PATTERN_SIZE + row * ROW_SIZE + voice * 4;
    const uint8_t byte2 = module_u8(m, offset + 2);
    return (FtCell) { .period = module_u16(m, offset),
                      .instrument = (uint8_t)(byte2 >> 4),
                      .effect = byte2 & 0x0F,
                      .param = module_u8(m, offset + 3) };
}

// The sample a table index names; null if it does not start inside the module. Any length can follow it: past the
// module it plays the zeros of the Paula tail.
static const int8_t* sample_at(const FtModule* m, uint32_t address) {
    const uint32_t offset = table_offset(m, address);
    return offset <= m->size ? (const int8_t*)m->data + offset : nullptr;
}

// A note: restart the voice on the instrument's sample. A sample whose loop flag (low byte of its sample table entry)
// is 1 loops whole; any other sample plays once, then the voice loops on 4 words of silence.
static void start_note(const FtModule* m, FtVoice* v, const FtCell* cell) {
    if (cell->instrument != 0) {
        v->instrument = cell->instrument;
    }
    v->started = true;
    const uint32_t slot = m->instruments + (uint32_t)v->instrument * 4;
    const uint32_t sample = (uint32_t)module_u16(m, slot) * 4;
    const uint16_t volume = module_u16(m, slot + 2);
    const uint16_t length = module_u16(m, m->sample_lengths + sample + 2);
    const uint8_t loop_flag = module_u8(m, m->sample_loop_flags + sample + 3);

    v->last_period = cell->period;
    nuade_paula_channel_dma_off(&v->paula);
    v->paula.lc = sample_at(m, module_u32(m, m->sample_starts + sample));
    v->paula.len_words = length;
    v->paula.period = cell->period;
    v->paula.volume = (uint8_t)volume;
    nuade_paula_channel_dma_on(&v->paula);
    if (loop_flag != 1) {
        v->paula.lc = k_silence;
        v->paula.len_words = SILENCE_WORDS;
    }

    v->note = -1;
    for (int i = 0; i < PERIOD_COUNT; i++) {
        if (k_periods[i] == cell->period) {
            v->note = NOTE_BASE + i;
        }
    }
}

static void play_row(FtPlayer* p) {
    const FtModule* m = &p->module;
    FtSong* s = &p->song;

    if (s->row == ROWS_DONE) {
        s->pattern = module_u8(m, m->order_list + s->order);
        s->shown_order = (int)s->order;
        s->order++;
        if (s->order == m->order_count) {
            s->order = 0;
            s->looped = true;
        }
        s->row = 0;
    }
    // The original steps to the next row before playing, so row 0 of a pattern never plays.
    s->row++;
    s->tick = 0;
    s->shown_row = s->row < FT_ROWS ? s->row : FT_ROWS - 1; // row 64 is the next pattern's first row, shown as 63
    for (int i = 0; i < FT_VOICES; i++) {
        const FtCell cell = cell_at(m, s->pattern, (uint32_t)i, s->row);
        if (cell.period != 0) {
            start_note(m, &s->voices[i], &cell);
        }
    }
}

// Arpeggio on ticks 1..5: the period `semitones` table entries above the voice's note; tick 3 restores the note.
// A period that is not in the table (or runs past it) makes the original search into its own variables; no song does.
static void run_arpeggio(FtVoice* v, const FtCell* cell, uint8_t tick) {
    if (cell->effect != 1) {
        return;
    }
    if (tick == 3) {
        v->paula.period = v->last_period;
        return;
    }
    const uint8_t semitones = (tick == 1 || tick == 4) ? cell->param >> 4 : cell->param & 0x0F;
    for (int i = 0; i + semitones < PERIOD_COUNT; i++) {
        if (k_periods[i] == v->last_period) {
            v->paula.period = k_periods[i + semitones];
            return;
        }
    }
}

static void play_tick(FtPlayer* p) {
    FtSong* s = &p->song;
    s->tick++;
    if (s->tick == TICKS_PER_ROW) {
        play_row(p);
        return;
    }
    for (int i = 0; i < FT_VOICES; i++) {
        const FtCell cell = cell_at(&p->module, s->pattern, (uint32_t)i, s->row);
        run_arpeggio(&s->voices[i], &cell, s->tick);
    }
}

// InitSound
bool ft_start(FtPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    FtSong* s = &player->song;
    *s = (FtSong) { 0 };
    s->tick = TICKS_PER_ROW - 1;
    s->row = ROWS_DONE;
    for (int i = 0; i < FT_VOICES; i++) {
        s->voices[i].note = -1;
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

FtTickResult ft_tick(FtPlayer* player, NuadeVoices* voices) {
    FtSong* s = &player->song;
    s->looped = false;
    play_tick(player);
    for (int i = 0; i < FT_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, (uint32_t)i, voices);
    }
    return (FtTickResult) { .looped = s->looped };
}

FtPosition ft_position(const FtPlayer* player) {
    const FtSong* s = &player->song;
    return (FtPosition) { .order = s->shown_order, .row = s->shown_row, .speed = TICKS_PER_ROW };
}

int ft_order_count(const FtPlayer* player) {
    return (int)player->module.order_count;
}

FtVoiceState ft_voice_state(const FtPlayer* player, int voice) {
    const FtVoice* v = &player->song.voices[voice];
    return (FtVoiceState) { .note = v->note, .instrument = v->started ? v->instrument : -1 };
}

FtCell ft_pattern_cell(const FtPlayer* player, int order, int voice, int row) {
    const FtModule* m = &player->module;
    const uint32_t pattern = module_u8(m, m->order_list + (uint32_t)order);
    return cell_at(m, pattern, (uint32_t)voice, (uint32_t)row);
}
