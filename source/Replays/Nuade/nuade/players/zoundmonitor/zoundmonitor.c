// SPDX-License-Identifier: MIT
// ZoundMonitor player, translated from the UADE eagleplayer "ZoundMonitor player module V1.0" (Wanted Team).
//
// ZmModule: a 5-byte header, 16 instruments, the position list (4 bytes per voice per position) and the patterns
// (32 rows of one 4-byte cell). Samples are external files named after the instruments.

#include <nuade/players/zoundmonitor.h>

#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_paula_channel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE 5
#define INSTRUMENT_SIZE 0x36
#define INSTRUMENT_BLOCK (ZM_INSTRUMENTS * INSTRUMENT_SIZE)
#define POSITION_SIZE 16
#define PATTERN_SIZE (ZM_ROWS * 4)

#define NOTE_REST 0x3F
#define VOICE_MASK 0x000F

// An IFF 8SVX sample: the player takes the BODY length from a fixed offset and the data right after it.
#define IFF_LENGTH_OFFSET 0x64
#define IFF_DATA_OFFSET 0x68

// The player's data block, as offsets of its variables: a period index past the table reads them (see state_word).
#define PERIOD_COUNT 37
#define STATE_VOICES 0x5A
#define STATE_VOICE_SIZE 0x28
#define STATE_END 0xFC

static const uint16_t k_periods[PERIOD_COUNT] = {
    0,   856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320,
    302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
};

// One position-list entry of a voice.
typedef struct Track {
    uint8_t pattern;
    int8_t volume;
    uint8_t instrument_transpose;
    uint8_t note_transpose;
} Track;

static uint32_t read_be32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

// The module as the replayer sees it: InitPlayer moves the instruments, positions and patterns over the header, in
// place, and reads everything from there. Bytes past the moved block keep their file contents.
static uint8_t image_byte(const ZmModule* m, uint32_t offset) {
    const uint32_t file_offset = offset < m->moved_size ? offset + HEADER_SIZE : offset;
    return file_offset < m->size ? m->data[file_offset] : 0;
}

static uint32_t image_long(const ZmModule* m, uint32_t offset) {
    return (uint32_t)image_byte(m, offset) << 24 | (uint32_t)image_byte(m, offset + 1) << 16
           | (uint32_t)image_byte(m, offset + 2) << 8 | image_byte(m, offset + 3);
}

static uint16_t image_word(const ZmModule* m, uint32_t offset) {
    return (uint16_t)(image_byte(m, offset) << 8 | image_byte(m, offset + 1));
}

static ZmInstrument parse_instrument(const ZmModule* m, uint32_t index) {
    const uint32_t base = index * INSTRUMENT_SIZE;
    ZmInstrument ins = { 0 };
    for (uint32_t i = 0; i < ZM_NAME_SIZE; i++) {
        ins.name[i] = (char)image_byte(m, base + 4 + i);
    }
    ins.volume = (int8_t)image_byte(m, base + 0x2C);
    ins.length = image_word(m, base + 0x2E);
    ins.loop_length = image_word(m, base + 0x30);
    ins.loop_start = image_word(m, base + 0x32);
    return ins;
}

// 1-based, as the pattern names it. Past the 16th, the replayer reads the position list as instrument records.
static ZmInstrument instrument_at(const ZmModule* m, uint32_t number) {
    return number <= ZM_INSTRUMENTS ? m->instruments[number - 1] : parse_instrument(m, number - 1);
}

static Track track_at(const ZmModule* m, int position, int voice) {
    const uint32_t base = INSTRUMENT_BLOCK + (uint32_t)position * POSITION_SIZE + (uint32_t)voice * 4;
    return (Track) { .pattern = image_byte(m, base),
                     .volume = (int8_t)image_byte(m, base + 1),
                     .instrument_transpose = image_byte(m, base + 2),
                     .note_transpose = image_byte(m, base + 3) };
}

// The cell `row` of `pattern`; row 32 is the first row of the next pattern (the portamento target of row 31).
static uint32_t cell_at(const ZmModule* m, uint8_t pattern, int row) {
    return image_long(m, m->patterns_offset + (uint32_t)pattern * PATTERN_SIZE + (uint32_t)row * 4);
}

// A voice's variables as the words the replayer keeps them in.
static uint16_t voice_word(const ZmVoice* v, uint32_t offset) {
    switch (offset) {
        case 0x00:
            return (uint16_t)v->volume;
        case 0x02:
            return (uint16_t)v->slide;
        case 0x04:
            return v->period;
        case 0x06:
            return v->dma_bit;
        case 0x08:
            return v->length;
        case 0x0A:
            return v->loop_length;
        case 0x14:
            return (uint16_t)(v->note << 8 | v->control);
        case 0x16:
            return (uint16_t)(v->arpeggio[0] << 8 | v->arpeggio[1]);
        case 0x18:
            return (uint16_t)(v->arpeggio[2] << 8);
        case 0x1A:
            return (uint16_t)v->instrument_volume;
        case 0x1C:
            return v->porta_pending ? 1 : 0;
        case 0x1E:
            return v->counter;
        case 0x20:
            return (uint16_t)v->porta_delta;
        case 0x22:
            return v->porta_ticks;
        case 0x24:
            return (uint16_t)(v->legato_instrument << 8 | v->instrument);
        default:
            return 0; // sample and loop pointers (Amiga addresses) and padding
    }
}

// The period table is followed by the player's variables, and a note past the table (note plus transpose or
// arpeggio, up to 255) reads them as a period. Songs do this: a rest on a transposed track is no longer a rest and
// plays at the period it finds here. Offset is the byte offset from the start of the table.
static uint16_t state_word(const ZmPlayer* p, uint32_t offset) {
    const ZmModule* m = &p->module;
    const ZmSong* s = &p->song;
    switch (offset) {
        case 0x4A:
            return (uint16_t)(0x02 << 8 | m->speed);
        case 0x4C:
            return VOICE_MASK;
        case 0x4E:
            return (uint16_t)(m->pattern_count - 1);
        case 0x50:
            return (uint16_t)((m->position_count - 1) << 8 | s->tick);
        case 0x52:
            return (uint16_t)(s->row << 8 | s->position);
        case 0x54:
            return (uint16_t)(s->loop_position << 8 | s->end_position);
        case 0x56:
            return (uint16_t)(m->start_position << 8 | m->end_position);
        case 0x58:
            return s->dma_mask;
        case 0xFA:
            return 0xF400; // the last word of the hunk
        default:
            break;
    }
    if (offset >= STATE_VOICES && offset < STATE_END - 2) {
        const uint32_t voice = (offset - STATE_VOICES) / STATE_VOICE_SIZE;
        return voice_word(&s->voices[voice], (offset - STATE_VOICES) % STATE_VOICE_SIZE);
    }
    return 0; // memory past the player
}

static uint16_t period_at(const ZmPlayer* p, uint8_t index) {
    return index < PERIOD_COUNT ? k_periods[index] : state_word(p, (uint32_t)index * 2);
}

// Per tick: arpeggio, portamento or slide, then the loop registers.
static void run_effects(const ZmPlayer* p, ZmVoice* v) {
    if (v->control & ZmControl_Arpeggio) {
        if (!(v->control & ZmControl_Slide)) {
            v->counter++;
            if (!(v->counter & 1)) {
                const uint8_t offset = v->arpeggio[(v->counter >> 1) % 3];
                v->paula.period = period_at(p, (uint8_t)(offset + v->note));
            }
        } else if (v->counter != v->porta_ticks) {
            v->counter++;
            const int32_t step = (int32_t)v->porta_delta * v->counter / (int16_t)v->porta_ticks;
            v->paula.period = (uint16_t)(step + v->period);
        }
    } else if (v->control & ZmControl_Slide) {
        v->period = (uint16_t)(v->period + v->slide);
        v->paula.period = v->period;
    }
    v->paula.lc = v->loop;
    v->paula.len_words = v->loop_length;
}

static void set_volume(ZmVoice* v, uint32_t cell, const Track* track) {
    const int16_t volume = (int16_t)(v->instrument_volume + (int8_t)(cell >> 8) + track->volume);
    v->volume = (uint16_t)volume < 0x40 ? volume : (volume < 0 ? 0 : 0x40);
}

static void play_row_voice(ZmPlayer* p, ZmVoice* v, const Track* track) {
    const ZmModule* m = &p->module;
    ZmSong* s = &p->song;

    if (v->porta_pending) {
        v->porta_pending = false;
        run_effects(p, v);
        return;
    }
    const uint32_t cell = cell_at(m, track->pattern, s->row);
    const uint32_t next = cell_at(m, track->pattern, s->row + 1);
    const uint8_t note = (uint8_t)((cell >> 24) & 0x3F);
    if (note == 0) {
        run_effects(p, v);
        return;
    }
    v->note = note;
    v->control = (uint8_t)((cell >> 16) & 0x0F);
    if (!(v->control & ZmControl_NoNoteTranspose)) {
        v->note = (uint8_t)(v->note + track->note_transpose);
    }
    v->period = period_at(p, v->note);

    if (v->control & ZmControl_Arpeggio) {
        if (!(v->control & ZmControl_Slide)) {
            v->arpeggio[1] = (uint8_t)((cell & 0xFF) >> 4);
            v->arpeggio[2] = (uint8_t)(cell & 0x0F);
            v->counter = 0;
        } else {
            // Portamento to the next row's note over `param` rows; that row is not played as a note
            v->porta_pending = true;
            uint8_t target = (uint8_t)(next >> 24);
            if (!(next & (ZmControl_NoNoteTranspose << 16))) {
                target = (uint8_t)(target + track->note_transpose);
            }
            v->porta_delta = (int16_t)(period_at(p, target & 0x3F) - v->period);
            v->counter = 0;
            v->porta_ticks = (uint16_t)((cell & 0xFF) * m->speed);
        }
    } else if (v->control & ZmControl_Slide) {
        v->slide = (int8_t)(cell & 0xFF);
    }

    uint8_t instrument = (uint8_t)((cell >> 20) & 0x0F);
    if (instrument != 0 && !(v->control & ZmControl_NoInstrumentTranspose)) {
        instrument = (uint8_t)(instrument + track->instrument_transpose);
    }
    if (instrument != 0 && instrument != v->instrument) {
        v->instrument = instrument;
        const ZmInstrument ins = instrument_at(m, instrument);
        v->instrument_volume = ins.volume;
        set_volume(v, cell, track);
        v->sample = ins.sample;
        v->length = ins.length;
        v->loop_length = ins.loop_length;
        v->loop = ins.sample != nullptr ? ins.sample + (uint32_t)ins.loop_start * 2 : nullptr;
    } else {
        set_volume(v, cell, track);
    }

    // Legato: the same instrument again only changes period and volume
    const bool legato = (cell & 0x80000000u) && v->instrument == v->legato_instrument;
    if (cell & 0x80000000u) {
        v->legato_instrument = v->instrument;
    }
    if (!legato) {
        nuade_paula_channel_dma_off(&v->paula);
        if (v->note == NOTE_REST) {
            return;
        }
        s->dma_mask |= v->dma_bit;
        v->paula.lc = v->sample;
        v->paula.len_words = v->length;
    }
    v->paula.period = v->period;
    v->paula.volume = (uint8_t)(v->volume & 0x7F);
}

static void play_row(ZmPlayer* p) {
    const ZmModule* m = &p->module;
    ZmSong* s = &p->song;

    s->tick = 0;
    s->shown_position = s->position;
    s->shown_row = s->row;
    for (int i = 0; i < ZM_VOICES; i++) {
        const Track track = track_at(m, s->position, i);
        play_row_voice(p, &s->voices[i], &track);
    }

    s->row++;
    if (s->row == ZM_ROWS) {
        s->row = 0;
        s->position++;
        if (s->position == s->end_position) {
            s->position = s->loop_position;
            s->looped = true;
        }
    }
}

static void play_tick(ZmPlayer* p) {
    ZmSong* s = &p->song;
    s->tick++;
    if (s->tick == p->module.speed) {
        play_row(p);
        return;
    }
    if (s->dma_mask != 0) {
        // DMA comes on first; the loop registers run_effects writes after the raster wait are the repeat
        for (int i = 0; i < ZM_VOICES; i++) {
            if (s->dma_mask & s->voices[i].dma_bit) {
                nuade_paula_channel_dma_on(&s->voices[i].paula);
            }
        }
        s->dma_mask = 0;
    }
    for (int i = 0; i < ZM_VOICES; i++) {
        run_effects(p, &s->voices[i]);
    }
}

bool zm_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size < HEADER_SIZE) {
        return false;
    }
    const size_t path
        = HEADER_SIZE + INSTRUMENT_BLOCK + (size_t)(data[0] + 1) * POSITION_SIZE + (size_t)(data[1] + 1) * PATTERN_SIZE;
    if (path + 4 > size) {
        return false;
    }
    const uint8_t* s = data + path;
    if (s[0] == 'd') {
        return s[1] == 'f' && s[3] == ':';
    }
    return s[1] == 'a' && s[2] == 'm' && s[3] == 'p';
}

// Load an instrument's sample as InitPlayer does: an IFF file gives its BODY length (replacing the instrument's),
// a raw one is used whole. The first word is cleared: it is the silent loop of one-shot samples. The buffer reaches the
// loop start even when that lies past the file, then has the Paula tail: AUDxLC at the sample or its loop can play any
// AUDxLEN, 0 (65536 words) too, and what runs past the file plays zeros (the Amiga reads whatever follows).
static int8_t* load_sample(const NuadeIo* io, const char* url, ZmInstrument* ins) {
    const NuadeIoReadResult file = NuadeIo_read_url_to_memory(io, url);
    if (file.data == nullptr) {
        return nullptr;
    }
    const uint8_t* data = file.data;
    size_t available = file.size;
    if (file.size >= IFF_DATA_OFFSET && memcmp(file.data, "FORM", 4) == 0) {
        ins->length = (uint16_t)(read_be32(file.data + IFF_LENGTH_OFFSET) >> 1);
        data = file.data + IFF_DATA_OFFSET;
        available = file.size - IFF_DATA_OFFSET;
    }
    const size_t loop_start = (size_t)ins->loop_start * 2;
    int8_t* sample = (int8_t*)nuade_paula_padded_copy(nullptr, available > loop_start ? available : loop_start);
    if (sample != nullptr) {
        memcpy(sample, data, available);
        sample[0] = 0;
        sample[1] = 0;
    }
    NuadeIo_free_url_to_memory(io, file.data);
    return sample;
}

static ZmStatus load_samples(ZmModule* m, const char* path, const NuadeIo* io, char* missing, size_t missing_cap) {
    char dir[1024] = { 0 };
    char samples_dir[1024] = { 0 };
    NuadeFileIndex* index = nullptr;
    if (path != nullptr && nuade_path_parent(path, dir, sizeof(dir))
        && nuade_path_join(dir, "Samples", samples_dir, sizeof(samples_dir))) {
        const char* dirs[] = { samples_dir };
        index = nuade_file_index_create(io, dirs, 1);
    }

    ZmStatus status = ZmStatus_Ok;
    for (int i = 0; i < ZM_LOADED_INSTRUMENTS && status == ZmStatus_Ok; i++) {
        ZmInstrument* ins = &m->instruments[i];
        if (ins->name[0] == '\0') {
            continue;
        }
        const char* url = index != nullptr ? nuade_file_index_find(index, ins->name) : nullptr;
        m->samples[i] = url != nullptr ? load_sample(io, url, ins) : nullptr;
        if (m->samples[i] == nullptr) {
            snprintf(missing, missing_cap, "%s", ins->name);
            status = url != nullptr ? ZmStatus_OutOfMemory : ZmStatus_MissingSample;
        }
        ins->sample = m->samples[i];
    }
    nuade_file_index_destroy(index);
    return status;
}

ZmCreateResult zm_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    ZmCreateResult result = { .status = ZmStatus_InvalidData };
    if (!zm_probe(data, size) || io == nullptr) {
        return result;
    }
    ZmPlayer* p = calloc(1, sizeof(ZmPlayer));
    uint8_t* copy = malloc(size);
    if (p == nullptr || copy == nullptr) {
        free(p);
        free(copy);
        result.status = ZmStatus_OutOfMemory;
        return result;
    }
    memcpy(copy, data, size);

    ZmModule* m = &p->module;
    m->data = copy;
    m->size = (uint32_t)size;
    m->position_count = data[0] + 1;
    m->pattern_count = data[1] + 1;
    m->start_position = data[2];
    m->end_position = data[3];
    m->speed = data[4];
    m->patterns_offset = INSTRUMENT_BLOCK + (uint32_t)m->position_count * POSITION_SIZE;
    m->moved_size = m->patterns_offset + (uint32_t)m->pattern_count * PATTERN_SIZE;
    for (uint32_t i = 0; i < ZM_INSTRUMENTS; i++) {
        m->instruments[i] = parse_instrument(m, i);
    }

    result.status = load_samples(m, path, io, result.missing_sample, sizeof(result.missing_sample));
    if (result.status != ZmStatus_Ok) {
        zm_destroy(p);
        return result;
    }
    result.player = p;
    return result;
}

void zm_destroy(ZmPlayer* player) {
    if (player == nullptr) {
        return;
    }
    for (int i = 0; i < ZM_LOADED_INSTRUMENTS; i++) {
        free(player->module.samples[i]);
    }
    free(player->module.data);
    free(player);
}

// InitSound
bool zm_start(ZmPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    const ZmModule* m = &player->module;
    ZmSong* s = &player->song;
    *s = (ZmSong) { 0 };
    s->loop_position = m->start_position;
    s->end_position = m->end_position;
    s->position = s->loop_position;
    s->tick = (uint8_t)(m->speed - 1);
    for (int i = 0; i < ZM_VOICES; i++) {
        s->voices[i].dma_bit = (uint16_t)(1u << i);
        nuade_voice_stop(voices, (uint32_t)i);
    }
    return true;
}

ZmTickResult zm_tick(ZmPlayer* player, NuadeVoices* voices) {
    ZmSong* s = &player->song;
    s->looped = false;
    play_tick(player);
    for (int i = 0; i < ZM_VOICES; i++) {
        nuade_paula_channel_sync(&s->voices[i].paula, (uint32_t)i, voices);
    }
    return (ZmTickResult) { .looped = s->looped };
}

const char* zm_instrument_name(const ZmPlayer* player, int instrument) {
    if (instrument < 0 || instrument >= ZM_INSTRUMENTS) {
        return "";
    }
    return player->module.instruments[instrument].name;
}

ZmPosition zm_position(const ZmPlayer* player) {
    const ZmSong* s = &player->song;
    return (ZmPosition) { .order = s->shown_position, .row = s->shown_row, .speed = player->module.speed };
}

int zm_order_count(const ZmPlayer* player) {
    return player->module.position_count;
}

ZmVoiceState zm_voice_state(const ZmPlayer* player, int voice) {
    const ZmVoice* v = &player->song.voices[voice];
    return (ZmVoiceState) { .note = v->note, .instrument = v->instrument };
}

ZmCell zm_pattern_cell(const ZmPlayer* player, int order, int voice, int row) {
    const ZmModule* m = &player->module;
    const Track track = track_at(m, order, voice);
    const uint32_t cell = cell_at(m, track.pattern, row);
    return (ZmCell) { .note = (uint8_t)((cell >> 24) & 0x3F),
                      .instrument = (uint8_t)((cell >> 20) & 0x0F),
                      .control = (uint8_t)((cell >> 16) & 0x0F),
                      .volume = (int8_t)(cell >> 8),
                      .param = (uint8_t)cell,
                      .legato = (cell & 0x80000000u) != 0 };
}
