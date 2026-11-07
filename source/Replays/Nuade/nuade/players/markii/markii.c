// SPDX-License-Identifier: MIT
// Mark II player: the replayer every module carries, one function per routine. A step is two ticks. The first one
// stops the DMA of the voices whose last step asked for a restart and, in the later versions, slides the periods.
// The second one moves to the next step (the next sequence position of every voice, the next row after the last
// step), sets up the four voices from it and starts all DMA.

#include <nuade/players/markii.h>

#include "mk2_internal.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>

#define MK2_DRY_RUN_TICKS 2000000u
#define MK2_DRY_RUN_LOOPS 2u
#define MK2_ROW_BYTES 8u // two bytes (sequence, flags) per column
#define MK2_STEP_BYTES 4u
#define MK2_VOLUME_RAMP_MAX 0x40
#define MK2_HOLD_NOTE 0x32 // a step with this note keeps the voice's period
#define MK2_NOTE_BIT7 0x80 // slides clear only bit 7 of the stored note
#define MK2_SLIDE_SUBTRACT 0x20
#define MK2_SLIDE_RESERVED 0x40

// The period table word at `byte_offset` from its start. The code indexes it with a 16-bit signed value, so notes and
// slides can read before and past the table.

static uint16_t read_period(Mk2Player* player, int16_t byte_offset) {
    uint16_t period = 0;
    if (!mk2_period(&player->module, byte_offset, &period)) {
        player->fault = true;
    }
    return period;
}

// AUDxLC and AUDxLEN of a sample. One that starts outside the module leaves the model; one that runs past the end of
// the module (an empty instrument plays 65536 words) plays the zeros that follow it in chip memory.

static void set_sample(Mk2Player* player, NuadePaulaChannel* paula, uint32_t sample) {
    Mk2Instrument instrument;
    if (!mk2_instrument(&player->module, sample, &instrument)) {
        player->fault = true;
        return;
    }
    const uint32_t start = (player->song.sample_start + instrument.offset) & ~1u; // AUDxLC bit 0 does not exist
    paula->len_words = instrument.length_words;
    if (start >= player->module.size) {
        player->fault = true;
        paula->lc = nullptr;
        return;
    }
    paula->lc = (const int8_t*)player->chip + start;
}

// The period of a step: the note plus the row's transpose through the period table. A note with bit 7 moves the voice's
// last period by its other bits instead (up when the step's parameter byte has bit 7). The note 0x32 of the later
// versions keeps the last period.

static uint16_t step_period(Mk2Player* player, Mk2Track* track, const Mk2RowCell* cell, const Mk2Step* step) {
    uint16_t slide = 0;
    if (!(player->module.layout->holds_note && step->note == MK2_HOLD_NOTE)) {
        const uint8_t note
            = (uint8_t)(cell->transpose_down ? step->note - cell->transpose : step->note + cell->transpose);
        track->note = note;
        const uint16_t period = read_period(player, (int16_t)(note * 2));
        if (!step->note_slide) {
            return period;
        }
        slide = step->slide_amount;
    }
    return (uint16_t)(step->slide_up ? track->last_period + slide : track->last_period - slide);
}

// The volume of a step: its volume (at least 1) less a ramp that the row's flags select. Bit 5 counts the falling
// counter down (the rising one follows it); otherwise the falling counter is set to its start and bit 6 counts the
// rising counter up to MK2_VOLUME_RAMP_MAX (the falling one follows it) while no flag clears the rising one. The volume
// never goes below 1.

static uint16_t step_volume(Mk2Track* track, const Mk2RowCell* cell, const Mk2Step* step) {
    const int volume = step->volume > 0 ? step->volume : 1;
    int16_t ramp = 0; // the counters are signed words that wrap
    if (cell->volume_fall) {
        if (track->envelope_fall == 0) {
            return (uint16_t)volume;
        }
        ramp = (int16_t)(track->envelope_fall - 1);
        track->envelope_fall = ramp;
        track->envelope_rise = ramp;
    } else {
        track->envelope_fall = MK2_VOLUME_RAMP_MAX;
        if (!cell->volume_rise) {
            track->envelope_rise = 0;
            return (uint16_t)volume;
        }
        ramp = (int16_t)(track->envelope_rise + 1);
        if (ramp > MK2_VOLUME_RAMP_MAX) {
            ramp = MK2_VOLUME_RAMP_MAX;
        }
        track->envelope_rise = ramp;
        track->envelope_fall = ramp;
    }
    return (uint16_t)(volume > ramp ? volume - ramp : 1);
}

// One column of the row: its sequence's step sets the sample, period and volume of the column's voice. A step with
// bit 0 of the parameter clear stops the DMA of its voice in the next tick, which restarts the sample.

static void play_column(Mk2Player* player, uint32_t column) {
    Mk2Track* track = &player->tracks[column];
    Mk2RowCell cell;
    Mk2Step step;
    const uint32_t sequence_offset = player->row_start + 2u * (MK2_VOICES - 1u - column);
    if (!mk2_row_cell(&player->module, sequence_offset, &cell)) {
        player->fault = true;
        return;
    }
    // mulu.w, then the low word of the product, sign-extended, is the index
    const int16_t step_index = (int16_t)(uint16_t)(cell.sequence * player->song.block_size);
    if (!mk2_step(&player->module, player->step_offset + (uint32_t)(int32_t)step_index, &step)) {
        player->fault = true;
        return;
    }
    if (column > 0 && step.swap) {
        Mk2Track* below = &player->tracks[column - 1];
        const uint8_t voice = track->voice;
        const uint8_t dma_voice = track->dma_voice;
        track->voice = below->voice;
        track->dma_voice = below->dma_voice;
        below->voice = voice;
        below->dma_voice = dma_voice;
    }
    Mk2Voice* voice = &player->voices[track->voice];
    track->slide = step.slide;
    set_sample(player, &voice->paula, step.sample);
    voice->instrument = (int)step.sample;
    track->last_period = step_period(player, track, &cell, &step);
    voice->paula.period = track->last_period;
    track->param = step.param;
    voice->paula.volume = (uint8_t)step_volume(track, &cell, &step);
    player->voices[track->dma_voice].restart = !step.keep_dma;
}

// The four columns of the row, then DMA on for every voice (the voices that run on are not restarted).

static void play_row_step(Mk2Player* player) {
    for (uint32_t v = 0; v < MK2_VOICES; v++) {
        player->voices[v].restart = false;
    }
    for (uint32_t column = MK2_VOICES; column-- > 0;) {
        play_column(player, column);
    }
    for (uint32_t v = 0; v < MK2_VOICES; v++) {
        nuade_paula_channel_dma_on(&player->voices[v].paula);
    }
}

// The first tick of a step in the versions with slides: every column with the slide bit in its sample byte writes the
// period of its note moved by half its parameter byte (bit 5 of that: down). The column's channel is taken in order,
// whatever the steps swapped.

static void slide_periods(Mk2Player* player) {
    for (uint32_t column = MK2_VOICES; column-- > 0;) {
        const Mk2Track* track = &player->tracks[column];
        if (!track->slide) {
            continue;
        }
        const uint16_t note = track->note & (uint16_t)~MK2_NOTE_BIT7;
        uint16_t amount = (uint16_t)((track->param >> 1) & ~MK2_SLIDE_RESERVED);
        const bool down = (amount & MK2_SLIDE_SUBTRACT) != 0;
        amount &= (uint16_t)~MK2_SLIDE_SUBTRACT;
        const uint16_t moved = (uint16_t)(down ? note - amount : note + amount);
        const uint16_t period = read_period(player, (int16_t)(moved * 2));
        player->voices[MK2_VOICES - 1u - column].paula.period = period;
    }
}

// The first tick of a step.

static void dma_tick(Mk2Player* player) {
    for (uint32_t v = 0; v < MK2_VOICES; v++) {
        if (player->voices[v].restart) {
            nuade_paula_channel_dma_off(&player->voices[v].paula);
        }
    }
    if (player->module.layout->slides) {
        slide_periods(player);
    }
}

// Moves on one step of the sequences; after the last one, to the next row and the first step; after the last row, to
// the start of the song. Returns true at that wrap.

static bool next_step(Mk2Player* player) {
    player->step_offset += MK2_STEP_BYTES;
    player->step_bytes = (uint16_t)(player->step_bytes + MK2_STEP_BYTES);
    if (player->step_bytes != player->song.block_size) {
        return false;
    }
    player->step_bytes = 0;
    player->step_offset = player->song.sequence_start;
    player->row_start += MK2_ROW_BYTES;
    player->row = (uint16_t)(player->row + 1);
    if (player->row != player->song.rows) {
        return false;
    }
    player->row = 0;
    player->row_start = player->song.data_start;
    return true;
}

// Returns true when the tick moved the song past its last row.

static bool run_tick(Mk2Player* player) {
    if (!player->step_next) {
        player->step_next = true;
        dma_tick(player);
        return false;
    }
    player->step_next = false;
    const bool wrapped = next_step(player);
    play_row_step(player);
    return wrapped;
}

// The init of the module: DMA off, the voices in their channels, then the first step of the first row.

static void reset_state(Mk2Player* player) {
    const Mk2Initial* initial = &player->initial;
    player->row = 0;
    player->step_bytes = 0;
    player->step_next = initial->toggle;
    player->row_start = player->song.data_start;
    player->step_offset = player->song.sequence_start;
    player->fault = false;
    for (uint32_t i = 0; i < MK2_VOICES; i++) {
        player->tracks[i] = (Mk2Track) {
            .voice = (uint8_t)(MK2_VOICES - 1u - i),
            .dma_voice = (uint8_t)(MK2_VOICES - 1u - i),
            .slide = initial->slide[i],
            .param = initial->param[i],
            .note = initial->note[i],
            .last_period = initial->last_period[i],
            .envelope_fall = (int16_t)initial->envelope_fall[i],
            .envelope_rise = (int16_t)initial->envelope_rise[i],
        };
        player->voices[i] = (Mk2Voice) { .instrument = -1 };
    }
}

bool mk2_start(Mk2Player* player, NuadeVoices* voices) {
    reset_state(player);
    if (voices != nullptr) {
        for (uint32_t v = 0; v < MK2_VOICES; v++) {
            nuade_voice_stop(voices, v);
        }
    }
    play_row_step(player);
    if (voices != nullptr) {
        for (uint32_t v = 0; v < MK2_VOICES; v++) {
            nuade_paula_channel_sync(&player->voices[v].paula, v, voices);
        }
    }
    return true;
}

Mk2TickResult mk2_tick(Mk2Player* player, NuadeVoices* voices) {
    const bool wrapped = run_tick(player);
    for (uint32_t v = 0; v < MK2_VOICES; v++) {
        nuade_paula_channel_sync(&player->voices[v].paula, v, voices);
    }
    return (Mk2TickResult) { .looped = wrapped && player->module.layout->signals_song_end };
}

// Plays the song twice round without a mixer: everything it reaches must stay inside what the port models.

static bool dry_run(Mk2Player* player) {
    if (!mk2_start(player, nullptr) || player->fault) {
        return false;
    }
    uint32_t wraps = 0;
    for (uint32_t tick = 0; tick < MK2_DRY_RUN_TICKS && wraps < MK2_DRY_RUN_LOOPS; tick++) {
        const bool wrapped = run_tick(player);
        if (player->fault) {
            return false;
        }
        wraps += wrapped ? 1u : 0u;
    }
    return true;
}

Mk2Player* mk2_create(const uint8_t* data, size_t size) {
    Mk2Player* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    player->chip = nuade_paula_padded_copy(data, size);
    if (player->chip == nullptr || !mk2_module_parse(&player->module, player->chip, size)
        || !mk2_read_song(&player->module, &player->song) || !mk2_read_initial(&player->module, &player->initial)
        || !dry_run(player)) {
        mk2_destroy(player);
        return nullptr;
    }
    return player;
}

void mk2_destroy(Mk2Player* player) {
    if (player != nullptr) {
        free(player->chip);
        free(player);
    }
}

uint32_t mk2_grid_ticks(const Mk2Player* player) {
    (void)player;
    return 2;
}

Mk2VoiceState mk2_voice_state(const Mk2Player* player, uint32_t voice) {
    if (voice >= MK2_VOICES) {
        return (Mk2VoiceState) { .period = 0, .instrument = -1 };
    }
    const Mk2Voice* v = &player->voices[voice];
    return (Mk2VoiceState) { .period = v->paula.period, .instrument = v->instrument };
}
