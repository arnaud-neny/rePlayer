// SPDX-License-Identifier: MIT
// Fred Gray player: decoding the song sections. A section is a 68k routine of the module; the decoder turns every
// instruction sequence it knows into one FgOp and fails on any other. The locator hands it what it found in the
// engine (the variables the sections store to, the voice stride) and gets back the sections and the routines they call.

#pragma once

#include "fg_code.h"

#define FG_MAX_ENGINE_CALLS 64

// The request variables: three bytes in a row
typedef enum FgRequestVar {
    FgRequestVar_Request = 0,     // the pending request (a subsong's first section)
    FgRequestVar_NextSection = 1, // the section to start with the next tick
    FgRequestVar_Section = 2,     // the current section
} FgRequestVar;

static inline uint32_t fg_request_var(uint32_t base, FgRequestVar which) {
    return base + (uint32_t)which;
}

// The engine routines the sections call, found by what they start with; FG_NOT_FOUND where no section calls one
typedef struct FgCallees {
    uint32_t tempo;
    uint32_t setup;
    uint32_t track[FG_ACTIVE_VOICES];
    uint32_t track_nibble;
    uint32_t mask;
    uint32_t reset;
    uint32_t tail_next;
    uint32_t tail_advance;
} FgCallees;

// Every engine routine a section calls (without repeats)
typedef struct FgEngineCalls {
    uint32_t routine[FG_MAX_ENGINE_CALLS];
    uint32_t count;
} FgEngineCalls;

typedef struct FgSectionContext {
    const FgModule* m;
    uint32_t stride;        // bytes of a voice record
    uint32_t engine_start;  // code at or after this address is the engine
    uint32_t position_var;  // the section position (a word)
    uint32_t transpose_var; // FG_NOT_FOUND where the version has none
    uint32_t cap_var;       // FG_NOT_FOUND where the version has none
    uint32_t request_vars;  // see FgRequestVar
} FgSectionContext;

// Decodes the routines of sections 1 .. count-1 (section 0 is the song end) into the layout's sections and op pool.
// Sections that share a routine share their operations; an entry that is the engine's first routine is an empty
// section.
bool fg_decode_sections(const FgSectionContext* ctx, const uint32_t* entries, uint32_t count, FgLayout* layout,
                        FgCallees* callees, FgEngineCalls* calls);
