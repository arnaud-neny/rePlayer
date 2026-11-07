// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/futurecomposer.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include "nuade_tracker.h"

// Tables

static const uint16_t fc_periods[132] = {
    1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960,  906,  856,  808,  762,  720,  678,  640,  604,
    570,  538,  508,  480,  453,  428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,  214,  202,
    190,  180,  170,  160,  151,  143,  135,  127,  120,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,
    113,  113,  113,  3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1812, 1712, 1616, 1524, 1440,
    1356, 1280, 1208, 1140, 1076, 1016, 960,  906,  856,  808,  762,  720,  678,  640,  604,  570,  538,  508,  480,
    453,  428,  404,  381,  360,  339,  320,  302,  285,  269,  254,  240,  226,  214,  202,  190,  180,  170,  160,
    151,  143,  135,  127,  120,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113,  113

};

static const uint8_t fc_silent[8] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe1

};

#define FC_SILENT_LEN 8
#define FC_PERIODS_LEN 132
#define FC_SAMPLE_COUNT 90
#define FC_MAX_PATTERNS 256
#define FC_MAX_VOL_SEQUENCES 256
#define FC_MAX_SEQUENCES 32767
// Commands one sequence of a voice runs in a tick: jumps that go round in a damaged file stop here.
#define FC_MAX_SEQ_COMMANDS 1024

// Structs

// Big-endian reader
typedef struct FcReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} FcReader;

// Reader helpers

static void reader_init(FcReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool reader_eof(const FcReader* r) {
    return r->pos > r->size;
}

static uint8_t reader_u8(FcReader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static int8_t reader_i8(FcReader* r) {
    return (int8_t)reader_u8(r);
}

static uint16_t reader_b_u16(FcReader* r) {
    uint8_t hi = reader_u8(r);
    uint8_t lo = reader_u8(r);
    return (uint16_t)((hi << 8) | lo);
}

static uint32_t reader_b_u32(FcReader* r) {
    uint16_t hi = reader_b_u16(r);
    uint16_t lo = reader_b_u16(r);
    return ((uint32_t)hi << 16) | lo;
}

static void reader_skip(FcReader* r, size_t bytes) {
    r->pos += bytes;
}

static void reader_seek(FcReader* r, size_t pos) {
    r->pos = pos;
}

static void reader_read(FcReader* r, void* dst, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ((uint8_t*)dst)[i] = reader_u8(r);
    }
}

static void reader_read_signed(FcReader* r, int8_t* dst, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dst[i] = reader_i8(r);
    }
}

static bool reader_read_mark(FcReader* r, char* buf, int len) {
    for (int i = 0; i < len; i++) {
        buf[i] = (char)reader_u8(r);
    }
    buf[len] = '\0';
    return !reader_eof(r);
}

// Visit tracking

static void clear_visited(FcModule* m) {
    memset(m->visited, 0, sizeof(m->visited));
}

static void mark_visited(FcModule* m, int pos) {
    if (pos >= 0 && pos < 256)
        m->visited[pos] = 1;
}

static bool has_visited(const FcModule* m, int pos) {
    if (pos >= 0 && pos < 256)
        return m->visited[pos] != 0;
    return false;
}

// Channel operations (IChannel equivalent)

static void ch_mute(FcChannel* ch) {
    ch->active = false;
    ch->play_pending = false;
    ch->queue_pending = false;
}

static void ch_play_sample(FcChannel* ch, const int8_t* data, uint32_t length) {
    ch->head = (NuadeSample) { .data = data, .format = NuadeSampleFormat_S8, .length = length };
    ch->play_pending = true;
    ch->queue_pending = false;
    ch->active = true;
}

// The region Paula repeats once the head has played.
static void ch_set_loop(FcChannel* ch, const int8_t* data, uint32_t start, uint32_t length) {
    ch->next = (NuadeSample) {
        .data = data + start, .format = NuadeSampleFormat_S8, .length = length, .loop_length = length
    };
    ch->queue_pending = true;
}

static void ch_set_amiga_period(FcChannel* ch, uint16_t period) {
    ch->period = period;
}

static void ch_set_amiga_volume(FcChannel* ch, uint16_t vol) {
    if (vol > 64)
        vol = 64;
    ch->volume = vol;
}

// DoVolBend

static void do_vol_bend(FcVoiceInfo* voi) {
    voi->vol_bend_flag = !voi->vol_bend_flag;
    if (voi->vol_bend_flag) {
        voi->volume_bend_time--;
        voi->volume += (int8_t)voi->volume_bend_speed;

        if (voi->volume > 64) {
            voi->volume = 64;
            voi->volume_bend_time = 0;
        } else {
            if (voi->volume < 0) {
                voi->volume = 0;
                voi->volume_bend_time = 0;
            }
        }
    }
}

// NewNote

// Frequency sequence byte; reading past the sequence data ends the sequence.
static uint8_t frq_at(const FcModule* m, int pos) {
    return pos >= 0 && pos < m->frq_sequences_len ? m->frq_sequences[pos] : 0xe1;
}

// Patterns referenced by a sequence that do not exist play as pattern 0.
static FcPattern* pattern_at(const FcModule* m, uint8_t number) {
    return &m->patterns[number < m->pat_num ? number : 0];
}

static void fc_new_note(FcModule* m, uint32_t chan) {
    FcVoiceInfo* voi = &m->voice_data[chan];
    FcChannel* ch = &m->channels[chan];

    // Check for end of pattern or "END" mark in pattern
    if ((voi->pattern_pos == 32) || (voi->cur_pattern->rows[voi->pattern_pos].note == 0x49)) {
        // New position
        voi->song_pos++;
        voi->pattern_pos = 0;

        // Have we reached the end of the module
        if (voi->song_pos >= m->seq_num)
            voi->song_pos = 0;

        // Count the speed counter
        m->playing_info.spd_temp++;
        if (m->playing_info.spd_temp == 5) {
            m->playing_info.spd_temp = 1;

            // Get new replay speed
            if (m->sequences[voi->song_pos].speed != 0) {
                m->playing_info.re_sp_cnt = m->sequences[voi->song_pos].speed;
                m->playing_info.rep_spd = m->playing_info.re_sp_cnt;
            }
        }

        // Get pattern information
        voi->transpose = m->sequences[voi->song_pos].voice_seq[chan].transpose;
        voi->sound_transpose = m->sequences[voi->song_pos].voice_seq[chan].sound_transpose;

        voi->cur_pattern = pattern_at(m, m->sequences[voi->song_pos].voice_seq[chan].pattern);

        if (chan == 0) {
            if (has_visited(m, voi->song_pos))
                m->end_reached = true;

            mark_visited(m, voi->song_pos);
        }
    }

    voi->shown_song_pos = voi->song_pos;
    voi->shown_row = voi->pattern_pos;

    // Get the pattern row
    uint8_t note = voi->cur_pattern->rows[voi->pattern_pos].note;
    uint8_t info = voi->cur_pattern->rows[voi->pattern_pos].info;

    // Check to see if we need to make portamento
    if ((note != 0) || ((info & 0xc0) != 0)) {
        if (note != 0)
            voi->pitch = 0;

        if ((info & 0x80) != 0)
            voi->portamento = voi->pattern_pos < 31 ? voi->cur_pattern->rows[voi->pattern_pos + 1].info : 0;
        else
            voi->portamento = 0;
    }

    // Got any note
    note &= 0x7f;
    if (note != 0) {
        voi->cur_note = (int8_t)note;

        // Mute the channel
        m->playing_info.aud_temp[chan] = false;
        ch_mute(ch);

        // Find the volume sequence
        uint8_t inst = (uint8_t)((info & 0x3f) + voi->sound_transpose);
        voi->shown_note = voi->cur_note + voi->transpose;
        voi->shown_inst = inst < m->vol_num ? inst : -1;
        if (inst >= m->vol_num)
            inst = 0;
        else
            inst++;

        voi->volume_seq_pos = 0;
        voi->volume_counter = m->vol_sequences[inst].speed;
        voi->volume_speed = m->vol_sequences[inst].speed;
        voi->vol_sus_counter = 0;

        voi->vib_speed = m->vol_sequences[inst].vib_speed;
        voi->vib_flag = 0x40;
        voi->vib_depth = m->vol_sequences[inst].vib_depth;
        voi->vib_value = m->vol_sequences[inst].vib_depth;
        voi->vib_delay = (int8_t)m->vol_sequences[inst].vib_delay;
        voi->volume_seq = m->vol_sequences[inst].values;
        voi->volume_seq_len = FC_VOL_SEQ_LEN;

        // Find the frequency sequence
        voi->frequency_seq_start_offset = (uint16_t)(FC_SILENT_LEN + m->vol_sequences[inst].frq_number * 64);
        voi->frequency_seq_pos = 0;
        voi->sus_counter = 0;
    }

    // Go to the next pattern row
    voi->pattern_pos++;
}

// Effect

static void fc_effect(FcModule* m, uint32_t chan) {
    FcVoiceInfo* voi = &m->voice_data[chan];
    FcChannel* ch = &m->channels[chan];

    // Parse the frequency sequence commands
    int commands_left = FC_MAX_SEQ_COMMANDS;
    bool one_more;
    do {
        one_more = false;

        if (voi->sus_counter != 0) {
            voi->sus_counter--;
            break;
        }

        // Sustain counter is zero, run the next part of the sequence
        uint16_t seq_poi = (uint16_t)(voi->frequency_seq_start_offset + voi->frequency_seq_pos);
        if (seq_poi >= m->frq_sequences_len)
            break;

        bool parse_effect;
        do {
            uint8_t dat;
            parse_effect = false;

            // Get the next command in the sequence
            uint8_t cmd = frq_at(m, seq_poi++);

            // Check for end of sequence
            if (cmd == 0xe1)
                break;

            // Check for "loop to other part of sequence" command
            if (cmd == 0xe0) {
                dat = (uint8_t)(frq_at(m, seq_poi) & 0x3f);

                voi->frequency_seq_pos = dat;
                seq_poi = (uint16_t)(voi->frequency_seq_start_offset + dat);

                cmd = frq_at(m, seq_poi++);
            }

            // Check for all the effects
            switch (cmd) {
                // Set wave form
                case 0xe2: {
                    dat = frq_at(m, seq_poi++);

                    if (dat < 90) {
                        if (m->samp_info[dat].address != nullptr) {
                            ch_play_sample(ch, m->samp_info[dat].address, m->samp_info[dat].length);
                            if (m->samp_info[dat].loop_length > 2) {
                                if ((m->samp_info[dat].loop_start + m->samp_info[dat].loop_length)
                                    > m->samp_info[dat].length)
                                    ch_set_loop(ch, m->samp_info[dat].address, m->samp_info[dat].loop_start,
                                                (uint32_t)(m->samp_info[dat].length - m->samp_info[dat].loop_start));
                                else
                                    ch_set_loop(ch, m->samp_info[dat].address, m->samp_info[dat].loop_start,
                                                m->samp_info[dat].loop_length);
                            }
                        }
                    }

                    voi->volume_seq_pos = 0;
                    voi->volume_counter = 1;
                    voi->frequency_seq_pos += 2;
                    m->playing_info.aud_temp[chan] = true;
                    break;
                }

                // Set loop
                case 0xe4: {
                    if (m->playing_info.aud_temp[chan]) {
                        dat = frq_at(m, seq_poi++);

                        if (dat < FC_SAMPLE_COUNT && m->samp_info[dat].address != nullptr) {
                            ch_set_loop(ch, m->samp_info[dat].address, m->samp_info[dat].loop_start,
                                        m->samp_info[dat].loop_length);
                        }

                        voi->frequency_seq_pos += 2;
                    }
                    break;
                }

                // Set sample (multi sample)
                case 0xe9: {
                    m->playing_info.aud_temp[chan] = true;

                    dat = frq_at(m, seq_poi++);

                    if ((dat < 90) && (m->samp_info[dat].multi != nullptr)) {
                        FcMultiSample* mul_samp = m->samp_info[dat].multi;

                        dat = frq_at(m, seq_poi++);

                        if (dat < 20) {
                            if (mul_samp->sample[dat].address != nullptr) {
                                ch_play_sample(ch, mul_samp->sample[dat].address, mul_samp->sample[dat].length);
                                if (mul_samp->sample[dat].loop_length > 2) {
                                    if ((mul_samp->sample[dat].loop_start + mul_samp->sample[dat].loop_length)
                                        > mul_samp->sample[dat].length)
                                        ch_set_loop(ch, mul_samp->sample[dat].address, mul_samp->sample[dat].loop_start,
                                                    (uint32_t)(mul_samp->sample[dat].length
                                                               - mul_samp->sample[dat].loop_start));
                                    else
                                        ch_set_loop(ch, mul_samp->sample[dat].address, mul_samp->sample[dat].loop_start,
                                                    mul_samp->sample[dat].loop_length);
                                }
                            }
                        }

                        voi->volume_seq_pos = 0;
                        voi->volume_counter = 1;
                    }

                    voi->frequency_seq_pos += 3;
                    break;
                }

                // Pattern jump
                case 0xe7: {
                    parse_effect = true;

                    dat = frq_at(m, seq_poi);

                    seq_poi = (uint16_t)(FC_SILENT_LEN + dat * 64);
                    if (seq_poi >= m->frq_sequences_len)
                        seq_poi = 0;

                    voi->frequency_seq_start_offset = seq_poi;
                    voi->frequency_seq_pos = 0;
                    break;
                }

                // Pitch bend
                case 0xea: {
                    voi->pitch_bend_speed = (int8_t)frq_at(m, seq_poi++);
                    voi->pitch_bend_time = frq_at(m, seq_poi++);
                    voi->frequency_seq_pos += 3;
                    break;
                }

                // New sustain
                case 0xe8: {
                    voi->sus_counter = frq_at(m, seq_poi++);
                    voi->frequency_seq_pos += 2;

                    one_more = true;
                    break;
                }

                // New vibrato
                case 0xe3: {
                    voi->vib_speed = (int8_t)frq_at(m, seq_poi++);
                    voi->vib_depth = (int8_t)frq_at(m, seq_poi++);
                    voi->frequency_seq_pos += 3;
                    break;
                }
            }

            if (!parse_effect && !one_more) {
                seq_poi = (uint16_t)(voi->frequency_seq_start_offset + voi->frequency_seq_pos);
                voi->patt_transpose = (int8_t)frq_at(m, seq_poi);
                voi->frequency_seq_pos++;
            }
        } while (--commands_left > 0 && parse_effect);
    } while (one_more && commands_left > 0);

    // Parse the volume sequence commands
    if (voi->vol_sus_counter != 0)
        voi->vol_sus_counter--;
    else {
        if (voi->volume_bend_time != 0)
            do_vol_bend(voi);
        else {
            voi->volume_counter--;
            if (voi->volume_counter == 0) {
                voi->volume_counter = voi->volume_speed;

                int vol_commands_left = FC_MAX_SEQ_COMMANDS;
                bool parse_effect;
                do {
                    parse_effect = false;

                    // Check for end of sequence
                    if ((voi->volume_seq_pos >= FC_VOL_SEQ_LEN) || (voi->volume_seq[voi->volume_seq_pos] == 0xe1))
                        break;

                    switch (voi->volume_seq[voi->volume_seq_pos]) {
                        // Volume bend
                        case 0xea: {
                            voi->volume_bend_speed = voi->volume_seq[voi->volume_seq_pos + 1];
                            voi->volume_bend_time = voi->volume_seq[voi->volume_seq_pos + 2];
                            voi->volume_seq_pos += 3;

                            do_vol_bend(voi);
                            break;
                        }

                        // New volume sustain
                        case 0xe8: {
                            voi->vol_sus_counter = voi->volume_seq[voi->volume_seq_pos + 1];
                            voi->volume_seq_pos += 2;
                            break;
                        }

                        // Set new position
                        case 0xe0: {
                            voi->volume_seq_pos = (uint16_t)((voi->volume_seq[voi->volume_seq_pos + 1] & 0x3f) - 5);
                            parse_effect = true;
                            break;
                        }

                        // Set volume
                        default: {
                            voi->volume = (int8_t)(voi->volume_seq[voi->volume_seq_pos] & 0x7f);
                            voi->volume_seq_pos++;
                            break;
                        }
                    }
                } while (--vol_commands_left > 0 && parse_effect);
            }
        }
    }

    // Calculate the period
    int8_t note = voi->patt_transpose;
    if (note >= 0)
        note += (int8_t)(voi->cur_note + voi->transpose);

    note &= 0x7f;

    // Get the period
    uint16_t period = (uint8_t)note < FC_PERIODS_LEN ? fc_periods[(uint8_t)note] : 0;
    uint8_t vib_flag = voi->vib_flag;

    // Shall we vibrate?
    if (voi->vib_delay != 0)
        voi->vib_delay--;
    else {
        uint16_t vib_base = (uint16_t)(note * 2);
        int8_t vib_dep = (int8_t)(voi->vib_depth * 2);
        int8_t vib_val = voi->vib_value;

        if (((vib_flag & 0x80) == 0) || ((vib_flag & 0x01) == 0)) {
            if ((vib_flag & 0x20) != 0) {
                vib_val += voi->vib_speed;
                if (vib_val >= vib_dep) {
                    vib_flag &= 0xdf; // ~0x20
                    vib_val = vib_dep;
                }
            } else {
                vib_val -= voi->vib_speed;
                if (vib_val < 0) {
                    vib_flag |= 0x20;
                    vib_val = 0;
                }
            }

            voi->vib_value = vib_val;
        }

        vib_dep /= 2;
        vib_val -= vib_dep;
        vib_base += 160;

        while (vib_base < 256) {
            vib_val *= 2;
            vib_base += 24;
        }

        period += (uint16_t)(int16_t)vib_val;
    }

    voi->vib_flag = (uint8_t)(vib_flag ^ 0x01);

    // Do the portamento thing
    voi->port_flag = !voi->port_flag;
    if (voi->port_flag && (voi->portamento != 0)) {
        if (voi->portamento <= 31)
            voi->pitch -= voi->portamento;
        else
            voi->pitch += (uint16_t)(voi->portamento & 0x1f);
    }

    // Pitch bend
    voi->pitch_bend_flag = !voi->pitch_bend_flag;
    if (voi->pitch_bend_flag && (voi->pitch_bend_time != 0)) {
        voi->pitch_bend_time--;
        voi->pitch -= (uint16_t)voi->pitch_bend_speed;
    }

    period += voi->pitch;

    // Check for bounds
    if (period < 113)
        period = 113;
    else {
        if (period > 3424)
            period = 3424;
    }

    if (voi->volume < 0)
        voi->volume = 0;
    else {
        if (voi->volume > 64)
            voi->volume = 64;
    }

    // Play the period
    ch_set_amiga_period(ch, period);
    ch_set_amiga_volume(ch, (uint16_t)voi->volume);
}

// Play tick

static void play_tick(FcModule* m) {
    // Decrease replay speed counter
    m->playing_info.re_sp_cnt--;
    if (m->playing_info.re_sp_cnt == 0) {
        // Restore replay speed counter
        m->playing_info.re_sp_cnt = m->playing_info.rep_spd;

        // Get new note for each channel
        fc_new_note(m, 0);
        fc_new_note(m, 1);
        fc_new_note(m, 2);
        fc_new_note(m, 3);
    }

    // Calculate effects for each channel
    fc_effect(m, 0);
    fc_effect(m, 1);
    fc_effect(m, 2);
    fc_effect(m, 3);

    if (m->end_reached) {
        m->loop_count++;
        m->end_reached = false;
    }
}

// InitializeSound

static void initialize_sound(FcModule* m, int start_position) {
    // Initialize speed
    uint16_t spd = m->sequences[start_position].speed;
    if (spd == 0)
        spd = 3;

    m->playing_info.re_sp_cnt = spd;
    m->playing_info.rep_spd = spd;
    m->playing_info.spd_temp = 1;
    m->playing_info.aud_temp[0] = false;
    m->playing_info.aud_temp[1] = false;
    m->playing_info.aud_temp[2] = false;
    m->playing_info.aud_temp[3] = false;

    m->end_reached = false;
    m->loop_count = 0;

    // Initialize channel variables
    for (int i = 0; i < 4; i++) {
        FcVoiceInfo* voi = &m->voice_data[i];

        voi->pitch_bend_speed = 0;
        voi->pitch_bend_time = 0;
        voi->song_pos = (uint16_t)start_position;
        voi->cur_note = 0;
        voi->shown_song_pos = (uint16_t)start_position;
        voi->shown_row = 0;
        voi->shown_note = -1;
        voi->shown_inst = -1;
        voi->volume_seq = m->vol_sequences[0].values;
        voi->volume_seq_len = FC_VOL_SEQ_LEN;
        voi->volume_bend_speed = 0;
        voi->volume_bend_time = 0;
        voi->volume_seq_pos = 0;
        voi->volume_counter = 1;
        voi->volume_speed = 1;
        voi->vol_sus_counter = 0;
        voi->sus_counter = 0;
        voi->vib_speed = 0;
        voi->vib_depth = 0;
        voi->vib_value = 0;
        voi->vib_delay = 0;
        voi->vol_bend_flag = false;
        voi->port_flag = false;
        voi->pattern_pos = 0;
        voi->pitch_bend_flag = false;
        voi->patt_transpose = 0;
        voi->volume = 0;
        voi->vib_flag = 0;
        voi->portamento = 0;
        voi->frequency_seq_start_offset = 0;
        voi->frequency_seq_pos = 0;
        voi->pitch = 0;
        voi->cur_pattern = pattern_at(m, m->sequences[start_position].voice_seq[i].pattern);
        voi->transpose = m->sequences[start_position].voice_seq[i].transpose;
        voi->sound_transpose = m->sequences[start_position].voice_seq[i].sound_transpose;

        // Reset channel
        FcChannel* ch = &m->channels[i];
        memset(ch, 0, sizeof(FcChannel));
    }
}

// Loading

// A loop starts inside the sample and ends at or before its end.
static void clamp_loop(uint16_t* loop_start, uint16_t* loop_length, uint16_t length) {
    if (*loop_start >= length) {
        *loop_start = 0;
        *loop_length = 2;
    } else if (*loop_start + *loop_length > length) {
        *loop_length = (uint16_t)(length - *loop_start);
    }
}

static bool load_module(FcModule* m, const uint8_t* data, size_t size) {
    FcReader reader;
    reader_init(&reader, data, size);

    // Skip mark "FC14"
    reader_skip(&reader, 4);

    // Get the length of the sequences
    uint32_t seq_length = reader_b_u32(&reader);

    // Get the offsets into the file
    uint32_t pat_offset = reader_b_u32(&reader);
    uint32_t pat_length = reader_b_u32(&reader);

    uint32_t frq_offset = reader_b_u32(&reader);
    uint32_t frq_length = reader_b_u32(&reader);

    uint32_t vol_offset = reader_b_u32(&reader);
    uint32_t vol_length = reader_b_u32(&reader);

    uint32_t smp_offset = reader_b_u32(&reader);
    uint32_t wav_offset = reader_b_u32(&reader);

    if (reader_eof(&reader))
        return false;

    // A block cannot be longer than the file; this bounds every allocation made from the header.
    seq_length = seq_length < size ? seq_length : (uint32_t)size;
    pat_length = pat_length < size ? pat_length : (uint32_t)size;
    frq_length = frq_length < size ? frq_length : (uint32_t)size;
    vol_length = vol_length < size ? vol_length : (uint32_t)size;

    // Read the sample information
    m->samp_num = 10;

    int i;
    for (i = 0; i < 10; i++) {
        FcSample* samp = &m->samp_info[i];

        samp->address = nullptr;
        samp->length = (uint16_t)(reader_b_u16(&reader) * 2);
        samp->loop_start = reader_b_u16(&reader);
        samp->loop_length = (uint16_t)(reader_b_u16(&reader) * 2);
        samp->multi = nullptr;

        clamp_loop(&samp->loop_start, &samp->loop_length, samp->length);
    }

    // Read the wave table lengths
    for (i = 10; i < 90; i++) {
        FcSample* samp = &m->samp_info[i];

        samp->address = nullptr;
        samp->length = (uint16_t)(reader_u8(&reader) * 2);
        samp->loop_start = 0;
        samp->loop_length = samp->length;
        samp->multi = nullptr;
    }

    if (reader_eof(&reader))
        return false;

    // Find out how many wave tables that are used
    for (i = 89; i >= 10; i--) {
        if (m->samp_info[i].length != 0)
            break;
    }

    m->wav_num = (int16_t)(i - 9);

    // Allocate memory to hold the sequences
    uint32_t seq_count = seq_length / 13;
    m->seq_num = (int16_t)(seq_count < FC_MAX_SEQUENCES ? seq_count : FC_MAX_SEQUENCES);

    if (m->seq_num == 0) {
        m->sequences = (FcSequence*)calloc(1, sizeof(FcSequence));
        m->seq_num = 1;
        if (!m->sequences)
            return false;
    } else {
        m->sequences = (FcSequence*)calloc(m->seq_num, sizeof(FcSequence));
        if (!m->sequences)
            return false;

        // The reader should already be at the right position for sequences
        // (after the header: 4 mark + 4 seqlen + 8*4 offsets + 10*6 samples + 80*1 wavetable lengths = 140 bytes)
        // Actually sequences follow immediately after the sample info in the file

        for (i = 0; i < m->seq_num; i++) {
            FcSequence* seq = &m->sequences[i];

            seq->voice_seq[0].pattern = reader_u8(&reader);
            seq->voice_seq[0].transpose = reader_i8(&reader);
            seq->voice_seq[0].sound_transpose = reader_i8(&reader);

            seq->voice_seq[1].pattern = reader_u8(&reader);
            seq->voice_seq[1].transpose = reader_i8(&reader);
            seq->voice_seq[1].sound_transpose = reader_i8(&reader);

            seq->voice_seq[2].pattern = reader_u8(&reader);
            seq->voice_seq[2].transpose = reader_i8(&reader);
            seq->voice_seq[2].sound_transpose = reader_i8(&reader);

            seq->voice_seq[3].pattern = reader_u8(&reader);
            seq->voice_seq[3].transpose = reader_i8(&reader);
            seq->voice_seq[3].sound_transpose = reader_i8(&reader);

            seq->speed = reader_u8(&reader);
        }
    }

    // Allocate memory to hold the patterns
    uint32_t pat_count = pat_length / 64;
    m->pat_num = (int16_t)(pat_count < FC_MAX_PATTERNS ? pat_count : FC_MAX_PATTERNS);
    if (m->pat_num == 0)
        return false;
    m->patterns = (FcPattern*)calloc(m->pat_num, sizeof(FcPattern));
    if (!m->patterns)
        return false;

    // Read the patterns
    reader_seek(&reader, pat_offset);

    for (i = 0; i < m->pat_num; i++) {
        FcPattern* patt = &m->patterns[i];

        for (int j = 0; j < 32; j++) {
            patt->rows[j].note = reader_u8(&reader);
            patt->rows[j].info = reader_u8(&reader);
        }
    }

    if (reader_eof(&reader))
        return false;

    // Allocate memory to hold the frequency sequences
    if (frq_length > INT_MAX - FC_SILENT_LEN - 1)
        return false;
    m->frq_sequences_len = (int)(FC_SILENT_LEN + frq_length + 1);
    m->frq_sequences = (uint8_t*)malloc(m->frq_sequences_len);
    if (!m->frq_sequences)
        return false;

    // Copy silent sequence to the first block
    memcpy(m->frq_sequences, fc_silent, FC_SILENT_LEN);

    // Read the frequency sequences
    reader_seek(&reader, frq_offset);
    reader_read(&reader, m->frq_sequences + FC_SILENT_LEN, frq_length);

    // Set "end of sequence" mark
    m->frq_sequences[m->frq_sequences_len - 1] = 0xe1;

    // Allocate memory to hold the volume sequences
    uint32_t vol_count = vol_length / 64;
    m->vol_num = (int16_t)(vol_count < FC_MAX_VOL_SEQUENCES ? vol_count : FC_MAX_VOL_SEQUENCES);
    int total_vol = 1 + m->vol_num;
    m->vol_sequences = (FcVolSequence*)calloc(total_vol, sizeof(FcVolSequence));
    if (!m->vol_sequences)
        return false;

    // Set up silent volume sequence at index 0
    m->vol_sequences[0].speed = fc_silent[0];
    m->vol_sequences[0].frq_number = fc_silent[1];
    m->vol_sequences[0].vib_speed = (int8_t)fc_silent[2];
    m->vol_sequences[0].vib_depth = (int8_t)fc_silent[3];
    m->vol_sequences[0].vib_delay = fc_silent[4];

    // Copy remaining silent bytes into values (Silent has 8 bytes, first 4 are header, but values start at offset 4)
    // Actually: Silent[0] = speed, [1] = frqNumber, [2] = vibSpeed, [3] = vibDepth, [4] = vibDelay
    // Then values are [5], [6], [7] (3 bytes out of 59)
    memset(m->vol_sequences[0].values, 0, FC_VOL_SEQ_LEN);
    int silent_values_len = FC_SILENT_LEN - 5;
    if (silent_values_len > FC_VOL_SEQ_LEN)
        silent_values_len = FC_VOL_SEQ_LEN;
    // C# code: Array.Copy(Tables.Silent, 4, volSequences[0].Values, 0, Tables.Silent.Length - 4);
    // That copies Silent[4..7] into Values[0..3]
    memcpy(m->vol_sequences[0].values, fc_silent + 4, FC_SILENT_LEN - 4);

    // Read the volume sequences
    reader_seek(&reader, vol_offset);

    for (i = 1; i <= m->vol_num; i++) {
        FcVolSequence* vs = &m->vol_sequences[i];

        vs->speed = reader_u8(&reader);
        vs->frq_number = reader_u8(&reader);
        vs->vib_speed = reader_i8(&reader);
        vs->vib_depth = reader_i8(&reader);
        vs->vib_delay = reader_u8(&reader);

        reader_read(&reader, vs->values, FC_VOL_SEQ_LEN);
    }

    if (reader_eof(&reader))
        return false;

    // Load the samples
    reader_seek(&reader, smp_offset);

    for (i = 0; i < 10; i++) {
        if (m->samp_info[i].length != 0) {
            // Read the first 4 bytes to see if it's a multi sample
            char mark[5];
            reader_read_mark(&reader, mark, 4);

            if (strcmp(mark, "SSMP") == 0) {
                // Multi sample
                FcMultiSample* multi = (FcMultiSample*)calloc(1, sizeof(FcMultiSample));
                if (!multi)
                    return false;
                m->samp_info[i].multi = multi;
                uint32_t multi_offsets[20];

                for (int j = 0; j < 20; j++) {
                    multi_offsets[j] = reader_b_u32(&reader);

                    FcMultiSampleEntry* ms = &multi->sample[j];

                    ms->length = (uint16_t)(reader_b_u16(&reader) * 2);
                    ms->loop_start = reader_b_u16(&reader);
                    ms->loop_length = (uint16_t)(reader_b_u16(&reader) * 2);
                    ms->address = nullptr;
                    clamp_loop(&ms->loop_start, &ms->loop_length, ms->length);

                    // Skip pad bytes
                    reader_skip(&reader, 6);
                }

                // Decrement sample count for the container
                m->samp_num--;

                // Read the sample data
                size_t samp_start_offset = reader.pos;

                for (int j = 0; j < 20; j++) {
                    if (multi->sample[j].length != 0) {
                        reader_seek(&reader, samp_start_offset + multi_offsets[j]);
                        multi->sample[j].address = (int8_t*)malloc(multi->sample[j].length);
                        if (!multi->sample[j].address)
                            return false;
                        reader_read_signed(&reader, multi->sample[j].address, multi->sample[j].length);

                        // Skip pad bytes
                        reader_b_u16(&reader);

                        if (reader_eof(&reader)) {
                            // Partial load - still set what we have
                        }

                        m->samp_num++;
                    }
                }

            } else {
                // Normal sample - seek back 4 bytes
                reader.pos -= 4;

                m->samp_info[i].address = (int8_t*)malloc(m->samp_info[i].length);
                if (!m->samp_info[i].address)
                    return false;
                reader_read_signed(&reader, m->samp_info[i].address, m->samp_info[i].length);
            }
        }

        // Skip pad bytes
        reader_b_u16(&reader);

        if (reader_eof(&reader))
            return false;
    }

    // Load the wave tables
    reader_seek(&reader, wav_offset);

    for (i = 10; i < 90; i++) {
        if (m->samp_info[i].length != 0) {
            m->samp_info[i].address = (int8_t*)malloc(m->samp_info[i].length);
            if (!m->samp_info[i].address)
                return false;
            reader_read_signed(&reader, m->samp_info[i].address, m->samp_info[i].length);

            if (reader_eof(&reader))
                return false;
        }
    }

    return true;
}

// Voices

// Hand the registers the tick wrote to the voice. Channels 0 and 3 are left, 1 and 2 right.
static void sync_voice(FcModule* m, uint32_t index) {
    FcChannel* ch = &m->channels[index];

    if (!ch->active || ch->head.data == nullptr) {
        if (nuade_voice_is_playing(m->voices, index))
            nuade_voice_stop(m->voices, index);
    } else {
        if (ch->play_pending)
            nuade_voice_play(m->voices, index, &ch->head, 0);
        if (ch->queue_pending) {
            nuade_voice_queue(m->voices, index, &ch->next);
        }
    }
    ch->play_pending = false;
    ch->queue_pending = false;

    const bool silent = ch->period == 0;
    nuade_voice_set_pitch(m->voices, index, silent ? 0.0 : nuade_paula_period_hz(ch->period));
    nuade_voice_set_volume(m->voices, index, silent ? 0.0f : (float)ch->volume / 64.0f);
    nuade_voice_set_pan(m->voices, index, (index == 0 || index == 3) ? -1.0f : 1.0f);
}

bool fc_tick(FcModule* module) {
    for (uint32_t i = 0; i < FC_VOICES; i++)
        module->channels[i].active = module->channels[i].active && nuade_voice_is_playing(module->voices, i);

    const uint32_t loops = module->loop_count;
    play_tick(module);

    for (uint32_t i = 0; i < FC_VOICES; i++)
        sync_voice(module, i);
    return module->loop_count != loops;
}

// SMOD → FC14 converter (Future Composer 1.0-1.3)
//
// FC 1.3 and earlier write files with the "SMOD" magic and use a hardcoded set
// of wave tables baked into the player binary. FC 1.4 files ("FC14") carry the
// wave tables inline. The two formats are otherwise identical *except* that
// FC14 stores 80 extra wave-length bytes right after the sample info and a
// trailing wave-tables block.
//
// Ported 1:1 from NostalgicPlayer's FutureComposer13Format.cs converter
// (Thomas Neumann, MIT). We convert SMOD → FC14 in a freshly malloc'd buffer,
// then hand that buffer to the existing FC14 loader below.

// Per-wave length in the implicit FC 1.3 wave-table set (80 waves, u8 each,
// stored value = bytes / 2 to match FC14 sample-length convention).
static const uint8_t smod_wave_length[80]
    = { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x10, 0x08, 0x10, 0x10, 0x08, 0x08, 0x18, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Wave-table PCM data (1344 bytes; concatenation of all non-empty waves).
static const uint8_t smod_wave_tables[1344] = {
    0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8, 0x3f, 0x37, 0x2f,
    0x27, 0x1f, 0x17, 0x0f, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37, 0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8,
    0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8, 0xc0, 0x37, 0x2f, 0x27, 0x1f, 0x17, 0x0f, 0x07, 0xff,
    0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37, 0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8,
    0xe0, 0xd8, 0xd0, 0xc8, 0xc0, 0xb8, 0x2f, 0x27, 0x1f, 0x17, 0x0f, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f,
    0x37, 0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8, 0xc0, 0xb8,
    0xb0, 0x27, 0x1f, 0x17, 0x0f, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37, 0xc0, 0xc0, 0xd0, 0xd8, 0xe0,
    0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8, 0xc0, 0xb8, 0xb0, 0xa8, 0x1f, 0x17, 0x0f, 0x07,
    0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37, 0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0,
    0xe8, 0xe0, 0xd8, 0xd0, 0xc8, 0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x17, 0x0f, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27,
    0x2f, 0x37, 0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8, 0xc0,
    0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x0f, 0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37, 0xc0, 0xc0, 0xd0, 0xd8,
    0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8, 0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90,
    0x07, 0xff, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37, 0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8,
    0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8, 0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0xff, 0x07, 0x0f, 0x17, 0x1f,
    0x27, 0x2f, 0x37, 0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8,
    0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0x80, 0x07, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37, 0xc0, 0xc0, 0xd0,
    0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8, 0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98,
    0x90, 0x88, 0x80, 0x88, 0x0f, 0x17, 0x1f, 0x27, 0x2f, 0x37, 0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00,
    0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8, 0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0x80, 0x88, 0x90, 0x17,
    0x1f, 0x27, 0x2f, 0x37, 0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0,
    0xc8, 0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0x80, 0x88, 0x90, 0x98, 0x1f, 0x27, 0x2f, 0x37, 0xc0, 0xc0,
    0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8, 0xc0, 0xb8, 0xb0, 0xa8, 0xa0,
    0x98, 0x90, 0x88, 0x80, 0x88, 0x90, 0x98, 0xa0, 0x27, 0x2f, 0x37, 0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8,
    0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8, 0xd0, 0xc8, 0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0x80, 0x88, 0x90,
    0x98, 0xa0, 0xa8, 0x2f, 0x37, 0xc0, 0xc0, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0xf8, 0xf0, 0xe8, 0xe0, 0xd8,
    0xd0, 0xc8, 0xc0, 0xb8, 0xb0, 0xa8, 0xa0, 0x98, 0x90, 0x88, 0x80, 0x88, 0x90, 0x98, 0xa0, 0xa8, 0xb0, 0x37, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x7f, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x80, 0x80, 0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80,
    0x80, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x90, 0x98, 0xa0, 0xa8, 0xb0,
    0xb8, 0xc0, 0xc8, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0, 0xf8, 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48,
    0x50, 0x58, 0x60, 0x68, 0x70, 0x7f, 0x80, 0x80, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x00, 0x10, 0x20, 0x30, 0x40,
    0x50, 0x60, 0x70, 0x45, 0x45, 0x79, 0x7d, 0x7a, 0x77, 0x70, 0x66, 0x61, 0x58, 0x53, 0x4d, 0x2c, 0x20, 0x18, 0x12,
    0x04, 0xdb, 0xd3, 0xcd, 0xc6, 0xbc, 0xb5, 0xae, 0xa8, 0xa3, 0x9d, 0x99, 0x93, 0x8e, 0x8b, 0x8a, 0x45, 0x45, 0x79,
    0x7d, 0x7a, 0x77, 0x70, 0x66, 0x5b, 0x4b, 0x43, 0x37, 0x2c, 0x20, 0x18, 0x12, 0x04, 0xf8, 0xe8, 0xdb, 0xcf, 0xc6,
    0xbe, 0xb0, 0xa8, 0xa4, 0x9e, 0x9a, 0x95, 0x94, 0x8d, 0x83, 0x00, 0x00, 0x40, 0x60, 0x7f, 0x60, 0x40, 0x20, 0x00,
    0xe0, 0xc0, 0xa0, 0x80, 0xa0, 0xc0, 0xe0, 0x00, 0x00, 0x40, 0x60, 0x7f, 0x60, 0x40, 0x20, 0x00, 0xe0, 0xc0, 0xa0,
    0x80, 0xa0, 0xc0, 0xe0, 0x80, 0x80, 0x90, 0x98, 0xa0, 0xa8, 0xb0, 0xb8, 0xc0, 0xc8, 0xd0, 0xd8, 0xe0, 0xe8, 0xf0,
    0xf8, 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x7f, 0x80, 0x80,
    0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70
};

// Big-endian uint32 write helper
static inline void wr_be_u32(uint8_t* dst, uint32_t v) {
    dst[0] = (uint8_t)(v >> 24);
    dst[1] = (uint8_t)(v >> 16);
    dst[2] = (uint8_t)(v >> 8);
    dst[3] = (uint8_t)(v);
}

static inline uint32_t rd_be_u32(const uint8_t* src) {
    return ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) | ((uint32_t)src[2] << 8) | (uint32_t)src[3];
}

// Convert an SMOD (FC 1.0 - 1.3) module buffer to an equivalent FC14 module.
// Returns a freshly malloc'd buffer on success (caller frees) with size
// written to *out_size. Returns NULL on error.
static uint8_t* smod_to_fc14(const uint8_t* src, size_t src_size, size_t* out_size) {
    if (!src || src_size < 100 || !out_size)
        return nullptr;
    if (memcmp(src, "SMOD", 4) != 0)
        return nullptr;

    // Read SMOD header
    uint32_t seq_length = rd_be_u32(src + 4);
    uint32_t off[8];
    for (int i = 0; i < 8; i++)
        off[i] = rd_be_u32(src + 8 + i * 4);

    // Per NostalgicPlayer: if seqLength is zero, derive it from patOffset
    // (cult.fc workaround: some files leave the seq-length field empty).
    if (seq_length == 0) {
        if (off[0] < 0x100)
            return nullptr;
        seq_length = off[0] - 0x100;
    }

    // Sanity checks on the 6 length-bearing fields (pat, frq, vol)
    const uint32_t pat_offset = off[0];
    const uint32_t pat_length = off[1];
    const uint32_t frq_offset = off[2];
    const uint32_t frq_length = off[3];
    const uint32_t vol_offset = off[4];
    const uint32_t vol_length = off[5];
    const uint32_t smp_offset = off[6];
    if ((uint64_t)pat_offset + pat_length > src_size)
        return nullptr;
    if ((uint64_t)frq_offset + frq_length > src_size)
        return nullptr;
    if ((uint64_t)vol_offset + vol_length > src_size)
        return nullptr;
    if (smp_offset > src_size)
        return nullptr;
    if (100 + (uint64_t)seq_length > src_size)
        return nullptr;

    // FC14 stores seqLength rounded up to even
    const uint64_t even_seq_length = ((uint64_t)seq_length + 1u) & ~(uint64_t)1u;

    // Output size: header, sample info and wave lengths, the sequences with their even-alignment padding, the
    // pattern, frequency and volume blocks, the sample tail and the appended wave tables. FC14 offsets are 32 bits.
    const uint64_t cap64 = 4 + 4 + 8 * 4 + 60 + 80 + even_seq_length + 2 + (uint64_t)pat_length + frq_length
                           + vol_length + (src_size - smp_offset) + sizeof(smod_wave_tables);
    if (cap64 > UINT32_MAX)
        return nullptr;
    const uint32_t fc14_seq_length = (uint32_t)even_seq_length;
    const size_t cap = (size_t)cap64;
    uint8_t* out = (uint8_t*)calloc(1, cap);
    if (!out)
        return nullptr;

    // Output bump pointer
    size_t o = 0;

    // --- FC14 header ---
    memcpy(out + o, "FC14", 4);
    o += 4;
    wr_be_u32(out + o, fc14_seq_length);
    o += 4;
    // Reserve 8 offsets; fill later.
    size_t offsets_pos = o;
    o += 8 * 4;

    // --- Sample info (10 × 6 bytes, unchanged) ---
    // SMOD layout: sample info sits immediately after the 8 offsets (at src offset 40).
    if (40 + 60 > src_size) {
        free(out);
        return nullptr;
    }
    memcpy(out + o, src + 40, 60);
    o += 60;

    // --- 80 wave lengths ---
    memcpy(out + o, smod_wave_length, 80);
    o += 80;

    // --- Sequences (seq_length bytes from src; FC14 expects fc14_seq_length even) ---
    // Sequences follow the sample info in SMOD, so src pos is 40 + 60 = 100.
    if (100 + seq_length > src_size) {
        free(out);
        return nullptr;
    }
    memcpy(out + o, src + 100, seq_length);
    o += seq_length;
    if (fc14_seq_length > seq_length) {
        out[o++] = 0;
    }

    // --- Patterns (with portamento fix) ---
    // Even-align pattern offset (FC14 reader expects patOffset aligned).
    if (o & 1u) {
        out[o++] = 0;
    }
    uint32_t new_pat_offset = (uint32_t)o;
    if (pat_offset + pat_length > src_size) {
        free(out);
        return nullptr;
    }
    memcpy(out + o, src + pat_offset, pat_length);

    // Fix portamento: scan every odd byte within the pattern block. When bit 7
    // of pattBuf[i] is set, the slide target in pattBuf[i+2] must be doubled.
    // Layout: pairs of (note, info), where info bit 0x80 = portamento flag on
    // the *next* row's note byte's low 5 bits.
    if (pat_length >= 3) {
        uint8_t* p = out + o;
        for (uint32_t i = 1; i < pat_length - 2; i += 2) {
            if (p[i] & 0x80) {
                uint8_t v = p[i + 2];
                p[i + 2] = (uint8_t)(((((uint32_t)(v & 0x1f)) * 2u) & 0x1fu) | (v & 0x20u));
            }
        }
    }
    o += pat_length;

    // --- Frequency sequences (verbatim) ---
    uint32_t new_frq_offset = (uint32_t)o;
    if (frq_offset + frq_length > src_size) {
        free(out);
        return nullptr;
    }
    memcpy(out + o, src + frq_offset, frq_length);
    o += frq_length;

    // --- Volume sequences (verbatim) ---
    uint32_t new_vol_offset = (uint32_t)o;
    if (vol_offset + vol_length > src_size) {
        free(out);
        return nullptr;
    }
    memcpy(out + o, src + vol_offset, vol_length);
    o += vol_length;

    // --- Sample data ---
    // FC14 expects: for each of 10 samples, (sample bytes if length > 0) + 2 pad bytes.
    // SMOD shares this layout, so we copy straight from src + smp_offset to the end
    // of the sample-data section (= start of wave tables in FC14, which SMOD lacks).
    uint32_t new_smp_offset = (uint32_t)o;
    // Samples run from smp_offset to end-of-file in SMOD (everything past the vol-seq block
    // is sample data and the FC14 sample section must cover it). We copy the tail.
    if (smp_offset > src_size) {
        free(out);
        return nullptr;
    }
    size_t smp_bytes = src_size - smp_offset;
    memcpy(out + o, src + smp_offset, smp_bytes);
    o += smp_bytes;

    // --- Wave tables (appended) ---
    uint32_t new_wav_offset = (uint32_t)o;
    memcpy(out + o, smod_wave_tables, sizeof(smod_wave_tables));
    o += sizeof(smod_wave_tables);

    // --- Fill in the 8 offsets in the header ---
    // FC14 order: patOffset, patLength, frqOffset, frqLength, volOffset, volLength, smpOffset, wavOffset
    wr_be_u32(out + offsets_pos + 0, new_pat_offset);
    wr_be_u32(out + offsets_pos + 4, pat_length);
    wr_be_u32(out + offsets_pos + 8, new_frq_offset);
    wr_be_u32(out + offsets_pos + 12, frq_length);
    wr_be_u32(out + offsets_pos + 16, new_vol_offset);
    wr_be_u32(out + offsets_pos + 20, vol_length);
    wr_be_u32(out + offsets_pos + 24, new_smp_offset);
    wr_be_u32(out + offsets_pos + 28, new_wav_offset);

    *out_size = o;
    return out;
}

// Public API

bool fc_probe(const uint8_t* data, size_t size) {
    if (!data || size < 100)
        return false;
    if (memcmp(data, "FC14", 4) == 0)
        return size >= 180;
    if (memcmp(data, "SMOD", 4) == 0) {
        size_t converted_size = 0;
        uint8_t* converted = smod_to_fc14(data, size, &converted_size);
        const bool ok = converted != nullptr;
        free(converted);
        return ok;
    }
    return false;
}

FcModule* fc_create(const uint8_t* data, size_t size) {
    if (!data || size < 100)
        return nullptr;

    // Accept FC14 directly, or convert SMOD (FC 1.0-1.3) → FC14 first.
    uint8_t* converted = nullptr;
    size_t converted_size = 0;

    if (memcmp(data, "FC14", 4) == 0) {
        if (size < 180)
            return nullptr;
        // use data / size as-is
    } else if (memcmp(data, "SMOD", 4) == 0) {
        converted = smod_to_fc14(data, size, &converted_size);
        if (!converted)
            return nullptr;
        data = converted;
        size = converted_size;
    } else {
        return nullptr;
    }

    FcModule* m = (FcModule*)calloc(1, sizeof(FcModule));
    if (!m) {
        if (converted)
            free(converted);
        return nullptr;
    }

    m->is_smod = converted != nullptr;

    if (!load_module(m, data, size)) {
        fc_destroy(m);
        if (converted)
            free(converted);
        return nullptr;
    }

    // load_module has fully consumed `data` (samples/patterns copied out), so the temporary SMOD→FC14 buffer is
    // safe to release now.
    if (converted)
        free(converted);

    return m;
}

void fc_destroy(FcModule* module) {
    if (!module)
        return;

    // Free samples
    for (int i = 0; i < 90; i++) {
        if (module->samp_info[i].address)
            free(module->samp_info[i].address);

        if (module->samp_info[i].multi) {
            for (int j = 0; j < 20; j++) {
                if (module->samp_info[i].multi->sample[j].address)
                    free(module->samp_info[i].multi->sample[j].address);
            }
            free(module->samp_info[i].multi);
        }
    }

    if (module->sequences)
        free(module->sequences);
    if (module->patterns)
        free(module->patterns);
    if (module->frq_sequences)
        free(module->frq_sequences);
    if (module->vol_sequences)
        free(module->vol_sequences);

    free(module);
}

int fc_subsong_count(const FcModule* module) {
    if (!module)
        return 0;
    return 1; // FC14 has only one song
}

bool fc_start(FcModule* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong != 0 || module->seq_num <= 0)
        return false;

    module->voices = voices;
    clear_visited(module);
    initialize_sound(module, 0);
    return true;
}

const char* fc_format_name(const FcModule* module) {
    return module && module->is_smod ? "Future Composer 1.0-1.3" : "Future Composer 1.4";
}

// Display

// The note column value of a period table index (semitones from C-0, less the column's octave shift); -1 outside the
// table.
static int note_of_index(int index) {
    return index >= 0 && index < FC_PERIODS_LEN ? nuade_period_to_note_shifted(fc_periods[index], FC_NOTE_OCTAVE_SHIFT)
                                                : -1;
}

// Rows a voice plays of a pattern: up to the END mark, or all 32.
static int pattern_rows(const FcPattern* pattern) {
    for (int row = 0; row < 32; row++) {
        if (pattern->rows[row].note == 0x49)
            return row;
    }
    return 32;
}

int fc_order_count(const FcModule* module) {
    return module ? module->seq_num : 0;
}

int fc_pattern_rows(const FcModule* module, int order) {
    int rows = 0;
    if (!module || order < 0 || order >= module->seq_num)
        return 0;
    for (int voice = 0; voice < FC_VOICES; voice++) {
        const int n = pattern_rows(pattern_at(module, module->sequences[order].voice_seq[voice].pattern));
        rows = n > rows ? n : rows;
    }
    return rows;
}

FcVoiceState fc_voice_state(const FcModule* module, int voice) {
    FcVoiceState state = { .note = -1, .instrument = -1 };
    if (!module || voice < 0 || voice >= FC_VOICES)
        return state;
    const FcVoiceInfo* voi = &module->voice_data[voice];
    state.note = note_of_index(voi->shown_note);
    state.instrument = voi->shown_inst;
    state.order = voi->shown_song_pos;
    state.row = voi->shown_row;
    state.speed = module->playing_info.rep_spd;
    return state;
}

FcCell fc_cell(const FcModule* module, int order, int voice, int row) {
    FcCell cell = { .note = -1, .instrument = -1, .flags = 0 };
    if (!module || order < 0 || order >= module->seq_num || voice < 0 || voice >= FC_VOICES || row < 0 || row >= 32)
        return cell;
    const FcPattern* pattern = pattern_at(module, module->sequences[order].voice_seq[voice].pattern);
    if (row >= pattern_rows(pattern))
        return cell;
    const FcPatternRow* line = &pattern->rows[row];
    if ((line->note & 0x7f) != 0) {
        cell.note = note_of_index(line->note & 0x7f);
        cell.instrument = line->info & 0x3f;
    }
    cell.flags = line->info & 0xc0;
    return cell;
}
