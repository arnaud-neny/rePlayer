// SPDX-License-Identifier: MIT
// Dave Lowe player: the game drivers, translated from the "Dave Lowe" eagleplayer.
//
// A module is an Amiga executable of one code hunk: after the usual first instruction (moveq #-1,d0 / rts) comes the
// tag "UNCLEART" and a header of addresses (hunk offsets, see DlGameHeader), then the driver's code, tables and music.
// The subsong table has four list addresses (16 bytes) per subsong, the first entry for subsong 1. The driver's
// interrupt runs the four music voices; most drivers also have sound effect voices that share channels 2 and 3 (or all
// four channels) with them. The eagleplayer asks for no effect, except for one effect in the first subsong of one
// driver (the header long at 0x4c), so only that driver's effect machinery runs here.
//
// The drivers differ from game to game in small ways of the voice routine, which the player reads from the routine's
// code: what each entry of the command table does, how many header words the envelope command skips, whether the
// vibrato uses its delay, and whether the note volume adds the bonus that command 13 enables. A driver whose code the
// player does not recognise is rejected, so that the host plays it with the original code.

#include "dl_internal.h"
#include "nuade_utils.h"

#define TAG_UNCL 0x554e434cu // "UNCL"
#define TAG_EART 0x45415254u // "EART"

// The header of the code hunk follows the tag; the memory flags of the hunk are in the size word of the hunk header
#define HEADER_START 0x0c
#define HEADER_EFFECT_REQUEST_AT 0x4c
#define HUNK_MEMORY_FLAGS_AT 0x14

// A subsong table entry: four list addresses
#define SUBSONG_ENTRY_SIZE 16
// The eagleplayer counts a table entry as a subsong when its first list address reaches this
#define SUBSONG_MIN_LIST 0x100

// The voice routine and its command table lie within this many bytes of each other
#define ROUTINE_SPAN 0x700

// A driver needs at least these commands to play anything
#define REQUIRED_COMMANDS 4

// Displacements in the driver's voice record that the locator tells code apart by
enum {
    VoiceField_Count = 0x02,
    VoiceField_State = 0x04,
    VoiceField_SlideMode = 0x08,
    VoiceField_VibratoState = 0x10,
    VoiceField_ListPosition = 0x1a,
    VoiceField_ListStart = 0x1e,
    VoiceField_EnvelopeStart = 0x22,
    VoiceField_VibratoDelay = 0x3a,
    VoiceField_VolumeCut = 0x3e,
    VoiceField_BonusFlag = 0x42,
};

// The audio channel registers: the interrupt hands a2 the AUDxLC of each channel
#define AUDIO_CHANNEL_REGISTERS 0xdff0a0u
#define AUDIO_CHANNEL_STRIDE 0x10u
#define DMACON_HIGH_WORD 0x00df
#define DMACON_LOW_WORD 0xf096
// AUDxVOL, relative to the channel registers
#define AUDIO_VOLUME_REGISTER 0x08

// The volume ramp: the init routine of a subsong calls a routine with the ramp speed in d0 that sets a mode word to 1,
// the speed, and the start level DL_RAMP_START_LEVEL
#define RAMP_MODE_ON 1

// The header of the code hunk
typedef struct DlGameHeader {
    uint32_t init;
    uint32_t interrupt;
    uint32_t stop;
    uint32_t subsong_variable; // the address of the word the eagleplayer writes the subsong number to
    uint32_t sample_records;   // the first and the end of the table of sample records
    uint32_t sample_records_end;
    uint32_t hook_before_init; // routines the eagleplayer calls: before the init (no module has one), after loading
    uint32_t load_hook;
    uint32_t subsong_table; // 0: the module has one song
    uint32_t title;         // offsets of the texts in the module
    uint32_t author;
    uint32_t copyright;
    uint32_t module_size;    // the size the eagleplayer reports, and the marker of the one module it patches
    uint32_t effect_request; // the effect the eagleplayer asks for in the first subsong, 0 = none
} DlGameHeader;

static DlGameHeader read_header(const DlImage* image) {
    DlCursor cursor = { .image = image, .address = HEADER_START };
    DlGameHeader h = { 0 };
    h.init = dlc_u32(&cursor);
    h.interrupt = dlc_u32(&cursor);
    h.stop = dlc_u32(&cursor);
    h.subsong_variable = dlc_u32(&cursor);
    h.sample_records = dlc_u32(&cursor);
    h.sample_records_end = dlc_u32(&cursor);
    h.hook_before_init = dlc_u32(&cursor);
    h.load_hook = dlc_u32(&cursor);
    h.subsong_table = dlc_u32(&cursor);
    h.title = dlc_u32(&cursor);
    h.author = dlc_u32(&cursor);
    h.copyright = dlc_u32(&cursor);
    h.module_size = dlc_u32(&cursor);
    h.effect_request = dli_u32(image, HEADER_EFFECT_REQUEST_AT);
    return h;
}

bool dl_game_extract(const DlImage* file, uint32_t* code_size) {
    DlCursor cursor = { .image = file, .address = DL_HUNK_CODE_OFFSET + DL_MODULE_TAG_AT };
    return dli_u8(file, HUNK_MEMORY_FLAGS_AT) != 0 && dl_hunk_code(file, code_size) && dlc_u32(&cursor) == TAG_UNCL
           && dlc_u32(&cursor) == TAG_EART;
}

// The Check2 of the eagleplayer: the header has an init, an interrupt and the subsong number's address
static bool header_is_valid(const DlGameHeader* h) {
    return h->init != 0 && h->interrupt != 0 && h->subsong_variable != 0;
}

static bool code_contains(const DlImage* image, uint32_t from, uint32_t to, const uint32_t* pattern, uint32_t count) {
    return dl_code_find(image, from, to, pattern, count) != DL_NOT_FOUND;
}

// The voice routine tests the note counter against 0 and 1 and the sample state against 3 (the test against 0 is
// tst.w or cmpi.w). Returns the address of the first test, or DL_NOT_FOUND.
static uint32_t find_voice_routine(const DlImage* image) {
    const uint32_t with_tst[] = { Op_TstWordDisplacedA0,
                                  VoiceField_Count,
                                  Op_BeqWord,
                                  DL_ANY_WORD,
                                  Op_CmpiWordDisplacedA0,
                                  1,
                                  VoiceField_Count,
                                  Op_BeqWord,
                                  DL_ANY_WORD,
                                  Op_CmpiWordDisplacedA0,
                                  3,
                                  VoiceField_State };
    const uint32_t with_cmpi[] = { Op_CmpiWordDisplacedA0, 0, VoiceField_Count, Op_BeqWord, DL_ANY_WORD,
                                   Op_CmpiWordDisplacedA0, 1, VoiceField_Count, Op_BeqWord, DL_ANY_WORD,
                                   Op_CmpiWordDisplacedA0, 3, VoiceField_State };
    const uint32_t found = dl_code_find(image, 0, image->module_size, with_tst, nuade_sizeof_array(with_tst));
    return found != DL_NOT_FOUND ? found
                                 : dl_code_find(image, 0, image->module_size, with_cmpi, nuade_sizeof_array(with_cmpi));
}

// The command table: the voice routine loads its address into a3, takes the handler of the command and jumps to it
static uint32_t find_command_table(const DlImage* image, uint32_t routine) {
    const uint32_t with_movea[]
        = { Op_MoveaLongImmediateA3, DL_ANY_WORD, DL_ANY_WORD, Op_MoveaIndexedA3ToA4, 0, Op_JmpIndirectA4 };
    const uint32_t with_lea[]
        = { Op_LeaAbsoluteA3, DL_ANY_WORD, DL_ANY_WORD, Op_MoveaIndexedA3ToA4, 0, Op_JmpIndirectA4 };
    uint32_t at = dl_code_find(image, routine, routine + ROUTINE_SPAN, with_movea, nuade_sizeof_array(with_movea));
    if (at == DL_NOT_FOUND) {
        at = dl_code_find(image, routine, routine + ROUTINE_SPAN, with_lea, nuade_sizeof_array(with_lea));
    }
    if (at == DL_NOT_FOUND) {
        return DL_NOT_FOUND;
    }
    DlCursor cursor = { .image = image, .address = at + 2 };
    const uint32_t table = dlc_u32(&cursor);
    return table < image->module_size ? table : DL_NOT_FOUND;
}

// A handler that does nothing: the voice stands still on its command
static bool handler_does_nothing(const DlImage* image, uint32_t handler) {
    const uint32_t just_return[] = { Op_Rts };
    const uint32_t nop_return[] = { Op_Nop, Op_Rts };
    return dl_code_matches(image, handler, just_return, nuade_sizeof_array(just_return))
           || dl_code_matches(image, handler, nop_return, nuade_sizeof_array(nop_return));
}

// How many header words the envelope command's handler skips: the pointer in the pattern leads to a header of two words
// (move.l (a4)+,d16(a0)), of one (move.w (a4)+,d16(a0)), or to the values (early engine, see classify_handler).
// Returns false for another handler.
static bool envelope_header_words(const DlImage* image, uint32_t handler, uint32_t* words) {
    const uint32_t two_words[] = { Op_MoveaPostIncA1ToA4, Op_MoveLongPostIncA4ToDisplacedA0, VoiceField_VolumeCut };
    const uint32_t one_word[] = { Op_MoveaPostIncA1ToA4, Op_MoveWordPostIncA4ToDisplacedA0, VoiceField_VolumeCut };
    if (dl_code_matches(image, handler, two_words, nuade_sizeof_array(two_words))) {
        *words = 2;
        return true;
    }
    if (dl_code_matches(image, handler, one_word, nuade_sizeof_array(one_word))) {
        *words = 1;
        return true;
    }
    return false;
}

// What a command handler does, from its first instructions (with the exact fields of the voice record); for the
// envelope command also how many header words it skips (0 in the early engine). Returns false for a handler that is not
// known: the driver would run whatever the table entry points at.
static bool classify_handler(const DlImage* image, uint32_t handler, uint32_t init, DlCommand* command,
                             uint32_t* envelope_words) {
    DlCursor cursor = { .image = image, .address = handler };
    const uint16_t first = dlc_u16(&cursor);
    const uint16_t second = dlc_u16(&cursor);
    const uint16_t third = dli_u16(image, cursor.address);
    *envelope_words = 0;
    if (handler_does_nothing(image, handler)) {
        *command = DlCommand_Freeze;
    } else if (first == Op_MoveaPostIncA1ToA3) {
        *command = DlCommand_Sample;
    } else if (first == Op_MoveaDisplacedA0ToA1 && second == VoiceField_ListPosition) {
        *command = DlCommand_NextPattern;
    } else if (envelope_header_words(image, handler, envelope_words)
               || (first == Op_MoveLongPostIncA1ToDisplacedA0 && second == VoiceField_EnvelopeStart)) {
        *command = DlCommand_Envelope;
    } else if (first == Op_MoveLongPostIncA1ToDisplacedA0 && second == VoiceField_SlideMode) {
        *command = DlCommand_Slide;
    } else if (first == Op_MoveWordImmediateToDisplacedA0 && third == VoiceField_VibratoState) {
        *command = DlCommand_Vibrato;
    } else if (first == Op_MoveWordImmediateToDisplacedA0 && third == VoiceField_BonusFlag) {
        *command = DlCommand_BonusOn;
    } else if (first == Op_ClrWordDisplacedA0 && second == VoiceField_SlideMode) {
        *command = DlCommand_SlideOff;
    } else if (first == Op_ClrWordDisplacedA0 && second == VoiceField_VibratoState) {
        *command = DlCommand_VibratoOff;
    } else if (first == Op_MoveWordPostIncA1ToDisplacedA0 && second == VoiceField_Count) {
        *command = DlCommand_Rest;
    } else if (first == Op_CmpiWordIndirectA0) {
        *command = DlCommand_Stop;
    } else if (first == Op_BsetCia || first == Op_BclrCia) {
        *command = DlCommand_Filter;
    } else if (first == Op_MoveaIndirectA1ToA1 || first == Op_MoveLongIndirectA1ToD3) {
        *command = DlCommand_Jump;
    } else if (first == Op_TstWordIndirectA1) {
        *command = DlCommand_Repeat;
    } else if (first == Op_JsrAbsolute && (((uint32_t)second << 16) | third) == init) {
        *command = DlCommand_Restart;
    } else {
        return false;
    }
    return true;
}

static bool has_command(const DlVariant* variant, DlCommand command) {
    for (uint32_t i = 0; i < variant->command_count; i++) {
        if (variant->commands[i] == command) {
            return true;
        }
    }
    return false;
}

// The table is a run of handler addresses that ends where the first handler after it begins. Returns false if a handler
// is unknown or the table lacks a command that every driver has.
static bool read_commands(const DlImage* image, uint32_t table, uint32_t init, DlVariant* variant) {
    DlCursor cursor = { .image = image, .address = table };
    uint32_t limit = image->module_size;
    uint32_t count = 0;
    while (count < DL_MAX_COMMANDS && cursor.address < limit) {
        const uint32_t handler = dlc_u32(&cursor);
        if ((handler & 1) != 0 || handler == 0 || handler >= image->module_size) {
            break;
        }
        if (handler > table && handler < limit) {
            limit = handler;
        }
        DlCommand command;
        uint32_t envelope_words;
        if (!classify_handler(image, handler, init, &command, &envelope_words)) {
            return false;
        }
        if (command == DlCommand_Envelope) {
            variant->envelope_header_words = envelope_words;
        }
        variant->commands[count++] = command;
    }
    variant->command_count = count;
    return has_command(variant, DlCommand_Sample) && has_command(variant, DlCommand_NextPattern)
           && has_command(variant, DlCommand_Envelope) && has_command(variant, DlCommand_Rest);
}

// The eagleplayer's count: the first entry, then the entries whose first list address is at least 0x100
static uint32_t count_subsongs(const DlImage* image, uint32_t table) {
    if (table == 0) {
        return 1;
    }
    uint32_t count = 1;
    for (uint32_t entry = table + SUBSONG_ENTRY_SIZE; entry + 4 <= image->module_size; entry += SUBSONG_ENTRY_SIZE) {
        if (dli_u32(image, entry) < SUBSONG_MIN_LIST) {
            break;
        }
        count++;
    }
    return count;
}

// A driver without a subsong table plays one song. Its interrupt calls the voice routine for each channel with the
// address of the voice record in a0 (lea record,a0 / lea AUDxLC,a2); the list of a voice is the record's list start,
// which the init routine sets (move.l #list,field) or the module stores.
static bool find_single_song(const DlImage* image, const DlGameHeader* header, DlSubsong* song) {
    uint32_t records[DL_VOICES];
    uint32_t found = 0;
    for (uint32_t at = header->interrupt; at < header->interrupt + ROUTINE_SPAN && found < DL_VOICES; at += 2) {
        const uint32_t channel = AUDIO_CHANNEL_REGISTERS + found * AUDIO_CHANNEL_STRIDE;
        const uint32_t pattern[]
            = { Op_LeaAbsoluteA0, DL_ANY_WORD, DL_ANY_WORD, Op_LeaAbsoluteA2, channel >> 16, channel & 0xffff };
        if (dl_code_matches(image, at, pattern, nuade_sizeof_array(pattern))) {
            DlCursor operand = { .image = image, .address = at + 2 };
            records[found++] = dlc_u32(&operand);
        }
    }
    if (found != DL_VOICES) {
        return false;
    }
    for (uint32_t v = 0; v < DL_VOICES; v++) {
        const uint32_t list_start = records[v] + VoiceField_ListStart;
        song->lists[v] = dli_u32(image, list_start);
        for (uint32_t at = header->init; at < header->init + ROUTINE_SPAN; at += 2) {
            const uint32_t pattern[]
                = { Op_MoveLongImmediateToAbsolute, DL_ANY_WORD, DL_ANY_WORD, list_start >> 16, list_start & 0xffff };
            if (dl_code_matches(image, at, pattern, nuade_sizeof_array(pattern))) {
                song->lists[v] = dli_u32(image, at + 2);
            }
        }
    }
    return true;
}

// The table of sound effects. A driver that plays effects on channels 2 and 3 starts one by looking its pattern up in
// the table (lea table,a1 / adda.l d0,a1 / move.l (a1),pattern pointer of the effect voice) once for each of the two
// channels; a driver with effects on all four channels has the sequence four times and is not supported.
static uint32_t find_effect_table(const DlImage* image, uint32_t interrupt) {
    const uint32_t lookup[]
        = { Op_LeaAbsoluteA1, DL_ANY_WORD, DL_ANY_WORD, Op_AddaLongD0ToA1, Op_MoveLongIndirectA1ToAbsolute };
    const uint32_t first = dl_code_find(image, interrupt, interrupt + ROUTINE_SPAN, lookup, nuade_sizeof_array(lookup));
    if (first == DL_NOT_FOUND) {
        return DL_NOT_FOUND;
    }
    const uint32_t after_first = first + nuade_sizeof_array(lookup) * 2;
    const uint32_t second
        = dl_code_find(image, after_first, interrupt + ROUTINE_SPAN, lookup, nuade_sizeof_array(lookup));
    if (second == DL_NOT_FOUND
        || dl_code_find(image, second + nuade_sizeof_array(lookup) * 2, interrupt + ROUTINE_SPAN, lookup,
                        nuade_sizeof_array(lookup))
               != DL_NOT_FOUND) {
        return DL_NOT_FOUND;
    }
    DlCursor table = { .image = image, .address = first + 2 };
    DlCursor other = { .image = image, .address = second + 2 };
    const uint32_t address = dlc_u32(&table);
    return address == dlc_u32(&other) ? address : DL_NOT_FOUND;
}

// The init routine of one driver (Frontier) ends with: cmpi.l #n,subsong / bne / move.w #speed,d0 / jsr ramp, where the
// routine ramp sets the mode word to 1, the speed word to d0 and the level word to DL_RAMP_START_LEVEL. That subsong
// starts silent and rises to the music's volume: the voice routine subtracts the level from the envelope value, down to
// 0 (sub.w level,d0 / bcc / move.w #0,d0).
typedef struct RampCall {
    uint32_t subsong;  // the number the init compares the subsong with
    uint32_t compared; // the address it compares
    uint16_t ticks;
    uint32_t routine;
} RampCall;

static RampCall read_ramp_call(const DlImage* image, uint32_t at) {
    DlCursor cursor = { .image = image, .address = at };
    RampCall call;
    dlc_u16(&cursor); // cmpi.l
    call.subsong = dlc_u32(&cursor);
    call.compared = dlc_u32(&cursor);
    dlc_u16(&cursor); // bne.w
    dlc_u16(&cursor);
    dlc_u16(&cursor); // move.w #ticks,d0
    call.ticks = dlc_u16(&cursor);
    dlc_u16(&cursor); // jsr
    call.routine = dlc_u32(&cursor);
    return call;
}

// The address of the level word if `routine` sets the ramp up (mode 1, speed d0, level), else DL_NOT_FOUND
static uint32_t ramp_level_address(const DlImage* image, uint32_t routine) {
    const uint32_t setter[] = { Op_MoveWordImmediateToAbsolute,
                                RAMP_MODE_ON,
                                DL_ANY_WORD,
                                DL_ANY_WORD,
                                Op_MoveWordD0ToAbsolute,
                                DL_ANY_WORD,
                                DL_ANY_WORD,
                                Op_MoveWordImmediateToAbsolute,
                                DL_RAMP_START_LEVEL,
                                DL_ANY_WORD,
                                DL_ANY_WORD,
                                Op_Rts };
    if (!dl_code_matches(image, routine, setter, nuade_sizeof_array(setter))) {
        return DL_NOT_FOUND;
    }
    DlCursor cursor = { .image = image, .address = routine };
    dlc_u16(&cursor); // move.w #1,mode
    dlc_u16(&cursor);
    dlc_u32(&cursor);
    dlc_u16(&cursor); // move.w d0,speed
    dlc_u32(&cursor);
    dlc_u16(&cursor); // move.w #level,level
    dlc_u16(&cursor);
    return dlc_u32(&cursor);
}

// Returns false for a driver that has the call but not the subtraction in its voice routine
static bool find_volume_ramp(const DlImage* image, const DlGameHeader* header, uint32_t routine, DlGameModule* module) {
    const uint32_t call_code[]
        = { Op_CmpiLongToAbsolute,  DL_ANY_WORD, DL_ANY_WORD,    DL_ANY_WORD, DL_ANY_WORD, Op_BneWord, DL_ANY_WORD,
            Op_MoveWordImmediateD0, DL_ANY_WORD, Op_JsrAbsolute, DL_ANY_WORD, DL_ANY_WORD };
    const uint32_t at
        = dl_code_find(image, header->init, header->init + ROUTINE_SPAN, call_code, nuade_sizeof_array(call_code));
    if (at == DL_NOT_FOUND) {
        return true;
    }
    const RampCall call = read_ramp_call(image, at);
    const uint32_t level = ramp_level_address(image, call.routine);
    if ((call.compared != header->subsong_variable && call.compared + 2 != header->subsong_variable)
        || level == DL_NOT_FOUND) {
        return true; // some other call
    }
    const uint32_t subtract[]
        = { Op_SubWordAbsoluteToD0, level >> 16, level & 0xffff, Op_BccWord, DL_ANY_WORD, Op_MoveWordImmediateD0, 0 };
    if (!code_contains(image, routine, routine + ROUTINE_SPAN, subtract, nuade_sizeof_array(subtract))) {
        return false;
    }
    module->ramp_subsong = call.subsong;
    module->ramp_ticks = call.ticks;
    return true;
}

bool dl_game_locate(DlImage* image, DlGameModule* module) {
    const DlGameHeader header = read_header(image);
    if (!header_is_valid(&header) || !dl_run_load_hook(image, header.load_hook)) {
        return false;
    }
    const uint32_t routine = find_voice_routine(image);
    const uint32_t table = routine == DL_NOT_FOUND ? DL_NOT_FOUND : find_command_table(image, routine);
    if (table == DL_NOT_FOUND || !read_commands(image, table, header.init, &module->variant)) {
        return false;
    }
    const uint32_t routine_end = routine + ROUTINE_SPAN;
    const uint32_t bonus[] = { Op_CmpiWordDisplacedA0, 1, VoiceField_BonusFlag };
    module->variant.envelope_adds_volume = code_contains(image, routine, routine_end, bonus, nuade_sizeof_array(bonus));
    const uint32_t vibrato_delay[] = { Op_TstWordDisplacedA0, VoiceField_VibratoDelay };
    module->variant.vibrato_delay
        = code_contains(image, routine, routine_end, vibrato_delay, nuade_sizeof_array(vibrato_delay));
    // One driver (Formula One Grand Prix) writes the DMA request after every voice instead of once for all four
    const uint32_t dma_after_voice[]
        = { Op_JsrAbsolute, DL_ANY_WORD, DL_ANY_WORD, Op_MoveWordD1ToDmacon, DMACON_HIGH_WORD, DMACON_LOW_WORD };
    module->variant.dma_every_tick = code_contains(image, header.interrupt, header.interrupt + ROUTINE_SPAN,
                                                   dma_after_voice, nuade_sizeof_array(dma_after_voice));
    module->effect_request = header.effect_request;
    if (header.effect_request != 0) {
        // The engine ducks the music with the unclamped subtraction of the Time scanner driver
        const uint32_t cut[]
            = { Op_SubWordDisplacedA0ToD0, VoiceField_VolumeCut, Op_MoveWordD0ToDisplacedA2, AUDIO_VOLUME_REGISTER };
        module->variant.effect_table = find_effect_table(image, header.interrupt);
        if (module->variant.effect_table == DL_NOT_FOUND
            || !code_contains(image, routine, routine_end, cut, nuade_sizeof_array(cut))) {
            return false;
        }
    }
    if (!find_volume_ramp(image, &header, routine, module)) {
        return false;
    }
    module->subsong_table = header.subsong_table;
    module->subsong_count = count_subsongs(image, header.subsong_table);
    if (header.subsong_table == 0 && !find_single_song(image, &header, &module->single_song)) {
        return false;
    }
    module->sample_records = header.sample_records;
    dl_patch_midwinter(image, header.module_size, module->subsong_count);
    return true;
}

DlSubsong dl_game_subsong(const DlImage* image, const DlGameModule* module, uint32_t index) {
    DlSubsong subsong = module->single_song;
    if (module->subsong_table != 0) {
        DlCursor cursor = { .image = image, .address = module->subsong_table + index * SUBSONG_ENTRY_SIZE };
        subsong = (DlSubsong) { 0 };
        for (uint32_t v = 0; v < DL_VOICES; v++) {
            subsong.lists[v] = dlc_u32(&cursor);
        }
    }
    if (index == 0) {
        subsong.effect_request = module->effect_request;
    }
    if (module->ramp_subsong == index + 1) {
        subsong.ramp_ticks = module->ramp_ticks;
    }
    return subsong;
}
