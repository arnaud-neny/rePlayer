// SPDX-License-Identifier: MIT
// Jason Page player: types shared by the module loader, the sequencer and the voices.

#pragma once

#include <nuade/players/jasonpage.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdbool.h>
#include <stdint.h>

#define JP_SUBSONG_MASK 15 // the replayers index their tables with the subsong number & 15
#define JP_BYTE_MASK 0xFFu

// Track list words: the high byte names a pattern or one of these, the low byte is the pattern's transposition or the
// target of a loop.
#define JP_TRACK_END 0xFF
#define JP_TRACK_LOOP 0xFE
#define JP_TRACK_LOOP_ALT 0xFC // the replayer tests a flag that nothing sets, so it acts as JP_TRACK_LOOP
#define JP_TRACK_SKIP 0xFD

// Virocop's hold phase lets the volume fall to the hold volume; this value never falls (the volume is at most 0xFC00).
#define JP_HOLD_VOLUME_NONE 0xFFFFu

// Memory: the module, the sample file, and the two small buffers the replayer keeps for itself, in one image whose
// offsets play the part of the 68000 addresses. A read outside it gives 0, a write outside it is refused.

uint8_t jp_u8(const JpMemory* mem, uint32_t address);
uint16_t jp_u16(const JpMemory* mem, uint32_t address);
int16_t jp_s16(const JpMemory* mem, uint32_t address);
uint32_t jp_u32(const JpMemory* mem, uint32_t address);
bool jp_store_u8(JpMemory* mem, uint32_t address, uint8_t value);
bool jp_store_u16(JpMemory* mem, uint32_t address, uint16_t value);
bool jp_store_u32(JpMemory* mem, uint32_t address, uint32_t value);

// The player.

// jp_module.c
bool jp_module_probe(const uint8_t* data, size_t size);
JpStatus jp_module_load(JpPlayer* player, const uint8_t* data, size_t size, const char* path, const NuadeIo* io);
void jp_module_free(JpPlayer* player);
// Put the image back as it was loaded (scripts rewrite samples).
void jp_module_reset(JpPlayer* player);
// The address a table entry points at: Realms keeps absolute 32-bit pointers, the others a signed 16-bit offset from
// `base`.
uint32_t jp_table_pointer(const JpPlayer* player, uint32_t table, uint32_t base, uint32_t index);
// A word table entry; Graftgold and Virocop use only its low byte, Realms the whole word.
uint16_t jp_table_value(const JpPlayer* player, uint32_t table, uint32_t index);

// jp_seq.c
void jp_seq_reset(JpPlayer* player, int subsong);
void jp_seq_step(JpPlayer* player);

// jp_voice.c
void jp_voice_initial_state(JpVoice* voice, JpFormat format, uint8_t number);
// Queue an instrument to start at the next tick; false when a sound of higher priority plays.
bool jp_voice_queue(const JpPlayer* player, JpVoice* voice, uint16_t instrument);
// The period of a note index from a pattern, via the note table (fails the player on an index it cannot model).
uint16_t jp_voice_pattern_period(JpPlayer* player, uint8_t note);
void jp_voice_tick(JpPlayer* player, JpVoice* voice);
void jp_voice_output(JpPlayer* player, JpVoice* voice);
