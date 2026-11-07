// SPDX-License-Identifier: MIT
// AProSys player: the module container and the typed readers of its records.

#include "aprosys_internal.h"

#define APS_SECTIONS 9
#define APS_MAGIC_BYTES 8

// UADE's allocator puts the player's wave memory this far past the next 256-byte boundary behind the module
#define APS_WAVE_GAP 0x310u

// The sample record that stops a voice: kind 0xd, nothing else (the player's own data block holds it).
static const uint8_t k_silent_record[APS_SAMPLE_RECORD_SIZE] = { 0xd0, 0, 0, 0, 0, 0 };

// The exception vector table of UADE's 68k at address 0, in longs: all 0x1348 except these. A voice that has not
// started a note reads its instrument, envelope streams and sample record there; the zero pairs at the start stall an
// envelope (see the jump budget) until its stream walks past them, and a table of zeros would stall it for ever.
#define VECTOR_DEFAULT 0x1348u
static const struct {
    uint8_t index;
    uint32_t value;
} k_vector_exceptions[] = { { 0, 0 },       { 1, 0xd00 },   { 5, 0x1322 },  { 26, 0x3ec0 }, { 27, 0x4552 },
                            { 28, 0x4590 }, { 30, 0x4696 }, { 36, 0x16a6 }, { 37, 0x1968 }, { 38, 0x16b8 } };

uint8_t aps_u8(const ApsModule* module, uint32_t address) {
    if (address < module->size) {
        return module->data[address];
    }
    const uint32_t in_wave = address - module->wave_address;
    if (module->wave != nullptr && in_wave < APS_WAVE_BYTES) {
        return (uint8_t)module->wave[in_wave];
    }
    const uint32_t in_vectors = address - APS_ADDR_VECTORS;
    if (in_vectors < APS_VECTOR_BYTES) {
        uint32_t vector = VECTOR_DEFAULT;
        for (uint32_t i = 0; i < sizeof(k_vector_exceptions) / sizeof(k_vector_exceptions[0]); i++) {
            if (k_vector_exceptions[i].index == in_vectors / 4) {
                vector = k_vector_exceptions[i].value;
            }
        }
        return (uint8_t)(vector >> (8 * (3 - in_vectors % 4)));
    }
    const uint32_t in_record = address - APS_ADDR_SILENT_RECORD;
    return in_record < APS_SAMPLE_RECORD_SIZE ? k_silent_record[in_record] : 0;
}

uint16_t aps_u16(const ApsModule* module, uint32_t address) {
    return (uint16_t)((aps_u8(module, address) << 8) | aps_u8(module, address + 1));
}

static uint32_t aps_u32(const ApsModule* module, uint32_t address) {
    return ((uint32_t)aps_u16(module, address) << 16) | aps_u16(module, address + 2);
}

bool aps_module_parse(ApsModule* module, const uint8_t* data, size_t size, const int8_t* wave) {
    // Check2 looks at the magic only; InitPlayer walks the sections and refuses a file where one ends past the end
    static const uint8_t magic[APS_MAGIC_BYTES] = { 'A', 'D', 'R', 'V', 'P', 'A', 'C', 'K' };
    if (data == nullptr || size < APS_MAGIC_BYTES || size > 0x7fffffff) {
        return false;
    }
    for (uint32_t i = 0; i < APS_MAGIC_BYTES; i++) {
        if (data[i] != magic[i]) {
            return false;
        }
    }
    *module = (ApsModule) { .data = data, .size = (uint32_t)size, .wave = wave };
    // The player's chip allocations follow the file
    module->wave_address = ((((APS_MODULE_BASE + module->size) >> 8) + 1) << 8) + APS_WAVE_GAP - APS_MODULE_BASE;
    uint32_t starts[APS_SECTIONS];
    uint32_t at = APS_MAGIC_BYTES;
    for (uint32_t i = 0; i < APS_SECTIONS; i++) {
        if (at + 4 > size) {
            return false;
        }
        const uint32_t length = aps_u32(module, at);
        starts[i] = at + 4;
        if (length > size - starts[i]) {
            return false;
        }
        at = starts[i] + length;
    }
    module->order = starts[0];
    module->steps = starts[1];
    module->tracks = starts[2];
    module->arpeggios = starts[3];
    module->instruments = starts[4];
    module->envelopes = starts[5];
    module->sample_records = starts[6];
    module->sample_data = starts[7];
    return true;
}

ApsOrderEntry aps_order_entry(const ApsModule* module, uint8_t position) {
    const uint32_t at = module->order + (uint32_t)position * 2;
    const uint8_t first = aps_u8(module, at);
    return (ApsOrderEntry) { .command = first & 0xe0, .count = first & 0x1f, .param = aps_u8(module, at + 1) };
}

ApsStep aps_step(const ApsModule* module, uint32_t step) {
    return (ApsStep) { .duration = aps_u8(module, step), .rows = aps_u8(module, step + 1) };
}

ApsCell aps_cell(const ApsModule* module, uint32_t step, uint32_t voice) {
    const uint32_t at = step + 2 + voice * 6;
    const uint8_t note = aps_u8(module, at);
    const uint8_t volume = aps_u8(module, at + 2);
    return (ApsCell) { .note = note & 0x3f,
                       .restart_env3 = (note & 0x80) != 0,
                       .instrument = aps_u8(module, at + 1),
                       .volume_step = volume >> 6,
                       .volume_target = (uint8_t)(volume << 2),
                       .arpeggio = aps_u8(module, at + 3),
                       .track = aps_u16(module, at + 4) };
}

ApsTrackEntry aps_track_entry(const ApsModule* module, uint32_t entry) {
    const uint8_t note = aps_u8(module, entry);
    const uint8_t level = aps_u8(module, entry + 2);
    return (ApsTrackEntry) { .note = note & 0x3f,
                             .arpeggio = (note & 0x40) != 0,
                             .key = (note & 0x80) != 0,
                             .instrument = aps_u8(module, entry + 1),
                             .instrument_direct = (level & 0x80) != 0,
                             .tempo = (level & 0x40) != 0,
                             .level = level & 0x3f,
                             .param = aps_u8(module, entry + 3) };
}

ApsInstrument aps_instrument(const ApsModule* module, uint32_t instrument) {
    return (ApsInstrument) { .sample = aps_u8(module, instrument),
                             .release_sample = aps_u8(module, instrument + 1),
                             .env_speed = { aps_u8(module, instrument + 3), aps_u8(module, instrument + 0x10),
                                            aps_u8(module, instrument + 4), aps_u8(module, instrument + 5) },
                             .env_offset = { aps_u16(module, instrument + 6), aps_u16(module, instrument + 0xa),
                                             aps_u16(module, instrument + 0xc), aps_u16(module, instrument + 0xe) },
                             .release_env = aps_u16(module, instrument + 8),
                             .env1_modulation = aps_u8(module, instrument + 0x11),
                             .period_offset = (int8_t)aps_u8(module, instrument + 0x12),
                             .pitch_amount_a = (int8_t)aps_u8(module, instrument + 0x13),
                             .pitch_amount_b = (int8_t)aps_u8(module, instrument + 0x14),
                             .pitch_select = aps_u8(module, instrument + 0x18),
                             .volume_base = (int8_t)aps_u8(module, instrument + 0x15),
                             .volume_amount_a = (int8_t)aps_u8(module, instrument + 0x16),
                             .volume_amount_b = (int8_t)aps_u8(module, instrument + 0x17),
                             .volume_select = aps_u8(module, instrument + 0x19) };
}

ApsEnvPoint aps_env_point(const ApsModule* module, uint32_t point) {
    return (ApsEnvPoint) { .duration = aps_u8(module, point), .target = (int8_t)aps_u8(module, point + 1) };
}

ApsSampleRecord aps_sample_record(const ApsModule* module, uint32_t record) {
    const uint32_t head = aps_u32(module, record);
    const uint16_t tail = aps_u16(module, record + 4);
    const uint8_t size = head & 0x3f;
    return (ApsSampleRecord) { .kind = (uint8_t)(head >> 28),
                               .env_a = (head >> 26) & 3,
                               .env_b = (head >> 24) & 3,
                               .filter_inverse = (head & 0x800000) != 0,
                               .filter_reset = (head & 0x400000) != 0,
                               .mode = (head >> 20) & 3,
                               .op = (head >> 16) & 0xf,
                               .filter2_count = (head >> 12) & 0xf,
                               .filter1_count = (head >> 8) & 0xf,
                               .synth_length_words = (uint8_t)head,
                               .filter_offset_a = (head >> 8) & 0xff,
                               .filter_offset_b = (uint8_t)head,
                               .wave_bytes = size == 0 ? APS_WAVE_BUFFER_BYTES : (uint8_t)(size * 2),
                               .address = head & 0xffffff,
                               .length_words = tail,
                               .amount_a = tail >> 8,
                               .amount_b = (uint8_t)tail,
                               .window_length_words = (uint8_t)tail,
                               .filter_gain = tail >> 12 };
}
