// SPDX-License-Identifier: MIT
// Mike Davies' sound driver: the replayer every module carries, one function per routine. Each voice reads a track of
// notes (a period index and a duration) and commands, optionally calling a second track; a note or an instrument
// load marks the voice for the hardware pass, which stops its DMA, writes the registers, starts the DMA and writes
// the repeat.

#include <nuade/players/mikedavies.h>

#include "md_internal.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>

#define MD_DRY_RUN_SUBSONG_TICKS 100000u // the most one subsong is played in the dry run
#define MD_DRY_RUN_TICKS 3000000u        // the most all subsongs are played; the rest are not checked
#define MD_DRY_RUN_LOOPS 2u
#define MD_DRY_RUN_COMMANDS 50000000u // the most track entries all subsongs run through in the dry run
#define MD_FETCH_LIMIT 100000u        // commands one fetch may run through
#define MD_RESTART_LIMIT 16u          // restarts and stops one tick may run through
#define MD_INSTRUMENT_MASK 0x7F
#define MD_NO_INSTRUMENT (-1)
#define MD_NOT_A_REST 1 // the note after an instrument load: nonzero, so the DMA starts
#define MD_LOOP_COUNT_LOW_BYTE 0x00FFu
#define MD_DMA_REPEAT_LENGTH 2

// How a pass over the voices ends: all done, or a track command that leaves the pass.
typedef enum MdFlow {
    MdFlow_Continue,
    MdFlow_Restart, // the song starts again
    MdFlow_Stop,    // the replayer stops
    MdFlow_Fault,   // the module led out of what the port models
} MdFlow;

// The sample at a load address as an AUDxLC. Paula ignores bit 0; one that starts outside the module leaves the
// model.

static const int8_t* sample_address(MdPlayer* player, uint32_t address) {
    uint32_t offset = 0;
    if (!md_offset(&player->module, address, &offset)) {
        player->fault = true;
        return nullptr;
    }
    return (const int8_t*)player->chip + (offset & ~1u);
}

// The routine that switches everything off: the voices lose their tracks and the notes waiting, the volumes go to
// 0 and the DMA stops. The state of the voices that the routine does not touch stays.

static void clear_voices(MdPlayer* player) {
    for (uint32_t v = 0; v < MD_VOICES; v++) {
        MdVoice* voice = &player->voices[v];
        voice->main.pointer = MD_NO_TRACK;
        voice->trigger = false;
        voice->main.position = 0;
        voice->main.loop_count = 0;
        voice->called.pointer = 0;
        voice->called.position = 0;
        voice->called.loop_count = 0;
        voice->main.transpose = 0;
        voice->paula.volume = 0;
        nuade_paula_channel_dma_off(&voice->paula);
    }
    player->enabled = false;
}

// The init routine: everything off, then the voices get the tracks of the subsong and their first note is due.

static void init_song(MdPlayer* player, uint32_t subsong) {
    MdSubsong entry;
    player->subsong = subsong;
    clear_voices(player);
    if (!md_read_subsong(&player->module, subsong, &entry)) {
        player->fault = true;
        return;
    }
    player->enabled = true;
    for (uint32_t v = 0; v < MD_VOICES; v++) {
        MdVoice* voice = &player->voices[v];
        voice->main.pointer = entry.track[v];
        voice->duration = 1;
    }
}

// An instrument number, taken to instrument 0 when its record lies past the table, loads the voice's sample,
// period, volume, note transposition and repeat, and marks the voice for the hardware pass.

static bool load_instrument(MdPlayer* player, MdVoice* voice, uint16_t command) {
    MdInstrument instrument;
    uint32_t index = (uint32_t)(command & MD_INSTRUMENT_MASK);
    if (player->module.instruments + index * MD_INSTRUMENT_BYTES >= player->module.instruments_limit) {
        index = 0;
    }
    if (!md_read_instrument(&player->module, index, &instrument)) {
        return false;
    }
    voice->command = command;
    voice->sample = instrument.sample;
    voice->length_words = instrument.length_words;
    voice->period = instrument.period;
    voice->volume = instrument.volume;
    voice->instrument_transpose = instrument.transpose;
    voice->loop_offset = instrument.loop_offset;
    voice->trigger = true;
    voice->note = MD_NOT_A_REST;
    return true;
}

// The second track is the one that plays while it has a pointer.

static MdTrack* active_track(MdVoice* voice) {
    return voice->called.pointer != 0 ? &voice->called : &voice->main;
}

// The position of a track in the module. The code adds the 16-bit position sign-extended.

static uint32_t track_address(const MdTrack* track) {
    return track->pointer + (uint32_t)(int32_t)(int16_t)track->position;
}

// The end of a track: it starts again, and a call counts one play down; the last play ends the track.

static void command_end(MdVoice* voice, MdTrack* track) {
    track->position = 0;
    voice->repeats = (uint16_t)(voice->repeats - 1);
    if (voice->repeats == 0) {
        track->pointer = 0;
    }
}

// Runs the commands of the voice's track up to its next note. The two commands that leave the track end the
// fetch: the caller restarts the tick.

static MdFlow fetch_note(MdPlayer* player, MdVoice* voice, MdTrack** track_out, MdNote* note) {
    const MdModule* module = &player->module;
    const uint16_t unit = md_unit(module);
    for (uint32_t guard = 0; guard < MD_FETCH_LIMIT; guard++) {
        if (player->commands_left == 0) {
            return MdFlow_Fault;
        }
        player->commands_left--;
        MdTrack* track = active_track(voice);
        const uint32_t at = track_address(track);
        uint16_t code = 0;
        if (track->pointer == 0 || !md_read_code(module, at, &code)) {
            return MdFlow_Fault;
        }
        switch (md_command(module, code)) {
            case MdCommand_Note:
                if (!md_read_note(module, at, note)) {
                    return MdFlow_Fault;
                }
                track->position = (uint16_t)(track->position + 2u * unit);
                *track_out = track;
                return MdFlow_Continue;
            case MdCommand_End:
                command_end(voice, track);
                break;
            case MdCommand_Call: {
                MdCall call;
                if (!md_read_call(module, at, &call)) {
                    return MdFlow_Fault;
                }
                voice->repeats = call.repeats;
                voice->called.transpose = call.transpose;
                voice->called.pointer = call.track;
                voice->main.position = (uint16_t)(voice->main.position + 5u * unit);
                break;
            }
            case MdCommand_LoopNext:
                track->loop_count = (uint16_t)(track->loop_count - 1);
                if (track->loop_count == 0) {
                    track->position = (uint16_t)(track->position + unit);
                    break;
                }
                track->position = track->loop_position;
                if (module->layout->loop_back_ends_song) {
                    uint16_t next = 0;
                    if (!md_read_code(module, at + unit, &next)) {
                        return MdFlow_Fault;
                    }
                    player->looped = player->looped || md_command(module, next) == MdCommand_Stop;
                }
                break;
            case MdCommand_LoopStart: {
                uint16_t count = 0;
                if (!md_read_loop_count(module, at, &count)) {
                    return MdFlow_Fault;
                }
                track->position = (uint16_t)(track->position + 2u * unit);
                track->loop_position = track->position;
                // the byte tracks set the low byte of the count
                track->loop_count = module->layout->byte_tracks
                                        ? (uint16_t)((track->loop_count & ~MD_LOOP_COUNT_LOW_BYTE) | count)
                                        : count;
                break;
            }
            case MdCommand_Restart:
                return MdFlow_Restart;
            case MdCommand_Stop:
                return MdFlow_Stop;
            case MdCommand_Instrument:
                if (!load_instrument(player, voice, code)) {
                    return MdFlow_Fault;
                }
                track->position = (uint16_t)(track->position + unit);
                break;
        }
    }
    return MdFlow_Fault;
}

// The period of a note: the table entry of the note plus the transpositions (the index is a 16-bit signed byte
// offset). The later versions interpolate towards the next entry by the instrument's fraction.

static bool note_period(MdPlayer* player, const MdVoice* voice, uint16_t index, uint16_t* period) {
    const MdModule* module = &player->module;
    const int16_t offset = (int16_t)(index * 2u);
    uint16_t entry = 0;
    if (!md_read_period(module, offset, &entry)) {
        return false;
    }
    if (!module->layout->interpolates) {
        *period = entry;
        return true;
    }
    uint16_t next = 0;
    if (!md_read_period(module, (int16_t)(offset + 2), &next)) {
        return false;
    }
    const uint32_t step = (uint32_t)(uint16_t)(entry - next) * (uint16_t)voice->instrument_transpose;
    *period = (uint16_t)((step >> 16) + entry);
    return true;
}

// A note: its index into the period table is the note plus the track's transposition (a note 0 is a rest and gets none)
// plus the instrument's; the voice then waits for the hardware pass.

static bool play_note(MdPlayer* player, MdVoice* voice, const MdTrack* track, const MdNote* note) {
    const MdLayout* layout = player->module.layout;
    uint16_t index = note->note;
    const bool rest = note->note == 0;
    if (!rest) {
        index = (uint16_t)(index + (uint16_t)track->transpose);
    }
    if (!layout->interpolates && !(layout->rest_note && rest)) {
        index = (uint16_t)(index + (uint16_t)voice->instrument_transpose);
    }
    voice->note = index;
    if (!note_period(player, voice, index, &voice->period)) {
        return false;
    }
    voice->duration = note->duration;
    voice->trigger = true;
    return true;
}

// The sequencer pass: every voice whose note has run out takes its next note.

static MdFlow step_voices(MdPlayer* player) {
    if (!player->enabled) {
        return MdFlow_Continue;
    }
    for (uint32_t v = 0; v < MD_VOICES; v++) {
        MdVoice* voice = &player->voices[v];
        if (voice->main.pointer == MD_NO_TRACK) {
            continue;
        }
        voice->duration = (uint16_t)(voice->duration - 1);
        if (voice->duration != 0) {
            continue;
        }
        MdTrack* track = nullptr;
        MdNote note;
        const MdFlow flow = fetch_note(player, voice, &track, &note);
        if (flow != MdFlow_Continue) {
            return flow;
        }
        if (!play_note(player, voice, track, &note)) {
            return MdFlow_Fault;
        }
    }
    return MdFlow_Continue;
}

// The registers of a voice that starts: the DMA stops, the sample, period and volume are written. A period below the
// version's minimum is raised to it, or drops the voice from the pass.

static bool write_registers(MdPlayer* player, MdVoice* voice) {
    const MdLayout* layout = player->module.layout;
    if (voice->period < layout->min_period) {
        if (!layout->floor_clamps) {
            voice->trigger = false;
            return false;
        }
        voice->period = layout->min_period;
    }
    nuade_paula_channel_dma_off(&voice->paula);
    voice->paula.lc = sample_address(player, voice->sample);
    voice->paula.len_words = voice->length_words;
    voice->paula.period = voice->period;
    voice->paula.volume = (uint8_t)voice->volume;
    return true;
}

// The repeat registers, written once the DMA has started: the sample from its repeat offset on, or without one the
// first two words.

static void write_repeat(MdPlayer* player, MdVoice* voice) {
    voice->trigger = false;
    if (voice->loop_offset == 0) {
        voice->paula.lc = sample_address(player, voice->sample);
        voice->paula.len_words = MD_DMA_REPEAT_LENGTH;
        return;
    }
    // the length is computed in 16 bits: twice the words, less the offset, halved
    const uint16_t bytes = (uint16_t)((uint16_t)(voice->length_words * 2u) - (uint16_t)voice->loop_offset);
    voice->paula.lc = sample_address(player, voice->sample + (uint32_t)(int32_t)voice->loop_offset);
    voice->paula.len_words = (uint16_t)(bytes >> 1);
}

// The hardware pass. Three sweeps over the voices with a note waiting: registers (DMA off first), DMA on, repeat.
// Between them the code waits for the DMA to take the registers. A rest keeps its DMA off in the versions that mark it.

static void hardware(MdPlayer* player) {
    const bool rest_note = player->module.layout->rest_note;
    bool any = false;
    for (uint32_t v = 0; v < MD_VOICES; v++) {
        MdVoice* voice = &player->voices[v];
        if (voice->trigger) {
            any = write_registers(player, voice) || any;
        }
    }
    if (!any) {
        return;
    }
    for (uint32_t v = 0; v < MD_VOICES; v++) {
        MdVoice* voice = &player->voices[v];
        if (voice->trigger && !(rest_note && voice->note == 0)) {
            nuade_paula_channel_dma_on(&voice->paula);
        }
    }
    for (uint32_t v = 0; v < MD_VOICES; v++) {
        if (player->voices[v].trigger) {
            write_repeat(player, &player->voices[v]);
        }
    }
}

// The interrupt routine: the passes of sequencer and hardware. A restart or stop command leaves the routine through
// its beginning, after the init or the clearing it asked for.

static void run_tick(MdPlayer* player) {
    player->looped = false;
    player->ended = false;
    for (uint32_t restarts = 0; restarts <= MD_RESTART_LIMIT; restarts++) {
        MdFlow flow = MdFlow_Continue;
        for (uint32_t pass = 0; pass < player->module.layout->passes && flow == MdFlow_Continue; pass++) {
            flow = step_voices(player);
            if (flow == MdFlow_Continue) {
                hardware(player);
            }
        }
        switch (flow) {
            case MdFlow_Continue:
                return;
            case MdFlow_Restart:
                init_song(player, player->subsong);
                player->looped = true;
                break;
            case MdFlow_Stop:
                clear_voices(player);
                player->ended = true;
                break;
            case MdFlow_Fault:
                player->fault = true;
                return;
        }
    }
    player->fault = true;
}

static void sync_voices(MdPlayer* player, NuadeVoices* voices) {
    if (voices == nullptr) {
        return;
    }
    for (uint32_t v = 0; v < MD_VOICES; v++) {
        nuade_paula_channel_sync(&player->voices[v].paula, v, voices);
    }
}

bool md_start(MdPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (subsong >= player->module.subsong_count) {
        return false;
    }
    for (uint32_t v = 0; v < MD_VOICES; v++) {
        player->voices[v] = (MdVoice) { 0 };
        if (voices != nullptr) {
            nuade_voice_stop(voices, v);
        }
    }
    player->fault = false;
    player->looped = false;
    player->ended = false;
    init_song(player, subsong);
    sync_voices(player, voices);
    return !player->fault;
}

MdTickResult md_tick(MdPlayer* player, NuadeVoices* voices) {
    run_tick(player);
    sync_voices(player, voices);
    // a song that left the modelled data stops
    return (MdTickResult) { .looped = player->looped, .ended = player->ended || player->fault };
}

// Plays the subsongs without a mixer until each has looped twice, ended, or run for a long time: everything they
// reach must stay inside what the port models.

static bool dry_run(MdPlayer* player) {
    uint32_t total = 0;
    player->commands_left = MD_DRY_RUN_COMMANDS;
    for (uint32_t subsong = 0; subsong < player->module.subsong_count && total < MD_DRY_RUN_TICKS; subsong++) {
        if (!md_start(player, subsong, nullptr)) {
            return false;
        }
        uint32_t loops = 0;
        for (uint32_t tick = 0; tick < MD_DRY_RUN_SUBSONG_TICKS && loops < MD_DRY_RUN_LOOPS; tick++) {
            run_tick(player);
            total++;
            if (player->fault) {
                return false;
            }
            loops += player->looped ? 1u : 0u;
            if (player->ended) {
                break;
            }
        }
    }
    player->commands_left = UINT64_MAX;
    return true;
}

MdPlayer* md_create(const uint8_t* data, size_t size) {
    if (size > UINT32_MAX / 2) {
        return nullptr;
    }
    MdPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    player->chip = nuade_paula_padded_copy(data, size);
    if (player->chip == nullptr || !md_module_parse(&player->module, player->chip, (uint32_t)size)
        || !dry_run(player)) {
        md_destroy(player);
        return nullptr;
    }
    return player;
}

void md_destroy(MdPlayer* player) {
    if (player != nullptr) {
        free(player->chip);
        free(player);
    }
}

uint32_t md_subsong_count(const MdPlayer* player) {
    return player->module.subsong_count;
}

MdVoiceState md_voice_state(const MdPlayer* player, uint32_t voice) {
    if (voice >= MD_VOICES) {
        return (MdVoiceState) { .period = 0, .instrument = MD_NO_INSTRUMENT };
    }
    const MdVoice* v = &player->voices[voice];
    return (MdVoiceState) { .period = v->paula.period,
                            .instrument = v->command != 0 ? (int)(v->command & MD_INSTRUMENT_MASK) : MD_NO_INSTRUMENT };
}
