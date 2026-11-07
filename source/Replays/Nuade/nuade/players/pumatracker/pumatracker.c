// SPDX-License-Identifier: MIT
// PumaTracker player.

#include <nuade/players/pumatracker.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>
#include <string.h>

#define ROWS_PER_POSITION 0x20
#define DEFAULT_SPEED 2

#define HEADER_SIZE 0x50
#define POSITION_SIZE 14
#define WAVEFORM_COUNT 42
#define WAVEFORM_SIZE 32
#define WAVEFORM_WORDS 0x10
#define SOURCE_COUNT (PUMA_SAMPLE_COUNT + WAVEFORM_COUNT)

// Waveforms past this length (in words) play once, then repeat their first word
#define LONG_SAMPLE_WORDS 0x50

#define DEFAULT_NOTE_VOLUME 0x40

// Volume sequence commands
#define VOL_RAMP 0xA0
#define VOL_GOTO 0xB0
#define VOL_SAMPLE 0xC0

// Frequency sequence commands
#define FREQ_RAMP 0xA0
#define FREQ_GOTO 0xB0
#define FREQ_HOLD 0xD0

// Commands a sequence may run in one tick. Its position is a byte, and only a jump or a sample command goes on without
// holding the tick, so a run longer than every (position, fresh) pair loops for ever: the 68k hangs, the port stops the
// voice.
#define SEQUENCE_STEP_LIMIT 512

// Track entry effects (top three bits of the instrument byte)
#define EFFECT_MASK 0xE0
#define EFFECT_PORTA_DOWN 0x40
#define EFFECT_PORTA_UP 0x60
#define INSTRUMENT_MASK 0x1F

// A position that is not backed by any data; reads from it give zero
#define NO_DATA (INT64_MIN / 2)

#define MAGIC_PATT 0x70617474u
#define MAGIC_INST 0x696E7374u
#define MAGIC_INSF 0x696E7366u

// Tables

// Period for a note: indexed by byte offset, so notes are even numbers
static const uint16_t k_periods[] = {
    0,    6848, 6464, 6096, 5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3624, 3424, 3232, 3048, 2880, 2712, 2560,
    2416, 2280, 2152, 2032, 1920, 1812, 1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960,  906,  856,
    808,  762,  720,  678,  640,  604,  570,  538,  508,  480,  453,  428,  404,  381,  360,  339,  320,  302,  285,
    269,  254,  240,  226,  214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
};

// The 42 built-in single-cycle waveforms
static const uint8_t k_waveforms[WAVEFORM_COUNT * WAVEFORM_SIZE] = {
#include "pumatracker_waves.inc"
};

// PumaModule data access: every read outside the module gives zero

static uint8_t rd8(const PumaModule* mod, int64_t pos) {
    return (pos >= 0 && (uint64_t)pos < mod->size) ? mod->data[pos] : 0;
}

static uint16_t rd16(const PumaModule* mod, int64_t pos) {
    return (uint16_t)((rd8(mod, pos) << 8) | rd8(mod, pos + 1));
}

static uint32_t rd32(const PumaModule* mod, int64_t pos) {
    return ((uint32_t)rd16(mod, pos) << 16) | rd16(mod, pos + 2);
}

// Word at a byte offset into k_periods; the offset need not be even. Beyond the table it reads zero.
static uint16_t note_period(uint8_t byte_offset) {
    uint8_t bytes[2] = { 0, 0 };
    for (int i = 0; i < 2; i++) {
        const size_t at = (size_t)byte_offset + (size_t)i;
        if (at < sizeof(k_periods)) {
            bytes[i] = (uint8_t)((at & 1) ? (k_periods[at >> 1] & 0xFF) : (k_periods[at >> 1] >> 8));
        }
    }
    return (uint16_t)((bytes[0] << 8) | bytes[1]);
}

// Probe

bool puma_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size < HEADER_SIZE || data[12] != 0) {
        return false;
    }
    const PumaModule view = { .data = (uint8_t*)data, .size = size };
    int64_t at = HEADER_SIZE + (int64_t)((data[12] << 8) | data[13]) * POSITION_SIZE + POSITION_SIZE;
    if (rd32(&view, at) != MAGIC_PATT) {
        return false;
    }
    at += 4;
    // The first track's row lengths must add up to a full position
    uint8_t rows_left = ROWS_PER_POSITION;
    for (;;) {
        const uint8_t len = rd8(&view, at + 3);
        if (len == 0 || (size_t)at >= size) {
            return false;
        }
        rows_left = (uint8_t)(rows_left - len);
        if (rows_left == 0) {
            break;
        }
        if (rows_left & 0x80) {
            return false;
        }
        at += 4;
    }
    return rd32(&view, at + 4) == MAGIC_PATT;
}

// PumaModule loading

// Positions of the track starts: each track follows a 'patt' marker.
static void find_tracks(PumaModule* mod, int64_t* scan) {
    int64_t at = HEADER_SIZE + (int64_t)mod->positions * POSITION_SIZE;
    const uint32_t wanted = (uint32_t)rd16(mod, 14) + 1;
    for (uint32_t t = 0; t < wanted && t < PUMA_MAX_TRACKS; t++) {
        while (rd32(mod, at) != MAGIC_PATT) {
            at += 4;
            if ((uint64_t)at >= mod->size) {
                *scan = at;
                return;
            }
        }
        at += 4;
        mod->tracks[mod->track_count++] = at;
        if (t + 1 == wanted) {
            break;
        }
        // Skip the entries up to the next marker
        do {
            at += 4;
            if ((uint64_t)at >= mod->size) {
                *scan = at;
                return;
            }
        } while (rd32(mod, at) != MAGIC_PATT);
    }
    *scan = at;
}

// The 'inst' and 'insf' sections that follow the tracks hold each instrument's two sequences.
static void find_instruments(PumaModule* mod, int64_t at) {
    const uint32_t wanted = rd16(mod, 0x10);
    at -= 2;
    for (uint32_t i = 0; i < wanted && i < PUMA_MAX_INSTRUMENTS; i++) {
        do {
            at += 2;
            if ((uint64_t)at >= mod->size) {
                return;
            }
        } while (rd32(mod, at) != MAGIC_INST);
        at += 4;
        const int64_t volume_seq = at;
        at -= 2;
        do {
            at += 2;
            if ((uint64_t)at >= mod->size) {
                return;
            }
        } while (rd32(mod, at) != MAGIC_INSF);
        at += 4;
        mod->instruments[i] = (PumaInstrument) { .volume_seq = volume_seq, .frequency_seq = at };
        mod->instrument_count = i + 1;
    }
}

// A module whose samples reach past its end is refused, as the original does.
static bool samples_fit(const PumaModule* mod) {
    for (int i = 0; i < PUMA_SAMPLE_COUNT; i++) {
        if (mod->sample_offset[i] == 0) {
            continue;
        }
        uint32_t words = mod->sample_length[i];
        words -= words & 1;
        if ((uint64_t)mod->sample_offset[i] + (uint64_t)words * 2 > mod->size) {
            return false;
        }
    }
    return true;
}

PumaPlayer* puma_create(const uint8_t* data, size_t size) {
    if (data == nullptr || !puma_probe(data, size)) {
        return nullptr;
    }
    PumaPlayer* player = (PumaPlayer*)calloc(1, sizeof(PumaPlayer));
    if (player == nullptr) {
        return nullptr;
    }
    PumaModule* mod = &player->mod;
    mod->data = nuade_paula_padded_copy(data, size);
    if (mod->data == nullptr) {
        free(player);
        return nullptr;
    }
    mod->size = size;

    mod->positions = (uint16_t)(rd16(mod, 12) + 1);
    for (int i = 0; i < PUMA_SAMPLE_COUNT; i++) {
        mod->sample_offset[i] = rd32(mod, 0x14 + 4 * i);
        mod->sample_length[i] = rd16(mod, 0x3C + 2 * i);
    }

    if (!samples_fit(mod)) {
        puma_destroy(player);
        return nullptr;
    }

    int64_t scan = 0;
    find_tracks(mod, &scan);
    find_instruments(mod, scan);
    return player;
}

void puma_destroy(PumaPlayer* player) {
    if (player == nullptr) {
        return;
    }
    free(player->mod.data);
    free(player);
}

const char* puma_title(const PumaPlayer* player) {
    return (const char*)player->mod.data;
}

// Sequences

// Select a sample (0..9) or a built-in waveform (10..51) for the voice
static void select_source(const PumaModule* mod, PumaVoice* v, uint8_t index) {
    if (index < PUMA_SAMPLE_COUNT) {
        v->length = mod->sample_length[index];
        v->sample = (PumaSampleRef) { PumaSourceRegion_Module, mod->sample_offset[index] };
    } else if (index < SOURCE_COUNT) {
        v->length = WAVEFORM_WORDS;
        v->sample
            = (PumaSampleRef) { PumaSourceRegion_Waveforms, (int64_t)(index - PUMA_SAMPLE_COUNT) * WAVEFORM_SIZE };
    } else {
        v->length = 0;
        v->sample = (PumaSampleRef) { 0 };
    }
    if ((int16_t)(v->length - LONG_SAMPLE_WORDS) >= 0) {
        v->set_length_one = true;
    }
}

static void start_volume_ramp(const PumaModule* mod, PumaVoice* v, int64_t cmd) {
    const uint8_t from = rd8(mod, cmd + 1);
    const uint8_t to = rd8(mod, cmd + 2);
    v->volume_ticks = (uint8_t)(rd8(mod, cmd + 3) + 1);
    v->volume = from;
    v->volume_accum = 0;
    v->volume_dir = 1;
    v->volume_delta = (uint8_t)(to - from);
    if (to < from) {
        v->volume_delta = (uint8_t)-v->volume_delta;
        v->volume_dir = 0xFF;
    }
}

// One tick of a running ramp: a Bresenham step of `delta` per `duration`
static void step_volume_ramp(const PumaModule* mod, PumaVoice* v, int64_t cmd) {
    const uint8_t duration = rd8(mod, cmd + 3);
    uint8_t error = (uint8_t)(v->volume_accum + v->volume_delta);
    error = (uint8_t)(error - duration);
    if (!(error & 0x80)) {
        uint8_t change = 0;
        do {
            change = (uint8_t)(change + v->volume_dir);
            error = (uint8_t)(error - duration);
        } while (!(error & 0x80));
        v->volume = (uint8_t)(v->volume + change);
    }
    v->volume_accum = (uint8_t)(error + duration);
}

// Runs the volume sequence up to the command that holds this tick
static void run_volume_sequence(const PumaModule* mod, PumaVoice* v) {
    uint8_t pos = v->volume_pos;
    for (int steps = 0;; steps++) {
        if (steps == SEQUENCE_STEP_LIMIT) {
            v->volume = 0;
            v->playing = false;
            break;
        }
        const int64_t cmd = v->volume_seq == NO_DATA ? NO_DATA : v->volume_seq + pos;
        const uint8_t op = rd8(mod, cmd);
        if (op == VOL_RAMP) {
            if (v->volume_fresh) {
                v->volume_fresh = false;
                start_volume_ramp(mod, v, cmd);
                break;
            }
            if (--v->volume_ticks == 0) {
                pos = (uint8_t)(pos + 4);
                v->volume_fresh = true;
                continue;
            }
            step_volume_ramp(mod, v, cmd);
            break;
        }
        if (op == VOL_SAMPLE) {
            select_source(mod, v, rd8(mod, cmd + 1));
            v->wave_step = rd8(mod, cmd + 2);
            v->wave_cur = rd8(mod, cmd + 2);
            v->wave_count = rd8(mod, cmd + 3);
            pos = (uint8_t)(pos + 4);
            v->volume_fresh = true;
            continue;
        }
        if (op == VOL_GOTO) {
            pos = rd8(mod, cmd + 1);
            continue;
        }
        v->volume = 0;
        v->playing = false;
        break;
    }
    v->volume_pos = pos;
}

// Start of a pitch ramp: period moves from note + `from` to note + `to` over `duration` ticks
static void start_frequency_ramp(const PumaModule* mod, PumaVoice* v, int64_t cmd) {
    const int16_t from = (int8_t)rd8(mod, cmd + 1);
    const int16_t to = (int8_t)rd8(mod, cmd + 2);
    v->frequency_ticks = rd8(mod, cmd + 3);
    const uint16_t base = note_period(v->note);
    v->period = (uint16_t)(base + from);
    v->frequency_acc = (uint32_t)v->period << 16;

    const int32_t distance = to - from;
    const int32_t duration = v->frequency_ticks;
    if (duration == 0) {
        v->frequency_step = 0;
        return;
    }
    // The 68k divides distance * 256 as a 16 bit quotient; a quotient that does not fit divides the distance itself
    const int32_t fine = (distance * 256) / duration;
    if (fine >= INT16_MIN && fine <= INT16_MAX) {
        v->frequency_step = fine * 256;
    } else {
        v->frequency_step = (int32_t)((uint32_t)(int16_t)(distance / duration) << 16);
    }
}

// Runs the frequency sequence up to the command that holds this tick
static void run_frequency_sequence(const PumaModule* mod, PumaVoice* v) {
    uint8_t pos = v->frequency_pos;
    for (int steps = 0;; steps++) {
        if (steps == SEQUENCE_STEP_LIMIT) {
            v->volume = 0;
            v->playing = false;
            break;
        }
        const int64_t cmd = v->frequency_seq == NO_DATA ? NO_DATA : v->frequency_seq + pos;
        const uint8_t op = rd8(mod, cmd);
        if (op == FREQ_RAMP) {
            if (v->frequency_fresh) {
                v->frequency_fresh = false;
                start_frequency_ramp(mod, v, cmd);
                break;
            }
            if (--v->frequency_ticks == 0) {
                v->frequency_fresh = true;
                pos = (uint8_t)(pos + 4);
                continue;
            }
            v->frequency_acc += (uint32_t)v->frequency_step;
            v->period = (uint16_t)(v->frequency_acc >> 16);
            break;
        }
        if (op == FREQ_HOLD) {
            if (v->frequency_fresh) {
                v->frequency_fresh = false;
                v->frequency_ticks = rd8(mod, cmd + 3);
                v->period = note_period((uint8_t)(rd8(mod, cmd + 1) + v->note));
                break;
            }
            if (--v->frequency_ticks != 0) {
                break;
            }
            v->frequency_fresh = true;
            pos = (uint8_t)(pos + 4);
            continue;
        }
        if (op == FREQ_GOTO) {
            pos = rd8(mod, cmd + 1);
            continue;
        }
        break;
    }
    v->frequency_pos = pos;
}

// AUDxLC/AUDxLEN for a sample or waveform. The length goes out as the replayer has it (0 plays 65536 words); a sample
// that runs past the module plays the zeros of the padded copy. An AUDxLC outside the module or the waveform table
// (an out-of-range offset, or a waveform step past either end) points at memory the port does not have: none.
static void latch_source(const PumaModule* mod, NuadePaulaChannel* c, PumaSampleRef ref, uint16_t len_words) {
    const uint8_t* base = nullptr;
    size_t size = 0;
    size_t readable = 0;
    if (ref.region == PumaSourceRegion_Module) {
        base = mod->data;
        size = mod->size;
        readable = size + NUADE_PAULA_MAX_SAMPLE_BYTES;
    } else if (ref.region == PumaSourceRegion_Waveforms) {
        // Waveforms are 32-byte aligned and always WAVEFORM_WORDS long, so they never reach past the table
        base = k_waveforms;
        size = sizeof(k_waveforms);
        readable = size;
    }
    c->lc = nullptr;
    c->len_words = len_words;
    if (base == nullptr || ref.offset < 0 || (uint64_t)ref.offset >= size) {
        return;
    }
    const size_t bytes = len_words == 0 ? NUADE_PAULA_MAX_SAMPLE_BYTES : (size_t)len_words * 2;
    if (bytes <= readable - (size_t)ref.offset) {
        c->lc = (const int8_t*)(base + ref.offset);
    }
}

// Write the voice's registers: sample (stepped through the waveforms when animated), period and volume
static void write_registers(const PumaModule* mod, PumaVoice* v) {
    v->period = (uint16_t)(v->period + v->porta);

    PumaSampleRef lc = v->sample;
    if (v->wave_step != 0) {
        uint8_t step = v->wave_step;
        const bool turn = (int8_t)v->wave_cur <= 0 || !((uint8_t)(v->wave_cur - v->wave_count) & 0x80);
        if (turn) {
            step = (uint8_t)-step;
            v->wave_step = step;
        }
        v->wave_cur = (uint8_t)(v->wave_cur + step);
        lc.offset += (int16_t)((int8_t)v->wave_cur * WAVEFORM_SIZE);
    }

    NuadePaulaChannel* c = &v->paula;
    latch_source(mod, c, lc, v->length);
    c->period = v->period; // written as is (the eagleplayer has no clamp): 0 and periods below 113 too
    if (v->set_length_one) {
        v->set_length_one = false;
        v->length = 1;
    }

    const uint8_t level = (uint8_t)(v->volume + v->note_volume - DEFAULT_NOTE_VOLUME);
    c->volume = (level & 0x80) ? 0 : level; // the replayer clears a negative level; the channel applies Paula's rule
}

// Player

// The entry the voice's track points at: note, instrument and effect, and how many rows it lasts
static void read_track_entry(const PumaModule* mod, PumaVoice* v) {
    const int64_t at = v->track_pos;
    const uint8_t note = rd8(mod, at);
    const uint8_t instrument_effect = rd8(mod, at + 1);
    const uint8_t arg = rd8(mod, at + 2);

    if (note != 0) {
        nuade_paula_channel_dma_off(&v->paula);
        v->note = (uint8_t)(note + v->note_transpose);
        const uint8_t index = (uint8_t)(instrument_effect + v->instrument_transpose) & INSTRUMENT_MASK;
        v->shown_note = v->note;
        v->shown_instrument = index;
        if (index != 0 && index <= mod->instrument_count) {
            v->volume_seq = mod->instruments[index - 1].volume_seq;
            v->frequency_seq = mod->instruments[index - 1].frequency_seq;
        } else {
            v->volume_seq = NO_DATA;
            v->frequency_seq = NO_DATA;
        }
        v->playing = true;
        v->volume_fresh = true;
        v->frequency_fresh = true;
        v->set_length_one = false;
        v->volume_pos = 0;
        v->frequency_pos = 0;
    }
    v->rows_left = rd8(mod, at + 3);

    const uint8_t effect = instrument_effect & EFFECT_MASK;
    v->note_volume = DEFAULT_NOTE_VOLUME;
    v->porta = 0;
    if (effect == EFFECT_PORTA_UP) {
        v->porta = (uint16_t)-(int)arg;
    } else if (effect == EFFECT_PORTA_DOWN) {
        v->porta = arg;
    } else if (effect != 0) {
        v->note_volume = arg;
    }
}

// Load the four tracks, transposes and tempo of the song position
static void load_position(const PumaModule* mod, PumaSong* s) {
    const int64_t record = HEADER_SIZE + (int64_t)s->position * POSITION_SIZE;
    for (int i = 0; i < PUMA_VOICES; i++) {
        PumaVoice* v = &s->voices[i];
        const uint8_t track = rd8(mod, record + 3 * i);
        v->track_pos = track < mod->track_count ? mod->tracks[track] - 4 : NO_DATA;
        v->instrument_transpose = rd8(mod, record + 3 * i + 1);
        v->note_transpose = rd8(mod, record + 3 * i + 2);
        v->rows_left = 1;
    }
    const uint8_t speed = rd8(mod, record + 12);
    if (speed != 0) {
        s->speed = speed;
    }
}

static void play(const PumaModule* mod, PumaSong* s) {
    if (s->row == ROWS_PER_POSITION) {
        s->position++;
        if ((uint8_t)mod->positions == s->position) {
            s->position = 0;
            for (int i = 0; i < PUMA_VOICES; i++) {
                s->voices[i].playing = false;
            }
            s->looped = true;
        }
        s->row = 0;
        load_position(mod, s);
    }

    if (--s->tick_counter == 0) {
        s->row++;
        s->shown_position = s->position;
        s->shown_row = (uint8_t)(s->row - 1);
        s->tick_counter = s->speed;
        for (int i = 0; i < PUMA_VOICES; i++) {
            PumaVoice* v = &s->voices[i];
            if (--v->rows_left == 0) {
                v->track_pos = v->track_pos == NO_DATA ? NO_DATA : v->track_pos + 4;
                read_track_entry(mod, v);
            }
        }
    }

    bool active[PUMA_VOICES];
    for (int i = 0; i < PUMA_VOICES; i++) {
        PumaVoice* v = &s->voices[i];
        active[i] = v->playing;
        if (active[i]) {
            run_volume_sequence(mod, v);
            run_frequency_sequence(mod, v);
            write_registers(mod, v);
        }
    }
    // Voices that play get DMA on, the others off
    for (int i = 0; i < PUMA_VOICES; i++) {
        if (active[i]) {
            nuade_paula_channel_dma_on(&s->voices[i].paula);
        } else {
            nuade_paula_channel_dma_off(&s->voices[i].paula);
        }
    }
}

bool puma_start(PumaPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    PumaSong* s = &player->song;
    memset(s, 0, sizeof(*s));
    s->tick_counter = 1;
    s->row = ROWS_PER_POSITION;
    s->position = 0xFF;
    s->speed = DEFAULT_SPEED;
    for (int i = 0; i < PUMA_VOICES; i++) {
        s->voices[i].rows_left = 1;
        s->voices[i].shown_note = -1;
        s->voices[i].track_pos = NO_DATA;
        s->voices[i].volume_seq = NO_DATA;
        s->voices[i].frequency_seq = NO_DATA;
        nuade_voice_stop(voices, (uint32_t)i);
        nuade_voice_set_volume(voices, (uint32_t)i, 0.0f);
        // Paula channels 0 and 3 are left, 1 and 2 right
        nuade_voice_set_pan(voices, (uint32_t)i, (i == 0 || i == 3) ? -1.0f : 1.0f);
    }
    return true;
}

PumaTickResult puma_tick(PumaPlayer* player, NuadeVoices* voices) {
    PumaSong* s = &player->song;
    s->looped = false;
    play(&player->mod, s);
    for (int i = 0; i < PUMA_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, (uint32_t)i, voices);
    }
    return (PumaTickResult) { .looped = s->looped };
}

PumaPosition puma_position(const PumaPlayer* player) {
    const PumaSong* s = &player->song;
    return (PumaPosition) { .order = s->shown_position, .row = s->shown_row, .speed = s->speed };
}

int puma_position_count(const PumaPlayer* player) {
    return player->mod.positions;
}

PumaVoiceState puma_voice_state(const PumaPlayer* player, int voice) {
    const PumaVoice* v = &player->song.voices[voice];
    const bool has_instrument
        = v->shown_instrument != 0 && (uint32_t)v->shown_instrument <= player->mod.instrument_count;
    return (PumaVoiceState) { .note = v->shown_note, .instrument = has_instrument ? v->shown_instrument - 1 : -1 };
}

PumaCell puma_pattern_cell(const PumaPlayer* player, int position, int voice, int row) {
    const PumaModule* mod = &player->mod;
    const uint8_t track = rd8(mod, HEADER_SIZE + (int64_t)position * POSITION_SIZE + 3 * voice);
    PumaCell cell = { 0 };
    if (track >= mod->track_count) {
        return cell;
    }
    // Each entry covers `duration` rows from its own row on (0 counts as 256, the row counter is a byte).
    int64_t at = mod->tracks[track];
    int first_row = 0;
    while (first_row < ROWS_PER_POSITION) {
        const int duration = rd8(mod, at + 3) != 0 ? rd8(mod, at + 3) : 256;
        if (row < first_row + duration) {
            if (row == first_row) {
                cell = (PumaCell) { .note = rd8(mod, at),
                                    .instrument = rd8(mod, at + 1) & INSTRUMENT_MASK,
                                    .effect = rd8(mod, at + 1) >> 5,
                                    .effect_arg = rd8(mod, at + 2) };
            }
            return cell;
        }
        first_row += duration;
        at += 4;
    }
    return cell;
}
