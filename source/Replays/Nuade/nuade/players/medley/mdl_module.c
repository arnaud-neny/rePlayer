// SPDX-License-Identifier: MIT
// Medley: the module reader. Every read goes through the typed readers, which know the file's bounds.

#include "mdl_internal.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <string.h>

#define MDL_MAGIC 0x4d534f42u // "MSOB"
// Behind the magic: one long per table, each relative to its own address.
#define MDL_HEADER_BYTES 20u
#define MDL_ENTRY_BYTES 4u
#define MDL_MAX_SIZE 0x100000u

#define MDL_TRACK_HEADER_BYTES 0x10u
#define MDL_WAVE_DATA_OFFSET 0x18u
#define MDL_MODULATOR_BYTES 0x12u

// Notes with a sampled instrument start at this entry of the period table.
#define MDL_SAMPLED_PERIOD_BASE 0x30

// The notes at which the instrument's wave changes: region r covers the notes from mdl_region_start[r] up.
static const uint8_t mdl_region_start[MDL_REGIONS] = { 0x00, 0x1a, 0x26, 0x32, 0x3e, 0x4a, 0x56, 0x62 };

typedef enum MdlSongField {
    MdlSongField_Track = 0x10,
    MdlSongField_Instrument = 0x1a,
    MdlSongField_DefaultInstrument = 0x1e,
    MdlSongField_Transpose = 0x1f,
    MdlSongField_Speed = 0x20,
    MdlSongField_Arpeggio = 0x22,
    MdlSongField_ArpeggioSpeed = 0x23,
    MdlSongField_MasterVolume = 0x24,
    MdlSongField_RandomWaitMin = 0x26,
    MdlSongField_RandomWaitMask = 0x2a,
    MdlSongField_VoiceVolume = 0x2e,
} MdlSongField;

typedef enum MdlInstrumentField {
    MdlInstrumentField_Type = 0x10,
    MdlInstrumentField_Window = 0x11,
    MdlInstrumentField_Phase = 0x12,
    MdlInstrumentField_Transpose = 0x13,
    MdlInstrumentField_Wave = 0x14,
    MdlInstrumentField_Tone = 0x1c,
    MdlInstrumentField_MaxLevel = 0x28,
    MdlInstrumentField_SustainLevel = 0x2a,
    MdlInstrumentField_Attack = 0x2c,
    MdlInstrumentField_Decay = 0x2e,
    MdlInstrumentField_Release = 0x30,
    MdlInstrumentField_Modulators = 0x32,
} MdlInstrumentField;

typedef enum MdlModulatorField {
    MdlModulatorField_Kind = 0x00,
    MdlModulatorField_Gate = 0x01,
    MdlModulatorField_FreeRunning = 0x02,
    MdlModulatorField_OneShot = 0x03,
    MdlModulatorField_Invert = 0x05,
    MdlModulatorField_Delay = 0x06,
    MdlModulatorField_TopStart = 0x07,
    MdlModulatorField_HalfStart = 0x08,
    MdlModulatorField_Level = 0x0c,
    MdlModulatorField_Rise = 0x0e,
    MdlModulatorField_Fall = 0x10,
} MdlModulatorField;

typedef enum MdlWaveField {
    MdlWaveField_Length = 0x10,
    MdlWaveField_Octave = 0x14,
    MdlWaveField_SecondHalf = 0x16,
} MdlWaveField;

uint8_t mdl_u8(MdlModule* module, uint32_t offset) {
    if (offset >= module->size) {
        module->unmapped = true;
        return 0;
    }
    return module->bytes[offset];
}

uint16_t mdl_u16(MdlModule* module, uint32_t offset) {
    if (module->size < 2 || offset > module->size - 2) {
        module->unmapped = true;
        return 0;
    }
    return nuade_read_be16(module->bytes + offset);
}

uint32_t mdl_u32(MdlModule* module, uint32_t offset) {
    if (module->size < 4 || offset > module->size - 4) {
        module->unmapped = true;
        return 0;
    }
    return nuade_read_be32(module->bytes + offset);
}

// Check2 looks for the magic; InitPlayer turns the four offsets into table addresses and keeps the word before the
// song table as the subsong count. The port also wants the whole of every table inside the file.

bool mdl_module_open(MdlModule* module, const uint8_t* data, size_t size) {
    if (size < MDL_HEADER_BYTES || size > MDL_MAX_SIZE || nuade_read_be32(data) != MDL_MAGIC) {
        return false;
    }
    module->size = (uint32_t)size;
    for (uint32_t i = 0; i < MdlTable_Count; i++) {
        const uint32_t at = 4 + i * MDL_ENTRY_BYTES;
        const uint32_t base = at + nuade_read_be32(data + at);
        // The byte before the first entry is the highest index (for the song table the low byte of the count).
        if (base < 2 || base >= size) {
            return false;
        }
        const uint32_t entries = (uint32_t)data[base - 1] + 1;
        if (entries * MDL_ENTRY_BYTES > size - base) {
            return false;
        }
        module->base[i] = base;
    }
    module->subsong_count = nuade_read_be16(data + module->base[MdlTable_Song] - 2);
    return module->subsong_count != 0 && module->subsong_count == data[module->base[MdlTable_Song] - 1];
}

uint32_t mdl_table_target(MdlModule* module, MdlTable table, uint8_t index) {
    const uint32_t entry = module->base[table] + (uint32_t)index * MDL_ENTRY_BYTES;
    const uint32_t relative = mdl_u32(module, entry);
    return relative == 0 ? MDL_NO_OFFSET : entry + relative;
}

bool mdl_song(MdlModule* module, uint8_t number, MdlSong* out) {
    // InitSound refuses a number above the highest index of the table (the subsong count).
    if (number > module->subsong_count) {
        return false;
    }
    const uint32_t at = mdl_table_target(module, MdlTable_Song, number);
    if (at == MDL_NO_OFFSET) {
        return false;
    }
    *out = (MdlSong) { 0 };
    for (uint32_t v = 0; v < MDL_VOICES; v++) {
        out->track[v] = mdl_u8(module, at + MdlSongField_Track + v);
        out->instrument[v] = mdl_u8(module, at + MdlSongField_Instrument + v);
        out->random_wait_min[v] = mdl_u8(module, at + MdlSongField_RandomWaitMin + v);
        out->random_wait_mask[v] = mdl_u8(module, at + MdlSongField_RandomWaitMask + v);
        out->voice_volume[v] = mdl_u8(module, at + MdlSongField_VoiceVolume + v);
    }
    out->default_instrument = mdl_u8(module, at + MdlSongField_DefaultInstrument);
    out->transpose = (int8_t)mdl_u8(module, at + MdlSongField_Transpose);
    out->speed = mdl_u8(module, at + MdlSongField_Speed);
    out->arpeggio = mdl_u8(module, at + MdlSongField_Arpeggio);
    out->arpeggio_speed = mdl_u8(module, at + MdlSongField_ArpeggioSpeed);
    out->master_volume = mdl_u8(module, at + MdlSongField_MasterVolume);
    return !module->unmapped;
}

uint32_t mdl_track_start(MdlModule* module, uint8_t number) {
    const uint32_t at = mdl_table_target(module, MdlTable_Track, number);
    return at == MDL_NO_OFFSET ? MDL_NO_OFFSET : at + MDL_TRACK_HEADER_BYTES;
}

uint32_t mdl_track_from_table_start(MdlModule* module, uint8_t track) {
    const uint32_t relative = mdl_u32(module, module->base[MdlTable_Track] + track * MDL_ENTRY_BYTES);
    return relative == 0 ? MDL_NO_OFFSET : module->base[MdlTable_Track] + relative;
}

uint8_t mdl_random_list_count(MdlModule* module, uint32_t track_start) {
    return mdl_u8(module, track_start + 1);
}

uint8_t mdl_choice_track(MdlModule* module, uint32_t first_pair, uint16_t choice) {
    return mdl_u8(module, first_pair + choice * 2u + 1);
}

static MdlModulatorSetup mdl_modulator(MdlModule* module, uint32_t at) {
    return (MdlModulatorSetup) { .kind = (MdlModulatorKind)mdl_u8(module, at + MdlModulatorField_Kind),
                                 .gate = mdl_u8(module, at + MdlModulatorField_Gate) != 0,
                                 .free_running = mdl_u8(module, at + MdlModulatorField_FreeRunning) != 0,
                                 .one_shot = mdl_u8(module, at + MdlModulatorField_OneShot) != 0,
                                 .invert = mdl_u8(module, at + MdlModulatorField_Invert) != 0,
                                 .delay = mdl_u8(module, at + MdlModulatorField_Delay),
                                 .top_start = mdl_u8(module, at + MdlModulatorField_TopStart),
                                 .half_start = mdl_u8(module, at + MdlModulatorField_HalfStart),
                                 .level = mdl_u16(module, at + MdlModulatorField_Level),
                                 .rise = mdl_u16(module, at + MdlModulatorField_Rise),
                                 .fall = mdl_u16(module, at + MdlModulatorField_Fall) };
}

MdlInstrument mdl_instrument(MdlModule* module, uint32_t offset) {
    MdlInstrument instrument = { 0 };
    instrument.type = (MdlInstrumentType)mdl_u8(module, offset + MdlInstrumentField_Type);
    instrument.window = mdl_u8(module, offset + MdlInstrumentField_Window);
    instrument.phase = mdl_u8(module, offset + MdlInstrumentField_Phase);
    instrument.transpose = (int8_t)mdl_u8(module, offset + MdlInstrumentField_Transpose);
    for (uint32_t i = 0; i < MDL_REGIONS; i++) {
        instrument.wave[i] = mdl_u8(module, offset + MdlInstrumentField_Wave + i);
    }
    instrument.tone = mdl_u8(module, offset + MdlInstrumentField_Tone);
    instrument.max_level = mdl_u16(module, offset + MdlInstrumentField_MaxLevel);
    instrument.sustain_level = mdl_u16(module, offset + MdlInstrumentField_SustainLevel);
    instrument.attack = mdl_u16(module, offset + MdlInstrumentField_Attack);
    instrument.decay = mdl_u16(module, offset + MdlInstrumentField_Decay);
    instrument.release = mdl_u16(module, offset + MdlInstrumentField_Release);
    for (uint32_t i = 0; i < MDL_MODULATORS; i++) {
        instrument.modulator[i]
            = mdl_modulator(module, offset + MdlInstrumentField_Modulators + i * MDL_MODULATOR_BYTES);
    }
    return instrument;
}

uint32_t mdl_instrument_offset(MdlModule* module, uint8_t number) {
    return mdl_table_target(module, MdlTable_Instrument, number);
}

bool mdl_wave(MdlModule* module, uint8_t number, MdlWave* out) {
    const uint32_t at = mdl_table_target(module, MdlTable_Wave, number);
    if (at == MDL_NO_OFFSET) {
        return false;
    }
    out->length = mdl_u16(module, at + MdlWaveField_Length);
    out->octave = mdl_u8(module, at + MdlWaveField_Octave);
    out->second_half = mdl_u8(module, at + MdlWaveField_SecondHalf) != 0;
    out->data = at + MDL_WAVE_DATA_OFFSET;
    return true;
}

uint8_t mdl_note_region(uint8_t note) {
    uint8_t region = MDL_REGIONS - 1;
    while (region > 0 && note < mdl_region_start[region]) {
        region--;
    }
    return region;
}

uint8_t mdl_region_wave(const MdlInstrument* instrument, uint8_t region) {
    uint8_t wave = instrument->wave[region];
    while (wave == 0 && region > 0) {
        wave = instrument->wave[--region];
    }
    return wave;
}

uint8_t mdl_sample_period_index(uint8_t note, const MdlInstrument* instrument, const MdlWave* wave) {
    uint8_t index = (uint8_t)(note - wave->octave * MDL_NOTES_PER_OCTAVE);
    if (instrument->type == MdlInstrumentType_Looped) {
        index = (uint8_t)(index - MDL_NOTES_PER_OCTAVE);
    }
    return (uint8_t)(index + MDL_SAMPLED_PERIOD_BASE);
}

int64_t mdl_window_offset(const MdlWave* wave, bool second_half, uint16_t position) {
    int64_t offset = wave->data;
    if (second_half) {
        offset += (int16_t)wave->length;
    }
    return offset + (position & (uint16_t)((wave->length >> 1) - 1));
}

bool mdl_sample_inside(const MdlModule* module, int64_t offset, uint16_t words) {
    const int64_t bytes = words == 0 ? NUADE_PAULA_MAX_SAMPLE_BYTES : (int64_t)words * 2;
    return offset >= 0 && offset + bytes <= module->size;
}

bool mdl_period_is_call_stack_word(uint32_t index) {
    return index >= MDL_PERIOD_CALL_STACK_INDEX && index < MDL_PERIOD_CALL_STACK_INDEX + MDL_PERIOD_CALL_STACK_WORDS;
}

bool mdl_period_index_modelled(uint32_t index) {
    return index < MDL_PERIOD_COUNT || index == MDL_PERIOD_CALL_DEPTH_INDEX || mdl_period_is_call_stack_word(index);
}

bool mdl_voice_flag_valid(uint8_t flag) {
    return flag <= MDL_VOICE_FLAG_COUNT;
}

uint8_t mdl_song_voice_instrument(const MdlSong* song, int voice) {
    if (song->instrument[voice] != 0) {
        return song->instrument[voice];
    }
    return song->default_instrument != 0 ? song->default_instrument : 1;
}
