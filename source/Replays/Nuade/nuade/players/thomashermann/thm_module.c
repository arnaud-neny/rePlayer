// SPDX-License-Identifier: MIT
// Thomas Hermann module: the check, the load (relocation, the sample file) and the proof that the songs only use what
// the player models.
//
// File layout (big endian, pointers are relocated by InitPlayer; the offsets below are what they point at):
//   0x00  8 longs: the pattern pointer tables of the 4 voices (0x40 + 0x400 * voice, 256 longs: one pattern per
//         position), then the 4 transposition lists (0x1040 + 0x100 * voice, 256 signed bytes)
//   0x20  loop position, last position, start position, instrument count (1 .. 31)
//   0x24  pointer to the instrument pointer table (0x1440); 0x28 is unused
//   0x29  last row of a pattern; 0x2a timer value of the sequencer interrupt; 0x2c ticks per row, minus one
//   0x2e  the address the module was assembled for (the relocation base)
//   0x32  4 bytes, one per voice: nonzero keeps the voice from starting notes
//   0x40  the pattern pointer tables; 0x1440 the instrument pointers (32 longs); 0x14c0 the instrument records
// A pattern is rows of 3 bytes: note, instrument, flags. The samples are one file that holds the instruments' samples
// back to back, in instrument order, each as long as the long in front of its record says.

#include "thm_internal.h"

#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODULE_PREFIX "thm."
#define SAMPLE_PREFIX "smp."
#define PREFIX_LENGTH 4

#define MIN_MODULE_SIZE 0x1ac1 // the check wants more than 0x1ac0 bytes
#define PATTERN_TABLE 0x40
#define PATTERN_TABLE_SIZE 0x400
#define TRANSPOSE_LIST 0x1040
#define TRANSPOSE_LIST_SIZE 0x100
#define INSTRUMENT_POINTERS 0x1440
#define INSTRUMENT_RECORDS 0x14c0
#define POINTER_COUNT 1024 // the 4 pattern tables, relocated as one block
#define MAX_POSITIONS 256
#define NOTE_COUNT 12
#define MIN_TIMER 1000 // a sequencer interrupt faster than 700 Hz is not modelled

// The module size that tells the replayer how many subsongs the module has: the end of the last pattern.
#define SIZE_FIVE_SUBSONGS 0x2e10
#define SIZE_THREE_SUBSONGS 0x2780

// The replayer's period table, in note index order.
static const uint16_t k_periods[NOTE_COUNT]
    = { 0x1ac, 0x194, 0x17c, 0x168, 0x154, 0x140, 0x12e, 0x11e, 0x10e, 0x0fe, 0x0f0, 0x0e2 };

// The replayer's subsong tables for the two module sizes it knows. Any other module has one subsong, made from its
// header.
static const ThmSubsong k_three_subsongs[3] = {
    { 0x00, 0x6f, 0xfe, 3 },
    { 0x80, 0x87, 0x7f, 3 },
    { 0x89, 0x89, 0x88, 3 },
};
static const ThmSubsong k_five_subsongs[5] = {
    { 0x11, 0x5a, 0xff, 3 }, { 0x60, 0x85, 0x5f, 5 }, { 0x90, 0xb5, 0x8f, 5 },
    { 0xc0, 0xc7, 0xbf, 3 }, { 0xd5, 0xd6, 0xcf, 3 },
};

static ThmHeader read_header(const uint8_t* data) {
    ThmHeader header = { .loop_position = data[0x20],
                         .last_position = data[0x21],
                         .position = data[0x22],
                         .instrument_count = data[0x23],
                         .instrument_table = nuade_read_be32(data + 0x24),
                         .last_row = data[0x29],
                         .timer = nuade_read_be16(data + 0x2a),
                         .speed = data[0x2c],
                         .base = nuade_read_be32(data + 0x2e) };
    for (uint32_t v = 0; v < THM_VOICES; v++) {
        header.voice_off[v] = data[0x32 + v] != 0;
    }
    return header;
}

// Check2 of the eagleplayer: the size, an even positive relocation base and the 8 table pointers in their place.

static bool check_module(const uint8_t* data, size_t size) {
    if (size < MIN_MODULE_SIZE) {
        return false;
    }
    const uint32_t base = read_header(data).base;
    if (base == 0 || (base & 0x80000000u) != 0 || (base & 1) != 0) {
        return false;
    }
    for (uint32_t i = 0; i < 8; i++) {
        const uint32_t expected
            = i < 4 ? PATTERN_TABLE + i * PATTERN_TABLE_SIZE : TRANSPOSE_LIST + (i - 4) * TRANSPOSE_LIST_SIZE;
        if (nuade_read_be32(data + i * 4) - base != expected) {
            return false;
        }
    }
    return true;
}

static bool has_module_prefix(const char* filename) {
    const char* name = filename != nullptr ? nuade_path_basename(filename) : nullptr;
    return name != nullptr && nuade_strncasecmp(name, MODULE_PREFIX, PREFIX_LENGTH) == 0;
}

static uint32_t relocated(const ThmModule* module, uint32_t offset) {
    return nuade_read_be32(module->chip + offset) - module->header.base;
}

uint8_t thm_module_instrument_count(const ThmModule* module) {
    return module->header.instrument_count;
}

uint8_t thm_module_last_row(const ThmModule* module) {
    return module->header.last_row;
}

uint8_t thm_module_row_count(const ThmModule* module) {
    return (uint8_t)(thm_module_last_row(module) + 1);
}

uint16_t thm_module_timer(const ThmModule* module) {
    return module->header.timer;
}

bool thm_module_voice_off(const ThmModule* module, uint32_t voice) {
    return module->header.voice_off[voice % THM_VOICES];
}

ThmSubsong thm_module_subsong(const ThmModule* module, uint32_t subsong) {
    if (module->subsong_count == 5) {
        return k_five_subsongs[subsong % 5];
    }
    if (module->subsong_count == 3) {
        return k_three_subsongs[subsong % 3];
    }
    // InitPlayer copies the header's positions and speed to the table; the start position becomes 0xff unless it is
    // negative
    const ThmHeader* header = &module->header;
    return (ThmSubsong) { .loop_position = header->loop_position,
                          .last_position = header->last_position,
                          .position = (int8_t)header->position >= 0 ? 0xff : header->position,
                          .speed = header->speed };
}

// The callers pass numbers up to the instrument count; a larger one reads the empty record.

ThmInstrument thm_module_instrument(const ThmModule* module, uint8_t number) {
    number = number <= THM_MAX_INSTRUMENTS ? number : 0;
    const uint8_t* r = module->chip + INSTRUMENT_RECORDS + (uint32_t)number * THM_INSTRUMENT_SIZE;
    return (ThmInstrument) { .length = nuade_read_be16(r),
                             .volume = r[2],
                             .type = r[3],
                             .sample = module->sample[number],
                             .loop_offset = nuade_read_be16(r + 8),
                             .loop_bytes = nuade_read_be16(r + 0xa),
                             .base_period = nuade_read_be16(r + 0xc),
                             .flags = r[0x23],
                             .steps = { r[0x24], r[0x25], r[0x26], r[0x27] },
                             .levels = { r[0x28], r[0x29], r[0x2a] },
                             .slide_speed = r[0x2d] };
}

// The pattern of a voice at a position, as an offset of its first row, when all the rows lie in the module.

static bool pattern_offset(const ThmModule* module, uint32_t voice, uint8_t position, uint32_t* out) {
    const uint32_t offset = relocated(module, PATTERN_TABLE + voice * PATTERN_TABLE_SIZE + (uint32_t)position * 4);
    const uint32_t bytes = (uint32_t)thm_module_row_count(module) * 3;
    if (module->module_size < bytes || offset > module->module_size - bytes) {
        return false;
    }
    *out = offset;
    return true;
}

// A row the song cannot reach is never asked for (the proof in module_load); a stray request reads as "no note".

ThmRow thm_module_row(const ThmModule* module, uint32_t voice, uint8_t position, uint8_t row) {
    uint32_t pattern;
    if (!pattern_offset(module, voice, position, &pattern) || row >= thm_module_row_count(module)) {
        return (ThmRow) { .note = ThmCommand_None, .instrument = 0, .slide = false, .no_stop = true };
    }
    const uint8_t* r = module->chip + pattern + (uint32_t)row * 3;
    return (ThmRow) { .note = r[0], .instrument = r[1], .slide = (r[2] & 0x01) != 0, .no_stop = (r[2] & 0x02) != 0 };
}

int8_t thm_module_transposition(const ThmModule* module, uint32_t voice, uint8_t position) {
    return (int8_t)module->chip[TRANSPOSE_LIST + voice * TRANSPOSE_LIST_SIZE + position];
}

const int8_t* thm_module_chip(const ThmModule* module, uint64_t offset) {
    return offset <= module->chip_size ? (const int8_t*)module->chip + offset : nullptr;
}

// The transposition adds to the note index (the high nibble) and carries into the octave class (the low nibble) when
// the index leaves 0..11, with the replayer's 8-bit arithmetic. Commands pass.

uint8_t thm_transpose_note(uint8_t note, int8_t transposition) {
    if (note >= THM_COMMAND_NOTES) {
        return note;
    }
    int8_t index = (int8_t)(uint8_t)((note >> 4) + (uint8_t)transposition);
    uint8_t octave = note;
    if (index < 0) {
        index = (int8_t)(uint8_t)(index - 4);
        octave = (uint8_t)(octave - 1);
    } else if (index >= NOTE_COUNT) {
        index = (int8_t)(uint8_t)(index + 4);
        octave = (uint8_t)(octave + 1);
    }
    return (uint8_t)(((uint8_t)index << 4) | (octave & 0x0f));
}

// A sample that holds the next octave's data after its own is played from the second part with twice the length (the
// replayer's shifts, in the widths it uses).

static void double_sample(ThmNote* n) {
    n->sample += n->length;
    n->length = (uint16_t)(n->length << 1);
    n->loop_offset <<= 1;
    n->loop_bytes = (uint16_t)(n->loop_bytes << 1);
    n->loop_offset_carry = (uint16_t)(n->loop_offset >> 16);
}

static void quadruple_sample(ThmNote* n) {
    n->sample += (uint64_t)n->length * 3;
    n->length = (uint16_t)(n->length << 2);
    n->loop_offset <<= 2;
    n->loop_bytes = (uint16_t)(n->loop_bytes << 2);
    n->loop_offset_carry = (uint16_t)(n->loop_offset >> 16);
}

bool thm_note_build(uint8_t note, const ThmInstrument* instrument, ThmNote* out) {
    *out = (ThmNote) { .length = instrument->length,
                       .volume = instrument->volume,
                       .flags = instrument->flags,
                       .sample = instrument->sample,
                       .loop_offset = instrument->loop_offset,
                       .loop_bytes = instrument->loop_bytes,
                       .base_period = instrument->base_period };
    const bool retrigger = note == ThmCommand_Retrigger;
    if (note >= THM_COMMAND_NOTES && !retrigger) {
        return false;
    }
    const uint8_t octave = note & 0x0f;
    uint16_t period = retrigger ? 0 : k_periods[(note >> 4) % NOTE_COUNT];
    out->period = period;
    if (instrument->type == 1) {
        if (!retrigger) {
            switch (octave) {
                case 1:
                    break;
                case 2:
                    period >>= 1;
                    out->period = period;
                    if (period <= 0x82) {
                        return false;
                    }
                    break;
                case 0:
                    period = (uint16_t)(period << 1);
                    break;
                case 0xf:
                    period = (uint16_t)(period << 2);
                    break;
                default:
                    return false;
            }
        }
        out->period = period;
        return true;
    }
    if (retrigger || octave == 1) {
        double_sample(out);
        return true;
    }
    switch (octave) {
        case 3:
            period >>= 1;
            out->period = period;
            if (period <= 0x82) {
                return false;
            }
            break;
        case 2:
            break;
        case 0xf:
            period = (uint16_t)(period << 1);
            quadruple_sample(out);
            break;
        case 0:
            quadruple_sample(out);
            break;
        default:
            return false;
    }
    out->period = period;
    return true;
}

// The sample file: the module's name with "thm." changed to "smp.", next to it.

static uint8_t* read_samples(const char* path, const NuadeIo* io, size_t* size) {
    char dir[1024] = { 0 };
    char name[512];
    const char* base = path != nullptr ? nuade_path_basename(path) : nullptr;
    if (base == nullptr || io == nullptr || nuade_strncasecmp(base, MODULE_PREFIX, PREFIX_LENGTH) != 0
        || strlen(base) + 1 > sizeof(name)) {
        return nullptr;
    }
    if (!nuade_path_parent(path, dir, sizeof(dir))) {
        snprintf(dir, sizeof(dir), "."); // a bare file name: the current directory
    }
    snprintf(name, sizeof(name), SAMPLE_PREFIX "%s", base + PREFIX_LENGTH);
    const char* dirs[] = { dir };
    NuadeFileIndex* index = nuade_file_index_create(io, dirs, nuade_sizeof_array(dirs));
    const char* url = index != nullptr ? nuade_file_index_find(index, name) : nullptr;
    uint8_t* copy = nullptr;
    if (url != nullptr) {
        const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
        if (file.data != nullptr) {
            copy = malloc(file.size != 0 ? (size_t)file.size : 1);
            if (copy != nullptr) {
                memcpy(copy, file.data, (size_t)file.size);
                *size = (size_t)file.size;
            }
            NuadeIo_free_url_to_memory(io, file.data);
        }
    }
    nuade_file_index_destroy(index);
    return copy;
}

// InitPlayer's size estimate: the end of the last pattern. The replayer refuses a module smaller than that.

static uint32_t estimated_size(const uint8_t* data, uint32_t base, uint32_t instrument_count) {
    int32_t end = (int32_t)instrument_count - 1;
    for (uint32_t i = 0; i < POINTER_COUNT; i++) {
        const int32_t offset = (int32_t)(nuade_read_be32(data + PATTERN_TABLE + i * 4) - base);
        if (offset > end) {
            end = offset;
        }
    }
    return (uint32_t)end + 0x40;
}

// The pointer tables are where the replayer expects them: the instrument table in the header, and every instrument
// record where the sample loader walks (it steps 0x30 bytes from the first record, the player reads them through the
// table).

static bool pointers_in_place(const ThmModule* module) {
    if (module->header.instrument_table - module->header.base != INSTRUMENT_POINTERS) {
        return false;
    }
    for (uint32_t i = 0; i <= THM_MAX_INSTRUMENTS; i++) {
        if (relocated(module, INSTRUMENT_POINTERS + i * 4) != INSTRUMENT_RECORDS + i * THM_INSTRUMENT_SIZE) {
            return false;
        }
    }
    return true;
}

// The sample of instrument n starts where the lengths of instruments 1..n-1 end. The length is the long that ends at
// the record's first word: the record's own length word, and in front of it the last word of the previous record.

static void place_samples(ThmModule* module, uint32_t instrument_count) {
    module->sample[0] = module->chip_size; // the empty record points at address 0: zeros here
    uint64_t at = module->module_size;
    for (uint32_t n = 1; n <= instrument_count; n++) {
        // a placement that does not fit 32 bits is off the files: it saturates and the note check refuses it
        module->sample[n] = at > UINT32_MAX ? UINT32_MAX : (uint32_t)at;
        at += nuade_read_be32(module->chip + INSTRUMENT_RECORDS + n * THM_INSTRUMENT_SIZE - 2);
    }
}

// The Paula buffers a note plays lie in the module or the sample file, and the lengths are not 0.

static bool note_in_chip(const ThmModule* module, const ThmNote* n) {
    const uint32_t words = n->length >> 1;
    if (words == 0 || n->sample > module->chip_size || (uint64_t)words * 2 > module->chip_size - n->sample) {
        return false;
    }
    if (n->flags == 0) {
        return true;
    }
    const uint64_t loop = n->sample + n->loop_offset;
    const uint32_t loop_words = n->loop_bytes >> 1;
    return loop_words != 0 && loop <= module->chip_size && (uint64_t)loop_words * 2 <= module->chip_size - loop;
}

// The replayer runs the four voices of a row one after the other, and a voice's instrument number goes into the low
// byte of the value it forms the instrument-table index from: what an earlier voice left above that byte is part of
// the next voice's index.

typedef struct ThmIndexLeftover {
    uint16_t high;  // bits 16..31
    uint8_t middle; // bits 8..15
} ThmIndexLeftover;

static ThmIndexLeftover leftover_of(uint32_t value) {
    return (ThmIndexLeftover) { .high = (uint16_t)(value >> 16), .middle = (uint8_t)(value >> 8) };
}

// The table index of the instrument record for a voice: the shift by 2 acts on the low 16 bits only.

static uint32_t table_index(ThmIndexLeftover leftover, uint8_t instrument) {
    const uint32_t low = ((uint32_t)leftover.middle << 8) | instrument;
    return ((uint32_t)leftover.high << 16) | ((low << 2) & 0xffffu);
}

// The index is the instrument number times 4: the record is the instrument's.

static bool index_is_clean(ThmIndexLeftover leftover, uint8_t instrument) {
    return table_index(leftover, instrument) == (uint32_t)instrument * 4;
}

// A row is modelled when every voice that looks up an instrument finds it through a clean index (a stray index is
// an address the replayer would read as a pointer), every note it starts has an instrument and a note index inside
// the tables, and, with the sample file, the sample and loop it plays lie in the module or the sample file.
// `leftover` is what the previous voices left for this one and is updated to what this one leaves.

static bool voice_is_modelled(const ThmModule* module, ThmRow row, ThmIndexLeftover* leftover, bool with_samples) {
    if (row.note == ThmCommand_None || row.note == ThmCommand_Release || row.note == ThmCommand_Stop) {
        return true; // no lookup: the leftover stays
    }
    if (row.instrument > THM_MAX_INSTRUMENTS || !index_is_clean(*leftover, row.instrument)) {
        return false;
    }
    *leftover = (ThmIndexLeftover) { 0 };
    if (row.note >= THM_COMMAND_NOTES && row.note != ThmCommand_Retrigger) {
        return true; // no note: the replayer returns once it has looked at the record
    }
    if (row.instrument > module->header.instrument_count) {
        return false; // the sample pointer was never set
    }
    if (row.note != ThmCommand_Retrigger && (row.note >> 4) >= NOTE_COUNT) {
        return false; // the replayer reads its period from the data behind the table
    }
    const ThmInstrument record = thm_module_instrument(module, row.instrument);
    ThmNote started;
    if (!thm_note_build(row.note, &record, &started)) {
        *leftover = leftover_of(started.period); // it returns with the period it worked out
        return true;
    }
    // The envelope start without an attack time is the one exit that keeps the upper half
    if ((record.flags & THM_ENVELOPE_FLAG) != 0 && record.steps[0] == 0) {
        *leftover = (ThmIndexLeftover) { .high = started.loop_offset_carry };
    }
    if (row.instrument == 0) {
        return record.volume == 0; // the empty record: its note plays silence from nowhere
    }
    return !with_samples || note_in_chip(module, &started);
}

// Every position the song plays: from its start to the last position, then from the loop position on, round and round.

static bool subsong_is_modelled(const ThmModule* module, ThmSubsong subsong, bool with_samples) {
    bool visited[MAX_POSITIONS] = { false };
    uint8_t position = subsong.position;
    while (!visited[position]) {
        visited[position] = true;
        position = position == subsong.last_position ? subsong.loop_position : (uint8_t)(position + 1);
    }
    for (uint32_t p = 0; p < MAX_POSITIONS; p++) {
        if (!visited[p]) {
            continue;
        }
        for (uint32_t voice = 0; voice < THM_VOICES; voice++) {
            uint32_t pattern;
            if (!pattern_offset(module, voice, (uint8_t)p, &pattern)) {
                return false;
            }
        }
        for (uint32_t row = 0; row < thm_module_row_count(module); row++) {
            ThmIndexLeftover leftover = { 0 }; // every row starts with nothing left over
            for (uint32_t voice = 0; voice < THM_VOICES; voice++) {
                if (thm_module_voice_off(module, voice)) {
                    continue;
                }
                ThmRow r = thm_module_row(module, voice, (uint8_t)p, (uint8_t)row);
                r.note = thm_transpose_note(r.note, thm_module_transposition(module, voice, (uint8_t)p));
                if (!voice_is_modelled(module, r, &leftover, with_samples)) {
                    return false;
                }
            }
        }
    }
    return true;
}

void thm_module_free(ThmModule* module) {
    if (module != nullptr) {
        free(module->chip);
        free(module);
    }
}

static bool header_is_modelled(const ThmModule* module) {
    const uint32_t instrument_count = module->header.instrument_count;
    if (instrument_count == 0 || instrument_count > THM_MAX_INSTRUMENTS) {
        return false;
    }
    if (thm_module_last_row(module) == 0xff) {
        return false; // the replayer divides by the row count: zero
    }
    if (thm_module_timer(module) < MIN_TIMER) {
        return false;
    }
    return pointers_in_place(module)
           && estimated_size(module->chip, module->header.base, instrument_count) <= module->module_size;
}

// The module and its sample file (none: the probe works on the module alone) in one padded copy.

static ThmModule* build_module(const uint8_t* data, size_t size, const uint8_t* samples, size_t sample_size,
                               ThmStatus* status) {
    ThmModule* module = calloc(1, sizeof(ThmModule));
    uint8_t* joined = malloc(size + sample_size);
    if (module == nullptr || joined == nullptr || size + sample_size > UINT32_MAX) {
        free(module);
        free(joined);
        *status = ThmStatus_OutOfMemory;
        return nullptr;
    }
    memcpy(joined, data, size);
    if (sample_size != 0) {
        memcpy(joined + size, samples, sample_size);
    }
    module->chip = nuade_paula_padded_copy(joined, size + sample_size);
    free(joined);
    if (module->chip == nullptr) {
        free(module);
        *status = ThmStatus_OutOfMemory;
        return nullptr;
    }
    module->module_size = (uint32_t)size;
    module->chip_size = (uint32_t)(size + sample_size);
    module->header = read_header(data);
    return module;
}

// The header and tables are acceptable; then the subsong count and the sample placement follow from them.

static bool lay_out_module(ThmModule* module) {
    if (!header_is_modelled(module)) {
        return false;
    }
    const uint32_t instrument_count = module->header.instrument_count;
    const uint32_t end = estimated_size(module->chip, module->header.base, instrument_count);
    module->subsong_count = end == SIZE_FIVE_SUBSONGS ? 5 : end == SIZE_THREE_SUBSONGS ? 3 : 1;
    place_samples(module, instrument_count);
    return true;
}

// The rows of every subsong of a laid out module are ones the player models. With the sample file the notes' samples
// are checked too.

static bool module_is_modelled(const ThmModule* module, bool with_samples) {
    for (uint32_t s = 0; s < module->subsong_count; s++) {
        if (!subsong_is_modelled(module, thm_module_subsong(module, s), with_samples)) {
            return false;
        }
    }
    return true;
}

// The eagleplayer's check, its file name rule and everything the module alone makes the port refuse; only the
// sample file is left to the open (a missing "smp." file cannot be seen without io).

bool thm_probe(const uint8_t* data, size_t size, const char* filename) {
    if (!has_module_prefix(filename) || !check_module(data, size)) {
        return false;
    }
    ThmStatus status;
    ThmModule* module = build_module(data, size, nullptr, 0, &status);
    const bool modelled = module != nullptr && lay_out_module(module) && module_is_modelled(module, false);
    thm_module_free(module);
    return modelled;
}

ThmModule* thm_module_load(const uint8_t* data, size_t size, const char* path, const NuadeIo* io, ThmStatus* status) {
    *status = ThmStatus_InvalidData;
    if (!has_module_prefix(path) || !check_module(data, size)) {
        return nullptr;
    }
    size_t sample_size = 0;
    uint8_t* samples = read_samples(path, io, &sample_size);
    if (samples == nullptr) {
        *status = ThmStatus_MissingSamples;
        return nullptr;
    }
    ThmModule* module = build_module(data, size, samples, sample_size, status);
    free(samples);
    if (module == nullptr) {
        return nullptr;
    }
    if (!lay_out_module(module) || !module_is_modelled(module, true)) {
        thm_module_free(module);
        *status = ThmStatus_InvalidData;
        return nullptr;
    }
    *status = ThmStatus_Ok;
    return module;
}
