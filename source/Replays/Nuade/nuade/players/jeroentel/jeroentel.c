// SPDX-License-Identifier: MIT
// Jeroen Tel player: the replayer. One tick steps the four voices from the last to the first: a voice whose note has
// run out reads its track list and pattern, then every voice runs its sample sweep, vibrato or slide, volume envelope
// and arpeggio list; finally the registers are written and the DMA of every channel is switched on.

#include "jt_internal.h"
#include <string.h>

// Track list entries
#define TRACK_END 0xFF  // the song starts over
#define TRACK_STOP 0xFE // the replayer stops
#define TRACK_TRANSPOSE_BIAS 0xC0

// Pattern entries: the first byte of a note holds the step count and the kind
#define PATTERN_END 0xFF
#define STEP_COUNT_MASK 0x1F
#define LEGATO_BIT 0x20
#define KIND_MASK 0xC0
#define KIND_PAUSE_MIN 0xC0    // above this the entry is a pause
#define KIND_ARPEGGIO_MIN 0xE0 // above this an arpeggio list number follows
#define INSTRUMENT_ARPEGGIO_BIT 0x80
#define INSTRUMENT_ARPEGGIO_MASK 0x7F
#define PATTERN_ENTRY_MAX 0x7F // track entries up to this are pattern numbers

#define COUNTER_NOTE_DUE 0xFF // the counter value at which due voices read their next note
#define COUNTER_WRAPPED 0xFE
#define DURATION_NONE 0xFF
#define MAX_VOLUME 0x40
#define NOTE_C0_OFFSET 12 // the period table starts at C-1
#define NOTE_BIT_FREE_VIBRATO 0x80
#define NOTE_MASK 0x7F
#define PERIOD_INDEX_LIMIT 0x48 // byte offsets into the period table from here on use the last entry
#define PERIOD_INDEX_LAST 0x46
#define SWEEP_LAST 0x20 // the sweep takes the sample start through 33 positions
#define FETCH_LIMIT 512 // track entries and arpeggio bytes one voice reads in one step
#define ARPEGGIO_ABSOLUTE_BIT 0x80
#define ARPEGGIO_HOLD 0xFE
#define ARPEGGIO_LOOP 0xFF

// The kinds of note in the top bits of a pattern entry (pauses, 0xC0 and above, are told apart before)
typedef enum PatternKind {
    PatternKind_Note = 0x00,
    PatternKind_Slide = 0x40,      // a speed byte and the target note precede the note
    PatternKind_Instrument = 0x80, // an instrument number precedes the note
} PatternKind;

typedef enum NoteStart {
    NoteStart_None,    // no new note: the voice goes on
    NoteStart_Started, // a new note, with or without a restart of the channel
    NoteStart_Stop,    // the track list holds the stop mark
} NoteStart;

static uint8_t byte_at(const JtModule* m, uint32_t base, uint32_t index) {
    return base != JT_NO_OFFSET ? jt_u8(m, base + index) : 0;
}

// The period of a byte offset into the table; offsets beyond the table use its last entry.

static uint16_t period_at(const JtModule* m, unsigned byte_offset) {
    return jt_period(m, (byte_offset >= PERIOD_INDEX_LIMIT ? PERIOD_INDEX_LAST : byte_offset) / 2);
}

// The stop mark: the replayer switches all channels off and plays nothing more.

static void stop_song(JtPlayer* p) {
    p->playing = false;
    for (int i = 0; i < JT_VOICES; i++) {
        nuade_paula_channel_dma_off(&p->paula[i]);
    }
}

// The eagleplayer's SongEnd call: all four voices go back to their first track entry.

static void end_song(JtPlayer* p) {
    p->looped = true;
    for (int i = 0; i < JT_VOICES; i++) {
        p->voices[i].track_pos = p->module.first_track_pos[i];
    }
}

// The next track entry that is a pattern number; the transpositions before it change the voice's transpose. False when
// the replayer stopped.

static bool next_pattern(JtPlayer* p, JtVoice* v, uint8_t* pattern_number) {
    const JtModule* m = &p->module;
    for (int guard = 0; guard < FETCH_LIMIT; guard++) {
        uint8_t entry = byte_at(m, v->track, v->track_pos);
        if (entry == TRACK_END) {
            end_song(p);
            entry = byte_at(m, v->track, v->track_pos);
        } else if (entry == TRACK_STOP) {
            stop_song(p);
            return false;
        }
        if (entry <= PATTERN_ENTRY_MAX) {
            *pattern_number = entry;
            return true;
        }
        v->transpose = (uint8_t)(entry - TRACK_TRANSPOSE_BIAS);
        v->track_pos++;
    }
    stop_song(p);
    return false;
}

// The channel starts the new note's sample.

static void restart_channel(JtPlayer* p, int voice) {
    JtVoice* v = &p->voices[voice];
    NuadePaulaChannel* paula = &p->paula[voice];
    const JtInstrument* ins = &v->instrument;
    nuade_paula_channel_dma_off(paula);
    v->volume = 0;
    v->stage = JtEnvelopeStage_Attack;
    v->ticks = 0;
    v->sweep_pos = 0;
    v->arpeggio_pos = 1;
    v->vibrato_count = (int16_t)(ins->vibrato_length >> 1);
    v->vibrato_up = true;
    paula->lc = jt_sample_at(&p->module, ins->sample);
    paula->len_words = ins->length_words;
    v->gate = true;
}

// Reads the voice's next note from its pattern and track list.

static NoteStart read_note(JtPlayer* p, int voice) {
    const JtModule* m = &p->module;
    JtVoice* v = &p->voices[voice];
    uint8_t pattern_number = 0;
    if (!next_pattern(p, v, &pattern_number)) {
        return NoteStart_Stop;
    }
    const uint32_t pattern = jt_table_entry(m, m->pattern_table, (uint32_t)pattern_number * 4);
    uint8_t at = v->pattern_pos;
    const uint8_t head = byte_at(m, pattern, at);
    bool legato = (head & LEGATO_BIT) != 0;
    uint8_t kind = head;
    v->duration = head & STEP_COUNT_MASK;
    if (kind >= KIND_ARPEGGIO_MIN) {
        v->arpeggio_list = byte_at(m, pattern, at + 1u);
        at++;
        kind = byte_at(m, pattern, at);
        legato = false;
    }
    if (kind >= KIND_PAUSE_MIN) {
        v->pattern_pos++;
        if (byte_at(m, pattern, at + 1u) == PATTERN_END) {
            v->pattern_pos = 0;
            v->track_pos++;
        }
        return NoteStart_None;
    }
    switch ((PatternKind)(kind & KIND_MASK)) {
        case PatternKind_Instrument: {
            at++;
            const uint8_t number = byte_at(m, pattern, at);
            v->instrument = jt_read_instrument(m, jt_table_entry(m, m->instrument_table, (uint32_t)number * 4));
            v->arpeggio_list = v->instrument.arpeggio_list;
            v->display_instrument = number;
            const uint8_t next = byte_at(m, pattern, at + 1u);
            if ((next & INSTRUMENT_ARPEGGIO_BIT) != 0) {
                at++;
                v->arpeggio_list = next & INSTRUMENT_ARPEGGIO_MASK;
                legato = false;
            }
            v->slide_speed = 0;
            break;
        }
        case PatternKind_Slide:
            v->slide_speed = byte_at(m, pattern, at + 1u);
            at += 2;
            v->slide_target = (uint8_t)(byte_at(m, pattern, at) + v->transpose);
            break;
        case PatternKind_Note:
            v->slide_speed = 0;
            break;
    }
    at++;
    v->note = (uint8_t)(byte_at(m, pattern, at) + v->transpose);
    v->display_note = v->note + NOTE_C0_OFFSET;
    if (byte_at(m, pattern, at + 1u) == PATTERN_END) {
        v->pattern_pos = 0;
        v->track_pos++;
    } else {
        v->pattern_pos = (uint8_t)(at + 1);
    }
    if (legato) {
        v->stage = JtEnvelopeStage_Sustain;
        v->volume = v->instrument.sustain_volume;
        return NoteStart_Started;
    }
    restart_channel(p, voice);
    return NoteStart_Started;
}

// The sample start of an instrument with a sweep moves one byte on every second tick (the parity of voice 0's tick
// counter, which every voice reads).

static void sweep_sample(JtPlayer* p, int voice) {
    JtVoice* v = &p->voices[voice];
    if (v->instrument.sweep == 0 || (p->voices[0].ticks & 1) == 0) {
        return;
    }
    p->paula[voice].lc = jt_sample_at(&p->module, v->instrument.sample + v->sweep_pos);
    v->sweep_pos++;
    if (v->sweep_pos > SWEEP_LAST) {
        v->sweep_pos = 0;
    }
}

static void apply_vibrato(JtVoice* v) {
    const JtInstrument* ins = &v->instrument;
    v->period = (uint16_t)(v->vibrato_up ? v->period + ins->vibrato_step : v->period - ins->vibrato_step);
    v->vibrato_count--;
    if (v->vibrato_count < 0) {
        v->vibrato_up = !v->vibrato_up;
        v->vibrato_count = (int16_t)ins->vibrato_length;
    }
}

// The period moves towards the period of the target note by the slide speed; it stops there.

static void apply_slide(const JtModule* m, JtVoice* v) {
    const uint8_t target_note = v->slide_target & NOTE_MASK;
    const int16_t target = (int16_t)period_at(m, (unsigned)(target_note << 1));
    if ((int8_t)target_note >= (int8_t)v->note) {
        v->period = (uint16_t)(v->period - v->slide_speed);
        if (target >= (int16_t)v->period) {
            v->period = (uint16_t)target;
            v->slide_speed = 0;
        }
    } else {
        v->period = (uint16_t)(v->period + v->slide_speed);
        if (target < (int16_t)v->period) {
            v->period = (uint16_t)target;
            v->slide_speed = 0;
        }
    }
}

// What a voice does in a tick that starts no note: sweep, the release of the gate, the loop of the sample after the
// first tick, then the slide or the vibrato.

static void run_note(JtPlayer* p, int voice) {
    JtVoice* v = &p->voices[voice];
    NuadePaulaChannel* paula = &p->paula[voice];
    const JtInstrument* ins = &v->instrument;
    sweep_sample(p, voice);
    if (ins->release_tick != 0 && (int16_t)ins->release_tick <= (int16_t)v->ticks) {
        v->stage = JtEnvelopeStage_Sustain;
        v->gate = false;
    }
    v->ticks++;
    if (v->ticks == 1) {
        paula->lc = ins->loop != 0 ? jt_sample_at(&p->module, ins->loop) : nullptr;
        paula->len_words = ins->loop_length_words;
    }
    if (v->slide_speed != 0) {
        apply_slide(&p->module, v);
    } else if ((v->note & NOTE_BIT_FREE_VIBRATO) != 0 || (ins->vibrato_delay != 0 && v->ticks > ins->vibrato_delay)) {
        apply_vibrato(v);
    }
}

// The volume envelope: attack to the maximum, decay to the sustain level, or the release after the gate closed. The
// envelope moves on every second tick.

static void run_envelope(JtVoice* v) {
    const JtInstrument* ins = &v->instrument;
    if (!v->gate) {
        v->volume = (uint16_t)(v->volume - ins->release_rate);
        if ((int16_t)v->volume < 0) {
            v->volume = 0;
        }
        return;
    }
    if ((v->ticks & 1) != 0) {
        return;
    }
    switch (v->stage) {
        case JtEnvelopeStage_Attack:
            v->volume = (uint16_t)(v->volume + ins->attack_rate);
            if (v->volume > ins->max_volume) {
                v->volume = ins->max_volume;
                v->stage = JtEnvelopeStage_Decay;
            }
            break;
        case JtEnvelopeStage_Decay:
            v->volume = (uint16_t)(v->volume - ins->decay_rate);
            if ((int16_t)v->volume < (int16_t)ins->sustain_volume || (int16_t)v->volume < 0) {
                v->volume = ins->sustain_volume;
                v->stage = JtEnvelopeStage_Sustain;
            }
            break;
        case JtEnvelopeStage_Sustain:
            break;
    }
}

// The arpeggio list gives the period: a note offset (relative to the voice's note, or absolute with bit 7) per tick.
// 0xFF ends the list (its first byte is the position to loop to), 0xFE holds the period.

static void run_arpeggio(const JtModule* m, JtVoice* v) {
    const uint32_t list = jt_table_entry(m, m->arpeggio_table, v->arpeggio_list);
    uint8_t at = v->arpeggio_pos;
    for (int guard = 0; guard < FETCH_LIMIT; guard++) {
        uint8_t entry = byte_at(m, list, at);
        if (entry == ARPEGGIO_LOOP) {
            at = byte_at(m, list, 0);
            continue;
        }
        if (entry == ARPEGGIO_HOLD) {
            return;
        }
        v->arpeggio_pos = (uint8_t)(at + 1);
        if ((entry & ARPEGGIO_ABSOLUTE_BIT) == 0) {
            entry = (uint8_t)(entry + v->note);
        }
        v->period = period_at(m, (unsigned)((entry & NOTE_MASK) << 1));
        return;
    }
}

// One voice of one tick. False when the replayer stopped.

static bool step_voice(JtPlayer* p, int voice) {
    JtVoice* v = &p->voices[voice];
    NoteStart start = NoteStart_None;
    if (p->counter == COUNTER_NOTE_DUE) {
        if (v->duration == DURATION_NONE) {
            start = read_note(p, voice);
            if (start == NoteStart_Stop) {
                return false;
            }
        }
    } else if (p->counter == 0) {
        v->duration--;
        if ((int8_t)v->duration < 0) {
            v->gate = false;
        }
    }
    if (start == NoteStart_None) {
        run_note(p, voice);
    }
    run_envelope(v);
    run_arpeggio(&p->module, v);
    return true;
}

// The registers: period and volume of every voice, then its DMA on.

static void write_registers(JtPlayer* p) {
    for (int i = JT_VOICES - 1; i >= 0; i--) {
        const JtVoice* v = &p->voices[i];
        NuadePaulaChannel* paula = &p->paula[i];
        paula->period = v->period;
        paula->volume = (uint8_t)((int16_t)v->volume > MAX_VOLUME ? MAX_VOLUME : v->volume);
        nuade_paula_channel_dma_on(paula);
    }
}

bool jt_start(JtPlayer* player, int subsong, NuadeVoices* voices) {
    JtSong song;
    if (player == nullptr || voices == nullptr || subsong < 0
        || !jt_read_song(&player->module, (uint32_t)subsong, &song)) {
        return false;
    }
    player->counter = 0;
    player->speed = song.speed;
    player->playing = true;
    player->looped = false;
    memset(player->voices, 0, sizeof(player->voices));
    memset(player->paula, 0, sizeof(player->paula));
    for (int i = 0; i < JT_VOICES; i++) {
        JtVoice* v = &player->voices[i];
        v->track = song.tracks[i];
        v->stage = JtEnvelopeStage_Sustain;
        v->instrument = jt_default_instrument();
        v->display_note = -1;
        v->display_instrument = -1;
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

JtTickResult jt_tick(JtPlayer* player, NuadeVoices* voices) {
    player->looped = false;
    if (player->playing) {
        player->counter--;
        if (player->counter == COUNTER_WRAPPED) {
            player->counter = player->speed;
        }
        for (int i = JT_VOICES - 1; i >= 0; i--) {
            if (!step_voice(player, i)) {
                break;
            }
        }
        if (player->playing) {
            write_registers(player);
        }
    }
    for (int i = 0; i < JT_VOICES; i++) {
        nuade_paula_channel_sync(&player->paula[i], (uint32_t)i, voices);
    }
    return (JtTickResult) { .looped = player->looped, .ended = !player->playing };
}

JtVoiceState jt_voice_state(const JtPlayer* player, int voice) {
    if (voice < 0 || voice >= JT_VOICES) {
        return (JtVoiceState) { .note = -1, .instrument = -1 };
    }
    const JtVoice* v = &player->voices[voice];
    return (JtVoiceState) { .note = v->display_note, .instrument = v->display_instrument };
}
