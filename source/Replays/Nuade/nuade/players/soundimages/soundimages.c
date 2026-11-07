// SPDX-License-Identifier: MIT
// Sound Images: the driver of the modules, the routine the eagleplayer's Interrupt calls once per tick (50 Hz).
//
// Four voices read sequences of bytes: a note (below $80) or a command. A voice's record holds its state: the sample,
// the envelope, the effects. Every tick the driver first starts the notes of the last tick (DMA on; the audio
// interrupt that the DMA start raises writes the sample's repeat registers), then runs the song change the header
// asks for, then each voice: it writes the voice's period and volume, and when the tempo counter is 0 it counts the
// note down and reads the sequence once the note is over; the effects run every tick. The song ends when it stops
// itself (the stop command, the fade out reaching 0), which the eagleplayer's Interrupt reports as SongEnd.
//
// The driver also answers requests for sound effects on the fourth voice (header $2) that only a game sets; the
// eagleplayer never does, so the port leaves them out.

#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include "si_internal.h"
#include <nuade/players/soundimages.h>
#include <stdlib.h>
#include <string.h>

// DeliTracker's default tick: the CIA timer at 50 Hz.
#define TICK_LENGTH 14187

// The master volume a song starts with.
#define MASTER_VOLUME_FULL 0x10

// The word of the header that some versions force on the first voice's period every tick, and what the song start puts
// in it.
#define HEADER_WORD8 8
#define OVERRIDE_PERIOD 0x358

// The most bytes of a sequence one voice reads in one tick; more means the song loops without a note.
#define MAX_READ_STEPS 4096

// The period table index of a note is the note plus the voice's transposition, doubled, kept to the table of 64 words.
#define PERIOD_INDEX_MASK 0x7E

// The command byte that ends a step (the fifth entry of the command table): when it is the next byte, the release
// does not start, and when it is the last, the vibrato has no delay.
#define COMMAND_END_NOTE 0x85

// The first duration byte: a duration byte sets the note length to the byte minus this.
#define DURATION_BASE 0xDF

// A command byte indexes the command table with its low 7 bits.
#define COMMAND_INDEX_MASK 0x7F

typedef enum Flow {
    Flow_Read,       // read the next byte of the sequence
    Flow_EndCommand, // the list is at its end: run the module's end command
    Flow_EndNote,    // the step is over: the note length and the sequence pointer are stored
    Flow_Return,     // the voice's routine returns without a store
} Flow;

// Reads of the sequences

static bool fetch8(SiPlayer* player, int64_t* cursor, uint8_t* value) {
    if (!si_module_u8(&player->module, *cursor, value)) {
        player->broken = true;
        return false;
    }
    (*cursor)++;
    return true;
}

static bool fetch16(SiPlayer* player, int64_t cursor, uint16_t* value) {
    if (!si_module_u16(&player->module, cursor, value)) {
        player->broken = true;
        return false;
    }
    return true;
}

static bool read_bytes(SiPlayer* player, int64_t* cursor, uint8_t* out, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (!fetch8(player, cursor, &out[i])) {
            return false;
        }
    }
    return true;
}

// Paula

// Paula ignores bit 0 of AUDxLC.
static const int8_t* sample_pointer(const SiPlayer* player, uint32_t offset) {
    if (offset == 0) {
        return nullptr;
    }
    return (const int8_t*)(player->module.memory + (offset & ~1u));
}

// AUDxLC and AUDxLEN with the voice's sample.

static void write_sample(const SiPlayer* player, SiVoice* voice) {
    voice->paula.lc = sample_pointer(player, voice->sample);
    voice->paula.len_words = voice->length_words;
}

// AUDxLC and AUDxLEN with the sample's repeat: what the audio interrupt writes.

static void write_repeat(const SiPlayer* player, SiVoice* voice) {
    voice->paula.lc = sample_pointer(player, voice->loop);
    voice->paula.len_words = voice->loop_length_words;
}

// The period of a note

static void set_period(SiPlayer* player, SiVoice* voice, uint8_t note) {
    const uint8_t transposed = (uint8_t)(note + voice->transpose);
    const uint8_t doubled = (uint8_t)(transposed + transposed);
    uint16_t period = 0;
    if (!fetch16(player, (int64_t)player->module.layout.period_table + (doubled & PERIOD_INDEX_MASK), &period)) {
        return;
    }
    voice->period = period;
    if (voice->bend) {
        voice->period = (uint16_t)(voice->period + voice->bend_offset);
        voice->slide_left = voice->bend_ticks;
    }
    if (voice->slide_on_release) {
        voice->slide = false;
    }
}

// The stop command and the fade out reaching 0 end the song: all voices off, the header cleared.

static void stop_song(SiPlayer* player) {
    for (uint32_t i = 0; i < SI_VOICES; i++) {
        nuade_paula_channel_dma_off(&player->voices[i].paula);
        player->voices[i].active = false;
    }
    player->state.request = 0;
    player->state.song = 0;
    player->state.latched = 0;
    player->state.fade_speed = 0;
    player->state.fade_counter = 0;
    player->state.master_volume = MASTER_VOLUME_FULL;
}

// A note: the period, and for a voice that is not holding its sample the registers; the DMA goes off now and on at the
// next tick.

static void start_note(SiPlayer* player, SiVoice* voice, uint8_t note) {
    voice->note = note;
    set_period(player, voice, note);
    voice->shown_period = voice->period;
    if (!voice->hold) {
        if (voice->sample == 0) {
            player->broken = true;
            return;
        }
        write_sample(player, voice);
        voice->start_pending = true;
        nuade_paula_channel_dma_off(&voice->paula);
        voice->volume = voice->volume_base;
        voice->attack_left = voice->attack_count;
        voice->decay_left = voice->decay_count;
        voice->release_on = false;
    }
    voice->vibrato_left = voice->vibrato_rate;
}

// The offsets of the lists and sequences are signed words from the song table.

static int64_t song_offset(const SiPlayer* player, uint16_t word) {
    return (int64_t)player->module.layout.song_table + (int16_t)word;
}

static bool any_list_alive(const SiPlayer* player) {
    for (uint32_t i = 0; i < SI_VOICES; i++) {
        if (player->voices[i].list_alive) {
            return true;
        }
    }
    return false;
}

// The next sequence of the voice's list. At the end of the list the module's end command runs, with the cursor behind
// the zero that ends the list.

static Flow next_pattern(SiPlayer* player, SiVoice* voice, int64_t* cursor) {
    player->state.latched = player->state.request;
    voice->arpeggio = false;
    uint16_t word = 0;
    if (!fetch16(player, voice->list, &word)) {
        return Flow_Return;
    }
    if (word == 0) {
        // The eagleplayer's hook on this read: a voice at the end of its list leaves the mask, and with the mask empty
        // every such read is the SongEnd.
        voice->list_alive = false;
        if (!any_list_alive(player)) {
            player->looped = true;
        }
        *cursor = voice->list + 2;
        return Flow_EndCommand;
    }
    voice->list += 2;
    voice->sequence = song_offset(player, word);
    *cursor = voice->sequence;
    return Flow_Read;
}

// The slide commands read a step and the number of ticks it lasts.

static Flow slide_arguments(SiPlayer* player, SiVoice* voice, int64_t* cursor) {
    uint8_t b[3];
    if (!read_bytes(player, cursor, b, 3)) {
        return Flow_Return;
    }
    voice->slide_step = (uint16_t)((b[1] << 8) | b[0]);
    voice->slide_left = b[2];
    return Flow_Read;
}

static Flow run_command(SiPlayer* player, SiVoice* voice, const SiTableEntry* entry, int64_t* cursor) {
    uint8_t b[5];
    switch (entry->command) {
        case SiCommand_NextPattern:
            return next_pattern(player, voice, cursor);
        case SiCommand_NoteStepDown:
            voice->note_step = 0xFF;
            return Flow_Read;
        case SiCommand_NoteStepUp:
            voice->note_step = 1;
            return Flow_Read;
        case SiCommand_JumpList: {
            uint16_t word = 0;
            if (!fetch16(player, *cursor, &word)) {
                return Flow_Return;
            }
            voice->list = song_offset(player, word);
            return next_pattern(player, voice, cursor);
        }
        case SiCommand_Stop:
            stop_song(player);
            return Flow_Return;
        case SiCommand_Release:
            if (!read_bytes(player, cursor, b, 2)) {
                return Flow_Return;
            }
            voice->release_start = b[0];
            voice->release_wait = b[1] >> 4;
            voice->release_rate = b[1] >> 4;
            voice->release_step = b[1] & 0x0F;
            return Flow_Read;
        case SiCommand_EndNote:
            return Flow_EndNote;
        case SiCommand_Silence:
            voice->volume = 0;
            return Flow_EndNote;
        case SiCommand_Transpose:
            if (!fetch8(player, cursor, &voice->transpose)) {
                return Flow_Return;
            }
            return Flow_Read;
        case SiCommand_Vibrato:
            if (!read_bytes(player, cursor, b, 3)) {
                return Flow_Return;
            }
            voice->vibrato_delay = b[0];
            voice->vibrato_depth = b[1];
            voice->vibrato_rate = b[2];
            return Flow_Read;
        case SiCommand_Bend:
            if (!read_bytes(player, cursor, b, 5)) {
                return Flow_Return;
            }
            voice->bend_offset = (uint16_t)((b[1] << 8) | b[0]);
            voice->slide_step = (uint16_t)((b[3] << 8) | b[2]);
            voice->bend_ticks = b[4];
            voice->bend = true;
            return Flow_Read;
        case SiCommand_BendOff:
            voice->bend = false;
            return Flow_Read;
        case SiCommand_SlideOn:
            voice->slide = true;
            return slide_arguments(player, voice, cursor);
        case SiCommand_SlideOnRelease:
            voice->slide_on_release = true;
            return slide_arguments(player, voice, cursor);
        case SiCommand_SlideOnReleaseOff:
            voice->slide_on_release = false;
            return Flow_Read;
        case SiCommand_HoldOn:
            voice->hold = true;
            return Flow_Read;
        case SiCommand_HoldOff:
            voice->hold = false;
            return Flow_Read;
        case SiCommand_ArpeggioOff:
            voice->arpeggio = false;
            return Flow_Read;
        case SiCommand_VolumeDown:
            if (voice->volume_base != 0) {
                voice->volume_base--;
            }
            return Flow_Read;
        case SiCommand_VolumeUp:
            if (voice->volume_base < 0x0F) {
                voice->volume_base++;
            }
            return Flow_Read;
        case SiCommand_Fade:
            player->state.fade_speed = entry->argument;
            player->state.fade_counter = entry->argument;
            return Flow_Read;
        case SiCommand_VoiceOff:
            nuade_paula_channel_dma_off(&player->voices[SI_VOICES - 1].paula);
            return Flow_Return;
        case SiCommand_VoiceOffOwn:
        case SiCommand_VoiceOffOwnSilent:
            voice->volume = 0;
            if (entry->command == SiCommand_VoiceOffOwnSilent) {
                voice->paula.volume = 0;
            }
            nuade_paula_channel_dma_off(&voice->paula);
            voice->active = false;
            return Flow_Return;
    }
    player->broken = true;
    return Flow_Return;
}

// A sample command: the sample table entry becomes the voice's sample and repeat.

static Flow sample_command(SiPlayer* player, SiVoice* voice, uint32_t index) {
    if (index >= player->module.layout.sample_count) {
        player->broken = true;
        return Flow_Return;
    }
    const SiSample sample = si_module_sample(&player->module, index);
    voice->sample = (uint32_t)sample.offset;
    voice->length_words = sample.length_words;
    voice->loop = (uint32_t)sample.loop_offset;
    voice->loop_length_words = sample.loop_length_words;
    voice->shown_instrument = (int)index;
    return Flow_Read;
}

// The envelope command: the volume and the three nibble pairs of the attack, the decay and the wait of each.

static Flow envelope_command(SiPlayer* player, SiVoice* voice, uint8_t volume, int64_t* cursor) {
    uint8_t b[3];
    if (!read_bytes(player, cursor, b, 3)) {
        return Flow_Return;
    }
    voice->volume_base = volume;
    voice->attack_step = b[0] >> 4;
    voice->attack_count = b[0] & 0x0F;
    voice->decay_step = b[1] >> 4;
    voice->decay_count = b[1] & 0x0F;
    voice->attack_wait = b[2] >> 4;
    voice->attack_rate = b[2] >> 4;
    voice->decay_wait = b[2] & 0x0F;
    voice->decay_rate = b[2] & 0x0F;
    return Flow_Read;
}

// The arpeggio command: the pattern is at a signed word offset from the arpeggio table.

static Flow arpeggio_command(SiPlayer* player, SiVoice* voice, uint32_t index) {
    uint16_t word = 0;
    if (!fetch16(player, (int64_t)player->module.layout.arpeggio_table + 2 * (int64_t)index, &word)) {
        return Flow_Return;
    }
    voice->arpeggio_pattern = (int64_t)player->module.layout.arpeggio_table + (int16_t)word;
    voice->arpeggio_index = 0;
    voice->arpeggio = true;
    return Flow_Read;
}

// A command byte: the classes by range, the rest through the module's command table.

static Flow dispatch(SiPlayer* player, SiVoice* voice, uint8_t byte, int64_t* cursor) {
    const SiImage* image = player->module.image;
    if (byte >= image->duration_class) {
        voice->note_length = (uint8_t)(byte - DURATION_BASE);
        return Flow_Read;
    }
    if (byte >= image->sample_class) {
        return sample_command(player, voice, (uint32_t)(byte - image->sample_class));
    }
    if (byte >= image->envelope_class) {
        return envelope_command(player, voice, (uint8_t)(byte - image->envelope_class), cursor);
    }
    if (byte >= image->volume_class) {
        voice->volume_base = (uint8_t)(byte - image->volume_class);
        return Flow_Read;
    }
    if (byte >= image->arpeggio_class) {
        return arpeggio_command(player, voice, (uint32_t)(byte - image->arpeggio_class));
    }
    const uint32_t index = byte & COMMAND_INDEX_MASK;
    if (index >= image->command_count) {
        // The original reads its code as the table.
        player->broken = true;
        return Flow_Return;
    }
    return run_command(player, voice, &image->commands[index], cursor);
}

// The sequence: notes and commands until the step ends.

static void read_sequence(SiPlayer* player, SiVoice* voice) {
    int64_t cursor = voice->sequence;
    uint8_t byte = 0;
    bool end_command = false; // the byte is the module's end command, which runs as a command whatever its value
    for (uint32_t step = 0; step < MAX_READ_STEPS; step++) {
        if (!end_command && !fetch8(player, &cursor, &byte)) {
            return;
        }
        Flow flow = Flow_EndNote;
        if (!end_command && (byte & 0x80) == 0) {
            start_note(player, voice, byte);
            if (player->broken) {
                return;
            }
        } else {
            end_command = false;
            flow = dispatch(player, voice, byte, &cursor);
        }
        switch (flow) {
            case Flow_Read:
                break;
            case Flow_EndCommand:
                byte = player->module.layout.end_command;
                end_command = true;
                break;
            case Flow_EndNote:
                voice->duration = voice->note_length;
                voice->sequence = cursor;
                return;
            case Flow_Return:
                return;
        }
        if (player->broken) {
            return;
        }
    }
    player->broken = true;
}

// The effects of a tick

static void lower_volume(SiVoice* voice, uint8_t step) {
    voice->volume = voice->volume < step ? 0 : (uint8_t)(voice->volume - step);
}

// The volume: attack, then decay, then release once it has started.

static void step_envelope(SiVoice* voice) {
    if (voice->attack_left != 0) {
        if (--voice->attack_wait != 0) {
            return;
        }
        voice->attack_wait = voice->attack_rate;
        voice->attack_left--;
        voice->volume = (uint8_t)(voice->volume + voice->attack_step);
        return;
    }
    if (voice->decay_left != 0) {
        if (--voice->decay_wait != 0) {
            return;
        }
        voice->decay_wait = voice->decay_rate;
        voice->decay_left--;
        lower_volume(voice, voice->decay_step);
        return;
    }
    if (voice->release_on) {
        if (--voice->release_wait != 0) {
            return;
        }
        voice->release_wait = voice->release_rate;
        lower_volume(voice, voice->release_step);
    }
}

// The slide of the period: the step's top bit is its direction.

static void step_slide(SiVoice* voice) {
    if (voice->slide_left == 0) {
        return;
    }
    voice->slide_left--;
    if ((voice->slide_step & 0x8000) != 0) {
        voice->period = (uint16_t)(voice->period - (voice->slide_step & 0x7FFF));
    } else {
        voice->period = (uint16_t)(voice->period + voice->slide_step);
    }
}

// The arpeggio: the pattern's next note, added to the voice's note; a byte with the top bit set ends the pattern.

static void step_arpeggio(SiPlayer* player, SiVoice* voice) {
    if (!voice->arpeggio) {
        return;
    }
    uint8_t entry = 0;
    if (!si_module_u8(&player->module, voice->arpeggio_pattern + voice->arpeggio_index, &entry)) {
        player->broken = true;
        return;
    }
    voice->arpeggio_index++;
    if ((entry & 0x80) != 0) {
        voice->arpeggio_index = 0;
        if (player->module.image->arpeggio_end_masked) {
            entry &= 0x7F;
        }
    }
    set_period(player, voice, (uint8_t)(entry + voice->note));
}

// The vibrato: after its delay the period moves by the depth, up and down every rate ticks.

static void step_vibrato(SiPlayer* player, SiVoice* voice) {
    uint8_t previous = 0;
    if (!si_module_u8(&player->module, voice->sequence - 1, &previous)) {
        player->broken = true;
        return;
    }
    if (previous != COMMAND_END_NOTE) {
        const uint8_t elapsed = (uint8_t)(voice->note_length - voice->duration);
        if (elapsed < voice->vibrato_delay) {
            return;
        }
    }
    if (voice->vibrato_depth == 0) {
        return;
    }
    if (voice->vibrato_up) {
        voice->period = (uint16_t)(voice->period + voice->vibrato_depth);
    } else {
        voice->period = (uint16_t)(voice->period - voice->vibrato_depth);
    }
    if (--voice->vibrato_left == 0) {
        voice->vibrato_left = (uint8_t)(voice->vibrato_rate * 2);
        voice->vibrato_up = !voice->vibrato_up;
    }
}

// The note slides a semitone a tempo tick when the voice has a note step.

static void step_note(SiPlayer* player, SiVoice* voice) {
    if (voice->note_step == 0) {
        return;
    }
    voice->note = (uint8_t)(voice->note + voice->note_step);
    set_period(player, voice, voice->note);
    write_sample(player, voice);
}

// The release starts when the note has release_start ticks left, unless the sequence goes on with an end of step.

static void step_release(SiPlayer* player, SiVoice* voice) {
    if (voice->release_start != voice->duration) {
        return;
    }
    uint8_t next = 0;
    if (!si_module_u8(&player->module, voice->sequence, &next)) {
        player->broken = true;
        return;
    }
    if (next == COMMAND_END_NOTE) {
        return;
    }
    voice->release_on = true;
    voice->note_step = 0;
    voice->slide = false;
    if (voice->slide_on_release) {
        voice->slide = true;
    }
}

// A voice's routine of a tick

static void run_voice(SiPlayer* player, SiVoice* voice) {
    if (player->module.image->voice_flags ? !voice->active : player->state.song == 0) {
        return;
    }
    voice->paula.period = voice->period;
    // The volume table is indexed with the volume scaled by the master volume.
    const uint32_t index
        = (((uint32_t)(voice->volume_high << 8 | voice->volume) * player->state.master_volume) & 0xFFFF) >> 4;
    uint16_t level = 0;
    if (!fetch16(player, (int64_t)player->module.layout.volume_table + 2 * (int64_t)index, &level)) {
        return;
    }
    voice->paula.volume = (uint8_t)level;
    if (player->state.tempo_counter == 0) {
        voice->duration--;
        if (voice->duration == 0) {
            read_sequence(player, voice);
            return;
        }
        step_note(player, voice);
        step_release(player, voice);
    }
    step_envelope(voice);
    if (voice->slide) {
        step_slide(voice);
    }
    if (voice->bend) {
        step_slide(voice);
    }
    step_arpeggio(player, voice);
    step_vibrato(player, voice);
}

static void start_requested_song(SiPlayer* player) {
    SiState* state = &player->state;
    const SiImage* image = player->module.image;
    if (image->song_start_needs_word && state->word8 == 0) {
        return;
    }
    if (state->song == 0) {
        state->latched = state->request;
    }
    const uint8_t song = state->latched;
    if (song == 0) {
        return;
    }
    if (song > player->module.layout.song_count) {
        stop_song(player);
        return;
    }
    state->song = song;
    if (image->song_start_stops_dma) {
        for (uint32_t i = 0; i < SI_VOICES; i++) {
            nuade_paula_channel_dma_off(&player->voices[i].paula);
        }
    }
    state->tempo_counter = 1;
    state->fade_speed = 0;
    state->request = 0;
    state->latched = 0;
    if (image->prologue == SiPrologue_PeriodOverrideSong1
        || image->prologue == SiPrologue_PeriodOverrideNotSongs2And4) {
        state->word8 = OVERRIDE_PERIOD;
    }
    SiSong entry;
    if (!si_module_song(&player->module, (uint32_t)(song - 1), &entry)) {
        player->broken = true;
        return;
    }
    state->tempo = entry.tempo;
    for (uint32_t i = 0; i < SI_VOICES; i++) {
        SiVoice* voice = &player->voices[i];
        voice->list_alive = entry.list[i] != 0;
        if (image->voice_flags && entry.list[i] == 0) {
            voice->active = voice->active && !image->song_start_clears_voices;
            continue;
        }
        // The voice's list starts with the offset of its first sequence.
        const int64_t list = song_offset(player, entry.list[i]);
        uint16_t first = 0;
        if (!fetch16(player, list, &first)) {
            return;
        }
        voice->list = list + 2;
        voice->sequence = song_offset(player, first);
        voice->arpeggio = false;
        voice->slide = false;
        voice->bend = false;
        voice->vibrato_up = false;
        voice->hold = false;
        voice->slide_on_release = false;
        voice->vibrato_depth = 0;
        voice->transpose = 0;
        voice->duration = 1;
        voice->active = image->voice_flags;
    }
    state->master_volume = MASTER_VOLUME_FULL;
}

// The notes started in the last tick: the DMA comes on. The DMA start raises the voice's audio interrupt (its enable
// came with the DMACON word); the handler writes the voice's repeat registers at once and disables the interrupt again,
// so a note gets it once.

static void start_notes(SiPlayer* player) {
    for (uint32_t i = 0; i < SI_VOICES; i++) {
        SiVoice* voice = &player->voices[i];
        if (voice->start_pending) {
            voice->start_pending = false;
            nuade_paula_channel_dma_on(&voice->paula);
            write_repeat(player, voice);
        }
    }
}

// The fade out counts the master volume down to 0, where the song stops.

static void run_fade(SiPlayer* player) {
    SiState* state = &player->state;
    if (state->fade_speed == 0) {
        return;
    }
    if (--state->fade_counter != 0) {
        return;
    }
    state->fade_counter = state->fade_speed;
    if (--state->master_volume == 0) {
        stop_song(player);
    }
}

// What some versions of the tick do before the rest.

static void apply_prologue(SiPlayer* player) {
    const SiImage* image = player->module.image;
    SiState* state = &player->state;
    switch (image->prologue) {
        case SiPrologue_None:
            break;
        case SiPrologue_PeriodOverrideSong1:
            if (state->song == 1) {
                player->voices[0].period = state->word8;
            }
            break;
        case SiPrologue_PeriodOverrideNotSongs2And4:
            if (state->song != 0 && state->song != 2 && state->song != 4) {
                player->voices[0].period = state->word8;
            }
            break;
    }
}

// The song's work of a tick: the voices, the fade, the tempo counter.

static void run_song(SiPlayer* player) {
    SiState* state = &player->state;
    if (state->song == 0 && !player->module.image->voice_flags) {
        return;
    }
    state->tempo_counter--;
    for (uint32_t i = 0; i < SI_VOICES && !player->broken; i++) {
        run_voice(player, &player->voices[i]);
    }
    run_fade(player);
    if (state->tempo_counter == 0) {
        state->tempo_counter = state->tempo;
    }
}

SiTickResult si_tick(SiPlayer* player, NuadeVoices* voices) {
    player->looped = false;
    apply_prologue(player);
    start_notes(player);
    start_requested_song(player);
    if (!player->broken) {
        run_song(player);
    }
    for (uint32_t i = 0; i < SI_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return (SiTickResult) { .length = TICK_LENGTH,
                            .looped = player->looped,
                            .ended = player->state.song == 0 || player->broken };
}

static void init_voice(const SiModule* module, uint32_t index, SiVoice* voice) {
    const SiRecord r = si_module_record(module, index);
    memset(voice, 0, sizeof(*voice));
    voice->length_words = r.length_words;
    voice->period = r.period;
    voice->volume_high = r.volume_high;
    voice->volume = r.volume;
    voice->duration = r.duration;
    voice->note = r.note;
    voice->note_step = r.note_step;
    voice->loop_length_words = r.loop_length_words;
    voice->volume_base = r.volume_base;
    voice->attack_step = r.attack_step;
    voice->attack_count = r.attack_count;
    voice->decay_step = r.decay_step;
    voice->decay_count = r.decay_count;
    voice->release_step = r.release_step;
    voice->release_start = r.release_start;
    voice->release_on = r.release_on != 0;
    voice->attack_left = r.attack_left;
    voice->decay_left = r.decay_left;
    voice->vibrato_depth = r.vibrato_depth;
    voice->vibrato_rate = r.vibrato_rate;
    voice->vibrato_left = r.vibrato_left;
    voice->slide_left = r.slide_left;
    voice->slide_step = r.slide_step;
    voice->bend_offset = r.bend_offset;
    voice->bend_ticks = r.bend_ticks;
    voice->note_length = r.note_length;
    voice->attack_wait = r.attack_wait;
    voice->attack_rate = r.attack_rate;
    voice->decay_wait = r.decay_wait;
    voice->decay_rate = r.decay_rate;
    voice->release_wait = r.release_wait;
    voice->release_rate = r.release_rate;
    voice->vibrato_delay = r.vibrato_delay;
    voice->arpeggio_index = r.arpeggio_index;
    voice->transpose = r.transpose;
    voice->active = module->image->voice_flags && r.active != 0;
    voice->list = -1; // the file holds addresses of the saved game: a voice that no song start set up has no data
    voice->sequence = -1;
    voice->shown_instrument = -1;
}

bool si_start(SiPlayer* player, uint32_t song, NuadeVoices* voices) {
    if (song >= player->module.layout.song_count) {
        return false;
    }
    memset(&player->state, 0, sizeof(player->state));
    // The eagleplayer's InitSound writes the song number into the header and clears the variables behind it.
    player->state.request = (uint8_t)(song + 1);
    player->state.word8 = nuade_read_be16(player->module.memory + SI_HEADER_OFFSET + HEADER_WORD8);
    player->broken = false;
    for (uint32_t i = 0; i < SI_VOICES; i++) {
        init_voice(&player->module, i, &player->voices[i]);
        player->voices[i].start_pending = player->module.layout.start_pending[i];
        nuade_voice_stop(voices, i);
    }
    return true;
}

bool si_probe(const uint8_t* data, size_t size) {
    SiModule module;
    if (!si_module_open(&module, data, size)) {
        return false;
    }
    si_module_close(&module);
    return true;
}

SiCreateResult si_create(const uint8_t* data, size_t size) {
    SiPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return (SiCreateResult) { .status = SiStatus_OutOfMemory };
    }
    if (!si_module_open(&player->module, data, size)) {
        free(player);
        return (SiCreateResult) { .status = SiStatus_InvalidData };
    }
    return (SiCreateResult) { .player = player, .status = SiStatus_Ok };
}

void si_destroy(SiPlayer* player) {
    if (player != nullptr) {
        si_module_close(&player->module);
        free(player);
    }
}

uint32_t si_song_count(const SiPlayer* player) {
    return player->module.layout.song_count;
}

const char* si_format_name(const SiPlayer* player) {
    (void)player;
    return "Sound Images";
}

SiVoiceState si_voice_state(const SiPlayer* player, uint32_t voice) {
    const SiVoice* v = &player->voices[voice];
    return (SiVoiceState) { .period = v->shown_period, .instrument = v->shown_instrument };
}
