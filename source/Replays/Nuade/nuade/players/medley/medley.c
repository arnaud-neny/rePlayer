// SPDX-License-Identifier: MIT
// Medley: the replayer of the UADE eagleplayer "Medley". One tick steps the arpeggio table, then for each voice: the
// track (a note and a duration, or a command), every ($speed + 1) ticks the pitch, the envelope, the four modulators
// and the Paula registers. A voice with a random wait is stopped and restarted at a random time instead of looping.

#include <nuade/players/medley.h>

#include "mdl_internal.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

#define MDL_FULL_VOLUME 0x3f
// The synthesised wave: bytes in the buffer, and the octaves above which it shrinks and the period stops shifting.
#define MDL_WAVE_BYTES 32
#define MDL_SYNTH_PERIOD_BASE 12
#define MDL_SYNTH_SHRINK_FROM 4
#define MDL_SYNTH_MAX_SHRINK 4
// The Paula channel idles on this much silence.
#define MDL_SILENCE_BYTES 4
#define MDL_CHIP_BYTES (MDL_SILENCE_BYTES + MDL_VOICES * MDL_WAVE_BYTES)
// Bytes of a call stack entry (the call depth is counted in bytes).
#define MDL_CALL_ENTRY_BYTES 4
// The period table index of ProTracker's C-1 (856) and its note number.
#define MDL_C1_INDEX 50
#define MDL_C1_NOTE 12

// A random voice counts its wait down every fourth tick.
#define MDL_RANDOM_WAIT_TICK_MASK 3
#define MDL_TIE_BIT 0x80

MdlPlayer* mdl_create(const uint8_t* data, size_t size) {
    MdlPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    if (!mdl_module_open(&player->module, data, size)) {
        free(player);
        return nullptr;
    }
    player->module.bytes = nuade_paula_padded_copy(data, size);
    player->chip = nuade_paula_padded_copy(nullptr, MDL_CHIP_BYTES);
    if (player->module.bytes == nullptr || player->chip == nullptr) {
        mdl_destroy(player);
        return nullptr;
    }
    for (int i = 0; i < MDL_VOICES; i++) {
        player->voices[i].index = i;
        player->voices[i].wave_buffer = (int8_t*)player->chip + MDL_SILENCE_BYTES + i * MDL_WAVE_BYTES;
    }
    if (!mdl_module_covered(&player->module)) {
        mdl_destroy(player);
        return nullptr;
    }
    return player;
}

void mdl_destroy(MdlPlayer* player) {
    if (player != nullptr) {
        free(player->module.bytes);
        free(player->chip);
        free(player);
    }
}

bool mdl_probe(const uint8_t* data, size_t size) {
    MdlPlayer* player = mdl_create(data, size);
    mdl_destroy(player);
    return player != nullptr;
}

int mdl_subsong_count(const MdlPlayer* player) {
    return player->module.subsong_count;
}

// The volume table of the replayer: entry n is n * scale / 63, counted with a running remainder.

static void mdl_build_volume_table(uint8_t* table, uint8_t scale) {
    const uint8_t step = scale & MDL_FULL_VOLUME;
    uint8_t remainder = MDL_FULL_VOLUME;
    uint8_t value = 0;
    for (int i = 0; i < MDL_VOLUME_STEPS; i++) {
        table[i] = value;
        remainder = (uint8_t)(remainder + step);
        if (remainder >= MDL_FULL_VOLUME) {
            remainder = (uint8_t)(remainder - MDL_FULL_VOLUME);
            value++;
        }
    }
}

// The arpeggio table gives each voice a period offset; a frame is applied to all four voices.

static void mdl_apply_arpeggio_frame(MdlPlayer* player, const int8_t* frame) {
    for (int i = 0; i < MDL_VOICES; i++) {
        player->voices[i].detune = frame[i];
    }
}

// The replayer starts a voice by clearing its record and switching its DMA off.

static void mdl_reset_voice(MdlVoice* voice) {
    *voice = (MdlVoice) { .index = voice->index, .wave_buffer = voice->wave_buffer, .duration = 1, .shown_note = -1 };
    mdl_build_volume_table(voice->volume_table, MDL_FULL_VOLUME);
}

// The song set-up of one voice (InitSound runs it for each voice that has a track). The shared settings are written
// again by every voice, and the arpeggio's first frame goes to all four voices, so the voices after this one clear
// their detune when they are reset.

static void mdl_setup_voice(MdlPlayer* player, MdlVoice* voice) {
    MdlModule* module = &player->module;
    const MdlSong* song = &player->song;
    const int i = voice->index;
    voice->restart = mdl_track_start(module, song->track[i]);
    voice->sequence = voice->restart;
    const uint8_t number = mdl_song_voice_instrument(song, i);
    const uint32_t instrument = mdl_instrument_offset(module, number);
    if (voice->restart == MDL_NO_OFFSET || instrument == MDL_NO_OFFSET) {
        module->unmapped = true;
        return;
    }
    voice->instrument_number = number;
    voice->instrument = mdl_instrument(module, instrument);
    voice->random_wait_min = song->random_wait_min[i];
    voice->active = voice->random_wait_min == 0;
    voice->random_wait_mask = song->random_wait_mask[i];
    mdl_build_volume_table(player->volume_table, song->master_volume);
    player->speed = song->speed;
    player->transpose = song->transpose;
    player->arpeggio = song->arpeggio < MDL_ARPEGGIO_COUNT ? song->arpeggio : 0;
    player->arpeggio_position = 0;
    mdl_apply_arpeggio_frame(player, mdl_arpeggio_table[player->arpeggio].step[0]);
    player->arpeggio_speed = song->arpeggio_speed;
    player->arpeggio_counter = 1;
    mdl_build_volume_table(voice->volume_table, song->voice_volume[i]);
    voice->sequence_running = true;
}

// InitSound for subsong `subsong` (the replayer numbers them from 1).

static void mdl_reset(MdlPlayer* player, int subsong) {
    MdlModule* module = &player->module;
    module->unmapped = false;
    player->playing = false;
    player->song_end = false;
    player->ended = false;
    player->stalled = false;
    player->tick_count = 0;
    player->random_state = 0;
    player->transpose = 0;
    player->speed = 0;
    player->arpeggio = 0;
    player->arpeggio_speed = 0;
    player->arpeggio_counter = 0;
    player->arpeggio_position = 0;
    mdl_build_volume_table(player->volume_table, MDL_FULL_VOLUME);
    player->song = (MdlSong) { 0 };
    const bool found = mdl_song(module, (uint8_t)(subsong + 1), &player->song);
    for (int i = 0; i < MDL_VOICES; i++) {
        MdlVoice* voice = &player->voices[i];
        mdl_reset_voice(voice);
        if (found && player->song.track[i] != 0) {
            mdl_setup_voice(player, voice);
        }
    }
    player->playing = true;
}

bool mdl_start(MdlPlayer* player, int subsong, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr || subsong < 0 || subsong >= mdl_subsong_count(player)) {
        return false;
    }
    mdl_reset(player, subsong);
    for (uint32_t ch = 0; ch < MDL_VOICES; ch++) {
        nuade_voice_stop(voices, ch);
    }
    return !player->module.unmapped;
}

// The replayer's random numbers: a sum of the tick counter, 12 bits of it.

static uint16_t mdl_random(MdlPlayer* player) {
    player->random_state = (uint16_t)(player->random_state + player->tick_count);
    return (uint16_t)(player->random_state >> 4);
}

// A voice with a random wait waits, then starts a track picked from the list of choices at its own track's start.

static void mdl_random_start(MdlPlayer* player, MdlVoice* voice) {
    MdlModule* module = &player->module;
    if (voice->random_wait_min == 0 || (player->tick_count & MDL_RANDOM_WAIT_TICK_MASK) != 0) {
        return;
    }
    if (voice->random_wait == 0) {
        const uint8_t mask = (uint8_t)(voice->random_wait_mask - 1);
        voice->random_wait = (uint8_t)(((uint8_t)mdl_random(player) & mask) + voice->random_wait_min);
    }
    voice->random_wait--;
    if (voice->random_wait != 0) {
        return;
    }
    const uint16_t random = mdl_random(player);
    const uint8_t choices = mdl_random_list_count(module, voice->restart);
    const uint16_t choice = (uint16_t)(random & (uint16_t)(uint8_t)(choices - 1));
    const uint32_t start
        = mdl_track_start(module, mdl_choice_track(module, voice->restart + MDL_RANDOM_LIST_PAIRS, choice));
    if (start == MDL_NO_OFFSET) {
        return;
    }
    voice->sequence = start;
    voice->duration = 1;
    voice->active = true;
}

// Command 0: return from a call, or the end of the track. The end of a voice's own track restarts it and tells the
// eagleplayer; a random voice stops. False when the voice stopped.

static bool mdl_command_end(MdlPlayer* player, MdlVoice* voice, uint32_t* position) {
    if (voice->call_depth != 0) {
        voice->call_depth--;
        *position = voice->call_stack[voice->call_depth];
        return true;
    }
    if (voice->random_wait_min == 0) {
        *position = voice->restart;
        player->song_end = true;
        return true;
    }
    voice->active = false;
    nuade_paula_channel_dma_off(&voice->paula);
    return false;
}

// Command 3: a new instrument; an empty entry leaves the old one.

static void mdl_command_instrument(MdlPlayer* player, MdlVoice* voice, uint8_t number) {
    const uint32_t offset = mdl_instrument_offset(&player->module, number);
    if (offset != MDL_NO_OFFSET) {
        voice->instrument = mdl_instrument(&player->module, offset);
        voice->instrument_number = number;
    }
}

// Command 4: jump to a track picked at random from the list that follows the command (pairs of a byte and a track
// number). The replayer adds the entry's long to the start of the track table, not to the entry's address.

static void mdl_command_random_track(MdlPlayer* player, uint8_t count, uint32_t* position) {
    MdlModule* module = &player->module;
    if (count == 0) {
        return;
    }
    const uint16_t choice = (uint16_t)(mdl_random(player) & (uint16_t)(count - 1));
    const uint32_t target = mdl_track_from_table_start(module, mdl_choice_track(module, *position, choice));
    if (target != MDL_NO_OFFSET) {
        *position = target;
    }
}

// Command 5: call another track; the call stack holds eight returns, and a call on a full stack does nothing.

static void mdl_command_call(MdlPlayer* player, MdlVoice* voice, uint8_t track, uint32_t* position) {
    const uint32_t start = mdl_track_start(&player->module, track);
    if (start == MDL_NO_OFFSET || voice->call_depth >= MDL_STACK_DEPTH) {
        return;
    }
    voice->call_stack[voice->call_depth++] = *position;
    *position = start;
}

// Commands 6 and 7: a loop of `count` passes (a byte count that wraps: 0 is 256 passes).

static void mdl_command_loop_start(MdlVoice* voice, uint8_t count, uint32_t position) {
    if (voice->loop_depth >= MDL_STACK_DEPTH) {
        return;
    }
    voice->loop_start[voice->loop_depth] = position;
    voice->loop_count[voice->loop_depth] = count;
    voice->loop_depth++;
}

static void mdl_command_loop_end(MdlVoice* voice, uint32_t* position) {
    if (voice->loop_depth == 0) {
        return;
    }
    const uint8_t top = (uint8_t)(voice->loop_depth - 1);
    voice->loop_count[top]--;
    if (voice->loop_count[top] == 0) {
        voice->loop_depth--;
    } else {
        *position = voice->loop_start[top];
    }
}

// Runs one command with its parameter; false when the voice stopped.

static bool mdl_run_command(MdlPlayer* player, MdlVoice* voice, uint8_t command, uint8_t param, uint32_t* position) {
    switch (command) {
        case MdlCommand_End:
            return mdl_command_end(player, voice, position);
        case MdlCommand_Volume:
            mdl_build_volume_table(voice->volume_table, (uint8_t)(param >> 1));
            break;
        case MdlCommand_Instrument:
            mdl_command_instrument(player, voice, param);
            break;
        case MdlCommand_RandomTrack:
            mdl_command_random_track(player, param, position);
            break;
        case MdlCommand_Call:
            mdl_command_call(player, voice, param, position);
            break;
        case MdlCommand_LoopStart:
            mdl_command_loop_start(voice, param, *position);
            break;
        case MdlCommand_LoopEnd:
            mdl_command_loop_end(voice, position);
            break;
        case MdlCommand_Transpose:
            voice->note_offset = param;
            break;
        default: // MdlCommand_Nop and the unused numbers
            break;
    }
    return true;
}

// Reads the track up to the next note or rest: a note byte (1 up) or a rest (0) and a duration byte; a duration of 0
// is skipped, one with the top bit set ties the note to the one before. False when the voice stopped.

static bool mdl_next_step(MdlPlayer* player, MdlVoice* voice) {
    MdlModule* module = &player->module;
    uint32_t position = voice->sequence;
    voice->new_step = true;
    voice->divider = 0;
    for (int commands = 0; commands < MDL_MAX_STEP_COMMANDS; commands++) {
        const uint8_t byte = mdl_u8(module, position++);
        if ((byte & MDL_COMMAND_BIT) != 0) {
            const uint8_t param = mdl_u8(module, position++);
            if (!mdl_run_command(player, voice, byte & MDL_COMMAND_MASK, param, &position)) {
                return false;
            }
            continue;
        }
        if (byte != 0) {
            voice->note = byte;
            voice->key_on = true;
            voice->retrigger = true;
        } else {
            voice->key_on = false;
            voice->retrigger = false;
        }
        const uint8_t duration = mdl_u8(module, position++);
        if (duration == 0) {
            continue;
        }
        voice->duration = duration & (uint8_t)~MDL_TIE_BIT;
        if ((duration & MDL_TIE_BIT) != 0) {
            voice->retrigger = false;
        }
        voice->sequence = position;
        return true;
    }
    player->stalled = true;
    return false;
}

// Sets the wave the voice plays: the replayer reads the wave table without a check for an empty entry.

static void mdl_select_wave(MdlPlayer* player, MdlVoice* voice, uint8_t number) {
    if (!mdl_wave(&player->module, number, &voice->wave)) {
        player->module.unmapped = true;
    }
}

static uint16_t mdl_period(MdlPlayer* player, uint32_t index) {
    if (index < MDL_PERIOD_COUNT) {
        return mdl_period_table[index];
    }
    const MdlVoice* voice = &player->voices[0];
    if (index == MDL_PERIOD_CALL_DEPTH_INDEX) {
        return (uint16_t)(voice->call_depth * MDL_CALL_ENTRY_BYTES);
    }
    if (mdl_period_is_call_stack_word(index)) {
        // The return addresses: a stack entry keeps its value after the return. The high word comes first.
        const uint32_t word = index - MDL_PERIOD_CALL_STACK_INDEX;
        const uint32_t entry = voice->call_stack[word / 2];
        const uint32_t address = entry == 0 ? 0 : MDL_MODULE_ADDRESS + entry;
        return (uint16_t)(word % 2 == 0 ? address >> 16 : address);
    }
    player->module.unmapped = true;
    return 0;
}

// The pitch of a new note. A synthesised wave takes the octave from the note and shrinks with it; a sampled
// instrument picks the wave of the note's range (the nearest below that has one) and the period from the note's
// distance to the wave's octave.

static void mdl_select_pitch(MdlPlayer* player, MdlVoice* voice) {
    const MdlInstrument* instrument = &voice->instrument;
    const uint8_t note = (uint8_t)(voice->note + player->transpose + voice->note_offset + instrument->transpose);
    if (instrument->type == MdlInstrumentType_Synth) {
        voice->octave = note / MDL_NOTES_PER_OCTAVE;
        voice->base_period = mdl_period(player, MDL_SYNTH_PERIOD_BASE + note % MDL_NOTES_PER_OCTAVE);
        voice->shown_note = note;
        mdl_select_wave(player, voice, instrument->wave[0]);
        voice->wave_built = false;
        return;
    }
    mdl_select_wave(player, voice, mdl_region_wave(instrument, mdl_note_region(note)));
    if (voice->wave.octave >= MDL_REGIONS) {
        player->module.unmapped = true;
        return;
    }
    const uint8_t index = mdl_sample_period_index(note, instrument, &voice->wave);
    voice->base_period = mdl_period(player, index);
    voice->shown_note = index - MDL_C1_INDEX + MDL_C1_NOTE;
}

// The envelope: attack to the maximum, then decay to the sustain level while the key is down, release after it.

static void mdl_envelope(MdlVoice* voice) {
    const MdlInstrument* instrument = &voice->instrument;
    uint16_t level = voice->envelope;
    if (voice->retrigger) {
        level = 0;
        voice->decaying = false;
    }
    if (!voice->key_on) {
        level = level < instrument->release ? 0 : (uint16_t)(level - instrument->release);
    } else if (voice->decaying) {
        level = level < instrument->decay ? instrument->sustain_level : (uint16_t)(level - instrument->decay);
        if (level < instrument->sustain_level) {
            level = instrument->sustain_level;
        }
    } else {
        const uint32_t sum = (uint32_t)level + instrument->attack;
        if (sum > 0xffff || (uint16_t)sum >= instrument->max_level) {
            level = instrument->max_level;
            voice->decaying = true;
        } else {
            level = (uint16_t)sum;
        }
    }
    voice->envelope = level;
}

// The two voice flags a modulator can be tied to (index 1 and 2): set for voices 1, 2 and for voices 2, 3.

static bool mdl_voice_flag(MdlPlayer* player, const MdlVoice* voice, uint8_t flag) {
    if (!mdl_voice_flag_valid(flag)) {
        player->module.unmapped = true;
        return false;
    }
    return flag == 1 ? voice->index == 1 || voice->index == 2 : voice->index >= 2;
}

// A modulator at a note start: the delay, and where it begins.

static void mdl_modulator_start(MdlPlayer* player, const MdlVoice* voice, const MdlModulatorSetup* setup,
                                MdlModulatorState* state) {
    state->delay = setup->delay;
    if (setup->free_running) {
        return;
    }
    state->falling = false;
    state->value = 0;
    if (setup->top_start != 0 && mdl_voice_flag(player, voice, setup->top_start)) {
        state->falling = true;
        state->value = setup->level;
    }
    const bool half = (setup->kind == MdlModulatorKind_Vibrato && setup->delay != 0)
                      || (setup->half_start != 0 && mdl_voice_flag(player, voice, setup->half_start));
    if (half) {
        state->value = setup->level >> 1;
    }
}

// One step of a modulator: after its delay it rises to the level, then falls to zero, and rises again unless it is a
// one-shot.

static void mdl_modulator_step(const MdlModulatorSetup* setup, MdlModulatorState* state) {
    if (state->delay != 0) {
        state->delay--;
        return;
    }
    if (!state->falling) {
        const uint32_t sum = (uint32_t)state->value + setup->rise;
        if (sum > 0xffff || (uint16_t)sum >= setup->level) {
            state->value = setup->level;
            state->falling = true;
        } else {
            state->value = (uint16_t)sum;
        }
    } else if (state->value > setup->fall) {
        state->value = (uint16_t)(state->value - setup->fall);
    } else {
        state->value = 0;
        if (!setup->one_shot) {
            state->falling = false;
        }
    }
}

// Applies a modulator's output to the part of the voice its kind names.

static void mdl_modulator_apply(MdlVoice* voice, const MdlModulatorSetup* setup, uint16_t out) {
    switch (setup->kind) {
        case MdlModulatorKind_Vibrato: {
            const int16_t offset = (int16_t)(uint16_t)((setup->level >> 1) - out);
            voice->period = (uint16_t)(voice->period + (offset >> 8));
            break;
        }
        case MdlModulatorKind_Tremolo:
            voice->volume = voice->volume < out ? 0 : (uint16_t)(voice->volume - out);
            break;
        case MdlModulatorKind_Window:
            voice->window_offset = (uint16_t)(voice->window_offset + (out >> 8));
            break;
        case MdlModulatorKind_Phase:
            voice->phase_offset = (uint16_t)(voice->phase_offset + (out >> 8));
            break;
        case MdlModulatorKind_PeriodUp:
            voice->period = (uint16_t)(voice->period + (out >> 5));
            break;
        case MdlModulatorKind_PeriodDown:
            voice->period = (uint16_t)(voice->period - (out >> 5));
            break;
        default:
            voice->tone_offset = (uint8_t)(voice->tone_offset + (out >> 8));
            break;
    }
}

// The modulators of the instrument run after the envelope has set the voice's volume and period.

static void mdl_modulators(MdlPlayer* player, MdlVoice* voice) {
    voice->period = voice->base_period;
    voice->volume = voice->envelope;
    voice->window_offset = 0;
    voice->phase_offset = 0;
    voice->tone_offset = 0;
    for (int i = 0; i < MDL_MODULATORS; i++) {
        const MdlModulatorSetup* setup = &voice->instrument.modulator[i];
        MdlModulatorState* state = &voice->modulator[i];
        if (setup->kind == MdlModulatorKind_Off) {
            continue;
        }
        if (voice->retrigger) {
            mdl_modulator_start(player, voice, setup, state);
        }
        mdl_modulator_step(setup, state);
        uint16_t out = state->value;
        if (setup->gate) {
            out = state->falling ? setup->level : 0;
        }
        if (setup->invert) {
            out = (uint16_t)(setup->level - out);
        }
        mdl_modulator_apply(voice, setup, out);
    }
}

// AUDxLC for a sample in the module: Paula ignores the low bit of the address. A sample that ends outside the file
// plays memory the port does not model.

static const int8_t* mdl_module_lc(MdlPlayer* player, int64_t offset, uint16_t words) {
    if (!mdl_sample_inside(&player->module, offset, words)) {
        player->module.unmapped = true;
        return nullptr;
    }
    return (const int8_t*)player->module.bytes + (offset & ~INT64_C(1));
}

// Fills the voice's buffer with the sum of two reads of the wave: each steps through it at its own rate, the
// instrument's tone setting names the two steps (a nibble each). The rates double with the note's octave from the
// fourth, while the buffer shrinks to keep the cycle in step with the period.

static void mdl_build_wave(MdlPlayer* player, MdlVoice* voice, uint16_t phase) {
    MdlModule* module = &player->module;
    const uint8_t tone = (uint8_t)(voice->instrument.tone + voice->tone_offset);
    uint16_t step_a = tone >> 4;
    uint16_t step_b = tone & 0xf;
    step_a = step_a == 0 ? 0x10 : step_a;
    step_b = step_b == 0 ? 0x10 : step_b;
    uint8_t count = MDL_WAVE_BYTES;
    if (voice->octave >= MDL_SYNTH_SHRINK_FROM) {
        uint8_t shrink = (uint8_t)(voice->octave - MDL_SYNTH_SHRINK_FROM);
        shrink = shrink > MDL_SYNTH_MAX_SHRINK ? MDL_SYNTH_MAX_SHRINK : shrink;
        count = (uint8_t)(count >> shrink);
        step_a = (uint16_t)(step_a << shrink);
        step_b = (uint16_t)(step_b << shrink);
        voice->period_shift = MDL_SYNTH_SHRINK_FROM;
    } else {
        voice->period_shift = voice->octave;
    }
    voice->wave_words = (uint16_t)(count / 2);
    const uint16_t mask = (uint16_t)(voice->wave.length - 1);
    if (mask > 0x7fff) {
        module->unmapped = true;
        return;
    }
    uint16_t a = phase;
    uint16_t b = 0;
    for (uint8_t i = 0; i < count; i++) {
        a &= mask;
        b &= mask;
        voice->wave_buffer[i]
            = (int8_t)(uint8_t)(mdl_u8(module, voice->wave.data + a) + mdl_u8(module, voice->wave.data + b));
        a = (uint16_t)(a + step_a);
        b = (uint16_t)(b + step_b);
    }
    voice->wave_built = true;
}

// The sample a voice plays this tick, as AUDxLC and AUDxLEN.

typedef struct MdlSampleChoice {
    const int8_t* lc;
    uint16_t words;
} MdlSampleChoice;

static MdlSampleChoice mdl_choose_sample(MdlPlayer* player, MdlVoice* voice) {
    const MdlWave* wave = &voice->wave;
    MdlSampleChoice sample = { .lc = (const int8_t*)player->chip, .words = MDL_SILENCE_BYTES / 2 };
    switch (voice->instrument.type) {
        case MdlInstrumentType_Looped: {
            uint16_t window = (uint16_t)(voice->instrument.window + voice->window_offset);
            uint8_t low = (uint8_t)((uint8_t)window >> wave->octave);
            const bool second = (low & 1u) != 0;
            low = (uint8_t)(low & ~1u);
            window = (uint16_t)((window & 0xff00u) | low);
            const int64_t offset = mdl_window_offset(wave, second && wave->second_half, window);
            sample.words = (uint16_t)(wave->length >> 2);
            sample.lc = mdl_module_lc(player, offset, sample.words);
            break;
        }
        case MdlInstrumentType_Synth: {
            const uint16_t phase = (uint16_t)(voice->instrument.phase + voice->phase_offset);
            if (phase != voice->wave_phase || !voice->wave_built) {
                voice->wave_phase = phase;
                mdl_build_wave(player, voice, phase);
            }
            voice->period = (uint16_t)(voice->period >> voice->period_shift);
            sample.lc = voice->wave_buffer;
            sample.words = voice->wave_words;
            break;
        }
        case MdlInstrumentType_Sample:
            sample.words = (uint16_t)(wave->length >> 1);
            sample.lc = mdl_module_lc(player, wave->data, sample.words);
            break;
        default: // a one-shot instrument plays its wave at the note start and silence after it
            if (voice->retrigger) {
                sample.words = (uint16_t)(wave->length >> 1);
                sample.lc = mdl_module_lc(player, wave->data, sample.words);
            }
            break;
    }
    return sample;
}

// Writes the voice's Paula registers, in the replayer's order, and switches the DMA on.

static void mdl_write_registers(MdlPlayer* player, MdlVoice* voice) {
    const MdlSampleChoice sample = mdl_choose_sample(player, voice);
    voice->paula.lc = sample.lc;
    voice->paula.len_words = sample.words;
    voice->paula.period = (uint16_t)(voice->period + voice->detune);
    const uint8_t step = (uint8_t)(voice->volume >> 8) >> 2;
    voice->paula.volume = player->volume_table[voice->volume_table[step]];
    nuade_paula_channel_dma_on(&voice->paula);
}

// One tick of one voice.

static void mdl_voice_tick(MdlPlayer* player, MdlVoice* voice) {
    if (!voice->active) {
        mdl_random_start(player, voice);
        return;
    }
    if (voice->sequence_running) {
        voice->duration--;
        if (voice->duration == 0 && !mdl_next_step(player, voice)) {
            return;
        }
    }
    // The divider is tested as a signed byte: a speed of 128 or more updates the voice every tick.
    voice->divider--;
    if ((int8_t)voice->divider >= 0) {
        return;
    }
    voice->divider = player->speed;
    if (voice->new_step) {
        voice->new_step = false;
        mdl_select_pitch(player, voice);
    }
    mdl_envelope(voice);
    mdl_modulators(player, voice);
    mdl_write_registers(player, voice);
    voice->retrigger = false;
}

// The arpeggio table advances every `arpeggio_speed` ticks.

static void mdl_arpeggio_tick(MdlPlayer* player) {
    if (player->arpeggio_speed == 0) {
        return;
    }
    player->arpeggio_counter--;
    if (player->arpeggio_counter != 0) {
        return;
    }
    player->arpeggio_counter = player->arpeggio_speed;
    const MdlArpeggio* arpeggio = &mdl_arpeggio_table[player->arpeggio];
    if (player->arpeggio_position >= arpeggio->frames) {
        player->arpeggio_position = 0;
    }
    mdl_apply_arpeggio_frame(player, arpeggio->step[player->arpeggio_position]);
    player->arpeggio_position++;
}

// The interrupt. True when a voice called SongEnd.

static bool mdl_run_tick(MdlPlayer* player) {
    player->tick_count++;
    if (!player->playing) {
        return false;
    }
    player->song_end = false;
    mdl_arpeggio_tick(player);
    for (int i = 0; i < MDL_VOICES; i++) {
        mdl_voice_tick(player, &player->voices[i]);
    }
    return player->song_end;
}

MdlTickResult mdl_tick(MdlPlayer* player, NuadeVoices* voices) {
    MdlTickResult result = { 0 };
    if (player->ended) {
        result.ended = true;
        return result;
    }
    result.looped = mdl_run_tick(player);
    if (player->module.unmapped || player->stalled) {
        player->ended = true;
        result.ended = true;
        return result;
    }
    for (uint32_t ch = 0; ch < MDL_VOICES; ch++) {
        nuade_paula_channel_sync(&player->voices[ch].paula, ch, voices);
    }
    return result;
}

MdlVoiceState mdl_voice_state(const MdlPlayer* player, int voice) {
    const MdlVoice* v = &player->voices[voice];
    return (MdlVoiceState) { .note = v->shown_note,
                             .instrument = v->instrument_number != 0 ? v->instrument_number - 1 : -1 };
}
