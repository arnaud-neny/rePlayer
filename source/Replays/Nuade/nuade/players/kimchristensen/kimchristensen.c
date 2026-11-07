// SPDX-License-Identifier: MIT
// Kim Christensen player: the replayer of the eagleplayer, one function per routine. A tick steps the sequencer every
// `speed + 1` ticks (a new row: notes set up the voices and switch their DMA off), runs the volume, pitch and vibrato
// stages of the four voices and writes their volume and period; then DMA goes on for the voices of the row and, a
// raster wait later, each of them gets its repeat: the sample's loop, or four zero bytes.

#include <nuade/players/kimchristensen.h>

#include "kim_internal.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>

#define KIM_PITCH_SHIFT 4          // the period is kept times 16
#define KIM_VOLUME_SHIFT 8         // the volume is kept times 256
#define KIM_MAX_SKIPPED_BYTES 256u // 0xFF bytes in a row that the order list lookup follows
#define KIM_SILENCE_WORDS 2u
#define KIM_MASTER_VOLUME 64u // the eagleplayer's volume scaling at full volume
#define KIM_DRY_RUN_TICKS 1000000u

// 68000 DIVS.W: a quotient that does not fit in 16 bits leaves the dividend, and the word that the replayer keeps is
// then the dividend's low word. A divisor of 0 is tested for before the division and gives 0.

static int16_t divide_word(int32_t dividend, uint8_t divisor) {
    if (divisor == 0) {
        return 0;
    }
    const int32_t quotient = dividend / divisor;
    if (quotient < INT16_MIN || quotient > INT16_MAX) {
        return (int16_t)dividend;
    }
    return (int16_t)quotient;
}

// The tick's first half for a voice that got a note: the registers the note writes, AUDxVOL 0 included.

static void write_note_registers(KimPlayer* player, KimVoice* voice) {
    if (!kim_range_valid(&player->module, voice->start, voice->length_words)) {
        player->fault = true;
        return;
    }
    voice->paula.lc = (const int8_t*)player->chip + voice->start;
    voice->paula.len_words = voice->length_words;
    voice->paula.volume = 0;
    voice->paula.period = (uint16_t)(voice->period >> KIM_PITCH_SHIFT);
}

// The volume ramps of an instrument: each goes from the previous target to its own in `ticks` ticks.

static void set_volume_ramps(KimVoice* voice, const KimInstrument* instrument) {
    int previous = 0;
    for (uint32_t i = 0; i < KIM_VOLUME_STAGES; i++) {
        const KimVolumeStage* stage = &instrument->volume[i];
        voice->volume_ramps[i] = (KimVolumeRamp) {
            .ticks = stage->ticks,
            .delta = divide_word((int32_t)(stage->target - previous) * (1 << KIM_VOLUME_SHIFT), stage->ticks),
        };
        previous = stage->target;
    }
}

// The pitch ramps: each goes from the period of the note before it to the period of the note it names. The note is a
// byte, so it wraps. A ramp of 0 ticks has no slide whatever the notes are; a ramp that slides needs both periods from
// the table, and a note the table does not hold leaves the model.

static void set_pitch_ramps(KimPlayer* player, KimVoice* voice, const KimInstrument* instrument, uint8_t note) {
    for (uint32_t i = 0; i < KIM_PITCH_STAGES; i++) {
        const KimPitchStage* stage = &instrument->pitch[i];
        const uint8_t previous = note;
        note = (uint8_t)(note + (uint8_t)stage->note_offset);
        int16_t delta = 0;
        if (stage->ramp != 0) {
            uint16_t from = 0;
            uint16_t to = 0;
            if (!kim_period(previous, &from) || !kim_period(note, &to)) {
                player->fault = true;
                return;
            }
            delta = divide_word((int32_t)(to - from) * (1 << KIM_PITCH_SHIFT), stage->ramp);
        }
        voice->pitch_ramps[i] = (KimPitchRamp) { .hold = stage->hold, .ticks = stage->ramp, .delta = delta };
    }
}

// A note on a voice: the voice's sample, volume, pitch and the three effects of the instrument. The registers follow
// in write_note_registers.

static void start_note(KimPlayer* player, KimVoice* voice, const KimCell* cell) {
    KimInstrument instrument;
    uint16_t period = 0;
    uint32_t sample = 0;
    if (!kim_period(cell->note, &period) || !kim_instrument(&player->module, cell->instrument, &instrument)
        || !kim_sample_offset(&player->module, instrument.sample, &sample)) {
        player->fault = true;
        return;
    }
    voice->period = (int16_t)(period << KIM_PITCH_SHIFT);
    voice->loops = instrument.loops;
    voice->start = (sample + instrument.start) & ~1u; // AUDxLC bit 0 does not exist
    voice->loop = (sample + instrument.loop_start) & ~1u;
    voice->length_words = instrument.length_words;
    voice->loop_words = instrument.loop_words;
    voice->volume = 0;
    voice->instrument = cell->instrument;
    voice->started = true;

    set_volume_ramps(voice, &instrument);
    set_pitch_ramps(player, voice, &instrument, cell->note);
    voice->vibrato = (KimVibrato) {
        .delay = instrument.vibrato_delay,
        .ticks_left = (uint8_t)(instrument.vibrato_rate >> 1),
        .ticks_per_half = instrument.vibrato_rate,
        .delta = instrument.vibrato_step,
    };
}

// The pattern for the current position: the order list byte, skipping 0xFF bytes (each moves the position on). More
// than KIM_MAX_SKIPPED_BYTES of them in a row are not a song.

static bool current_pattern(KimPlayer* player, uint8_t* pattern) {
    for (uint32_t skipped = 0; skipped <= KIM_MAX_SKIPPED_BYTES; skipped++) {
        uint8_t number = 0;
        if (!kim_order_byte(&player->module, (uint32_t)player->position + skipped, &number)) {
            return false;
        }
        if (number != 0xFF) {
            player->position = (uint8_t)(player->position + skipped);
            *pattern = number;
            return true;
        }
    }
    return false;
}

// The row's step: the voices with a note start it. Returns true when the order list has run out.

static bool step_row(KimPlayer* player) {
    uint8_t pattern = 0;
    if (!current_pattern(player, &pattern)) {
        player->fault = true;
        return false;
    }
    player->row_countdown
        = player->module.speed < 0x80 ? player->module.speed : player->module.speed - 256; // a signed byte
    const uint32_t row = player->row;
    player->row = (uint8_t)(player->row + 1);
    bool looped = false;
    if (player->row == KIM_ROWS) {
        player->position = (uint8_t)(player->position + 1);
        player->row = 0;
        if (player->position > player->module.last_position) {
            player->position = player->module.first_position;
            looped = true;
        }
    }
    for (uint32_t v = 0; v < KIM_VOICES; v++) {
        KimVoice* voice = &player->voices[v];
        voice->started = false;
        KimCell cell;
        if (!kim_pattern_cell(&player->module, pattern, v, row, &cell)) {
            player->fault = true;
            return looped;
        }
        if (cell.note != 0) {
            start_note(player, voice, &cell);
            write_note_registers(player, voice);
        }
    }
    for (uint32_t v = 0; v < KIM_VOICES; v++) {
        if (player->voices[v].started) {
            nuade_paula_channel_dma_off(&player->voices[v].paula);
        }
    }
    return looped;
}

// The volume of a voice: the first ramp that has ticks left adds its delta.

static void volume_effect(KimVoice* voice) {
    for (uint32_t i = 0; i < KIM_VOLUME_STAGES; i++) {
        KimVolumeRamp* ramp = &voice->volume_ramps[i];
        if (ramp->ticks != 0) {
            ramp->ticks--;
            voice->volume = (int16_t)(voice->volume + ramp->delta);
            return;
        }
    }
}

// The pitch of a voice: each stage holds, then slides; the stages follow one another.

static void pitch_effect(KimVoice* voice) {
    for (uint32_t i = 0; i < KIM_PITCH_STAGES; i++) {
        KimPitchRamp* ramp = &voice->pitch_ramps[i];
        if (ramp->hold != 0) {
            ramp->hold--;
            return;
        }
        if (ramp->ticks != 0) {
            ramp->ticks--;
            voice->period = (int16_t)(voice->period + ramp->delta);
            return;
        }
    }
}

// The vibrato: after its delay the period moves by `delta` every tick, the direction changing after each half cycle.

static void vibrato_effect(KimVoice* voice) {
    KimVibrato* vibrato = &voice->vibrato;
    if (vibrato->delay != 0) {
        vibrato->delay--;
    } else if (vibrato->ticks_left != 0) {
        vibrato->ticks_left--;
        voice->period = (int16_t)(voice->period + vibrato->delta);
    } else {
        vibrato->ticks_left = vibrato->ticks_per_half;
        vibrato->delta = (int16_t)-vibrato->delta;
    }
}

// The registers a voice gets every tick: AUDxVOL is the volume's high byte through the eagleplayer's volume scaling
// (times 64, low word, divided by 64), AUDxPER the period.

static void write_effect_registers(KimVoice* voice) {
    const uint16_t scaled = (uint16_t)((uint16_t)(voice->volume >> KIM_VOLUME_SHIFT) * KIM_MASTER_VOLUME);
    voice->paula.volume = (uint8_t)(scaled >> 6);
    voice->paula.period = (uint16_t)(voice->period >> KIM_PITCH_SHIFT);
}

// DMA on for the voices of the row, then their repeat: the loop of a sample that has one, four zero bytes otherwise.

static void start_dma(KimPlayer* player) {
    for (uint32_t v = 0; v < KIM_VOICES; v++) {
        if (player->voices[v].started) {
            nuade_paula_channel_dma_on(&player->voices[v].paula);
        }
    }
    for (uint32_t v = 0; v < KIM_VOICES; v++) {
        KimVoice* voice = &player->voices[v];
        if (!voice->started) {
            continue;
        }
        if (voice->loops) {
            if (!kim_range_valid(&player->module, voice->loop, voice->loop_words)) {
                player->fault = true;
                continue;
            }
            voice->paula.lc = (const int8_t*)player->chip + voice->loop;
            voice->paula.len_words = voice->loop_words;
        } else {
            voice->paula.lc = (const int8_t*)player->chip + (player->module.silence & ~1u);
            voice->paula.len_words = KIM_SILENCE_WORDS;
        }
    }
}

static KimTickResult run_tick(KimPlayer* player) {
    KimTickResult result = { 0 };
    player->row_countdown--;
    if (player->row_countdown < INT8_MIN) {
        player->row_countdown += 256; // the byte wraps round
    }
    if (player->row_countdown < 0) {
        result.looped = step_row(player);
    }
    for (uint32_t v = 0; v < KIM_VOICES; v++) {
        volume_effect(&player->voices[v]);
        pitch_effect(&player->voices[v]);
        vibrato_effect(&player->voices[v]);
    }
    for (uint32_t v = 0; v < KIM_VOICES; v++) {
        write_effect_registers(&player->voices[v]);
    }
    start_dma(player);
    return result;
}

bool kim_start(KimPlayer* player, NuadeVoices* voices) {
    if (player == nullptr) {
        return false;
    }
    player->position = player->module.first_position;
    player->row = 0;
    player->row_countdown = 0;
    player->fault = false;
    for (uint32_t v = 0; v < KIM_VOICES; v++) {
        player->voices[v] = (KimVoice) { .instrument = -1 };
        if (voices != nullptr) {
            nuade_voice_stop(voices, v);
        }
    }
    return true;
}

KimTickResult kim_tick(KimPlayer* player, NuadeVoices* voices) {
    const KimTickResult result = run_tick(player);
    for (uint32_t v = 0; v < KIM_VOICES; v++) {
        nuade_paula_channel_sync(&player->voices[v].paula, v, voices);
    }
    return result;
}

// Plays the song to its first end (or KIM_DRY_RUN_TICKS ticks) without a mixer: the data it reaches must stay inside
// what the port models.

static bool dry_run(KimPlayer* player) {
    if (!kim_start(player, nullptr)) {
        return false;
    }
    for (uint32_t tick = 0; tick < KIM_DRY_RUN_TICKS; tick++) {
        const KimTickResult result = run_tick(player);
        if (player->fault) {
            return false;
        }
        if (result.looped) {
            break;
        }
    }
    return true;
}

KimPlayer* kim_create(const uint8_t* data, size_t size) {
    KimPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    player->chip = nuade_paula_padded_copy(data, size);
    if (player->chip == nullptr || !kim_module_parse(&player->module, player->chip, size) || !dry_run(player)) {
        kim_destroy(player);
        return nullptr;
    }
    return player;
}

void kim_destroy(KimPlayer* player) {
    if (player != nullptr) {
        free(player->chip);
        free(player);
    }
}

// Ticks between two rows. The counter is reloaded with `speed` and counts down as a signed byte, so speeds from 0x81 on
// start a row every tick and 0x80 waits for the byte to wrap round.

uint32_t kim_grid_ticks(const KimPlayer* player) {
    const uint32_t speed = player->module.speed;
    if (speed < 0x80) {
        return speed + 1;
    }
    return speed == 0x80 ? 129 : 1;
}

KimVoiceState kim_voice_state(const KimPlayer* player, uint32_t voice) {
    if (voice >= KIM_VOICES) {
        return (KimVoiceState) { .period = 0, .instrument = -1 };
    }
    const KimVoice* v = &player->voices[voice];
    return (KimVoiceState) { .period = v->period >> KIM_PITCH_SHIFT, .instrument = v->instrument };
}
