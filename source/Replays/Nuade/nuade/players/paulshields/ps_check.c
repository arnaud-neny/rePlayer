// SPDX-License-Identifier: MIT
// Paul Shields player: the static walk of a song. Every voice's order list names patterns, every pattern is a run of
// commands up to the one that leaves it. The replayer defines a meaning for most command bytes; for the rest it jumps
// into the middle of its code or onto an address register it left in an undefined state. A module whose voices reach
// such a command is refused, so that UADE plays it.

#include "ps_internal.h"
#include <stdlib.h>

// A pattern duration: one byte, or a zero and a big-endian word.

static int64_t skip_duration(const PshModule* m, int64_t at) {
    return psh_u8(m, at) == 0 ? at + 3 : at + 1;
}

// A sample command of the short and long layouts needs an entry in the sample table.

static bool sample_exists(const PshModule* m, int64_t at) {
    return psh_sample_number(m, psh_u8(m, at)) != nullptr;
}

// The commands of one pattern of the short and long layouts, up to the one that leaves it.

static bool check_pattern(const PshModule* m, int64_t at) {
    for (int guard = 0; guard < (int)m->size + PSH_COMMAND_LIMIT && at < (int64_t)m->size; guard++) {
        switch ((PshPatternCommand)psh_u8(m, at++)) {
            case PshPatternCommand_Note:
                at = skip_duration(m, at + 1);
                break;
            case PshPatternCommand_Sample:
                if (!sample_exists(m, at)) {
                    return false;
                }
                at++;
                break;
            case PshPatternCommand_VolumeOverride:
            case PshPatternCommand_DirectPitchEnvelope:
            case PshPatternCommand_VolumeEnvelope:
            case PshPatternCommand_NotePitchEnvelope:
                at++;
                break;
            case PshPatternCommand_Rest:
                at = skip_duration(m, at);
                break;
            case PshPatternCommand_Slide:
                at = skip_duration(m, at + 2);
                break;
            case PshPatternCommand_NextPattern:
            case PshPatternCommand_NextPatternAlias:
            case PshPatternCommand_RestartOrderList:
            case PshPatternCommand_Stop:
                return true;
            default:
                return false;
        }
    }
    return true;
}

// An instrument command of the old layout needs a record that names an entry of the sample table.

static bool instrument_exists(const PshModule* m, int64_t at) {
    return psh_instrument_sample(m, (int64_t)m->instruments + psh_u8(m, at)) != nullptr;
}

// The commands of one pattern of the old layout, up to the one that leaves it.

static bool check_old_pattern(const PshModule* m, int64_t at) {
    for (int guard = 0; guard < (int)m->size + PSH_COMMAND_LIMIT && at < (int64_t)m->size; guard++) {
        const uint8_t byte = psh_u8(m, at++);
        if (byte < PshOldPatternCommand_Rest) {
            at = skip_duration(m, at);
            continue;
        }
        switch ((PshOldPatternCommand)byte) {
            case PshOldPatternCommand_Rest:
                at = skip_duration(m, at);
                break;
            case PshOldPatternCommand_Slide:
                at = skip_duration(m, at + 2);
                break;
            case PshOldPatternCommand_VolumeOverride:
            case PshOldPatternCommand_VolumeOverrideAlias:
            case PshOldPatternCommand_LevelOnly:
                at++;
                break;
            case PshOldPatternCommand_Instrument:
                if (!instrument_exists(m, at)) {
                    return false;
                }
                at++;
                break;
            case PshOldPatternCommand_NextPattern:
                return true;
            case PshOldPatternCommand_FilterOn:
            case PshOldPatternCommand_FilterOff:
            case PshOldPatternCommand_FilterOffAlias:
            case PshOldPatternCommand_NoOperation:
                break;
            default:
                return false;
        }
    }
    return true;
}

// One voice: its order list up to the loop or stop code, with every pattern it names checked once.

static bool check_voice(const PshModule* m, int64_t at, bool* seen) {
    for (int guard = 0; guard < (int)m->size + PSH_COMMAND_LIMIT; guard++) {
        const uint16_t word = psh_u16(m, at);
        at += 2;
        if (word < PSH_ORDER_CODE_MIN) {
            if ((word & PSH_ORDER_REPEATS_BIT) != 0) {
                at += 2; // the repeat count
            }
            const uint32_t pattern = word & PSH_ORDER_OFFSET_MASK;
            if (pattern >= m->size || seen[pattern]) {
                continue;
            }
            seen[pattern] = true;
            const bool ok = m->layout->kind == PshKind_Old ? check_old_pattern(m, pattern) : check_pattern(m, pattern);
            if (!ok) {
                return false;
            }
            continue;
        }
        PshOrderAction action;
        if (!psh_order_action(m->layout, word, &action)) {
            return false;
        }
        switch (action) {
            case PshOrderAction_VolumeSlide:
            case PshOrderAction_AllVolumeSlide:
                at += 4;
                break;
            case PshOrderAction_PitchOffset:
            case PshOrderAction_PitchTune:
                at += 2;
                break;
            case PshOrderAction_Loop:
            case PshOrderAction_Stop:
                return true;
        }
    }
    return true;
}

bool psh_check_song(const PshModule* m) {
    bool* seen = calloc((size_t)m->size + 1, sizeof(bool));
    if (seen == nullptr) {
        return false;
    }
    bool ok = true;
    for (uint32_t i = 0; i < PSH_VOICES && ok; i++) {
        ok = check_voice(m, m->voices[i].order_list, seen);
    }
    free(seen);
    return ok;
}
