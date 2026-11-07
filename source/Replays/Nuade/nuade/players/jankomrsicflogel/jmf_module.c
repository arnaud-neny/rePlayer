// SPDX-License-Identifier: MIT
// Janko Mrsic-Flogel: the eagleplayer's Check2, the hunk loader, the identification of the replayer version and the
// typed readers of the module records. A pointer stored in the file is a code offset: the loader adds the hunk's
// address.

#include "jmf_internal.h"
#include "nuade_base.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define JMF_HUNK_HEADER 0x3F3u
#define JMF_HUNK_CODE 0x3E9u
#define JMF_HUNK_RELOC32 0x3ECu
#define JMF_HUNK_END 0x3F2u

// The file offset of the code hunk's first byte: header (3 words + first, last), 1 size word, hunk id and size.
#define JMF_CODE_FILE_OFFSET 0x20u

// The four bytes of the program the eagleplayer's Check2 expects at the start of the code: moveq #-1,d0; rts.
#define JMF_CODE_PROLOGUE 0x70FF4E75u

// The bytes of the header of the code: the prologue, the "J.FLOGEL" mark and the entry points.
#define JMF_HEADER_BYTES 0x30u

#define JMF_NO_TEXT UINT32_MAX

// The period table is indexed by a byte; the replayer wraps the index of a byte below the offset, which the port
// does not model: a table this short makes every wrapped index a miss.
#define JMF_MAX_PERIODS 128u

// The init routine is a chain of blocks, one per subsong: [tst.w d0 / cmpi.w #n,d0; bne.b next;] three times
// move.l #stream,variable; bra.w reinit. The last block has no test.
#define JMF_STREAM_MOVE_BYTES 10u // move.l #stream,variable
#define JMF_BLOCK_STREAMS_BYTES (JMF_NOTE_VOICES * JMF_STREAM_MOVE_BYTES)
#define JMF_BLOCK_BYTES (JMF_BLOCK_STREAMS_BYTES + 4u)
#define JMF_BNE_SKIP 0x22u

// The replayer's variables follow its period table: counters, modes, stream pointers, in the same layout in every
// version.
#define JMF_VARIABLE_BYTES 0xBAu

// move.l #address,destination
static const uint8_t k_move_immediate[] = { 0x23, 0xFC };

// Version signatures, see replayer_signature: Bomb Jack, Fire Blaster, Luxor, and Protector (also Space Station and
// Warzone). The waveforms play with AUDxLEN 16 words. The second waveform of Protector is 4 words long and plays at 16
// times the period.
static const JmfDriver k_drivers[] = {
    {
        .signature = 0xECC20970A8471127ULL,
        .length = 0x418,
        .reinit = 0x322,
        .tick_divisor = 4,
        .note_offset = -5,
        .wave_a_words = 16,
        .wave_b_words = 0,
        .wave_b_period_shift = 0,
        .idle_words = 0,
    },
    {
        .signature = 0xF13485B92949E709ULL,
        .length = 0x3E0,
        .reinit = 0x2F2,
        .tick_divisor = 6,
        .note_offset = 0,
        .wave_a_words = 16,
        .wave_b_words = 0,
        .wave_b_period_shift = 0,
        .idle_words = 0,
    },
    {
        .signature = 0x4B3501ABAC6DE87BULL,
        .length = 0x3FA,
        .reinit = 0x2FC,
        .tick_divisor = 4,
        .note_offset = 4,
        .wave_a_words = 16,
        .wave_b_words = 0,
        .wave_b_period_shift = 0,
        .idle_words = 256,
    },
    {
        .signature = 0xA4EFC89637A2BC21ULL,
        .length = 0x406,
        .reinit = 0x318,
        .tick_divisor = 6,
        .note_offset = 0,
        .wave_a_words = 16,
        .wave_b_words = 4,
        .wave_b_period_shift = 4,
        .idle_words = 0,
    },
};

// The jump table the mode byte indexes.
static const JmfAction k_mode_actions[JMF_MODE_COUNT] = {
    JmfAction_WaveA, JmfAction_WaveB, JmfAction_Skip,  JmfAction_Note, JmfAction_Skip,
    JmfAction_Note,  JmfAction_Note,  JmfAction_WaveA, JmfAction_Stop,
};

static bool fits(const JmfModule* module, uint64_t at, uint64_t bytes) {
    return module->size >= bytes && at <= module->size - bytes;
}

static bool is_relocated(const JmfModule* module, uint32_t at) {
    size_t low = 0;
    size_t high = module->relocation_count;
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        if (module->relocations[middle] == at) {
            return true;
        }
        if (module->relocations[middle] < at) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return false;
}

// A relocated 32 bit word: the code offset it points at, which must lie inside the hunk.

static bool pointer(const JmfModule* module, uint32_t at, uint32_t* out) {
    if (!fits(module, at, 4) || !is_relocated(module, at)) {
        return false;
    }
    const uint32_t target = nuade_read_be32(module->bytes + at);
    if (target >= module->size) {
        return false;
    }
    *out = target;
    return true;
}

static bool bytes_match(const JmfModule* module, uint32_t at, const uint8_t* expected, size_t length) {
    return fits(module, at, length) && (length == 0 || memcmp(module->bytes + at, expected, length) == 0);
}

static bool in_variables(const JmfModule* module, uint64_t at, uint64_t bytes) {
    return at < module->variables_end && at + bytes > module->variables;
}

bool jmf_byte(const JmfModule* module, uint32_t at, uint8_t* out) {
    if (at >= module->size || in_variables(module, at, 1)) {
        return false;
    }
    *out = module->bytes[at];
    return true;
}

uint32_t jmf_stream_start(const JmfModule* module, uint32_t subsong, uint32_t voice) {
    return module->streams[subsong * JMF_NOTE_VOICES + voice];
}

bool jmf_mode_action(uint8_t mode, JmfAction* out) {
    if (mode >= JMF_MODE_COUNT) {
        return false;
    }
    *out = k_mode_actions[mode];
    return true;
}

static bool action_has_note(JmfAction action) {
    return action == JmfAction_WaveA || action == JmfAction_WaveB || action == JmfAction_Note;
}

bool jmf_period(const JmfModule* module, uint8_t note, uint16_t* out) {
    const int index = (int)note + module->driver->note_offset;
    if (index < 0 || (uint32_t)index >= module->period_count) {
        return false;
    }
    *out = nuade_read_be16(module->bytes + module->period_table + (uint32_t)index * 2u);
    return true;
}

// The relocation table: HUNK_RELOC32 groups of (count, hunk, offsets) up to HUNK_END. The file has one hunk, so every
// group relocates against hunk 0.

static bool read_relocations(JmfModule* module, const uint8_t* file, size_t size, size_t at) {
    size_t capacity = 0;
    while (size - at >= 4) {
        const uint32_t id = nuade_read_be32(file + at) & 0x3FFFFFFFu;
        at += 4;
        if (id == JMF_HUNK_END) {
            return true;
        }
        if (id != JMF_HUNK_RELOC32) {
            return false;
        }
        for (;;) {
            if (size - at < 4) {
                return false;
            }
            const uint32_t count = nuade_read_be32(file + at);
            at += 4;
            if (count == 0) {
                break;
            }
            if (size - at < 4 || nuade_read_be32(file + at) != 0 || (size - at - 4) / 4 < count) {
                return false;
            }
            at += 4;
            if (module->relocation_count + count > capacity) {
                capacity = module->relocation_count + count;
                uint32_t* grown = realloc(module->relocations, capacity * sizeof(*grown));
                if (grown == nullptr) {
                    return false;
                }
                module->relocations = grown;
            }
            for (uint32_t i = 0; i < count; i++, at += 4) {
                const uint32_t offset = nuade_read_be32(file + at);
                // Sorted without repeats, the whole word inside the hunk
                if (!fits(module, offset, 4)
                    || (module->relocation_count > 0 && offset <= module->relocations[module->relocation_count - 1])) {
                    return false;
                }
                module->relocations[module->relocation_count++] = offset;
            }
        }
    }
    return false;
}

// The executable: HUNK_HEADER with one chip memory hunk, HUNK_CODE, the relocations, HUNK_END. Check2 also wants a
// memory flag in the size word.

static bool load_hunks(JmfModule* module, const uint8_t* file, size_t size) {
    if (size < JMF_CODE_FILE_OFFSET + 8 || nuade_read_be32(file) != JMF_HUNK_HEADER || nuade_read_be32(file + 4) != 0
        || nuade_read_be32(file + 8) != 1 || nuade_read_be32(file + 12) != 0 || nuade_read_be32(file + 16) != 0
        || file[0x14] == 0 || nuade_read_be32(file + 0x18) != JMF_HUNK_CODE) {
        return false;
    }
    const uint32_t longs = nuade_read_be32(file + 0x1C);
    if ((nuade_read_be32(file + 0x14) & 0x3FFFFFFFu) != longs || longs < JMF_HEADER_BYTES / 4
        || (size - JMF_CODE_FILE_OFFSET) / 4 < longs) {
        return false;
    }
    module->bytes = file + JMF_CODE_FILE_OFFSET;
    module->size = (size_t)longs * 4;
    return read_relocations(module, file, size, JMF_CODE_FILE_OFFSET + module->size);
}

// The header of the code: the prologue, the "J.FLOGEL" mark and the entry points. Check2 requires the first three
// words after the mark to be set.

static bool read_header(JmfModule* module) {
    static const uint8_t mark[] = { 'J', '.', 'F', 'L', 'O', 'G', 'E', 'L' };
    if (nuade_read_be32(module->bytes) != JMF_CODE_PROLOGUE || memcmp(module->bytes + 4, mark, sizeof(mark)) != 0
        || !pointer(module, 0x0C, &module->play) || !pointer(module, 0x10, &module->init)) {
        return false;
    }
    // A routine at an odd address would be an address error
    if (module->play % 2 != 0 || module->init % 2 != 0) {
        return false;
    }
    module->subsong_count = nuade_read_be32(module->bytes + 0x14);
    if (module->subsong_count == 0 || is_relocated(module, 0x14)) {
        return false;
    }
    for (uint32_t i = 0; i < nuade_sizeof_array(module->texts); i++) {
        if (!pointer(module, 0x1C + i * 4, &module->texts[i])) {
            module->texts[i] = JMF_NO_TEXT;
        }
    }
    return true;
}

// FNV-1a over the replayer's code, from the play routine to the period table. A relocated word that points into that
// region or in the variables behind the period table counts as its target counted from the play routine, one that
// points anywhere else (waveforms, streams) as zero.

static bool replayer_signature(const JmfModule* module, uint32_t length, uint64_t* out) {
    const uint32_t begin = module->play;
    if (!fits(module, begin, length)) {
        return false;
    }
    uint8_t* image = malloc(length);
    if (image == nullptr) {
        return false;
    }
    memcpy(image, module->bytes + begin, length);
    for (size_t i = 0; i < module->relocation_count; i++) {
        const uint32_t at = module->relocations[i];
        if (at + 4 <= begin || at >= begin + length) {
            continue;
        }
        const uint32_t target = nuade_read_be32(module->bytes + at);
        const uint32_t inside = target >= begin && target - begin < length + JMF_VARIABLE_BYTES ? target - begin : 0;
        for (uint32_t k = 0; k < 4; k++) {
            if (at + k >= begin && at + k < begin + length) {
                image[at + k - begin] = (uint8_t)(inside >> (24 - 8 * k));
            }
        }
    }
    uint64_t hash = 0xCBF29CE484222325ULL;
    for (uint32_t i = 0; i < length; i++) {
        hash = (hash ^ image[i]) * 0x100000001B3ULL;
    }
    free(image);
    *out = hash;
    return true;
}

static const JmfDriver* find_driver(const JmfModule* module) {
    for (size_t i = 0; i < nuade_sizeof_array(k_drivers); i++) {
        uint64_t signature;
        if (replayer_signature(module, k_drivers[i].length, &signature) && signature == k_drivers[i].signature) {
            return &k_drivers[i];
        }
    }
    return nullptr;
}

// Some versions test a pause flag first (tst.w flag). Only the stop routines set it and the eagleplayer never calls
// them, so the flag has to be clear in the file.

static bool pause_clear(const JmfModule* module) {
    static const uint8_t test_flag[] = { 0x4A, 0x79 };
    if (!bytes_match(module, module->play, test_flag, sizeof(test_flag))) {
        return true;
    }
    uint32_t flag;
    return pointer(module, module->play + 2, &flag) && fits(module, flag, 2)
           && nuade_read_be16(module->bytes + flag) == 0;
}

// An instruction of the replayer: `head`, `operands` relocated words, `tail`.
typedef struct Instruction {
    const uint8_t* head;
    size_t head_length;
    size_t operands;
    const uint8_t* tail;
    size_t tail_length;
} Instruction;

// Stores the first operand of each place the instruction occurs at in the replayer; more than `cap` places fail.

static bool find_operands(const JmfModule* module, const Instruction* instruction, uint32_t* out, size_t cap,
                          size_t* count) {
    const uint32_t end = module->play + module->driver->length;
    const size_t body = instruction->head_length + instruction->operands * 4 + instruction->tail_length;
    *count = 0;
    for (uint32_t at = module->play; end - at >= body; at += 2) {
        if (!bytes_match(module, at, instruction->head, instruction->head_length)) {
            continue;
        }
        uint32_t operand = at + (uint32_t)instruction->head_length;
        uint32_t first = 0;
        bool relocated = true;
        for (size_t i = 0; i < instruction->operands; i++, operand += 4) {
            uint32_t target = 0;
            relocated = relocated && pointer(module, operand, &target);
            if (i == 0) {
                first = target;
            }
        }
        if (relocated && bytes_match(module, operand, instruction->tail, instruction->tail_length)) {
            if (*count == cap) {
                return false;
            }
            out[(*count)++] = first;
        }
    }
    return true;
}

// The places that load a register of the voice's channel with an address: move.l #address,$dff0xx.

static bool find_register_loads(const JmfModule* module, uint8_t reg, uint32_t* out, size_t cap, size_t* count) {
    const uint8_t tail[] = { 0x00, 0xDF, 0xF0, reg };
    return find_operands(module, &(Instruction) { k_move_immediate, sizeof(k_move_immediate), 1, tail, sizeof(tail) },
                         out, cap, count);
}

// An address from the waveform records of the module: AUDxLC ignores its lowest bit, and the rip leaves a waveform at
// an odd address.

static uint32_t even(uint32_t offset) {
    return offset & ~1u;
}

// The waveforms: the replayer loads each of the three voices with the first one when it starts over, and with the
// first or the second one when a note says so. The fourth voice gets a waveform of its own that never plays.

static bool read_waveforms(JmfModule* module) {
    uint32_t a = 0;
    uint32_t b = 0;
    for (uint8_t voice = 0; voice < JMF_NOTE_VOICES; voice++) {
        uint32_t start[2];
        uint32_t note[3];
        size_t start_count;
        size_t note_count;
        if (!find_register_loads(module, (uint8_t)(0xA0 + voice * 0x10), start, 2, &start_count)
            || !find_register_loads(module, (uint8_t)(0xAA + voice * 0x10), note, 3, &note_count) || start_count != 1
            || note_count != 2 || start[0] != note[0]) {
            return false;
        }
        if (voice == 0) {
            a = note[0];
            b = note[1];
        } else if (a != note[0] || b != note[1]) {
            return false;
        }
    }
    uint32_t idle[2];
    size_t idle_count;
    if (!find_register_loads(module, 0xD0, idle, 2, &idle_count) || idle_count != 1) {
        return false;
    }
    module->wave_a = even(a);
    module->wave_b = even(b);
    module->idle_wave = even(idle[0]);
    const JmfDriver* driver = module->driver;
    const uint32_t b_words = driver->wave_b_words != 0 ? driver->wave_b_words : driver->wave_a_words;
    // Every waveform lies in the song data
    return fits(module, module->wave_a, driver->wave_a_words * 2u)
           && !in_variables(module, module->wave_a, driver->wave_a_words * 2u)
           && fits(module, module->wave_b, b_words * 2u) && !in_variables(module, module->wave_b, b_words * 2u)
           && module->idle_wave < module->size;
}

// The three variables the init routine stores the note streams into, in the order the start-over routine copies them
// into the voices.

static bool read_stream_variables(JmfModule* module) {
    static const uint8_t copy_head[] = { 0x23, 0xF9 }; // move.l variable,voice
    uint32_t sources[4];
    size_t count;
    if (!find_operands(module, &(Instruction) { copy_head, 2, 2, nullptr, 0 }, sources, 4, &count)
        || count != JMF_NOTE_VOICES) {
        return false;
    }
    for (uint32_t i = 0; i < JMF_NOTE_VOICES; i++) {
        // A variable of the replayer: a whole long word behind the counters
        if (sources[i] < module->variables || !fits(module, sources[i], 4)) {
            return false;
        }
        module->stream_variables[i] = sources[i];
        if (sources[i] + 4 > module->variables_end) {
            module->variables_end = sources[i] + 4;
        }
    }
    return true;
}

// The period table (the one lookup the notes use) and the variables. The counters are the first of the variables, the
// table ends where they start.

static bool read_tables(JmfModule* module) {
    static const uint8_t lookup_head[] = { 0x45, 0xF9 };
    static const uint8_t lookup_tail[] = { 0x32, 0x32, 0x10, 0x00 };
    static const uint8_t counter_head[] = { 0x04, 0x39, 0x00, 0x01 }; // subi.b #1,counter
    uint32_t table[2];
    uint32_t counters[8];
    size_t table_count;
    size_t counter_count;
    if (!find_operands(module, &(Instruction) { lookup_head, 2, 1, lookup_tail, 4 }, table, 2, &table_count)
        || !find_operands(module, &(Instruction) { counter_head, 4, 1, nullptr, 0 }, counters, 8, &counter_count)
        || table_count != 1 || counter_count != 4) {
        return false;
    }
    module->variables = counters[0];
    for (size_t i = 1; i < counter_count; i++) {
        if (counters[i] < module->variables) {
            module->variables = counters[i];
        }
    }
    module->variables_end = module->variables;
    module->period_table = table[0];
    if (module->period_table % 2 != 0 || module->period_table >= module->variables) {
        return false;
    }
    module->period_count = (module->variables - module->period_table) / 2;
    return module->period_count <= JMF_MAX_PERIODS && read_stream_variables(module);
}

// The init routine: for each subsong the starts of its three note streams.

static bool read_subsongs(JmfModule* module) {
    const uint32_t count = module->subsong_count;
    if ((module->size - module->init) / JMF_BLOCK_BYTES < count) {
        return false;
    }
    module->streams = calloc((size_t)count * JMF_NOTE_VOICES, sizeof(*module->streams));
    if (module->streams == nullptr) {
        return false;
    }
    uint32_t at = module->init;
    for (uint32_t subsong = 0; subsong < count; subsong++) {
        // The test that leads here: the first subsong compares with tst, the others with cmpi, the last one has none
        if (subsong + 1 < count) {
            const uint8_t test[] = { 0x0C, 0x40, (uint8_t)(subsong >> 8), (uint8_t)subsong };
            const uint8_t first[] = { 0x4A, 0x40 };
            const uint8_t* expected = subsong == 0 ? first : test;
            const size_t length = subsong == 0 ? sizeof(first) : sizeof(test);
            const uint8_t branch[] = { 0x66, JMF_BNE_SKIP };
            if (!bytes_match(module, at, expected, length) || !bytes_match(module, at + (uint32_t)length, branch, 2)) {
                return false;
            }
            at += (uint32_t)length + 2;
        }
        if (!fits(module, at, JMF_BLOCK_BYTES)) {
            return false;
        }
        for (uint32_t voice = 0; voice < JMF_NOTE_VOICES; voice++, at += JMF_STREAM_MOVE_BYTES) {
            uint32_t stream;
            uint32_t destination;
            if (!bytes_match(module, at, k_move_immediate, sizeof(k_move_immediate))
                || !pointer(module, at + 2, &stream) || !pointer(module, at + 6, &destination)
                || destination != module->stream_variables[voice]) {
                return false;
            }
            module->streams[subsong * JMF_NOTE_VOICES + voice] = stream;
        }
        // bra.w to the start-over routine
        if (!bytes_match(module, at, (const uint8_t[]) { 0x60, 0x00 }, 2)) {
            return false;
        }
        const int64_t target = (int64_t)at + 2 + (int16_t)nuade_read_be16(module->bytes + at + 2);
        if (target != (int64_t)module->play + module->driver->reinit) {
            return false;
        }
        at += 4;
    }
    return true;
}

// Does a note stream stay inside the data the port models? It is read event by event up to its end mark: a duration,
// then what the current mode adds (nothing, a skipped byte or a note). The replayer clears the mode when it starts
// over, so every stream starts in mode 0.

static bool stream_usable(const JmfModule* module, uint32_t start, uint32_t voice) {
    uint32_t at = start;
    uint8_t mode = 0;
    JmfAction action = k_mode_actions[0];
    for (;;) {
        uint8_t value;
        if (!jmf_byte(module, at++, &value)) {
            return false;
        }
        if (value == JMF_STREAM_END) {
            return true;
        }
        if (value == JMF_STREAM_MODE) {
            if (!jmf_byte(module, at++, &mode) || !jmf_mode_action(mode, &action)) {
                return false;
            }
            continue;
        }
        if (action == JmfAction_Stop) {
            continue;
        }
        // The third voice's handler of mode 4 jumps into the second voice's code, which the port does not model.
        if (voice == 2 && mode == 4) {
            return false;
        }
        uint8_t note;
        uint16_t period;
        if (!jmf_byte(module, at++, &note) || (action_has_note(action) && !jmf_period(module, note, &period))) {
            return false;
        }
    }
}

bool jmf_module_parse(JmfModule* module, const uint8_t* bytes, size_t size) {
    *module = (JmfModule) { 0 };
    if (!load_hunks(module, bytes, size) || !read_header(module)) {
        jmf_module_free(module);
        return false;
    }
    module->driver = find_driver(module);
    if (module->driver == nullptr || !pause_clear(module) || !read_tables(module) || !read_waveforms(module)
        || !read_subsongs(module)) {
        jmf_module_free(module);
        return false;
    }
    for (uint32_t subsong = 0; subsong < module->subsong_count; subsong++) {
        for (uint32_t voice = 0; voice < JMF_NOTE_VOICES; voice++) {
            if (!stream_usable(module, jmf_stream_start(module, subsong, voice), voice)) {
                jmf_module_free(module);
                return false;
            }
        }
    }
    return true;
}

void jmf_module_free(JmfModule* module) {
    free(module->relocations);
    free(module->streams);
    module->relocations = nullptr;
    module->streams = nullptr;
    module->relocation_count = 0;
}
