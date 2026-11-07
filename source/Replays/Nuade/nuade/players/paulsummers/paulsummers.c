// SPDX-License-Identifier: MIT
// Paul Summers player: the eagleplayer's copy of the game's sound engine, one function per routine. Every tick it
// starts the requested subsong, or steps the four voices in turn. A voice reads its command stream when its note has
// run out: a byte below 0x80 is a note, a byte from 0x80 a command. A note picks the sample of the instrument range it
// falls in, starts it and sets the length of the note in ticks; a slide then moves the period every few ticks.
//
// The names follow the original: the voice record, the stack of 128 bytes behind it, the song end flags (a voice
// clears its own with the end command; the eagleplayer calls SongEnd when all four are clear).

#include <nuade/players/paulsummers.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "ps_internal.h"
#include <stdlib.h>

#define PS_STEP_GUARD \
    (1u << 20) // commands one voice may read in one tick before the data counts as a loop (the
               // original would spend that long in its interrupt)
#define PS_SLIDE_GUARD 1024u
#define PS_NOTE_LIMIT 0x80u
#define PS_SLIDE_END_MARK 0xF7u
#define PS_SLIDE_SET_MARK 0xF8u
#define PS_DEFAULT_TIMER 14187u // DeliTracker's 50 Hz CIA timer, in effect until a song sets its own
#define PS_VOLUME_MASK 0x7Fu
#define PS_SLIDE_UNIT 0x40u
#define PS_RANGE_FIRST 2u // bytes of the word count before the first range

// The note start of voice 3 tests a byte that is not a flag of its own: the lowest byte of the stack pointer of voice 0
// (the status word of voice n is read at twice the voice mask, which for voice 3 lands in the voice 0 record). The
// eagleplayer's data block sits at 0xD008 in UADE, so the pointer starts at ...FE and moves down by 4 per stack word.
#define PS_STACK_TOP_LOW_BYTE 0xFEu
#define PS_REPEAT_SKIP_BIT 0x40u
#define PS_STATUS_VOICE 3u

typedef enum PsCommand {
    PsCommand_Rest,       // DMA off for the length that follows
    PsCommand_Jump,       // 32 bit offset from the offset's own address
    PsCommand_Call,       // the same, with the return address pushed
    PsCommand_Return,     // pop the return address
    PsCommand_LoopStart,  // 16 bit count, pushed with the address behind it
    PsCommand_LoopEnd,    // count down, back to the loop start while it is not 0
    PsCommand_Instrument, // 32 bit offset to the instrument range table
    PsCommand_LengthHigh, // high byte of the next length
    PsCommand_SlideSet,   // slide parameters: step, delay
    PsCommand_SlideSteps, // number of semitone steps a slide moves
    PsCommand_Unused10,   // the table sends these back to the same byte: the 68000 loops there forever
    PsCommand_Unused11,
    PsCommand_Unused12,
    PsCommand_Unused13,
    PsCommand_Volume, // volume of the voice
    PsCommand_End,    // the voice has finished
} PsCommand;

// A byte of the command stream; the cursor moves on. Past the image the data is out of the model.

static uint8_t next_byte(PsPlayer* player, uint32_t* cursor) {
    uint8_t value = 0;
    if (!ps_u8(&player->module, *cursor, &value)) {
        player->fault = true;
    }
    (*cursor)++;
    return value;
}

// The 32 bit big-endian operand of a jump, call or table command.

static uint32_t next_long(PsPlayer* player, uint32_t* cursor) {
    uint32_t value = 0;
    for (uint32_t i = 0; i < 4; i++) {
        value = (value << 8) | next_byte(player, cursor);
    }
    return value;
}

static void push(PsPlayer* player, PsVoice* voice, uint32_t value) {
    if (voice->stack_used >= PS_STACK_WORDS) {
        player->fault = true;
        return;
    }
    voice->stack[voice->stack_used++] = value;
}

static uint32_t pop(PsPlayer* player, PsVoice* voice) {
    if (voice->stack_used == 0) {
        player->fault = true;
        return 0;
    }
    return voice->stack[--voice->stack_used];
}

// The routine the eagleplayer calls when a voice reads its end command: the song ends when all four have.

static void voice_song_end(PsPlayer* player, uint32_t index) {
    player->song_flag[index] = false;
    for (uint32_t i = 0; i < PS_VOICES; i++) {
        if (player->song_flag[i]) {
            return;
        }
    }
    for (uint32_t i = 0; i < PS_VOICES; i++) {
        player->song_flag[i] = true;
    }
    player->song_end = true;
}

// The length of the note: the byte behind it, with the high byte a command left.

static void set_length(PsPlayer* player, PsVoice* voice, uint32_t* cursor) {
    voice->length = (uint16_t)(voice->length_high | next_byte(player, cursor));
    voice->length_high = 0;
    voice->stream = *cursor;
    voice->note_active = true;
}

// After the DMA start every note writes the silent repeat, unless the status byte of its voice has the skip bit set.
// Only voice 3 can have it (see PS_STACK_TOP_LOW_BYTE); then the sample repeats.

static bool repeat_is_written(const PsPlayer* player, uint32_t index) {
    if (index != PS_STATUS_VOICE) {
        return true;
    }
    const uint32_t pointer_low_byte = PS_STACK_TOP_LOW_BYTE - 4u * player->voices[0].stack_used;
    return (pointer_low_byte & PS_REPEAT_SKIP_BIT) == 0;
}

// A note: the instrument range it falls in gives the sample, the length of the sample, and the period table index
// (the note counted from the range's base).

static void start_note(PsPlayer* player, uint32_t index, uint8_t note) {
    PsVoice* voice = &player->voices[index];
    const PsModule* module = &player->module;
    NuadePaulaChannel* paula = &voice->paula;
    uint16_t count = 0;
    if (!voice->has_ranges || !ps_u16(module, voice->ranges, &count)) {
        player->fault = true;
        return;
    }
    uint32_t range_at = voice->ranges + PS_RANGE_FIRST;
    for (uint32_t i = 0; i < count; i++, range_at += PS_RANGE_BYTES) {
        PsRange range = { 0 };
        if (!ps_range(module, range_at, &range)) {
            player->fault = true;
            return;
        }
        if (note < (int16_t)range.low || note > (int16_t)range.high) {
            continue;
        }
        PsPeriodPair pair = { 0 };
        const int32_t table_index = (int32_t)note - (int32_t)range.base;
        const uint32_t sample = (range_at + (uint32_t)range.sample_offset) & ~1u; // Paula ignores bit 0 of AUDxLC
        if (!ps_period_pair(module, table_index, &pair) || sample >= module->size) {
            player->fault = true;
            return;
        }
        voice->note = table_index;
        paula->lc = (const int8_t*)module->mem + sample;
        paula->len_words = range.length_words;
        paula->period = (uint16_t)(pair.current >> 16);
        voice->period = paula->period;
        voice->instrument = (int)i;
        nuade_paula_channel_dma_on(paula);
        if (repeat_is_written(player, index)) {
            paula->lc = ps_idle_sample();
            paula->len_words = PS_IDLE_WORDS;
        }
        return;
    }
}

// The slide command: step and delay of the slide, unless one is set already (then its two bytes are skipped).

static void command_slide_set(PsPlayer* player, PsVoice* voice, uint32_t* cursor) {
    if (voice->sliding) {
        *cursor += 2;
        voice->stream = *cursor;
        return;
    }
    voice->sliding = true;
    voice->slide_step = next_byte(player, cursor);
    voice->slide_delay = (uint16_t)(voice->length_high | next_byte(player, cursor));
    voice->length_high = 0;
    voice->script = *cursor;
    voice->stream = *cursor;
}

// The slide command: the number of steps; its divisor is the share of a step one unit of the slide step moves.

static void command_slide_steps(PsPlayer* player, PsVoice* voice, uint32_t* cursor) {
    voice->slide_steps = next_byte(player, cursor);
    if (voice->slide_steps != 0) {
        voice->slide_divisor = (uint8_t)(PS_SLIDE_UNIT / voice->slide_steps);
    }
    voice->stream = *cursor;
}

// The end of a loop: count down; while it is not 0 the stream goes back to the loop start.

static void command_loop_end(PsPlayer* player, PsVoice* voice, uint32_t* cursor) {
    uint16_t count = (uint16_t)pop(player, voice);
    const uint32_t loop_start = pop(player, voice);
    count--;
    if (count != 0) {
        push(player, voice, loop_start);
        push(player, voice, count);
        *cursor = loop_start;
    }
    voice->stream = *cursor;
}

// A command byte: the low nibble selects the routine. Returns false when the voice is done for this tick.

static bool run_command(PsPlayer* player, uint32_t index, uint8_t command, uint32_t* cursor) {
    PsVoice* voice = &player->voices[index];
    const uint32_t operand = *cursor;
    switch ((PsCommand)(command & 0xFu)) {
        case PsCommand_Rest:
            voice->keep_off = true;
            nuade_paula_channel_dma_off(&voice->paula);
            set_length(player, voice, cursor);
            return true;
        case PsCommand_Jump:
            voice->stream = operand + next_long(player, cursor);
            return true;
        case PsCommand_Call: {
            const uint32_t offset = next_long(player, cursor);
            push(player, voice, *cursor);
            voice->stream = operand + offset;
            return true;
        }
        case PsCommand_Return:
            voice->stream = pop(player, voice);
            return true;
        case PsCommand_LoopStart: {
            const uint32_t count = (uint32_t)next_byte(player, cursor) << 8;
            const uint32_t counted = count | next_byte(player, cursor);
            push(player, voice, *cursor);
            push(player, voice, counted);
            voice->stream = *cursor;
            return true;
        }
        case PsCommand_LoopEnd:
            command_loop_end(player, voice, cursor);
            return true;
        case PsCommand_Instrument: {
            const uint32_t offset = next_long(player, cursor);
            voice->stream = *cursor;
            voice->ranges = operand + offset;
            voice->has_ranges = true;
            return true;
        }
        case PsCommand_LengthHigh:
            voice->length_high = (uint16_t)(next_byte(player, cursor) << 8);
            voice->stream = *cursor;
            return true;
        case PsCommand_SlideSet:
            command_slide_set(player, voice, cursor);
            return true;
        case PsCommand_SlideSteps:
            command_slide_steps(player, voice, cursor);
            return true;
        case PsCommand_Volume:
            voice->paula.volume = next_byte(player, cursor) & PS_VOLUME_MASK;
            voice->stream = *cursor;
            return true;
        case PsCommand_End:
            voice_song_end(player, index);
            voice->stopped = true;
            return false;
        case PsCommand_Unused10:
        case PsCommand_Unused11:
        case PsCommand_Unused12:
        case PsCommand_Unused13:
            break;
    }
    player->fault = true;
    return false;
}

// Reads the stream until a note or a rest sets a length. Returns false when the voice ended or the data is out of
// the model.

static bool read_stream(PsPlayer* player, uint32_t index) {
    PsVoice* voice = &player->voices[index];
    while (player->steps++ < PS_STEP_GUARD && !player->fault) {
        uint32_t cursor = voice->stream;
        const uint8_t byte = next_byte(player, &cursor);
        if (byte >= PS_NOTE_LIMIT) {
            if (!run_command(player, index, byte, &cursor)) {
                return false;
            }
            if (voice->note_active) {
                return true;
            }
            continue;
        }
        start_note(player, index, byte);
        set_length(player, voice, &cursor);
        return !player->fault;
    }
    player->fault = true;
    return false;
}

// The period a slide gives: `steps` semitones' share of the way from the note to the next one.

static uint16_t slide_period(PsPlayer* player, PsVoice* voice) {
    int32_t note = voice->note - (int32_t)voice->slide_steps;
    uint8_t remainder = voice->slide_step;
    uint32_t guard = 0;
    while ((int8_t)remainder >= (int8_t)voice->slide_divisor) {
        remainder = (uint8_t)(remainder - voice->slide_divisor);
        note++;
        if (++guard >= PS_SLIDE_GUARD) {
            player->fault = true;
            return 0;
        }
    }
    const uint16_t multiplier = (uint16_t)(remainder * voice->slide_steps);
    PsPeriodPair pair = { 0 };
    if (!ps_period_pair(&player->module, note, &pair)) {
        player->fault = true;
        return 0;
    }
    const uint32_t difference = pair.next - pair.current;
    const uint32_t low_product = (uint32_t)(uint16_t)difference * multiplier;
    const uint32_t high_product = (uint32_t)(difference >> 16) * multiplier;
    const uint32_t share = (((high_product & 0xFFFFu) << 16) | (low_product & 0xFFFFu)) >> 6;
    return (uint16_t)((pair.current - share) >> 16);
}

// The parameters of the next slide: bytes of the stream up to the next set mark; any other byte ends the slide.

static void read_slide_script(PsPlayer* player, PsVoice* voice) {
    uint32_t cursor = voice->script;
    for (uint32_t step = 0; step < PS_STEP_GUARD && !player->fault; step++) {
        const uint8_t byte = next_byte(player, &cursor);
        if (byte == PS_SLIDE_END_MARK) {
            voice->length_high = (uint16_t)(next_byte(player, &cursor) << 8);
        } else if (byte == PS_SLIDE_SET_MARK) {
            voice->slide_step = next_byte(player, &cursor);
            voice->slide_delay = (uint16_t)(voice->length_high | next_byte(player, &cursor));
            voice->length_high = 0;
            voice->script = cursor;
            return;
        } else {
            voice->sliding = false;
            return;
        }
    }
    player->fault = true;
}

// One tick of a slide: wait out the delay, write the slid period, read the parameters of the next one.

static void run_slide(PsPlayer* player, PsVoice* voice) {
    if (voice->slide_delay != 0) {
        voice->slide_delay--;
        if (voice->slide_delay != 0) {
            return;
        }
    }
    const uint16_t period = slide_period(player, voice);
    if (player->fault) {
        return;
    }
    voice->paula.period = period;
    voice->period = period;
    read_slide_script(player, voice);
}

// One voice of the tick.

static void run_voice(PsPlayer* player, uint32_t index) {
    PsVoice* voice = &player->voices[index];
    if (voice->stopped) {
        if (!voice->stop_done) {
            voice->stop_done = true;
            nuade_paula_channel_dma_off(&voice->paula);
        }
        return;
    }
    player->steps = 0;
    bool read = !voice->note_active;
    for (;;) {
        if (read) {
            if (!voice->keep_off) {
                nuade_paula_channel_dma_off(&voice->paula);
            }
            voice->sliding = false;
            voice->note_active = false;
            voice->keep_off = false;
            if (!read_stream(player, index)) {
                return;
            }
        }
        read = true;
        if (voice->length != 0) {
            break;
        }
    }
    voice->length--;
    if (voice->length == 0) {
        voice->sliding = false;
        voice->keep_off = false;
        voice->note_active = false;
        nuade_paula_channel_dma_off(&voice->paula);
    }
    if (voice->sliding) {
        run_slide(player, voice);
    }
}

// The request for a subsong: every voice stops and the voices with a stream start it.

static void start_subsong(PsPlayer* player) {
    for (uint32_t i = 0; i < PS_VOICES; i++) {
        PsVoice* voice = &player->voices[i];
        nuade_paula_channel_dma_off(&voice->paula);
        voice->stack_used = 0;
        voice->stopped = true;
        voice->stop_done = false;
        uint32_t stream = 0;
        if (ps_voice_stream(&player->module, player->subsong, i, &stream)) {
            voice->stream = stream;
            voice->stopped = false;
            voice->sliding = true;
            voice->length = 0;
            voice->length_high = 0;
        }
        voice->paula.volume = 0;
    }
    player->restart_pending = false;
}

// The tick gate: tempo 0 runs every tick, a negative tempo skips one tick in -tempo + 1.

static bool tempo_runs(PsPlayer* player) {
    if (player->tempo == 0) {
        return true;
    }
    if (player->tempo_counter != 0) {
        player->tempo_counter++;
        return true;
    }
    player->tempo_counter = player->tempo;
    return false;
}

PsPlayer* ps_create(const uint8_t* data, size_t size) {
    PsPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    if (!ps_module_parse(&player->module, data, size)) {
        free(player);
        return nullptr;
    }
    return player;
}

bool ps_probe(const uint8_t* data, size_t size) {
    PsModule module;
    if (!ps_module_parse(&module, data, size)) {
        return false;
    }
    ps_module_free(&module);
    return true;
}

void ps_destroy(PsPlayer* player) {
    if (player == nullptr) {
        return;
    }
    ps_module_free(&player->module);
    free(player);
}

uint32_t ps_subsong_count(const PsPlayer* player) {
    return player->module.subsong_count;
}

// InitSound: clears the engine's variables, takes the tempo and the timer of the song and requests the subsong.

bool ps_start(PsPlayer* player, uint32_t subsong, NuadeVoices* voices) {
    if (player == nullptr || subsong >= player->module.subsong_count) {
        return false;
    }
    PsSongEntry entry = { 0 };
    if (!ps_song_entry(&player->module, subsong + 1, &entry)) {
        return false;
    }
    for (uint32_t i = 0; i < PS_VOICES; i++) {
        player->voices[i] = (PsVoice) { .stopped = true, .instrument = -1 };
        player->song_flag[i] = true;
        if (voices != nullptr) {
            nuade_voice_stop(voices, i);
        }
    }
    player->subsong = subsong + 1;
    player->tempo = entry.tempo;
    player->tempo_counter = entry.tempo_init;
    if (entry.timer != 0) {
        player->timer = entry.timer;
    } else if (player->timer == 0) {
        player->timer = PS_DEFAULT_TIMER; // a song without a timer keeps the previous one
    }
    player->restart_pending = true;
    player->song_end = false;
    player->fault = false;
    return true;
}

PsTickResult ps_tick(PsPlayer* player, NuadeVoices* voices) {
    player->song_end = false;
    if (!player->fault && tempo_runs(player)) {
        if (player->restart_pending) {
            start_subsong(player);
        }
        for (uint32_t i = 0; i < PS_VOICES; i++) {
            run_voice(player, i);
        }
    }
    bool finished = true;
    for (uint32_t i = 0; i < PS_VOICES; i++) {
        finished = finished && player->voices[i].stop_done;
        if (voices != nullptr) {
            nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
        }
    }
    return (PsTickResult) { .length = player->timer, .looped = player->song_end, .ended = player->fault || finished };
}

PsVoiceState ps_voice_state(const PsPlayer* player, uint32_t voice) {
    const PsVoice* v = &player->voices[voice];
    return (PsVoiceState) { .period = v->period, .instrument = v->instrument };
}
