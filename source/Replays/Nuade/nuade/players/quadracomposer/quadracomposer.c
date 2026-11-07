// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/quadracomposer.h>

#include "nuade_base.h"
#include "nuade_paula.h"
#include <nuade/voice.h>
#include <stdlib.h>
#include <string.h>

// The original loads the CIA timer with this over the tempo (PAL); the framework counts ticks in CIA cycles.
#define CIA_TIMER_NUMERATOR 1773447U

// Enums

typedef enum QcExtraEffect {
    QC_EXTRA_SET_FILTER = 0x00,
    QC_EXTRA_FINE_SLIDE_UP = 0x10,
    QC_EXTRA_FINE_SLIDE_DOWN = 0x20,
    QC_EXTRA_SET_GLISSANDO = 0x30,
    QC_EXTRA_SET_VIBRATO_WAVE = 0x40,
    QC_EXTRA_SET_FINE_TUNE = 0x50,
    QC_EXTRA_PATTERN_LOOP = 0x60,
    QC_EXTRA_SET_TREMOLO_WAVE = 0x70,
    QC_EXTRA_RETRIG_NOTE = 0x90,
    QC_EXTRA_FINE_VOL_SLIDE_UP = 0xa0,
    QC_EXTRA_FINE_VOL_SLIDE_DOWN = 0xb0,
    QC_EXTRA_NOTE_CUT = 0xc0,
    QC_EXTRA_NOTE_DELAY = 0xd0,
    QC_EXTRA_PATTERN_DELAY = 0xe0
} QcExtraEffect;

// Tables

static const uint16_t qc_periods[16][36] = {
    // Tuning 0
    { 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320,
      302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113 },
    // Tuning 1
    { 850, 802, 757, 715, 674, 637, 601, 567, 535, 505, 477, 450, 425, 401, 379, 357, 337, 318,
      300, 284, 268, 253, 239, 225, 213, 201, 189, 179, 169, 159, 150, 142, 134, 126, 119, 113 },
    // Tuning 2
    { 844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447, 422, 398, 376, 355, 335, 316,
      298, 282, 266, 251, 237, 224, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118, 112 },
    // Tuning 3
    { 838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470, 444, 419, 395, 373, 352, 332, 314,
      296, 280, 264, 249, 235, 222, 209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118, 111 },
    // Tuning 4
    { 832, 785, 741, 699, 660, 623, 588, 555, 524, 495, 467, 441, 416, 392, 370, 350, 330, 312,
      294, 278, 262, 247, 233, 220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 124, 117, 110 },
    // Tuning 5
    { 826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437, 413, 390, 368, 347, 328, 309,
      292, 276, 260, 245, 232, 219, 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116, 109 },
    // Tuning 6
    { 820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460, 434, 410, 387, 365, 345, 325, 307,
      290, 274, 258, 244, 230, 217, 205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115, 109 },
    // Tuning 7
    { 814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431, 407, 384, 363, 342, 323, 305,
      288, 272, 256, 242, 228, 216, 204, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114, 108 },
    // Tuning -8
    { 907, 856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339,
      320, 302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120 },
    // Tuning -7
    { 900, 850, 802, 757, 715, 675, 636, 601, 567, 535, 505, 477, 450, 425, 401, 379, 357, 337,
      318, 300, 284, 268, 253, 238, 225, 212, 200, 189, 179, 169, 159, 150, 142, 134, 126, 119 },
    // Tuning -6
    { 894, 844, 796, 752, 709, 670, 632, 597, 563, 532, 502, 474, 447, 422, 398, 376, 355, 335,
      316, 298, 282, 266, 251, 237, 223, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118 },
    // Tuning -5
    { 887, 838, 791, 746, 704, 665, 628, 592, 559, 528, 498, 470, 444, 419, 395, 373, 352, 332,
      314, 296, 280, 264, 249, 235, 222, 209, 198, 187, 176, 166, 157, 148, 140, 132, 125, 118 },
    // Tuning -4
    { 881, 832, 785, 741, 699, 660, 623, 588, 555, 524, 494, 467, 441, 416, 392, 370, 350, 330,
      312, 294, 278, 262, 247, 233, 220, 208, 196, 185, 175, 165, 156, 147, 139, 131, 123, 117 },
    // Tuning -3
    { 875, 826, 779, 736, 694, 655, 619, 584, 551, 520, 491, 463, 437, 413, 390, 368, 347, 328,
      309, 292, 276, 260, 245, 232, 219, 206, 195, 184, 174, 164, 155, 146, 138, 130, 123, 116 },
    // Tuning -2
    { 868, 820, 774, 730, 689, 651, 614, 580, 547, 516, 487, 460, 434, 410, 387, 365, 345, 325,
      307, 290, 274, 258, 244, 230, 217, 205, 193, 183, 172, 163, 154, 145, 137, 129, 122, 115 },
    // Tuning -1
    { 862, 814, 768, 725, 684, 646, 610, 575, 543, 513, 484, 457, 431, 407, 384, 363, 342, 323,
      305, 288, 272, 256, 242, 228, 216, 203, 192, 181, 171, 161, 152, 144, 136, 128, 121, 114 }

};

static const int8_t qc_arpeggio_offsets[3] = { -1, 0, 1 };

static const int16_t qc_vibrato[3][64]
    = { { 0,      3211,   6392,   9511,   12539,  15446,  18204,  20787,  23169,  25329,  27244,  28897,  30272,
          31356,  32137,  32609,  32767,  32609,  32137,  31356,  30272,  28897,  27244,  25329,  23169,  20787,
          18204,  15446,  12539,  9511,   6392,   3211,   0,      -3211,  -6392,  -9511,  -12539, -15446, -18204,
          -20787, -23169, -25329, -27244, -28897, -30272, -31356, -32137, -32609, -32767, -32609, -32137, -31356,
          -30272, -28897, -27244, -25329, -23169, -20787, -18204, -15446, -12539, -9511,  -6392,  -3211 },
        { 32767,  31744,  30720,  29696,  28672,  27648,  26624,  25600,  24576,  23552,  22528,  21504,  20480,
          19456,  18432,  17408,  16384,  15360,  14336,  13312,  12288,  11264,  10240,  9216,   8192,   7168,
          6144,   5120,   4096,   3072,   2048,   1024,   0,      -1024,  -2048,  -3072,  -4096,  -5120,  -6144,
          -7168,  -8192,  -9216,  -10240, -11264, -12288, -13312, -14336, -15360, -16384, -17408, -18432, -19456,
          -20480, -21504, -22528, -23552, -24576, -25600, -26624, -27648, -28672, -29696, -30720, -31744 },
        { 32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,
          32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,  32767,
          32767,  32767,  32767,  32767,  32767,  32767,  -32767, -32767, -32767, -32767, -32767, -32767, -32767,
          -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767,
          -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767 }

      };

// Structs

#define QC_SAMPLE_CONTROL_LOOP 0x01

// Big-endian reader
typedef struct QcReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} QcReader;

// Reader helpers

static void qc_reader_init(QcReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool qc_reader_eof(const QcReader* r) {
    return r->pos > r->size;
}

static uint8_t qc_reader_u8(QcReader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

static int8_t qc_reader_i8(QcReader* r) {
    return (int8_t)qc_reader_u8(r);
}

static uint16_t qc_reader_b_u16(QcReader* r) {
    uint8_t hi = qc_reader_u8(r);
    uint8_t lo = qc_reader_u8(r);
    return (uint16_t)((hi << 8) | lo);
}

static uint32_t qc_reader_b_u32(QcReader* r) {
    uint16_t hi = qc_reader_b_u16(r);
    uint16_t lo = qc_reader_b_u16(r);
    return ((uint32_t)hi << 16) | lo;
}

static void qc_reader_skip(QcReader* r, size_t bytes) {
    r->pos += bytes;
}

static void qc_reader_read(QcReader* r, void* dst, size_t len) {
    for (size_t i = 0; i < len; i++)
        ((uint8_t*)dst)[i] = qc_reader_u8(r);
}

static void qc_reader_read_signed(QcReader* r, int8_t* dst, size_t len) {
    for (size_t i = 0; i < len; i++)
        dst[i] = qc_reader_i8(r);
}

static bool qc_reader_read_mark(QcReader* r, char* buf, int len) {
    for (int i = 0; i < len; i++)
        buf[i] = (char)qc_reader_u8(r);
    buf[len] = '\0';
    return !qc_reader_eof(r);
}

// Visit tracking

static void qc_clear_visited(QcModule* m) {
    memset(m->visited, 0, sizeof(m->visited));
}

static void qc_mark_visited(QcModule* m, int pos) {
    if (pos >= 0 && pos < 256)
        m->visited[pos] = 1;
}

static bool qc_has_visited(const QcModule* m, int pos) {
    if (pos >= 0 && pos < 256)
        return m->visited[pos] != 0;
    return false;
}

// Channel operations

static void qc_ch_mute(QcChannel* ch) {
    ch->action = QC_ACTION_STOP;
}

static void qc_ch_play_sample(QcChannel* ch, int8_t* data, uint32_t start_offset, uint32_t length) {
    ch->sample_data = data;
    ch->sample_offset = start_offset;
    ch->sample_length = start_offset + length;
    ch->loop_start = 0;
    ch->loop_length = 0;
    ch->action = QC_ACTION_PLAY;
}

static void qc_ch_set_loop(QcChannel* ch, uint32_t start_offset, uint32_t length) {
    ch->loop_start = start_offset;
    ch->loop_length = length;
}

static void qc_ch_set_amiga_period(QcChannel* ch, uint16_t period) {
    ch->period = period;
}

static void qc_ch_set_amiga_volume(QcChannel* ch, uint16_t vol) {
    if (vol > 64)
        vol = 64;
    ch->volume = vol;
}

// Helper to get pattern track line: pattern.Tracks[channel, row]
// In C# this is a 2D array [4, numRows+1]. We store flat: ch * stride + row
static QcTrackLine* qc_pattern_get(QcTrackLine* tracks, int stride, int channel, int row) {
    return &tracks[channel * stride + row];
}

// Loading

static bool qc_parse_emic(QcModule* m, QcReader* r, uint32_t chunk_size) {
    if (m->samples)
        return false; // only one EMIC chunk

    size_t start = r->pos;

    // Skip version (2 bytes)
    qc_reader_skip(r, 2);

    // Read name of song (20 bytes)
    qc_reader_read(r, m->title, 20);

    // Read composer (20 bytes) - skip
    qc_reader_skip(r, 20);

    // Read tempo
    m->start_tempo = qc_reader_u8(r);

    // Read sample information
    m->number_of_samples = qc_reader_u8(r);
    m->samples = (QcSample*)calloc(256, sizeof(QcSample));
    if (!m->samples)
        return false;

    for (int i = 0; i < m->number_of_samples; i++) {
        uint8_t number = qc_reader_u8(r);
        if (number == 0)
            return false;

        QcSample* samp = &m->samples[number - 1];

        samp->volume = qc_reader_u8(r);
        samp->length = qc_reader_b_u16(r) * 2U;

        qc_reader_read(r, samp->name, 20);

        samp->control_byte = qc_reader_u8(r);
        samp->fine_tune = (uint8_t)(qc_reader_u8(r) & 0x0f);
        samp->loop_start = qc_reader_b_u16(r) * 2U;
        samp->loop_length = qc_reader_b_u16(r) * 2U;

        if (qc_reader_eof(r))
            return false;

        // Skip offset (4 bytes)
        qc_reader_skip(r, 4);
    }

    // Read pattern information
    qc_reader_skip(r, 1); // skip padding
    m->number_of_patterns = qc_reader_u8(r);
    m->patterns = (QcPattern*)calloc(256, sizeof(QcPattern));
    if (!m->patterns)
        return false;

    for (int i = 0; i < m->number_of_patterns; i++) {
        uint8_t number = qc_reader_u8(r);

        QcPattern* patt = &m->patterns[number];
        patt->defined = true;
        patt->number_of_rows = qc_reader_u8(r);

        if (qc_reader_eof(r))
            return false;

        // Skip name (20 bytes) and offset (4 bytes)
        qc_reader_skip(r, 24);
    }

    // Read position list
    m->number_of_positions = qc_reader_u8(r);
    m->position_list = (uint8_t*)malloc(m->number_of_positions ? m->number_of_positions : 1);
    if (!m->position_list)
        return false;
    qc_reader_read(r, m->position_list, m->number_of_positions);

    if (qc_reader_eof(r))
        return false;

    // Align to 2-byte boundary
    if ((r->pos - start) & 1)
        qc_reader_skip(r, 1);

    // The outer reader has advanced past what is needed; the caller manages the position (start + chunk_size).
    (void)chunk_size;

    return true;
}

static bool qc_parse_patt(QcModule* m, QcReader* r) {
    if (!m->patterns)
        return false;

    for (int p = 0; p < 256; p++) {
        QcPattern* patt = &m->patterns[p];
        if (!patt->defined || patt->tracks != nullptr)
            continue;

        int stride = patt->number_of_rows + 1;
        patt->tracks_stride = stride;
        patt->tracks = (QcTrackLine*)calloc(4 * stride, sizeof(QcTrackLine));
        if (!patt->tracks)
            return false;

        for (int j = 0; j <= patt->number_of_rows; j++) {
            for (int k = 0; k < 4; k++) {
                uint8_t byt1 = qc_reader_u8(r);
                int8_t byt2 = qc_reader_i8(r);
                uint8_t byt3 = qc_reader_u8(r);
                uint8_t byt4 = qc_reader_u8(r);

                QcTrackLine* tl = qc_pattern_get(patt->tracks, stride, k, j);
                tl->sample = byt1;
                tl->note = byt2;
                tl->effect = (QcEffect)(byt3 & 0x0f);
                tl->effect_arg = byt4;
            }
        }

        if (qc_reader_eof(r))
            return false;
    }

    return true;
}

static bool qc_parse_8smp(QcModule* m, QcReader* r) {
    if (!m->samples)
        return false;

    for (int i = 0; i < 256; i++) {
        QcSample* samp = &m->samples[i];

        if (samp->length == 0)
            continue;

        if (samp->data)
            return false;
        samp->data = (int8_t*)malloc(samp->length);
        if (!samp->data)
            return false;
        samp->data_length = samp->length;
        qc_reader_read_signed(r, samp->data, samp->length);

        if (qc_reader_eof(r))
            return false;
    }

    return true;
}

static void qc_clamp_sample_loop(QcSample* samp) {
    if (samp->loop_start > samp->length)
        samp->loop_start = samp->length;
    if (samp->loop_length > samp->length - samp->loop_start)
        samp->loop_length = samp->length - samp->loop_start;
}

// Every position must name a pattern that has track data, and sample loops must lie inside the sample.
static bool qc_validate_module(QcModule* m) {
    if (!m->samples || !m->patterns || m->number_of_positions == 0 || m->start_tempo == 0)
        return false;

    for (int i = 0; i < m->number_of_positions; i++) {
        if (!m->patterns[m->position_list[i]].tracks)
            return false;
    }

    for (int i = 0; i < 256; i++) {
        qc_clamp_sample_loop(&m->samples[i]);
    }

    return true;
}

static bool qc_load_module(QcModule* m, const uint8_t* data, size_t size) {
    QcReader reader;
    qc_reader_init(&reader, data, size);

    // Skip to chunk area (after FORM + size + EMODEMIC = 16 bytes)
    reader.pos = 12;

    for (;;) {
        char chunk_name[5];
        if (!qc_reader_read_mark(&reader, chunk_name, 4))
            break;

        uint32_t chunk_size = qc_reader_b_u32(&reader);

        if (qc_reader_eof(&reader))
            break;

        if (reader.pos > size || chunk_size > (size - reader.pos))
            return false;

        if (strcmp(chunk_name, "EMIC") == 0) {
            if (!qc_parse_emic(m, &reader, chunk_size))
                return false;
        } else if (strcmp(chunk_name, "PATT") == 0) {
            if (!qc_parse_patt(m, &reader))
                return false;
        } else if (strcmp(chunk_name, "8SMP") == 0) {
            if (!qc_parse_8smp(m, &reader))
                return false;
        } else {
            qc_reader_skip(&reader, chunk_size);
        }
    }

    return qc_validate_module(m);
}

// Initialize

static void qc_initialize_sound(QcModule* m, int start_position) {
    QcPattern* patt = &m->patterns[m->position_list[start_position]];

    m->playing_info.current_position = (uint16_t)start_position;
    m->playing_info.current_pattern = patt->tracks;
    m->playing_info.current_pattern_stride = patt->tracks_stride;
    m->playing_info.break_row = patt->number_of_rows;
    m->playing_info.new_row = 0;
    m->playing_info.row_count = 0;

    m->playing_info.tempo = m->start_tempo;
    m->playing_info.speed = 6;
    m->playing_info.speed_count = 6;
    m->playing_info.set_tempo = true;

    m->playing_info.new_position_flag = false;
    m->playing_info.jump_break_flag = false;
    m->playing_info.intro_row = true;
    m->playing_info.loop_count = 0;
    m->playing_info.loop_row = 0;
    m->playing_info.pattern_wait = 0;

    m->end_reached = false;
    m->loop_count = 0;

    for (int i = 0; i < 4; i++) {
        memset(&m->channel_info[i], 0, sizeof(QcChannelInfo));
        memset(&m->channels[i], 0, sizeof(QcChannel));
    }

    m->tick_length = CIA_TIMER_NUMERATOR / m->playing_info.tempo;
}

// Effects

static void qc_do_arpeggio(QcModule* m, QcChannelInfo* ci, QcChannel* ch) {
    if (ci->track_line.effect_arg != 0) {
        int8_t offset = qc_arpeggio_offsets[m->playing_info.speed_count % 3];

        if (offset < 0)
            qc_ch_set_amiga_period(ch, ci->period);
        else {
            uint16_t note;
            if (offset == 0)
                note = (uint16_t)((ci->track_line.effect_arg >> 4) + ci->note_nr);
            else
                note = (uint16_t)((ci->track_line.effect_arg & 0x0f) + ci->note_nr);

            if (note > 35)
                note = 35;

            qc_ch_set_amiga_period(ch, qc_periods[ci->fine_tune][note]);
        }
    }
}

static void qc_do_slide_up(QcChannelInfo* ci, QcChannel* ch) {
    ci->period -= ci->track_line.effect_arg;
    if (ci->period < 113)
        ci->period = 113;
    qc_ch_set_amiga_period(ch, ci->period);
}

static void qc_do_slide_down(QcChannelInfo* ci, QcChannel* ch) {
    ci->period += ci->track_line.effect_arg;
    if (ci->period > 856)
        ci->period = 856;
    qc_ch_set_amiga_period(ch, ci->period);
}

static void qc_set_tone_portamento(QcChannelInfo* ci) {
    ci->wanted_period = qc_periods[ci->fine_tune][ci->note_nr];
    if (ci->wanted_period > ci->period)
        ci->port_direction = true;
    else
        ci->port_direction = false;
}

static void qc_do_actual_tone_portamento(QcChannelInfo* ci, QcChannel* ch) {
    uint16_t port_speed = ci->port_speed;

    if (ci->port_direction) {
        ci->period += port_speed;

        if (ci->period >= ci->wanted_period) {
            ci->period = ci->wanted_period;
            ci->wanted_period = 0;
            qc_ch_set_amiga_period(ch, ci->period);
            return;
        }
    } else {
        int16_t new_period = (int16_t)(ci->period - port_speed);

        if (new_period <= (int16_t)ci->wanted_period) {
            ci->period = ci->wanted_period;
            ci->wanted_period = 0;
            qc_ch_set_amiga_period(ch, ci->period);
            return;
        }

        ci->period = (uint16_t)new_period;
    }

    if (ci->glissando_control != 0) {
        for (int i = 0; i < 36; i++) {
            if (ci->period >= qc_periods[ci->fine_tune][i]) {
                qc_ch_set_amiga_period(ch, qc_periods[ci->fine_tune][i]);
                break;
            }
        }
    } else {
        qc_ch_set_amiga_period(ch, ci->period);
    }
}

static void qc_do_tone_portamento(QcChannelInfo* ci, QcChannel* ch) {
    if (ci->wanted_period == 0)
        return;

    if (ci->track_line.effect_arg != 0)
        ci->port_speed = ci->track_line.effect_arg;

    qc_do_actual_tone_portamento(ci, ch);
}

static void qc_do_actual_vibrato(QcChannelInfo* ci, QcChannel* ch) {
    ci->vibrato_position += (uint16_t)(ci->vibrato_command >> 4);
    ci->vibrato_position &= 0x3f;

    int16_t vib_val = qc_vibrato[ci->vibrato_wave][ci->vibrato_position];
    int16_t new_period = (int16_t)(ci->period + (((ci->vibrato_command & 0x0f) * vib_val) >> 14));

    if (new_period > 856)
        new_period = 856;
    else if (new_period < 113)
        new_period = 113;

    qc_ch_set_amiga_period(ch, (uint16_t)new_period);
}

static void qc_do_vibrato(QcChannelInfo* ci, QcChannel* ch) {
    uint8_t arg = ci->track_line.effect_arg;
    if (arg != 0) {
        if ((arg & 0x0f) != 0)
            ci->vibrato_command = (uint8_t)((ci->vibrato_command & 0xf0) | (arg & 0x0f));
        if ((arg & 0xf0) != 0)
            ci->vibrato_command = (uint8_t)((ci->vibrato_command & 0x0f) | (arg & 0xf0));
    }

    qc_do_actual_vibrato(ci, ch);
}

static void qc_do_volume_slide(QcChannelInfo* ci, QcChannel* ch) {
    int16_t new_vol = (int16_t)ci->volume;

    uint8_t vol_speed = (uint8_t)(ci->track_line.effect_arg >> 4);
    if (vol_speed != 0) {
        new_vol += vol_speed;
        if (new_vol > 64)
            new_vol = 64;
    } else {
        new_vol -= (int16_t)(ci->track_line.effect_arg & 0x0f);
        if (new_vol < 0)
            new_vol = 0;
    }

    ci->volume = (uint16_t)new_vol;
    qc_ch_set_amiga_volume(ch, ci->volume);
}

static void qc_do_tone_portamento_vol_slide(QcChannelInfo* ci, QcChannel* ch) {
    if (ci->wanted_period != 0)
        qc_do_actual_tone_portamento(ci, ch);
    qc_do_volume_slide(ci, ch);
}

static void qc_do_vibrato_vol_slide(QcChannelInfo* ci, QcChannel* ch) {
    qc_do_actual_vibrato(ci, ch);
    qc_do_volume_slide(ci, ch);
}

static void qc_do_tremolo(QcChannelInfo* ci, QcChannel* ch) {
    uint8_t arg = ci->track_line.effect_arg;
    if (arg != 0) {
        if ((arg & 0x0f) != 0)
            ci->tremolo_command = (uint8_t)((ci->tremolo_command & 0xf0) | (arg & 0x0f));
        if ((arg & 0xf0) != 0)
            ci->tremolo_command = (uint8_t)((ci->tremolo_command & 0x0f) | (arg & 0xf0));
    }

    ci->tremolo_position += (uint16_t)(ci->tremolo_command >> 4);
    ci->tremolo_position &= 0x3f;

    int16_t vib_val = qc_vibrato[ci->tremolo_wave][ci->tremolo_position];
    int16_t new_vol = (int16_t)(ci->volume + (((ci->tremolo_command & 0x0f) * vib_val) >> 14));

    if (new_vol > 64)
        new_vol = 64;
    else if (new_vol < 0)
        new_vol = 0;

    qc_ch_set_amiga_volume(ch, (uint16_t)new_vol);
}

static void qc_do_set_sample_offset(QcChannelInfo* ci, QcChannel* ch) {
    if (ci->track_line.effect_arg != 0)
        ci->sample_offset = ci->track_line.effect_arg;

    uint32_t offset = ci->sample_offset * 256U * 2;

    if (offset < ci->length)
        ci->start = offset;
    else {
        ci->start = ci->loop;
        ci->length = ci->loop_length;
    }

    if (ci->length > 0)
        qc_ch_play_sample(ch, ci->sample_data, ci->start, ci->length - ci->start);
    else
        qc_ch_mute(ch);
}

static void qc_do_position_jump(QcModule* m, QcChannelInfo* ci) {
    m->playing_info.new_position = ci->track_line.effect_arg;
    m->playing_info.new_position_flag = true;
    m->playing_info.new_row = 0;
}

static void qc_do_set_volume(QcChannelInfo* ci, QcChannel* ch) {
    ci->volume = ci->track_line.effect_arg;
    if (ci->volume > 64)
        ci->volume = 64;
    qc_ch_set_amiga_volume(ch, ci->volume);
}

static void qc_do_pattern_break(QcModule* m, QcChannelInfo* ci) {
    m->playing_info.new_position = (uint16_t)(m->playing_info.current_position + 1);
    m->playing_info.new_row = ci->track_line.effect_arg;
    m->playing_info.new_position_flag = true;
}

static void qc_do_set_speed(QcModule* m, QcChannelInfo* ci) {
    if (ci->track_line.effect_arg > 31) {
        m->playing_info.tempo = ci->track_line.effect_arg;
        m->playing_info.set_tempo = true;
    } else {
        uint16_t new_speed = ci->track_line.effect_arg;
        if (new_speed == 0)
            new_speed = 1;

        m->playing_info.speed = new_speed;
        m->playing_info.speed_count = 0;
    }
}

// Extra effects
static void qc_do_set_filter(QcChannelInfo* ci) {
    // No hardware filter
    (void)ci;
}

static void qc_do_fine_slide_up(QcChannelInfo* ci, QcChannel* ch) {
    ci->period -= (uint16_t)(ci->track_line.effect_arg & 0x0f);
    if (ci->period < 113)
        ci->period = 113;
    qc_ch_set_amiga_period(ch, ci->period);
}

static void qc_do_fine_slide_down(QcChannelInfo* ci, QcChannel* ch) {
    ci->period += (uint16_t)(ci->track_line.effect_arg & 0x0f);
    if (ci->period > 856)
        ci->period = 856;
    qc_ch_set_amiga_period(ch, ci->period);
}

static void qc_do_set_glissando(QcChannelInfo* ci) {
    ci->glissando_control = (uint8_t)(ci->track_line.effect_arg & 0x0f);
}

static void qc_do_set_vibrato_waveform(QcChannelInfo* ci) {
    // Only the three table waveforms exist; a damaged selector keeps the current one
    uint8_t wave = (uint8_t)(ci->track_line.effect_arg & 0x0f);
    if (wave < 3)
        ci->vibrato_wave = wave;
}

static void qc_do_set_fine_tune(QcChannelInfo* ci) {
    ci->fine_tune = (uint8_t)(ci->track_line.effect_arg & 0x0f);
}

static void qc_do_pattern_loop(QcModule* m, QcChannelInfo* ci) {
    uint8_t arg = (uint8_t)(ci->track_line.effect_arg & 0x0f);

    if (arg == 0)
        m->playing_info.loop_row = m->playing_info.row_count;
    else {
        if (m->playing_info.loop_count == 0) {
            m->playing_info.loop_count = arg;
            m->playing_info.jump_break_flag = true;
        } else {
            m->playing_info.loop_count--;
            if (m->playing_info.loop_count != 0)
                m->playing_info.jump_break_flag = true;
        }
    }
}

static void qc_do_set_tremolo_waveform(QcChannelInfo* ci) {
    // Only the three table waveforms exist; a damaged selector keeps the current one
    uint8_t wave = (uint8_t)(ci->track_line.effect_arg & 0x0f);
    if (wave < 3)
        ci->tremolo_wave = wave;
}

static void qc_do_retrig_note(QcModule* m, QcChannelInfo* ci, QcChannel* ch) {
    uint8_t arg = (uint8_t)(ci->track_line.effect_arg & 0x0f);

    ci->retrig++;
    if (ci->retrig >= arg) {
        ci->retrig = 0;
        qc_ch_play_sample(ch, ci->sample_data, ci->start, ci->length - ci->start);
        qc_ch_set_amiga_period(ch, ci->period);
    }
    (void)m;
}

static void qc_do_init_retrig(QcModule* m, QcChannelInfo* ci, QcChannel* ch) {
    ci->retrig = 0;
    qc_do_retrig_note(m, ci, ch);
}

static void qc_do_fine_vol_slide_up(QcChannelInfo* ci, QcChannel* ch) {
    ci->volume += (uint16_t)(ci->track_line.effect_arg & 0x0f);
    if (ci->volume > 64)
        ci->volume = 64;
    qc_ch_set_amiga_volume(ch, ci->volume);
}

static void qc_do_fine_vol_slide_down(QcChannelInfo* ci, QcChannel* ch) {
    int16_t new_vol = (int16_t)(ci->volume - (ci->track_line.effect_arg & 0x0f));
    if (new_vol < 0)
        new_vol = 0;
    ci->volume = (uint16_t)new_vol;
    qc_ch_set_amiga_volume(ch, ci->volume);
}

static void qc_do_note_cut(QcModule* m, QcChannelInfo* ci, QcChannel* ch) {
    if ((ci->track_line.effect_arg & 0x0f) <= m->playing_info.speed_count) {
        ci->volume = 0;
        qc_ch_set_amiga_volume(ch, 0);
    }
}

static void qc_do_note_delay(QcModule* m, QcChannelInfo* ci, QcChannel* ch) {
    if (ci->note_nr >= 0) {
        if ((ci->track_line.effect_arg & 0x0f) == m->playing_info.speed_count) {
            qc_ch_play_sample(ch, ci->sample_data, ci->start, ci->length - ci->start);
            qc_ch_set_amiga_period(ch, ci->period);
        }
    }
}

static void qc_do_pattern_delay(QcModule* m, QcChannelInfo* ci) {
    m->playing_info.pattern_wait = (uint8_t)(ci->track_line.effect_arg & 0x0f);
}

// PlayAfterPeriodExtraEffect

static void qc_play_after_period_extra_effect(QcModule* m, QcChannelInfo* ci, QcChannel* ch) {
    switch ((QcExtraEffect)(ci->track_line.effect_arg & 0xf0)) {
        case QC_EXTRA_SET_FILTER:
            qc_do_set_filter(ci);
            break;
        case QC_EXTRA_FINE_SLIDE_UP:
            qc_do_fine_slide_up(ci, ch);
            break;
        case QC_EXTRA_FINE_SLIDE_DOWN:
            qc_do_fine_slide_down(ci, ch);
            break;
        case QC_EXTRA_SET_GLISSANDO:
            qc_do_set_glissando(ci);
            break;
        case QC_EXTRA_SET_VIBRATO_WAVE:
            qc_do_set_vibrato_waveform(ci);
            break;
        case QC_EXTRA_SET_FINE_TUNE:
            qc_do_set_fine_tune(ci);
            break;
        case QC_EXTRA_PATTERN_LOOP:
            qc_do_pattern_loop(m, ci);
            break;
        case QC_EXTRA_SET_TREMOLO_WAVE:
            qc_do_set_tremolo_waveform(ci);
            break;
        case QC_EXTRA_RETRIG_NOTE:
            qc_do_init_retrig(m, ci, ch);
            break;
        case QC_EXTRA_FINE_VOL_SLIDE_UP:
            qc_do_fine_vol_slide_up(ci, ch);
            break;
        case QC_EXTRA_FINE_VOL_SLIDE_DOWN:
            qc_do_fine_vol_slide_down(ci, ch);
            break;
        case QC_EXTRA_NOTE_CUT:
            qc_do_note_cut(m, ci, ch);
            break;
        case QC_EXTRA_NOTE_DELAY:
            qc_do_note_delay(m, ci, ch);
            break;
        case QC_EXTRA_PATTERN_DELAY:
            qc_do_pattern_delay(m, ci);
            break;
        default:
            break;
    }
}

// PlayAfterPeriodEffect

static void qc_play_after_period_effect(QcModule* m, QcChannelInfo* ci, QcChannel* ch) {
    switch (ci->track_line.effect) {
        case QC_EFFECT_ARPEGGIO:
            qc_do_arpeggio(m, ci, ch);
            break;
        case QC_EFFECT_SET_SAMPLE_OFFSET:
            qc_do_set_sample_offset(ci, ch);
            break;
        case QC_EFFECT_POSITION_JUMP:
            qc_do_position_jump(m, ci);
            break;
        case QC_EFFECT_SET_VOLUME:
            qc_do_set_volume(ci, ch);
            break;
        case QC_EFFECT_PATTERN_BREAK:
            qc_do_pattern_break(m, ci);
            break;
        case QC_EFFECT_EXTRA:
            qc_play_after_period_extra_effect(m, ci, ch);
            break;
        case QC_EFFECT_SET_SPEED:
            qc_do_set_speed(m, ci);
            break;
        default:
            break;
    }
}

// PlayNote

static void qc_play_note(QcModule* m, QcChannelInfo* ci, QcChannel* ch, QcTrackLine* tl) {
    ci->track_line = *tl;

    if (tl->sample != 0) {
        QcSample* samp = &m->samples[tl->sample - 1];
        if (samp->data == nullptr && samp->length == 0)
            return;

        ci->shown_sample = tl->sample;
        ci->volume = samp->volume;
        ci->length = samp->length;
        ci->fine_tune = samp->fine_tune;
        ci->sample_data = samp->data;
        ci->start = 0;

        qc_ch_set_amiga_volume(ch, ci->volume);

        if ((samp->control_byte & QC_SAMPLE_CONTROL_LOOP) != 0) {
            ci->loop = samp->loop_start;
            ci->length = samp->loop_start + samp->loop_length;
            ci->loop_length = samp->loop_length;
        } else {
            ci->loop = 0;
            ci->loop_length = 0;
        }
    }

    if (tl->note >= 0) {
        ci->note_nr = tl->note > 35 ? 35 : tl->note;

        if ((tl->effect == QC_EFFECT_EXTRA) && ((tl->effect_arg & 0xf0) == 0x50))
            ci->fine_tune = (uint8_t)(tl->effect_arg & 0x0f);
        else if ((tl->effect == QC_EFFECT_TONE_PORTAMENTO) || (tl->effect == QC_EFFECT_TONE_PORTA_VOL_SLIDE)) {
            qc_set_tone_portamento(ci);
            return;
        }

        ci->period = qc_periods[ci->fine_tune][ci->note_nr];
        ci->shown_note = (uint8_t)(ci->note_nr + 1);

        if ((tl->effect == QC_EFFECT_EXTRA) && ((tl->effect_arg & 0xf0) == 0xd0)) {
            qc_do_note_delay(m, ci, ch);
            return;
        }

        qc_ch_play_sample(ch, ci->sample_data, ci->start, ci->length - ci->start);
        qc_ch_set_amiga_period(ch, ci->period);

        if (ci->loop_length != 0)
            qc_ch_set_loop(ch, ci->loop, ci->loop_length);
    }

    qc_play_after_period_effect(m, ci, ch);
}

// RunTickEffects

static void qc_play_tick_extra_effect(QcModule* m, QcChannelInfo* ci, QcChannel* ch) {
    switch ((QcExtraEffect)(ci->track_line.effect_arg & 0xf0)) {
        case QC_EXTRA_RETRIG_NOTE:
            qc_do_retrig_note(m, ci, ch);
            break;
        case QC_EXTRA_NOTE_CUT:
            qc_do_note_cut(m, ci, ch);
            break;
        case QC_EXTRA_NOTE_DELAY:
            qc_do_note_delay(m, ci, ch);
            break;
        default:
            break;
    }
}

static void qc_play_tick_effect(QcModule* m, QcChannelInfo* ci, QcChannel* ch) {
    switch (ci->track_line.effect) {
        case QC_EFFECT_ARPEGGIO:
            qc_do_arpeggio(m, ci, ch);
            break;
        case QC_EFFECT_SLIDE_UP:
            qc_do_slide_up(ci, ch);
            break;
        case QC_EFFECT_SLIDE_DOWN:
            qc_do_slide_down(ci, ch);
            break;
        case QC_EFFECT_TONE_PORTAMENTO:
            qc_do_tone_portamento(ci, ch);
            break;
        case QC_EFFECT_VIBRATO:
            qc_do_vibrato(ci, ch);
            break;
        case QC_EFFECT_TONE_PORTA_VOL_SLIDE:
            qc_do_tone_portamento_vol_slide(ci, ch);
            break;
        case QC_EFFECT_VIBRATO_VOL_SLIDE:
            qc_do_vibrato_vol_slide(ci, ch);
            break;
        case QC_EFFECT_TREMOLO:
            qc_do_tremolo(ci, ch);
            break;
        case QC_EFFECT_VOLUME_SLIDE:
            qc_do_volume_slide(ci, ch);
            break;
        case QC_EFFECT_EXTRA:
            qc_play_tick_extra_effect(m, ci, ch);
            break;
        default:
            break;
    }
}

static void qc_run_tick_effects(QcModule* m) {
    for (int i = 0; i < 4; i++)
        qc_play_tick_effect(m, &m->channel_info[i], &m->channels[i]);
}

// InitializeNewPosition

static void qc_initialize_new_position(QcModule* m) {
    if (m->playing_info.current_position >= m->number_of_positions)
        m->playing_info.current_position = 0;

    if (qc_has_visited(m, m->playing_info.current_position))
        m->end_reached = true;

    qc_mark_visited(m, m->playing_info.current_position);

    QcPattern* patt = &m->patterns[m->position_list[m->playing_info.current_position]];
    m->playing_info.current_pattern = patt->tracks;
    m->playing_info.current_pattern_stride = patt->tracks_stride;
    m->playing_info.break_row = patt->number_of_rows;

    m->playing_info.row_count = m->playing_info.new_row;
    m->playing_info.new_row = 0;

    if (m->playing_info.break_row < m->playing_info.row_count)
        m->playing_info.row_count = m->playing_info.break_row;
}

// GetNotes

static void qc_change_tempo_if_needed(QcModule* m) {
    if (m->playing_info.set_tempo) {
        m->playing_info.set_tempo = false;
        m->tick_length = CIA_TIMER_NUMERATOR / m->playing_info.tempo;
    }
}

static void qc_get_notes(QcModule* m) {
    if (!m->playing_info.intro_row) {
        qc_change_tempo_if_needed(m);

        if (m->playing_info.new_position_flag) {
            m->playing_info.new_position_flag = false;
            m->playing_info.current_position = m->playing_info.new_position;
            qc_initialize_new_position(m);
        } else {
            if (m->playing_info.jump_break_flag) {
                m->playing_info.jump_break_flag = false;

                if (m->playing_info.loop_row <= m->playing_info.break_row)
                    m->playing_info.row_count = m->playing_info.loop_row;
            } else {
                m->playing_info.row_count++;

                if (m->playing_info.row_count > m->playing_info.break_row) {
                    m->playing_info.current_position++;
                    qc_initialize_new_position(m);
                }
            }
        }
    }

    m->playing_info.intro_row = false;
    m->playing_info.speed_count = 0;

    for (int i = 0; i < 4; i++) {
        QcTrackLine* tl = qc_pattern_get(m->playing_info.current_pattern, m->playing_info.current_pattern_stride, i,
                                         m->playing_info.row_count);
        qc_play_note(m, &m->channel_info[i], &m->channels[i], tl);
    }
}

// Play tick

static void qc_play_tick(QcModule* m) {
    qc_change_tempo_if_needed(m);

    m->playing_info.speed_count++;
    if (m->playing_info.speed_count < m->playing_info.speed) {
        qc_run_tick_effects(m);
    } else {
        if (m->playing_info.pattern_wait != 0) {
            m->playing_info.pattern_wait--;
            m->playing_info.speed_count = 0;
            qc_run_tick_effects(m);
        } else {
            qc_get_notes(m);
        }
    }

    if (m->end_reached) {
        m->loop_count++;
        m->end_reached = false;
        qc_mark_visited(m, m->playing_info.current_position);
    }
}

// Voices

// Hand the registers the tick wrote to the voice.
static void qc_sync_voice(QcModule* m, uint32_t index) {
    QcChannel* c = &m->channels[index];

    if (c->action == QC_ACTION_PLAY) {
        // A sample that is empty or has no data (missing 8SMP chunk) is silent
        if (c->sample_data != nullptr && c->sample_offset < c->sample_length) {
            const NuadeSample sample = { .data = c->sample_data,
                                         .format = NuadeSampleFormat_S8,
                                         .length = c->sample_length,
                                         .loop_start = c->loop_start,
                                         .loop_length = c->loop_length };
            nuade_voice_play(m->voices, index, &sample, c->sample_offset);
        } else {
            nuade_voice_stop(m->voices, index);
        }
    } else if (c->action == QC_ACTION_STOP) {
        nuade_voice_stop(m->voices, index);
    }
    c->action = QC_ACTION_NONE;

    const bool silent = c->period == 0;
    nuade_voice_set_pitch(m->voices, index, silent ? 0.0 : nuade_paula_period_hz(c->period));
    nuade_voice_set_volume(m->voices, index, silent ? 0.0f : (float)c->volume / 64.0f);
    // Paula channels 0 and 3 are left, 1 and 2 right
    nuade_voice_set_pan(m->voices, index, (index == 0 || index == 3) ? -1.0f : 1.0f);
}

bool qc_tick(QcModule* module) {
    const uint32_t loops = module->loop_count;
    qc_play_tick(module);
    for (uint32_t i = 0; i < QC_CHANNELS; i++) {
        qc_sync_voice(module, i);
    }
    return module->loop_count != loops;
}

QcPosition qc_position(const QcModule* module) {
    // A tick that reads a row moves the position to it, so the position is the row that was just played.
    if (module->playing_info.intro_row) {
        return (QcPosition) { 0 };
    }
    return (QcPosition) { .valid = true,
                          .position = module->playing_info.current_position,
                          .pattern = module->position_list[module->playing_info.current_position],
                          .row = module->playing_info.row_count,
                          .speed = module->playing_info.speed,
                          .tick = module->playing_info.speed_count };
}

QcVoiceState qc_voice_state(const QcModule* module, int voice) {
    if (voice < 0 || voice >= QC_CHANNELS) {
        return (QcVoiceState) { 0 };
    }
    return (QcVoiceState) { .note = module->channel_info[voice].shown_note,
                            .sample = module->channel_info[voice].shown_sample };
}

int qc_position_pattern(const QcModule* module, int position) {
    return position >= 0 && position < module->number_of_positions ? module->position_list[position] : -1;
}

uint32_t qc_tick_length(const QcModule* module) {
    return module->tick_length;
}

// Public API

bool qc_probe(const uint8_t* data, size_t size) {
    if (data == nullptr || size < 64)
        return false;
    if (memcmp(data, "FORM", 4) != 0 || memcmp(data + 8, "EMODEMIC", 8) != 0)
        return false;
    // Format version
    return (((uint16_t)data[20] << 8) | data[21]) == 1;
}

QcModule* qc_create(const uint8_t* data, size_t size) {
    if (!qc_probe(data, size))
        return nullptr;

    QcModule* m = (QcModule*)calloc(1, sizeof(QcModule));
    if (!m)
        return nullptr;

    // Keep original data for export
    m->original_data = (uint8_t*)malloc(size);
    if (m->original_data) {
        memcpy(m->original_data, data, size);
        m->original_size = size;
    }

    if (!qc_load_module(m, data, size) || m->number_of_positions == 0) {
        qc_destroy(m);
        return nullptr;
    }

    return m;
}

void qc_destroy(QcModule* module) {
    if (!module)
        return;

    if (module->samples) {
        for (int i = 0; i < 256; i++) {
            if (module->samples[i].data)
                free(module->samples[i].data);
        }
        free(module->samples);
    }

    if (module->patterns) {
        for (int i = 0; i < 256; i++) {
            if (module->patterns[i].tracks)
                free(module->patterns[i].tracks);
        }
        free(module->patterns);
    }

    if (module->position_list)
        free(module->position_list);

    if (module->original_data)
        free(module->original_data);
    free(module);
}

int qc_subsong_count(const QcModule* module) {
    if (!module)
        return 0;
    return 1;
}

bool qc_start(QcModule* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong != 0)
        return false;

    module->voices = voices;
    qc_clear_visited(module);
    qc_initialize_sound(module, 0);
    return true;
}

const char* qc_title(const QcModule* module) {
    return module->title;
}

const char* qc_sample_name(const QcModule* module, int index) {
    if (index < 0 || index >= 256)
        return "";
    return module->samples[index].name;
}

// Edit API

int qc_get_instrument_count(const QcModule* module) {
    return module ? (int)module->number_of_samples : 0;
}

int qc_get_num_patterns(const QcModule* module) {
    return module ? (int)module->number_of_patterns : 0;
}

int qc_get_pattern_rows(const QcModule* module, int pattern) {
    if (!module || pattern < 0 || pattern >= module->number_of_patterns)
        return 0;
    return module->patterns[pattern].number_of_rows + 1;
}

int qc_get_num_positions(const QcModule* module) {
    return module ? (int)module->number_of_positions : 0;
}

void qc_get_cell(const QcModule* module, int pattern, int row, int channel, uint8_t* sample, int8_t* note,
                 uint8_t* effect, uint8_t* effect_arg) {
    if (!module || pattern < 0 || pattern >= module->number_of_patterns || channel < 0 || channel >= 4) {
        if (sample)
            *sample = 0;
        if (note)
            *note = -1;
        if (effect)
            *effect = 0;
        if (effect_arg)
            *effect_arg = 0;
        return;
    }
    const QcPattern* pat = &module->patterns[pattern];
    if (!pat->tracks || row < 0 || row > pat->number_of_rows) {
        if (sample)
            *sample = 0;
        if (note)
            *note = -1;
        if (effect)
            *effect = 0;
        if (effect_arg)
            *effect_arg = 0;
        return;
    }
    const QcTrackLine* tl = &pat->tracks[channel * pat->tracks_stride + row];
    if (sample)
        *sample = tl->sample;
    if (note)
        *note = tl->note;
    if (effect)
        *effect = (uint8_t)tl->effect;
    if (effect_arg)
        *effect_arg = tl->effect_arg;
}

void qc_set_cell(QcModule* module, int pattern, int row, int channel, uint8_t sample, int8_t note, uint8_t effect,
                 uint8_t effect_arg) {
    if (!module || pattern < 0 || pattern >= module->number_of_patterns || channel < 0 || channel >= 4)
        return;
    QcPattern* pat = &module->patterns[pattern];
    if (!pat->tracks || row < 0 || row > pat->number_of_rows)
        return;
    QcTrackLine* tl = &pat->tracks[channel * pat->tracks_stride + row];
    tl->sample = sample;
    tl->note = note;
    tl->effect = (QcEffect)effect;
    tl->effect_arg = effect_arg;
}

float qc_get_instrument_param(const QcModule* module, int inst, const char* param) {
    if (!module || inst < 0 || inst >= (int)module->number_of_samples || !param)
        return -1.0f;
    const QcSample* s = &module->samples[inst];

    if (strcmp(param, "length") == 0)
        return (float)s->length;
    if (strcmp(param, "loopStart") == 0)
        return (float)s->loop_start;
    if (strcmp(param, "loopLength") == 0)
        return (float)s->loop_length;
    if (strcmp(param, "volume") == 0)
        return (float)s->volume;
    if (strcmp(param, "controlByte") == 0)
        return (float)s->control_byte;
    if (strcmp(param, "fineTune") == 0)
        return (float)s->fine_tune;

    return -1.0f;
}

void qc_set_instrument_param(QcModule* module, int inst, const char* param, float value) {
    if (!module || inst < 0 || inst >= (int)module->number_of_samples || !param)
        return;
    QcSample* s = &module->samples[inst];
    uint32_t v32 = (uint32_t)value;
    uint8_t v8 = (uint8_t)value;

    if (strcmp(param, "length") == 0) {
        s->length = v32 < s->data_length ? v32 : s->data_length;
        qc_clamp_sample_loop(s);
        return;
    }
    if (strcmp(param, "loopStart") == 0) {
        s->loop_start = v32;
        qc_clamp_sample_loop(s);
        return;
    }
    if (strcmp(param, "loopLength") == 0) {
        s->loop_length = v32;
        qc_clamp_sample_loop(s);
        return;
    }
    if (strcmp(param, "volume") == 0) {
        s->volume = v8;
        return;
    }
    if (strcmp(param, "controlByte") == 0) {
        s->control_byte = v8;
        return;
    }
    if (strcmp(param, "fineTune") == 0) {
        s->fine_tune = v8 & 0x0f;
        return;
    }
}

size_t qc_export(const QcModule* module, uint8_t* out, size_t max_size) {
    if (!module || !module->original_data)
        return 0;
    size_t total = module->original_size;
    if (!out)
        return total;
    if (max_size < total)
        return 0;
    memcpy(out, module->original_data, total);
    return total;
}
