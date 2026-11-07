// SPDX-License-Identifier: MIT
// SynTracker player, translated from the UADE eagleplayer "Syntracker V1.70 Replayer-StkModule" (Wanted Team).
//
// StkModule: "SYNTRACKER-SONG:", the song length and the pattern count - 1 (words), 32 names of 32 bytes, 32 instrument
// records, the order list (per voice 128 pattern numbers, then per voice 128 transposes), the patterns (32 rows of
// one 4 byte cell: note, instrument, effect letter, parameter) and the sample data. A synth instrument (1 .. 16) owns
// a 64 byte block of the sample data: a 32 byte wave and a block of parameters (envelope, filter, vibrato, morph).

#include <nuade/players/syntracker.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

#define MAGIC "SYNTRACKER-SONG:"
#define MAGIC_SIZE 16
#define HEADER_SIZE (MAGIC_SIZE + 2 * 2) // the magic, the song length and the pattern count
#define INSTRUMENT_SIZE 16
#define ORDER_ENTRIES 128 // per voice
#define PATTERN_SIZE (STK_ROWS * 4)
#define WAVE_BLOCK_SIZE 64 // a synth instrument's wave and parameters; the morph and vibrato tables are such blocks

#define MAX_LEVEL 0x3F
#define VIBRATO_SIZE 32

#define NO_JUMP 0xFFFF
#define DEFAULT_SPEED 6
#define INITIAL_PERIOD 1000

// Values of the instrument column of a cell.
#define COLUMN_KEEP 0xFF            // the note changes pitch, the instrument keeps playing
#define COLUMN_LAST_INSTRUMENT 0x20 // 1 .. 0x20 start that instrument
#define COLUMN_SWAP 0x40            // 0x40 .. 0x4F: swap in the synth wave of that instrument
#define COLUMN_SWAP_LAST 0x4F

// The remainder the portamento division starts a row with. See effect_portamento.
#define ROW_START_REMAINDER 0

// The replayer's period table (72 words), followed in its code by the default vibrato table. A note index past the
// period table reads that table as words, so both live in one reader.
#define PERIOD_COUNT 72
static const uint16_t k_periods[PERIOD_COUNT] = {
    6832, 6464, 6096, 5744, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3616, 3416, 3232, 3048, 2872, 2712, 2560,
    2416, 2280, 2152, 2032, 1920, 1808, 1708, 1616, 1524, 1436, 1356, 1280, 1208, 1140, 1076, 1016, 960,  904,
    854,  808,  762,  718,  678,  640,  604,  570,  538,  508,  480,  452,  427,  404,  381,  359,  339,  320,
    302,  285,  269,  254,  240,  226,  214,  202,  190,  180,  169,  160,  151,  143,  134,  127,  120,  113,
};

#define VIBRATO_BYTES 34 // the table is 32 bytes; the code's last two bytes follow it
static const int8_t k_vibrato[VIBRATO_BYTES] = {
    0,   24,  47,  69,  88,  104,  116,  123,  126,  125,  119,  108,  94,  76,  55,  32,   8,
    -15, -39, -61, -81, -98, -112, -121, -126, -126, -122, -113, -100, -83, -63, -41, -100, -83,
};

static const int8_t k_silence[STK_WAVE_SIZE];

// A cursor over the module; past the file it reads zeros.
typedef struct Reader {
    const StkModule* module;
    uint32_t pos;
} Reader;

typedef struct Instrument {
    uint32_t data; // from the start of the sample data
    uint16_t length;
    uint16_t volume;
    uint16_t start;
    uint16_t repeat_start;
    uint16_t repeat_length;
} Instrument;

// The replayer's chip-ram buffers, as waves in a row: a silent one, the waves the synth voices build (one per voice),
// and the copies Paula plays (one per voice). They are one row because the morph writes run on from one voice's work
// wave into the next ones (see update_voices), and a long wave length reads on into the next copies.
#define SLOT_SILENCE 0

static uint8_t read_u8(Reader* r) {
    const StkModule* m = r->module;
    const uint8_t value = r->pos < m->size ? m->data[r->pos] : 0;
    r->pos++;
    return value;
}

static uint16_t read_u16(Reader* r) {
    const uint16_t high = read_u8(r);
    return (uint16_t)(high << 8 | read_u8(r));
}

static uint32_t read_u32(Reader* r) {
    const uint32_t high = read_u16(r);
    return high << 16 | read_u16(r);
}

static Reader reader_at(const StkModule* m, uint32_t pos) {
    return (Reader) { .module = m, .pos = pos };
}

// `bytes` of the module at `offset` as a wave; null when they do not lie in the module and its padding.

static const int8_t* wave_at(const StkModule* m, uint32_t offset, uint32_t bytes) {
    return (uint64_t)offset + bytes <= (uint64_t)m->size + NUADE_PAULA_MAX_SAMPLE_BYTES
               ? (const int8_t*)m->data + offset
               : nullptr;
}

// A 64 byte block of the sample data as a 32 byte table; silence when it is out of range.

static const int8_t* table_block(const StkModule* m, uint8_t block) {
    const int8_t* table = wave_at(m, m->sample_base + (uint32_t)block * WAVE_BLOCK_SIZE, VIBRATO_SIZE);
    return table != nullptr ? table : k_silence;
}

// The instrument record `slot` of the table. The replayer does not bound the slot: a number past the table reads
// whatever the module holds there.
static Instrument instrument_at(const StkModule* m, uint8_t slot) {
    Reader r = reader_at(m, m->instruments_base + (uint32_t)slot * INSTRUMENT_SIZE);
    Instrument ins;
    ins.data = read_u32(&r);
    ins.length = read_u16(&r);
    ins.volume = read_u16(&r);
    ins.start = read_u16(&r);
    ins.repeat_start = read_u16(&r);
    ins.repeat_length = read_u16(&r);
    return ins;
}

static StkSynthParams read_synth_params(Reader r) {
    StkSynthParams p;
    for (int i = 0; i < STK_PHASES; i++) {
        p.envelope[i].duration = read_u8(&r);
        p.envelope[i].amount = read_u8(&r);
    }
    for (int i = 0; i < STK_PHASES; i++) {
        p.filter[i].duration = read_u8(&r);
        p.filter[i].amount = read_u8(&r);
    }
    p.vibrato = read_u8(&r);
    p.morph_speed = read_u8(&r);
    p.vibrato_wave = read_u8(&r);
    read_u8(&r);
    p.morph_start = read_u8(&r);
    p.morph_end = read_u8(&r);
    p.morph_jump = (int8_t)read_u8(&r);
    read_u8(&r);
    p.morph_wave = read_u8(&r);
    return p;
}

// Point the voice at the synth instrument `slot`: its wave and parameters.

static void select_synth(const StkModule* m, StkVoice* v, uint8_t slot) {
    const uint32_t block = instrument_at(m, slot).data + m->sample_base;
    const int8_t* wave = wave_at(m, block, STK_WAVE_SIZE);
    v->base_wave = wave != nullptr ? wave : k_silence;
    v->synth = read_synth_params(reader_at(m, block + STK_WAVE_SIZE));
    v->synth_active = true;
}

// The order list entry of a voice. A position past the voice's 128 entries reads into the next voice's.
typedef struct Track {
    uint8_t pattern;
    uint8_t transpose;
} Track;

static Track track_at(const StkModule* m, int voice, uint32_t position) {
    const uint32_t entry = (uint32_t)voice * ORDER_ENTRIES + position;
    Reader pattern = reader_at(m, m->orders_base + entry);
    Reader transpose = reader_at(m, m->orders_base + STK_VOICES * ORDER_ENTRIES + entry);
    return (Track) { .pattern = read_u8(&pattern), .transpose = read_u8(&transpose) };
}

static StkCell cell_at(const StkModule* m, uint8_t pattern, uint32_t row) {
    Reader r = reader_at(m, m->patterns_base + (uint32_t)pattern * PATTERN_SIZE + row * 4);
    StkCell c;
    c.note = read_u8(&r);
    c.instrument = read_u8(&r);
    c.command = read_u8(&r);
    c.param = read_u8(&r);
    return c;
}

// The word `index` places into the replayer's period table.
static uint16_t period_at(int index) {
    if (index < 0) {
        return 0; // the word before the table is zero
    }
    if (index < PERIOD_COUNT) {
        return k_periods[index];
    }
    const int byte = (index - PERIOD_COUNT) * 2;
    return byte + 1 < VIBRATO_BYTES ? (uint16_t)((uint8_t)k_vibrato[byte] << 8 | (uint8_t)k_vibrato[byte + 1]) : 0;
}

// mulu.w a, b followed by lsr.w #shift: the shift acts on the low word of the product.

static uint16_t scale(uint16_t a, uint16_t b, int shift) {
    return (uint16_t)((uint16_t)((uint32_t)a * b) >> shift);
}

// `level` (0 .. MAX_LEVEL) of the voice's instrument volume.

static uint16_t instrument_level(const StkVoice* v, uint16_t level) {
    return scale(level, v->instrument_volume, 6);
}

// Hand a queued wave to the channel's registers.

static void load_wave(StkWaveLoad* load, NuadePaulaChannel* paula) {
    if (load->len != 0) {
        paula->lc = load->lc;
        paula->len_words = load->len;
        load->len = 0;
    }
}

bool stk_probe(const uint8_t* data, size_t size) {
    return size >= HEADER_SIZE + STK_INSTRUMENTS * STK_NAME_SIZE + STK_INSTRUMENTS * INSTRUMENT_SIZE
                       + 2 * STK_VOICES * ORDER_ENTRIES
           && memcmp(data, MAGIC, MAGIC_SIZE) == 0;
}

StkPlayer* stk_create(const uint8_t* data, size_t size) {
    if (!stk_probe(data, size) || size > UINT32_MAX - NUADE_PAULA_MAX_SAMPLE_BYTES) {
        return nullptr;
    }
    StkPlayer* p = calloc(1, sizeof(StkPlayer));
    uint8_t* copy = nuade_paula_padded_copy(data, size);
    if (p == nullptr || copy == nullptr) {
        free(p);
        free(copy);
        return nullptr;
    }

    StkModule* m = &p->module;
    m->data = copy;
    m->size = (uint32_t)size;
    Reader header = reader_at(m, MAGIC_SIZE);
    m->song_length = read_u16(&header);
    const uint32_t pattern_count = (uint32_t)read_u16(&header) + 1;
    const uint32_t names_base = header.pos;
    m->instruments_base = names_base + STK_INSTRUMENTS * STK_NAME_SIZE;
    m->orders_base = m->instruments_base + STK_INSTRUMENTS * INSTRUMENT_SIZE;
    m->patterns_base = m->orders_base + 2 * STK_VOICES * ORDER_ENTRIES;
    m->sample_base = m->patterns_base + pattern_count * PATTERN_SIZE;
    for (int i = 0; i < STK_INSTRUMENTS; i++) {
        memcpy(p->names[i], data + names_base + (size_t)i * STK_NAME_SIZE, STK_NAME_SIZE);
    }
    return p;
}

void stk_destroy(StkPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module.data);
    free(player);
}

// Arpeggio, vibrato and slide: the period register, every tick.
static void update_period(StkVoice* v) {
    v->arpeggio_ticks++;
    if (v->arpeggio_speed == v->arpeggio_ticks) {
        v->arpeggio_ticks = 0;
        v->period = v->periods[v->arpeggio_phase];
        v->arpeggio_phase = (uint16_t)((v->arpeggio_phase + 1) % 3);
    }

    uint16_t period = v->period;
    if (v->vibrato_depth != 0) {
        // The note is looked up from its own table entry on: the vibrato bends towards its neighbours.
        int index = v->note_index;
        while (index < PERIOD_COUNT + VIBRATO_BYTES / 2 && period_at(index) != period) {
            index++;
        }
        const uint8_t value = (uint8_t)v->vibrato_table[v->vibrato_pos];
        if (value & 0x80) {
            const uint16_t amount = scale((uint8_t)-value, v->vibrato_depth, 7);
            const uint16_t lower = period_at(index - 1);
            period = (uint16_t)(period + scale((uint16_t)(lower - period), amount, 5));
        } else {
            const uint16_t amount = scale(value, v->vibrato_depth, 7);
            const uint16_t higher = period_at(index + 1);
            period = (uint16_t)(period - scale((uint16_t)(period - higher), amount, 5));
        }
        v->vibrato_pos = (uint16_t)((v->vibrato_pos + v->vibrato_speed) & (VIBRATO_SIZE - 1));
    }

    v->slide_offset = (uint16_t)(v->slide_offset + v->slide);
    v->paula.period = (uint16_t)(period + v->slide_offset);
}

// The volume the synth voice's envelope gives, advancing it one tick. A sample voice has no envelope.
static uint16_t envelope_volume(StkVoice* v) {
    if (!v->synth_active) {
        return v->volume;
    }
    while (v->envelope_phase != STK_PHASES) {
        const StkPhase phase = v->synth.envelope[v->envelope_phase];
        if (phase.duration == v->envelope_ticks) {
            v->envelope_ticks = 0;
            v->envelope_phase++;
            continue;
        }
        v->envelope_ticks++;
        uint8_t level = v->envelope_level;
        if (v->envelope_phase % 2 == 1) {
            level = (uint8_t)(level - phase.amount);
            if (level & 0x80) {
                level = 0;
            }
        } else {
            level = (uint8_t)(level + phase.amount);
            if ((int8_t)level > MAX_LEVEL) {
                level = MAX_LEVEL;
            }
        }
        v->envelope_level = level;
        break;
    }
    return scale(v->volume, v->envelope_level & MAX_LEVEL, 6);
}

// A low-pass over the working wave, once per tick while a filter phase runs. The running value starts as the unsigned
// last byte and keeps its 16 bits between samples.
static void filter_wave(StkVoice* v, int8_t* wave) {
    while (v->synth_active && v->filter_phase != STK_PHASES) {
        const StkPhase phase = v->synth.filter[v->filter_phase];
        if (phase.duration == v->filter_ticks) {
            v->filter_ticks = 0;
            v->filter_phase++;
            continue;
        }
        v->filter_ticks++;
        uint16_t running = (uint8_t)wave[STK_WAVE_SIZE - 1];
        for (int i = 0; i < STK_WAVE_SIZE; i++) {
            const int16_t sample = wave[i];
            const int32_t product = (int16_t)(running - sample) * phase.amount;
            const int16_t step = (int16_t)(product & 0xFFFF) >> 7;
            running = (uint16_t)(step + sample);
            wave[i] = (int8_t)running;
        }
        return;
    }
}

// The working wave: the instrument's wave, with a second wave mixed over it where that one is not negative; the second
// one's window sweeps back and forth. Returns whether it wrote the wave: the replayer's output pointer advances with
// it.
static bool morph_wave(const StkModule* m, StkVoice* v, int8_t* wave) {
    if (!v->synth_active) {
        return false;
    }
    if (v->morph_speed == 0) {
        if (v->new_note) {
            v->new_note = false;
            memcpy(wave, v->base_wave, STK_WAVE_SIZE);
            return true;
        }
        return false;
    }
    v->new_note = false;
    v->morph_ticks++;
    if ((int8_t)v->morph_speed >= (int8_t)v->morph_ticks) {
        return false;
    }
    v->morph_ticks = 0;
    v->morph_pos = (uint8_t)((v->morph_pos + v->morph_step) & (STK_WAVE_SIZE - 1));
    if (v->synth.morph_start == v->morph_pos && v->synth.morph_jump < 0) {
        v->morph_step = 1;
    } else if (v->synth.morph_end == v->morph_pos) {
        if (v->synth.morph_jump < 0) {
            v->morph_step = -1;
        } else {
            v->morph_pos = (uint8_t)((v->morph_pos + v->synth.morph_jump) & (STK_WAVE_SIZE - 1));
        }
    }

    const int8_t* over = table_block(m, v->synth.morph_wave);
    for (int i = 0; i < STK_WAVE_SIZE; i++) {
        const int8_t top = over[(v->morph_pos + i) & (STK_WAVE_SIZE - 1)];
        wave[i] = top < 0 ? v->base_wave[i] : top;
    }
    return true;
}

// The volume register of a voice: the decay, then the envelope.
static void update_volume(StkVoice* v) {
    int16_t decayed = (int16_t)(v->volume - v->decay);
    v->volume = decayed < 0 ? 0 : (uint16_t)decayed;
    v->paula.volume = (uint8_t)envelope_volume(v);
}

// Start the instrument on a voice: DMA off, the new wave pointers queued for the end of the tick.
static void start_instrument(StkPlayer* p, StkVoice* v, int voice, uint8_t number) {
    const StkModule* m = &p->module;
    StkChipRam* ram = &p->song.ram;
    const uint8_t slot = (uint8_t)(number - 1);
    const Instrument ins = instrument_at(m, slot);
    nuade_paula_channel_dma_off(&v->paula);
    if (slot < STK_SYNTH_INSTRUMENTS) {
        // A length past the end of the waves would read outside the player: the replayer's own memory ends there too.
        const uint16_t words_left = (uint16_t)((STK_SLOT_COUNT - STK_SLOT_PLAY - voice) * STK_WAVE_SIZE / 2);
        v->start = (StkWaveLoad) { .lc = ram->slots[STK_SLOT_PLAY + voice],
                                   .len = ins.length < words_left ? ins.length : words_left };
        v->new_note = true;
        v->instrument_volume = ins.volume;
        v->volume = instrument_level(v, MAX_LEVEL);
        select_synth(m, v, slot);
        v->vibrato_speed = v->synth.vibrato >> 4;
        v->vibrato_depth = v->synth.vibrato & 0xF;
        v->vibrato_table = table_block(m, v->synth.vibrato_wave);
        v->morph_step = 1;
        v->morph_pos = v->synth.morph_start;
        v->morph_speed = v->synth.morph_speed;
        v->morph_ticks = v->morph_speed;
        if (v->morph_ticks != 0) {
            v->morph_ticks--;
        }
        v->envelope_phase = 0;
        v->filter_phase = 0;
    } else {
        if (ins.length == 0) {
            v->start = (StkWaveLoad) { .lc = ram->slots[SLOT_SILENCE], .len = 1 };
            v->paula.volume = 0;
        } else {
            v->start
                = (StkWaveLoad) { .lc = wave_at(m, ins.data + m->sample_base + ins.start, (uint32_t)ins.length * 2),
                                  .len = ins.length };
            v->instrument_volume = ins.volume;
            v->volume = instrument_level(v, MAX_LEVEL);
            if (ins.repeat_length == 0) {
                v->repeat = (StkWaveLoad) { .lc = ram->slots[SLOT_SILENCE], .len = 1 };
            } else {
                v->repeat = (StkWaveLoad) { .lc = wave_at(m, ins.data + m->sample_base + ins.repeat_start,
                                                          (uint32_t)ins.repeat_length * 2),
                                            .len = ins.repeat_length };
            }
        }
        v->synth_active = false;
        v->vibrato_depth = 0;
        v->vibrato_speed = 0;
        v->envelope_phase = STK_PHASES;
        v->filter_phase = STK_PHASES;
        v->morph_speed = 0;
    }
    v->decay = 0;
    v->filter_ticks = 0;
    v->envelope_ticks = 0;
    v->envelope_level = 0;
}

// The portamento effect: a slide that reaches the next note of the voice in this pattern when its row starts. The
// original divides a 32 bit dividend by 16 bits (divu.w) in a register whose upper word it never clears. When the note
// slides down that upper word is part of the dividend: it is the remainder of the division an earlier voice of the same
// row left there (ROW_START_REMAINDER before the first one), kept in StkSong.portamento_remainder.
static void effect_portamento(StkPlayer* p, StkVoice* v, uint8_t pattern, StkCell cell, uint8_t transpose) {
    const StkModule* m = &p->module;
    StkSong* s = &p->song;
    if (cell.note == 0) {
        return;
    }
    uint32_t next_row = s->row + 1;
    StkCell next = { 0 };
    for (; next_row < STK_ROWS; next_row++) {
        next = cell_at(m, pattern, next_row);
        if (next.note != 0) {
            break;
        }
    }
    if (next_row == STK_ROWS) {
        return;
    }

    const uint16_t target = period_at((uint8_t)(next.note + transpose) - 1);
    const uint16_t divisor = (uint16_t)((next_row - s->row) * s->speed);
    if (divisor == 0) {
        return;
    }
    const uint16_t from = v->periods[0];
    uint32_t dividend;
    bool down;
    if ((int16_t)from > (int16_t)target) {
        dividend = (uint32_t)s->portamento_remainder << 16 | (uint16_t)(from - target);
        down = true;
    } else {
        dividend = (uint16_t)(target - from);
        down = false;
    }
    uint32_t result = dividend; // divu leaves the dividend alone when the quotient does not fit
    if (dividend / divisor <= 0xFFFF) {
        result = (dividend % divisor) << 16 | (dividend / divisor);
    }
    uint16_t step = (uint16_t)result;
    if (result > 0xFFFF) {
        step++;
    }
    if (down) {
        s->portamento_remainder = (uint16_t)(result >> 16);
    }
    v->slide = down ? (int16_t)-(int16_t)step : (int16_t)step;
}

// The effect of a cell, for the voice, after its note and instrument.
static void apply_effect(StkPlayer* p, StkVoice* v, uint8_t pattern, StkCell cell, uint8_t transpose) {
    const StkModule* m = &p->module;
    StkSong* s = &p->song;
    const uint8_t param = cell.param;
    switch (cell.command) {
        case 'A': // arpeggio: two semitone offsets, one per nibble
            if (v->note_index != 0) {
                v->periods[1] = period_at(v->note_index + (param & 0xF));
                v->periods[2] = period_at(v->note_index + (param >> 4));
            }
            break;
        case 'L': // volume
            v->volume = instrument_level(v, param);
            break;
        case 'M': // morph speed
            v->morph_speed = param;
            break;
        case 'V': // vibrato, with the built-in table
            v->vibrato_speed = param >> 4;
            v->vibrato_depth = param & 0xF;
            v->vibrato_table = k_vibrato;
            break;
        case 'F': // the LED filter
            break;
        case 'S': // speed
            if (param != 0) {
                s->speed = param;
            }
            break;
        case 'O': // arpeggio speed
            if (param != 0) {
                v->arpeggio_speed = param;
                v->arpeggio_ticks = 0;
            }
            break;
        case 'B': // jump to a position; 0: the next one
            if (param == 0) {
                s->jump = (uint16_t)(s->position + 1);
                if (s->jump >= m->song_length) {
                    s->jump = 0;
                }
            } else {
                s->jump = param;
                if (s->jump >= m->song_length) {
                    s->jump = NO_JUMP;
                }
            }
            break;
        case 'U': // slide up one step, as long as the period stays above 0x39
            if ((int16_t)(v->periods[2] - param) >= 0x39) {
                v->slide_offset = (uint16_t)(v->slide_offset - param);
            }
            break;
        case 'D': // slide down one step, down to the lowest note of the table
            if ((int16_t)(v->periods[0] + param) <= k_periods[0]) {
                v->slide_offset = (uint16_t)(v->slide_offset + param);
            }
            break;
        case 'G': // volume decay per tick; an empty instrument column restarts the volume
            if (cell.instrument == 0) {
                v->volume = instrument_level(v, MAX_LEVEL);
            }
            v->decay = instrument_level(v, param);
            break;
        case 'E': // portamento to the next note
            effect_portamento(p, v, pattern, cell, transpose);
            break;
        default:
            break;
    }
}

// Decode the row's cell of one voice.
static void play_voice_row(StkPlayer* p, int voice) {
    const StkModule* m = &p->module;
    StkSong* s = &p->song;
    StkVoice* v = &s->voices[voice];
    const Track track = track_at(m, voice, s->position);
    const StkCell cell = cell_at(m, track.pattern, s->row);
    const int8_t column = (int8_t)cell.instrument;

    if (column >= COLUMN_SWAP) {
        // 0x40 .. 0x4F: swap in another synth instrument's parameters and wave without a new note; the note column is
        // ignored. Above that nothing but the effect.
        if (column <= COLUMN_SWAP_LAST) {
            select_synth(m, v, (uint8_t)(column - COLUMN_SWAP));
            v->new_note = true;
            v->start.lc = s->ram.slots[STK_SLOT_PLAY + voice];
        }
        apply_effect(p, v, track.pattern, cell, track.transpose);
        return;
    }
    if (cell.note != 0) {
        int8_t index = (int8_t)(cell.note + track.transpose - 1);
        if (index < 0) {
            index = 0;
        }
        v->note_index = (uint16_t)index;
        v->period = period_at(index);
        v->periods[0] = v->periods[1] = v->periods[2] = v->period;
        v->slide = 0;
        v->slide_offset = 0;
        v->shown_note = index;
        bool start = false;
        if (cell.instrument == 0) {
            start = true;
        } else if (cell.instrument != COLUMN_KEEP && column <= COLUMN_LAST_INSTRUMENT) {
            // A new instrument number: the vibrato restarts with the built-in table.
            v->vibrato_table = k_vibrato;
            v->vibrato_pos = 0;
            v->vibrato_depth = 0;
            v->instrument = cell.instrument;
            start = true;
        }
        if (start) {
            v->shown_instrument = v->instrument;
            start_instrument(p, v, voice, v->instrument);
        }
    }
    apply_effect(p, v, track.pattern, cell, track.transpose);
}

// One row: every voice decodes its cell, the row counter advances.
static void play_row(StkPlayer* p) {
    StkSong* s = &p->song;
    const StkModule* m = &p->module;
    if (s->jump != NO_JUMP) {
        s->position = s->jump;
        s->row = 0;
        s->jump = NO_JUMP;
    }
    s->shown_position = s->position;
    s->shown_row = s->row;
    s->portamento_remainder = ROW_START_REMAINDER;
    for (int i = 0; i < STK_VOICES; i++) {
        play_voice_row(p, i);
    }
    s->row++;
    if (s->row == STK_ROWS) {
        s->row = 0;
        s->position++;
        if (s->position == m->song_length) {
            s->position = 0;
            s->looped = true;
        }
    }
}

// The part of the interrupt that runs every tick: the registers, the working waves, the DMA start.
static void update_voices(StkPlayer* p) {
    const StkModule* m = &p->module;
    StkSong* s = &p->song;
    for (int i = 0; i < STK_VOICES; i++) {
        update_period(&s->voices[i]);
    }
    for (int i = 0; i < STK_VOICES; i++) {
        update_volume(&s->voices[i]);
    }
    for (int i = 0; i < STK_VOICES; i++) {
        filter_wave(&s->voices[i], s->ram.slots[STK_SLOT_WORK + i]);
    }
    // The replayer steps the output pointer one wave on after each voice, and the morph itself leaves it one wave on
    // when it wrote: after a voice that morphed, the next voice's wave lands one slot further (it can run into the
    // copies Paula plays, which the copy below overwrites).
    int slot = STK_SLOT_WORK;
    for (int i = 0; i < STK_VOICES; i++) {
        slot += morph_wave(m, &s->voices[i], s->ram.slots[slot]) ? 2 : 1;
    }
    for (int i = 0; i < STK_VOICES; i++) {
        load_wave(&s->voices[i].start, &s->voices[i].paula);
    }
    for (int i = 0; i < STK_VOICES; i++) {
        nuade_paula_channel_dma_on(&s->voices[i].paula);
    }
    // The waves Paula plays are copied after the DMA start; the repeat registers follow.
    memcpy(s->ram.slots[STK_SLOT_PLAY], s->ram.slots[STK_SLOT_WORK], STK_VOICES * STK_WAVE_SIZE);
    for (int i = 0; i < STK_VOICES; i++) {
        load_wave(&s->voices[i].repeat, &s->voices[i].paula);
    }
}

// InitSound: the voices play the silent wave at volume 0 until the first row.
void stk_start(StkPlayer* player) {
    StkSong* s = &player->song;
    *s = (StkSong) { .speed = DEFAULT_SPEED, .jump = 0 };
    for (int i = 0; i < STK_VOICES; i++) {
        StkVoice* v = &s->voices[i];
        v->arpeggio_speed = 1;
        v->shown_note = -1;
        v->vibrato_table = k_silence;
        v->base_wave = k_silence;
        v->paula.lc = s->ram.slots[SLOT_SILENCE];
        v->paula.len_words = STK_WAVE_SIZE / 2;
        v->paula.period = INITIAL_PERIOD;
        v->paula.volume = 0;
        nuade_paula_channel_dma_on(&v->paula);
    }
}

// Interrupt
StkTickResult stk_tick(StkPlayer* player, NuadeVoices* voices) {
    StkSong* s = &player->song;
    s->looped = false;
    s->tick++;
    if (s->tick == s->speed) {
        s->tick = 0;
        play_row(player);
    }
    update_voices(player);
    for (int i = 0; i < STK_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, (uint32_t)i, voices);
    }
    return (StkTickResult) { .looped = s->looped };
}

const char* stk_instrument_name(const StkPlayer* player, int instrument) {
    return instrument >= 0 && instrument < STK_INSTRUMENTS ? player->names[instrument] : "";
}

StkPosition stk_position(const StkPlayer* player) {
    const StkSong* s = &player->song;
    return (StkPosition) { .order = s->shown_position, .row = s->shown_row, .speed = s->speed };
}

int stk_order_count(const StkPlayer* player) {
    return player->module.song_length;
}

StkVoiceState stk_voice_state(const StkPlayer* player, int voice) {
    const StkVoice* v = &player->song.voices[voice];
    return (StkVoiceState) { .note = v->shown_note, .instrument = v->shown_instrument };
}

StkCell stk_pattern_cell(const StkPlayer* player, int order, int voice, int row) {
    const StkModule* m = &player->module;
    return cell_at(m, track_at(m, voice, (uint32_t)order).pattern, (uint32_t)row);
}
