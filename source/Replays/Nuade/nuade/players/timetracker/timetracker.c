// SPDX-License-Identifier: MIT
// TimeTracker player, translated from the UADE eagleplayer "TimeTracker player module V1.0" (Wanted Team).
//
// The replayer is a ProTracker clone with a packed module format: the module (TMK.<name>) holds the sample
// table, the position list and the patterns, the samples are a separate file (SMP.<name>), optionally delta coded.
//
// TtModule: "TMK", the subsong count, a table of the sizes of the first (count - 1) subsong blocks, then the blocks.
// A block is the position count, the sample count (bit 7 = delta coded samples, first block only), the sample
// table (8 bytes: length, loop start, loop length, finetune, volume), the position list (even size) and the
// reference table (64 words per pattern: the offset of each row's data from the start of the table), followed by the
// row data: four cells per row, one to three bytes each.

#include <nuade/players/timetracker.h>

#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_paula_channel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_HEADER_SIZE 8

// Samples may run past the file into zeros. AUDxLC can lie up to this many bytes past the start of a sample: the loop
// start and the 9xx offsets each add less than 64 KiB, and EFx moves the loop up to 64 KiB past the offset start. The
// sample copy keeps this much room before its Paula padding.
#define LOOP_START_REACH 0x20000u

// The variables of the replayer's data block that a table read past its end reaches (see table_word).
#define PERIOD_TABLE_BYTES (16 * 36 * 2)
#define VOICE_RECORD_SIZE 0x2E

// ProTracker periods, 16 finetunes of 36 notes (note 1 = C-1). Finetune 8..15 are -8..-1.
static const uint16_t k_periods[16 * 36] = {
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240,
    226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113, 850, 802, 757, 715, 674, 637, 601, 567, 535, 505,
    477, 450, 425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 239, 225, 213, 201, 189, 179, 169, 159, 150, 142, 134,
    126, 119, 113, 844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447, 422, 398, 376, 355, 335, 316, 298, 282,
    266, 251, 237, 224, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118, 112, 838, 791, 746, 704, 665, 628, 592,
    559, 528, 498, 470, 444, 419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235, 222, 209, 198, 187, 176, 166, 157,
    148, 140, 132, 125, 118, 111, 832, 785, 741, 699, 660, 623, 588, 555, 524, 495, 467, 441, 416, 392, 370, 350, 330,
    312, 294, 278, 262, 247, 233, 220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 124, 117, 110, 826, 779, 736, 694,
    655, 619, 584, 551, 520, 491, 463, 437, 413, 390, 368, 347, 328, 309, 292, 276, 260, 245, 232, 219, 206, 195, 184,
    174, 164, 155, 146, 138, 130, 123, 116, 109, 820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460, 434, 410, 387,
    365, 345, 325, 307, 290, 274, 258, 244, 230, 217, 205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115, 109, 814,
    768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431, 407, 384, 363, 342, 323, 305, 288, 272, 256, 242, 228, 216,
    204, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114, 108, 907, 856, 808, 762, 720, 678, 640, 604, 570, 538, 508,
    480, 453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135,
    127, 120, 900, 850, 802, 757, 715, 675, 636, 601, 567, 535, 505, 477, 450, 425, 401, 379, 357, 337, 318, 300, 284,
    268, 253, 238, 225, 212, 200, 189, 179, 169, 159, 150, 142, 134, 126, 119, 894, 844, 796, 752, 709, 670, 632, 597,
    563, 532, 502, 474, 447, 422, 398, 376, 355, 335, 316, 298, 282, 266, 251, 237, 223, 211, 199, 188, 177, 167, 158,
    149, 141, 133, 125, 118, 887, 838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470, 444, 419, 395, 373, 352, 332,
    314, 296, 280, 264, 249, 235, 222, 209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118, 881, 832, 785, 741, 699,
    660, 623, 588, 555, 524, 494, 467, 441, 416, 392, 370, 350, 330, 312, 294, 278, 262, 247, 233, 220, 208, 196, 185,
    175, 165, 156, 147, 139, 131, 123, 117, 875, 826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437, 413, 390,
    368, 347, 328, 309, 292, 276, 260, 245, 232, 219, 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116, 868, 820,
    774, 730, 689, 651, 614, 580, 547, 516, 487, 460, 434, 410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230, 217,
    205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115, 862, 814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457,
    431, 407, 384, 363, 342, 323, 305, 288, 272, 256, 242, 228, 216, 203, 192, 181, 171, 161, 152, 144, 136, 128, 121,
    114,
};

static const uint8_t k_sine[32] = {
    0,   24,  49,  74,  97,  120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
    255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97,  74,  49,  24,
};

// EFx: the funk table, indexed by the speed.
static const uint8_t k_funk[16] = { 0, 5, 6, 7, 8, 10, 11, 13, 16, 19, 22, 26, 32, 43, 64, 128 };

// The volume table is the identity for 0..64. A volume past that (a sample header byte is not clamped) indexes on into
// the data that follows it in the replayer: the finetune offsets (word multiples of 72) and the end of the hunk.
static const uint8_t k_volume_tail[35] = {
    0,  0, 0,   0, 72,  0, 144, 0, 216, 1, 32,  1, 104, 1, 176, 1, 248, 2,
    64, 2, 136, 2, 208, 3, 24,  3, 96,  3, 168, 3, 240, 4, 56,  0, 0,
};

// The 8-byte sample table entry.
typedef struct SampleHeader {
    uint16_t length;      // words
    uint16_t loop_start;  // words, 0 = no loop
    uint16_t loop_length; // words
    uint8_t finetune;
    uint8_t volume; // not limited to 64
} SampleHeader;

static uint8_t mod_byte(const TtModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static uint16_t mod_word(const TtModule* m, uint32_t offset) {
    return (uint16_t)(mod_byte(m, offset) << 8 | mod_byte(m, offset + 1));
}

static SampleHeader sample_header(const TtModule* m, uint32_t table, uint32_t index) {
    const uint32_t at = table + index * SAMPLE_HEADER_SIZE;
    return (SampleHeader) { .length = mod_word(m, at),
                            .loop_start = mod_word(m, at + 2),
                            .loop_length = mod_word(m, at + 4),
                            .finetune = mod_byte(m, at + 6),
                            .volume = mod_byte(m, at + 7) };
}

// A voice's variables as the words the replayer keeps them in.
static uint16_t voice_word(const TtVoice* v, unsigned index, uint32_t offset) {
    switch (offset) {
        case 0x00:
            return v->note_period;
        case 0x02:
            return (uint16_t)(v->cmd << 8 | v->param);
        case 0x08:
            return v->length;
        case 0x0E:
            return v->loop_length;
        case 0x10:
            return v->period;
        case 0x12:
            return (uint16_t)(v->finetune << 8 | v->volume);
        case 0x14:
            return (uint16_t)(1u << index);
        case 0x16:
            return (uint16_t)(v->porta_down << 8 | v->porta_speed);
        case 0x18:
            return v->porta_target;
        case 0x1A:
            return (uint16_t)(v->vibrato << 8 | v->vibrato_pos);
        case 0x1C:
            return (uint16_t)(v->tremolo << 8 | v->tremolo_pos);
        case 0x1E:
            return (uint16_t)(v->waveform << 8 | v->glissando_funk);
        case 0x20:
            return (uint16_t)(v->offset_memory << 8 | v->loop_row * 2);
        case 0x22:
            return (uint16_t)(v->loop_count << 8 | v->funk_acc);
        case 0x28:
            return v->full_length;
        default:
            return 0; // sample and loop pointers (Amiga addresses) and padding
    }
}

// A word of the period tables, by byte offset from the start of the first one. The tables are followed by the
// voice records: a note or an arpeggio on the highest finetune reads them as periods.
static uint16_t table_word(const TtSong* s, uint32_t offset) {
    if (offset < PERIOD_TABLE_BYTES) {
        return k_periods[offset / 2];
    }
    const uint32_t past = offset - PERIOD_TABLE_BYTES;
    const uint32_t voice = past / VOICE_RECORD_SIZE;
    return voice < TT_VOICES ? voice_word(&s->voices[voice], voice, past % VOICE_RECORD_SIZE) : 0;
}

// The byte offset of finetune `finetune`'s table. The replayer indexes its offset table (0, 72, 144, ... for the 16
// finetunes) with the whole finetune byte, which only E5x keeps within a nibble; the data past the table is not
// modelled.
static uint32_t finetune_offset(uint8_t finetune) {
    return finetune < 16 ? (uint32_t)finetune * 72 : 0;
}

static uint8_t volume_table(uint8_t volume) {
    if (volume <= 64) {
        return volume;
    }
    const uint32_t i = (uint32_t)volume - 65;
    return i < sizeof(k_volume_tail) ? k_volume_tail[i] : 0;
}

// The word index of the first entry at or below `period` among `entries` words from byte offset `base`, -1 if none.
static int period_index(const TtSong* s, uint32_t base, uint16_t period, unsigned entries) {
    for (unsigned i = 0; i < entries; i++) {
        if (period >= table_word(s, base + i * 2)) {
            return (int)i;
        }
    }
    return -1;
}

// AUDxVOL from the volume register: through the volume table.
static void write_volume(TtVoice* v, uint8_t volume) {
    v->paula.volume = volume_table(volume);
}

static void set_volume(TtVoice* v, uint8_t volume) {
    v->volume = volume;
    write_volume(v, volume);
}

static void write_period(TtVoice* v, uint16_t period) {
    v->paula.period = period;
}

static void song_end(TtSong* s) {
    s->looped = true;
}

// Pattern cells

typedef struct Cell {
    uint8_t note;   // period table index, 0 none
    uint8_t sample; // 1-based, 0 none
    uint8_t cmd;
    uint8_t param;
} Cell;

// Reads the cell at `offset`; returns its size. A cell is one byte (0x80: empty), two (0xC0 | effect, parameter; or
// 0x80 | note, sample) or three (note << 1 | sample bit 4, sample << 4 | effect, parameter).
static uint32_t read_cell(const TtModule* m, uint32_t offset, Cell* cell) {
    const uint8_t b0 = mod_byte(m, offset);
    *cell = (Cell) { 0 };
    if (b0 & 0x80) {
        if ((b0 & 0x7F) == 0) {
            return 1;
        }
        if (b0 & 0x40) {
            cell->cmd = b0 & 0x3F;
            cell->param = mod_byte(m, offset + 1);
        } else {
            cell->note = b0 & 0x3F;
            cell->sample = mod_byte(m, offset + 1);
        }
        return 2;
    }
    const uint8_t b1 = mod_byte(m, offset + 1);
    cell->note = (b0 >> 1) & 0x3F;
    cell->sample = (uint8_t)((b0 & 1) << 4 | b1 >> 4);
    cell->cmd = b1 & 0x0F;
    cell->param = mod_byte(m, offset + 2);
    return 3;
}

// The offset of the data of `row` of the pattern at `order`.
static uint32_t row_data(const TtModule* m, const TtBlock* b, uint32_t order, uint32_t row) {
    const uint8_t entry = mod_byte(m, b->patterns + order);
    // The replayer takes the position list byte (twice the pattern number) into a table of multiples of 128
    const uint32_t pattern_base = entry < 128 ? (uint32_t)(entry & ~1u) * 64 : 0;
    return b->refs + mod_word(m, b->refs + pattern_base + row * 2);
}

// 124a: EFx moves the loop start through the sample's loop by the loop's length.
static void funk_step(TtVoice* v) {
    const uint8_t speed = v->glissando_funk >> 4;
    if (speed == 0) {
        return;
    }
    v->funk_acc = (uint8_t)(v->funk_acc + k_funk[speed]);
    if (!(v->funk_acc & 0x80)) {
        return;
    }
    v->funk_acc = 0;
    if (v->sample == nullptr || v->funk_pos == nullptr) {
        return;
    }
    // The addresses compare as plain numbers: the end of the sample less one loop can lie before its start
    const int64_t end = (int64_t)(uint16_t)(v->full_length << 1) - ((int64_t)v->loop_length << 1);
    const int8_t* pos = v->funk_pos + ((uint32_t)v->loop_length << 1);
    if ((int64_t)(pos - v->sample) > end) {
        pos = v->loop;
    }
    v->funk_pos = pos;
    v->paula.lc = pos;
}

// ef2: 9xx
static void sample_offset(TtVoice* v) {
    if (v->param != 0) {
        v->offset_memory = v->param;
    }
    const uint16_t offset = (uint16_t)(v->offset_memory << 7);
    if ((int16_t)offset >= (int16_t)v->length) {
        v->length = 1;
        return;
    }
    v->length = (uint16_t)(v->length - offset);
    if (v->sample != nullptr) {
        v->sample += (uint16_t)(offset << 1);
    }
}

static void volume_up(TtVoice* v, uint8_t amount) {
    uint8_t volume = (uint8_t)(v->volume + amount);
    if (((volume - 0x40) & 0x80) == 0) { // cmp.b #$40 / bmi
        volume = 0x40;
    }
    set_volume(v, volume);
}

static void volume_down(TtVoice* v, uint8_t amount) {
    uint8_t volume = (uint8_t)(v->volume - amount);
    if ((int8_t)volume < 0) {
        volume = 0;
    }
    set_volume(v, volume);
}

// f1e: Axy
static void volume_slide(TtVoice* v) {
    write_period(v, v->period);
    const uint8_t up = v->param >> 4;
    if (up != 0) {
        volume_up(v, up);
    } else {
        volume_down(v, v->param & 0x0F);
    }
}

// 1146: restart the sample (E9x, EDx): DMA off, the sample, DMA on, then the repeat. The repeat is written as a long
// over AUDxLEN and AUDxPER, which sets the period too.
static void retrigger(TtVoice* v) {
    nuade_paula_channel_dma_off(&v->paula);
    v->paula.lc = v->sample;
    v->paula.len_words = v->length;
    nuade_paula_channel_dma_on(&v->paula);
    v->paula.lc = v->loop;
    v->paula.len_words = v->loop_length;
    write_period(v, v->period);
}

// c08 / c52: 1xx, 2xx, E1x, E2x
static void porta_up(TtSong* s, TtVoice* v) {
    const uint8_t amount = v->param & s->fine_mask;
    s->fine_mask = 0xFF;
    v->period = (uint16_t)(v->period - amount);
    if ((int16_t)(v->period & 0xFFF) - 0x71 < 0) {
        v->period = (uint16_t)((v->period & 0xF000) | 0x71);
    }
    write_period(v, v->period & 0xFFF);
}

static void porta_down(TtSong* s, TtVoice* v) {
    const uint8_t amount = v->param & s->fine_mask;
    s->fine_mask = 0xFF;
    v->period = (uint16_t)(v->period + amount);
    if ((int16_t)(v->period & 0xFFF) - 0x358 >= 0) {
        v->period = (uint16_t)((v->period & 0xF000) | 0x358);
    }
    write_period(v, v->period & 0xFFF);
}

// c8e: 3xx / 5xx at a row with a note: the note's period (finetune 0) becomes the target
static void tone_porta_setup(const TtSong* s, TtVoice* v) {
    // The replayer steps 74 bytes per finetune here (the tables of ProTracker, which end in a zero word) but its own
    // tables are 72 bytes apart, so the search starts one note off. It scans 37 words and stops on the last note.
    const uint32_t base = (uint32_t)v->finetune * 0x4A;
    int i = period_index(s, base, v->note_period, 37);
    if (i < 0) {
        i = 35;
    }
    if ((v->finetune & 8) && i != 0) {
        i--;
    }
    v->porta_target = table_word(s, base + (uint32_t)i * 2);
    v->porta_down = false;
    if (v->porta_target == v->period) {
        v->porta_target = 0;
    } else if ((int16_t)v->porta_target < (int16_t)v->period) {
        v->porta_down = true;
    }
}

// cfa: one step of the tone portamento (3xx, 5xy)
static void tone_porta_step(const TtSong* s, TtVoice* v) {
    if (v->porta_target == 0) {
        return;
    }
    if (!v->porta_down) {
        v->period = (uint16_t)(v->period + v->porta_speed);
        if (!((int16_t)v->porta_target > (int16_t)v->period)) {
            v->period = v->porta_target;
            v->porta_target = 0;
        }
    } else {
        v->period = (uint16_t)(v->period - v->porta_speed);
        if (!((int16_t)v->porta_target < (int16_t)v->period)) {
            v->period = v->porta_target;
            v->porta_target = 0;
        }
    }
    uint16_t period = v->period;
    if (v->glissando_funk & 0x0F) {
        const uint32_t base = finetune_offset(v->finetune);
        int i = period_index(s, base, period, 36);
        if (i < 0) {
            i = 35;
        }
        period = table_word(s, base + (uint32_t)i * 2);
    }
    write_period(v, period);
}

// b92: 0xy
static void arpeggio(const TtSong* s, TtVoice* v) {
    uint8_t semitones;
    switch (s->tick % 3) {
        case 0:
            write_period(v, v->period);
            return;
        case 2:
            semitones = v->param & 0x0F;
            break;
        default:
            semitones = v->param >> 4;
            break;
    }
    const uint32_t base = finetune_offset(v->finetune);
    const int i = period_index(s, base, v->period, 37);
    if (i >= 0) { // a period below the table: no write
        write_period(v, table_word(s, base + (uint32_t)(i + semitones) * 2));
    }
}

// The vibrato and tremolo shape: 0 sine, 1 ramp down, otherwise square. The ramp takes its sign from the vibrato
// position, also for the tremolo.
static uint8_t wave_amplitude(const TtVoice* v, uint8_t position, uint8_t waveform) {
    const uint8_t index = (position >> 2) & 0x1F;
    if (waveform == 0) {
        return k_sine[index];
    }
    if (waveform == 1) {
        const uint8_t ramp = (uint8_t)(index << 3);
        return (v->vibrato_pos & 0x80) ? (uint8_t)(0xFF - ramp) : ramp;
    }
    return 0xFF;
}

// d84: 4xy; the parameter nibbles that are zero keep the old speed or depth
static void update_wave_parameter(TtVoice* v, uint8_t* wave) {
    if (v->param == 0) {
        return;
    }
    uint8_t value = *wave;
    if (v->param & 0x0F) {
        value = (uint8_t)((value & 0xF0) | (v->param & 0x0F));
    }
    if (v->param & 0xF0) {
        value = (uint8_t)((value & 0x0F) | (v->param & 0xF0));
    }
    *wave = value;
}

// dae
static void vibrato_step(TtVoice* v) {
    const uint8_t amplitude = wave_amplitude(v, v->vibrato_pos, v->waveform & 3);
    const uint16_t delta = (uint16_t)((amplitude * (v->vibrato & 0x0F)) >> 7);
    write_period(v, (v->vibrato_pos & 0x80) ? (uint16_t)(v->period - delta) : (uint16_t)(v->period + delta));
    v->vibrato_pos = (uint8_t)(v->vibrato_pos + ((v->vibrato >> 2) & 0x3C));
}

// e2e: 7xy
static void tremolo_step(TtVoice* v) {
    write_period(v, v->period);
    update_wave_parameter(v, &v->tremolo);
    const uint8_t amplitude = wave_amplitude(v, v->tremolo_pos, (v->waveform >> 4) & 3);
    const uint16_t delta = (uint16_t)((amplitude * (v->tremolo & 0x0F)) >> 6);
    int32_t volume = (v->tremolo_pos & 0x80) ? (int32_t)v->volume - delta : (int32_t)v->volume + delta;
    if (volume < 0) {
        volume = 0;
    }
    if (volume > 0x40) {
        volume = 0x40;
    }
    write_volume(v, (uint8_t)volume);
    v->tremolo_pos = (uint8_t)(v->tremolo_pos + ((v->tremolo >> 2) & 0x3C));
}

// 102c: Exy, on the row and on every tick; most take effect on the first tick only
static void extended_effect(TtSong* s, TtVoice* v) {
    const uint8_t x = v->param & 0x0F;
    switch (v->param >> 4) {
        case 0x0: // E0x: the filter (the LED), no audio in the port
            break;
        case 0x1:
            if (s->tick == 0) {
                s->fine_mask = 0x0F;
                porta_up(s, v);
            }
            break;
        case 0x2:
            if (s->tick == 0) {
                s->fine_mask = 0x0F;
                porta_down(s, v);
            }
            break;
        case 0x3:
        case 0x4: // E4x writes the glissando control too: the vibrato waveform is never set
            v->glissando_funk = (uint8_t)((v->glissando_funk & 0xF0) | x);
            break;
        case 0x5:
            v->finetune = x;
            break;
        case 0x6: // pattern loop
            if (s->tick != 0) {
                break;
            }
            if (x == 0) {
                v->loop_row = s->row;
                break;
            }
            if (v->loop_count == 0) {
                v->loop_count = x;
            } else {
                v->loop_count--;
                if (v->loop_count == 0) {
                    break;
                }
            }
            s->jump_row = v->loop_row;
            s->jump_flag = true;
            break;
        case 0x7:
            v->waveform = (uint8_t)((x << 4) | (v->waveform & 0x0F));
            break;
        case 0x8:
            break;
        case 0x9: // retrigger every x ticks, also on the first tick of a row without a note
            if (x != 0 && !(s->tick == 0 && v->note_period != 0) && s->tick % x == 0) {
                retrigger(v);
            }
            break;
        case 0xA:
            if (s->tick == 0) {
                volume_up(v, x);
            }
            break;
        case 0xB:
            if (s->tick == 0) {
                volume_down(v, x);
            }
            break;
        case 0xC:
            if (x == s->tick) {
                set_volume(v, 0);
            }
            break;
        case 0xD: // note delay
            if (x == s->tick && v->note_period != 0) {
                retrigger(v);
            }
            break;
        case 0xE:
            if (s->tick == 0 && s->delay_count == 0) {
                s->delay_set = (uint8_t)(x + 1);
            }
            break;
        default: // EFx
            if (s->tick != 0) {
                break;
            }
            v->glissando_funk = (uint8_t)((x << 4) | (v->glissando_funk & 0x0F));
            if (x != 0) {
                funk_step(v);
            }
            break;
    }
}

// The effects of the first tick of a row (ff6)
static void row_effect(TtSong* s, TtVoice* v) {
    funk_step(v);
    switch (v->cmd) {
        case 0x9:
            sample_offset(v);
            break;
        case 0xB: // position jump; the replayer tells the host the song ended
            s->position = (uint8_t)(v->param - 1);
            song_end(s);
            s->jump_row = 0;
            s->break_flag = true;
            break;
        case 0xC:
            set_volume(v, v->param > 0x40 ? 0x40 : v->param);
            break;
        case 0xD: {
            const uint8_t row = (uint8_t)((v->param >> 4) * 10 + (v->param & 0x0F));
            s->jump_row = row > 0x3F ? 0 : row;
            s->break_flag = true;
            break;
        }
        case 0xE:
            extended_effect(s, v);
            break;
        case 0xF:
            if (v->param != 0) {
                s->tick = 0;
                s->speed = v->param;
            }
            break;
        default:
            break;
    }
}

// b1e: the effects of the other ticks
static void tick_effect(TtSong* s, TtVoice* v) {
    funk_step(v);
    if (v->cmd == 0 && v->param == 0) {
        write_period(v, v->period);
        return;
    }
    switch (v->cmd) {
        case 0x0:
            arpeggio(s, v);
            break;
        case 0x1:
            porta_up(s, v);
            break;
        case 0x2:
            porta_down(s, v);
            break;
        case 0x3:
            if (v->param != 0) {
                v->porta_speed = v->param;
                v->param = 0;
            }
            tone_porta_step(s, v);
            break;
        case 0x4:
            update_wave_parameter(v, &v->vibrato);
            vibrato_step(v);
            break;
        case 0x5:
            tone_porta_step(s, v);
            volume_slide(v);
            break;
        case 0x6:
            vibrato_step(v);
            volume_slide(v);
            break;
        case 0x7:
            tremolo_step(v);
            break;
        case 0xA:
            volume_slide(v);
            break;
        case 0xE:
            extended_effect(s, v);
            break;
        default:
            write_period(v, v->period);
            break;
    }
}

// 9cc: a sample number selects the sample, its loop, finetune and volume
static void select_sample(const TtModule* m, TtVoice* v, unsigned number) {
    const unsigned index = number - 1;
    const SampleHeader h = sample_header(m, m->sample_table, index);
    v->sample_number = (uint8_t)number;
    v->sample = nullptr;
    if (index < (unsigned)m->sample_count) {
        v->sample = (const int8_t*)m->samples + m->sample_offset[index];
    }
    v->length = h.length;
    v->full_length = h.length;
    v->finetune = h.finetune;
    v->loop_length = h.loop_length;
    if (h.loop_start != 0) {
        v->loop = v->sample != nullptr ? v->sample + (uint16_t)(h.loop_start * 2) : nullptr;
        v->length = (uint16_t)(h.loop_length + h.loop_start);
    } else {
        v->loop = v->sample;
    }
    v->funk_pos = v->loop;
    set_volume(v, h.volume);
}

// The period of table note `note` (1-based) in the table of finetune 0
static uint16_t note_period(const TtSong* s, unsigned note) {
    return note == 0 ? 0 : table_word(s, (note - 1) * 2);
}

// a9a: start the note
static void start_note(TtSong* s, TtVoice* v, unsigned note) {
    v->note = (uint8_t)note;
    v->period = table_word(s, (note - 1) * 2 + finetune_offset(v->finetune));
    if (v->cmd == 0xE && (v->param >> 4) == 0xD) {
        return; // note delay: the retrigger comes with the effect
    }
    nuade_paula_channel_dma_off(&v->paula);
    if (!(v->waveform & 0x04)) {
        v->vibrato_pos = 0;
    }
    if (!(v->waveform & 0x40)) {
        v->tremolo_pos = 0;
    }
    v->paula.lc = v->sample;
    v->paula.len_words = v->length;
    write_period(v, v->period);
    v->started = true;
}

// 958: one cell of the row
static void play_cell(TtSong* s, const TtModule* m, TtVoice* v, const Cell* cell) {
    if (v->note_period == 0 && v->cmd == 0 && v->param == 0) {
        write_period(v, v->period);
    }
    v->cmd = cell->cmd;
    v->param = cell->param;
    if (cell->sample != 0) {
        select_sample(m, v, cell->sample);
    }
    v->note_period = note_period(s, cell->note);
    if (cell->note == 0) {
        row_effect(s, v);
        return;
    }
    if (v->cmd == 0xE && (v->param >> 4) == 0x5) {
        v->finetune = v->param & 0x0F;
    } else if (v->cmd == 3 || v->cmd == 5) {
        tone_porta_setup(s, v);
        row_effect(s, v);
        return;
    } else if (v->cmd == 9) {
        row_effect(s, v); // the offset applies before the start, and again after it
    }
    start_note(s, v, cell->note);
    row_effect(s, v);
}

// 762: after a row, the channels that got a note start their DMA, then every channel's repeat is written (the
// replayer waits for the DMA start in between)
static void start_dma(TtSong* s) {
    for (int i = 0; i < TT_VOICES; i++) {
        if (s->voices[i].started) {
            nuade_paula_channel_dma_on(&s->voices[i].paula);
        }
    }
    for (int i = 0; i < TT_VOICES; i++) {
        TtVoice* v = &s->voices[i];
        v->paula.lc = v->loop;
        v->paula.len_words = v->loop_length;
    }
}

// 89e: decode the row and play it
static void play_row(TtSong* s, const TtModule* m) {
    s->shown_position = s->position;
    s->shown_row = s->row;
    uint32_t offset = row_data(m, &s->block, s->position, s->row);
    for (int i = 0; i < TT_VOICES; i++) {
        Cell cell;
        offset += read_cell(m, offset, &cell);
        s->voices[i].started = false;
        play_cell(s, m, &s->voices[i], &cell);
    }
    start_dma(s);
}

// 92a
static void next_position(TtSong* s) {
    s->row = s->jump_row;
    s->jump_row = 0;
    s->break_flag = false;
    s->position++;
    if (s->position >= s->block.order_count) {
        s->position = 0;
        song_end(s);
    }
}

// 8f4 and 94c: on to the next row
static void advance_row(TtSong* s) {
    s->row++;
    if (s->delay_set != 0) {
        s->delay_count = s->delay_set;
        s->delay_set = 0;
    }
    if (s->delay_count != 0) {
        s->delay_count--;
        if (s->delay_count != 0) {
            s->row--;
        }
    }
    if (s->jump_flag) {
        s->jump_flag = false;
        s->row = s->jump_row;
        s->jump_row = 0;
    }
    if (s->row >= TT_ROWS) {
        next_position(s);
    }
    if (s->break_flag) {
        next_position(s);
    }
}

// 7e4: the interrupt
static void run_interrupt(TtPlayer* p) {
    TtSong* s = &p->song;
    for (int i = 0; i < TT_VOICES; i++) {
        write_volume(&s->voices[i], s->voices[i].volume);
    }
    s->tick++;
    if (s->tick < s->speed) {
        for (int i = 0; i < TT_VOICES; i++) {
            tick_effect(s, &s->voices[i]);
        }
        if (s->break_flag) {
            next_position(s);
        }
        return;
    }
    s->tick = 0;
    if (s->delay_count != 0) {
        for (int i = 0; i < TT_VOICES; i++) {
            tick_effect(s, &s->voices[i]);
        }
    } else {
        play_row(s, &p->module);
    }
    advance_row(s);
}

bool tt_probe(const uint8_t* data, size_t size) {
    return data != nullptr && size >= 4 && data[0] == 'T' && data[1] == 'M' && data[2] == 'K' && data[3] != 0;
}

// The sample file's name for a module file name: "name.pat" -> "name.SAM", "TMK.name" -> "SMP.name"; any other name
// loses its last character.
static void sample_file_name(const char* module_name, char* out, size_t cap) {
    snprintf(out, cap, "%s", module_name);
    const size_t n = strlen(out);
    const char* ext = n >= 4 ? out + n - 4 : out;
    const bool pat
        = n >= 4 && ext[0] == '.' && (ext[1] | 0x20) == 'p' && (ext[2] | 0x20) == 'a' && (ext[3] | 0x20) == 't';
    if (pat) {
        memcpy(out + n - 3, "SAM", 3);
    } else if (n >= 4 && (out[0] | 0x20) == 't' && (out[1] | 0x20) == 'm' && (out[2] | 0x20) == 'k' && out[3] == '.') {
        memcpy(out, "SMP", 3);
    } else if (n > 0) {
        out[n - 1] = '\0';
    }
}

static bool load_sample_file(const NuadeIo* io, const char* path, uint8_t** data, uint64_t* size, char* missing,
                             size_t missing_cap) {
    char name[128];
    char dir[1024] = { 0 };
    sample_file_name(nuade_path_basename(path), name, sizeof(name));
    snprintf(missing, missing_cap, "%s", name);
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        dir[0] = '\0';
    }
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, 1);
    const char* url = index != nullptr ? nuade_file_index_find(index, name) : nullptr;
    NuadeIoReadResult file = { 0 };
    if (url != nullptr) {
        file = NuadeIo_read_url_to_memory(io, url);
    }
    nuade_file_index_destroy(index);
    if (file.data == nullptr) {
        return false;
    }
    *data = file.data;
    *size = file.size;
    return true;
}

// InitPlayer's size check: the sample bytes the table asks for must fit the sample file. It sums the table entries
// by the count in byte 5 of the file (the first block's, for a single subsong). A count of 0 makes the replayer's
// dbra loop run 65536 times, far past the table, which a real module cannot satisfy.
static bool sample_file_fits(const TtModule* m, uint64_t file_size) {
    const unsigned counted = m->data[5] & 0x7F;
    const unsigned entries = counted != 0 ? counted : 65536;
    uint64_t bytes = 0;
    for (unsigned i = 0; i < entries; i++) {
        bytes += (uint64_t)sample_header(m, m->sample_table, i).length * 2;
    }
    return bytes <= file_size;
}

// 634: with the delta flag every sample byte is the sum of itself and all the bytes after it, over all samples. The
// first four bytes of every sample are cleared (the silent loop of a sample without one). The copy has room for the
// file and the table's total (which can exceed the file when byte 5 counts fewer samples than the table), so every
// sample starts inside it, then LOOP_START_REACH.
static bool prepare_samples(TtModule* m, const uint8_t* file, uint64_t file_size) {
    uint32_t total = 0;
    for (int i = 0; i < m->sample_count; i++) {
        m->sample_offset[i] = total;
        total += (uint32_t)sample_header(m, m->sample_table, (uint32_t)i).length * 2;
    }
    if (!sample_file_fits(m, file_size)) {
        return false;
    }
    m->samples = nuade_paula_padded_copy(nullptr, (size_t)file_size + total + LOOP_START_REACH);
    if (m->samples == nullptr) {
        return false;
    }
    memcpy(m->samples, file, (size_t)file_size);
    if (m->delta && total > 0) {
        for (uint32_t i = total - 1; i-- > 0;) {
            m->samples[i] = (uint8_t)(m->samples[i] + m->samples[i + 1]);
        }
    }
    for (int i = 0; i < m->sample_count; i++) {
        memset(m->samples + m->sample_offset[i], 0, 4);
    }
    return true;
}

TtCreateResult tt_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    TtCreateResult result = { .status = TtStatus_InvalidData };
    if (!tt_probe(data, size) || io == nullptr || path == nullptr) {
        return result;
    }
    TtPlayer* p = calloc(1, sizeof(TtPlayer));
    uint8_t* copy = malloc(size);
    if (p == nullptr || copy == nullptr) {
        free(p);
        free(copy);
        result.status = TtStatus_OutOfMemory;
        return result;
    }
    memcpy(copy, data, size);
    TtModule* m = &p->module;
    m->data = copy;
    m->size = (uint32_t)size;
    m->subsong_count = data[3];
    m->first_block = 2 + 2 * (uint32_t)m->subsong_count;
    if (m->first_block + 2 > size) {
        tt_destroy(p);
        return result;
    }
    const uint8_t count_byte = data[m->first_block + 1];
    m->delta = (count_byte & 0x80) != 0;
    m->sample_count = count_byte & 0x7F;
    m->sample_table = m->first_block + 2;
    if (m->sample_count > TT_MAX_SAMPLES) {
        tt_destroy(p);
        return result;
    }

    uint8_t* file = nullptr;
    uint64_t file_size = 0;
    if (!load_sample_file(io, path, &file, &file_size, result.missing_file, sizeof(result.missing_file))) {
        tt_destroy(p);
        result.status = TtStatus_MissingSamples;
        return result;
    }
    const bool ok = prepare_samples(m, file, file_size);
    NuadeIo_free_url_to_memory(io, file);
    if (!ok) {
        tt_destroy(p);
        return result;
    }
    result.player = p;
    result.status = TtStatus_Ok;
    return result;
}

void tt_destroy(TtPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module.samples);
    free(player->module.data);
    free(player);
}

int tt_subsong_count(const TtPlayer* player) {
    return player->module.subsong_count;
}

// The block of a subsong (1-based): the sizes of the blocks before it are in the table after the subsong count. Bit 7
// of the sample count is the delta flag in the first block only.
static TtBlock subsong_block(const TtModule* m, int subsong) {
    uint32_t block = m->first_block;
    for (int i = 0; i < subsong - 1; i++) {
        block += mod_word(m, 4 + 2 * (uint32_t)i);
    }
    const uint8_t count_byte = mod_byte(m, block + 1);
    const uint32_t sample_count = block == m->first_block ? count_byte & 0x7F : count_byte;
    TtBlock b = { .order_count = mod_byte(m, block) };
    b.patterns = block + 2 + sample_count * SAMPLE_HEADER_SIZE;
    b.refs = b.patterns + (uint8_t)(b.order_count + 1) / 2 * 2;
    return b;
}

// 6ba: InitSound
bool tt_start(TtPlayer* player, int subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || subsong >= player->module.subsong_count) {
        return false;
    }
    TtSong* s = &player->song;
    *s = (TtSong) { 0 };
    s->block = subsong_block(&player->module, subsong + 1);
    s->speed = 6;
    for (int i = 0; i < TT_VOICES; i++) {
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

TtTickResult tt_tick(TtPlayer* player, NuadeVoices* voices) {
    TtSong* s = &player->song;
    s->looped = false;
    run_interrupt(player);
    for (int i = 0; i < TT_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, (uint32_t)i, voices);
    }
    return (TtTickResult) { .looped = s->looped };
}

TtPosition tt_position(const TtPlayer* player) {
    const TtSong* s = &player->song;
    return (TtPosition) { .order = s->shown_position, .row = s->shown_row, .speed = s->speed };
}

int tt_order_count(const TtPlayer* player) {
    return player->song.block.order_count;
}

TtVoiceState tt_voice_state(const TtPlayer* player, int voice) {
    const TtVoice* v = &player->song.voices[voice];
    return (TtVoiceState) { .note = v->note, .sample = v->sample_number };
}

TtCell tt_pattern_cell(const TtPlayer* player, int order, int voice, int row) {
    const TtModule* m = &player->module;
    uint32_t offset = row_data(m, &player->song.block, (uint32_t)order, (uint32_t)row);
    Cell cell = { 0 };
    for (int i = 0; i <= voice; i++) {
        offset += read_cell(m, offset, &cell);
    }
    return (TtCell) { .note = cell.note, .sample = cell.sample, .cmd = cell.cmd, .param = cell.param };
}
