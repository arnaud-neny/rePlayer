// SPDX-License-Identifier: MIT
// Scott Johnston player: types shared by the module, the tune walk and the replayer.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nuade_base.h"
#include <nuade/io.h>
#include <nuade/players/scottjohnston.h>

// Module layout of the "ScottJohnston" eagleplayer: a header of 16 bytes naming the instrument files, from
// SJ_FILE_INDEX_OFFSET; the tune from its tune offset: the CIA timer (low byte, high byte), the voice mask, then rows
// of SJ_ROW_BYTES, 3 bytes per voice. The "SoundPlayer" eagleplayer's module is the tune alone (offset 0).
#define SJ_FILE_INDEX_OFFSET 0x1C
#define SJ_TUNE_HEADER_BYTES 3
#define SJ_ROW_BYTES 12
#define SJ_CELL_BYTES 3
#define SJ_MAX_MODULE_BYTES 0x100000u

#define SJ_PERIOD_COUNT 39
#define SJ_COMMAND_TABLE_BYTES 256
#define SJ_MAX_NOTE (SJ_PERIOD_COUNT + SJ_COMMAND_TABLE_BYTES / 2)

// The interrupt counts the ticks of a row down from TICKS_PER_ROW - 1; the row work comes when it reaches 0.
#define SJ_TICKS_PER_ROW 6

#define SJ_ALL_VOICES_MASK 0x0F
#define SJ_ROW_UNSET INT32_MIN
#define SJ_NO_LOOP 0xFF

// The tune

const SjVariant* sj_variant(SjFormat format);

// The format of a song by its file name (the eagleplayer's prefix); false for any other name.
bool sj_format_from_path(const char* path, SjFormat* format);

// The header and the tune's own header; false for anything the port does not model.
bool sj_module_parse(SjModule* module, SjFormat format, const uint8_t* data, size_t size);

// Does voice `voice` walk a track of the tune?
bool sj_module_voice_is_armed(const SjModule* module, uint32_t voice);

// The cell of `voice` in row `row` (-1 is the row before the first); false if it lies outside the module.
bool sj_module_cell(const SjModule* module, int32_t row, uint32_t voice, SjCell* cell);

// The period a note plays at; notes past the period table read the command table as words.
uint16_t sj_period_for_note(uint8_t note);

// The name of the instrument file with the 1-based index `file_index` in the eagleplayer's table; null if none.
const char* sj_instrument_name(uint32_t file_index);

// Does the replayer read only the tables the port has for this cell?
bool sj_cell_is_modelled(const SjCell* cell);

// Commands, and the part of them that moves the track

typedef enum SjCommandKind {
    SjCommand_None,
    SjCommand_SetVolume,
    SjCommand_StopChannel,
    SjCommand_Wait,
    SjCommand_FadeUp,
    SjCommand_FadeDown,
    SjCommand_HoldOn,
    SjCommand_HoldOff,
    SjCommand_LoopBegin,
    SjCommand_LoopEnd,
    SjCommand_RepeatRow,
    SjCommand_Restart,
} SjCommandKind;

typedef struct SjCommand {
    SjCommandKind kind;
    uint8_t param; // the command byte less the base of its range
} SjCommand;

SjCommand sj_decode_command(uint8_t byte);

void sj_track_init(SjTrack* track);

// One row of the walk: advance to the next row unless the track waits, then read the voice's cell. `read` tells whether
// a cell was read (a waiting track keeps the voice's last one). False if the track points outside the module.
bool sj_track_step(SjTrack* track, const SjModule* module, uint32_t voice, SjCell* cell, bool* read);

// The effect of a command on the track.
void sj_track_apply(SjTrack* track, SjCommand command);

// Walks every track of the tune until its state repeats; false if one leaves the module, uses a note or instrument the
// port does not model, or does not repeat within the module's step budget.
bool sj_tune_is_closed(const SjModule* module);

// Instruments

typedef enum SjLoadStatus {
    SjLoad_Ok,
    SjLoad_Missing,
    SjLoad_Invalid,
    SjLoad_OutOfMemory,
} SjLoadStatus;

// The instruments of a song, as its eagleplayer loads them: the ScottJohnston header's files from "instruments" next to
// the song, or the SoundPlayer's sample bundle "smp.<name>" next to the song.
SjLoadStatus sj_samples_load(SjSamples* samples, const SjModule* module, const char* path, const NuadeIo* io);
void sj_samples_free(SjSamples* samples);
