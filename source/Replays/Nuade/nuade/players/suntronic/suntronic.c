// SPDX-License-Identifier: MIT
// SUN-Tronic replayer: a wavetable synthesizer. Every voice plays a short waveform (16 to 64 words) in a loop; each
// tick the replayer computes the next waveform from two stored ones, so the sound changes without a sample being
// replaced. The module carries the replayer (three generations, see sun_internal.h); this file translates its routines.
//
// One tick, generation A:  stage 1  DMA off for the voices that were triggered, period, volume and start address,
//                                   snapshot of the repeat registers
//                          fade     the master volume fades by the fade command
//                          rows     every `speed` ticks a row: the sequencer reads each voice's pattern; every tick the
//                                   voice's synth updates volume and period
//                          stage 2  DMA on for the triggered voices, repeat registers, back buffer -> front buffer
//                          morph    the next waveform is computed into the back buffer
// Generations B and C do the DMA work in one routine at the start of the tick that follows a row, and compute the morph
// in the voice's synth.

#include <nuade/players/suntronic.h>

#include "nuade_paula_channel.h"
#include "sun_internal.h"
#include <stdlib.h>
#include <string.h>

// The pitch of generation A wraps between 0 and this (12 octaves of 256 steps).
#define SUN_PITCH_LIMIT 0x4800

// Generation A, morph type 1: the morph positions with a special meaning.
#define SUN_MORPH_COPY 0xfe
#define SUN_MORPH_RANDOM 0xff

// Pitch

// The period for a pitch (with vibrato): the note picks an entry of the period table, the fraction interpolates.
static uint16_t lookup_period(const SunModule* module, const SunVoice* voice, uint16_t pitch) {
    const SunMemory* mem = &module->memory;
    const SunGen gen = module->build->gen;
    const uint32_t table = module->vars + module->build->period_table;
    const uint32_t adjust = module->vars + module->build->adjust_table;
    const uint32_t column_shift = gen == SunGen_A ? 4 : 3;
    const uint32_t row = ((uint32_t)voice->period_row << column_shift) + voice->period_column;
    const uint32_t note = (uint8_t)((pitch >> 8) - sun_mem_u8(mem, adjust + row));
    const uint16_t first = sun_mem_u16(mem, table + 2 * note);
    const uint32_t fraction = pitch & 0xff;
    if (fraction == 0) {
        return first;
    }
    const uint16_t second = sun_mem_u16(mem, table + 2 * note + 2);
    if (gen == SunGen_A) {
        return (uint16_t)(first + (uint16_t)(((uint32_t)(uint16_t)(second - first) * fraction) >> 8));
    }
    return (uint16_t)(((uint32_t)first * (0x100 - fraction) + (uint32_t)second * fraction) >> 8);
}

// A, B: the vibrato moves the pitch by (|phase| - 0x4000) * table value / 4096 (16 bit arithmetic).
static uint16_t vibrato_pitch(const SunModule* module, const SunVoice* voice) {
    int16_t phase = (int16_t)voice->vibrato_phase;
    if (phase < 0) {
        phase = (int16_t)-phase;
    }
    phase = (int16_t)(phase - 0x4000);
    const uint32_t amount = sun_mem_u8(&module->memory, voice->inst.vibrato + voice->vibrato_pos);
    const uint32_t product = (uint32_t)((int32_t)phase * (int32_t)amount);
    return (uint16_t)(voice->pitch + (uint16_t)((product >> 6) >> 6));
}

// A table position that counts up and returns to `loop` on reaching `end`.
static void advance_position(uint16_t* position, uint16_t end, uint16_t loop) {
    (*position)++;
    if (*position == end) {
        *position = loop;
    }
}

// C: a position that runs once up to `end`, then loops between `loop` and `stop`.
static void advance_position_c(uint16_t* position, bool* looping, uint16_t end, uint16_t loop, uint16_t stop) {
    (*position)++;
    if (!*looping) {
        if (*position == end) {
            *position = loop;
            *looping = true;
        }
    } else if (*position == stop) {
        *position = loop;
    }
}

// Waveform morph

static int16_t wave_sample(const SunMemory* mem, uint32_t address) {
    return (int8_t)sun_mem_u8(mem, address);
}

static void load_wave(const SunMemory* mem, uint32_t address, int8_t* out, uint32_t bytes) {
    for (uint32_t i = 0; i < bytes; i++) {
        out[i] = (int8_t)sun_mem_u8(mem, address + i);
    }
}

// B with a second waveform, C: each sample is the two waveforms weighted by (128 - weight) and weight, in 16 bit
// arithmetic.
static void morph_blend(const SunMemory* mem, const SunInstrument* inst, int32_t weight, int8_t* out) {
    const int32_t first_weight = 0x80 - weight;
    for (uint32_t i = 0; i < SUN_WAVE_BYTES_BC; i++) {
        const int16_t a = (int16_t)(wave_sample(mem, inst->wave_a + i) * first_weight);
        const int16_t b = (int16_t)(wave_sample(mem, inst->wave_b + i) * weight);
        out[i] = (int8_t)((int16_t)(a + b) >> 7);
    }
}

// B without a second waveform: a one pole filter over the first waveform (or over the last output), which starts from
// the source's last sample; `weight` is the share of the filter state.
static void morph_hold(const SunMemory* mem, const SunInstrument* inst, bool in_place, int32_t weight, int8_t* out) {
    int8_t source[SUN_WAVE_BYTES_BC];
    if (in_place) {
        memcpy(source, out, sizeof(source));
    } else {
        load_wave(mem, inst->wave_a, source, sizeof(source));
    }
    const int32_t input_weight = 0x80 - weight;
    int16_t state = source[SUN_WAVE_BYTES_BC - 1];
    for (uint32_t i = 0; i < SUN_WAVE_BYTES_BC; i++) {
        const int16_t held = (int16_t)(state * weight);
        const int16_t input = (int16_t)(source[i] * input_weight);
        state = (int16_t)((int16_t)(held + input) >> 7);
        out[i] = (int8_t)state;
    }
}

// A, type 1, position below 0xfe: each sample moves towards the source by (128 - position) / 128 of the distance.
static void morph_smooth(const SunMemory* mem, const SunInstrument* inst, bool in_place, uint32_t position, int8_t* out,
                         uint32_t bytes) {
    int8_t source[SUN_WAVE_BYTES];
    if (in_place) {
        memcpy(source, out, bytes);
    } else {
        load_wave(mem, inst->wave_a, source, bytes);
    }
    const int32_t weight = (int32_t)((0x80 - position) & 0xff);
    int16_t level = source[bytes - 1];
    for (uint32_t i = 0; i < bytes; i++) {
        const int16_t distance = (int16_t)(source[i] - level);
        level = (int16_t)(level + ((int16_t)(distance * weight) >> 7));
        out[i] = (int8_t)level;
    }
}

// A, type 1, position 0xff: random words from the two seeds.
static void morph_random(SunPlayer* player, int8_t* out, uint32_t bytes) {
    uint32_t state = player->seed[0];
    for (uint32_t i = 0; i < bytes / 2; i++) {
        state = ((uint32_t)(uint16_t)state * (uint16_t)state) >> 8;
        state = (state & 0xffff0000u) | ((state & 0xffffu) ^ 0xac91u);
        out[2 * i] = (int8_t)(state >> 8);
        out[2 * i + 1] = (int8_t)state;
    }
    const uint32_t second = ((((uint32_t)player->seed[1] * player->seed[1]) >> 8) & 0xffffu) ^ 0x6e47u;
    player->seed[1] = (uint16_t)second;
    player->seed[0] = (uint16_t)((state & 0xffffu) ^ second);
}

// A, type 4: a resonant filter run over the first waveform, starting from the end of the previous output; the position
// sets its frequency and damping. `previous` is the last output (or the first waveform before there is one).
static void morph_filter(const SunMemory* mem, const SunBuild* build, const int8_t* previous, uint32_t source,
                         uint32_t position, int8_t* out, uint32_t bytes) {
    const uint16_t quotient = (uint16_t)(0xfffe0u / (position + 0x20));
    const uint32_t damping = 0x26u * (uint16_t)(int16_t)(int8_t)position;
    const uint16_t pull_gain = (uint16_t)(quotient - (uint16_t)damping);
    const uint32_t scaled = (uint32_t)(uint16_t)(0x7fff - pull_gain) * build->filter_scale;
    const int16_t friction = (int16_t)(scaled >> 16);

    int16_t state = (int16_t)(previous[bytes - 1] * 128);
    int16_t velocity = (int16_t)((int8_t)(previous[bytes - 1] - previous[bytes - 2]) * 128);
    for (uint32_t i = 0; i < bytes; i++) {
        velocity = (int16_t)(((int32_t)velocity * friction >> 16) * 2);
        const int16_t target = (int16_t)(wave_sample(mem, source + i) * 128);
        const int16_t distance = (int16_t)(target - state);
        const int16_t pull = (int16_t)(((int32_t)distance * (int16_t)pull_gain >> 16) * 2);
        velocity = (int16_t)(velocity + pull);
        state = (int16_t)(state + velocity);
        out[i] = (int8_t)((uint16_t)state >> 7);
    }
}

// Generation A: computes the next waveform of an active voice into its back buffer, then advances the morph position.
static void morph_a(SunPlayer* player, SunVoice* voice) {
    const SunMemory* mem = &player->module.memory;
    const SunInstrument* inst = &voice->inst;
    const uint32_t bytes = (uint32_t)inst->words * 2;
    int8_t* out = voice->wave[1];
    const uint32_t position = sun_mem_u8(mem, inst->morph + voice->morph_pos);

    switch (inst->morph_type) {
        case 1:
            if (position == SUN_MORPH_COPY) {
                load_wave(mem, inst->wave_a, out, bytes);
            } else if (position == SUN_MORPH_RANDOM) {
                voice->primed = true;
                morph_random(player, out, bytes);
            } else {
                const bool in_place = voice->primed;
                voice->primed = true;
                morph_smooth(mem, inst, in_place, position, out, bytes);
            }
            break;
        case 2:
            // The first `position` bytes come from the second waveform, the rest from the first, shifted
            load_wave(mem, inst->wave_b, out, position);
            load_wave(mem, inst->wave_a + position, out + position, bytes - position);
            break;
        case 4: {
            int8_t first[SUN_WAVE_BYTES];
            load_wave(mem, inst->wave_a, first, bytes);
            const int8_t* previous = voice->primed ? out : first;
            int8_t result[SUN_WAVE_BYTES];
            voice->primed = true;
            morph_filter(mem, player->module.build, previous, inst->wave_a, position, result, bytes);
            memcpy(out, result, bytes);
            break;
        }
        default:
            // The first waveform stays
            if (!voice->primed) {
                voice->primed = true;
                load_wave(mem, inst->wave_a, out, bytes);
            }
            break;
    }
    advance_position(&voice->morph_pos, inst->morph_end, inst->morph_loop);
}

// Synth: one voice, every tick

// The volume the envelope and the voice volume give; the master volume scales it in generation A.
static uint8_t envelope_level(const SunPlayer* player, const SunVoice* voice) {
    const SunGen gen = player->module.build->gen;
    const uint32_t envelope = sun_mem_u8(&player->module.memory, voice->inst.envelope + voice->envelope_pos);
    uint32_t level = envelope * voice->volume;
    if (gen == SunGen_C) {
        return (uint8_t)(level >> 6);
    }
    level >>= 7;
    if (gen == SunGen_A) {
        level = (uint16_t)(level * player->master_volume) >> 6;
    }
    return (uint8_t)level;
}

// A, B: the volume changes by its slide and stays within 0 .. 0x80.
static void slide_volume_ab(SunVoice* voice) {
    const int sum = voice->volume + voice->volume_slide;
    voice->volume = (uint8_t)(sum < 0 ? 0 : (sum >= 0x81 ? 0x80 : sum));
}

// C: the volume is a byte that changes by its slide; a negative result clears it, one above 0x40 limits it.
static void slide_volume_c(SunVoice* voice) {
    const uint8_t sum = (uint8_t)(voice->volume + (uint8_t)voice->volume_slide);
    voice->volume = sum >= 0x80 ? 0 : (sum >= 0x41 ? 0x40 : sum);
}

// A: one tick of an active voice's synth.
static void synth_a(SunPlayer* player, SunVoice* voice) {
    const SunModule* module = &player->module;
    const SunInstrument* inst = &voice->inst;
    voice->out_volume = envelope_level(player, voice);
    voice->period = lookup_period(module, voice, vibrato_pitch(module, voice));

    voice->pitch = (uint16_t)(voice->pitch + voice->pitch_slide);
    if ((int16_t)voice->pitch < 0) {
        voice->pitch = (uint16_t)(voice->pitch + SUN_PITCH_LIMIT);
    }
    if ((int16_t)(voice->pitch - SUN_PITCH_LIMIT) >= 0) {
        voice->pitch = (uint16_t)(voice->pitch - SUN_PITCH_LIMIT);
    }
    slide_volume_ab(voice);
    voice->period_column = (voice->period_column + 1) & 0xf;
    advance_position(&voice->envelope_pos, inst->envelope_end, inst->envelope_loop);
    voice->vibrato_phase = (uint16_t)(voice->vibrato_phase + inst->vibrato_step);
    advance_position(&voice->vibrato_pos, inst->vibrato_end, inst->vibrato_loop);
}

// B: one tick of an active voice's synth, including the morph.
static void synth_b(SunPlayer* player, SunVoice* voice) {
    const SunModule* module = &player->module;
    const SunMemory* mem = &module->memory;
    const SunInstrument* inst = &voice->inst;
    voice->out_volume = envelope_level(player, voice);
    voice->period = lookup_period(module, voice, vibrato_pitch(module, voice));

    const int32_t weight = sun_mem_u8(mem, inst->morph + voice->morph_pos);
    if (inst->wave_b != 0) {
        morph_blend(mem, inst, weight, voice->wave[1]);
    } else {
        const bool in_place = voice->primed;
        voice->primed = true;
        morph_hold(mem, inst, in_place, weight, voice->wave[1]);
    }

    voice->pitch = (uint16_t)(voice->pitch + voice->pitch_slide);
    slide_volume_ab(voice);
    voice->period_column = (voice->period_column + 1) & 7;
    advance_position(&voice->envelope_pos, inst->envelope_end, inst->envelope_loop);
    voice->vibrato_phase = (uint16_t)(voice->vibrato_phase + inst->vibrato_step);
    advance_position(&voice->vibrato_pos, inst->vibrato_end, inst->vibrato_loop);
    advance_position(&voice->morph_pos, inst->morph_end, inst->morph_loop);
}

// C: one tick of an active voice's synth, including the morph. The envelope and the morph position each run once up to
// their end, then loop between the loop position and the stop position.
static void synth_c(SunPlayer* player, SunVoice* voice) {
    const SunModule* module = &player->module;
    const SunMemory* mem = &module->memory;
    const SunInstrument* inst = &voice->inst;
    voice->out_volume = envelope_level(player, voice);
    voice->period = lookup_period(module, voice, voice->pitch);

    morph_blend(mem, inst, sun_mem_u8(mem, inst->morph + voice->morph_pos), voice->wave[1]);

    voice->pitch = (uint16_t)(voice->pitch + voice->pitch_slide);
    slide_volume_c(voice);
    voice->period_column = (voice->period_column + 1) & 7;
    advance_position_c(&voice->envelope_pos, &voice->envelope_looping, inst->envelope_end, inst->envelope_loop,
                       inst->envelope_stop);
    advance_position_c(&voice->morph_pos, &voice->morph_looping, inst->morph_end, inst->morph_loop, inst->morph_stop);
}

static void synth_voice(SunPlayer* player, SunVoice* voice) {
    if (!voice->active) {
        return;
    }
    switch (player->module.build->gen) {
        case SunGen_A:
            synth_a(player, voice);
            break;
        case SunGen_B:
            synth_b(player, voice);
            break;
        case SunGen_C:
            synth_c(player, voice);
            break;
    }
}

// Sequencer: one voice, once per row

static void start_instrument(SunPlayer* player, SunVoice* voice, uint8_t value) {
    // The song walk proved the byte selects a record of the synth table
    const uint32_t index = (uint32_t)value - 0x40;
    voice->last_instrument = value;
    voice->inst = sun_module_instrument(&player->module, index);
    voice->instrument = (int)index;
    voice->active = true;
    voice->period_row = 0;
    voice->period_column = 0;
    voice->envelope_pos = 0;
    voice->morph_pos = 0;
    voice->vibrato_phase = 0;
    voice->vibrato_pos = 0;
    voice->primed = false;
    voice->envelope_looping = false;
    voice->morph_looping = false;
    voice->restart_words = voice->inst.words;
}

// A note sets the pitch (and in generation A starts the last instrument again).
static void start_note(SunPlayer* player, SunVoice* voice, uint8_t note) {
    const SunGen gen = player->module.build->gen;
    const uint8_t semitone = gen == SunGen_C ? (uint8_t)~note : (uint8_t)(~note - voice->transpose);
    voice->pitch = (uint16_t)(semitone << 8);
    voice->pitch_slide = 0;
    if (gen == SunGen_A && voice->last_instrument != 0) {
        start_instrument(player, voice, voice->last_instrument);
    }
}

static void apply_command(SunPlayer* player, SunVoice* voice, const SunItem* item) {
    const SunGen gen = player->module.build->gen;
    switch (item->value) {
        case SunCommand_PeriodRow:
            voice->period_row = item->args[0];
            voice->period_column = 0;
            break;
        case SunCommand_Slide:
            voice->pitch_slide
                = gen == SunGen_A ? (int16_t)((item->args[0] << 8) | item->args[1]) : (int16_t)(int8_t)item->args[0];
            break;
        case SunCommand_VolumeSlide:
            voice->volume_slide = (int8_t)item->args[0];
            break;
        case SunCommand_Volume:
            voice->volume = gen == SunGen_C ? item->args[0] : (uint8_t)(item->args[0] << 1);
            voice->volume_slide = 0;
            break;
        case SunCommand_Speed:
            player->speed = item->args[0];
            break;
        default:
            break;
    }
    if (gen != SunGen_A) {
        return;
    }
    switch (item->value) {
        case SunCommand_Seed:
            player->seed[0] = (uint16_t)((item->args[0] << 8) | item->args[1]);
            player->seed[1] = player->seed[0] ^ 0x7e28;
            break;
        case SunCommand_RestartMorph:
            voice->morph_pos = 0;
            break;
        case SunCommand_RestartEnvelope:
            voice->envelope_pos = 0;
            break;
        case SunCommand_SetPitch:
            voice->pitch = (uint16_t)((uint8_t)(~item->args[0] - voice->transpose) << 8);
            voice->pitch_slide = 0;
            break;
        case SunCommand_Fade:
            player->fade_step = (int8_t)item->args[0];
            player->fade_reload = item->args[1];
            break;
        case SunCommand_MasterVolume:
            // The build that reads its master volume from a fixed byte never changes it
            if (!player->module.build->fixed_master_volume) {
                player->master_volume = item->args[0];
            }
            break;
        default:
            break;
    }
}

// Reads a voice's pattern up to the end of the row.
static void sequence_voice(SunPlayer* player, SunVoice* voice) {
    const SunGen gen = player->module.build->gen;
    for (int items = 0; items < SUN_MAX_ROW_BYTES; items++) {
        const SunItem item = sun_read_item(&player->module.memory, gen, &voice->pattern);
        switch (item.kind) {
            case SunItem_RowEnd:
                return;
            case SunItem_Instrument:
                start_instrument(player, voice, item.value);
                break;
            case SunItem_Note:
                start_note(player, voice, item.value);
                break;
            case SunItem_Command:
                apply_command(player, voice, &item);
                break;
        }
    }
}

// DMA and the Paula registers

static uint32_t wave_copy_bytes(const SunPlayer* player, const SunVoice* voice) {
    if (player->module.build->gen != SunGen_A) {
        return SUN_WAVE_BYTES_BC;
    }
    // The replayer copies whole longs, at least one
    const uint32_t longs = voice->inst.words >> 1;
    return 4 * (longs != 0 ? longs : 1);
}

// A: before the tick's work. The voices triggered by the last row stop their DMA and get the new start address.
static void stage_one_a(SunPlayer* player) {
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        SunVoice* voice = &player->voices[v];
        voice->start_dma = voice->restart_words != 0;
        if (voice->start_dma) {
            nuade_paula_channel_dma_off(&voice->paula);
        }
    }
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        SunVoice* voice = &player->voices[v];
        voice->paula.volume = voice->out_volume;
        voice->paula.period = voice->period;
        if (voice->start_dma) {
            voice->paula.lc = voice->wave[0];
            voice->paula.len_words = voice->restart_words;
        }
        voice->latched_words = voice->restart_words;
    }
}

// A: after the tick's work. DMA starts for the triggered voices; the registers written after it are the repeat. The
// back buffers become the front buffers.
static void stage_two_a(SunPlayer* player) {
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        SunVoice* voice = &player->voices[v];
        if (voice->start_dma) {
            nuade_paula_channel_dma_on(&voice->paula);
        }
    }
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        SunVoice* voice = &player->voices[v];
        if (voice->latched_words != 0) {
            voice->paula.lc = voice->wave[0];
            voice->paula.len_words = voice->latched_words;
            voice->restart_words = 0;
        }
        if (voice->active) {
            memcpy(voice->wave[0], voice->wave[1], wave_copy_bytes(player, voice));
        }
    }
}

// B, C: the tick after a row. Like stages one and two together, with the front buffers of all voices refreshed.
static void stage_full_bc(SunPlayer* player) {
    bool restart[SUN_VOICES];
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        restart[v] = player->voices[v].restart_words != 0;
        if (restart[v]) {
            nuade_paula_channel_dma_off(&player->voices[v].paula);
        }
    }
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        SunVoice* voice = &player->voices[v];
        voice->paula.volume = voice->out_volume;
        voice->paula.period = voice->period;
        if (restart[v]) {
            voice->paula.lc = voice->wave[0];
            voice->paula.len_words = voice->restart_words;
        }
        memcpy(voice->wave[0], voice->wave[1], SUN_WAVE_BYTES_BC);
    }
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        if (restart[v]) {
            nuade_paula_channel_dma_on(&player->voices[v].paula);
        }
    }
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        SunVoice* voice = &player->voices[v];
        if (restart[v]) {
            voice->paula.lc = voice->wave[0];
            voice->paula.len_words = SUN_WAVE_BYTES_BC / 2;
            voice->restart_words = 0;
        }
    }
}

// B, C: the other ticks. Volume and period, and the front buffers of the synth voices.
static void stage_light_bc(SunPlayer* player) {
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        SunVoice* voice = &player->voices[v];
        voice->paula.volume = voice->out_volume;
        voice->paula.period = voice->period;
        if (voice->active) {
            memcpy(voice->wave[0], voice->wave[1], SUN_WAVE_BYTES_BC);
        }
    }
}

// A: the master volume fades by fade_step every fade_reload + 1 ticks until it reaches 0 or 0x40.
static void fade_a(SunPlayer* player) {
    if (player->fade_step == 0) {
        return;
    }
    player->fade_timer--;
    if (player->fade_timer >= 0) {
        return;
    }
    player->fade_timer = (int8_t)player->fade_reload;
    player->master_volume = (uint8_t)(player->master_volume + player->fade_step);
    if ((int8_t)player->master_volume < 0) {
        player->master_volume = 0;
        player->fade_step = 0;
    }
    if (player->master_volume >= 0x41) {
        player->master_volume = 0x40;
        player->fade_step = 0;
    }
}

// Song position

static void load_step(SunPlayer* player, const SunStep* step) {
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        player->voices[v].pattern = step->pattern[v];
        player->voices[v].transpose = step->transpose[v];
    }
}

// The next step; the end of the table wraps to the first step (the eagleplayer's SongEnd, in generation A).
static void next_step(SunPlayer* player, bool* looped) {
    player->step_index++;
    for (;;) {
        bool end = false;
        const SunStep step = sun_module_step(&player->module, player->step_index, &end);
        if (!end) {
            load_step(player, &step);
            return;
        }
        player->step_index = 0;
        *looped = player->module.build->gen == SunGen_A;
    }
}

// Counts a tick; true when it is a row.
static bool advance_tick(SunPlayer* player, bool* looped) {
    player->tick_count++;
    if (player->tick_count != player->speed) {
        return false;
    }
    player->tick_count = 0;
    player->row_count++;
    if (player->row_count == SUN_ROWS_PER_STEP) {
        player->row_count = 0;
        next_step(player, looped);
    }
    return true;
}

// Player

bool sun_probe(const uint8_t* data, size_t size) {
    SunModule module;
    if (!sun_module_open(&module, data, size)) {
        return false;
    }
    sun_module_close(&module);
    return true;
}

SunCreateResult sun_create(const uint8_t* data, size_t size) {
    SunPlayer* player = calloc(1, sizeof(*player));
    if (player == NULL) {
        return (SunCreateResult) { .status = SunStatus_OutOfMemory };
    }
    if (!sun_module_open(&player->module, data, size)) {
        free(player);
        return (SunCreateResult) { .status = SunStatus_InvalidData };
    }
    return (SunCreateResult) { .player = player, .status = SunStatus_Ok };
}

void sun_destroy(SunPlayer* player) {
    if (player != NULL) {
        sun_module_close(&player->module);
        free(player);
    }
}

bool sun_start(SunPlayer* player, NuadeVoices* voices) {
    const SunModule* module = &player->module;
    const SunMemory* mem = &module->memory;
    const SunGen gen = module->build->gen;

    memset(player->voices, 0, sizeof(player->voices));
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        player->voices[v].instrument = -1;
        nuade_voice_stop(voices, v);
    }
    switch (gen) {
        case SunGen_A:
            player->tick_count = 5;
            player->row_count = 0xff;
            player->speed = 6;
            player->step_index = 0;
            break;
        case SunGen_B:
            player->tick_count = 4;
            player->row_count = 0xf;
            player->speed = 5;
            player->step_index = (uint16_t)sun_module_start_step(module);
            break;
        case SunGen_C:
            player->tick_count = 5;
            player->row_count = 0xff;
            player->speed = 6;
            player->step_index = (uint16_t)sun_module_start_step(module);
            break;
    }
    player->fade_step = 0;
    player->fade_reload = 0;
    player->fade_timer = 0;
    player->master_volume = sun_mem_u8(mem, module->vars + module->build->master_volume);
    player->seed[0] = sun_mem_u16(mem, module->vars + SUN_A_SEED);
    player->seed[1] = sun_mem_u16(mem, module->vars + SUN_A_SEED + 2);

    // InitSound hands the first step's patterns to the voices
    bool end = false;
    const SunStep first = sun_module_step(module, 0, &end);
    load_step(player, &first);
    return !end;
}

SunTickResult sun_tick(SunPlayer* player, NuadeVoices* voices) {
    SunTickResult result = { 0 };
    const SunGen gen = player->module.build->gen;

    if (gen == SunGen_A) {
        stage_one_a(player);
        fade_a(player);
    } else if (player->tick_count == 0) {
        stage_full_bc(player);
    } else {
        stage_light_bc(player);
    }

    const bool row = advance_tick(player, &result.looped);
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        SunVoice* voice = &player->voices[v];
        if (row) {
            sequence_voice(player, voice);
        }
        synth_voice(player, voice);
    }

    if (gen == SunGen_A) {
        stage_two_a(player);
        for (uint32_t v = 0; v < SUN_VOICES; v++) {
            if (player->voices[v].active) {
                morph_a(player, &player->voices[v]);
            }
        }
    }
    for (uint32_t v = 0; v < SUN_VOICES; v++) {
        nuade_paula_channel_sync(&player->voices[v].paula, v, voices);
    }
    return result;
}

const char* sun_format_name(const SunPlayer* player) {
    (void)player;
    return "SUN-Tronic";
}

SunVoiceState sun_voice_state(const SunPlayer* player, uint32_t voice) {
    const SunVoice* v = &player->voices[voice];
    return (SunVoiceState) { .period = v->period, .instrument = v->instrument };
}
