// SPDX-License-Identifier: MIT
// Future Composer BSI replayer, translated from the UADE eagleplayer "FutureComposer-BSI" (the replayer is in the
// eagleplayer; the module holds only data).
//
// Module layout: "FUCO", then
//   4 x 256 bytes   sequence of each voice (pattern numbers and commands)
//   64 x 256 bytes  patterns
//   4 bytes, 64 x 16 bytes instruments (sample address filled in at load, length, loop offset, loop length), the table
//                   overlaps the first 12 bytes of the sample data
//   sample data     8-bit, the samples one after the other
// One step of every voice runs each speed + 1 ticks, voice 3 first.

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <nuade/players/fcbsi.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC "FUCO"
#define MAGIC_SIZE 4
#define SEQUENCE_SIZE 256
#define PATTERN_SIZE 256
#define PATTERN_COUNT 64
#define INSTRUMENT_SIZE 16
#define SEQUENCE_OFFSET MAGIC_SIZE
#define PATTERN_OFFSET (SEQUENCE_OFFSET + FCBSI_VOICES * SEQUENCE_SIZE)
#define INSTRUMENT_OFFSET (PATTERN_OFFSET + PATTERN_COUNT * PATTERN_SIZE + 4)
#define SAMPLE_OFFSET (INSTRUMENT_OFFSET + 0x3F4)
#define MIN_MODULE_SIZE (INSTRUMENT_OFFSET + FC_INSTRUMENT_COUNT * INSTRUMENT_SIZE)

#define DEFAULT_SPEED 2
#define DEFAULT_DURATION 0x10
#define DEFAULT_VOLUME 0x40
#define PATTERN_POS_START 0xFFFF // low byte 0xFF: the first step reads the sequence
#define PATTERN_END 0xFF
#define MAX_STEPS 256 // fetches one voice makes in a tick before the replayer gives up (SongEnd)
#define PERIOD_MIN 0x7F
#define PERIOD_MAX 0x800
#define NOTE_COUNT 84

// Sequence bytes
#define SEQ_PATTERN_LIMIT 0x40 // below: pattern number
#define SEQ_REPEAT_FIRST 0x40  // 0x40..0x7F: repeat the next entry (n - 0x3F) times
#define SEQ_REPEAT_LIMIT 0x80
#define SEQ_TRANSPOSE_LIMIT 0xC0 // 0x80..0xBF: transpose; larger values, and those above 0x23, set the power LED
#define SEQ_MAX_TRANSPOSE 0x23
#define SEQ_LAST_ENTRY 0xFB // above: commands
#define SEQ_SET_VOLUME 0xFC
#define SEQ_JUMP 0xFD
#define SEQ_END_SONG 0xFE
#define SEQ_RESTART 0xFF

// Pattern bytes
#define PAT_NOTE_LIMIT 0x30       // below: note
#define PAT_SLIDE_FIRST 0x30      // 0x30..0x6F: slide (or its direction)
#define PAT_SPEED_FIRST 0x70      // 0x70..0x7F: speed
#define PAT_DURATION_FIRST 0x80   // 0x80..0xBF: default duration
#define PAT_INSTRUMENT_FIRST 0xC0 // 0xC0..0xFE: instrument
// Slide commands: 0x30 + n sets the amount to 2n, except these three, which set the direction
#define SLIDE_DIRECTION_UP 0x3D
#define SLIDE_DIRECTION_NONE 0x3E
#define SLIDE_DIRECTION_DOWN 0x3F

static const uint16_t fcbsi_periods[NOTE_COUNT] = {
    0x6B0, 0x650, 0x5F0, 0x5A0, 0x550, 0x500, 0x4B0, 0x470, 0x430, 0x3F8, 0x3C0, 0x388, 0x358, 0x328,
    0x2F8, 0x2D0, 0x2A8, 0x280, 0x258, 0x238, 0x218, 0x1FC, 0x1E0, 0x1C4, 0x1AC, 0x194, 0x17C, 0x168,
    0x154, 0x140, 0x12C, 0x11C, 0x10C, 0x0FE, 0x0F0, 0x0E2, 0x0D6, 0x0CA, 0x0BE, 0x0B4, 0x0AA, 0x0A5,
    0x096, 0x08E, 0x086, 0x07F, 0x078, 0x071, 0x06B, 0x065, 0x05F, 0x05A, 0x055, 0x053, 0x04B, 0x047,
    0x043, 0x040, 0x03C, 0x039, 0x036, 0x033, 0x030, 0x02D, 0x02B, 0x029, 0x026, 0x024, 0x022, 0x020,
    0x01E, 0x01C, 0x01B, 0x019, 0x018, 0x017, 0x015, 0x014, 0x013, 0x012, 0x011, 0x010, 0x00F, 0x00E,
};

typedef enum FcBsiStage {
    FcBsiStage_PatternEnd,    // the pattern ended: the repeat count decides between the sequence and the countdown
    FcBsiStage_SequenceStart, // read the sequence, the entry repeats once
    FcBsiStage_Sequence,      // read the next sequence byte
    FcBsiStage_Countdown,     // count down the duration of the last note
    FcBsiStage_Pattern,       // read the next pattern byte
    FcBsiStage_Done,
} FcBsiStage;

typedef struct FcBsiInstrument {
    uint32_t length;
    uint32_t loop_offset;
    uint32_t loop_length;
} FcBsiInstrument;

bool fcbsi_probe(const uint8_t* data, size_t size) {
    return size >= MIN_MODULE_SIZE && memcmp(data, MAGIC, MAGIC_SIZE) == 0;
}

// A byte of module memory; what the replayer reads outside the module is unknown, it reads 0 here.
static uint8_t fcbsi_byte(const FcBsiPlayer* p, int64_t offset) {
    return offset >= 0 && offset < (int64_t)p->size ? p->chip[offset] : 0;
}

static FcBsiInstrument fcbsi_instrument(const FcBsiPlayer* p, unsigned index) {
    const uint8_t* record = p->chip + INSTRUMENT_OFFSET + (size_t)index * INSTRUMENT_SIZE;
    return (FcBsiInstrument) { .length = nuade_read_be32(record + 4),
                               .loop_offset = nuade_read_be32(record + 8),
                               .loop_length = nuade_read_be32(record + 12) };
}

// The sample address the voice gets for a module offset (32-bit like the 68000's); null outside the module.
static const int8_t* fcbsi_sample_at(const FcBsiPlayer* p, uint32_t offset) {
    return offset < p->size ? (const int8_t*)(p->chip + offset) : nullptr;
}

FcBsiPlayer* fcbsi_create(const uint8_t* data, size_t size) {
    if (!fcbsi_probe(data, size)) {
        return nullptr;
    }
    FcBsiPlayer* p = (FcBsiPlayer*)calloc(1, sizeof(*p));
    if (p == nullptr) {
        return nullptr;
    }
    p->chip = nuade_paula_padded_copy(data, size);
    if (p->chip == nullptr) {
        free(p);
        return nullptr;
    }
    p->size = size;
    // The samples follow each other; the 64th record reads its length from the sample data
    uint32_t offset = SAMPLE_OFFSET;
    for (unsigned i = 0; i < FC_INSTRUMENT_COUNT; i++) {
        p->sample_offset[i] = offset;
        offset += fcbsi_instrument(p, i).length;
    }
    return p;
}

void fcbsi_destroy(FcBsiPlayer* p) {
    if (p != nullptr) {
        free(p->chip);
        free(p);
    }
}

bool fcbsi_start(FcBsiPlayer* p, NuadeVoices* voices) {
    if (p == nullptr || voices == nullptr) {
        return false;
    }
    p->speed = DEFAULT_SPEED;
    p->speed_counter = 0;
    p->steps = 0;
    p->stopped = false;
    for (int i = 0; i < FCBSI_VOICES; i++) {
        p->voices[i] = (FcBsiVoice) { .pattern_pos = PATTERN_POS_START,
                                      .repeat_count = 1,
                                      .default_duration = DEFAULT_DURATION,
                                      .volume = DEFAULT_VOLUME,
                                      .note = -1 };
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

// The replayer counts its fetches; the 256th ends the song.
static bool fcbsi_count_step(FcBsiPlayer* p) {
    p->steps++;
    if (p->steps >= MAX_STEPS) {
        p->stopped = true;
        return false;
    }
    return true;
}

static uint8_t fcbsi_sequence_byte(const FcBsiPlayer* p, int voice, uint8_t pos) {
    return fcbsi_byte(p, SEQUENCE_OFFSET + voice * SEQUENCE_SIZE + pos);
}

// The pattern address is a 16-bit sum, signed when used as an index.
static uint8_t fcbsi_pattern_byte(const FcBsiPlayer* p, const FcBsiVoice* v) {
    const int16_t index = (int16_t)(uint16_t)(v->pattern_pos + (v->pattern << 8));
    return fcbsi_byte(p, PATTERN_OFFSET + index);
}

// One sequence byte; returns where the voice goes on. The power LED commands (the audio filter) are not modelled.
static FcBsiStage fcbsi_sequence_step(FcBsiPlayer* p, int voice) {
    FcBsiVoice* v = &p->voices[voice];
    if (!fcbsi_count_step(p)) {
        return FcBsiStage_Done;
    }
    const uint8_t code = fcbsi_sequence_byte(p, voice, v->sequence_pos);
    if (code < SEQ_PATTERN_LIMIT) {
        v->sequence_pos++;
        v->pattern = code;
        return FcBsiStage_Countdown;
    }
    if (code < SEQ_REPEAT_LIMIT) {
        v->sequence_pos++;
        v->repeat_count = (uint16_t)(code - (SEQ_REPEAT_FIRST - 1));
        return FcBsiStage_Sequence;
    }
    if (code <= SEQ_LAST_ENTRY) {
        if (code < SEQ_TRANSPOSE_LIMIT && (code & 0x3F) <= SEQ_MAX_TRANSPOSE) {
            v->transpose = code & 0x3F;
        }
        v->sequence_pos++;
        return FcBsiStage_Sequence;
    }
    switch (code) {
        case SEQ_SET_VOLUME:
            v->sequence_pos = (uint8_t)(v->sequence_pos + 1);
            v->volume = fcbsi_sequence_byte(p, voice, v->sequence_pos);
            v->paula.volume = v->volume;
            v->sequence_pos++;
            return FcBsiStage_Sequence;
        case SEQ_JUMP:
            v->sequence_pos = fcbsi_sequence_byte(p, voice, (uint8_t)(v->sequence_pos + 1));
            return FcBsiStage_SequenceStart;
        case SEQ_END_SONG:
            p->stopped = true;
            return FcBsiStage_Done;
        case SEQ_RESTART:
            v->sequence_pos = 0;
            return FcBsiStage_SequenceStart;
        default:
            return FcBsiStage_Sequence;
    }
}

// A note: restart the instrument's sample on the voice's channel.
static void fcbsi_start_note(FcBsiPlayer* p, FcBsiVoice* v, uint8_t note) {
    const FcBsiInstrument instrument = fcbsi_instrument(p, v->instrument);
    if (instrument.length != 0) {
        NuadePaulaChannel* c = &v->paula;
        const uint32_t sample = p->sample_offset[v->instrument];
        nuade_paula_channel_dma_off(c);
        c->lc = fcbsi_sample_at(p, sample);
        c->len_words = (uint16_t)(instrument.length >> 1);
        // note < 0x30 and transpose <= 0x23, so the index stays inside the table
        const unsigned index = note + v->transpose;
        v->note = (int)index;
        v->period = (uint16_t)(fcbsi_periods[index] + v->slide_amount);
        c->period = v->period;
        nuade_paula_channel_dma_on(c);
        // The loop registers are written after the sample started
        if (instrument.loop_length == 0) {
            c->len_words = 1;
        } else {
            c->len_words = (uint16_t)(instrument.loop_length >> 1);
            c->lc = fcbsi_sample_at(p, sample + instrument.loop_offset);
        }
        c->volume = v->volume;
    }
    v->duration_counter = (int16_t)v->default_duration;
    v->pattern_pos++;
}

// One pattern byte; returns where the voice goes on.
static FcBsiStage fcbsi_pattern_step(FcBsiPlayer* p, int voice) {
    FcBsiVoice* v = &p->voices[voice];
    if (!fcbsi_count_step(p)) {
        return FcBsiStage_Done;
    }
    const uint8_t code = fcbsi_pattern_byte(p, v);
    if (code == PATTERN_END) {
        return FcBsiStage_PatternEnd;
    }
    if (code < PAT_NOTE_LIMIT) {
        fcbsi_start_note(p, v, code);
        return FcBsiStage_Done;
    }
    if (code < PAT_SPEED_FIRST) {
        switch (code - PAT_SLIDE_FIRST) {
            case SLIDE_DIRECTION_UP:
                v->slide = FcBsiSlide_Up;
                break;
            case SLIDE_DIRECTION_NONE:
                v->slide = FcBsiSlide_None;
                break;
            case SLIDE_DIRECTION_DOWN:
                v->slide = FcBsiSlide_Down;
                break;
            default:
                v->slide_amount = (uint16_t)((code - PAT_SLIDE_FIRST) * 2);
                break;
        }
    } else if (code < PAT_DURATION_FIRST) {
        p->speed = code & 0x0F;
    } else if (code < PAT_INSTRUMENT_FIRST) {
        v->default_duration = (code & 0x3F) != 0 ? (code & 0x3F) : 0x40;
    } else {
        v->instrument = code & 0x3F;
    }
    v->pattern_pos = (uint16_t)((v->pattern_pos & 0xFF00) | (uint8_t)(v->pattern_pos + 1));
    return FcBsiStage_Pattern;
}

// The slide moves the period every step, clamped.
static void fcbsi_apply_slide(FcBsiVoice* v) {
    if (v->slide == FcBsiSlide_None) {
        return;
    }
    const uint16_t amount = v->slide == FcBsiSlide_Up ? v->slide_amount : (uint16_t)-v->slide_amount;
    uint16_t period = (uint16_t)(v->period + amount);
    if (period < PERIOD_MIN) {
        period = PERIOD_MIN;
    }
    if (period >= PERIOD_MAX) {
        period = PERIOD_MAX;
    }
    v->paula.period = period;
    v->period = period;
}

static void fcbsi_voice_step(FcBsiPlayer* p, int voice) {
    FcBsiVoice* v = &p->voices[voice];
    p->steps = 0;
    fcbsi_apply_slide(v);
    FcBsiStage stage = (v->pattern_pos & 0xFF) == PATTERN_END ? FcBsiStage_PatternEnd : FcBsiStage_Countdown;
    while (stage != FcBsiStage_Done) {
        switch (stage) {
            case FcBsiStage_PatternEnd:
                v->pattern_pos = 0;
                v->repeat_count--;
                stage = v->repeat_count != 0 ? FcBsiStage_Countdown : FcBsiStage_SequenceStart;
                break;
            case FcBsiStage_SequenceStart:
                v->repeat_count = 1;
                stage = FcBsiStage_Sequence;
                break;
            case FcBsiStage_Sequence:
                stage = fcbsi_sequence_step(p, voice);
                break;
            case FcBsiStage_Countdown:
                v->duration_counter--;
                stage = v->duration_counter > 0 ? FcBsiStage_Done : FcBsiStage_Pattern;
                break;
            case FcBsiStage_Pattern:
                stage = fcbsi_pattern_step(p, voice);
                break;
            case FcBsiStage_Done:
                break;
        }
    }
}

FcBsiTickResult fcbsi_tick(FcBsiPlayer* p, NuadeVoices* voices) {
    const bool was_stopped = p->stopped;
    if (was_stopped) {
        for (int i = 0; i < FCBSI_VOICES; i++) {
            nuade_paula_channel_dma_off(&p->voices[i].paula);
        }
    } else {
        p->speed_counter--;
        if (p->speed_counter < 0) {
            p->speed_counter = (int16_t)p->speed;
            for (int i = FCBSI_VOICES - 1; i >= 0; i--) {
                fcbsi_voice_step(p, i);
            }
        }
    }
    for (int i = 0; i < FCBSI_VOICES; i++) {
        nuade_paula_channel_sync(&p->voices[i].paula, (uint32_t)i, voices);
    }
    return (FcBsiTickResult) { .ended = was_stopped };
}

uint32_t fcbsi_grid_ticks(void) {
    return DEFAULT_SPEED + 1;
}

FcBsiVoiceState fcbsi_voice_state(const FcBsiPlayer* p, int voice) {
    if (voice < 0 || voice >= FCBSI_VOICES) {
        return (FcBsiVoiceState) { .note = -1, .instrument = -1 };
    }
    return (FcBsiVoiceState) { .note = p->voices[voice].note, .instrument = p->voices[voice].instrument };
}
