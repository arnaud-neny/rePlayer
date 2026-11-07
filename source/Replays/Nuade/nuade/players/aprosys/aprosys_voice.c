// SPDX-License-Identifier: MIT
// AProSys player: the voices. Every tick each voice advances its four envelopes, turns them into period and volume with
// the instrument's modulation, and acts on its sample record: a synthesised voice builds its wave, a window voice moves
// its start. Chained samples (kinds 0, 1 and 2) are loaded by the channel's audio interrupt.

#include "aprosys_internal.h"

#define PERIOD_TABLE_ENTRIES 48

// An interrupt handler may start the DMA, which asks for a second interrupt (the block start)
#define INTERRUPT_LIMIT 4

static const uint16_t k_periods[PERIOD_TABLE_ENTRIES] = {
    1710, 1614, 1524, 1438, 1357, 1281, 1209, 1141, 1077, 1017, 960, 906, 855, 807, 762, 719,
    679,  641,  605,  571,  539,  508,  480,  453,  428,  404,  381, 360, 339, 320, 302, 285,
    269,  254,  240,  226,  214,  202,  190,  180,  170,  160,  151, 143, 135, 127, 120, 113,
};

// Memory the Paula registers point into

// The silent block the player allocates, just before the wave memory
static uint32_t silence_address(const ApsModule* m) {
    return m->wave_address - APS_SILENCE_DISTANCE;
}

// AUDxLC: the module or the player's chip allocations. Both have the Paula tail, so any AUDxLEN can play from there
// (what runs past them plays zeros); an address that nothing holds is none.

static void set_lc(const ApsPlayer* p, ApsVoice* v, uint32_t address) {
    const ApsModule* m = &p->module;
    if (address < m->size) {
        v->paula.lc = (const int8_t*)m->data + address;
    } else if (address - silence_address(m) < APS_CHIP_BYTES) {
        v->paula.lc = p->chip + (address - silence_address(m));
    } else {
        v->paula.lc = nullptr;
    }
}

// AUDxLEN as written: 0 plays 65536 words.

static void set_len(ApsVoice* v, uint16_t words) {
    v->paula.len_words = words;
}

// The address of a sample in the sample data section, from its offset in words
static uint32_t sample_address(const ApsModule* m, uint32_t words) {
    return m->sample_data + words * 2;
}

// A note past the end of the 48-entry period table (note + table position + arpeggio offset) reads the 68k player's
// data block behind it, and songs do (theme and simplemelody2 reach indexes 48..125, memdump-verified byte for byte):
// the table pointer, the INTENA masks, the four audio interrupt nodes (relocated pointers to their names and handlers),
// the custom chip base, the old interrupt vectors and the player's own variables. Offsets from 0x126 (the module and
// wave pointers) read 0: no song reaches them. The addresses are where UADE loads the eagleplayer and what its exec
// returns.
#define DATA_ADDRESS 0x11310u // the data block; the code hunk is at 0xF008
#define DATA_CODE_ADDRESS 0xf008u
#define DATA_TABLE 0x2a           // the period table, 48 words
#define DATA_TABLE_POINTER 0x8a   // a long: DATA_ADDRESS + DATA_TABLE + 2 * period position
#define DATA_AUDIO_INT_MASKS 0x8e // INTENA bits of the four audio interrupts, a word each
#define DATA_INT_NODES 0x96       // four interrupt nodes
#define DATA_INT_NODE_SIZE 0x16
#define DATA_CUSTOM_BASE 0xee    // a long
#define DATA_REPEAT_COUNT 0xf2   // a byte
#define DATA_OLD_VECTORS 0xf4    // four longs
#define DATA_KICK_FLAGS 0x104    // a word per voice
#define DATA_FILTER_FIRST 0x10e  // a word per voice
#define DATA_FILTER_SECOND 0x116 // a word per voice
#define DATA_VARIABLES 0x11e     // row, steps done, order position, state, ticks left, row ticks, speed, jump budget
#define DATA_STOPPED 1           // the state byte after the stop routine
#define DATA_PLAYING 2

#define NODE_TYPE 8 // ln_Type of an interrupt node
#define NT_INTERRUPT 2
#define NODE_NAME 10
#define NODE_HANDLER 18
#define NODE_NAME_CODE_OFFSET 0x22d4 // of the first node's name and handler in the code hunk
#define NODE_NAME_SIZE 13
#define NODE_HANDLER_CODE_OFFSET 0xf20
#define NODE_HANDLER_SIZE 0x18
#define OLD_VECTOR_FIRST 0x4806u // what exec's SetIntVector returned: four consecutive addresses
#define CUSTOM_BASE 0xdff000u

// Byte `position` of the big-endian `value` of `size` bytes
static uint8_t be_byte(uint32_t value, uint32_t size, uint32_t position) {
    return (uint8_t)(value >> (8 * (size - 1 - position)));
}

// One byte of interrupt node `node` at offset `at`: no links or data, the handler's name and code
static uint8_t interrupt_node_byte(uint32_t node, uint32_t at) {
    if (at == NODE_TYPE) {
        return NT_INTERRUPT;
    }
    if (at >= NODE_NAME && at < NODE_NAME + 4) {
        return be_byte(DATA_CODE_ADDRESS + NODE_NAME_CODE_OFFSET + NODE_NAME_SIZE * node, 4, at - NODE_NAME);
    }
    if (at >= NODE_HANDLER && at < NODE_HANDLER + 4) {
        return be_byte(DATA_CODE_ADDRESS + NODE_HANDLER_CODE_OFFSET + NODE_HANDLER_SIZE * node, 4, at - NODE_HANDLER);
    }
    return 0;
}

// One byte of the data block at offset `at`
static uint8_t data_block_byte(const ApsPlayer* p, uint32_t at) {
    if (at < DATA_TABLE_POINTER) {
        const uint32_t entry = (at - DATA_TABLE) / 2;
        return be_byte(k_periods[entry], 2, (at - DATA_TABLE) % 2);
    }
    if (at < DATA_AUDIO_INT_MASKS) {
        return be_byte(DATA_ADDRESS + DATA_TABLE + 2u * p->period_base, 4, at - DATA_TABLE_POINTER);
    }
    if (at < DATA_AUDIO_INT_MASKS + 2 * APS_VOICES) {
        return be_byte(0x80u << ((at - DATA_AUDIO_INT_MASKS) / 2), 2, (at - DATA_AUDIO_INT_MASKS) % 2);
    }
    if (at >= DATA_INT_NODES && at < DATA_INT_NODES + APS_VOICES * DATA_INT_NODE_SIZE) {
        const uint32_t offset = at - DATA_INT_NODES;
        return interrupt_node_byte(offset / DATA_INT_NODE_SIZE, offset % DATA_INT_NODE_SIZE);
    }
    if (at >= DATA_CUSTOM_BASE && at < DATA_CUSTOM_BASE + 4) {
        return be_byte(CUSTOM_BASE, 4, at - DATA_CUSTOM_BASE);
    }
    if (at == DATA_REPEAT_COUNT) {
        return p->repeat_left;
    }
    if (at >= DATA_OLD_VECTORS && at < DATA_OLD_VECTORS + 4 * APS_VOICES) {
        const uint32_t offset = at - DATA_OLD_VECTORS;
        return be_byte(OLD_VECTOR_FIRST + 4 * (offset / 4), 4, offset % 4);
    }
    if (at >= DATA_KICK_FLAGS && at < DATA_KICK_FLAGS + 2 * APS_VOICES) {
        return be_byte(p->voices[(at - DATA_KICK_FLAGS) / 2].kick, 2, (at - DATA_KICK_FLAGS) % 2);
    }
    if (at >= DATA_FILTER_FIRST && at < DATA_FILTER_SECOND + 2 * APS_VOICES) {
        const bool second = at >= DATA_FILTER_SECOND;
        const uint32_t offset = at - (second ? DATA_FILTER_SECOND : DATA_FILTER_FIRST);
        const ApsWaveFilter* f = &p->voices[offset / 2].filter;
        return be_byte((uint16_t)(second ? f->second : f->first), 2, offset % 2);
    }
    const uint8_t variables[] = { p->row,        p->steps_done, p->order_pos, p->ended ? DATA_STOPPED : DATA_PLAYING,
                                  p->ticks_left, p->row_ticks,  p->speed,     p->jump_budget };
    if (at >= DATA_VARIABLES && at < DATA_VARIABLES + sizeof(variables)) {
        return variables[at - DATA_VARIABLES];
    }
    return 0;
}

uint16_t aps_period_lookup(const ApsPlayer* p, uint32_t note) {
    const uint32_t at = DATA_TABLE + 2 * (p->period_base + note);
    return (uint16_t)((data_block_byte(p, at) << 8) | data_block_byte(p, at + 1));
}

// Audio interrupts

// A chained sound's audio interrupt, at every block start: load the sound's record into the channel, then what comes
// next. Kinds 1 and 2 keep the interrupt on; the others switch it off.
static void audio_interrupt(ApsPlayer* p, ApsVoice* v) {
    const ApsModule* m = &p->module;
    v->irq_enabled = false;
    const ApsSampleRecord r = aps_sample_record(m, v->sample);

    switch (r.kind) {
        case ApsKind_Silence:
            set_lc(p, v, silence_address(m));
            set_len(v, 1);
            return;
        case ApsKind_Once:
            v->sample = APS_ADDR_SILENT_RECORD;
            v->irq_enabled = true;
            break;
        case ApsKind_Chain:
            v->sample += APS_SAMPLE_RECORD_SIZE;
            v->irq_enabled = true;
            break;
        case ApsKind_Loop:
            break;
        default:
            return;
    }
    set_lc(p, v, sample_address(m, r.address));
    set_len(v, r.length_words);
    const bool was_on = v->paula.dma;
    nuade_paula_channel_dma_on(&v->paula);
    if (!was_on) {
        v->irq_pending = true; // the DMA start is a block start
    }
}

void aps_service_interrupts(ApsPlayer* p, ApsVoice* v) {
    for (uint32_t n = 0; n < INTERRUPT_LIMIT && v->irq_pending && v->irq_enabled; n++) {
        v->irq_pending = false;
        audio_interrupt(p, v);
    }
}

// Block starts the mixer played since the last tick are the interrupts that fired in between.
void aps_answer_wraps(ApsPlayer* p, ApsVoice* v, uint32_t index, const NuadeVoices* voices) {
    NuadeVoiceState state;
    nuade_voice_get_state(voices, index, &state);
    for (uint32_t n = state.wraps - v->wraps_seen; n > 0; n--) {
        v->irq_pending = true;
        aps_service_interrupts(p, v);
    }
    v->wraps_seen = state.wraps;
}

// Envelopes

// Envelope `number` of voice `voice`; numbers past 3 reach into the following voices' envelopes (the instrument's
// selectors are 4 bits wide), and past the last voice read 0
static int16_t env_level_number(const ApsPlayer* p, uint32_t voice, uint32_t number) {
    const uint32_t flat = voice * APS_ENVELOPES + number;
    return flat < APS_VOICES * APS_ENVELOPES ? p->voices[flat / APS_ENVELOPES].env[flat % APS_ENVELOPES].level : 0;
}

static void envelope_levels(const ApsVoice* v, int16_t levels[APS_ENVELOPES]) {
    for (uint32_t e = 0; e < APS_ENVELOPES; e++) {
        levels[e] = v->env[e].level;
    }
}

// Point an envelope at the start of a stream. The delta of the step before stays until the first step is over.
static void start_envelope(const ApsModule* m, ApsEnvelope* e, uint16_t offset_words) {
    e->stream = m->envelopes + (uint32_t)offset_words * 2;
    e->level = (int16_t)(aps_env_point(m, e->stream).target * 256);
    e->remaining = 0;
}

// A note with the key flag restarts the envelopes. Envelope 3 only at the first row of a step and when the cell says
// so.
void aps_restart_envelopes(const ApsPlayer* p, ApsVoice* v, const ApsCell* cell) {
    const ApsModule* m = &p->module;
    const ApsInstrument ins = aps_instrument(m, v->instrument);
    for (uint32_t e = 0; e < 3; e++) {
        start_envelope(m, &v->env[e], ins.env_offset[e]);
    }
    if (p->row == 0 && cell->restart_env3) {
        start_envelope(m, &v->env[3], ins.env_offset[3]);
    }
    v->fresh = true;
}

// Envelope 0 switches to the release stream; its level carries on from where it is
void aps_release_envelope(const ApsPlayer* p, ApsVoice* v) {
    const ApsModule* m = &p->module;
    const ApsInstrument ins = aps_instrument(m, v->instrument);
    ApsEnvelope* e = &v->env[0];
    e->stream = m->envelopes + (uint32_t)ins.release_env * 2;
    const ApsEnvPoint point = aps_env_point(m, e->stream);
    e->remaining = (uint16_t)(point.duration * ins.env_speed[0]);
    if (e->remaining != 0) {
        const int16_t span = (int16_t)((int16_t)(point.target * 256) - e->level);
        e->delta = aps_divs16(span, (int16_t)e->remaining);
    }
}

// One tick of envelope `index`. False when the jump budget ran out: the voice's update is abandoned.
static bool advance_envelope(ApsPlayer* p, ApsVoice* v, const ApsInstrument* ins, uint32_t index) {
    const ApsModule* m = &p->module;
    ApsEnvelope* e = &v->env[index];
    const uint8_t speed = ins->env_speed[index];
    if (speed == 0) {
        return true;
    }
    e->level = (int16_t)(uint16_t)(e->level + e->delta);
    const uint16_t before = e->remaining;
    e->remaining--;
    if (before != 0) {
        return true;
    }

    // The pair is over: its target is reached and the next pair starts. Envelope 1 scales the pair lengths by the
    // current level of envelope 2, plus one more for every zero-length pair it skips.
    ApsEnvPoint point = aps_env_point(m, e->stream);
    e->level = (int16_t)(point.target * 256);
    e->stream += 2;
    uint16_t factor = speed;
    if (index == 1) {
        factor = (uint16_t)((uint16_t)(ins->env1_modulation * (uint16_t)v->env[2].level) >> 8) + speed;
    }
    for (;;) {
        point = aps_env_point(m, e->stream);
        if (index == 1) {
            factor++;
        }
        e->remaining = (uint16_t)(point.duration * factor);
        if (e->remaining != 0) {
            break;
        }
        if (--p->jump_budget == 0) {
            return false;
        }
        e->stream += (uint32_t)(point.target * 2);
    }
    const int16_t span = (int16_t)((int16_t)(point.target * 256) - e->level);
    e->delta = aps_divs16(span, (int16_t)e->remaining);
    return true;
}

// Voices

// The wave the voice builds this tick, into its spare buffer; that buffer becomes the one Paula repeats
static void synth_tick(ApsPlayer* p, ApsVoice* v) {
    ApsWaveJob job = { .module = &p->module,
                       .memory = p->wave,
                       .dst = v->wave_build,
                       .record = v->sample,
                       .fresh = v->fresh,
                       .filter = &v->filter };
    envelope_levels(v, job.levels);
    aps_wave_build(&job);
    v->fresh = false;
    const uint32_t built = v->wave_build;
    v->wave_build = v->wave_spare;
    v->wave_spare = built;
    set_lc(p, v, p->module.wave_address + built);
}

// The pitch and volume the instrument's modulation makes of the voice's period, volume and envelope levels
static void write_pitch_and_volume(const ApsPlayer* p, ApsVoice* v, uint32_t index, const ApsInstrument* ins) {
    const uint8_t pitch_select = ins->pitch_select;
    int32_t bend = ((int32_t)env_level_number(p, index, pitch_select >> 4) * ins->pitch_amount_a) >> 8;
    bend = ((int32_t)(int16_t)bend * env_level_number(p, index, (pitch_select >> 2) & 3)) >> 16;
    bend += ((int32_t)env_level_number(p, index, pitch_select & 3) * ins->pitch_amount_b) >> 10;
    v->paula.period = (uint16_t)((uint32_t)(uint16_t)(v->period - ins->period_offset) - (uint32_t)bend);

    const uint8_t volume_select = ins->volume_select;
    int32_t gain = (int32_t)env_level_number(p, index, volume_select >> 2) * ins->volume_amount_a;
    gain += (int32_t)env_level_number(p, index, volume_select & 3) * ins->volume_amount_b;
    gain = (gain >> 8) + ins->volume_base * 16;
    // The eagleplayer's volume hook masks to 7 bits (andi.w #$7f) before AUDxVOL
    v->paula.volume = (uint8_t)((((int32_t)(int16_t)gain * v->volume) >> 12) & 0x7f);
}

// The envelopes, pitch and volume of one voice, and what the sample record does every tick
static void update_voice(ApsPlayer* p, uint32_t index) {
    const ApsModule* m = &p->module;
    ApsVoice* v = &p->voices[index];
    const ApsInstrument ins = aps_instrument(m, v->instrument);

    for (uint32_t e = 0; e < APS_ENVELOPES; e++) {
        if (!advance_envelope(p, v, &ins, e)) {
            return;
        }
    }
    write_pitch_and_volume(p, v, index, &ins);

    const ApsSampleRecord r = aps_sample_record(m, v->sample);
    int16_t levels[APS_ENVELOPES];
    envelope_levels(v, levels);
    switch (r.kind) {
        case ApsKind_Synth:
            synth_tick(p, v);
            break;
        case ApsKind_Window:
        case ApsKind_Window2:
            set_lc(p, v, sample_address(m, r.address + (uint32_t)aps_window_offset(&r, levels)));
            break;
        default:
            break;
    }
}

// The period of a voice that plays an arpeggio stream: bytes with bit 7 set jump by their low 7 bits (signed), the
// others are note offsets. False when the jump budget ran out.
static bool arpeggio_period(ApsPlayer* p, ApsVoice* v) {
    const ApsModule* m = &p->module;
    int8_t entry = (int8_t)aps_u8(m, v->arpeggio);
    while (entry < 0) {
        if (--p->jump_budget == 0) {
            return false;
        }
        int32_t jump = entry & 0x7f;
        if (jump >= 0x40) {
            jump -= 0x80;
        }
        v->arpeggio += (uint32_t)jump;
        entry = (int8_t)aps_u8(m, v->arpeggio);
    }
    v->period = aps_period_lookup(p, (uint32_t)v->note + (entry & 0x3f));
    v->arpeggio++;
    return true;
}

// Without an arpeggio the period glides to the note's period by `slide` per tick
static void glide_period(const ApsPlayer* p, ApsVoice* v) {
    const uint16_t target = aps_period_lookup(p, v->note);
    if (v->slide == 0) {
        v->period = target;
        return;
    }
    if (v->period < target) {
        v->period = (uint16_t)(v->period + v->slide);
        if (v->period >= target) {
            v->period = target;
            return;
        }
    }
    if (v->period > target) {
        v->period = (uint16_t)(v->period - v->slide);
        if (v->period <= target) {
            v->period = target;
        }
    }
}

// The per-tick work of all voices. A voice whose channel volume is 0 only writes the volume.
void aps_update_voices(ApsPlayer* p) {
    for (uint32_t i = 0; i < APS_VOICES; i++) {
        ApsVoice* v = &p->voices[i];
        if (v->channel_volume == 0) {
            v->paula.volume = 0;
            continue;
        }
        if (!aps_track_entry(&p->module, v->track).arpeggio) {
            glide_period(p, v);
        } else if (!arpeggio_period(p, v)) {
            return;
        }
        update_voice(p, i);
    }
}

// Start the sample record's sound: synthesised and window samples go straight to DMA (their loop is the wave or the
// window, moved every tick); the chained kinds wait for their audio interrupt, which only a key flag raises.
void aps_start_sound(ApsPlayer* p, ApsVoice* v, const ApsTrackEntry* t) {
    const ApsModule* m = &p->module;
    const ApsSampleRecord r = aps_sample_record(m, v->sample);
    switch (r.kind) {
        case ApsKind_Synth:
            set_len(v, r.synth_length_words);
            set_lc(p, v, m->wave_address + v->wave_build);
            break;
        case ApsKind_Window:
        case ApsKind_Window2:
            set_len(v, r.window_length_words);
            set_lc(p, v, sample_address(m, r.address));
            break;
        default:
            if (t->key) {
                v->kick = true;
                nuade_paula_channel_dma_off(&v->paula);
            }
            return;
    }
    v->irq_enabled = false;
    nuade_paula_channel_dma_on(&v->paula);
}
