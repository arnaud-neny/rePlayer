// SPDX-License-Identifier: MIT
// Dynamic Synthesizer player: types shared by the module loader and the replayer.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/dynamicsynthesizer.h>

#define DNS_NO_VOICE 0xFFFFu // the voice map's "no voice" value
#define DNS_MODULE_PREFIX "dns."
#define DNS_SAMPLE_PREFIX "smp."

// Does the file name (without its directory) start with "dns."? The sample file's name is made from it.
bool dns_has_module_prefix(const char* name);

// The module and the samples: bytes, zero beyond the end (a read past the end returns 0; the replayer would read
// whatever follows in memory).

uint8_t dns_u8(const DnsMemory* m, int64_t offset);
uint16_t dns_u16(const DnsMemory* m, int64_t offset);
uint32_t dns_u32(const DnsMemory* m, int64_t offset);

bool dns_samples_init(DnsSamples* samples, const uint8_t* data, size_t size);
void dns_samples_free(DnsSamples* samples);

// The module's records

typedef struct DnsSong {
    uint16_t position;      // first position (row) of the song
    uint16_t loop_position; // where it loops to
    uint16_t end_position;
    uint16_t speed;         // ticks per step
    bool muted[DNS_TRACKS]; // the track is silent
    uint16_t step_limit;    // bytes of a pattern that play, 0 for the rows version
} DnsSong;

// The pattern and the transpositions that a track plays at a position.
typedef struct DnsTrackRow {
    uint8_t pattern; // 1-based, 0 = none
    uint8_t note_add;
    uint8_t sample_add;
} DnsTrackRow;

// The eagleplayer's check and the code scan: false for anything the player cannot play. The module's memory points
// at `data`.
bool dns_module_locate(DnsModule* module, const uint8_t* data, size_t size);
// Locates the replayer, then makes a copy of the module.
DnsStatus dns_module_load(DnsModule* module, const uint8_t* data, size_t size);
void dns_module_free(DnsModule* module);

uint16_t dns_read_period(const DnsModule* module, unsigned index);
DnsSong dns_read_song(const DnsModule* module, unsigned song);
DnsInstrument dns_read_instrument(const DnsModule* module, unsigned number);
DnsTrackRow dns_read_track_row(const DnsModule* module, unsigned track, uint16_t position);
// The offset of the pattern with the 1-based number `pattern`.
uint32_t dns_pattern_offset(const DnsModule* module, uint8_t pattern);
uint16_t dns_read_pattern_word(const DnsModule* module, uint32_t pattern, int16_t step);

// The replayer's state

void dns_replayer_start(DnsPlayer* player, unsigned song);
void dns_replayer_tick(DnsPlayer* player);
