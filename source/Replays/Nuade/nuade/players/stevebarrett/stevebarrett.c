// SPDX-License-Identifier: MIT
// Steve Barrett: the driver. One tick plays the four voices from the last to the first, then writes the Paula
// registers of all of them.

#include "sbr_internal.h"

#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

// The song request byte: 0 stops, 1.. starts a song, this value is "nothing to do".
#define REQUEST_RUNNING 0xffu

static const int8_t* chip(const SbrPlayer* p, uint32_t offset) {
    // AUDxLC ignores bit 0
    offset &= ~1u;
    if (offset >= p->module.size) {
        return nullptr;
    }
    return (const int8_t*)(p->module.image + offset);
}

// A byte of the voice records, which a note number far past the period table reaches: field `byte & ~3` of voice
// `byte & 3`.

static uint8_t record_byte(const SbrPlayer* p, unsigned byte) {
    const uint32_t voice = byte & 3u;
    const SbrVoice* c = &p->voices[voice];
    switch (byte & ~3u) {
        case 0x00:
            return (uint8_t)(0x10u * voice);
        case 0x04:
            return c->chord_base;
        case 0x08:
            return c->note;
        case 0x0c:
            return c->list_pos;
        case 0x10:
            return c->duration;
        case 0x14:
            return c->duration_left;
        case 0x18:
            return (uint8_t)c->mode;
        case 0x1c:
            return c->chord_mask;
        case 0x20:
            return c->transpose;
        case 0x34:
            return c->bend_note;
        case 0x38:
            return c->chord_pos;
        case 0x3c:
            return (uint8_t)c->vibrato_dir;
        case 0x40:
            return c->vibrato_step;
        case 0x44:
            return c->vibrato_delay_reload;
        case 0x48:
            return c->vibrato_delay;
        case 0x4c:
            return c->wave;
        case 0x50:
            return c->env;
        case 0x54:
            return c->volume_target;
        case 0x58:
            return c->volume_step;
        case 0x5c:
            return c->volume_release;
        case 0x60:
            return c->volume;
        case 0x64:
            return (uint8_t)(1u << voice); // the voice's DMACON bit
        case 0x68:
            return c->vibrato;
        case 0x6c:
            return c->released ? 1 : 0;
        case 0x70:
            return c->sustain_shift;
        default: // $24, $28, $2c, $30: the slide and glide fields, never set
            return 0;
    }
}

// A word of the period table by note number. The driver does not stop at the table's end; the module check has proven
// that the songs read only what the module model has.

static uint16_t period_word(SbrPlayer* p, unsigned index) {
    switch (sbr_module_period_kind(&p->module, index)) {
        case SbrWord_Live:
            return p->voices[sbr_module_period_voice(&p->module, index)].period;
        case SbrWord_Record: {
            const unsigned byte = sbr_module_period_record(&p->module, index);
            return (uint16_t)((record_byte(p, byte) << 8) | record_byte(p, byte + 1));
        }
        case SbrWord_Image:
        case SbrWord_Instrument:
            return sbr_module_period_word(&p->module, index);
        case SbrWord_Unmodelled:
            break;
    }
    p->broken = true;
    return 0;
}

static void set_period(SbrPlayer* p, SbrVoice* c) {
    c->base_period = period_word(p, c->note);
    c->period = c->base_period;
}

// The eagleplayer's hook in the list reader: SongEnd once every voice has read the $ff that ends its list.

static void list_end_seen(SbrPlayer* p) {
    if (p->voice_counter < SBR_VOICES) {
        p->list_pending[p->voice_counter] = false;
    }
    bool any = false;
    for (uint32_t i = 0; i < SBR_VOICES; i++) {
        any = any || p->list_pending[i];
    }
    if (!any) {
        for (uint32_t i = 0; i < SBR_VOICES; i++) {
            p->list_pending[i] = true;
        }
        p->looped = true;
    }
}

// The next pattern of the voice's list. Entries below $80 are pattern numbers; $80..$fe set the transposition; $ff
// restarts the list.

static void next_pattern(SbrPlayer* p, uint32_t voice) {
    SbrVoice* c = &p->voices[voice];
    for (uint32_t guard = 0; guard < 4096; guard++) {
        const uint8_t entry = sbr_module_byte(&p->module, sbr_offset(c->list) + c->list_pos);
        c->list_pos++;
        if (entry == 0xff) {
            list_end_seen(p);
            c->list_pos = 0;
        } else if (entry & 0x80) {
            c->transpose = (uint8_t)(entry + 0x40);
        } else {
            c->pattern = sbr_module_pattern(&p->module, entry);
            p->last_pattern = entry;
            return;
        }
    }
    p->broken = true;
}

// The chord buffer of a voice: the chord's note offsets, the notes of the mask in the order of the chord table.

static void build_chord(SbrPlayer* p, const SbrVoice* c, uint8_t mask) {
    uint32_t pos = (uint32_t)c->chord_base + 1;
    for (unsigned bit = 0; bit < 8; bit++) {
        if ((mask >> bit) & 1u) {
            if (pos < sizeof(p->chord_buffer)) {
                p->chord_buffer[pos] = sbr_module_chord(&p->module, bit);
            }
            pos++;
        }
    }
    if (pos < sizeof(p->chord_buffer)) {
        p->chord_buffer[pos] = 0xff;
    }
}

// The sample a note plays and its repeat, from the wave record. The first buffer is written to Paula at once; the
// repeat goes there behind the DMA start.

static void start_wave(SbrPlayer* p, SbrVoice* c) {
    SbrWave wave;
    if (!sbr_module_wave(&p->module, c->wave, &wave)) {
        p->broken = true;
        return;
    }
    const SbrInstrument* inst = &p->module.instruments[wave.instrument % p->module.instrument_count];
    c->loop_start = wave.loop_start;
    c->loop_length = wave.loop_length;
    c->sample = inst->offset;
    c->paula.len_words = inst->length_words;
    c->paula.lc = chip(p, inst->offset);
    c->shown_instrument = wave.instrument;
}

static void start_env(SbrPlayer* p, SbrVoice* c) {
    SbrEnv env;
    if (!sbr_module_env(&p->module, c->env, &env)) {
        p->broken = true;
        return;
    }
    c->volume = env.volume;
    c->volume_target = env.target;
    c->volume_step = env.step;
    c->volume_release = env.release;
    c->sustain_shift = env.sustain_shift;
    c->vibrato = env.vibrato;
}

// The bend decides on the low bit of a byte that the driver's register a1 points at. For a note with a chord that
// register is stale: it still holds what the pattern reader left in it, the pattern pointer of the next voice or, if
// the pattern list ended in this note, a word of the pattern table. A note without a chord reads the envelope's flags.

static bool bend_bit(const SbrPlayer* p, uint32_t voice, bool wrapped, uint8_t wrapped_pattern) {
    const SbrVoice* c = &p->voices[voice];
    if (c->chord_mask == 0) {
        SbrEnv env;
        return sbr_module_env(&p->module, c->env, &env) && (env.flags & 1u) != 0;
    }
    if (wrapped) {
        return (sbr_module_byte(&p->module, (int64_t)p->module.build->patterns + 2 * wrapped_pattern + 7) & 1u) != 0;
    }
    // behind the last voice's pattern pointer lies the list pointer of voice 0
    const int32_t pointer = voice + 1 < SBR_VOICES ? p->voices[voice + 1].pattern : p->voices[0].list;
    return (pointer & 1) != 0;
}

// The next note of a voice. False when the pattern says to stop the song.

static bool start_note(SbrPlayer* p, uint32_t voice) {
    SbrVoice* c = &p->voices[voice];
    const SbrBuild* build = p->module.build;
    if (c->mode != SbrChordMode_Hold) {
        c->mode = SbrChordMode_None;
        c->chord_mask = 0;
    }
    c->released = false;
    SbrEvent e = { .kind = SbrEvent_Truncated };
    bool wrapped = false;
    uint8_t wrapped_pattern = 0;
    for (uint32_t guard = 0; guard < 4096; guard++) {
        e = sbr_decode_event(&p->module, sbr_offset(c->pattern));
        if (e.has_tempo) {
            p->tempo = e.tempo;
        }
        if (e.has_wave) {
            c->wave = e.wave;
        }
        if (e.has_env) {
            c->env = e.env;
        }
        if (e.kind == SbrEvent_Stop) {
            return false;
        }
        if (e.kind == SbrEvent_Note || e.kind == SbrEvent_Truncated) {
            break;
        }
        next_pattern(p, voice);
        wrapped = true;
        wrapped_pattern = p->last_pattern;
    }
    // a pattern that runs out of the module, or lists that give no note: the driver would run on or hang
    if (e.kind != SbrEvent_Note || p->broken) {
        p->broken = true;
        return false;
    }
    if (e.has_duration) {
        c->duration = e.duration;
    }
    switch (e.command) {
        case SbrCommand_VibratoDelay:
            c->vibrato_delay_reload = e.parameter;
            break;
        case SbrCommand_Chord:
        case SbrCommand_ChordHold:
            c->mode = e.command == SbrCommand_Chord ? SbrChordMode_Note : SbrChordMode_Hold;
            c->chord_mask = e.parameter;
            build_chord(p, c, e.parameter);
            break;
        case SbrCommand_None:
        case SbrCommand_Unmodelled:
            break;
    }
    c->silent = false;
    nuade_paula_channel_dma_off(&c->paula);
    start_wave(p, c);
    start_env(p, c);
    c->note = (uint8_t)(e.note + c->transpose);
    if (build->extended && bend_bit(p, voice, wrapped, wrapped_pattern)) {
        c->bend_note = c->note;
        c->note++;
    }
    set_period(p, c);
    c->shown_period = c->period;
    c->pattern += (uint8_t)e.consumed;
    c->duration_left = c->duration;
    c->vibrato_delay = c->vibrato_delay_reload;
    if (build->extended) {
        c->chord_pos = (uint8_t)(c->chord_base + 1);
    }
    p->vibrato_depth_high = 0;
    return true;
}

// Vibrato: the period moves by a step that is the distance to the next note, halved `speed` times; the swing grows by
// one step per tick up to the tick counter and back down. The loops run on bytes the driver tests for the sign.

static void vibrato(SbrPlayer* p, SbrVoice* c) {
    const uint8_t speed = c->vibrato & 0xf;
    const uint8_t depth = (uint8_t)((p->vibrato_depth_high << 4) | (c->vibrato >> 4));
    // the driver doubles the note in a byte
    const unsigned index = c->note & 0x7fu;
    uint16_t step = (uint16_t)(period_word(p, index) - period_word(p, index + 1));
    for (uint8_t i = 0; i < speed; i++) {
        step = (uint16_t)((step >> 1) | 1u);
    }
    bool rise = c->vibrato_dir >= 0;
    if (!rise) {
        c->vibrato_step--;
        if (c->vibrato_step == 0) {
            c->vibrato_dir++;
            rise = c->vibrato_dir < 0;
        }
    }
    if (rise) {
        c->vibrato_step++;
        if (c->vibrato_step >= p->tick_count) {
            c->vibrato_dir--;
        }
    }
    if (c->vibrato_delay != 0) {
        c->vibrato_delay--;
        return;
    }
    uint16_t period = c->base_period;
    for (uint8_t i = 0; i < depth / 2; i++) {
        period = (uint16_t)(period + step);
    }
    const uint8_t down = c->vibrato_step <= 128 ? c->vibrato_step : 0;
    for (uint8_t i = 0; i < down; i++) {
        period = (uint16_t)(period - step);
    }
    c->period = period;
}

// The chord plays one of its notes per tick; the start byte of the buffer plays the note itself.

static void arpeggio(SbrPlayer* p, SbrVoice* c) {
    for (uint32_t guard = 0; guard < 2; guard++) {
        const uint8_t offset = c->chord_pos < sizeof(p->chord_buffer) ? p->chord_buffer[c->chord_pos] : 0xff;
        if (offset == 0xff) {
            c->chord_pos = c->chord_base;
            continue;
        }
        c->period = period_word(p, (uint8_t)(offset + c->note));
        c->chord_pos++;
        return;
    }
}

// The volume: it moves to its target; at the end of the note (or, with a sustain shift, at the fraction of it) the
// release takes over and the voice goes silent when the volume reaches 0.

static void release_volume(SbrPlayer* p, SbrVoice* c) {
    c->released = true;
    p->vibrato_depth_high = 0;
    const uint8_t volume = (uint8_t)(c->volume - c->volume_release);
    if ((int8_t)volume <= 0) {
        c->volume = 0;
        c->silent = true;
    } else {
        c->volume = volume;
    }
}

static void step_volume(SbrPlayer* p, SbrVoice* c) {
    if (c->duration_left == 0) {
        return;
    }
    if (c->released) {
        release_volume(p, c);
        return;
    }
    if (c->sustain_shift == 0) {
        c->released = true;
        return;
    }
    if (c->sustain_shift == 0xff) {
        return;
    }
    // lsr.b by a register: the count is taken modulo 64
    const unsigned shift = c->sustain_shift & 63u;
    const uint8_t sustained = shift < 8 ? (uint8_t)(c->duration >> shift) : 0;
    if ((uint8_t)(sustained - c->duration_left) == 0) {
        release_volume(p, c);
    } else if (c->volume != c->volume_target) {
        c->volume = (uint8_t)(c->volume + c->volume_step);
    }
}

// What a voice does in a tick that does not start a note.

static void keep_note(SbrPlayer* p, SbrVoice* c) {
    if (c->note == 0) {
        return;
    }
    if (c->mode == SbrChordMode_None && c->vibrato != 0) {
        vibrato(p, c);
    } else if (p->module.build->extended && c->chord_mask != 0) {
        arpeggio(p, c);
    }
}

// One voice of a tick. False when the song stops.

static bool run_voice(SbrPlayer* p, uint32_t voice) {
    SbrVoice* c = &p->voices[voice];
    if (p->tick_count == 0) {
        c->duration_left--;
        if (c->sustain_shift == 0 && c->duration_left == 0) {
            c->silent = true;
            keep_note(p, c);
        } else if ((int8_t)c->duration_left < 0) {
            if (!start_note(p, voice)) {
                return false;
            }
        } else {
            keep_note(p, c);
        }
    } else if (c->bend_note != 0) {
        c->bend_note = 0;
        c->note--;
        set_period(p, c);
        p->vibrato_depth_high = (c->base_period >> 8) & 0xfu;
    } else {
        keep_note(p, c);
    }
    step_volume(p, c);
    return true;
}

// The registers of the tick: the periods, the DMA of the voices that are not silent, the volumes, then the repeat of
// every sample (a DMA start latches the first buffer, the repeat is what the driver writes afterwards).

static void write_registers(SbrPlayer* p) {
    for (uint32_t i = 0; i < SBR_VOICES; i++) {
        p->voices[i].paula.period = p->voices[i].period;
    }
    for (uint32_t i = 0; i < SBR_VOICES; i++) {
        if (!p->voices[i].silent) {
            nuade_paula_channel_dma_on(&p->voices[i].paula);
        }
    }
    for (uint32_t i = 0; i < SBR_VOICES; i++) {
        if (p->voices[i].silent) {
            nuade_paula_channel_dma_off(&p->voices[i].paula);
        }
    }
    for (uint32_t i = 0; i < SBR_VOICES; i++) {
        p->voices[i].paula.volume = p->voices[i].volume;
    }
    if (p->module.build->loop_style == SbrLoop_EveryVoice) {
        for (uint32_t i = 0; i < SBR_VOICES; i++) {
            SbrVoice* c = &p->voices[i];
            c->paula.len_words = c->loop_length >> 1;
            c->paula.lc = chip(p, c->sample + c->loop_start);
        }
    } else {
        // the build writes the repeat of the voice it worked on last, voice 0, and not when its length is 0
        SbrVoice* c = &p->voices[0];
        if (c->loop_length != 0) {
            c->paula.len_words = c->loop_length;
            c->paula.lc = chip(p, c->sample + c->loop_start);
        }
    }
}

static void stop_audio(SbrPlayer* p) {
    for (uint32_t i = 0; i < SBR_VOICES; i++) {
        p->voices[i].paula.volume = 0;
        p->voices[i].volume = 0;
    }
    // the eagleplayer's hook in the stop routine: SongEnd from the second call on
    if (p->stop_seen) {
        p->ended = true;
    }
    p->stop_seen = true;
}

static void start_song(SbrPlayer* p, uint32_t song) {
    for (uint32_t i = 0; i < SBR_VOICES; i++) {
        p->voices[i].list = sbr_module_list(&p->module, song, i);
    }
    for (uint32_t i = SBR_VOICES; i-- > 0;) {
        SbrVoice* c = &p->voices[i];
        c->duration_left = 0;
        c->mode = SbrChordMode_None;
        c->list_pos = 0;
        next_pattern(p, i);
    }
    stop_audio(p);
    p->tick_count = 0;
    p->playing = true;
    for (uint32_t i = 0; i < SBR_VOICES; i++) {
        p->voices[i].silent = true;
    }
}

static void stop_song(SbrPlayer* p) {
    p->playing = false;
    stop_audio(p);
}

// The tick entry: a song number in the request byte starts the song, 0 stops it.

static void run_tick(SbrPlayer* p) {
    const uint8_t request = p->request;
    p->request = REQUEST_RUNNING;
    if (request == 0) {
        stop_song(p);
        return;
    }
    if (request <= p->module.build->max_song) {
        start_song(p, request - 1u);
        return;
    }
    if (!p->playing) {
        return;
    }
    p->vibrato_depth_high = 0;
    for (int voice = SBR_VOICES - 1; voice >= 0; voice--) {
        p->voice_counter = (uint8_t)voice;
        if (!run_voice(p, (uint32_t)voice)) {
            stop_song(p);
            return;
        }
    }
    p->voice_counter = 0xff; // the driver's counter ends below 0
    write_registers(p);
    p->tick_count--;
    if ((int8_t)p->tick_count < 0) {
        p->tick_count = p->tempo;
    }
}

SbrTickResult sbr_tick(SbrPlayer* player, NuadeVoices* voices) {
    player->looped = false;
    player->ended = false;
    run_tick(player);
    for (uint32_t i = 0; i < SBR_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return (SbrTickResult) { .length = player->module.tick_length,
                             .looped = player->looped,
                             .ended = player->ended || player->broken };
}

bool sbr_start(SbrPlayer* player, uint32_t song, NuadeVoices* voices) {
    if (song >= player->module.song_count) {
        return false;
    }
    SbrModule module = player->module;
    memset(player, 0, sizeof(*player));
    player->module = module;
    player->request = (uint8_t)(song + 1);
    player->tempo = 1; // the variable's value in the module
    for (uint32_t i = 0; i < SBR_VOICES; i++) {
        SbrVoice* c = &player->voices[i];
        c->chord_base = (uint8_t)(SBR_CHORD_BYTES * i);
        c->silent = true;
        c->shown_instrument = -1;
        player->list_pending[i] = true;
        nuade_voice_stop(voices, i);
    }
    return true;
}

bool sbr_probe(const uint8_t* data, size_t size) {
    SbrModule module;
    if (sbr_module_open(&module, data, size) != SbrStatus_Ok) {
        return false;
    }
    sbr_module_close(&module);
    return true;
}

SbrCreateResult sbr_create(const uint8_t* data, size_t size) {
    SbrPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return (SbrCreateResult) { .status = SbrStatus_OutOfMemory };
    }
    const SbrStatus status = sbr_module_open(&player->module, data, size);
    if (status != SbrStatus_Ok) {
        free(player);
        return (SbrCreateResult) { .status = status };
    }
    return (SbrCreateResult) { .player = player, .status = SbrStatus_Ok };
}

void sbr_destroy(SbrPlayer* player) {
    if (player != nullptr) {
        sbr_module_close(&player->module);
        free(player);
    }
}

uint32_t sbr_song_count(const SbrPlayer* player) {
    return player->module.song_count;
}

const char* sbr_format_name(const SbrPlayer* player) {
    return player->module.build->name;
}

SbrVoiceState sbr_voice_state(const SbrPlayer* player, uint32_t voice) {
    const SbrVoice* c = &player->voices[voice];
    return (SbrVoiceState) { .period = c->shown_period, .instrument = c->shown_instrument };
}
