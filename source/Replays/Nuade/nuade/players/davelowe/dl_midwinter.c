// SPDX-License-Identifier: MIT
// Dave Lowe player: the Midwinter II patch of the "Dave Lowe" eagleplayer.
//
// For one driver (Midwinter II: 17 subsongs and a header word 0x12ef2 at 0x3c) the eagleplayer replaces entries of the
// module's pattern lists with patterns of its own: a few short phrases played with one sample record of the module and
// two volume envelopes. The patterns are the eagleplayer's data; this file builds the same ones after the module.

#include "dl_internal.h"
#include "nuade_utils.h"

#define SIGNATURE 0x12ef2u
#define SIGNATURE_SUBSONGS 17

// The module's sample record that the phrases play
#define PHRASE_SAMPLE_RECORD 0x4f82
// The phrase with the most parts
#define MAX_PARTS 2

typedef struct Note {
    uint16_t period;
    uint16_t length;
} Note;

typedef struct Envelope {
    uint16_t volume_cut;
    uint16_t volume_bonus;
    const uint16_t* values;
    uint32_t count;
} Envelope;

// A part of a phrase: an envelope and the notes played with it
typedef struct Part {
    const Envelope* envelope;
    const Note* notes;
    uint32_t count;
} Part;

// A phrase and the entries of the module's pattern lists that point at it
typedef struct Phrase {
    const Part* parts;
    uint32_t part_count;
    uint32_t list_entries;
    uint32_t list_entry_count;
} Phrase;

static const Envelope s_held_envelope = { 0, 44, nullptr, 0 };
static const uint16_t s_decay_values[] = { 64, 59, 54, 49, 44, 40, 35, 30, 25, 20, 15 };
static const Envelope s_decay_envelope = { 0, 64, s_decay_values, nuade_sizeof_array(s_decay_values) };

static const Note s_notes_a[]
    = { { 428, 10 }, { 404, 10 }, { 453, 10 }, { 428, 10 }, { 428, 10 }, { 404, 10 }, { 453, 10 }, { 428, 10 } };
static const Note s_notes_b_first[]
    = { { 428, 24 }, { 381, 32 }, { 360, 32 }, { 381, 40 }, { 428, 24 }, { 381, 32 }, { 360, 32 }, { 320, 40 } };
static const Note s_notes_b_second[]
    = { { 285, 8 }, { 285, 8 }, { 285, 8 }, { 285, 8 }, { 285, 8 }, { 285, 8 }, { 285, 8 }, { 285, 8 }, { 285, 8 },
        { 285, 8 }, { 285, 8 }, { 285, 8 }, { 285, 8 }, { 320, 8 }, { 360, 8 }, { 381, 8 }, { 428, 8 } };
static const Note s_notes_d[] = { { 428, 10 }, { 428, 10 }, { 214, 10 }, { 360, 5 }, { 240, 10 },
                                  { 320, 5 },  { 214, 10 }, { 285, 10 }, { 302, 10 } };

static const Part s_parts_a[] = { { &s_held_envelope, s_notes_a, nuade_sizeof_array(s_notes_a) } };
static const Part s_parts_b[] = { { &s_held_envelope, s_notes_b_first, nuade_sizeof_array(s_notes_b_first) },
                                  { &s_decay_envelope, s_notes_b_second, nuade_sizeof_array(s_notes_b_second) } };
static const Part s_parts_d[] = { { &s_held_envelope, s_notes_d, nuade_sizeof_array(s_notes_d) } };

static const Phrase s_phrases[] = {
    { s_parts_a, nuade_sizeof_array(s_parts_a), 0x2e44, 4 },
    { s_parts_b, nuade_sizeof_array(s_parts_b), 0x261a, 1 },
    { s_parts_d, nuade_sizeof_array(s_parts_d), 0x2f6e, 5 },
};

// Appends data after the built-in tables (a write past the image is dropped)
typedef struct Builder {
    DlImage* image;
    uint32_t at;
} Builder;

static void put_u16(Builder* b, uint16_t value) {
    dli_set16(b->image, b->at, value);
    b->at += 2;
}

static void put_u32(Builder* b, uint32_t value) {
    put_u16(b, (uint16_t)(value >> 16));
    put_u16(b, (uint16_t)value);
}

static uint32_t put_envelope(Builder* b, const Envelope* envelope) {
    const uint32_t start = b->at;
    put_u16(b, envelope->volume_cut);
    put_u16(b, envelope->volume_bonus);
    for (uint32_t i = 0; i < envelope->count; i++) {
        put_u16(b, envelope->values[i]);
    }
    put_u16(b, DL_ENVELOPE_END);
    return start;
}

// The envelopes of the phrase, then its pattern: per part an envelope and a sample command, the notes; the next pattern
// command ends it. Returns the address of the pattern.
static uint32_t put_phrase(Builder* b, const Phrase* phrase) {
    uint32_t envelopes[MAX_PARTS];
    for (uint32_t p = 0; p < phrase->part_count; p++) {
        envelopes[p] = put_envelope(b, phrase->parts[p].envelope);
    }
    const uint32_t start = b->at;
    for (uint32_t p = 0; p < phrase->part_count; p++) {
        const Part* part = &phrase->parts[p];
        put_u16(b, DlCommandWord_Envelope);
        put_u32(b, envelopes[p]);
        put_u16(b, DlCommandWord_Sample);
        put_u32(b, PHRASE_SAMPLE_RECORD);
        for (uint32_t n = 0; n < part->count; n++) {
            put_u16(b, part->notes[n].period);
            put_u16(b, part->notes[n].length);
        }
    }
    put_u16(b, DlCommandWord_NextPattern);
    return start;
}

void dl_patch_midwinter(DlImage* image, uint32_t size_marker, uint32_t subsong_count) {
    if (size_marker != SIGNATURE || subsong_count != SIGNATURE_SUBSONGS) {
        return;
    }
    Builder b = { .image = image, .at = image->builtin_free };
    for (uint32_t i = 0; i < nuade_sizeof_array(s_phrases); i++) {
        const Phrase* phrase = &s_phrases[i];
        const uint32_t pattern = put_phrase(&b, phrase);
        for (uint32_t e = 0; e < phrase->list_entry_count; e++) {
            dli_set32(image, phrase->list_entries + e * 4, pattern);
        }
    }
    image->builtin_free = b.at;
}
