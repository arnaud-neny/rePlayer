// SPDX-License-Identifier: MIT
// Scott Johnston player: the replayer's constant tables and the command decoding built on them.

#include "sj_internal.h"

#define SJ_NAME_COUNT 95

// The instrument file names of the eagleplayer's ExtLoad, 12 bytes each in the original.
static const char* const sj_instrument_names[SJ_NAME_COUNT] = {
    "acoustic",   "blocks",   "brass1",    "chello",    "clap",     "dogbark1",   "drum",       "elecbass",
    "elecguitar", "grunt",    "jazzorgan", "piano",     "piano1",   "sax",        "slurdrum",   "snare",
    "springy",    "strings2", "synthharp", "synthlead", "taccbass", "tbassdrum1", "tbassdrum2", "tbell",
    "tbigchord",  "tbuzz",    "tcrinkle",  "tfbass",    "tharp",    "thump",      "tkmix",      "todd",
    "tpianohi",   "tpianolo", "tpizz",     "tshaker",   "tsnare1",  "tsnare2",    "tspacebass", "tsprinkle",
    "tsqrbell",   "tstring",  "ttink",     "ttrumpet",  "zap",      "abass",      "abdr",       "asnare",
    "ag1",        "ag2",      "ag3",       "alead",     "along",    "achord",     "b1bass",     "b1bsdr",
    "b1maj",      "b1min",    "b1pan",     "b1snare",   "b1mrm",    "b1sonar",    "b1tom2",     "b2bass",
    "b2string",   "b2tom1",   "b2bal",     "b2hand1",   "b2leaad",  "b2bal2",     "mbdr",       "mcym",
    "mgtr1",      "mgtr2",    "msnr",      "mtp1",      "mtp2",     "mspeedup",   "mdanger",    "bass3",
    "bass4",      "blaster1", "blaster2",  "bleep1",    "drum2",    "highnote",   "lead11",     "mega",
    "new2",       "new4",     "new5",      "new6",      "new7",     "new8",       "organ2",
};

// The replayer's period table (word index = note - 1).
static const uint16_t sj_period_table[SJ_PERIOD_COUNT] = {
    0x0434, 0x03F8, 0x03C0, 0x0358, 0x0328, 0x02FA, 0x02D0, 0x02A6, 0x0280, 0x025C, 0x023A, 0x021A, 0x01FC,
    0x01E0, 0x01C5, 0x01AC, 0x0194, 0x017D, 0x0168, 0x0153, 0x0140, 0x012E, 0x011D, 0x010D, 0x00FE, 0x00F0,
    0x00E2, 0x00D6, 0x00CA, 0x00BE, 0x00B4, 0x00AA, 0x00A0, 0x0097, 0x008F, 0x0087, 0x007F, 0x0078, 0x0071,
};

// The replayer's command table, right behind the period table: per command byte the offset of its handler in the
// jump table, 4 bytes per handler. A note past the period table reads this table as words.
static const uint8_t sj_command_table[SJ_COMMAND_TABLE_BYTES] = {
    0x00, 0x04, 0x08, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14,
    0x14, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x24, 0x28,
    0x2C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C, 0x50,
    0x54, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58,
    0x58, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C, 0x00, 0x00,
};

// The handlers of the jump table, in order (index = table value / 4); the handlers that only set or clear a flag
// for the eagleplayer's host interface, call its "PokeCommand" (nothing audible) or return at once, are SjCommand_None.
typedef enum SjHandler {
    SjHandler_Return,
    SjHandler_PokeCommandOff,
    SjHandler_PokeCommandOn,
    SjHandler_Volume,
    SjHandler_StopChannel,
    SjHandler_Wait,
    SjHandler_FadeUp,
    SjHandler_FadeDown,
    SjHandler_SetFlag,
    SjHandler_HoldOn,
    SjHandler_HoldOff,
    SjHandler_ResetFlags,
    SjHandler_LoopBegin,
    SjHandler_LoopEnd,
    SjHandler_RepeatRow,
    SjHandler_Restart,
    SjHandler_ClearFlag = 22,
} SjHandler;

// The bases the handlers subtract from the command byte.
#define SJ_VOLUME_BASE 3
#define SJ_WAIT_BASE 0x56
#define SJ_FADE_UP_BASE 0xA6
#define SJ_FADE_DOWN_BASE 0xB0
#define SJ_LOOP_BASE 0xD1

#define SJ_HANDLER_STRIDE 4

const char* sj_instrument_name(uint32_t file_index) {
    return file_index >= 1 && file_index <= SJ_NAME_COUNT ? sj_instrument_names[file_index - 1] : nullptr;
}

uint16_t sj_period_for_note(uint8_t note) {
    const uint32_t index = (uint32_t)note - 1u;
    if (index < SJ_PERIOD_COUNT) {
        return sj_period_table[index];
    }
    const uint32_t byte = (index - SJ_PERIOD_COUNT) * 2u;
    if (byte + 1u >= SJ_COMMAND_TABLE_BYTES) {
        return 0;
    }
    return (uint16_t)(sj_command_table[byte] << 8 | sj_command_table[byte + 1]);
}

bool sj_cell_is_modelled(const SjCell* cell) {
    if (cell->note == 0) {
        return true;
    }
    // Instrument 0 reads the last entry of the song table (an empty record); past the 38 records the table runs into
    // the voice records.
    return cell->note <= SJ_MAX_NOTE && cell->instrument <= SJ_INSTRUMENT_SLOTS;
}

SjCommand sj_decode_command(uint8_t byte) {
    switch (sj_command_table[byte] / SJ_HANDLER_STRIDE) {
        case SjHandler_Volume:
            return (SjCommand) { SjCommand_SetVolume, (uint8_t)(byte - SJ_VOLUME_BASE) };
        case SjHandler_StopChannel:
            return (SjCommand) { SjCommand_StopChannel, 0 };
        case SjHandler_Wait:
            return (SjCommand) { SjCommand_Wait, (uint8_t)(byte - SJ_WAIT_BASE) };
        case SjHandler_FadeUp:
            return (SjCommand) { SjCommand_FadeUp, (uint8_t)(byte - SJ_FADE_UP_BASE) };
        case SjHandler_FadeDown:
            return (SjCommand) { SjCommand_FadeDown, (uint8_t)(byte - SJ_FADE_DOWN_BASE) };
        case SjHandler_HoldOn:
            return (SjCommand) { SjCommand_HoldOn, 0 };
        case SjHandler_HoldOff:
            return (SjCommand) { SjCommand_HoldOff, 0 };
        case SjHandler_LoopBegin:
            return (SjCommand) { SjCommand_LoopBegin, (uint8_t)(byte - SJ_LOOP_BASE) };
        case SjHandler_LoopEnd:
            return (SjCommand) { SjCommand_LoopEnd, 0 };
        case SjHandler_RepeatRow:
            return (SjCommand) { SjCommand_RepeatRow, 0 };
        case SjHandler_Restart:
            return (SjCommand) { SjCommand_Restart, 0 };
        default:
            return (SjCommand) { SjCommand_None, 0 };
    }
}
