// SPDX-License-Identifier: MIT
// Mark II: the module (code, variables, tables) and the typed readers of its records. A module is the replayer's
// code followed by its variables and the song; the code addresses everything pc-relative, so every offset here is an
// offset into the module and the replayer version fixes where the variables lie.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/players/markii.h>

// A voice's cell of a row.
typedef struct Mk2RowCell {
    uint8_t sequence;
    uint8_t transpose;   // added to the notes of the sequence's steps (a byte sum)
    bool transpose_down; // subtracted instead
    bool volume_rise;
    bool volume_fall;
} Mk2RowCell;

// One step of a sequence.
typedef struct Mk2Step {
    uint8_t sample;
    bool slide;      // versions with slides: the first tick of the step slides the period
    uint8_t note;    // the whole byte: the slide flag below is part of the sum with the transpose
    bool note_slide; // bit 7 of the note: the period moves by `slide_amount` from the last one
    uint8_t slide_amount;
    uint8_t volume;
    bool swap; // swap the voice with the one below
    uint8_t param;
    bool slide_up; // bit 7 of the parameter: the period moves up
    bool keep_dma; // bit 0 of the parameter: the DMA is not restarted
} Mk2Step;

typedef struct Mk2Instrument {
    uint32_t offset; // from the sample start
    uint16_t length_words;
} Mk2Instrument;

// Check2 of the eagleplayer and the replayer version. False when the module is refused or the code is unknown.
bool mk2_module_parse(Mk2Module* module, const uint8_t* data, size_t size);

bool mk2_read_initial(const Mk2Module* module, Mk2Initial* out);
bool mk2_read_song(const Mk2Module* module, Mk2Song* out);

bool mk2_row_cell(const Mk2Module* module, uint32_t offset, Mk2RowCell* out);
bool mk2_step(const Mk2Module* module, uint32_t offset, Mk2Step* out);
bool mk2_instrument(const Mk2Module* module, uint32_t sample, Mk2Instrument* out);

// The word of the period table `byte_offset` bytes from its start (negative: the bytes before it).
bool mk2_period(const Mk2Module* module, int16_t byte_offset, uint16_t* out);
