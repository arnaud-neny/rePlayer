// SPDX-License-Identifier: MIT
// Anders 0land player, translated from the UADE eagleplayer "Anders 0land player module" (Wanted Team).
//
// A module is three chunks: "mpl" (the composer's own 68k player, not used here), "mdt" (song data) and "msm" (the
// samples). The mdt chunk starts with a table of word offsets to its tables: the per-voice track lists, the subsong
// tables, the patterns, the instruments, the volume envelopes, the effects and the sample tables.
//
// The sequencer runs in steps of `speed` ticks. A voice reads its next event when its duration has counted down; voices
// 0 and 1 do that on the first tick of a step, voices 2 and 3 on the second. Nothing reaches the hardware until the
// last tick of the step: DMA is switched off for the voices with a new note, then the sample registers are written
// (the attack part of a sample), and DMA starts on the first tick of the next step. On the second tick the loop part of
// the sample is written to the registers, so that it plays after the attack.

#include <nuade/players/andersoland.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define CHUNK_HEADER_SIZE 8
#define BYTES_PER_LONG 4

#define SUBSONG_ENTRY_SIZE 4 // the loop point table and the parameter table have 4 bytes per subsong
#define INSTRUMENT_SIZE 8
#define ENVELOPE_SIZE 4

// Track list entries
#define TRACK_LOOP 0xFF      // continue at the subsong's loop point for this voice
#define TRACK_STOP 0xFE      // the voice stops
#define TRACK_TRANSPOSE 0x80 // flag: the entry is a transposition, the pattern number follows

// Pattern events
#define PATTERN_END 0xFF      // pattern byte: the pattern is over
#define EVENT_INSTRUMENT 0x80 // event flag: an instrument byte follows
#define EVENT_EFFECT 0x40     // event flag: an effect byte follows
#define EVENT_ALTERNATE 0x20  // event flag: use the instrument's second envelope and sample
#define EVENT_DURATION_MASK 0x1F
#define EVENT_NO_NOTE 0x80 // flag in the byte after the event flags: no note byte follows
#define NOTE_LEGATO 0x80   // flag in the note byte: do not restart the sample
#define NOTE_MASK 0x7F     // the note number of a note byte
#define NUMBER_MASK 0x7F   // transposition, instrument, effect and envelope numbers
#define ENVELOPE_KEEP 0xFF // envelope level: keep the current volume
#define NO_VIBRATO 0xFF    // vibrato delay: none

// Effect table words
#define EFFECT_FADE 0x4000  // start a fade out of the loud voices
#define EFFECT_SPEED 0x2000 // the low byte is the new speed
#define EFFECT_UP 0x1000    // pitch slide direction: set = slide the period up
#define EFFECT_MASK 0x0FFF

#define FADE_TICKS 0x40

// Room past the file for the sample starts the tables name: capped, and a start beyond the 68000's 24-bit address space
// is no sample (it would only make the allocation huge).
#define MAX_PADDED_SAMPLES 256
#define ADDRESS_LIMIT 0x1000000u

// Voices 0 and 1 read their events on the first tick of a step, voices 2 and 3 on the second.
#define VOICES_PER_TICK 2

// The replayer's tick counter is compared signed with 1: counts from 0x80 on take the first tick's path.
#define TICK_SIGN_BIT 0x80

// The period table the replayer indexes with note + transposition (a byte sum, so up to 255). Entries 69 and 70 are not
// periods but the two code words that follow the table, entry 71 is the last word of the hunk. Further reads leave the
// hunk (zero here).
#define PERIOD_COUNT 72
#define NOTE_BASE 12 // semitones from C-0 of table entry 0 less 12: entry 24 (856) is C-1 = 12

static const uint16_t k_periods[PERIOD_COUNT] = {
    3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1812, 1712, 1616, 1524, 1440,   1356,   1280,
    1208, 1140, 1076, 1016, 960,  906,  856,  808,  762,  720,  678,  640,  604,  570,  538,  508,    480,    453,
    428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,  214,  202,  190,  180,    170,    160,
    151,  143,  135,  127,  120,  114,  107,  101,  95,   85,   80,   80,   80,   80,   80,   0x8000, 0x00FF, 0x5754,
};

// A run of sample data: where it starts in the sample chunk and how many words it plays.
typedef struct SampleRef {
    uint32_t start;
    uint16_t length_words;
} SampleRef;

// One instrument. Every event with the alternate flag picks the second envelope and sample, the others the first.
typedef struct Instrument {
    uint8_t envelope[2];
    uint8_t sample[2];
    uint8_t vibrato_delay; // ticks before the vibrato starts, NO_VIBRATO for none
    int8_t vibrato_rate;   // pitch change per tick (x16 in the accumulator); the sign flips every half period
    uint8_t vibrato_period;
    // byte 7 is copied into the replayer's data and never read
} Instrument;

// A step of the volume envelope: the volume is set to `level` (unless ENVELOPE_KEEP), then `step` is added `count`
// times, then the entry `next` is loaded.
typedef struct Envelope {
    uint8_t level;
    uint8_t step;
    uint8_t count;
    uint8_t next;
} Envelope;

// The parameters of a subsong.
typedef struct Subsong {
    uint8_t enabled_voices; // bit n: voice n reads events
    uint8_t dma_voices;     // bit n: the replayer switches voice n's DMA on
    uint8_t speed;          // ticks per step
    uint8_t loop_point[AO_VOICES];
} Subsong;

// AoModule reads; the original reads whatever memory lies around the module, here it is zero.

static uint8_t module_u8(const AoModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static uint16_t module_u16(const AoModule* m, uint32_t offset) {
    return (uint16_t)(module_u8(m, offset) << 8 | module_u8(m, offset + 1));
}

static uint32_t module_u32(const AoModule* m, uint32_t offset) {
    return (uint32_t)module_u16(m, offset) << 16 | module_u16(m, offset + 2);
}

typedef struct Chunks {
    uint32_t mdt; // offset of the song data chunk
    uint32_t msm; // offset of the sample chunk
} Chunks;

static bool tag_is(const uint8_t* chunk, const char* tag) {
    return chunk[0] == (uint8_t)tag[0] && chunk[1] == (uint8_t)tag[1] && chunk[2] == (uint8_t)tag[2];
}

// The eagleplayer's Check2: the chunks "mpl", "mdt" and "msm" one after the other, sizes counting their header and
// even for the first two, together at most the file size. The original subtracts each size from the bytes left as a
// signed long and refuses a negative result; a size that moves the next chunk out of the file fails the tag test here.

static bool probe_chunks(const uint8_t* data, size_t size, Chunks* chunks) {
    static const char* const tags[] = { "mpl", "mdt", "msm" };
    int64_t offset = 0;
    int32_t left = (int32_t)size;
    uint32_t offsets[3] = { 0 };
    for (int i = 0; i < 3; i++) {
        if (offset < 0 || offset + CHUNK_HEADER_SIZE > (int64_t)size || !tag_is(data + offset, tags[i])) {
            return false;
        }
        const int32_t chunk = (int32_t)nuade_read_be32(data + offset + 4);
        left = (int32_t)((uint32_t)left - (uint32_t)chunk);
        if (left < 0 || (i < 2 && (chunk & 1) != 0)) {
            return false;
        }
        offsets[i] = (uint32_t)offset;
        offset += chunk;
    }
    if (chunks != nullptr) {
        *chunks = (Chunks) { .mdt = offsets[1], .msm = offsets[2] };
    }
    return true;
}

bool ao_probe(const uint8_t* data, size_t size) {
    return data != nullptr && probe_chunks(data, size, nullptr);
}

// A cursor over the word offsets at the start of the mdt chunk's data; each is the (signed) offset of a table from the
// start of that data.

typedef struct HeaderReader {
    const AoModule* module;
    uint32_t base;
    uint32_t pos;
} HeaderReader;

static uint32_t header_table(HeaderReader* r) {
    const uint32_t table = r->base + (uint32_t)(int32_t)(int16_t)module_u16(r->module, r->pos);
    r->pos += 2;
    return table;
}

static void header_skip(HeaderReader* r) {
    r->pos += 2;
}

// The attack part of a sample as the note start reads it: the sample number is doubled in a byte to index the length
// table and again to index the start table, so a number from 64 on wraps.

static SampleRef attack_part(const AoModule* m, uint8_t sample) {
    const uint8_t length_index = (uint8_t)(sample * 2u);
    const uint8_t start_index = (uint8_t)(length_index * 2u);
    return (SampleRef) { .start = module_u32(m, m->sample_starts + start_index),
                         .length_words = module_u16(m, m->sample_lengths + length_index) };
}

// The loop part, which the second tick of a step reads with long arithmetic on the number (no wrap).

static SampleRef loop_part(const AoModule* m, uint8_t sample) {
    return (SampleRef) { .start = module_u32(m, m->loop_starts + (uint32_t)sample * BYTES_PER_LONG),
                         .length_words = module_u16(m, m->loop_lengths + (uint32_t)sample * 2u) };
}

// Where a sample starts in the module: AUDxLC ignores its lowest bit, so a sample at an odd offset is fetched from the
// byte before it (the module sits at an even address).

static uint64_t sample_start(const AoModule* m, uint32_t offset) {
    return ((uint64_t)m->samples + offset) & ~(uint64_t)1;
}

// The furthest sample start the tables name. Samples may start (and run) past the file, the Amiga then plays what
// follows the module in memory: zeros here.

static uint64_t sample_starts_end(const AoModule* m) {
    const uint32_t count = (m->sample_lengths - m->sample_starts) / BYTES_PER_LONG;
    uint64_t end = m->size;
    for (uint32_t i = 0; i < count && i < MAX_PADDED_SAMPLES; i++) {
        const uint64_t starts[2]
            = { sample_start(m, attack_part(m, (uint8_t)i).start), sample_start(m, loop_part(m, (uint8_t)i).start) };
        for (int j = 0; j < 2; j++) {
            if (starts[j] < ADDRESS_LIMIT && starts[j] > end) {
                end = starts[j];
            }
        }
    }
    return end;
}

// Zeros up to the last sample start; the Paula tail of nuade_paula_padded_copy then covers any length from there.

static bool pad_samples(AoModule* m) {
    const uint64_t end = sample_starts_end(m);
    if (end <= m->size) {
        return true;
    }
    uint8_t* padded = nuade_paula_padded_copy(nullptr, (size_t)end);
    if (padded == nullptr) {
        return false;
    }
    memcpy(padded, m->data, m->size);
    free(m->data);
    m->data = padded;
    m->size = (uint32_t)end;
    return true;
}

// InitPlayer: the table addresses. The probe has checked that the chunks lie inside the file.

static bool load_module(AoModule* m, const uint8_t* data, size_t size, const Chunks* chunks) {
    m->data = nuade_paula_padded_copy(data, size);
    if (m->data == nullptr) {
        return false;
    }
    m->size = (uint32_t)size;
    m->samples = chunks->msm + CHUNK_HEADER_SIZE;

    const uint32_t mdt_data = chunks->mdt + CHUNK_HEADER_SIZE;
    HeaderReader header = { .module = m, .base = mdt_data, .pos = mdt_data };
    m->track_lists = header_table(&header);
    m->loop_points = header_table(&header);
    m->subsong_params = header_table(&header);
    header_skip(&header);
    header_skip(&header);
    m->patterns = header_table(&header);
    m->instruments = header_table(&header);
    m->envelopes = header_table(&header);
    m->effects = header_table(&header);
    m->sample_starts = header_table(&header);
    m->sample_lengths = header_table(&header);
    m->loop_starts = header_table(&header);
    m->loop_lengths = header_table(&header);

    // SubSongRange: the loop point table and the parameter table are adjacent. A module with no room for a subsong
    // would still play subsong 0, with whatever follows the table.
    const uint32_t count = (uint16_t)(m->subsong_params - m->loop_points) / SUBSONG_ENTRY_SIZE;
    m->subsong_count = count > 0 ? count : 1;
    return pad_samples(m);
}

AoPlayer* ao_create(const uint8_t* data, size_t size) {
    Chunks chunks;
    if (data == nullptr || !probe_chunks(data, size, &chunks)) {
        return nullptr;
    }
    AoPlayer* p = calloc(1, sizeof(AoPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    if (!load_module(&p->module, data, size, &chunks)) {
        ao_destroy(p);
        return nullptr;
    }
    return p;
}

void ao_destroy(AoPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->module.data);
    free(player);
}

int ao_subsong_count(const AoPlayer* player) {
    return (int)player->module.subsong_count;
}

// The sample data of a run; null if it does not start inside the module (with its padding). Any length can follow: the
// padded copy has the Paula tail behind the module.

static const int8_t* sample_data(const AoModule* m, SampleRef ref) {
    if (sample_start(m, ref.start) > m->size) {
        return nullptr;
    }
    return (const int8_t*)m->data + sample_start(m, ref.start);
}

static Instrument instrument_at(const AoModule* m, uint8_t number) {
    const uint32_t entry = m->instruments + (uint32_t)number * INSTRUMENT_SIZE;
    return (Instrument) { .envelope = { module_u8(m, entry), module_u8(m, entry + 1) },
                          .sample = { module_u8(m, entry + 2), module_u8(m, entry + 3) },
                          .vibrato_delay = module_u8(m, entry + 4),
                          .vibrato_rate = (int8_t)module_u8(m, entry + 5),
                          .vibrato_period = module_u8(m, entry + 6) };
}

static Envelope envelope_at(const AoModule* m, uint8_t number) {
    const uint32_t entry = m->envelopes + (uint32_t)number * ENVELOPE_SIZE;
    return (Envelope) { .level = module_u8(m, entry),
                        .step = module_u8(m, entry + 1),
                        .count = module_u8(m, entry + 2),
                        .next = module_u8(m, entry + 3) };
}

static Subsong subsong_at(const AoModule* m, uint32_t number) {
    const uint32_t params = m->subsong_params + number * SUBSONG_ENTRY_SIZE;
    const uint32_t loops = m->loop_points + number * SUBSONG_ENTRY_SIZE;
    Subsong subsong = { .enabled_voices = module_u8(m, params),
                        .dma_voices = module_u8(m, params + 2),
                        .speed = module_u8(m, params + 3) };
    for (uint32_t i = 0; i < AO_VOICES; i++) {
        subsong.loop_point[i] = module_u8(m, loops + i);
    }
    return subsong;
}

static uint16_t period_at(uint8_t index) {
    return index < PERIOD_COUNT ? k_periods[index] : 0;
}

// SongEnd bookkeeping: the song has reached its end when each voice has reached the end of its track list since the
// last time. It is a loop if some voice still plays, the end of the song if every voice has stopped.

static void voice_ended(AoSong* s, AoVoice* v) {
    v->end_pending = false;
    for (int i = 0; i < AO_VOICES; i++) {
        if (s->voices[i].end_pending) {
            return;
        }
    }
    bool any_enabled = false;
    for (int i = 0; i < AO_VOICES; i++) {
        s->voices[i].end_pending = true;
        any_enabled |= s->voices[i].enabled;
    }
    s->looped = any_enabled;
    s->ended = !any_enabled;
}

// The byte of the voice's pattern. The pattern the replayer starts on is a lone PATTERN_END.

static uint8_t pattern_byte(const AoModule* m, const AoVoice* v, uint32_t pos) {
    return v->has_pattern ? module_u8(m, v->pattern + pos) : PATTERN_END;
}

// The pattern a track list entry names.

static uint32_t pattern_start(const AoModule* m, uint8_t number) {
    return m->patterns + module_u32(m, m->patterns + (uint32_t)number * BYTES_PER_LONG);
}

// The track list gave the end of the pattern: take the next one. Returns false if the voice stops.

static bool next_pattern(const AoModule* m, AoSong* s, AoVoice* v) {
    uint8_t entry = module_u8(m, v->track + v->track_pos);
    if (entry == TRACK_LOOP) {
        v->track_pos = v->track_loop;
        entry = module_u8(m, v->track + v->track_pos);
        voice_ended(s, v);
    } else if (entry == TRACK_STOP) {
        v->enabled = false;
        v->envelope = 0;
        voice_ended(s, v);
        return false;
    }
    if ((entry & TRACK_TRANSPOSE) != 0) {
        v->transpose = entry & NUMBER_MASK;
        entry = module_u8(m, v->track + v->track_pos + 1u);
        v->track_pos++;
    }
    v->track_pos++;
    v->pattern_pos = 0;
    v->pattern = pattern_start(m, entry);
    v->has_pattern = true;
    return true;
}

// An effect byte: fade, speed change, or a pitch slide for the next note.

static void run_effect(const AoModule* m, AoSong* s, AoVoice* v, uint8_t number) {
    const uint16_t word = module_u16(m, m->effects + (uint8_t)((number & NUMBER_MASK) * 2u));
    if ((word & EFFECT_FADE) != 0) {
        s->fade = true;
    } else if ((word & EFFECT_SPEED) != 0) {
        s->speed = (uint8_t)word;
    } else if ((word & EFFECT_UP) == 0) {
        v->slide_next = (uint16_t)-(word & EFFECT_MASK);
    } else {
        v->slide_next = word & EFFECT_MASK;
    }
}

// Event layout: flags (duration in the low 5 bits), then an instrument byte or an effect byte (its top bit says no note
// follows), then the note byte unless the flags say otherwise. Without instrument or effect byte, the alternate flag
// means no note either.

static void read_event(const AoModule* m, AoSong* s, AoVoice* v) {
    v->slide_next = 0;
    v->new_note = true;
    v->legato = false;

    uint32_t pos = v->pattern_pos;
    uint8_t flags = pattern_byte(m, v, pos);
    if (flags == PATTERN_END) {
        if (!next_pattern(m, s, v)) {
            return;
        }
        pos = 0;
        flags = pattern_byte(m, v, pos);
    }

    bool has_note;
    uint8_t length;
    if ((flags & (EVENT_INSTRUMENT | EVENT_EFFECT)) != 0) {
        const uint8_t second = pattern_byte(m, v, pos + 1u);
        if ((flags & EVENT_INSTRUMENT) != 0) {
            v->instrument = second & NUMBER_MASK;
        } else {
            run_effect(m, s, v, second);
        }
        has_note = (second & EVENT_NO_NOTE) == 0;
        length = has_note ? 3 : 2;
    } else {
        has_note = (flags & EVENT_ALTERNATE) == 0;
        length = has_note ? 2 : 1;
    }
    v->pattern_pos = (uint8_t)(v->pattern_pos + length);

    // The first envelope and sample of the instrument, or with the alternate flag the second ones (once)
    if ((flags & EVENT_ALTERNATE) != 0) {
        if (!v->alternate_taken) {
            v->alternate_taken = true;
            const Instrument inst = instrument_at(m, v->instrument);
            v->envelope = inst.envelope[1] & NUMBER_MASK;
            v->sample = inst.sample[1];
        }
    } else {
        v->alternate_taken = false;
        const Instrument inst = instrument_at(m, v->instrument);
        v->envelope = inst.envelope[0] & NUMBER_MASK;
        v->sample = inst.sample[0];
    }

    v->duration = flags & EVENT_DURATION_MASK;
    if (has_note) {
        const uint8_t note = pattern_byte(m, v, pos + length - 1u); // the last byte of the event
        v->legato = (note & NOTE_LEGATO) != 0;
        const uint8_t table_index = (uint8_t)((note & NOTE_MASK) + v->transpose);
        v->note_period = period_at(table_index);
        v->note = (int)table_index - NOTE_BASE;
    }
}

// The events of two voices that are due.

static void read_events(const AoModule* m, AoSong* s, int first) {
    for (int i = first; i < first + VOICES_PER_TICK; i++) {
        AoVoice* v = &s->voices[i];
        if (!v->enabled) {
            continue;
        }
        if (v->duration != 0) {
            v->duration--;
            continue;
        }
        read_event(m, s, v);
    }
}

// DMA is off for the voices with a new note that do not keep their sample on the last tick of a step; on again for all
// voices on the other ticks.

static void switch_dma(AoSong* s, bool last_tick) {
    for (int i = 0; i < AO_VOICES; i++) {
        AoVoice* v = &s->voices[i];
        if (last_tick) {
            if (v->new_note && !v->legato) {
                nuade_paula_channel_dma_off(&v->paula);
            }
        } else if (v->dma) {
            nuade_paula_channel_dma_on(&v->paula);
        }
    }
}

// The second tick of a step: voices that read an event in the previous step get the loop part of their sample, which
// the channel takes up when the attack ends. The first tick's DMA start has latched the attack by now.

static void write_loops(const AoModule* m, AoSong* s) {
    for (int i = 0; i < AO_VOICES; i++) {
        AoVoice* v = &s->voices[i];
        if (!v->dma || !v->prev_new_note) {
            continue;
        }
        const SampleRef loop = loop_part(m, v->prev_sample);
        v->paula.len_words = loop.length_words;
        v->paula.lc = sample_data(m, loop);
    }
}

static void start_vibrato(const AoModule* m, AoVoice* v) {
    const Instrument inst = instrument_at(m, v->instrument);
    v->vibrato_offset = 0;
    v->vibrato_delay = inst.vibrato_delay;
    v->vibrato_rate = inst.vibrato_rate;
    v->vibrato_period = inst.vibrato_period;
    v->vibrato_count = inst.vibrato_period >> 1;
    v->vibrato_phase = 0;
}

// The end of a step: a voice with a new note gets its sample (the attack part), its pitch and its first envelope level.
// DMA is off for these voices until the next tick.

static void start_notes(const AoModule* m, AoSong* s) {
    for (int i = 0; i < AO_VOICES; i++) {
        AoVoice* v = &s->voices[i];
        if (!v->dma || !v->new_note) {
            continue;
        }
        if (!v->legato) {
            if (!v->alternate_taken) {
                start_vibrato(m, v);
            }
            const SampleRef attack = attack_part(m, v->sample);
            v->paula.len_words = attack.length_words;
            v->paula.lc = sample_data(m, attack);

            v->env_count = 0;
            const Envelope first = envelope_at(m, v->envelope);
            if (first.level != ENVELOPE_KEEP) {
                v->volume = first.level;
            }
        }
        v->period = v->note_period;
        v->slide = v->slide_next;
    }
}

static void run_vibrato(AoVoice* v) {
    if (v->vibrato_delay == NO_VIBRATO) {
        return;
    }
    if (v->vibrato_delay != 0) {
        v->vibrato_delay--;
        return;
    }
    v->vibrato_phase = (uint16_t)(v->vibrato_phase + ((uint16_t)(int16_t)v->vibrato_rate << 4));
    v->vibrato_offset = (int8_t)(v->vibrato_phase >> 8);
    v->vibrato_count--;
    if (v->vibrato_count == 0) {
        v->vibrato_rate = (int8_t)-v->vibrato_rate;
        v->vibrato_count = v->vibrato_period;
    }
}

// The volume envelope: while an entry's count lasts its step is added to the volume, then the next entry is loaded.

static void run_envelope(const AoModule* m, AoVoice* v) {
    if (v->env_count != 0) {
        v->volume = (uint8_t)(v->volume + v->env_step);
        v->env_count--;
        return;
    }
    const Envelope entry = envelope_at(m, v->envelope);
    if (entry.level != ENVELOPE_KEEP) {
        v->volume = entry.level;
    }
    v->env_step = entry.step;
    v->env_count = entry.count;
    v->envelope = entry.next;
}

// The fade takes volume 64 and up down by the number of ticks since the first fade, for the voices that come before
// the fade ends. The counter advances once for every voice.

static void run_fade(AoSong* s, AoVoice* v) {
    v->volume = (uint8_t)(v->volume - (uint8_t)((v->volume >> 6) * s->fade_count));
    s->fade_count++;
    if (s->fade_count == FADE_TICKS) {
        s->fade = false;
    }
}

static void run_modulation(const AoModule* m, AoSong* s) {
    for (int i = 0; i < AO_VOICES; i++) {
        AoVoice* v = &s->voices[i];
        if (!v->enabled || !v->dma || v->new_note) {
            continue;
        }
        run_vibrato(v);
        run_envelope(m, v);
        if (s->fade) {
            run_fade(s, v);
        }
    }
}

// Period and volume registers, every tick.

static void write_registers(AoSong* s) {
    for (int i = 0; i < AO_VOICES; i++) {
        AoVoice* v = &s->voices[i];
        if (!v->dma) {
            continue;
        }
        v->period = (uint16_t)(v->period + v->slide);
        v->paula.period = (uint16_t)(v->period + v->vibrato_offset);
        v->paula.volume = v->volume;
    }
}

// The first tick of a step reads the events of voices 0 and 1 (after keeping the last step's flags), the second those
// of voices 2 and 3.

static void read_step_events(const AoModule* m, AoSong* s) {
    if (s->tick == 1) {
        read_events(m, s, VOICES_PER_TICK);
    } else if (s->tick == 0 || s->tick >= TICK_SIGN_BIT) {
        for (int i = 0; i < AO_VOICES; i++) {
            AoVoice* v = &s->voices[i];
            v->prev_new_note = v->new_note;
            v->prev_sample = v->sample;
            v->new_note = false;
            v->legato = false;
        }
        read_events(m, s, 0);
    }
}

// The replayer's interrupt.

static void run_tick(AoPlayer* p) {
    const AoModule* m = &p->module;
    AoSong* s = &p->song;

    read_step_events(m, s);
    const bool last_tick = (uint8_t)(s->speed - 1u) == s->tick;
    switch_dma(s, last_tick);
    if (!last_tick && s->tick == 1) {
        write_loops(m, s);
    }

    s->tick++;
    if (s->tick == s->speed) {
        s->tick = 0;
        start_notes(m, s);
    }
    run_modulation(m, s);
    write_registers(s);
}

// InitSound

bool ao_start(AoPlayer* player, int subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || (uint32_t)subsong >= player->module.subsong_count) {
        return false;
    }
    const AoModule* m = &player->module;
    AoSong* s = &player->song;
    *s = (AoSong) { 0 };

    const Subsong params = subsong_at(m, (uint32_t)subsong);
    s->speed = params.speed;
    for (int i = 0; i < AO_VOICES; i++) {
        AoVoice* v = &s->voices[i];
        v->enabled = (params.enabled_voices >> i & 1) != 0;
        v->dma = (params.dma_voices >> i & 1) != 0;
        v->end_pending = true;
        v->track = m->track_lists + module_u32(m, m->track_lists + (uint32_t)i * BYTES_PER_LONG);
        v->track_loop = params.loop_point[i];
        v->note = -1;
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

AoTickResult ao_tick(AoPlayer* player, NuadeVoices* voices) {
    AoSong* s = &player->song;
    s->looped = false;
    s->ended = false;
    run_tick(player);
    for (int i = 0; i < AO_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, (uint32_t)i, voices);
    }
    return (AoTickResult) { .looped = s->looped, .ended = s->ended };
}

uint32_t ao_grid_ticks(const AoPlayer* player, int subsong) {
    if (subsong < 0 || (uint32_t)subsong >= player->module.subsong_count) {
        return 0;
    }
    return subsong_at(&player->module, (uint32_t)subsong).speed;
}

AoVoiceState ao_voice_state(const AoPlayer* player, int voice) {
    if (voice < 0 || voice >= AO_VOICES) {
        return (AoVoiceState) { .note = -1, .instrument = -1 };
    }
    const AoVoice* v = &player->song.voices[voice];
    return (AoVoiceState) { .note = v->note, .instrument = v->note >= 0 ? v->instrument : -1 };
}
