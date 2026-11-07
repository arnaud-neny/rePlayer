// SPDX-License-Identifier: MIT
// The Musical Enlightenment player, translated from the UADE eagleplayer "The Musical Enlightenment 2.0 player
// module" (Wanted Team). The replayer is in the eagleplayer; modules are data only.
//
// TmeModule: a header (the file size, 16 song entries), 32 instrument records of 128 bytes, 64 arpeggio records of 9
// bytes, 16 LFO waves of 128 bytes, the position list (4 voices of 3 bytes), the steps (6 bytes, tracks of 32), the
// command lists (4 bytes per command, ending at a zero first byte, 256 of them) and the instruments' 8-bit samples.
//
// The replayer reads memory the format does not define for some module contents (a period past its table, an
// instrument that was never selected, a sample past the file). tme_create runs every subsong through two passes and
// refuses a module that reaches any of that, so UADE plays it instead.

#include <nuade/players/musicalenlightenment.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define MIN_MODULE_SIZE 0x1B58
#define FILE_SIZE_OFFSET 0
#define SONG_TABLE_OFFSET 4
#define SONG_SIZE 4
#define INSTRUMENTS_OFFSET 0x44
#define INSTRUMENT_SIZE 0x80
#define ARPEGGIOS_OFFSET 0x1044
#define ARPEGGIO_SIZE 9
#define WAVES_OFFSET 0x1284
#define WAVE_SIZE 0x80
#define WAVE_COUNT 16
#define POSITION_COUNT_OFFSET 0x1A84
#define STEP_COUNT_OFFSET 0x1A86
#define POSITIONS_OFFSET 0x1AAA
#define TRACK_ENTRY_SIZE 3
#define POSITION_SIZE (TME_VOICES * TRACK_ENTRY_SIZE)
#define STEP_SIZE 6
#define TRACK_SIZE (TME_ROWS * STEP_SIZE)
#define COMMAND_SIZE 4

// Check2's shortcut signatures.
#define SIGNATURE_A_OFFSET 0x3C
#define SIGNATURE_B_OFFSET 0x40
#define SIGNATURE_AB 0x50F
#define SIGNATURE_C_OFFSET 0x1284
#define SIGNATURE_C 0x40B11
#define SIGNATURE_D_OFFSET 0x1188
#define SIGNATURE_D 0x181E2329
#define SIGNATURE_E_OFFSET 0x128C
#define SIGNATURE_E 0x2F363C41

// Work limits: the open-time dry run of the whole module (every tick and every list command counts) and the
// commands one voice runs in one tick (the original loops forever on a list that jumps to itself).
#define DRY_RUN_BUDGET 5000000
#define LIST_COMMAND_LIMIT 4096

#define MIN_PERIOD 0x71
#define MAX_VOLUME 0x40
#define ENVELOPE_RELEASE 2
#define ENVELOPE_DONE 3
#define LFO_PITCH_SHIFT 7
#define LFO_VOLUME_SHIFT 9
#define ARPEGGIO_DEFAULT_LENGTH 8

// The period table (three octaves) and the three words after it, which a note past the table reads.
static const uint16_t k_periods[] = {
    0x358, 0x328, 0x2FA, 0x2D0, 0x2A6, 0x280, 0x25C, 0x23A, 0x21A, 0x1FC, 0x1E0, 0x1C5, 0x1AC,
    0x194, 0x17D, 0x168, 0x153, 0x140, 0x12E, 0x11D, 0x10D, 0x0FE, 0x0F0, 0x0E2, 0x0D6, 0x0CA,
    0x0BE, 0x0B4, 0x0AA, 0x0A0, 0x097, 0x08F, 0x087, 0x07F, 0x078, 0x071, 0x001, 0x002, 0x001,
};

// What the envelope phase advances by when a phase's level is reached: attack to decay, decay straight to the hold.
static const uint16_t k_envelope_advance[TME_ENVELOPE_PHASES] = { 1, 2, 1 };

enum {
    StepNote_Tie = 0x80,
    StepNote_Mask = 0x7F,
    StepNote_Release = 0x7E,
    StepNote_Stop = 0x7F,
};

enum {
    StepFlag_Arpeggio = 0x01,
    StepFlag_Slide = 0x02,
    StepFlag_AbsoluteNote = 0x04,
    StepFlag_AbsoluteInstrument = 0x08,
    StepFlag_Portamento = 0x10,
    StepFlag_ArpeggioOnce = 0x20,
    StepFlag_ArpeggioRestart = 0x40,
};

// The high nibble of a command list entry.
typedef enum ListOp {
    ListOp_End = 0,
    ListOp_PitchDepth,
    ListOp_PitchRate,
    ListOp_VolumeDepth,
    ListOp_VolumeRate,
    ListOp_ThirdDepth,
    ListOp_ThirdRate,
    ListOp_Delay,
    ListOp_Jump,
} ListOp;

// A byte of the module as the replayer reads it; past the file it reads memory the format does not define.
static uint8_t image_u8(TmePlayer* p, uint64_t offset) {
    if (offset >= p->module.size) {
        p->unmodelled = true;
        return 0;
    }
    return p->module.image[offset];
}

static uint16_t image_u16(TmePlayer* p, uint64_t offset) {
    return (uint16_t)(image_u8(p, offset) << 8 | image_u8(p, offset + 1));
}

static uint16_t period_at(TmePlayer* p, uint8_t index) {
    if (index >= nuade_sizeof_array(k_periods)) {
        p->unmodelled = true;
        return 0;
    }
    return k_periods[index];
}

static TmeTrack track_at(const TmeModule* m, uint32_t position, int voice) {
    const uint8_t* e
        = m->image + POSITIONS_OFFSET + (size_t)position * POSITION_SIZE + (size_t)voice * TRACK_ENTRY_SIZE;
    return (TmeTrack) { .track = e[0], .instrument_transpose = e[1], .note_transpose = e[2] };
}

static TmeCell step_from(const uint8_t* s) {
    return (TmeCell) {
        .note = s[0], .instrument = s[1], .list = s[2], .volume = (int8_t)s[3], .flags = s[4], .param = s[5]
    };
}

// Parse the instrument records and give each sampled one its place after the command lists.
static bool parse_instruments(TmeModule* m, uint64_t sample_base, uint64_t* end) {
    uint64_t offset = sample_base;
    for (int i = 0; i < TME_INSTRUMENTS; i++) {
        const uint8_t* r = m->image + INSTRUMENTS_OFFSET + (size_t)i * INSTRUMENT_SIZE;
        TmeInstrument* ins = &m->instruments[i];
        ins->sample_size = nuade_read_be32(r + 4);
        ins->has_sample = r[0x18] != 0;
        ins->length_words = nuade_read_be16(r + 0x0A);
        ins->loop_start_words = nuade_read_be32(r + 0x0C);
        ins->loop_length_words = nuade_read_be16(r + 0x12);
        ins->volume = (int8_t)r[0x38];
        for (int k = 0; k < TME_ENVELOPE_PHASES; k++) {
            ins->env_time[k] = r[0x3A + k];
            ins->env_level[k] = r[0x3D + k];
        }
        ins->list_offset = INSTRUMENTS_OFFSET + (uint32_t)i * INSTRUMENT_SIZE + 0x40;
        if (ins->has_sample) {
            ins->sample_offset = (uint32_t)offset;
            offset += ins->sample_size;
            if (offset > m->size) {
                return false;
            }
        }
    }
    *end = offset;
    return true;
}

// The start of every command list: each one ends after a command whose first byte is zero.
static bool parse_lists(TmeModule* m, uint64_t lists_offset, uint64_t* end) {
    uint64_t offset = lists_offset;
    for (int i = 0; i < TME_LIST_COUNT; i++) {
        m->list_offset[i] = (uint32_t)offset;
        do {
            offset += COMMAND_SIZE;
            if (offset > m->size) {
                return false;
            }
        } while (m->image[offset - COMMAND_SIZE] != 0);
    }
    *end = offset;
    return true;
}

static bool parse_songs(TmeModule* m) {
    for (int i = 0; i < TME_MAX_SONGS; i++) {
        const uint8_t* e = m->image + SONG_TABLE_OFFSET + i * SONG_SIZE;
        if (e[0] == 0 && e[1] == 0) {
            break;
        }
        m->songs[i] = (TmeSong) { .start = e[0], .end = e[1], .speed = e[2] };
        for (int v = 0; v < TME_VOICES; v++) {
            m->songs[i].voice_enabled[v] = (e[3] >> v) & 1;
        }
        if (e[0] >= e[1] || e[1] > m->position_count) {
            return false;
        }
        m->song_count++;
    }
    return m->song_count > 0;
}

// Check2 and the layout: the file size in the header must be what the sections add up to, unless the file carries
// one of the signatures Check2 accepts instead.
static bool parse_module(TmeModule* m, const uint8_t* data, size_t size) {
    if (size < MIN_MODULE_SIZE || size > UINT32_MAX / 2 || data[0] != 0) {
        return false;
    }
    const uint32_t file_size = nuade_read_be32(data + FILE_SIZE_OFFSET);
    // InitPlayer refuses a module smaller than its header says
    if (file_size == 0 || (int32_t)file_size > (int32_t)size) {
        return false;
    }
    m->image = nuade_paula_padded_copy(data, size);
    if (m->image == nullptr) {
        return false;
    }
    m->size = (uint32_t)size;
    m->position_count = nuade_read_be16(data + POSITION_COUNT_OFFSET);
    const uint32_t step_count = nuade_read_be16(data + STEP_COUNT_OFFSET);
    m->steps_offset = POSITIONS_OFFSET + m->position_count * POSITION_SIZE;
    const uint64_t lists_offset = (uint64_t)m->steps_offset + (uint64_t)step_count * STEP_SIZE;
    uint64_t end = 0;
    if (lists_offset > size || !parse_lists(m, lists_offset, &end) || !parse_instruments(m, end, &end)
        || !parse_songs(m)) {
        return false;
    }
    const bool signature_ab = nuade_read_be32(data + SIGNATURE_A_OFFSET) == SIGNATURE_AB
                              && nuade_read_be32(data + SIGNATURE_B_OFFSET) == SIGNATURE_AB;
    const bool signature_cde = nuade_read_be32(data + SIGNATURE_C_OFFSET) == SIGNATURE_C
                               && nuade_read_be32(data + SIGNATURE_D_OFFSET) == SIGNATURE_D
                               && nuade_read_be32(data + SIGNATURE_E_OFFSET) == SIGNATURE_E;
    return signature_ab || signature_cde || end == file_size;
}

// The Paula pointer of a module offset, null (and unmodelled) when the sample would run past the file.
static const int8_t* sample_pointer(TmePlayer* p, uint64_t offset, uint16_t len_words) {
    const uint64_t bytes = (len_words != 0 ? (uint64_t)len_words : 65536u) * 2;
    if (offset + bytes > p->module.size) {
        p->unmodelled = true;
        return nullptr;
    }
    return (const int8_t*)p->module.image + offset;
}

static void set_lfo_depth(TmeLfo* lfo, uint8_t target, uint16_t ramp, uint32_t wave_offset) {
    lfo->depth = target;
    lfo->depth_ramp = (uint16_t)(ramp + 1);
    lfo->wave_offset = wave_offset;
}

static void set_lfo_rate(TmeLfo* lfo, uint8_t target, uint16_t ramp) {
    lfo->rate = target;
    lfo->rate_ramp = (uint16_t)(ramp + 1);
}

// The command lists, one per voice and tick: set up the LFOs, wait, jump or end.
static void run_list(TmePlayer* p, TmeVoice* v) {
    if (!v->list_active) {
        return;
    }
    const bool due = v->list_delay == 0;
    v->list_delay--;
    if (!due) {
        return;
    }
    for (int guard = 0; guard < LIST_COMMAND_LIMIT; guard++) {
        if (p->budget-- == 0) {
            break;
        }
        const uint8_t byte0 = image_u8(p, v->list);
        const uint8_t byte1 = image_u8(p, v->list + 1);
        const uint16_t word = image_u16(p, v->list + 2);
        const uint32_t wave = WAVES_OFFSET + (uint32_t)(byte0 & 0x0F) * WAVE_SIZE;
        const unsigned op = byte0 >> 4;
        if (op > ListOp_Jump) {
            p->unmodelled = true;
            v->list_active = false;
            return;
        }
        switch ((ListOp)op) {
            case ListOp_End:
                v->list_active = false;
                return;
            case ListOp_PitchDepth:
                set_lfo_depth(&v->pitch, byte1, word, wave);
                break;
            case ListOp_PitchRate:
                set_lfo_rate(&v->pitch, byte1, word);
                break;
            case ListOp_VolumeDepth:
                set_lfo_depth(&v->volume, byte1, word, wave);
                break;
            case ListOp_VolumeRate:
                set_lfo_rate(&v->volume, byte1, word);
                break;
            case ListOp_ThirdDepth: // a third LFO that nothing runs
            case ListOp_ThirdRate:
                break;
            case ListOp_Delay:
                v->list_delay = word;
                v->list += COMMAND_SIZE;
                return;
            case ListOp_Jump:
                v->list = p->module.list_offset[byte1];
                continue;
        }
        v->list += COMMAND_SIZE;
    }
    p->unmodelled = true;
    v->list_active = false;
}

static void retrigger(TmeVoice* v, bool enabled) {
    v->env_phase = 0;
    v->env_level = 0;
    v->env_ticks = 0;
    v->dma_state = TmeDmaState_Start;
    if (enabled) {
        v->start_dma = true;
        nuade_paula_channel_dma_off(&v->paula);
    }
}

static const TmeInstrument* voice_instrument(TmePlayer* p, TmeVoice* v) {
    if (v->instrument < 0) {
        p->unmodelled = true;
        v->instrument = 0;
    }
    return &p->module.instruments[v->instrument];
}

// A step's instrument: none keeps the voice's, which must exist.
static void select_instrument(TmePlayer* p, TmeVoice* v, const TmeCell* st) {
    if (st->instrument != 0) {
        int selected = st->instrument - 1;
        if (!(st->flags & StepFlag_AbsoluteInstrument)) {
            selected = (selected + v->track.instrument_transpose) & (TME_INSTRUMENTS - 1);
        } else if (selected >= TME_INSTRUMENTS) {
            p->unmodelled = true;
            selected &= TME_INSTRUMENTS - 1;
        }
        v->instrument = selected;
    }
    voice_instrument(p, v);
}

// The note, slide, portamento and arpeggio parts of a step.
static void start_note(TmePlayer* p, TmeVoice* v, const TmeCell* st, uint8_t note, uint32_t step_offset) {
    v->note = note;
    v->pitch.base = period_at(p, note);
    v->slide = (st->flags & StepFlag_Slide) ? (int8_t)st->param : 0;
    v->arp_left = 0;
    v->porta_ticks = 0;
    if (st->flags & StepFlag_Portamento) {
        // The next row holds the target note; it is not played as a step
        uint8_t target = image_u8(p, step_offset + STEP_SIZE) & StepNote_Mask;
        if (!(st->flags & StepFlag_AbsoluteNote)) {
            target = (uint8_t)(target + v->track.note_transpose);
        }
        if (target != 0) {
            v->porta_target = period_at(p, (uint8_t)(target - 1));
            v->porta_ticks = (uint32_t)st->param + 1;
            v->porta_skip = true;
        }
    } else if (st->flags & StepFlag_Arpeggio) {
        v->arp_once = (st->flags & StepFlag_ArpeggioOnce) ? 1 : 0;
        v->arp_restart = (st->flags & StepFlag_ArpeggioRestart) != 0;
        const uint32_t record = ARPEGGIOS_OFFSET + (uint32_t)st->param * ARPEGGIO_SIZE;
        const uint8_t header = image_u8(p, record);
        v->arp_offset = record + 1;
        v->arp_speed = header & 0x0F;
        v->arp_counter = (uint8_t)(v->arp_speed + 1);
        v->arp_length = header >> 4;
        if (v->arp_length == 0) {
            v->arp_length = ARPEGGIO_DEFAULT_LENGTH;
        }
        v->arp_left = (uint8_t)(v->arp_length - 1);
        v->arp_index = 0;
    }
}

static void play_step(TmePlayer* p, int index) {
    TmeVoice* v = &p->voices[index];
    const bool enabled = p->song->voice_enabled[index];
    const uint32_t step_offset = v->step_offset;
    v->step_offset += STEP_SIZE;
    if (v->porta_skip) {
        v->porta_skip = false;
        return;
    }
    uint8_t raw[STEP_SIZE];
    for (int i = 0; i < STEP_SIZE; i++) {
        raw[i] = image_u8(p, step_offset + (uint32_t)i);
    }
    const TmeCell st = step_from(raw);
    const uint8_t note_byte = st.note & StepNote_Mask;
    if (note_byte == 0) {
        return;
    }
    if (note_byte == StepNote_Stop) {
        if (enabled) {
            nuade_paula_channel_dma_off(&v->paula);
        }
        return;
    }
    if (note_byte == StepNote_Release) {
        v->env_phase = ENVELOPE_RELEASE;
        return;
    }
    uint8_t note = note_byte;
    if (!(st.flags & StepFlag_AbsoluteNote)) {
        note = (uint8_t)(note + v->track.note_transpose);
    }
    if (note == 0) {
        return;
    }
    start_note(p, v, &st, (uint8_t)(note - 1), step_offset);

    if (!(st.note & StepNote_Tie)) {
        select_instrument(p, v, &st);
        retrigger(v, enabled);
        v->list = st.list != 0 ? p->module.list_offset[st.list] : p->module.instruments[v->instrument].list_offset;
        v->pitch.depth_now = 0;
        v->pitch.rate_now = 0;
        v->pitch.phase = 0;
        v->volume.depth_now = 0;
        v->volume.rate_now = 0;
        v->volume.phase = 0;
        v->list_active = true;
        v->list_delay = 0;
    }
    const TmeInstrument* ins = voice_instrument(p, v);
    v->volume_scale = (int8_t)(uint8_t)(ins->volume + st.volume);
}

// A new position: each voice gets its track and transposes. The song wraps when the last position is loaded.
static void load_position(TmePlayer* p) {
    for (int i = 0; i < TME_VOICES; i++) {
        TmeVoice* v = &p->voices[i];
        v->track = track_at(&p->module, p->position, i);
        v->step_offset = p->module.steps_offset + (uint32_t)v->track.track * TRACK_SIZE;
    }
    p->shown_order = p->position;
    p->position++;
    if (p->position == p->song->end) {
        p->position = p->song->start;
        p->looped = true;
    }
    p->rows_left = TME_ROWS;
}

static void run_list_all(TmePlayer* p) {
    for (int i = 0; i < TME_VOICES; i++) {
        run_list(p, &p->voices[i]);
    }
}

// One LFO step: ramp depth and rate to their targets, read the wave at the phase and add it to the base.
static void run_lfo(TmePlayer* p, TmeLfo* lfo, unsigned shift) {
    if (lfo->depth == 0) {
        lfo->out = lfo->base;
        return;
    }
    if (lfo->depth_ramp != 0) {
        const int32_t diff = (int8_t)(uint8_t)(lfo->depth - lfo->depth_now);
        lfo->depth_now = (uint8_t)(lfo->depth_now + (uint8_t)(diff / (int16_t)lfo->depth_ramp));
        lfo->depth_ramp--;
    }
    if (lfo->rate_ramp != 0) {
        const int32_t diff = (int8_t)(uint8_t)(lfo->rate - lfo->rate_now);
        lfo->rate_now = (uint8_t)(lfo->rate_now + (uint8_t)(diff / (int16_t)lfo->rate_ramp));
        lfo->rate_ramp--;
    }
    if (lfo->wave_offset == 0) {
        p->unmodelled = true;
        return;
    }
    const int32_t sample = (int8_t)image_u8(p, lfo->wave_offset + lfo->phase);
    const int32_t product = sample * (int32_t)lfo->depth_now;
    lfo->out = (uint16_t)(lfo->base + (uint16_t)((uint32_t)product >> shift));
    lfo->phase = (uint8_t)((lfo->phase + lfo->rate_now) & 0x7F);
}

// The arpeggio: step through the note offsets every arp_speed + 1 ticks.
static void run_arpeggio(TmePlayer* p, TmeVoice* v, bool enabled) {
    if (v->arp_counter-- == 0) {
        v->arp_index = (uint16_t)((v->arp_index + 1) % v->arp_length);
        v->arp_counter = v->arp_speed;
        v->arp_left = (uint8_t)(v->arp_left - v->arp_once);
        if (v->arp_restart) {
            retrigger(v, enabled);
        }
    }
    const uint8_t offset = image_u8(p, v->arp_offset + v->arp_index);
    v->pitch.base = period_at(p, (uint8_t)(v->note + offset));
}

// The volume envelope: three phases of (time, level), then a hold.
static void run_envelope(TmeVoice* v, const TmeInstrument* ins) {
    if (v->env_ticks != 0) {
        const int32_t diff = (int32_t)v->env_target - (int32_t)v->env_level;
        v->env_level = (uint16_t)(v->env_level + (uint16_t)(diff / (int16_t)v->env_ticks));
        v->env_ticks--;
    } else if (v->env_phase < ENVELOPE_DONE) {
        v->env_ticks = (uint32_t)ins->env_time[v->env_phase] + 1;
        v->env_target = ins->env_level[v->env_phase];
        v->env_phase = (uint16_t)(v->env_phase + k_envelope_advance[v->env_phase]);
    }
}

static void run_synth(TmePlayer* p, int index) {
    TmeVoice* v = &p->voices[index];
    if (v->instrument < 0) {
        return;
    }
    const bool enabled = p->song->voice_enabled[index];
    const TmeInstrument* ins = &p->module.instruments[v->instrument];

    v->pitch.base = (uint16_t)(v->pitch.base + (uint16_t)v->slide);
    if (v->porta_ticks != 0) {
        const int32_t diff = (int32_t)v->porta_target - (int32_t)v->pitch.base;
        v->pitch.base = (uint16_t)(v->pitch.base + (uint16_t)(diff / (int16_t)v->porta_ticks));
        v->porta_ticks--;
    } else if (v->arp_left != 0) {
        run_arpeggio(p, v, enabled);
    }
    run_lfo(p, &v->pitch, LFO_PITCH_SHIFT);
    v->regs.period = (int16_t)v->pitch.out < MIN_PERIOD ? MIN_PERIOD : v->pitch.out;

    run_envelope(v, ins);
    const int16_t scaled = (int16_t)((uint32_t)((int32_t)v->volume_scale * (int16_t)v->env_level) & 0xFFFF);
    v->volume.base = (uint16_t)(scaled >> 8);
    run_lfo(p, &v->volume, LFO_VOLUME_SHIFT);
    v->regs.volume = (int16_t)v->volume.out > MAX_VOLUME ? MAX_VOLUME : v->volume.out;

    if (v->dma_state == TmeDmaState_Start) {
        if (!ins->has_sample) {
            p->unmodelled = true;
        }
        v->regs.lc = ins->sample_offset;
        v->regs.lc_set = ins->has_sample;
        v->regs.len_words = ins->length_words;
        v->dma_state = TmeDmaState_Repeat;
    } else if (v->dma_state == TmeDmaState_Repeat) {
        v->regs.lc = (uint64_t)ins->sample_offset + (uint64_t)ins->loop_start_words * 2;
        v->regs.lc_set = ins->has_sample;
        v->regs.len_words = ins->loop_length_words;
        v->dma_state = TmeDmaState_Idle;
    }
}

// The replayer's copy of the registers to Paula, for the voices the song enables.
static void write_paula(TmePlayer* p, int index) {
    TmeVoice* v = &p->voices[index];
    if (!p->song->voice_enabled[index]) {
        return;
    }
    NuadePaulaChannel* c = &v->paula;
    // Samples follow each other without padding, so one can start at an odd address; Paula ignores AUDxLC bit 0 and
    // the module sits at an even address
    c->lc = v->regs.lc_set ? sample_pointer(p, v->regs.lc & ~(uint64_t)1, v->regs.len_words) : nullptr;
    c->len_words = v->regs.len_words;
    c->period = v->regs.period;
    c->volume = (uint8_t)(v->regs.volume & 0x7F);
}

static void play_row(TmePlayer* p) {
    p->tick_counter = p->song->speed;
    p->rows_left--;
    if (p->rows_left == 0) {
        load_position(p);
    }
    p->shown_row = TME_ROWS - p->rows_left;
    for (int i = 0; i < TME_VOICES; i++) {
        play_step(p, i);
    }
}

TmeTickResult tme_tick(TmePlayer* p, NuadeVoices* voices) {
    p->looped = false;
    // DMACON comes first; the loop registers the tick writes after the raster wait are the repeat
    for (int i = 0; i < TME_VOICES; i++) {
        if (p->voices[i].start_dma) {
            nuade_paula_channel_dma_on(&p->voices[i].paula);
            p->voices[i].start_dma = false;
        }
    }
    if (p->tick_counter == 0) {
        play_row(p);
    } else {
        p->tick_counter--;
    }
    run_list_all(p);
    for (int i = 0; i < TME_VOICES; i++) {
        run_synth(p, i);
    }
    for (int i = 0; i < TME_VOICES; i++) {
        write_paula(p, i);
        if (voices != nullptr) {
            nuade_paula_channel_sync(&p->voices[i].paula, (uint32_t)i, voices);
        }
    }
    return (TmeTickResult) { .looped = p->looped };
}

// InitSound
bool tme_start(TmePlayer* p, uint32_t subsong, NuadeVoices* voices) {
    if (p == nullptr || subsong >= (uint32_t)p->module.song_count) {
        return false;
    }
    p->song = &p->module.songs[subsong];
    memset(p->voices, 0, sizeof(p->voices));
    for (int i = 0; i < TME_VOICES; i++) {
        p->voices[i].instrument = -1;
        p->voices[i].note = -1;
        if (voices != nullptr) {
            nuade_voice_stop(voices, (uint32_t)i);
        }
    }
    p->position = p->song->start;
    load_position(p);
    p->looped = false;
    p->rows_left = TME_ROWS + 1;
    p->shown_row = 0;
    p->tick_counter = 0;
    return true;
}

// The replayer's state apart from the Paula channels, which nothing reads back.
typedef struct Snapshot {
    uint32_t position;
    uint32_t rows_left;
    uint8_t tick_counter;
    TmeVoice voices[TME_VOICES];
} Snapshot;

static void snapshot_take(const TmePlayer* p, Snapshot* s) {
    memset(s, 0, sizeof(*s));
    s->position = p->position;
    s->rows_left = p->rows_left;
    s->tick_counter = p->tick_counter;
    for (int i = 0; i < TME_VOICES; i++) {
        memcpy(&s->voices[i], &p->voices[i], offsetof(TmeVoice, paula));
    }
}

static bool snapshot_equal(const TmePlayer* p, const Snapshot* s) {
    if (s->position != p->position || s->rows_left != p->rows_left || s->tick_counter != p->tick_counter) {
        return false;
    }
    for (int i = 0; i < TME_VOICES; i++) {
        if (memcmp(&s->voices[i], &p->voices[i], offsetof(TmeVoice, paula)) != 0) {
            return false;
        }
    }
    return true;
}

// Every subsong until its complete state repeats (Brent's cycle detection): from there on it only replays states
// the run has seen, so nothing unmodelled can appear later. One work budget covers the whole module.
static bool dry_run(TmePlayer* p) {
    p->budget = DRY_RUN_BUDGET;
    Snapshot saved;
    bool ok = true;
    for (int song = 0; song < p->module.song_count && ok; song++) {
        p->unmodelled = false;
        tme_start(p, (uint32_t)song, nullptr);
        snapshot_take(p, &saved);
        uint32_t power = 1;
        uint32_t steps = 0;
        for (;;) {
            if (p->budget-- == 0) {
                ok = false;
                break;
            }
            tme_tick(p, nullptr);
            if (p->unmodelled) {
                ok = false;
                break;
            }
            if (snapshot_equal(p, &saved)) {
                break;
            }
            if (++steps == power) {
                snapshot_take(p, &saved);
                power *= 2;
                steps = 0;
            }
        }
    }
    p->budget = UINT64_MAX;
    return ok;
}

TmePlayer* tme_create(const uint8_t* data, size_t size) {
    if (data == nullptr) {
        return nullptr;
    }
    TmePlayer* p = calloc(1, sizeof(TmePlayer));
    if (p == nullptr) {
        return nullptr;
    }
    p->budget = UINT64_MAX;
    if (!parse_module(&p->module, data, size) || !dry_run(p)) {
        tme_destroy(p);
        return nullptr;
    }
    tme_start(p, 0, nullptr);
    return p;
}

bool tme_probe(const uint8_t* data, size_t size) {
    TmePlayer* p = tme_create(data, size);
    const bool ok = p != nullptr;
    tme_destroy(p);
    return ok;
}

void tme_destroy(TmePlayer* p) {
    if (p == nullptr) {
        return;
    }
    free(p->module.image);
    free(p);
}

int tme_subsong_count(const TmePlayer* p) {
    return p->module.song_count;
}

TmePosition tme_position(const TmePlayer* p) {
    return (TmePosition) { .order = (int)p->shown_order, .row = (int)p->shown_row, .speed = p->song->speed + 1 };
}

int tme_order_count(const TmePlayer* p) {
    return (int)p->module.position_count;
}

TmeVoiceState tme_voice_state(const TmePlayer* p, int voice) {
    const TmeVoice* v = &p->voices[voice];
    return (TmeVoiceState) { .note = v->note, .instrument = v->instrument };
}

TmeCell tme_pattern_cell(const TmePlayer* p, uint32_t order, uint32_t voice, uint32_t row) {
    const TmeModule* m = &p->module;
    if (order >= m->position_count || voice >= TME_VOICES || row >= TME_ROWS) {
        return (TmeCell) { 0 };
    }
    const TmeTrack track = track_at(m, order, (int)voice);
    const uint64_t offset = (uint64_t)m->steps_offset + (uint64_t)track.track * TRACK_SIZE + (uint64_t)row * STEP_SIZE;
    if (offset + STEP_SIZE > m->size) {
        return (TmeCell) { 0 };
    }
    return step_from(m->image + offset);
}
