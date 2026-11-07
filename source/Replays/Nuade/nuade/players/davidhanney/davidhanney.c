// SPDX-License-Identifier: MIT
// David Hanney player, translated from the UADE eagleplayer "David Hanney player module V1.1" (Wanted Team).
//
// DhModule: "DSNG", then chunks (tag, length, data): "SEQU" (one pattern number per song position), "INFO" (five words:
// the sequence length, a word nothing reads, the speed, two more nothing reads) and one "BLK<n>" per pattern (the row
// count, then 12 bytes per row: note, instrument and effect byte of each of the four voices). The eagleplayer finds a
// chunk by walking the list from the first chunk, and reads the module by address, past the end of a chunk if a
// count says so; the reads here do the same on the module bytes (zero past the end of the file).
//
// The instruments are not in the module: the eagleplayer holds sixteen (dh_data.c, see DhInstrument).
//
// One tick: every voice runs, then, when the row counter runs out, the next row is decoded and its events start the
// notes. A note start switches the DMA off and writes the sample registers; the voice's next tick writes period and
// volume and switches the DMA on, so the sound starts one tick after the row.

#include <nuade/players/davidhanney.h>

#include "dh_internal.h"
#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define MODULE_MAGIC 0x44534E47u // "DSNG"
#define TAG_SEQUENCE 0x53455155u // "SEQU"
#define TAG_INFO 0x494E464Fu     // "INFO"
#define TAG_PATTERN 0x424C4B00u  // "BLK\0", plus the pattern number
#define TAG_PATTERN_END 0x424C4B01u
#define CHUNK_HEADER 8
#define FIRST_CHUNK 4

#define ROW_BYTES 12
#define ROW_COUNT_BYTES 2
#define CELL_BYTES 3

#define DEFAULT_ENVELOPE_SPEED 2
#define MAX_WAVE_WORDS (DH_WAVE_BYTES / 2)
#define MAX_VIBRATO_WRAP 16
#define MAX_ENVELOPE_LEVEL 63

// Offsets in the replayer's data block from the start of its chord table (see poke_replayer_byte).
#define ROW_OFFSET 176
#define LAST_ROW_OFFSET 188
// What a release writes (see release_note): the pointer it writes through, a byte it clears and a word it sets.
#define RELEASE_CHORD_POINTER 8
#define RELEASE_CLEAR_OFFSET 0x15
#define RELEASE_WORD_OFFSET 0x18
#define RELEASE_WORD 6
#define MAX_VOLUME 63 // the volume table maps the envelope's 0..255 / 4 to 0..63 at full master volume
#define ENVELOPE_LEVEL_SCALE 4
#define PWM_HIGH_VALUE 0x3F // the sweep writes these into the wave
#define PWM_LOW_VALUE ((int8_t)0xC0)
#define WAVE_LOOP_AFTER_START_WORDS 1 // the repeat of a one-shot wave

static uint8_t module_u8(const DhModule* m, int64_t offset) {
    return offset >= 0 && offset < (int64_t)m->size ? m->data[offset] : 0;
}

static uint16_t module_u16(const DhModule* m, int64_t offset) {
    return (uint16_t)((module_u8(m, offset) << 8) | module_u8(m, offset + 1));
}

static uint32_t module_u32(const DhModule* m, int64_t offset) {
    return ((uint32_t)module_u16(m, offset) << 16) | module_u16(m, offset + 2);
}

// The data offset of the first chunk with `tag`. A chunk that is not in the list gives the offset just past the zero
// tag that ends it (past the end of the file if the list runs off it), which is where the replayer looks then.
static int64_t find_chunk(const DhModule* m, uint32_t tag) {
    int64_t offset = FIRST_CHUNK;
    while (offset < (int64_t)m->size) {
        const uint32_t found = module_u32(m, offset);
        if (found == 0) {
            break;
        }
        if (found == tag) {
            return offset + CHUNK_HEADER;
        }
        offset += (int64_t)CHUNK_HEADER + module_u32(m, offset + 4);
    }
    return offset + 4;
}

// Check2: the chunks SEQU, INFO and BLK0 in this order, each skipped by its length, then the tag of BLK1. The
// replayer checks no size; here every step has to stay in the file.
bool dh_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size < FIRST_CHUNK || size > 0x7fffffffu) {
        return false;
    }
    const DhModule m = { .data = (uint8_t*)data, .size = (uint32_t)size };
    if (module_u32(&m, 0) != MODULE_MAGIC) {
        return false;
    }
    int64_t offset = FIRST_CHUNK;
    const uint32_t chunks[] = { TAG_SEQUENCE, TAG_INFO, TAG_PATTERN };
    for (size_t i = 0; i < nuade_sizeof_array(chunks); i++) {
        if (offset + CHUNK_HEADER > (int64_t)m.size || module_u32(&m, offset) != chunks[i]) {
            return false;
        }
        offset += (int64_t)CHUNK_HEADER + module_u32(&m, offset + 4);
    }
    return offset + 4 <= (int64_t)m.size && module_u32(&m, offset) == TAG_PATTERN_END;
}

// The player

typedef enum Effect {
    Effect_None = 0,
    Effect_Slide = 1,         // slide to the note at param + 1 per tick
    Effect_EnvelopeSpeed = 2, // ticks per envelope step - 1
    Effect_Chord = 3,
    Effect_Retrigger = 4, // the note starts again half a row later
} Effect;

static uint8_t chord_byte(const DhPlayer* p, unsigned byte) {
    return p->chords[byte / DH_CHORD_STEPS][byte % DH_CHORD_STEPS];
}

// The period table as the replayer addresses it: the 70 periods, then the chord table read as big-endian words, which
// is where a note plus a chord step past the last period lands. Past the chords are the replayer's variables, which
// are not modelled (no song gets there): they read as 0.
static uint16_t period_at(const DhPlayer* p, uint8_t index) {
    if (index < DH_PERIODS) {
        return k_dh_periods[index];
    }
    const unsigned byte = (unsigned)(index - DH_PERIODS) * 2;
    if (byte + 1 >= sizeof(p->chords)) {
        return 0;
    }
    return (uint16_t)(chord_byte(p, byte) << 8 | chord_byte(p, byte + 1));
}

DhPlayer* dh_create(const uint8_t* data, size_t size) {
    if (!dh_probe(data, size)) {
        return nullptr;
    }
    DhPlayer* p = calloc(1, sizeof(DhPlayer));
    if (p == nullptr) {
        return nullptr;
    }
    p->module.data = malloc(size);
    if (p->module.data == nullptr) {
        free(p);
        return nullptr;
    }
    memcpy(p->module.data, data, size);
    p->module.size = (uint32_t)size;
    p->sequence_base = find_chunk(&p->module, TAG_SEQUENCE);
    p->info_base = find_chunk(&p->module, TAG_INFO);
    return p;
}

void dh_destroy(DhPlayer* player) {
    if (player != nullptr) {
        free(player->module.data);
        free(player);
    }
}

int dh_note_value(uint8_t note) {
    return note > DH_NOTE_RELEASE ? note - DH_NOTE_TABLE_BIAS + DH_NOTE_VALUE_BASE : -1;
}

// The pattern number at a sequence position. The replayer adds the position as a signed word.
static uint8_t sequence_pattern(const DhPlayer* p, uint16_t order) {
    return module_u8(&p->module, p->sequence_base + (int16_t)order);
}

// The data offset of a pattern's row count. The tag is "BLK\0" plus the sign-extended pattern byte.
static int64_t pattern_base(const DhPlayer* p, uint8_t pattern) {
    return find_chunk(&p->module, TAG_PATTERN + (uint32_t)(int32_t)(int8_t)pattern);
}

static uint16_t pattern_rows(const DhPlayer* p, uint8_t pattern) {
    return module_u16(&p->module, pattern_base(p, pattern));
}

static DhCell pattern_cell(const DhPlayer* p, uint8_t pattern, uint16_t row, int voice) {
    const int64_t offset = pattern_base(p, pattern) + ROW_COUNT_BYTES + (int64_t)row * ROW_BYTES + voice * CELL_BYTES;
    const uint8_t effect_byte = module_u8(&p->module, offset + 2);
    return (DhCell) {
        .note = module_u8(&p->module, offset),
        .instrument = module_u8(&p->module, offset + 1),
        .effect = effect_byte >> 4,
        .param = effect_byte & 0x0F,
    };
}

// The pattern at the sequence position, from its first row.
static void select_pattern(DhPlayer* p) {
    p->pattern = sequence_pattern(p, p->position);
    p->last_row = (uint16_t)(pattern_rows(p, p->pattern) - 1);
    p->row = 0;
}

// The replayer clamps the synthetic instruments when the song starts.
static void sanitize_instrument(DhInstrument* ins) {
    if (ins->wave_kind != 0) {
        return;
    }
    ins->length_words = ins->length_words > MAX_WAVE_WORDS ? MAX_WAVE_WORDS : ins->length_words;
    ins->vibrato_wrap = ins->vibrato_wrap > MAX_VIBRATO_WRAP ? MAX_VIBRATO_WRAP : ins->vibrato_wrap;
    ins->pwm_low = ins->pwm_low >= DH_WAVE_BYTES ? DH_WAVE_BYTES - 1 : ins->pwm_low;
    ins->pwm_high = ins->pwm_high >= DH_WAVE_BYTES ? DH_WAVE_BYTES - 1 : ins->pwm_high;
    for (int i = 0; i < DH_ENVELOPE_STAGES; i++) {
        ins->envelope[i].level
            = ins->envelope[i].level > MAX_ENVELOPE_LEVEL ? MAX_ENVELOPE_LEVEL : ins->envelope[i].level;
    }
    if (ins->pwm_low > ins->pwm_high) {
        const uint8_t low = ins->pwm_high;
        ins->pwm_high = ins->pwm_low;
        ins->pwm_low = low;
    }
    if (ins->pwm_pos < ins->pwm_low || ins->pwm_pos > ins->pwm_high) {
        ins->pwm_pos = ins->pwm_low;
    }
    if (ins->pwm_timer > ins->pwm_speed) {
        ins->pwm_timer = 0;
    }
    if (ins->pwm_dir != 1 && ins->pwm_dir != -1) {
        ins->pwm_dir = 1;
    }
}

bool dh_start(DhPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    memcpy(player->instruments, k_dh_instruments, sizeof(player->instruments));
    for (int i = 0; i < DH_INSTRUMENTS; i++) {
        sanitize_instrument(&player->instruments[i]);
    }
    memcpy(player->chords, k_dh_chords, sizeof(player->chords));
    for (int i = 0; i < DH_VOICES; i++) {
        DhVoice* v = &player->voices[i];
        *v = (DhVoice) { .envelope_speed = DEFAULT_ENVELOPE_SPEED, .note = -1, .instrument_number = -1 };
        v->paula.volume = 0;
        nuade_paula_channel_dma_off(&v->paula);
        nuade_voice_stop(voices, (uint32_t)i);
    }
    const DhModule* m = &player->module;
    player->sequence_length = module_u16(m, player->info_base);
    player->speed = module_u16(m, player->info_base + 4);
    player->position = 0;
    select_pattern(player);
    player->row_timer = 0;
    player->tick_counter = 0;
    player->shown = (DhPosition) { .rows = (int)player->last_row + 1, .speed = (int)player->speed + 1 };
    return true;
}

static void stop_voice(DhVoice* v) {
    nuade_paula_channel_dma_off(&v->paula);
    v->mode = DhVoiceMode_Idle;
}

// A note on an instrument: the DMA goes off and the sample registers are written now; period and volume, and the DMA
// start, follow in the voice's next tick (a one-shot wave sets period and volume at once, and starts a tick later).
static void start_note(DhVoice* v, DhInstrument* ins) {
    v->instrument = ins;
    nuade_paula_channel_dma_off(&v->paula);
    if (ins->wave_kind == 0) {
        v->vibrato_delay = ins->vibrato_delay;
        v->vibrato_pos = 0;
        v->envelope_level = 0;
        v->envelope_stage = 0;
        v->envelope_timer = 0;
        v->paula.lc = ins->wave;
        v->paula.len_words = ins->length_words;
        v->mode = DhVoiceMode_Run;
        return;
    }
    uint16_t length_words;
    v->paula.lc = dh_builtin_wave(ins->wave_kind, &length_words);
    v->paula.len_words = length_words;
    v->paula.volume = MAX_VOLUME;
    v->paula.period = v->period;
    v->mode = DhVoiceMode_StartDma;
}

// A byte of the replayer's data block, addressed from the start of its chord table: the table itself, then the
// variables behind it. Only the ones a release writes are modelled: the row and last row words. The other bytes it can
// reach (unused variables, the low byte of the sequence pointer after chord 13, the voice records after chords 14 and
// 15) are not.
static void poke_replayer_byte(DhPlayer* p, unsigned offset, uint8_t value) {
    if (offset < sizeof(p->chords)) {
        p->chords[offset / DH_CHORD_STEPS][offset % DH_CHORD_STEPS] = value;
    } else if (offset == ROW_OFFSET) {
        p->row = (uint16_t)((value << 8) | (p->row & 0x00FF));
    } else if (offset == ROW_OFFSET + 1) {
        p->row = (uint16_t)((p->row & 0xFF00) | value);
    } else if (offset == LAST_ROW_OFFSET) {
        p->last_row = (uint16_t)((value << 8) | (p->last_row & 0x00FF));
    } else if (offset == LAST_ROW_OFFSET + 1) {
        p->last_row = (uint16_t)((p->last_row & 0xFF00) | value);
    }
}

// A note byte of 2 is meant to release the envelope. The replayer clears a byte and writes the word 6 at fixed offsets
// from a pointer it does not set for it: after effect 3 the pointer is the end of the chord just loaded (the chord's
// address + 8), so the release damages the chord table (the period table index can read it) and, after chords 12 and
// 13, the row counter and the row count. After the other effects the pointer is a code address (effects 2 and 4:
// self-modifying code, the original corrupts itself or crashes; not modelled) or whatever the caller left (nothing
// else: the writes are harmless there as far as UADE shows).
static void release_note(DhPlayer* p, const DhVoice* v) {
    if (v->cell.effect != Effect_Chord || v->cell.param >= DH_CHORDS) {
        return;
    }
    const unsigned pointer = (unsigned)v->cell.param * DH_CHORD_STEPS + RELEASE_CHORD_POINTER;
    poke_replayer_byte(p, pointer + RELEASE_CLEAR_OFFSET, 0);
    poke_replayer_byte(p, pointer + RELEASE_WORD_OFFSET, RELEASE_WORD >> 8);
    poke_replayer_byte(p, pointer + RELEASE_WORD_OFFSET + 1, RELEASE_WORD & 0xFF);
}

// The note of the cell.
static void play_note(DhPlayer* p, DhVoice* v) {
    const uint8_t note = v->cell.note;
    if (note == 0) {
        return;
    }
    if (note == DH_NOTE_RELEASE) {
        release_note(p, v);
        return;
    }
    if (note == DH_NOTE_STOP) {
        stop_voice(v);
        return;
    }
    v->note_index = (uint16_t)(note - DH_NOTE_TABLE_BIAS);
    v->period = period_at(p, (uint8_t)v->note_index);
    if (v->cell.instrument >= DH_INSTRUMENTS) {
        stop_voice(v); // the replayer takes a pointer from beyond its table here
        return;
    }
    v->note = dh_note_value(note);
    v->instrument_number = v->cell.instrument;
    start_note(v, &p->instruments[v->cell.instrument]);
}

// The event of a row: the effect, then the note. A slide moves the voice to the note and starts no note.
static void play_cell(DhPlayer* p, DhVoice* v) {
    const DhCell* cell = &v->cell;
    if (cell->note == 0 && cell->instrument == 0 && cell->effect == 0 && cell->param == 0) {
        return;
    }
    v->slide_speed = 0;
    switch (cell->effect) {
        case Effect_Slide:
            v->slide_speed = (uint16_t)(cell->param + 1);
            v->slide_target = period_at(p, cell->note);
            return;
        case Effect_EnvelopeSpeed:
            v->envelope_speed = cell->param;
            break;
        case Effect_Chord:
            if (cell->param < DH_CHORDS) {
                memcpy(v->chord, p->chords[cell->param], sizeof(v->chord));
            } else {
                memset(v->chord, 0, sizeof(v->chord)); // the replayer reads its variables here
            }
            break;
        case Effect_Retrigger:
            v->retrigger_timer = (uint8_t)((uint16_t)(p->speed + 1) >> 1);
            break;
        default:
            break;
    }
    play_note(p, v);
}

static uint16_t slide_period(const DhVoice* v) {
    const uint32_t from = v->period;
    const uint32_t to = v->slide_target;
    if (from < to) {
        return (uint16_t)(to - from > v->slide_speed ? from + v->slide_speed : to);
    }
    return (uint16_t)(from - to > v->slide_speed ? from - v->slide_speed : to);
}

// The period of a voice that does not slide: the note plus the chord step, which advances every tick.
static uint16_t chord_period(const DhPlayer* p, DhVoice* v) {
    const uint8_t index = (uint8_t)(v->note_index + v->chord[v->chord_pos]);
    v->chord_pos = (uint8_t)((v->chord_pos + 1) % DH_CHORD_STEPS);
    return period_at(p, index);
}

// The vibrato position steps down by the speed and wraps up by twice the wrap value.
static uint16_t apply_vibrato(DhVoice* v, uint16_t period) {
    const DhInstrument* ins = v->instrument;
    if (ins->vibrato_speed == 0) {
        return period;
    }
    if (v->vibrato_delay != 0) {
        v->vibrato_delay--;
        return period;
    }
    const int step = v->vibrato_pos < DH_VIBRATO_STEPS ? ins->vibrato[v->vibrato_pos] : 0;
    const uint16_t result = (uint16_t)(period + step * ins->vibrato_depth);
    if (v->vibrato_pos < ins->vibrato_speed) {
        v->vibrato_pos = (uint8_t)(v->vibrato_pos - ins->vibrato_speed + 2 * ins->vibrato_wrap);
    } else {
        v->vibrato_pos = (uint8_t)(v->vibrato_pos - ins->vibrato_speed);
    }
    return result;
}

// The next stage starts when the level reaches the stage's target; the last stage stays.
static void step_envelope(DhVoice* v) {
    const DhEnvelopeStage* stage = &v->instrument->envelope[v->envelope_stage];
    const uint16_t target = (uint16_t)(stage->level * ENVELOPE_LEVEL_SCALE);
    if (v->envelope_level > target) {
        v->envelope_level = v->envelope_level - target > stage->rate ? v->envelope_level - stage->rate : target;
    } else {
        v->envelope_level = target - v->envelope_level > stage->rate ? v->envelope_level + stage->rate : target;
    }
    if (v->envelope_level == target && v->envelope_stage < DH_ENVELOPE_STAGES - 1) {
        v->envelope_stage++;
    }
}

// The pulse width sweep: every `pwm_speed` ticks one byte of the wave is overwritten and the position moves on,
// turning round at the ends. Voices that share an instrument advance it once per tick.
static void sweep_wave(const DhPlayer* p, DhInstrument* ins) {
    if (ins->pwm_speed == 0 || p->tick_counter == ins->pwm_last_tick) {
        return;
    }
    ins->pwm_last_tick = p->tick_counter;
    if (ins->pwm_timer != 0) {
        ins->pwm_timer--;
        return;
    }
    ins->pwm_timer = (uint8_t)(ins->pwm_speed - 1);
    const unsigned pos = ins->pwm_pos % DH_WAVE_BYTES;
    if (ins->pwm_dir < 0) {
        ins->wave[pos] = PWM_HIGH_VALUE;
        if (pos == ins->pwm_low) {
            ins->pwm_dir = -ins->pwm_dir;
        } else {
            ins->pwm_pos = (uint8_t)(pos - 1);
        }
    } else {
        ins->wave[pos] = PWM_LOW_VALUE;
        if (pos == ins->pwm_high) {
            ins->pwm_dir = -ins->pwm_dir;
        } else {
            ins->pwm_pos = (uint8_t)(pos + 1);
        }
    }
}

static void run_voice(const DhPlayer* p, DhVoice* v) {
    uint16_t period;
    if (v->slide_speed != 0) {
        period = v->period;
        if (period != v->slide_target) {
            v->envelope_stage = 0; // a slide restarts the envelope at its first stage, which keeps the volume level
            period = slide_period(v);
            v->period = period;
        }
    } else {
        period = chord_period(p, v);
    }
    v->paula.period = apply_vibrato(v, period);

    if (v->envelope_timer == 0) {
        v->envelope_timer = v->envelope_speed;
        step_envelope(v);
    } else {
        v->envelope_timer--;
    }
    v->paula.volume = (uint8_t)(v->envelope_level / ENVELOPE_LEVEL_SCALE);

    sweep_wave(p, v->instrument);
    nuade_paula_channel_dma_on(&v->paula);
}

// The DMA of a one-shot wave starts; the wave's end repeats the first word.
static void start_one_shot(DhVoice* v) {
    nuade_paula_channel_dma_on(&v->paula);
    v->paula.len_words = WAVE_LOOP_AFTER_START_WORDS;
    v->mode = DhVoiceMode_Idle;
}

static void tick_voice(DhPlayer* p, DhVoice* v) {
    switch (v->mode) {
        case DhVoiceMode_Idle:
            break;
        case DhVoiceMode_StartDma:
            start_one_shot(v);
            break;
        case DhVoiceMode_Run:
            run_voice(p, v);
            break;
    }
    if (v->retrigger_timer != 0 && --v->retrigger_timer == 0) {
        v->cell.effect = Effect_None;
        v->cell.param = 0;
        play_cell(p, v);
    }
}

static void play_row(DhPlayer* p) {
    p->shown = (DhPosition) {
        .order = p->position, .row = p->row, .rows = (int)p->last_row + 1, .speed = (int)p->speed + 1
    };
    for (int i = 0; i < DH_VOICES; i++) {
        p->voices[i].cell = pattern_cell(p, p->pattern, p->row, i);
    }
    for (int i = 0; i < DH_VOICES; i++) {
        play_cell(p, &p->voices[i]);
    }
}

// To the next row, or at the end of the pattern to the next position; the end of the sequence is the eagleplayer's
// song end and the song restarts at position 0. Returns true when the sequence wrapped.
static bool advance_row(DhPlayer* p) {
    if (p->row < p->last_row) {
        p->row++;
        return false;
    }
    p->position++;
    const bool wrapped = p->position == p->sequence_length;
    if (wrapped) {
        p->position = 0;
    }
    select_pattern(p);
    return wrapped;
}

DhTickResult dh_tick(DhPlayer* player, NuadeVoices* voices) {
    DhTickResult result = { 0 };
    player->tick_counter++;
    for (int i = 0; i < DH_VOICES; i++) {
        tick_voice(player, &player->voices[i]);
    }
    if (player->row_timer-- == 0) {
        player->row_timer = player->speed;
        play_row(player);
        result.looped = advance_row(player);
    }
    for (int i = 0; i < DH_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, (uint32_t)i, voices);
    }
    return result;
}

DhPosition dh_position(const DhPlayer* player) {
    return player->shown;
}

int dh_order_count(const DhPlayer* player) {
    return player->sequence_length;
}

DhVoiceState dh_voice_state(const DhPlayer* player, int voice) {
    const DhVoice* v = &player->voices[voice];
    return (DhVoiceState) { .note = v->note, .instrument = v->instrument_number };
}

int dh_pattern_rows(const DhPlayer* player, int order) {
    return pattern_rows(player, sequence_pattern(player, (uint16_t)order));
}

DhCell dh_pattern_cell(const DhPlayer* player, int order, int voice, int row) {
    return pattern_cell(player, sequence_pattern(player, (uint16_t)order), (uint16_t)row, voice);
}
