// SPDX-License-Identifier: MIT
// Art and Magic music driver (.aam).
//
// An .aam file is a relocatable 68k program: the driver (about 1.7 KB of code), its song tables and the samples as 8SVX
// forms back to back, ended by an "EXIT" tag. UADE's eagleplayer relocates the table of absolute addresses inside it,
// runs its init code (which strings the sample bodies together and builds the sample table) and then calls the play
// routine once per tick. The driver plays on Paula channels 1..3, one voice each.
//
// Two builds of the driver exist in the corpus (Agony level 3 differs from the others): the same program, with the
// DMA bit table moved into the code and every later address shifted by a few bytes. `locate_layout` finds what the
// eagleplayer and the driver address from the displacements of the driver's pc-relative `lea` instructions.
//
// The file holds addresses of the original link in its pointer tables. The eagleplayer relocates them to where UADE
// loads the module (LOAD_ADDRESS); the image does the same, so that the driver reads the same words from it, also when
// it reads past a table. `module_pointer` turns an address word back into a file offset.

#include <nuade/players/artandmagic.h>

#include "nuade_paula_channel.h"
#include "nuade_tracker.h"
#include "nuade_utils.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// An address that points nowhere.
#define NO_ADDR 0xFFFFFFFFu

// Sample numbers a byte can select. The init code has no limit (it runs to the "EXIT" tag); a file with more forms than
// this is not a module of the corpus, and its later forms get no table entry.
#define MAX_SAMPLES 256

// Bytes of a record of the sample table, of the voice table and of the instrument table, and of an address.
#define SAMPLE_RECORD_SIZE 12
#define VOICE_TABLE_ENTRY_SIZE 8
#define INSTRUMENT_SIZE 8
#define ADDRESS_SIZE 4

// Where UADE loads the module (uade.c: modaddr = ((relocaddr + size of the relocated eagleplayer) & 0x7FFFF000) +
// 0x2000, which is 0xF000 for this eagleplayer). The init code puts absolute addresses into the sample table, which
// lies in the module's memory, in front of the sample area: from the third entry on it overwrites the first bytes of
// the first sample, and the first sample plays those bytes (the addresses depend on the load address).
#define LOAD_ADDRESS 0xF000u

// Entries of the voice table: one per Paula channel and one spare; the eagleplayer relocates the track list pointer of
// each. Channel 0 never plays.
#define VOICE_TABLE_ENTRIES 5

// The stored address that the relocation maps to the instruments lies this many bytes before them.
#define ORIGIN_BEFORE_INSTRUMENTS 9

// Pointers in the pitch sequence table (a sequence is selected by 5 bits).
#define PITCH_SEQS 32

// The driver multiplies every volume by this master volume (a fade it has, which nothing calls, scales it).
#define MASTER_VOLUME 0x40

// Bytes of a voice's note stream. Below NoteByte_RestFirst a note is a number; the prefixes come in this order.
enum {
    NoteByte_ModulationFirst = 0x60, // 0x60..0x7F: select a pitch sequence
    NoteByte_DurationFirst = 0x80,   // 0x80..0xBF: note duration in steps
    NoteByte_InstrumentFirst = 0xC0, // 0xC0..0xEF: instrument number
    NoteByte_RestFirst = 0xF0,       // 0xF0..0xFA: a rest of n steps
    NoteByte_Legato = 0xFB,          // the next note does not restart the sample
    NoteByte_Level = 0xFC,           // the next byte is the note's volume
    NoteByte_Portamento = 0xFD,      // slide from one note to another
    NoteByte_Stop = 0xFE,            // the voice stops
    NoteByte_NextPattern = 0xFF,     // the pattern ends
};

// Bytes of a voice's track list.
enum {
    TrackByte_NoteBase = 0x80,       // 0x80..0xFE: note transposition of the patterns that follow
    TrackByte_InstrumentBase = 0x70, // 0x70..0x7F: instrument offset of the patterns that follow
    TrackByte_Repeats = 0x60,        // 0x60..0x6F: the pattern plays this many more times
    TrackByte_Loop = 0xFF,           // the next byte is where in the track list to start over
};

// Bytes of a pitch sequence (a list of notes added to the note).
enum {
    PitchSeqByte_Stop = 0xFE,
    PitchSeqByte_Loop = 0xFF, // the next byte is where in the sequence to start over
};

// Instrument flag byte.
enum {
    InstrumentFlag_PitchSequence = 0x40,
};

// Types

// A voice's record of the voice table.
typedef struct VoiceTableEntry {
    uint32_t track_list; // file offset
    uint16_t swing[2];   // ticks per step: the shorter and longer step of a swung pair, minus 1
} VoiceTableEntry;

// An instrument record: 8 bytes.
typedef struct Instrument {
    int8_t level;           // volume (0 takes the note's own)
    uint8_t vibrato;        // depth shift in the high nibble, speed in the low one; 0 for none
    uint8_t vibrato_delay;  // steps that pass before the vibrato starts
    uint8_t flags;          // InstrumentFlag_*
    int8_t detune;          // subtracted from the period
    uint8_t envelope_speed; // the volume ramp's step per tick, minus 1
    int8_t envelope;        // negative: fade in; else steps before the end of the note at which it fades out; 0 none
    uint8_t sample;
} Instrument;

// A record of the sample table as the init code writes it into the module: absolute addresses.
typedef struct SampleRecord {
    uint32_t start;
    uint32_t repeat_start;
    uint32_t length; // bytes of the start part: the one-shot part and the repeat
} SampleRecord;

// A sample, from its record of the sample table.
typedef struct SampleEntry {
    uint32_t start;        // file offset of the first byte, in the compacted sample area
    uint16_t length_words; // AUDxLEN of the start
    uint32_t repeat_start; // file offset of the repeat
    uint16_t repeat_words; // AUDxLEN of the repeat
} SampleEntry;

// Module access. Reads outside the file give 0.

static uint8_t rd8(const AamPlayer* p, int64_t off) {
    return (off >= 0 && (uint64_t)off < p->size) ? p->image[off] : 0;
}

static uint16_t rd16(const AamPlayer* p, int64_t off) {
    return (uint16_t)((rd8(p, off) << 8) | rd8(p, off + 1));
}

static uint32_t rd32(const AamPlayer* p, int64_t off) {
    return ((uint32_t)rd16(p, off) << 16) | rd16(p, off + 2);
}

// The next byte of a note stream or track list and the position after it. A stream that runs off the file reads as the
// byte that stops a voice.

static uint8_t stream_byte(const AamPlayer* p, uint32_t* pos) {
    const uint8_t value = *pos < p->size ? p->image[*pos] : NoteByte_Stop;
    (*pos)++;
    return value;
}

// The file offset an address word of a pointer table points at.

static uint32_t module_pointer(const AamPlayer* p, int64_t at) {
    return rd32(p, at) - LOAD_ADDRESS;
}

static VoiceTableEntry read_voice_table_entry(const AamPlayer* p, uint8_t channel) {
    const uint32_t at = p->layout.voice_table + VOICE_TABLE_ENTRY_SIZE * channel;
    return (VoiceTableEntry) { .track_list = module_pointer(p, at), .swing = { rd16(p, at + 4), rd16(p, at + 6) } };
}

static Instrument read_instrument(const AamPlayer* p, uint8_t index) {
    const uint32_t at = p->layout.instruments + INSTRUMENT_SIZE * index;
    return (Instrument) { .level = (int8_t)rd8(p, at),
                          .vibrato = rd8(p, at + 1),
                          .vibrato_delay = rd8(p, at + 2),
                          .flags = rd8(p, at + 3),
                          .detune = (int8_t)rd8(p, at + 4),
                          .envelope_speed = rd8(p, at + 5),
                          .envelope = (int8_t)rd8(p, at + 6),
                          .sample = rd8(p, at + 7) };
}

// The period table entry of a note. A note past the table reads the driver's tables that follow it.

static uint16_t period_at(const AamPlayer* p, int note) {
    return rd16(p, (int64_t)p->layout.periods + 2 * (int64_t)note);
}

static SampleRecord read_sample_record(const AamPlayer* p, uint32_t number) {
    const uint32_t at = p->layout.sample_table + SAMPLE_RECORD_SIZE * number;
    return (SampleRecord) { .start = rd32(p, at), .repeat_start = rd32(p, at + 4), .length = rd32(p, at + 8) };
}

static SampleEntry read_sample_entry(const AamPlayer* p, uint8_t number) {
    const SampleRecord record = read_sample_record(p, number);
    return (SampleEntry) { .start = record.start - LOAD_ADDRESS,
                           .length_words = (uint16_t)(record.length >> 1),
                           .repeat_start = record.repeat_start - LOAD_ADDRESS,
                           .repeat_words = (uint16_t)(record.length - (record.repeat_start - record.start)) >> 1 };
}

// Paula reads from an even address; an address past the file has nothing to play.

static const int8_t* sample_data(const AamPlayer* p, uint32_t off) {
    return off <= p->size ? (const int8_t*)(p->image + (off & ~1u)) : nullptr;
}

// AamLayout and set-up

#define EXIT_TAG 0x45584954u // "EXIT"
#define FORM_TAG 0x464F524Du // "FORM"
#define CODE_TAG_OFFSET 0x1A
#define BUILD_MARK_OFFSET 0x20E
#define BUILD_MARK 0x01020408u // the DMA bit table, in the code of the second build

// `lea d16(pc),An`: opcode with the register field masked out.
#define LEA_PC_MASK 0xF1FF
#define LEA_PC_OPCODE 0x41FA

// Where each address of the AamLayout is loaded by a `lea d16(pc),An` of the driver: its code offset in the first and
// in the second build. The second build's code differs after offset 0x20E.
typedef struct LayoutSite {
    size_t field; // offsetof(AamLayout, ...)
    uint16_t site[2];
} LayoutSite;

// clang-format off
static const LayoutSite s_layout_sites[] = {
    { offsetof(AamLayout, first_form),   { 0x0A,  0x0A  } },
    { offsetof(AamLayout, sample_table), { 0x12,  0x12  } },
    { offsetof(AamLayout, voice_table),  { 0xE0,  0xE0  } },
    { offsetof(AamLayout, first_note),   { 0xFC,  0xFC  } },
    { offsetof(AamLayout, instruments),  { 0x15C, 0x15C } },
    { offsetof(AamLayout, periods),      { 0x39E, 0x3A0 } },
    { offsetof(AamLayout, patterns),     { 0x472, 0x476 } },
    { offsetof(AamLayout, pitch_seqs),   { 0x4E4, 0x4E8 } },
};
// clang-format on

// The address a `lea d16(pc),An` at `site` loads, or NO_ADDR when there is no such instruction.

static uint32_t lea_target(const AamPlayer* p, uint32_t site) {
    if ((rd16(p, site) & LEA_PC_MASK) != LEA_PC_OPCODE) {
        return NO_ADDR;
    }
    const int64_t target = (int64_t)site + 2 + (int16_t)rd16(p, site + 2);
    return (target >= 0 && target < (int64_t)p->size) ? (uint32_t)target : NO_ADDR;
}

static bool locate_layout(const AamPlayer* p, AamLayout* layout) {
    const int build = rd32(p, BUILD_MARK_OFFSET) == BUILD_MARK ? 1 : 0;
    for (size_t i = 0; i < nuade_sizeof_array(s_layout_sites); i++) {
        const uint32_t address = lea_target(p, s_layout_sites[i].site[build]);
        if (address == NO_ADDR) {
            return false;
        }
        memcpy((uint8_t*)layout + s_layout_sites[i].field, &address, sizeof(address));
    }
    // The pointer table: the voice table's pointers (every 8th byte of 40), then the longs from `patterns` to the
    // origin. The pitch sequence table lies inside it.
    layout->origin = layout->instruments - ORIGIN_BEFORE_INSTRUMENTS;
    if (layout->instruments < ORIGIN_BEFORE_INSTRUMENTS || layout->origin < layout->patterns
        || layout->patterns != layout->voice_table + VOICE_TABLE_ENTRY_SIZE * VOICE_TABLE_ENTRIES) {
        return false;
    }
    // The eagleplayer relocates (origin - patterns) / 4 + 1 longs from `patterns`.
    layout->pointers_end = layout->patterns + ADDRESS_SIZE * (((layout->origin - layout->patterns) >> 2) + 1);
    return layout->pitch_seqs >= layout->patterns
           && layout->pitch_seqs + ADDRESS_SIZE * PITCH_SEQS <= layout->pointers_end;
}

static void write32(AamPlayer* p, uint32_t at, uint32_t value) {
    for (uint32_t i = 0; i < 4; i++) {
        p->image[at + i] = (uint8_t)(value >> (24 - 8 * i));
    }
}

// The eagleplayer's relocation: every address of the pointer tables moves from the original link to the load address.

static void relocate_pointers(AamPlayer* p) {
    const AamLayout* layout = &p->layout;
    const uint32_t delta = LOAD_ADDRESS - rd32(p, layout->origin) + layout->instruments;
    for (uint32_t i = 0; i < VOICE_TABLE_ENTRIES; i++) {
        const uint32_t at = layout->voice_table + VOICE_TABLE_ENTRY_SIZE * i;
        write32(p, at, rd32(p, at) + delta);
    }
    for (uint32_t at = layout->patterns; at < layout->pointers_end; at += ADDRESS_SIZE) {
        write32(p, at, rd32(p, at) + delta);
    }
}

static void write_sample_record(AamPlayer* p, uint32_t number, SampleRecord record) {
    const uint32_t at = p->layout.sample_table + SAMPLE_RECORD_SIZE * number;
    write32(p, at, record.start);
    write32(p, at + 4, record.repeat_start);
    write32(p, at + 8, record.length);
}

// One 8SVX form of the sample area, as the init code reads it: the header chunk, two chunks it skips (NAME, ANNO) and
// the BODY.

typedef struct SampleForm {
    uint32_t one_shot; // bytes played once before the repeat
    uint32_t repeat;   // bytes of the repeat (0: none)
    uint32_t body;     // file offset of the body
    uint32_t body_size;
} SampleForm;

static bool read_sample_form(const AamPlayer* p, uint32_t at, SampleForm* form) {
    // Chunk starts are file offsets; 64 bits keep the sums of crafted sizes from wrapping.
    int64_t chunk = (int64_t)at + 20 + rd32(p, at + 16);
    form->one_shot = rd32(p, at + 20);
    form->repeat = rd32(p, at + 24);
    for (int skipped = 0; skipped < 2; skipped++) {
        chunk += 8 + (int64_t)rd32(p, chunk + 4);
    }
    form->body_size = rd32(p, chunk + 4);
    form->body = (uint32_t)(chunk + 8);
    return chunk + 8 + (int64_t)form->body_size <= (int64_t)p->size;
}

// The init code of the module: for every form from the first on, a record of the sample table (absolute addresses of
// the sample and of its repeat, and the length), then the body moved down behind the previous one. A form without a
// repeat repeats its last two bytes.

static void build_samples(AamPlayer* p) {
    uint32_t form_at = p->layout.first_form;
    uint32_t dest = p->layout.first_form;
    for (uint32_t number = 0; number < MAX_SAMPLES && rd32(p, form_at) == FORM_TAG; number++) {
        SampleForm form;
        if (!read_sample_form(p, form_at, &form)
            || p->layout.sample_table + SAMPLE_RECORD_SIZE * (number + 1) > p->size) {
            break;
        }
        uint32_t one_shot = form.one_shot;
        uint32_t repeat = form.repeat;
        if (repeat == 0) {
            one_shot -= 2;
            repeat = 2;
        }
        write_sample_record(p, number,
                            (SampleRecord) { .start = LOAD_ADDRESS + dest,
                                             .repeat_start = LOAD_ADDRESS + dest + one_shot,
                                             .length = one_shot + repeat });
        memmove(p->image + dest, p->image + form.body, form.body_size);
        dest += form.body_size;
        form_at = form.body + form.body_size;
    }
}

// The voice routines

// Half of a nibble, rounded up: the driver shifts right and adds the bit that fell out.

static int8_t half_rounded_up(uint8_t nibble) {
    return (int8_t)((nibble >> 1) + (nibble & 1));
}

// The track list: the next pattern of the voice, with the transposition and repeat count the entry carries. The end of
// the list starts it over; that is where the eagleplayer calls SongEnd.

static void next_track_entry(AamPlayer* p, AamVoice* v) {
    uint32_t pos = v->track_pos;
    if (pos >= p->size) {
        v->active = false;
        return;
    }
    uint8_t b = stream_byte(p, &pos);
    if (b == TrackByte_Loop) {
        p->looped = true;
        const uint32_t list = read_voice_table_entry(p, v->channel).track_list;
        if (list >= p->size) {
            v->active = false;
            return;
        }
        pos = list + rd8(p, pos);
        b = stream_byte(p, &pos);
    }
    if (b >= TrackByte_NoteBase) {
        v->note_base = b & 0x7F;
        b = stream_byte(p, &pos);
    }
    if (b >= TrackByte_InstrumentBase) {
        v->instrument_base = b & 0xF;
        b = stream_byte(p, &pos);
    }
    v->pattern_repeats = 0;
    if (b >= TrackByte_Repeats) {
        v->pattern_repeats = b & 0xF;
        b = stream_byte(p, &pos);
    }
    v->pattern = (int8_t)b;
    v->track_pos = pos;
}

// The pattern ended: repeat it, or take the next entry of the track list. Then point the voice at the pattern's notes.

static void next_pattern(AamPlayer* p, AamVoice* v) {
    if (v->pattern_repeats > 0) {
        v->pattern_repeats--;
    } else {
        next_track_entry(p, v);
        if (!v->active) {
            return;
        }
    }
    v->note_pos = module_pointer(p, (int64_t)p->layout.patterns + ADDRESS_SIZE * (int64_t)v->pattern);
}

// A note starts: the sample goes into the channel's registers (its DMA starts a tick later), the pitch and volume
// restart and the volume envelope is set up.

static void start_note(AamPlayer* p, AamVoice* v) {
    const Instrument ins = read_instrument(p, v->instrument);
    const SampleEntry sample = read_sample_entry(p, ins.sample);
    const uint16_t period = period_at(p, v->note);
    v->sample = ins.sample;
    v->paula.lc = sample_data(p, sample.start);
    v->paula.len_words = sample.length_words;

    if (v->level_base == 0) {
        v->level_base = (uint16_t)(int16_t)ins.level;
    }
    v->level = v->level_base;
    v->pitch = (uint32_t)period << 16;
    v->dma_stage = AamDmaStage_Start;
    v->fade_in = false;
    v->fade_out_from = -1;
    if (ins.envelope < 0) {
        v->fade_in = true;
        v->level = 0;
        v->ramp = 0x200;
    } else if (ins.envelope > 0) {
        v->fade_out_from = (int16_t)(ins.envelope + 1);
        v->ramp = 0x200;
    }
    v->shown_note = v->note >= 0 && period != 0 ? nuade_period_to_note(period) : -1;
    v->shown_instrument = v->instrument;
}

// A note (or a tie, which starts the same note again) ends the event: the stream continues at `pos`.

static void begin_note(AamPlayer* p, AamVoice* v, uint32_t pos) {
    v->note_pos = pos;
    start_note(p, v);
    v->steps_left = v->duration;
}

// Portamento event, after its command byte: slide speed and the step at which it starts, the note to start on and the
// note to slide to.

static void begin_portamento(AamPlayer* p, AamVoice* v, uint32_t pos) {
    const uint8_t params = stream_byte(p, &pos);
    v->slide_speed = (int8_t)((params >> 4) + 1);
    v->slide_from = (int8_t)(uint8_t)(v->duration - (params & 0xF) + 1);
    v->note = (int8_t)(stream_byte(p, &pos) + v->note_base);
    v->slide_target = (int8_t)(stream_byte(p, &pos) + v->note_base);
    begin_note(p, v, pos);
}

// The byte after a prefix of a note event. It ends the event when it is a portamento (if the prefix allows one) or a
// byte of the prefix's own class or above, which means the event has no note and plays the previous one again.
// Otherwise it is left in `b` for the next prefix.

static bool prefix_ends_event(AamPlayer* p, AamVoice* v, uint32_t* pos, uint8_t* b, uint8_t replay_from,
                              bool portamento_allowed) {
    *b = stream_byte(p, pos);
    if (portamento_allowed && *b == NoteByte_Portamento) {
        begin_portamento(p, v, *pos);
        return true;
    }
    if (*b >= replay_from) {
        begin_note(p, v, *pos - 1);
        return true;
    }
    return false;
}

// The next event of the note stream: a note, a rest, a portamento or the end of the voice.

static void read_note(AamPlayer* p, AamVoice* v) {
    v->slide_from = -1;
    uint32_t pos = v->note_pos;
    uint8_t b = stream_byte(p, &pos);
    if (b == NoteByte_Legato) {
        b = stream_byte(p, &pos);
    } else {
        nuade_paula_channel_dma_off(&v->paula);
    }
    if (b == NoteByte_NextPattern) {
        next_pattern(p, v);
        if (!v->active) {
            return;
        }
        pos = v->note_pos;
        b = stream_byte(p, &pos);
    }
    if (b == NoteByte_Stop) {
        v->active = false;
        return;
    }
    if (b == NoteByte_Portamento) {
        begin_portamento(p, v, pos);
        return;
    }

    v->level_base = 0;
    if (b == NoteByte_Level) {
        v->level_base = (uint16_t)(int16_t)(int8_t)stream_byte(p, &pos);
        if (prefix_ends_event(p, v, &pos, &b, NoteByte_Legato, true)) {
            return;
        }
    }
    if (b >= NoteByte_RestFirst) {
        v->duration = b & 0xF;
        v->note = -1;
        v->note_pos = pos;
        v->steps_left = v->duration;
        return;
    }
    if (b >= NoteByte_InstrumentFirst) {
        v->instrument = (uint8_t)((b & 0x3F) + v->instrument_base);
        if (prefix_ends_event(p, v, &pos, &b, NoteByte_InstrumentFirst, true)) {
            return;
        }
    }
    if (b >= NoteByte_DurationFirst) {
        v->duration = b & 0x3F;
        if (prefix_ends_event(p, v, &pos, &b, NoteByte_DurationFirst, true)) {
            return;
        }
    }
    if (b >= NoteByte_ModulationFirst) {
        v->pitch_seq = b & 0x1F;
        if (prefix_ends_event(p, v, &pos, &b, NoteByte_ModulationFirst, false)) {
            return;
        }
    }
    v->note = (int8_t)(b + v->note_base);
    begin_note(p, v, pos);
}

// The volume ramp: one tick of the envelope's time. False when the ramp has run out.

static bool ramp_advance(const Instrument* ins, AamVoice* v) {
    const uint16_t step = (uint16_t)(ins->envelope_speed + 1);
    const bool left = v->ramp >= step;
    v->ramp = (uint16_t)(v->ramp - step);
    return left;
}

// The level for a ramp position: the note's volume scaled by ramp / 512.

static uint16_t ramp_level(const AamVoice* v, uint16_t ramp) {
    const uint32_t product = (uint32_t)ramp * v->level_base;
    return (uint16_t)((product >> 9) & 0x7FFF);
}

// One tick of the slide towards the target note: the pitch moves a fraction of the distance, and snaps to the target
// when it gets there.

static void slide_step(const AamPlayer* p, AamVoice* v) {
    const uint16_t from = period_at(p, v->note);
    const uint16_t target = period_at(p, v->slide_target);
    const int32_t distance = (int16_t)(uint16_t)(from - target);
    const uint32_t step = (uint32_t)(distance * v->slide_speed) << 10;
    // The step is subtracted from the unsigned 32-bit pitch. It is larger than the pitch when the period is to rise
    // (the step is negative, a huge unsigned value), and also when a falling period would pass zero; then the target
    // counts as reached from below, else from above. That is not the slide's direction for a pitch that is far from the
    // note's own (a pitch sequence sets it), so the comparison stays.
    const bool step_exceeds_pitch = v->pitch < step;
    v->pitch -= step;
    const int16_t period = (int16_t)(v->pitch >> 16);
    const bool arrived = step_exceeds_pitch ? period >= (int16_t)target : period <= (int16_t)target;
    if (arrived) {
        v->pitch = (uint32_t)target << 16;
        v->note = v->slide_target;
        v->slide_from = -1;
        v->vibrato_counter = half_rounded_up(v->vibrato_shift & 0xF);
        v->vibrato_down = false;
    }
}

// One tick of the vibrato: the pitch moves by a fraction of the distance to the next note, up and down.

static void vibrato_step(const AamPlayer* p, const Instrument* ins, AamVoice* v) {
    v->vibrato_counter--;
    if (v->vibrato_counter < 0) {
        v->vibrato_down = !v->vibrato_down;
        v->vibrato_counter = (int8_t)(ins->vibrato & 0xF);
    }
    const uint16_t distance = (uint16_t)(period_at(p, v->note) - period_at(p, v->note + 1));
    const int32_t delta = (int32_t)((uint32_t)(uint16_t)(distance << 1) << 16) >> v->vibrato_shift;
    v->pitch += v->vibrato_down ? 0u - (uint32_t)delta : (uint32_t)delta;
}

// Where the voice's pitch sequence starts.

static uint32_t pitch_seq_start(const AamPlayer* p, const AamVoice* v) {
    return module_pointer(p, (int64_t)p->layout.pitch_seqs + ADDRESS_SIZE * (int64_t)v->pitch_seq);
}

// One step of the pitch sequence: a list of notes added to the note, ending in a loop (255, offset) or a stop (254).

static void pitch_seq_step(const AamPlayer* p, AamVoice* v) {
    uint32_t pos = v->pitch_seq_pos;
    uint8_t b = stream_byte(p, &pos);
    if (b == PitchSeqByte_Loop) {
        const uint32_t start = pitch_seq_start(p, v);
        if (start >= p->size) {
            v->pitch_seq_on = false;
            return;
        }
        pos = start + rd8(p, pos);
        b = stream_byte(p, &pos);
    }
    if (b == PitchSeqByte_Stop) {
        v->pitch_seq_on = false;
        return;
    }
    const int note = (int8_t)(uint8_t)(b + (uint8_t)v->note);
    v->pitch = (uint32_t)period_at(p, note) << 16;
    v->pitch_seq_pos = pos;
}

// The volume envelope: a fade in over the first ticks, or a fade out when the note is about to end.

static void envelope_step(const Instrument* ins, AamVoice* v) {
    if (v->fade_in) {
        if (ramp_advance(ins, v)) {
            v->level = ramp_level(v, (uint16_t)(0x200 - v->ramp));
            return;
        }
        v->fade_in = false;
        v->level = v->level_base;
    }
    if (v->steps_left >= v->fade_out_from) {
        return;
    }
    if (ramp_advance(ins, v)) {
        v->level = ramp_level(v, v->ramp);
    } else {
        v->fade_out_from = -1;
        v->level = 0;
    }
}

// The per-tick effects of a note: set-up on the tick the DMA starts, then slide or vibrato, the pitch sequence and the
// envelope.

static void voice_effects(const AamPlayer* p, const Instrument* ins, AamVoice* v) {
    if (v->dma_stage == AamDmaStage_SetRepeat) {
        if (ins->vibrato != 0) {
            v->vibrato_from = (int8_t)(uint8_t)(v->duration - ins->vibrato_delay + 1);
            v->vibrato_shift = ins->vibrato >> 4;
            v->vibrato_counter = half_rounded_up(ins->vibrato & 0xF);
            v->vibrato_down = false;
        } else {
            v->vibrato_from = -1;
        }
        v->pitch_seq_on = (ins->flags & InstrumentFlag_PitchSequence) != 0;
        if (v->pitch_seq_on) {
            v->pitch_seq_pos = pitch_seq_start(p, v);
        }
    }

    if (v->steps_left <= v->slide_from) {
        slide_step(p, v);
    } else if (v->steps_left < v->vibrato_from) {
        vibrato_step(p, ins, v);
    }
    if (v->pitch_seq_on) {
        pitch_seq_step(p, v);
    }
    envelope_step(ins, v);
}

// One tick of a voice.

static void voice_tick(AamPlayer* p, AamVoice* v) {
    if (!v->active) {
        return;
    }
    NuadePaulaChannel* channel = &v->paula;
    const Instrument ins = read_instrument(p, v->instrument);
    const int8_t detune = (int8_t)(uint8_t)(0u - (uint8_t)ins.detune); // neg.b: the detune lowers the period
    channel->period = (uint16_t)((v->pitch >> 16) + (uint16_t)(int16_t)detune);
    const uint16_t product = (uint16_t)(MASTER_VOLUME * v->level); // mulu.w; the shift works on its low word
    channel->volume = (uint8_t)(product >> 6);

    if (v->dma_stage == AamDmaStage_Start) {
        v->dma_stage = AamDmaStage_SetRepeat;
        nuade_paula_channel_dma_on(channel);
    } else if (v->dma_stage == AamDmaStage_SetRepeat) {
        v->dma_stage = AamDmaStage_Idle;
        const SampleEntry sample = read_sample_entry(p, v->sample);
        channel->lc = sample_data(p, sample.repeat_start);
        channel->len_words = sample.repeat_words;
    }

    if (v->swing_ticks_left > 0) {
        v->swing_ticks_left--;
    } else {
        v->swing_odd = !v->swing_odd;
        v->swing_ticks_left = read_voice_table_entry(p, v->channel).swing[v->swing_odd ? 1 : 0];
        v->steps_left--;
        if (v->steps_left == 0) {
            read_note(p, v);
            return;
        }
    }
    if (v->note >= 0) {
        voice_effects(p, &ins, v);
    }
}

// The init routine of the driver for one voice: it starts on the first track list entry, one step from reading its
// first note.

static void voice_init(AamPlayer* p, AamVoice* v, uint8_t channel) {
    memset(v, 0, sizeof(*v));
    v->active = true;
    v->channel = channel;
    v->track_pos = read_voice_table_entry(p, channel).track_list;
    v->note_pos = p->layout.first_note;
    v->steps_left = 1;
    v->shown_note = -1;
    v->shown_instrument = -1;
}

// Public API

bool aam_probe(const uint8_t* data, size_t size) {
    return size >= CODE_TAG_OFFSET + 4 && size <= INT32_MAX && nuade_read_be32(data + CODE_TAG_OFFSET) == EXIT_TAG
           && nuade_read_be32(data + size - 4) == EXIT_TAG;
}

AamPlayer* aam_create(const uint8_t* data, size_t size) {
    if (!aam_probe(data, size)) {
        return nullptr;
    }
    AamPlayer* p = calloc(1, sizeof(AamPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    p->image = nuade_paula_padded_copy(data, size);
    if (p->image == nullptr) {
        free(p);
        return nullptr;
    }
    p->size = size;
    if (!locate_layout(p, &p->layout)) {
        aam_destroy(p);
        return nullptr;
    }
    relocate_pointers(p);
    build_samples(p);
    return p;
}

void aam_destroy(AamPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->image);
    free(player);
}

bool aam_start(AamPlayer* player, NuadeVoices* voices) {
    if (voices == nullptr) {
        return false;
    }
    // Channel 0 never plays: its record stays empty
    memset(&player->voices[0], 0, sizeof(player->voices[0]));
    player->voices[0].shown_note = -1;
    player->voices[0].shown_instrument = -1;
    for (uint8_t i = 0; i < AAM_VOICES; i++) {
        nuade_voice_stop(voices, i);
        nuade_voice_set_volume(voices, i, 0.0f);
        // Paula channels 0 and 3 are left, 1 and 2 right
        nuade_voice_set_pan(voices, i, (i == 0 || i == 3) ? -1.0f : 1.0f);
        if (i > 0) {
            voice_init(player, &player->voices[i], i);
        }
    }
    return true;
}

AamTickResult aam_tick(AamPlayer* player, NuadeVoices* voices) {
    player->looped = false;
    bool any = false;
    for (uint8_t i = 1; i < AAM_VOICES; i++) {
        AamVoice* v = &player->voices[i];
        voice_tick(player, v);
        nuade_paula_channel_sync(&v->paula, i, voices);
        any = any || v->active;
    }
    return (AamTickResult) { .looped = player->looped, .ended = !any };
}

AamVoiceState aam_voice_state(const AamPlayer* player, int voice) {
    if (voice < 0 || voice >= AAM_VOICES) {
        return (AamVoiceState) { .note = -1, .instrument = -1 };
    }
    const AamVoice* v = &player->voices[voice];
    return (AamVoiceState) { .note = v->shown_note, .instrument = v->shown_instrument };
}
