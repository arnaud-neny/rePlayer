// SPDX-License-Identifier: MIT
// Silmarils player: the music player. The tick counts a wait down, then reads commands from the music stream until a
// wait byte, then switches on the DMA of the channels that got a note. The sample's loop and the end of a one-shot
// sample are set up by the channel's audio interrupt handler, which the original chains from the DMA start and the
// end of the block; the port runs the handler where the DMA starts.

#include "sil_internal.h"

#include "nuade_paula.h"
#include "nuade_tracker.h"

#define SIL_NOTES 128
#define SIL_MAX_WORDS 0xFFFF // longest AUDxLEN the port models (0 would play 65536 words)
#define SIL_MAX_PASSES 8     // passes of the stream the validation runs before it gives up
#define SIL_WAIT_BYTE 0x80   // a stream byte from here is a wait of (byte - 0x80) units
#define SIL_COMMAND_ESCAPE 0xFF
#define SIL_COMMAND_MASK 0x3F
#define SIL_COMMAND_CHANNEL_SHIFT 6
#define SIL_WAIT_SHIFT 8 // the wait counter holds units in its high byte
#define SIL_VOLUME_MASK 0x7F
#define SIL_TEMPO_MASK 0x7F
#define SIL_TEMPO_SHIFT 3
#define SIL_TEMPO_BIAS 7 // the rate of a module without note bases is 7 below tempo * 8

// Periods by note + the instrument's note base: the eagleplayer's table; past the last note it repeats the last value
static const uint16_t s_periods[SIL_NOTES] = {
    0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x1660,
    0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x1660, 0x151C, 0x13F0, 0x12D0, 0x11C0, 0x10C0, 0x0FD0,
    0x0EF0, 0x0E18, 0x0D4C, 0x0C8E, 0x0BDA, 0x0B30, 0x0A8F, 0x09F7, 0x0968, 0x08E1, 0x0861, 0x08B1, 0x0777, 0x070C,
    0x06A7, 0x0647, 0x05ED, 0x0598, 0x0547, 0x04FC, 0x04B4, 0x0470, 0x0430, 0x03F4, 0x03BC, 0x0386, 0x0353, 0x0324,
    0x02F6, 0x02CC, 0x02A4, 0x027E, 0x025A, 0x0238, 0x0218, 0x01FA, 0x01DE, 0x01C3, 0x01AA, 0x0192, 0x017B, 0x0166,
    0x0152, 0x013F, 0x012D, 0x011C, 0x010C, 0x00FD, 0x00EF, 0x00E2, 0x00D5, 0x00C9, 0x00BE, 0x00B3, 0x00A9, 0x009F,
    0x0096, 0x008E, 0x0086, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F,
    0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F,
    0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F,
};

// The header in front of a sample's data. A loop start of 0 means no loop; a loop end of 0 means the data ends where
// the block does.

static bool read_header(const SilMemory* memory, uint32_t sample, SilBlock* header) {
    return sample >= SIL_SAMPLE_HEADER && sil_block_read(memory, sample - SIL_SAMPLE_HEADER, header);
}

// What AUDxLC and AUDxLEN hold for `bytes` of data at `address`: AUDxLC ignores its lowest bit, AUDxLEN counts words.
// The port plays runs of 1 .. SIL_MAX_WORDS words that lie inside the memory.

typedef struct Run {
    uint32_t address;
    uint16_t words;
} Run;

static bool make_run(const SilMemory* memory, uint64_t address, uint32_t bytes, Run* run) {
    const uint32_t words = bytes >> 1;
    const uint64_t even = address & ~(uint64_t)1;
    if (words < 1 || words > SIL_MAX_WORDS || !sil_in_memory(memory, even, 2u * words)) {
        return false;
    }
    *run = (Run) { .address = (uint32_t)even, .words = (uint16_t)words };
    return true;
}

static uint32_t first_end(const SilBlock* header) {
    return header->loop_end != 0 ? header->loop_end : header->size - SIL_SAMPLE_HEADER;
}

// The first block of a note: from the start of the data to the loop end (or the end of the data).

static bool first_run(const SilMemory* memory, uint32_t sample, const SilBlock* header, Run* run) {
    return make_run(memory, sample, first_end(header), run);
}

// The repeat of a sample with a loop: from the loop start to the loop end.

static bool loop_run(const SilMemory* memory, uint32_t sample, const SilBlock* header, Run* run) {
    const uint32_t end = first_end(header);
    return end > header->loop_start
           && make_run(memory, (uint64_t)sample + header->loop_start, end - header->loop_start, run);
}

static void set_registers(const SilMemory* memory, NuadePaulaChannel* paula, const Run* run) {
    paula->lc = (const int8_t*)(memory->data + run->address);
    paula->len_words = run->words;
}

// DMACON switches the channel on, and the audio interrupt of the DMA start runs the handler.

static void start_dma(const SilMemory* memory, SilChannel* c) {
    nuade_paula_channel_dma_on(&c->paula);
    c->one_shot = false;
    if (!c->interrupt_enabled) {
        return;
    }
    switch (c->handler) {
        case SilHandler_Loop: {
            SilBlock header;
            Run run;
            if (read_header(memory, c->handler_sample, &header) && loop_run(memory, c->handler_sample, &header, &run)) {
                set_registers(memory, &c->paula, &run);
            }
            c->interrupt_enabled = false;
            break;
        }
        case SilHandler_OneShot:
            c->handler = SilHandler_Stop;
            c->one_shot = true;
            break;
        case SilHandler_None:
        case SilHandler_Stop:
            break;
    }
}

static bool select_instrument(SilMusic* music, SilChannel* c, uint8_t instrument) {
    if (instrument >= SIL_SLOTS) {
        return false;
    }
    c->sample = music->slots[instrument].sample;
    c->note_base = music->slots[instrument].note_base;
    c->instrument = instrument;
    return true;
}

// A note command stops the channel; with a volume it also loads the registers and asks for the DMA start at the end of
// the tick.

static bool play_note(SilMusic* music, SilChannel* c, uint8_t note, uint8_t volume) {
    c->interrupt_enabled = false;
    nuade_paula_channel_dma_off(&c->paula);
    if (volume == 0) {
        return true;
    }
    SilBlock header;
    Run first;
    Run loop;
    const uint16_t index = (uint16_t)(note + c->note_base);
    if (c->sample == 0 || !read_header(music->memory, c->sample, &header)
        || !first_run(music->memory, c->sample, &header, &first)
        || (header.loop_start != 0 && !loop_run(music->memory, c->sample, &header, &loop)) || index >= SIL_NOTES) {
        return false;
    }
    set_registers(music->memory, &c->paula, &first);
    c->paula.period = s_periods[index];
    c->paula.volume = volume & SIL_VOLUME_MASK;
    c->handler = header.loop_start != 0 ? SilHandler_Loop : SilHandler_OneShot;
    c->handler_sample = c->sample;
    c->chained_tail = false;
    c->want_dma = true;
    c->want_interrupt = true;
    c->note = nuade_period_to_note(c->paula.period);
    return true;
}

// What the channel's voice does now. The validation pass has no voices and gets a voice that runs and has not wrapped.

static NuadeVoiceState channel_voice(const SilMusic* music, const SilChannel* c) {
    NuadeVoiceState state = { .playing = true };
    if (music->voices != nullptr) {
        nuade_voice_get_state(music->voices, (uint32_t)(c - music->channels), &state);
    }
    return state;
}

// Whether the channel's DMA still runs: the voice plays and is not a one-shot past its end.

static bool channel_running(const SilMusic* music, const SilChannel* c) {
    const NuadeVoiceState state = channel_voice(music, c);
    const bool past_end
        = state.sample != nullptr && state.sample->loop_length == 0 && state.position >= (double)state.sample->length;
    return c->paula.dma && state.playing && !past_end;
}

// The tail command plays the part of the sample after its loop end, once. The command switches DMACON off and on again
// with no wait in between, so a channel that still runs does not notice and takes the tail as its next block, after
// the block that runs now; a channel that has stopped starts again. A sample without a tail is just stopped.
//
// The channel's interrupt request is set at every block end, and the handler of a looping sample has switched the
// enable off, so after the first block end a request is waiting. Enabling the interrupt runs the handler at once: it
// arms the stop, and the stop runs at the next block end, which is the start of the tail.

static bool play_tail(SilMusic* music, SilChannel* c) {
    SilBlock header;
    if (c->sample == 0 || c->want_dma || c->chained_tail || !read_header(music->memory, c->sample, &header)) {
        return false;
    }
    c->interrupt_enabled = false;
    const int32_t tail = (int32_t)(header.size - SIL_SAMPLE_HEADER - header.loop_end);
    if (header.loop_end == 0 || tail <= 0 || ((uint32_t)tail >> 1) == 0) {
        nuade_paula_channel_dma_off(&c->paula);
        return true;
    }
    Run run;
    if (!make_run(music->memory, (uint64_t)c->sample + header.loop_end, (uint32_t)tail, &run)) {
        return false;
    }
    const bool running = channel_running(music, c);
    if (!running) {
        nuade_paula_channel_dma_off(&c->paula);
    }
    set_registers(music->memory, &c->paula, &run);
    c->interrupt_enabled = true;
    if (!running) {
        c->handler = SilHandler_OneShot;
        start_dma(music->memory, c);
        return true;
    }
    const NuadeVoiceState state = channel_voice(music, c);
    c->handler = SilHandler_Stop;
    c->chained_tail = true;
    c->stop_at_switch = state.wraps > c->wraps_at_start;
    c->switch_wraps = state.wraps + 1;
    return true;
}

// The stop that the handler runs at the start of a tail: the voice has switched to the tail.

static void run_stops(SilMusic* music) {
    for (uint32_t i = 0; i < SIL_USED_VOICES; i++) {
        SilChannel* c = &music->channels[i];
        if (c->stop_at_switch && channel_voice(music, c).wraps >= c->switch_wraps) {
            c->stop_at_switch = false;
            c->interrupt_enabled = false;
            nuade_paula_channel_dma_off(&c->paula);
        }
    }
}

static bool read_byte(SilMusic* music, uint8_t* value) {
    if (!sil_in_memory(music->memory, music->position, 1)) {
        return false;
    }
    *value = music->memory->data[music->position++];
    return true;
}

// The command at the stream position: its arguments follow it. False when the port does not model it.

static bool run_command(SilMusic* music) {
    if (music->command_channel >= SIL_USED_VOICES) {
        return false;
    }
    SilChannel* c = &music->channels[music->command_channel];
    uint8_t first = 0;
    uint8_t second = 0;
    switch (music->command) {
        case SilCommand_Note:
            return read_byte(music, &first) && read_byte(music, &second) && play_note(music, c, first, second);
        case SilCommand_Select:
            return read_byte(music, &first) && select_instrument(music, c, first);
        case SilCommand_Tail:
            return play_tail(music, c);
        case SilCommand_End:
        default:
            return false;
    }
}

// The command a stream byte introduces: the kind in the low six bits, the channel in the top two; 0xFF is the end.

static void decode_command(SilMusic* music, uint8_t byte) {
    const bool end = byte == SIL_COMMAND_ESCAPE;
    music->command = (SilCommand)(end ? SilCommand_End : byte & SIL_COMMAND_MASK);
    music->command_channel = end ? 0 : byte >> SIL_COMMAND_CHANNEL_SHIFT;
}

// Reads commands until a wait byte or the end command. A byte below 0x80 repeats the last command with new arguments;
// 0xFF introduces a command (0xFF as the command is the end command).

static SilBatch run_batch(SilMusic* music) {
    for (;;) {
        uint8_t byte = 0;
        if (!read_byte(music, &byte)) {
            return SilBatch_Error;
        }
        if (byte >= SIL_WAIT_BYTE && byte != SIL_COMMAND_ESCAPE) {
            music->wait = (uint16_t)((byte - SIL_WAIT_BYTE) << SIL_WAIT_SHIFT);
            return SilBatch_Wait;
        }
        bool repeats = false;
        if (byte == SIL_COMMAND_ESCAPE) {
            uint8_t command = 0;
            if (!read_byte(music, &command)) {
                return SilBatch_Error;
            }
            decode_command(music, command);
        } else {
            music->position--;
            repeats = true;
        }
        const uint32_t arguments_at = music->position;
        if (music->command == SilCommand_End) {
            music->command_channel = 0;
            return SilBatch_End;
        }
        // A repeated command that takes no argument would read the same byte again for ever
        if (!run_command(music) || (repeats && music->position == arguments_at)) {
            return SilBatch_Error;
        }
    }
}

// The end of the tick: the interrupt enables go out first, then DMACON, and the audio interrupt of each DMA start runs.

static void commit(SilMusic* music) {
    for (uint32_t i = 0; i < SIL_USED_VOICES; i++) {
        SilChannel* c = &music->channels[i];
        if (c->want_interrupt) {
            c->interrupt_enabled = true;
            c->want_interrupt = false;
        }
    }
    for (uint32_t i = 0; i < SIL_USED_VOICES; i++) {
        SilChannel* c = &music->channels[i];
        if (c->want_dma) {
            c->want_dma = false;
            start_dma(music->memory, c);
        }
    }
}

void sil_music_start(SilMusic* music, SilMemory* memory, const SilSlot* slots, uint32_t stream, uint8_t tempo,
                     bool slot_parameters) {
    *music = (SilMusic) { .memory = memory, .slots = slots, .start = stream, .position = stream };
    music->rate = (uint16_t)((tempo & SIL_TEMPO_MASK) << SIL_TEMPO_SHIFT);
    if (!slot_parameters) {
        music->rate = (uint16_t)(music->rate - SIL_TEMPO_BIAS);
    }
    for (uint32_t i = 0; i < SIL_USED_VOICES; i++) {
        music->channels[i].instrument = -1;
        music->channels[i].note = -1;
    }
}

bool sil_music_tick(SilMusic* music, const NuadeVoices* voices, bool* looped) {
    *looped = false;
    music->voices = voices;
    run_stops(music);
    if (music->wait > music->rate) {
        music->wait = (uint16_t)(music->wait - music->rate);
    } else {
        music->wait = 0;
        SilBatch batch = run_batch(music);
        if (batch == SilBatch_End) {
            *looped = true;
            music->position = music->start;
            batch = run_batch(music); // a stream that ends without a wait would never leave the tick
        }
        if (batch != SilBatch_Wait) {
            return false;
        }
    }
    commit(music);
    return true;
}

// What carries over from one pass of the stream to the next. Built zeroed, so that two states compare with memcmp.
typedef struct PassChannel {
    uint32_t sample;
    uint16_t note_base;
    bool want_dma;
    bool want_interrupt;
    bool chained_tail;
    bool dma;
    SilHandler handler;
    uint32_t handler_sample;
    bool interrupt_enabled;
    bool one_shot;
} PassChannel;

typedef struct PassState {
    SilCommand command;
    uint8_t command_channel;
    PassChannel channels[SIL_USED_VOICES];
} PassState;

static PassState pass_state(const SilMusic* music) {
    PassState state;
    memset(&state, 0, sizeof(state));
    state.command = music->command;
    state.command_channel = music->command_channel;
    for (uint32_t i = 0; i < SIL_USED_VOICES; i++) {
        const SilChannel* c = &music->channels[i];
        state.channels[i] = (PassChannel) { .sample = c->sample,
                                            .note_base = c->note_base,
                                            .want_dma = c->want_dma,
                                            .want_interrupt = c->want_interrupt,
                                            .chained_tail = c->chained_tail,
                                            .dma = c->paula.dma,
                                            .handler = c->handler,
                                            .handler_sample = c->handler_sample,
                                            .interrupt_enabled = c->interrupt_enabled,
                                            .one_shot = c->one_shot };
    }
    return state;
}

// The stream is read from its start to its end command, again and again, and what carries over from pass to pass is the
// state above. When a pass starts with a state that an earlier pass started with, every later pass repeats that one, so
// the passes run so far have seen everything the music will do. The validation's voice always runs, so the runtime's
// `dma` and `chained_tail` can only be a subset of the validation's (a tail chains only while the voice runs, and
// `run_stops` only switches DMA off); every other refusal depends on the data, the selected sample and the pending DMA
// request alone.

bool sil_music_validate(SilMusic* music, uint32_t* budget) {
    PassState seen[SIL_MAX_PASSES + 1];
    uint32_t count = 0;
    seen[count++] = pass_state(music);
    bool waited = false;
    uint32_t batch_start = music->position;
    for (;;) {
        const SilBatch batch = run_batch(music);
        const uint32_t used = music->position > batch_start ? music->position - batch_start : 1;
        if (batch == SilBatch_Error || used > *budget) {
            return false;
        }
        *budget -= used;
        batch_start = music->position;
        if (batch == SilBatch_Wait) {
            waited = true;
            commit(music);
            continue;
        }
        if (!waited) {
            return false;
        }
        waited = false;
        music->position = music->start;
        batch_start = music->start;
        const PassState now = pass_state(music);
        for (uint32_t i = 0; i < count; i++) {
            if (memcmp(&seen[i], &now, sizeof(now)) == 0) {
                return true;
            }
        }
        if (count > SIL_MAX_PASSES) {
            return false;
        }
        seen[count++] = now;
    }
}

// A sample is a block that plays once, or the block and then the repeat. The voices play what the DMA run was started
// with; nuade_paula_channel_sync does the same for a block that repeats whole, but cannot stop at the end of a block.

static NuadeSample block_sample(const int8_t* lc, uint16_t words, bool repeats) {
    const uint32_t bytes = (uint32_t)words * 2u;
    return (NuadeSample) {
        .data = lc,
        .format = NuadeSampleFormat_S8,
        .length = bytes,
        .loop_start = 0,
        .loop_length = repeats ? bytes : 0,
    };
}

static void sync_channel(SilChannel* c, uint32_t voice, NuadeVoices* voices) {
    NuadePaulaChannel* paula = &c->paula;
    nuade_voice_set_pitch(voices, voice, nuade_paula_period_hz(paula->period));
    nuade_voice_set_volume(voices, voice, (float)nuade_paula_volume(paula->volume) / 64.0f);
    if (!paula->dma) {
        if (paula->voice_on) {
            nuade_voice_stop(voices, voice);
            paula->voice_on = false;
        }
        paula->sent_lc = nullptr;
        paula->dma_started = false;
        return;
    }
    if (paula->dma_started) {
        paula->dma_started = false;
        const NuadeSample start = block_sample(paula->start_lc, paula->start_len_words, !c->one_shot);
        NuadeVoiceState before;
        nuade_voice_get_state(voices, voice, &before);
        c->wraps_at_start = before.wraps;
        c->stop_at_switch = false;
        nuade_voice_play(voices, voice, &start, 0);
        paula->voice_on = true;
        paula->sent_lc = paula->start_lc;
        paula->sent_len_words = paula->start_len_words;
    }
    if (paula->lc != nullptr && (paula->lc != paula->sent_lc || paula->len_words != paula->sent_len_words)) {
        const NuadeSample repeat = block_sample(paula->lc, paula->len_words, !c->chained_tail);
        nuade_voice_queue(voices, voice, &repeat);
        paula->sent_lc = paula->lc;
        paula->sent_len_words = paula->len_words;
    }
}

void sil_music_sync(SilMusic* music, NuadeVoices* voices) {
    for (uint32_t i = 0; i < SIL_USED_VOICES; i++) {
        sync_channel(&music->channels[i], i, voices);
    }
}
