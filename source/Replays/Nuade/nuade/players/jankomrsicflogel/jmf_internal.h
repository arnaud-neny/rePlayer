// SPDX-License-Identifier: MIT
// Janko Mrsic-Flogel: the module (the executable the replayer lives in), the known replayer versions and the typed
// readers of the module records.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/jankomrsicflogel.h>

// The voices that play notes; the fourth one is set up and never started.
#define JMF_NOTE_VOICES 3
#define JMF_IDLE_VOICE 3

// What a mode byte (the byte after 0xFE in a note stream) makes the following events do. A mode above 8 jumps out of
// the table into other code: not modelled.
#define JMF_MODE_COUNT 9u

// Bytes that are not durations.
#define JMF_STREAM_END 0xFFu
#define JMF_STREAM_MODE 0xFEu

// Parses the executable, finds the replayer version and checks every note stream of every subsong. `bytes` stays
// owned by the caller and must outlive the module.
bool jmf_module_parse(JmfModule* module, const uint8_t* bytes, size_t size);
void jmf_module_free(JmfModule* module);

// A byte of the song data (before the variables); false past it.
bool jmf_byte(const JmfModule* module, uint32_t at, uint8_t* out);

// The start of the note stream of a voice.
uint32_t jmf_stream_start(const JmfModule* module, uint32_t subsong, uint32_t voice);

// The action of a mode byte; false for a mode the table does not have.
bool jmf_mode_action(uint8_t mode, JmfAction* out);

// The period of a note byte; false for a note outside the period table.
bool jmf_period(const JmfModule* module, uint8_t note, uint16_t* out);
