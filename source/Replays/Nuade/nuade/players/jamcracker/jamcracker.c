// SPDX-License-Identifier: MIT
// JamCracker module player.

#include <nuade/players/jamcracker.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include <stdlib.h>
#include <string.h>

#define MAGIC_LENGTH 4
#define INSTRUMENT_RECORD_SIZE 40
#define PATTERN_RECORD_SIZE 6
#define NOTE_SIZE 8

#define WAVESIZE 0x40
// An AM instrument holds its two source waves in the first 2 * WAVESIZE bytes and one output window per voice after.
#define AM_WAVE_AREA (2 * WAVESIZE + JC_VOICES * WAVESIZE)

#define DEFAULT_WAIT 6
#define PERIOD_MIN 135
#define PERIOD_MAX 1019
#define VOLUME_MAX 0x40

enum InstrumentFlags {
    InstrumentFlag_Loop = 1,
    InstrumentFlag_Am = 2,
};

enum NoteSpeed {
    NoteSpeed_SetVolume = 0x80,
    NoteSpeed_TonePortamento = 0x40,
    NoteSpeed_Wait = 0x0f,
};

enum NoteField {
    Note_Period,
    Note_Instrument,
    Note_Speed,
    Note_Arpeggio,
    Note_Vibrato,
    Note_Phase,
    Note_Volume,
    Note_Porta,
};

static const uint16_t pp_periods[] = {
    1019, 962, 908, 857, 809, 763, 720, 680, 642, 606, 572, 540, 509, 481, 454, 428, 404,
    381,  360, 340, 321, 303, 286, 270, 254, 240, 227, 214, 202, 190, 180, 170, 160, 151,
    143,  135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135, 135,
};
#define PERIOD_COUNT ((int)(sizeof(pp_periods) / sizeof(pp_periods[0])))

// Played by a voice that has nothing to play.
static int8_t pp_nullwave[2];

static uint16_t read_u16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t read_u32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static int16_t period_at(int index) {
    return (int16_t)pp_periods[index < PERIOD_COUNT ? index : PERIOD_COUNT - 1];
}

bool jc_probe(const uint8_t* data, size_t size) {
    // id, instrument count, pattern count, song length
    return size >= MAGIC_LENGTH + 2 + 2 + 2 && memcmp(data, "BeEp", MAGIC_LENGTH) == 0;
}

static void module_free(JcModule* m) {
    if (m->instruments != nullptr) {
        for (uint16_t i = 0; i < m->instrument_count; i++) {
            free(m->instruments[i].address);
        }
    }
    free(m->instruments);
    free(m->patterns);
    free(m->songtable);
    *m = (JcModule) { 0 };
}

// Bounds-checked sequential reader over the file.
typedef struct Cursor {
    const uint8_t* data;
    size_t size;
    size_t pos;
} Cursor;

static const uint8_t* cursor_take(Cursor* c, uint64_t bytes) {
    if (bytes > c->size - c->pos) {
        return nullptr;
    }
    const uint8_t* p = c->data + c->pos;
    c->pos += (size_t)bytes;
    return p;
}

// Fixed-size record tables are read before the data they describe, so each parse step needs the previous one.
static bool module_parse(JcModule* m, const uint8_t* data, size_t size) {
    Cursor c = { data, size, MAGIC_LENGTH };

    const uint8_t* p = cursor_take(&c, 2);
    if (p == nullptr) {
        return false;
    }
    m->instrument_count = read_u16(p);
    const uint8_t* instrument_records = cursor_take(&c, (uint64_t)m->instrument_count * INSTRUMENT_RECORD_SIZE);

    p = cursor_take(&c, 2);
    if (instrument_records == nullptr || p == nullptr) {
        return false;
    }
    m->pattern_count = read_u16(p);
    const uint8_t* pattern_records = cursor_take(&c, (uint64_t)m->pattern_count * PATTERN_RECORD_SIZE);

    p = cursor_take(&c, 2);
    if (pattern_records == nullptr || p == nullptr) {
        return false;
    }
    m->songlen = read_u16(p);
    const uint8_t* song = cursor_take(&c, (uint64_t)m->songlen * 2);
    if (song == nullptr || m->songlen == 0 || m->pattern_count == 0) {
        return false;
    }

    m->songtable = malloc((size_t)m->songlen * sizeof(uint16_t));
    m->patterns = calloc(m->pattern_count, sizeof(JcPattern));
    m->instruments = calloc(m->instrument_count != 0 ? m->instrument_count : 1, sizeof(JcInstrument));
    if (m->songtable == nullptr || m->patterns == nullptr || m->instruments == nullptr) {
        return false;
    }

    for (uint16_t i = 0; i < m->songlen; i++) {
        m->songtable[i] = read_u16(song + i * 2);
        if (m->songtable[i] >= m->pattern_count) {
            return false;
        }
    }

    for (uint16_t i = 0; i < m->pattern_count; i++) {
        JcPattern* pattern = &m->patterns[i];
        pattern->size = read_u16(pattern_records + i * PATTERN_RECORD_SIZE);
        if (pattern->size == 0) {
            return false;
        }
        // The row counter is a byte: a size that is a multiple of 256 plays 256 rows.
        pattern->rows = (uint16_t)(((pattern->size - 1) & 0xff) + 1);
        pattern->rows_data = cursor_take(&c, (uint64_t)pattern->size * JC_VOICES * NOTE_SIZE);
        if (pattern->rows_data == nullptr) {
            return false;
        }
    }

    // A file cut short plays the part of the wave that is there.
    for (uint16_t i = 0; i < m->instrument_count; i++) {
        JcInstrument* instrument = &m->instruments[i];
        const uint8_t* record = instrument_records + i * INSTRUMENT_RECORD_SIZE;
        memcpy(instrument->name, record, JC_NAME_LENGTH);
        instrument->flags = record[JC_NAME_LENGTH];
        const uint32_t declared = read_u32(record + JC_NAME_LENGTH + 1);
        const size_t available = c.size - c.pos;
        instrument->size = declared < available ? declared : (uint32_t)available;

        size_t capacity = instrument->size;
        if ((instrument->flags & InstrumentFlag_Am) != 0 && capacity < AM_WAVE_AREA) {
            capacity = AM_WAVE_AREA;
        }
        instrument->address = calloc(capacity != 0 ? capacity : 1, 1);
        if (instrument->address == nullptr) {
            return false;
        }
        memcpy(instrument->address, cursor_take(&c, instrument->size), instrument->size);
    }
    return true;
}

JcPlayer* jc_create(const uint8_t* data, size_t size) {
    if (data == nullptr || !jc_probe(data, size)) {
        return nullptr;
    }
    JcPlayer* player = calloc(1, sizeof(JcPlayer));
    if (player == nullptr) {
        return nullptr;
    }
    if (!module_parse(&player->module, data, size)) {
        module_free(&player->module);
        free(player);
        return nullptr;
    }
    return player;
}

void jc_destroy(JcPlayer* player) {
    if (player == nullptr) {
        return;
    }
    module_free(&player->module);
    free(player);
}

int jc_instrument_count(const JcPlayer* player) {
    return player->module.instrument_count;
}

const char* jc_instrument_name(const JcPlayer* player, int instrument) {
    if (instrument < 0 || instrument >= player->module.instrument_count) {
        return "";
    }
    return player->module.instruments[instrument].name;
}

// Point the row cursor at the first row of the pattern the song position names.
static void enter_pattern(JcPlayer* p) {
    const JcPattern* pattern = &p->module.patterns[p->module.songtable[p->songpos]];
    p->notecnt = (uint8_t)pattern->rows;
    p->address = pattern->rows_data;
}

static void voice_init(JcVoice* v, int index) {
    *v = (JcVoice) { 0 };
    v->waveoffset = (uint16_t)(2 * WAVESIZE + index * WAVESIZE);
    v->insaddress = pp_nullwave;
    v->inslen = 1;
    v->peraddress = 0;
    v->pers[0] = PERIOD_MAX;
    v->vollevel = VOLUME_MAX;
}

bool jc_start(JcPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    player->songpos = 0;
    enter_pattern(player);
    player->shown_songpos = 0;
    player->shown_row = 0;
    player->shown_rows = (uint16_t)player->notecnt;
    player->wait = DEFAULT_WAIT;
    player->waitcnt = 1;
    for (int i = 0; i < JC_VOICES; i++) {
        voice_init(&player->variables[i], i);
        nuade_voice_stop(voices, (uint32_t)i);
        nuade_voice_set_volume(voices, (uint32_t)i, 0.0f);
        // Paula channels 0 and 3 are left, 1 and 2 right
        nuade_voice_set_pan(voices, (uint32_t)i, (i == 0 || i == 3) ? -1.0f : 1.0f);
    }
    return true;
}

// Rotate the arpeggio ring.
static void pp_rot(JcVoice* v) {
    const int16_t first = v->pers[0];
    v->pers[0] = v->pers[1];
    v->pers[1] = v->pers[2];
    v->pers[2] = first;
}

static void set_all_pers(JcVoice* v, int16_t period) {
    v->pers[0] = period;
    v->pers[1] = period;
    v->pers[2] = period;
}

// Rebuild the voice's output window as the mean of the base wave and the same wave shifted by the phase.
static void pp_am_wave(JcVoice* v) {
    if (v->insaddress == pp_nullwave) {
        return;
    }
    const int8_t* source = v->insaddress - v->waveoffset;
    const int8_t* shifted = source + ((uint16_t)v->phase >> 2);
    for (int i = 0; i < WAVESIZE; i++) {
        v->insaddress[i] = (int8_t)((source[i] + shifted[i]) >> 1);
    }
    v->phase = (int16_t)(v->phase + v->deltaphase);
    if (v->phase >= WAVESIZE << 2) {
        v->phase = (int16_t)(v->phase - (WAVESIZE << 2));
    }
}

// Per-tick update of one voice: period (portamento, vibrato, arpeggio), volume slide, AM wave.
static void pp_uvs(JcVoice* v) {
    // A ring of three zero periods cannot occur (the table has none); stop rather than spin
    for (int guard = 0; v->pers[0] == 0 && guard < 3; guard++) {
        pp_rot(v);
    }
    int period = v->pers[0] + v->por;
    if (v->por < 0) {
        if (period < v->porlevel) {
            period = v->porlevel;
        }
    } else if (v->por > 0) {
        if (period > v->porlevel) {
            period = v->porlevel;
        }
    }
    period += v->vib;
    if (period < PERIOD_MIN) {
        period = PERIOD_MIN;
    } else if (period > PERIOD_MAX) {
        period = PERIOD_MAX;
    }
    v->period = (uint16_t)period;
    pp_rot(v);

    v->por = (int16_t)(v->por + v->deltapor);
    if (v->por < -PERIOD_MAX) {
        v->por = -PERIOD_MAX;
    } else if (v->por > PERIOD_MAX) {
        v->por = PERIOD_MAX;
    }

    if (v->vibcnt != 0) {
        v->vib = (int16_t)(v->vib + v->deltavib);
        v->vibcnt--;
        if (v->vibcnt == 0) {
            v->deltavib = (int16_t)-v->deltavib;
            v->vibcnt = v->vibmax;
        }
    }

    const int shown = v->vol & 0x7f;
    v->volume = (uint16_t)(shown < VOLUME_MAX ? shown : VOLUME_MAX);
    v->vol = (int16_t)(v->vol + v->deltavol);
    if (v->vol < 0) {
        v->vol = 0;
    } else if (v->vol > VOLUME_MAX) {
        v->vol = VOLUME_MAX;
    }

    if ((v->flags & InstrumentFlag_Am) != 0 && v->deltaphase != 0) {
        if (v->deltaphase < 0) {
            v->deltaphase = 0;
        }
        pp_am_wave(v);
    }
}

// Switch the voice to the instrument a note names; a missing instrument plays the null wave.
static void select_instrument(JcPlayer* p, JcVoice* v, uint8_t number) {
    const uint16_t index = (uint16_t)(int16_t)(int8_t)number;
    if (index >= p->module.instrument_count) {
        v->insaddress = pp_nullwave;
        v->inslen = 1;
        v->flags = 0;
        return;
    }
    const JcInstrument* instrument = &p->module.instruments[index];
    if ((instrument->flags & InstrumentFlag_Am) != 0) {
        v->insaddress = instrument->address + v->waveoffset;
        v->inslen = WAVESIZE >> 1;
    } else {
        v->insaddress = instrument->address;
        v->inslen = (uint16_t)(instrument->size >> 1);
    }
    v->flags = instrument->flags;
    v->vol = v->vollevel;
}

static void note_period(JcPlayer* p, JcVoice* v, const uint8_t* note) {
    const int index = note[Note_Period] - 1;
    if ((note[Note_Speed] & NoteSpeed_TonePortamento) != 0) {
        v->porlevel = period_at(index);
        return;
    }
    v->triggered = true;
    v->shown_note = note[Note_Period];
    v->shown_instrument = (int8_t)note[Note_Instrument];
    v->peraddress = index;
    set_all_pers(v, period_at(index));
    v->por = 0;
    select_instrument(p, v, note[Note_Instrument]);
}

static void note_arpeggio(JcVoice* v, uint8_t arpeggio) {
    if (arpeggio == 0) {
        return;
    }
    if (arpeggio == 0xff) {
        set_all_pers(v, period_at(v->peraddress));
        return;
    }
    v->pers[2] = period_at(v->peraddress + (arpeggio & 0x0f));
    v->pers[1] = period_at(v->peraddress + (arpeggio >> 4));
    v->pers[0] = period_at(v->peraddress);
}

static void note_vibrato(JcVoice* v, uint8_t vibrato) {
    if (vibrato == 0) {
        return;
    }
    v->vib = 0;
    if (vibrato == 0xff) {
        v->deltavib = 0;
        v->vibcnt = 0;
        return;
    }
    v->deltavib = vibrato & 0x0f;
    v->vibmax = vibrato >> 4;
    v->vibcnt = v->vibmax >> 1;
}

static void note_phase(JcVoice* v, uint8_t phase) {
    if (phase == 0) {
        return;
    }
    v->phase = 0;
    // A negative delta stops the wave from changing after one more rebuild
    v->deltaphase = phase == 0xff ? -1 : (int16_t)(phase & 0x0f);
}

static void note_volume(JcVoice* v, uint8_t volume, bool set_volume) {
    if (volume == 0xff) {
        v->deltavol = 0;
        return;
    }
    if (volume == 0 && !set_volume) {
        return;
    }
    if (set_volume) {
        v->vol = (int16_t)((v->vol & 0xff00) | volume);
        v->vollevel = (int16_t)((v->vollevel & 0xff00) | volume);
        v->deltavol = 0;
        return;
    }
    v->deltavol = (volume & 0x80) != 0 ? (int16_t)-(volume & 0x7f) : (int16_t)volume;
}

static void note_porta(JcVoice* v, uint8_t porta, bool tone_portamento) {
    if (porta == 0) {
        return;
    }
    if (porta == 0xff) {
        v->por = 0;
        v->deltapor = 0;
        return;
    }
    v->por = 0;
    int8_t delta;
    if (tone_portamento) {
        delta = (int8_t)(v->porlevel > v->pers[0] ? porta : -porta);
    } else if ((porta & 0x80) != 0) {
        v->porlevel = PERIOD_MAX;
        delta = (int8_t)(porta & 0x7f);
    } else {
        v->porlevel = PERIOD_MIN;
        delta = (int8_t)-porta;
    }
    v->deltapor = delta;
}

// Decode one note into the voice state.
static void pp_nnt(JcPlayer* p, JcVoice* v, const uint8_t* note) {
    if (note[Note_Period] != 0) {
        note_period(p, v, note);
    }
    const uint8_t speed = note[Note_Speed];
    if ((speed & NoteSpeed_Wait) != 0) {
        p->wait = speed & NoteSpeed_Wait;
    }
    note_arpeggio(v, note[Note_Arpeggio]);
    note_vibrato(v, note[Note_Vibrato]);
    note_phase(v, note[Note_Phase]);
    note_volume(v, note[Note_Volume], (speed & NoteSpeed_SetVolume) != 0);
    note_porta(v, note[Note_Porta], (speed & NoteSpeed_TonePortamento) != 0);
}

// Restart the voice's DMA on its instrument. A wave that does not loop is followed by the null wave.
static void pp_scr(JcVoice* v, uint32_t index, NuadeVoices* voices) {
    const uint32_t length = (uint32_t)v->inslen * 2;
    if (v->insaddress == pp_nullwave || length == 0) {
        nuade_voice_stop(voices, index);
    } else {
        const bool loops = (v->flags & InstrumentFlag_Loop) != 0;
        const NuadeSample sample = { .data = v->insaddress,
                                     .format = NuadeSampleFormat_S8,
                                     .length = length,
                                     .loop_start = 0,
                                     .loop_length = loops ? length : 0 };
        nuade_voice_play(voices, index, &sample, 0);
    }
    if ((v->flags & InstrumentFlag_Loop) == 0) {
        v->insaddress = pp_nullwave;
        v->inslen = 1;
    }
}

// New row: step to the next row (and pattern), decode the four notes and restart the voices that got a new note.
static void pp_nwnt(JcPlayer* p, NuadeVoices* voices, JcTickResult* result) {
    const uint8_t* row = p->address;
    p->shown_songpos = p->songpos;
    p->shown_rows = p->module.patterns[p->module.songtable[p->songpos]].rows;
    p->shown_row = (uint16_t)(p->shown_rows - p->notecnt);
    p->address += JC_VOICES * NOTE_SIZE;
    p->notecnt--;
    if (p->notecnt == 0) {
        p->songpos++;
        if (p->songpos == p->module.songlen) {
            result->looped = true;
            p->songpos = 0;
        }
        enter_pattern(p);
    }

    for (int i = 0; i < JC_VOICES; i++) {
        p->variables[i].triggered = false;
        pp_nnt(p, &p->variables[i], row + i * NOTE_SIZE);
    }
    for (int i = 0; i < JC_VOICES; i++) {
        if (p->variables[i].triggered) {
            pp_scr(&p->variables[i], (uint32_t)i, voices);
        }
    }
}

JcPosition jc_position(const JcPlayer* player) {
    return (JcPosition) {
        .order = player->shown_songpos, .row = player->shown_row, .rows = player->shown_rows, .speed = player->wait
    };
}

int jc_order_count(const JcPlayer* player) {
    return player->module.songlen;
}

int jc_pattern_rows(const JcPlayer* player, int order) {
    return player->module.patterns[player->module.songtable[order]].rows;
}

JcVoiceState jc_voice_state(const JcPlayer* player, int voice) {
    const JcVoice* v = &player->variables[voice];
    return (JcVoiceState) { .note = v->shown_note, .instrument = v->shown_instrument };
}

JcCell jc_pattern_cell(const JcPlayer* player, int order, int voice, int row) {
    const JcPattern* pattern = &player->module.patterns[player->module.songtable[order]];
    const uint8_t* note = pattern->rows_data + ((size_t)row * JC_VOICES + (size_t)voice) * NOTE_SIZE;
    return (JcCell) { .period = note[Note_Period],
                      .instrument = note[Note_Instrument],
                      .speed = note[Note_Speed],
                      .arpeggio = note[Note_Arpeggio],
                      .vibrato = note[Note_Vibrato],
                      .phase = note[Note_Phase],
                      .volume = note[Note_Volume],
                      .porta = note[Note_Porta] };
}

JcTickResult jc_tick(JcPlayer* player, NuadeVoices* voices) {
    JcTickResult result = { 0 };
    player->waitcnt--;
    if (player->waitcnt == 0) {
        pp_nwnt(player, voices, &result);
        player->waitcnt = player->wait;
    }
    for (int i = 0; i < JC_VOICES; i++) {
        JcVoice* v = &player->variables[i];
        pp_uvs(v);
        nuade_voice_set_pitch(voices, (uint32_t)i, nuade_paula_period_hz(v->period));
        nuade_voice_set_volume(voices, (uint32_t)i, (float)v->volume / 64.0f);
    }
    return result;
}
