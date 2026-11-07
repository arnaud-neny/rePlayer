// SPDX-License-Identifier: MIT
// Identification of the replayer that a Rob Hubbard module carries: a decoder for the 68000 instructions the replayer
// is made of, and a hash of its code with the operands that change from module to module taken out.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Most pc-relative table pointers the replayer's code holds.
#define RH_MAX_PC_TARGETS 24

typedef struct RhCodeScan {
    uint32_t end;  // offset behind the last instruction; the replayer's variables start here
    uint64_t hash; // of the code
    // The targets of the pc-relative addresses, in code order, without the loads of the module base (`lea 0(pc),a3`).
    uint32_t pc_targets[RH_MAX_PC_TARGETS];
    int pc_count;
} RhCodeScan;

// Decodes the code at the start of the module and hashes it. What does not enter the hash: the displacements of the
// table pointers (they are returned in the scan), the number of samples (the immediate of the moveq at
// `sample_count_at`), and the position of the song table (`song_table`: an operand that is that offset from the module
// base is the song table, any other offset from the module base is taken relative to the end of the code).
// false when the code holds an instruction that is not part of the replayers, or runs past the module.
bool rh_scan_code(const uint8_t* data, size_t size, uint32_t song_table, uint32_t sample_count_at, RhCodeScan* scan);
