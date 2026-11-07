// SPDX-License-Identifier: MIT
// Tim Follin player: locating and reading the module (the eagleplayer's Check2, InitPlayer and SubSongRange).

#include "nuade_utils.h"
#include "tfol_internal.h"
#include "tfol_layout.h"

#define TFOL_OP_JMP 0x4ef9
#define TFOL_TOS_HEADER 0x601a
#define TFOL_TOS_HEADER_SIZE 0x1c
#define TFOL_DATA_MARK_A 0x1010
#define TFOL_DATA_MARK_B 0x1012
#define TFOL_JUMP_SLOT_SIZE 6
#define TFOL_JUMP_SLOTS 15
#define TFOL_JUMP_TABLE_CHECKED 6
// The song table of a module that starts with the jump table.
#define TFOL_JUMP_TABLE_DATA 0x432
// The first long of the song 0 stream that the eagleplayer searches for (a note, then the stop command).
#define TFOL_SONG0_STREAM_HEAD 0x81043202u
#define TFOL_SONG0_STREAM_TAIL 0x8f00u

static bool tfol_has(size_t size, uint32_t offset, uint32_t length) {
    return size >= length && offset <= size - length;
}

static uint16_t tfol_file_u16(const uint8_t* data, size_t size, uint32_t offset) {
    return tfol_has(size, offset, 2) ? nuade_read_be16(data + offset) : 0;
}

static uint32_t tfol_file_u32(const uint8_t* data, size_t size, uint32_t offset) {
    return tfol_has(size, offset, 4) ? nuade_read_be32(data + offset) : 0;
}

// The first branch of Check2: six jmp instructions.
static bool tfol_starts_with_jumps(const uint8_t* data, size_t size) {
    for (uint32_t i = 0; i < TFOL_JUMP_TABLE_CHECKED; i++) {
        if (tfol_file_u16(data, size, i * TFOL_JUMP_SLOT_SIZE) != TFOL_OP_JMP) {
            return false;
        }
    }
    return true;
}

// The second branch of Check2: an optional TOS header, a data mark and the four equal stream addresses of song 0.
// Returns the offset of the song table (the word after the mark), or 0 when the check fails.
static uint32_t tfol_data_start(const uint8_t* data, size_t size) {
    uint32_t at = 0;
    if (tfol_file_u16(data, size, 0) == TFOL_TOS_HEADER) {
        at = TFOL_TOS_HEADER_SIZE;
    }
    const uint16_t mark = tfol_file_u16(data, size, at);
    if (mark != TFOL_DATA_MARK_A && mark != TFOL_DATA_MARK_B) {
        return 0;
    }
    const uint32_t table = at + 2;
    // The four stream longs of song 0 are read below.
    if (!tfol_has(size, table, TFOL_SONG_VOICES_OFFSET + TFOL_VOICES * 4)) {
        return 0;
    }
    const uint32_t first = nuade_read_be32(data + table + 4);
    for (uint32_t voice = 1; voice < TFOL_VOICES; voice++) {
        if (nuade_read_be32(data + table + 4 + voice * 4) != first) {
            return 0;
        }
    }
    return table;
}

// InitPlayer for a module without the jump table: the offset is where the stream of song 0 really is minus the address
// the table gives for it.
static bool tfol_find_song0_stream(const uint8_t* data, size_t size, uint32_t table, uint32_t* out_found) {
    for (size_t at = table; at + 6 <= size; at += 2) {
        if (nuade_read_be32(data + at) == TFOL_SONG0_STREAM_HEAD
            && nuade_read_be16(data + at + 4) == TFOL_SONG0_STREAM_TAIL) {
            *out_found = (uint32_t)at;
            return true;
        }
    }
    return false;
}

// The relocation loop adds the offset to longs while their high byte is 0; the player needs it to cover the whole
// table of songs and instrument tables.
static bool tfol_table_is_relocated(const uint8_t* data, size_t size, uint32_t table) {
    if (!tfol_has(size, table, TFOL_TABLE_SIZE)) {
        return false;
    }
    for (uint32_t i = 0; i < TFOL_RELOCATED_LONGS; i++) {
        if (data[table + i * 4] != 0) {
            return false;
        }
    }
    return data[table + TFOL_RELOCATED_LONGS * 4] != 0;
}

bool tfol_module_locate(const uint8_t* data, size_t size, TfolModule* module) {
    if (size > TFOL_MAX_SIZE) {
        return false;
    }
    uint32_t table = 0;
    uint32_t delta = 0;
    if (tfol_file_u16(data, size, 0) == TFOL_OP_JMP) {
        if (!tfol_starts_with_jumps(data, size)) {
            return false;
        }
        table = TFOL_JUMP_TABLE_DATA;
        delta = 0u - tfol_file_u32(data, size, 2);
    } else {
        table = tfol_data_start(data, size);
        uint32_t found = 0;
        if (table == 0 || !tfol_find_song0_stream(data, size, table, &found)) {
            return false;
        }
        delta = found - nuade_read_be32(data + table + 4);
    }
    if (!tfol_table_is_relocated(data, size, table)) {
        return false;
    }
    uint32_t subsongs = 0;
    for (uint32_t slot = 0; slot < TFOL_JUMP_SLOTS; slot++) {
        subsongs += tfol_file_u16(data, size, slot * TFOL_JUMP_SLOT_SIZE) == TFOL_OP_JMP;
    }
    *module = (TfolModule) { .mem = (uint8_t*)data, // read only until tfol_create copies it
                             .size = size,
                             .table = table,
                             .address_delta = delta,
                             .subsong_count = subsongs != 0 ? subsongs : 1 };
    const bool proved = tfol_module_prove(module);
    module->mem = nullptr;
    return proved;
}

uint8_t tfol_module_u8(const TfolModule* module, uint32_t offset) {
    return offset < module->size ? module->mem[offset] : 0;
}

uint32_t tfol_module_u32(const TfolModule* module, uint32_t offset) {
    return tfol_has(module->size, offset, 4) ? nuade_read_be32(module->mem + offset) : 0;
}

void tfol_module_poke(TfolModule* module, uint32_t offset, uint8_t value) {
    if (offset < module->size) {
        module->mem[offset] = value;
    }
}

uint32_t tfol_module_offset(const TfolModule* module, uint32_t address) {
    return address + module->address_delta;
}

TfolSong tfol_module_song(const TfolModule* module, uint32_t song) {
    const uint32_t record = module->table + TFOL_SONG_VOICES_OFFSET + song * TFOL_SONG_RECORD_SIZE;
    TfolSong out = { 0 };
    for (uint32_t voice = 0; voice < TFOL_VOICES; voice++) {
        out.stream[voice] = tfol_module_offset(module, tfol_module_u32(module, record + voice * 4));
    }
    out.instrument_table
        = tfol_module_offset(module, tfol_module_u32(module, module->table + TFOL_INSTRUMENT_TABLES_OFFSET + song * 4));
    out.voices = tfol_module_u8(module, module->table + TFOL_VOICE_COUNTS_OFFSET + song) == TFOL_VOICES
                     ? TFOL_VOICES
                     : TFOL_MIN_VOICES;
    return out;
}

TfolInstrument tfol_module_instrument(const TfolModule* module, uint32_t instrument_table, uint8_t index) {
    const uint32_t offset = tfol_module_offset(module, tfol_module_u32(module, instrument_table + index * 4u));
    TfolInstrument out = { 0 };
    if (!tfol_has(module->size, offset, TFOL_INSTRUMENT_SAMPLE_OFFSET)) {
        return out;
    }
    out.offset = offset;
    out.sample = offset + TFOL_INSTRUMENT_SAMPLE_OFFSET;
    out.length_words = nuade_read_be16(module->mem + offset);
    // AUDxLEN 0 plays 65536 words.
    out.sample_bytes = out.length_words != 0 ? out.length_words * 2u : 0x20000u;
    out.valid = (uint64_t)out.sample + out.sample_bytes <= module->size;
    return out;
}

// The argument bytes of each command.
static const uint8_t tfol_argument_counts[TfolCommand_Count] = {
    [TfolCommand_Instrument] = 1, [TfolCommand_LoopStart] = 1,  [TfolCommand_Envelope] = 3,
    [TfolCommand_Portamento] = 1, [TfolCommand_Vibrato] = 3,    [TfolCommand_PitchModulation] = 4,
    [TfolCommand_Transpose] = 1,  [TfolCommand_NoteLength] = 1, [TfolCommand_EnvelopeRestart] = 1,
    [TfolCommand_WaveFlags] = 1,  [TfolCommand_SweepRate] = 1,  [TfolCommand_EnvelopeStep] = 1,
};

TfolEvent tfol_module_read_event(const TfolModule* module, uint32_t at, bool note_has_length) {
    TfolEvent ev = { .start = at, .next = at };
    if (!tfol_has(module->size, at, 1)) {
        return ev;
    }
    const uint8_t byte = module->mem[at++];
    uint32_t arguments = 0;
    if (byte != 0 && (byte & 0x80) == 0) {
        ev.is_note = true;
        ev.note = byte;
        arguments = note_has_length ? 0 : 1;
    } else {
        ev.command = (TfolCommand)(byte & 0x7f);
        if (ev.command >= TfolCommand_Count) {
            return ev;
        }
        arguments = tfol_argument_counts[ev.command];
    }
    const bool address = !ev.is_note && (ev.command == TfolCommand_Call || ev.command == TfolCommand_Jump);
    if (address) {
        // An address long, aligned to an even file offset.
        at += at & 1;
        if (!tfol_has(module->size, at, 4)) {
            return ev;
        }
        ev.target = tfol_module_offset(module, nuade_read_be32(module->mem + at));
        at += 4;
        if (ev.target >= module->size) {
            return ev;
        }
    }
    if (!tfol_has(module->size, at, arguments)) {
        return ev;
    }
    memcpy(ev.args, module->mem + at, arguments);
    ev.next = at + arguments;
    ev.valid = true;
    return ev;
}
