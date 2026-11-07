// SPDX-License-Identifier: MIT
// Tronic player (the TronicTracker replayer of the eagleplayer).

#include <nuade/players/tronic.h>

#include "nuade_base.h"
#include "tronic_internal.h"
#include <stdlib.h>

#define ENVELOPE_STAGE_BYTES 3
#define ENVELOPE_END_POS 0x0F
#define ENVELOPE_LAST_POS 0x0C
#define VIBRATO_STAGE_BYTES 3
#define VIBRATO_END_STAGE 0x0F
#define VIBRATO_LAST_STAGE 0x0C
#define WAVEFORM_JUMP 0xFF
#define SAMPLE_REPEAT_INITIAL_WORDS 4

// Module access

// The sample data at `at` for AUDxLC/AUDxLEN; a sample that does not lie inside the file sets the overrun flag.
static const int8_t* sample_pointer(TronicModule* module, int64_t at, uint16_t words) {
    const uint64_t bytes = (words != 0 ? (uint64_t)words : 0x10000u) * 2;
    if (at < 0 || (uint64_t)at + bytes > module->size) {
        module->overrun = true;
        return tronic_silence(module);
    }
    return (const int8_t*)(module->data + at);
}

static int64_t sample_start(TronicModule* module, uint8_t number) {
    return (int64_t)module->samples + tronic_long(module, (uint64_t)module->sample_table + 4u * number);
}

uint32_t tronic_effect_wave_table(const TronicModule* module, TronicCell row) {
    // The table address takes its upper bits from a register the row leaves over: after a note, the instrument offset's
    // high byte; without a note they are taken as 0, so the full argument byte selects the table.
    const unsigned high_byte = row.note == 0 ? 0 : (uint16_t)(row.instrument * 2 - 2) >> 8;
    return module->wave_tables + (((uint32_t)high_byte << 8 | row.effect_arg) << 4);
}

// Voice

void tronic_voice_reset(TronicModule* module, TronicVoice* voice, int index) {
    *voice = (TronicVoice) { 0 };
    voice->index = index;
    voice->max_volume = TRONIC_FULL_VOLUME;
    voice->wave_table = module->wave_tables;
    voice->instrument_number = -1;
    voice->paula.lc = tronic_silence(module);
    voice->paula.len_words = SAMPLE_REPEAT_INITIAL_WORDS;
}

// A note starts: the instrument's state and, for a sample, AUDxLC/AUDxLEN of its first part.
void tronic_start_note(TronicModule* module, TronicVoice* voice, unsigned instrument_number) {
    TronicInstrument instrument;
    if (!tronic_instrument(module, instrument_number, &instrument)) {
        module->overrun = true;
        voice->has_instrument = false;
        return;
    }
    voice->has_instrument = true;
    voice->instrument_number = (int)instrument_number;
    voice->instrument = instrument;

    voice->sample_repeat_pending = instrument.is_sample;
    if (instrument.is_sample) {
        voice->paula.lc = sample_pointer(module, sample_start(module, instrument.param), instrument.length_words);
        voice->paula.len_words = instrument.length_words;
    }

    voice->waveform_ticks = 0;
    voice->waveform_pos = 0;
    voice->volume = 0;
    voice->envelope_pos = 0;
    voice->envelope_hold = 0;
    voice->wave_table_pos = 0;
    voice->vibrato_down = false;
    voice->vibrato = 0;
    const TronicVibratoStage first_stage = tronic_vibrato_stage(module, &instrument, 0);
    voice->vibrato_ticks = first_stage.ticks;
    voice->vibrato_stage = 0;
    voice->vibrato_hold = first_stage.hold;
}

// The tick after a sample started: AUDxLC/AUDxLEN get the repeat, and the sample's first word is cleared so that a
// repeat of the first word is silent.
void tronic_sample_repeat(TronicModule* module, TronicVoice* voice, bool clear_first_word) {
    const TronicInstrument* instrument = &voice->instrument;
    voice->sample_repeat_pending = false;

    const int64_t start = sample_start(module, instrument->param);
    voice->paula.len_words = instrument->loop_words;
    if (start < 0 || (uint64_t)start + 2 > module->size) {
        module->overrun = true;
        return;
    }
    if (clear_first_word) {
        module->data[start] = 0;
        module->data[start + 1] = 0;
    }
    voice->paula.lc = sample_pointer(module, start + (int16_t)instrument->loop_offset, instrument->loop_words);
}

// A waveform instrument: every `param` + 1 ticks AUDxLC moves to the next waveform of the list. 0xFF in the list jumps
// to the position in the next byte; a jump onto another 0xFF ends the list for good.
void tronic_run_waveform_list(TronicModule* module, TronicVoice* voice) {
    if (voice->waveform_ticks != 0) {
        voice->waveform_ticks--;
        return;
    }
    const TronicInstrument* instrument = &voice->instrument;
    voice->waveform_ticks = instrument->param;

    uint8_t pos = voice->waveform_pos;
    uint8_t waveform = tronic_instrument_byte(module, instrument, TronicField_Steps + pos);
    if (waveform == WAVEFORM_JUMP) {
        pos = tronic_instrument_byte(module, instrument, TronicField_Steps + pos + 1u);
        waveform = tronic_instrument_byte(module, instrument, TronicField_Steps + pos);
        if (waveform == WAVEFORM_JUMP) {
            return;
        }
    }
    const int16_t waveform_offset = (int16_t)tronic_word(module, module->waveform_index + 2u * waveform);
    voice->paula.lc = sample_pointer(module, (int64_t)module->waveforms + waveform_offset, instrument->length_words);
    voice->paula.len_words = instrument->length_words;
    voice->waveform_pos = (uint8_t)(pos + 1);
}

// The vibrato: the pitch offset moves by `step` every tick, up and down in turns; the stages run one after the other.
void tronic_run_vibrato(TronicModule* module, TronicVoice* voice) {
    const TronicInstrument* instrument = &voice->instrument;
    const TronicVibratoStage stage = tronic_vibrato_stage(module, instrument, voice->vibrato_stage);
    if (voice->vibrato_down) {
        voice->vibrato = (uint16_t)(voice->vibrato - stage.step);
    } else {
        voice->vibrato = (uint16_t)(voice->vibrato + stage.step);
    }

    if (--voice->vibrato_ticks == 0) {
        voice->vibrato_ticks = stage.ticks;
        voice->vibrato_down = !voice->vibrato_down;
    }

    if (voice->vibrato_hold != 0) {
        voice->vibrato_hold--;
        return;
    }
    // The hold of the next stage; at the last stage that is the byte behind the stages, as in the replayer.
    voice->vibrato_hold
        = tronic_vibrato_stage(module, instrument, (uint8_t)(voice->vibrato_stage + VIBRATO_STAGE_BYTES)).hold;
    voice->vibrato_stage = (uint8_t)(voice->vibrato_stage + VIBRATO_STAGE_BYTES);
    if (voice->vibrato_stage == VIBRATO_END_STAGE) {
        voice->vibrato_stage = VIBRATO_LAST_STAGE;
    }
}

// The volume envelope: stages of (step, target, hold). The level moves to the target by `step` per tick, then the stage
// holds. The last stage repeats; a falling stage does not wrap to it.
void tronic_run_envelope(TronicModule* module, TronicVoice* voice) {
    if (voice->envelope_hold != 0) {
        voice->envelope_hold--;
        return;
    }
    const TronicInstrument* instrument = &voice->instrument;
    const TronicEnvelopeStage stage = tronic_envelope_stage(module, instrument, voice->envelope_pos);

    bool reached;
    if ((int16_t)(stage.target - voice->volume) >= 0) {
        voice->volume = (int16_t)(voice->volume + stage.step);
        reached = (int16_t)(stage.target - voice->volume) < 0;
        if (reached) {
            voice->volume = stage.target;
            voice->envelope_pos += ENVELOPE_STAGE_BYTES;
            if (voice->envelope_pos == ENVELOPE_END_POS) {
                voice->envelope_pos = ENVELOPE_LAST_POS;
            }
        }
    } else {
        voice->volume = (int16_t)(voice->volume - stage.step);
        reached = (int16_t)(stage.target - voice->volume) >= 0;
        if (reached) {
            voice->volume = stage.target;
            voice->envelope_pos += ENVELOPE_STAGE_BYTES;
        }
    }
    if (reached) {
        voice->envelope_hold = stage.hold;
    }
}

// Effect 5: the period moves towards the note's by `slide_to_note` per tick.
static void run_slide_to_note(TronicVoice* voice) {
    if (voice->slide_to_note == 0) {
        return;
    }
    const int16_t target = (int16_t)voice->target_period;
    if (target - (int16_t)voice->period >= 0) {
        voice->period = (uint16_t)(voice->period + voice->slide_to_note);
        if (target - (int16_t)voice->period < 0) {
            voice->period = (uint16_t)target;
        }
    } else {
        voice->period = (uint16_t)(voice->period - voice->slide_to_note);
        if (target - (int16_t)voice->period >= 0) {
            voice->period = (uint16_t)target;
        }
    }
}

// The next note offset of the voice's wave table (an arpeggio); 0x80 restarts the table.
static uint8_t next_wave_table_byte(TronicModule* module, TronicVoice* voice) {
    for (;;) {
        const uint8_t value = tronic_byte(module, (uint64_t)voice->wave_table + voice->wave_table_pos);
        if (value != TRONIC_WAVE_TABLE_RESTART) {
            voice->wave_table_pos = (uint16_t)((voice->wave_table_pos + 1) & (TRONIC_WAVE_TABLE_BYTES - 1));
            return value;
        }
        if (voice->wave_table_pos == 0) {
            // The table starts with the restart byte: the replayer never leaves the loop.
            module->overrun = true;
            return 0;
        }
        voice->wave_table_pos = 0;
    }
}

// AUDxPER and AUDxVOL from the voice's state.
static void write_registers(TronicModule* module, TronicSong* song, TronicVoice* voice) {
    const uint8_t wave_note = next_wave_table_byte(module, voice);
    if (voice->slide_to_note == 0) {
        voice->period = tronic_period((uint8_t)(wave_note + voice->note + voice->transpose));
    }
    voice->vibrato = (uint16_t)(voice->vibrato - voice->instrument.detune);
    voice->paula.period = (uint16_t)(voice->period + voice->vibrato);

    uint8_t volume = ((uint16_t)voice->volume >> 2) & 0x3F;
    // The replayer's compares test the sign of the byte difference.
    if (((volume - voice->max_volume) & 0x80) == 0) {
        volume = voice->max_volume;
    }
    if (((volume - song->max_volume) & 0x80) == 0) {
        volume = song->max_volume;
    }
    voice->paula.volume = volume & 0x7F;
}

static void run_instrument(TronicModule* module, TronicSong* song, TronicVoice* voice) {
    if (!voice->instrument.is_sample) {
        tronic_run_waveform_list(module, voice);
    }
    tronic_run_vibrato(module, voice);
    tronic_run_envelope(module, voice);
    run_slide_to_note(voice);
    write_registers(module, song, voice);
}

// The pitch slides (3, 4) are not modelled; the open-time check refuses songs that use them.
static void apply_effect(TronicModule* module, TronicSong* song, TronicVoice* voice, TronicCell row) {
    switch (row.effect) {
        case TronicEffect_None:
        case TronicEffect_Nothing:
            break;
        case TronicEffect_Speed:
            song->speed = row.effect_arg;
            break;
        case TronicEffect_SlideToNote:
            voice->slide_to_note = row.effect_arg;
            break;
        case TronicEffect_VoiceVolume:
            voice->max_volume = row.effect_arg;
            break;
        case TronicEffect_MasterVolume:
            song->max_volume = row.effect_arg;
            break;
        case TronicEffect_WaveTable:
            voice->wave_table = tronic_effect_wave_table(module, row);
            break;
        default:
            module->overrun = true;
            break;
    }
}

// A row tick: the next row of the pattern, and at the pattern's start the next step of the sequence.
static void read_row(TronicModule* module, TronicSong* song, TronicVoice* voice) {
    if (voice->row_offset == 0) {
        const TronicStep step = tronic_step(module, voice->index, voice->sequence_pos);
        voice->shown_order = voice->sequence_pos / 2;
        voice->pattern = step.pattern;
        voice->transpose = step.transpose;
        voice->sequence_pos = (uint16_t)(voice->sequence_pos + 2);
        const TronicSequence* sequence = &module->sequence[voice->index];
        if (voice->sequence_pos >= sequence->length) {
            voice->sequence_pos = sequence->restart;
            song->looped = true;
        }
    }

    const unsigned row_index = voice->row_offset / TRONIC_ROW_BYTES;
    voice->shown_row = (int)row_index;
    const TronicCell row = tronic_pattern_row(module, voice->pattern, row_index);
    if (row.note != 0) {
        nuade_paula_channel_dma_off(&voice->paula);
        voice->note = row.note;
        voice->target_period = tronic_period((uint8_t)(row.note + voice->transpose));
        tronic_start_note(module, voice, row.instrument);
    }
    apply_effect(module, song, voice, row);
    voice->row_offset = (uint16_t)((voice->row_offset + TRONIC_ROW_BYTES) & (TRONIC_PATTERN_BYTES - 1));
}

static void run_voice(TronicModule* module, TronicSong* song, TronicVoice* voice, bool row_tick) {
    if (voice->sample_repeat_pending) {
        tronic_sample_repeat(module, voice, true);
    }
    if (row_tick) {
        read_row(module, song, voice);
    }
    // A voice that has not started a note plays the replayer's silent dummy sample.
    if (voice->has_instrument) {
        run_instrument(module, song, voice);
        nuade_paula_channel_dma_on(&voice->paula);
    }
}

// Player

TronicPlayer* tronic_create(const uint8_t* data, size_t size) {
    TronicPlayer* player = (TronicPlayer*)calloc(1, sizeof(TronicPlayer));
    if (player == nullptr) {
        return nullptr;
    }
    if (!tronic_module_parse(&player->module, data, size) || !tronic_module_prove(&player->module)) {
        tronic_module_free(&player->module);
        free(player);
        return nullptr;
    }
    return player;
}

void tronic_destroy(TronicPlayer* player) {
    if (player == nullptr) {
        return;
    }
    tronic_module_free(&player->module);
    free(player);
}

bool tronic_probe(const uint8_t* data, size_t size) {
    TronicModule module;
    const bool ok = tronic_module_parse(&module, data, size) && tronic_module_prove(&module);
    tronic_module_free(&module);
    return ok;
}

bool tronic_start(TronicPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    TronicSong* song = &player->song;
    *song = (TronicSong) { .speed = TRONIC_DEFAULT_SPEED, .max_volume = TRONIC_FULL_VOLUME };
    for (int i = 0; i < TRONIC_VOICES; i++) {
        tronic_voice_reset(&player->module, &song->voices[i], i);
        nuade_voice_stop(voices, (uint32_t)i);
        nuade_voice_set_volume(voices, (uint32_t)i, 0.0f);
        // Paula channels 0 and 3 are left, 1 and 2 right
        nuade_voice_set_pan(voices, (uint32_t)i, (i == 0 || i == 3) ? -1.0f : 1.0f);
    }
    return true;
}

TronicTickResult tronic_tick(TronicPlayer* player, NuadeVoices* voices) {
    TronicModule* module = &player->module;
    TronicSong* song = &player->song;
    song->looped = false;

    // The counter reaches zero on a row tick; the first tick only loads it.
    song->counter = (int8_t)(song->counter - 1);
    if (song->counter < 0) {
        song->counter = (int8_t)song->speed;
    }
    const bool row_tick = song->counter == 0;
    for (int i = 0; i < TRONIC_VOICES; i++) {
        run_voice(module, song, &song->voices[i], row_tick);
    }
    for (int i = 0; i < TRONIC_VOICES; i++) {
        nuade_paula_channel_sync(&song->voices[i].paula, (uint32_t)i, voices);
    }
    return (TronicTickResult) { .looped = song->looped };
}

TronicPosition tronic_position(const TronicPlayer* player) {
    const TronicVoice* voice = &player->song.voices[0];
    return (TronicPosition) { .order = voice->shown_order, .row = voice->shown_row, .speed = player->song.speed + 1 };
}

int tronic_order_count(const TronicPlayer* player) {
    return player->module.sequence[0].length / 2;
}

TronicVoiceState tronic_voice_state(const TronicPlayer* player, int voice) {
    const TronicVoice* v = &player->song.voices[voice];
    return (TronicVoiceState) { .period = v->has_instrument ? v->target_period : -1,
                                .instrument = v->has_instrument ? v->instrument_number - 1 : -1 };
}

TronicCell tronic_pattern_cell(const TronicPlayer* player, int order, int voice, int row) {
    // The readers flag reads outside the file in the module they get; a copy keeps the player's own const.
    TronicModule module = player->module;
    if (order < 0 || (unsigned)order * 2 >= module.sequence[voice].length || row < 0 || row >= TRONIC_ROWS) {
        return (TronicCell) { 0 };
    }
    return tronic_pattern_row(&module, tronic_step(&module, voice, (unsigned)order * 2).pattern, (unsigned)row);
}
