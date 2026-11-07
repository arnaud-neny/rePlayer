// SPDX-License-Identifier: MIT
// Paul Shields player: the module check (the eagleplayer's Check2 and InitPlayer) and the readers of the module.

#include "nuade_utils.h"
#include "ps_internal.h"
#include <stdlib.h>
#include <string.h>

#define MAX_MODULE_SIZE (1u << 28)
#define FIRST_BYTES_ZERO 10 // Check2: the first sample record starts with ten zero bytes

// Where the first order list of an old module is announced: the word before it ends the previous list.
#define OLD_LIST_END_STOP 0xFFE8u
#define OLD_LIST_END_LOOP 0xFFECu

// The data hunk of the eagleplayer file. Its first bytes are the built-in wave forms (samples 16 to 21); bytes 0x3C..
// (built-in sample 22) are 68k code bytes that the replayer plays as a sample.
static const uint8_t k_wave_data[PSH_WAVE_DATA_SIZE] = {
    0x7F, 0x7F, 0x81, 0x81, 0x7F, 0x7F, 0x7F, 0x7F, 0x81, 0x81, 0x81, 0x81, 0x00, 0x7F, 0x00, 0x81, 0x00, 0x59, 0x7F,
    0x59, 0x00, 0xA7, 0x81, 0xA7, 0x7F, 0x2A, 0xD5, 0x80, 0x7F, 0x5A, 0x36, 0x11, 0xEE, 0xC9, 0xA5, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x48, 0xE7, 0xE0, 0xEE, 0x4B, 0xFA, 0x07, 0x8E, 0x4D, 0xF9, 0x00, 0xDF, 0xF0, 0x00, 0x30, 0x2D,
    0x00, 0x18, 0x02, 0x40, 0x07, 0x80, 0x3D, 0x40, 0x00, 0x9C, 0x3D, 0x6D, 0x00, 0x18, 0x00, 0x9A, 0x3D, 0x6D, 0x00,
    0x16, 0x00, 0x96, 0x42, 0x6D, 0x00, 0x16, 0x42, 0x6D, 0x00, 0x18, 0x4A, 0x39, 0x00, 0x00, 0xA8, 0xBC, 0x67, 0x0C,
    0x0C, 0x6D, 0x00, 0x40, 0x00, 0x1A, 0x67, 0x04, 0x53, 0x6D, 0x00, 0x1A, 0x4A, 0x39, 0x00, 0x00, 0xA8, 0xBD, 0x67,
    0x0C, 0x0C, 0x6D, 0x00, 0x80, 0x00, 0x1A, 0x67, 0x04, 0x52, 0x6D, 0x00, 0x1A, 0x41, 0xFA, 0x06, 0xDE, 0x49, 0xEE,
    0x00, 0xD0, 0x34, 0x3C, 0x00, 0x03, 0x08, 0x28, 0x00, 0x00, 0x00, 0x2C, 0x67, 0x00, 0x03, 0x1A, 0x53, 0x68, 0x00,
    0x4E, 0x66, 0x1C, 0x30, 0x28, 0x00, 0x52, 0xB0, 0x68, 0x00, 0x50, 0x67, 0x12, 0x65, 0x06, 0x53, 0x68, 0x00, 0x52,
    0x60, 0x04, 0x52, 0x68, 0x00, 0x52, 0x31, 0x68, 0x00, 0x4C, 0x00, 0x4E, 0x53, 0x68, 0x00, 0x28, 0x66, 0x00, 0x02,
    0x42, 0x22, 0x68, 0x00, 0x0C, 0x42, 0x40, 0x10, 0x19, 0x4E, 0xFB, 0x00, 0x02, 0x60, 0x00, 0x01, 0xC4, 0x60, 0x00,
    0x03, 0xE4, 0x60, 0x00, 0x01, 0xA8, 0x60, 0x00, 0x00, 0x2E, 0x60, 0x00, 0x01, 0xA8, 0x60, 0x00, 0x01, 0x6A, 0x60,
    0x00, 0x01, 0x42, 0x60, 0x00, 0x01, 0x6A, 0x60, 0x00, 0x00, 0xCE, 0x60, 0x00, 0x00, 0x2E, 0x60, 0x00, 0x00, 0x38,
    0x60, 0x00, 0x00, 0x8A, 0x60, 0x00, 0x00, 0xB6, 0x60, 0x00, 0x00, 0x38, 0x60, 0x00, 0x00, 0x2E, 0x53, 0x68, 0x00,
    0x2A, 0x66, 0x00, 0x00, 0x9E, 0x24, 0x68, 0x00, 0x04, 0x30, 0x1A, 0xB0, 0x7C, 0xFF, 0xE8, 0x65, 0x6E, 0x4E, 0xFB,
    0x00, 0xEA, 0x31, 0x5A, 0x00, 0x50, 0x31, 0x52, 0x00, 0x4C, 0x31, 0x5A, 0x00, 0x4E, 0x60, 0xE6, 0x31, 0x5A, 0x00,
    0x30, 0x60, 0xE0, 0x31, 0x5A, 0x00, 0x2E, 0x60, 0xDA, 0x48, 0x50, 0x30, 0x1A, 0x32, 0x1A, 0x41, 0xFA, 0x05, 0x04,
    0x31, 0x40, 0x00, 0x50, 0x31, 0x41, 0x00, 0x4C, 0x31, 0x41, 0x00, 0x4E, 0x41, 0xFA, 0x05, 0x52, 0x31, 0x40, 0x00,
    0x50, 0x31, 0x41, 0x00, 0x4C, 0x31, 0x41, 0x00, 0x4E, 0x41, 0xFA, 0x05, 0xA0, 0x31, 0x40, 0x00, 0x50, 0x31, 0x41,
    0x00, 0x4C, 0x31, 0x41, 0x00, 0x4E, 0x41, 0xFA, 0x05, 0xEE, 0x31, 0x40, 0x00, 0x50, 0x31, 0x41, 0x00, 0x4C, 0x31,
    0x41, 0x00, 0x4E, 0x20, 0x5F, 0x60, 0x90, 0x24, 0x68, 0x00, 0x00, 0x60, 0x8A, 0x31, 0x7C, 0x00, 0x01, 0x00, 0x2A,
    0x08, 0x80, 0x00, 0x0F, 0x67, 0x04, 0x31, 0x5A, 0x00, 0x2A, 0x22, 0x6D, 0x00, 0x0A, 0xD2, 0xC0, 0x21, 0x49, 0x00,
    0x08, 0x21, 0x4A, 0x00, 0x04, 0x60, 0x00, 0xFF, 0x1A, 0x22, 0x68, 0x00, 0x08, 0x60, 0x00, 0xFF, 0x12, 0x42, 0x68,
    0x00, 0x2C, 0x60, 0x00,
};

typedef struct WaveEntry {
    uint32_t offset;
    uint16_t length_words;
} WaveEntry;

static const WaveEntry k_waves[PSH_WAVE_COUNT] = {
    { 0, 2 }, { 4, 4 }, { 12, 2 }, { 16, 4 }, { 24, 2 }, { 28, 4 }, { 60, 200 },
};
#define WAVE_SILENCE 0x2C // zeros in the data hunk

static const PshLayout k_layouts[] = {
    {
        .kind = PshKind_Short,
        .sample_record_size = 10,
        .tables_offset = 0xA0,
        .table_count = 2,
        .voices_offset = 0xA4,
        .table_marker = 0x00B400B6u,
        .next_pattern_command = PshPatternCommand_NextPattern,
        .loop_channel = 1,
        .order_actions = { PshOrderAction_VolumeSlide, PshOrderAction_PitchOffset, PshOrderAction_Loop,
                           PshOrderAction_Stop, PshOrderAction_AllVolumeSlide, PshOrderAction_PitchTune },
    },
    {
        .kind = PshKind_Long,
        .sample_record_size = 32,
        .tables_offset = 0x200,
        .table_count = 2,
        .voices_offset = 0x204,
        .table_marker = 0x02140216u,
        .next_pattern_command = PshPatternCommand_NextPattern,
        .loop_channel = 1,
        .order_actions = { PshOrderAction_VolumeSlide, PshOrderAction_PitchOffset, PshOrderAction_Loop,
                           PshOrderAction_Stop, PshOrderAction_AllVolumeSlide, PshOrderAction_PitchTune },
    },
    {
        .kind = PshKind_Old,
        .sample_record_size = 32,
        .tables_offset = 0x200,
        .table_count = 1,
        .voices_offset = 0x202,
        .table_marker = 0,
        .next_pattern_command = PshOldPatternCommand_NextPattern,
        .loop_channel = 0,
        .order_actions = { PshOrderAction_Stop, PshOrderAction_Loop, PshOrderAction_VolumeSlide,
                           PshOrderAction_AllVolumeSlide, PshOrderAction_PitchOffset, PshOrderAction_PitchTune },
    },
};

const PshLayout* psh_layout(PshKind kind) {
    return &k_layouts[kind];
}

uint8_t psh_u8(const PshModule* m, int64_t offset) {
    return offset >= 0 && offset < (int64_t)m->size + 2 ? m->chip[offset] : 0;
}

uint16_t psh_u16(const PshModule* m, int64_t offset) {
    return (uint16_t)(psh_u8(m, offset) << 8 | psh_u8(m, offset + 1));
}

bool psh_order_action(const PshLayout* layout, uint16_t word, PshOrderAction* action) {
    if (word < PSH_ORDER_CODE_MIN || (word & 3) != 0) {
        return false;
    }
    const unsigned index = (word - PSH_ORDER_CODE_MIN) / 4;
    if (index >= PSH_ORDER_CODE_COUNT) {
        return false;
    }
    *action = layout->order_actions[index];
    return true;
}

// The song header of the raw file: the table words, then a start volume and an order list offset for every voice, in
// the file's order (the first pair belongs to Paula channel 3).

typedef struct FileHeader {
    uint16_t tables[2];
    struct {
        uint16_t volume;
        uint16_t order_list;
    } voices[PSH_VOICES];
} FileHeader;

static FileHeader read_file_header(const uint8_t* data, const PshLayout* layout) {
    FileHeader h = { { 0, 0 }, { { 0, 0 } } };
    for (uint32_t i = 0; i < layout->table_count; i++) {
        h.tables[i] = nuade_read_be16(data + layout->tables_offset + 2 * i);
    }
    for (uint32_t i = 0; i < PSH_VOICES; i++) {
        const uint8_t* pair = data + layout->voices_offset + 4 * i;
        h.voices[i].volume = nuade_read_be16(pair);
        h.voices[i].order_list = nuade_read_be16(pair + 2);
    }
    return h;
}

// The bytes the layout's header needs: Check2 and InitPlayer read nothing behind them.

static uint32_t header_size(const PshLayout* layout) {
    return layout->voices_offset + 4 * PSH_VOICES;
}

// Check2 for one layout: ten zero bytes, four equal start volumes and the layout's marker.

static bool layout_matches(const uint8_t* data, size_t size, const PshLayout* layout) {
    if (size < header_size(layout)) {
        return false;
    }
    for (uint32_t i = 0; i < FIRST_BYTES_ZERO; i++) {
        if (data[i] != 0) {
            return false;
        }
    }
    const FileHeader h = read_file_header(data, layout);
    for (uint32_t i = 1; i < PSH_VOICES; i++) {
        if (h.voices[i].volume != h.voices[0].volume) {
            return false;
        }
    }
    if (layout->table_marker != 0) {
        const uint16_t offset = h.tables[0];
        return offset != 0 && offset < 0x8000 && (offset & 1) == 0 && offset <= size - 4
               && nuade_read_be32(data + offset) == layout->table_marker;
    }
    const uint16_t first_list = h.voices[0].order_list;
    if (first_list < 2 || first_list >= 0x8000 || (first_list & 1) != 0 || first_list > size - 2) {
        return false;
    }
    const uint16_t before = nuade_read_be16(data + first_list - 2);
    return before == OLD_LIST_END_LOOP || before == OLD_LIST_END_STOP;
}

static const PshLayout* detect_layout(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < nuade_sizeof_array(k_layouts); i++) {
        if (layout_matches(data, size, &k_layouts[i])) {
            return &k_layouts[i];
        }
    }
    return nullptr;
}

// The fields of one sample record. The short layout has no sample volume.

typedef struct SampleRecord {
    uint16_t length_words;
    int16_t loop_offset; // bytes from the sample start
    uint16_t loop_length_words;
    uint16_t tune;
    uint16_t volume;
} SampleRecord;

static SampleRecord read_sample_record(const uint8_t* data, const PshLayout* layout, uint32_t index) {
    const uint8_t* record = data + index * layout->sample_record_size;
    if (layout->kind == PshKind_Short) {
        return (SampleRecord) { .length_words = nuade_read_be16(record + 2),
                                .loop_offset = (int16_t)nuade_read_be16(record + 4),
                                .loop_length_words = nuade_read_be16(record + 6),
                                .tune = nuade_read_be16(record + 8),
                                .volume = 0 };
    }
    return (SampleRecord) { .length_words = nuade_read_be16(record + 0x16),
                            .loop_offset = (int16_t)nuade_read_be16(record + 0x1A),
                            .loop_length_words = nuade_read_be16(record + 0x1C),
                            .tune = nuade_read_be16(record + 0x1E),
                            .volume = nuade_read_be16(record + 0x18) };
}

// InitPlayer's first part: where the sample bytes start. The order list with the highest offset is the last of the song
// data; its end is the word after the first repeat of the word that precedes it. Returns false when that word does not
// come again inside the module.

static bool find_sample_base(const uint8_t* data, size_t size, const PshLayout* layout, uint32_t* sample_base) {
    const FileHeader h = read_file_header(data, layout);
    int16_t last_list = 0;
    for (uint32_t i = 0; i < PSH_VOICES; i++) {
        if ((int16_t)h.voices[i].order_list > last_list) {
            last_list = (int16_t)h.voices[i].order_list;
        }
    }
    if (last_list < 2 || (size_t)last_list > size - 2) {
        return false;
    }
    const uint16_t marker = nuade_read_be16(data + last_list - 2);
    for (size_t at = (size_t)last_list; at + 2 <= size; at += 2) {
        if (nuade_read_be16(data + at) == marker) {
            *sample_base = (uint32_t)at + 2;
            return true;
        }
    }
    return false;
}

// The sample bytes of the 16 records have to fit in the file behind the song data.

static bool samples_fit(const uint8_t* data, size_t size, const PshLayout* layout, uint32_t sample_base) {
    uint64_t total = sample_base;
    for (uint32_t i = 0; i < PSH_SAMPLE_COUNT; i++) {
        total += 2u * read_sample_record(data, layout, i).length_words;
    }
    return total <= size;
}

// The sample table the replayer builds at song start: the samples follow each other behind the song data, then come
// the built-in wave forms.

static void build_samples(PshModule* m, const uint8_t* data) {
    int64_t cursor = m->sample_base;
    for (uint32_t i = 0; i < PSH_SAMPLE_COUNT; i++) {
        const SampleRecord r = read_sample_record(data, m->layout, i);
        PshSample* s = &m->samples[i];
        s->start = (uint32_t)cursor;
        s->length_words = r.length_words;
        s->tune = r.tune;
        s->volume = r.volume;
        if (r.loop_length_words < 2) {
            s->loop = m->silence;
            s->loop_length_words = 2;
        } else {
            const int64_t loop = cursor + r.loop_offset;
            s->loop = loop >= 0 && loop < (int64_t)m->size ? (uint32_t)loop : m->silence;
            s->loop_length_words = r.loop_length_words;
        }
        cursor += 2 * (int16_t)r.length_words;
    }
    m->sample_count = PSH_SAMPLE_COUNT;
    if (m->layout->kind == PshKind_Old) {
        return;
    }
    for (uint32_t i = 0; i < PSH_WAVE_COUNT; i++) {
        const uint32_t start = m->wave_base + k_waves[i].offset;
        m->samples[m->sample_count++] = (PshSample) { .start = start,
                                                      .length_words = k_waves[i].length_words,
                                                      .loop = start,
                                                      .loop_length_words = k_waves[i].length_words };
    }
}

// Check2 and the size check of InitPlayer: the layout and the start of the sample bytes.

static const PshLayout* check_module(const uint8_t* data, size_t size, uint32_t* sample_base) {
    if (size >= MAX_MODULE_SIZE) {
        return nullptr;
    }
    const PshLayout* layout = detect_layout(data, size);
    if (layout == nullptr || !find_sample_base(data, size, layout, sample_base)
        || !samples_fit(data, size, layout, *sample_base)) {
        return nullptr;
    }
    return layout;
}

// The header behind the sample records: the envelope or instrument table and the voices' start volumes and order
// lists. The first pair belongs to Paula channel 3.

static void read_header(PshModule* m, const uint8_t* data) {
    const FileHeader h = read_file_header(data, m->layout);
    if (m->layout->kind == PshKind_Old) {
        m->instruments = (int16_t)h.tables[0]; // the oldest replayer adds the offsets with sign extension
    } else {
        m->pitch_envelopes = h.tables[0];
        m->volume_envelopes = h.tables[1];
    }
    for (uint32_t i = 0; i < PSH_VOICES; i++) {
        PshVoiceHeader* v = &m->voices[PSH_VOICES - 1 - i];
        v->volume = h.voices[i].volume;
        v->order_list = m->layout->kind == PshKind_Old ? (int16_t)h.voices[i].order_list : h.voices[i].order_list;
    }
}

bool psh_module_load(PshModule* m, const uint8_t* data, size_t size) {
    memset(m, 0, sizeof(*m));
    m->layout = check_module(data, size, &m->sample_base);
    if (m->layout == nullptr) {
        return false;
    }
    m->size = (uint32_t)size;
    m->stream_idle = m->size;
    m->envelope_idle = m->size + 1;
    m->wave_base = (m->size + 3) & ~1u;
    m->silence = m->wave_base + WAVE_SILENCE;
    m->idle_sample = m->layout->kind == PshKind_Old ? m->silence : m->wave_base;
    uint8_t* image = calloc(1, m->wave_base + PSH_WAVE_DATA_SIZE);
    if (image == nullptr) {
        return false;
    }
    memcpy(image, data, size);
    image[m->stream_idle] = m->layout->next_pattern_command;
    image[m->envelope_idle] = PSH_ENVELOPE_END;
    memcpy(image + m->wave_base, k_wave_data, sizeof(k_wave_data));
    m->chip = nuade_paula_padded_copy(image, m->wave_base + PSH_WAVE_DATA_SIZE);
    free(image);
    if (m->chip == nullptr) {
        return false;
    }
    read_header(m, data);
    build_samples(m, data);
    if (!psh_check_song(m)) {
        psh_module_free(m);
        return false;
    }
    return true;
}

void psh_module_free(PshModule* m) {
    free(m->chip);
    m->chip = nullptr;
}

bool psh_probe(const uint8_t* data, size_t size) {
    PshModule m;
    if (!psh_module_load(&m, data, size)) {
        return false;
    }
    psh_module_free(&m);
    return true;
}

const PshSample* psh_sample_number(const PshModule* m, unsigned number) {
    return number < m->sample_count ? &m->samples[number] : nullptr;
}

const PshSample* psh_instrument_sample(const PshModule* m, int64_t record) {
    const int16_t entry_offset = (int16_t)psh_u16(m, record);
    if (entry_offset < 0 || entry_offset % PSH_SAMPLE_ENTRY_SIZE != 0) {
        return nullptr;
    }
    return psh_sample_number(m, (unsigned)entry_offset / PSH_SAMPLE_ENTRY_SIZE);
}

const int8_t* psh_chip_pointer(const PshModule* m, int64_t offset) {
    if (offset < 0 || offset >= (int64_t)m->wave_base + PSH_WAVE_DATA_SIZE) {
        offset = m->silence;
    }
    return (const int8_t*)(m->chip + (offset & ~INT64_C(1)));
}
