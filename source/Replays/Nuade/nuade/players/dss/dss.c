// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
#include <nuade/players/dss.h>

#include "nuade_base.h"
#include "nuade_paula.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DSS_VOICES 4

// Structs

#define DSS_POSITION_SLOTS 128

// A pattern in the file: 64 rows of 4 channels, 4 bytes each.
#define DSS_PATTERN_FILE_SIZE (64 * 4 * 4)

// Period tables

static const uint16_t dss_periods[16][48] = {
    // Tuning 0 (normal)
    { 1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960, 906, 856, 808, 762, 720,
      678,  640,  604,  570,  538,  508,  480,  453,  428,  404,  381, 360, 339, 320, 302, 285,
      269,  254,  240,  226,  214,  202,  190,  180,  170,  160,  151, 143, 135, 127, 120, 113 },
    // Tuning 1
    { 1700, 1604, 1514, 1430, 1348, 1274, 1202, 1134, 1070, 1010, 954, 900, 850, 802, 757, 715,
      674,  637,  601,  567,  535,  505,  477,  450,  425,  401,  379, 357, 337, 318, 300, 284,
      268,  253,  239,  225,  213,  201,  189,  179,  169,  159,  150, 142, 134, 126, 119, 113 },
    // Tuning 2
    { 1688, 1592, 1504, 1418, 1340, 1264, 1194, 1126, 1064, 1004, 948, 894, 844, 796, 752, 709,
      670,  632,  597,  563,  532,  502,  474,  447,  422,  398,  376, 355, 335, 316, 298, 282,
      266,  251,  237,  224,  211,  199,  188,  177,  167,  158,  149, 141, 133, 125, 118, 112 },
    // Tuning 3
    { 1676, 1582, 1492, 1408, 1330, 1256, 1184, 1118, 1056, 996, 940, 888, 838, 791, 746, 704,
      665,  628,  592,  559,  528,  498,  470,  444,  419,  395, 373, 352, 332, 314, 296, 280,
      264,  249,  235,  222,  209,  198,  187,  176,  166,  157, 148, 140, 132, 125, 118, 111 },
    // Tuning 4
    { 1664, 1570, 1482, 1398, 1320, 1246, 1176, 1110, 1048, 990, 934, 882, 832, 785, 741, 699,
      660,  623,  588,  555,  524,  495,  467,  441,  416,  392, 370, 350, 330, 312, 294, 278,
      262,  247,  233,  220,  208,  196,  185,  175,  165,  156, 147, 139, 131, 124, 117, 110 },
    // Tuning 5
    { 1652, 1558, 1472, 1388, 1310, 1238, 1168, 1102, 1040, 982, 926, 874, 826, 779, 736, 694,
      655,  619,  584,  551,  520,  491,  463,  437,  413,  390, 368, 347, 328, 309, 292, 276,
      260,  245,  232,  219,  206,  195,  184,  174,  164,  155, 146, 138, 130, 123, 116, 109 },
    // Tuning 6
    { 1640, 1548, 1460, 1378, 1302, 1228, 1160, 1094, 1032, 974, 920, 868, 820, 774, 730, 689,
      651,  614,  580,  547,  516,  487,  460,  434,  410,  387, 365, 345, 325, 307, 290, 274,
      258,  244,  230,  217,  205,  193,  183,  172,  163,  154, 145, 137, 129, 122, 115, 109 },
    // Tuning 7
    { 1628, 1536, 1450, 1368, 1292, 1220, 1150, 1086, 1026, 968, 914, 862, 814, 768, 725, 684,
      646,  610,  575,  543,  513,  484,  457,  431,  407,  384, 363, 342, 323, 305, 288, 272,
      256,  242,  228,  216,  204,  192,  181,  171,  161,  152, 144, 136, 128, 121, 114, 108 },
    // Tuning -8
    { 1814, 1712, 1616, 1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960, 907, 856, 808, 762,
      720,  678,  640,  604,  570,  538,  508,  480,  453,  428,  404,  381, 360, 339, 320, 302,
      285,  269,  254,  240,  226,  214,  202,  190,  180,  170,  160,  151, 143, 135, 127, 120 },
    // Tuning -7
    { 1800, 1700, 1604, 1514, 1430, 1350, 1272, 1202, 1134, 1070, 1010, 954, 900, 850, 802, 757,
      715,  675,  636,  601,  567,  535,  505,  477,  450,  425,  401,  379, 357, 337, 318, 300,
      284,  268,  253,  238,  225,  212,  200,  189,  179,  169,  159,  150, 142, 134, 126, 119 },
    // Tuning -6
    { 1788, 1688, 1592, 1504, 1418, 1340, 1264, 1194, 1126, 1064, 1004, 948, 894, 844, 796, 752,
      709,  670,  632,  597,  563,  532,  502,  474,  447,  422,  398,  376, 355, 335, 316, 298,
      282,  266,  251,  237,  223,  211,  199,  188,  177,  167,  158,  149, 141, 133, 125, 118 },
    // Tuning -5
    { 1774, 1676, 1582, 1492, 1408, 1330, 1256, 1184, 1118, 1056, 996, 940, 887, 838, 791, 746,
      704,  665,  628,  592,  559,  528,  498,  470,  444,  419,  395, 373, 352, 332, 314, 296,
      280,  264,  249,  235,  222,  209,  198,  187,  176,  166,  157, 148, 140, 132, 125, 118 },
    // Tuning -4
    { 1762, 1664, 1570, 1482, 1398, 1320, 1246, 1176, 1110, 1048, 988, 934, 881, 832, 785, 741,
      699,  660,  623,  588,  555,  524,  494,  467,  441,  416,  392, 370, 350, 330, 312, 294,
      278,  262,  247,  233,  220,  208,  196,  185,  175,  165,  156, 147, 139, 131, 123, 117 },
    // Tuning -3
    { 1750, 1652, 1558, 1472, 1388, 1310, 1238, 1168, 1102, 1040, 982, 926, 875, 826, 779, 736,
      694,  655,  619,  584,  551,  520,  491,  463,  437,  413,  390, 368, 347, 328, 309, 292,
      276,  260,  245,  232,  219,  206,  195,  184,  174,  164,  155, 146, 138, 130, 123, 116 },
    // Tuning -2
    { 1736, 1640, 1548, 1460, 1378, 1302, 1228, 1160, 1094, 1032, 974, 920, 868, 820, 774, 730,
      689,  651,  614,  580,  547,  516,  487,  460,  434,  410,  387, 365, 345, 325, 307, 290,
      274,  258,  244,  230,  217,  205,  193,  183,  172,  163,  154, 145, 137, 129, 122, 115 },
    // Tuning -1
    { 1724, 1628, 1536, 1450, 1368, 1292, 1220, 1150, 1086, 1026, 968, 914, 862, 814, 768, 725,
      684,  646,  610,  575,  543,  513,  484,  457,  431,  407,  384, 363, 342, 323, 305, 288,
      272,  256,  242,  228,  216,  203,  192,  181,  171,  161,  152, 144, 136, 128, 121, 114 }

};

static const uint16_t dss_period_limits[16][2]
    = { { 1712, 113 }, { 1700, 113 }, { 1688, 112 }, { 1676, 111 }, { 1664, 110 }, { 1652, 109 },
        { 1640, 109 }, { 1628, 108 }, { 1814, 120 }, { 1800, 119 }, { 1788, 118 }, { 1774, 118 },
        { 1762, 117 }, { 1750, 116 }, { 1736, 115 }, { 1724, 114 }

      };

// Reader helpers

typedef struct DssReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
} DssReader;

static void dss_reader_init(DssReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
}

static bool dss_reader_eof(const DssReader* r) {
    return r->pos > r->size;
}

static uint8_t dss_read_u8(DssReader* r) {
    if (r->pos >= r->size) {
        r->pos = r->size + 1;
        return 0;
    }
    return r->data[r->pos++];
}

// dss_read_i8 not needed for this format: all reads are unsigned

static uint16_t dss_read_b_u16(DssReader* r) {
    uint8_t hi = dss_read_u8(r);
    uint8_t lo = dss_read_u8(r);
    return (uint16_t)((hi << 8) | lo);
}

static uint32_t dss_read_b_u32(DssReader* r) {
    uint16_t hi = dss_read_b_u16(r);
    uint16_t lo = dss_read_b_u16(r);
    return ((uint32_t)hi << 16) | lo;
}

static void dss_reader_skip(DssReader* r, size_t n) {
    r->pos += n;
}

// Visited tracking

static void dss_clear_visited(DssModule* m) {
    memset(m->visited, 0, sizeof(m->visited));
}

static bool dss_has_visited(DssModule* m, uint16_t p) {
    if (p >= DSS_MAX_VISITED)
        return false;
    return (m->visited[p / 8] & (1 << (p % 8))) != 0;
}

static void dss_mark_visited(DssModule* m, uint16_t p) {
    if (p >= DSS_MAX_VISITED)
        return;
    m->visited[p / 8] |= (1 << (p % 8));
}

// Channel helpers

static void dss_ch_mute(DssChannel* c) {
    c->active = false;
}

static void dss_ch_play(DssChannel* c, int8_t* data, uint32_t data_size, uint32_t start, uint32_t len) {
    c->sample_data = data;
    c->sample_data_size = data_size;
    c->sample_offset = start;
    c->sample_length = start + len;
    c->loop_start = 0;
    c->loop_length = 0;
    c->play_pending = true;
    c->active = true;
}

static void dss_ch_set_loop(DssChannel* c, uint32_t start, uint32_t len) {
    c->loop_start = start;
    c->loop_length = len;
    c->sample_length = start + len;
}

static void dss_ch_set_period(DssChannel* c, uint16_t p) {
    c->period = p;
}

static void dss_ch_set_volume(DssChannel* c, uint16_t v) {
    if (v > 64)
        v = 64;
    c->volume = v;
}

// Forward declarations

static void dss_play_tick(DssModule* m);
static void dss_set_bpm(DssModule* m, uint8_t bpm);

// Helper functions

static void dss_set_volume(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    int vol = v->volume - m->playing_info.inverse_master_volume;
    if (vol < 0)
        vol = 0;
    dss_ch_set_volume(c, (uint16_t)vol);
}

static void dss_set_volume_all(DssModule* m) {
    for (int i = 0; i < 4; i++)
        dss_set_volume(m, &m->voices[i], &m->channels[i]);
}

static void dss_add_volume(DssModule* m, uint8_t add, DssVoiceInfo* v, DssChannel* c) {
    v->volume += add;
    if (v->volume > 64)
        v->volume = 64;
    dss_set_volume(m, v, c);
}

static void dss_sub_volume(DssModule* m, uint8_t sub, DssVoiceInfo* v, DssChannel* c) {
    int vol = v->volume - sub;
    if (vol < 0)
        vol = 0;
    v->volume = (uint8_t)vol;
    dss_set_volume(m, v, c);
}

static void dss_add_master(DssModule* m, uint8_t add) {
    int vol = m->playing_info.inverse_master_volume - add;
    if (vol < 0)
        vol = 0;
    m->playing_info.inverse_master_volume = (uint16_t)vol;
    dss_set_volume_all(m);
}

static void dss_sub_master(DssModule* m, uint8_t sub) {
    m->playing_info.inverse_master_volume += sub;
    if (m->playing_info.inverse_master_volume > 64)
        m->playing_info.inverse_master_volume = 64;
    dss_set_volume_all(m);
}

static uint16_t dss_adjust_fine_tune(uint16_t period, DssVoiceInfo* v) {
    if (v->fine_tune != 0) {
        int i;
        for (i = 0; i < 48; i++) {
            if (dss_periods[0][i] == period) {
                i++;
                break;
            }
        }
        period = dss_periods[v->fine_tune][i - 1];
    }
    return period;
}

static uint16_t dss_adjust_tone_porta(uint16_t period, DssVoiceInfo* v) {
    int i;
    for (i = 0; i < 48; i++) {
        if (period >= dss_periods[v->fine_tune][i]) {
            i++;
            break;
        }
    }
    return dss_periods[v->fine_tune][i - 1];
}

static void dss_apply_pitch_period(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    (void)m;
    uint16_t period = v->pitch_period;
    if (v->use_tone_porta_for_slide)
        period = dss_adjust_tone_porta(period, v);
    dss_ch_set_period(c, period);
}

static void dss_setup_portamento(uint16_t period, DssVoiceInfo* v) {
    v->portamento_end_period = dss_adjust_fine_tune(period, v);
    v->portamento_direction = false;
    if (v->portamento_end_period == v->pitch_period)
        v->portamento_end_period = 0;
    else if (v->portamento_end_period < v->pitch_period)
        v->portamento_direction = true;
}

static void dss_play_sample(DssVoiceInfo* v, DssChannel* c) {
    if (v->sample_data && v->sample_length > 0) {
        dss_ch_play(c, v->sample_data, v->sample_data_size, v->sample_start_offset,
                    (uint32_t)((v->sample_length + v->loop_length) * 2));
        if (v->loop_length != 0)
            dss_ch_set_loop(c, v->loop_start, v->loop_length * 2U);
    }
}

static void dss_do_portamento(DssVoiceInfo* v, DssChannel* c) {
    if (v->portamento_end_period != 0) {
        if (v->portamento_direction) {
            v->pitch_period -= v->portamento_speed;
            if (v->pitch_period <= v->portamento_end_period) {
                v->pitch_period = v->portamento_end_period;
                v->portamento_end_period = 0;
            }
        } else {
            v->pitch_period += v->portamento_speed;
            if (v->pitch_period >= v->portamento_end_period) {
                v->pitch_period = v->portamento_end_period;
                v->portamento_end_period = 0;
            }
        }

        uint16_t period = v->pitch_period;
        if (v->use_tone_porta_for_porta)
            period = dss_adjust_tone_porta(period, v);
        dss_ch_set_period(c, period);
    }
}

// Effect handlers

static void dss_eff_arpeggio(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    int arp = m->playing_info.arpeggio_counter - m->playing_info.speed_counter;
    if (arp == 0) {
        m->playing_info.arpeggio_counter += 3;
        dss_ch_set_period(c, v->pitch_period);
        return;
    }
    int arp_offset = (arp == 1) ? (v->effect_arg & 0x0f) : (v->effect_arg >> 4);
    (void)arp_offset;
    // Find period in table and offset
    for (int i = 0; i < 48; i++) {
        uint16_t p = dss_periods[v->fine_tune][i];
        if (p == dss_period_limits[v->fine_tune][1] || p == v->pitch_period) {
            dss_ch_set_period(c, p);
            break;
        }
    }
}

static void dss_eff_slide_up(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    v->pitch_period -= v->effect_arg;
    if (v->pitch_period < dss_period_limits[v->fine_tune][1])
        v->pitch_period = dss_period_limits[v->fine_tune][1];
    dss_apply_pitch_period(m, v, c);
}

static void dss_eff_slide_down(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    v->pitch_period += v->effect_arg;
    if (v->pitch_period > dss_period_limits[v->fine_tune][0])
        v->pitch_period = dss_period_limits[v->fine_tune][0];
    dss_apply_pitch_period(m, v, c);
}

static void dss_eff_set_volume(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    v->volume = v->effect_arg;
    dss_set_volume(m, v, c);
}

static void dss_eff_set_master_vol(DssModule* m, DssVoiceInfo* v) {
    int vol = 64 - v->effect_arg;
    if (vol >= 0)
        m->playing_info.inverse_master_volume = (uint16_t)vol;
}

static void dss_eff_set_speed(DssModule* m, DssVoiceInfo* v) {
    if (v->effect_arg != 0) {
        m->playing_info.speed_counter = 0;
        m->playing_info.next_retrig_tick = 0;
        m->playing_info.arpeggio_counter = 3;
        m->playing_info.current_speed = v->effect_arg;
    }
}

static void dss_eff_position_jump(DssModule* m, DssVoiceInfo* v) {
    if (v->effect_arg == 0xff || v->effect_arg == 0) {
        m->playing_info.new_position = m->playing_info.current_position;
        m->playing_info.position_jump = true;
    } else if (v->effect_arg <= m->num_positions) {
        m->playing_info.new_position = (uint16_t)(v->effect_arg - 2);
        m->playing_info.position_jump = true;
    }
}

static void dss_eff_set_filter(DssVoiceInfo* v) {
    (void)v; // AmigaFilter - not relevant for WASM
}

static void dss_eff_pitch_up(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    if (v->effect_arg != 0) {
        bool old = v->use_tone_porta_for_slide;
        v->use_tone_porta_for_slide = false;
        dss_eff_slide_up(m, v, c);
        v->use_tone_porta_for_slide = old;
    }
}

static void dss_eff_pitch_down(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    if (v->effect_arg != 0) {
        bool old = v->use_tone_porta_for_slide;
        v->use_tone_porta_for_slide = false;
        dss_eff_slide_down(m, v, c);
        v->use_tone_porta_for_slide = old;
    }
}

static void dss_eff_pitch_control(DssVoiceInfo* v) {
    v->use_tone_porta_for_slide = v->effect_arg != 0;
}

static void dss_eff_set_tempo(DssModule* m, DssVoiceInfo* v) {
    if (v->effect_arg >= 28) {
        m->playing_info.current_tempo = v->effect_arg;
        dss_set_bpm(m, m->playing_info.current_tempo);
        m->playing_info.speed_counter = 0;
        m->playing_info.next_retrig_tick = 0;
        m->playing_info.arpeggio_counter = 3;
    }
}

static void dss_eff_volume_up(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    if (v->effect_arg != 0)
        dss_add_volume(m, v->effect_arg, v, c);
}

static void dss_eff_volume_down(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    if (v->effect_arg != 0)
        dss_sub_volume(m, v->effect_arg, v, c);
}

static void dss_eff_set_loop_start(DssModule* m, DssVoiceInfo* v) {
    v->loop_row = m->playing_info.current_row;
}

static void dss_eff_jump_to_loop(DssModule* m, DssVoiceInfo* v) {
    if (v->effect_arg == 0) {
        v->loop_row = -1;
    } else if (v->loop_row >= 0) {
        if (v->loop_counter == 0)
            v->loop_counter = v->effect_arg;
        else {
            v->loop_counter--;
            if (v->loop_counter == 0) {
                v->loop_row = -1;
                return;
            }
        }
        m->playing_info.loop_row = (int16_t)(v->loop_row - 1);
        m->playing_info.set_loop_row = true;
        v->loop_row = -1;
    }
}

static void dss_eff_retrig(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    if (v->effect_arg < m->playing_info.current_speed) {
        if (m->playing_info.next_retrig_tick == 0)
            m->playing_info.next_retrig_tick = v->effect_arg;
        if (m->playing_info.speed_counter == m->playing_info.next_retrig_tick) {
            m->playing_info.next_retrig_tick += v->effect_arg;
            dss_play_sample(v, c);
        }
    }
}

static void dss_eff_note_delay(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    if (v->effect_arg < m->playing_info.current_speed) {
        if (v->effect_arg == m->playing_info.speed_counter) {
            if (v->sample != 0) {
                if (v->period != 0) {
                    uint16_t period = dss_adjust_fine_tune(v->period, v);
                    v->pitch_period = period;
                    dss_ch_set_period(c, period);
                }
                dss_play_sample(v, c);
            }
        }
    } else {
        v->effect = 0;
        v->effect_arg = 0;
    }
}

static void dss_eff_note_cut(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    if (v->effect_arg < m->playing_info.current_speed) {
        if (v->effect_arg == m->playing_info.speed_counter) {
            v->volume = 0;
            dss_ch_mute(c);
        }
    } else {
        v->effect = 0;
        v->effect_arg = 0;
    }
}

static void dss_eff_set_sample_offset(DssVoiceInfo* v) {
    if (v->effect_arg != 0)
        v->sample_offset = (uint16_t)(v->effect_arg << 7);
    if (v->sample_offset != 0) {
        if (v->sample_offset >= v->sample_length)
            v->sample_length = 0;
        else {
            v->sample_length -= v->sample_offset;
            v->sample_start_offset += v->sample_offset * 2U;
        }
    }
}

static void dss_eff_set_fine_tune(DssVoiceInfo* v) {
    v->fine_tune = (uint8_t)(v->effect_arg & 0x0f);
}

static void dss_eff_portamento(DssVoiceInfo* v, DssChannel* c) {
    if (v->effect_arg != 0) {
        v->portamento_speed = v->effect_arg;
        v->effect_arg = 0;
    }
    dss_do_portamento(v, c);
}

static void dss_eff_porta_vol_up(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    dss_do_portamento(v, c);
    dss_add_volume(m, v->effect_arg, v, c);
}

static void dss_eff_porta_vol_down(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    dss_do_portamento(v, c);
    dss_sub_volume(m, v->effect_arg, v, c);
}

static void dss_eff_porta_control(DssVoiceInfo* v) {
    v->use_tone_porta_for_porta = v->effect_arg != 0;
}

// MakeEffects: real-time effects

static void dss_make_effects(DssModule* m, DssVoiceInfo* v, DssChannel* c) {
    if (v->effect_arg != 0) {
        switch (v->effect) {
            case DSS_EFF_ARPEGGIO:
                dss_eff_arpeggio(m, v, c);
                break;
            case DSS_EFF_SLIDE_UP:
                dss_eff_slide_up(m, v, c);
                break;
            case DSS_EFF_SLIDE_DOWN:
                dss_eff_slide_down(m, v, c);
                break;
            case DSS_EFF_VOLUME_SLIDE_UP:
                dss_add_volume(m, v->effect_arg, v, c);
                break;
            case DSS_EFF_VOLUME_SLIDE_DOWN:
                dss_sub_volume(m, v->effect_arg, v, c);
                break;
            case DSS_EFF_MASTER_VOL_SLIDE_UP:
                dss_add_master(m, v->effect_arg);
                break;
            case DSS_EFF_MASTER_VOL_SLIDE_DOWN:
                dss_sub_master(m, v->effect_arg);
                break;
            case DSS_EFF_RETRIG_NOTE:
                dss_eff_retrig(m, v, c);
                break;
            case DSS_EFF_NOTE_DELAY:
                dss_eff_note_delay(m, v, c);
                break;
            case DSS_EFF_NOTE_CUT:
                dss_eff_note_cut(m, v, c);
                break;
            case DSS_EFF_PORTA_VOL_SLIDE_UP:
                dss_eff_porta_vol_up(m, v, c);
                break;
            case DSS_EFF_PORTA_VOL_SLIDE_DOWN:
                dss_eff_porta_vol_down(m, v, c);
                break;
            default:
                break;
        }
    }

    if (v->effect == DSS_EFF_PORTAMENTO)
        dss_eff_portamento(v, c);
}

// SetupInstrument: parse row

static void dss_setup_instrument(DssModule* m, DssTrackLine* tl, DssVoiceInfo* v, DssChannel* c) {
    v->sample = tl->sample;
    v->period = tl->period;
    v->effect = tl->effect;
    v->effect_arg = tl->effect_arg;

    if (v->sample != 0) {
        DssSample* s = &m->samples[v->sample - 1];
        v->playing_sample_number = (uint8_t)(v->sample - 1);
        v->sample_data = s->data;
        v->sample_data_size = s->data_size;
        v->loop_start = 0;
        v->loop_length = 0;

        if (v->sample_data) {
            v->sample_start_offset = s->start_offset;
            v->sample_length = s->length;
            if (s->loop_length > 1) {
                v->loop_start = s->start_offset + s->loop_start;
                v->loop_length = s->loop_length;
            }
            v->fine_tune = s->fine_tune;
            v->volume = s->volume;
        }
    }

    uint16_t period = v->period;

    if (v->sample != 0 && period != 0) {
        if (v->effect == DSS_EFF_PORTAMENTO || v->effect == DSS_EFF_PORTA_VOL_SLIDE_UP
            || v->effect == DSS_EFF_PORTA_VOL_SLIDE_DOWN) {
            dss_setup_portamento(period, v);
            dss_set_volume(m, v, c);
            return;
        }

        if (period == 0x7ff) {
            dss_ch_mute(c);
            return;
        }

        v->started_period = period;
        v->started_sample = v->sample;

        if (v->effect == DSS_EFF_SET_FINE_TUNE)
            dss_eff_set_fine_tune(v);
        else if (v->effect == DSS_EFF_NOTE_DELAY) {
            dss_set_volume(m, v, c);
            return;
        } else if (v->effect == DSS_EFF_SET_SAMPLE_OFFSET)
            dss_eff_set_sample_offset(v);

        if (v->sample_data && v->sample_length > 0) {
            dss_ch_play(c, v->sample_data, v->sample_data_size, v->sample_start_offset,
                        (uint32_t)((v->sample_length + v->loop_length) * 2));

            period = dss_adjust_fine_tune(period, v);
            v->pitch_period = period;
            dss_ch_set_period(c, period);
        } else
            dss_ch_mute(c);
    }

    switch (v->effect) {
        case DSS_EFF_SET_MASTER_VOLUME:
            dss_eff_set_master_vol(m, v);
            break;
        case DSS_EFF_VOLUME_UP:
            dss_eff_volume_up(m, v, c);
            break;
        case DSS_EFF_VOLUME_DOWN:
            dss_eff_volume_down(m, v, c);
            break;
        case DSS_EFF_MASTER_VOL_UP:
            if (v->effect_arg != 0)
                dss_add_master(m, v->effect_arg);
            break;
        case DSS_EFF_MASTER_VOL_DOWN:
            if (v->effect_arg != 0)
                dss_sub_master(m, v->effect_arg);
            break;
        case DSS_EFF_SET_VOLUME:
            dss_eff_set_volume(m, v, c);
            return;
        default:
            break;
    }

    if (period != 0)
        dss_set_volume(m, v, c);

    switch (v->effect) {
        case DSS_EFF_SET_SONG_SPEED:
            dss_eff_set_speed(m, v);
            break;
        case DSS_EFF_POSITION_JUMP:
            dss_eff_position_jump(m, v);
            break;
        case DSS_EFF_SET_FILTER:
            dss_eff_set_filter(v);
            break;
        case DSS_EFF_PITCH_UP:
            dss_eff_pitch_up(m, v, c);
            break;
        case DSS_EFF_PITCH_DOWN:
            dss_eff_pitch_down(m, v, c);
            break;
        case DSS_EFF_PITCH_CONTROL:
            dss_eff_pitch_control(v);
            break;
        case DSS_EFF_SET_SONG_TEMPO:
            dss_eff_set_tempo(m, v);
            break;
        case DSS_EFF_SET_LOOP_START:
            dss_eff_set_loop_start(m, v);
            break;
        case DSS_EFF_JUMP_TO_LOOP:
            dss_eff_jump_to_loop(m, v);
            break;
        case DSS_EFF_PORTAMENTO_CONTROL:
            dss_eff_porta_control(v);
            break;
        default:
            break;
    }
}

// PlayNextRow

static void dss_play_next_row(DssModule* m) {
    DssGlobalInfo* pi = &m->playing_info;
    pi->new_position = pi->current_position;

    DssPattern* pat = &m->patterns[m->positions[pi->current_position]];
    int16_t row = pi->current_row;
    pi->shown_position = pi->current_position;
    pi->shown_row = (uint16_t)row;

    for (int i = 0; i < 4; i++)
        dss_setup_instrument(m, &pat->tracks[i][row], &m->voices[i], &m->channels[i]);

    for (int i = 0; i < 4; i++) {
        DssVoiceInfo* v = &m->voices[i];
        DssChannel* c = &m->channels[i];
        if (v->effect != DSS_EFF_NOTE_DELAY) {
            if (v->loop_length != 0)
                dss_ch_set_loop(c, v->loop_start, v->loop_length * 2U);
        }
    }

    if (pi->set_loop_row) {
        pi->current_row = pi->loop_row;
        pi->set_loop_row = false;
    }

    pi->current_row++;

    if (pi->current_row >= 64 || pi->position_jump) {
        pi->position_jump = false;
        pi->current_row = 0;

        m->voices[0].loop_row = -1;
        m->voices[1].loop_row = -1;
        m->voices[2].loop_row = -1;
        m->voices[3].loop_row = -1;

        pi->current_position = (uint16_t)(pi->new_position + 1);

        if (pi->current_position == m->num_positions)
            pi->current_position = 0;

        if (dss_has_visited(m, pi->current_position)) {
            m->loop_count++;
            dss_clear_visited(m);
        }

        dss_mark_visited(m, pi->current_position);
    }
}

// Play tick

static void dss_set_bpm(DssModule* m, uint8_t bpm) {
    // BPM-based tempo: 2 * BPM / 5 ticks per second
    m->tick_length = bpm == 0 ? 0 : (uint32_t)(((uint64_t)DSS_TIMEBASE_HZ * 5 + bpm) / (2 * (uint64_t)bpm));
}

static void dss_play_tick(DssModule* m) {
    DssGlobalInfo* pi = &m->playing_info;
    pi->speed_counter++;

    if (pi->speed_counter == pi->current_speed) {
        pi->speed_counter = 0;
        pi->next_retrig_tick = 0;
        pi->arpeggio_counter = 3;
        dss_play_next_row(m);
    } else {
        for (int i = 0; i < 4; i++) {
            dss_make_effects(m, &m->voices[i], &m->channels[i]);
        }
    }
}

// Load module

static bool dss_load(DssModule* m, const uint8_t* data, size_t size) {
    DssReader r;
    dss_reader_init(&r, data, size);

    // Check mark
    if (!dss_probe(data, size))
        return false;

    // Skip mark + 4 extra bytes
    r.pos = 8;

    m->start_tempo = dss_read_u8(&r);
    m->start_speed = dss_read_u8(&r);

    // Read sample info
    for (int i = 0; i < 31; i++) {
        DssSample* s = &m->samples[i];
        for (int n = 0; n < 30; n++)
            s->name[n] = (char)dss_read_u8(&r);
        s->name[30] = '\0';
        s->start_offset = dss_read_b_u32(&r) & 0xfffffffe;
        s->length = dss_read_b_u16(&r);
        s->loop_start = dss_read_b_u32(&r);
        s->loop_length = dss_read_b_u16(&r);
        s->fine_tune = dss_read_u8(&r) & 0x0f;
        s->volume = dss_read_u8(&r);
        s->frequency = dss_read_b_u16(&r);
        if (dss_reader_eof(&r))
            return false;
    }

    // Read positions
    uint16_t num_positions = dss_read_b_u16(&r);
    if (num_positions == 0 || num_positions > DSS_POSITION_SLOTS)
        return false;
    m->num_positions = num_positions;
    m->positions = (uint8_t*)malloc(num_positions);
    if (!m->positions)
        return false;
    for (int i = 0; i < num_positions; i++)
        m->positions[i] = dss_read_u8(&r);
    if (dss_reader_eof(&r))
        return false;
    dss_reader_skip(&r, DSS_POSITION_SLOTS - num_positions);

    // Find highest pattern number
    uint8_t highest = 0;
    for (int i = 0; i < num_positions; i++)
        if (m->positions[i] > highest)
            highest = m->positions[i];

    // Read patterns
    m->num_patterns = highest + 1;
    if (r.pos > r.size || (size_t)m->num_patterns > (r.size - r.pos) / DSS_PATTERN_FILE_SIZE)
        return false;
    m->patterns = (DssPattern*)calloc(m->num_patterns, sizeof(DssPattern));
    if (!m->patterns)
        return false;

    for (int p = 0; p < m->num_patterns; p++) {
        for (int row = 0; row < 64; row++) {
            for (int ch = 0; ch < 4; ch++) {
                uint8_t b1 = dss_read_u8(&r);
                uint8_t b2 = dss_read_u8(&r);
                uint8_t b3 = dss_read_u8(&r);
                uint8_t b4 = dss_read_u8(&r);

                m->patterns[p].tracks[ch][row].sample = b1 >> 3;
                m->patterns[p].tracks[ch][row].period = (uint16_t)(((b1 & 0x07) << 8) | b2);
                m->patterns[p].tracks[ch][row].effect = (DssEffect)b3;
                m->patterns[p].tracks[ch][row].effect_arg = b4;
            }
        }
        if (dss_reader_eof(&r))
            return false;
    }

    // Read sample data
    for (int i = 0; i < 31; i++) {
        DssSample* s = &m->samples[i];
        uint64_t len = s->length;
        if (len == 0)
            continue;

        if (s->loop_length > 1)
            len += s->loop_length;
        len *= 2;
        const uint64_t total = (uint64_t)len + s->start_offset;

        if (r.pos > r.size || total > r.size - r.pos)
            return false;
        s->data = (int8_t*)malloc((size_t)total);
        if (!s->data)
            return false;
        s->data_size = (uint32_t)total;
        memcpy(s->data, r.data + r.pos, (size_t)total);
        r.pos += (size_t)total;
    }

    return true;
}

// Initialize sound

static void dss_initialize_sound(DssModule* m, int start_pos) {
    DssGlobalInfo* pi = &m->playing_info;
    pi->current_tempo = m->start_tempo == 0 ? 125 : m->start_tempo;
    pi->current_speed = m->start_speed;
    pi->speed_counter = 0;
    pi->current_position = (uint16_t)start_pos;
    pi->current_row = 0;
    pi->shown_position = (uint16_t)start_pos;
    pi->shown_row = 0;
    pi->position_jump = false;
    pi->new_position = 0;
    pi->set_loop_row = false;
    pi->loop_row = 0;
    pi->inverse_master_volume = 0;
    pi->next_retrig_tick = 0;
    pi->arpeggio_counter = 3;

    m->loop_count = 0;

    for (int i = 0; i < 4; i++) {
        memset(&m->voices[i], 0, sizeof(DssVoiceInfo));
        memset(&m->channels[i], 0, sizeof(DssChannel));
    }

    dss_set_bpm(m, pi->current_tempo);
}

// Voices

// Hand the registers the tick wrote to the voice. Paula channels 0 and 3 are left, 1 and 2 right.
static void dss_sync_voice(DssModule* m, uint32_t index) {
    DssChannel* c = &m->channels[index];

    if (!c->active || c->sample_data == nullptr) {
        if (nuade_voice_is_playing(m->nuade_voices, index))
            nuade_voice_stop(m->nuade_voices, index);
    } else {
        const NuadeSample sample
            = { .data = c->sample_data,
                .format = NuadeSampleFormat_S8,
                .length = c->sample_length < c->sample_data_size ? c->sample_length : c->sample_data_size,
                .loop_start = c->loop_start,
                .loop_length = c->loop_length };
        if (c->play_pending) {
            nuade_voice_play(m->nuade_voices, index, &sample, c->sample_offset);
        } else {
            // A row can change the loop of the sample that is already playing. Paula latches the new pointers and
            // length when the current pass ends, then repeats from there: a queued sample of just the loop.
            if (nuade_voice_is_playing(m->nuade_voices, index)
                && (c->handed.length != sample.length || c->handed.loop_start != sample.loop_start
                    || c->handed.loop_length != sample.loop_length)
                && sample.loop_length != 0 && sample.loop_start <= sample.length
                && sample.loop_length <= sample.length - sample.loop_start) {
                const NuadeSample repeat = { .data = (const int8_t*)sample.data + sample.loop_start,
                                             .format = NuadeSampleFormat_S8,
                                             .length = sample.loop_length,
                                             .loop_start = 0,
                                             .loop_length = sample.loop_length };
                nuade_voice_queue(m->nuade_voices, index, &repeat);
            }
        }
        c->handed = sample;
    }
    c->play_pending = false;

    // Pitch 0 holds the position, so a channel without a period is silenced by its volume.
    const bool silent = c->period == 0;
    nuade_voice_set_pitch(m->nuade_voices, index, silent ? 0.0 : nuade_paula_period_hz(c->period));
    nuade_voice_set_volume(m->nuade_voices, index, silent ? 0.0f : (float)c->volume / 64.0f);
    nuade_voice_set_pan(m->nuade_voices, index, (index == 0 || index == 3) ? -1.0f : 1.0f);
}

bool dss_tick(DssModule* module, uint32_t* length) {
    for (uint32_t i = 0; i < DSS_VOICES; i++)
        module->channels[i].active = module->channels[i].active && nuade_voice_is_playing(module->nuade_voices, i);

    const uint32_t loops = module->loop_count;
    dss_play_tick(module);
    for (uint32_t i = 0; i < DSS_VOICES; i++)
        dss_sync_voice(module, i);

    *length = module->tick_length;
    return module->loop_count != loops;
}

// Public API

bool dss_probe(const uint8_t* data, size_t size) {
    return data != nullptr && size >= DSS_MIN_SIZE && memcmp(data, "MMU2", 4) == 0;
}

DssModule* dss_create(const uint8_t* data, size_t size) {
    if (!dss_probe(data, size))
        return nullptr;

    DssModule* m = (DssModule*)calloc(1, sizeof(DssModule));
    if (!m)
        return nullptr;

    // Keep original data for export
    m->original_data = (uint8_t*)malloc(size);
    if (m->original_data) {
        memcpy(m->original_data, data, size);
        m->original_size = size;
    }

    if (!dss_load(m, data, size)) {
        dss_destroy(m);
        return nullptr;
    }

    return m;
}

void dss_destroy(DssModule* module) {
    if (!module)
        return;
    if (module->positions)
        free(module->positions);
    if (module->patterns)
        free(module->patterns);
    for (int i = 0; i < 31; i++)
        if (module->samples[i].data)
            free(module->samples[i].data);
    if (module->original_data)
        free(module->original_data);
    free(module);
}

int dss_subsong_count(const DssModule* module) {
    (void)module;
    return 1;
}

bool dss_start(DssModule* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong != 0)
        return false;
    module->nuade_voices = voices;
    dss_clear_visited(module);
    dss_initialize_sound(module, 0);
    dss_mark_visited(module, 0);
    return true;
}

const char* dss_format_name(const DssModule* module) {
    (void)module;
    return "Digital Sound Studio";
}

// Edit API

int dss_get_instrument_count(const DssModule* module) {
    return module ? 31 : 0;
}

int dss_get_num_patterns(const DssModule* module) {
    return module ? module->num_patterns : 0;
}

int dss_get_num_positions(const DssModule* module) {
    return module ? module->num_positions : 0;
}

/**
 * Where the player actually IS, packed as (position << 8) | row.
 *
 * The grid used to be driven by the TypeScript scheduler, which starts
 * counting from the moment PLAY was pressed and knows nothing about this
 * player. Any difference between the two (an engine that takes a moment to
 * start, a position jump, a pattern break) became a permanent offset that
 * nothing corrected. Measured on `digital-sound-studio/zrimay.dss`
 * (2026-09-24): currentRow 39 while the audio was at row 1.
 *
 * Both numbers were already tracked; nothing outside could read them. Rows
 * are 0..63 and positions fit comfortably in the rest, so one int carries
 * both and the caller needs no second crossing.
 */
int dss_get_play_position(const DssModule* module) {
    if (!module)
        return -1;
    return ((int)module->playing_info.current_position << 8) | ((int)module->playing_info.current_row & 0xff);
}

DssPosition dss_position(const DssModule* module) {
    return (DssPosition) { .position = module->playing_info.shown_position,
                           .row = module->playing_info.shown_row,
                           .speed = module->playing_info.current_speed,
                           .tempo = module->playing_info.current_tempo };
}

DssVoiceState dss_voice_state(const DssModule* module, int channel) {
    const DssVoiceInfo* v = &module->voices[channel];
    return (DssVoiceState) { .period = v->started_period, .sample = v->started_sample };
}

DssCell dss_pattern_cell(const DssModule* module, int position, int channel, int row) {
    const DssTrackLine* line = &module->patterns[module->positions[position]].tracks[channel][row];
    return (DssCell) {
        .period = line->period, .sample = line->sample, .effect = (uint8_t)line->effect, .effect_arg = line->effect_arg
    };
}

void dss_get_cell(const DssModule* module, int pattern, int row, int channel, uint8_t* sample, uint16_t* period,
                  uint8_t* effect, uint8_t* effect_arg) {
    if (!module || pattern < 0 || pattern >= module->num_patterns || row < 0 || row >= 64 || channel < 0
        || channel >= 4) {
        if (sample)
            *sample = 0;
        if (period)
            *period = 0;
        if (effect)
            *effect = 0;
        if (effect_arg)
            *effect_arg = 0;
        return;
    }
    const DssTrackLine* tl = &module->patterns[pattern].tracks[channel][row];
    if (sample)
        *sample = tl->sample;
    if (period)
        *period = tl->period;
    if (effect)
        *effect = (uint8_t)tl->effect;
    if (effect_arg)
        *effect_arg = tl->effect_arg;
}

void dss_set_cell(DssModule* module, int pattern, int row, int channel, uint8_t sample, uint16_t period, uint8_t effect,
                  uint8_t effect_arg) {
    if (!module || pattern < 0 || pattern >= module->num_patterns || row < 0 || row >= 64 || channel < 0
        || channel >= 4)
        return;
    DssTrackLine* tl = &module->patterns[pattern].tracks[channel][row];
    tl->sample = sample;
    tl->period = period;
    tl->effect = (DssEffect)effect;
    tl->effect_arg = effect_arg;
}

const char* dss_get_instrument_name(const DssModule* module, int inst) {
    if (!module || inst < 0 || inst >= 31)
        return "";
    return module->samples[inst].name;
}

float dss_get_instrument_param(const DssModule* module, int inst, const char* param) {
    if (!module || inst < 0 || inst >= 31 || !param)
        return -1.0f;
    const DssSample* s = &module->samples[inst];

    if (strcmp(param, "startOffset") == 0)
        return (float)s->start_offset;
    if (strcmp(param, "length") == 0)
        return (float)s->length;
    if (strcmp(param, "loopStart") == 0)
        return (float)s->loop_start;
    if (strcmp(param, "loopLength") == 0)
        return (float)s->loop_length;
    if (strcmp(param, "fineTune") == 0)
        return (float)s->fine_tune;
    if (strcmp(param, "volume") == 0)
        return (float)s->volume;
    if (strcmp(param, "frequency") == 0)
        return (float)s->frequency;

    return -1.0f;
}

void dss_set_instrument_param(DssModule* module, int inst, const char* param, float value) {
    if (!module || inst < 0 || inst >= 31 || !param)
        return;
    DssSample* s = &module->samples[inst];
    uint32_t v32 = (uint32_t)value;
    uint16_t v16 = (uint16_t)value;
    uint8_t v8 = (uint8_t)value;

    if (strcmp(param, "startOffset") == 0) {
        s->start_offset = v32;
        return;
    }
    if (strcmp(param, "length") == 0) {
        s->length = v16;
        return;
    }
    if (strcmp(param, "loopStart") == 0) {
        s->loop_start = v32;
        return;
    }
    if (strcmp(param, "loopLength") == 0) {
        s->loop_length = v16;
        return;
    }
    if (strcmp(param, "fineTune") == 0) {
        s->fine_tune = v8 & 0x0f;
        return;
    }
    if (strcmp(param, "volume") == 0) {
        s->volume = v8;
        return;
    }
    if (strcmp(param, "frequency") == 0) {
        s->frequency = v16;
        return;
    }
}

size_t dss_export(const DssModule* module, uint8_t* out, size_t max_size) {
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
