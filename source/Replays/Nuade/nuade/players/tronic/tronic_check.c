// SPDX-License-Identifier: MIT
// Tronic: the open-time check. The replayer reads module memory with indexes the song controls (notes, instrument
// numbers, effect parameters, list positions); this proves that a song stays inside what the port models.

#include "tronic_internal.h"

// Work units (envelope ticks, list steps, pattern rows) the check may use for a whole module.
#define CHECK_BUDGET 20000000u

// A wave table (16 bytes) is selected by a word, so a song could use thousands; the check handles a few per voice.
#define MAX_TABLES_PER_VOICE 64

// An envelope stage lasts at most 255 hold ticks plus 255 level steps; a voice whose envelope stays on one stage for
// more ticks than that never leaves it.
#define ENVELOPE_SETTLE_TICKS 1024

typedef struct VoiceUse {
    bool note[256];
    bool transpose[256];
    bool table_byte[256];
    uint32_t tables[MAX_TABLES_PER_VOICE];
    unsigned table_count;
} VoiceUse;

static bool use_table(VoiceUse* use, uint32_t table) {
    for (unsigned i = 0; i < use->table_count; i++) {
        if (use->tables[i] == table) {
            return true;
        }
    }
    if (use->table_count == MAX_TABLES_PER_VOICE) {
        return false;
    }
    use->tables[use->table_count++] = table;
    return true;
}

// One row. `first_note_seen` is true once the voice has started a note on an earlier row.
static bool check_row(TronicModule* module, TronicCell row, VoiceUse* use, bool instrument_used[256],
                      bool* first_note_seen) {
    // The pitch slides (3, 4) and the effects past 8 are not modelled.
    if (row.effect > TronicEffect_Last || row.effect == TronicEffect_SlideUp || row.effect == TronicEffect_SlideDown) {
        return false;
    }
    // Until its first note a voice runs on the replayer's dummy instrument, whose period the slide to the note would
    // start from; that period is not modelled, so no slide may be set up to and including the first note's row.
    if (row.effect == TronicEffect_SlideToNote && row.effect_arg != 0 && !*first_note_seen) {
        return false;
    }
    if (row.note != 0) {
        TronicInstrument instrument;
        if (!tronic_instrument(module, row.instrument, &instrument)) {
            return false;
        }
        instrument_used[row.instrument] = true;
        use->note[row.note] = true;
        *first_note_seen = true;
    }
    if (row.effect == TronicEffect_WaveTable) {
        return use_table(use, tronic_effect_wave_table(module, row));
    }
    return true;
}

// Every step, pattern and row of a voice's sequence.
static bool check_voice_sequence(TronicModule* module, int voice, VoiceUse* use, bool instrument_used[256],
                                 uint64_t* budget) {
    bool first_note_seen = false;
    use->table_count = 0;
    if (!use_table(use, module->wave_tables)) {
        return false;
    }
    for (unsigned pos = 0; pos < module->sequence[voice].length; pos += 2) {
        const TronicStep step = tronic_step(module, voice, pos);
        if (*budget < TRONIC_ROWS) {
            return false;
        }
        *budget -= TRONIC_ROWS;
        if ((uint64_t)step.pattern + TRONIC_PATTERN_BYTES > (uint64_t)module->patterns + module->pattern_bytes) {
            return false;
        }
        use->transpose[step.transpose] = true;
        for (unsigned row = 0; row < TRONIC_ROWS; row++) {
            if (!check_row(module, tronic_pattern_row(module, step.pattern, row), use, instrument_used,
                           &first_note_seen)) {
                return false;
            }
        }
    }
    return true;
}

// The bytes of the wave tables the voice can read: a table is read from its start up to its first 0x80.
static bool collect_table_bytes(TronicModule* module, VoiceUse* use) {
    for (unsigned t = 0; t < use->table_count; t++) {
        if ((uint64_t)use->tables[t] + TRONIC_WAVE_TABLE_BYTES > module->size) {
            return false;
        }
        for (unsigned i = 0; i < TRONIC_WAVE_TABLE_BYTES; i++) {
            const uint8_t value = module->data[use->tables[t] + i];
            if (value == TRONIC_WAVE_TABLE_RESTART) {
                if (i == 0) {
                    return false; // the replayer never leaves the loop
                }
                break;
            }
            use->table_byte[value] = true;
        }
    }
    return true;
}

// The period table has TRONIC_PERIOD_COUNT entries; note + transpose (+ table byte) must index inside it for every
// combination the voice can have, as the note of an earlier pattern plays on under the transpose of a later one.
static bool check_period_indexes(const VoiceUse* use) {
    for (unsigned note = 1; note < 256; note++) {
        if (!use->note[note]) {
            continue;
        }
        for (unsigned transpose = 0; transpose < 256; transpose++) {
            if (!use->transpose[transpose]) {
                continue;
            }
            const unsigned sum = (note + transpose) & 0xFF;
            if (sum >= TRONIC_PERIOD_COUNT) {
                return false;
            }
            for (unsigned table_byte = 0; table_byte < 256; table_byte++) {
                if (use->table_byte[table_byte] && ((sum + table_byte) & 0xFF) >= TRONIC_PERIOD_COUNT) {
                    return false;
                }
            }
        }
    }
    return true;
}

// Runs the parts of an instrument that read the record with indexes the record itself controls, on a scratch voice.
static bool check_instrument(TronicModule* module, unsigned number, uint64_t* budget) {
    TronicVoice voice;
    tronic_voice_reset(module, &voice, 0);
    tronic_start_note(module, &voice, number);
    if (module->overrun || !voice.has_instrument) {
        return false;
    }

    if (voice.instrument.is_sample) {
        if (voice.instrument.param >= TRONIC_SAMPLE_COUNT) {
            return false;
        }
        tronic_sample_repeat(module, &voice, false);
    } else {
        // The list position is the whole state: walk it until a position comes back.
        bool seen[256] = { false };
        while (!seen[voice.waveform_pos]) {
            seen[voice.waveform_pos] = true;
            voice.waveform_ticks = 0;
            tronic_run_waveform_list(module, &voice);
        }
    }

    uint16_t stage = voice.envelope_pos;
    unsigned settled = 0;
    while (settled < ENVELOPE_SETTLE_TICKS && !module->overrun) {
        if (*budget == 0) {
            return false;
        }
        (*budget)--;
        tronic_run_envelope(module, &voice);
        // The replayer adds the position as a signed word.
        if (voice.envelope_pos >= 0x8000) {
            return false;
        }
        settled = voice.envelope_pos == stage ? settled + 1 : 0;
        stage = voice.envelope_pos;
    }
    return !module->overrun;
}

bool tronic_module_prove(TronicModule* module) {
    module->overrun = false;
    uint64_t budget = CHECK_BUDGET;
    bool instrument_used[256] = { false };

    for (int voice = 0; voice < TRONIC_VOICES; voice++) {
        VoiceUse use = { 0 };
        if (!check_voice_sequence(module, voice, &use, instrument_used, &budget) || !collect_table_bytes(module, &use)
            || !check_period_indexes(&use) || module->overrun) {
            return false;
        }
    }
    for (unsigned number = 1; number < 256; number++) {
        if (instrument_used[number] && !check_instrument(module, number, &budget)) {
            return false;
        }
    }
    return !module->overrun;
}
