// SPDX-License-Identifier: MIT
// Mark Cooksey player, translated from the replayer inside the "*.mc" modules and the UADE eagleplayer "Mark Cooksey"
// (Wanted Team), which finds the replayer's entry points and calls into the module.
//
// The replayer exists in two builds (McFlavor) that play the same way.
//
// The eagleplayer patches the replayer in memory (the patched bytes show in a memory dump of UADE): the volume code
// writes level / 2 (at most 64) in both builds, the hardware waits become raster waits (no effect on what is written),
// and the decode of the commands 0x67 and 0x68 calls the eagleplayer, which ends the song when all four voices have
// run one of them.
//
// McModule: the replayer code at the start of the file, then the data it addresses pc-relative:
//   - the subsong table, 16 bytes per subsong: four longs, one per voice, each the distance from the long to the
//     voice's command stream (0: the voice is not used),
//   - the sub-sequence table: words, each the distance from the word to a sequence of commands,
//   - the instrument table: words, each the distance from the word to an instrument record (see read_instrument),
//   - the period table: words, four per semitone,
//   - the sample data (samples and envelopes are addressed from its start),
//   - in the Late build, a table of 256 volume bytes that the patched replayer does not read,
//   - the replayer's variables, which hold a snapshot of the state the game was in when the module was ripped.
//
// A voice reads commands from its stream: a note (bit 7 aside, a byte below 0x60: pitch, then instrument, length and
// a byte nothing reads), or a command byte 0x60..0x6b (wait, volume, call a sub-sequence, return, end, jump, glide on
// and off). Each tick a voice counts its length down; when the length has run out it reads commands until a note or a
// wait sets the next length. Then its volume envelope, glide and pitch envelope run.

#include <nuade/players/markcooksey.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_tracker.h"
#include "nuade_utils.h"
#include <stdlib.h>

// McModule layout. The replayer starts with an entry table of `bra.w`; its position tells the build.
#define MAGIC_HEADER 0xD040D040u // add.w d0,d0 twice
#define OP_JMP_TABLE 0x4EFB
#define OP_BRA_W 0x6000
#define OP_LEA_A1_PC 0x43FA
#define OP_LEA_A0_PC 0x41FA
#define OP_LEA_A3_PC 0x47FA
#define OP_LEA_A5_A5 0x4BED // lea d16(a5),a5

// The entry table: four `bra.w` (tick, start of a subsong, start of an effects subsong, volume) at 8 (Early) or at
// 0xC (Late; the entry at 8 sets the hardware up, a fifth entry sits at 0x18). Check2 also wants the first lea of the
// replayer at 0x28 (Early) or 0x96 (Late).
#define ENTRY_TABLE_EARLY 8
#define ENTRY_TABLE_LATE 0xC
#define ENTRY_SIZE 4
#define ENTRY_COUNT 4
#define CHECK_LEA_EARLY 0x28
#define CHECK_LEA_LATE 0x96
#define CHECK_ENTRY_LATE 0x18

// The code of a routine is searched for the leas that name its tables; this far from its start.
#define TICK_CODE_SPAN 0x600
#define ENTRY_CODE_SPAN 0x40

#define SUBSONG_RECORD_SIZE 16 // four longs
#define SUBSONG_MAX 255

// Voices and commands.
#define COMMAND_BASE 0x60
#define NOTE_BIT_MASK 0x7F
#define COMMANDS_PER_TICK 4096

// Pitch is counted in quarter semitones; the period table has one word per step.
#define STEPS_PER_NOTE 4
#define PITCH_FRACTION_BITS 4

// The sample base holds a silent block of this many words that a one-shot sample repeats.
#define SILENCE_WORDS 0x20

#define VOLUME_MAX 0x40

// The replayer's variables start with a flag byte, the flags of the effects slots and the master volume; the voice
// records follow.
#define VARIABLES_HEADER_SIZE 4

typedef enum McCommand {
    McCommand_Wait = 0x60,
    McCommand_Volume = 0x63,
    McCommand_Call = 0x64,
    McCommand_CallRepeat = 0x65,
    McCommand_Return = 0x66,
    McCommand_End = 0x67,
    McCommand_Jump = 0x68,
    McCommand_GlideOn = 0x69,
    McCommand_GlideOff = 0x6B,
} McCommand;

// McModule reads. A read outside the file gives 0.

static uint8_t module_u8(const McModule* m, uint32_t offset) {
    return offset < m->size ? m->data[offset] : 0;
}

static uint16_t module_u16(const McModule* m, uint32_t offset) {
    return (uint16_t)((module_u8(m, offset) << 8) | module_u8(m, offset + 1));
}

static uint32_t module_u32(const McModule* m, uint32_t offset) {
    return ((uint32_t)module_u16(m, offset) << 16) | module_u16(m, offset + 2);
}

// The target of the pc-relative displacement word at `offset`.
static uint32_t pc_relative_target(const McModule* m, uint32_t offset) {
    return offset + (uint32_t)(int32_t)(int16_t)module_u16(m, offset);
}

// The target of a bra.w at `offset`; 0 if there is none.
static uint32_t bra_target(const McModule* m, uint32_t offset) {
    return module_u16(m, offset) == OP_BRA_W ? pc_relative_target(m, offset + 2) : 0;
}

// The end of a scan of the code at [from, from + span): the span cut at the end of the file and of the 32-bit offsets.
static uint64_t scan_end(const McModule* m, uint32_t from, uint32_t span) {
    uint64_t end = (uint64_t)from + span;
    end = end < m->size ? end : m->size;
    return end < UINT32_MAX ? end : UINT32_MAX;
}

// The target of the `index`-th (from 0) `lea d16(pc),aN` with the opcode `lea_opcode` in the code at [from, from +
// span); 0 if there are fewer.
static uint32_t find_lea(const McModule* m, uint32_t from, uint32_t span, uint16_t lea_opcode, int index) {
    const uint64_t end = scan_end(m, from, span);
    for (uint64_t at = from; at + 4 <= end; at += 2) {
        if (module_u16(m, (uint32_t)at) == lea_opcode && index-- == 0) {
            return pc_relative_target(m, (uint32_t)at + 2);
        }
    }
    return 0;
}

// The sample at `offset`; null if it starts outside the file.
static const int8_t* module_sample(const McModule* m, uint32_t offset) {
    return offset == 0 || offset >= m->size ? nullptr : (const int8_t*)(m->data + offset);
}

// The silent block at the start of the sample data.
static const int8_t* silent_block(const McModule* m, uint16_t* words) {
    *words = SILENCE_WORDS;
    return module_sample(m, m->samples);
}

static McFlavor flavor_of(const uint8_t* data, size_t size) {
    return size >= CHECK_ENTRY_LATE + 2 && nuade_read_be16(data + CHECK_ENTRY_LATE) == OP_BRA_W ? McFlavor_Late
                                                                                                : McFlavor_Early;
}

bool mc_probe(const uint8_t* data, size_t size) {
    if (size < CHECK_LEA_LATE + 2 || nuade_read_be32(data) != MAGIC_HEADER
        || nuade_read_be16(data + 4) != OP_JMP_TABLE) {
        return false;
    }
    for (size_t entry = ENTRY_TABLE_EARLY; entry < ENTRY_TABLE_EARLY + ENTRY_COUNT * ENTRY_SIZE; entry += ENTRY_SIZE) {
        if (nuade_read_be16(data + entry) != OP_BRA_W) {
            return false;
        }
    }
    if (nuade_read_be16(data + CHECK_LEA_EARLY) == OP_LEA_A1_PC) {
        return true;
    }
    return nuade_read_be16(data + CHECK_ENTRY_LATE) == OP_BRA_W
           && nuade_read_be16(data + CHECK_LEA_LATE) == OP_LEA_A1_PC;
}

// The distance between the voice records of the variables: the tick steps from one record to the previous one with
// `lea -N(a5),a5`. 0 if there is no such step.
static int find_voice_stride(const McModule* m, uint32_t tick) {
    const uint64_t end = scan_end(m, tick, TICK_CODE_SPAN);
    for (uint64_t at = tick; at + 4 <= end; at += 2) {
        if (module_u16(m, (uint32_t)at) == OP_LEA_A5_A5) {
            return -(int16_t)module_u16(m, (uint32_t)at + 2);
        }
    }
    return 0;
}

// Finds the tables through the leas of the replayer's routines. The entries are the routines the eagleplayer calls:
// the tick, the subsong start and the start of the second (effects) subsong table, which ends the first.
static bool locate_tables(McModule* m) {
    const bool late = m->flavor == McFlavor_Late;
    const uint32_t first_entry = late ? ENTRY_TABLE_LATE : ENTRY_TABLE_EARLY;
    const uint32_t tick = bra_target(m, first_entry);
    const uint32_t start = bra_target(m, first_entry + ENTRY_SIZE);
    const uint32_t effects = bra_target(m, first_entry + 2 * ENTRY_SIZE);
    if (tick == 0 || start == 0 || effects == 0) {
        return false;
    }
    m->subsongs = find_lea(m, start, ENTRY_CODE_SPAN, OP_LEA_A1_PC, 0);
    const uint32_t subsongs_end = find_lea(m, effects, ENTRY_CODE_SPAN, OP_LEA_A1_PC, 0);
    m->state = find_lea(m, tick, TICK_CODE_SPAN, OP_LEA_A3_PC, 0);
    m->sequences = find_lea(m, tick, TICK_CODE_SPAN, OP_LEA_A0_PC, 0);
    m->instruments = find_lea(m, tick, TICK_CODE_SPAN, OP_LEA_A0_PC, 1);
    m->samples = find_lea(m, tick, TICK_CODE_SPAN, OP_LEA_A1_PC, 0);
    m->periods = find_lea(m, tick, TICK_CODE_SPAN, OP_LEA_A0_PC, late ? 3 : 2);
    m->voice_stride = find_voice_stride(m, tick);
    if (m->subsongs == 0 || subsongs_end <= m->subsongs || m->state == 0 || m->sequences == 0 || m->instruments == 0
        || m->samples == 0 || m->periods == 0 || m->voice_stride == 0) {
        return false;
    }
    const uint32_t count = (subsongs_end - m->subsongs) / SUBSONG_RECORD_SIZE;
    if (count == 0 || count > SUBSONG_MAX || m->subsongs + count * SUBSONG_RECORD_SIZE > m->size) {
        return false;
    }
    m->subsong_count = (int)count;
    return true;
}

McPlayer* mc_create(const uint8_t* data, size_t size) {
    if (!mc_probe(data, size)) {
        return nullptr;
    }
    McPlayer* player = (McPlayer*)calloc(1, sizeof(McPlayer));
    if (player == nullptr) {
        return nullptr;
    }
    McModule* m = &player->module;
    m->data = nuade_paula_padded_copy(data, size);
    if (m->data == nullptr) {
        free(player);
        return nullptr;
    }
    m->size = size;
    m->flavor = flavor_of(data, size);
    if (!locate_tables(m)) {
        mc_destroy(player);
        return nullptr;
    }
    return player;
}

void mc_destroy(McPlayer* player) {
    if (player != nullptr) {
        free(player->module.data);
    }
    free(player);
}

int mc_subsong_count(const McPlayer* player) {
    return player->module.subsong_count;
}

// Records

// The instrument record `number`: the instrument table holds the distance of each record from its own entry.
static McInstrument read_instrument(const McModule* m, int number) {
    const uint32_t entry = m->instruments + 2u * (uint32_t)number;
    const uint32_t at = pc_relative_target(m, entry);
    McInstrument ins = { 0 };
    const uint32_t sample = module_u32(m, at);
    const uint32_t loop = module_u32(m, at + 4);
    const uint32_t volume_envelope = module_u32(m, at + 8);
    const uint32_t pitch_envelope = module_u32(m, at + 12);
    ins.sample = sample != 0 ? m->samples + sample : 0;
    ins.loop = loop != 0 ? m->samples + loop : 0;
    ins.volume_envelope = volume_envelope != 0 ? at + 8 + volume_envelope : 0;
    ins.pitch_envelope = pitch_envelope != 0 ? at + 12 + pitch_envelope : 0;
    ins.sample_words = module_u16(m, at + 16);
    ins.loop_words = module_u16(m, at + 18);
    ins.period_scale = module_u16(m, at + 20);
    ins.period_scale_retune = module_u16(m, at + 22);
    ins.volume_envelope_speed = module_u8(m, at + 24);
    ins.pitch_envelope_speed = module_u8(m, at + 25);
    ins.gain = module_u16(m, at + 26);
    return ins;
}

// The period of pitch step `step`.
static uint16_t period_at(const McModule* m, uint16_t step) {
    return module_u16(m, m->periods + 2u * step);
}

// Volume

// AUDxVOL: half the level, at most 64 (the patched replayer).
static uint8_t hardware_volume(uint16_t level) {
    return (uint8_t)(level >> 1 > VOLUME_MAX ? VOLUME_MAX : level >> 1);
}

static void set_volume(McVoice* v, uint16_t level) {
    v->paula.volume = hardware_volume(level);
}

// Starting a note

// period * scale / 0x10000, the period of a note at the instrument's tuning.
static uint16_t scaled_period(uint16_t period, uint16_t scale) {
    return (uint16_t)(((uint32_t)period * scale) >> 16);
}

// A note with a sample restarts the channel and plays the first block once, then the repeat block; a note without one
// only retunes the running sample.
static void start_sample(const McModule* m, McVoice* v, uint16_t period) {
    const McInstrument* ins = &v->instrument;
    const int8_t* loop = module_sample(m, ins->loop);
    if (ins->sample == 0) {
        v->retune_only = true;
        v->paula.lc = loop;
        v->paula.len_words = ins->loop_words;
        v->paula.period = scaled_period(period, ins->period_scale_retune);
        return;
    }
    v->retune_only = false;
    nuade_paula_channel_dma_off(&v->paula);
    v->paula.lc = module_sample(m, ins->sample);
    v->paula.len_words = ins->sample_words;
    v->paula.period = scaled_period(period, ins->period_scale);
    nuade_paula_channel_dma_on(&v->paula);
    // The replayer waits for the DMA start before it writes the repeat
    if (loop != nullptr) {
        v->paula.lc = loop;
        v->paula.len_words = ins->loop_words;
    } else {
        v->paula.lc = silent_block(m, &v->paula.len_words);
    }
}

// A note byte: pitch (with the transposition of the sub-sequence), instrument, length and a byte that nothing reads.
static void start_note(const McModule* m, McVoice* v, uint8_t note_byte) {
    const uint16_t pitch = (uint16_t)((note_byte + v->transpose) * STEPS_PER_NOTE);
    v->glide_step = 0;
    if (v->glide) {
        v->start_pitch = v->target_pitch;
        v->pitch = v->target_pitch;
        const int32_t distance = ((int32_t)(int16_t)(pitch - v->start_pitch)) * (1 << PITCH_FRACTION_BITS);
        if (v->glide_ticks != 0) {
            v->glide_step = (int16_t)(distance / (int16_t)v->glide_ticks);
        }
    } else {
        v->start_pitch = pitch;
        v->pitch = pitch;
    }
    v->target_pitch = pitch;
    v->instrument_number = module_u8(m, v->stream);
    v->length = module_u8(m, v->stream + 1);
    v->stream += 3;
    v->instrument = read_instrument(m, v->instrument_number);
    v->volume_envelope_at = v->instrument.volume_envelope;
    v->pitch_envelope_at = v->instrument.pitch_envelope;
    v->volume_envelope_wait = 0;
    v->pitch_envelope_wait = 0;
    set_volume(v, (uint16_t)((uint16_t)((uint32_t)v->instrument.gain * v->volume_scale) >> 8));
    const uint16_t period = period_at(m, pitch);
    v->shown_note = period != 0 ? nuade_period_to_note(period) : -1;
    start_sample(m, v, period);
}

// Commands

// Pushes a call of sub-sequence `sequence`, repeated `repeats` more times; false when the stack is full.
static bool call_sequence(const McModule* m, McVoice* v, uint8_t sequence, int8_t transpose, uint8_t repeats) {
    if (v->depth == MC_STACK_FRAMES) {
        return false;
    }
    const uint32_t target = pc_relative_target(m, m->sequences + 2u * sequence);
    v->transpose = transpose;
    v->stack[v->depth++] = (McStackFrame) { .repeats_left = repeats, .loop_start = target, .resume = v->stream };
    v->stream = target;
    return true;
}

// The end of a sub-sequence: play it again while repeats are left, else go back to the caller.
static bool return_from_sequence(McVoice* v) {
    if (v->depth == 0) {
        return false;
    }
    McStackFrame* frame = &v->stack[v->depth - 1];
    if (--frame->repeats_left >= 0) {
        v->stream = frame->loop_start;
        return true;
    }
    v->stream = frame->resume;
    v->depth--;
    v->transpose = 0;
    return true;
}

typedef enum StepResult {
    StepResult_Continue, // read the next command
    StepResult_Note,     // a note started: the envelopes run
    StepResult_Done,     // the tick is over for this voice
    StepResult_Stop,     // the stream is broken: the voice stops
} StepResult;

static StepResult end_voice(const McModule* m, McVoice* v) {
    v->stream = 0;
    v->paula.lc = silent_block(m, &v->paula.len_words);
    v->paula.volume = 0;
    return StepResult_Done;
}

// The patched replayer reports every end and jump command it decodes (with bit 7 clear); when all voices have reported
// one, the song has ended and the reports start over.
static void report_end_command(McPlayer* p, McVoice* v) {
    v->reached_end = true;
    for (int i = 0; i < MC_VOICES; i++) {
        if (!p->voices[i].reached_end) {
            return;
        }
    }
    p->song_ended = true;
    for (int i = 0; i < MC_VOICES; i++) {
        p->voices[i].reached_end = false;
    }
}

static StepResult step_command(McPlayer* p, McVoice* v) {
    const McModule* m = &p->module;
    const uint8_t first = module_u8(m, v->stream);
    if (first == McCommand_End || first == McCommand_Jump) {
        report_end_command(p, v);
    }
    const uint8_t code = first & NOTE_BIT_MASK;
    if (code < COMMAND_BASE) {
        v->stream++;
        start_note(m, v, first);
        return StepResult_Note;
    }
    const uint32_t args = v->stream + 1;
    switch (code) {
        case McCommand_Wait:
            v->length = module_u16(m, args);
            v->stream = args + 3;
            v->paula.volume = 0;
            return StepResult_Done;
        case McCommand_Volume:
            v->volume_scale = module_u8(m, args);
            v->stream = args + 1;
            return StepResult_Continue;
        case McCommand_Call:
        case McCommand_CallRepeat: {
            const bool repeat = code == McCommand_CallRepeat;
            v->stream = args + (repeat ? 3 : 2);
            const uint8_t repeats = repeat ? module_u8(m, args + 2) : 0;
            return call_sequence(m, v, module_u8(m, args), (int8_t)module_u8(m, args + 1), repeats)
                       ? StepResult_Continue
                       : StepResult_Stop;
        }
        case McCommand_Return:
            return return_from_sequence(v) ? StepResult_Continue : StepResult_Stop;
        case McCommand_End:
            return end_voice(m, v);
        case McCommand_Jump:
            v->stream += (uint32_t)(int32_t)(int16_t)module_u16(m, args);
            return StepResult_Continue;
        case McCommand_GlideOn:
            v->glide = true;
            v->glide_ticks = module_u8(m, args);
            v->stream = args + 1;
            return StepResult_Continue;
        case McCommand_GlideOff:
            v->glide = false;
            v->stream = args;
            return StepResult_Continue;
        default:
            // The replayer reads the same byte forever
            return StepResult_Stop;
    }
}

// Envelopes and glide

// The volume envelope: a list of levels, one per `speed` ticks; 0 ends it and the last level stays.
static void run_volume_envelope(const McModule* m, McVoice* v) {
    if (v->volume_envelope_at == 0) {
        return;
    }
    if (v->volume_envelope_wait-- > 1) {
        return;
    }
    v->volume_envelope_wait = v->instrument.volume_envelope_speed;
    const uint8_t level = module_u8(m, v->volume_envelope_at);
    if (level == 0) {
        v->volume_envelope_at = 0;
        return;
    }
    v->volume_envelope_at++;
    const uint32_t product = (uint32_t)(level + 1u) * v->volume_scale;
    set_volume(v, (uint16_t)(((product & 0xFFFFu) * v->instrument.gain) >> 16));
}

// The glide moves the pitch by `glide_step` sixteenths of a step per tick (the fraction is dropped every tick).
static void run_glide(McVoice* v) {
    if (v->glide_step == 0) {
        return;
    }
    uint16_t position = (uint16_t)((uint16_t)(v->pitch << PITCH_FRACTION_BITS) + (uint16_t)v->glide_step);
    if ((int16_t)position < 0) {
        position = 0;
    }
    position >>= PITCH_FRACTION_BITS;
    const bool rising = v->glide_step > 0;
    if (rising ? position >= v->target_pitch : position <= v->target_pitch) {
        v->pitch = v->target_pitch;
        v->glide_step = 0;
    } else {
        v->pitch = position;
    }
}

// The pitch envelope adds a signed step to the pitch for one tick; 0x80 ends it. Without one the period follows the
// glide.
static void run_pitch(const McModule* m, McVoice* v) {
    bool update = false;
    uint16_t pitch = v->pitch;
    if (v->pitch_envelope_at != 0 && v->pitch_envelope_wait-- <= 1) {
        v->pitch_envelope_wait = v->instrument.pitch_envelope_speed;
        const uint8_t step = module_u8(m, v->pitch_envelope_at);
        if (step == 0x80) {
            v->pitch_envelope_at = 0;
        } else {
            v->pitch_envelope_at++;
            pitch = (uint16_t)(v->pitch + (int8_t)step);
            update = true;
        }
    }
    if (!update) {
        update = v->start_pitch != v->target_pitch;
    }
    if (!update) {
        return;
    }
    const uint16_t scale = v->retune_only ? v->instrument.period_scale_retune : v->instrument.period_scale;
    v->paula.period = scaled_period(period_at(m, pitch), scale);
}

// The tick

static void run_voice(McPlayer* p, McVoice* v) {
    const McModule* m = &p->module;
    // The Early build counts the length down before it looks at the stream, so the envelopes of a voice that has ended
    // go on running while the length (wrapped to 0xFFFF) is 2 or more; the Late build looks at the stream first.
    if (v->stream == 0 && m->flavor == McFlavor_Late) {
        return;
    }
    const bool read_commands = v->length < 2;
    v->length--;
    if (read_commands) {
        if (v->stream == 0) {
            return;
        }
        StepResult result = StepResult_Continue;
        for (int steps = 0; result == StepResult_Continue; steps++) {
            result = steps < COMMANDS_PER_TICK ? step_command(p, v) : StepResult_Stop;
        }
        if (result == StepResult_Stop) {
            v->stream = 0;
        }
        if (result != StepResult_Note) {
            return;
        }
    }
    run_volume_envelope(m, v);
    run_glide(v);
    run_pitch(m, v);
}

McTickResult mc_tick(McPlayer* player, NuadeVoices* voices) {
    player->song_ended = false;
    for (int i = MC_VOICES - 1; i >= 0; i--) {
        run_voice(player, &player->voices[i]);
    }
    bool running = false;
    for (int i = 0; i < MC_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, (uint32_t)i, voices);
        running = running || player->voices[i].stream != 0;
    }
    return (McTickResult) { .looped = player->song_ended, .ended = !running };
}

// What a voice's record in the replayer's variables holds that matters at the start: the voice volume that notes play
// at until the first volume command (the record holds a snapshot of the state the game was in; every other field is
// written before it is read).
typedef struct VoiceSnapshot {
    uint16_t volume_scale;
} VoiceSnapshot;

static VoiceSnapshot read_voice_snapshot(const McModule* m, int voice) {
    const uint32_t record = m->state + VARIABLES_HEADER_SIZE + (uint32_t)(voice * m->voice_stride);
    return (VoiceSnapshot) { .volume_scale = module_u16(m, record + 0x48) };
}

bool mc_start(McPlayer* player, int subsong, NuadeVoices* voices) {
    McModule* m = &player->module;
    if (subsong < 0 || subsong >= m->subsong_count) {
        return false;
    }
    for (int i = 0; i < MC_VOICES; i++) {
        McVoice* v = &player->voices[i];
        const uint32_t entry = m->subsongs + (uint32_t)subsong * SUBSONG_RECORD_SIZE + 4u * (uint32_t)i;
        const uint32_t distance = module_u32(m, entry);
        *v = (McVoice) { .stream = distance != 0 ? entry + distance : 0,
                         .volume_scale = read_voice_snapshot(m, i).volume_scale,
                         .instrument_number = -1,
                         .shown_note = -1 };
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

McVoiceState mc_voice_state(const McPlayer* player, int voice) {
    const McVoice* v = &player->voices[voice];
    return (McVoiceState) { .note = v->shown_note, .instrument = v->instrument_number };
}
