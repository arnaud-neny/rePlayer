// SPDX-License-Identifier: MIT
// Synth Pack, the word layout (see sp_words.h).

#include "sp_words.h"

// Sample set
#define SP_WORD_HEADER_SIZE 0x21 // the fields the replayer reads
#define SP_WORD_WAVE_OFFSET 0x22
#define SP_WORD_VOLUME_LFO_WAVES 12 // the tables, then the pitch tables that follow them in memory
#define SP_WORD_LFO_PHASE_STEP 8
#define SP_WORD_LFO_PHASE_MASK 0x3E

// The arpeggio steps through a table of this many ticks.
#define SP_WORD_MAX_SPEED 12

// The subsongs the eagleplayer's position table holds.
#define SP_WORD_MAX_SUBSONGS 7

// Note blocks
#define SP_WORD_BLOCK_SIZE 0x180
#define SP_WORD_NO_NOTE 0xFF

// The song with the subsongs of one game starts them at these positions.
static const uint8_t sp_word_subsong_positions[SP_WORD_MAX_SUBSONGS] = { 0, 1, 2, 3, 5, 9, 12 };

// Instruments

typedef struct SpWordInstrument {
    SpWave wave;
    int8_t note_offset;
    int8_t tuning;
    uint16_t sweep_speed; // 0: no sweep
    SpSlide slide;
    SpEnvelope envelope;
    SpLfoSettings volume_lfo;
    SpLfoSettings pitch_lfo;
} SpWordInstrument;

typedef struct SpWordEvent {
    uint8_t note;
    uint8_t volume;
    uint8_t instrument;
    uint8_t param;
} SpWordEvent;

// The instruments the table holds: it ends where the first instrument starts.

static unsigned sp_word_instrument_count(const SpMemory* set) {
    return sp_u16(set, 0) / 2u;
}

// The header of instrument `index`; false if the table has no such entry or the header lies outside the set.

static bool sp_word_instrument(const SpMemory* set, unsigned index, SpWordInstrument* instrument) {
    const bool listed = index < SP_INSTRUMENTS && index < sp_word_instrument_count(set);
    const uint32_t offset = listed ? sp_u16(set, 2u * index) : 0;
    *instrument = (SpWordInstrument) { .wave = sp_wave_read(set, offset, SP_WORD_WAVE_OFFSET),
                                       .note_offset = sp_s8(set, offset),
                                       .tuning = sp_s8(set, offset + 1),
                                       .sweep_speed = sp_u16(set, offset + 8),
                                       .slide = { .start = (int16_t)sp_u16(set, offset + 0x0A),
                                                  .step = (int16_t)sp_u16(set, offset + 0x0C),
                                                  .target = (int16_t)sp_u16(set, offset + 0x0E),
                                                  .release_step = (int16_t)sp_u16(set, offset + 0x10) },
                                       .envelope = { .attack_level = sp_u8(set, offset + 0x12),
                                                     .attack_rate = sp_u8(set, offset + 0x13),
                                                     .decay_rate = sp_u8(set, offset + 0x14),
                                                     .sustain_level = sp_u8(set, offset + 0x15),
                                                     .release_rate = sp_u8(set, offset + 0x16) },
                                       .volume_lfo = sp_lfo_settings(set, offset + 0x17),
                                       .pitch_lfo = sp_lfo_settings(set, offset + 0x1C) };
    return listed && sp_memory_has(set, offset, SP_WORD_HEADER_SIZE);
}

static SpWordEvent sp_word_event(const SpMemory* song, uint32_t at) {
    return (SpWordEvent) { .note = sp_u8(song, at),
                           .volume = sp_u8(song, at + 1),
                           .instrument = sp_u8(song, at + 2),
                           .param = sp_u8(song, at + 5) };
}

static uint32_t sp_word_event_offset(uint8_t block, unsigned row) {
    return SP_NOTES_OFFSET + (uint32_t)SP_WORD_BLOCK_SIZE * block + (uint32_t)SP_EVENT_SIZE * row;
}

// The LFO tables. The volume LFO's wave numbers run on past its three tables, into three empty ones and then the pitch
// tables (as words, the value in the low byte).

static uint16_t sp_word_volume_lfo_value(const SpWordLfo* lfo) {
    const unsigned at = lfo->phase / 2;
    if (!lfo->table || lfo->wave >= SP_WORD_VOLUME_LFO_WAVES) {
        return 0;
    }
    if (lfo->wave < SP_WORD_VOLUME_LFO_TABLES) {
        return sp_word_volume_lfo_tables[lfo->wave][at];
    }
    if (lfo->wave >= SP_WORD_VOLUME_LFO_WAVES / 2) {
        return (uint8_t)sp_word_pitch_lfo_tables[lfo->wave - SP_WORD_VOLUME_LFO_WAVES / 2][at];
    }
    return 0;
}

static int8_t sp_word_pitch_lfo_value(const SpWordLfo* lfo) {
    return lfo->table && lfo->wave < SP_WORD_PITCH_LFO_TABLES ? sp_word_pitch_lfo_tables[lfo->wave][lfo->phase / 2] : 0;
}

static SpWordLfo sp_word_lfo_start(const SpLfoSettings* settings) {
    return (SpWordLfo) { .delay = (uint16_t)(settings->delay << 8),
                         .counter = settings->speed,
                         .phase = 0,
                         .wave = settings->wave,
                         .table = true };
}

// An LFO runs once its delay has passed (a delay of d counts d * 4 ticks), and with `after_release` only after the
// key was released. Its phase moves on whenever the counter runs out. Returns whether the LFO's value is to be used
// this tick.

static bool sp_word_lfo_advance(SpWordLfo* lfo, const SpLfoSettings* settings, bool released) {
    if (settings->after_release && !released) {
        return false;
    }
    if ((lfo->delay >> 8) != 0) {
        lfo->delay = (uint16_t)(lfo->delay - SP_LFO_DELAY_UNIT);
        return false;
    }
    lfo->counter = (uint16_t)(lfo->counter - SP_LFO_DELAY_UNIT);
    if ((int16_t)lfo->counter < 0) {
        lfo->counter = settings->speed;
        if (settings->speed == 0) {
            return false;
        }
        lfo->phase = (uint16_t)((lfo->phase + SP_WORD_LFO_PHASE_STEP) & SP_WORD_LFO_PHASE_MASK);
    }
    return true;
}

// Validation

// The ticks per row of a pattern line: the low nibble of its first cell, 6 for 0.
static unsigned sp_word_speed(const SpMemory* song, unsigned pattern) {
    const unsigned speed = sp_cell(song, pattern, 0).tempo & 0x0F;
    return speed != 0 ? speed : SP_DEFAULT_SPEED;
}

// One event that selects an instrument; a volume starts a note on it.

static bool sp_word_check_event(SpCheck* check, const SpWordEvent* event, const SpCell* cell) {
    const unsigned index = sp_event_instrument(event->instrument, cell->instrument_shift);
    const unsigned high = event->param >> 4;
    const unsigned low = event->param & 0x0F;
    const unsigned reach = high > low ? high : low;
    if (index >= SP_INSTRUMENTS || (event->volume != 0 && !sp_check_note_reach(cell->note_shift, reach))) {
        return false;
    }
    if (check->set == nullptr) {
        return true;
    }
    SpWordInstrument instrument;
    if (!sp_word_instrument(check->set, index, &instrument)) {
        return false;
    }
    check->named[index] = true;
    if (event->volume == 0) {
        return true;
    }
    const int note = sp_event_note(event->note, instrument.note_offset, cell->note_shift);
    return note >= 0 && (unsigned)note + reach < SP_PERIOD_COUNT;
}

// The reads a column makes over the rows of one position.

static bool sp_word_check_column(SpCheck* check, const SpPosition* entry, unsigned column) {
    if (((entry->voices >> column) & 1) == 0) {
        return true;
    }
    const SpCell cell = sp_cell(check->song, entry->pattern, column);
    const unsigned rows = entry->rows != 0 ? entry->rows : 1;
    for (unsigned row = 0; row < rows; row++) {
        const uint32_t at = sp_word_event_offset(cell.notes, row);
        if (!sp_check_spend(check, 1) || !sp_memory_has(check->song, at, SP_EVENT_SIZE)) {
            return false;
        }
        const SpWordEvent event = sp_word_event(check->song, at);
        if (event.instrument != 0 && !sp_word_check_event(check, &event, &cell)) {
            return false;
        }
    }
    return true;
}

static bool sp_word_check_position(SpCheck* check, const SpPosition* entry) {
    if (sp_word_speed(check->song, entry->pattern) > SP_WORD_MAX_SPEED) {
        return false;
    }
    for (unsigned column = 0; column < SP_VOICES; column++) {
        if (!sp_word_check_column(check, entry, column)) {
            return false;
        }
    }
    return true;
}

// An instrument a voice can run on: its header and the LFO tables it reads, its waveform, and every address the sweep
// can put in AUDxLC for the longest repeat any instrument can have left in AUDxLEN. Instrument 0 that no event selects
// only has its header, LFO waves and sweep checked: no note starts on it, so its waveform is never played.

static bool sp_word_check_instrument(const SpMemory* set, unsigned index, bool named, uint16_t longest_repeat) {
    SpWordInstrument instrument;
    if (!sp_word_instrument(set, index, &instrument) || instrument.volume_lfo.wave >= SP_WORD_VOLUME_LFO_WAVES
        || instrument.pitch_lfo.wave >= SP_WORD_PITCH_LFO_TABLES || (named && !sp_wave_in_set(set, &instrument.wave))) {
        return false;
    }
    return !sp_wave_scanned(&instrument.wave) || instrument.sweep_speed == 0
           || sp_wave_scan_in_set(set, &instrument.wave, SP_MORPH_POSITIONS - 1, longest_repeat);
}

// The instruments the events selected, plus instrument 0 that the voices start on. A voice keeps its AUDxLEN when an
// event only switches its instrument, so the sweep of one instrument can run with the repeat length of another.

static bool sp_word_check_instruments(const SpCheck* check) {
    uint16_t longest_repeat = 1;
    for (unsigned index = 0; index < SP_INSTRUMENTS; index++) {
        SpWordInstrument instrument;
        if (check->named[index] && sp_word_instrument(check->set, index, &instrument)) {
            const uint16_t repeat = sp_wave_repeat_length(&instrument.wave);
            longest_repeat = repeat > longest_repeat ? repeat : longest_repeat;
        }
    }
    for (unsigned index = 0; index < SP_INSTRUMENTS; index++) {
        if ((check->named[index] || index == 0)
            && !sp_word_check_instrument(check->set, index, check->named[index], longest_repeat)) {
            return false;
        }
    }
    return true;
}

static unsigned sp_word_start_position(const SpMemory* song, int subsong) {
    (void)song;
    return sp_word_subsong_positions[subsong % SP_WORD_MAX_SUBSONGS];
}

const SpRules sp_word_rules = { .max_subsongs = SP_WORD_MAX_SUBSONGS,
                                .loop_point = 0,
                                .start_position = sp_word_start_position,
                                .check_position = sp_word_check_position,
                                .check_instruments = sp_word_check_instruments };

// The arpeggio: after the row's tick, the ticks of a row cycle through the note, the parameter's high nibble and its
// low nibble.

static void sp_word_arpeggio(SpWordState* state, SpWordVoice* voice) {
    unsigned add = 0;
    switch (state->sequencer.tick % 3) {
        case 1:
            add = voice->base.arp_param >> 4;
            break;
        case 2:
            add = voice->base.arp_param & 0x0F;
            break;
        default:
            break;
    }
    if (state->sequencer.tick % 3 != 0 && add == 0) {
        return;
    }
    SpWordInstrument instrument;
    sp_word_instrument(state->set, voice->base.instrument, &instrument);
    voice->base.base_period = sp_period(voice->base.arp_note + add, instrument.tuning);
}

// A note starts: the LFOs, the sweep and the slide start over.

static void sp_word_note_on(SpWordVoice* voice, const SpWordInstrument* instrument, unsigned note) {
    sp_voice_note_on(&voice->base, note, instrument->tuning, &instrument->slide);
    voice->volume_lfo = sp_word_lfo_start(&instrument->volume_lfo);
    voice->pitch_lfo = sp_word_lfo_start(&instrument->pitch_lfo);
    voice->sweep_counter = instrument->sweep_speed != 0 ? (int16_t)(instrument->sweep_speed - 1) : 0;
    voice->sweep_position = 0;
    voice->sweep_down = false;
}

// One column of a row.

static void sp_word_column_row(SpWordState* state, unsigned column) {
    SpSequencer* sequencer = &state->sequencer;
    SpWordVoice* voice = &state->voice[column];
    if (((sequencer->entry.voices >> column) & 1) == 0) {
        if (sequencer->row == 0) {
            sp_voice_release(&voice->base);
        }
        return;
    }
    const SpCell cell = sp_cell(state->song, sequencer->entry.pattern, column);
    const SpWordEvent event = sp_word_event(state->song, sp_word_event_offset(cell.notes, sequencer->row));
    if (event.instrument == 0) {
        return;
    }
    const unsigned index = sp_event_instrument(event.instrument, cell.instrument_shift);
    voice->base.instrument = (uint8_t)index;
    if (event.volume == 0) {
        if (event.note == SP_WORD_NO_NOTE) {
            sp_voice_release(&voice->base);
        }
        return;
    }
    SpWordInstrument instrument;
    sp_word_instrument(state->set, index, &instrument);
    voice->base.volume = (uint16_t)(int16_t)(int8_t)event.volume;
    voice->base.arp_param = event.param;
    sp_word_note_on(voice, &instrument, (unsigned)sp_event_note(event.note, instrument.note_offset, cell.note_shift));
}

static void sp_word_row(SpWordState* state) {
    SpSequencer* sequencer = &state->sequencer;
    sequencer->entry = sp_position(state->song, sequencer->position);
    sequencer->speed = sp_word_speed(state->song, sequencer->entry.pattern);
    for (unsigned column = 0; column < SP_VOICES; column++) {
        sp_word_column_row(state, column);
    }
    sp_sequencer_row_end(sequencer, &state->events);
    if (state->events.stopped) {
        for (int i = 0; i < SP_VOICES; i++) {
            sp_voice_release(&state->voice[i].base);
        }
    }
}

static void sp_word_sequencer_tick(SpWordState* state) {
    if (!state->sequencer.running) {
        return;
    }
    if (sp_sequencer_tick(&state->sequencer)) {
        sp_word_row(state);
        return;
    }
    for (int i = 0; i < SP_VOICES; i++) {
        sp_word_arpeggio(state, &state->voice[i]);
    }
}

// The voice step

static void sp_word_update_level(SpWordVoice* voice, const SpWordInstrument* instrument) {
    const int16_t level = voice->base.adsr.level;
    voice->base.level = level;
    if (!sp_word_lfo_advance(&voice->volume_lfo, &instrument->volume_lfo, voice->base.released) || level == 0) {
        return;
    }
    const uint16_t dip = sp_lsr16(sp_word_volume_lfo_value(&voice->volume_lfo), instrument->volume_lfo.depth);
    const int16_t lowered = (int16_t)(level - dip);
    voice->base.level = lowered < 0 ? 0 : lowered;
}

static void sp_word_update_period(SpWordVoice* voice, const SpWordInstrument* instrument) {
    int16_t bend = 0;
    if (sp_word_lfo_advance(&voice->pitch_lfo, &instrument->pitch_lfo, voice->base.released)) {
        bend = sp_asr16(sp_word_pitch_lfo_value(&voice->pitch_lfo), instrument->pitch_lfo.depth);
    }
    voice->base.period = (uint16_t)(voice->base.base_period + bend);
}

// A short waveform is swept: AUDxLC moves through the waveform's memory, and a sweep that goes down also raises the
// pitch.

static void sp_word_sweep(SpWordState* state, SpWordVoice* voice, const SpWordInstrument* instrument,
                          SpChannel* channel) {
    if (!sp_wave_scanned(&instrument->wave) || instrument->sweep_speed == 0) {
        return;
    }
    voice->sweep_counter = (int16_t)(voice->sweep_counter - 1);
    if (voice->sweep_counter < 0) {
        voice->sweep_counter = (int16_t)(instrument->sweep_speed - 1);
        if (voice->sweep_down) {
            voice->base.period = (uint16_t)(voice->base.period - 1);
            voice->sweep_position = (uint16_t)(voice->sweep_position - 1);
            if ((int16_t)voice->sweep_position < 0) {
                voice->sweep_position = 0;
                voice->sweep_down = false;
            }
        } else {
            voice->sweep_position = (uint16_t)(voice->sweep_position + 1);
            if ((int16_t)voice->sweep_position > SP_MORPH_POSITIONS - 1) {
                voice->sweep_position = SP_MORPH_POSITIONS - 1;
                voice->sweep_down = true;
            }
        }
    }
    sp_channel_set_lc(channel, state->set, sp_wave_scan_offset(&instrument->wave, voice->sweep_position));
}

static uint8_t sp_word_volume(const SpVoice* voice) {
    const uint32_t product = (uint32_t)(uint16_t)voice->level * voice->volume;
    return sp_paula_volume((product & 0xFFFF) >> 6);
}

static void sp_word_voice_tick(SpWordState* state, SpHardware* hardware, unsigned channel_index) {
    SpWordVoice* voice = &state->voice[channel_index];
    SpChannel* channel = &hardware->channel[channel_index];
    SpWordInstrument instrument;
    sp_word_instrument(state->set, voice->base.instrument, &instrument);

    sp_adsr_step(&voice->base.adsr, &instrument.envelope, voice->base.released);
    sp_word_update_level(voice, &instrument);
    sp_word_update_period(voice, &instrument);
    sp_voice_slide(&voice->base, &instrument.slide);
    sp_word_sweep(state, voice, &instrument, channel);
    channel->paula.volume = sp_word_volume(&voice->base);
    channel->paula.period = voice->base.period;
    sp_voice_restart(&voice->base, &instrument.wave, channel, state->set);
}

void sp_word_start(SpWordState* state, const SpMemory* song, const SpMemory* set, SpHardware* hardware, int subsong) {
    *state = (SpWordState) { .song = song, .set = set };
    sp_sequencer_start(&state->sequencer, sp_word_start_position(song, subsong), sp_word_rules.loop_point);
    for (int i = 0; i < SP_VOICES; i++) {
        state->voice[i].base = sp_voice_new(false);
    }
    sp_hardware_reset(hardware);
}

void sp_word_tick(SpWordState* state, SpHardware* hardware) {
    state->events = (SpSongEvents) { 0 };
    sp_word_sequencer_tick(state);
    sp_hardware_begin_tick(hardware);
    for (unsigned channel = 0; channel < SP_VOICES; channel++) {
        sp_word_voice_tick(state, hardware, channel);
    }
    sp_hardware_end_tick(hardware);
}

bool sp_word_silent(const SpWordState* state) {
    for (int i = 0; i < SP_VOICES; i++) {
        if (!sp_voice_silent(&state->voice[i].base)) {
            return false;
        }
    }
    return true;
}

const SpVoice* sp_word_channel_voice(const SpWordState* state, unsigned channel) {
    return &state->voice[channel % SP_VOICES].base;
}
