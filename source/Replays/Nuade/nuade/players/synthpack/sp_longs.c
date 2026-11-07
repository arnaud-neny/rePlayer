// SPDX-License-Identifier: MIT
// Synth Pack, the long layout (see sp_longs.h).

#include "sp_longs.h"
#include "nuade_utils.h"

// Sample set
#define SP_LONG_SET_TABLE_OFFSET 0x0C
#define SP_LONG_HEADER_SIZE 0x24 // the fields the replayer reads
#define SP_LONG_WAVE_OFFSET 0x30

// Both master volumes of the replayer stay at their start value.
#define SP_LONG_MASTER_VOLUME 0x40

// Columns of the sequencer drive these Paula channels.
static const uint8_t sp_long_slot_channel[SP_VOICES] = { 0, 1, 3, 2 };

// Instruments

typedef struct SpLongInstrument {
    SpWave wave;
    int8_t note_offset;
    int8_t tuning;
    SpSlide slide;
    SpEnvelope envelope;
    SpLfoSettings volume_lfo;
    SpLfoSettings pitch_lfo;
    SpLfoSettings morph;
} SpLongInstrument;

typedef struct SpLongEvent {
    uint8_t note;
    uint8_t volume; // 0 releases the voice
    uint8_t instrument;
    uint8_t effect;
    uint8_t row;
    uint8_t param;
} SpLongEvent;

// The header of instrument `index`; false if the table has no such entry or the header lies outside the set.

static bool sp_long_instrument(const SpMemory* set, unsigned index, SpLongInstrument* instrument) {
    const uint32_t offset = index < SP_INSTRUMENTS ? sp_u32(set, SP_LONG_SET_TABLE_OFFSET + 4u * index) : 0;
    *instrument = (SpLongInstrument) { .wave = sp_wave_read(set, offset, SP_LONG_WAVE_OFFSET),
                                       .note_offset = sp_s8(set, offset),
                                       .tuning = sp_s8(set, offset + 1),
                                       .slide = { .start = (int16_t)sp_u16(set, offset + 8),
                                                  .step = (int16_t)sp_u16(set, offset + 0x0A),
                                                  .target = (int16_t)sp_u16(set, offset + 0x0C),
                                                  .release_step = (int16_t)sp_u16(set, offset + 0x0E) },
                                       .envelope = { .attack_level = sp_u8(set, offset + 0x10),
                                                     .attack_rate = sp_u8(set, offset + 0x11),
                                                     .decay_rate = sp_u8(set, offset + 0x12),
                                                     .sustain_level = sp_u8(set, offset + 0x13),
                                                     .release_rate = sp_u8(set, offset + 0x14) },
                                       .volume_lfo = sp_lfo_settings(set, offset + 0x15),
                                       .pitch_lfo = sp_lfo_settings(set, offset + 0x1A),
                                       .morph = sp_lfo_settings(set, offset + 0x1F) };
    return index < SP_INSTRUMENTS && sp_memory_has(set, offset, SP_LONG_HEADER_SIZE);
}

static SpLongEvent sp_long_event(const SpMemory* song, uint32_t at) {
    return (SpLongEvent) { .note = sp_u8(song, at),
                           .volume = sp_u8(song, at + 1),
                           .instrument = sp_u8(song, at + 2),
                           .effect = sp_u8(song, at + 3),
                           .row = sp_u8(song, at + 4),
                           .param = sp_u8(song, at + 5) };
}

// Offset of the first event of note list `notes`.

static uint32_t sp_long_note_list(const SpMemory* song, uint8_t notes) {
    return SP_NOTES_OFFSET + sp_u16(song, SP_NOTES_OFFSET + 2u * notes);
}

// The LFO tables

static int8_t sp_long_lfo_value(const SpLongLfo* lfo) {
    return lfo->table && lfo->wave < SP_LONG_LFO_TABLES
               ? sp_long_lfo_tables[lfo->wave][(lfo->phase >> 8) & (SP_LONG_LFO_LENGTH - 1)]
               : 0;
}

static SpLongLfo sp_long_lfo_start(const SpLfoSettings* settings) {
    return (SpLongLfo) { .delay = (uint16_t)(settings->delay << 8),
                         .speed = (uint16_t)(settings->speed << 4),
                         .phase = 0,
                         .wave = settings->wave,
                         .table = true };
}

// An LFO runs once its delay has passed (a delay of d counts d * 4 ticks), and with `after_release` only after the
// key was released. Returns whether the LFO's value is to be used this tick.

static bool sp_long_lfo_advance(SpLongLfo* lfo, const SpLfoSettings* settings, bool released) {
    if (settings->after_release && !released) {
        return false;
    }
    if ((lfo->delay >> 8) != 0) {
        lfo->delay = (uint16_t)(lfo->delay - SP_LFO_DELAY_UNIT);
        return false;
    }
    lfo->phase = (uint16_t)(lfo->phase + lfo->speed);
    return settings->depth != SP_LFO_OFF;
}

// Validation

// The highest period index an arpeggio adds to the note.
static unsigned sp_long_arpeggio_reach(const SpLongEvent* event) {
    const unsigned high = event->param >> 4;
    const unsigned low = (event->effect & 7) != 0 ? event->param & 0x0F : 0;
    return high > low ? high : low;
}

// One event that starts a note.

static bool sp_long_check_event(SpCheck* check, const SpLongEvent* event, const SpCell* cell) {
    const unsigned index = sp_event_instrument(event->instrument, cell->instrument_shift);
    const unsigned reach = sp_long_arpeggio_reach(event);
    if (index >= SP_INSTRUMENTS || !sp_check_note_reach(cell->note_shift, reach)) {
        return false;
    }
    if (check->set == nullptr) {
        return true;
    }
    SpLongInstrument instrument;
    if (!sp_long_instrument(check->set, index, &instrument)) {
        return false;
    }
    const int note = sp_event_note(event->note, instrument.note_offset, cell->note_shift);
    check->named[index] = true;
    return note >= 0 && (unsigned)note + reach < SP_PERIOD_COUNT;
}

// The reads a column makes over the rows of one position.

static bool sp_long_check_column(SpCheck* check, const SpPosition* entry, unsigned column) {
    const SpMemory* song = check->song;
    const SpCell cell = sp_cell(song, entry->pattern, column);
    if (!sp_memory_has(song, SP_NOTES_OFFSET + 2u * cell.notes, 2)) {
        return false;
    }
    uint32_t at = sp_long_note_list(song, cell.notes);
    const unsigned rows = entry->rows != 0 ? entry->rows : 1;
    const bool plays = ((entry->voices >> column) & 1) != 0;
    for (unsigned row = 0; row < rows; row++) {
        if (!sp_check_spend(check, 1) || !sp_memory_has(song, at, SP_EVENT_SIZE)) {
            return false;
        }
        const SpLongEvent event = sp_long_event(song, at);
        if (event.row != (uint8_t)row) {
            continue;
        }
        if (plays && event.volume != 0 && !sp_long_check_event(check, &event, &cell)) {
            return false;
        }
        at += SP_EVENT_SIZE;
    }
    return true;
}

static bool sp_long_check_position(SpCheck* check, const SpPosition* entry) {
    for (unsigned column = 0; column < SP_VOICES; column++) {
        if (!sp_long_check_column(check, entry, column)) {
            return false;
        }
    }
    return true;
}

// An instrument a voice runs on: its header, and the LFO tables it reads. `sounding` for one a note starts on: its
// waveform and every address the morph puts in AUDxLC, for the longest repeat any instrument can have left in AUDxLEN
// (a note's start writes the morph's address before it writes the new length).

static bool sp_long_check_instrument(const SpMemory* set, unsigned index, bool sounding, uint16_t longest_repeat) {
    SpLongInstrument instrument;
    if (!sp_long_instrument(set, index, &instrument)) {
        return false;
    }
    const SpLfoSettings* lfos[] = { &instrument.volume_lfo, &instrument.pitch_lfo, &instrument.morph };
    for (size_t i = 0; i < nuade_sizeof_array(lfos); i++) {
        if (lfos[i]->depth != SP_LFO_OFF && lfos[i]->wave >= SP_LONG_LFO_TABLES) {
            return false;
        }
    }
    if (!sounding) {
        return true;
    }
    const SpWave* wave = &instrument.wave;
    if (!sp_wave_in_set(set, wave)) {
        return false;
    }
    const unsigned last
        = instrument.morph.depth == SP_LFO_OFF ? 0 : sp_lsr16(SP_MORPH_POSITIONS - 1, instrument.morph.depth);
    return !sp_wave_scanned(wave) || sp_wave_scan_in_set(set, wave, last, longest_repeat);
}

// The voices start on instrument 0 with no morph table: a morphing one would read the Amiga's low memory, and one
// that does not morph still points AUDxLC at its waveform.

static bool sp_long_check_idle_instrument(const SpMemory* set) {
    SpLongInstrument instrument;
    if (!sp_long_check_instrument(set, 0, false, 1) || !sp_long_instrument(set, 0, &instrument)) {
        return false;
    }
    if (!sp_wave_scanned(&instrument.wave)) {
        return true;
    }
    return instrument.morph.depth == SP_LFO_OFF && sp_wave_scan_in_set(set, &instrument.wave, 0, 1);
}

static bool sp_long_check_instruments(const SpCheck* check) {
    if (!sp_long_check_idle_instrument(check->set)) {
        return false;
    }
    uint16_t longest_repeat = 1;
    for (unsigned index = 0; index < SP_INSTRUMENTS; index++) {
        SpLongInstrument instrument;
        if (check->named[index] && sp_long_instrument(check->set, index, &instrument)) {
            const uint16_t repeat = sp_wave_repeat_length(&instrument.wave);
            longest_repeat = repeat > longest_repeat ? repeat : longest_repeat;
        }
    }
    for (unsigned index = 0; index < SP_INSTRUMENTS; index++) {
        if (check->named[index] && !sp_long_check_instrument(check->set, index, true, longest_repeat)) {
            return false;
        }
    }
    return true;
}

// The position a subsong starts at: the n-th position that has a start mark, else the first.

static unsigned sp_long_start_position(const SpMemory* song, int subsong) {
    int found = 0;
    for (unsigned position = 0; position < SP_POSITION_COUNT; position++) {
        if ((sp_position(song, position).voices & SP_SUBSONG_MARK) != 0) {
            if (found == subsong) {
                return position;
            }
            found++;
        }
    }
    return 0;
}

const SpRules sp_long_rules = { .max_subsongs = SP_POSITION_COUNT,
                                .loop_point = SP_NO_LOOP_POINT,
                                .start_position = sp_long_start_position,
                                .check_position = sp_long_check_position,
                                .check_instruments = sp_long_check_instruments };

// The arpeggio of an effect cycles through steps that add nothing, the parameter's high nibble or its low nibble.

typedef enum SpLongArpStep {
    SpLongArp_Base,
    SpLongArp_High,
    SpLongArp_Low,
    SpLongArp_End,
} SpLongArpStep;

#define SP_LONG_ARP_PATTERNS 8
#define SP_LONG_ARP_STEPS 13

static const SpLongArpStep sp_long_arpeggios[SP_LONG_ARP_PATTERNS][SP_LONG_ARP_STEPS] = {
    { SpLongArp_Base, SpLongArp_High, SpLongArp_End },
    { SpLongArp_Base, SpLongArp_High, SpLongArp_Base, SpLongArp_Low, SpLongArp_End },
    { SpLongArp_Base, SpLongArp_High, SpLongArp_Low, SpLongArp_End },
    { SpLongArp_Base, SpLongArp_High, SpLongArp_Low, SpLongArp_High, SpLongArp_End },
    { SpLongArp_Base, SpLongArp_Base, SpLongArp_High, SpLongArp_High, SpLongArp_Low, SpLongArp_Low, SpLongArp_End },
    { SpLongArp_Base, SpLongArp_Base, SpLongArp_High, SpLongArp_High, SpLongArp_Low, SpLongArp_Low, SpLongArp_High,
      SpLongArp_High, SpLongArp_End },
    { SpLongArp_Base, SpLongArp_Base, SpLongArp_Base, SpLongArp_High, SpLongArp_High, SpLongArp_High, SpLongArp_Low,
      SpLongArp_Low, SpLongArp_Low, SpLongArp_End },
    { SpLongArp_Base, SpLongArp_Base, SpLongArp_Base, SpLongArp_High, SpLongArp_High, SpLongArp_High, SpLongArp_Low,
      SpLongArp_Low, SpLongArp_Low, SpLongArp_High, SpLongArp_High, SpLongArp_High, SpLongArp_End },
};

static void sp_long_arpeggio(SpLongState* state, SpLongVoice* voice) {
    const SpLongArpStep* steps = sp_long_arpeggios[voice->effect & (SP_LONG_ARP_PATTERNS - 1)];
    SpLongArpStep step = steps[voice->arp_step++];
    if (step == SpLongArp_End) {
        voice->arp_step = 0;
        step = steps[0];
    }
    unsigned add = 0;
    if (step == SpLongArp_Low) {
        add = voice->base.arp_param & 0x0F;
    } else if (step == SpLongArp_High) {
        add = voice->base.arp_param >> 4;
    }
    if (step != SpLongArp_Base && add == 0) {
        return;
    }
    SpLongInstrument instrument;
    sp_long_instrument(state->set, voice->base.instrument, &instrument);
    voice->base.base_period = sp_period(voice->base.arp_note + add, instrument.tuning);
}

// The sequencer

// A note starts: the LFOs start over with their tables.
static void sp_long_note_on(SpLongVoice* voice, const SpLongInstrument* instrument, unsigned note) {
    sp_voice_note_on(&voice->base, note, instrument->tuning, &instrument->slide);
    voice->volume_lfo = sp_long_lfo_start(&instrument->volume_lfo);
    voice->pitch_lfo = sp_long_lfo_start(&instrument->pitch_lfo);
    voice->morph = sp_long_lfo_start(&instrument->morph);
    voice->arp_step = 0;
}

// One column of a row.

static void sp_long_column_row(SpLongState* state, unsigned column) {
    SpSequencer* sequencer = &state->sequencer;
    SpLongVoice* voice = &state->voice[column];
    const SpMemory* song = state->song;
    const SpCell cell = sp_cell(song, sequencer->entry.pattern, column);
    if (sequencer->row == 0) {
        voice->events = sp_long_note_list(song, cell.notes);
    }
    const SpLongEvent event = sp_long_event(song, voice->events);
    const bool due = (uint8_t)sequencer->row == event.row;
    if (due) {
        voice->events += SP_EVENT_SIZE;
    }
    if (((sequencer->entry.voices >> column) & 1) == 0) {
        if (sequencer->row == 0) {
            sp_voice_release(&voice->base);
        }
        return;
    }
    if (!due) {
        return;
    }
    if (event.volume == 0) {
        sp_voice_release(&voice->base);
        return;
    }
    const unsigned index = sp_event_instrument(event.instrument, cell.instrument_shift);
    SpLongInstrument instrument;
    sp_long_instrument(state->set, index, &instrument);
    voice->base.instrument = (uint8_t)index;
    voice->base.volume = (uint16_t)((uint16_t)((uint16_t)(int16_t)(int8_t)event.volume * SP_LONG_MASTER_VOLUME) >> 6);
    voice->effect = event.effect;
    voice->base.arp_param = event.param;
    sp_long_note_on(voice, &instrument, (unsigned)sp_event_note(event.note, instrument.note_offset, cell.note_shift));
}

static void sp_long_row(SpLongState* state) {
    SpSequencer* sequencer = &state->sequencer;
    sequencer->entry = sp_position(state->song, sequencer->position);
    sequencer->speed = sp_cell(state->song, sequencer->entry.pattern, 0).tempo;
    for (unsigned column = 0; column < SP_VOICES; column++) {
        sp_long_column_row(state, column);
    }
    sp_sequencer_row_end(sequencer, &state->events);
    if (state->events.stopped) {
        for (int i = 0; i < SP_VOICES; i++) {
            sp_voice_release(&state->voice[i].base);
        }
    }
}

static void sp_long_sequencer_tick(SpLongState* state) {
    if (!state->sequencer.running) {
        return;
    }
    const bool row_due = sp_sequencer_tick(&state->sequencer);
    for (int i = 0; i < SP_VOICES; i++) {
        sp_long_arpeggio(state, &state->voice[i]);
    }
    if (row_due) {
        sp_long_row(state);
    }
}

// The voice step

static void sp_long_update_level(SpLongVoice* voice, const SpLongInstrument* instrument) {
    const int16_t level = voice->base.adsr.level;
    voice->base.level = level;
    const bool active = sp_long_lfo_advance(&voice->volume_lfo, &instrument->volume_lfo, voice->base.released);
    if (!active || level == 0) {
        return;
    }
    const uint8_t shaped = (uint8_t)((uint8_t)sp_long_lfo_value(&voice->volume_lfo) ^ 0x80) >> 1;
    const int dip = sp_lsr16(shaped, instrument->volume_lfo.depth);
    const int16_t lowered = (int16_t)(level - dip);
    voice->base.level = lowered < 0 ? 0 : lowered;
}

static void sp_long_update_period(SpLongVoice* voice, const SpLongInstrument* instrument) {
    int16_t bend = 0;
    if (sp_long_lfo_advance(&voice->pitch_lfo, &instrument->pitch_lfo, voice->base.released)) {
        bend = sp_asr16(sp_long_lfo_value(&voice->pitch_lfo), instrument->pitch_lfo.depth);
    }
    voice->base.period = (uint16_t)(voice->base.base_period + bend);
}

// A short waveform is morphed: AUDxLC moves through the waveform's memory each tick.

static void sp_long_morph(SpLongState* state, SpLongVoice* voice, const SpLongInstrument* instrument,
                          SpChannel* channel) {
    if (!sp_wave_scanned(&instrument->wave)) {
        return;
    }
    unsigned position = 0;
    if (sp_long_lfo_advance(&voice->morph, &instrument->morph, voice->base.released)) {
        const uint8_t shaped = (uint8_t)~(uint8_t)((uint8_t)sp_long_lfo_value(&voice->morph) ^ 0x80) >> 1;
        position = sp_lsr16(shaped, instrument->morph.depth);
    }
    sp_channel_set_lc(channel, state->set, sp_wave_scan_offset(&instrument->wave, position));
}

static uint8_t sp_long_volume(const SpVoice* voice) {
    const uint32_t scaled = (uint32_t)(uint16_t)voice->level * voice->volume;
    const uint32_t product = (scaled & 0xFFFF) * SP_LONG_MASTER_VOLUME;
    return sp_paula_volume(((product << 4) >> 16) & 0xFFFF);
}

static void sp_long_voice_tick(SpLongState* state, SpHardware* hardware, unsigned slot) {
    SpLongVoice* voice = &state->voice[slot];
    SpChannel* channel = &hardware->channel[sp_long_slot_channel[slot]];
    SpLongInstrument instrument;
    sp_long_instrument(state->set, voice->base.instrument, &instrument);

    sp_adsr_step(&voice->base.adsr, &instrument.envelope, voice->base.released);
    sp_long_update_level(voice, &instrument);
    sp_long_update_period(voice, &instrument);
    sp_long_morph(state, voice, &instrument, channel);
    sp_voice_slide(&voice->base, &instrument.slide);
    channel->paula.volume = sp_long_volume(&voice->base);
    channel->paula.period = voice->base.period;
    sp_voice_restart(&voice->base, &instrument.wave, channel, state->set);
}

void sp_long_start(SpLongState* state, const SpMemory* song, const SpMemory* set, SpHardware* hardware, int subsong) {
    *state = (SpLongState) { .song = song, .set = set };
    sp_sequencer_start(&state->sequencer, sp_long_start_position(song, subsong), sp_long_rules.loop_point);
    for (int i = 0; i < SP_VOICES; i++) {
        state->voice[i].base = sp_voice_new(true);
    }
    sp_hardware_reset(hardware);
}

void sp_long_tick(SpLongState* state, SpHardware* hardware) {
    state->events = (SpSongEvents) { 0 };
    sp_hardware_begin_tick(hardware);
    for (unsigned slot = 0; slot < SP_VOICES; slot++) {
        sp_long_voice_tick(state, hardware, slot);
    }
    sp_hardware_end_tick(hardware);
    sp_long_sequencer_tick(state);
}

bool sp_long_silent(const SpLongState* state) {
    for (int i = 0; i < SP_VOICES; i++) {
        if (!sp_voice_silent(&state->voice[i].base)) {
            return false;
        }
    }
    return true;
}

const SpVoice* sp_long_channel_voice(const SpLongState* state, unsigned channel) {
    for (unsigned slot = 0; slot < SP_VOICES; slot++) {
        if (sp_long_slot_channel[slot] == channel) {
            return &state->voice[slot].base;
        }
    }
    return &state->voice[0].base;
}
