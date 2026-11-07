// SPDX-License-Identifier: MIT
// Dave Lowe player: the load hooks of some game drivers.
//
// Four of the drivers cannot play as they come off the disk: their sample records and pointers hold offsets and
// indices, which a routine inside the module turns into addresses. The eagleplayer runs that routine once when it
// loads the module (the load hook of the header). The player recognises the three routines that occur by their code
// and applies their effect to the image; a module with another hook is not supported.
//
//   resolve indices: each of N longs, at the addresses of a list, is an index into a table of longs; replace it with
//                    the table entry. (F-15 Strike Eagle II)
//   fix records:     tables of (field address, offset, size, loop length) entries turn the offsets into sample record
//                    pointers: the pointer is the bank address plus the offset, the loop pointer the bank address plus
//                    the size. (Beneath a Steel Sky)
//   set bases:       a chain of pointer set-ups: every table starts where the previous one ended. (Raffles)

#include "dl_internal.h"
#include "nuade_utils.h"

// The loop of the index hook: movea.l (a0)+,a1 / move.l (a1),d0 / move.l (a2,d0.w),(a1) / dbra d7 / rts
static const uint16_t s_index_loop[] = { 0x2258, 0x2011, 0x22b2, 0x0000, 0x51cf, 0xfff6, Op_Rts };
// The words of the record fixer that precede the comparison with the marker of a record that has no loop
static const uint16_t s_record_fixer_start[] = { 0x5341, 0x2608, 0x265a, 0x241a, 0xd483, 0x2682, 0x201a, 0x0c80 };

// The record fixer is called for three banks of samples
#define FIXER_CALLS 3

// A hook that does not stop is broken
#define MAX_HOOK_STEPS 4096

static uint32_t signed_word(const DlImage* image, uint32_t address) {
    return (uint32_t)(int32_t)(int16_t)dli_u16(image, address);
}

static bool expect_words(DlCursor* cursor, const uint16_t* words, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (dlc_u16(cursor) != words[i]) {
            return false;
        }
    }
    return true;
}

// An lea whose opcode is `pc_relative_opcode` (relative to its extension word) or the absolute form, which is one
// less; returns false for another opcode
static bool read_lea(DlCursor* cursor, uint16_t pc_relative_opcode, uint32_t* target) {
    const uint16_t op = dlc_u16(cursor);
    if (op == pc_relative_opcode) {
        const uint32_t extension = cursor->address;
        *target = extension + signed_word(cursor->image, extension);
        cursor->address += 2;
        return true;
    }
    if (op == pc_relative_opcode - 1) {
        *target = dlc_u32(cursor);
        return true;
    }
    return false;
}

// lea table,a2 / lea list,a0 / moveq #n-1,d7 / the loop
static bool hook_resolve_indices(DlImage* image, uint32_t at) {
    DlCursor cursor = { .image = image, .address = at };
    if (dlc_u16(&cursor) != Op_LeaAbsoluteA2) {
        return false;
    }
    const uint32_t table = dlc_u32(&cursor);
    if (dlc_u16(&cursor) != Op_LeaAbsoluteA0) {
        return false;
    }
    const uint32_t list = dlc_u32(&cursor);
    const uint16_t count_op = dlc_u16(&cursor);
    if ((count_op & 0xff00) != Op_MoveqD7 || !expect_words(&cursor, s_index_loop, nuade_sizeof_array(s_index_loop))) {
        return false;
    }
    const uint32_t count = (count_op & 0xffu) + 1u;
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t field = dli_u32(image, list + i * 4);
        dli_set32(image, field, dli_u32(image, table + signed_word(image, field + 2)));
    }
    return true;
}

// One table of the record fixer: `count` entries of (address of a record's pointer field, offset, size, loop length)
static void fix_records(DlImage* image, uint32_t bank, uint32_t table, uint32_t count, uint32_t no_loop_marker) {
    DlCursor entry = { .image = image, .address = table };
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t record = dl_sample_record_of_address_field(dlc_u32(&entry));
        const uint32_t offset = dlc_u32(&entry);
        const uint32_t size = dlc_u32(&entry);
        const uint16_t loop_length = dlc_u16(&entry);
        dl_write_sample_address(image, record, bank + offset);
        if (size != no_loop_marker) {
            dl_write_sample_loop(image, record, bank + size, loop_length);
        }
    }
}

// A call of the record fixer: the setup is lea table,a2 / move.w #n,d1 / jsr fixer / rts, and the fixer compares each
// size with the address that marks "no loop"
static bool run_record_setup(DlImage* image, uint32_t setup, uint32_t bank) {
    DlCursor cursor = { .image = image, .address = setup };
    uint32_t table;
    if (!read_lea(&cursor, Op_LeaPcA2, &table) || dlc_u16(&cursor) != Op_MoveWordImmediateD1) {
        return false;
    }
    const uint16_t count = dlc_u16(&cursor);
    if (dlc_u16(&cursor) != Op_JsrAbsolute) {
        return false;
    }
    DlCursor fixer = { .image = image, .address = dlc_u32(&cursor) };
    if (!expect_words(&fixer, s_record_fixer_start, nuade_sizeof_array(s_record_fixer_start))) {
        return false;
    }
    fix_records(image, bank, table, count, dlc_u32(&fixer));
    return true;
}

// Three calls of (lea bank,a0 / bsr setup), then rts
static bool hook_fix_records(DlImage* image, uint32_t at) {
    DlCursor cursor = { .image = image, .address = at };
    uint32_t banks[FIXER_CALLS];
    uint32_t setups[FIXER_CALLS];
    for (uint32_t i = 0; i < FIXER_CALLS; i++) {
        if (dlc_u16(&cursor) != Op_LeaAbsoluteA0) {
            return false;
        }
        banks[i] = dlc_u32(&cursor);
        if (dlc_u16(&cursor) != Op_BsrWord) {
            return false;
        }
        setups[i] = cursor.address + signed_word(image, cursor.address);
        cursor.address += 2;
    }
    if (dlc_u16(&cursor) != Op_Rts) {
        return false;
    }
    for (uint32_t i = 0; i < FIXER_CALLS; i++) {
        if (!run_record_setup(image, setups[i], banks[i])) {
            return false;
        }
    }
    return true;
}

// The address arithmetic of a straight-line hook: `pointer` is the address being built, `field` the place that the next
// store writes it to, `steps` a table of words that the closing loop adds to the pointer.
typedef struct BaseChain {
    uint32_t pointer;
    uint32_t field;
    uint32_t steps;
    uint32_t loops_left;
} BaseChain;

static bool hook_set_bases(DlImage* image, uint32_t at) {
    BaseChain chain = { 0 };
    DlCursor cursor = { .image = image, .address = at };
    for (int step = 0; step < MAX_HOOK_STEPS; step++) {
        const uint32_t instruction = cursor.address;
        const uint16_t op = dlc_u16(&cursor);
        switch (op) {
            case Op_LeaPcA0:
            case Op_LeaAbsoluteA0:
                cursor.address = instruction;
                read_lea(&cursor, Op_LeaPcA0, &chain.pointer);
                break;
            case Op_LeaPcA1:
            case Op_LeaAbsoluteA1:
                cursor.address = instruction;
                read_lea(&cursor, Op_LeaPcA1, &chain.field);
                break;
            case Op_LeaPcA2:
            case Op_LeaAbsoluteA2:
                cursor.address = instruction;
                read_lea(&cursor, Op_LeaPcA2, &chain.steps);
                break;
            case Op_LeaDisplacedA0:
                chain.pointer += signed_word(image, cursor.address);
                cursor.address += 2;
                break;
            case Op_LeaDisplacedA1:
                chain.field += signed_word(image, cursor.address);
                cursor.address += 2;
                break;
            case Op_MoveLongA0ToIndirectA1:
                dli_set32(image, chain.field, chain.pointer);
                break;
            case Op_MoveLongA0ToDisplacedA1:
                dli_set32(image, chain.field + signed_word(image, cursor.address), chain.pointer);
                cursor.address += 2;
                break;
            case Op_AddaWordPostIncA2ToA0:
                chain.pointer += signed_word(image, chain.steps);
                chain.steps += 2;
                break;
            case Op_DbraD0:
                if (chain.loops_left-- != 0) {
                    cursor.address += signed_word(image, cursor.address);
                } else {
                    cursor.address += 2;
                }
                break;
            case Op_Rts:
                return true;
            default:
                if ((op & 0xff00) != Op_MoveqD0) {
                    return false;
                }
                chain.loops_left = op & 0xffu;
                break;
        }
    }
    return false;
}

bool dl_run_load_hook(DlImage* image, uint32_t hook) {
    if (hook == 0) {
        return true;
    }
    return hook_resolve_indices(image, hook) || hook_fix_records(image, hook) || hook_set_bases(image, hook);
}
