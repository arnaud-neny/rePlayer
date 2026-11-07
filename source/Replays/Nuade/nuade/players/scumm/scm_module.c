// SPDX-License-Identifier: MIT
// SCUMM sound drivers: finds the driver version in a module, places its song data and proves at load time that every
// byte the driver can read or write is inside the module and is not one of the working fields of the instrument
// records (the port keeps those apart from the module image).

#include "scm_internal.h"

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define SCM_FNV_OFFSET 0xCBF29CE484222325ULL
#define SCM_FNV_PRIME 0x100000001B3ULL
#define SCM_BRANCH_OPCODE 0x6000u // bra.w
#define SCM_ENTRY 4u              // the eagleplayer calls the module here
#define SCM_SAMPLE_BASE_OFFSET 2u // the signed word that places the samples
#define SCM_WORD_BYTES 2u
#define SCM_EVENT_READ_BYTES 10u // the words the driver reads of an event

#define SCM_ALARM_SAMPLES 2u // the alarm alternates two halves of the waveform

// Byte offsets of the instrument record's fields.
typedef enum ScmRecordField {
    ScmRecordField_VolumeEnvelope = 0x00,
    ScmRecordField_VolumePosition = 0x02, // the working fields: this, the duration (0x04) and the phase (0x06)
    ScmRecordField_PitchOffset = 0x0A,
    ScmRecordField_PitchEnvelope = 0x0C,
    ScmRecordField_PitchPosition = 0x0E,
    ScmRecordField_LoopBytes = 0x10,
    ScmRecordField_StartOffset = 0x14,
    ScmRecordField_LoopOffset = 0x16,
    ScmRecordField_StartBytes = 0x18,
    ScmRecordField_Channel = 0x1A,
} ScmRecordField;

// Byte offsets of an event's words.
typedef enum ScmEventField {
    ScmEventField_Time = 0,
    ScmEventField_PitchOffset = 2,
    ScmEventField_Duration = 4,
    ScmEventField_Channel = 6,
    ScmEventField_Instrument = 8,
} ScmEventField;

#define SCM_RECORD_WORKING_WORDS 3 // volume position, duration, phase
#define SCM_ZERO_WRITE_BYTES 4u
// UADE loads the module at 0xF000 and its chip memory behind the module is zero up to 0x7F310 at least: a long cleared
// at a module offset below 0x70000 and behind the module changes nothing.
#define SCM_ZERO_MEMORY_END 0x70000u

// Operand offsets of the known versions, from the code's first byte. The four-byte versions place the song data with
// adda.l immediates, the mm ones with adda.w.
#define SCM_OPERANDS_LONG 4
#define SCM_OPERANDS_WORD 2

static const ScmVersion scm_versions[] = {
    {
        .name = "Alarm",
        .driver = ScmDriver_Alarm,
        .code_len = 0x13E,
        .code_hash = 0x8A4B44B67F2C0F67ULL,
    },
    {
        .name = "Teleport out",
        .driver = ScmDriver_TeleportDown,
        .code_len = 0xDA,
        .code_hash = 0x089DB6E270813F68ULL,
    },
    {
        .name = "Teleport in",
        .driver = ScmDriver_TeleportUp,
        .code_len = 0xDA,
        .code_hash = 0xE272429CCD5C1892ULL,
    },
    {
        .name = "Duo, restarting",
        .driver = ScmDriver_Duo,
        .code_len = 0x43C,
        .code_hash = 0x5AB476C2FA848124ULL,
        .first_track_end = ScmTrackEnd_Restart,
        .operand_bytes = SCM_OPERANDS_LONG,
        .operand_at = { [ScmOperand_Records] = 10,
                        [ScmOperand_Envelopes] = 16,
                        [ScmOperand_Events0] = 22,
                        [ScmOperand_Events1] = 160,
                        [ScmOperand_Events2] = 246,
                        [ScmOperand_Events3] = 332 },
    },
    {
        .name = "Duo, rewinding",
        .driver = ScmDriver_Duo,
        .code_len = 0x44E,
        .code_hash = 0x48B8B327D3613DC8ULL,
        .first_track_end = ScmTrackEnd_RestartReturn,
        .operand_bytes = SCM_OPERANDS_LONG,
        .operand_at = { [ScmOperand_Records] = 10,
                        [ScmOperand_Envelopes] = 16,
                        [ScmOperand_Events0] = 22,
                        [ScmOperand_Events1] = 170,
                        [ScmOperand_Events2] = 256,
                        [ScmOperand_Events3] = 342,
                        [ScmOperand_ZeroWrite] = 692 },
    },
    {
        .name = "Duo, stopping",
        .driver = ScmDriver_Duo,
        .code_len = 0x436,
        .code_hash = 0xA7AE5BE0566A63C3ULL,
        .first_track_end = ScmTrackEnd_Freeze,
        .operand_bytes = SCM_OPERANDS_LONG,
        .operand_at = { [ScmOperand_Records] = 10,
                        [ScmOperand_Envelopes] = 16,
                        [ScmOperand_Events0] = 22,
                        [ScmOperand_Events1] = 154,
                        [ScmOperand_Events2] = 240,
                        [ScmOperand_Events3] = 326 },
    },
    {
        .name = "Quad",
        .driver = ScmDriver_Quad,
        .code_len = 0x490,
        .code_hash = 0xF020E68461D9414EULL,
        .first_track_end = ScmTrackEnd_Freeze,
        .operand_bytes = SCM_OPERANDS_LONG,
        .operand_at = { [ScmOperand_Records] = 10,
                        [ScmOperand_Envelopes] = 16,
                        [ScmOperand_Events0] = 22,
                        [ScmOperand_Events1] = 166,
                        [ScmOperand_Events2] = 252,
                        [ScmOperand_Events3] = 338 },
    },
    {
        .name = "Quad, restarting channels",
        .driver = ScmDriver_Quad,
        .via_branch = true,
        .code_len = 0x45E,
        .code_hash = 0xFFC48EFEE2E8E3A3ULL,
        .first_track_end = ScmTrackEnd_Freeze,
        .stops_dma_on_start = true,
        .operand_bytes = SCM_OPERANDS_WORD,
        .operand_at = { [ScmOperand_Records] = 10,
                        [ScmOperand_Envelopes] = 14,
                        [ScmOperand_Events0] = 18,
                        [ScmOperand_Events1] = 142,
                        [ScmOperand_Events2] = 222,
                        [ScmOperand_Events3] = 302 },
    },
};

// The two-channel driver writes 0xFF to byte 0x10 + instrument of its work area when a note has faded out: only the
// first two are its channel bytes, the next ones are the eagleplayer's Check2 code, which is never run again; behind
// that lies code that is.

#define SCM_DUO_INSTRUMENTS 0x38u

static const ScmEngine scm_engines[] = {
    [ScmDriver_Duo]
    = { .tracker = true, .channels = 2, .instruments = SCM_DUO_INSTRUMENTS, .first_voice = 2, .two_channel = true },
    [ScmDriver_Quad] = { .tracker = true, .channels = 4, .instruments = SCM_MAX_INSTRUMENTS },
};

const ScmEngine* scm_engine(ScmDriver driver) {
    static const ScmEngine sweep = { 0 };
    return driver == ScmDriver_Duo || driver == ScmDriver_Quad ? &scm_engines[driver] : &sweep;
}

static bool scm_range_in(uint32_t size, int64_t at, int64_t bytes) {
    return at >= 0 && bytes >= 0 && at <= (int64_t)size && bytes <= (int64_t)size - at;
}

static uint16_t scm_u16(const ScmModule* module, int64_t at) {
    return scm_range_in(module->size, at, SCM_WORD_BYTES) ? nuade_read_be16(module->image + at) : 0;
}

static uint32_t scm_u32(const ScmModule* module, int64_t at) {
    return scm_range_in(module->size, at, sizeof(uint32_t)) ? nuade_read_be32(module->image + at) : 0;
}

// Event addresses: the position is shifted by 4 in a word that the driver then sign-extends.

static int64_t scm_event_offset(const ScmModule* module, uint32_t track, uint16_t position) {
    return (int64_t)module->events[track] + (int16_t)(uint16_t)(position << 4);
}

ScmEvent scm_module_event(const ScmModule* module, uint32_t track, uint16_t position) {
    const int64_t at = scm_event_offset(module, track, position);
    return (ScmEvent) {
        .time = (int16_t)scm_u16(module, at + ScmEventField_Time),
        .pitch_offset = scm_u16(module, at + ScmEventField_PitchOffset),
        .duration = (int16_t)scm_u16(module, at + ScmEventField_Duration),
        .channel = scm_u16(module, at + ScmEventField_Channel),
        .instrument = scm_u16(module, at + ScmEventField_Instrument),
    };
}

static int64_t scm_record_offset(const ScmModule* module, uint32_t index) {
    return (int64_t)module->records + (int64_t)index * SCM_RECORD_BYTES;
}

ScmInstrument scm_module_instrument(const ScmModule* module, uint32_t index) {
    const int64_t at = scm_record_offset(module, index);
    return (ScmInstrument) {
        .volume_envelope = scm_u16(module, at + ScmRecordField_VolumeEnvelope),
        .pitch_envelope = scm_u16(module, at + ScmRecordField_PitchEnvelope),
        .start_offset = (int16_t)scm_u16(module, at + ScmRecordField_StartOffset),
        .start_bytes = scm_u16(module, at + ScmRecordField_StartBytes),
        .loop_offset = (int16_t)scm_u16(module, at + ScmRecordField_LoopOffset),
        .loop_bytes = scm_u16(module, at + ScmRecordField_LoopBytes),
    };
}

static int64_t scm_envelope_offset(const ScmModule* module, uint16_t envelope, uint16_t position) {
    return (int64_t)module->envelopes + (int16_t)(uint16_t)((uint16_t)(envelope << 9) + 2u * position);
}

uint16_t scm_module_envelope(const ScmModule* module, uint16_t envelope, uint16_t position) {
    return scm_u16(module, scm_envelope_offset(module, envelope, position));
}

uint32_t scm_module_sample_offset(const ScmModule* module, int16_t offset) {
    return (uint32_t)((int64_t)module->samples + offset);
}

const int8_t* scm_module_sample(const ScmModule* module, uint32_t sample_offset) {
    return (const int8_t*)(module->image + (sample_offset & ~1u));
}

bool scm_module_zero_write(const ScmModule* module, uint32_t sample_offset, uint32_t* at) {
    if (module->version->operand_at[ScmOperand_ZeroWrite] == 0) {
        return false;
    }
    *at = (uint32_t)((int64_t)sample_offset + module->zero_write);
    return true;
}

void scm_module_clear_long(ScmModule* module, uint32_t at) {
    if (scm_range_in(module->size, at, SCM_ZERO_WRITE_BYTES)) {
        memset(module->image + at, 0, SCM_ZERO_WRITE_BYTES);
    }
}

// What the load-time proof collects: the instruments the events use, a map of the module's bytes and the longs the
// driver clears.

typedef enum ScmMark {
    ScmMark_Working = 1, // a working field of an instrument record: the port keeps it outside the module image
    ScmMark_Fixed = 2,   // read by the driver or the eagleplayer as something else than sample data
} ScmMark;

typedef struct ScmClear {
    uint32_t at;
    bool may_leave_module; // the clear may land behind the module, in chip memory that is zero
} ScmClear;

typedef struct ScmCheck {
    bool used[SCM_MAX_INSTRUMENTS];
    uint8_t* marks; // module size bytes, ScmMark bits
    uint32_t event_count;
    uint32_t events[SCM_TRACKS * SCM_MAX_TRACK_EVENTS]; // module offsets of the events read
    uint32_t clear_count;
    ScmClear clears[2 * SCM_MAX_INSTRUMENTS];
} ScmCheck;

// Marks bytes. A working field may not lie on a fixed byte nor the other way round: the port reads the fixed bytes from
// the module once, and the working fields from its own state.

static bool scm_mark(ScmCheck* check, uint32_t size, int64_t at, uint32_t bytes, ScmMark mark) {
    if (!scm_range_in(size, at, bytes)) {
        return false;
    }
    const uint8_t conflict = mark == ScmMark_Working ? ScmMark_Fixed : ScmMark_Working;
    for (uint32_t i = 0; i < bytes; i++) {
        if ((check->marks[at + i] & conflict) != 0) {
            return false;
        }
    }
    for (uint32_t i = 0; i < bytes; i++) {
        check->marks[at + i] |= (uint8_t)mark;
    }
    return true;
}

// A read of sample data: inside the module and clear of every working field.

static bool scm_touch_ok(const ScmCheck* check, uint32_t size, int64_t at, int64_t bytes) {
    if (!scm_range_in(size, at, bytes)) {
        return false;
    }
    for (int64_t i = 0; i < bytes; i++) {
        if ((check->marks[at + i] & ScmMark_Working) != 0) {
            return false;
        }
    }
    return true;
}

// A clear of a long: only sample data may change (the port reads everything else once at load). An odd address is an
// address error on the 68000. Behind the module the chip memory is zero, so the clear changes nothing and is dropped.

static bool scm_clear_ok(const ScmCheck* check, uint32_t size, const ScmClear* clear) {
    if (clear->at % 2 != 0) {
        return false;
    }
    if (clear->at >= size && clear->may_leave_module) {
        return clear->at <= SCM_ZERO_MEMORY_END - SCM_ZERO_WRITE_BYTES;
    }
    if (!scm_range_in(size, clear->at, SCM_ZERO_WRITE_BYTES)) {
        return false;
    }
    for (uint32_t i = 0; i < SCM_ZERO_WRITE_BYTES; i++) {
        if (check->marks[clear->at + i] != 0) {
            return false;
        }
    }
    return true;
}

// Walks the event list of every track as the driver does. A list that does not end, an event outside the module, a
// channel or instrument the driver would use as an index outside its tables, or a list that restarts and never waits
// are refused.

static bool scm_check_tracks(const ScmModule* module, ScmCheck* check) {
    const ScmEngine* engine = module->engine;
    for (uint32_t t = 0; t < SCM_TRACKS; t++) {
        bool waits = false; // some event is later than time 0: a restarted track stops reading events
        bool ended = false;
        for (uint32_t p = 0; p < SCM_MAX_TRACK_EVENTS && !ended; p++) {
            const int64_t at = scm_event_offset(module, t, (uint16_t)p);
            if (!scm_range_in(module->size, at, SCM_EVENT_READ_BYTES)) {
                return false;
            }
            const ScmEvent event = scm_module_event(module, t, (uint16_t)p);
            check->events[check->event_count++] = (uint32_t)at;
            waits = waits || event.time > 0;
            if (event.pitch_offset == SCM_EVENT_END) {
                ended = true;
            } else if (event.channel >= engine->channels || event.instrument >= engine->instruments) {
                return false;
            } else {
                check->used[event.instrument] = true;
            }
        }
        const bool restarts = t > 0 || module->version->first_track_end == ScmTrackEnd_Restart;
        if (!ended || (restarts && !waits)) {
            return false;
        }
    }
    return true;
}

// The words of a sample: at least one, whole, inside the module and clear of the working fields.

static bool scm_check_sample(const ScmModule* module, const ScmCheck* check, int16_t offset, uint16_t bytes) {
    const uint32_t words = bytes >> 1;
    const uint32_t at = scm_module_sample_offset(module, offset) & ~1u;
    return words != 0 && scm_touch_ok(check, module->size, at, (int64_t)words * 2);
}

// What a used instrument makes the driver read and clear. The clears are collected: they are checked when every
// fixed byte is known.

static bool scm_check_instrument(const ScmModule* module, ScmCheck* check, uint32_t index) {
    const ScmInstrument instrument = scm_module_instrument(module, index);
    const int64_t at = scm_record_offset(module, index);
    // The record fields the driver only reads.
    const uint32_t read_fields[]
        = { ScmRecordField_VolumeEnvelope, ScmRecordField_PitchEnvelope, ScmRecordField_LoopBytes,
            ScmRecordField_StartOffset,    ScmRecordField_LoopOffset,    ScmRecordField_StartBytes };
    for (uint32_t i = 0; i < nuade_sizeof_array(read_fields); i++) {
        if (!scm_mark(check, module->size, at + read_fields[i], SCM_WORD_BYTES, ScmMark_Fixed)) {
            return false;
        }
    }
    if (!scm_check_sample(module, check, instrument.start_offset, instrument.start_bytes)
        || !scm_check_sample(module, check, instrument.loop_offset, instrument.loop_bytes)) {
        return false;
    }
    uint32_t zero_at = 0;
    if (scm_module_zero_write(module, scm_module_sample_offset(module, instrument.start_offset), &zero_at)) {
        check->clears[check->clear_count++] = (ScmClear) { .at = zero_at, .may_leave_module = true };
    }
    if (!module->engine->two_channel && instrument.loop_bytes >> 1 == 1) {
        const uint32_t loop = scm_module_sample_offset(module, instrument.loop_offset);
        check->clears[check->clear_count++] = (ScmClear) { .at = loop };
    }
    for (uint32_t position = 0; position < SCM_ENVELOPE_POSITIONS; position++) {
        const uint16_t envelopes[] = { instrument.volume_envelope, instrument.pitch_envelope };
        for (uint32_t e = 0; e < nuade_sizeof_array(envelopes); e++) {
            const int64_t word = scm_envelope_offset(module, envelopes[e], (uint16_t)position);
            if (!scm_mark(check, module->size, word, SCM_WORD_BYTES, ScmMark_Fixed)) {
                return false;
            }
        }
    }
    return true;
}

// The bytes the eagleplayer and the driver read as something else than sample data besides the song's records: the word
// that places the samples and the code (the port reads both once).

static bool scm_mark_code(const ScmModule* module, ScmCheck* check) {
    return scm_mark(check, module->size, SCM_SAMPLE_BASE_OFFSET, SCM_WORD_BYTES, ScmMark_Fixed)
           && scm_mark(check, module->size, SCM_ENTRY, SCM_WORD_BYTES * 2, ScmMark_Fixed)
           && scm_mark(check, module->size, module->entry, module->version->code_len, ScmMark_Fixed);
}

// The working fields of the used instruments' records.

static bool scm_mark_working_fields(const ScmModule* module, ScmCheck* check) {
    for (uint32_t i = 0; i < module->engine->instruments; i++) {
        if (!check->used[i]) {
            continue;
        }
        const int64_t at = scm_record_offset(module, i);
        if (!scm_range_in(module->size, at, SCM_RECORD_BYTES)
            || !scm_mark(check, module->size, at + ScmRecordField_VolumePosition,
                         SCM_RECORD_WORKING_WORDS * SCM_WORD_BYTES, ScmMark_Working)
            || !scm_mark(check, module->size, at + ScmRecordField_PitchOffset, SCM_WORD_BYTES, ScmMark_Working)
            || !scm_mark(check, module->size, at + ScmRecordField_PitchPosition, SCM_WORD_BYTES, ScmMark_Working)
            || !scm_mark(check, module->size, at + ScmRecordField_Channel, SCM_WORD_BYTES, ScmMark_Working)) {
            return false;
        }
    }
    return true;
}

// Everything a tracker driver can touch.

static bool scm_check_tracker_data(const ScmModule* module, ScmCheck* check) {
    if (!scm_mark_code(module, check) || !scm_check_tracks(module, check) || !scm_mark_working_fields(module, check)) {
        return false;
    }
    for (uint32_t e = 0; e < check->event_count; e++) {
        if (!scm_mark(check, module->size, check->events[e], SCM_EVENT_READ_BYTES, ScmMark_Fixed)) {
            return false;
        }
    }
    for (uint32_t i = 0; i < module->engine->instruments; i++) {
        if (check->used[i] && !scm_check_instrument(module, check, i)) {
            return false;
        }
    }
    for (uint32_t c = 0; c < check->clear_count; c++) {
        if (!scm_clear_ok(check, module->size, &check->clears[c])) {
            return false;
        }
    }
    return true;
}

static bool scm_check_tracker(const ScmModule* module) {
    ScmCheck* check = calloc(1, sizeof(*check));
    if (check == nullptr) {
        return false;
    }
    check->marks = calloc(1, module->size);
    const bool ok = check->marks != nullptr && scm_check_tracker_data(module, check);
    free(check->marks);
    free(check);
    return ok;
}

// The sweep effects play 16 words from the sample base (the alarm also from 32 bytes behind it).

static bool scm_check_sweep(const ScmModule* module) {
    const uint32_t samples = module->version->driver == ScmDriver_Alarm ? SCM_ALARM_SAMPLES : 1u;
    return scm_range_in(module->size, (int64_t)(module->samples & ~1u), (int64_t)samples * SCM_SWEEP_SAMPLE_BYTES);
}

static uint64_t scm_code_hash(const ScmModule* module, const ScmVersion* version, uint32_t entry) {
    uint8_t* code = malloc(version->code_len);
    if (code == nullptr) {
        return 0;
    }
    memcpy(code, module->image + entry, version->code_len);
    for (uint32_t i = 0; i < ScmOperand_Count; i++) {
        const uint32_t at = version->operand_at[i];
        if (at != 0) {
            memset(code + at, 0, i == ScmOperand_ZeroWrite ? SCM_WORD_BYTES : version->operand_bytes);
        }
    }
    uint64_t hash = SCM_FNV_OFFSET;
    for (uint32_t i = 0; i < version->code_len; i++) {
        hash = (hash ^ code[i]) * SCM_FNV_PRIME;
    }
    free(code);
    return hash;
}

// The driver version whose code starts at the module's entry.

static const ScmVersion* scm_find_version(const ScmModule* module, uint32_t* entry_out) {
    const bool branch = scm_u16(module, SCM_ENTRY) == SCM_BRANCH_OPCODE;
    const int64_t entry
        = branch ? (int64_t)SCM_ENTRY + SCM_WORD_BYTES + (int16_t)scm_u16(module, SCM_ENTRY + 2) : (int64_t)SCM_ENTRY;
    for (uint32_t v = 0; v < nuade_sizeof_array(scm_versions); v++) {
        const ScmVersion* version = &scm_versions[v];
        if (version->via_branch != branch || !scm_range_in(module->size, entry, version->code_len)) {
            continue;
        }
        if (scm_code_hash(module, version, (uint32_t)entry) == version->code_hash) {
            *entry_out = (uint32_t)entry;
            return version;
        }
    }
    return nullptr;
}

// The addresses the code forms: the song data from the entry address, each track's events from the track before.

static bool scm_place_data(ScmModule* module, uint32_t entry) {
    const ScmVersion* version = module->version;
    int64_t operands[ScmOperand_Count] = { 0 };
    for (uint32_t i = 0; i < ScmOperand_Count; i++) {
        const uint32_t at = version->operand_at[i];
        if (at == 0) {
            continue;
        }
        if (i == ScmOperand_ZeroWrite) {
            operands[i] = (int16_t)scm_u16(module, (int64_t)entry + at);
        } else if (version->operand_bytes == SCM_OPERANDS_LONG) {
            operands[i] = (int32_t)scm_u32(module, (int64_t)entry + at);
        } else {
            operands[i] = (int16_t)scm_u16(module, (int64_t)entry + at);
        }
    }
    const int64_t records = SCM_ENTRY + operands[ScmOperand_Records];
    const int64_t envelopes = SCM_ENTRY + operands[ScmOperand_Envelopes];
    int64_t events[SCM_TRACKS];
    events[0] = SCM_ENTRY + operands[ScmOperand_Events0];
    for (uint32_t t = 1; t < SCM_TRACKS; t++) {
        events[t] = events[t - 1] + operands[ScmOperand_Events1 + t - 1];
    }
    bool ok = scm_range_in(module->size, records, 0) && scm_range_in(module->size, envelopes, 0);
    for (uint32_t t = 0; t < SCM_TRACKS; t++) {
        ok = ok && scm_range_in(module->size, events[t], 0);
    }
    if (!ok) {
        return false;
    }
    module->records = (uint32_t)records;
    module->envelopes = (uint32_t)envelopes;
    for (uint32_t t = 0; t < SCM_TRACKS; t++) {
        module->events[t] = (uint32_t)events[t];
    }
    module->zero_write = (int16_t)operands[ScmOperand_ZeroWrite];
    return true;
}

bool scm_module_load(ScmModule* module, const uint8_t* data, size_t size) {
    memset(module, 0, sizeof(*module));
    if (data == nullptr || size < SCM_ENTRY + 2 * SCM_WORD_BYTES || size > SCM_MAX_MODULE_BYTES) {
        return false;
    }
    module->size = (uint32_t)size;
    module->image = nuade_paula_padded_copy(data, size);
    if (module->image == nullptr) {
        return false;
    }
    uint32_t entry = 0;
    module->version = scm_find_version(module, &entry);
    if (module->version != nullptr) {
        module->engine = scm_engine(module->version->driver);
        module->entry = entry;
    }
    const int64_t samples = (int64_t)SCM_ENTRY + (int16_t)scm_u16(module, SCM_SAMPLE_BASE_OFFSET);
    bool ok = module->version != nullptr && scm_range_in(module->size, samples, 0);
    if (ok) {
        module->samples = (uint32_t)samples;
        ok = module->engine->tracker ? scm_place_data(module, entry) && scm_check_tracker(module)
                                     : scm_check_sweep(module);
    }
    if (!ok) {
        scm_module_free(module);
    }
    return ok;
}

void scm_module_free(ScmModule* module) {
    free(module->image);
    memset(module, 0, sizeof(*module));
}
