// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Collin
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/sidmon2.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include "nuade_tracker.h"
#include <stdlib.h>
#include <string.h>

// Tables

static const uint16_t s_periods[]
    = { 0,    5760, 5424, 5120, 4832, 4560, 4304, 4064, 3840, 3616, 3424, 3232, 3048, 2880, 2712,
        2560, 2416, 2280, 2152, 2032, 1920, 1808, 1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140,
        1076, 1016, 960,  904,  856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,
        453,  428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,  214,  202,
        190,  180,  170,  160,  151,  143,  135,  127,  120,  113,  107,  101,  95 };

#define PERIODS_COUNT (sizeof(s_periods) / sizeof(s_periods[0]))

// Types

// One row of a track: note 0 = none, instrument 0 = keep the voice's, effect 0 = none.
typedef struct TrackRow {
    uint16_t note;
    uint16_t instrument;
    uint16_t effect;
    uint16_t effect_arg;
} TrackRow;

// Byte-reading helpers

static uint8_t read_u8(const uint8_t* data, size_t* pos) {
    return data[(*pos)++];
}

static uint16_t read_be16(const uint8_t* data, size_t* pos) {
    uint16_t val = (uint16_t)((data[*pos] << 8) | data[*pos + 1]);
    *pos += 2;
    return val;
}

static int16_t read_be16_signed(const uint8_t* data, size_t* pos) {
    return (int16_t)read_be16(data, pos);
}

static uint32_t read_be32(const uint8_t* data, size_t* pos) {
    uint32_t val = ((uint32_t)data[*pos] << 24) | ((uint32_t)data[*pos + 1] << 16) | ((uint32_t)data[*pos + 2] << 8)
                   | (uint32_t)data[*pos + 3];
    *pos += 4;
    return val;
}

static int32_t read_be32_signed(const uint8_t* data, size_t* pos) {
    return (int32_t)read_be32(data, pos);
}

// Module parsing

// True when [offset, offset + length) lies inside a buffer of `size` bytes (no 32-bit wrap).
static bool range_in_bounds(uint32_t offset, uint32_t length, size_t size) {
    return (uint64_t)offset + length <= size;
}

bool sd2_probe(const uint8_t* data, size_t size) {
    if (size < 86) {
        return false;
    }
    // Check "SIDMON II - THE MIDI VERSION" at offset 58
    return memcmp(data + 58, "SIDMON II - THE MIDI VERSION", 28) == 0;
}

static bool load_single_list_u8(const uint8_t* data, size_t size, uint32_t offset, uint32_t length, uint8_t*** out_list,
                                int32_t* out_count) {
    if ((length % 16) != 0 || !range_in_bounds(offset, length, size)) {
        return false;
    }

    uint32_t count = length / 16;
    // Never empty, so list[0] is always the block that the destructor frees
    uint8_t** list = (uint8_t**)calloc(count ? count : 1, sizeof(uint8_t*));
    uint8_t* block = (uint8_t*)malloc(length ? length : 1);
    if (list == nullptr || block == nullptr) {
        free(list);
        free(block);
        return false;
    }

    memcpy(block, data + offset, length);
    list[0] = block;
    for (uint32_t i = 1; i < count; i++) {
        list[i] = block + i * 16;
    }

    *out_list = list;
    *out_count = (int32_t)count;
    return true;
}

static bool load_single_list_i8(const uint8_t* data, size_t size, uint32_t offset, uint32_t length, int8_t*** out_list,
                                int32_t* out_count) {
    if ((length % 16) != 0 || !range_in_bounds(offset, length, size)) {
        return false;
    }

    uint32_t count = length / 16;
    // Never empty, so list[0] is always the block that the destructor frees
    int8_t** list = (int8_t**)calloc(count ? count : 1, sizeof(int8_t*));
    int8_t* block = (int8_t*)malloc(length ? length : 1);
    if (list == nullptr || block == nullptr) {
        free(list);
        free(block);
        return false;
    }

    memcpy(block, data + offset, length);
    list[0] = block;
    for (uint32_t i = 1; i < count; i++) {
        list[i] = block + i * 16;
    }

    *out_list = list;
    *out_count = (int32_t)count;
    return true;
}

static bool load_sequences(const uint8_t* data, size_t size, uint32_t pos_offset, uint32_t pos_length,
                           uint32_t note_offset, uint32_t note_length, uint32_t inst_offset, uint32_t inst_length,
                           uint16_t num_positions, Sd2Sequence** out_sequences) {
    if (pos_length != note_length || note_length != inst_length) {
        return false;
    }
    if (pos_length != (uint32_t)(num_positions * 4)) {
        return false;
    }
    if (!range_in_bounds(pos_offset, pos_length, size) || !range_in_bounds(note_offset, note_length, size)
        || !range_in_bounds(inst_offset, inst_length, size)) {
        return false;
    }

    // 4 channels * num_positions
    Sd2Sequence* seqs = (Sd2Sequence*)calloc(4 * num_positions, sizeof(Sd2Sequence));
    if (seqs == nullptr) {
        return false;
    }

    for (int ch = 0; ch < 4; ch++) {
        for (int j = 0; j < num_positions; j++) {
            int idx = ch * num_positions + j;
            int src_idx = ch * num_positions + j;
            seqs[idx].track_number = data[pos_offset + src_idx];
            seqs[idx].note_transpose = (int8_t)data[note_offset + src_idx];
            seqs[idx].instrument_transpose = (int8_t)data[inst_offset + src_idx];
        }
    }

    *out_sequences = seqs;
    return true;
}

static bool load_tracks(const uint8_t* data, size_t size, uint32_t table_offset, uint32_t table_length,
                        uint32_t tracks_offset, uint32_t tracks_length, Sd2Track** out_tracks, int32_t* out_count,
                        uint32_t* out_sample_offset) {
    uint32_t num_tracks = table_length / 2;
    if (!range_in_bounds(table_offset, table_length, size) || !range_in_bounds(tracks_offset, tracks_length, size)) {
        return false;
    }

    // Read track offset table
    uint16_t* track_offsets = (uint16_t*)calloc(num_tracks, sizeof(uint16_t));
    if (track_offsets == nullptr) {
        return false;
    }

    size_t tpos = table_offset;
    for (uint32_t i = 0; i < num_tracks; i++) {
        track_offsets[i] = read_be16(data, &tpos);
    }

    Sd2Track* trks = (Sd2Track*)calloc(num_tracks, sizeof(Sd2Track));
    if (trks == nullptr) {
        free(track_offsets);
        return false;
    }

    for (uint32_t i = 0; i < num_tracks; i++) {
        // A track runs to the next track's start (or the end of the block); offsets that go backwards or
        // leave the block give an empty track.
        uint32_t track_start = track_offsets[i];
        uint32_t track_end = (i == num_tracks - 1) ? tracks_length : track_offsets[i + 1];
        if (track_end > tracks_length)
            track_end = tracks_length;
        uint32_t track_len = track_end > track_start ? track_end - track_start : 0;
        size_t src = (size_t)tracks_offset + track_start;
        if (src + track_len > size) {
            // Truncate to available data
            track_len = (src < size) ? (uint32_t)(size - src) : 0;
        }

        trks[i].track_data = (uint8_t*)malloc(track_len > 0 ? track_len : 1);
        if (trks[i].track_data == nullptr) {
            for (uint32_t j = 0; j < i; j++) {
                free(trks[j].track_data);
            }
            free(trks);
            free(track_offsets);
            return false;
        }
        if (track_len > 0) {
            memcpy(trks[i].track_data, data + src, track_len);
        }
        trks[i].length = track_len;
        trks[i].image_offset
            = tracks_offset + (track_start < size - tracks_offset ? track_start : (uint32_t)size - tracks_offset);
    }

    // Sd2Sample data starts right after the tracks block, rounded up to an even address.
    uint32_t sample_off = (tracks_offset + tracks_length + 1) & ~1u;

    *out_tracks = trks;
    *out_count = (int32_t)num_tracks;
    *out_sample_offset = sample_off;

    free(track_offsets);
    return true;
}

#define INSTRUMENT_SIZE 32

static Sd2Instrument parse_instrument(const uint8_t* b) {
    return (Sd2Instrument) {
        .waveform_list_number = b[0],
        .waveform_list_length = b[1],
        .waveform_list_speed = b[2],
        .waveform_list_delay = b[3],
        .arpeggio_number = b[4],
        .arpeggio_length = b[5],
        .arpeggio_speed = b[6],
        .arpeggio_delay = b[7],
        .vibrato_number = b[8],
        .vibrato_length = b[9],
        .vibrato_speed = b[10],
        .vibrato_delay = b[11],
        .pitch_bend_speed = (int8_t)b[12],
        .pitch_bend_delay = b[13],
        .attack_max = b[16],
        .attack_speed = b[17],
        .decay_min = b[18],
        .decay_speed = b[19],
        .sustain_time = b[20],
        .release_min = b[21],
        .release_speed = b[22],
    };
}

static bool load_instruments(const uint8_t* data, size_t size, uint32_t offset, uint32_t length,
                             Sd2Instrument** out_instruments, int32_t* out_count) {
    if ((length % INSTRUMENT_SIZE) != 0) {
        return false;
    }
    if (offset + length > size) {
        return false;
    }

    uint32_t count = length / INSTRUMENT_SIZE;
    Sd2Instrument* insts = (Sd2Instrument*)calloc(count, sizeof(Sd2Instrument));
    if (insts == nullptr) {
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        insts[i] = parse_instrument(data + offset + i * INSTRUMENT_SIZE);
    }

    *out_instruments = insts;
    *out_count = (int32_t)count;
    return true;
}

static bool load_samples(const uint8_t* data, size_t size, uint32_t sample_offset, uint16_t num_samples,
                         uint32_t info_offset, uint32_t info_length, Sd2Sample** out_samples,
                         Sd2SampleNegateInfo** out_negate) {
    if (!range_in_bounds(info_offset, info_length, size)) {
        return false;
    }

    Sd2Sample* samps = (Sd2Sample*)calloc(num_samples, sizeof(Sd2Sample));
    Sd2SampleNegateInfo* negates = (Sd2SampleNegateInfo*)calloc(num_samples, sizeof(Sd2SampleNegateInfo));
    if (samps == nullptr || negates == nullptr) {
        free(samps);
        free(negates);
        return false;
    }

    // Sample info entries are 64 bytes: 4 (ptr) + 2+2+2 (len/loopstart/looplen) + 18 (negate) + 4 (skip) + 32 (name).
    size_t pos = info_offset;
    for (int i = 0; i < num_samples; i++) {
        if (pos + 64 > size) {
            free(samps);
            free(negates);
            return false;
        }

        pos += 4; // skip sample data pointer

        samps[i].length = (uint32_t)read_be16(data, &pos) * 2;
        samps[i].loop_start = (uint32_t)read_be16(data, &pos) * 2;
        samps[i].loop_length = (uint32_t)read_be16(data, &pos) * 2;

        if (samps[i].loop_start > samps[i].length) {
            samps[i].loop_start = 0;
            samps[i].loop_length = 0;
        } else if (samps[i].loop_start + samps[i].loop_length > samps[i].length) {
            samps[i].loop_length = samps[i].length - samps[i].loop_start;
        }

        // Negate info
        negates[i].start_offset = (uint32_t)read_be16(data, &pos) * 2;
        negates[i].end_offset = (uint32_t)read_be16(data, &pos) * 2;
        negates[i].loop_index = read_be16(data, &pos);
        negates[i].status = read_be16(data, &pos);
        negates[i].speed = read_be16_signed(data, &pos);
        negates[i].position = read_be32_signed(data, &pos);
        negates[i].index = read_be16(data, &pos);
        negates[i].do_negation = read_be16_signed(data, &pos);

        pos += 4;  // skip 4 bytes
        pos += 32; // skip name
    }

    // Read sample data
    size_t sdata_pos = sample_offset;
    for (int i = 0; i < num_samples; i++) {
        if (samps[i].length == 0) {
            samps[i].sample_data = (int8_t*)calloc(4, 1);
            continue;
        }
        if (sdata_pos + samps[i].length > size) {
            // Truncate or fail
            uint32_t avail = (sdata_pos < size) ? (uint32_t)(size - sdata_pos) : 0;
            samps[i].sample_data = (int8_t*)calloc(samps[i].length, 1);
            if (samps[i].sample_data != nullptr && avail > 0) {
                memcpy(samps[i].sample_data, data + sdata_pos, avail);
            }
            sdata_pos += samps[i].length;
            continue;
        }
        samps[i].sample_data = (int8_t*)malloc(samps[i].length);
        if (samps[i].sample_data == nullptr) {
            // cleanup
            for (int j = 0; j < i; j++) {
                free(samps[j].sample_data);
            }
            free(samps);
            free(negates);
            return false;
        }
        memcpy(samps[i].sample_data, data + sdata_pos, samps[i].length);
        sdata_pos += samps[i].length;
    }

    *out_samples = samps;
    *out_negate = negates;
    return true;
}

static bool sd2_parse(Sd2Module* mod, const uint8_t* data, size_t size) {
    if (!sd2_probe(data, size)) {
        return false;
    }

    mod->image = (uint8_t*)malloc(size);
    if (mod->image == nullptr) {
        return false;
    }
    memcpy(mod->image, data, size);
    mod->image_size = (uint32_t)size;

    size_t pos = 2; // skip MIDI mode
    // The stored count is positions - 1, so 0xff means 256 positions.
    uint16_t num_pos = (uint16_t)(read_u8(data, &pos) + 1);
    mod->start_speed = read_u8(data, &pos);

    uint16_t num_samples = read_be16(data, &pos);
    num_samples /= 64;

    // Block offsets: start after header
    // offset 6: 10 x uint32 block lengths
    // Block data starts at offset 58 + id_length + song_length_length

    uint64_t offset = 58;

    // Read id_length and song_length_length
    uint32_t id_length = read_be32(data, &pos);       // at pos 6
    uint32_t song_len_length = read_be32(data, &pos); // at pos 10

    offset += id_length;
    offset += song_len_length;

    // Read 10 block lengths
    uint32_t block_lengths[10];
    for (int i = 0; i < 10; i++) {
        block_lengths[i] = read_be32(data, &pos);
    }

    // Compute block offsets
    uint32_t block_offsets[10];
    uint64_t cur_offset = offset;
    for (int i = 0; i < 10; i++) {
        if (cur_offset > size)
            return false;
        block_offsets[i] = (uint32_t)cur_offset;
        cur_offset += block_lengths[i];
    }
    if (cur_offset > size)
        return false;

    // Block indices (from C# LoadSongData order):
    // 0: positionTable, 1: noteTranspose, 2: instrumentTranspose
    // 3: instruments, 4: waveformList, 5: arpeggios
    // 6: vibratoes, 7: sampleInfo, 8: trackTable, 9: tracks

    // Load lists
    if (!load_single_list_u8(data, size, block_offsets[4], block_lengths[4], &mod->waveform_info,
                             &mod->waveform_count)) {
        return false;
    }

    if (!load_single_list_i8(data, size, block_offsets[5], block_lengths[5], &mod->arpeggios, &mod->arpeggio_count)) {
        return false;
    }

    if (!load_single_list_i8(data, size, block_offsets[6], block_lengths[6], &mod->vibratoes, &mod->vibrato_count)) {
        return false;
    }

    // Load sequences
    mod->num_positions = num_pos;
    if (!load_sequences(data, size, block_offsets[0], block_lengths[0], block_offsets[1], block_lengths[1],
                        block_offsets[2], block_lengths[2], num_pos, &mod->sequences)) {
        return false;
    }

    // Load tracks
    uint32_t sample_data_offset = 0;
    if (!load_tracks(data, size, block_offsets[8], block_lengths[8], block_offsets[9], block_lengths[9], &mod->tracks,
                     &mod->track_count, &sample_data_offset)) {
        return false;
    }

    // Load instruments
    mod->instruments_offset = block_offsets[3];
    if (!load_instruments(data, size, block_offsets[3], block_lengths[3], &mod->instruments, &mod->instrument_count)) {
        return false;
    }

    // Load samples
    mod->sample_count = num_samples;
    if (!load_samples(data, size, sample_data_offset, num_samples, block_offsets[7], block_lengths[7], &mod->samples,
                      &mod->sample_negate_info)) {
        return false;
    }

    // numberOfPositions-- (C# does this after loading)
    mod->number_of_positions = (uint8_t)(num_pos - 1);

    return true;
}

// Voice rendering helpers

static NuadeSample s8_sample(const int8_t* data, uint32_t length, uint32_t loop_start, uint32_t loop_length) {
    return (NuadeSample) { .data = data,
                           .format = NuadeSampleFormat_S8,
                           .length = length,
                           .loop_start = loop_start,
                           .loop_length = loop_length };
}

// Restarts the channel; a loop of two bytes or less is no loop.
static void voice_render_play_sample(Sd2VoiceRender* vr, const int8_t* sample_data, uint32_t length,
                                     const int8_t* loop_data, uint32_t loop_offset, uint32_t loop_length) {
    vr->active = true;
    vr->play_pending = true;
    vr->queue_pending = false;

    if (loop_length <= 2) {
        vr->head = s8_sample(sample_data, length, 0, 0);
    } else if (loop_data == sample_data) {
        vr->head = s8_sample(sample_data, length, loop_offset, loop_length);
    } else {
        // The loop lives in another sample: play this one up to the loop end, then wrap there
        const uint32_t loop_end = loop_offset + loop_length;
        vr->head = s8_sample(sample_data, loop_end < length ? loop_end : length, 0, 0);
        vr->queued = s8_sample(loop_data + loop_offset, loop_length, 0, loop_length);
        vr->queue_pending = true;
    }
}

// Takes over at the end of the playing sample, starting at `offset`, and loops there when longer than two bytes.
static void voice_render_set_sample(Sd2VoiceRender* vr, const int8_t* sample_data, uint32_t offset, uint32_t length) {
    static const int8_t silence[1] = { 0 };
    // An empty sample cannot be queued; one silent frame ends the channel the same way
    vr->queued = length == 0 ? s8_sample(silence, 1, 0, 0)
                             : s8_sample(sample_data + offset, length, 0, length > 2 ? length : 0);
    vr->queue_pending = true;
}

static void voice_render_mute(Sd2VoiceRender* vr) {
    vr->active = false;
    vr->volume = 0;
    vr->play_pending = false;
    vr->queue_pending = false;
}

// Player state init

// The player indexes the instrument table with a byte and does not check it; past the table it reads the rest
// of the module and, past that, zeros.
static Sd2Instrument* select_instrument(Sd2Module* mod, Sd2VoiceInfo* vi, uint8_t index) {
    if (index < mod->instrument_count) {
        return &mod->instruments[index];
    }
    uint8_t raw[INSTRUMENT_SIZE] = { 0 };
    const uint64_t start = (uint64_t)mod->instruments_offset + (uint64_t)index * INSTRUMENT_SIZE;
    if (start < mod->image_size) {
        const uint32_t available = mod->image_size - (uint32_t)start;
        memcpy(raw, mod->image + start, available < INSTRUMENT_SIZE ? available : INSTRUMENT_SIZE);
    }
    vi->unmapped_instrument = parse_instrument(raw);
    return &vi->unmapped_instrument;
}

// The bytes of a sequence entry's track as the player reads them; none for a track that does not exist.
static void track_bytes(const Sd2Module* mod, const Sd2Sequence* seq, const uint8_t** data, uint32_t* length) {
    if (seq->track_number < mod->track_count) {
        const Sd2Track* trk = &mod->tracks[seq->track_number];
        if (trk->edited) {
            *data = trk->track_data;
            *length = trk->length;
        } else {
            *data = mod->image + trk->image_offset;
            *length = mod->image_size - trk->image_offset;
        }
    } else {
        *data = nullptr;
        *length = 0;
    }
}

static void find_note(Sd2Module* mod, Sd2VoiceInfo* vi) {
    Sd2Sequence* seq = &mod->sequences[0]; // base for this channel
    // Find correct sequence for this channel
    int ch_idx = (int)(vi - &mod->voices[0]);
    seq = &mod->sequences[ch_idx * mod->num_positions + mod->current_position];

    const uint8_t* track;
    track_bytes(mod, seq, &track, &vi->current_track_length);
    vi->current_track = (uint8_t*)track;
    vi->track_position = 0;
    vi->note_transpose = seq->note_transpose;
    vi->instrument_transpose = seq->instrument_transpose;
    vi->empty_notes_counter = 0;
}

static void init_sound(Sd2Module* mod) {
    mod->current_position = 0;
    mod->current_row = 0;
    mod->pattern_length = 64;
    mod->current_rast = 0;
    mod->current_rast2 = 0;
    mod->speed = mod->start_speed;
    mod->shown_position = 0;
    mod->shown_row = 0;

    for (int i = 0; i < 4; i++) {
        Sd2VoiceInfo* vi = &mod->voices[i];
        memset(vi, 0, sizeof(Sd2VoiceInfo));
        vi->shown_note = -1;
        vi->shown_instrument = -1;

        vi->sequence_list = &mod->sequences[i * mod->num_positions];

        find_note(mod, vi);

        Sd2Instrument* inst = select_instrument(mod, vi, (uint8_t)vi->instrument_transpose);
        vi->instrument = inst;

        const uint8_t sample_number
            = inst->waveform_list_number < mod->waveform_count ? mod->waveform_info[inst->waveform_list_number][0] : 0;
        vi->current_sample = sample_number;

        if (sample_number < mod->sample_count) {
            Sd2Sample* smp = &mod->samples[sample_number];
            vi->sample_data = smp->sample_data;
            vi->sample_length = smp->length;
            vi->loop_sample = smp->sample_data;
            vi->loop_offset = smp->loop_start;
            vi->loop_length = smp->loop_length;
        }

        vi->envelope_state = Sd2EnvelopeState_Attack;
        vi->sample_period = 0;

        // Init render state
        Sd2VoiceRender* vr = &mod->voice_render[i];
        memset(vr, 0, sizeof(Sd2VoiceRender));
    }
}

// Note and voice processing

// One row of a track as the player reads it. `empty_notes` counts the empty rows still owed by a run.
static TrackRow read_track_row(const uint8_t* track_data, int32_t length, int32_t* position, uint16_t* empty_notes) {
    TrackRow row = { 0 };

    if ((*empty_notes) == 0) {
        int8_t track_value;
        if (track_data == nullptr || (*position) >= (int32_t)length) {
            track_value = -1;
        } else {
            track_value = (int8_t)track_data[(*position)++];
        }

        if (track_value == 0) {
            if ((*position) + 1 < (int32_t)length) {
                row.effect = track_data[(*position)++];
                row.effect_arg = track_data[(*position)++];
            }
            return row;
        }

        if (track_value > 0) {
            if (track_value >= 0x70) {
                row.effect = (uint8_t)track_value;
                if ((*position) < (int32_t)length) {
                    row.effect_arg = track_data[(*position)++];
                }
                return row;
            }

            row.note = (uint16_t)track_value;

            if ((*position) < (int32_t)length) {
                track_value = (int8_t)track_data[(*position)++];
                if (track_value >= 0) {
                    if (track_value >= 0x70) {
                        row.effect = (uint8_t)track_value;
                        if ((*position) < (int32_t)length) {
                            row.effect_arg = track_data[(*position)++];
                        }
                        return row;
                    }

                    row.instrument = (uint16_t)track_value;

                    if ((*position) < (int32_t)length) {
                        track_value = (int8_t)track_data[(*position)++];
                        if (track_value >= 0) {
                            row.effect = (uint8_t)track_value;
                            if ((*position) < (int32_t)length) {
                                row.effect_arg = track_data[(*position)++];
                            }
                            return row;
                        }
                    } else {
                        return row;
                    }
                }
            } else {
                return row;
            }
        }

        (*empty_notes) = (uint16_t)(~(int)track_value);
    } else {
        (*empty_notes)--;
    }
    return row;
}

static void get_note2(Sd2VoiceInfo* vi) {
    const TrackRow row = read_track_row(vi->current_track, (int32_t)vi->current_track_length, &vi->track_position,
                                        &vi->empty_notes_counter);
    vi->current_note = row.note;
    vi->current_instrument = row.instrument;
    vi->current_effect = row.effect;
    vi->current_effect_arg = row.effect_arg;
}

static void get_note(Sd2VoiceInfo* vi) {
    get_note2(vi);

    if (vi->current_note != 0) {
        vi->current_note = (uint16_t)(vi->current_note + vi->note_transpose);
    }
}

static void play_voice(Sd2Module* mod, Sd2VoiceInfo* vi, Sd2VoiceRender* vr) {
    vi->pitch_bend_value = 0;

    if (vi->current_note != 0) {
        vi->shown_note = vi->current_note;
        if (vi->current_instrument != 0) {
            vi->shown_instrument = vi->current_instrument + vi->instrument_transpose;
        }
        vi->sample_volume = 0;
        vi->wave_list_delay = 0;
        vi->wave_list_offset = 0;
        vi->arpeggio_delay = 0;
        vi->arpeggio_offset = 0;
        vi->vibrato_delay = 0;
        vi->vibrato_offset = 0;
        vi->pitch_bend_counter = 0;
        vi->note_slide_speed = 0;

        vi->envelope_state = Sd2EnvelopeState_Attack;
        vi->sustain_counter = 0;

        if (vi->current_instrument != 0) {
            vi->instrument
                = select_instrument(mod, vi, (uint8_t)(vi->current_instrument - 1 + vi->instrument_transpose));

            if (vi->instrument->waveform_list_number < mod->waveform_count) {
                uint8_t* waveform_list = mod->waveform_info[vi->instrument->waveform_list_number];
                vi->current_sample = waveform_list[0];

                if (vi->current_sample < (uint16_t)mod->sample_count) {
                    Sd2Sample* smp = &mod->samples[vi->current_sample];
                    vi->sample_data = smp->sample_data;
                    vi->sample_length = smp->length;
                    vi->loop_sample = smp->sample_data;
                    vi->loop_offset = smp->loop_start;
                    vi->loop_length = smp->loop_length;
                }
            }
        }

        if (vi->instrument->arpeggio_number < mod->arpeggio_count) {
            int8_t* arpeggio = mod->arpeggios[vi->instrument->arpeggio_number];
            int note = vi->current_note + arpeggio[0];
            if (note >= 0 && note < (int)PERIODS_COUNT) {
                vi->original_note = (uint16_t)note;
                vi->sample_period = s_periods[note];

                if (vi->sample_length != 0) {
                    voice_render_play_sample(vr, vi->sample_data, vi->sample_length, vi->loop_sample, vi->loop_offset,
                                             vi->loop_length);
                } else {
                    voice_render_mute(vr);
                }
            } else {
                voice_render_mute(vr);
            }
        } else {
            voice_render_mute(vr);
        }

        vr->period = vi->sample_period;
    }
}

// Effect processing

static void do_adsr_curve(Sd2VoiceInfo* vi, Sd2VoiceRender* vr) {
    Sd2Instrument* inst = vi->instrument;
    (void)vr;

    switch (vi->envelope_state) {
        case Sd2EnvelopeState_Attack:
            vi->sample_volume += inst->attack_speed;
            if (vi->sample_volume >= inst->attack_max) {
                vi->sample_volume = inst->attack_max;
                vi->envelope_state = Sd2EnvelopeState_Decay;
            }
            break;

        case Sd2EnvelopeState_Decay:
            if (inst->decay_speed == 0) {
                vi->envelope_state = Sd2EnvelopeState_Sustain;
            } else {
                vi->sample_volume -= inst->decay_speed;
                if (vi->sample_volume <= inst->decay_min) {
                    vi->sample_volume = inst->decay_min;
                    vi->envelope_state = Sd2EnvelopeState_Sustain;
                }
            }
            break;

        case Sd2EnvelopeState_Sustain:
            if (vi->sustain_counter == inst->sustain_time) {
                vi->envelope_state = Sd2EnvelopeState_Release;
            } else {
                vi->sustain_counter++;
            }
            break;

        case Sd2EnvelopeState_Release:
            if (inst->release_speed == 0) {
                vi->envelope_state = Sd2EnvelopeState_Done;
            } else {
                vi->sample_volume -= inst->release_speed;
                if (vi->sample_volume <= inst->release_min) {
                    vi->sample_volume = inst->release_min;
                    vi->envelope_state = Sd2EnvelopeState_Done;
                }
            }
            break;

        case Sd2EnvelopeState_Done:
            break;
    }

    // SetVolume maps to vr->volume. C# uses 0-256 range.
    uint16_t vol = (vi->sample_volume < 0) ? 0 : (uint16_t)vi->sample_volume;
    vr->volume = vol;
}

static void do_waveform(Sd2Module* mod, Sd2VoiceInfo* vi, Sd2VoiceRender* vr) {
    Sd2Instrument* inst = vi->instrument;

    if (inst->waveform_list_length != 0) {
        if (vi->wave_list_delay == inst->waveform_list_delay) {
            vi->wave_list_delay -= inst->waveform_list_speed;

            if (vi->wave_list_offset == inst->waveform_list_length) {
                vi->wave_list_offset = -1;
            }

            vi->wave_list_offset++;

            if (inst->waveform_list_number < mod->waveform_count && vi->wave_list_offset < 16) {
                int8_t waveform_value = (int8_t)mod->waveform_info[inst->waveform_list_number][vi->wave_list_offset];
                if (waveform_value >= 0) {
                    vi->current_sample = (uint16_t)waveform_value;

                    if (vi->current_sample < (uint16_t)mod->sample_count) {
                        Sd2Sample* smp = &mod->samples[vi->current_sample];
                        vi->loop_sample = smp->sample_data;
                        vi->loop_offset = smp->loop_start;
                        vi->loop_length = smp->loop_length;

                        if (vr->active) {
                            voice_render_set_sample(vr, vi->loop_sample, vi->loop_offset, vi->loop_length);
                        }
                    }
                } else {
                    vi->wave_list_offset--;
                }
            }
        } else {
            vi->wave_list_delay++;
        }
    }
}

static void do_arpeggio(Sd2Module* mod, Sd2VoiceInfo* vi) {
    Sd2Instrument* inst = vi->instrument;

    if (inst->arpeggio_length != 0) {
        if (vi->arpeggio_delay == inst->arpeggio_delay) {
            vi->arpeggio_delay -= inst->arpeggio_speed;

            if (vi->arpeggio_offset == inst->arpeggio_length) {
                vi->arpeggio_offset = -1;
            }

            vi->arpeggio_offset++;

            if (inst->arpeggio_number < mod->arpeggio_count && vi->arpeggio_offset < 16) {
                int8_t arp_value = mod->arpeggios[inst->arpeggio_number][vi->arpeggio_offset];
                int16_t new_note = (int16_t)(vi->original_note + arp_value);
                if (new_note >= 0 && new_note < (int16_t)PERIODS_COUNT) {
                    vi->sample_period = s_periods[new_note];
                }
            }
        } else {
            vi->arpeggio_delay++;
        }
    }
}

static void do_vibrato(Sd2Module* mod, Sd2VoiceInfo* vi) {
    Sd2Instrument* inst = vi->instrument;

    if (inst->vibrato_length != 0) {
        if (vi->vibrato_delay == inst->vibrato_delay) {
            vi->vibrato_delay -= inst->vibrato_speed;

            if (vi->vibrato_offset == inst->vibrato_length) {
                vi->vibrato_offset = -1;
            }

            vi->vibrato_offset++;

            if (inst->vibrato_number < mod->vibrato_count && vi->vibrato_offset < 16) {
                int8_t vib_value = mod->vibratoes[inst->vibrato_number][vi->vibrato_offset];
                vi->sample_period = (uint16_t)(vi->sample_period + vib_value);
            }
        } else {
            vi->vibrato_delay++;
        }
    }
}

static void do_pitch_bend(Sd2VoiceInfo* vi) {
    Sd2Instrument* inst = vi->instrument;

    if (inst->pitch_bend_speed != 0) {
        if (vi->pitch_bend_counter == inst->pitch_bend_delay) {
            vi->pitch_bend_value = (int16_t)(vi->pitch_bend_value + inst->pitch_bend_speed);
        } else {
            vi->pitch_bend_counter++;
        }
    }
}

static void do_note_slide(Sd2VoiceInfo* vi) {
    if (vi->current_effect != 0 && vi->current_effect < 0x70 && vi->current_effect_arg != 0) {
        if (vi->current_effect < PERIODS_COUNT) {
            vi->note_slide_note = s_periods[vi->current_effect];
        }

        int direction = (int)vi->note_slide_note - (int)vi->sample_period;
        if (direction == 0) {
            return;
        }

        int16_t effect_arg = (int16_t)vi->current_effect_arg;
        if (direction < 0) {
            effect_arg = (int16_t)-effect_arg;
        }

        vi->note_slide_speed = effect_arg;
    }

    int16_t speed = vi->note_slide_speed;
    if (speed != 0) {
        if (speed < 0) {
            vi->sample_period = (uint16_t)(vi->sample_period + speed);
            if (vi->sample_period <= vi->note_slide_note) {
                vi->sample_period = vi->note_slide_note;
                vi->note_slide_speed = 0;
            }
        } else {
            vi->sample_period = (uint16_t)(vi->sample_period + speed);
            if (vi->sample_period >= vi->note_slide_note) {
                vi->sample_period = vi->note_slide_note;
                vi->note_slide_speed = 0;
            }
        }
    }
}

static void do_sound_tracker(Sd2Module* mod, Sd2VoiceInfo* vi, Sd2VoiceRender* vr) {
    uint16_t effect = vi->current_effect;
    if (effect >= 0x70) {
        effect &= 0x0f;

        if (mod->current_rast != 0 || effect >= 5) {
            switch (effect) {
                // Arpeggio
                case 0x0: {
                    uint8_t arp_tab[]
                        = { (uint8_t)(vi->current_effect_arg >> 4), 0, (uint8_t)(vi->current_effect_arg & 0x0f), 0 };
                    uint8_t arp_value = arp_tab[mod->current_rast2];
                    int idx = vi->original_note + arp_value;
                    if (idx >= 0 && idx < (int)PERIODS_COUNT) {
                        vi->sample_period = s_periods[idx];
                    }
                    break;
                }

                // Pitch up
                case 0x1:
                    vi->pitch_bend_value = (int16_t)(-(int16_t)vi->current_effect_arg);
                    break;

                // Pitch down
                case 0x2:
                    vi->pitch_bend_value = (int16_t)vi->current_effect_arg;
                    break;

                // Volume up
                case 0x3:
                    if (vi->envelope_state == Sd2EnvelopeState_Done) {
                        if (mod->current_rast == 0 && vi->current_instrument != 0) {
                            vi->sample_volume = vi->instrument->attack_speed;
                        }
                        int16_t volume = (int16_t)(vi->sample_volume + vi->current_effect_arg * 4);
                        if (volume >= 256) {
                            volume = 255;
                        }
                        vi->sample_volume = volume;
                    }
                    break;

                // Volume down
                case 0x4:
                    if (vi->envelope_state == Sd2EnvelopeState_Done) {
                        if (mod->current_rast == 0 && vi->current_instrument != 0) {
                            vi->sample_volume = vi->instrument->attack_speed;
                        }
                        int16_t volume = (int16_t)(vi->sample_volume - vi->current_effect_arg * 4);
                        if (volume < 0) {
                            volume = 0;
                        }
                        vi->sample_volume = volume;
                    }
                    break;

                // Set ADSR attack
                case 0x5:
                    vi->instrument->attack_max = (uint8_t)vi->current_effect_arg;
                    vi->instrument->attack_speed = (uint8_t)vi->current_effect_arg;
                    break;

                // Set pattern length
                case 0x6:
                    mod->pattern_length = (uint8_t)vi->current_effect_arg;
                    break;

                // Volume change
                case 0xc: {
                    // The argument goes to AUDxVOL as it is, and Paula reads the low 7 bits (64 and up is full volume)
                    const uint16_t argument = vi->current_effect_arg;
                    const uint16_t paula_volume = (argument & 0x7f) > 64 ? 64 : (argument & 0x7f);
                    vi->sample_volume = (int16_t)(argument * 4 > 255 ? 255 : argument * 4);
                    vr->volume = (uint16_t)(paula_volume * 4);
                    break;
                }

                // Speed change
                case 0xf: {
                    uint8_t new_speed = (uint8_t)(vi->current_effect_arg & 0x0f);
                    if (new_speed != 0 && new_speed != mod->speed) {
                        mod->speed = new_speed;
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }
}

static void do_effect(Sd2Module* mod, Sd2VoiceInfo* vi, Sd2VoiceRender* vr) {
    do_adsr_curve(vi, vr);
    do_waveform(mod, vi, vr);
    do_arpeggio(mod, vi);
    do_sound_tracker(mod, vi, vr);
    do_vibrato(mod, vi);
    do_pitch_bend(vi);
    do_note_slide(vi);

    // The replayer adds and clamps as signed 16-bit words, so a bend below zero ends at the minimum period.
    int16_t period = (int16_t)(vi->sample_period + vi->pitch_bend_value);
    if (period < 95) {
        period = 95;
    } else if (period > 5760) {
        period = 5760;
    }
    vi->sample_period = (uint16_t)period;

    vr->period = vi->sample_period;
}

// Sd2Sample negation

// A 32-bit add that wraps, as the 68000 add.l does (a damaged file can start the position near INT32_MAX).

static int32_t add_wrapping(int32_t a, int32_t b) {
    return (int32_t)((uint32_t)a + (uint32_t)b);
}

static void do_negation(Sd2Module* mod) {
    Sd2SampleNegateInfo* working[4];

    for (int i = 0; i < 4; i++) {
        Sd2VoiceInfo* vi = &mod->voices[i];
        uint16_t sample_number = vi->current_sample;
        if (sample_number >= (uint16_t)mod->sample_count) {
            working[i] = nullptr;
            continue;
        }
        Sd2SampleNegateInfo* ni = &mod->sample_negate_info[sample_number];
        working[i] = ni;

        if (ni->do_negation == 0) {
            ni->do_negation = -1;

            if (ni->index == 0) {
                ni->index = ni->loop_index;

                if (ni->status != 0) {
                    Sd2Sample* smp = &mod->samples[sample_number];
                    uint32_t end_offset = ni->end_offset - 1;

                    int32_t position = (int32_t)(ni->start_offset + ni->position);
                    if (position >= 0 && position < (int32_t)smp->length) {
                        smp->sample_data[position] = (int8_t)~smp->sample_data[position];
                    }

                    ni->position = add_wrapping(ni->position, ni->speed);
                    if (ni->position < 0) {
                        if (ni->status == 2) {
                            ni->position = (int32_t)end_offset;
                        } else {
                            ni->position = add_wrapping(ni->position, -ni->speed);
                            ni->speed = (int16_t)-ni->speed;
                        }
                    } else {
                        if ((uint32_t)ni->position > end_offset) {
                            if (ni->status == 1) {
                                ni->position = 0;
                            } else {
                                ni->position = add_wrapping(ni->position, -ni->speed);
                                ni->speed = (int16_t)-ni->speed;
                            }
                        }
                    }
                }
            } else {
                ni->index--;
                ni->index &= 0x1f;
            }
        }
    }

    for (int i = 0; i < 4; i++) {
        if (working[i] != nullptr) {
            working[i]->do_negation = 0;
        }
    }
}

// Tick function

static bool player_tick(Sd2Module* mod) {
    bool looped = false;
    mod->current_rast2++;
    if (mod->current_rast2 == 3) {
        mod->current_rast2 = 0;
    }

    mod->current_rast++;
    if (mod->current_rast >= mod->speed) {
        mod->current_rast = 0;
        mod->current_rast2 = 0;
        mod->shown_position = mod->current_position;
        mod->shown_row = mod->current_row;

        for (int i = 0; i < 4; i++) {
            get_note(&mod->voices[i]);
        }

        for (int i = 0; i < 4; i++) {
            play_voice(mod, &mod->voices[i], &mod->voice_render[i]);
        }

        do_negation(mod);

        mod->current_row++;
        if (mod->current_row == mod->pattern_length) {
            mod->current_row = 0;

            if (mod->current_position == mod->number_of_positions) {
                mod->current_position = -1;
                looped = true;
            }

            mod->current_position++;

            for (int i = 0; i < 4; i++) {
                find_note(mod, &mod->voices[i]);
            }
        }
    }

    for (int i = 0; i < 4; i++) {
        do_effect(mod, &mod->voices[i], &mod->voice_render[i]);
    }

    if (mod->current_rast != 0) {
        do_negation(mod);
    }
    return looped;
}

// Voices

// Hand the registers the tick wrote to the voice. Paula channels 0 and 3 are left, 1 and 2 right.
static void sync_voice(Sd2Module* mod, uint32_t channel) {
    Sd2VoiceRender* vr = &mod->voice_render[channel];

    if (!vr->active) {
        if (nuade_voice_is_playing(mod->nuade_voices, channel)) {
            nuade_voice_stop(mod->nuade_voices, channel);
        }
    } else {
        if (vr->play_pending) {
            nuade_voice_play(mod->nuade_voices, channel, &vr->head, 0);
        }
        if (vr->queue_pending) {
            nuade_voice_queue(mod->nuade_voices, channel, &vr->queued);
        }
    }
    vr->play_pending = false;
    vr->queue_pending = false;

    const bool silent = vr->period == 0;
    nuade_voice_set_pitch(mod->nuade_voices, channel, silent ? 0.0 : nuade_paula_period_hz(vr->period));
    nuade_voice_set_volume(mod->nuade_voices, channel, silent ? 0.0f : (float)vr->volume / 256.0f);
    nuade_voice_set_pan(mod->nuade_voices, channel, (channel == 0 || channel == 3) ? -1.0f : 1.0f);
}

bool sd2_tick(Sd2Module* mod) {
    if (mod == nullptr || mod->nuade_voices == nullptr) {
        return false;
    }

    // is_playing, not the sample position: a head that ran off its end on the last frame may still have a queued sample
    for (uint32_t ch = 0; ch < SD2_VOICES; ch++) {
        Sd2VoiceRender* vr = &mod->voice_render[ch];
        vr->active = vr->active && nuade_voice_is_playing(mod->nuade_voices, ch);
    }

    const bool looped = player_tick(mod);

    for (uint32_t ch = 0; ch < SD2_VOICES; ch++) {
        sync_voice(mod, ch);
    }
    return looped;
}

// Player create/destroy

Sd2Module* sd2_create(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) {
        return nullptr;
    }

    if (!sd2_probe(data, size)) {
        return nullptr;
    }

    Sd2Module* mod = (Sd2Module*)calloc(1, sizeof(Sd2Module));
    if (mod == nullptr) {
        return nullptr;
    }

    if (!sd2_parse(mod, data, size)) {
        sd2_destroy(mod);
        return nullptr;
    }

    init_sound(mod);
    return mod;
}

void sd2_destroy(Sd2Module* mod) {
    if (mod == nullptr) {
        return;
    }

    if (mod->waveform_info != nullptr) {
        free(mod->waveform_info[0]); // free the flat data block
        free(mod->waveform_info);    // free the pointer array
    }

    if (mod->arpeggios != nullptr) {
        free(mod->arpeggios[0]);
        free(mod->arpeggios);
    }

    if (mod->vibratoes != nullptr) {
        free(mod->vibratoes[0]);
        free(mod->vibratoes);
    }

    free(mod->sequences);

    if (mod->tracks != nullptr) {
        for (int i = 0; i < mod->track_count; i++) {
            free(mod->tracks[i].track_data);
        }
        free(mod->tracks);
    }
    free(mod->image);

    free(mod->instruments);

    if (mod->samples != nullptr) {
        for (int i = 0; i < mod->sample_count; i++) {
            free(mod->samples[i].sample_data);
        }
        free(mod->samples);
    }

    free(mod->sample_negate_info);
    free(mod);
}

// Public API

int sd2_subsong_count(const Sd2Module* mod) {
    if (mod == nullptr) {
        return 0;
    }
    return 1;
}

bool sd2_start(Sd2Module* mod, int subsong, NuadeVoices* voices) {
    if (mod == nullptr || voices == nullptr || subsong != 0) {
        return false;
    }
    mod->nuade_voices = voices;
    init_sound(mod);
    return true;
}

int sd2_channel_count(const Sd2Module* mod) {
    if (mod == nullptr) {
        return 0;
    }
    return 4;
}

// Sd2Track editing API

// Decoded cell for internal use
typedef struct DecodedCell {
    uint8_t note;       // 0 = empty, 1-72 = note
    uint8_t instrument; // 0 = none
    uint8_t effect;     // 0 = none
    uint8_t param;      // 0 = none
} DecodedCell;

// Decode an entire track into a fixed-size cell array.
// Returns the number of rows decoded.
static int decode_track(const Sd2Track* trk, DecodedCell* cells, int max_rows) {
    if (trk == nullptr || trk->track_data == nullptr || trk->length == 0) {
        return 0;
    }

    const uint8_t* data = trk->track_data;
    const uint32_t len = trk->length;
    uint32_t pos = 0;
    int row = 0;
    int empty_counter = 0;

    while (pos < len && row < max_rows) {
        if (empty_counter > 0) {
            // Still counting down empty rows
            cells[row].note = 0;
            cells[row].instrument = 0;
            cells[row].effect = 0;
            cells[row].param = 0;
            row++;
            empty_counter--;
            continue;
        }

        int8_t v = (int8_t)data[pos++];

        if (v == 0) {
            // Effect-only row (no note): next 2 bytes are effect + param
            cells[row].note = 0;
            cells[row].instrument = 0;
            if (pos + 1 < len) {
                cells[row].effect = data[pos++];
                cells[row].param = data[pos++];
            } else {
                cells[row].effect = 0;
                cells[row].param = 0;
            }
            row++;
        } else if (v > 0) {
            if (v >= 0x70) {
                // Effect byte (>= 112), no note
                cells[row].note = 0;
                cells[row].instrument = 0;
                cells[row].effect = (uint8_t)v;
                cells[row].param = (pos < len) ? data[pos++] : 0;
                row++;
            } else {
                // Note (1-0x6F)
                cells[row].note = (uint8_t)v;
                cells[row].instrument = 0;
                cells[row].effect = 0;
                cells[row].param = 0;

                // Matches get_note2() logic exactly:
                // After note, read next byte as int8_t
                if (pos < len) {
                    int8_t next = (int8_t)data[pos++];
                    if (next >= 0) {
                        if (next >= 0x70) {
                            // Effect follows note (no instrument)
                            cells[row].effect = (uint8_t)next;
                            cells[row].param = (pos < len) ? data[pos++] : 0;
                        } else {
                            // Sd2Instrument follows note (0 = instrument 0)
                            cells[row].instrument = (uint8_t)next;

                            // Check for effect after instrument
                            if (pos < len) {
                                int8_t next2 = (int8_t)data[pos++];
                                if (next2 >= 0) {
                                    cells[row].effect = (uint8_t)next2;
                                    cells[row].param = (pos < len) ? data[pos++] : 0;
                                } else {
                                    // Negative = empty counter
                                    empty_counter = (int)(~(int)next2);
                                }
                            }
                        }
                    } else {
                        // Negative after note = empty counter
                        empty_counter = (int)(~(int)next);
                    }
                }
                row++;
            }
        } else {
            // Negative byte: empty row + set counter for subsequent empty rows
            cells[row].note = 0;
            cells[row].instrument = 0;
            cells[row].effect = 0;
            cells[row].param = 0;
            empty_counter = (int)(~(int)v);
            row++;
        }
    }

    // Fill remaining rows from empty_counter
    while (empty_counter > 0 && row < max_rows) {
        cells[row].note = 0;
        cells[row].instrument = 0;
        cells[row].effect = 0;
        cells[row].param = 0;
        row++;
        empty_counter--;
    }

    return row;
}

// Encode cells back to variable-length track format matching get_note2() decoder.
// Returns the number of bytes written.
//
// After note + instrument with no effect, the decoder reads the next byte as
// effect (if >= 0) or empty_counter (if < 0). To prevent the next row's first byte
// from being consumed as effect, we interpose a 0xFF separator (-1 as int8_t,
// giving empty_counter = ~(-1) = 0, consuming the byte harmlessly).
static uint32_t encode_track(const DecodedCell* cells, int num_rows, uint8_t* out, uint32_t out_capacity) {
    uint32_t pos = 0;
    int row = 0;

    while (row < num_rows && pos < out_capacity) {
        // Count consecutive empty rows
        int empty_run = 0;
        int scan = row;
        while (scan < num_rows && cells[scan].note == 0 && cells[scan].instrument == 0 && cells[scan].effect == 0
               && cells[scan].param == 0) {
            empty_run++;
            scan++;
        }

        if (empty_run > 0) {
            // Negative byte v: current row empty, empty_counter = ~v for subsequent rows.
            // For N empty rows: v = ~(N-1). Max N=128 per byte (v = -128).
            while (empty_run > 0 && pos < out_capacity) {
                int chunk = (empty_run > 128) ? 128 : empty_run;
                out[pos++] = (uint8_t)(int8_t)(~(chunk - 1));
                empty_run -= chunk;
                row += chunk;
            }
            continue;
        }

        const DecodedCell* c = &cells[row];

        if (c->note == 0) {
            // Effect-only row
            if (c->effect >= 0x70) {
                if (pos + 1 >= out_capacity)
                    break;
                out[pos++] = c->effect;
                out[pos++] = c->param;
            } else {
                if (pos + 2 >= out_capacity)
                    break;
                out[pos++] = 0;
                out[pos++] = c->effect;
                out[pos++] = c->param;
            }
        } else {
            // Note row
            if (pos >= out_capacity)
                break;
            out[pos++] = c->note;

            if (c->effect >= 0x70 && c->instrument == 0) {
                // Effect >= 0x70 directly after note (decoder reads as effect, not instrument)
                if (pos + 1 >= out_capacity)
                    break;
                out[pos++] = c->effect;
                out[pos++] = c->param;
            } else {
                // Emit instrument (always, decoder consumes next byte as instrument if < 0x70)
                if (pos >= out_capacity)
                    break;
                out[pos++] = c->instrument;

                if (c->effect != 0) {
                    if (pos + 1 >= out_capacity)
                        break;
                    out[pos++] = c->effect;
                    out[pos++] = c->param;
                } else if (row + 1 < num_rows && pos < out_capacity) {
                    // No effect: decoder will consume next byte as effect/empty_counter.
                    // Interpose 0xFF separator so next row's data isn't eaten.
                    out[pos++] = 0xFF;
                }
            }
        }

        row++;
    }

    return pos;
}

int sd2_get_num_tracks(const Sd2Module* mod) {
    if (mod == nullptr)
        return 0;
    return mod->track_count;
}

int sd2_get_track_length(const Sd2Module* mod, int track_idx) {
    if (mod == nullptr || track_idx < 0 || track_idx >= mod->track_count)
        return 0;
    DecodedCell cells[256];
    return decode_track(&mod->tracks[track_idx], cells, 256);
}

uint32_t sd2_get_cell(const Sd2Module* mod, int track_idx, int row) {
    if (mod == nullptr || track_idx < 0 || track_idx >= mod->track_count)
        return 0;
    DecodedCell cells[256];
    int num_rows = decode_track(&mod->tracks[track_idx], cells, 256);
    if (row < 0 || row >= num_rows)
        return 0;
    return ((uint32_t)cells[row].note << 24) | ((uint32_t)cells[row].instrument << 16)
           | ((uint32_t)cells[row].effect << 8) | (uint32_t)cells[row].param;
}

void sd2_set_cell(Sd2Module* mod, int track_idx, int row, int note, int instrument, int effect, int param) {
    if (mod == nullptr || track_idx < 0 || track_idx >= mod->track_count)
        return;

    Sd2Track* trk = &mod->tracks[track_idx];
    DecodedCell cells[256];
    int num_rows = decode_track(trk, cells, 256);

    // Extend with empty rows if needed
    if (row >= num_rows && row < 256) {
        for (int i = num_rows; i <= row; i++) {
            cells[i].note = 0;
            cells[i].instrument = 0;
            cells[i].effect = 0;
            cells[i].param = 0;
        }
        num_rows = row + 1;
    }
    if (row < 0 || row >= num_rows)
        return;

    cells[row].note = (uint8_t)note;
    cells[row].instrument = (uint8_t)instrument;
    cells[row].effect = (uint8_t)effect;
    cells[row].param = (uint8_t)param;

    // Re-encode: worst case is 4 bytes per row (note + instr + effect + param)
    uint8_t encoded[1024];
    uint32_t encoded_len = encode_track(cells, num_rows, encoded, sizeof(encoded));

    // Replace track data
    uint8_t* new_data = (uint8_t*)malloc(encoded_len);
    if (new_data == nullptr)
        return;
    memcpy(new_data, encoded, encoded_len);
    free(trk->track_data);
    trk->track_data = new_data;
    trk->length = encoded_len;
    trk->edited = true;
}

// Sd2Instrument preview API

int sd2_get_instrument_count(const Sd2Module* mod) {
    if (mod == nullptr)
        return 0;
    return mod->instrument_count;
}

void sd2_preview_note_on(Sd2Module* mod, int instrument, int note, int velocity) {
    if (mod == nullptr)
        return;
    if (instrument < 0 || instrument >= mod->instrument_count)
        return;
    if (note < 1 || note >= (int)PERIODS_COUNT)
        return;

    Sd2VoiceInfo* vi = &mod->voices[0];
    Sd2VoiceRender* vr = &mod->voice_render[0];

    // Reset voice state for preview
    vi->sample_volume = 0;
    vi->wave_list_delay = 0;
    vi->wave_list_offset = 0;
    vi->arpeggio_delay = 0;
    vi->arpeggio_offset = 0;
    vi->vibrato_delay = 0;
    vi->vibrato_offset = 0;
    vi->pitch_bend_counter = 0;
    vi->pitch_bend_value = 0;
    vi->note_slide_speed = 0;
    vi->envelope_state = Sd2EnvelopeState_Attack;
    vi->sustain_counter = 0;
    vi->current_note = (uint16_t)note;
    vi->current_instrument = (uint16_t)(instrument + 1); // 1-based for play_voice
    vi->current_effect = 0;
    vi->current_effect_arg = 0;

    // Set instrument
    vi->instrument = &mod->instruments[instrument];
    Sd2Instrument* inst = vi->instrument;

    // Look up sample from waveform list
    if (inst->waveform_list_number < mod->waveform_count) {
        uint8_t* waveform_list = mod->waveform_info[inst->waveform_list_number];
        vi->current_sample = waveform_list[0];

        if (vi->current_sample < (uint16_t)mod->sample_count) {
            Sd2Sample* smp = &mod->samples[vi->current_sample];
            vi->sample_data = smp->sample_data;
            vi->sample_length = smp->length;
            vi->loop_sample = smp->sample_data;
            vi->loop_offset = smp->loop_start;
            vi->loop_length = smp->loop_length;
        }
    }

    // Set period from note + arpeggio offset
    if (inst->arpeggio_number < mod->arpeggio_count) {
        int8_t* arpeggio = mod->arpeggios[inst->arpeggio_number];
        int final_note = note + arpeggio[0];
        if (final_note >= 0 && final_note < (int)PERIODS_COUNT) {
            vi->original_note = (uint16_t)final_note;
            vi->sample_period = s_periods[final_note];
        }
    } else {
        vi->original_note = (uint16_t)note;
        vi->sample_period = s_periods[note];
    }

    // Scale volume by velocity (0-127 mapped to multiplier)
    (void)velocity; // Volume is handled by ADSR envelope, velocity unused in SidMon II

    // Start sample playback on voice render
    if (vi->sample_length != 0) {
        voice_render_play_sample(vr, vi->sample_data, vi->sample_length, vi->loop_sample, vi->loop_offset,
                                 vi->loop_length);
    }

    vr->period = vi->sample_period;
}

void sd2_preview_note_off(Sd2Module* mod) {
    if (mod == nullptr)
        return;
    Sd2VoiceInfo* vi = &mod->voices[0];
    Sd2VoiceRender* vr = &mod->voice_render[0];

    // Force release state
    vi->envelope_state = Sd2EnvelopeState_Release;

    // Mute immediately
    vr->volume = 0;
    vi->sample_volume = 0;
    voice_render_mute(vr);
}

// Serialize module to binary

static void write_u8(uint8_t* buf, uint32_t* pos, uint8_t val) {
    buf[(*pos)++] = val;
}

static void write_be16(uint8_t* buf, uint32_t* pos, uint16_t val) {
    buf[(*pos)++] = (uint8_t)(val >> 8);
    buf[(*pos)++] = (uint8_t)(val & 0xFF);
}

static void write_be32(uint8_t* buf, uint32_t* pos, uint32_t val) {
    buf[(*pos)++] = (uint8_t)(val >> 24);
    buf[(*pos)++] = (uint8_t)((val >> 16) & 0xFF);
    buf[(*pos)++] = (uint8_t)((val >> 8) & 0xFF);
    buf[(*pos)++] = (uint8_t)(val & 0xFF);
}

uint32_t sd2_serialize(const Sd2Module* mod, uint8_t** out_data) {
    if (mod == nullptr || out_data == nullptr)
        return 0;

    // Calculate sizes of each block
    uint32_t num_pos = (uint32_t)mod->num_positions;
    uint32_t pos_table_len = num_pos * 4; // 4 channels * num_positions per channel

    // Block 0-2: sequence data (position, noteTranspose, instrumentTranspose)
    uint32_t seq_block_len = pos_table_len; // same for all 3 passes

    // Block 3: instruments (32 bytes each, instrument 0 excluded from file)
    uint32_t instr_count = (uint32_t)(mod->instrument_count > 0 ? mod->instrument_count - 1 : 0);
    uint32_t instr_block_len = instr_count * 32;

    // Block 4: waveform lists (16 bytes each)
    uint32_t wave_block_len = (uint32_t)mod->waveform_count * 16;

    // Block 5: arpeggios (16 bytes each)
    uint32_t arp_block_len = (uint32_t)mod->arpeggio_count * 16;

    // Block 6: vibratoes (16 bytes each)
    uint32_t vib_block_len = (uint32_t)mod->vibrato_count * 16;

    // Block 7: sample info (64 bytes each)
    uint32_t sample_info_len = (uint32_t)mod->sample_count * 64;

    // Block 8: track offset table (2 bytes per track)
    uint32_t track_table_len = (uint32_t)mod->track_count * 2;

    // Block 9: track data (re-encode all tracks)
    // First pass: compute track data sizes
    uint32_t* track_sizes = (uint32_t*)calloc((uint32_t)mod->track_count, sizeof(uint32_t));
    uint8_t** track_encoded = (uint8_t**)calloc((uint32_t)mod->track_count, sizeof(uint8_t*));
    // 64-bit: overlapping tracks in a damaged file can add up to more than 4 GiB.
    uint64_t tracks_total_len = 0;

    for (int i = 0; i < mod->track_count; i++) {
        // Use existing track data directly (already encoded)
        track_sizes[i] = mod->tracks[i].length;
        track_encoded[i] = mod->tracks[i].track_data;
        tracks_total_len += track_sizes[i];
    }

    // ID string
    const char* id_str = "SIDMON II - THE MIDI VERSION";
    uint32_t id_len = 28;

    // song_length block: 2 bytes
    uint32_t song_len_block = 2;

    // Header layout (big endian):
    //   0-1   MIDI mode
    //   2     num_positions
    //   3     speed
    //   4-5   num_samples * 64
    //   6-9   id_length
    //   10-13 song_length_length
    //   14-53 ten block lengths (4 bytes each)
    // The data starts at the fixed offset 58, so offsets 54-57 are not read. The id string follows at 58, then the
    // song_length block at 58 + id_length, then the data blocks. For "SIDMON II - THE MIDI VERSION" (id_length 28)
    // and a 2 byte song_length block, the blocks start at 88.

    uint32_t header_size = 58;
    // Total data size after header:
    uint64_t total_data = id_len + song_len_block + seq_block_len * 3 // blocks 0, 1, 2
                          + instr_block_len                           // block 3
                          + wave_block_len                            // block 4
                          + arp_block_len                             // block 5
                          + vib_block_len                             // block 6
                          + sample_info_len                           // block 7
                          + track_table_len                           // block 8
                          + tracks_total_len;                         // block 9

    // Sd2Sample PCM data
    uint32_t sample_pcm_total = 0;
    for (int i = 0; i < mod->sample_count; i++) {
        sample_pcm_total += mod->samples[i].length;
    }

    // Word-align after track data if needed
    uint32_t padding = 0;
    uint64_t after_tracks = header_size + total_data;
    if (after_tracks & 1)
        padding = 1;

    const uint64_t total_size64 = header_size + total_data + padding + sample_pcm_total;
    if (total_size64 > UINT32_MAX) {
        free(track_sizes);
        free(track_encoded);
        return 0;
    }
    uint32_t total_size = (uint32_t)total_size64;

    uint8_t* buf = (uint8_t*)calloc(total_size, 1);
    if (buf == nullptr) {
        free(track_sizes);
        free(track_encoded);
        return 0;
    }

    uint32_t p = 0;

    // -- Header (58 bytes) --
    // MIDI mode (2 bytes) - write 0
    write_be16(buf, &p, 0);
    // num_positions (1 byte) - stored decremented
    write_u8(buf, &p, (uint8_t)(mod->num_positions - 1));
    // speed (1 byte)
    write_u8(buf, &p, mod->start_speed);
    // num_samples * 64 (2 bytes BE)
    write_be16(buf, &p, (uint16_t)(mod->sample_count * 64));

    // Block lengths (id_length, song_len_length, then 10 block lengths)
    write_be32(buf, &p, id_len);         // id_length
    write_be32(buf, &p, song_len_block); // song_len_length

    // 10 block lengths:
    // 0: positionTable, 1: noteTranspose, 2: instrumentTranspose
    // 3: instruments, 4: waveformList, 5: arpeggios
    // 6: vibratoes, 7: sampleInfo, 8: trackTable, 9: tracks
    write_be32(buf, &p, seq_block_len);              // block 0: positionTable
    write_be32(buf, &p, seq_block_len);              // block 1: noteTranspose
    write_be32(buf, &p, seq_block_len);              // block 2: instrumentTranspose
    write_be32(buf, &p, instr_block_len);            // block 3: instruments
    write_be32(buf, &p, wave_block_len);             // block 4: waveformList
    write_be32(buf, &p, arp_block_len);              // block 5: arpeggios
    write_be32(buf, &p, vib_block_len);              // block 6: vibratoes
    write_be32(buf, &p, sample_info_len);            // block 7: sampleInfo
    write_be32(buf, &p, track_table_len);            // block 8: trackTable
    write_be32(buf, &p, (uint32_t)tracks_total_len); // block 9: tracks

    // Pad to offset 58 (header has: 2+1+1+2 + 4+4 + 10*4 = 54 bytes, need 4 more)
    while (p < header_size) {
        write_u8(buf, &p, 0);
    }

    // -- ID string --
    memcpy(buf + p, id_str, id_len);
    p += id_len;

    // -- Song length block (2 bytes) --
    // Not entirely clear what goes here; the parser skips it. Write 0.
    write_be16(buf, &p, 0);

    // -- Block 0: position table --
    for (int ch = 0; ch < 4; ch++) {
        for (int j = 0; j < (int)num_pos; j++) {
            int idx = ch * (int)num_pos + j;
            write_u8(buf, &p, mod->sequences[idx].track_number);
        }
    }

    // -- Block 1: note transpose --
    for (int ch = 0; ch < 4; ch++) {
        for (int j = 0; j < (int)num_pos; j++) {
            int idx = ch * (int)num_pos + j;
            write_u8(buf, &p, (uint8_t)mod->sequences[idx].note_transpose);
        }
    }

    // -- Block 2: instrument transpose --
    for (int ch = 0; ch < 4; ch++) {
        for (int j = 0; j < (int)num_pos; j++) {
            int idx = ch * (int)num_pos + j;
            write_u8(buf, &p, (uint8_t)mod->sequences[idx].instrument_transpose);
        }
    }

    // -- Block 3: instruments (32 bytes each, skip instrument 0) --
    for (int i = 1; i < mod->instrument_count; i++) {
        Sd2Instrument* inst = &mod->instruments[i];
        write_u8(buf, &p, inst->waveform_list_number);
        write_u8(buf, &p, inst->waveform_list_length);
        write_u8(buf, &p, inst->waveform_list_speed);
        write_u8(buf, &p, inst->waveform_list_delay);
        write_u8(buf, &p, inst->arpeggio_number);
        write_u8(buf, &p, inst->arpeggio_length);
        write_u8(buf, &p, inst->arpeggio_speed);
        write_u8(buf, &p, inst->arpeggio_delay);
        write_u8(buf, &p, inst->vibrato_number);
        write_u8(buf, &p, inst->vibrato_length);
        write_u8(buf, &p, inst->vibrato_speed);
        write_u8(buf, &p, inst->vibrato_delay);
        write_u8(buf, &p, (uint8_t)inst->pitch_bend_speed);
        write_u8(buf, &p, inst->pitch_bend_delay);
        write_u8(buf, &p, 0); // skip byte
        write_u8(buf, &p, 0); // skip byte
        write_u8(buf, &p, inst->attack_max);
        write_u8(buf, &p, inst->attack_speed);
        write_u8(buf, &p, inst->decay_min);
        write_u8(buf, &p, inst->decay_speed);
        write_u8(buf, &p, inst->sustain_time);
        write_u8(buf, &p, inst->release_min);
        write_u8(buf, &p, inst->release_speed);
        // 9 padding bytes
        for (int j = 0; j < 9; j++)
            write_u8(buf, &p, 0);
    }

    // -- Block 4: waveform lists (16 bytes each) --
    for (int i = 0; i < mod->waveform_count; i++) {
        memcpy(buf + p, mod->waveform_info[i], 16);
        p += 16;
    }

    // -- Block 5: arpeggios (16 bytes each) --
    for (int i = 0; i < mod->arpeggio_count; i++) {
        memcpy(buf + p, mod->arpeggios[i], 16);
        p += 16;
    }

    // -- Block 6: vibratoes (16 bytes each) --
    for (int i = 0; i < mod->vibrato_count; i++) {
        memcpy(buf + p, mod->vibratoes[i], 16);
        p += 16;
    }

    // -- Block 7: sample info (64 bytes each) --
    uint32_t running_sample_offset = 0;
    for (int i = 0; i < mod->sample_count; i++) {
        Sd2Sample* smp = &mod->samples[i];
        Sd2SampleNegateInfo* ni = &mod->sample_negate_info[i];

        write_be32(buf, &p, 0); // sample data pointer (unused in file)
        write_be16(buf, &p, (uint16_t)(smp->length / 2));
        write_be16(buf, &p, (uint16_t)(smp->loop_start / 2));
        write_be16(buf, &p, (uint16_t)(smp->loop_length / 2));

        // Negate info
        write_be16(buf, &p, (uint16_t)(ni->start_offset / 2));
        write_be16(buf, &p, (uint16_t)(ni->end_offset / 2));
        write_be16(buf, &p, ni->loop_index);
        write_be16(buf, &p, ni->status);
        write_be16(buf, &p, (uint16_t)ni->speed);
        write_be32(buf, &p, (uint32_t)ni->position);
        write_be16(buf, &p, ni->index);
        write_be16(buf, &p, (uint16_t)ni->do_negation);

        write_be32(buf, &p, 0); // skip 4 bytes
        // Name: 32 bytes of zeros (names not stored in Sd2Module)
        for (int j = 0; j < 32; j++)
            write_u8(buf, &p, 0);

        running_sample_offset += smp->length;
    }

    // -- Block 8: track offset table --
    uint32_t running_offset = 0;
    for (int i = 0; i < mod->track_count; i++) {
        write_be16(buf, &p, (uint16_t)running_offset);
        running_offset += track_sizes[i];
    }

    // -- Block 9: track data --
    for (int i = 0; i < mod->track_count; i++) {
        if (track_encoded[i] != nullptr && track_sizes[i] > 0) {
            memcpy(buf + p, track_encoded[i], track_sizes[i]);
            p += track_sizes[i];
        }
    }

    // Padding for word alignment
    if (padding)
        write_u8(buf, &p, 0);

    // -- Sd2Sample PCM data --
    for (int i = 0; i < mod->sample_count; i++) {
        if (mod->samples[i].length > 0 && mod->samples[i].sample_data != nullptr) {
            memcpy(buf + p, mod->samples[i].sample_data, mod->samples[i].length);
            p += mod->samples[i].length;
        }
    }

    free(track_sizes);
    free(track_encoded);

    *out_data = buf;
    return p;
}

// Display

// The note column value of a period table index (semitones from C-0, less the column's octave shift); -1 outside the
// table.
static int note_semitone(int note) {
    return note > 0 && note < (int)PERIODS_COUNT ? nuade_period_to_note_shifted(s_periods[note], SD2_NOTE_OCTAVE_SHIFT)
                                                 : -1;
}

int sd2_order_count(const Sd2Module* mod) {
    return mod ? (int)mod->number_of_positions + 1 : 0;
}

int sd2_pattern_rows(const Sd2Module* mod) {
    return mod ? mod->pattern_length : 0;
}

Sd2VoiceState sd2_voice_state(const Sd2Module* mod, int voice) {
    Sd2VoiceState state = { .note = -1, .instrument = -1 };
    if (mod == nullptr || voice < 0 || voice >= SD2_VOICES)
        return state;
    const Sd2VoiceInfo* vi = &mod->voices[voice];
    state.note = note_semitone(vi->shown_note);
    state.instrument = vi->shown_instrument;
    state.order = mod->shown_position;
    state.row = mod->shown_row;
    state.speed = mod->speed;
    return state;
}

Sd2Cell sd2_pattern_cell(const Sd2Module* mod, int order, int voice, int row) {
    Sd2Cell cell = { .note = -1, .instrument = -1, .effect = -1, .param = 0 };
    if (mod == nullptr || voice < 0 || voice >= SD2_VOICES || order < 0 || order >= mod->num_positions || row < 0)
        return cell;

    const uint8_t* track;
    uint32_t length;
    track_bytes(mod, &mod->sequences[voice * mod->num_positions + order], &track, &length);

    int32_t position = 0;
    uint16_t empty_notes = 0;
    TrackRow decoded = { 0 };
    for (int i = 0; i <= row; i++) {
        decoded = read_track_row(track, (int32_t)length, &position, &empty_notes);
    }
    if (decoded.note != 0) {
        cell.note = note_semitone(decoded.note);
        if (decoded.instrument != 0)
            cell.instrument = decoded.instrument;
    }
    if (decoded.effect != 0) {
        cell.effect = decoded.effect;
        cell.param = decoded.effect_arg;
    }
    return cell;
}
