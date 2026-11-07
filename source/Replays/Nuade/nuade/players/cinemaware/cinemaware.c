// SPDX-License-Identifier: MIT
// Cinemaware player, translated from the UADE eagleplayer "Cinemaware player module V1.0" (Wanted Team).
//
// CwModule: "IBLK", the instrument count and a byte that nothing reads, the instrument blocks (0x8A bytes: a program
// number, the pitch and volume envelopes and three sample zones that name IFF 8SVX files in Instruments/), 16 bytes
// that map the MIDI channels to Paula voices, and "ASEQ" followed by the event stream.
//
// The events are MIDI-like, 5 bytes each: the wait in ticks before the event, a status byte (kind in the high nibble,
// channel in the low one) and two data bytes. Notes play on four voices, each with a volume envelope, a pitch
// envelope and a vibrato. A voice is picked by a score when the channel is not tied to a voice by the map. The song
// ends with an event of kind 1; it then pauses and restarts.

#include <nuade/players/cinemaware.h>

#include "nuade_base.h"
#include "nuade_file_index.h"
#include "nuade_path.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SAMPLES (CW_PROGRAMS * CW_ZONES_PER_INSTRUMENT)

// CwModule layout.
#define MAGIC_INSTRUMENTS "IBLK"
#define MAGIC_EVENTS "ASEQ"
#define COUNT_OFFSET 4 // the instrument count; the byte after it is not read
#define INSTRUMENTS_OFFSET 6
#define BLOCK_SIZE 0x8A
#define ZONES_OFFSET 0x3C // in the block
#define ZONE_SIZE 0x1A
#define EVENT_SIZE 5

// Check2 looks for ASEQ in the 0x100 bytes that start 0x12 bytes past the instrument blocks, from the "IBLK" count.
#define CHECK_SKIP 0x12
#define CHECK_WINDOW 0x100

// An IFF file: "FORM", the length, and the BODY chunk found at an even offset.
#define IFF_HEADER_SIZE 8
#define BODY_HEADER_SIZE 8

// The song ends at the first event of kind EventKind_End. InitPlayer insists that the stream holds the end event
// 00 00 10 2F 00, which is the one the songs use.
#define END_DATA1 0x2F

#define NO_VOICE (-1)   // no voice for a note
#define NO_CHANNEL (-1) // CwVoice.channel of a free voice
#define NOTE_AGE_START 0xFF
#define FIXED_PITCH_KEY 60 // an instrument with BEND_FIXED_PITCH plays every key as this one

// Paula writes.
#define START_PERIOD 0x80
#define DMA_LATCH_TICKS 2         // ticks after a note start before the repeat registers are written
#define PAUSE_TICKS 0x19          // ticks the song pauses at its end before it restarts
#define VOLUME_REGISTER_MASK 0x7F // the eagleplayer's master volume glue keeps 7 bits for AUDxVOL

// Pitch: 20 steps per semitone, 240 per octave; the pitch maps to a frequency through the table below.
#define STEPS_PER_SEMITONE 20
#define STEPS_PER_OCTAVE 240
#define PITCH_CENTER 0x80
#define BEND_CENTER 0x40
#define BEND_FIXED_PITCH 0xFF
#define PERIOD_SHIFT 20
#define VIBRATO_DIVISOR 5
#define VELOCITY_MAX 0x7F

#define MAX_EVENTS_PER_TICK 65536

// frequency * 0x8000 of the 240 steps of an octave: 0x8000 * 2^(step / 240), rounded down but for two entries.
static const uint16_t k_frequency[STEPS_PER_OCTAVE] = {
    0x8000, 0x805E, 0x80BD, 0x811D, 0x817C, 0x81DC, 0x823C, 0x829D, 0x82FD, 0x835E, 0x83C0, 0x8421, 0x8483, 0x84E5,
    0x8548, 0x85AA, 0x860D, 0x8670, 0x86D4, 0x8738, 0x879C, 0x8800, 0x8865, 0x88CA, 0x892F, 0x8995, 0x89FB, 0x8A61,
    0x8AC7, 0x8B2E, 0x8B95, 0x8BFD, 0x8C64, 0x8CCC, 0x8D34, 0x8D9D, 0x8E06, 0x8E6F, 0x8ED8, 0x8F42, 0x8FAC, 0x9017,
    0x9081, 0x90EC, 0x9158, 0x91C3, 0x922F, 0x929B, 0x9308, 0x9375, 0x93E2, 0x9450, 0x94BD, 0x952C, 0x959A, 0x9609,
    0x9678, 0x96E7, 0x9757, 0x97C7, 0x9837, 0x98A8, 0x9919, 0x998B, 0x99FC, 0x9A6E, 0x9AE1, 0x9B53, 0x9BC6, 0x9C3A,
    0x9CAD, 0x9D21, 0x9D96, 0x9E0A, 0x9E7F, 0x9EF5, 0x9F6A, 0x9FE0, 0xA057, 0xA0CE, 0xA145, 0xA1BC, 0xA234, 0xA2AC,
    0xA324, 0xA39D, 0xA416, 0xA490, 0xA50A, 0xA584, 0xA5FE, 0xA679, 0xA6F5, 0xA770, 0xA7EC, 0xA868, 0xA8E5, 0xA962,
    0xA9E0, 0xAA5D, 0xAADC, 0xAB5A, 0xABD9, 0xAC58, 0xACD8, 0xAD58, 0xADD8, 0xAE59, 0xAEDA, 0xAF5B, 0xAFDD, 0xB05F,
    0xB0E2, 0xB165, 0xB1E8, 0xB26C, 0xB2F0, 0xB375, 0xB3FA, 0xB47F, 0xB504, 0xB58A, 0xB611, 0xB698, 0xB71F, 0xB7A7,
    0xB82E, 0xB8B7, 0xB940, 0xB9C9, 0xBA52, 0xBADC, 0xBB67, 0xBBF1, 0xBC7D, 0xBD08, 0xBD94, 0xBE20, 0xBEAD, 0xBF3A,
    0xBFC8, 0xC056, 0xC0E4, 0xC173, 0xC203, 0xC292, 0xC322, 0xC3B3, 0xC444, 0xC4D5, 0xC567, 0xC5F9, 0xC68B, 0xC71E,
    0xC7B2, 0xC846, 0xC8DA, 0xC96F, 0xCA04, 0xCA99, 0xCB2F, 0xCBC6, 0xCC5D, 0xCCF4, 0xCD8C, 0xCE24, 0xCEBD, 0xCF56,
    0xCFEF, 0xD089, 0xD124, 0xD1BE, 0xD25A, 0xD2F6, 0xD392, 0xD42E, 0xD4CC, 0xD569, 0xD607, 0xD6A6, 0xD744, 0xD7E4,
    0xD884, 0xD924, 0xD9C5, 0xDA66, 0xDB08, 0xDBAA, 0xDC4D, 0xDCF0, 0xDD93, 0xDE37, 0xDEDC, 0xDF81, 0xE026, 0xE0CC,
    0xE173, 0xE21A, 0xE2C1, 0xE369, 0xE411, 0xE4BA, 0xE564, 0xE60E, 0xE6B8, 0xE763, 0xE80E, 0xE8BA, 0xE966, 0xEA13,
    0xEAC0, 0xEB6E, 0xEC1C, 0xECCB, 0xED7B, 0xEE2A, 0xEEDB, 0xEF8C, 0xF03D, 0xF0EF, 0xF1A1, 0xF254, 0xF308, 0xF3BC,
    0xF470, 0xF525, 0xF5DB, 0xF691, 0xF747, 0xF7FE, 0xF8B6, 0xF96E, 0xFA27, 0xFAE0, 0xFB9A, 0xFC54, 0xFD0F, 0xFDCA,
    0xFE86, 0xFF43,
};

// What a one-shot sample repeats: two silent words.
static const int8_t k_silence[4] = { 0 };

typedef enum EventKind {
    EventKind_End = 0x1,
    EventKind_NoteOff = 0x8,
    EventKind_NoteOn = 0x9,
    EventKind_Controller = 0xB,
    EventKind_Program = 0xC,
    EventKind_PitchBend = 0xE,
} EventKind;

typedef enum Controller {
    Controller_Volume = 7,
    Controller_Pedal = 0x40,
} Controller;

typedef struct Event {
    int16_t delay; // ticks before the event
    uint8_t kind;  // EventKind, others do nothing
    uint8_t channel;
    uint8_t data1;
    uint8_t data2;
} Event;

typedef struct LoadedSample {
    int8_t* data; // owned
    uint32_t size;
} LoadedSample;

typedef struct SampleLoader {
    const NuadeIo* io;
    NuadeFileIndex* index;
    LoadedSample loaded[MAX_SAMPLES];
    int count;
    char missing[CW_ZONE_NAME_SIZE + 1];
} SampleLoader;

typedef struct Note {
    uint8_t channel;
    uint8_t program;
    uint8_t key;
    uint8_t velocity;
} Note;

typedef enum VoiceScore {
    VoiceScore_Oldest = 0x01, // plays the oldest note of all voices seen so far
    VoiceScore_SameKey = 0x02,
    VoiceScore_SameChannel = 0x04,
    VoiceScore_Reusable = 0x08, // the key is up, or the same key on the same channel
    VoiceScore_Free = 0x10,
    VoiceScore_Retrigger = 0x30, // a released voice of the same channel (unless channel 15 is tied to a voice)
} VoiceScore;

static CwZone read_zone(const uint8_t* record) {
    CwZone zone = { 0 };
    memcpy(zone.name, record, CW_ZONE_NAME_SIZE);
    for (size_t len = strlen(zone.name); len > 0 && zone.name[len - 1] == ' '; len--) {
        zone.name[len - 1] = '\0'; // ExtLoad removes the trailing spaces of the name
    }
    zone.has_sample = nuade_read_be32(record + 6) != 0;
    zone.key_low = record[0xA];
    zone.key_high = record[0xB];
    zone.period = nuade_read_be16(record + 0xC);
    zone.start = (int32_t)nuade_read_be32(record + 0xE);
    zone.loop = (int32_t)nuade_read_be32(record + 0x12);
    zone.end = (int32_t)nuade_read_be32(record + 0x16);
    return zone;
}

static CwInstrument read_instrument(const uint8_t* block) {
    CwInstrument ins = { 0 };
    ins.program = block[0];
    ins.transpose = (int8_t)block[0x12];
    ins.velocity_boost = block[0x14];
    ins.vibrato_max = block[0x15];
    ins.vibrato_step = block[0x16];
    ins.bend_range = block[0x17];
    ins.pitch_release = block[0x18];
    ins.pitch_last = block[0x19];
    memcpy(ins.pitch_level, block + 0x1A, CW_ENVELOPE_STAGES);
    memcpy(ins.pitch_rate, block + 0x22, CW_ENVELOPE_STAGES);
    ins.volume_release = block[0x2A];
    ins.volume_last = block[0x2B];
    memcpy(ins.volume_level, block + 0x2C, CW_ENVELOPE_STAGES);
    memcpy(ins.volume_rate, block + 0x34, CW_ENVELOPE_STAGES);
    for (int i = 0; i < CW_ZONES_PER_INSTRUMENT; i++) {
        ins.zones[i] = read_zone(block + ZONES_OFFSET + i * ZONE_SIZE);
    }
    return ins;
}

// An event past the end of the file reads as the end event.
static Event read_event(const CwModule* m, size_t offset) {
    if (offset + EVENT_SIZE > m->size) {
        return (Event) { .kind = EventKind_End };
    }
    const uint8_t* e = m->data + offset;
    return (Event) { .delay = (int16_t)nuade_read_be16(e),
                     .kind = e[2] >> 4,
                     .channel = e[2] & (CW_CHANNEL_COUNT - 1),
                     .data1 = e[3],
                     .data2 = e[4] };
}

static bool has_event_magic(const uint8_t* data, size_t size, size_t offset) {
    return offset + 4 <= size && memcmp(data + offset, MAGIC_EVENTS, 4) == 0;
}

bool cw_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size <= COUNT_OFFSET || memcmp(data, MAGIC_INSTRUMENTS, 4) != 0) {
        return false;
    }
    const size_t count = data[COUNT_OFFSET];
    if (count == 0 || count > CW_PROGRAMS) {
        return false;
    }
    const size_t scan = COUNT_OFFSET + count * BLOCK_SIZE + CHECK_SKIP;
    for (size_t offset = scan; offset < scan + CHECK_WINDOW; offset += 2) {
        if (has_event_magic(data, size, offset)) {
            return true;
        }
    }
    return false;
}

// InitPlayer: the instrument table, the channel map and the event stream, which must hold the end event.
static bool parse_module(CwModule* m, const uint8_t* data, size_t size) {
    m->data = data;
    m->size = size;
    m->instrument_count = data[COUNT_OFFSET];
    for (int i = 0; i < CW_PROGRAMS; i++) {
        m->block_of_program[i] = -1;
    }
    const size_t map_offset = INSTRUMENTS_OFFSET + (size_t)m->instrument_count * BLOCK_SIZE;
    if (map_offset + CW_CHANNEL_COUNT > size) {
        return false;
    }
    for (int i = 0; i < m->instrument_count; i++) {
        m->instruments[i] = read_instrument(data + INSTRUMENTS_OFFSET + (size_t)i * BLOCK_SIZE);
        // The replayer indexes its table with the program as a signed byte; a program past 127 writes outside it.
        if (m->instruments[i].program < CW_PROGRAMS) {
            m->block_of_program[m->instruments[i].program] = (int16_t)i;
        }
    }
    memcpy(m->channel_voice, data + map_offset, CW_CHANNEL_COUNT);

    size_t events = map_offset;
    while (events + 4 <= size && !has_event_magic(data, size, events)) {
        events += 2;
    }
    if (events + 4 > size) {
        return false;
    }
    m->events = events + 4;
    for (size_t offset = m->events; offset + EVENT_SIZE <= size; offset += EVENT_SIZE) {
        const Event e = read_event(m, offset);
        if (e.kind == EventKind_End && e.channel == 0 && e.data1 == END_DATA1 && e.data2 == 0) {
            return true;
        }
    }
    return false;
}

static void free_loaded(SampleLoader* loader) {
    for (int i = 0; i < loader->count; i++) {
        free(loader->loaded[i].data);
        loader->loaded[i].data = nullptr;
    }
}

// The sample data of an IFF file: what follows the first BODY chunk header at an even offset (the replayer never reads
// the chunk length). Returns false if the file is not an IFF file with a BODY chunk.
static bool find_body(const uint8_t* file, size_t size, size_t* offset, size_t* length) {
    if (size < IFF_HEADER_SIZE || memcmp(file, "FORM", 4) != 0) {
        return false;
    }
    for (size_t at = IFF_HEADER_SIZE; at + BODY_HEADER_SIZE <= size; at += 2) {
        if (memcmp(file + at, "BODY", 4) == 0) {
            *offset = at + BODY_HEADER_SIZE;
            *length = size - *offset;
            return true;
        }
    }
    return false;
}

// ExtLoad: the sample file of a zone is Instruments/<name>.
static CwStatus load_sample(SampleLoader* loader, const CwZone* zone) {
    snprintf(loader->missing, sizeof(loader->missing), "%s", zone->name);
    const char* url = loader->index != nullptr ? nuade_file_index_find(loader->index, zone->name) : nullptr;
    if (url == nullptr || zone->name[0] == '\0') {
        return CwStatus_MissingSample;
    }
    const NuadeIoReadResult file = NuadeIo_read_url_to_memory(loader->io, url);
    if (file.data == nullptr) {
        return CwStatus_MissingSample;
    }
    size_t offset = 0;
    size_t length = 0;
    CwStatus status = CwStatus_InvalidData;
    if (find_body(file.data, (size_t)file.size, &offset, &length)) {
        int8_t* data = (int8_t*)nuade_paula_padded_copy(file.data + offset, length);
        if (data != nullptr) {
            loader->loaded[loader->count++] = (LoadedSample) { .data = data, .size = (uint32_t)length };
            status = CwStatus_Ok;
        } else {
            status = CwStatus_OutOfMemory;
        }
    }
    NuadeIo_free_url_to_memory(loader->io, file.data);
    return status;
}

// ExtLoad loads the samples of the zones in file order; InitPlayer then hands them out to the zones of the
// instruments by program number, in the order they were loaded. The two orders differ when the blocks are not in
// program order, or when a program is defined twice.
static CwStatus load_samples(CwModule* m, SampleLoader* loader) {
    for (int i = 0; i < m->instrument_count; i++) {
        for (int z = 0; z < CW_ZONES_PER_INSTRUMENT; z++) {
            if (!m->instruments[i].zones[z].has_sample) {
                continue;
            }
            const CwStatus status = load_sample(loader, &m->instruments[i].zones[z]);
            if (status != CwStatus_Ok) {
                return status;
            }
        }
    }
    int next = 0;
    for (int program = 0; program < CW_PROGRAMS; program++) {
        if (m->block_of_program[program] < 0) {
            continue;
        }
        CwInstrument* ins = &m->instruments[m->block_of_program[program]];
        for (int z = 0; z < CW_ZONES_PER_INSTRUMENT; z++) {
            CwZone* zone = &ins->zones[z];
            if (!zone->has_sample) {
                continue;
            }
            if (next >= loader->count) {
                return CwStatus_InvalidData;
            }
            zone->data = loader->loaded[next].data;
            zone->size = loader->loaded[next].size;
            loader->loaded[next++].data = nullptr;
        }
    }
    return CwStatus_Ok;
}

// A song without samples needs no Instruments/ directory.
static bool needs_samples(const CwModule* m) {
    for (int i = 0; i < m->instrument_count; i++) {
        for (int z = 0; z < CW_ZONES_PER_INSTRUMENT; z++) {
            if (m->instruments[i].zones[z].has_sample) {
                return true;
            }
        }
    }
    return false;
}

static CwStatus load_module_samples(CwModule* m, const char* path, const NuadeIo* io, char* missing,
                                    size_t missing_cap) {
    if (!needs_samples(m)) {
        return CwStatus_Ok;
    }
    SampleLoader* loader = calloc(1, sizeof(SampleLoader));
    if (loader == nullptr) {
        return CwStatus_OutOfMemory;
    }
    loader->io = io;
    char dir[1024] = { 0 };
    if (path != nullptr && nuade_path_resolve(path, "Instruments", dir, sizeof(dir))) {
        const char* dirs[] = { dir };
        loader->index = nuade_file_index_create(io, dirs, 1);
    }
    const CwStatus status = load_samples(m, loader);
    if (status == CwStatus_MissingSample) {
        snprintf(missing, missing_cap, "%s", loader->missing);
    }
    free_loaded(loader);
    nuade_file_index_destroy(loader->index);
    free(loader);
    return status;
}

static void free_module_samples(CwModule* m) {
    for (int i = 0; i < m->instrument_count; i++) {
        for (int z = 0; z < CW_ZONES_PER_INSTRUMENT; z++) {
            free(m->instruments[i].zones[z].data);
            m->instruments[i].zones[z].data = nullptr;
        }
    }
}

CwCreateResult cw_create(const uint8_t* data, size_t size, const char* path, const NuadeIo* io) {
    CwCreateResult result = { .status = CwStatus_InvalidData };
    if (!cw_probe(data, size) || io == nullptr) {
        return result;
    }
    CwPlayer* p = calloc(1, sizeof(CwPlayer));
    if (p == nullptr) {
        result.status = CwStatus_OutOfMemory;
        return result;
    }
    if (parse_module(&p->module, data, size)) {
        result.status = load_module_samples(&p->module, path, io, result.missing_sample, sizeof(result.missing_sample));
    }
    if (result.status != CwStatus_Ok) {
        free_module_samples(&p->module);
        free(p);
        return result;
    }
    result.player = p;
    return result;
}

void cw_destroy(CwPlayer* player) {
    if (player != nullptr) {
        free_module_samples(&player->module);
        free(player);
    }
}

// The instrument of a program, 0 if there is none (a note then plays nothing: the replayer would read address 0).
static const CwInstrument* program_instrument(const CwModule* m, uint8_t program) {
    return program < CW_PROGRAMS && m->block_of_program[program] >= 0 ? &m->instruments[m->block_of_program[program]]
                                                                      : nullptr;
}

// A stage of an envelope table; a stage outside it (a release stage past the table) reads as 0.
static uint8_t stage_value(const uint8_t* table, int8_t stage) {
    return stage >= 0 && stage < CW_ENVELOPE_STAGES ? table[stage] : 0;
}

// The sign-extended byte as a 16-bit value, which is how the replayer compares the key bounds of a zone.
static uint16_t sign_extended(uint8_t value) {
    return (uint16_t)(int16_t)(int8_t)value;
}

static bool zone_covers(const CwZone* zone, uint8_t key) {
    return key >= sign_extended(zone->key_low) && key <= sign_extended(zone->key_high);
}

// The first zone that covers the key; the keys of the zones that follow it are not tried.
static const CwZone* find_zone(const CwInstrument* ins, uint8_t key) {
    for (int i = 0; i < CW_ZONES_PER_INSTRUMENT; i++) {
        if (zone_covers(&ins->zones[i], key)) {
            return &ins->zones[i];
        }
    }
    return nullptr;
}

// AUDxLC and AUDxLEN for the sample bytes from `from` to the end of the zone. A start outside the loaded data has a
// null AUDxLC; a length that runs past it plays the zeros behind the padded copy (Paula would read on into whatever
// follows it).
static void set_window(const CwZone* zone, int32_t from, const int8_t** lc, uint16_t* len_words) {
    const int32_t first = from / 2 * 2;
    *len_words = (uint16_t)((int32_t)((uint32_t)zone->end - (uint32_t)from) / 2);
    *lc = first >= 0 && (uint32_t)first <= zone->size ? zone->data + first : nullptr;
}

// The voice stops, then starts the zone's sample with volume 0 and the period register at its start
// value. The repeat registers follow DMA_LATCH_TICKS ticks later.
static void voice_start(CwVoice* v, const CwChannel* channel, const CwInstrument* ins, const CwZone* zone,
                        const Note* note) {
    v->paula.period = START_PERIOD;
    nuade_paula_channel_dma_off(&v->paula);

    if (zone->loop >= 0) {
        set_window(zone, zone->loop, &v->loop_lc, &v->loop_len_words);
    } else {
        v->loop_lc = k_silence;
        v->loop_len_words = sizeof(k_silence) / 2;
    }
    v->latch_ticks = DMA_LATCH_TICKS;
    v->key = (int8_t)note->key;
    v->program = note->program;
    v->velocity = (int8_t)note->velocity;
    v->period_scale = zone->period;
    v->volume_stage = 0;
    v->pitch_stage = 0;
    v->volume_level = 0;
    v->pitch_level = PITCH_CENTER;
    v->age = NOTE_AGE_START;
    v->played = true;
    v->vibrato = 0;
    v->vibrato_step = ins->vibrato_step;
    v->key_down = true;
    v->pedal = channel->pedal;
    v->channel = (int8_t)note->channel;

    v->paula.volume = 0;
    set_window(zone, zone->start, &v->paula.lc, &v->paula.len_words);
}

// The note off of a voice: both envelopes jump to their release stages. The pedal bit stays.
static void voice_release(CwVoice* v, const CwInstrument* ins) {
    v->volume_stage = (int8_t)ins->volume_release;
    v->pitch_stage = (int8_t)ins->pitch_release;
    v->key_down = false;
}

// Higher is better. `oldest` is the age of the oldest note seen so far.
static int score_voice(const CwModule* m, const CwVoice* v, uint8_t channel, uint8_t key, uint8_t* oldest) {
    int score = 0;
    if (v->channel < 0) {
        score |= VoiceScore_Free;
    }
    if (!v->key_down) {
        score |= VoiceScore_Reusable;
        if (v->channel == channel && m->channel_voice[CW_CHANNEL_COUNT - 1] == 0) {
            score |= VoiceScore_Retrigger;
        }
    }
    if (v->channel == channel) {
        score |= VoiceScore_SameChannel;
        if (v->key == key) {
            score |= VoiceScore_Reusable;
        }
    }
    if (v->key == key) {
        score |= VoiceScore_SameKey;
    }
    if (v->age < *oldest) {
        *oldest = v->age;
        score |= VoiceScore_Oldest;
    }
    return score;
}

// The voice for a note: the one the map ties the channel to, else the best scoring one (the first of equals; none
// when all score 0).
static int choose_voice(const CwPlayer* p, uint8_t channel, uint8_t key) {
    const uint8_t tied = p->module.channel_voice[channel];
    if (tied < CW_VOICES) {
        return tied;
    }
    int best = NO_VOICE;
    int best_score = 0;
    uint8_t oldest = NOTE_AGE_START;
    for (int i = 0; i < CW_VOICES; i++) {
        const int score = score_voice(&p->module, &p->voices[i], channel, key, &oldest);
        if (best_score < score) {
            best_score = score;
            best = i;
        }
    }
    return best;
}

static void note_on(CwPlayer* p, uint8_t channel, uint8_t key, uint8_t velocity) {
    const Note note = { .channel = channel, .program = p->channels[channel].program, .key = key, .velocity = velocity };
    const CwInstrument* ins = program_instrument(&p->module, note.program);
    const int voice = choose_voice(p, channel, key);
    if (ins == nullptr || voice == NO_VOICE) {
        return;
    }
    const CwZone* zone = find_zone(ins, key);
    if (zone != nullptr && zone->data != nullptr) {
        voice_start(&p->voices[voice], &p->channels[channel], ins, zone, &note);
    }
}

static void note_off(CwPlayer* p, uint8_t channel, uint8_t key) {
    for (int i = 0; i < CW_VOICES; i++) {
        CwVoice* v = &p->voices[i];
        if (v->key == key && v->channel == channel && v->key_down) {
            voice_release(v, program_instrument(&p->module, v->program));
        }
    }
}

// The pedal of a channel changed. A voice whose key is down remembers it; a voice whose key is up and held by the
// pedal is released again when the pedal goes up.
static void set_pedal(CwPlayer* p, uint8_t channel, bool pedal) {
    for (int i = 0; i < CW_VOICES; i++) {
        CwVoice* v = &p->voices[i];
        if (v->channel != channel) {
            continue;
        }
        if (v->key_down) {
            v->pedal = pedal;
        } else if (!pedal && v->pedal) {
            voice_release(v, program_instrument(&p->module, v->program));
            v->pedal = false;
        }
    }
}

static void reset_channels(CwPlayer* p) {
    for (int i = 0; i < CW_CHANNEL_COUNT; i++) {
        p->channels[i] = (CwChannel) { .program = (uint8_t)i, .bend = BEND_CENTER, .volume = 0x7F };
    }
}

// The voices stop being allocated (their registers keep playing), the channels and the sequencer start over.
static void restart_song(CwPlayer* p) {
    for (int i = 0; i < CW_VOICES; i++) {
        p->voices[i].channel = NO_CHANNEL;
        p->voices[i].key_down = false;
        p->voices[i].pedal = false;
    }
    reset_channels(p);
    p->sequencer.position = p->module.events;
    p->sequencer.delay = read_event(&p->module, p->sequencer.position).delay;
}

// The end event: SongEnd, then a pause with the audio off, then the song again.
static void end_song(CwPlayer* p) {
    p->looped = true;
    p->sequencer.pause = PAUSE_TICKS;
    restart_song(p);
}

static void run_event(CwPlayer* p, const Event* e) {
    CwChannel* channel = &p->channels[e->channel];
    switch (e->kind) {
        case EventKind_End:
            end_song(p);
            break;
        case EventKind_NoteOff:
            note_off(p, e->channel, e->data1);
            break;
        case EventKind_NoteOn:
            if (e->data2 == 0) {
                note_off(p, e->channel, e->data1);
            } else {
                note_on(p, e->channel, e->data1, e->data2);
            }
            break;
        case EventKind_Controller:
            // Controller 1 is stored by the replayer and never read.
            if (e->data1 == Controller_Volume) {
                channel->volume = e->data2;
            } else if (e->data1 == Controller_Pedal) {
                channel->pedal = e->data2 > 0;
                set_pedal(p, e->channel, channel->pedal);
            }
            break;
        case EventKind_Program:
            channel->program = e->data1;
            break;
        case EventKind_PitchBend:
            channel->bend = e->data2;
            break;
        default:
            break;
    }
}

// Runs the events that are due. Returns false if the song ended (the voices are left alone this tick).
static bool run_events(CwPlayer* p) {
    CwSequencer* s = &p->sequencer;
    for (int i = 0; i < MAX_EVENTS_PER_TICK; i++) {
        if (s->delay > 0) {
            s->delay--;
            return true;
        }
        const Event event = read_event(&p->module, s->position);
        run_event(p, &event);
        if (s->pause > 0) {
            return false;
        }
        s->position += EVENT_SIZE;
        s->delay = read_event(&p->module, s->position).delay;
    }
    return true;
}

// The volume envelope moves the level to the target of its stage at the stage's rate; a stage with rate 0 holds while
// the key is down. A level below 1 or a stage past the last frees the voice.
static void volume_envelope(CwVoice* v, const CwInstrument* ins, bool key_down) {
    uint16_t rate = stage_value(ins->volume_rate, v->volume_stage);
    if (!key_down && rate == 0) {
        rate = 1;
    }
    const int16_t target = stage_value(ins->volume_level, v->volume_stage);
    int16_t level = (int16_t)v->volume_level;
    if (target > level) {
        level = (int16_t)(level + rate);
        if (target <= level) {
            level = target;
            v->volume_stage++;
        }
    } else {
        level = (int16_t)(level - rate);
        if (target >= level) {
            level = target;
            v->volume_stage++;
        }
        if (level < 1) {
            v->channel = NO_CHANNEL;
            v->key_down = false;
        }
    }
    v->volume_level = (uint16_t)level;
    if (key_down && v->volume_stage > (int8_t)ins->volume_last) {
        v->volume_stage = 0;
    }
    if (v->volume_stage >= CW_ENVELOPE_STAGES) {
        v->volume_level = 0;
        v->channel = NO_CHANNEL;
    }
}

// The pitch envelope, the same way (unsigned 16-bit levels).
static void pitch_envelope(CwVoice* v, const CwInstrument* ins, bool key_down) {
    if (v->pitch_stage >= CW_ENVELOPE_STAGES) {
        return;
    }
    const uint16_t target = stage_value(ins->pitch_level, v->pitch_stage);
    const uint16_t rate = stage_value(ins->pitch_rate, v->pitch_stage);
    if (target > v->pitch_level) {
        v->pitch_level = (uint16_t)(v->pitch_level + rate);
        if (target <= v->pitch_level) {
            v->pitch_level = target;
            v->pitch_stage++;
        }
    } else {
        v->pitch_level = (uint16_t)(v->pitch_level - rate);
        if (target >= v->pitch_level) {
            v->pitch_level = target;
            v->pitch_stage++;
        }
    }
    if (key_down && v->pitch_stage > (int8_t)ins->pitch_last) {
        v->pitch_stage = 0;
    }
}

static void vibrato_tick(CwVoice* v, const CwInstrument* ins) {
    v->vibrato = (int16_t)(v->vibrato + v->vibrato_step);
    if (v->vibrato < 0) {
        v->vibrato = 0;
        v->vibrato_step = ins->vibrato_step;
    }
    if ((uint16_t)v->vibrato > ins->vibrato_max) {
        v->vibrato = ins->vibrato_max;
        v->vibrato_step = (int16_t)-ins->vibrato_step;
    }
}

// The pitch in 1/20 semitones: key and transpose, vibrato, pitch envelope and pitch bend, all in 16-bit arithmetic.
static int16_t voice_pitch(const CwVoice* v, const CwInstrument* ins, const CwChannel* channel) {
    const int key = ins->bend_range == BEND_FIXED_PITCH ? FIXED_PITCH_KEY : v->key;
    uint16_t pitch = (uint16_t)((key + ins->transpose) * STEPS_PER_SEMITONE);
    pitch = (uint16_t)(pitch + v->vibrato / VIBRATO_DIVISOR);
    pitch = (uint16_t)(pitch + (uint16_t)((v->pitch_level - PITCH_CENTER) * 4));
    uint16_t bend = (uint16_t)((uint16_t)(channel->bend - BEND_CENTER) * ins->bend_range);
    bend = (uint16_t)(bend * STEPS_PER_SEMITONE);
    return (int16_t)(pitch + (uint16_t)((int16_t)bend >> 6));
}

// The period register: the zone's period of key 60 divided by the frequency ratio of the pitch.
static uint16_t voice_period(const CwVoice* v, int16_t pitch) {
    const int octave = pitch / STEPS_PER_OCTAVE;
    const int step = pitch % STEPS_PER_OCTAVE;
    if (step < 0) {
        // A pitch in -239..-1: the replayer reads the words before its frequency table, which hold code. No song has
        // one; the voice gets period 0 here (Paula's longest period).
        return 0;
    }
    const uint32_t shift = (uint32_t)octave & 63;
    uint32_t frequency = shift < 32 ? (uint32_t)k_frequency[step] << shift : 0;
    if (frequency == 0) {
        frequency = 1;
    }
    return (uint16_t)((int64_t)(int32_t)((uint32_t)v->period_scale << PERIOD_SHIFT) / (int32_t)frequency);
}

// AUDxVOL: velocity (plus the instrument's boost, at most 127) times the channel volume times the envelope.
static uint8_t voice_volume(const CwVoice* v, const CwInstrument* ins, const CwChannel* channel) {
    int16_t velocity = (int16_t)(v->velocity + ins->velocity_boost);
    if (velocity > VELOCITY_MAX) {
        velocity = VELOCITY_MAX;
    }
    uint16_t volume = (uint16_t)(channel->volume * (uint16_t)velocity);
    volume = (uint16_t)((volume >> 7) * v->volume_level);
    volume = volume >> 9;
    return volume & VOLUME_REGISTER_MASK;
}

// The interrupt, for one voice. The original waits several raster lines before it switches the DMA on.
static void voice_tick(CwPlayer* p, CwVoice* v) {
    if (v->channel < 0) {
        return;
    }
    const CwChannel* channel = &p->channels[v->channel];
    const CwInstrument* ins = program_instrument(&p->module, v->program);
    const bool key_down = v->key_down;
    if (v->age > 0) {
        v->age--;
    }
    volume_envelope(v, ins, key_down);
    pitch_envelope(v, ins, key_down);
    vibrato_tick(v, ins);

    v->paula.period = voice_period(v, voice_pitch(v, ins, channel));
    if (v->latch_ticks > 0 && --v->latch_ticks == 0) {
        v->paula.lc = v->loop_lc;
        v->paula.len_words = v->loop_len_words;
    }
    v->paula.volume = voice_volume(v, ins, channel);
    if (v->channel < 0) {
        nuade_paula_channel_dma_off(&v->paula);
    } else {
        nuade_paula_channel_dma_on(&v->paula);
    }
}

bool cw_start(CwPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    player->looped = false;
    player->sequencer = (CwSequencer) { 0 };
    for (int i = 0; i < CW_VOICES; i++) {
        player->voices[i] = (CwVoice) { 0 };
        nuade_voice_stop(voices, (uint32_t)i);
    }
    restart_song(player);
    return true;
}

// A pause after the end event: the DMA of all voices is off.
static void pause_tick(CwPlayer* p) {
    for (int i = 0; i < CW_VOICES; i++) {
        nuade_paula_channel_dma_off(&p->voices[i].paula);
    }
    p->sequencer.pause--;
}

CwTickResult cw_tick(CwPlayer* player, NuadeVoices* voices) {
    player->looped = false;
    if (player->sequencer.pause > 0) {
        pause_tick(player);
    } else if (run_events(player)) {
        for (int i = 0; i < CW_VOICES; i++) {
            voice_tick(player, &player->voices[i]);
        }
    }
    for (int i = 0; i < CW_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, (uint32_t)i, voices);
    }
    return (CwTickResult) { .looped = player->looped };
}

const char* cw_instrument_name(const CwPlayer* player, int instrument) {
    const CwInstrument* ins = instrument >= 0 ? program_instrument(&player->module, (uint8_t)instrument) : nullptr;
    for (int i = 0; ins != nullptr && i < CW_ZONES_PER_INSTRUMENT; i++) {
        if (ins->zones[i].has_sample) {
            return ins->zones[i].name;
        }
    }
    return "";
}

CwVoiceState cw_voice_state(const CwPlayer* player, int voice) {
    const CwVoice* v = &player->voices[voice];
    return (CwVoiceState) { .note = v->played ? v->key : -1, .instrument = v->played ? v->program : -1 };
}
