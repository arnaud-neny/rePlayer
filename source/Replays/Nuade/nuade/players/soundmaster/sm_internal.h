// SPDX-License-Identifier: MIT
// Sound Master player: types shared by the module scan (sm_module.c) and the replayer (soundmaster.c, sm_voice.c).
//
// A module holds the replayer's 68k code, its variables and the song's tables. The code is one of seven versions
// (SmstVersion). The port runs that version's replayer on the module's bytes: every table is read from the module image
// at the address the original computes, and the variables the original keeps next to the code are the fields of
// SmstPlayer.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <nuade/players/soundmaster.h>

#define FADE_FULL 0x40
#define FADE_UP 0x80          // fade control: the fade rises
#define VOICE_NO_FADE 0x80    // a voice's position byte: the fade leaves it alone
#define NOTE_RELEASE 0x80     // note byte: bit 7
#define SLOT_SIZE 10u         // Offsets layout: bytes of a sample slot
#define SUBSONG_ENTRY_SIZE 3u // the subsong table: first position, loop start, end

// The module

// A read outside the image returns 0, as the zeroed memory around the module would.
uint8_t smst_u8(const SmstModule* m, uint32_t address);
uint16_t smst_u16(const SmstModule* m, uint32_t address);
uint32_t smst_u32(const SmstModule* m, uint32_t address);

// The version whose code the module carries, null when none; the Check2 conditions are part of it.
const SmstVersion* smst_find_version(const uint8_t* data, size_t size);

// Subsongs of the module as the eagleplayer's InitPlayer counts them.
int smst_count_subsongs(const SmstModule* m);

// Fills the module from the image; false when the tables are not where the replayer can read them safely.
bool smst_locate(SmstModule* m, const SmstVersion* version);

// A subsong record of the table (SmstInit_Table).
typedef struct SmstSubsong {
    uint8_t position;
    uint8_t loop_start;
    uint8_t end;
} SmstSubsong;

SmstSubsong smst_subsong(const SmstModule* m, uint32_t index);

// Fixed layout: what the module holds in the scratch words the init leaves alone: the channels the last tick's wave
// switches stopped (a bit each, checked at locate) and the global fade.
uint8_t smst_initial_wave_switches(const SmstModule* m);
uint8_t smst_initial_fade(const SmstModule* m);

// A sample slot, as the replayer reads it.
typedef struct SmstSlot {
    uint32_t sample;      // address of the first byte
    uint16_t length;      // words
    uint16_t loop_offset; // bytes from the start
    uint16_t loop_length; // words
} SmstSlot;

SmstSlot smst_slot(const SmstModule* m, uint8_t number);

// An instrument record.
typedef struct SmstInstrument {
    uint8_t wave; // 0: sample number, bit 7: the voice starts in the loop part
    uint8_t attack;
    uint8_t peak;
    uint8_t decay;
    uint8_t sustain;
    uint8_t vibrato_delay;
    uint8_t vibrato_depth;
    uint8_t vibrato_speed; // low nibble: period, high nibble: depth step
    uint8_t release;
    uint8_t arpeggio_start;
    uint8_t arpeggio_length;
    uint8_t arpeggio_repeat;
    uint8_t arpeggio_note;
    uint8_t note_offset;
    uint8_t sweep_period;
    uint8_t sweep_step;
} SmstInstrument;

SmstInstrument smst_instrument(const SmstModule* m, uint8_t number);

// The sample start the replayer hands to Paula: the address inside the image, a pointer into the zeros behind it when
// the address is outside.
const int8_t* smst_sample_pointer(const SmstModule* m, uint32_t address);

// The period table entry.
uint16_t smst_period(const SmstModule* m, uint8_t index);

// The voice routines (sm_voice.c)

// AUDxLC / AUDxLEN of the voice's loop, which Paula takes over when the sample ends.
void smst_write_loop_registers(const SmstPlayer* p, SmstVoice* v);

// The note of the voice's row: sample, envelope, period.
void smst_note_start(SmstPlayer* p, SmstVoice* v);

// Arpeggio, vibrato, slide, sweep, envelope and volume: every tick. `step` is the tick's latest arpeggio step: the
// sweep of a voice without an arpeggio reads the one of an earlier voice.
void smst_voice_effects(SmstPlayer* p, SmstVoice* v, uint8_t* step);
