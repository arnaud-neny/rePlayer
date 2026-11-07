// SPDX-License-Identifier: MIT
// Identification of the replayer that a Sean Conran module carries: a decoder for the 68000 instructions the replayer
// is made of, and a hash of its code with the initial stream pointers taken out.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ScrCodeScan {
    uint32_t end;  // offset behind the last instruction; the replayer's scratch bytes start here
    uint64_t hash; // of the code
} ScrCodeScan;

// Decodes the replayer from `start` up to the scratch bytes it copies stream parameters into, and hashes it. What does
// not enter the hash: the absolute addresses the code loads its stream pointers from (`lea.l abs.l,a0`): they are
// overwritten before they are read. Everything else is in the
// hash, so a module whose code differs in an opcode, an offset, a branch or an initial value of a variable is refused.
// false when the code holds an instruction that is not part of the replayers, or does not end where the scratch bytes
// begin.
bool scr_scan_code(const uint8_t* data, size_t size, uint32_t start, ScrCodeScan* scan);
