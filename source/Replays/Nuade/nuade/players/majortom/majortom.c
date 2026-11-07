// SPDX-License-Identifier: MIT
// Major Tom: the replayer of the UADE eagleplayer "MajorTom". One tick runs the row sequencer every `speed` ticks and
// then, for every voice that did not start a note in this tick, its four sequences (waveform, note offset, period
// offset, volume); the DMA of all voices is switched on at the end of the tick.

#include <nuade/players/majortom.h>

#include "mt_internal.h"
#include "nuade_paula_channel.h"
#include <stdlib.h>

// The first row is read in the fourth tick.
#define MT_FIRST_ROW_TICKS 4
// The replayer's data hunk as UADE loads it, directly behind the code hunk's last word (the period table's entry 89):
// four voice records, the noise table, then a block of zeros. The period table read can run into it.
#define MT_DATA_BASE 0xeb98u
#define MT_DATA_ENTRY 91
#define MT_NOTES_PER_OCTAVE 12
#define MT_RECORD_BYTES 0x78
// The zero bytes behind the noise table, up to the end of the data hunk.
#define MT_DATA_ZERO_BYTES 68
#define MT_NOISE_OFFSET 0x1e0
#define MT_ZERO_BLOCK_OFFSET 0xd9c

// The fields of a voice record, by their offset in the data hunk (the unnamed offsets hold zeros).
typedef enum MtRecordField {
    MtRecord_Note = 0x00,
    MtRecord_Effect = 0x02,
    MtRecord_Param = 0x03,
    MtRecord_BlockPointers = 0x04, // four longs
    MtRecord_Indexes = 0x14,       // four longs
    MtRecord_Noise = 0x24,         // word
    MtRecord_Timers = 0x28,        // four bytes
    MtRecord_Rates = 0x2c,         // four bytes
    MtRecord_Volume = 0x32,        // word
    MtRecord_Slide = 0x3c,         // word
    MtRecord_Current = 0x46,       // word
    MtRecord_Target = 0x48,        // word
    MtRecord_Direction = 0x4c,     // word
    MtRecord_NewNote = 0x50,
    MtRecord_NoteOffset = 0x5a,
    MtRecord_SampleAddress = 0x64, // long
    MtRecord_PeriodCopy = 0x68,    // word, a second copy of the target
    MtRecord_NoteOffsetUsed = 0x6e,
} MtRecordField;

// The Paula registers the replayer writes by itself.
#define MT_IDLE_PERIOD 200
#define MT_IDLE_WORDS 2
#define MT_WAVE_WORDS 16
#define MT_NOISE_WORDS 700
#define MT_NOISE_PERIOD 200

#define MT_WAVE_NOISE 0xff
// The long at this offset of a waveform sequence block is what the jump of that sequence adds to the block address.
#define MT_WAVE_JUMP_LONG 0x18
// A sequence index that reaches this value is held at the last step of its block.
#define MT_INDEX_END 0x3f

// The tick count within which the run that checks a song must see its state repeat.
#define MT_CHECK_TICKS 100000

typedef enum MtEffect {
    MtEffect_None = 0,
    MtEffect_SlideUp = 1,
    MtEffect_SlideDown = 2,
    MtEffect_Portamento = 3,
    MtEffect_PatternBreak = 0xd,
    MtEffect_Speed = 0xf,
} MtEffect;

static const uint8_t mt_hold_index[MT_SEQUENCES] = { 0x3c, 0x3e, 0x3e, 0x3c };

static bool mt_run_song(MtPlayer* player);

MtPlayer* mt_create(const uint8_t* data, size_t size) {
    if (!mt_module_check(data, size)) {
        return nullptr;
    }
    MtPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    player->module = (MtModule) { .bytes = nuade_paula_padded_copy(data, size), .size = (uint32_t)size };
    player->noise = nuade_paula_padded_copy(mt_noise_table, MT_NOISE_BYTES);
    if (player->module.bytes == nullptr || player->noise == nullptr) {
        mt_destroy(player);
        return nullptr;
    }
    player->file_speed = mt_speed(&player->module);
    if (!mt_run_song(player)) {
        mt_destroy(player);
        return nullptr;
    }
    return player;
}

void mt_destroy(MtPlayer* player) {
    if (player != nullptr) {
        free(player->module.bytes);
        free(player->noise);
        free(player);
    }
}

bool mt_probe(const uint8_t* data, size_t size) {
    MtPlayer* player = mt_create(data, size);
    mt_destroy(player);
    return player != nullptr;
}

// InitSound: Paula idles on a short silent sample, the voices read the zero block until their first note, and the
// first row is read in the fourth tick.

static void mt_reset(MtPlayer* player) {
    mt_set_speed(&player->module, player->file_speed);
    player->speed_counter = MT_FIRST_ROW_TICKS;
    player->position = 0;
    player->row_offset = 0;
    player->unmapped = false;
    player->ended = false;
    for (int ch = 0; ch < MT_VOICES; ch++) {
        MtVoice* voice = &player->voices[ch];
        *voice = (MtVoice) { .shown_instrument = -1 };
        voice->paula = (NuadePaulaChannel) { .lc = (const int8_t*)player->noise + MT_NOISE_BYTES,
                                             .len_words = MT_IDLE_WORDS,
                                             .period = MT_IDLE_PERIOD };
        for (int s = 0; s < MT_SEQUENCES; s++) {
            voice->sequence[s] = (MtSequenceState) { .block = { .zeros = true }, .index = 1, .hold = mt_hold_index[s] };
        }
    }
}

bool mt_start(MtPlayer* player, NuadeVoices* voices) {
    if (player == nullptr || voices == nullptr) {
        return false;
    }
    mt_reset(player);
    for (uint32_t ch = 0; ch < MT_VOICES; ch++) {
        nuade_voice_stop(voices, ch);
    }
    return true;
}

// The step a sequence is on, following a jump to the step the next byte names.

static uint8_t mt_step(MtPlayer* player, MtSequenceState* sequence) {
    uint8_t step = mt_block_step(&player->module, sequence->block, sequence->index, &player->unmapped);
    if (step == MT_STEP_JUMP) {
        sequence->index
            = mt_block_step(&player->module, sequence->block, (int64_t)sequence->index + 1, &player->unmapped);
        step = mt_block_step(&player->module, sequence->block, sequence->index, &player->unmapped);
    }
    return step;
}

static void mt_next_step(MtSequenceState* sequence) {
    sequence->index++;
    if (sequence->index == MT_INDEX_END) {
        sequence->index = sequence->hold;
    }
}

// True when the sequence has a step to play in this tick; it reloads the timer.

static bool mt_sequence_due(MtSequenceState* sequence) {
    sequence->timer--;
    if (sequence->timer != 0) {
        return false;
    }
    sequence->timer = sequence->rate;
    return true;
}

// The noise table is read from the offset the volume sequence's index gives; the zeros after the table are what the
// eagleplayer's data holds behind it.

static void mt_start_noise(MtPlayer* player, MtVoice* voice) {
    const uint64_t offset = (uint64_t)voice->sequence[MtSequence_Volume].index * 2;
    if (offset + MT_NOISE_WORDS * 2 > MT_NOISE_BYTES + MT_DATA_ZERO_BYTES) {
        player->unmapped = true;
        return;
    }
    nuade_paula_channel_dma_off(&voice->paula);
    voice->paula.lc = (const int8_t*)player->noise + offset;
    voice->sample_address = MT_DATA_BASE + MT_NOISE_OFFSET + (uint32_t)offset;
    voice->paula.len_words = MT_NOISE_WORDS;
    voice->paula.period = MT_NOISE_PERIOD;
    voice->noise = true;
}

// The waveform sequence. Its jump does not read the step at the new index: the replayer adds the long at offset 0x18
// of the block to the block address (a typo for the voice record) and reads the step there.

static uint8_t mt_wave_step(MtPlayer* player, MtSequenceState* sequence, bool* jumped) {
    uint8_t step = mt_block_step(&player->module, sequence->block, sequence->index, &player->unmapped);
    *jumped = step == MT_STEP_JUMP;
    if (!*jumped) {
        return step;
    }
    sequence->index = mt_block_step(&player->module, sequence->block, (int64_t)sequence->index + 1, &player->unmapped);
    const uint32_t add = mt_read_long(&player->module, sequence->block.offset + MT_WAVE_JUMP_LONG, &player->unmapped);
    const uint32_t address = (uint32_t)(MT_SONG_BASE + sequence->block.offset) + add;
    return mt_read_byte(&player->module, (int64_t)address - MT_SONG_BASE, &player->unmapped);
}

// A step of 0 at the block index keeps the waveform; the step a jump lands on selects a waveform even when it is 0.

static void mt_run_wave_sequence(MtPlayer* player, MtVoice* voice) {
    MtSequenceState* sequence = &voice->sequence[MtSequence_Wave];
    if (!mt_sequence_due(sequence)) {
        return;
    }
    bool jumped;
    const uint8_t step = mt_wave_step(player, sequence, &jumped);
    if (step == MT_WAVE_NOISE) {
        mt_start_noise(player, voice);
    } else if (step != 0 || jumped) {
        if (voice->noise) {
            nuade_paula_channel_dma_off(&voice->paula);
            voice->noise = false;
        }
        voice->paula.lc = (const int8_t*)player->module.bytes + mt_waveform_offset(step);
        voice->sample_address = MT_SONG_BASE + mt_waveform_offset(step);
    }
    mt_next_step(sequence);
}

static void mt_run_note_sequence(MtPlayer* player, MtVoice* voice) {
    MtSequenceState* sequence = &voice->sequence[MtSequence_Note];
    if (mt_sequence_due(sequence)) {
        voice->note_offset = mt_step(player, sequence);
        mt_next_step(sequence);
    }
}

// The Amiga address of a sequence block.

static uint32_t mt_block_address(MtBlock block) {
    return block.zeros ? MT_DATA_BASE + MT_ZERO_BLOCK_OFFSET : (uint32_t)(MT_SONG_BASE + block.offset);
}

// Byte `index` (0 = most significant) of a big-endian value of `size` bytes.

static uint8_t mt_value_byte(uint32_t value, uint32_t size, uint32_t index) {
    return (uint8_t)(value >> (8 * (size - 1 - index)));
}

// A byte of the replayer's voice records (its data hunk). The period table read can run past the table, through the
// allocation gap behind the code, into the records.

static uint8_t mt_record_byte(const MtPlayer* player, uint32_t address) {
    const MtVoice* voice = &player->voices[address / MT_RECORD_BYTES];
    const uint32_t field = address % MT_RECORD_BYTES;
    if (field >= MtRecord_BlockPointers && field < MtRecord_BlockPointers + 4 * MT_SEQUENCES) {
        const uint32_t sequence = (field - MtRecord_BlockPointers) / 4;
        return mt_value_byte(mt_block_address(voice->sequence[sequence].block), 4, field % 4);
    }
    if (field >= MtRecord_Indexes && field < MtRecord_Indexes + 4 * MT_SEQUENCES) {
        return mt_value_byte(voice->sequence[(field - MtRecord_Indexes) / 4].index, 4, field % 4);
    }
    if (field >= MtRecord_Timers && field < MtRecord_Timers + MT_SEQUENCES) {
        return voice->sequence[field - MtRecord_Timers].timer;
    }
    if (field >= MtRecord_Rates && field < MtRecord_Rates + MT_SEQUENCES) {
        return voice->sequence[field - MtRecord_Rates].rate;
    }
    switch (field) {
        case MtRecord_Note:
            return voice->note;
        case MtRecord_Effect:
            return voice->effect;
        case MtRecord_Param:
            return voice->param;
        case MtRecord_Noise + 1:
            return voice->noise ? 1 : 0;
        case MtRecord_Volume:
        case MtRecord_Volume + 1:
            return mt_value_byte(voice->volume_step, 2, field - MtRecord_Volume);
        case MtRecord_Slide:
        case MtRecord_Slide + 1:
            return mt_value_byte((uint16_t)voice->slide, 2, field - MtRecord_Slide);
        case MtRecord_Current:
        case MtRecord_Current + 1:
            return mt_value_byte(voice->current, 2, field - MtRecord_Current);
        case MtRecord_Target:
        case MtRecord_Target + 1:
        case MtRecord_PeriodCopy:
        case MtRecord_PeriodCopy + 1:
            return mt_value_byte(voice->target, 2, field & 1);
        case MtRecord_Direction + 1:
            return (uint8_t)voice->direction;
        case MtRecord_NewNote:
            return voice->new_note ? 1 : 0;
        case MtRecord_NoteOffset:
            return voice->note_offset;
        case MtRecord_NoteOffsetUsed:
            return voice->note_offset_used;
        case MtRecord_SampleAddress:
        case MtRecord_SampleAddress + 1:
        case MtRecord_SampleAddress + 2:
        case MtRecord_SampleAddress + 3:
            return mt_value_byte(voice->sample_address, 4, field - MtRecord_SampleAddress);
        default:
            return 0;
    }
}

// The word `entry` words after the start of the period table: the table, the gap of the memory allocation, the data.

static uint16_t mt_table_word(MtPlayer* player, uint32_t entry) {
    if (entry < MT_PERIOD_WORDS) {
        return mt_period_words[entry];
    }
    if (entry < MT_DATA_ENTRY) {
        return 0;
    }
    const uint32_t address = (entry - MT_DATA_ENTRY) * 2;
    if (address < MT_VOICES * MT_RECORD_BYTES) {
        return (uint16_t)((mt_record_byte(player, address) << 8) | mt_record_byte(player, address + 1));
    }
    if (address >= MT_NOISE_OFFSET && address - MT_NOISE_OFFSET + 1 < MT_NOISE_BYTES) {
        const uint8_t* noise = mt_noise_table + (address - MT_NOISE_OFFSET);
        return (uint16_t)((noise[0] << 8) | noise[1]);
    }
    if (address >= MT_NOISE_OFFSET + MT_NOISE_BYTES
        && address - MT_NOISE_OFFSET - MT_NOISE_BYTES + 1 < MT_DATA_ZERO_BYTES) {
        return 0;
    }
    player->unmapped = true;
    return 0;
}

// The period of the note plus the note sequence's offset, read from the table by octave and semitone.

static uint16_t mt_note_period(MtPlayer* player, MtVoice* voice) {
    const uint8_t semitone = (uint8_t)((voice->note & 0xf) + (voice->note_offset & 0xf));
    const uint8_t octaves = (uint8_t)((uint8_t)(voice->note - 0x10) / 16 + voice->note_offset / 16);
    voice->note_offset_used = voice->note_offset;
    return mt_table_word(player, (uint32_t)octaves * MT_NOTES_PER_OCTAVE + semitone);
}

// The portamento effect moves `current` to `target` by `param` per tick; the period it returns is the one reached.

static uint16_t mt_portamento(MtVoice* voice) {
    if (voice->direction == MtDirection_None) {
        if (voice->effect != MtEffect_Portamento) {
            return voice->target;
        }
        voice->direction = (int16_t)(voice->target - voice->current) < 0 ? MtDirection_Down : MtDirection_Up;
    }
    bool arrived;
    if (voice->direction == MtDirection_Up) {
        voice->current = (uint16_t)(voice->current + voice->param);
        arrived = (int16_t)(voice->current - voice->target) >= 0;
    } else {
        voice->current = (uint16_t)(voice->current - voice->param);
        arrived = (int16_t)(voice->target - voice->current) >= 0;
    }
    if (arrived) {
        voice->current = voice->target;
    }
    return voice->current;
}

// The pattern break effect, run by the voice: the next row read is the first of the next position.

static void mt_pattern_break(MtPlayer* player, MtVoice* voice) {
    voice->effect = MtEffect_None;
    player->row_offset = 0;
    player->position++;
    if (player->position >= mt_song_length(&player->module)) {
        player->position = 0;
    }
}

static void mt_run_period_sequence(MtPlayer* player, MtVoice* voice, uint16_t period) {
    MtSequenceState* sequence = &voice->sequence[MtSequence_Period];
    if (!mt_sequence_due(sequence)) {
        return;
    }
    const uint8_t step = mt_step(player, sequence);
    period = (uint16_t)(step < 0x10 ? period + step : period - (step & 0xf));
    voice->paula.period = period;
    if (!voice->noise) {
        voice->paula.len_words = MT_WAVE_WORDS;
    }
    mt_next_step(sequence);
}

// The volume register: the eagleplayer's master volume and balance scaling is the identity at full volume.

static void mt_run_volume_sequence(MtPlayer* player, MtVoice* voice) {
    MtSequenceState* sequence = &voice->sequence[MtSequence_Volume];
    if (mt_sequence_due(sequence)) {
        voice->volume_step = mt_step(player, sequence);
        voice->paula.volume = voice->volume_step;
        mt_next_step(sequence);
    }
}

static void mt_run_voice(MtPlayer* player, MtVoice* voice) {
    mt_run_wave_sequence(player, voice);
    mt_run_note_sequence(player, voice);
    voice->target = mt_note_period(player, voice);
    uint16_t period = mt_portamento(voice);
    if (voice->effect == MtEffect_PatternBreak) {
        mt_pattern_break(player, voice);
    }
    if (voice->effect == MtEffect_SlideDown) {
        voice->slide = (int16_t)(voice->slide + voice->param);
    }
    if (voice->effect == MtEffect_SlideUp) {
        voice->slide = (int16_t)(voice->slide - voice->param);
    }
    mt_run_period_sequence(player, voice, (uint16_t)(period + voice->slide));
    mt_run_volume_sequence(player, voice);
}

// A note with an instrument restarts the four sequences at their first step.

static void mt_start_instrument(MtPlayer* player, MtVoice* voice, uint8_t number) {
    const MtInstrument instrument = mt_instrument(&player->module, number);
    for (int s = 0; s < MT_SEQUENCES; s++) {
        MtSequenceState* sequence = &voice->sequence[s];
        sequence->block = mt_block((MtSequence)s, instrument.block[s]);
        sequence->index = 1;
        sequence->timer = 1;
        sequence->rate = mt_block_step(&player->module, sequence->block, 0, &player->unmapped);
    }
    voice->slide = 0;
    voice->note_offset = 1;
    voice->new_note = true;
    voice->shown_instrument = number;
}

// A note byte of 0 leaves the voice alone; a note without an instrument changes the pitch of the sounding one. The
// effect of the cell is taken in every case, the speed effect at once.

static void mt_read_cell(MtPlayer* player, int ch) {
    MtVoice* voice = &player->voices[ch];
    const MtCell row = mt_row(&player->module, mt_order_pattern(&player->module, player->position),
                              player->row_offset / MT_ROW_BYTES, ch, &player->unmapped);
    const bool starts_note = row.note != 0;
    if (starts_note) {
        voice->note = (uint8_t)(row.note + mt_transpose(&player->module, player->position));
        voice->current = voice->target;
        voice->direction = MtDirection_None;
    }
    voice->effect = row.effect;
    voice->param = row.param;
    if (starts_note && row.instrument != 0) {
        mt_start_instrument(player, voice, row.instrument);
    }
    if (voice->effect == MtEffect_Speed) {
        voice->effect = MtEffect_None;
        mt_set_speed(&player->module, voice->param);
    }
}

// Reads the four cells of the row and moves to the next row. Returns true when the song passed its last position.

static bool mt_run_row(MtPlayer* player) {
    for (int ch = 0; ch < MT_VOICES; ch++) {
        mt_read_cell(player, ch);
    }
    player->row_offset += MT_ROW_BYTES;
    if (player->row_offset != MT_PATTERN_BYTES) {
        return false;
    }
    player->row_offset = 0;
    player->position++;
    if (player->position != mt_song_length(&player->module)) {
        return false;
    }
    player->position = mt_restart(&player->module);
    return true;
}

// One tick of the replayer, up to the DMA switch-on at its end. Returns true when the song looped.

static bool mt_run_tick(MtPlayer* player) {
    bool looped = false;
    player->speed_counter--;
    if (player->speed_counter == 0) {
        player->speed_counter = mt_speed(&player->module);
        looped = mt_run_row(player);
    }
    for (int ch = 0; ch < MT_VOICES; ch++) {
        MtVoice* voice = &player->voices[ch];
        if (!voice->new_note) {
            mt_run_voice(player, voice);
        }
        voice->new_note = false;
    }
    return looped;
}

// Whether the replayer's state is the same: everything the next ticks depend on, not the Paula channels.

static bool mt_same_sequence(const MtSequenceState* a, const MtSequenceState* b) {
    return a->block.zeros == b->block.zeros && a->block.offset == b->block.offset && a->index == b->index
           && a->timer == b->timer && a->rate == b->rate && a->hold == b->hold;
}

static bool mt_same_voice(const MtVoice* a, const MtVoice* b) {
    for (int s = 0; s < MT_SEQUENCES; s++) {
        if (!mt_same_sequence(&a->sequence[s], &b->sequence[s])) {
            return false;
        }
    }
    return a->note == b->note && a->effect == b->effect && a->param == b->param && a->noise == b->noise
           && a->new_note == b->new_note && a->slide == b->slide && a->current == b->current && a->target == b->target
           && a->direction == b->direction && a->note_offset == b->note_offset
           && a->note_offset_used == b->note_offset_used && a->volume_step == b->volume_step
           && a->sample_address == b->sample_address;
}

static bool mt_same_state(const MtPlayer* a, const MtPlayer* b, uint16_t b_speed) {
    if (a->speed_counter != b->speed_counter || a->position != b->position || a->row_offset != b->row_offset
        || mt_speed(&a->module) != b_speed) {
        return false;
    }
    for (int ch = 0; ch < MT_VOICES; ch++) {
        if (!mt_same_voice(&a->voices[ch], &b->voices[ch])) {
            return false;
        }
    }
    return true;
}

// Runs the song without output until its state repeats (Brent's cycle detection): the replayer is deterministic, so
// every later read has then been made. False when it reads memory the port does not model or the tick budget runs out.

static bool mt_run_song(MtPlayer* player) {
    mt_reset(player);
    MtPlayer saved = *player;
    uint16_t saved_speed = mt_speed(&player->module); // the copy shares the module bytes
    uint32_t power = 1;
    uint32_t length = 0;
    for (uint32_t tick = 0; tick < MT_CHECK_TICKS; tick++) {
        mt_run_tick(player);
        if (player->unmapped) {
            return false;
        }
        if (mt_same_state(player, &saved, saved_speed)) {
            return true;
        }
        length++;
        if (length == power) {
            saved = *player;
            saved_speed = mt_speed(&player->module);
            power *= 2;
            length = 0;
        }
    }
    return false;
}

MtTickResult mt_tick(MtPlayer* player, NuadeVoices* voices) {
    MtTickResult result = { 0 };
    if (player->ended) {
        result.ended = true;
        return result;
    }
    result.looped = mt_run_tick(player);
    if (player->unmapped) {
        player->ended = true;
        result.ended = true;
        return result;
    }
    for (uint32_t ch = 0; ch < MT_VOICES; ch++) {
        nuade_paula_channel_dma_on(&player->voices[ch].paula);
        nuade_paula_channel_sync(&player->voices[ch].paula, ch, voices);
    }
    return result;
}

MtPosition mt_position(const MtPlayer* player) {
    return (MtPosition) { .order = player->position,
                          .row = (int)(player->row_offset / MT_ROW_BYTES),
                          .speed = mt_speed(&player->module) };
}

int mt_order_count(const MtPlayer* player) {
    return mt_song_length(&player->module);
}

MtVoiceState mt_voice_state(const MtPlayer* player, int voice) {
    const MtVoice* v = &player->voices[voice];
    return (MtVoiceState) { .note = v->note, .instrument = v->shown_instrument };
}

MtCell mt_pattern_cell(const MtPlayer* player, int order, int voice, int row) {
    return mt_row(&player->module, mt_order_pattern(&player->module, (uint8_t)order), (uint32_t)row, voice, nullptr);
}
