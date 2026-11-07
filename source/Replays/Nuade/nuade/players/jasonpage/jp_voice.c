// SPDX-License-Identifier: MIT
// Jason Page player: one voice per tick: the instrument script, pitch, volume envelope and the Paula registers.

#include "jp_internal.h"

// The scripts run until a command ends the voice's tick. A script that loops without ending it never returns on the
// 68000; the port gives up after this many commands.
#define MAX_COMMANDS_PER_TICK 4096

#define ENV_MAX_LEVEL 0xFF00u
#define ENV_TICKS_DONE 0xFF   // the tick count a finished ramp has
#define ENV_PHASE_OFF 0xFFFFu // a hold or release word that is not running
#define VOLUME_SHIFT 10
#define HOLD_VOLUME_START 0x0900u // the hold phase of a Virocop voice's first note lets the volume fall to this
#define SAMPLE_COPY_BYTES 128
#define NO_LOOP_YET 0xFFFFFFFFu // Virocop and Realms start a note with this in the loop length until the script sets it

// The note tables hold 84 periods; the words behind them are the first instruction of the replayer that follows.
#define PERIOD_TABLE_SIZE 84
#define PERIOD_TAIL_FIRST 0x48E7
#define PERIOD_TAIL_SECOND 0xC1C0

#define COMMAND_TABLE_SIZE 24 // the jump tables of the three replayers
#define VIROCOP_COMMAND_HOLD 24
#define REALMS_COMMAND_SCALE 4 // Realms' command words are offsets into its table of longs

#define SAMPLE_OFFSET_LIMIT 0x8000u // the replayers scale an index by 4 in 16 bits and use it as a signed offset
#define SAMPLE_SCALE 4u
#define SAMPLE_COPY_LONGS (SAMPLE_COPY_BYTES / 4)

typedef enum JpCommand {
    JpCommand_StopIdle,    // the voice goes silent and runs the idle script
    JpCommand_Yield,       // end of this tick for the script
    JpCommand_SetSample,   // select a sample
    JpCommand_SetLoopWord, // bytes of the sample that play, as a word
    JpCommand_SetLoopLong, // the same as a long
    JpCommand_Wait,        // wait n ticks
    JpCommand_LoopPush,
    JpCommand_LoopEnd,
    JpCommand_AddStart,
    JpCommand_AddLength,
    JpCommand_AddLoopBytes,
    JpCommand_AddPeriod,
    JpCommand_AddVolume,
    JpCommand_Vibrato,
    JpCommand_SetPeriod,
    JpCommand_SetVolume,
    JpCommand_DmaOn,
    JpCommand_StopNote, // DMA off, registers to the silent word
    JpCommand_Envelope,
    JpCommand_SetNote,
    JpCommand_AddNote,
    JpCommand_CopySample, // 128 bytes from one sample over another
    JpCommand_MorphSample,
    JpCommand_HoldVolume, // Virocop: the hold phase's volume comes from the last pattern command
} JpCommand;

// Commands 2..23, the same in the three replayers. Commands 0 and 1 and Virocop's 24 differ.
static const JpCommand k_commands[COMMAND_TABLE_SIZE] = {
    [2] = JpCommand_SetSample,     [3] = JpCommand_SetLoopWord,  [4] = JpCommand_SetLoopLong, [5] = JpCommand_Wait,
    [6] = JpCommand_LoopPush,      [7] = JpCommand_LoopEnd,      [8] = JpCommand_AddStart,    [9] = JpCommand_AddLength,
    [10] = JpCommand_AddLoopBytes, [11] = JpCommand_AddPeriod,   [12] = JpCommand_AddVolume,  [13] = JpCommand_Vibrato,
    [14] = JpCommand_SetPeriod,    [15] = JpCommand_SetVolume,   [16] = JpCommand_DmaOn,      [17] = JpCommand_StopNote,
    [18] = JpCommand_Yield,        [19] = JpCommand_Envelope,    [20] = JpCommand_SetNote,    [21] = JpCommand_AddNote,
    [22] = JpCommand_CopySample,   [23] = JpCommand_MorphSample,
};

static const uint16_t k_periods_standard[PERIOD_TABLE_SIZE]
    = { 0xEEE, 0xE17, 0xD4D, 0xC8E, 0xBD9, 0xB2F, 0xA8E, 0x9F7, 0x967, 0x8E0, 0x861, 0x7E8, 0x777, 0x70B,
        0x6A6, 0x647, 0x5EC, 0x597, 0x547, 0x4FB, 0x4B3, 0x470, 0x430, 0x3F4, 0x3BB, 0x385, 0x353, 0x323,
        0x2F6, 0x2CB, 0x2A3, 0x27D, 0x25A, 0x238, 0x218, 0x1FA, 0x1DD, 0x1C3, 0x1A9, 0x191, 0x17B, 0x165,
        0x151, 0x13E, 0x12D, 0x11C, 0x10C, 0xFD,  0xEE,  0xE1,  0xD4,  0xC8,  0xBD,  0xB3,  0xA8,  0x9F,
        0x96,  0x8E,  0x86,  0x7E,  0x77,  0x70,  0x6A,  0x64,  0x5E,  0x59,  0x54,  0x4F,  0x4B,  0x47,
        0x43,  0x3F,  0x3B,  0x38,  0x35,  0x32,  0x2F,  0x2C,  0x2A,  0x27,  0x25,  0x23,  0x21,  0x1F };

static const uint16_t k_periods_virocop[PERIOD_TABLE_SIZE]
    = { 0x6B0, 0x650, 0x5F4, 0x5A0, 0x54C, 0x500, 0x4B8, 0x474, 0x434, 0x3F8, 0x3C0, 0x38A, 0x358, 0x328,
        0x2FA, 0x2D0, 0x2A6, 0x280, 0x25C, 0x23A, 0x21A, 0x1FC, 0x1E0, 0x1C5, 0x1AC, 0x194, 0x17D, 0x168,
        0x153, 0x140, 0x12E, 0x11D, 0x10D, 0xFE,  0xF0,  0xE2,  0xD6,  0xCA,  0xBE,  0xB4,  0xA9,  0xA0,
        0x97,  0x8E,  0x86,  0x7F,  0x78,  0x71,  0x6B,  0x65,  0x5F,  0x5A,  0x54,  0x50,  0x4B,  0x47,
        0x43,  0x3F,  0x3C,  0x38,  0x35,  0x32,  0x2F,  0x2D,  0x2A,  0x28,  0x25,  0x23,  0x21,  0x1F,
        0x1E,  0x1C,  0x1A,  0x19,  0x17,  0x16,  0x15,  0x14,  0x12,  0x11,  0x10,  0xF,   0xF,   0xE };

// The note tables as the replayer reads them: by a 16-bit byte offset, as a signed index. The words behind the table
// are code of the eagleplayer; the first two are the ones a note capped at 84 reads. Anything further is not modelled.

static bool period_at(const JpPlayer* player, uint16_t byte_offset, uint16_t* period) {
    const int16_t offset = (int16_t)byte_offset;
    if (offset < 0) {
        return false;
    }
    const uint32_t index = (uint32_t)offset / 2;
    const uint16_t* table = player->format == JpFormat_Virocop ? k_periods_virocop : k_periods_standard;
    if (index < PERIOD_TABLE_SIZE) {
        *period = table[index];
    } else if (index == PERIOD_TABLE_SIZE) {
        *period = PERIOD_TAIL_FIRST;
    } else if (index == PERIOD_TABLE_SIZE + 1) {
        *period = PERIOD_TAIL_SECOND;
    } else {
        return false;
    }
    return true;
}

uint16_t jp_voice_pattern_period(JpPlayer* player, uint8_t note) {
    // A note from a pattern: Graftgold and Virocop keep the low byte of the doubled index, Realms reads the table at up
    // to 510 bytes.
    const uint16_t offset = player->format == JpFormat_Realms ? (uint16_t)(note * 2u) : (uint16_t)((note * 2u) & 0xFF);
    uint16_t period = 0;
    if (!period_at(player, offset, &period)) {
        player->failed = true;
    }
    return period;
}

// The Paula registers of a voice, written as the replayer does (the channel applies the length 0 and period 0 rules). A
// sample address with bit 0 set (scripts add byte counts to it) plays from the even address below.

static void write_registers(const JpPlayer* player, JpVoice* voice, uint32_t address, uint16_t words, uint16_t period) {
    NuadePaulaChannel* channel = &voice->paula;
    address &= ~1u; // AUDxLC is a word address: bit 0 is not stored
    channel->lc = address < player->mem.size ? (const int8_t*)(player->mem.data + address) : nullptr;
    channel->len_words = words;
    channel->period = period;
}

// The registers of a voice that plays nothing: one word of silence at the period it had.

static void write_silence(const JpPlayer* player, JpVoice* voice) {
    write_registers(player, voice, player->empty_buffer, 1, voice->period);
}

static void stop_envelope(JpVoice* voice) {
    voice->attack = (JpEnvelopeRamp) { 0 };
    voice->decay = (JpEnvelopeRamp) { 0 };
    voice->hold = (JpEnvelopePhase) { 0 };
    voice->release = (JpEnvelopePhase) { 0 };
}

// Switch the voice's DMA off and point its registers at the silent word. Graftgold and Virocop also clear the volume,
// the envelope phases and the loop length.

static void stop_sound(JpPlayer* player, JpVoice* voice) {
    nuade_paula_channel_dma_off(&voice->paula);
    if (player->format != JpFormat_Realms) {
        voice->volume = 0;
        stop_envelope(voice);
        voice->loop_bytes = 0;
        voice->paula.volume = 0;
    }
    write_silence(player, voice);
}

// The sound a queued instrument starts: its script, from the start.

static uint32_t instrument_script(const JpPlayer* player, uint16_t instrument) {
    const JpLayout* layout = &player->layout;
    return jp_table_pointer(player, layout->instrument_table, layout->script_base, instrument);
}

static void start_note(JpPlayer* player, JpVoice* voice) {
    const uint16_t instrument = voice->pending_instrument;
    if (player->format != JpFormat_Realms) {
        stop_sound(player, voice);
    }
    voice->wait = 0;
    voice->volume = 0;
    voice->vibrato_depth = 0;
    voice->vibrato_step = 0;
    voice->vibrato_count = 0;
    voice->slide = 0;
    voice->loop_free = 0;
    voice->loop_top = 0;
    voice->has_pending = false;
    stop_envelope(voice);
    voice->script = instrument_script(player, instrument);
    voice->loop_bytes = player->format == JpFormat_Graftgold ? 0 : NO_LOOP_YET;
    if (player->format == JpFormat_Virocop) {
        voice->hold_volume = JP_HOLD_VOLUME_NONE;
    } else if (player->format == JpFormat_Realms) {
        stop_sound(player, voice);
    }
    voice->instrument_shown = (int)instrument;
    voice->note = (int)voice->last_note;
}

// An instrument from the pattern waits in the voice until the next tick, if its priority is at least that of the sound
// that plays.

bool jp_voice_queue(const JpPlayer* player, JpVoice* voice, uint16_t instrument) {
    const uint16_t priority = jp_table_value(player, player->layout.priority_table, instrument);
    if (priority < voice->priority) {
        return false;
    }
    voice->priority = priority;
    voice->has_pending = true;
    voice->pending_instrument = instrument;
    return true;
}

// The command a script word names. Graftgold and Virocop use the low byte as a table index; Realms stores the offset
// into its table of longs. A word outside the table sends the 68000 into the code that follows it.

static bool decode_command(JpFormat format, uint16_t word, JpCommand* command) {
    uint32_t index = word & JP_BYTE_MASK;
    if (format == JpFormat_Realms) {
        if ((word & (REALMS_COMMAND_SCALE - 1)) != 0 || word >= COMMAND_TABLE_SIZE * REALMS_COMMAND_SCALE) {
            return false;
        }
        index = word / REALMS_COMMAND_SCALE;
    }
    if (format == JpFormat_Virocop && index == VIROCOP_COMMAND_HOLD) {
        *command = JpCommand_HoldVolume;
    } else if (index >= COMMAND_TABLE_SIZE) {
        return false;
    } else if (index == 0) {
        *command = JpCommand_StopIdle;
    } else if (index == 1) {
        *command = format == JpFormat_Graftgold ? JpCommand_StopIdle : JpCommand_Yield;
    } else {
        *command = k_commands[index];
    }
    return true;
}

// A sample's start, by the index a script command gives. The tables hold the samples the module declares.

static bool sample_address(JpPlayer* player, uint32_t index, uint32_t* address) {
    if (index >= player->layout.sample_count) {
        player->failed = true;
        return false;
    }
    *address = player->layout.sample_start[index];
    return true;
}

// The sample a script word names: the replayer multiplies it by 4 in a 16-bit register and uses the result as a signed
// offset into its table, so an offset of 0x8000 or more reads before the table (outside the declared samples).

static bool sample_by_word(JpPlayer* player, uint16_t word, uint32_t* address) {
    const uint16_t offset = (uint16_t)(word * SAMPLE_SCALE);
    if (offset >= SAMPLE_OFFSET_LIMIT) {
        player->failed = true;
        return false;
    }
    return sample_address(player, offset / SAMPLE_SCALE, address);
}

// Command 22: 128 bytes of one sample over the start of another.

static void copy_sample(JpPlayer* player, uint32_t source_index, uint32_t target_index) {
    uint32_t source = 0;
    uint32_t target = 0;
    if (!sample_address(player, source_index, &source) || !sample_address(player, target_index, &target)) {
        return;
    }
    for (uint32_t i = 0; i < SAMPLE_COPY_LONGS; i++) {
        if (!jp_store_u32(&player->mem, target + i * 4, jp_u32(&player->mem, source + i * 4))) {
            player->failed = true;
            return;
        }
    }
}

// Command 23: the first 128 bytes of the voice's sample step one unit towards another sample's.

static void morph_sample(JpPlayer* player, const JpVoice* voice, uint16_t target_word) {
    uint32_t target = 0;
    if (!sample_by_word(player, target_word, &target)) {
        return;
    }
    for (uint32_t i = 0; i < SAMPLE_COPY_BYTES; i++) {
        const uint8_t goal = jp_u8(&player->mem, target + i);
        const uint8_t now = jp_u8(&player->mem, voice->sample_base + i);
        const uint8_t next = now > goal ? (uint8_t)(now - 1) : now < goal ? (uint8_t)(now + 1) : now;
        if (!jp_store_u8(&player->mem, voice->sample_base + i, next)) {
            player->failed = true;
            return;
        }
    }
}

static void loop_push(JpVoice* voice, uint16_t count, uint32_t return_to) {
    const uint8_t slot = voice->loop_free;
    voice->loop_top = slot;
    voice->loop_count[slot] = count;
    voice->loop_return[slot] = return_to;
    voice->loop_free = (uint8_t)((slot + 1) % JP_LOOP_DEPTH);
}

// The end of a loop: a count of 0 repeats for ever; otherwise the count runs down and the script goes on when it hits
// 0.

static void loop_end(JpVoice* voice, uint32_t* script) {
    const uint8_t slot = voice->loop_top;
    if (voice->loop_count[slot] != 0) {
        voice->loop_count[slot]--;
        if (voice->loop_count[slot] == 0) {
            if (slot != 0) {
                voice->loop_free = (uint8_t)((voice->loop_free + JP_LOOP_DEPTH - 1) % JP_LOOP_DEPTH);
                voice->loop_top = (uint8_t)(slot - 1);
            }
            return;
        }
    }
    *script = voice->loop_return[slot];
}

// The script's words for the envelope phases.

static JpEnvelopeRamp load_ramp(uint16_t word) {
    const uint8_t ticks = (uint8_t)word;
    return (JpEnvelopeRamp) { .rate = (uint8_t)(word >> 8), .ticks = ticks, .running = ticks != ENV_TICKS_DONE };
}

static JpEnvelopePhase load_phase(uint16_t word) {
    return (JpEnvelopePhase) { .value = word, .running = word != ENV_PHASE_OFF };
}

// Run one script command. Returns true when it ends the voice's script for this tick; `script` is the address of the
// word after the command's own.

static bool execute(JpPlayer* player, JpVoice* voice, JpCommand command, uint32_t* script) {
    const JpMemory* mem = &player->mem;
    switch (command) {
        case JpCommand_StopIdle:
            *script = player->idle_script;
            voice->priority = 0;
            stop_sound(player, voice);
            return true;
        case JpCommand_Yield:
            return true;
        case JpCommand_SetSample: {
            uint32_t address = 0;
            const uint16_t word = jp_u16(mem, *script);
            *script += 2;
            if (sample_by_word(player, word, &address)) {
                voice->sample_base = address;
                voice->sample_start = address;
            }
            return false;
        }
        case JpCommand_SetLoopWord:
            voice->loop_bytes = jp_u16(mem, *script);
            voice->sample_words = (uint16_t)(voice->loop_bytes >> 1);
            *script += 2;
            return false;
        case JpCommand_SetLoopLong:
            voice->loop_bytes = jp_u32(mem, *script);
            if (voice->loop_bytes != 0) {
                voice->sample_words = (uint16_t)((voice->loop_bytes & 0xFFFF) >> 1);
            }
            *script += 4;
            return false;
        case JpCommand_Wait:
            voice->wait = jp_u16(mem, *script);
            *script += 2;
            return true;
        case JpCommand_LoopPush: {
            const uint16_t count = jp_u16(mem, *script);
            *script += 2;
            loop_push(voice, count, *script);
            return false;
        }
        case JpCommand_LoopEnd:
            loop_end(voice, script);
            return false;
        case JpCommand_AddStart:
            voice->sample_start += jp_u32(mem, *script);
            *script += 4;
            return false;
        case JpCommand_AddLength:
            voice->sample_words = (uint16_t)(voice->sample_words + (jp_s16(mem, *script) >> 1));
            *script += 2;
            return false;
        case JpCommand_AddLoopBytes:
            voice->loop_bytes += jp_u32(mem, *script);
            *script += 4;
            return false;
        case JpCommand_AddPeriod:
            voice->period = (uint16_t)(voice->period + jp_u16(mem, *script));
            *script += 2;
            return false;
        case JpCommand_AddVolume:
            voice->volume = (uint16_t)(voice->volume + jp_u16(mem, *script));
            *script += 2;
            return false;
        case JpCommand_Vibrato: {
            const uint16_t word = jp_u16(mem, *script);
            voice->vibrato_step = (int16_t)(word >> 8);
            voice->vibrato_depth = word & JP_BYTE_MASK;
            voice->vibrato_count = (uint16_t)(voice->vibrato_depth >> 1);
            *script += 2;
            return false;
        }
        case JpCommand_SetPeriod:
            voice->period = jp_u16(mem, *script);
            *script += 2;
            return false;
        case JpCommand_SetVolume:
            voice->volume = jp_u16(mem, *script);
            *script += 2;
            return false;
        case JpCommand_DmaOn:
            nuade_paula_channel_dma_on(&voice->paula);
            return false;
        case JpCommand_StopNote:
            stop_sound(player, voice);
            return true;
        case JpCommand_Envelope:
            voice->attack = load_ramp(jp_u16(mem, *script));
            voice->decay = load_ramp(jp_u16(mem, *script + 2));
            voice->hold = load_phase(jp_u16(mem, *script + 4));
            voice->release = load_phase(jp_u16(mem, *script + 6));
            voice->env_level = 0;
            *script += 8;
            return false;
        case JpCommand_SetNote: {
            voice->last_note = jp_u16(mem, *script) & JP_BYTE_MASK;
            if (!period_at(player, (uint16_t)(voice->last_note * 2u), &voice->period)) {
                player->failed = true;
            }
            *script += 2;
            return false;
        }
        case JpCommand_AddNote: {
            const uint16_t note = (uint16_t)(jp_u16(mem, *script) + voice->last_note);
            uint16_t period = 0;
            if (!period_at(player, (uint16_t)(note * 2u), &period)) {
                player->failed = true;
            }
            voice->period = period;
            *script += 2;
            return false;
        }
        case JpCommand_CopySample: {
            // The 68k turns the source byte into a table byte offset (source * 4), then loads the destination byte into
            // the low byte of the same register, so the high byte of that offset stays; the result is scaled by 4 again
            // as a sample index.
            const uint32_t source = jp_u8(mem, *script);
            const uint32_t kept_high_byte = (source * SAMPLE_SCALE) & ~JP_BYTE_MASK;
            const uint32_t target_index = kept_high_byte | jp_u8(mem, *script + 1);
            *script += 2;
            copy_sample(player, source, target_index);
            return false;
        }
        case JpCommand_MorphSample: {
            const uint16_t word = jp_u16(mem, *script);
            *script += 2;
            morph_sample(player, voice, word);
            return false;
        }
        case JpCommand_HoldVolume:
            voice->hold_volume = voice->hold_volume_next;
            return false;
    }
    return false;
}

static void run_script(JpPlayer* player, JpVoice* voice) {
    uint32_t script = voice->script;
    for (int count = 0; count < MAX_COMMANDS_PER_TICK; count++) {
        const uint16_t word = jp_u16(&player->mem, script);
        script += 2;
        JpCommand command;
        if (!decode_command(player->format, word, &command)) {
            player->failed = true;
            return;
        }
        const bool ends_tick = execute(player, voice, command, &script);
        if (player->failed) {
            return;
        }
        if (ends_tick) {
            voice->script = script;
            return;
        }
    }
    player->failed = true;
}

// Vibrato: the period moves by vibrato_step every tick and the step changes sign every vibrato_depth + 1 ticks.
// Realms runs the counter even when the step is 0.

static void vibrato(const JpPlayer* player, JpVoice* voice) {
    if (voice->vibrato_step == 0 && player->format != JpFormat_Realms) {
        return;
    }
    voice->period = (uint16_t)(voice->period + voice->vibrato_step);
    if (voice->vibrato_count == 0) {
        voice->vibrato_count = voice->vibrato_depth;
        voice->vibrato_step = (int16_t)-voice->vibrato_step;
    } else {
        voice->vibrato_count--;
    }
}

// Virocop's slide stops at the target period.

static void slide_to_target(JpVoice* voice) {
    if (voice->slide == 0) {
        return;
    }
    const uint16_t next = (uint16_t)(voice->period + voice->slide);
    if (voice->slide > 0) {
        voice->period = next < voice->target_period ? next : voice->target_period;
    } else {
        voice->period = next >= voice->target_period ? next : voice->target_period;
    }
}

static void pitch(const JpPlayer* player, JpVoice* voice) {
    vibrato(player, voice);
    if (player->format == JpFormat_Virocop) {
        slide_to_target(voice);
    } else {
        voice->period = (uint16_t)(voice->period + voice->slide);
    }
}

// One tick of a ramp: the tick count runs down (through 0 to ENV_TICKS_DONE, which ends the phase). Returns the rate
// the ramp adds to or takes from the 16-bit level.

static uint32_t ramp_step(JpEnvelopeRamp* ramp) {
    ramp->ticks = (uint8_t)(ramp->ticks - 1);
    ramp->running = ramp->ticks != ENV_TICKS_DONE;
    return (uint32_t)ramp->rate << 8;
}

// Volume envelope: attack, decay, hold, release; the first phase that runs is the one that acts.

static void envelope(const JpPlayer* player, JpVoice* voice) {
    uint32_t level = voice->env_level;
    if (voice->attack.running) {
        level += ramp_step(&voice->attack);
        if (level > 0xFFFF) {
            level = ENV_MAX_LEVEL;
            voice->attack.running = false;
        }
    } else if (voice->decay.running) {
        const uint32_t rate = ramp_step(&voice->decay);
        level = level >= rate ? level - rate : 0;
        if (rate > voice->env_level) {
            voice->decay.running = false;
        }
    } else if (voice->hold.running) {
        voice->hold.value--;
        voice->hold.running = voice->hold.value != ENV_PHASE_OFF;
        if (player->format == JpFormat_Virocop) {
            // The volume falls to the hold volume, and stays where it is below it.
            if (voice->hold_volume >= voice->volume) {
                return;
            }
            level = voice->hold_volume;
        }
    } else if (voice->release.running) {
        level = level >= voice->release.value ? level - voice->release.value : 0;
        if (voice->release.value > voice->env_level) {
            voice->release.running = false;
        }
    } else {
        return;
    }
    voice->env_level = (uint16_t)level;
    voice->volume = (uint16_t)level;
}

void jp_voice_tick(JpPlayer* player, JpVoice* voice) {
    if (voice->has_pending) {
        start_note(player, voice);
    }
    if (voice->wait == 0 || --voice->wait == 0) {
        run_script(player, voice);
    }
    pitch(player, voice);
    envelope(player, voice);
}

// The registers a voice carries to the end of the tick: the volume, and the sample (or the silent word).

void jp_voice_output(JpPlayer* player, JpVoice* voice) {
    const uint32_t level = voice->volume >> VOLUME_SHIFT; // 0..63: the master volume is full
    voice->paula.volume = (uint8_t)level;
    if (voice->loop_bytes == 0) {
        write_silence(player, voice);
        return;
    }
    const uint32_t address = voice->sample_start + 2u * voice->sample_words - voice->loop_bytes;
    write_registers(player, voice, address, voice->sample_words, voice->period);
}

// What the voice holds before the first song starts. The volume of the first three is whatever the replayer's data
// block had there; the first tick writes it to the volume register of a channel whose DMA is off.

void jp_voice_initial_state(JpVoice* voice, JpFormat format, uint8_t number) {
    static const uint16_t k_initial_volumes[JP_VOICES] = { 0x0800, 0x0900, 0x0A00, 0 };
    *voice = (JpVoice) { 0 };
    voice->number = number;
    voice->volume = k_initial_volumes[number];
    voice->note = -1;
    voice->instrument_shown = -1;
    if (format == JpFormat_Virocop) {
        voice->hold_volume = HOLD_VOLUME_START;
        voice->hold_volume_next = HOLD_VOLUME_START;
    }
}
