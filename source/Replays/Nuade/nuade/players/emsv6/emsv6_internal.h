// SPDX-License-Identifier: MIT
// EMS v6 player: the types the module reader, the sequencer and the mixer share.

#pragma once

#include <nuade/players/emsv6.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define E6_NOTE_MASK 0x7F   // the period table index of a note
#define E6_RING_SIZE 0x8000 // bytes of each of the four Paula buffers
#define E6_RING_MASK (E6_RING_SIZE - 1)
#define E6_MASTER_FULL 0x40 // the master volume at the start of a song

// UADE's configuration of the eagleplayer (players/ENV/EaglePlayer/ems_v6.cfg): mixing mode 5, 24000 Hz, boost 0. The
// Paula period and the rate follow from the integer divisions the eagleplayer makes.
#define E6_PAULA_CLOCK 3546895
#define E6_CONFIG_RATE 24000
#define E6_PAULA_PERIOD (E6_PAULA_CLOCK / E6_CONFIG_RATE) // 147
#define E6_RATE (E6_PAULA_CLOCK / E6_PAULA_PERIOD)        // 24128 samples per second
#define E6_SEQUENCER_HZ 50                                // interrupts per second the DMA position model counts
#define E6_READ_POS_MASK ((E6_RING_SIZE << 8) - 1)        // the position model is 24.8 fixed point

// The module

uint8_t e6_module_u8(const Ems6Module* m, uint32_t offset);
uint16_t e6_module_u16(const Ems6Module* m, uint32_t offset);
uint32_t e6_module_u32(const Ems6Module* m, uint32_t offset);

Ems6Subsong e6_module_subsong(const Ems6Module* m, uint32_t index);

typedef enum E6SampleKind {
    E6SampleKind_Raw,       // 8-bit data that plays once
    E6SampleKind_Looped,    // an 8SVX with a repeat part
    E6SampleKind_Wavetable, // "SYN" + size: waves of that size, the loop is one of them
} E6SampleKind;

// What an instrument holds. Offsets are module offsets, lengths bytes.
typedef struct E6Sample {
    E6SampleKind kind;
    uint32_t data;
    uint32_t length;      // from `data` to the end of the sound
    uint32_t loop_start;  // from `data`
    uint32_t loop_length; // for a wavetable the size of one wave
} E6Sample;

E6Sample e6_module_sample(const Ems6Module* m, uint32_t instrument);

// The first wave of the instrument read as a wavetable, whatever it is.
uint32_t e6_module_wave_data(const Ems6Module* m, uint32_t instrument);

// Entry `pos` of arpeggio list `table`: a note offset, 0xFF (jump to the index in the next byte) or 0xFE (end).
uint8_t e6_module_arpeggio_entry(const Ems6Module* m, uint32_t table, uint32_t pos);

// The module offset of pattern `number`.
uint32_t e6_module_pattern(const Ems6Module* m, uint32_t number);

// A pattern byte and the four bytes after it, which the commands take as arguments.
typedef struct PatternCommand {
    uint8_t code;
    uint8_t args[4];
} PatternCommand;

PatternCommand e6_module_pattern_command(const Ems6Module* m, uint32_t offset);

// The period of `note` in finetune table `finetune` (0 .. 15). Notes run on into the next table, and -1 of table 0
// is one note below the first.
uint16_t e6_period_table(int finetune, int note);

// The mixer: 32 voices summed into two 16-bit buffers (even tracks left, odd right), each split into a high and a low
// byte stream in the four Paula ring buffers.

bool e6_mixer_init(Ems6Mixer* mx, int channels);
void e6_mixer_free(Ems6Mixer* mx);
void e6_mixer_reset(Ems6Mixer* mx);

// Mix `count` samples of all voices and append them to the rings.
void e6_mixer_render_block(Ems6Mixer* mx, const Ems6Module* m, uint32_t count);

// The samples the rings can take at this interrupt, 0 for too few.
uint16_t e6_mixer_room(const Ems6Mixer* mx);

// The tempo: samples per sequencer tick for `bpm`.
uint16_t e6_mixer_ticks_for_bpm(uint32_t bpm);

// The sequencer

void e6_sequencer_init(Ems6Sequencer* s, const Ems6Subsong* subsong);
void e6_sequencer_tick(Ems6Sequencer* s, Ems6Mixer* mx, const Ems6Module* m);

// Every track has jumped back in its list or ended.
bool e6_sequencer_finished(const Ems6Sequencer* s);
// Tracks up to the last one whose list does not end at once (at least 1): the ones a song uses.
uint32_t e6_sequencer_used_tracks(const Ems6Module* m, const Ems6Subsong* subsong);
