// SPDX-License-Identifier: MIT
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
// Digital Mugician / Digital Mugician 2 replayer
#include <nuade/players/digmug.h>

#include <stdlib.h>
#include <string.h>

#include "nuade_base.h"
#include "nuade_paula.h"

// Structs

#define DM_WAVEFORM_SIZE 128

// Period Table

#define PERIOD_START_OFFSET 7

static const uint16_t dm_periods[] = {
    4825, 4554, 4299, 4057, 3830, 3615, 3412,

    3220, 3040, 2869, 2708, 2556, 2412, 2277, 2149, 2029, 1915, 1807, 1706, 1610, 1520, 1434, 1354, 1278, 1206, 1139,
    1075, 1014, 957,  904,  853,  805,  760,  717,  677,  639,  603,  569,  537,  507,  479,  452,  426,  403,  380,
    359,  338,  319,  302,  285,  269,  254,  239,  226,  213,  201,  190,  179,  169,  160,  151,  142,  134,  127,

    4842, 4571, 4314, 4072, 3843, 3628, 3424, 3232, 3051, 2879, 2718, 2565, 2421, 2285, 2157, 2036, 1922, 1814, 1712,
    1616, 1525, 1440, 1359, 1283, 1211, 1143, 1079, 1018, 961,  907,  856,  808,  763,  720,  679,  641,  605,  571,
    539,  509,  480,  453,  428,  404,  381,  360,  340,  321,  303,  286,  270,  254,  240,  227,  214,  202,  191,
    180,  170,  160,  151,  143,  135,  127,

    4860, 4587, 4330, 4087, 3857, 3641, 3437, 3244, 3062, 2890, 2728, 2574, 2430, 2294, 2165, 2043, 1929, 1820, 1718,
    1622, 1531, 1445, 1364, 1287, 1215, 1147, 1082, 1022, 964,  910,  859,  811,  765,  722,  682,  644,  607,  573,
    541,  511,  482,  455,  430,  405,  383,  361,  341,  322,  304,  287,  271,  255,  241,  228,  215,  203,  191,
    181,  170,  161,  152,  143,  135,  128,

    4878, 4604, 4345, 4102, 3871, 3654, 3449, 3255, 3073, 2900, 2737, 2584, 2439, 2302, 2173, 2051, 1936, 1827, 1724,
    1628, 1536, 1450, 1369, 1292, 1219, 1151, 1086, 1025, 968,  914,  862,  814,  768,  725,  684,  646,  610,  575,
    543,  513,  484,  457,  431,  407,  384,  363,  342,  323,  305,  288,  272,  256,  242,  228,  216,  203,  192,
    181,  171,  161,  152,  144,  136,  128,

    4895, 4620, 4361, 4116, 3885, 3667, 3461, 3267, 3084, 2911, 2747, 2593, 2448, 2310, 2181, 2058, 1943, 1834, 1731,
    1634, 1542, 1455, 1374, 1297, 1224, 1155, 1090, 1029, 971,  917,  865,  817,  771,  728,  687,  648,  612,  578,
    545,  515,  486,  458,  433,  408,  385,  364,  343,  324,  306,  289,  273,  257,  243,  229,  216,  204,  193,
    182,  172,  162,  153,  144,  136,  129,

    4913, 4637, 4377, 4131, 3899, 3681, 3474, 3279, 3095, 2921, 2757, 2603, 2456, 2319, 2188, 2066, 1950, 1840, 1737,
    1639, 1547, 1461, 1379, 1301, 1228, 1159, 1094, 1033, 975,  920,  868,  820,  774,  730,  689,  651,  614,  580,
    547,  516,  487,  460,  434,  410,  387,  365,  345,  325,  307,  290,  274,  258,  244,  230,  217,  205,  193,
    183,  172,  163,  154,  145,  137,  129,

    4931, 4654, 4393, 4146, 3913, 3694, 3486, 3291, 3106, 2932, 2767, 2612, 2465, 2327, 2196, 2073, 1957, 1847, 1743,
    1645, 1553, 1466, 1384, 1306, 1233, 1163, 1098, 1037, 978,  923,  872,  823,  777,  733,  692,  653,  616,  582,
    549,  518,  489,  462,  436,  411,  388,  366,  346,  326,  308,  291,  275,  259,  245,  231,  218,  206,  194,
    183,  173,  163,  154,  145,  137,  130,

    4948, 4671, 4409, 4161, 3928, 3707, 3499, 3303, 3117, 2942, 2777, 2621, 2474, 2335, 2204, 2081, 1964, 1854, 1750,
    1651, 1559, 1471, 1389, 1311, 1237, 1168, 1102, 1040, 982,  927,  875,  826,  779,  736,  694,  655,  619,  584,
    551,  520,  491,  463,  437,  413,  390,  368,  347,  328,  309,  292,  276,  260,  245,  232,  219,  206,  195,
    184,  174,  164,  155,  146,  138,  130,

    4966, 4688, 4425, 4176, 3942, 3721, 3512, 3315, 3129, 2953, 2787, 2631, 2483, 2344, 2212, 2088, 1971, 1860, 1756,
    1657, 1564, 1477, 1394, 1315, 1242, 1172, 1106, 1044, 985,  930,  878,  829,  782,  738,  697,  658,  621,  586,
    553,  522,  493,  465,  439,  414,  391,  369,  348,  329,  310,  293,  277,  261,  246,  233,  219,  207,  196,
    185,  174,  164,  155,  146,  138,  131,

    4984, 4705, 4441, 4191, 3956, 3734, 3524, 3327, 3140, 2964, 2797, 2640, 2492, 2352, 2220, 2096, 1978, 1867, 1762,
    1663, 1570, 1482, 1399, 1320, 1246, 1176, 1110, 1048, 989,  934,  881,  832,  785,  741,  699,  660,  623,  588,
    555,  524,  495,  467,  441,  416,  392,  370,  350,  330,  312,  294,  278,  262,  247,  233,  220,  208,  196,
    185,  175,  165,  156,  147,  139,  131,

    5002, 4722, 4457, 4206, 3970, 3748, 3537, 3339, 3151, 2974, 2807, 2650, 2501, 2361, 2228, 2103, 1985, 1874, 1769,
    1669, 1576, 1487, 1404, 1325, 1251, 1180, 1114, 1052, 993,  937,  884,  835,  788,  744,  702,  662,  625,  590,
    557,  526,  496,  468,  442,  417,  394,  372,  351,  331,  313,  295,  279,  263,  248,  234,  221,  209,  197,
    186,  175,  166,  156,  148,  139,  131,

    5020, 4739, 4473, 4222, 3985, 3761, 3550, 3351, 3163, 2985, 2818, 2659, 2510, 2369, 2236, 2111, 1992, 1881, 1775,
    1675, 1581, 1493, 1409, 1330, 1255, 1185, 1118, 1055, 996,  940,  887,  838,  791,  746,  704,  665,  628,  592,
    559,  528,  498,  470,  444,  419,  395,  373,  352,  332,  314,  296,  280,  264,  249,  235,  222,  209,  198,
    187,  176,  166,  157,  148,  140,  132,

    5039, 4756, 4489, 4237, 3999, 3775, 3563, 3363, 3174, 2996, 2828, 2669, 2519, 2378, 2244, 2118, 2000, 1887, 1781,
    1681, 1587, 1498, 1414, 1335, 1260, 1189, 1122, 1059, 1000, 944,  891,  841,  794,  749,  707,  667,  630,  594,
    561,  530,  500,  472,  445,  420,  397,  374,  353,  334,  315,  297,  281,  265,  250,  236,  223,  210,  198,
    187,  177,  167,  157,  149,  140,  132,

    5057, 4773, 4505, 4252, 4014, 3788, 3576, 3375, 3186, 3007, 2838, 2679, 2528, 2387, 2253, 2126, 2007, 1894, 1788,
    1688, 1593, 1503, 1419, 1339, 1264, 1193, 1126, 1063, 1003, 947,  894,  844,  796,  752,  710,  670,  632,  597,
    563,  532,  502,  474,  447,  422,  398,  376,  355,  335,  316,  298,  282,  266,  251,  237,  223,  211,  199,
    188,  177,  167,  158,  149,  141,  133,

    5075, 4790, 4521, 4268, 4028, 3802, 3589, 3387, 3197, 3018, 2848, 2688, 2538, 2395, 2261, 2134, 2014, 1901, 1794,
    1694, 1599, 1509, 1424, 1344, 1269, 1198, 1130, 1067, 1007, 951,  897,  847,  799,  754,  712,  672,  634,  599,
    565,  533,  504,  475,  449,  423,  400,  377,  356,  336,  317,  299,  283,  267,  252,  238,  224,  212,  200,
    189,  178,  168,  159,  150,  141,  133,

    5093, 4808, 4538, 4283, 4043, 3816, 3602, 3399, 3209, 3029, 2859, 2698, 2547, 2404, 2269, 2142, 2021, 1908, 1801,
    1700, 1604, 1514, 1429, 1349, 1273, 1202, 1134, 1071, 1011, 954,  900,  850,  802,  757,  715,  675,  637,  601,
    567,  535,  505,  477,  450,  425,  401,  379,  357,  337,  318,  300,  284,  268,  253,  238,  225,  212,  201,
    189,  179,  169,  159,  150,  142,  134

};

static const int DM_PERIODS_COUNT = (int)(sizeof(dm_periods) / sizeof(dm_periods[0]));

// Big-endian reader helpers

typedef struct DmReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
    bool overrun; /* a read or skip went past the end of the data */
} DmReader;

static void reader_init(DmReader* r, const uint8_t* data, size_t size) {
    r->data = data;
    r->size = size;
    r->pos = 0;
    r->overrun = false;
}

/* True when the file ran out: a read (or skip) needed bytes past its end.
 * Being exactly at the end after reading everything is not running out -
 * testing pos >= size refused every module whose data ends with its
 * arpeggio tables, as both Mugician files in the corpus do. */
static bool reader_eof(const DmReader* r) {
    return r->overrun || r->pos > r->size;
}

/* True when `count` records of `unit` bytes still fit in the data; checked before allocating for them. */
static bool reader_can_hold(const DmReader* r, uint64_t count, uint64_t unit) {
    if (r->overrun || r->pos > r->size)
        return false;
    return count <= (uint64_t)(r->size - r->pos) / unit;
}

static void reader_seek(DmReader* r, size_t pos) {
    r->pos = pos;
}

static void reader_skip(DmReader* r, size_t count) {
    r->pos += count;
}

static uint8_t read_u8(DmReader* r) {
    if (r->pos >= r->size) {
        r->overrun = true;
        return 0;
    }
    return r->data[r->pos++];
}

static int8_t read_i8(DmReader* r) {
    return (int8_t)read_u8(r);
}

static uint16_t read_b_u16(DmReader* r) {
    uint8_t hi = read_u8(r);
    uint8_t lo = read_u8(r);
    return (uint16_t)((hi << 8) | lo);
}

static uint32_t read_b_u32(DmReader* r) {
    uint8_t b3 = read_u8(r);
    uint8_t b2 = read_u8(r);
    uint8_t b1 = read_u8(r);
    uint8_t b0 = read_u8(r);
    return ((uint32_t)b3 << 24) | ((uint32_t)b2 << 16) | ((uint32_t)b1 << 8) | (uint32_t)b0;
}

static int32_t read_b_i32(DmReader* r) {
    return (int32_t)read_b_u32(r);
}

static void read_bytes(DmReader* r, uint8_t* dst, size_t count) {
    for (size_t i = 0; i < count; i++)
        dst[i] = read_u8(r);
}

// GetPeriod

static uint16_t get_period(uint16_t note, int16_t transpose, uint16_t fine_tune) {
    int index = PERIOD_START_OFFSET + (int)note + (int)transpose + (int)fine_tune * 64;
    if (index < 0)
        return 0;
    if (index >= DM_PERIODS_COUNT)
        return 0;
    return dm_periods[index];
}

// Sequence access helpers
// Sequences are stored as [4 * seq_len], layout: for each position j, channels k=0..3 at [k * seq_len + j]
// C# layout: sequences[k, j] → we store as [j * 4 + k] for cache locality matching C# iteration order

// Access: sequence[channel, position]
static DmSequence* seq_get(DmSequence* sequences, uint32_t seq_len, int channel, int position) {
    return &sequences[channel * seq_len + position];
}

// Instrument effect handlers

static void do_inst_effect_filter(int8_t* waveform_data) {
    for (int i = 0; i < 127; i++)
        waveform_data[i] = (int8_t)((waveform_data[i] + waveform_data[i + 1]) >> 1);
}

static void do_inst_effect_mixing(int8_t* waveform_data, DmInstrument* instr, int8_t** waveforms) {
    int8_t* source1_data = waveforms[instr->source_wave1];
    int8_t* source2_data = waveforms[instr->source_wave2];

    int index = instr->effect_index & 0x7f; // the loaded start index is a raw byte; a waveform has 128

    instr->effect_index++;
    instr->effect_index &= 0x7f;

    for (int i = 0; i < (int)instr->loop_length; i++) {
        waveform_data[i] = (int8_t)((source1_data[i] + source2_data[index]) >> 1);

        index++;
        index &= 0x7f;
    }
}

static void do_inst_effect_scr_left(int8_t* waveform_data) {
    int8_t first = waveform_data[0];

    for (int i = 0; i < 127; i++)
        waveform_data[i] = waveform_data[i + 1];

    waveform_data[127] = first;
}

static void do_inst_effect_scr_right(int8_t* waveform_data) {
    int8_t last = waveform_data[127];

    for (int i = 126; i >= 0; i--)
        waveform_data[i + 1] = waveform_data[i];

    waveform_data[0] = last;
}

static void do_inst_effect_upsampling(int8_t* waveform_data) {
    int source_index = 0;
    int dest_index = 0;

    for (int i = 0; i < 64; i++) {
        waveform_data[dest_index++] = waveform_data[source_index];
        source_index += 2;
    }

    source_index = 0;

    for (int i = 0; i < 64; i++)
        waveform_data[dest_index++] = waveform_data[source_index++];
}

static void do_inst_effect_downsampling(int8_t* waveform_data) {
    int source_index = 64;
    int dest_index = 128;

    for (int i = 0; i < 64; i++) {
        waveform_data[--dest_index] = waveform_data[--source_index];
        waveform_data[--dest_index] = waveform_data[source_index];
    }
}

static void do_inst_effect_negate(int8_t* waveform_data, DmInstrument* instr) {
    int index = instr->effect_index & 0x7f;
    waveform_data[index] = (int8_t)-waveform_data[index];

    instr->effect_index++;
    if (instr->effect_index >= instr->loop_length)
        instr->effect_index = 0;
}

static void do_inst_effect_mad_mix1(int8_t* waveform_data, DmInstrument* instr, int8_t** waveforms) {
    instr->effect_index++;
    instr->effect_index &= 0x7f;

    int8_t* source2_data = waveforms[instr->source_wave2];

    int8_t increment = source2_data[instr->effect_index];
    int8_t add = 3;

    for (int i = 0; i < (int)instr->loop_length; i++) {
        waveform_data[i] += add;
        add += increment;
    }
}

static void do_inst_effect_addition(int8_t* waveform_data, DmInstrument* instr, int8_t** waveforms) {
    int8_t* source2_data = waveforms[instr->source_wave2];

    for (int i = 0; i < (int)instr->loop_length; i++)
        waveform_data[i] = (int8_t)(source2_data[i] + waveform_data[i]);
}

static void do_inst_effect_filter2(int8_t* waveform_data) {
    for (int i = 0; i < 126; i++)
        waveform_data[i + 1] = (int8_t)((waveform_data[i] * 3 + waveform_data[i + 2]) >> 2);
}

static void do_inst_effect_morphing(int8_t* waveform_data, DmInstrument* instr, int8_t** waveforms) {
    int8_t* source1_data = waveforms[instr->source_wave1];
    int8_t* source2_data = waveforms[instr->source_wave2];

    instr->effect_index++;
    instr->effect_index &= 0x7f;

    int mul1, mul2;

    if (instr->effect_index < 64) {
        mul1 = instr->effect_index;
        mul2 = (instr->effect_index ^ 0xff) & 0x3f;
    } else {
        mul1 = 127 - instr->effect_index;
        mul2 = (mul1 ^ 0xff) & 0x3f;
    }

    for (int i = 0; i < (int)instr->loop_length; i++)
        waveform_data[i] = (int8_t)((source1_data[i] * mul1 + source2_data[i] * mul2) >> 6);
}

static void do_inst_effect_morph_f(int8_t* waveform_data, DmInstrument* instr, int8_t** waveforms) {
    int8_t* source1_data = waveforms[instr->source_wave1];
    int8_t* source2_data = waveforms[instr->source_wave2];

    instr->effect_index++;
    instr->effect_index &= 0x1f;

    int mul1, mul2;

    if (instr->effect_index < 16) {
        mul1 = instr->effect_index;
        mul2 = (instr->effect_index ^ 0xff) & 0x0f;
    } else {
        mul1 = 31 - instr->effect_index;
        mul2 = (mul1 ^ 0xff) & 0x0f;
    }

    for (int i = 0; i < (int)instr->loop_length; i++)
        waveform_data[i] = (int8_t)((source1_data[i] * mul1 + source2_data[i] * mul2) >> 4);
}

static void do_inst_effect_filter3(int8_t* waveform_data) {
    for (int i = 0; i < 126; i++)
        waveform_data[i + 1] = (int8_t)((waveform_data[i] + waveform_data[i + 2]) >> 1);
}

static void do_inst_effect_polygate(int8_t* waveform_data, DmInstrument* instr) {
    int index = instr->effect_index & 0x7f;
    waveform_data[index] = (int8_t)-waveform_data[index];

    index = (index + instr->source_wave2) & (instr->loop_length - 1) & 0x7f;
    waveform_data[index] = (int8_t)-waveform_data[index];

    instr->effect_index++;
    if (instr->effect_index >= instr->loop_length)
        instr->effect_index = 0;
}

static void do_inst_effect_colgate(int8_t* waveform_data, DmInstrument* instr) {
    do_inst_effect_filter(waveform_data);

    instr->effect_index++;
    if (instr->effect_index == instr->source_wave2) {
        instr->effect_index = 0;
        do_inst_effect_upsampling(waveform_data);
    }
}

// Paula register writes of a channel

// Start a new sample: DMA restarts from the first frame
static void channel_play_sample(DmChannel* ch, const int8_t* data, uint32_t start_offset, uint32_t length) {
    ch->sample_data = data;
    ch->sample_offset = start_offset;
    ch->sample_length = start_offset + length;
    ch->active = true;
    ch->loop_start = 0;
    ch->loop_length = 0;
    ch->play_pending = true;
    ch->latch_pending = false;
}

// Loop registers; the play length always reaches the loop end
static void channel_set_loop_no_data(DmChannel* ch, uint32_t start_offset, uint32_t length) {
    ch->loop_start = start_offset;
    ch->loop_length = length;
    if (ch->sample_length < start_offset + length)
        ch->sample_length = start_offset + length;
}

// Loop registers pointing at other data
static void channel_set_loop(DmChannel* ch, const int8_t* data, uint32_t start_offset, uint32_t length) {
    if (data != nullptr) {
        // The new data only holds the loop region; an older, longer play length must not read past it.
        if (ch->sample_data != data && ch->sample_length > start_offset + length)
            ch->sample_length = start_offset + length;
        ch->sample_data = data;
    }
    channel_set_loop_no_data(ch, start_offset, length);
}

static void channel_set_amiga_volume(DmChannel* ch, uint16_t vol) {
    if (vol > 64)
        vol = 64;
    ch->volume = vol;
}

static void channel_set_amiga_period(DmChannel* ch, uint32_t period) {
    ch->period = period;
}

// ChangeSpeed

static void change_speed(DmModule* m, uint16_t new_speed) {
    if (new_speed != m->last_shown_speed) {
        m->last_shown_speed = new_speed;
        m->current_speed = new_speed;
    }
}

// InitializeSound

static void initialize_sound(DmModule* m, int song_number) {
    m->sub_song_number = song_number;

    int real_sub_song = m->sub_song_mapping[song_number];
    m->current_sub_song = &m->sub_songs[real_sub_song];

    m->speed = m->current_sub_song->song_speed;
    m->current_speed
        = (uint16_t)(((m->current_sub_song->song_speed & 0x0f) << 4) | (m->current_sub_song->song_speed & 0x0f));
    m->new_pattern = true;
    m->new_row = true;
    m->current_position = 0;
    m->song_length = m->current_sub_song->number_of_sequences;
    m->current_row = 0;
    m->pattern_length = 64;

    m->last_shown_speed = m->current_speed;

    memset(m->voice_info, 0, sizeof(m->voice_info));
    for (int i = 0; i < 7; i++) {
        m->voice_info[i].shown_note = -1;
        m->voice_info[i].shown_instrument = -1;
    }
    m->shown_position = 0;
    m->shown_row = 0;
    m->shown_rows = 64;

    m->current_sequence = m->sub_song_sequences[real_sub_song];
    m->current_sequence_len = m->sub_song_sequence_length[real_sub_song];

    if (m->module_type == DM_TYPE_DIGITAL_MUGICIAN2) {
        m->current_sequence2 = m->sub_song_sequences[real_sub_song + 1];
        m->current_sequence2_len = m->sub_song_sequence_length[real_sub_song + 1];
    } else {
        m->current_sequence2 = nullptr;
        m->current_sequence2_len = 0;
    }

    memset(m->ch_tab, 0, sizeof(m->ch_tab));
    m->ch_tab_index = 0;

    // Reset channels
    for (int i = 0; i < m->number_of_channels; i++) {
        memset(&m->channels[i], 0, sizeof(DmChannel));
        m->channels[i].volume = 0;
    }
}

// PlayNote

static void play_note(DmModule* m, int channel_number) {
    DmVoiceInfo* vi = &m->voice_info[channel_number];
    DmChannel* ch = &m->channels[channel_number];

    if (m->new_pattern) {
        DmSequence* sequence;

        if ((m->module_type == DM_TYPE_DIGITAL_MUGICIAN) || (channel_number < 3))
            sequence = seq_get(m->current_sequence, m->current_sequence_len, channel_number, m->current_position);
        else
            sequence = seq_get(m->current_sequence2, m->current_sequence2_len, channel_number - 3, m->current_position);

        vi->track = sequence->track_number;
        vi->transpose = sequence->transpose;
    }

    if (m->new_row) {
        DmTrack* track = &m->tracks[vi->track];
        DmTrackRow* row = &track->rows[m->current_row];

        if (row->note != 0) {
            if (row->effect != ((int)DM_EFFECT_NO_WANDER + 62)) {
                vi->last_note = row->note;

                if (row->instrument != 0)
                    vi->last_instrument = (uint16_t)(row->instrument - 1);
            }

            vi->last_instrument &= 0x3f;
            vi->last_effect = row->effect < 64 ? DM_EFFECT_PITCH_BEND : (DmEffect)(row->effect - 62);
            vi->last_effect_param = row->effect_param;

            if (vi->last_instrument < m->number_of_instruments) {
                DmInstrument* instr = &m->instruments[vi->last_instrument];
                vi->fine_tune = instr->finetune;

                if (vi->last_effect == DM_EFFECT_NO_WANDER) {
                    vi->pitch_bend_end_note = row->note;
                    vi->pitch_bend_end_period = get_period(vi->pitch_bend_end_note, vi->transpose, vi->fine_tune);
                } else {
                    vi->pitch_bend_end_note = row->effect;

                    if (vi->last_effect == DM_EFFECT_PITCH_BEND)
                        vi->pitch_bend_end_period = get_period(vi->pitch_bend_end_note, vi->transpose, vi->fine_tune);
                }

                if (vi->last_effect == DM_EFFECT_ARPEGGIO)
                    instr->arpeggio_number = (uint8_t)(vi->last_effect_param & 7);

                uint8_t waveform = instr->waveform_number;

                if (vi->last_effect != DM_EFFECT_NO_WANDER) {
                    if (waveform >= 32) {
                        DmSample* sample = &m->samples[waveform - 32];

                        channel_play_sample(ch, sample->sample_data, sample->start_offset,
                                            sample->end_offset - sample->start_offset);

                        if (sample->loop_start >= 0)
                            channel_set_loop_no_data(ch, (uint32_t)sample->loop_start,
                                                     sample->end_offset - (uint32_t)sample->loop_start);
                    } else {
                        int8_t* waveform_data = m->waveforms[waveform];

                        if (vi->last_effect != DM_EFFECT_NO_DMA)
                            channel_play_sample(ch, waveform_data, 0, instr->loop_length);
                        else {
                            ch->latch_pending = true; // new address latched; takes over at the loop end
                            ch->active = true;
                        }

                        channel_set_loop(ch, waveform_data, 0, instr->loop_length);

                        if (m->module_type == DM_TYPE_DIGITAL_MUGICIAN) {
                            if ((instr->effect != DM_INST_EFFECT_NONE)
                                && (vi->last_effect != DM_EFFECT_NO_INSTRUMENT_EFFECT)
                                && (vi->last_effect != DM_EFFECT_NO_INSTRUMENT_EFFECT_AND_VOLUME)) {
                                int8_t* source = m->waveforms[instr->source_wave1];
                                memmove(waveform_data, source, 128); // the source may be the waveform itself

                                instr->effect_index = 0;
                                vi->instrument_effect_speed = instr->effect_speed;
                            }
                        }
                    }
                }

                if ((vi->last_effect != DM_EFFECT_NO_INSTRUMENT_VOLUME)
                    && (vi->last_effect != DM_EFFECT_NO_INSTRUMENT_EFFECT_AND_VOLUME)
                    && (vi->last_effect != DM_EFFECT_NO_WANDER)) {
                    vi->volume_speed_counter = 1;
                    vi->volume_index = 0;
                }

                if (vi->last_effect != DM_EFFECT_NO_WANDER) {
                    vi->shown_note = vi->last_note + vi->transpose;
                    vi->shown_instrument = vi->last_instrument;
                }

                vi->current_pitch_bend_period = 0;
                vi->instrument_delay = instr->delay;
                vi->pitch_index = 0;
                vi->arpeggio_index = 0;
            }
        }
    }

    switch (vi->last_effect) {
        case DM_EFFECT_PATTERN_LENGTH: {
            if ((vi->last_effect_param != 0) && (vi->last_effect_param <= 64))
                m->pattern_length = vi->last_effect_param;
            break;
        }

        case DM_EFFECT_SONG_SPEED: {
            uint8_t parm = (uint8_t)(vi->last_effect_param & 0x0f);
            uint8_t spd = (uint8_t)(((parm << 4) | parm));

            if ((parm != 0) && (parm <= 15))
                change_speed(m, spd);

            break;
        }

        case DM_EFFECT_FILTER_ON: {
            m->amiga_filter = true;
            break;
        }

        case DM_EFFECT_FILTER_OFF: {
            m->amiga_filter = false;
            break;
        }

        case DM_EFFECT_SHUFFLE: {
            vi->last_effect = DM_EFFECT_NONE;

            if (((vi->last_effect_param & 0x0f) != 0) && ((vi->last_effect_param & 0xf0) != 0))
                change_speed(m, vi->last_effect_param);

            break;
        }

        default:
            break;
    }
}

// DoEffects

static void do_effects(DmModule* m, int channel_number) {
    DmVoiceInfo* vi = &m->voice_info[channel_number];
    DmChannel* ch = &m->channels[channel_number];

    if (vi->last_effect == DM_EFFECT_SWITCH_FILTER)
        m->amiga_filter = !m->amiga_filter;

    if (vi->last_instrument < m->number_of_instruments) {
        DmInstrument* instr = &m->instruments[vi->last_instrument];

        if ((instr->effect != DM_INST_EFFECT_NONE) && (instr->waveform_number < 32)) {
            uint8_t instr_num = (uint8_t)(vi->last_instrument + 1);

            if ((m->ch_tab[0] != instr_num) && (m->ch_tab[1] != instr_num) && (m->ch_tab[2] != instr_num)
                && (m->ch_tab[3] != instr_num)) {
                m->ch_tab[m->ch_tab_index++ & 3] = instr_num;

                if (vi->instrument_effect_speed == 0) {
                    vi->instrument_effect_speed = instr->effect_speed;

                    int8_t* waveform_data = m->waveforms[instr->waveform_number];

                    switch (instr->effect) {
                        case DM_INST_EFFECT_FILTER:
                            do_inst_effect_filter(waveform_data);
                            break;
                        case DM_INST_EFFECT_MIXING:
                            do_inst_effect_mixing(waveform_data, instr, m->waveforms);
                            break;
                        case DM_INST_EFFECT_SCR_LEFT:
                            do_inst_effect_scr_left(waveform_data);
                            break;
                        case DM_INST_EFFECT_SCR_RIGHT:
                            do_inst_effect_scr_right(waveform_data);
                            break;
                        case DM_INST_EFFECT_UPSAMPLE:
                            do_inst_effect_upsampling(waveform_data);
                            break;
                        case DM_INST_EFFECT_DOWNSAMPLE:
                            do_inst_effect_downsampling(waveform_data);
                            break;
                        case DM_INST_EFFECT_NEGATE:
                            do_inst_effect_negate(waveform_data, instr);
                            break;
                        case DM_INST_EFFECT_MAD_MIX1:
                            do_inst_effect_mad_mix1(waveform_data, instr, m->waveforms);
                            break;
                        case DM_INST_EFFECT_ADDITION:
                            do_inst_effect_addition(waveform_data, instr, m->waveforms);
                            break;
                        case DM_INST_EFFECT_FILTER2:
                            do_inst_effect_filter2(waveform_data);
                            break;
                        case DM_INST_EFFECT_MORPHING:
                            do_inst_effect_morphing(waveform_data, instr, m->waveforms);
                            break;
                        case DM_INST_EFFECT_MORPH_F:
                            do_inst_effect_morph_f(waveform_data, instr, m->waveforms);
                            break;
                        case DM_INST_EFFECT_FILTER3:
                            do_inst_effect_filter3(waveform_data);
                            break;
                        case DM_INST_EFFECT_POLYGATE:
                            do_inst_effect_polygate(waveform_data, instr);
                            break;
                        case DM_INST_EFFECT_COLGATE:
                            do_inst_effect_colgate(waveform_data, instr);
                            break;
                        default:
                            break;
                    }
                } else {
                    vi->instrument_effect_speed--;
                }
            }
        }

        if (vi->volume_speed_counter != 0) {
            vi->volume_speed_counter--;

            if (vi->volume_speed_counter == 0) {
                vi->volume_speed_counter = instr->volume_speed;
                vi->volume_index++;
                vi->volume_index &= 0x7f;

                if ((vi->volume_index == 0) && !instr->volume_loop)
                    vi->volume_speed_counter = 0;
                else {
                    // C# code: int volume = -(sbyte)(waveforms[instr.Volume][voiceInfo.VolumeIndex] + 0x81);
                    // volume = (volume & 0xff) / 4;
                    int8_t raw = m->waveforms[instr->volume][vi->volume_index];
                    int volume = -((int8_t)((uint8_t)raw + 0x81));
                    volume = (volume & 0xff) / 4;

                    channel_set_amiga_volume(ch, (uint16_t)volume);
                }
            }
        }

        int note = vi->last_note;

        if (instr->arpeggio_number != 0) {
            uint8_t* arp_data = m->arpeggios[instr->arpeggio_number];
            note += arp_data[vi->arpeggio_index];

            vi->arpeggio_index++;
            vi->arpeggio_index &= 0x1f;
        }

        uint16_t period = get_period((uint16_t)note, vi->transpose, vi->fine_tune);
        vi->note_period = period;

        if ((vi->last_effect == DM_EFFECT_NO_WANDER) || (vi->last_effect == DM_EFFECT_PITCH_BEND)) {
            int period_increment = -(int8_t)vi->last_effect_param;

            vi->current_pitch_bend_period = (int16_t)(vi->current_pitch_bend_period + period_increment);
            vi->note_period = (uint16_t)(vi->note_period + vi->current_pitch_bend_period);

            if (vi->last_effect_param != 0) {
                if (period_increment < 0) {
                    if (vi->note_period <= vi->pitch_bend_end_period) {
                        vi->current_pitch_bend_period = (int16_t)(vi->pitch_bend_end_period - period);
                        vi->last_effect_param = 0;
                    }
                } else {
                    if (vi->note_period >= vi->pitch_bend_end_period) {
                        vi->current_pitch_bend_period = (int16_t)(vi->pitch_bend_end_period - period);
                        vi->last_effect_param = 0;
                    }
                }
            }
        }

        if (instr->pitch != 0) {
            if (vi->instrument_delay == 0) {
                int8_t pitch_waveform_data = m->waveforms[instr->pitch][vi->pitch_index & 0x7f];

                vi->pitch_index++;
                vi->pitch_index &= 0x7f;

                if (vi->pitch_index == 0)
                    vi->pitch_index = instr->pitch_loop;

                vi->note_period = (uint16_t)(vi->note_period - pitch_waveform_data);
            } else {
                vi->instrument_delay--;
            }
        }

        channel_set_amiga_period(ch, vi->note_period);
    }
}

// PlayIt

static void play_it(DmModule* m) {
    m->ch_tab[0] = 0x80;
    m->ch_tab[1] = 0x80;
    m->ch_tab[2] = 0x80;
    m->ch_tab[3] = 0x80;
    m->ch_tab_index = 0;

    for (int i = 0; i < m->number_of_channels; i++)
        play_note(m, i);

    for (int i = 0; i < m->number_of_channels; i++)
        do_effects(m, i);

    m->shown_position = m->current_position;
    m->shown_row = m->current_row;
    m->shown_rows = m->pattern_length;

    m->new_pattern = false;
    m->new_row = false;

    m->speed--;
    if (m->speed == 0) {
        m->speed = (uint16_t)((m->current_speed) & 0x0f);
        change_speed(m, (uint16_t)(((m->current_speed & 0x0f) << 4) | ((m->current_speed & 0xf0) >> 4)));

        m->new_row = true;

        m->current_row++;
        if ((m->current_row == 64) || (m->current_row == m->pattern_length)) {
            m->current_row = 0;
            m->new_pattern = true;

            m->current_position++;
            if (m->current_position == m->song_length) {
                // A loop position past the sequence table (damaged file) restarts the sub-song instead
                if (m->current_sub_song->loop_song && m->current_sub_song->loop_position < m->song_length)
                    m->current_position = m->current_sub_song->loop_position;
                else
                    initialize_sound(m, m->sub_song_number);

                m->loop_count++;
            }
        }
    }
}

// Loading

// A waveform holds DM_WAVEFORM_SIZE bytes, so no instrument loop may be longer.
static uint16_t clamp_loop_length(unsigned length) {
    return (uint16_t)(length > DM_WAVEFORM_SIZE ? DM_WAVEFORM_SIZE : length);
}

// True when every table the instrument indexes during playback has that entry.
static bool instrument_refs_valid(const DmModule* m, const DmInstrument* in) {
    if (in->waveform_number < 32) {
        if (in->waveform_number >= m->number_of_waveforms)
            return false;
    } else if ((uint32_t)in->waveform_number - 32 >= m->number_of_samples) {
        return false;
    }
    if (in->arpeggio_number >= 8)
        return false;
    if (in->volume >= m->number_of_waveforms)
        return false;
    if (in->pitch != 0 && in->pitch >= m->number_of_waveforms)
        return false;
    if (in->effect != DM_INST_EFFECT_NONE
        && (in->source_wave1 >= m->number_of_waveforms || in->source_wave2 >= m->number_of_waveforms))
        return false;
    return true;
}

static bool load_module(DmModule* m, const uint8_t* data, size_t size) {
    DmReader reader;
    reader_init(&reader, data, size);

    // Skip the 24-byte identifier, already validated
    reader_skip(&reader, 24);

    // Read header
    bool arpeggios_enabled = read_b_u16(&reader) != 0;
    m->number_of_tracks = read_b_u16(&reader);

    for (int i = 0; i < 8; i++)
        m->sub_song_sequence_length[i] = read_b_u32(&reader);

    m->number_of_instruments = read_b_u32(&reader);
    m->number_of_waveforms = read_b_u32(&reader);
    m->number_of_samples = read_b_u32(&reader);
    uint32_t samples_size = read_b_u32(&reader);

    if (reader_eof(&reader))
        return false;

    // Read sub-song information
    for (int i = 0; i < 8; i++) {
        DmSubSong* ss = &m->sub_songs[i];

        ss->loop_song = read_u8(&reader) != 0;
        ss->loop_position = read_u8(&reader);
        ss->song_speed = read_u8(&reader);
        ss->number_of_sequences = read_u8(&reader);

        uint8_t buf[12];
        read_bytes(&reader, buf, 12);
        memcpy(ss->name, buf, 12);
        ss->name[12] = '\0';
    }

    if (reader_eof(&reader))
        return false;

    // Read sequences
    for (int i = 0; i < 8; i++) {
        uint32_t seq_len = m->sub_song_sequence_length[i];
        if (!reader_can_hold(&reader, 4ull * seq_len, 2))
            return false;
        DmSequence* sequences = (DmSequence*)calloc(4ull * seq_len, sizeof(DmSequence));
        if (!sequences && seq_len > 0)
            return false;

        for (uint32_t j = 0; j < seq_len; j++) {
            for (int k = 0; k < 4; k++) {
                DmSequence* seq = &sequences[k * seq_len + j];
                seq->track_number = read_u8(&reader);
                seq->transpose = read_i8(&reader);
            }
        }

        m->sub_song_sequences[i] = sequences;
    }

    if (reader_eof(&reader))
        return false;

    // Read instruments
    if (!reader_can_hold(&reader, m->number_of_instruments, 16))
        return false;
    m->instruments = (DmInstrument*)calloc(m->number_of_instruments, sizeof(DmInstrument));
    if (!m->instruments && m->number_of_instruments > 0)
        return false;

    m->instrument_to_sample_info_mapping = (int16_t*)calloc(m->number_of_instruments, sizeof(int16_t));
    if (!m->instrument_to_sample_info_mapping && m->number_of_instruments > 0)
        return false;

    // Track which sample waveform numbers have been seen (for dedup like C#)
    // Use a simple linear scan like the C# Dictionary for small counts
    uint32_t* samples_taken_keys = (uint32_t*)calloc(m->number_of_instruments, sizeof(uint32_t));
    int16_t* samples_taken_vals = (int16_t*)calloc(m->number_of_instruments, sizeof(int16_t));
    if ((!samples_taken_keys || !samples_taken_vals) && m->number_of_instruments > 0) {
        free(samples_taken_keys);
        free(samples_taken_vals);
        return false;
    }
    int samples_taken_count = 0;

    int16_t sample_info_index = 0;

    for (uint32_t i = 0; i < m->number_of_instruments; i++) {
        DmInstrument* inst = &m->instruments[i];

        inst->waveform_number = read_u8(&reader);
        inst->loop_length = clamp_loop_length(read_u8(&reader) * 2);
        inst->volume = read_u8(&reader);
        inst->volume_speed = read_u8(&reader);
        inst->arpeggio_number = read_u8(&reader);
        inst->pitch = read_u8(&reader);
        inst->effect_index = read_u8(&reader);
        inst->delay = read_u8(&reader);
        inst->finetune = read_u8(&reader);
        inst->pitch_loop = read_u8(&reader);
        inst->pitch_speed = read_u8(&reader);
        inst->effect = (DmInstrumentEffect)read_u8(&reader);
        inst->source_wave1 = read_u8(&reader);
        inst->source_wave2 = read_u8(&reader);
        inst->effect_speed = read_u8(&reader);
        inst->volume_loop = read_u8(&reader) != 0;

        if (reader_eof(&reader)) {
            free(samples_taken_keys);
            free(samples_taken_vals);
            return false;
        }

        if (inst->waveform_number < 32) {
            m->instrument_to_sample_info_mapping[i] = sample_info_index++;
        } else {
            // Check if this sample waveform number was already seen
            bool found = false;
            for (int s = 0; s < samples_taken_count; s++) {
                if (samples_taken_keys[s] == inst->waveform_number) {
                    m->instrument_to_sample_info_mapping[i] = samples_taken_vals[s];
                    found = true;
                    break;
                }
            }
            if (!found) {
                samples_taken_keys[samples_taken_count] = inst->waveform_number;
                samples_taken_vals[samples_taken_count] = sample_info_index;
                samples_taken_count++;
                m->instrument_to_sample_info_mapping[i] = sample_info_index++;
            }
        }
    }

    free(samples_taken_keys);
    free(samples_taken_vals);

    // Read waveforms
    if (!reader_can_hold(&reader, m->number_of_waveforms, DM_WAVEFORM_SIZE))
        return false;
    m->waveforms = (int8_t**)calloc(m->number_of_waveforms, sizeof(int8_t*));
    if (!m->waveforms && m->number_of_waveforms > 0)
        return false;

    for (uint32_t i = 0; i < m->number_of_waveforms; i++) {
        m->waveforms[i] = (int8_t*)calloc(DM_WAVEFORM_SIZE, sizeof(int8_t));
        if (!m->waveforms[i])
            return false;

        for (int j = 0; j < DM_WAVEFORM_SIZE; j++)
            m->waveforms[i][j] = read_i8(&reader);

        if (reader_eof(&reader))
            return false;
    }

    // Read sample information
    if (!reader_can_hold(&reader, m->number_of_samples, 32))
        return false;
    m->samples = (DmSample*)calloc(m->number_of_samples, sizeof(DmSample));
    if (!m->samples && m->number_of_samples > 0)
        return false;

    for (uint32_t i = 0; i < m->number_of_samples; i++) {
        DmSample* sample = &m->samples[i];

        sample->start_offset = read_b_u32(&reader);
        sample->end_offset = read_b_u32(&reader);
        sample->loop_start = read_b_i32(&reader);

        if (reader_eof(&reader))
            return false;

        reader_skip(&reader, 20); // skip 20 bytes
    }

    // Read tracks
    if (!reader_can_hold(&reader, m->number_of_tracks, 64 * sizeof(DmTrackRow)))
        return false;
    m->tracks = (DmTrack*)calloc(m->number_of_tracks, sizeof(DmTrack));
    if (!m->tracks && m->number_of_tracks > 0)
        return false;

    for (uint32_t i = 0; i < m->number_of_tracks; i++) {
        DmTrack* track = &m->tracks[i];

        for (int j = 0; j < 64; j++) {
            DmTrackRow* row = &track->rows[j];

            row->note = read_u8(&reader);
            row->instrument = read_u8(&reader);
            row->effect = read_u8(&reader);
            row->effect_param = read_u8(&reader);
        }

        if (reader_eof(&reader))
            return false;
    }

    // Read sample data and fix-up information
    size_t sample_start_offset = reader.pos;

    for (uint32_t i = 0; i < m->number_of_samples; i++) {
        DmSample* sample = &m->samples[i];

        if (sample->end_offset <= sample->start_offset) {
            sample->sample_data = nullptr;
            sample->start_offset = 0;
            sample->end_offset = 0;
            sample->loop_start = -1;
            continue;
        }

        const uint32_t length = sample->end_offset - sample->start_offset;
        const uint64_t read_pos = (uint64_t)sample_start_offset + sample->start_offset;
        if (read_pos > size || length > size - read_pos)
            return false;

        sample->sample_data = (int8_t*)calloc(length, sizeof(int8_t));
        if (!sample->sample_data)
            return false;

        memcpy(sample->sample_data, data + read_pos, length);

        const int64_t loop_start = (int64_t)sample->loop_start - (int64_t)sample->start_offset;
        sample->loop_start
            = (sample->loop_start != 0 && loop_start >= 0 && loop_start <= (int64_t)length) ? (int32_t)loop_start : -1;

        sample->start_offset = 0;
        sample->end_offset = length;
    }

    // Read arpeggios
    memset(m->arpeggios, 0, sizeof(m->arpeggios));

    if (arpeggios_enabled) {
        size_t arp_offset = sample_start_offset + (size_t)samples_size;
        reader_seek(&reader, arp_offset);

        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 32; j++)
                m->arpeggios[i][j] = read_u8(&reader);

            if (reader_eof(&reader))
                return false;
        }
    }

    for (uint32_t i = 0; i < m->number_of_instruments; i++) {
        if (!instrument_refs_valid(m, &m->instruments[i]))
            return false;
    }

    return true;
}

// True when every position of a sub-song names an existing track, in both sequence tables of a Mugician 2 song.
static bool sub_song_valid(const DmModule* m, int index) {
    const int tables = (m->module_type == DM_TYPE_DIGITAL_MUGICIAN2) ? 2 : 1;
    const uint32_t song_length = m->sub_song_sequence_length[index];

    for (int t = 0; t < tables; t++) {
        const uint32_t seq_len = m->sub_song_sequence_length[index + t];
        if (seq_len < song_length)
            return false;
        const DmSequence* sequences = m->sub_song_sequences[index + t];
        for (uint64_t j = 0; j < 4ull * seq_len; j++) {
            if (sequences[j].track_number >= m->number_of_tracks)
                return false;
        }
    }
    return true;
}

// A sub-song is playable when its sequence table is complete, names a note somewhere and refers to existing tracks.
static bool sub_song_playable(const DmModule* m, int index) {
    const uint32_t seq_len = m->sub_song_sequence_length[index];
    if (seq_len != m->sub_songs[index].number_of_sequences)
        return false;

    const DmSequence* sequences = m->sub_song_sequences[index];
    bool has_content = false;
    for (uint64_t j = 0; j < 4ull * seq_len; j++) {
        if (sequences[j].track_number != 0 || sequences[j].transpose != 0)
            has_content = true;
    }
    return has_content && sub_song_valid(m, index);
}

// InitPlayer: find playable sub-songs

static bool init_player(DmModule* m) {
    m->number_of_channels = (m->module_type == DM_TYPE_DIGITAL_MUGICIAN) ? 4 : 7;

    int increment = (m->module_type == DM_TYPE_DIGITAL_MUGICIAN) ? 1 : 2;

    int count = 0;
    for (int i = 0; i < 8; i += increment) {
        if (sub_song_playable(m, i))
            count++;
    }

    m->sub_song_count = count;
    m->sub_song_mapping = (int*)calloc(count, sizeof(int));
    if (!m->sub_song_mapping && count > 0)
        return false;

    int idx = 0;
    for (int i = 0; i < 8; i += increment) {
        if (sub_song_playable(m, i))
            m->sub_song_mapping[idx++] = i;
    }

    return true;
}

// Voices: Mugician channel n is voice n. Channels 0 and 3 play left, 1 and 2 right; Mugician 2's extra channels left.

static float channel_pan(const DmModule* m, int channel) {
    if (m->module_type == DM_TYPE_DIGITAL_MUGICIAN)
        return (channel == 0 || channel == 3) ? -1.0f : 1.0f;
    return (channel == 1 || channel == 2) ? 1.0f : -1.0f;
}

// Hand the registers the tick wrote to the voice.
static void sync_voice(DmModule* m, int channel) {
    DmChannel* c = &m->channels[channel];
    const uint32_t index = (uint32_t)channel;

    if (!c->active || c->sample_data == nullptr) {
        if (nuade_voice_is_playing(m->voices, index))
            nuade_voice_stop(m->voices, index);
    } else if (c->play_pending || c->latch_pending) {
        const NuadeSample sample = { .data = c->sample_data,
                                     .format = NuadeSampleFormat_S8,
                                     .length = c->sample_length,
                                     .loop_start = c->loop_start,
                                     .loop_length = c->loop_length };
        if (c->play_pending)
            nuade_voice_play(m->voices, index, &sample, c->sample_offset);
        else if (sample.length != 0) {
            // The hardware keeps DMA on: a channel that is not playing takes the latched address at once.
            if (nuade_voice_is_playing(m->voices, index))
                nuade_voice_queue(m->voices, index, &sample);
            else
                nuade_voice_play(m->voices, index, &sample, c->loop_start);
        }
    }
    c->play_pending = false;
    c->latch_pending = false;

    const bool silent = c->muted || c->period == 0;
    nuade_voice_set_pitch(m->voices, index, silent ? 0.0 : nuade_paula_period_hz(c->period));
    nuade_voice_set_volume(m->voices, index, silent ? 0.0f : (float)c->volume / 64.0f);
    nuade_voice_set_pan(m->voices, index, channel_pan(m, channel));
}

bool dm_tick(DmModule* module) {
    // A one-shot that ran off its end with a sample queued behind it still counts as playing; the mixer switches on the
    // next frame.
    for (int i = 0; i < module->number_of_channels; i++) {
        DmChannel* c = &module->channels[i];
        c->active = c->active && nuade_voice_is_playing(module->voices, (uint32_t)i);
    }

    const uint32_t loops = module->loop_count;
    play_it(module);

    for (int i = 0; i < module->number_of_channels; i++)
        sync_voice(module, i);
    return module->loop_count != loops;
}

// Public API

static DmModuleType sniff_type(const uint8_t* data, size_t size) {
    if (!data || size < 204)
        return DM_TYPE_UNKNOWN;
    if (memcmp(data, " MUGICIAN/SOFTEYES 1990 ", 24) == 0)
        return DM_TYPE_DIGITAL_MUGICIAN;
    if (memcmp(data, " MUGICIAN2/SOFTEYES 1990", 24) == 0)
        return DM_TYPE_DIGITAL_MUGICIAN2;
    return DM_TYPE_UNKNOWN;
}

bool dm_probe(const uint8_t* data, size_t size) {
    return sniff_type(data, size) != DM_TYPE_UNKNOWN;
}

DmModule* dm_create(const uint8_t* data, size_t size) {
    const DmModuleType type = sniff_type(data, size);
    if (type == DM_TYPE_UNKNOWN)
        return nullptr;

    DmModule* m = (DmModule*)calloc(1, sizeof(DmModule));
    if (!m)
        return nullptr;

    m->module_type = type;
    m->file_size = size;
    // Keep original data for export
    m->original_data = (uint8_t*)malloc(size);
    if (m->original_data) {
        memcpy(m->original_data, data, size);
        m->original_size = size;
    }

    if (!load_module(m, data, size)) {
        dm_destroy(m);
        return nullptr;
    }

    if (!init_player(m)) {
        dm_destroy(m);
        return nullptr;
    }

    return m;
}

void dm_destroy(DmModule* module) {
    if (!module)
        return;

    if (module->sub_song_mapping)
        free(module->sub_song_mapping);

    for (int i = 0; i < 8; i++) {
        if (module->sub_song_sequences[i])
            free(module->sub_song_sequences[i]);
    }

    if (module->tracks)
        free(module->tracks);

    if (module->instruments)
        free(module->instruments);

    if (module->instrument_to_sample_info_mapping)
        free(module->instrument_to_sample_info_mapping);

    if (module->waveforms) {
        for (uint32_t i = 0; i < module->number_of_waveforms; i++) {
            if (module->waveforms[i])
                free(module->waveforms[i]);
        }
        free(module->waveforms);
    }

    if (module->samples) {
        for (uint32_t i = 0; i < module->number_of_samples; i++) {
            if (module->samples[i].sample_data)
                free(module->samples[i].sample_data);
        }
        free(module->samples);
    }

    if (module->original_data)
        free(module->original_data);
    free(module);
}

int dm_subsong_count(const DmModule* module) {
    if (!module)
        return 0;
    return module->sub_song_count;
}

bool dm_start(DmModule* module, int subsong, NuadeVoices* voices) {
    if (!module || !voices || subsong < 0 || subsong >= module->sub_song_count)
        return false;

    module->voices = voices;
    module->loop_count = 0;
    initialize_sound(module, subsong);
    return true;
}

// Digital Mugician 2 runs from the audio interrupt of its software mixer: one tick per mix buffer. The buffer is
// MixLength words at the default mixing rate (Paula period 149); the module whose size is exactly the MoonChild
// file size has its BPM raised to 202, which shortens the buffer by 130/210. The original compares a size computed
// from the header, which equals the file size for every module in the corpus.
#define DM2_PAULA_CLOCK_HZ 3546895u
#define DM2_MIX_PERIOD 149u
#define DM2_MIX_ROOT 71591u
#define DM2_MOONCHILD_SIZE 625381u
#define DM2_MOONCHILD_TEMPO_NUM 130u
#define DM2_MOONCHILD_TEMPO_DEN 210u

static uint32_t dm2_mix_length(const DmModule* module) {
    uint32_t length = DM2_MIX_ROOT / DM2_MIX_PERIOD + 1;
    if (module->file_size == DM2_MOONCHILD_SIZE) {
        length = length * DM2_MOONCHILD_TEMPO_NUM / DM2_MOONCHILD_TEMPO_DEN;
    }
    return length / 2;
}

uint32_t dm_timebase_hz(const DmModule* module) {
    return module->module_type == DM_TYPE_DIGITAL_MUGICIAN2 ? DM2_PAULA_CLOCK_HZ : 50;
}

uint32_t dm_tick_length(const DmModule* module) {
    return module->module_type == DM_TYPE_DIGITAL_MUGICIAN2 ? DM2_MIX_PERIOD * 2 * dm2_mix_length(module) : 1;
}

int dm_channel_count(const DmModule* module) {
    if (!module)
        return 0;
    return module->number_of_channels;
}

void dm_set_channel_mask(DmModule* module, uint32_t mask) {
    if (!module)
        return;

    for (int i = 0; i < module->number_of_channels; i++)
        module->channels[i].muted = ((mask >> i) & 1) == 0;
}

const char* dm_title(const DmModule* module) {
    if (!module || !module->current_sub_song)
        return "";
    return module->current_sub_song->name;
}

const char* dm_format_name(const DmModule* module) {
    if (!module)
        return "";
    return module->module_type == DM_TYPE_DIGITAL_MUGICIAN2 ? "Digital Mugician 2" : "Digital Mugician";
}

// Edit API

int dm_get_instrument_count(const DmModule* module) {
    return module ? (int)module->number_of_instruments : 0;
}

int dm_get_num_tracks(const DmModule* module) {
    return module ? (int)module->number_of_tracks : 0;
}

void dm_get_cell(const DmModule* module, int track_idx, int row, uint8_t* note, uint8_t* instrument, uint8_t* effect,
                 uint8_t* effect_arg) {
    if (!module || track_idx < 0 || track_idx >= (int)module->number_of_tracks || row < 0 || row >= 64
        || !module->tracks) {
        if (note)
            *note = 0;
        if (instrument)
            *instrument = 0;
        if (effect)
            *effect = 0;
        if (effect_arg)
            *effect_arg = 0;
        return;
    }
    const DmTrackRow* r = &module->tracks[track_idx].rows[row];
    if (note)
        *note = r->note;
    if (instrument)
        *instrument = r->instrument;
    if (effect)
        *effect = r->effect;
    if (effect_arg)
        *effect_arg = r->effect_param;
}

void dm_set_cell(DmModule* module, int track_idx, int row, uint8_t note, uint8_t instrument, uint8_t effect,
                 uint8_t effect_arg) {
    if (!module || track_idx < 0 || track_idx >= (int)module->number_of_tracks || row < 0 || row >= 64
        || !module->tracks)
        return;
    DmTrackRow* r = &module->tracks[track_idx].rows[row];
    r->note = note;
    r->instrument = instrument;
    r->effect = effect;
    r->effect_param = effect_arg;
}

float dm_get_instrument_param(const DmModule* module, int inst, const char* param) {
    if (!module || inst < 0 || inst >= (int)module->number_of_instruments || !param || !module->instruments)
        return -1.0f;
    const DmInstrument* in = &module->instruments[inst];

    if (strcmp(param, "waveformNumber") == 0)
        return (float)in->waveform_number;
    if (strcmp(param, "loopLength") == 0)
        return (float)in->loop_length;
    if (strcmp(param, "finetune") == 0)
        return (float)in->finetune;
    if (strcmp(param, "arpeggioNumber") == 0)
        return (float)in->arpeggio_number;
    if (strcmp(param, "volume") == 0)
        return (float)in->volume;
    if (strcmp(param, "volumeSpeed") == 0)
        return (float)in->volume_speed;
    if (strcmp(param, "volumeLoop") == 0)
        return (float)in->volume_loop;
    if (strcmp(param, "pitch") == 0)
        return (float)in->pitch;
    if (strcmp(param, "pitchSpeed") == 0)
        return (float)in->pitch_speed;
    if (strcmp(param, "pitchLoop") == 0)
        return (float)in->pitch_loop;
    if (strcmp(param, "delay") == 0)
        return (float)in->delay;
    if (strcmp(param, "effect") == 0)
        return (float)in->effect;
    if (strcmp(param, "effectSpeed") == 0)
        return (float)in->effect_speed;
    if (strcmp(param, "effectIndex") == 0)
        return (float)in->effect_index;
    if (strcmp(param, "sourceWave1") == 0)
        return (float)in->source_wave1;
    if (strcmp(param, "sourceWave2") == 0)
        return (float)in->source_wave2;

    return -1.0f;
}

static void apply_instrument_param(DmInstrument* in, const char* param, float value) {
    uint8_t b = (uint8_t)value;
    uint16_t v = (uint16_t)value;

    if (strcmp(param, "waveformNumber") == 0) {
        in->waveform_number = b;
        return;
    }
    if (strcmp(param, "loopLength") == 0) {
        in->loop_length = clamp_loop_length(v);
        return;
    }
    if (strcmp(param, "finetune") == 0) {
        in->finetune = b;
        return;
    }
    if (strcmp(param, "arpeggioNumber") == 0) {
        in->arpeggio_number = b;
        return;
    }
    if (strcmp(param, "volume") == 0) {
        in->volume = b;
        return;
    }
    if (strcmp(param, "volumeSpeed") == 0) {
        in->volume_speed = b;
        return;
    }
    if (strcmp(param, "volumeLoop") == 0) {
        in->volume_loop = (bool)(int)value;
        return;
    }
    if (strcmp(param, "pitch") == 0) {
        in->pitch = b;
        return;
    }
    if (strcmp(param, "pitchSpeed") == 0) {
        in->pitch_speed = b;
        return;
    }
    if (strcmp(param, "pitchLoop") == 0) {
        in->pitch_loop = b;
        return;
    }
    if (strcmp(param, "delay") == 0) {
        in->delay = b;
        return;
    }
    if (strcmp(param, "effect") == 0) {
        in->effect = (DmInstrumentEffect)(int)value;
        return;
    }
    if (strcmp(param, "effectSpeed") == 0) {
        in->effect_speed = b;
        return;
    }
    if (strcmp(param, "effectIndex") == 0) {
        in->effect_index = b;
        return;
    }
    if (strcmp(param, "sourceWave1") == 0) {
        in->source_wave1 = b;
        return;
    }
    if (strcmp(param, "sourceWave2") == 0) {
        in->source_wave2 = b;
        return;
    }
}

void dm_set_instrument_param(DmModule* module, int inst, const char* param, float value) {
    if (!module || inst < 0 || inst >= (int)module->number_of_instruments || !param || !module->instruments)
        return;
    DmInstrument* in = &module->instruments[inst];
    const DmInstrument previous = *in;
    apply_instrument_param(in, param, value);
    if (!instrument_refs_valid(module, in))
        *in = previous;
}

size_t dm_export(const DmModule* module, uint8_t* out, size_t max_size) {
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

// Display API

DmPosition dm_position(const DmModule* module) {
    return (DmPosition) { .order = module->shown_position,
                          .row = module->shown_row,
                          .rows = module->shown_rows,
                          .speed = (module->current_speed >> 4) & 0x0f };
}

int dm_position_count(const DmModule* module) {
    return module->current_sub_song != nullptr ? module->song_length : 0;
}

DmVoiceState dm_voice_state(const DmModule* module, int channel) {
    const DmVoiceInfo* vi = &module->voice_info[channel];
    return (DmVoiceState) { .note = vi->shown_note, .instrument = vi->shown_instrument };
}

DmPatternCell dm_pattern_cell(const DmModule* module, int position, int channel, int row) {
    DmPatternCell cell = { 0 };
    if (module->current_sub_song == nullptr || position < 0 || position >= module->song_length)
        return cell;
    const DmSequence* sequence;
    if (module->module_type == DM_TYPE_DIGITAL_MUGICIAN || channel < 3)
        sequence = &module->current_sequence[channel * module->current_sequence_len + position];
    else
        sequence = &module->current_sequence2[(channel - 3) * module->current_sequence2_len + position];
    const DmTrackRow* r = &module->tracks[sequence->track_number].rows[row];
    return (DmPatternCell) {
        .note = r->note, .instrument = r->instrument, .effect = r->effect, .effect_arg = r->effect_param
    };
}
