// SPDX-License-Identifier: MIT
// Laxity (Powertracker) player, translated from the UADE eagleplayer "Laxity-Player" (Eagleplayer V2.1, 1997).
//
// LxModule: an 8-byte header (pattern area offset, instrument block size, positions per voice, loop position), 24-byte
// instrument records (8 bytes of parameters, a 16-byte name; record 0 is the header and the title), the pattern size,
// four position lists of one byte per position, the patterns (4-byte cells) and the samples. An instrument is a sample
// or a synthesised pulse wave whose width sweeps between two limits; the waves live in 4 x 32 bytes of the module's
// memory at offset 0x1166.

#include <nuade/players/laxity.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE 8
#define RECORD_SIZE 0x18
#define CELL_SIZE 4
#define MAX_NOTE 63 // the cell holds a 6-bit note

// Records: the parameters at the start, a flag byte (nonzero: synth instrument) and a 16-byte name.
#define RECORD_PARAMS_SIZE 8
#define RECORD_FLAG_OFFSET 7
#define RECORD_NAME_OFFSET 8

// Check2: the flag bytes of instruments 1 and 2 are 0xFF and the title (the name of record 0) is the default one, or
// the first record bytes are those of one known module.
#define MARKER_VALUE 0xFF
#define KNOWN_HEADER_LONG 0x00820048u
#define KNOWN_RECORD2_LONG0 0x00010008u
#define KNOWN_RECORD2_LONG1 0x001800FFu

// The largest module: UADE loads it at 0x11000 and the replayer's own synth records write at 0x57D3E, so only a module
// longer than 0x57D3E - 0x11000 is touched by them.
#define MAX_MODULE_SIZE 0x46D3E

// Position list entries: patterns below, then these.
// Volume column bytes: below VOLUME_SLIDE_UP the voice volume, then the slides by the low nibble, then these.
#define VOLUME_SLIDE_UP 0x80
#define VOLUME_SLIDE_DOWN 0x90
#define VOLUME_BREAK 0xFE

#define ORDER_SKIP 0xFE    // the voice keeps playing
#define ORDER_SILENCE 0xFF // the voice's volume goes to 0

// The synthesised waves: 32 bytes per voice in the module's memory, right behind where the replayer expects the
// module's samples to end.
#define SYNTH_BUFFER_OFFSET 0x1166
#define SYNTH_BUFFER_SIZE 32
#define SYNTH_WAVE_WORDS 16
#define SYNTH_MAX_WIDTH 32 // the widest pulse the buffer holds, with a step of margin
#define SYNTH_BUFFERS_SIZE (LX_VOICES * SYNTH_BUFFER_SIZE)

#define DEFAULT_SPEED 6
#define FIRST_TICK_COUNTER 2
#define MIN_SLIDE_PERIOD 0x71
#define MAX_VOLUME 0x40
// The tone portamento's target period: the replayer never stores one, so it is the initial 0.
#define PORTA_TARGET 0
// The arpeggio step table covers the tick counters up to the speed limit below; a higher speed makes the replayer read
// relocated addresses.
#define MAX_SPEED 197

// Where a pointer outside the module memory points: chip memory that reads as zeros.
#define OFFSET_ZERO_WORD (-1)

static const char k_default_title[LX_NAME_SIZE + 1] = "YET ANOTHER COOL";

// The replayer indexes two tables with note * 2 from adjacent bases: the scale of the instrument's finetune (note 1 is
// the table's first word) and the period table, 60 words further on. A note past the first table reads the second, an
// arpeggio past the period table reads the replayer's variables.
#define SCALE_COUNT 60
#define PERIOD_BASE 60
#define PERIOD_COUNT 63

static const uint16_t k_finetune_scale[SCALE_COUNT] = {
    24, 24, 20, 20, 20, 18, 16, 16, 16, 14, 14, 12, 12, 12, 10, 10, 10, 9, 8, 8, 8, 7, 7, 6, 6, 6, 5, 5, 5,
    4,  4,  4,  4,  3,  3,  3,  3,  3,  2,  2,  2,  2,  2,  2,  2,  1,  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

// The last three entries are the data that follows the table.
static const uint16_t k_periods[PERIOD_COUNT] = {
    3424, 3232, 3040, 2880, 2720, 2560, 2416, 2288, 2160, 2032, 1920, 1808, 1712, 1616, 1520, 1440,
    1360, 1280, 1208, 1144, 1080, 1016, 960,  904,  856,  808,  760,  720,  680,  640,  604,  572,
    540,  508,  480,  452,  428,  404,  380,  360,  340,  320,  302,  286,  270,  254,  240,  226,
    214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,  252,  3648, 0,
};

// Vibrato: the first 8 entries of a 32-entry sine (the position's low 5 bits are divided by 4).
static const uint8_t k_vibrato_sine[8] = { 0, 24, 49, 74, 97, 120, 141, 161 };

// Which arpeggio step a tick plays, by the ticks left in the row: 0 the note, 1 the high nibble, 2 the low nibble.
// From counter 32 on these are the bytes of the replayer's code that follow its 32-byte table.
static const uint8_t k_arpeggio_step[MAX_SPEED] = {
    0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2,
    2, 0, 2, 2, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 0, 2, 2, 2, 2, 2,
    0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 2, 2, 0, 2, 2, 0, 2, 2, 2, 2, 2, 2, 0, 2, 2, 0, 2, 2, 0, 2, 2, 2, 2,
    2, 0, 2, 2, 2, 2, 2, 0, 2, 2, 0, 2, 2, 2, 0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 2, 2, 0, 2, 2, 0, 2, 2, 2,
    2, 2, 2, 0, 2, 2, 0, 2, 2, 2, 2, 2, 2, 2, 0, 2, 2, 2, 0, 2, 0, 2, 2, 2, 2, 2, 0, 2, 0, 2, 2, 2, 2,
    2, 0, 2, 2, 1, 0, 2, 2, 2, 0, 2, 2, 2, 2, 2, 0, 2, 2, 0, 2, 2, 0, 2, 2, 0, 2, 2, 2, 2, 0, 1, 0,
};

// Word `index` of the replayer's table area: word 0 is the code before the scale table, words 1..60 the scale table
// and 61..123 the period table with the three words that follow it. 0 past the area.
static uint16_t table_word(unsigned index) {
    if (index >= 1 && index <= SCALE_COUNT) {
        return k_finetune_scale[index - 1];
    }
    const unsigned period = index - SCALE_COUNT - 1;
    return index > SCALE_COUNT && period < PERIOD_COUNT ? k_periods[period] : 0;
}

static uint16_t finetune_scale(unsigned note) {
    return table_word(note);
}

static uint16_t note_period(unsigned note) {
    return table_word(PERIOD_BASE + note);
}

// A read-only view of module memory: bytes outside it read as zero.
typedef struct Memory {
    const uint8_t* bytes;
    uint32_t size;
} Memory;

static uint8_t memory_u8(Memory m, int64_t offset) {
    return offset >= 0 && offset < (int64_t)m.size ? m.bytes[offset] : 0;
}

static uint16_t memory_u16(Memory m, int64_t offset) {
    return (uint16_t)((uint16_t)memory_u8(m, offset) << 8 | memory_u8(m, offset + 1));
}

static uint32_t memory_u32(Memory m, int64_t offset) {
    return (uint32_t)memory_u16(m, offset) << 16 | memory_u16(m, offset + 2);
}

// Record n sits at n * 0x18; record 0 is the header and the title.
static int64_t record_offset(int n) {
    return (int64_t)n * RECORD_SIZE;
}

// The replayer indexes with word registers, which sign-extend.
static int32_t sign_extend16(uint16_t value) {
    return (int16_t)value;
}

typedef struct Instrument {
    uint16_t length;      // words, a sample's first pass
    uint16_t loop_start;  // words
    uint16_t loop_length; // words, 0 = no loop
    int8_t finetune;
    bool synth;
    uint8_t synth_rate;     // ticks per width step
    int8_t synth_width_min; // the sweep's limits
    int8_t synth_width_max;
} Instrument;

// The record at `offset` (instrument n sits at n * 0x18; the finetune of a note without an instrument is read from
// wherever the replayer's stale index points).
static Instrument read_instrument(Memory m, int64_t offset) {
    return (Instrument) { .length = memory_u16(m, offset),
                          .loop_start = memory_u16(m, offset + 2),
                          .loop_length = memory_u16(m, offset + 4),
                          .finetune = (int8_t)memory_u8(m, offset + 6),
                          .synth = memory_u8(m, offset + 7) != 0,
                          .synth_rate = memory_u8(m, offset + 1),
                          .synth_width_min = (int8_t)memory_u8(m, offset + 3),
                          .synth_width_max = (int8_t)memory_u8(m, offset + 5) };
}

static void decode_volume_column(LxCell* cell, uint8_t byte) {
    cell->volume_byte = byte;
    cell->volume_kind = LxVolumeColumn_None;
    cell->volume_amount = byte & 0x0F;
    if (byte < VOLUME_SLIDE_UP) {
        cell->volume_kind = LxVolumeColumn_Set;
        cell->volume_amount = byte;
    } else if (byte == VOLUME_BREAK) {
        cell->volume_kind = LxVolumeColumn_PatternBreak;
    } else if ((byte & 0xF0) == VOLUME_SLIDE_UP) {
        cell->volume_kind = LxVolumeColumn_SlideUp;
    } else if ((byte & 0xF0) == VOLUME_SLIDE_DOWN) {
        cell->volume_kind = LxVolumeColumn_SlideDown;
    }
}

static LxCell decode_cell(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    LxCell cell = {
        .note = b0 & 0x3F, .instrument = (uint8_t)(((b0 >> 6) << 4) | (b2 >> 4)), .effect = b2 & 0x0F, .param = b3
    };
    decode_volume_column(&cell, b1);
    return cell;
}

static LxCell read_cell(Memory m, int64_t offset) {
    return decode_cell(memory_u8(m, offset), memory_u8(m, offset + 1), memory_u8(m, offset + 2),
                       memory_u8(m, offset + 3));
}

static uint16_t pattern_size_offset(const LxHeader* h) {
    return (uint16_t)(HEADER_SIZE + h->instrument_bytes);
}

// The position list byte of `voice` at `position`: the lists are `steps` bytes each, behind the pattern size.
static uint8_t order_entry(Memory m, const LxHeader* h, uint16_t position, int voice) {
    const int64_t base = HEADER_SIZE + 2 + sign_extend16((uint16_t)(h->instrument_bytes + position));
    return memory_u8(m, base + (int64_t)voice * h->steps);
}

// The 16-bit product the replayer forms from a list entry (sign-extended to a word first) and the pattern size.
static uint16_t pattern_product(const LxHeader* h, uint8_t entry) {
    return (uint16_t)((uint16_t)(int16_t)(int8_t)entry * h->pattern_size);
}

static int64_t pattern_offset(const LxHeader* h, uint8_t entry) {
    return h->pattern_base + sign_extend16(pattern_product(h, entry));
}

// InitPlayer/InitSound: the header fields, the pattern size and where each instrument's sample starts. The samples
// follow the patterns: the replayer skips (highest list entry + 1) patterns behind the lists, as a 16-bit offset.
static bool parse_header(Memory m, LxHeader* h) {
    if (m.size < HEADER_SIZE + 2) {
        return false;
    }
    h->pattern_base = memory_u16(m, 0);
    h->instrument_bytes = memory_u16(m, 2);
    h->steps = memory_u16(m, 4);
    h->loop_step = memory_u16(m, 6);
    if (h->pattern_base >= 0x8000 || h->instrument_bytes >= 0x8000 || h->steps == 0 || h->steps >= 0x4000
        || h->loop_step >= h->steps) {
        return false;
    }
    h->instrument_count = (uint16_t)(h->instrument_bytes / RECORD_SIZE);
    h->pattern_size = memory_u16(m, pattern_size_offset(h));
    h->orders_start = (uint32_t)HEADER_SIZE + 2 + h->instrument_bytes;
    h->orders_end = h->orders_start + 4u * h->steps;
    if (h->instrument_count == 0 || h->instrument_count > LX_MAX_INSTRUMENT || h->pattern_size == 0
        || h->pattern_size >= 0x8000 || h->pattern_size % CELL_SIZE != 0 || h->orders_end > m.size) {
        return false;
    }

    uint8_t highest = 0;
    for (uint32_t i = h->orders_start; i < h->orders_end; i++) {
        highest = m.bytes[i] > highest ? m.bytes[i] : highest;
    }
    int64_t sample = h->orders_end + sign_extend16((uint16_t)((highest + 1) * h->pattern_size));
    for (int i = 0; i <= LX_MAX_INSTRUMENT; i++) {
        h->sample_offset[i] = OFFSET_ZERO_WORD;
    }
    for (int i = 1; i <= h->instrument_count; i++) {
        h->sample_offset[i] = sample;
        sample += 2 * (int64_t)memory_u16(m, record_offset(i));
    }
    return true;
}

// The voice records as the replayer's data block holds them at start. The first tick runs the effects of these cells,
// and voices 0-2 start with a synth record sweeping a pulse in the buffer their sample pointer names.
static const struct {
    LxCell cell;
    uint16_t period;
    int8_t volume;
    uint8_t vibrato;
    uint8_t note_index;
    LxSynthWave synth;
} k_initial_voices[LX_VOICES] = {
    { { .note = 0x25, .volume_kind = LxVolumeColumn_Set, .volume_amount = 0x20 },
      0x01AC,
      0x20,
      0xA2,
      0x4A,
      { .active = true, .counter = 1, .rate = 1, .step = 1, .width = 3, .width_min = 0, .width_max = 0x13 } },
    { { .note = 0x08, .volume_kind = LxVolumeColumn_Set, .volume_amount = 0x40 },
      0x08F0,
      0x40,
      0x00,
      0x10,
      { .active = true, .counter = 1, .rate = 1, .step = 1, .width = 3, .width_min = 0, .width_max = 0x13 } },
    { { .param = 0x37 },
      0x011E,
      0x20,
      0x00,
      0x58,
      { .active = true, .counter = 1, .rate = 1, .step = 1, .width = 0x0F, .width_min = 9, .width_max = 0x19 } },
    { { .note = 0x27, .volume_kind = LxVolumeColumn_Set, .volume_amount = 0x20, .effect = 2, .param = 0x3F },
      0x1FF0,
      0x20,
      0xA5,
      0x4E,
      { .active = false } },
};

// The sweep's widths stay inside the 32-byte wave: the replayer's fill counts are 16-bit, so a wider pulse would write
// 64 KB over whatever follows the buffer (the width sweeps between its limits, one step past them at most).
static bool synth_widths_fit(const Instrument* ins) {
    const int low = ins->synth_width_min < ins->synth_width_max ? ins->synth_width_min : ins->synth_width_max;
    const int high = ins->synth_width_min < ins->synth_width_max ? ins->synth_width_max : ins->synth_width_min;
    return low >= 0 && high <= SYNTH_MAX_WIDTH;
}

// The patterns one voice's list names are inside the module and clear of the synth buffers, none of their cells names
// an instrument past the table, an arpeggio cannot index past the period table (the replayer would read its variables
// there: the highest note the voice can have played plus the highest arpeggio nibble stays inside it), and a position
// jump lands inside the lists (past them the replayer reads patterns and whatever follows them as list entries). The
// highest speed set is added to `highest_speed`.
static bool scan_voice(Memory m, const LxHeader* h, int voice, unsigned initial_note, bool has_synth,
                       unsigned* highest_speed) {
    bool seen[256] = { false };
    unsigned highest_note = initial_note;
    unsigned highest_arpeggio = 0;
    for (uint32_t step = 0; step < h->steps; step++) {
        const uint8_t entry = m.bytes[h->orders_start + (uint32_t)voice * h->steps + step];
        if (entry >= ORDER_SKIP || seen[entry]) {
            continue;
        }
        seen[entry] = true;
        const int64_t base = pattern_offset(h, entry);
        if (base < 0 || base + h->pattern_size > m.size) {
            return false;
        }
        if (has_synth && base < SYNTH_BUFFER_OFFSET + SYNTH_BUFFERS_SIZE
            && base + h->pattern_size > SYNTH_BUFFER_OFFSET) {
            return false;
        }
        for (int64_t row = 0; row < h->pattern_size; row += CELL_SIZE) {
            const LxCell cell = read_cell(m, base + row);
            if (cell.instrument > h->instrument_count) {
                return false;
            }
            highest_note = cell.note > highest_note ? cell.note : highest_note;
            switch (cell.effect) {
                case LxEffect_PositionJump:
                    if (cell.param == 0 || cell.param > h->steps || h->steps > 0xFF) {
                        return false;
                    }
                    break;
                case LxEffect_SetSpeed:
                    *highest_speed = cell.param > *highest_speed ? cell.param : *highest_speed;
                    break;
                case LxEffect_Arpeggio: {
                    const unsigned high = cell.param >> 4;
                    const unsigned low = cell.param & 0x0F;
                    highest_arpeggio = high > highest_arpeggio ? high : highest_arpeggio;
                    highest_arpeggio = low > highest_arpeggio ? low : highest_arpeggio;
                    break;
                }
                default:
                    break;
            }
        }
    }
    return highest_note + highest_arpeggio <= MAX_NOTE;
}

static bool validate_patterns(Memory m, const LxHeader* h) {
    bool has_synth = false;
    for (int i = 1; i <= h->instrument_count; i++) {
        const Instrument ins = read_instrument(m, record_offset(i));
        if (ins.synth && !synth_widths_fit(&ins)) {
            return false;
        }
        has_synth |= ins.synth;
    }
    // The waves overwrite the memory at SYNTH_BUFFER_OFFSET, which must not hold the lists or the instruments.
    if (has_synth && h->orders_end > SYNTH_BUFFER_OFFSET) {
        return false;
    }
    unsigned highest_speed = 0;
    for (int voice = 0; voice < LX_VOICES; voice++) {
        if (!scan_voice(m, h, voice, k_initial_voices[voice].note_index / 2U, has_synth, &highest_speed)) {
            return false;
        }
    }
    // The initial arpeggio of voice 2 can still play after a speed above the step table's range.
    return highest_speed <= MAX_SPEED;
}

// Check2.
static bool check_module(Memory file) {
    const int64_t record2 = record_offset(2);
    if (file.size < record2 + RECORD_PARAMS_SIZE
        || memory_u8(file, record_offset(1) + RECORD_FLAG_OFFSET) != MARKER_VALUE
        || memory_u8(file, record2 + RECORD_FLAG_OFFSET) != MARKER_VALUE) {
        return false;
    }
    const bool known_header = memory_u32(file, 0) == KNOWN_HEADER_LONG
                              && memory_u32(file, record2) == KNOWN_RECORD2_LONG0
                              && memory_u32(file, record2 + 4) == KNOWN_RECORD2_LONG1;
    return known_header
           || memcmp(file.bytes + record_offset(0) + RECORD_NAME_OFFSET, k_default_title, LX_NAME_SIZE) == 0;
}

bool lx_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size > MAX_MODULE_SIZE) {
        return false;
    }
    const Memory file = { data, (uint32_t)size };
    if (!check_module(file)) {
        return false;
    }
    LxHeader header;
    return parse_header(file, &header) && validate_patterns(file, &header);
}

static Memory module_memory(const LxModule* m) {
    return (Memory) { m->image, m->image_size };
}

// An AUDxLC value for a module offset: Paula ignores bit 0.
static const int8_t* chip_pointer(const LxModule* m, int64_t offset) {
    offset &= ~(int64_t)1;
    if (offset < 0 || offset >= (int64_t)m->image_size) {
        return (const int8_t*)m->zeros;
    }
    return (const int8_t*)m->image + offset;
}

// Paula register writes (DeliTracker's PokeAdr, PokeLen, PokePer, PokeVol).

static void poke_address(const LxModule* m, LxVoice* v, int64_t offset) {
    v->paula.lc = chip_pointer(m, offset);
}

static void poke_length(LxVoice* v, uint16_t words) {
    v->paula.len_words = words;
}

static void poke_period(LxVoice* v, uint16_t period) {
    v->paula.period = period;
}

static void poke_volume(LxVoice* v, uint8_t volume) {
    v->paula.volume = volume;
}

static void voice_dma_off(LxVoice* v) {
    nuade_paula_channel_dma_off(&v->paula);
}

// Every voice's loop pointer and length: the repeat of the sounds just started.
static void set_loop_registers(LxSong* s, const LxModule* m) {
    for (int i = 0; i < LX_VOICES; i++) {
        LxVoice* v = &s->voices[i];
        poke_address(m, v, v->loop);
        poke_length(v, v->loop_length);
    }
}

// Switches DMA on for the voice and writes the loop registers.
static void voice_dma_on_and_set_loops(LxSong* s, const LxModule* m, LxVoice* started) {
    nuade_paula_channel_dma_on(&started->paula);
    set_loop_registers(s, m);
}

// The width sweep of the synth instruments: writes a 32-byte pulse wave, 0x80 for the first part, `width` zeros, 0x7F
// for the rest. lx_probe keeps the widths up to 32, so the wave fits the buffer.
static void write_wave(LxModule* m, uint32_t offset, uint8_t width) {
    if (width > SYNTH_MAX_WIDTH) {
        return; // not reachable for a module lx_probe accepts
    }
    const unsigned first = 16U - (width >> 1U);
    const unsigned last = first - (width & 1U);
    uint8_t* wave = m->image + offset;
    memset(wave, 0x80, first);
    memset(wave + first, 0x00, width);
    memset(wave + first + width, 0x7F, last);
}

static void synth_update(LxSong* s, LxModule* m) {
    for (int i = 0; i < LX_VOICES; i++) {
        LxSynthWave* w = &s->voices[i].synth;
        if (!w->active) {
            continue;
        }
        w->counter--;
        if (w->counter >= 0) {
            continue;
        }
        w->counter = (int8_t)w->rate;
        if (w->step != 0) {
            w->width = (uint8_t)(w->width + (uint8_t)w->step);
            if ((int8_t)w->width < w->width_min) {
                w->width = (uint8_t)w->width_min;
                w->step = (int8_t)-w->step;
            } else if ((int8_t)w->width > w->width_max) {
                w->width = (uint8_t)w->width_max;
                w->step = (int8_t)-w->step;
            }
        }
        write_wave(m, w->offset, w->width);
    }
}

static void synth_start(LxVoice* v, int voice, const Instrument* ins) {
    v->synth = (LxSynthWave) { .active = true,
                               .offset = SYNTH_BUFFER_OFFSET + SYNTH_BUFFER_SIZE * (uint32_t)voice,
                               .counter = 0,
                               .rate = ins->synth_rate,
                               .step = 1,
                               .width = (uint8_t)ins->synth_width_min,
                               .width_min = ins->synth_width_min,
                               .width_max = ins->synth_width_max };
    v->sample = v->synth.offset;
}

// Sets the voice's sample, loop and lengths from instrument `number` (the replayer's table is by instrument number).
static void start_instrument(LxVoice* v, const LxModule* m, int voice, int number, const Instrument* ins) {
    if (ins->synth) {
        synth_start(v, voice, ins);
        v->loop = v->sample;
        v->length = SYNTH_WAVE_WORDS;
        v->loop_length = SYNTH_WAVE_WORDS;
        return;
    }
    v->synth.active = false; // the replayer's synth record of the voice is switched off
    v->sample = m->header.sample_offset[number];
    v->length = ins->length;
    if (ins->loop_length == 0) {
        v->loop_length = 1;
        v->loop = OFFSET_ZERO_WORD;
    } else {
        v->loop_length = ins->loop_length;
        v->loop = v->sample + 2 * (int64_t)ins->loop_start;
    }
}

static void volume_up(LxVoice* v, uint8_t amount) {
    v->volume = (int8_t)(v->volume + amount);
    if (v->volume > MAX_VOLUME) {
        v->volume = MAX_VOLUME;
    }
    poke_volume(v, (uint8_t)v->volume);
}

static void volume_down(LxVoice* v, uint8_t amount) {
    const int result = v->volume - amount;
    v->volume = result >= 0 ? (int8_t)result : 0;
    poke_volume(v, (uint8_t)v->volume);
}

// The cell's volume column slides and breaks. Runs on every tick.
static void volume_column(LxSong* s, LxVoice* v) {
    switch (v->cell.volume_kind) {
        case LxVolumeColumn_SlideUp:
            volume_up(v, v->cell.volume_amount);
            break;
        case LxVolumeColumn_SlideDown:
            volume_down(v, v->cell.volume_amount);
            break;
        case LxVolumeColumn_PatternBreak:
            s->break_count = 0xFF;
            break;
        default:
            break;
    }
}

// The effects that act on the row's first tick.
static void row_effect(LxSong* s, LxVoice* v) {
    const uint8_t param = v->cell.param;
    switch (v->cell.effect) {
        case LxEffect_TimedVolumeSlide:
            s->slide_counter = 1;
            break;
        case LxEffect_TickMultiplier:
            s->counter = (int16_t)(uint16_t)(s->speed * param);
            break;
        case LxEffect_PositionJump:
            s->position = (uint16_t)((s->position & 0xFF00) | (uint8_t)(param - 1));
            s->break_count = 0xFF;
            break;
        case LxEffect_SetVolume:
            v->vibrato = param > MAX_VOLUME ? MAX_VOLUME : param;
            poke_volume(v, v->vibrato);
            break;
        case LxEffect_PatternBreak:
            s->row_offset = (uint16_t)(param * CELL_SIZE);
            s->break_count++;
            break;
        case LxEffect_SetSpeed:
            s->speed = param;
            break;
        default: // The filter effect is not modelled
            break;
    }
}

// One voice's row: its list entry at `position`, the cell at `row_offset`, then instrument, note and effect.
static void row_voice(LxSong* s, LxModule* m, int index, uint16_t position, uint16_t row_offset) {
    LxVoice* v = &s->voices[index];
    const LxHeader* h = &m->header;
    const Memory mem = module_memory(m);

    const uint8_t entry = order_entry(mem, h, position, index);
    if (entry >= ORDER_SKIP) {
        if (entry == ORDER_SILENCE) {
            poke_volume(v, 0);
            v->synth.active = false;
        }
        return;
    }
    const uint16_t product = pattern_product(h, entry);
    v->cell = read_cell(mem, h->pattern_base + sign_extend16(product) + sign_extend16(row_offset));
    const LxCell cell = v->cell;

    if (cell.volume_kind == LxVolumeColumn_Set) {
        v->volume = (int8_t)cell.volume_amount;
    }
    // A cell without an instrument leaves the replayer's record index at the pattern number scaled by the pattern
    // size (its low byte cleared): a note's finetune is read from there.
    int64_t record = sign_extend16(product & 0xFF00);
    if (cell.instrument != 0) {
        record = record_offset(cell.instrument);
        const Instrument ins = read_instrument(mem, record);
        start_instrument(v, m, index, cell.instrument, &ins);
        v->instrument = cell.instrument;
    }
    poke_volume(v, (uint8_t)v->volume);

    if (cell.note != 0) {
        v->note_index = (uint8_t)(cell.note * 2);
        if (v->length == 0) {
            v->start_dma = true;
        } else {
            const int8_t finetune = read_instrument(mem, record).finetune;
            v->finetune_offset = (int16_t)(finetune * (int)finetune_scale(cell.note));
            const uint16_t period = (uint16_t)(v->finetune_offset + note_period(cell.note));
            if (cell.effect == LxEffect_TonePortamento) {
                if (period == v->period) {
                    return;
                }
                v->porta_down = (int16_t)period < (int16_t)v->period;
            } else {
                v->period = period;
                poke_address(m, v, v->sample);
                poke_length(v, v->length);
                poke_period(v, v->period);
                v->vibrato_pos = 0;
                v->start_dma = true;
                v->note = cell.note;
            }
        }
    }
    volume_column(s, v);
    row_effect(s, v);
}

// Arpeggio period for the note `steps` semitones above the last one, at the same finetune.
static uint16_t arpeggio_period(const LxVoice* v, unsigned semitones) {
    return (uint16_t)(note_period(v->note_index / 2U + semitones) + v->finetune_offset);
}

static void effect_arpeggio(const LxSong* s, LxVoice* v) {
    uint8_t param = v->cell.param;
    if (param == 0) {
        return;
    }
    const unsigned counter = (unsigned)s->counter;
    const unsigned step
        = counter < nuade_sizeof_array(k_arpeggio_step) ? k_arpeggio_step[counter] : 0; // lx_probe: in range
    if (step == 0) {
        poke_period(v, v->period);
        return;
    }
    if (step == 1) {
        param >>= 4;
    }
    poke_period(v, arpeggio_period(v, param & 0x0F));
}

static void effect_slide(LxVoice* v, int direction) {
    v->period = (uint16_t)(v->period + direction * v->cell.param);
    if (direction < 0 && (int16_t)v->period < MIN_SLIDE_PERIOD) {
        v->period = MIN_SLIDE_PERIOD;
    }
    poke_period(v, v->period);
}

// Tone portamento. Only a target below the current period starts it, and since the target is never stored it runs down
// to period 0.
static void effect_tone_portamento(LxVoice* v) {
    if (!v->porta_down) {
        return;
    }
    v->period = (uint16_t)(v->period - v->cell.param);
    if ((int16_t)v->period < PORTA_TARGET) {
        poke_period(v, PORTA_TARGET);
        v->porta_down = false;
        return;
    }
    poke_period(v, v->period);
}

static void effect_vibrato(LxVoice* v) {
    const uint8_t param = v->cell.param;
    if (param != 0) {
        if ((param & 0x0F) == 0) {
            v->vibrato = (uint8_t)((v->vibrato & 0x0F) | param);
        } else if ((param >> 4) == 0) {
            v->vibrato = (uint8_t)((v->vibrato & 0xF0) | param);
        } else {
            v->vibrato = param;
        }
    }
    const unsigned depth = v->vibrato & 0x0F;
    const unsigned delta = (k_vibrato_sine[(v->vibrato_pos & 0x1F) >> 2] * depth) >> 7;
    const bool negative = (v->vibrato_pos & 0x80) != 0;
    poke_period(v, (uint16_t)(negative ? v->period - delta : v->period + delta));
    v->vibrato_pos = (uint8_t)(v->vibrato_pos + (((v->vibrato >> 2) & 0x3C)));
}

// Volume slide by 1 every n ticks (n = a nibble of the parameter), counted in one counter shared by the voices.
static void effect_timed_volume_slide(LxSong* s, LxVoice* v) {
    s->slide_counter--;
    if (s->slide_counter != 0) {
        return;
    }
    const uint8_t up = v->cell.param >> 4;
    if (up != 0) {
        s->slide_counter = up;
        volume_up(v, 1);
    } else {
        s->slide_counter = v->cell.param & 0x0F;
        volume_down(v, 1);
    }
}

// Restarts the sound every `param` ticks.
static void effect_retrigger(LxSong* s, const LxModule* m, LxVoice* v) {
    const unsigned param = v->cell.param;
    if (param == 0 || (unsigned)s->counter % param != 0) {
        return;
    }
    voice_dma_off(v);
    poke_address(m, v, v->sample);
    poke_length(v, v->length);
    voice_dma_on_and_set_loops(s, m, v);
}

static void effect_volume_slide(LxVoice* v) {
    const uint8_t up = v->cell.param >> 4;
    if (up != 0) {
        volume_up(v, up);
    } else {
        volume_down(v, v->cell.param & 0x0F);
    }
}

// The effects of a tick that does not start a row.
static void tick_voice(LxSong* s, const LxModule* m, LxVoice* v) {
    volume_column(s, v);
    switch (v->cell.effect) {
        case LxEffect_Arpeggio:
            effect_arpeggio(s, v);
            break;
        case LxEffect_SlideUp:
            effect_slide(v, -1);
            break;
        case LxEffect_SlideDown:
            effect_slide(v, 1);
            break;
        case LxEffect_TonePortamento:
            effect_tone_portamento(v);
            break;
        case LxEffect_Vibrato:
            effect_vibrato(v);
            break;
        case LxEffect_TimedVolumeSlide:
            effect_timed_volume_slide(s, v);
            break;
        case LxEffect_Retrigger:
            effect_retrigger(s, m, v);
            break;
        default:
            poke_period(v, v->period);
            if (v->cell.effect == LxEffect_VolumeSlide) {
                effect_volume_slide(v);
            }
            break;
    }
}

static void end_pattern(LxSong* s, const LxHeader* h) {
    s->row_offset = 0;
    s->break_count = 0;
    s->position++;
    if (s->position == h->steps) {
        s->looped = true;
        s->position = h->loop_step;
    }
}

static void row_tick(LxSong* s, LxModule* m) {
    // The list and pattern offsets are fixed for the whole row, whatever a jump or break of an earlier voice sets.
    const uint16_t position = s->position;
    const uint16_t row_offset = s->row_offset;
    s->shown_order = position;
    s->shown_row = row_offset / CELL_SIZE;
    for (int i = 0; i < LX_VOICES; i++) {
        row_voice(s, m, i, position, row_offset);
    }
    synth_update(s, m);
    bool any_started = false;
    for (int i = 0; i < LX_VOICES; i++) {
        if (s->voices[i].start_dma) {
            voice_dma_off(&s->voices[i]);
            any_started = true;
        }
    }
    if (any_started) {
        for (int i = 0; i < LX_VOICES; i++) {
            if (s->voices[i].start_dma) {
                nuade_paula_channel_dma_on(&s->voices[i].paula);
                s->voices[i].start_dma = false;
            }
        }
        set_loop_registers(s, m);
    }
    s->counter = (int16_t)s->speed;
    s->row_offset = (uint16_t)(s->row_offset + CELL_SIZE);
    if (m->header.pattern_size <= s->row_offset) {
        end_pattern(s, &m->header);
    }
}

static void play_tick(LxSong* s, LxModule* m) {
    s->counter--;
    if (s->counter > 0) {
        for (int i = 0; i < LX_VOICES; i++) {
            tick_voice(s, m, &s->voices[i]);
        }
        synth_update(s, m);
    } else {
        row_tick(s, m);
    }
    if (s->break_count != 0) {
        end_pattern(s, &m->header);
    }
}

static void trim_name(char* out, Memory m, int64_t offset) {
    for (int i = 0; i < LX_NAME_SIZE; i++) {
        const uint8_t c = memory_u8(m, offset + i);
        out[i] = c >= 0x20 && c < 0x7F ? (char)c : ' ';
    }
    int end = LX_NAME_SIZE;
    while (end > 0 && out[end - 1] == ' ') {
        end--;
    }
    out[end] = '\0';
}

LxPlayer* lx_create(const uint8_t* data, size_t size) {
    if (!lx_probe(data, size)) {
        return nullptr;
    }
    LxPlayer* p = calloc(1, sizeof(LxPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    LxModule* m = &p->module;
    const uint32_t synth_end = SYNTH_BUFFER_OFFSET + SYNTH_BUFFERS_SIZE;
    m->initial_wave_offset = (uint32_t)size > synth_end ? (uint32_t)size : synth_end;
    m->initial_wave_offset = (m->initial_wave_offset + 1U) & ~1U;
    m->image_size = m->initial_wave_offset + SYNTH_BUFFERS_SIZE;
    m->image = nuade_paula_padded_copy(nullptr, m->image_size);
    m->zeros = nuade_paula_padded_copy(nullptr, 2);
    if (m->image == nullptr || m->zeros == nullptr) {
        lx_destroy(p);
        return nullptr;
    }
    memcpy(m->image, data, size);
    const Memory mem = module_memory(m);
    parse_header(mem, &m->header);
    trim_name(m->title, mem, record_offset(0) + RECORD_NAME_OFFSET);
    for (int i = 1; i < m->header.instrument_count; i++) {
        trim_name(m->names[i], mem, record_offset(i) + RECORD_NAME_OFFSET);
    }
    return p;
}

void lx_destroy(LxPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module.image);
    free(player->module.zeros);
    free(player);
}

// InitSound.
bool lx_start(LxPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    LxSong* s = &player->song;
    *s = (LxSong) { 0 };
    s->speed = DEFAULT_SPEED;
    s->counter = FIRST_TICK_COUNTER;
    for (int i = 0; i < LX_VOICES; i++) {
        LxVoice* v = &s->voices[i];
        v->cell = k_initial_voices[i].cell;
        v->synth = k_initial_voices[i].synth;
        v->synth.offset = player->module.initial_wave_offset + SYNTH_BUFFER_SIZE * (uint32_t)i;
        v->sample = v->synth.offset;
        v->loop = v->synth.offset;
        v->length = SYNTH_WAVE_WORDS;
        v->loop_length = SYNTH_WAVE_WORDS;
        v->period = k_initial_voices[i].period;
        v->volume = k_initial_voices[i].volume;
        v->vibrato = k_initial_voices[i].vibrato;
        v->note_index = k_initial_voices[i].note_index;
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

LxTickResult lx_tick(LxPlayer* player, NuadeVoices* voices) {
    LxSong* s = &player->song;
    s->looped = false;
    play_tick(s, &player->module);
    for (int i = 0; i < LX_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, (uint32_t)i, voices);
    }
    return (LxTickResult) { .looped = s->looped };
}

const char* lx_title(const LxPlayer* player) {
    return player->module.title;
}

const char* lx_instrument_name(const LxPlayer* player, int instrument) {
    if (instrument < 1 || instrument > LX_MAX_INSTRUMENT) {
        return "";
    }
    return player->module.names[instrument];
}

int lx_instrument_count(const LxPlayer* player) {
    return player->module.header.instrument_count;
}

LxPosition lx_position(const LxPlayer* player) {
    const LxSong* s = &player->song;
    return (LxPosition) { .order = s->shown_order,
                          .row = s->shown_row,
                          .rows = player->module.header.pattern_size / CELL_SIZE,
                          .speed = s->speed };
}

int lx_order_count(const LxPlayer* player) {
    return player->module.header.steps;
}

LxVoiceState lx_voice_state(const LxPlayer* player, int voice) {
    const LxVoice* v = &player->song.voices[voice];
    return (LxVoiceState) { .note = v->note, .instrument = v->instrument };
}

LxCell lx_pattern_cell(const LxPlayer* player, int order, int voice, int row) {
    const LxModule* m = &player->module;
    const Memory mem = module_memory(m);
    const uint8_t entry = order_entry(mem, &m->header, (uint16_t)order, voice);
    if (entry >= ORDER_SKIP || row < 0 || row >= m->header.pattern_size / CELL_SIZE) {
        return (LxCell) { 0 };
    }
    return read_cell(mem, pattern_offset(&m->header, entry) + (int64_t)row * CELL_SIZE);
}
