// SPDX-License-Identifier: MIT
// Quartet: the replayer of the eagleplayer "Quartet", one tick per call.
//
// What the eagleplayer's glue does that matters here: the interrupt runs the slides, counts down the row counter and
// plays a row when it runs out. A new note switches the voice's DMA off, writes the registers, waits eight raster
// lines, switches the DMA on, waits again and then writes the repeat registers. Its volume patch scales by 64/64 and
// all four voices are enabled, so the volume registers get the replayer's value unchanged. SongEnd is called when
// the row has a 0xFFFF word.

#include "nuade_base.h"
#include "qt_internal.h"
#include <string.h>

#define INITIAL_PERIOD 0x120
#define NO_REPEAT_WORDS 1
#define SLIDE_FIRST_ROW 2   // a slide's target is searched from the second row after the note
#define SLIDE_LAST_ROW 0x24 // and up to this many rows after it
#define OCTAVE_LOW_BIT 1
#define OCTAVE_HALVE_BIT 2
#define SLIDE_UP 1
#define SLIDE_DOWN (-1)

// The period table; the replayer reads past it (the words after its last entry are the next data of its hunk) for notes
// 13..15, and note 0 reads the word 0xFE bytes behind its start (the opcode of an instruction).
static const uint16_t note_periods[QT_NOTES] = {
    0x33C3, 0x00CC, 0x00D8, 0x00E5, 0x00F2, 0x0101, 0x0110, 0x0120,
    0x0131, 0x0143, 0x0157, 0x016B, 0x0180, 0x666C, 0x7279, 0x8088,
};

// The period of a note started in a row: octaves 2 and 3 halve the table value, octave 0 doubles it.

static uint16_t note_period(const QtVoiceWord* word) {
    uint16_t period = note_periods[word->note];
    if ((word->octave & OCTAVE_HALVE_BIT) != 0) {
        period = (uint16_t)(period >> 1);
    }
    if (word->octave == 0) {
        period = (uint16_t)(period << 1);
    }
    return period;
}

// The period a slide aims at: the same, except that octave 3 keeps the table value.

static uint16_t slide_target_period(const QtVoiceWord* word) {
    uint16_t period = note_periods[word->note];
    if ((word->octave & OCTAVE_LOW_BIT) != 0) {
        return period;
    }
    return (word->octave & OCTAVE_HALVE_BIT) != 0 ? (uint16_t)(period >> 1) : (uint16_t)(period << 1);
}

// A note whose next row has the 0xFE marker in the same voice slides to the next note of the voice that has volume
// bits, the distance in rows times the tempo ticks long. The step is the wrapped 16-bit difference divided by the
// ticks, its direction comes from the signed comparison of the two periods (the replayer's `bgt` after the subtraction,
// which sees the overflow of a difference beyond 16 bits). The slide goes on after it got there, until the voice starts
// another note.

static void slide_start(QtPlayer* player, QtVoice* v, uint32_t index, uint32_t row) {
    QtVoiceWord target = { 0 };
    uint32_t distance = SLIDE_FIRST_ROW;
    for (; distance <= SLIDE_LAST_ROW; distance++) {
        if (!qt_module_voice_word(&player->module, row + distance, index, &target)) {
            player->unmodeled = true;
            return;
        }
        if (target.volume_code != 0) {
            break;
        }
    }
    if (distance > SLIDE_LAST_ROW) {
        return;
    }
    const int16_t ticks = (int16_t)(distance * player->module.tempo);
    const uint16_t aim = slide_target_period(&target);
    const int16_t difference = (int16_t)(uint16_t)(aim - v->period);
    const int16_t rest = (int16_t)(difference % ticks);
    const int16_t remainder = rest < 0 ? (int16_t)-rest : rest;
    const bool upward = (int16_t)aim > (int16_t)v->period;
    v->slide = (QtSlide) { .active = true,
                           .quotient = (int16_t)(difference / ticks),
                           .remainder = remainder,
                           .sign = upward ? SLIDE_UP : SLIDE_DOWN,
                           .accumulated = 0,
                           .length = ticks };
}

static void slide_tick(QtVoice* v) {
    QtSlide* s = &v->slide;
    v->period = (uint16_t)(v->period + (uint16_t)s->quotient);
    int16_t accumulated = (int16_t)(s->accumulated + s->remainder);
    if (accumulated >= s->length) {
        v->period = (uint16_t)(v->period + (uint16_t)s->sign);
        accumulated = (int16_t)(accumulated - s->length);
    }
    s->accumulated = accumulated;
    v->paula.period = v->period;
}

// Points the channel at a sample range; a range outside the set is something the model does not cover.

static const int8_t* sample_lc(QtPlayer* player, uint32_t offset, uint16_t length_words) {
    const int8_t* lc = qt_samples_lc(&player->samples, offset, length_words);
    if (lc == nullptr && player->samples.data != nullptr) {
        player->unmodeled = true;
    }
    return lc;
}

// Address 0 of the machine, where the replayer points the repeat of a sample that does not loop: zeros, here the
// padding behind the sample set.

static const int8_t* silent_word(const QtSamples* samples) {
    return samples->data != nullptr ? (const int8_t*)samples->data + samples->size : nullptr;
}

// A new note: DMA off, the registers, DMA on, then the repeat. A sample that loops repeats whole; any other one repeats
// one word of silence.

static void voice_start_note(QtPlayer* player, QtVoice* v, const QtVoiceWord* word) {
    const QtSampleEntry* sample = &player->samples.entry[word->sample];
    const uint32_t start = sample->start + sample->offset;
    const uint16_t length = sample->length_override != 0 ? sample->length_override : sample->length_words;
    v->slide.active = false;
    v->period = note_period(word);
    v->sample = word->sample;
    v->has_note = true;
    nuade_paula_channel_dma_off(&v->paula);
    const int8_t* sample_start = sample_lc(player, start, length);
    v->paula.lc = sample_start;
    v->paula.len_words = length;
    v->paula.period = v->period;
    v->paula.volume = (uint8_t)(word->volume_code + 1);
    nuade_paula_channel_dma_on(&v->paula);
    if (((player->samples.loop_mask >> word->sample) & 1) != 0) {
        v->paula.lc = sample_start;
        v->paula.len_words = length;
    } else {
        v->paula.lc = silent_word(&player->samples);
        v->paula.len_words = NO_REPEAT_WORDS;
    }
}

static void voice_row(QtPlayer* player, uint32_t index) {
    QtVoice* v = &player->voices[index];
    QtVoiceWord word;
    if (!qt_module_voice_word(&player->module, player->row, index, &word)) {
        player->unmodeled = true;
        return;
    }
    if (!word.plays) {
        return;
    }
    voice_start_note(player, v, &word);
    QtVoiceWord next;
    if (!qt_module_voice_word(&player->module, player->row + 1, index, &next)) {
        player->unmodeled = true;
        return;
    }
    if (next.glide_marker) {
        slide_start(player, v, index, player->row);
    }
}

static void row_play(QtPlayer* player) {
    player->row_counter = player->module.tempo;
    for (uint32_t i = 0; i < QT_VOICES; i++) {
        voice_row(player, i);
    }
    player->row++;
}

// The row counter ran out. A row with a 0xFFFF word calls SongEnd. A long song then starts over at once; a short one
// stays on that row, and as the counter is not reloaded it runs 256 ticks to the next SongEnd.

static void row_next(QtPlayer* player) {
    bool ends = false;
    if (!qt_module_row_ends(&player->module, player->row, &ends)) {
        player->unmodeled = true;
        return;
    }
    if (ends) {
        player->looped = true;
        if (!player->module.loops) {
            return;
        }
        player->row = 0;
    }
    row_play(player);
}

static void tick_logic(QtPlayer* player) {
    for (uint32_t i = 0; i < QT_VOICES; i++) {
        if (player->voices[i].slide.active) {
            slide_tick(&player->voices[i]);
        }
    }
    player->row_counter = (uint8_t)(player->row_counter - 1);
    if (player->row_counter == 0) {
        row_next(player);
    }
}

QtTickResult qt_tick(QtPlayer* player, NuadeVoices* voices) {
    tick_logic(player);
    for (uint32_t i = 0; i < QT_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    const QtTickResult result = { .looped = player->looped, .ended = player->unmodeled };
    player->looped = false;
    return result;
}

// InitSound: the first row, the tempo as row counter, every period 0x120 and every volume 0.

static void player_reset(QtPlayer* player) {
    memset(player->voices, 0, sizeof(player->voices));
    for (uint32_t i = 0; i < QT_VOICES; i++) {
        player->voices[i].period = INITIAL_PERIOD;
    }
    player->row = 0;
    player->row_counter = player->module.tempo;
    player->unmodeled = false;
    player->looped = false;
}

bool qt_start(QtPlayer* player, NuadeVoices* voices) {
    (void)voices;
    player_reset(player);
    return true;
}

uint32_t qt_tempo(const QtPlayer* player) {
    return player->module.tempo;
}

QtVoiceState qt_voice_state(const QtPlayer* player, uint32_t voice) {
    if (voice >= QT_VOICES || !player->voices[voice].has_note) {
        return (QtVoiceState) { .period = 0, .instrument = -1 };
    }
    return (QtVoiceState) { .period = player->voices[voice].period, .instrument = player->voices[voice].sample };
}

// What decides whether the replayer reaches something the model does not cover is song data only (rows, slide
// searches, sample ranges), never audio. Playing the rows once, from the first to the song end, tells before the song
// plays.

bool qt_song_modeled(const QtModule* module, const QtSamples* samples) {
    QtPlayer player = { .module = *module, .samples = *samples };
    player_reset(&player);
    const uint64_t max_rows = module->size / QT_ROW_BYTES + 2;
    for (uint64_t i = 0; i < max_rows && !player.looped && !player.unmodeled; i++) {
        row_next(&player);
    }
    return !player.unmodeled;
}
