// SPDX-License-Identifier: MIT
// Delta Music 2.0 replayer. A module carries the player's 68000 code in front of the song data; this is a port of
// that code's play routine. The player reads everything through pointers into the module image, so the port keeps
// the image as bytes: tables are read where the player reads them (also when a song makes it run past their end),
// and the two things the player writes into the image, the noise waveform and the cleared first word of a played
// sample, are written there too.
//
// Offsets in comments ($xx) are the field's offset in the player's voice record or global record, and image
// offsets are module-relative.
#include <nuade/players/deltamusic2.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include "nuade_tracker.h"
#include "nuade_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Image offsets fixed by the player's code.
#define DM2_VARS 0xb8c         // global record
#define DM2_VOICES_PROBE 0xbc6 // ".FNL"
#define DM2_ARPEGGIOS 0xbca    // 64 tables of 16 signed bytes
#define DM2_ARPEGGIO_BYTES 16
#define DM2_SONG_HEADER 0xfca // 4 x (loop position, length) of the tracks, in bytes
#define DM2_PERIODS 0xae2     // word table indexed by note + transpose
#define DM2_MIN_SIZE 0xfda

// The image is padded with zeros so that the reads of a song that runs past the data stay inside it.
#define DM2_IMAGE_PAD 0x40000

#define DM2_BLOCK_BYTES 64 // 16 rows of 4 bytes
#define DM2_ROW_MASK 0x3f

// Instrument record (in the image, $ is the offset in the record).
#define INST_LENGTH 0x00        // sound length in words
#define INST_REPEAT_START 0x02  // bytes
#define INST_REPEAT_LENGTH 0x04 // words
#define INST_VOLUME_TABLE 0x06  // 5 x (speed, level, sustain)
#define INST_VIBRATO_TABLE 0x15 // 5 x (speed, delay, sustain)
#define INST_PITCH_BEND 0x24
#define INST_SAMPLE_FLAG 0x26 // top bit set: a sample instrument
#define INST_INDEX 0x27       // sample number (low 3 bits) or sound table delay
#define INST_SOUND_TABLE 0x28

#define NO_BUFFER 0xffffffffu // the buffer Paula's address registers hold after reset

// Image access

static uint8_t rd8(const Dm2Module* m, uint32_t at) {
    return at < m->image_size ? m->image[at] : 0;
}

static uint16_t rd16(const Dm2Module* m, uint32_t at) {
    return (uint16_t)((rd8(m, at) << 8) | rd8(m, at + 1));
}

static uint32_t rd32(const Dm2Module* m, uint32_t at) {
    return ((uint32_t)rd16(m, at) << 16) | rd16(m, at + 2);
}

static void wr8(Dm2Module* m, uint32_t at, uint8_t v) {
    if (at < m->image_size)
        m->image[at] = v;
}

static void wr16(Dm2Module* m, uint32_t at, uint16_t v) {
    wr8(m, at, (uint8_t)(v >> 8));
    wr8(m, at + 1, (uint8_t)v);
}

// Start-up ($1fc, $36c)

// Where the player's start-up finds the parts of the song.
static void locate_song(const Dm2Module* m, Dm2Vars* v) {
    uint32_t p = DM2_SONG_HEADER + 16;
    v->tracks[0] = p;
    for (int i = 1; i < DM2_CHANNELS; i++) {
        p += rd16(m, DM2_SONG_HEADER + 4 * (uint32_t)i - 2);
        v->tracks[i] = p;
    }
    p += rd16(m, DM2_SONG_HEADER + 4 * 3 + 2);

    const uint32_t block_bytes = rd32(m, p);
    p += 4;
    v->blocks = p;
    p += block_bytes;
    v->inst_offsets = p;
    p += 0xfe;
    const uint32_t inst_bytes = rd16(m, p);
    p += 2;
    v->inst_base = p;
    p += inst_bytes;
    const uint32_t wave_bytes = rd32(m, p);
    p += 4;
    v->waveforms = p;
    p += wave_bytes;
    v->sample_offsets = p + 0x40;
    v->sample_base = p + 0x60;
}

// A voice starts from the record the module was saved with; start-up resets some fields and leaves the others.
static void init_voice(const Dm2Module* m, int index, Dm2Voice* v) {
    static const uint16_t record[DM2_CHANNELS] = { 0x878, 0x8b4, 0x904, 0x954 };
    const uint32_t r = record[index];

    memset(v, 0, sizeof(*v));
    v->table_delay = rd8(m, r + 0x16);
    v->table_pos = rd8(m, r + 0x17);
    v->period = rd16(m, r + 0x18);
    v->target_period = rd16(m, r + 0x1a);
    v->note = rd8(m, r + 0x1c);
    v->vibrato_dir = rd8(m, r + 0x27);
    v->vibrato = rd16(m, r + 0x28);
    v->vibrato_delay = rd8(m, r + 0x2a);
    v->transpose = (int8_t)rd8(m, r + 0x2b);
    v->vibrato_pos = rd8(m, r + 0x34);
    v->vibrato_sustain = rd8(m, r + 0x35);
    v->block = rd32(m, r + 0x0e);

    v->track = m->vars.tracks[index];
    v->loop_pos = rd16(m, DM2_SONG_HEADER + 4 * (uint32_t)index);
    v->track_len = rd16(m, DM2_SONG_HEADER + 4 * (uint32_t)index + 2);
    v->instrument = m->vars.inst_base;
    v->arpeggio = DM2_ARPEGGIOS;
    v->max_volume = 0x3f;
    v->shown_instrument = -1;
}

static void init_sound(Dm2Module* m) {
    memcpy(m->image, m->file, m->file_size);
    memset(m->image + m->file_size, 0, m->image_size - m->file_size);

    Dm2Vars* v = &m->vars;
    const uint32_t vars = DM2_VARS;
    locate_song(m, v);
    v->noise_seed = rd32(m, vars + 0x2a);
    v->master_volume = (int8_t)rd8(m, vars + 0x2e);
    v->speed = (int8_t)rd8(m, vars + 0x2f);
    v->counter = 1;

    for (int i = 0; i < DM2_CHANNELS; i++) {
        init_voice(m, i, &m->voice[i]);
        m->paula[i] = (Dm2Paula) { .lc = NO_BUFFER, .len = 1, .per = 0, .vol = 0 };
    }

    m->loop_count = 0;
    m->wrapped_mask = 0;
    m->tick_number = 0;
}

// Play routine ($3bc, $49e)

// The noise waveform, the first 64 bytes of the first waveform, is rewritten every tick.
static void generate_noise(Dm2Module* m) {
    uint32_t d1 = m->vars.noise_seed;
    for (uint32_t i = 0; i < 16; i++) {
        d1 = (d1 << 7) | (d1 >> 25);
        d1 += 0x6eca756d;
        d1 ^= 0x9e59a92b;
        for (uint32_t b = 0; b < 4; b++)
            wr8(m, m->vars.waveforms + i * 4 + b, (uint8_t)(d1 >> (24 - 8 * b)));
    }
    m->vars.noise_seed = d1;
}

static uint16_t period_at(const Dm2Module* m, uint8_t index) {
    return rd16(m, DM2_PERIODS + 2u * index);
}

// Effect numbers 1..0x80 run routine (number - 1) & 7; the others do nothing.
static void do_effect(Dm2Module* m, Dm2Voice* v, uint8_t effect, uint8_t arg) {
    const uint8_t index = (uint8_t)(effect - 1);
    if (index & 0x80)
        return;

    switch (index & 7) {
        case 0: // set speed
            m->vars.speed = (int8_t)(arg & 0x0f);
            break;
        case 1: // filter
            break;
        case 2: // pitch bend up
            v->pitch_bend = (uint16_t)-(int)arg;
            break;
        case 3: // pitch bend down
            v->pitch_bend = arg;
            break;
        case 4: // portamento
            v->portamento = arg;
            break;
        case 5: // voice volume limit
            v->max_volume = arg & 0x3f;
            break;
        case 6: // master volume
            m->vars.master_volume = (int8_t)(arg & 0x3f);
            break;
        case 7: // arpeggio table
            v->arpeggio = DM2_ARPEGGIOS + (uint32_t)(arg & 0x3f) * DM2_ARPEGGIO_BYTES;
            break;
    }
}

// The address of a sample instrument's sound.
static uint32_t sample_address(const Dm2Module* m, uint32_t inst) {
    return rd32(m, m->vars.sample_offsets + 4u * (rd8(m, inst + INST_INDEX) & 7)) + m->vars.sample_base;
}

static void set_buffer(Dm2Paula* p, uint32_t lc, uint16_t len) {
    p->lc = lc;
    p->len = len;
    p->lc_len_written = true;
}

// The track list entry at the voice's position starts the next block.
static void next_block(Dm2Module* m, int index, Dm2Voice* v) {
    const uint32_t entry = v->track + v->track_pos;
    v->shown_track_pos = v->track_pos;
    v->block = m->vars.blocks + (uint32_t)rd8(m, entry) * DM2_BLOCK_BYTES;
    v->transpose = (int8_t)rd8(m, entry + 1);
    v->track_pos = (uint16_t)(v->track_pos + 2);
    if ((int16_t)(uint16_t)(v->track_pos - v->track_len) >= 0) {
        v->track_pos = v->loop_pos;
        m->wrapped_mask |= 1u << index;
    }
}

static void start_note(Dm2Module* m, Dm2Voice* v, Dm2Paula* paula, uint32_t row) {
    const uint8_t note = rd8(m, row);

    paula->dma_off = true;
    v->note = note;
    v->shown_note = (uint8_t)(note + (uint8_t)v->transpose);
    v->shown_instrument = rd8(m, row + 1);
    v->target_period = period_at(m, v->shown_note);

    // Instrument number n is the word n - 1 of the offset table
    // (a 16-bit subtraction: for 0 it is the word 0xfffe bytes on).
    const uint16_t word = (uint16_t)(rd8(m, row + 1) * 2 - 2);
    const uint32_t offset = rd16(m, m->vars.inst_offsets + word);
    const uint32_t inst = offset + m->vars.inst_base;
    v->instrument = inst;

    v->sample_start = rd8(m, inst + INST_SAMPLE_FLAG);
    if (v->sample_start & 0x80)
        set_buffer(paula, sample_address(m, inst), rd16(m, inst + INST_LENGTH));

    v->table_delay = 0;
    v->table_pos = 0;
    v->volume = 0;
    v->volume_sustain = 0;
    v->volume_pos = 0;
    v->arpeggio_pos = 0;
    v->vibrato_dir = 0;
    v->vibrato = 0;
    v->vibrato_delay = rd8(m, inst + INST_VIBRATO_TABLE + 1);
    v->vibrato_pos = 0;
    v->vibrato_sustain = rd8(m, inst + INST_VIBRATO_TABLE + 2);
}

static void play_sound_table(Dm2Module* m, Dm2Voice* v, Dm2Paula* paula, uint32_t inst) {
    if (v->table_delay != 0) {
        v->table_delay--;
        return;
    }
    v->table_delay = rd8(m, inst + INST_INDEX);

    const uint32_t table = inst + INST_SOUND_TABLE;
    uint8_t position = v->table_pos;
    uint8_t wave = rd8(m, table + position);
    if (wave == 0xff) {
        position = rd8(m, table + position + 1u);
        wave = rd8(m, table + position);
        if (wave == 0xff)
            return; // two jumps in a row: the position stays
    }
    set_buffer(paula, m->vars.waveforms + ((uint32_t)wave << 8), rd16(m, inst + INST_LENGTH));
    v->table_pos = (uint8_t)((position + 1) & DM2_ROW_MASK);
}

static void play_vibrato(Dm2Module* m, Dm2Voice* v, uint32_t inst) {
    const uint32_t entry = inst + INST_VIBRATO_TABLE + v->vibrato_pos;
    const uint8_t speed = rd8(m, entry);

    if (v->vibrato_dir == 0)
        v->vibrato = (uint16_t)(v->vibrato + speed);
    else
        v->vibrato = (uint16_t)(v->vibrato - speed);

    if (--v->vibrato_delay == 0) {
        v->vibrato_delay = rd8(m, entry + 1);
        v->vibrato_dir = (uint8_t)~v->vibrato_dir;
    }

    if (v->vibrato_sustain != 0) {
        v->vibrato_sustain--;
    } else {
        v->vibrato_pos = (uint8_t)(v->vibrato_pos + 3);
        if (v->vibrato_pos == 0xf)
            v->vibrato_pos = 0xc;
        v->vibrato_sustain = rd8(m, entry + 5);
    }
}

// True when a - b is not negative as a 16-bit value (cmp.w, bpl).
static bool word_not_negative(uint16_t a, uint16_t b) {
    return (uint16_t)(a - b) < 0x8000;
}

static void play_volume_envelope(Dm2Module* m, Dm2Voice* v, uint32_t inst) {
    if (v->volume_sustain != 0) {
        v->volume_sustain--;
        return;
    }

    const uint32_t entry = inst + INST_VOLUME_TABLE + v->volume_pos;
    const uint16_t speed = rd8(m, entry);
    const uint16_t level = rd8(m, entry + 1);

    if (!word_not_negative(level, v->volume)) {
        v->volume = (uint16_t)(v->volume - speed);
        if (!word_not_negative(level, v->volume))
            return;
        v->volume = level;
        v->volume_pos = (uint16_t)(v->volume_pos + 3);
        v->volume_sustain = rd8(m, entry + 2);
    } else {
        v->volume = (uint16_t)(v->volume + speed);
        if (word_not_negative(level, v->volume))
            return;
        v->volume = level;
        v->volume_pos = (uint16_t)(v->volume_pos + 3);
        if (v->volume_pos == 0xf)
            v->volume_pos = 0xc;
        v->volume_sustain = rd8(m, entry + 2);
    }
}

static void play_portamento(Dm2Voice* v) {
    const uint16_t step = v->portamento;
    if (step == 0)
        return;

    if (word_not_negative(v->target_period, v->period)) {
        v->period = (uint16_t)(v->period + step);
        if (!word_not_negative(v->target_period, v->period))
            v->period = v->target_period;
    } else {
        v->period = (uint16_t)(v->period - step);
        if (word_not_negative(v->target_period, v->period))
            v->period = v->target_period;
    }
}

static uint8_t next_arpeggio(const Dm2Module* m, Dm2Voice* v) {
    for (;;) {
        const uint8_t step = rd8(m, v->arpeggio + v->arpeggio_pos);
        if ((v->arpeggio_pos & 0xff) != 0 && step == 0x80) {
            v->arpeggio_pos = 0;
            continue;
        }
        v->arpeggio_pos = (uint16_t)((v->arpeggio_pos + 1) & 0xf);
        return step;
    }
}

static void play_voice(Dm2Module* m, int index, bool row_tick) {
    Dm2Voice* v = &m->voice[index];
    Dm2Paula* paula = &m->paula[index];
    uint32_t inst = v->instrument;

    // The repeat of a sample is set a tick after it started; its first word is cleared.
    if (v->sample_start & 0x80) {
        v->sample_start = 0;
        const uint32_t base = sample_address(m, inst);
        paula->len = rd16(m, inst + INST_REPEAT_LENGTH);
        paula->lc_len_written = true;
        wr16(m, base, 0);
        paula->lc = base + rd16(m, inst + INST_REPEAT_START);
    }

    if (row_tick) {
        if (v->block_pos == 0)
            next_block(m, index, v);

        const uint32_t row = v->block + v->block_pos;
        if (rd8(m, row) != 0) {
            start_note(m, v, paula, row);
            inst = v->instrument;
        }
        do_effect(m, v, rd8(m, row + 2), rd8(m, row + 3));
        v->shown_row = v->block_pos;
        v->block_pos = (uint16_t)((v->block_pos + 4) & DM2_ROW_MASK);
    }

    if ((rd8(m, inst + INST_SAMPLE_FLAG) & 0x80) == 0)
        play_sound_table(m, v, paula, inst);
    play_vibrato(m, v, inst);
    play_volume_envelope(m, v, inst);
    play_portamento(v);

    const uint8_t step = next_arpeggio(m, v);
    uint16_t period;
    if (v->portamento != 0) {
        period = v->period;
    } else {
        period = period_at(m, (uint8_t)(step + v->note + (uint8_t)v->transpose));
        v->period = period;
    }

    const uint16_t bend = (uint16_t)(rd16(m, inst + INST_PITCH_BEND) - v->pitch_bend);
    v->vibrato = (uint16_t)(v->vibrato - bend);
    paula->per = (uint16_t)(period + v->vibrato);

    // Volume: the envelope level in steps of 4, limited by the voice and by the master volume (signed compares).
    int8_t volume = (int8_t)((v->volume >> 2) & 0x3f);
    if ((int8_t)(volume - (int8_t)v->max_volume) >= 0)
        volume = (int8_t)v->max_volume;
    if ((int8_t)(volume - m->vars.master_volume) >= 0)
        volume = m->vars.master_volume;
    paula->vol = (uint16_t)(uint8_t)volume;
}

static void play_tick(Dm2Module* m) {
    generate_noise(m);

    m->vars.counter--;
    if (m->vars.counter < 0)
        m->vars.counter = m->vars.speed;

    for (int i = 0; i < DM2_CHANNELS; i++)
        m->paula[i].dma_off = false;

    // A row is played when the counter is 0; the other ticks only run the effects.
    for (int i = 0; i < DM2_CHANNELS; i++)
        play_voice(m, i, m->vars.counter == 0);

    if (m->wrapped_mask == (1u << DM2_CHANNELS) - 1) {
        m->loop_count++;
        m->wrapped_mask = 0;
    }
}

// Voices

// Paula channels 0 and 3 are left, 1 and 2 right.
static void sync_voice(Dm2Module* m, uint32_t index) {
    static const int8_t zeros[131072];
    Dm2Paula* p = &m->paula[index];

    // DMA is on at the end of every tick: a channel that was switched off by a note, or has not run yet, restarts
    // from its address registers; one that is running takes new registers at the end of its buffer.
    if (p->dma_off || !p->running || p->lc_len_written) {
        const uint32_t bytes = (p->len != 0 ? p->len : 65536u) * 2u;
        const bool inside = p->lc != NO_BUFFER && (uint64_t)p->lc + bytes <= m->image_size;
        const NuadeSample sample = { .data = inside ? (const void*)(m->image + p->lc) : (const void*)zeros,
                                     .format = NuadeSampleFormat_S8,
                                     .length = bytes,
                                     .loop_start = 0,
                                     .loop_length = bytes };
        if (p->dma_off || !p->running)
            nuade_voice_play(m->voices, index, &sample, 0);
        else
            nuade_voice_queue(m->voices, index, &sample);
    }
    p->running = true;
    p->dma_off = false;
    p->lc_len_written = false;

    nuade_voice_set_pitch(m->voices, index, nuade_paula_period_hz(p->per));
    const uint32_t volume = p->vol & 0x7f;
    nuade_voice_set_volume(m->voices, index, (float)(volume > 64 ? 64 : volume) / 64.0f);
    nuade_voice_set_pan(m->voices, index, (index == 0 || index == 3) ? -1.0f : 1.0f);
}

// Three lines per voice and tick (the T, U and V groups of the original's state dump), after the tick.
static void dump_voices(const Dm2Module* m) {
    if (m->dump_state == nullptr)
        return;
    for (int i = 0; i < DM2_CHANNELS; i++) {
        const Dm2Voice* v = &m->voice[i];
        fprintf(m->dump_state, "T %u ch=%d inst=%u note=%u per=%u vper=%u av=%u vpos=%u lc=%u len=%u\n", m->tick_number,
                i, v->instrument - m->vars.inst_base, v->note, v->period, v->vibrato, v->volume, v->volume_pos,
                m->paula[i].lc, m->paula[i].len);
        fprintf(m->dump_state, "U %u ch=%d tr=%u bp=%u trans=%d stp=%u std=%u bend=%u\n", m->tick_number, i,
                v->track_pos, v->block_pos, v->transpose, v->table_pos, v->table_delay, v->pitch_bend);
        fprintf(m->dump_state, "V %u ch=%d vs=%u vd=%u vsus=%u vp=%u dir=%u tper=%u ap=%u port=%u\n", m->tick_number, i,
                v->volume_sustain, v->vibrato_delay, v->vibrato_sustain, v->vibrato_pos, v->vibrato_dir,
                v->target_period, v->arpeggio_pos, v->portamento);
    }
}

bool dm2_tick(Dm2Module* module) {
    const uint32_t loops = module->loop_count;
    play_tick(module);
    dump_voices(module);
    module->tick_number++;

    for (uint32_t i = 0; i < DM2_CHANNELS; i++)
        sync_voice(module, i);
    return module->loop_count != loops;
}

// Public API

bool dm2_probe(const uint8_t* data, size_t size) {
    return data != nullptr && size >= DM2_MIN_SIZE && memcmp(data + DM2_VOICES_PROBE, ".FNL", 4) == 0;
}

Dm2Module* dm2_create(const uint8_t* data, size_t size) {
    if (!dm2_probe(data, size))
        return nullptr;

    Dm2Module* m = (Dm2Module*)calloc(1, sizeof(Dm2Module));
    if (m == nullptr)
        return nullptr;
    m->file_size = size;
    m->image_size = size + DM2_IMAGE_PAD;
    m->file = (uint8_t*)malloc(size);
    m->image = (uint8_t*)malloc(m->image_size);
    if (m->file == nullptr || m->image == nullptr) {
        dm2_destroy(m);
        return nullptr;
    }
    memcpy(m->file, data, size);
    init_sound(m);

    // The parts of the song must lie in the file.
    const Dm2Vars* v = &m->vars;
    if (v->blocks > size || v->inst_offsets > size || v->inst_base > size || v->waveforms > size
        || v->sample_offsets > size || v->sample_base > size) {
        dm2_destroy(m);
        return nullptr;
    }
    return m;
}

void dm2_destroy(Dm2Module* module) {
    if (module == nullptr)
        return;
    if (module->dump_state != nullptr && module->dump_state != stderr)
        fclose(module->dump_state);
    free(module->file);
    free(module->image);
    free(module);
}

bool dm2_set_dump_file(Dm2Module* module, const char* path) {
    if (module->dump_state != nullptr && module->dump_state != stderr)
        fclose(module->dump_state);
    module->dump_state = nullptr;
    if (path[0] == '\0')
        return true;
    module->dump_state = strcmp(path, "-") == 0 ? stderr : fopen(path, "w");
    return module->dump_state != nullptr;
}

int dm2_subsong_count(const Dm2Module* module) {
    return module != nullptr ? 1 : 0;
}

bool dm2_start(Dm2Module* module, int subsong, NuadeVoices* voices) {
    if (module == nullptr || voices == nullptr || subsong != 0)
        return false;

    module->voices = voices;
    init_sound(module);
    return true;
}

// Display

// The semitone from C-0 + DM2_NOTE_OCTAVE_SHIFT octaves of a note index; -1 for none.
static int note_of_index(const Dm2Module* m, uint8_t index) {
    return index == 0 ? -1 : nuade_period_to_note(period_at(m, index) / 4u);
}

int dm2_order_count(const Dm2Module* module) {
    int count = 0;
    for (int i = 0; i < DM2_CHANNELS; i++)
        if (module->voice[i].track_len / 2 > count)
            count = module->voice[i].track_len / 2;
    return count;
}

Dm2VoiceState dm2_voice_state(const Dm2Module* module, int voice) {
    const Dm2Voice* v = &module->voice[voice];
    return (Dm2VoiceState) { .note = v->shown_instrument >= 0 ? note_of_index(module, v->shown_note) : -1,
                             .instrument = v->shown_instrument,
                             .order = v->shown_track_pos / 2,
                             .row = v->shown_row / 4,
                             .speed = module->vars.speed + 1 };
}

Dm2Cell dm2_cell(const Dm2Module* module, int order, int voice, int row) {
    const Dm2Cell empty = { -1, -1, 0, 0 };
    if (voice < 0 || voice >= DM2_CHANNELS || order < 0 || order >= module->voice[voice].track_len / 2 || row < 0
        || row >= DM2_BLOCK_ROWS)
        return empty;
    const uint32_t entry = module->vars.tracks[voice] + 2u * (uint32_t)order;
    const uint32_t line = module->vars.blocks + rd8(module, entry) * DM2_BLOCK_BYTES + 4u * (uint32_t)row;
    Dm2Cell cell = empty;
    if (rd8(module, line) != 0) {
        cell.note = note_of_index(module, (uint8_t)(rd8(module, line) + rd8(module, entry + 1)));
        cell.instrument = rd8(module, line + 1);
    }
    cell.effect = rd8(module, line + 2);
    cell.effect_arg = rd8(module, line + 3);
    return cell;
}
