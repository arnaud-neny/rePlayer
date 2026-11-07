// SPDX-License-Identifier: MIT
// AMOS Music AmosBank player, translated from the UADE eagleplayer "Amos Music AmosBank Player" (Dec 2004).
//
// The eagleplayer does not play the AMOS bank directly. InitPlayer converts it to a ProTracker song in memory
// (samples, an order list, 64-row patterns of four-byte cells), then a ProTracker 2.x replayer plays that song. This
// port keeps both halves: `convert_*` build the song from the bank, `play_*` are the replayer.
//
// AmosBank: "AmBk", a bank number and flags, the length long (top byte 0x80), "Music   ", the offset of the song block,
// the instrument table (a count word and 32-byte records) and the samples; see `read_header` for the offsets. The same
// bank without the 4 magic bytes is accepted: its length long is at 0 with the top byte 0xC0, and the other fields sit
// 4 bytes earlier than in the AmBk form.

#include <nuade/players/amos.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define BANK_MAGIC 0x416D426Bu // "AmBk"
#define BANK_MUSIC_MARK "Music   "
#define INSTRUMENT_SIZE 32

// AmosSong block
#define SONG_LIST_COUNT 6 // the word the check wants at the start

#define PATTERN_MAX 256 // an order entry is a byte

// The replayer's tables: 16 finetunes of 36 notes and a closing 0.
#define PERIOD_ROW 37
#define FINETUNES 16
#define PERIOD_SLIDE_MIN 0x71
#define PERIOD_SLIDE_MAX 0x358
#define VIBRATO_SHIFT 7

// CIA timer value of 125 BPM (50 Hz) and the dividend of the tempo command: 1773447 / BPM.
#define TIMER_DIVIDEND 1773447u
#define TIMER_DEFAULT 14187

// A loop starts up to 0xFFFE bytes past the start of its sample (the replayer adds loop_start * 2 as a word), so the
// zeros after the last sample reach that far before the Paula tail of nuade_paula_padded_copy.
#define LOOP_REACH 0x10000

// The finetune rows one after the other; a row ends with 0.
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

static const uint8_t k_sine[32] = { 0,   24,  49,  74,  97,  120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
                                    255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97,  74,  49,  24 };

static const uint8_t k_funk_speeds[16] = { 0, 5, 6, 7, 8, 10, 11, 13, 16, 19, 22, 26, 32, 43, 64, 128 };

// The ProTracker effect numbers a cell can hold. The converter writes 0 - 4, A - D and F; a row's one-shot effect is
// OR-ed with the persistent one, which can make D (volume with slide up), E (volume with slide down or volume slide)
// and F (volume with tone portamento, position jump with vibrato). 5 - 9 never occur.
typedef enum Effect {
    Effect_Arpeggio = 0x0,
    Effect_SlideUp = 0x1,
    Effect_SlideDown = 0x2,
    Effect_TonePortamento = 0x3,
    Effect_Vibrato = 0x4,
    Effect_VolumeSlide = 0xA,
    Effect_PositionJump = 0xB,
    Effect_Volume = 0xC,
    Effect_PatternBreak = 0xD,
    Effect_Extended = 0xE,
    Effect_Speed = 0xF,
} Effect;

// The sub-commands of Effect_Extended (the high nibble of the parameter).
typedef enum ExtendedEffect {
    Extended_Filter = 0x0,
    Extended_FineSlideUp = 0x1,
    Extended_FineSlideDown = 0x2,
    Extended_Glissando = 0x3,
    Extended_VibratoWaveform = 0x4,
    Extended_Finetune = 0x5,
    Extended_PatternLoop = 0x6,
    Extended_Retrigger = 0x9,
    Extended_FineVolumeUp = 0xA,
    Extended_FineVolumeDown = 0xB,
    Extended_NoteCut = 0xC,
    Extended_NoteDelay = 0xD,
    Extended_PatternDelay = 0xE,
    Extended_InvertLoop = 0xF,
} ExtendedEffect;

// An effect and its parameter: the persistent effect of a track, or what a row ends with.
typedef struct EffectWord {
    uint8_t effect;
    uint8_t param;
} EffectWord;

static uint8_t bank_u8(const AmosBank* b, int64_t offset) {
    return offset >= 0 && (uint64_t)offset < b->size ? b->data[offset] : 0;
}

static uint16_t bank_u16(const AmosBank* b, int64_t offset) {
    return (uint16_t)(bank_u8(b, offset) << 8 | bank_u8(b, offset + 1));
}

static int16_t bank_i16(const AmosBank* b, int64_t offset) {
    return (int16_t)bank_u16(b, offset);
}

static uint32_t bank_u32(const AmosBank* b, int64_t offset) {
    return (uint32_t)bank_u16(b, offset) << 16 | bank_u16(b, offset + 2);
}

static bool bank_in_range(const AmosBank* b, int64_t offset, int64_t length) {
    return offset >= 0 && length >= 0 && (uint64_t)(offset + length) <= b->size;
}

// Copies up to `count` bytes from `offset`; what lies outside the bank is left as it is.
static void bank_copy(const AmosBank* b, size_t offset, void* destination, size_t count) {
    if (offset >= b->size) {
        return;
    }
    memcpy(destination, b->data + offset, count < b->size - offset ? count : b->size - offset);
}

// Where the parts of the bank are.
typedef struct BankHeader {
    int64_t instruments; // the instrument table: a count word, then the 32-byte records
    int64_t song;        // the song block
} BankHeader;

// The song block: a word 6, four offsets to the position lists (one per voice), the title and the lists.
typedef struct SongBlock {
    char title[AMOS_TITLE_SIZE + 1];
    int64_t order_list; // the position list of voice 0, which is the order list
    int64_t next_list;  // the list after it: the check wants the first list to end right before it
    int64_t last_list;  // the position list of the last voice; the track table follows its terminator
} SongBlock;

// An instrument record. The sample is described twice: by the start / end offsets and by the lengths in words; the
// second length (when not 0) wins.
typedef struct InstrumentRecord {
    uint32_t start;
    uint32_t end;
    uint16_t length_a;
    uint16_t loop_a;
    uint8_t finetune;
    uint8_t volume;
    uint16_t length_b;
    char name[AMOS_NAME_SIZE + 1];
} InstrumentRecord;

// Check2 for the bank header.
static bool read_header(const AmosBank* bank, BankHeader* header) {
    if (bank->size < 0x30) {
        return false;
    }
    const int64_t base = nuade_read_be32(bank->data) == BANK_MAGIC ? 4 : 0;
    // the length long of the bank starts with 0x80 (public memory flag) or, without the magic, with 0xC0
    if (base != 0 ? bank_u8(bank, base + 4) != 0x80 : bank_u8(bank, 0) != 0xC0) {
        return false;
    }
    if (memcmp(bank->data + base + 8, BANK_MUSIC_MARK, 8) != 0) {
        return false;
    }
    const int64_t song_field = base + 0x14;
    header->song = song_field + (int32_t)bank_u32(bank, song_field);
    header->instruments = base + 0x20;
    return true;
}

// The start of position list `index` of the song block: the offsets are signed words relative to the first one.
static int64_t position_list(const AmosBank* bank, int64_t song, int index) {
    return song + 2 + bank_i16(bank, song + 2 + 2 * index);
}

static bool read_song_block(const AmosBank* bank, int64_t song, SongBlock* block) {
    if (!bank_in_range(bank, song, 12) || bank_u16(bank, song) != SONG_LIST_COUNT) {
        return false;
    }
    for (int c = 0; c < AMOS_TITLE_SIZE; c++) {
        block->title[c] = (char)bank_u8(bank, song + 0xE + c);
    }
    block->title[AMOS_TITLE_SIZE] = '\0';
    block->order_list = position_list(bank, song, 0);
    block->next_list = position_list(bank, song, 1);
    block->last_list = position_list(bank, song, 3);
    return true;
}

static InstrumentRecord read_instrument(const AmosBank* bank, int64_t table, int index) {
    const int64_t at = table + 2 + (int64_t)INSTRUMENT_SIZE * index;
    InstrumentRecord r = { .start = bank_u32(bank, at),
                           .end = bank_u32(bank, at + 4),
                           .length_a = bank_u16(bank, at + 8),
                           .loop_a = bank_u16(bank, at + 10),
                           .finetune = bank_u8(bank, at + 12),
                           .volume = bank_u8(bank, at + 13),
                           .length_b = bank_u16(bank, at + 14) };
    for (int c = 0; c < AMOS_NAME_SIZE; c++) {
        r.name[c] = (char)bank_u8(bank, at + 16 + c);
    }
    return r;
}

// A track of the track table: a signed word offset from the table start, after the pattern count word.
static int64_t track_address(const AmosBank* bank, int64_t table, uint32_t track) {
    return table + bank_i16(bank, table + 2 + 2 * (int64_t)track);
}

// Check2 and the layout: the header, the song block, and the first position list ending right before the second.
static bool parse_bank(const AmosBank* bank, BankHeader* header, SongBlock* block) {
    if (!read_header(bank, header) || !read_song_block(bank, header->song, block)) {
        return false;
    }
    if (!bank_in_range(bank, block->next_list - 2, 2)) {
        return false;
    }
    const uint16_t before = bank_u16(bank, block->next_list - 2);
    return before == 0xFFFE || before == 0;
}

bool amos_probe(const uint8_t* data, size_t size) {
    if (data == nullptr) {
        return false;
    }
    const AmosBank bank = { .data = data, .size = size };
    BankHeader header;
    SongBlock block;
    return parse_bank(&bank, &header, &block);
}

// The commands of a track word with bit 15 set. They are the high byte; the low byte is the parameter.
enum {
    Command_EndTrack = 0x8000,
    Command_Volume = 0x8300,
    Command_ClearEffect = 0x8400,
    Command_Tempo = 0x8800,
    Command_Instrument = 0x8900,
    Command_Arpeggio = 0x8A00,
    Command_TonePortamento = 0x8B00,
    Command_Vibrato = 0x8C00,
    Command_VolumeSlide = 0x8D00,
    Command_SlideUp = 0x8E00,
    Command_SlideDown = 0x8F00,
    Command_Rest = 0x9000,
    Command_Jump = 0x9100,
};

// The other words of a track: 0x7Fnn + an operand word is a note of nn rows, a word with bits 12 and 13 set below
// that is a period for the row (OR-ed in), all other words are skipped.
#define WORD_COMMAND 0x8000
#define WORD_NOTE 0x7F00
#define WORD_PERIOD 0x3000

// The effect each persistent command (0x8A..0x8F) sets, in command order.
static const uint8_t k_persistent_effects[]
    = { Effect_Arpeggio, Effect_TonePortamento, Effect_Vibrato, Effect_VolumeSlide, Effect_SlideUp, Effect_SlideDown };

// State a track's conversion carries from row to row.
typedef struct TrackState {
    int8_t wait;            // rows left of the current note or rest
    EffectWord effect_keep; // the persistent effect, repeated on every row
    EffectWord effect_row;  // the effect this row ends with
} TrackState;

// The read position of a track stream. It does not run past the bank: the original would walk through memory.
typedef struct Stream {
    const AmosBank* bank;
    int64_t pos;
} Stream;

static uint16_t stream_next(Stream* s) {
    const uint16_t word = bank_u16(s->bank, s->pos);
    s->pos += 2;
    return word;
}

// A one-shot effect of this row only (jump, volume, tempo): the cell takes it and the row's effect follows it.
static void track_oneshot(AmosPtCell* c, TrackState* t, uint8_t effect, uint8_t param) {
    c->effect = effect;
    c->param = param;
    t->effect_row = (EffectWord) { effect, param };
}

// A persistent effect: the cell takes it and every following row of the note repeats it.
static void track_persistent(AmosPtCell* c, TrackState* t, uint8_t effect, uint8_t param) {
    c->effect = effect;
    c->param = param;
    t->effect_keep = (EffectWord) { effect, param };
    t->effect_row = t->effect_keep;
}

// The row ends with the persistent effect OR-ed into its cell.
static void track_apply_effect(AmosPtCell* c, EffectWord e) {
    c->effect |= e.effect;
    c->param |= e.param;
}

// Sets the instrument of the cell (the next sample number after the one in the word).
static void track_instrument(AmosPtCell* c, uint8_t param) {
    c->sample |= (uint8_t)(param + 1) & 0x1F;
}

// Command 0x7Fxx + operand word: a note. The operand holds the sample number (top 6 bits) and the period.
static void track_note(AmosPtCell* c, TrackState* t, uint16_t word, uint16_t operand) {
    c->sample = (uint8_t)((operand >> 10) + c->sample) & 0x1F;
    c->period = operand & 0x3FF;
    t->wait = (int8_t)(word & 0x7F);
}

// Converts one track into the cells of `channel` of `pattern` (the sub-routine at 0x420). The previous pattern is
// where an end-of-track mark on row 0 writes.
static void convert_track(const AmosBank* bank, int64_t track, AmosPattern* patterns, uint32_t pattern, int channel) {
    AmosPattern* pat = &patterns[pattern];
    Stream stream = { .bank = bank, .pos = track };
    TrackState t = { 0 };

    for (int row = 0; row < AMOS_ROWS; row++) {
        AmosPtCell* cell = &pat->cell[row][channel];
        *cell = (AmosPtCell) { 0 };

        // a note of n rows keeps the following n - 1 rows on its effect
        const int remaining = t.wait - 1;
        t.wait = (int8_t)remaining;
        if (remaining > 0) {
            track_apply_effect(cell, t.effect_keep);
            continue;
        }
        t.effect_row = t.effect_keep;

        bool row_done = false;
        bool keep_effect = true; // a rest ends its row without the persistent effect
        while (!row_done) {
            if ((uint64_t)stream.pos >= bank->size) {
                return; // ran off the bank
            }
            const uint16_t word = stream_next(&stream);
            if (word & WORD_COMMAND) {
                const uint8_t param = (uint8_t)word;
                const uint16_t command = word & 0xFF00;
                switch (command) {
                    case Command_Rest: {
                        t.wait = (int8_t)param;
                        cell->effect = t.effect_row.effect;
                        cell->param = t.effect_row.param;
                        row_done = true;
                        keep_effect = false;
                        break;
                    }
                    case Command_Jump:
                        track_oneshot(cell, &t, Effect_PositionJump, param);
                        break;
                    case Command_Volume:
                        track_oneshot(cell, &t, Effect_Volume, param);
                        break;
                    case Command_ClearEffect:
                        t.effect_keep = (EffectWord) { 0 };
                        break;
                    case Command_Tempo:
                        // 100 / n as the speed; an n of 0 is skipped
                        if (param != 0) {
                            track_oneshot(cell, &t, Effect_Speed, (uint8_t)(100 / param));
                        }
                        break;
                    case Command_Instrument:
                        track_instrument(cell, param);
                        break;
                    case Command_EndTrack: {
                        // a pattern break on the previous row
                        if (row > 0 || pattern > 0) {
                            AmosPtCell* prev = row > 0 ? &pat->cell[row - 1][channel]
                                                       : &patterns[pattern - 1].cell[AMOS_ROWS - 1][channel];
                            prev->param = 0;
                            prev->effect = Effect_PatternBreak;
                        }
                        return;
                    }
                    default:
                        if (command >= Command_Arpeggio && command <= Command_SlideDown) {
                            track_persistent(cell, &t, k_persistent_effects[(command - Command_Arpeggio) >> 8], param);
                        }
                        break; // other commands are skipped
                }
                continue;
            }
            if ((word & WORD_NOTE) == WORD_NOTE) {
                track_note(cell, &t, word, stream_next(&stream));
                row_done = true;
            } else if ((word & WORD_PERIOD) == WORD_PERIOD) {
                cell->period |= word & 0xFFF;
            }
        }
        if (keep_effect) {
            track_apply_effect(cell, t.effect_keep);
        }
    }
}

// InitPlayer, sample part: the instrument records become PT sample slots.
static void convert_instruments(AmosModule* m, const AmosBank* bank, int64_t table) {
    const uint16_t count = bank_u16(bank, table);
    m->sample_count = count > AMOS_SAMPLES ? AMOS_SAMPLES : count;
    m->samples_offset = (size_t)(table + 2 + (int64_t)INSTRUMENT_SIZE * count);

    for (int i = 0; i < AMOS_SAMPLES; i++) {
        m->samples[i].loop_length = 1;
    }
    for (int i = 0; i < m->sample_count; i++) {
        const InstrumentRecord r = read_instrument(bank, table, i);
        AmosSample* s = &m->samples[i];

        s->finetune = r.finetune;
        s->volume = r.volume;
        if (r.length_b != 0) {
            s->length = r.length_b;
            if ((int16_t)r.loop_a > 2) {
                s->loop_length = r.loop_a;
                s->loop_start = (uint16_t)(r.length_a * 2);
            }
        } else {
            s->length = r.length_a;
            if ((int16_t)r.loop_a > 2) {
                s->loop_length = r.loop_a;
                s->loop_start = (uint16_t)(r.end - r.start);
            }
        }
        memcpy(s->name, r.name, sizeof(s->name));
    }
}

// A word list up to its negative terminator: `end` is the position after the terminator, `max` the largest entry,
// `count` the number of entries.
typedef struct WordList {
    int64_t end;
    int max;
    uint32_t count;
    uint8_t entries[AMOS_ORDER_MAX]; // the first entries, as bytes
} WordList;

// False if the bank ends first.
static bool scan_list(const AmosBank* bank, int64_t start, WordList* list) {
    *list = (WordList) { 0 };
    int64_t pos = start;
    while (bank_in_range(bank, pos, 2)) {
        const int16_t value = bank_i16(bank, pos);
        pos += 2;
        if (value < 0) {
            list->end = pos;
            return true;
        }
        if (value > list->max) {
            list->max = value;
        }
        if (list->count < AMOS_ORDER_MAX) {
            list->entries[list->count] = (uint8_t)value;
        }
        list->count++;
    }
    return false;
}

static bool convert_song(AmosModule* m, const AmosBank* bank, const SongBlock* block) {
    memcpy(m->title, block->title, sizeof(m->title));

    WordList order;
    if (!scan_list(bank, block->order_list, &order)) {
        return false;
    }
    memcpy(m->order, order.entries, sizeof(m->order));
    m->order_length = (uint8_t)order.count;

    // the pattern count comes from the last voice's list, and the track table follows that list
    WordList last;
    if (!scan_list(bank, block->last_list, &last)) {
        return false;
    }
    int64_t track_table = last.end;
    if (bank_u16(bank, track_table) == 0) {
        track_table += 2;
    }
    m->pattern_count = (uint32_t)last.max + 1;
    if (m->pattern_count > PATTERN_MAX) {
        m->pattern_count = PATTERN_MAX;
    }
    m->patterns = (AmosPattern*)calloc(m->pattern_count, sizeof(AmosPattern));
    if (m->patterns == nullptr) {
        return false;
    }
    // pattern n holds tracks 4n .. 4n + 3 of the table
    for (uint32_t p = 0; p < m->pattern_count; p++) {
        for (int c = 0; c < AMOS_VOICES; c++) {
            convert_track(bank, track_address(bank, track_table, p * AMOS_VOICES + (uint32_t)c), m->patterns, p, c);
        }
    }
    return true;
}

AmosPlayer* amos_create(const uint8_t* data, size_t size) {
    if (data == nullptr) {
        return nullptr;
    }
    // the offsets and the title found in the file are the ones of the copy
    const AmosBank file = { .data = data, .size = size };
    BankHeader header;
    SongBlock block;
    if (!parse_bank(&file, &header, &block)) {
        return nullptr;
    }
    AmosPlayer* p = (AmosPlayer*)calloc(1, sizeof(AmosPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    AmosModule* m = &p->module;
    m->bank_data = (uint8_t*)malloc(size);
    if (m->bank_data == nullptr) {
        free(p);
        return nullptr;
    }
    memcpy(m->bank_data, data, size);
    m->bank = (AmosBank) { .data = m->bank_data, .size = size };

    convert_instruments(m, &m->bank, header.instruments);
    uint32_t sample_bytes = 0;
    for (int i = 0; i < AMOS_SAMPLES; i++) {
        m->samples[i].offset = sample_bytes;
        sample_bytes += (uint32_t)m->samples[i].length * 2;
    }
    p->sample_memory_size = (size_t)sample_bytes + LOOP_REACH + NUADE_PAULA_MAX_SAMPLE_BYTES;
    p->sample_memory = (int8_t*)nuade_paula_padded_copy(nullptr, (size_t)sample_bytes + LOOP_REACH);

    if (p->sample_memory == nullptr || !convert_song(m, &m->bank, &block)) {
        amos_destroy(p);
        return nullptr;
    }
    return p;
}

void amos_destroy(AmosPlayer* p) {
    if (p == nullptr) {
        return;
    }
    free(p->sample_memory);
    free(p->module.patterns);
    free(p->module.bank_data);
    free(p);
}

// The replayer

static bool cell_is_empty(const AmosPtCell* c) {
    return c->period == 0 && c->effect == 0 && c->param == 0 && c->sample == 0;
}

static const AmosPtCell* pattern_cell(const AmosModule* m, uint32_t pattern, int row, int channel) {
    static const AmosPtCell empty = { 0 };
    return pattern < m->pattern_count ? &m->patterns[pattern].cell[row][channel] : &empty;
}

// A word of the finetune tables. The original reads them as one array, so an index past a row runs into the next
// finetune; past the last row it would read the replayer's own variables, which this does not model (0).
static uint16_t period_at(uint8_t finetune, int index) {
    const int flat = finetune * PERIOD_ROW + index;
    return flat < FINETUNES * PERIOD_ROW ? k_periods[flat] : 0;
}

// The first entry of the finetune row that does not exceed the period (the row ends with a 0, so one always does).
static int period_index(uint8_t finetune, uint16_t period) {
    int index = 0;
    while (index < PERIOD_ROW - 1 && period < period_at(finetune, index)) {
        index++;
    }
    return index;
}

// The invert loop: each time the speed accumulator overflows, one byte of the loop is flipped.
static void funk_update(AmosVoice* v) {
    const uint8_t speed = v->funk_speed;
    if (speed == 0) {
        return;
    }
    v->funk_offset = (uint8_t)(v->funk_offset + k_funk_speeds[speed]);
    if (!(v->funk_offset & 0x80)) {
        return;
    }
    v->funk_offset = 0;
    if (v->loop_start == nullptr) {
        return;
    }
    int8_t* end = v->loop_start + 2 * (ptrdiff_t)v->loop_length;
    int8_t* next = v->funk_position + 1;
    if (next >= end) {
        next = v->loop_start;
    }
    v->funk_position = next;
    *next = (int8_t)(0xFF - (uint8_t)*next);
}

static void volume_slide_up(AmosVoice* v, uint8_t amount) {
    v->volume = (uint8_t)(v->volume + amount);
    // cmpi.b #$40 / bmi: the byte volume - 0x40 is not negative, which is 0x40 .. 0xBF
    if ((uint8_t)(v->volume - 0x40) < 0x80) {
        v->volume = 0x40;
    }
}

static void volume_slide_down(AmosVoice* v, uint8_t amount) {
    v->volume = (uint8_t)(v->volume - amount);
    if ((int8_t)v->volume < 0) {
        v->volume = 0;
    }
}

static void volume_slide(AmosVoice* v) {
    const uint8_t up = v->cell.param >> 4;
    if (up != 0) {
        volume_slide_up(v, up);
    } else {
        volume_slide_down(v, v->cell.param & 0xF);
    }
}

// Portamento up / down by `amount`, with the PT period limits. The period keeps its top nibble.
static void slide_up(AmosVoice* v, uint8_t amount) {
    v->period = (uint16_t)(v->period - amount);
    if ((int16_t)(v->period & 0xFFF) < PERIOD_SLIDE_MIN) {
        v->period = (uint16_t)((v->period & 0xF000) | PERIOD_SLIDE_MIN);
    }
    v->paula.period = v->period & 0xFFF;
}

static void slide_down(AmosVoice* v, uint8_t amount) {
    v->period = (uint16_t)(v->period + amount);
    if ((int16_t)(v->period & 0xFFF) >= PERIOD_SLIDE_MAX) {
        v->period = (uint16_t)((v->period & 0xF000) | PERIOD_SLIDE_MAX);
    }
    v->paula.period = v->period & 0xFFF;
}

// The parameter of a slide goes through a mask that starts at 0 and becomes 0xFF with the first slide (the fine
// slides set it to 0xF for their own parameter).
static uint8_t slide_amount(AmosSong* s, const AmosVoice* v) {
    const uint8_t amount = v->cell.param & s->slide_mask;
    s->slide_mask = 0xFF;
    return amount;
}

static void tone_portamento(AmosVoice* v) {
    if (v->cell.param != 0) {
        v->porta_speed = v->cell.param;
        v->cell.param = 0;
    }
    if (v->porta_target == 0) {
        return;
    }
    if (!v->porta_down) {
        v->period = (uint16_t)(v->period + v->porta_speed);
        if ((int16_t)v->porta_target <= (int16_t)v->period) {
            v->period = v->porta_target;
            v->porta_target = 0;
        }
    } else {
        v->period = (uint16_t)(v->period - v->porta_speed);
        if ((int16_t)v->porta_target >= (int16_t)v->period) {
            v->period = v->porta_target;
            v->porta_target = 0;
        }
    }
    // with the glissando flag the period snaps to a note
    v->paula.period = v->glissando ? period_at(v->finetune, period_index(v->finetune, v->period)) : v->period;
}

// Sets the target of a tone portamento from the note of the cell.
static void tone_portamento_setup(AmosVoice* v) {
    int index = period_index(v->finetune, v->cell.period);
    if ((v->finetune & 8) && index != 0) {
        index--;
    }
    v->porta_target = period_at(v->finetune, index);
    v->porta_down = false;
    if (v->porta_target == v->period) {
        v->porta_target = 0;
    } else if ((int16_t)v->porta_target < (int16_t)v->period) {
        v->porta_down = true;
    }
}

// The vibrato waveform: sine, ramp or square, indexed by bits 2..6 of the position.
static uint8_t waveform(uint8_t shape, uint8_t position) {
    const uint8_t index = (position >> 2) & 0x1F;
    switch (shape) {
        case 0:
            return k_sine[index];
        case 1: {
            const uint8_t ramp = (uint8_t)(index << 3);
            return (position & 0x80) ? (uint8_t)(0xFF - ramp) : ramp;
        }
        default:
            return 0xFF;
    }
}

// A parameter byte changes the depth (low nibble) and speed (high nibble) of the vibrato command.
static void update_vibrato_command(AmosVoice* v) {
    const uint8_t param = v->cell.param;
    if (param & 0xF) {
        v->vibrato_depth = param & 0xF;
    }
    if (param & 0xF0) {
        v->vibrato_speed = param >> 4;
    }
}

static void vibrato(AmosVoice* v) {
    if (v->cell.param != 0) {
        update_vibrato_command(v);
    }
    const uint8_t wave = waveform(v->vibrato_waveform, v->vibrato_pos);
    const uint16_t delta = (uint16_t)((wave * v->vibrato_depth) >> VIBRATO_SHIFT);
    v->paula.period = (v->vibrato_pos & 0x80) ? (uint16_t)(v->period - delta) : (uint16_t)(v->period + delta);
    v->vibrato_pos = (uint8_t)(v->vibrato_pos + v->vibrato_speed * 4);
}

static void arpeggio(const AmosSong* s, AmosVoice* v) {
    int offset;
    switch (s->tick % 3) {
        case 1:
            offset = v->cell.param >> 4;
            break;
        case 2:
            offset = v->cell.param & 0xF;
            break;
        default:
            v->paula.period = v->period;
            return;
    }
    for (int i = 0; i < PERIOD_ROW; i++) {
        if (v->period >= period_at(v->finetune, i)) {
            v->paula.period = period_at(v->finetune, i + offset);
            return;
        }
    }
}

// Restarts the sample of the voice: DMA off, registers, DMA on, then the repeat (the original waits a few raster lines
// between the steps).
static void retrigger(AmosVoice* v) {
    nuade_paula_channel_dma_off(&v->paula);
    v->paula.lc = v->start;
    v->paula.len_words = v->length;
    v->paula.period = v->period;
    nuade_paula_channel_dma_on(&v->paula);
    v->paula.lc = v->loop_start;
    v->paula.len_words = v->loop_length;
}

// E9x: retrigger every x ticks, from the first tick when the row has no note
static void retrigger_every(const AmosSong* s, AmosVoice* v) {
    const uint8_t interval = v->cell.param & 0xF;
    if (interval == 0 || (s->tick == 0 && v->cell.period != 0)) {
        return;
    }
    if (s->tick % interval == 0) {
        retrigger(v);
    }
}

static void pattern_jump(AmosSong* s, const AmosVoice* v) {
    s->song_position = (uint8_t)(v->cell.param - 1);
    s->break_row = 0;
    s->break_pending = true;
}

static void pattern_break(AmosSong* s, const AmosVoice* v) {
    const unsigned row = (unsigned)(v->cell.param >> 4) * 10 + (v->cell.param & 0xF);
    s->break_row = row > 0x3F ? 0 : (uint8_t)row;
    s->break_pending = true;
}

// E6x: x = 0 marks the loop row, otherwise loop back x times.
static void pattern_loop(AmosSong* s, AmosVoice* v) {
    const uint8_t count = v->cell.param & 0xF;
    if (count == 0) {
        v->loop_row = (uint8_t)(s->row & 0x3F);
        return;
    }
    if (v->loop_count == 0) {
        v->loop_count = count;
    } else if (--v->loop_count == 0) {
        return;
    }
    s->break_row = v->loop_row;
    s->loop_pending = true;
}

// Fxx: below 0x20 the speed (ticks per row), from there on the tempo as a CIA timer value.
static void set_speed(AmosSong* s, const AmosVoice* v) {
    const uint8_t param = v->cell.param;
    if (param == 0) {
        return;
    }
    if (param < 0x20) {
        s->tick = 0;
        s->speed = param;
    } else if (param != 0xFF) {
        s->timer = (uint16_t)(TIMER_DIVIDEND / param);
    }
}

// The E commands. The fine slides and the pattern commands act on the first tick of a row only.
static void extended(AmosSong* s, AmosVoice* v) {
    const uint8_t arg = v->cell.param & 0xF;
    switch (v->cell.param >> 4) {
        case Extended_Filter:
            break; // E0x: the LED filter, which the reference output has off
        case Extended_FineSlideUp:
            if (s->tick == 0) {
                s->slide_mask = 0xF;
                slide_up(v, slide_amount(s, v));
            }
            break;
        case Extended_FineSlideDown:
            if (s->tick == 0) {
                s->slide_mask = 0xF;
                slide_down(v, slide_amount(s, v));
            }
            break;
        case Extended_Glissando:
            v->glissando = arg != 0;
            break;
        case Extended_VibratoWaveform:
            v->vibrato_waveform = arg & 3;
            v->vibrato_continues = (arg & 4) != 0;
            break;
        case Extended_Finetune:
            v->finetune = arg;
            break;
        case Extended_PatternLoop:
            if (s->tick == 0) {
                pattern_loop(s, v);
            }
            break;
        case Extended_Retrigger:
            retrigger_every(s, v);
            break;
        case Extended_FineVolumeUp:
            if (s->tick == 0) {
                volume_slide_up(v, arg);
            }
            break;
        case Extended_FineVolumeDown:
            if (s->tick == 0) {
                volume_slide_down(v, arg);
            }
            break;
        case Extended_NoteCut:
            if (arg == s->tick) {
                v->volume = 0;
                v->paula.volume = 0;
            }
            break;
        case Extended_NoteDelay:
            if (arg == s->tick && v->cell.period != 0) {
                retrigger(v);
            }
            break;
        case Extended_PatternDelay:
            if (s->tick == 0 && s->delay_count == 0) {
                s->delay_request = (uint8_t)(arg + 1);
            }
            break;
        case Extended_InvertLoop:
            if (s->tick == 0) {
                v->funk_speed = arg;
                if (arg != 0) {
                    funk_update(v);
                }
            }
            break;
        default:
            break; // E7x (the tremolo waveform, there is no tremolo) and E8x
    }
}

// The commands of the first tick of a row, after the note.
static void first_tick_command(AmosSong* s, AmosVoice* v) {
    switch (v->cell.effect) {
        case Effect_PositionJump:
            pattern_jump(s, v);
            break;
        case Effect_PatternBreak:
            pattern_break(s, v);
            break;
        case Effect_Extended:
            extended(s, v);
            break;
        case Effect_Speed:
            set_speed(s, v);
            break;
        case Effect_Volume:
            v->volume = v->cell.param > 0x40 ? 0x40 : v->cell.param;
            break;
        default:
            v->paula.period = v->period;
            break;
    }
}

// The effects of the ticks after the first (the effects 5 - 9 cannot occur, see Effect).
static void tick_effects(AmosSong* s, AmosVoice* v) {
    funk_update(v);
    if (v->cell.effect == Effect_Arpeggio && v->cell.param == 0) {
        return;
    }
    switch (v->cell.effect) {
        case Effect_Arpeggio:
            arpeggio(s, v);
            break;
        case Effect_SlideUp:
            slide_up(v, slide_amount(s, v));
            break;
        case Effect_SlideDown:
            slide_down(v, slide_amount(s, v));
            break;
        case Effect_TonePortamento:
            tone_portamento(v);
            break;
        case Effect_Vibrato:
            vibrato(v);
            break;
        case Effect_VolumeSlide:
            v->paula.period = v->period;
            volume_slide(v);
            break;
        case Effect_Extended:
            extended(s, v);
            break;
        default:
            v->paula.period = v->period;
            break;
    }
}

static void run_tick_effects(AmosSong* s) {
    for (int i = 0; i < AMOS_VOICES; i++) {
        tick_effects(s, &s->voices[i]);
        s->voices[i].paula.volume = s->voices[i].volume;
    }
}

// Loads the sample the cell names: pointers, length, finetune, volume and the loop.
static void select_sample(const AmosPlayer* p, AmosVoice* v, uint8_t sample) {
    const AmosSample* slot = &p->song.samples[sample - 1];
    v->sample = sample;
    v->start = p->sample_memory + slot->offset;
    v->length = slot->length;
    v->finetune = slot->finetune;
    v->volume = slot->volume;
    if (slot->loop_start != 0) {
        v->loop_start = v->start + (uint16_t)(slot->loop_start * 2);
        v->funk_position = v->loop_start;
        v->length = (uint16_t)(slot->loop_start + (slot->loop_length == 0 ? 2 : 0) + slot->loop_length);
        v->loop_length = slot->loop_length;
    } else {
        v->loop_start = v->start;
        v->funk_position = v->start;
        v->loop_length = slot->loop_length != 0 ? slot->loop_length : 2;
    }
}

// Starts the note of the cell. The sample's DMA goes off now and comes on once all voices of the row are set.
static void start_note(AmosSong* s, AmosVoice* v) {
    v->note = period_index(0, v->cell.period);
    v->period = period_at(v->finetune, v->note);
    if (v->cell.effect == Effect_Extended && (v->cell.param >> 4) == Extended_NoteDelay) {
        first_tick_command(s, v); // note delay: the note starts on its tick
        return;
    }
    nuade_paula_channel_dma_off(&v->paula);
    if (!v->vibrato_continues) {
        v->vibrato_pos = 0;
    }
    v->paula.len_words = v->length;
    v->paula.lc = v->start;
    if (v->start == nullptr) {
        v->loop_start = nullptr;
        v->paula.len_words = 1;
        v->loop_length = 1;
    }
    v->paula.period = v->period;
    v->dma_pending = true;
    first_tick_command(s, v);
}

static void play_voice(AmosPlayer* p, AmosVoice* v, const AmosPtCell* cell) {
    AmosSong* s = &p->song;
    if (!v->cell_set) {
        v->paula.period = v->period;
    }
    v->cell = *cell;
    v->cell_set = !cell_is_empty(cell);

    if (cell->sample != 0) {
        select_sample(p, v, cell->sample);
    }
    if (v->cell.period == 0) {
        first_tick_command(s, v);
        return;
    }
    if (v->cell.effect == Effect_Extended && (v->cell.param >> 4) == Extended_Finetune) {
        v->finetune = v->cell.param & 0xF;
    } else if (v->cell.effect == Effect_TonePortamento) {
        tone_portamento_setup(v);
        first_tick_command(s, v);
        return;
    }
    start_note(s, v);
}

// The first tick of a row: SongEnd check, every voice reads its cell, DMA comes on for the started notes.
static void play_row(AmosPlayer* p) {
    AmosSong* s = &p->song;
    const AmosModule* m = &p->module;
    const uint32_t row = s->row;
    const uint32_t pattern = m->order[s->song_position];

    // a row that was played before, with no pattern loop running, is the end of the song
    bool* seen = &s->visited[s->song_position][row];
    bool loop_running = false;
    for (int i = 0; i < AMOS_VOICES; i++) {
        loop_running = loop_running || s->voices[i].loop_count != 0;
    }
    if (*seen && !loop_running) {
        // the original calls SongEnd on every row from here on; the first call is the end of the pass, so start the
        // next pass with nothing played
        s->looped = true;
        memset(s->visited, 0, sizeof(s->visited));
    }
    *seen = true;
    s->shown_order = (int)s->song_position;
    s->shown_row = (int)row;

    for (int i = 0; i < AMOS_VOICES; i++) {
        AmosVoice* v = &s->voices[i];
        play_voice(p, v, pattern_cell(m, pattern, (int)row, i));
        v->paula.volume = v->volume;
    }
    for (int i = 0; i < AMOS_VOICES; i++) {
        if (s->voices[i].dma_pending) {
            s->voices[i].dma_pending = false;
            nuade_paula_channel_dma_on(&s->voices[i].paula);
        }
    }
    // the repeat registers, written after the DMA start
    for (int i = 0; i < AMOS_VOICES; i++) {
        s->voices[i].paula.lc = s->voices[i].loop_start;
        s->voices[i].paula.len_words = s->voices[i].loop_length;
    }
}

static void next_position(AmosSong* s, const AmosModule* m) {
    s->row = s->break_row;
    s->break_row = 0;
    s->break_pending = false;
    s->song_position = (s->song_position + 1) & 0x7F;
    if ((uint8_t)s->song_position >= m->order_length) {
        s->song_position = 0;
    }
}

// After the row tick: next row, pattern delay, pattern loop, end of pattern.
static void advance_row(AmosSong* s, const AmosModule* m) {
    s->row++;
    if (s->delay_request != 0) {
        s->delay_count = s->delay_request;
        s->delay_request = 0;
    }
    if (s->delay_count != 0) {
        s->delay_count--;
        if (s->delay_count != 0) {
            s->row--;
        }
    }
    if (s->loop_pending) {
        s->loop_pending = false;
        s->row = s->break_row;
        s->break_row = 0;
    }
    if (s->row == AMOS_ROWS || s->break_pending) {
        next_position(s, m);
    }
}

static void play_tick(AmosPlayer* p) {
    AmosSong* s = &p->song;
    const AmosModule* m = &p->module;
    s->tick++;
    if (s->tick < s->speed) {
        run_tick_effects(s);
        if (s->break_pending) {
            next_position(s, m);
        }
        return;
    }
    s->tick = 0;
    if (s->delay_count == 0) {
        play_row(p);
    } else {
        run_tick_effects(s);
    }
    advance_row(s, m);
}

// Entry points

// InitSound: the sample memory as the bank has it, the first long of every sample cleared and a loop start that lies
// beyond the sample halved, then the song at its start.
bool amos_start(AmosPlayer* p, NuadeVoices* voices) {
    if (p == nullptr || voices == nullptr) {
        return false;
    }
    const AmosModule* m = &p->module;
    AmosSong* s = &p->song;
    *s = (AmosSong) { 0 };
    s->speed = 6;
    s->timer = TIMER_DEFAULT;

    memset(p->sample_memory, 0, p->sample_memory_size);
    for (int i = 0; i < AMOS_SAMPLES; i++) {
        s->samples[i] = m->samples[i];
        bank_copy(&m->bank, m->samples_offset + s->samples[i].offset, p->sample_memory + s->samples[i].offset,
                  (size_t)s->samples[i].length * 2);
    }
    for (int i = 0; i < AMOS_SAMPLES; i++) {
        AmosSample* sample = &s->samples[i];
        memset(p->sample_memory + sample->offset, 0, 4);
        if ((uint16_t)(sample->loop_start + sample->loop_length) > sample->length) {
            sample->loop_start >>= 1;
        }
    }

    for (int i = 0; i < AMOS_VOICES; i++) {
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

AmosTickResult amos_tick(AmosPlayer* p, NuadeVoices* voices) {
    AmosSong* s = &p->song;
    s->looped = false;
    // the CIA reloads its timer before the interrupt runs: a tempo change shows from the tick after the next
    const uint16_t length = s->timer;
    play_tick(p);
    for (int i = 0; i < AMOS_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, (uint32_t)i, voices);
    }
    return (AmosTickResult) { .looped = s->looped, .length = length };
}

const char* amos_title(const AmosPlayer* p) {
    return p->module.title;
}

const char* amos_sample_name(const AmosPlayer* p, int sample) {
    return sample >= 0 && sample < AMOS_SAMPLES ? p->module.samples[sample].name : "";
}

AmosPosition amos_position(const AmosPlayer* p) {
    const AmosSong* s = &p->song;
    return (AmosPosition) { .order = s->shown_order, .row = s->shown_row, .speed = (int)s->speed };
}

int amos_order_count(const AmosPlayer* p) {
    return p->module.order_length < AMOS_ORDER_MAX ? p->module.order_length : AMOS_ORDER_MAX;
}

AmosVoiceState amos_voice_state(const AmosPlayer* p, int voice) {
    const AmosVoice* v = &p->song.voices[voice];
    return (AmosVoiceState) { .note = v->sample != 0 ? v->note : -1, .sample = v->sample };
}

AmosCell amos_pattern_cell(const AmosPlayer* p, int order, int voice, int row) {
    const AmosModule* m = &p->module;
    if (order < 0 || order >= AMOS_ORDER_MAX) {
        return (AmosCell) { 0 };
    }
    const AmosPtCell* c = pattern_cell(m, m->order[order], row, voice);
    return (AmosCell) { .period = c->period, .sample = c->sample, .effect = c->effect, .param = c->param };
}
