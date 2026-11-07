// SPDX-License-Identifier: MIT
// Pierre Adane Packer: the replayer of the eagleplayer "PierreAdane", one tick per call.
//
// What the eagleplayer's glue does that matters here: the interrupt runs the replayer, then waits eight raster lines
// before it switches the DMA of the new notes on (the repeat registers are written after that), and the track list
// of the first voice ends the song with a SongEnd call. Its volume patch scales by 64/64 and all four voices are
// enabled, so the volume registers get the replayer's value unchanged.

#include "nuade_base.h"
#include "pa_internal.h"
#include <string.h>

#define INITIAL_SPEED 3
#define SLIDE_MASK 0x0F
#define MIN_SLIDE_PERIOD 0x71
#define MAX_SLIDE_PERIOD 0x358
#define VOLUME_MASK 0x7F
#define NIBBLE_BITS 4
#define NIBBLE_MASK 0x0F
#define LIST_ENTRY_BYTES 2
#define DMA_SET_BIT 0x8000
#define SIGN_BIT 0x8000
// Word indexes behind the arpeggio table (which has ARPEGGIO_PERIODS words, the last one 0): the replayer's variables.
#define ARPEGGIO_LIST_POSITION 39
#define ARPEGGIO_ROW 40
#define ARPEGGIO_DMA 41
#define ARPEGGIO_TICK 42
#define ARPEGGIO_SPEED 43
#define ARPEGGIO_ZERO 44
#define ARPEGGIO_TICK15_MASK 0x1E
#define DRY_RUN_TICKS 3000000u // ticks the open-time dry run covers at most

// The arpeggio table: three octaves, widest period first, then the zero word the replayer's data starts with. A note
// is looked up in it from the front.
#define ARPEGGIO_PERIODS 37
static const uint16_t arpeggio_periods[ARPEGGIO_PERIODS] = {
    0x358, 0x328, 0x2FA, 0x2D0, 0x2A6, 0x280, 0x25C, 0x23A, 0x21A, 0x1FC, 0x1E0, 0x1C5, 0x1AC,
    0x194, 0x17D, 0x168, 0x153, 0x140, 0x12E, 0x11D, 0x10D, 0x0FE, 0x0F0, 0x0E2, 0x0D6, 0x0CA,
    0x0BE, 0x0B4, 0x0AA, 0x0A0, 0x097, 0x08F, 0x087, 0x07F, 0x078, 0x071, 0x000,
};

// The words behind the arpeggio table are the replayer's own variables. Words 37 and 38 are the address the song was
// loaded at and everything from word 45 is outside the replayer's hunk: not modelled.

typedef struct ArpeggioWord {
    uint16_t value;
    bool known;
} ArpeggioWord;

// Whether a - b is negative as a 16-bit value (wrapped): the slides clamp on it.

static bool difference_negative(uint16_t a, uint16_t b) {
    return ((uint16_t)(a - b) & SIGN_BIT) != 0;
}

static void voice_set_volume(PaVoice* v, uint8_t volume) {
    v->paula.volume = volume & VOLUME_MASK;
}

// The DMA word: bit 15 and the voices that started a note in the last row (so far, while a row runs).

static uint16_t player_dma_word(const PaPlayer* player) {
    uint16_t word = DMA_SET_BIT;
    for (uint32_t i = 0; i < PA_VOICES; i++) {
        word = (uint16_t)(word | (player->voices[i].start_dma ? 1u << i : 0u));
    }
    return word;
}

static ArpeggioWord arpeggio_word(const PaPlayer* player, uint32_t index) {
    if (index < ARPEGGIO_PERIODS) {
        return (ArpeggioWord) { arpeggio_periods[index], true };
    }
    switch (index) {
        case ARPEGGIO_LIST_POSITION:
            return (ArpeggioWord) { player->list_position, true };
        case ARPEGGIO_ROW:
            return (ArpeggioWord) { player->row_in_track, true };
        case ARPEGGIO_DMA:
            return (ArpeggioWord) { player_dma_word(player), true };
        case ARPEGGIO_TICK:
            return (ArpeggioWord) { player->tick, true };
        case ARPEGGIO_SPEED:
            return (ArpeggioWord) { player->speed, true };
        case ARPEGGIO_ZERO:
            return (ArpeggioWord) { 0, true };
        default:
            return (ArpeggioWord) { 0, false };
    }
}

// An arpeggio tick: the word `steps` after the note's place in the table. The note is looked up from the front; the
// lookup runs on through the variables when it is not in the table.

static void voice_arpeggio_step(PaPlayer* player, PaVoice* v, uint32_t steps) {
    uint32_t found = 0;
    while (found < ARPEGGIO_PERIODS && arpeggio_periods[found] != v->base_period) {
        found++;
    }
    const ArpeggioWord word
        = found < ARPEGGIO_PERIODS ? arpeggio_word(player, found + steps) : (ArpeggioWord) { 0, false };
    if (!word.known) {
        player->unmodeled = true;
        return;
    }
    v->paula.period = word.value;
}

// What the replayer does on tick `tick` of a row. Ticks 0 to 6 are table entries of its jump; the later ones run into
// the code behind the table, which decodes to the cases below. Tick 19 and later run an illegal instruction.

static void voice_arpeggio(PaPlayer* player, PaVoice* v) {
    const uint8_t high = v->note.param >> NIBBLE_BITS;
    const uint8_t low = v->note.param & NIBBLE_MASK;
    switch (player->tick) {
        case 0:
        case 2:
        case 5:
        case 14:
            voice_arpeggio_step(player, v, low);
            break;
        case 1:
        case 4:
        case 8:
        case 10:
            voice_arpeggio_step(player, v, high);
            break;
        case 3:
        case 6:
            v->paula.period = v->base_period;
            break;
        case 7:
        case 9: // nothing reaches the channel
            break;
        case 11:
            voice_arpeggio_step(player, v, 22);
            break;
        case 12:
            voice_arpeggio_step(player, v, 1);
            break;
        case 13:
            voice_arpeggio_step(player, v, 26);
            break;
        case 15:
            voice_arpeggio_step(player, v, v->note.param & ARPEGGIO_TICK15_MASK);
            break;
        case 16:
            voice_arpeggio_step(player, v, 16);
            break;
        case 17:
            voice_arpeggio_step(player, v, 34);
            break;
        case 18:
            voice_arpeggio_step(player, v, 18);
            break;
        default:
            player->unmodeled = true;
            break;
    }
}

static void voice_set_period(PaVoice* v, uint16_t period) {
    v->period = period;
    v->paula.period = period;
}

static void voice_effect(PaPlayer* player, PaVoice* v) {
    const uint16_t amount = v->note.param & SLIDE_MASK;
    switch (v->note.effect) {
        case PaEffect_Arpeggio:
            voice_arpeggio(player, v);
            break;
        case PaEffect_SlideUp: {
            uint16_t period = (uint16_t)(v->period - amount);
            if (difference_negative(period, MIN_SLIDE_PERIOD)) {
                period = MIN_SLIDE_PERIOD;
            }
            voice_set_period(v, period);
            break;
        }
        case PaEffect_SlideDown: {
            uint16_t period = (uint16_t)(v->period + amount);
            if (!difference_negative(period, MAX_SLIDE_PERIOD)) {
                period = MAX_SLIDE_PERIOD;
            }
            voice_set_period(v, period);
            break;
        }
        case PaEffect_Volume:
            voice_set_volume(v, v->note.param);
            break;
        case PaEffect_Speed:
            if (amount != 0) {
                player->speed = amount;
            }
            break;
        default:
            break;
    }
}

// The sample of a note's instrument. A looped instrument starts at its loop: the repeat is the only part played.

static void voice_load_instrument(PaPlayer* player, PaVoice* v) {
    const uint8_t number = v->note.instrument;
    PaInstrument instrument;
    if (!pa_module_instrument(&player->module, number, &instrument)) {
        player->unmodeled = true;
        return;
    }
    v->instrument = number;
    // The record of a number past the table is whatever the file holds there, and its sample pointer is zero: fine
    // until a note starts on it.
    v->sample_known = number <= player->module.instrument_count;
    v->sample = v->sample_known ? player->samples.start[number - 1] : 0;
    v->length_words = instrument.length_words;
    voice_set_volume(v, v->note.effect == PaEffect_Volume ? v->note.param : (uint8_t)instrument.volume);
    v->loop_length_words = instrument.loop_length_words;
    if (instrument.loop_length_words != PA_NO_LOOP_WORDS) {
        v->sample += instrument.loop_offset;
        v->length_words = instrument.loop_length_words;
    }
}

// A new note stops the channel and writes its registers; the DMA starts at the end of the tick.

static void voice_start_note(PaPlayer* player, PaVoice* v) {
    if (!v->sample_known) {
        player->unmodeled = true; // the registers would point into the Amiga's low memory
        return;
    }
    nuade_paula_channel_dma_off(&v->paula);
    if (v->loop_length_words == 0) {
        v->loop_length_words = PA_NO_LOOP_WORDS;
    }
    v->base_period = v->note.period;
    voice_set_period(v, v->note.period);
    v->paula.lc = pa_samples_lc(&player->samples, v->sample);
    v->paula.len_words = v->length_words;
    v->start_dma = true;
}

// Next entry of the voice's track; the entry's row count carries over to the following entries.

static void voice_read_entry(PaPlayer* player, PaVoice* v) {
    PaTrackEntry entry;
    if (!pa_module_track_entry(&player->module, (int16_t)v->track, &entry)) {
        player->unmodeled = true;
        return;
    }
    if (entry.sets_length) {
        v->row_length = (uint16_t)(entry.length - 1);
    }
    v->track = (uint16_t)(v->track + entry.bytes);
    v->note = entry.note;
    v->rows_left = (int16_t)v->row_length;
}

static bool note_is_empty(const PaNote* note) {
    return note->instrument == 0 && note->effect == PaEffect_Arpeggio && note->param == 0;
}

static void voice_row(PaPlayer* player, uint32_t index) {
    PaVoice* v = &player->voices[index];
    v->rows_left = (int16_t)(v->rows_left - 1);
    v->note = (PaNote) { 0 };
    if (v->rows_left < 0) {
        voice_read_entry(player, v);
    }
    if (v->note.instrument != 0) {
        voice_load_instrument(player, v);
    }
    if (v->note.period != 0) {
        voice_start_note(player, v);
    } else if (!note_is_empty(&v->note)) {
        voice_effect(player, v);
    }
}

// After 64 rows every voice takes the next track of its list; the end of the first voice's list ends the song.

static void tracks_advance(PaPlayer* player) {
    uint16_t track = 0;
    if (!pa_module_list_entry(&player->module, 0, (int16_t)player->list_position, &track)) {
        player->unmodeled = true;
        return;
    }
    if (track == 0) {
        player->looped = true;
        player->list_position = 0;
        if (!pa_module_list_entry(&player->module, 0, 0, &track)) {
            player->unmodeled = true;
            return;
        }
    }
    player->voices[0].track = track;
    for (uint32_t i = 1; i < PA_VOICES; i++) {
        if (!pa_module_list_entry(&player->module, i, (int16_t)player->list_position, &player->voices[i].track)) {
            player->unmodeled = true;
            return;
        }
    }
    player->list_position = (uint16_t)(player->list_position + LIST_ENTRY_BYTES);
}

static void row_tick(PaPlayer* player) {
    player->tick = 0;
    for (uint32_t i = 0; i < PA_VOICES; i++) {
        player->voices[i].start_dma = false;
    }
    for (uint32_t i = 0; i < PA_VOICES; i++) {
        voice_row(player, i);
    }
    player->row_in_track = (uint16_t)((player->row_in_track + 1) % PA_ROWS_PER_TRACK);
    if (player->row_in_track == 0) {
        tracks_advance(player);
    }
}

static void effect_tick(PaPlayer* player) {
    for (uint32_t i = 0; i < PA_VOICES; i++) {
        PaVoice* v = &player->voices[i];
        if (v->note.param != 0) {
            voice_effect(player, v);
        }
    }
}

// After the raster wait: the DMA of the voices the last row started goes on (the replayer writes the DMA word on every
// tick, a channel that runs is not affected). A note whose instrument has no loop then gets a repeat of one word,
// the first word of its sample, which the sample set has cleared.

static void dma_enable(PaPlayer* player) {
    for (uint32_t i = 0; i < PA_VOICES; i++) {
        if (player->voices[i].start_dma) {
            nuade_paula_channel_dma_on(&player->voices[i].paula);
        }
    }
    for (uint32_t i = 0; i < PA_VOICES; i++) {
        PaVoice* v = &player->voices[i];
        if (v->loop_length_words == PA_NO_LOOP_WORDS) {
            v->loop_length_words = 0;
            v->paula.len_words = PA_NO_LOOP_WORDS;
        }
    }
}

static void tick_logic(PaPlayer* player) {
    player->tick = (uint16_t)(player->tick + 1);
    if (player->tick == player->speed) {
        row_tick(player);
    } else {
        effect_tick(player);
    }
    dma_enable(player);
}

PaTickResult pa_tick(PaPlayer* player, NuadeVoices* voices) {
    tick_logic(player);
    for (uint32_t i = 0; i < PA_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    const PaTickResult result = { .looped = player->looped, .ended = player->unmodeled };
    player->looped = false;
    return result;
}

// InitSound: the first track of every list, speed 3, and the channels stopped with volume 0.

bool pa_start(PaPlayer* player, NuadeVoices* voices) {
    (void)voices;
    memset(player->voices, 0, sizeof(player->voices));
    player->speed = INITIAL_SPEED;
    player->tick = 0;
    player->row_in_track = 0;
    player->unmodeled = false;
    player->looped = false;
    for (uint32_t i = 0; i < PA_VOICES; i++) {
        if (!pa_module_list_entry(&player->module, i, 0, &player->voices[i].track)) {
            return false;
        }
    }
    player->list_position = LIST_ENTRY_BYTES;
    return true;
}

PaVoiceState pa_voice_state(const PaPlayer* player, uint32_t voice) {
    if (voice >= PA_VOICES) {
        return (PaVoiceState) { .period = 0, .instrument = -1 };
    }
    const PaVoice* v = &player->voices[voice];
    return (PaVoiceState) { .period = v->base_period, .instrument = v->instrument != 0 ? v->instrument : -1 };
}

// What decides whether the tick logic reaches something the model does not cover is song data only (rows, lists,
// speeds, instrument numbers), never audio. The dry run of the logic therefore tells before the song plays. It ends
// when the state at the song end repeats, or after DRY_RUN_TICKS.

typedef struct VoiceCycleState {
    uint16_t note_period;
    uint8_t note_instrument;
    uint8_t note_effect;
    uint8_t note_param;
    int16_t rows_left;
    uint16_t row_length;
    uint16_t track;
    uint16_t base_period;
    bool sample_known;
    bool start_dma;
} VoiceCycleState;

typedef struct CycleState {
    uint16_t list_position;
    uint16_t row_in_track;
    uint16_t tick;
    uint16_t speed;
    VoiceCycleState voices[PA_VOICES];
} CycleState;

static CycleState cycle_state(const PaPlayer* player) {
    CycleState state;
    memset(&state, 0, sizeof(state));
    state.list_position = player->list_position;
    state.row_in_track = player->row_in_track;
    state.tick = player->tick;
    state.speed = player->speed;
    for (uint32_t i = 0; i < PA_VOICES; i++) {
        const PaVoice* v = &player->voices[i];
        state.voices[i].note_period = v->note.period;
        state.voices[i].note_instrument = v->note.instrument;
        state.voices[i].note_effect = (uint8_t)v->note.effect;
        state.voices[i].note_param = v->note.param;
        state.voices[i].rows_left = v->rows_left;
        state.voices[i].row_length = v->row_length;
        state.voices[i].track = v->track;
        state.voices[i].base_period = v->base_period;
        state.voices[i].sample_known = v->sample_known;
        state.voices[i].start_dma = v->start_dma;
    }
    return state;
}

bool pa_song_modeled(const PaModule* module) {
    PaPlayer player;
    memset(&player, 0, sizeof(player));
    player.module = *module;
    uint32_t end = 0;
    pa_module_sample_layout(module, player.samples.start, &end);
    if (!pa_start(&player, nullptr)) {
        return false;
    }
    CycleState previous;
    bool have_previous = false;
    for (uint32_t ticks = 0; ticks < DRY_RUN_TICKS; ticks++) {
        tick_logic(&player);
        if (player.unmodeled) {
            return false;
        }
        if (player.looped) {
            player.looped = false;
            const CycleState state = cycle_state(&player);
            if (have_previous && memcmp(&state, &previous, sizeof(state)) == 0) {
                return true;
            }
            memcpy(&previous, &state, sizeof(previous));
            have_previous = true;
        }
    }
    return true;
}
