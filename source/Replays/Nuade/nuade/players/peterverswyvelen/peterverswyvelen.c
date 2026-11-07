// SPDX-License-Identifier: MIT
// Peter Verswyvelen player: the replayer of the UADE eagleplayer, one function per routine. A tick counts the speed
// down; on the tick it reaches 0 every voice steps one row (and every 16 rows the sequence loads four patterns), on the
// other ticks the voices run their effect. After a row's voices the DMA of the retriggered voices is switched on and
// the repeat registers are written (the eagleplayer waits for the DMA start between the two).
//
// Opening a song runs its sequence in a dry run (rows only, no sound) until the state of the sequencer repeats. A song
// whose replayer would read memory outside the module, make an odd word access (an address error on the 68000) or play
// a sample outside the module is refused, so UADE plays it: the port does not model what lies around the module in
// UADE's memory.

#include <nuade/players/peterverswyvelen.h>

#include "nuade_base.h"
#include "nuade_paula_channel.h"
#include "nuade_utils.h"
#include <stdlib.h>
#include <string.h>

#define PV_SAMPLE_HEADER_BYTES 8u
#define PV_HEADER_BYTES 0x100u // 31 sample headers, then the counts, then the pattern offset table
#define PV_PATTERN_ROWS 16
#define PV_NOTE_BYTES 4u
#define PV_SEQUENCE_ENTRY_BYTES 8 // four pattern references of one word
#define PV_DEFAULT_SPEED 6
#define PV_MAX_VOLUME 0x40
#define PV_PERIOD_MIN 0x71
#define PV_PERIOD_MAX 0x358
#define PV_SEQUENCE_LIMIT 0x1000u // sequence entries: more wrap 16-bit offsets: refused
#define PV_PATTERN_TABLE_LIMIT 0x4000u
#define PV_SEQUENCE_STEP_MASK 0xFFE0 // pattern break and jump work in units of four sequence entries
#define PV_SEQUENCE_STEP 0x20
#define PV_VIBRATO_SIZE 32
#define PV_ARPEGGIO_LAST 36       // the arpeggio's search stops at this table index
#define PV_MODULE_ADDRESS 0xF000u // where UADE loads the module; the arpeggio can read it from the player's variables
#define PV_DRY_RUN_ROWS (1u << 20)
#define PV_PAULA_MAX_WORDS 0x10000u // AUDxLEN 0

// The period table of the arpeggio: 36 periods; the words behind it are read too (see arpeggio_word).
static const uint16_t pv_periods[PV_ARPEGGIO_LAST] = {
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320,
    302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
};

// One cycle of the vibrato; the replayer reads the bytes as unsigned.
static const uint8_t pv_vibrato_table[PV_VIBRATO_SIZE] = {
    0x00, 0x18, 0x31, 0x4A, 0x61, 0x78, 0x8D, 0xA1, 0xB4, 0xC5, 0xD4, 0xE0, 0xEB, 0xF4, 0xFA, 0xFD,
    0xFF, 0xFD, 0xFA, 0xF4, 0xEB, 0xE0, 0xD4, 0xC5, 0xB4, 0xA1, 0x8D, 0x78, 0x61, 0x4A, 0x31, 0x18,
};

// The module layout.

typedef struct PvHeader {
    uint16_t table_count;    // entries of the pattern offset table
    uint16_t sequence_count; // sequence entries
    uint16_t restart_entry;
    uint16_t pattern_bytes; // size of the pattern data
} PvHeader;

typedef struct PvSampleHeader {
    uint16_t length_words;
    uint16_t volume;
    uint16_t loop_start_words; // 0: the sample plays once, then repeats from its start
    uint16_t loop_words;
} PvSampleHeader;

typedef struct PvNote {
    uint8_t sample; // 0: the voice keeps its sample
    bool sounding;
    uint16_t period; // 0: no new note
    PvEffect effect;
    uint8_t duration; // rows the note is repeated
    uint8_t param;
} PvNote;

static uint16_t raw_u16(const uint8_t* data, size_t size, size_t offset) {
    return offset <= size - 2 ? nuade_read_be16(data + offset) : 0;
}

static uint8_t module_u8(PvPlayer* player, int64_t offset) {
    if (offset < 0 || (uint64_t)offset >= player->size) {
        player->fault = true;
        return 0;
    }
    return player->mem[offset];
}

// A word access of the replayer: an odd address is an address error of the 68000.

static uint16_t module_u16(PvPlayer* player, int64_t offset) {
    if ((offset & 1) != 0) {
        player->fault = true;
    }
    return (uint16_t)(((unsigned)module_u8(player, offset) << 8) | module_u8(player, offset + 1));
}

static PvSampleHeader sample_header(const uint8_t* data, size_t size, uint32_t index) {
    const size_t at = (size_t)index * PV_SAMPLE_HEADER_BYTES;
    return (PvSampleHeader) { .length_words = raw_u16(data, size, at),
                              .volume = raw_u16(data, size, at + 2),
                              .loop_start_words = raw_u16(data, size, at + 4),
                              .loop_words = raw_u16(data, size, at + 6) };
}

static PvNote read_note(PvPlayer* player, int32_t offset) {
    const uint8_t first = module_u8(player, offset);
    const uint16_t period_word = module_u16(player, offset);
    const uint8_t effect_byte = module_u8(player, offset + 2);
    return (PvNote) { .sample = first >> 3,
                      .sounding = (first & 4) != 0,
                      .period = period_word & 0x3FF,
                      .effect = (PvEffect)(effect_byte & 0xF),
                      .duration = effect_byte >> 4,
                      .param = module_u8(player, offset + 3) };
}

// The four words behind the sample headers.

static PvHeader read_header(const uint8_t* data, size_t size) {
    const size_t at = (size_t)PV_SAMPLES * PV_SAMPLE_HEADER_BYTES;
    return (PvHeader) { .table_count = raw_u16(data, size, at),
                        .sequence_count = raw_u16(data, size, at + 2),
                        .restart_entry = raw_u16(data, size, at + 4),
                        .pattern_bytes = raw_u16(data, size, at + 6) };
}

// Check2 of the eagleplayer: the sample headers, the counts and the pattern offset table are sane. A word the check
// reads outside the file fails it.

static bool check_header(const uint8_t* data, size_t size, const PvHeader* header) {
    for (uint32_t i = 0; i < PV_SAMPLES; i++) {
        const PvSampleHeader s = sample_header(data, size, i);
        if (s.length_words >= 0x8000 || s.volume > PV_MAX_VOLUME || s.loop_start_words >= 0x8000
            || s.loop_words >= 0x8000) {
            return false;
        }
    }
    if (header->table_count == 0 || header->table_count >= 0x8000 || header->sequence_count == 0
        || header->sequence_count >= 0x8000 || (header->sequence_count & 1) != 0
        || header->sequence_count <= header->restart_entry || header->pattern_bytes == 0
        || header->pattern_bytes >= 0x8000) {
        return false;
    }
    // The offsets rise, stay even and inside the pattern data; the last is compared with the first word behind the
    // table. The replayer's loop counts a 16-bit counter: a single entry runs it 65536 times.
    const uint32_t checks = header->table_count == 1 ? 0x10000u : (uint32_t)header->table_count - 1;
    for (uint32_t i = 0; i < checks; i++) {
        const size_t at = PV_HEADER_BYTES + (size_t)i * 2;
        if (at > size - 4) {
            return false;
        }
        const uint16_t offset = nuade_read_be16(data + at);
        if (offset >= 0x8000 || (offset & 1) != 0 || offset > nuade_read_be16(data + at + 2)
            || offset > header->pattern_bytes) {
            return false;
        }
    }
    return true;
}

// The offsets of the parts of the module, and the InitPlayer test that the samples fit in the file. The counts that
// wrap the replayer's 16-bit address steps are refused, and a pattern size that is odd (every sample would start at an
// odd address, where InitSound's long write is an address error).

static bool parse_layout(const uint8_t* data, size_t size, PvLayout* out) {
    if (size < PV_HEADER_BYTES) {
        return false;
    }
    const PvHeader header = read_header(data, size);
    if (!check_header(data, size, &header) || header.table_count >= PV_PATTERN_TABLE_LIMIT
        || header.sequence_count >= PV_SEQUENCE_LIMIT || (header.pattern_bytes & 1) != 0) {
        return false;
    }
    uint64_t sample_bytes = 0;
    for (uint32_t i = 0; i < PV_SAMPLES; i++) {
        sample_bytes += (uint64_t)sample_header(data, size, i).length_words * 2;
    }
    const uint32_t sequence = PV_HEADER_BYTES + (uint32_t)header.table_count * 2;
    const uint32_t patterns = sequence + (uint32_t)header.sequence_count * PV_SEQUENCE_ENTRY_BYTES;
    const uint32_t samples = patterns + header.pattern_bytes;
    if (samples + sample_bytes > size) {
        return false;
    }
    *out = (PvLayout) { .sequence = sequence,
                        .patterns = patterns,
                        .samples = samples,
                        .sequence_bytes = (int16_t)(header.sequence_count * PV_SEQUENCE_ENTRY_BYTES),
                        .restart_bytes = (int16_t)(header.restart_entry * PV_SEQUENCE_ENTRY_BYTES) };
    return true;
}

static const int8_t* sample_pointer(const PvPlayer* player, int32_t offset) {
    return (const int8_t*)player->mem + offset;
}

// The buffer a Paula block plays must lie inside the module (AUDxLEN 0 plays 65536 words), and be one the module
// defines: a voice that never loaded a sample would play the memory at address 0.

static void check_block(PvPlayer* player, const PvVoice* voice, int32_t start, uint16_t words) {
    const uint64_t length = (words == 0 ? PV_PAULA_MAX_WORDS : words) * 2u;
    if (voice->sample == 0 || start < 0 || (uint64_t)start + length > player->size) {
        player->fault = true;
    }
}

bool pv_start(PvPlayer* player, NuadeVoices* voices) {
    if (player == nullptr) {
        return false;
    }
    player->speed = PV_DEFAULT_SPEED;
    player->speed_counter = 0;
    player->position = 0;
    player->row_counter = 0;
    player->looped = false;
    for (uint32_t i = 0; i < PV_VOICES; i++) {
        player->voices[i] = (PvVoice) { .last_sample = -1 };
        if (voices != nullptr) {
            nuade_voice_stop(voices, i);
        }
    }
    return true;
}

// AUDxVOL: the volume goes through the eagleplayer's volume filter, the identity at the default master volume.

static void write_volume(PvVoice* voice, uint16_t volume) {
    voice->paula.volume = (uint8_t)(volume & 0x7F);
}

// The replayer's SongEnd call.

static void song_end(PvPlayer* player) {
    player->looped = true;
}

// The sample of a note: start, length, volume and what repeats.

static void load_sample(const PvPlayer* player, PvVoice* voice, uint8_t number) {
    voice->sample = number;
    voice->last_sample = number - 1;
    const PvSampleHeader header = sample_header(player->mem, player->size, (uint32_t)number - 1);
    const int32_t start = (int32_t)player->sample_start[number - 1];
    voice->start = start;
    voice->length_words = header.length_words;
    voice->volume = header.volume;
    if (header.loop_start_words != 0) {
        voice->length_words = (uint16_t)(header.loop_start_words + header.loop_words);
        voice->loop_start = start + (int32_t)header.loop_start_words * 2;
    } else {
        voice->loop_start = start;
    }
    voice->loop_words = header.loop_words;
}

// A note starts: DMA off, the sample as the first block; the row's DMA routine starts it.

static void retrigger(PvPlayer* player, PvVoice* voice, uint16_t period) {
    voice->current_period = (int16_t)period;
    voice->vibrato_pos = 0;
    voice->paula.volume = 0;
    voice->paula.period = 1;
    nuade_paula_channel_dma_off(&voice->paula);
    voice->dma_start = true;
    voice->triggered = true;
    check_block(player, voice, voice->start, voice->length_words);
    voice->paula.lc = sample_pointer(player, voice->start);
    voice->paula.len_words = voice->length_words;
}

// The effects that act once, when the row starts.

static void row_effect(PvPlayer* player, PvVoice* voice) {
    voice->volume_row = voice->volume;
    voice->period_row = (uint16_t)voice->current_period;
    switch (voice->effect) {
        case PvEffect_SequenceJump:
            player->position = (int16_t)(((int)voice->param - 1) * PV_SEQUENCE_STEP);
            player->row_counter = 0;
            break;
        case PvEffect_SetVolume:
            voice->volume = voice->param > PV_MAX_VOLUME ? PV_MAX_VOLUME : voice->param;
            write_volume(voice, voice->volume);
            break;
        case PvEffect_PatternBreak: {
            int16_t next = (int16_t)((player->position & PV_SEQUENCE_STEP_MASK) + PV_SEQUENCE_STEP);
            if (next >= player->layout.sequence_bytes) {
                next = player->layout.restart_bytes;
                song_end(player);
            }
            player->position = next;
            player->row_counter = 0;
            break;
        }
        case PvEffect_Filter: // the LED filter
            break;
        case PvEffect_Speed:
            player->speed = voice->param;
            player->speed_counter = voice->param;
            break;
        default:
            break;
    }
}

// A new note, read when the repeat count of the last one has run out.

static void new_note(PvPlayer* player, PvVoice* voice) {
    const PvNote note = read_note(player, voice->note_ptr);
    voice->sounding = note.sounding;
    if (note.sample != 0) {
        load_sample(player, voice, note.sample);
    }
    voice->effect = note.effect;
    voice->duration = (int8_t)note.duration;
    voice->param = note.param;
    voice->param_low = note.param & 0xF;
    voice->param_high = note.param >> 4;
    voice->note_ptr += (int32_t)PV_NOTE_BYTES;
    voice->period = note.period;
    if (note.period == 0) {
        row_effect(player, voice);
        return;
    }
    voice->last_period = note.period;
    if (note.effect == PvEffect_TonePortamento) {
        voice->porta_target = note.period;
        voice->porta_down = (int16_t)note.period < voice->current_period;
        voice->porta_speed = note.param;
        voice->dma_start = true;
        return;
    }
    retrigger(player, voice, note.period);
    row_effect(player, voice);
}

// One row of one voice.

static void voice_row(PvPlayer* player, PvVoice* voice) {
    voice->duration--;
    if (voice->duration < 0) {
        new_note(player, voice);
        return;
    }
    if (!voice->sounding) {
        voice->period = 0;
        voice->effect = PvEffect_Arpeggio; // clears the effect byte and the parameter byte behind it
        voice->param = 0;
        return;
    }
    if (voice->period == 0) {
        return;
    }
    // A repeated note starts again, except under a tone portamento
    voice->volume = voice->volume_row;
    voice->current_period = (int16_t)voice->period_row;
    if (voice->effect != PvEffect_TonePortamento) {
        retrigger(player, voice, voice->period);
        row_effect(player, voice);
    }
}

// A word of the arpeggio's period table, which is read past its end (up to 15 words behind the 36 periods): the table
// is followed by two zero words and a word of the eagleplayer's code, then by the 8 bytes between the player's two
// hunks and the player's variables, which are modelled by their current values.

enum {
    PvArpeggio_Code = 38,       // the word of the eagleplayer's code behind the table
    PvArpeggio_ModuleHigh = 43, // the player's variables start here, after the gap between the hunks
    PvArpeggio_ModuleLow,
    PvArpeggio_Unused,
    PvArpeggio_Speed, // speed and speed counter
    PvArpeggio_Position,
    PvArpeggio_SequenceBytes,
    PvArpeggio_RowCounter,
    PvArpeggio_DmaMask,
    PvArpeggio_SampleHigh,
};

static uint16_t arpeggio_word(const PvPlayer* player, uint32_t index) {
    switch (index) {
        case PvArpeggio_Code:
            return 0x5754;
        case PvArpeggio_ModuleHigh:
            return (uint16_t)(PV_MODULE_ADDRESS >> 16); // the module address
        case PvArpeggio_ModuleLow:
            return (uint16_t)PV_MODULE_ADDRESS;
        case PvArpeggio_Speed:
            return (uint16_t)((player->speed << 8) | player->speed_counter);
        case PvArpeggio_Position:
            return (uint16_t)player->position;
        case PvArpeggio_SequenceBytes:
            return (uint16_t)player->layout.sequence_bytes;
        case PvArpeggio_RowCounter:
            return (uint16_t)player->row_counter;
        case PvArpeggio_DmaMask: { // the voices whose DMA the row starts
            uint16_t mask = 0;
            for (uint32_t i = 0; i < PV_VOICES; i++) {
                mask |= (uint16_t)(player->voices[i].dma_start ? 1u << i : 0);
            }
            return mask;
        }
        case PvArpeggio_SampleHigh: // the first sample's address, high word
            return (uint16_t)((PV_MODULE_ADDRESS + player->sample_start[0]) >> 16);
        default:
            return index < PV_ARPEGGIO_LAST ? pv_periods[index] : 0;
    }
}

// The period the arpeggio plays: the first table entry not above the voice's period, `steps` semitones further.

static uint16_t arpeggio_period(const PvPlayer* player, int16_t period, uint8_t steps) {
    uint32_t index = 0;
    while (index < PV_ARPEGGIO_LAST && period < (int16_t)pv_periods[index]) {
        index++;
    }
    return arpeggio_word(player, index + steps);
}

static void arpeggio(const PvPlayer* player, PvVoice* voice) {
    if (voice->param == 0) {
        return;
    }
    const uint8_t phase = (uint8_t)(player->speed - player->speed_counter);
    uint16_t period = (uint16_t)voice->current_period;
    if (phase != 0 && phase != 3) {
        period = arpeggio_period(player, voice->current_period,
                                 phase == 2 || phase == 5 ? voice->param_low : voice->param_high);
    }
    voice->paula.period = period;
}

static void volume_slide(PvVoice* voice) {
    int volume = voice->volume;
    if (voice->param_high != 0) {
        volume += voice->param_high;
        if (volume > PV_MAX_VOLUME) {
            volume = PV_MAX_VOLUME;
        }
    } else {
        volume -= voice->param_low;
        if (volume < 0) {
            volume = 0;
        }
    }
    voice->volume = (uint16_t)volume;
    write_volume(voice, voice->volume);
}

static void tone_portamento(PvVoice* voice) {
    if (voice->param != 0) {
        voice->porta_speed = voice->param;
    }
    int period = voice->current_period;
    const int target = (int16_t)voice->porta_target;
    if (period == target) {
        return;
    }
    if (!voice->porta_down) {
        period += voice->porta_speed;
        if (period >= target) {
            period = target;
        }
    } else {
        period -= voice->porta_speed;
        if (period <= target) {
            period = target;
        }
    }
    voice->current_period = (int16_t)period;
    voice->paula.period = (uint16_t)period;
}

static void vibrato(PvVoice* voice) {
    if (voice->param != 0) {
        voice->vibrato_speed = voice->param >> 4;
        voice->vibrato_depth = voice->param & 0xF;
    }
    const uint32_t swing = (uint32_t)pv_vibrato_table[(voice->vibrato_pos & 0x7F) >> 2] * voice->vibrato_depth >> 6;
    voice->paula.period = (uint16_t)((uint16_t)voice->current_period + swing);
    voice->vibrato_pos = (uint8_t)(voice->vibrato_pos + voice->vibrato_speed * 4);
}

// The effect of a voice on a tick that does not step a row.

static void voice_effect(PvPlayer* player, PvVoice* voice) {
    switch (voice->effect) {
        case PvEffect_Arpeggio:
            arpeggio(player, voice);
            break;
        case PvEffect_VolumeSlide:
            volume_slide(voice);
            break;
        case PvEffect_SlideUp: {
            int period = voice->current_period - voice->param;
            voice->current_period = (int16_t)(period < PV_PERIOD_MIN ? PV_PERIOD_MIN : period);
            voice->paula.period = (uint16_t)voice->current_period;
            break;
        }
        case PvEffect_SlideDown: {
            int period = voice->current_period + voice->param;
            voice->current_period = (int16_t)(period > PV_PERIOD_MAX ? PV_PERIOD_MAX : period);
            voice->paula.period = (uint16_t)voice->current_period;
            break;
        }
        case PvEffect_TonePortamento:
            tone_portamento(voice);
            break;
        case PvEffect_Vibrato:
            vibrato(voice);
            break;
        default:
            break;
    }
}

// Every 16 rows: the sequence entry names the pattern of each voice.

static void load_patterns(PvPlayer* player) {
    player->row_counter = PV_PATTERN_ROWS;
    const int32_t entry = (int32_t)player->layout.sequence + player->position;
    int16_t next = (int16_t)(player->position + PV_SEQUENCE_ENTRY_BYTES);
    if (next >= player->layout.sequence_bytes) {
        next = player->layout.restart_bytes;
        song_end(player);
    }
    player->position = next;
    for (uint32_t i = 0; i < PV_VOICES; i++) {
        const int16_t table_index = (int16_t)module_u16(player, entry + (int32_t)i * 2);
        const int16_t pattern = (int16_t)module_u16(player, (int32_t)PV_HEADER_BYTES + table_index);
        player->voices[i].note_ptr = (int32_t)player->layout.patterns + pattern;
    }
}

// The DMA of the voices that started a note or a tone portamento, then the repeat registers, period and volume.

static void start_dma(PvPlayer* player) {
    for (uint32_t i = 0; i < PV_VOICES; i++) {
        if (player->voices[i].dma_start) {
            nuade_paula_channel_dma_on(&player->voices[i].paula);
        }
    }
    for (uint32_t i = 0; i < PV_VOICES; i++) {
        PvVoice* voice = &player->voices[i];
        if (voice->dma_start) {
            // DMA on a voice no note has started plays whatever the Paula registers hold
            player->fault = player->fault || !voice->triggered;
            check_block(player, voice, voice->loop_start, voice->loop_words);
            voice->paula.lc = sample_pointer(player, voice->loop_start);
            voice->paula.len_words = voice->loop_words;
            voice->paula.period = (uint16_t)voice->current_period;
            write_volume(voice, voice->volume);
        }
    }
}

// The tick that reaches the end of the speed count: every 16 rows the patterns load, then each voice steps a row and
// the DMA of the new notes starts.

static void step_row(PvPlayer* player) {
    player->speed_counter = player->speed;
    for (uint32_t i = 0; i < PV_VOICES; i++) {
        player->voices[i].dma_start = false;
    }
    player->row_counter--;
    if (player->row_counter <= 0) {
        load_patterns(player);
    }
    for (uint32_t i = 0; i < PV_VOICES; i++) {
        voice_row(player, &player->voices[i]);
    }
    start_dma(player);
}

PvTickResult pv_tick(PvPlayer* player, NuadeVoices* voices) {
    player->looped = false;
    // subq.b / bgt: the counter is a signed byte
    const int counter = (int8_t)player->speed_counter - 1;
    player->speed_counter = (uint8_t)counter;
    if (counter > 0) {
        for (uint32_t i = 0; i < PV_VOICES; i++) {
            voice_effect(player, &player->voices[i]);
        }
    } else {
        step_row(player);
    }
    for (uint32_t i = 0; i < PV_VOICES; i++) {
        nuade_paula_channel_sync(&player->voices[i].paula, i, voices);
    }
    return (PvTickResult) { .looped = player->looped };
}

// What decides which notes the sequencer reads: the position, the row count and, per voice, the note being played.

typedef struct PvFlow {
    int16_t position;
    int16_t row_counter;
    struct {
        int32_t note_ptr;
        int8_t duration;
        uint8_t sample;
        bool sounding;
        bool has_period;
        PvEffect effect;
        uint8_t param;
    } voices[PV_VOICES];
} PvFlow;

static PvFlow flow_of(const PvPlayer* player) {
    PvFlow flow = { .position = player->position, .row_counter = player->row_counter };
    for (uint32_t i = 0; i < PV_VOICES; i++) {
        const PvVoice* v = &player->voices[i];
        flow.voices[i].note_ptr = v->note_ptr;
        flow.voices[i].duration = v->duration;
        flow.voices[i].sample = v->sample;
        flow.voices[i].sounding = v->sounding;
        flow.voices[i].has_period = v->period != 0;
        flow.voices[i].effect = v->effect;
        flow.voices[i].param = v->param;
    }
    return flow;
}

static bool same_flow(const PvFlow* a, const PvFlow* b) {
    if (a->position != b->position || a->row_counter != b->row_counter) {
        return false;
    }
    for (uint32_t i = 0; i < PV_VOICES; i++) {
        if (a->voices[i].note_ptr != b->voices[i].note_ptr || a->voices[i].duration != b->voices[i].duration
            || a->voices[i].sample != b->voices[i].sample || a->voices[i].sounding != b->voices[i].sounding
            || a->voices[i].has_period != b->voices[i].has_period || a->voices[i].effect != b->voices[i].effect
            || a->voices[i].param != b->voices[i].param) {
            return false;
        }
    }
    return true;
}

// Plays the song row by row without sound until the sequencer state repeats (Brent's cycle search). Every module read,
// word access and sample buffer of the song's whole run goes through the checks that set `fault`. A song that has not
// repeated within PV_DRY_RUN_ROWS rows is refused.

static bool dry_run_is_clean(PvPlayer* player) {
    pv_start(player, nullptr);
    PvFlow tortoise = flow_of(player);
    uint32_t power = 1;
    uint32_t steps = 0;
    for (uint32_t row = 0; row < PV_DRY_RUN_ROWS; row++) {
        step_row(player);
        if (player->fault) {
            return false;
        }
        const PvFlow hare = flow_of(player);
        if (same_flow(&tortoise, &hare)) {
            return true;
        }
        if (++steps == power) {
            tortoise = hare;
            power *= 2;
            steps = 0;
        }
    }
    return false;
}

// InitSound's module part: the sample addresses (the replayer adds them with 16-bit steps), and the first four bytes
// of every sample are cleared (the repeat of a sample that plays once is then silent).

PvPlayer* pv_create(const uint8_t* data, size_t size) {
    PvLayout layout;
    if (data == nullptr || !parse_layout(data, size, &layout)) {
        return nullptr;
    }
    PvPlayer* player = (PvPlayer*)calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    player->mem = nuade_paula_padded_copy(data, size);
    if (player->mem == nullptr) {
        free(player);
        return nullptr;
    }
    player->size = size;
    player->layout = layout;
    int32_t at = (int32_t)layout.samples;
    for (uint32_t i = 0; i < PV_SAMPLES; i++) {
        // An odd start is an address error, one outside the module would clear the player's memory
        if (at < 0 || (uint32_t)at > size || (at & 1) != 0) {
            pv_destroy(player);
            return nullptr;
        }
        player->sample_start[i] = (uint32_t)at;
        memset(player->mem + at, 0, 4);
        at += (int16_t)(uint16_t)(sample_header(player->mem, size, i).length_words * 2u);
    }
    if (!dry_run_is_clean(player)) {
        pv_destroy(player);
        return nullptr;
    }
    pv_start(player, nullptr);
    return player;
}

void pv_destroy(PvPlayer* player) {
    if (player != nullptr) {
        free(player->mem);
        free(player);
    }
}

bool pv_probe(const uint8_t* data, size_t size) {
    PvPlayer* player = pv_create(data, size);
    const bool playable = player != nullptr;
    pv_destroy(player);
    return playable;
}

uint32_t pv_grid_ticks(const PvPlayer* player) {
    return player->speed != 0 ? player->speed : 1;
}

PvVoiceState pv_voice_state(const PvPlayer* player, uint32_t voice) {
    const PvVoice* v = &player->voices[voice];
    return (PvVoiceState) { .period = v->last_period, .instrument = v->last_sample };
}
