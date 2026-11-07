// SPDX-License-Identifier: MIT
// Sound Programming Language player, translated from the replayers inside the "*.spl" modules (Holger Gehrmann's
// sound routines of Amiga games) and the UADE eagleplayer "SoundProgrammingLanguage", which finds the module's init
// and play routines in its header and patches the replayer to report the end of the song.
//
// Each module is a program: a code hunk with a replayer that writes the Paula registers itself. The replayer is one
// design in several builds (see spl_builds.c; spl_module.c identifies the build by a hash of the module up to its
// samples). Four voices each read a command stream: lengths (0xC0..0xEF), notes, calls (0xF1, 0xF2), jumps (0xF3), a
// copy of an instrument record into the voice's record (0xF0) and no-ops (0xF4). A note starts the sample with the
// Paula registers written in the order of the original: LEN and PER get 1 first, the DMA goes off, PER, LC, LEN and
// VOL are written and the DMA goes on. A second routine runs every tick and animates the notes: it swaps or rotates
// the last periods (an arpeggio), lowers the volume and restarts the sample at falling volume.
//
// The eagleplayer's patches that matter: the f3 handler reports a voice that jumps back, and the song ends when all
// four have (SongEnd). The delay loops between the register writes become raster waits and the volume writes go
// through the master volume (identity): neither changes what is written.

#include <nuade/players/spl.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "spl_module.h"
#include <stdlib.h>

#define MAX_CALL_DEPTH 4096        // the stacks live in the module's memory and run into whatever lies behind them
#define MAX_COMMANDS_PER_STEP 4096 // a stream that reads this many commands without a note hangs the replayer
#define HOLD_PERIOD 0x1AC          // the period the init gives the startup sample
#define FULL_VOLUME 0x40

// Command bytes
#define CMD_COPY_RECORD 0xF0
#define CMD_CALL 0xF1
#define CMD_RETURN 0xF2
#define CMD_JUMP 0xF3
#define CMD_SKIP 0xF4
#define CMD_STOP 0xF5
#define CMD_TRANSPOSE 0xF6
#define CMD_SKIP_FE 0xFE
#define CMD_CLASS_MASK 0xF0
#define CMD_LENGTH_FIRST 0xC0
#define CMD_LENGTH_LAST 0xE0 // the class bytes 0xC0, 0xD0 and 0xE0 set a length

// Effect flags of a record
#define FLAG_PERIOD 0x01
#define FLAG_ROTATE 0x02
#define FLAG_DECAY 0x04
#define FLAG_RELEASE 0x08
#define FLAG_EXTRA_NOTES 0x01 // the same bit: the effect flag of Table/Full notes also says more notes follow
#define FLAG_MORE_NOTES 0x20

#define DECAY_STEP 8
#define SWING_HALF 3

static uint8_t read_u8(const SplPlayer* p, uint32_t at) {
    return spl_module_u8(p->module, at);
}

static uint16_t read_u16(const SplPlayer* p, uint32_t at) {
    return spl_module_u16(p->module, at);
}

static uint32_t read_u32(const SplPlayer* p, uint32_t at) {
    return spl_module_u32(p->module, at);
}

// Instrument records

// The 0xF0 command copies a record from the stream. These are the fields of each family's record.

static void load_record_full(const SplPlayer* p, SplRecord* r, uint32_t at) {
    r->sample = read_u32(p, at);
    r->length = read_u16(p, at + 0x04);
    r->octave = read_u16(p, at + 0x0C);
    r->flags = read_u16(p, at + 0x0E);
    r->period_speed = read_u16(p, at + 0x10);
    r->decay_speed = read_u16(p, at + 0x12);
    r->release_speed = read_u16(p, at + 0x14);
    r->release_step = read_u16(p, at + 0x16);
    r->volume = read_u16(p, at + 0x1E);
    r->transpose = read_u16(p, at + 0x20);
}

static void load_record_tiny(const SplPlayer* p, SplRecord* r, uint32_t at) {
    r->sample = read_u32(p, at);
    r->length = read_u16(p, at + 0x04);
    r->octave = read_u8(p, at + 0x08);
    r->volume = read_u8(p, at + 0x0F);
}

static void load_record_table(const SplPlayer* p, SplRecord* r, uint32_t at) {
    r->sample_number = read_u16(p, at);
    r->loop_start = read_u16(p, at + 0x02);
    r->octave = read_u16(p, at + 0x04);
    r->flags = read_u16(p, at + 0x06);
    r->period_speed = read_u16(p, at + 0x08);
    r->decay_speed = read_u16(p, at + 0x0A);
    r->release_speed = read_u16(p, at + 0x0C);
    r->release_step = read_u16(p, at + 0x0E);
    r->start_offset = read_u16(p, at + 0x14);
    r->volume = read_u16(p, at + 0x16);
    r->transpose = read_u16(p, at + 0x18);
}

// Bytes a record takes in the stream.
static uint32_t record_bytes(SplFamily family) {
    switch (family) {
        case SplFamily_Tiny:
            return 0x10;
        case SplFamily_Table:
            return 0x1A;
        default:
            return 0x22;
    }
}

static void load_record(const SplPlayer* p, SplRecord* r, uint32_t at) {
    switch (p->module->build->family) {
        case SplFamily_Tiny:
            load_record_tiny(p, r, at);
            break;
        case SplFamily_Table:
            load_record_table(p, r, at);
            break;
        default:
            load_record_full(p, r, at);
            break;
    }
}

// Notes

// Moves a note byte (octave in the high nibble, semitone in the low) up by `steps` semitones.
static uint8_t transpose_note(uint8_t note, uint8_t steps) {
    for (; steps != 0; steps--) {
        note++;
        if ((note & 0x0F) == 0x0C) {
            note = (uint8_t)((note + 0x10) & 0xF0);
        }
    }
    return note;
}

// The period of a note byte after the record's octave shift: a table entry halved once for each octave.
static uint16_t note_period(const SplPlayer* p, const SplRecord* r, uint8_t note) {
    const uint8_t shifted = (uint8_t)(note + (uint8_t)(r->octave << 4));
    const uint16_t entry
        = read_u16(p, spl_module_address(p->module, p->module->build->period_table) + 2u * (shifted & 0x0F));
    return (uint16_t)(entry >> (shifted >> 4));
}

static void push_history(SplRecord* r, uint16_t period, uint32_t depth) {
    for (uint32_t i = depth - 1; i > 0; i--) {
        r->history[i] = r->history[i - 1];
    }
    r->history[0] = period;
}

static void clear_run_time(SplRecord* r) {
    r->period_timer = 0;
    r->decay_timer = 0;
    r->current_volume = 0;
    r->release_timer = 0;
}

// A note of the Full, Lite and Tiny families.
static void start_note_plain(SplPlayer* p, uint32_t index, uint8_t note) {
    const SplBuild* build = p->module->build;
    SplVoice* v = &p->voices[index];
    SplRecord* r = &v->record;
    NuadePaulaChannel* paula = &v->paula;
    paula->len_words = 1;
    paula->period = 1;
    const uint8_t steps = build->family == SplFamily_Tiny ? p->transpose : (uint8_t)(p->transpose + r->transpose);
    const uint8_t moved = transpose_note(note, steps);
    clear_run_time(r);
    if (build->family == SplFamily_Full) {
        r->current_volume = r->volume;
        r->release_volume = r->volume;
        r->release_timer = r->release_speed;
    }
    const uint16_t period = note_period(p, r, moved);
    nuade_paula_channel_dma_off(paula);
    paula->period = period;
    if (build->family == SplFamily_Full) {
        push_history(r, period, SPL_HISTORY);
    } else if (build->family == SplFamily_Lite) {
        push_history(r, period, 2);
    }
    paula->lc = spl_module_chip_pointer(
        p->module,
        r->sample + (build->family == SplFamily_Full ? spl_module_address(p->module, build->sample_base) : 0));
    paula->len_words = r->length;
    paula->volume = (uint8_t)r->volume;
    nuade_paula_channel_dma_on(paula);
}

// A note of the Table family: the sample comes from the tables, its first long is cleared, and the loop follows.
static void start_note_table(SplPlayer* p, uint32_t index, uint8_t note) {
    const SplBuild* build = p->module->build;
    SplVoice* v = &p->voices[index];
    SplRecord* r = &v->record;
    NuadePaulaChannel* paula = &v->paula;
    if (note == 0) {
        return;
    }
    const uint16_t size
        = read_u16(p, spl_module_address(p->module, build->size_table) + (uint16_t)(r->sample_number << 1));
    const uint32_t address
        = read_u32(p, spl_module_address(p->module, build->address_table) + (uint16_t)(r->sample_number << 2));
    paula->len_words = 1;
    paula->period = 1;
    const uint8_t moved = transpose_note(note, (uint8_t)(p->transpose + r->transpose));
    clear_run_time(r);
    r->current_volume = r->volume;
    r->release_volume = r->volume;
    r->release_timer = r->release_speed;
    const uint16_t period = note_period(p, r, moved);
    nuade_paula_channel_dma_off(paula);
    paula->period = period;
    push_history(r, period, SPL_HISTORY);
    paula->lc = spl_module_chip_pointer(p->module, address + r->start_offset);
    spl_module_write32(p->module, address, 0);
    paula->len_words = (uint16_t)((uint16_t)(size - r->start_offset) >> 1);
    paula->volume = (uint8_t)r->volume;
    nuade_paula_channel_dma_on(paula);
    const uint16_t loop = (uint16_t)(r->loop_start >> 1);
    if (loop != 0) {
        paula->len_words = (uint16_t)(size - loop);
        paula->lc = spl_module_chip_pointer(p->module, address + 2u * loop);
    } else {
        paula->len_words = 1;
    }
}

static void start_note(SplPlayer* p, uint32_t index, uint8_t note) {
    const SplBuild* build = p->module->build;
    if (build->silent_middle_voices && (index == 1 || index == 2)) {
        return;
    }
    if (build->family == SplFamily_Table) {
        start_note_table(p, index, note);
    } else {
        start_note_plain(p, index, note);
    }
}

// Effects, once a tick before the streams

// The period swap or rotation of the effects.
static void run_period_effect(SplVoice* v, bool rotate) {
    SplRecord* r = &v->record;
    if (r->period_timer != 0) {
        r->period_timer--;
        return;
    }
    r->period_timer = r->period_speed;
    const uint16_t last = r->history[0];
    if (rotate) {
        r->history[0] = r->history[3];
        r->history[3] = r->history[2];
        r->history[2] = r->history[1];
        r->history[1] = last;
    } else {
        r->history[0] = r->history[1];
        r->history[1] = last;
    }
    v->paula.period = r->history[1];
}

static void run_decay_effect(SplVoice* v) {
    SplRecord* r = &v->record;
    if (r->decay_timer != 0) {
        r->decay_timer--;
        return;
    }
    r->decay_timer = r->decay_speed;
    if (r->current_volume != 0) {
        r->current_volume = (uint16_t)(r->current_volume - DECAY_STEP);
        v->paula.volume = (uint8_t)r->current_volume;
    }
}

// The release lowers the volume and restarts the sample at the new volume.
static void run_release_effect(const SplPlayer* p, SplVoice* v) {
    SplRecord* r = &v->record;
    NuadePaulaChannel* paula = &v->paula;
    if (r->release_timer != 0) {
        r->release_timer--;
        return;
    }
    r->release_timer = r->release_speed;
    if (r->release_volume == 0) {
        return;
    }
    r->release_volume = (uint16_t)(r->release_volume - r->release_step);
    if ((int16_t)r->release_volume < 0) {
        r->release_volume = 0;
    }
    paula->volume = (uint8_t)r->release_volume;
    r->current_volume = r->release_volume;
    paula->len_words = 1;
    paula->period = 1;
    nuade_paula_channel_dma_off(paula);
    if (p->module->build->family == SplFamily_Table) {
        paula->len_words = read_u16(p, spl_module_address(p->module, p->module->build->size_table)
                                           + (uint16_t)(r->sample_number << 1));
    } else {
        paula->len_words = r->length;
    }
    paula->period = r->history[0];
    nuade_paula_channel_dma_on(paula);
}

static void run_effects(SplPlayer* p) {
    const SplFamily family = p->module->build->family;
    if (family == SplFamily_Tiny) {
        return;
    }
    for (uint32_t i = 0; i < p->voice_count; i++) {
        SplVoice* v = &p->voices[i];
        const uint16_t flags = v->record.flags;
        if ((flags & FLAG_PERIOD) != 0) {
            run_period_effect(v, family != SplFamily_Lite && (flags & FLAG_ROTATE) != 0);
        }
        if (family == SplFamily_Lite) {
            continue;
        }
        if ((flags & FLAG_DECAY) != 0) {
            run_decay_effect(v);
        }
        if ((flags & FLAG_RELEASE) != 0) {
            run_release_effect(p, v);
        }
    }
}

// Command streams

// A voice that jumps back (or to the byte behind its jump command) has finished the song once; the eagleplayer's
// SongEnd comes when all four have.
static void note_jump(SplPlayer* p, uint32_t index, uint32_t operand_at, uint32_t target) {
    if ((int32_t)operand_at < (int32_t)target) {
        return;
    }
    p->voices[index].alive = false;
    for (uint32_t i = 0; i < SPL_VOICES; i++) {
        if (p->voices[i].alive) {
            return;
        }
    }
    p->looped = true;
    for (uint32_t i = 0; i < SPL_VOICES; i++) {
        p->voices[i].alive = true;
    }
}

// Number of notes that follow the first one of a note command, from the record's flags.
static uint32_t extra_notes(const SplBuild* build, uint16_t flags) {
    if (build->family == SplFamily_Tiny || (flags & FLAG_EXTRA_NOTES) == 0) {
        return 0;
    }
    if (build->family == SplFamily_Lite) {
        return 1;
    }
    return (flags & FLAG_MORE_NOTES) != 0 ? 3 : 1;
}

// Where entry `depth` of voice `index`'s return stack lies.
static uint32_t stack_slot(const SplPlayer* p, uint32_t index, uint32_t depth) {
    return spl_module_address(p->module, p->module->build->stacks[index]) + 4 * depth;
}

// Reads commands of voice `index` until a note ends its step. False when the song has stopped.
static bool step_voice(SplPlayer* p, uint32_t index) {
    const SplBuild* build = p->module->build;
    SplVoice* v = &p->voices[index];
    if (v->halted) {
        return true;
    }
    if (v->length_left != 0) {
        v->length_left--;
        if (v->length_left != 0) {
            return true;
        }
    }
    for (uint32_t commands = 0; commands < MAX_COMMANDS_PER_STEP; commands++) {
        const uint32_t at = v->stream_at;
        const uint8_t byte = read_u8(p, at);
        if (byte == CMD_SKIP || (build->skip_fe && byte == CMD_SKIP_FE)) {
            v->stream_at = at + 1;
        } else if (byte == CMD_JUMP) {
            const uint32_t target = read_u32(p, at + 1);
            v->stream_at = target;
            note_jump(p, index, at + 1, target);
        } else if (byte == CMD_CALL) {
            if (v->depth >= MAX_CALL_DEPTH) {
                v->halted = true;
                return true;
            }
            spl_module_write32(p->module, stack_slot(p, index, v->depth), at + 1);
            v->depth++;
            v->stream_at = read_u32(p, at + 1);
        } else if (byte == CMD_RETURN) {
            if (v->depth == 0) {
                v->halted = true;
                return true;
            }
            v->depth--;
            v->stream_at = read_u32(p, stack_slot(p, index, v->depth)) + 4;
        } else if (byte == CMD_COPY_RECORD) {
            load_record(p, &v->record, at + 1);
            v->stream_at = at + 1 + record_bytes(build->family);
        } else if (build->stop_f5 && byte == CMD_STOP) {
            p->playing = false;
            return false;
        } else if (build->stop_f5 && byte == CMD_TRANSPOSE) {
            p->transpose = read_u8(p, at + 1);
            v->stream_at = at + 2;
        } else if ((byte & CMD_CLASS_MASK) >= CMD_LENGTH_FIRST && (byte & CMD_CLASS_MASK) <= CMD_LENGTH_LAST) {
            v->length_left = (uint8_t)(byte - CMD_LENGTH_FIRST);
            v->length_reload = v->length_left;
            v->stream_at = at + 1;
        } else {
            start_note(p, index, byte);
            const uint32_t more = extra_notes(build, v->record.flags);
            for (uint32_t i = 0; i < more; i++) {
                v->stream_at++;
                start_note(p, index, read_u8(p, v->stream_at));
            }
            v->stream_at++;
            v->length_left = v->length_reload;
            return true;
        }
    }
    v->halted = true;
    return true;
}

static void step_streams(SplPlayer* p) {
    for (uint32_t i = 0; i < p->voice_count; i++) {
        if (!step_voice(p, i)) {
            return;
        }
    }
}

// Whether the streams advance in this tick.
static bool divider_fires(SplPlayer* p) {
    const SplBuild* build = p->module->build;
    uint16_t reload = build->divider_reload;
    uint16_t half = SWING_HALF;
    switch (build->divider) {
        case SplDivider_Every:
            return true;
        case SplDivider_Bits:
            p->ticks++;
            return ((build->pattern >> (p->ticks & 0x1F)) & 1) != 0;
        case SplDivider_Tempo:
            reload = p->song->tempo;
            half = (uint16_t)(reload >> 1);
            break;
        case SplDivider_Phased:
            reload = 6;
            break;
        case SplDivider_Swing:
            break;
    }
    if (p->divider == half) {
        p->divider--;
        return true;
    }
    if (p->divider == 0) {
        p->divider = reload;
        if (build->divider == SplDivider_Phased) {
            p->phase = (p->phase + 1) & 3;
            if (p->phase != 0) {
                p->divider = 7;
            }
        }
        return true;
    }
    p->divider--;
    return false;
}

// Starting

static void start_voice(SplPlayer* p, uint32_t index) {
    SplVoice* v = &p->voices[index];
    *v = (SplVoice) { .stream_at = spl_module_address(p->module, p->song->streams[index]), .alive = true };
}

// The init routine's hardware part: DMA on for some voices, and a startup sample on voice 0.
static void start_hardware(SplPlayer* p) {
    const SplBuild* build = p->module->build;
    const SplStartup* startup = &build->startup;
    SplVoice* v0 = &p->voices[0];
    if (build->family == SplFamily_Tiny) {
        v0->record.octave = startup->record_octave;
    } else if (build->family != SplFamily_Table) {
        v0->record.octave = (uint16_t)(startup->record_octave << 8);
    }
    if (startup->plays_sample) {
        v0->record.sample = spl_module_address(p->module, startup->sample);
        v0->record.length = (uint16_t)(read_u16(p, spl_module_address(p->module, startup->length_cell)) >> 1);
        v0->paula.lc = spl_module_chip_pointer(p->module, spl_module_address(p->module, startup->sample));
        v0->paula.len_words = v0->record.length;
        v0->paula.period = HOLD_PERIOD;
        v0->paula.volume = FULL_VOLUME;
    } else {
        v0->record.sample = spl_module_address(p->module, startup->record_sample);
    }
    for (uint32_t i = 0; i < SPL_VOICES; i++) {
        if (startup->dma_on[i]) {
            nuade_paula_channel_dma_on(&p->voices[i].paula);
        }
    }
}

bool spl_start(SplPlayer* p, int subsong, NuadeVoices* voices) {
    const SplBuild* build = p->module->build;
    if (subsong < 0 || (uint32_t)subsong >= build->song_count) {
        return false;
    }
    spl_module_reset(p->module);
    p->song = &build->songs[subsong];
    p->voice_count = p->song->voices != 0 ? p->song->voices : SPL_VOICES;
    p->transpose = p->song->sets_transpose ? p->song->transpose : build->transpose;
    p->divider = 0;
    p->phase = 0;
    p->ticks = 0;
    p->playing = true;
    p->looped = false;
    for (uint32_t i = 0; i < SPL_VOICES; i++) {
        start_voice(p, i);
    }
    start_hardware(p);
    for (uint32_t i = 0; i < SPL_VOICES; i++) {
        nuade_voice_stop(voices, i);
        nuade_paula_channel_sync(&p->voices[i].paula, i, voices);
    }
    return true;
}

SplTickResult spl_tick(SplPlayer* p, NuadeVoices* voices) {
    p->looped = false;
    if (p->playing) {
        run_effects(p);
    }
    if (divider_fires(p)) {
        step_streams(p);
    }
    for (uint32_t i = 0; i < SPL_VOICES; i++) {
        nuade_paula_channel_sync(&p->voices[i].paula, i, voices);
    }
    return (SplTickResult) { .looped = p->looped };
}

// Opening

SplPlayer* spl_create(const uint8_t* data, size_t size) {
    SplModule* module = spl_module_open(data, size);
    if (module == nullptr) {
        return nullptr;
    }
    SplPlayer* player = (SplPlayer*)calloc(1, sizeof(SplPlayer));
    if (player == nullptr) {
        spl_module_close(module);
        return nullptr;
    }
    player->module = module;
    return player;
}

bool spl_probe(const uint8_t* data, size_t size) {
    SplModule* module = spl_module_open(data, size);
    const bool supported = module != nullptr;
    spl_module_close(module);
    return supported;
}

void spl_destroy(SplPlayer* player) {
    if (player != nullptr) {
        spl_module_close(player->module);
    }
    free(player);
}

int spl_subsong_count(const SplPlayer* player) {
    return (int)player->module->subsong_count;
}

uint32_t spl_tick_length(const SplPlayer* player) {
    return player->module->timer;
}
