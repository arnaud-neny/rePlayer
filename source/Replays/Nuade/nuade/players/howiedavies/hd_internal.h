// SPDX-License-Identifier: MIT
// Howie Davies: the module (the executable the replayer lives in), the three known replayer versions and the typed
// readers of the module records.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/howiedavies.h>

// Entries of the period table; the replayer indexes past it into its own tables, which is not modelled.
#define HD_PERIOD_COUNT 86u

// "No address": a pointer the replayer never loaded.
#define HD_NO_POINTER UINT32_MAX

// The 16 byte record of a sample.
typedef struct HdSample {
    const int8_t* start;
    uint16_t words;
    const int8_t* loop; // the repeat; silence for a sample without loop
    uint16_t loop_words;
    uint16_t volume;
    uint16_t envelope; // index of the volume envelope
} HdSample;

// Parses the executable and finds the replayer version (the eagleplayer's Check2 and the signature). `bytes` stays
// owned by the caller and must outlive the module.
bool hd_module_parse(HdModule* module, const uint8_t* bytes, size_t size);
void hd_module_free(HdModule* module);

// A byte of the hunk; false past its end.
bool hd_byte(const HdModule* module, uint32_t at, uint8_t* out);

// A relocated 32 bit word of the hunk: the code offset it points at, which must lie inside the hunk.
bool hd_pointer(const HdModule* module, uint32_t at, uint32_t* out);

// The period of a note of the period table.
bool hd_period(const HdModule* module, uint32_t note, uint16_t* out);

// Sample record `index`; false when the record, its pointers or its data lie outside the hunk.
bool hd_sample(const HdModule* module, uint32_t index, HdSample* out);

// The first pattern byte of pattern `index`.
bool hd_pattern(const HdModule* module, uint32_t index, uint32_t* out);

// The first envelope step of envelope `index`.
bool hd_envelope(const HdModule* module, uint32_t index, uint32_t* out);

// The voice pointers of a subsong: the start of the position list of `voice`.
bool hd_voice_list(const HdModule* module, uint32_t subsong, uint32_t voice, uint32_t* out);

// The routine the command byte `command` leads to through the table at `table`.
bool hd_handler(const HdModule* module, uint32_t table, uint8_t command, HdHandler* out);

// Silence: the 4 zero bytes the eagleplayer substitutes for the address of sample record 0 and for a zero repeat.
const int8_t* hd_silence(void);
