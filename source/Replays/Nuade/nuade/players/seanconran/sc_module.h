// SPDX-License-Identifier: MIT
// A Sean Conran module as the eagleplayer sees it: the replayer's code, the tables behind it, and the sequence data.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/seanconran.h>

#define SCR_COMMAND_STEP 4
#define SCR_COMMAND_SLOT_NEXT 13 // the slot of the next stream of the track list

// The commands of the sequence data. Their values are the letters of the build tables, one letter for each slot of
// a voice's command table.
typedef enum ScrCommand {
    ScrCommand_Note = 'N',           // length, pitch bias; the pitch and volume sequences go on
    ScrCommand_Rest = 'T',           // length; the pitch and volume sequences go on
    ScrCommand_VolumeBase = 'V',     // the base the volume sequence is added to
    ScrCommand_PitchShift = 'S',     // the shift that scales the period of the pitch table
    ScrCommand_VolumeRestart = 'P',  // where the volume sequence starts
    ScrCommand_PitchRestart = 'Q',   // where the pitch sequence starts
    ScrCommand_Return = 'R',         // back to where the last call came from, once
    ScrCommand_Call = 'C',           // continue elsewhere, remembering where to return to
    ScrCommand_SampleLoop = 'L',     // sample, with the repeat set by the audio interrupt
    ScrCommand_Sample = 'A',         // sample start (AUDxLC); the length stays
    ScrCommand_SampleOpen = 'B',     // sample start, length 0
    ScrCommand_MasterVolume = 'M',   // master volume plus a signed byte, clamped
    ScrCommand_Mute = 'X',           // length; the pitch and volume sequences stop
    ScrCommand_Retrigger = 'E',      // restart the sequences and the DMA, then length
    ScrCommand_NoteOn = 'O',         // the DMA on and the sequences restarted, then a note
    ScrCommand_NextLoop = 'J',       // the next stream of the track list; a marker byte loops the list
    ScrCommand_NextStop = 'K',       // the same, a byte behind the marker with the sign bit set stops
    ScrCommand_NextStopFF = 'H',     // the same, a byte 0xFF behind the marker stops
    ScrCommand_NextEnd = 'G',        // the same, a marker stops
    ScrCommand_NextPlain = 'F',      // the next stream of the track list; no marker
    ScrCommand_NextStopOrHang = 'D', // a byte 0xFF behind the marker stops, any other makes the replayer loop for ever
    ScrCommand_NextHang = 'Z',       // a marker makes the replayer loop for ever
    ScrCommand_Unmodelled = '-',     // the slot jumps into the middle of another routine
} ScrCommand;

#define SCR_OUTSIDE_MODULE 0xFFFFFFFFu

// null when the eagleplayer would not play the data or the port does not know its replayer or sequence data.
ScrModule* scr_module_open(const uint8_t* data, size_t size);
void scr_module_close(ScrModule* module);

uint8_t scr_module_u8(const ScrModule* module, uint32_t offset);
uint16_t scr_module_u16(const ScrModule* module, uint32_t offset);
