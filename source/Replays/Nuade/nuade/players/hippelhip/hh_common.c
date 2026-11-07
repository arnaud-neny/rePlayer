// SPDX-License-Identifier: MIT
// Jochen Hippel ".hip" player: what the three engines share. The module's records (samples, instruments), the song
// start and the tick, the sequencer (tracks, patterns, rows), the sequence commands the engines have in common, the
// volume sequence, the period with vibrato and portamento.

#include "hh_internal.h"

#define PATTERN_END_MARKER 1
#define TRACK_STOP 0x80
#define SAMPLE_PACK_MAGIC 0x53534d50 // "SSMP"
#define SAMPLE_PACK_RECORD_SIZE 0x18
#define VSEQ_HEADER 5
#define INFO_INSTRUMENT 0x1f
#define INFO_PORTAMENTO 0x20
#define INFO_ALT_FSEQ 0x40
#define PERIOD_INDEX_MASK 0x7f
#define VIBRATO_OCTAVE_NOTES 12
#define VIBRATO_WIDENED_BELOW 48 // notes below the 4th octave

// AUDxLC before a voice plays anything: one silent word
static const int8_t k_silence[2] = { 0, 0 };

HhRegisters hh_registers(const HhModule* m, uint32_t offset, uint16_t words) {
    // Past the module the memory is clear, so an AUDxLC there reads the same zeros as the module's end, which has
    // the zero tail of the padded copy behind it
    const uint32_t even = offset & ~1u;
    const uint32_t lc = even < m->size ? even : m->size;
    return (HhRegisters) { .lc = (const int8_t*)m->data + lc, .len_words = words };
}

HhSample hh_sample(const HhModule* m, uint8_t number) {
    const uint32_t record = hhm_record(m->samples, number, HH_SAMPLE_RECORD_SIZE);
    return (HhSample) { .start = hhm_u32(m, record + 0x12),
                        .length = hhm_u16(m, record + 0x16),
                        .loop_offset = hhm_u16(m, record + 0x1a),
                        .loop_length = hhm_u16(m, record + 0x1c) };
}

// AUDxLC/AUDxLEN = the sample, the repeat registers = its loop (engines A and B)
static void load_sample(const HhModule* m, HhVoice* v, uint8_t number) {
    const HhSample sample = hh_sample(m, number);
    const uint32_t start = m->sample_data + sample.start;
    hh_write_registers(&v->paula, hh_registers(m, start, sample.length));
    v->repeat = hh_registers(m, start + sample.loop_offset, sample.loop_length);
}

HhPack hh_sample_pack(const HhModule* m, uint8_t number) {
    const uint32_t block = m->sample_data + hh_sample(m, number).start;
    const uint32_t records = block + 8;
    // the record count, then an extra size in longs; the replayer adds the latter as a signed word
    const uint32_t extra = (uint32_t)(int32_t)(int16_t)(hhm_u16(m, block + 6) << 2);
    return (HhPack) { .magic = hhm_u32(m, block) == SAMPLE_PACK_MAGIC,
                      .records = records,
                      .data = records + (uint32_t)hhm_u16(m, block + 4) * SAMPLE_PACK_RECORD_SIZE + extra };
}

HhPackEntry hh_pack_entry(const HhModule* m, const HhPack* pack, uint8_t record) {
    const uint32_t entry = pack->records + (uint32_t)record * SAMPLE_PACK_RECORD_SIZE;
    return (HhPackEntry) { .start = hhm_u32(m, entry), .end = hhm_u32(m, entry + 4) };
}

void hh_command_sample(const HhModule* m, HhVoice* v, uint8_t number) {
    v->current_sample = 0xff;
    hh_channel_restart(v);
    load_sample(m, v, number);
    hh_restart_volume(v);
    v->sweep.active = false;
    v->fseq_pos += 2;
}

void hh_command_sample_no_reset(const HhModule* m, HhVoice* v, uint8_t number) {
    load_sample(m, v, number);
    v->sweep.active = false;
    v->fseq_pos += 2;
}

void hh_command_sample_if_new(const HhModule* m, HhVoice* v, uint8_t number) {
    if (number != v->current_sample) {
        v->current_sample = number;
        hh_channel_restart(v);
        load_sample(m, v, number);
    }
    hh_restart_volume(v);
    v->sweep.active = false;
    v->fseq_pos += 2;
}

void hh_command_vibrato(const HhModule* m, HhVoice* v, uint32_t at) {
    v->vibrato_speed = hhm_u8(m, at + 1);
    v->vibrato_depth = hhm_u8(m, at + 2);
    v->fseq_pos += 3;
}

void hh_run_fseq_chain(const HhModule* m, const HhVersion* version, HhVoice* v, HhCommandFn command_fn) {
    bool count_wait = true;
    for (int guard = 0; guard < 256; guard++) {
        if (count_wait && v->fseq_wait != 0) {
            v->fseq_wait--;
            return;
        }
        count_wait = true;
        uint32_t at = v->fseq + (uint32_t)v->fseq_pos;
        uint8_t command = hhm_u8(m, at);
        if (command == HhCommand_Hold) {
            return;
        }
        if (command == HhCommand_Jump) {
            // the command found at the target is run without looking for another jump or hold
            v->fseq_pos = hhm_u8(m, at + 1) & version->engine->jump_mask;
            at = v->fseq + (uint32_t)v->fseq_pos;
            command = hhm_u8(m, at);
        }
        if (command == HhCommand_Wait) {
            v->fseq_wait = hhm_u8(m, at + 1);
            v->fseq_pos += 2;
            continue;
        }
        if (command == HhCommand_Vibrato) {
            hh_command_vibrato(m, v, at);
        } else if (command_fn(m, version, v, command, at) == HhNext_Switch) {
            count_wait = false;
            continue;
        }
        v->fseq_value = hhm_u8(m, v->fseq + (uint32_t)v->fseq_pos);
        v->fseq_pos++;
        return;
    }
}

// A volume sequence record: 5 header bytes, then the sequence
typedef struct Instrument {
    uint8_t speed;
    uint8_t fseq;
    uint8_t vibrato_speed;
    uint8_t vibrato_depth;
    uint8_t vibrato_delay;
    uint32_t sequence; // module offset
} Instrument;

static Instrument read_instrument(const HhModule* m, uint8_t number) {
    const uint32_t record = hhm_record(m->vseqs, number, HH_SEQUENCE_RECORD_SIZE);
    return (Instrument) { .speed = hhm_u8(m, record),
                          .fseq = hhm_u8(m, record + 1),
                          .vibrato_speed = hhm_u8(m, record + 2),
                          .vibrato_depth = hhm_u8(m, record + 3),
                          .vibrato_delay = hhm_u8(m, record + 4),
                          .sequence = record + VSEQ_HEADER };
}

// A position of a track, for one voice
typedef struct TrackRow {
    uint8_t pattern;
    uint8_t note_transpose;
    uint8_t instrument_transpose;
} TrackRow;

static TrackRow read_track_row(const HhModule* m, uint32_t offset) {
    return (TrackRow) { .pattern = hhm_u8(m, offset),
                        .note_transpose = hhm_u8(m, offset + 1),
                        .instrument_transpose = hhm_u8(m, offset + 2) };
}

// A row of a pattern: the note (bit 7: tie, no instrument) and the info byte
typedef struct PatternRow {
    uint8_t note;
    uint8_t info;
} PatternRow;

static PatternRow read_pattern_row(const HhModule* m, uint32_t offset) {
    return (PatternRow) { .note = hhm_u8(m, offset), .info = hhm_u8(m, offset + 1) };
}

// Voice `index` of a subsong: its column of the first and of the last track position
static void init_track(const HhModule* m, const HhSubsong* subsong, int index, HhVoice* v) {
    const uint32_t column = (uint32_t)index * 3;
    v->track = m->tracks + subsong->first * HH_TRACK_ROW_SIZE + column;
    v->track_end = m->tracks + (uint32_t)(uint16_t)(subsong->last + 1) * HH_TRACK_ROW_SIZE + column;
    v->track_pos = HH_TRACK_ROW_SIZE;
    const TrackRow row = read_track_row(m, v->track);
    v->pattern = m->patterns + (uint32_t)row.pattern * m->pattern_size;
    v->note_transpose = row.note_transpose;
    v->instrument_transpose = row.instrument_transpose;
}

// The replayer's init for one voice: its sequences, its track and first pattern, and the channel's registers
static void init_voice(const HhModule* m, const HhVersion* version, const HhSubsong* subsong, int index, HhVoice* v) {
    v->vseq = version->start_sequence;
    v->vseq_pos = 0;
    v->vseq_counter = 1;
    v->vseq_speed = 1;
    v->vseq_wait = 0;
    v->volume = 0;
    v->fseq = version->start_sequence;
    v->fseq_pos = 0;
    v->fseq_wait = 0;
    v->fseq_value = 0;
    v->vibrato_speed = 0;
    v->vibrato_depth = 0;
    v->vibrato_value = 0;
    v->vibrato_delay = 0;
    v->porta_param = 0;
    v->porta = 0;
    v->pattern_pos = 0;
    v->last_instrument = -1;
    init_track(m, subsong, index, v);
    // the channel's registers: nothing to play, DMA off
    hh_channel_stop(v);
    v->paula.lc = k_silence;
    v->paula.len_words = 1;
    v->paula.period = 0;
    v->paula.volume = 0;
}

void hh_song_start(HhSong* song, const HhModule* m, const HhVersion* version) {
    const HhEngine* engine = version->engine;
    song->counter = engine->first_step_in_first_tick ? 1 : song->subsong.speed;
    song->reload = song->subsong.speed;
    song->stopped = false;
    for (int i = 0; i < HH_VOICES; i++) {
        init_voice(m, version, &song->subsong, i, &song->voices[i]);
        engine->init_voice(&song->voices[i]);
    }
}

uint8_t hh_file_note(const HhModule* m, const HhVersion* version, int voice) {
    const HhEngine* engine = version->engine;
    return engine->record_size == 0
               ? 0
               : hhm_u8(m, version->voice_records + (uint32_t)voice * engine->record_size + engine->record_note_offset);
}

// The next position of the voice's track. A position whose instrument transpose is 0x80 stops the song; running off
// the end of the subsong wraps to its start.
static void read_track(const HhModule* m, HhSong* song, HhVoice* v) {
    uint32_t position = v->track + v->track_pos;
    if (position == v->track_end) {
        v->track_pos = 0;
        position = v->track;
        song->looped = true;
    }
    const TrackRow row = read_track_row(m, position);
    v->note_transpose = row.note_transpose;
    v->instrument_transpose = row.instrument_transpose;
    if (row.instrument_transpose == TRACK_STOP) {
        song->stopped = true;
    }
    v->pattern_pos = 0;
    v->pattern = m->patterns + (uint32_t)row.pattern * m->pattern_size;
    v->track_pos += HH_TRACK_ROW_SIZE;
}

// What a sequencer step asks of the voice
typedef struct Step {
    bool new_note; // a note that starts an instrument
    uint8_t instrument;
    bool alt_fseq; // the frequency sequence is the one named by the voice's porta_param
} Step;

// One voice's step: the next pattern row, the next track position when the pattern is used up
static Step step_voice(const HhModule* m, HhSong* song, HhVoice* v) {
    Step step = { 0 };
    if ((hhm_u8(m, v->pattern + v->pattern_pos) & 0x7f) == PATTERN_END_MARKER || v->pattern_pos == m->pattern_size) {
        read_track(m, song, v);
    }
    const uint32_t at = v->pattern + v->pattern_pos;
    const PatternRow row = read_pattern_row(m, at);
    if ((row.note & 0x7f) != 0) {
        v->porta = 0;
        v->note = row.note & 0x7f;
        // The portamento speed and the alternative frequency sequence come from the info byte before this row's,
        // which for the first row of a pattern is the last byte of the pattern.
        const uint32_t before = v->pattern_pos == 0 ? at + (uint32_t)(int32_t)(int16_t)m->pattern_size - 1 : at - 1;
        v->porta_param = hhm_u8(m, before);
        v->portamento = (row.info & INFO_PORTAMENTO) != 0;
        if (!(row.note & 0x80)) {
            step.new_note = true;
            step.instrument = (uint8_t)((row.info & INFO_INSTRUMENT) + v->instrument_transpose);
            step.alt_fseq = (row.info & INFO_ALT_FSEQ) != 0;
        }
    }
    v->pattern_pos = (uint16_t)(v->pattern_pos + 2);
    return step;
}

// A note starts an instrument: its volume sequence, vibrato and frequency sequence
static void start_instrument(const HhModule* m, const HhEngine* engine, HhVoice* v, const Step* step) {
    const Instrument instrument = read_instrument(m, step->instrument);
    v->vseq_pos = 0;
    v->vseq_counter = instrument.speed;
    v->vseq_speed = instrument.speed;
    v->vibrato_speed = instrument.vibrato_speed;
    v->vibrato_rising = false;
    v->vibrato_hold = false;
    v->vibrato_depth = instrument.vibrato_depth;
    v->vibrato_value = instrument.vibrato_depth;
    v->vibrato_delay = instrument.vibrato_delay;
    v->vseq = instrument.sequence;
    const uint8_t fseq = engine->alt_fseq_allowed && step->alt_fseq ? v->porta_param : instrument.fseq;
    v->fseq = hhm_record(m->fseqs, fseq, HH_SEQUENCE_RECORD_SIZE);
    v->fseq_pos = 0;
    v->fseq_wait = 0;
    v->vseq_wait = 0;
    v->last_instrument = step->instrument;
}

// The tick counter, and when it runs out one sequencer step of every voice
static void step_voices(const HhModule* m, const HhVersion* version, HhSong* song) {
    song->counter--;
    if (song->counter != 0) {
        return;
    }
    song->counter = song->reload;
    for (int i = 0; i < version->voices; i++) {
        HhVoice* v = &song->voices[i];
        const Step step = step_voice(m, song, v);
        if (step.new_note) {
            if (version->engine->new_note_stops_channel) {
                hh_channel_stop(v);
            }
            start_instrument(m, version->engine, v, &step);
        }
    }
}

static void run_vseq(const HhModule* m, const HhEngine* engine, HhVoice* v) {
    for (int guard = 0; guard < 256; guard++) {
        if (v->vseq_wait != 0) {
            v->vseq_wait--;
            return;
        }
        v->vseq_counter--;
        if (v->vseq_counter != 0) {
            return;
        }
        v->vseq_counter = v->vseq_speed;
        for (int inner = 0; inner < 256; inner++) {
            const uint32_t at = v->vseq + (uint32_t)v->vseq_pos;
            const uint8_t command = hhm_u8(m, at);
            if (command == HhCommand_Wait) {
                v->vseq_pos += 2;
                v->vseq_wait = hhm_u8(m, at + 1);
                break; // the wait is counted down at the top
            }
            if (command == HhCommand_Jump) {
                v->vseq_pos = (int32_t)(hhm_u8(m, at + 1) & engine->jump_mask) - VSEQ_HEADER;
                continue;
            }
            if (command == HhCommand_Hold || (engine->vseq_commands_hold && command >= HhCommand_FirstCommand)) {
                return;
            }
            v->volume = command;
            v->vseq_pos++;
            return;
        }
    }
}

// The index into the period table: the frequency sequence's value is a note offset, or with bit 7 an absolute note
static uint8_t note_index(const HhVoice* v) {
    uint8_t index = v->fseq_value;
    if (!(index & 0x80)) {
        index = (uint8_t)(index + v->note + v->note_transpose);
    }
    return index & PERIOD_INDEX_MASK;
}

// The vibrato of engine A adds the distance from the middle of the swing to the period, the same at every pitch.
static uint16_t apply_vibrato_flat(HhVoice* v, uint16_t period) {
    if (v->vibrato_delay != 0) {
        v->vibrato_delay--;
        return period;
    }
    int32_t speed = v->vibrato_speed;
    if (speed & 0x80) {
        speed &= 0x7f;
        v->vibrato_hold = !v->vibrato_hold;
    }
    const int32_t depth = v->vibrato_depth;
    const int32_t limit = depth * 2;
    int32_t value = v->vibrato_value;
    if (!v->vibrato_hold) {
        if (!v->vibrato_rising) {
            value -= speed;
            if (value < 0) {
                v->vibrato_rising = true;
                value = 0;
            }
        } else {
            value += speed;
            if (value > limit) {
                v->vibrato_rising = false;
                value = limit;
            }
        }
    }
    v->vibrato_value = (uint16_t)value;
    return (uint16_t)(period + (value - depth));
}

// Engines B and C: the swing is a byte, doubled for every octave that the note is below the 4th
static uint16_t apply_vibrato_scaled(HhVoice* v, uint16_t period, uint8_t index) {
    if (v->vibrato_delay != 0) {
        v->vibrato_delay--;
        return period;
    }
    const uint8_t limit = (uint8_t)(v->vibrato_depth * 2);
    unsigned value = v->vibrato_value;
    if (!v->vibrato_rising) {
        if (value < v->vibrato_speed) {
            v->vibrato_rising = true;
            value = 0;
        } else {
            value -= v->vibrato_speed;
        }
    } else {
        value = (uint8_t)(value + v->vibrato_speed);
        if (value >= limit) {
            v->vibrato_rising = false;
            value = limit;
        }
    }
    v->vibrato_value = (uint16_t)value;
    int swing = (int)value - (limit >> 1);
    if (index < VIBRATO_WIDENED_BELOW) {
        swing *= 1 << ((VIBRATO_WIDENED_BELOW - index + VIBRATO_OCTAVE_NOTES - 1) / VIBRATO_OCTAVE_NOTES);
    }
    return (uint16_t)(period + swing);
}

// The portamento slides by an amount that grows every tick; the info byte of the previous row gives its speed
static uint16_t apply_portamento(HhVoice* v, uint16_t period) {
    if (!v->portamento) {
        return period;
    }
    const uint8_t speed = v->porta_param;
    if (!(speed & 0x80)) {
        v->porta += (uint32_t)speed << 11;
        return (uint16_t)(period - (uint16_t)(v->porta >> 16));
    }
    v->porta += (uint32_t)(uint8_t)-speed << 11;
    return (uint16_t)(period + (uint16_t)(v->porta >> 16));
}

// The replayer's DMA mask so far in the tick: the channels of voices 0..`last` whose DMA a command switched off
static uint16_t dma_shadow(const HhSong* song, int last) {
    uint16_t shadow = 0;
    for (int i = 0; i <= last; i++) {
        if (song->voices[i].dma_restart) {
            shadow |= (uint16_t)(1u << i);
        }
    }
    return shadow;
}

// Is the word at `at` the replayer variable `distance` bytes in front of the header?
static bool variable_at(const HhModule* m, uint32_t at, uint8_t distance) {
    return distance != 0 && at == m->header - distance;
}

// A word of the period table. The table has 72 entries; behind it come the replayer's variables (versions that have
// them) and the header and song data. A note past the table reads them: the variables as the running replayer holds
// them, the rest as in the file.
static uint16_t period_table_word(const HhModule* m, const HhVersion* version, uint8_t index, uint16_t shadow) {
    const uint32_t at = version->period_table + (uint32_t)index * 2;
    const uint32_t subsongs = HH_LOAD_ADDRESS + m->subsongs;
    if (variable_at(m, at, version->shadow_before_header)) {
        return shadow;
    }
    if (variable_at(m, at, version->init_flag_before_header)) {
        return 1;
    }
    if (variable_at(m, at, version->subsongs_before_header)) {
        return (uint16_t)(subsongs >> 16);
    }
    if (variable_at(m, at - 2, version->subsongs_before_header)) {
        return (uint16_t)subsongs;
    }
    if (variable_at(m, at, version->effect_before_header)) {
        return 0;
    }
    return hhm_u16(m, at);
}

// The period of a voice: its note from the period table, then vibrato and portamento
static uint16_t voice_period(const HhModule* m, const HhVersion* version, HhVoice* v, uint16_t shadow) {
    const uint8_t index = note_index(v);
    uint16_t period = period_table_word(m, version, index, shadow);
    period = version->engine->flat_vibrato ? apply_vibrato_flat(v, period) : apply_vibrato_scaled(v, period, index);
    return apply_portamento(v, period);
}

// One voice's tick: the sequences and the effects. Leaves AUDxPER and AUDxVOL in the channel.
static void run_voice(const HhModule* m, const HhVersion* version, HhSong* song, int index) {
    HhVoice* v = &song->voices[index];
    version->engine->run_frequency(m, version, v);
    run_vseq(m, version->engine, v);
    v->period = voice_period(m, version, v, dma_shadow(song, index));
    v->paula.period = v->period;
    v->paula.volume = v->volume;
}

void hh_song_tick(HhSong* song, const HhModule* m, const HhVersion* version, NuadeVoices* voices) {
    if (song->stopped) {
        // DMA off, SongEnd (the eagleplayer starts the subsong again), volumes 0
        song->looped = true;
        hh_song_start(song, m, version);
        if (version->engine->stop_keeps_periods) {
            for (int i = 0; i < version->voices; i++) {
                song->voices[i].paula.period = song->voices[i].period;
            }
        }
    } else {
        step_voices(m, version, song);
        for (int i = 0; i < version->voices; i++) {
            song->voices[i].dma_restart = false;
        }
        for (int i = 0; i < version->voices; i++) {
            run_voice(m, version, song, i);
        }
        // DMA on for the channels the commands stopped, a raster line, then the repeat registers
        for (int i = 0; i < version->voices; i++) {
            if (song->voices[i].dma_restart) {
                nuade_paula_channel_dma_on(&song->voices[i].paula);
            }
        }
        version->engine->write_repeat(song, m, version);
    }
    for (int i = 0; i < HH_VOICES; i++) {
        nuade_paula_channel_sync(&song->voices[i].paula, (uint32_t)i, voices);
    }
}
