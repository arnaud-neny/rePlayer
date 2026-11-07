// SPDX-License-Identifier: MIT
// Protracker4 player, translated from the UADE eagleplayer "Protracker4" (prefix ptm / mod3).
//
// The module is an IFF file (FORM MODL) with the chunks VERS, INFO (name, start tempo, tempo mode) and PTDT, which
// holds an ordinary 31 sample ProTracker module (title, sample headers, length, position list, tag, patterns) and is
// followed by the sample data. The sample data can run on past the end of the IFF form, so the replayer's memory is the
// whole file. The replayer is a ProTracker 2 style one: it keeps its tick counter, plays every effect of the original
// including the funk repeat and Karplus-Strong (E8x), and drives Paula through the eagleplayer's Poke functions.

#include <nuade/players/protracker4.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_tracker.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define PERIOD_ROW 37 // 36 notes and the terminating 0
#define FINETUNES 16

// The PTDT chunk holds a ProTracker module: this many bytes before the patterns
#define PTDT_HEADER_BYTES 0x43C
#define SAMPLE_NAME_SIZE 22
#define CELL_BYTES 4
// The song length byte, in the module and (where the position jump wrongly reads it) in the file
#define SONG_LENGTH_OFFSET 0x3B6
#define PATTERN_BYTES (PT4_ROWS * PT4_VOICES * CELL_BYTES)

#define VOLUME_MAX 64
// The period limits of the 1xx and 2xx slides
#define PERIOD_SLIDE_MIN 0x71
#define PERIOD_SLIDE_MAX 0x358

// The CIA timer value of a tempo is this divided by it. The replayer takes the NTSC dividend (the NTSC clock / 2) when
// the host's graphics base lacks the PAL flag, which UADE's does: a tick is 0.9 % longer than at the PAL dividend.
#define TIMER_DIVIDEND 1789773u
#define DEFAULT_BPM 125

// AUDxLC can lie this far past the end of the file (a sample start plus a loop start of up to 64 Ki words, then the
// funk repeat or Karplus-Strong moving through the loop); the memory copy keeps this much room before its Paula
// padding.
#define LOOP_REACH 0x20000u

// The IFF chunk tags
#define TAG_FORM 0x464F524Du
#define TAG_MODL 0x4D4F444Cu
#define TAG_VERS 0x56455253u
#define TAG_INFO 0x494E464Fu
#define TAG_PTDT 0x50544454u
#define TAG_8CHN 0x3843484Eu

#define INFO_BYTES 0x34

// The period tables of the 16 finetunes, 36 notes each and a 0 after them. A lookup always stops at the 0.
static const uint16_t k_periods[FINETUNES * PERIOD_ROW] = {
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240,
    226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113, 0,   850, 802, 757, 715, 674, 637, 601, 567, 535,
    505, 477, 450, 425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 239, 225, 213, 201, 189, 179, 169, 159, 150, 142,
    134, 126, 119, 113, 0,   844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447, 422, 398, 376, 355, 335, 316,
    298, 282, 266, 251, 237, 224, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118, 112, 0,   838, 791, 746, 704,
    665, 628, 592, 559, 528, 498, 470, 444, 419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235, 222, 209, 198, 187,
    176, 166, 157, 148, 140, 132, 125, 118, 111, 0,   832, 785, 741, 699, 660, 623, 588, 555, 524, 495, 467, 441, 416,
    392, 370, 350, 330, 312, 294, 278, 262, 247, 233, 220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 124, 117, 110,
    0,   826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437, 413, 390, 368, 347, 328, 309, 292, 276, 260, 245,
    232, 219, 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116, 109, 0,   820, 774, 730, 689, 651, 614, 580, 547,
    516, 487, 460, 434, 410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230, 217, 205, 193, 183, 172, 163, 154, 145,
    137, 129, 122, 115, 109, 0,   814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431, 407, 384, 363, 342, 323,
    305, 288, 272, 256, 242, 228, 216, 204, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114, 108, 0,   907, 856, 808,
    762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226, 214,
    202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 0,   900, 850, 802, 757, 715, 675, 636, 601, 567, 535, 505, 477,
    450, 425, 401, 379, 357, 337, 318, 300, 284, 268, 253, 238, 225, 212, 200, 189, 179, 169, 159, 150, 142, 134, 126,
    119, 0,   894, 844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447, 422, 398, 376, 355, 335, 316, 298, 282,
    266, 251, 237, 223, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118, 0,   887, 838, 791, 746, 704, 665, 628,
    592, 559, 528, 498, 470, 444, 419, 395, 373, 352, 332, 314, 296, 280, 264, 249, 235, 222, 209, 198, 187, 176, 166,
    157, 148, 140, 132, 125, 118, 0,   881, 832, 785, 741, 699, 660, 623, 588, 555, 524, 494, 467, 441, 416, 392, 370,
    350, 330, 312, 294, 278, 262, 247, 233, 220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 123, 117, 0,   875, 826,
    779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437, 413, 390, 368, 347, 328, 309, 292, 276, 260, 245, 232, 219,
    206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116, 0,   868, 820, 774, 730, 689, 651, 614, 580, 547, 516, 487,
    460, 434, 410, 387, 365, 345, 325, 307, 290, 274, 258, 244, 230, 217, 205, 193, 183, 172, 163, 154, 145, 137, 129,
    122, 115, 0,   862, 814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431, 407, 384, 363, 342, 323, 305, 288,
    272, 256, 242, 228, 216, 203, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114, 0,
};

static const uint8_t k_sine[32] = {
    0,   24,  49,  74,  97,  120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
    255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97,  74,  49,  24,
};

// EFx: the funk table, indexed by the speed.
static const uint8_t k_funk[16] = { 0, 5, 6, 7, 8, 10, 11, 13, 16, 19, 22, 26, 32, 43, 64, 128 };

// The effect nibble of a cell
typedef enum Effect {
    Effect_Arpeggio = 0x0,
    Effect_SlideUp = 0x1,
    Effect_SlideDown = 0x2,
    Effect_TonePortamento = 0x3,
    Effect_Vibrato = 0x4,
    Effect_TonePortamentoVolumeSlide = 0x5,
    Effect_VibratoVolumeSlide = 0x6,
    Effect_Tremolo = 0x7,
    Effect_SampleOffset = 0x9,
    Effect_VolumeSlide = 0xA,
    Effect_PositionJump = 0xB,
    Effect_Volume = 0xC,
    Effect_PatternBreak = 0xD,
    Effect_Extended = 0xE,
    Effect_Speed = 0xF,
} Effect;

// The high nibble of the parameter of Effect_Extended
typedef enum ExtendedEffect {
    Extended_Filter = 0x0,
    Extended_FineSlideUp = 0x1,
    Extended_FineSlideDown = 0x2,
    Extended_Glissando = 0x3,
    Extended_VibratoWaveform = 0x4,
    Extended_Finetune = 0x5,
    Extended_PatternLoop = 0x6,
    Extended_TremoloWaveform = 0x7,
    Extended_SmoothLoop = 0x8,
    Extended_Retrigger = 0x9,
    Extended_FineVolumeUp = 0xA,
    Extended_FineVolumeDown = 0xB,
    Extended_NoteCut = 0xC,
    Extended_NoteDelay = 0xD,
    Extended_PatternDelay = 0xE,
    Extended_Funk = 0xF,
} ExtendedEffect;

// Pt4Module

// Replayer state

// Pt4Module parsing

// A sample the port models: its finetune indexes the period tables, and it starts inside the file.
static bool sample_usable(const Pt4Module* m, const Pt4Sample* sample) {
    return sample->finetune < FINETUNES && sample->start <= m->size;
}

static bool find_chunk(const uint8_t* data, size_t size, uint32_t from, uint32_t form_size, uint32_t tag,
                       uint32_t* at) {
    // The replayer steps from chunk to chunk by adding the size field (which includes the 8 byte header) until the
    // position passes the end of the form; a chunk of size 0 would keep it there for ever, and a found one of size 0
    // counts as missing.
    uint64_t pos = from;
    for (unsigned i = 0; i < 256; i++) {
        if (pos > size || size - pos < 8) {
            return false;
        }
        const uint32_t chunk_size = nuade_read_be32(data + pos + 4);
        if (chunk_size == 0) {
            return false;
        }
        if (nuade_read_be32(data + pos) == tag) {
            *at = (uint32_t)pos;
            return true;
        }
        pos += chunk_size;
        if (pos >= form_size) {
            return false;
        }
    }
    return false;
}

// The cell at `p`: a 12 bit period, the sample number split over the first and third byte, effect and parameter.
static Pt4Cell read_cell(const uint8_t* p) {
    return (Pt4Cell) { .period = (uint16_t)((p[0] & 0x0F) << 8 | p[1]),
                       .sample = (uint8_t)((p[2] >> 4) | (p[0] & 0xF0)),
                       .effect = (uint8_t)(p[2] & 0x0F),
                       .param = p[3] };
}

// The INFO chunk: name, start tempo and tempo mode.
typedef struct InfoChunk {
    char title[PT4_INFO_TITLE_SIZE + 1];
    uint16_t bpm;
    bool tempo_commands; // flags bit 0: Fxx from 0x20 on sets the tempo (otherwise a speed)
} InfoChunk;

// `chunk` has INFO_BYTES
static InfoChunk read_info(const uint8_t* chunk) {
    InfoChunk info = { .bpm = nuade_read_be16(chunk + 0x30), .tempo_commands = (chunk[0x32] & 1) != 0 };
    memcpy(info.title, chunk + 8, PT4_INFO_TITLE_SIZE);
    return info;
}

// The header of the module in the PTDT chunk.
typedef struct PtdtHeader {
    uint8_t order_count;
    uint8_t orders[PT4_ORDERS];
    bool eight_channels; // the tag is "8CHN"
} PtdtHeader;

// `module` has PTDT_HEADER_BYTES
static PtdtHeader read_ptdt_header(const uint8_t* module) {
    PtdtHeader h
        = { .order_count = module[SONG_LENGTH_OFFSET], .eight_channels = nuade_read_be32(module + 0x438) == TAG_8CHN };
    memcpy(h.orders, module + 0x3B8, PT4_ORDERS);
    return h;
}

// The song length the position jump compares with: the replayer reads that byte of the file, as if the module started
// there instead of in the PTDT chunk.
static uint8_t read_jump_song_length(const uint8_t* file) {
    return file[SONG_LENGTH_OFFSET];
}

static Pt4Sample read_sample(const uint8_t* module, uint32_t start, unsigned index) {
    const uint8_t* h = module + 20 + index * 30 + SAMPLE_NAME_SIZE;
    return (Pt4Sample) { .start = start,
                         .length = nuade_read_be16(h),
                         .finetune = h[2],
                         .volume = h[3],
                         .loop_start = nuade_read_be16(h + 4),
                         .loop_length = nuade_read_be16(h + 6) };
}

// The cells of the played positions must stay inside what the port models: pattern numbers below 0x80, sample numbers
// 1..31 of samples that start in the file and have a finetune of 0..15. (A row past the end of a pattern, reached by a
// Dxx to row 64, is checked when it is played.)
static bool reachable_cells_ok(const Pt4Module* m, const uint8_t* data) {
    bool used[PT4_SAMPLES] = { false };
    const unsigned reach = m->order_count == 0 ? 1 : (m->order_count < PT4_ORDERS ? m->order_count : PT4_ORDERS);
    for (unsigned order = 0; order < reach; order++) {
        if (m->orders[order] >= 0x80) {
            return false; // the replayer does not count it, its pattern pointer is read from past its table
        }
        const uint8_t* pattern = data + m->patterns + (uint32_t)m->orders[order] * PATTERN_BYTES;
        for (unsigned i = 0; i < PT4_ROWS * PT4_VOICES; i++) {
            const Pt4Cell cell = read_cell(pattern + i * CELL_BYTES);
            if (cell.sample > PT4_SAMPLES) {
                return false;
            }
            if (cell.sample != 0) {
                used[cell.sample - 1] = true;
            }
        }
    }
    for (unsigned i = 0; i < PT4_SAMPLES; i++) {
        if (used[i] && !sample_usable(m, &m->samples[i])) {
            return false;
        }
    }
    return true;
}

// The checks of Check2, InitPlayer and InitSound, then the module's tables. `data` is only read.
static bool parse_module(const uint8_t* data, size_t size, Pt4Module* m) {
    if (size < 0x20 || size > 0x7FFFFFFF) {
        return false;
    }
    const uint32_t form_size = nuade_read_be32(data + 4);
    if (nuade_read_be32(data) != TAG_FORM || form_size > size || nuade_read_be32(data + 8) != TAG_MODL
        || nuade_read_be32(data + 12) != TAG_VERS) {
        return false;
    }
    // A VERS chunk that says 0x16 is skipped by 0x12 bytes, the size of the one the editor writes
    const uint32_t first = nuade_read_be32(data + 16) == 0x16 ? 0x1E : 12;
    uint32_t info_at = 0;
    uint32_t ptdt_at = 0;
    if (!find_chunk(data, size, first, form_size, TAG_INFO, &info_at) || size - info_at < INFO_BYTES
        || !find_chunk(data, size, first, form_size, TAG_PTDT, &ptdt_at) || size - ptdt_at < 8 + PTDT_HEADER_BYTES
        || size <= SONG_LENGTH_OFFSET) {
        return false;
    }
    const uint8_t* module = data + ptdt_at + 8;
    const PtdtHeader header = read_ptdt_header(module);
    if (header.eight_channels) {
        return false; // eight channels drive Paula registers that do not exist
    }
    const InfoChunk info = read_info(data + info_at);
    *m = (Pt4Module) { .size = (uint32_t)size,
                       .song = ptdt_at + 8,
                       .order_count = header.order_count,
                       .file_song_length = read_jump_song_length(data),
                       .patterns = ptdt_at + 8 + PTDT_HEADER_BYTES,
                       .bpm = info.bpm,
                       .tempo_commands = info.tempo_commands };
    memcpy(m->orders, header.orders, PT4_ORDERS);
    memcpy(m->title, info.title, sizeof(m->title));
    // The highest pattern is found with a signed byte compare: numbers from 0x80 on never count
    int max_pattern = 0;
    for (unsigned i = 0; i < PT4_ORDERS; i++) {
        if ((int8_t)m->orders[i] > max_pattern) {
            max_pattern = (int8_t)m->orders[i];
        }
    }
    const uint64_t samples_at = (uint64_t)m->patterns + (uint64_t)(max_pattern + 1) * PATTERN_BYTES;
    if (samples_at > size) {
        return false;
    }
    uint64_t start = samples_at;
    for (unsigned i = 0; i < PT4_SAMPLES; i++) {
        m->samples[i] = read_sample(module, start > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)start, i);
        start += (uint64_t)m->samples[i].length * 2;
    }
    return reachable_cells_ok(m, data);
}

bool pt4_probe(const uint8_t* data, size_t size) {
    Pt4Module m;
    return data != nullptr && parse_module(data, size, &m);
}

// Paula

// PokeLen: a length of 0 or 1 word is written as 0 (65536 words)
static void write_length(Pt4Voice* v, uint32_t length) {
    v->paula.len_words = length <= 1 ? 0 : (uint16_t)length;
}

// PokeAdr. Paula ignores bit 0 of the address.
static void write_address(const Pt4Song* s, Pt4Voice* v, const int8_t* sample) {
    if (sample != nullptr && ((sample - (const int8_t*)s->module->mem) & 1)) {
        sample--;
    }
    v->paula.lc = sample;
}

static uint16_t timer_for_bpm(uint16_t bpm) {
    const uint32_t timer = TIMER_DIVIDEND / bpm;
    // The divide overflows below 28 BPM and leaves the dividend, of which the timer takes the low word
    return timer > 0xFFFF ? (uint16_t)TIMER_DIVIDEND : (uint16_t)timer;
}

// Period tables

// A word of the replayer's data block by word index from the start of the period tables. The tables end the block's
// first part; the word after them is the channel count and voice 0's record follows (the Amiga addresses in it, the
// sample and loop pointers, are not known: they read as 0). A flat read past the last table reaches it: an arpeggio on
// finetune 15 near the lower end of the table.
static uint16_t table_word(const Pt4Song* s, unsigned index) {
    const unsigned table_words = nuade_sizeof_array(k_periods);
    if (index < table_words) {
        return k_periods[index];
    }
    const Pt4Voice* v = &s->voices[0];
    switch (index - table_words) {
        case 0:
            return PT4_VOICES;
        case 1: // $00 the first word of the cell: the high bits of the sample number and the period
            return (uint16_t)(((v->cell.sample & 0xF0) << 8) | v->cell.period);
        case 2: // $02 effect and parameter, with the low bits of the sample number
            return (uint16_t)(((v->cell.sample & 0x0F) << 12) | (v->cell.effect << 8) | v->cell.param);
        case 6: // $0A length (the long at $08; its high word is 0)
            return (uint16_t)v->length;
        case 10: // $12 loop length (the long at $10; its high word is 0)
            return (uint16_t)v->loop_length;
        case 11: // $14
            return v->period;
        case 12: // $16
            return (uint16_t)((v->finetune << 8) | v->volume);
        case 13: // $18 the voice's DMA bit
            return 1;
        case 14: // $1A
            return (uint16_t)(((v->porta_down ? 1 : 0) << 8) | v->porta_speed);
        default: // pointers, and beyond $1A what no read reaches
            return 0;
    }
}

// The index of the first entry of the finetune's row that is at or below `period` (the row's 0 always is).
static unsigned period_index(uint8_t finetune, uint16_t period) {
    unsigned i = 0;
    while (i < PERIOD_ROW - 1 && period < k_periods[finetune * PERIOD_ROW + i]) {
        i++;
    }
    return i;
}

// Effects

// 1A92: EFx moves the loop byte that is inverted through the sample's loop
static void funk_step(Pt4Song* s, Pt4Voice* v) {
    if (v->funk_speed == 0) {
        return;
    }
    v->funk_acc = (uint8_t)(v->funk_acc + k_funk[v->funk_speed]);
    if (!(v->funk_acc & 0x80)) {
        return;
    }
    v->funk_acc = 0;
    if (v->loop == nullptr || v->funk_pos == nullptr) {
        return;
    }
    const int8_t* end = v->loop + 2 * (size_t)v->loop_length;
    const int8_t* pos = v->funk_pos + 1;
    if (pos >= end) {
        pos = v->loop;
    }
    v->funk_pos = pos;
    const uint8_t* mem = s->module->mem;
    const ptrdiff_t at = (const uint8_t*)pos - mem;
    if (at >= 0 && (size_t)at < (size_t)s->module->size + LOOP_REACH) {
        s->module->mem[at] = (uint8_t)(0xFF - mem[at]);
    }
}

// 164E: 9xx
static void sample_offset(Pt4Voice* v) {
    if (v->cell.param != 0) {
        v->offset_memory = v->cell.param;
    }
    const uint32_t offset = (uint32_t)(v->offset_memory << 7);
    if (offset >= v->length) {
        v->length = 1;
        return;
    }
    v->length -= offset;
    if (v->sample != nullptr) {
        v->sample += offset * 2;
    }
}

// 1688: add to the volume, at most 64
static void volume_up(Pt4Voice* v, uint8_t amount) {
    v->volume = (uint8_t)(v->volume + amount);
    // A sample volume of 0xC0 and up is not clamped (the compare is a signed byte one)
    if (v->volume >= VOLUME_MAX && v->volume < 0xC0) {
        v->volume = VOLUME_MAX;
    }
}

// 16AA: subtract from the volume, at least 0
static void volume_down(Pt4Voice* v, uint8_t amount) {
    v->volume = (uint8_t)(v->volume - amount);
    if (v->volume >= 0x80) { // it wrapped below 0
        v->volume = 0;
    }
}

// 167C: Axy
static void volume_slide(Pt4Voice* v) {
    const uint8_t up = v->cell.param >> 4;
    if (up != 0) {
        volume_up(v, up);
    } else {
        volume_down(v, v->cell.param & 0x0F);
    }
}

// 1982: restart the sample (E9x, EDx): DMA off, the sample, DMA on, then the repeat
static void retrigger(const Pt4Song* s, Pt4Voice* v) {
    nuade_paula_channel_dma_off(&v->paula);
    write_address(s, v, v->sample);
    write_length(v, v->length);
    v->paula.period = v->period;
    nuade_paula_channel_dma_on(&v->paula);
    write_address(s, v, v->loop);
    write_length(v, v->loop_length);
    v->paula.period = v->period;
}

// 1370: 1xx, E1x
static void porta_up(Pt4Song* s, Pt4Voice* v) {
    const uint8_t amount = v->cell.param & s->fine_mask;
    s->fine_mask = 0xFF;
    v->period = (uint16_t)(v->period - amount);
    if ((v->period & 0xFFF) < PERIOD_SLIDE_MIN) {
        v->period = (uint16_t)((v->period & 0xF000) | PERIOD_SLIDE_MIN);
    }
    v->paula.period = v->period & 0xFFF;
}

// 13C0: 2xx, E2x
static void porta_down(Pt4Song* s, Pt4Voice* v) {
    const uint8_t amount = v->cell.param & s->fine_mask;
    s->fine_mask = 0xFF;
    v->period = (uint16_t)(v->period + amount);
    if ((v->period & 0xFFF) >= PERIOD_SLIDE_MAX) {
        v->period = (uint16_t)((v->period & 0xF000) | PERIOD_SLIDE_MAX);
    }
    v->paula.period = v->period & 0xFFF;
}

// 13FE: 3xx / 5xy at a row with a note: the note's period becomes the target
static void tone_porta_setup(Pt4Voice* v) {
    unsigned i = period_index(v->finetune, v->cell.period);
    if ((v->finetune & 8) && i != 0) {
        i--;
    }
    v->porta_target = k_periods[v->finetune * PERIOD_ROW + i];
    v->porta_down = false;
    if (v->porta_target == v->period) {
        v->porta_target = 0;
    } else if ((int16_t)v->porta_target < (int16_t)v->period) {
        v->porta_down = true;
    }
}

// 146E: one step of the tone portamento (3xx, 5xy)
static void tone_porta_step(Pt4Voice* v) {
    if (v->porta_target == 0) {
        return;
    }
    if (!v->porta_down) {
        v->period = (uint16_t)(v->period + v->porta_speed);
        if ((int16_t)v->porta_target <= (int16_t)v->period) { // reached
            v->period = v->porta_target;
            v->porta_target = 0;
        }
    } else {
        v->period = (uint16_t)(v->period - v->porta_speed);
        if ((int16_t)v->porta_target >= (int16_t)v->period) { // reached
            v->period = v->porta_target;
            v->porta_target = 0;
        }
    }
    uint16_t period = v->period;
    if (v->glissando != 0) {
        period = k_periods[v->finetune * PERIOD_ROW + period_index(v->finetune, period)];
    }
    v->paula.period = period;
}

// 1460: 3xx
static void tone_porta(Pt4Voice* v) {
    if (v->cell.param != 0) {
        v->porta_speed = v->cell.param;
        v->cell.param = 0;
    }
    tone_porta_step(v);
}

// 12FE: 0xy
static void arpeggio(const Pt4Song* s, Pt4Voice* v) {
    unsigned semitones;
    switch (s->counter % 3) {
        case 0:
            v->paula.period = v->period;
            return;
        case 1:
            semitones = v->cell.param >> 4;
            break;
        default:
            semitones = v->cell.param & 0x0F;
            break;
    }
    const unsigned i = period_index(v->finetune, v->period);
    v->paula.period = table_word(s, v->finetune * PERIOD_ROW + i + semitones);
}

// The vibrato and tremolo shape: 0 sine, 1 ramp down, otherwise square. The ramp takes its sign from the vibrato
// position, also for the tremolo.
static uint8_t wave_amplitude(const Pt4Voice* v, uint8_t position, uint8_t waveform) {
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

// 14F4 / 159E: 4xy, 7xy; the parameter nibbles that are zero keep the old speed or depth
static void update_wave_parameter(const Pt4Voice* v, uint8_t* speed, uint8_t* depth) {
    if (v->cell.param & 0x0F) {
        *depth = v->cell.param & 0x0F;
    }
    if (v->cell.param & 0xF0) {
        *speed = v->cell.param >> 4;
    }
}

// 151E
static void vibrato_step(Pt4Voice* v) {
    const uint8_t amplitude = wave_amplitude(v, v->vibrato_pos, v->vibrato_wave);
    const uint16_t delta = (uint16_t)((amplitude * v->vibrato_depth) >> 7);
    v->paula.period = (v->vibrato_pos & 0x80) ? (uint16_t)(v->period - delta) : (uint16_t)(v->period + delta);
    v->vibrato_pos = (uint8_t)(v->vibrato_pos + v->vibrato_speed * 4);
}

// 15CA: the tremolo writes the volume itself
static void tremolo_step(Pt4Voice* v) {
    const uint8_t amplitude = wave_amplitude(v, v->tremolo_pos, v->tremolo_wave);
    const uint16_t delta = (uint16_t)((amplitude * v->tremolo_depth) >> 6);
    int32_t volume = (v->tremolo_pos & 0x80) ? (int32_t)v->volume - delta : (int32_t)v->volume + delta;
    if (volume < 0) {
        volume = 0;
    }
    if (volume > VOLUME_MAX) {
        volume = VOLUME_MAX;
    }
    v->paula.volume = (uint8_t)volume;
    v->tremolo_pos = (uint8_t)(v->tremolo_pos + v->tremolo_speed * 4);
}

// 1912: E8x averages the loop's bytes with their neighbours, in the sample memory
static void smooth_loop(Pt4Song* s, Pt4Voice* v) {
    if (v->loop == nullptr) {
        return;
    }
    Pt4Module* m = s->module;
    const size_t limit = (size_t)m->size + LOOP_REACH;
    const ptrdiff_t first = (const uint8_t*)v->loop - m->mem;
    if (first < 0) {
        return;
    }
    // The last byte is averaged with the first. The first loop of the original runs once more than its count says
    // when the loop is empty.
    int64_t remaining = (int64_t)v->loop_length * 2 - 2;
    size_t at = (size_t)first;
    do {
        if (at + 1 < limit) {
            const int a = (int8_t)m->mem[at];
            const int b = (int8_t)m->mem[at + 1];
            m->mem[at] = (uint8_t)((a + b) >> 1);
        }
        at++;
        remaining--;
    } while (remaining >= 0);
    if (at < limit && (size_t)first < limit) {
        const int a = (int8_t)m->mem[at];
        const int b = (int8_t)m->mem[first];
        m->mem[at] = (uint8_t)((a + b) >> 1);
    }
}

// 17E6: Exy, on the row and on every tick; most take effect on the first tick only
static void extended_effect(Pt4Song* s, Pt4Voice* v) {
    const uint8_t x = v->cell.param & 0x0F;
    switch ((ExtendedEffect)(v->cell.param >> 4)) {
        case Extended_Filter: // E0x: the filter, no audio in the port
            break;
        case Extended_FineSlideUp:
            if (s->counter == 0) {
                s->fine_mask = 0x0F;
                porta_up(s, v);
            }
            break;
        case Extended_FineSlideDown:
            if (s->counter == 0) {
                s->fine_mask = 0x0F;
                porta_down(s, v);
            }
            break;
        case Extended_Glissando:
            v->glissando = x;
            break;
        case Extended_VibratoWaveform:
            v->vibrato_wave = x & 3;
            v->vibrato_continues = (x & 4) != 0;
            break;
        case Extended_Finetune:
            v->finetune = x;
            break;
        case Extended_PatternLoop: // pattern loop
            if (s->counter != 0) {
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
            s->loop_pending = true;
            break;
        case Extended_TremoloWaveform:
            v->tremolo_wave = x & 3;
            v->tremolo_continues = (x & 4) != 0;
            break;
        case Extended_SmoothLoop:
            smooth_loop(s, v);
            break;
        case Extended_Retrigger: // retrigger every x ticks, also on the first tick of a row without a note
            if (x != 0 && (s->counter != 0 || v->cell.period == 0) && s->counter % x == 0) {
                retrigger(s, v);
            }
            break;
        case Extended_FineVolumeUp:
            if (s->counter == 0) {
                volume_up(v, x);
            }
            break;
        case Extended_FineVolumeDown:
            if (s->counter == 0) {
                volume_down(v, x);
            }
            break;
        case Extended_NoteCut:
            if (x == s->counter) {
                v->volume = 0;
            }
            break;
        case Extended_NoteDelay: // note delay
            if (x == s->counter && v->cell.period != 0) {
                retrigger(s, v);
            }
            break;
        case Extended_PatternDelay:
            if (s->counter == 0 && s->delay_count == 0) {
                s->delay_set = (uint8_t)(x + 1);
            }
            break;
        case Extended_Funk:
            if (s->counter != 0) {
                break;
            }
            v->funk_speed = x;
            if (x != 0) {
                funk_step(s, v);
            }
            break;
    }
}

// 16BA: Bxx. A jump to the position being played ends the song when that is the last one, which the replayer decides
// from the song length byte it reads from the file start (see read_jump_song_length).
static void position_jump(Pt4Song* s, const Pt4Voice* v) {
    const uint32_t target = v->cell.param;
    const uint8_t last = (uint8_t)(s->module->file_song_length - 1);
    if (target <= s->order && (target < s->order || (uint8_t)s->order == last)) {
        s->looped = true;
    }
    s->order = target - 1;
    s->jump_row = 0;
    s->jump_pending = true;
}

// 171C: Dxx, a row past the pattern's end jumps to row 0
static void pattern_break(Pt4Song* s, const Pt4Voice* v) {
    const unsigned row = (v->cell.param >> 4) * 10 + (v->cell.param & 0x0F);
    s->jump_row = row > PT4_ROWS ? 0 : (uint16_t)row;
    s->jump_pending = true;
}

// 1744: Fxx
static void set_speed_or_tempo(Pt4Song* s, const Pt4Voice* v) {
    const uint8_t param = v->cell.param;
    if (param == 0) {
        return;
    }
    if (s->module->tempo_commands && param >= 0x20) {
        s->timer = timer_for_bpm(param);
        return;
    }
    s->counter = 0;
    s->speed = param;
}

// 17AC: the effects of the first tick of a row
static void row_effect(Pt4Song* s, Pt4Voice* v) {
    switch ((Effect)v->cell.effect) {
        case Effect_SampleOffset:
            sample_offset(v);
            break;
        case Effect_PositionJump:
            position_jump(s, v);
            break;
        case Effect_Volume:
            v->volume = v->cell.param > VOLUME_MAX ? VOLUME_MAX : v->cell.param;
            break;
        case Effect_PatternBreak:
            pattern_break(s, v);
            break;
        case Effect_Extended:
            extended_effect(s, v);
            break;
        case Effect_Speed:
            set_speed_or_tempo(s, v);
            break;
        default:
            v->paula.period = v->period;
            break;
    }
}

// 1278: the effects of the other ticks, then the volume
static void tick_voice(Pt4Song* s, Pt4Voice* v) {
    funk_step(s, v);
    const Pt4Cell* c = &v->cell;
    if (c->effect != 0 || c->param != 0) {
        switch ((Effect)c->effect) {
            case Effect_Arpeggio:
                arpeggio(s, v);
                break;
            case Effect_SlideUp:
                porta_up(s, v);
                break;
            case Effect_SlideDown:
                porta_down(s, v);
                break;
            case Effect_TonePortamento:
                tone_porta(v);
                break;
            case Effect_Vibrato:
                update_wave_parameter(v, &v->vibrato_speed, &v->vibrato_depth);
                vibrato_step(v);
                break;
            case Effect_TonePortamentoVolumeSlide:
                tone_porta_step(v);
                volume_slide(v);
                break;
            case Effect_VibratoVolumeSlide:
                vibrato_step(v);
                volume_slide(v);
                break;
            case Effect_Tremolo:
                v->paula.period = v->period;
                update_wave_parameter(v, &v->tremolo_speed, &v->tremolo_depth);
                tremolo_step(v); // sets the volume itself
                return;
            case Effect_VolumeSlide:
                v->paula.period = v->period;
                volume_slide(v);
                break;
            case Effect_Extended:
                extended_effect(s, v);
                break;
            default:
                v->paula.period = v->period;
                break;
        }
    }
    v->paula.volume = v->volume;
}

// Rows

// 0FD4: a sample number selects the sample, its loop, finetune and volume
static void select_sample(const Pt4Module* m, Pt4Voice* v, unsigned number) {
    if (number > PT4_SAMPLES || !sample_usable(m, &m->samples[number - 1])) {
        return; // only a row past the end of a pattern (Dxx to row 64) can name such a sample
    }
    const Pt4Sample* h = &m->samples[number - 1];
    v->sample_number = (uint8_t)number;
    v->sample = (const int8_t*)m->mem + h->start;
    v->length = h->length;
    v->finetune = h->finetune;
    v->volume = h->volume;
    v->loop_length = h->loop_length;
    if (h->loop_start != 0) {
        v->loop = v->sample + (uint16_t)(h->loop_start * 2);
        v->length = (uint16_t)(h->loop_start + h->loop_length);
    } else {
        v->loop = v->sample;
    }
    v->funk_pos = v->loop;
}

// 10C8: a note starts the sample: DMA off, the sample's registers; the DMA starts after the row
static void start_note(Pt4Song* s, Pt4Voice* v) {
    // the note is looked up in the table of finetune 0 and taken from the same place in the sample's table
    v->period = k_periods[v->finetune * PERIOD_ROW + period_index(0, v->cell.period)];
    v->note = nuade_period_to_note(v->cell.period);
    if (v->cell.effect == Effect_Extended && (v->cell.param >> 4) == Extended_NoteDelay) {
        return; // note delay: the retrigger comes with the effect
    }
    nuade_paula_channel_dma_off(&v->paula);
    if (!v->vibrato_continues) {
        v->vibrato_pos = 0;
    }
    if (!v->tremolo_continues) {
        v->tremolo_pos = 0;
    }
    write_length(v, v->length);
    write_address(s, v, v->sample);
    if (v->sample == nullptr) {
        v->loop = nullptr;
        v->loop_length = 1;
        write_length(v, v->loop_length);
    }
    v->paula.period = v->period;
    v->started = true;
}

// 0FD4: one cell of the row
static void play_cell(Pt4Song* s, Pt4Voice* v, const Pt4Cell* cell) {
    if (v->cell.period == 0 && v->cell.sample == 0 && v->cell.effect == 0 && v->cell.param == 0) {
        v->paula.period = v->period;
    }
    v->cell = *cell;
    if (cell->sample != 0) {
        select_sample(s->module, v, cell->sample);
    }
    if (cell->period == 0) {
        row_effect(s, v);
        return;
    }
    if (cell->effect == Effect_Extended && (cell->param >> 4) == Extended_Finetune) {
        v->finetune = cell->param & 0x0F;
    } else if (cell->effect == Effect_TonePortamento || cell->effect == Effect_TonePortamentoVolumeSlide) {
        tone_porta_setup(v);
        row_effect(s, v);
        return;
    } else if (cell->effect == Effect_SampleOffset) {
        row_effect(s, v); // the offset applies before the start, and again after it
    }
    start_note(s, v);
    row_effect(s, v);
}

// 0FAA: after the row, the voices that got a note start their DMA, then every voice's repeat is written (the
// eagleplayer waits for the DMA start in between)
static void start_dma(Pt4Song* s) {
    for (int i = 0; i < PT4_VOICES; i++) {
        if (s->voices[i].started) {
            nuade_paula_channel_dma_on(&s->voices[i].paula);
        }
    }
    for (int i = 0; i < PT4_VOICES; i++) {
        Pt4Voice* v = &s->voices[i];
        write_address(s, v, v->loop);
        write_length(v, v->loop_length);
    }
}

// F82: play the row
static void play_row(Pt4Song* s) {
    s->shown_order = (int)s->order;
    s->shown_row = s->row;
    // mulu.w: the cell offset is taken from the low word of the row * 4
    const uint32_t row_offset = ((uint32_t)s->row * CELL_BYTES & 0xFFFF) * PT4_VOICES;
    for (int i = 0; i < PT4_VOICES; i++) {
        Pt4Voice* v = &s->voices[i];
        v->started = false;
        const uint64_t at = (uint64_t)s->pattern + row_offset + (uint64_t)i * CELL_BYTES;
        uint8_t bytes[CELL_BYTES] = { 0 };
        for (unsigned b = 0; b < CELL_BYTES; b++) {
            if (at + b < s->module->size) {
                bytes[b] = s->module->mem[at + b];
            }
        }
        const Pt4Cell cell = read_cell(bytes);
        play_cell(s, v, &cell);
        v->paula.volume = v->volume;
    }
    start_dma(s);
}

// EB6: the pattern of the position
static void load_pattern(Pt4Song* s) {
    const Pt4Module* m = s->module;
    s->pattern = m->patterns + (uint32_t)m->orders[s->order & (PT4_ORDERS - 1)] * PATTERN_BYTES;
}

// 120C: on to the next position
static void next_position(Pt4Song* s) {
    s->row = s->jump_row;
    s->jump_row = 0;
    s->jump_pending = false;
    s->order = (s->order + 1) & (PT4_ORDERS - 1);
    if (s->order >= s->module->order_count) {
        s->order = 0;
        s->looped = true;
    }
    load_pattern(s);
}

// 11B2: on to the next row
static void advance_row(Pt4Song* s) {
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
    if (s->loop_pending) {
        s->loop_pending = false;
        s->row = s->jump_row;
        s->jump_row = 0;
    }
    if (s->row == PT4_ROWS) {
        next_position(s);
    }
}

// F26: the interrupt
static void run_interrupt(Pt4Song* s) {
    s->counter++;
    if (s->counter < s->speed) {
        for (int i = 0; i < PT4_VOICES; i++) {
            tick_voice(s, &s->voices[i]);
        }
    } else {
        s->counter = 0;
        if (s->delay_count != 0) { // pattern delay: the effects run again as on the first tick
            for (int i = 0; i < PT4_VOICES; i++) {
                tick_voice(s, &s->voices[i]);
            }
        } else {
            play_row(s);
        }
        advance_row(s);
    }
    if (s->jump_pending) {
        next_position(s);
    }
}

Pt4Player* pt4_create(const uint8_t* data, size_t size) {
    Pt4Player* p = calloc(1, sizeof(Pt4Player));
    if (p == nullptr) {
        return nullptr;
    }
    if (data == nullptr || !parse_module(data, size, &p->module)) {
        free(p);
        return nullptr;
    }
    Pt4Module* m = &p->module;
    m->mem = nuade_paula_padded_copy(nullptr, size + LOOP_REACH);
    if (m->mem == nullptr) {
        free(p);
        return nullptr;
    }
    memcpy(m->mem, data, size);
    m->original = malloc(size);
    if (m->original == nullptr) {
        pt4_destroy(p);
        return nullptr;
    }
    memcpy(m->original, data, size);
    return p;
}

void pt4_destroy(Pt4Player* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module.mem);
    free(player->module.original);
    free(player);
}

// D42: InitSound. The file is loaded afresh: the replayer patches the sample data as it plays.
bool pt4_start(Pt4Player* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    Pt4Module* m = &player->module;
    memcpy(m->mem, m->original, m->size);
    memset(m->mem + m->size, 0, LOOP_REACH);
    Pt4Song* s = &player->song;
    *s = (Pt4Song) { .module = m, .speed = 6, .timer = timer_for_bpm(m->bpm != 0 ? m->bpm : DEFAULT_BPM) };
    load_pattern(s);
    for (int i = 0; i < PT4_VOICES; i++) {
        s->voices[i].note = -1;
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

Pt4TickResult pt4_tick(Pt4Player* player, NuadeVoices* voices) {
    Pt4Song* s = &player->song;
    s->looped = false;
    // the CIA reloads its timer before the interrupt runs: a tempo change shows from the tick after the next
    const uint32_t length = s->timer;
    run_interrupt(s);
    for (int i = 0; i < PT4_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, (uint32_t)i, voices);
    }
    return (Pt4TickResult) { .looped = s->looped, .length = length };
}

Pt4Position pt4_position(const Pt4Player* player) {
    const Pt4Song* s = &player->song;
    return (Pt4Position) { .order = s->shown_order, .row = s->shown_row, .speed = s->speed };
}

int pt4_order_count(const Pt4Player* player) {
    return player->module.order_count;
}

const char* pt4_title(const Pt4Player* player) {
    return player->module.title;
}

Pt4VoiceState pt4_voice_state(const Pt4Player* player, int voice) {
    const Pt4Voice* v = &player->song.voices[voice];
    return (Pt4VoiceState) { .note = v->note, .sample = v->sample_number };
}

Pt4Cell pt4_pattern_cell(const Pt4Player* player, int order, int voice, int row) {
    const Pt4Module* m = &player->module;
    const uint64_t at = (uint64_t)m->patterns + (uint64_t)m->orders[order & (PT4_ORDERS - 1)] * PATTERN_BYTES
                        + (uint64_t)row * PT4_VOICES * CELL_BYTES + (uint64_t)voice * CELL_BYTES;
    if (at > m->size - CELL_BYTES) {
        return (Pt4Cell) { 0 };
    }
    return read_cell(m->mem + at);
}
