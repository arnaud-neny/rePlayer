// SPDX-License-Identifier: MIT
// Sound Master player: the song. One tick is the original's play routine: the rows of the voices are read every
// `speed` ticks, the notes start, every voice's effects run (sm_voice.c), the position advances and the DMACON writes
// of the tick are made. The seven code versions share that structure; SmstVersion lists where they differ.

#include "sm_internal.h"
#include <stdlib.h>
#include <string.h>

#define MAX_MODULE_BYTES 0x200000u // the chip memory UADE has for a module
#define DUMMY_PERIOD 0xA0
#define FADE_STEPS_MASK 0x7F
#define ROW_SPEED 0xFD
#define ROW_END 0xFE
#define POSITION_FADE_BIT 0x80
#define POSITION_FADE_OUT 0x40
#define POSITION_FADE_ZERO 0x20
#define POSITION_FADE_SPEED 0x1F
#define FIXED_COLUMN 0x40u
#define FIXED_TRANSPOSE_COLUMN 0x400u
#define STEP_NONE 0xFF // no arpeggio step: what the init's position load leaves, except after a fade start

// Fades. One fade for all voices (Fixed layout) or one per voice.

static void reset_fades(SmstPlayer* p) {
    if (p->module.version->voice_fade) {
        for (int i = 0; i < SMST_VOICES; i++) {
            p->voices[i].fade = FADE_FULL;
        }
    } else {
        p->fade = 0;
    }
}

// The position's fade byte. Bit 7 starts a fade: bit 6 fades out, bit 5 starts from the end volume, the low bits are
// the ticks a step takes. Without bit 7 it is the volume itself. Returns the volume the voices take (bit 7 set: none).

static uint8_t start_fade(SmstPlayer* p, uint8_t byte) {
    const bool voice_fade = p->module.version->voice_fade;
    if (!(byte & POSITION_FADE_BIT)) {
        p->fade_control = 0;
        if (voice_fade) {
            p->fade_count = 0;
        }
        return byte;
    }
    uint8_t volume = voice_fade ? byte : p->fade;
    const uint8_t steps = byte & POSITION_FADE_SPEED;
    p->fade_count = steps;
    if (byte & POSITION_FADE_OUT) {
        if (byte & POSITION_FADE_ZERO) {
            volume = FADE_FULL;
        }
        p->fade_control = steps;
    } else {
        if (byte & POSITION_FADE_ZERO) {
            volume = 0;
        }
        p->fade_control = steps | FADE_UP;
    }
    return volume;
}

// The position record: the block range, the transpose, the fade and the voices' instrument offsets. Returns the
// arpeggio step it leaves behind (see SmstPlayer.idle_step).

static uint8_t load_position(SmstPlayer* p) {
    const SmstModule* m = &p->module;
    const SmstTables* t = &m->tables;
    const SmstVersion* ver = m->version;
    uint8_t fade_byte;
    uint8_t voice_byte[SMST_VOICES];
    if (ver->layout == SmstLayout_Offsets) {
        const uint32_t at = t->positions + (uint32_t)p->position * 8u;
        p->block_first = smst_u8(m, at);
        p->block_last = smst_u8(m, at + 1);
        p->position_transpose = smst_u8(m, at + 2);
        fade_byte = smst_u8(m, at + 3);
        for (uint32_t i = 0; i < SMST_VOICES; i++) {
            voice_byte[i] = smst_u8(m, at + 4 + i);
        }
    } else {
        const uint32_t at = t->positions + p->position;
        p->block_first = smst_u8(m, at);
        p->block_last = smst_u8(m, at + FIXED_COLUMN);
        p->position_transpose = smst_u8(m, at + 2 * FIXED_COLUMN);
        fade_byte = smst_u8(m, at + 3 * FIXED_COLUMN);
        for (uint32_t i = 0; i < SMST_VOICES; i++) {
            voice_byte[i] = smst_u8(m, at + (4 + i) * FIXED_COLUMN);
        }
    }
    const uint8_t volume = start_fade(p, fade_byte);
    if (!ver->voice_fade) {
        p->fade = volume;
    }
    for (int i = 0; i < SMST_VOICES; i++) {
        SmstVoice* v = &p->voices[i];
        v->position_instrument = voice_byte[i];
        if (ver->voice_fade && !(voice_byte[i] & VOICE_NO_FADE) && !(volume & POSITION_FADE_BIT)) {
            v->fade = volume;
        }
    }
    p->block = p->block_first;
    return !ver->voice_fade && (fade_byte & POSITION_FADE_BIT) ? p->fade_control : STEP_NONE;
}

// The pattern a voice plays in the current block.

static void load_pattern(SmstPlayer* p, SmstVoice* v) {
    const SmstModule* m = &p->module;
    uint32_t at = m->tables.blocks + v->column;
    uint8_t number;
    if (m->version->layout == SmstLayout_Offsets) {
        at += (uint32_t)p->block * 8u;
        number = smst_u8(m, at);
        v->pattern_transpose = smst_u8(m, at + 1);
    } else {
        at += p->block;
        number = smst_u8(m, at);
        v->pattern_transpose = smst_u8(m, at + FIXED_TRANSPOSE_COLUMN);
    }
    v->pattern = m->tables.patterns + (uint32_t)m->pattern_length * number;
}

static void load_patterns(SmstPlayer* p) {
    for (int i = 0; i < SMST_VOICES; i++) {
        load_pattern(p, &p->voices[i]);
    }
}

// The row of a voice: note and info bytes. A note starts the voice's DMA over; the row commands set the speed or end
// the pattern.

static void read_row(SmstPlayer* p, SmstVoice* v) {
    const SmstModule* m = &p->module;
    v->note = smst_u8(m, v->pattern + p->row);
    v->info = smst_u8(m, v->pattern + p->row + 1u);
    if (m->version->row_commands) {
        if (v->note == ROW_SPEED) {
            p->speed = v->info;
            v->note = 0;
            v->info = 0;
            return;
        }
        if (v->note == ROW_END) {
            p->end_of_pattern = true;
            v->note = 0;
            return;
        }
    }
    if (v->note != 0 && !(v->note & NOTE_RELEASE)) {
        v->dma_restart = true;
    }
}

// The end of the song: the original calls SongEnd here, and starts the song over.

static void wrap_song(SmstPlayer* p) {
    p->position = p->start_position;
    p->fade_control = 0;
    p->looped = true;
    p->speed = p->module.speed;
    reset_fades(p);
}

// The tick counter, the row, the block, the position.

static void advance(SmstPlayer* p) {
    const SmstModule* m = &p->module;
    if (m->version->row_commands && p->end_of_pattern) {
        p->row = (uint8_t)(m->pattern_length - 2);
    }
    p->tick++;
    if (p->tick != p->speed) {
        return;
    }
    p->tick = 0;
    p->row = (uint8_t)(p->row + 2);
    if (p->row != m->pattern_length) {
        return;
    }
    p->row = 0;
    if (p->block != p->block_last) {
        p->block++;
        return;
    }
    p->position++;
    if (p->position == p->end_position) {
        wrap_song(p);
    }
    load_position(p);
}

// The fade counter, once a tick.

static void fade_tick(SmstPlayer* p) {
    if (p->module.version->voice_fade) {
        if (p->fade_count != 0) {
            p->fade_count--;
        } else {
            p->fade_count = p->fade_control & FADE_STEPS_MASK;
        }
        return;
    }
    if (p->fade_count != 0) {
        p->fade_count--;
        return;
    }
    const uint8_t steps = p->fade_control & FADE_STEPS_MASK;
    if (steps == 0) {
        return;
    }
    p->fade_count = steps;
    if (!(p->fade_control & FADE_UP)) {
        if (p->fade != 0) {
            p->fade--;
        }
    } else if (p->fade == FADE_FULL) {
        p->fade_control = 0;
    } else {
        p->fade++;
    }
}

SmstTickResult smst_tick(SmstPlayer* p, NuadeVoices* voices) {
    const SmstVersion* ver = p->module.version;
    const bool rows_first = ver->dma_flow == SmstDmaFlow_RowThenNote;
    uint8_t step = p->idle_step;
    p->looped = false;
    p->end_of_pattern = false;
    for (int i = 0; i < SMST_VOICES; i++) {
        SmstVoice* v = &p->voices[i];
        if (rows_first && v->dma_off) {
            nuade_paula_channel_dma_off(&v->paula);
        }
        v->dma_restart = false;
    }
    for (int i = 0; i < SMST_VOICES; i++) {
        SmstVoice* v = &p->voices[i];
        smst_write_loop_registers(p, v);
        if (p->tick == 0) {
            if (ver->layout == SmstLayout_Fixed && p->row == 0) {
                load_pattern(p, v);
            }
            read_row(p, v);
        }
    }
    for (int i = 0; i < SMST_VOICES; i++) {
        SmstVoice* v = &p->voices[i];
        if (v->dma_restart || (!rows_first && v->dma_off)) {
            nuade_paula_channel_dma_off(&v->paula);
        }
        if (rows_first) {
            v->dma_restart = false;
        }
        v->dma_off_next = false;
    }
    for (int i = 0; i < SMST_VOICES; i++) {
        SmstVoice* v = &p->voices[i];
        if (p->tick == 0) {
            smst_note_start(p, v);
        }
        smst_voice_effects(p, v, &step);
    }
    advance(p);
    if (ver->layout == SmstLayout_Offsets && p->tick == 0 && p->row == 0) {
        load_patterns(p);
    }
    for (int i = 0; i < SMST_VOICES; i++) {
        SmstVoice* v = &p->voices[i];
        if (v->dma_restart || v->dma_off) {
            nuade_paula_channel_dma_on(&v->paula);
        }
        v->dma_off = v->dma_off_next;
    }
    fade_tick(p);
    for (uint32_t i = 0; i < SMST_VOICES; i++) {
        nuade_paula_channel_sync(&p->voices[i].paula, i, voices);
    }
    return (SmstTickResult) { .looped = p->looped };
}

// The init writes into the module's memory: it copies the built-in samples' slots into the module's slot table.

static void put_u8(SmstModule* m, uint32_t address, uint8_t value) {
    if (address < m->limit) {
        m->memory[address] = value;
    }
}

static void put_u16(SmstModule* m, uint32_t address, uint16_t value) {
    put_u8(m, address, (uint8_t)(value >> 8));
    put_u8(m, address + 1, (uint8_t)value);
}

static void put_u32(SmstModule* m, uint32_t address, uint32_t value) {
    put_u16(m, address, (uint16_t)(value >> 16));
    put_u16(m, address + 2, (uint16_t)value);
}

static void copy_builtin_slots(SmstModule* m) {
    const SmstVersion* ver = m->version;
    if (ver->builtin_count == 0) {
        return;
    }
    const SmstTables* t = &m->tables;
    const uint32_t data = ver->vars + ver->data_at;
    const uint32_t bias = (uint32_t)ver->builtin_origin - (t->samples - data) - t->sample_bias;
    uint32_t from = ver->vars + (uint32_t)ver->builtin_template;
    uint32_t to = t->samples + ver->builtin_at;
    for (uint32_t i = 0; i < ver->builtin_count; i++) {
        put_u32(m, to, smst_u32(m, from) + bias);
        put_u32(m, to + 4, smst_u32(m, from + 4));
        put_u16(m, to + 8, smst_u16(m, from + 8));
        from += SLOT_SIZE;
        to += SLOT_SIZE;
    }
}

// The init: every voice plays the silent word until its first note (most versions), the first position is read.

static void start_dummy_voices(SmstPlayer* p) {
    for (int i = 0; i < SMST_VOICES; i++) {
        SmstVoice* v = &p->voices[i];
        v->paula.lc = smst_sample_pointer(&p->module, p->module.tables.dummy);
        v->paula.len_words = 1;
        v->paula.period = DUMMY_PERIOD;
        nuade_paula_channel_dma_on(&v->paula);
    }
}

bool smst_start(SmstPlayer* p, int subsong) {
    if (subsong < 0 || subsong >= p->subsongs) {
        return false;
    }
    SmstModule* m = &p->module;
    const SmstVersion* ver = m->version;
    memcpy(m->memory, p->original, m->size);
    memset(m->memory + m->size, 0, m->limit - m->size);
    copy_builtin_slots(m);

    SmstPlayer fresh = { .module = *m, .original = p->original, .subsongs = p->subsongs };
    *p = fresh;
    for (int i = 0; i < SMST_VOICES; i++) {
        p->voices[i].column = m->voice_column[i];
        p->voices[i].shown_note = -1;
        p->voices[i].shown_instrument = -1;
    }
    p->start_position = m->first_position;
    p->end_position = m->last_position;
    p->speed = m->speed;
    p->position = m->first_position;
    if (ver->layout == SmstLayout_Fixed) {
        // the scratch words the init does not clear keep what the module holds
        const uint8_t switches = smst_initial_wave_switches(m);
        for (int i = 0; i < SMST_VOICES; i++) {
            p->voices[i].dma_off = (switches >> i & 1) != 0;
        }
        p->fade = smst_initial_fade(m);
    } else {
        reset_fades(p);
    }
    switch (ver->init) {
        case SmstInit_Table: {
            const SmstSubsong entry = smst_subsong(m, (uint32_t)subsong);
            p->position = entry.position;
            p->start_position = entry.loop_start;
            p->end_position = entry.end;
            break;
        }
        case SmstInit_Window:
            p->start_position = subsong != 0 ? 0x0C : 0x00;
            p->end_position = subsong != 0 ? 0x14 : 0x0C;
            p->position = p->start_position;
            break;
        case SmstInit_NoSubsongs:
        case SmstInit_Mode:
            break;
    }
    if (ver->dummy_voices) {
        start_dummy_voices(p);
    }
    p->idle_step = load_position(p);
    if (ver->layout == SmstLayout_Offsets) {
        load_patterns(p);
    }
    return true;
}

SmstPlayer* smst_create(const uint8_t* data, size_t size) {
    if (size == 0 || size > MAX_MODULE_BYTES) {
        return nullptr;
    }
    const SmstVersion* version = smst_find_version(data, size);
    if (version == nullptr) {
        return nullptr;
    }
    SmstPlayer* p = calloc(1, sizeof(SmstPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    p->original = malloc(size);
    p->module.memory = nuade_paula_padded_copy(data, size);
    if (p->original == nullptr || p->module.memory == nullptr) {
        smst_destroy(p);
        return nullptr;
    }
    memcpy(p->original, data, size);
    p->module.size = (uint32_t)size;
    p->module.limit = (uint32_t)size + NUADE_PAULA_MAX_SAMPLE_BYTES;
    if (!smst_locate(&p->module, version)) {
        smst_destroy(p);
        return nullptr;
    }
    p->subsongs = smst_count_subsongs(&p->module);
    return p;
}

void smst_destroy(SmstPlayer* p) {
    if (p == nullptr) {
        return;
    }
    free(p->original);
    free(p->module.memory);
    free(p);
}

bool smst_probe(const uint8_t* data, size_t size) {
    SmstPlayer* p = smst_create(data, size);
    const bool supported = p != nullptr;
    smst_destroy(p);
    return supported;
}

int smst_subsong_count(const SmstPlayer* p) {
    return p->subsongs;
}

int smst_ticks_per_row(const SmstPlayer* p) {
    return p->module.speed;
}

SmstVoiceState smst_voice_state(const SmstPlayer* p, int voice) {
    if (voice < 0 || voice >= SMST_VOICES) {
        return (SmstVoiceState) { .note = -1, .instrument = -1 };
    }
    return (SmstVoiceState) { .note = p->voices[voice].shown_note, .instrument = p->voices[voice].shown_instrument };
}
