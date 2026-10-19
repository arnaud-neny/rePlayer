// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniel Collin
// Ported from NostalgicPlayer C# implementation by Thomas Neumann (MIT)
//
// Supports AON4 (4 channels) and AON8 (8 channels).
// Output: interleaved stereo float samples at configurable sample rate.

#include "aon_player.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef nullptr
#define nullptr ((void*)0)
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef float f32;
typedef double f64;

// PAL clock frequency for Amiga period-to-Hz conversion
#define AON_PAL_CLOCK 3546895.0

// Internal limits
#define AON_MAX_INSTRUMENTS 128
#define AON_MAX_PATTERNS 256
#define AON_MAX_WAVEFORMS 256

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Period table: 16 fine-tune rows x 60 notes (5 octaves x 12 notes)
// Tuning 0-7 = positive fine tune, 8-15 = negative fine tune (-8 to -1)

static const u16 aon_period_table[16][60] = {
    // Tuning 0, normal
    {
        3434, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1812, 1712, 1616, 1524,
        1440, 1356, 1280, 1208, 1140, 1076, 1016, 960,  906,  856,  808,  762,  720,  678,  640,
        604,  570,  538,  508,  480,  453,  428,  404,  381,  360,  339,  320,  302,  285,  269,
        254,  240,  226,  214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,  113,
    },
    // Tuning 1
    {
        3400, 3208, 3028, 2860, 2696, 2548, 2404, 2268, 2140, 2020, 1908, 1800, 1700, 1604, 1514,
        1430, 1348, 1274, 1202, 1134, 1070, 1010, 954,  900,  850,  802,  757,  715,  674,  637,
        601,  567,  535,  505,  477,  450,  425,  401,  379,  357,  337,  318,  300,  284,  268,
        253,  239,  225,  213,  201,  189,  179,  169,  159,  150,  142,  134,  126,  119,  113,
    },
    // Tuning 2
    {
        3376, 3184, 3008, 2836, 2680, 2528, 2388, 2252, 2128, 2008, 1896, 1788, 1688, 1592, 1504,
        1418, 1340, 1264, 1194, 1126, 1064, 1004, 948,  894,  844,  796,  752,  709,  670,  632,
        597,  563,  532,  502,  474,  447,  422,  398,  376,  355,  335,  316,  298,  282,  266,
        251,  237,  224,  211,  199,  188,  177,  167,  158,  149,  141,  133,  125,  118,  112,
    },
    // Tuning 3
    {
        3352, 3164, 2984, 2816, 2660, 2512, 2368, 2236, 2112, 1992, 1880, 1776, 1676, 1582, 1492,
        1408, 1330, 1256, 1184, 1118, 1056, 996,  940,  888,  838,  791,  746,  704,  665,  628,
        592,  559,  528,  498,  470,  444,  419,  395,  373,  352,  332,  314,  296,  280,  264,
        249,  235,  222,  209,  198,  187,  176,  166,  157,  148,  140,  132,  125,  118,  111,
    },
    // Tuning 4
    {
        3328, 3140, 2964, 2796, 2640, 2492, 2352, 2220, 2096, 1980, 1868, 1764, 1664, 1570, 1482,
        1398, 1320, 1246, 1176, 1110, 1048, 990,  934,  882,  832,  785,  741,  699,  660,  623,
        588,  555,  524,  495,  467,  441,  416,  392,  370,  350,  330,  312,  294,  278,  262,
        247,  233,  220,  208,  196,  185,  175,  165,  156,  147,  139,  131,  124,  117,  110,
    },
    // Tuning 5
    {
        3304, 3116, 2944, 2776, 2620, 2476, 2336, 2204, 2080, 1964, 1852, 1748, 1652, 1558, 1472,
        1388, 1310, 1238, 1168, 1102, 1040, 982,  926,  874,  826,  779,  736,  694,  655,  619,
        584,  551,  520,  491,  463,  437,  413,  390,  368,  347,  328,  309,  292,  276,  260,
        245,  232,  219,  206,  195,  184,  174,  164,  155,  146,  138,  130,  123,  116,  109,
    },
    // Tuning 6
    {
        3280, 3096, 2920, 2756, 2604, 2456, 2320, 2188, 2064, 1948, 1840, 1736, 1640, 1548, 1460,
        1378, 1302, 1228, 1160, 1094, 1032, 974,  920,  868,  820,  774,  730,  689,  651,  614,
        580,  547,  516,  487,  460,  434,  410,  387,  365,  345,  325,  307,  290,  274,  258,
        244,  230,  217,  205,  193,  183,  172,  163,  154,  145,  137,  129,  122,  115,  109,
    },
    // Tuning 7
    {
        3256, 3072, 2900, 2736, 2584, 2440, 2300, 2172, 2052, 1936, 1828, 1724, 1628, 1536, 1450,
        1368, 1292, 1220, 1150, 1086, 1026, 968,  914,  862,  814,  768,  725,  684,  646,  610,
        575,  543,  513,  484,  457,  431,  407,  384,  363,  342,  323,  305,  288,  272,  256,
        242,  228,  216,  204,  192,  181,  171,  161,  152,  144,  136,  128,  121,  114,  108,
    },
    // Tuning -8 (index 8)
    {
        3628, 3424, 3232, 3048, 2880, 2712, 2560, 2416, 2280, 2152, 2032, 1920, 1814, 1712, 1616,
        1524, 1440, 1356, 1280, 1208, 1140, 1076, 1016, 960,  907,  856,  808,  762,  720,  678,
        640,  604,  570,  538,  508,  480,  453,  428,  404,  381,  360,  339,  320,  302,  285,
        269,  254,  240,  226,  214,  202,  190,  180,  170,  160,  151,  143,  135,  127,  120,
    },
    // Tuning -7 (index 9)
    {
        3600, 3400, 3208, 3028, 2860, 2700, 2544, 2404, 2268, 2140, 2020, 1908, 1800, 1700, 1604,
        1514, 1430, 1350, 1272, 1202, 1134, 1070, 1010, 954,  900,  850,  802,  757,  715,  675,
        636,  601,  567,  535,  505,  477,  450,  425,  401,  379,  357,  337,  318,  300,  284,
        268,  253,  238,  225,  212,  200,  189,  179,  169,  159,  150,  142,  134,  126,  119,
    },
    // Tuning -6 (index 10)
    {
        3576, 3376, 3184, 3008, 2836, 2680, 2528, 2388, 2252, 2128, 2008, 1896, 1788, 1688, 1592,
        1504, 1418, 1340, 1264, 1194, 1126, 1064, 1004, 948,  894,  844,  796,  752,  709,  670,
        632,  597,  563,  532,  502,  474,  447,  422,  398,  376,  355,  335,  316,  298,  282,
        266,  251,  237,  223,  211,  199,  188,  177,  167,  158,  149,  141,  133,  125,  118,
    },
    // Tuning -5 (index 11)
    {
        3548, 3352, 3164, 2984, 2816, 2660, 2512, 2368, 2236, 2112, 1992, 1880, 1774, 1676, 1582,
        1492, 1408, 1330, 1256, 1184, 1118, 1056, 996,  940,  887,  838,  791,  746,  704,  665,
        628,  592,  559,  528,  498,  470,  444,  419,  395,  373,  352,  332,  314,  296,  280,
        264,  249,  235,  222,  209,  198,  187,  176,  166,  157,  148,  140,  132,  125,  118,
    },
    // Tuning -4 (index 12)
    {
        3524, 3328, 3140, 2964, 2796, 2640, 2492, 2352, 2220, 2096, 1976, 1868, 1762, 1664, 1570,
        1482, 1398, 1320, 1246, 1176, 1110, 1048, 988,  934,  881,  832,  785,  741,  699,  660,
        623,  588,  555,  524,  494,  467,  441,  416,  392,  370,  350,  330,  312,  294,  278,
        262,  247,  233,  220,  208,  196,  185,  175,  165,  156,  147,  139,  131,  123,  117,
    },
    // Tuning -3 (index 13)
    {
        3500, 3304, 3116, 2944, 2776, 2620, 2476, 2336, 2204, 2080, 1964, 1852, 1750, 1652, 1558,
        1472, 1388, 1310, 1238, 1168, 1102, 1040, 982,  926,  875,  826,  779,  736,  694,  655,
        619,  584,  551,  520,  491,  463,  437,  413,  390,  368,  347,  328,  309,  292,  276,
        260,  245,  232,  219,  206,  195,  184,  174,  164,  155,  146,  138,  130,  123,  116,
    },
    // Tuning -2 (index 14)
    {
        3472, 3280, 3096, 2920, 2756, 2604, 2456, 2320, 2188, 2064, 1948, 1840, 1736, 1640, 1548,
        1460, 1378, 1302, 1228, 1160, 1094, 1032, 974,  920,  868,  820,  774,  730,  689,  651,
        614,  580,  547,  516,  487,  460,  434,  410,  387,  365,  345,  325,  307,  290,  274,
        258,  244,  230,  217,  205,  193,  183,  172,  163,  154,  145,  137,  129,  122,  115,
    },
    // Tuning -1 (index 15)
    {
        3448, 3256, 3072, 2900, 2736, 2584, 2440, 2300, 2172, 2052, 1936, 1828, 1724, 1628, 1536,
        1450, 1368, 1292, 1220, 1150, 1086, 1026, 968,  914,  862,  814,  768,  725,  684,  646,
        610,  575,  543,  513,  484,  457,  431,  407,  384,  363,  342,  323,  305,  288,  272,
        256,  242,  228,  216,  203,  192,  181,  171,  161,  152,  144,  136,  128,  121,  114,
    },
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const u8 aon_vibrato_sine[32] = {
    0,   24,  49,  74,  97,  120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
    255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97,  74,  49,  24,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const u8 aon_vibrato_ramp_down[32] = {
    255, 248, 240, 232, 224, 216, 208, 200, 192, 184, 176, 168, 160, 152, 144, 136,
    128, 120, 112, 104, 96,  88,  80,  72,  64,  56,  48,  40,  32,  24,  16,  8,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const u8 aon_vibrato_square[32] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const i8 aon_nibble_tab[16] = {
    0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1,
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum {
    AonInstrumentType_Sample = 0,
    AonInstrumentType_Synth = 1,
} AonInstrumentType;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    AonInstrumentType type;
    char name[33];
    u8 volume;
    u8 fine_tune;
    u8 waveform;

    // ADSR envelope
    u8 envelope_start;
    u8 envelope_add;
    u8 envelope_end;
    u8 envelope_sub;

    // Sample-specific (type == Sample)
    u32 start_offset; // in words
    u32 length;       // in words
    u32 loop_start;   // in words
    u32 loop_length;  // in words

    // Synth-specific (type == Synth)
    u8 synth_length; // waveform length in words per wave-table entry
    u8 vib_param;
    u8 vib_delay;
    u8 vib_wave; // 0=sine, 1=triangle, 2=rectangle, 3=disable
    u8 wave_speed;
    u8 wave_length; // number of wave-table entries
    u8 wave_loop_start;
    u8 wave_loop_length;
    u8 wave_loop_control; // 0=normal, 1=backwards, 2=ping-pong
} AonInstrument;

typedef struct {
    u8 note;
    u8 instrument;
    u8 arpeggio;
    u8 effect;
    u8 effect_arg;
} AonTrackCell;

typedef struct {
    AonTrackCell cells[64][AON_MAX_CHANNELS];
} AonPattern;

typedef enum {
    AonEnvelopeState_Done = 0,
    AonEnvelopeState_Add = 1,
    AonEnvelopeState_Sub = 2,
} AonEnvelopeState;

// Effect enum values matching AoN format
enum {
    AON_FX_ARPEGGIO = 0,
    AON_FX_SLIDE_UP = 1,
    AON_FX_SLIDE_DOWN = 2,
    AON_FX_TONE_PORTAMENTO = 3,
    AON_FX_VIBRATO = 4,
    AON_FX_TONE_PORT_VOL = 5,
    AON_FX_VIB_VOL = 6,
    AON_FX_SAMPLE_OFFSET = 9,
    AON_FX_VOLUME_SLIDE = 10,       // A
    AON_FX_POSITION_JUMP = 11,      // B
    AON_FX_SET_VOLUME = 12,         // C
    AON_FX_PATTERN_BREAK = 13,      // D
    AON_FX_EXTRA = 14,              // E
    AON_FX_SET_SPEED = 15,          // F
    AON_FX_NEW_VOLUME = 16,         // G
    AON_FX_SYNTH_CONTROL = 17,      // H
    AON_FX_WAVE_SPEED = 18,         // I
    AON_FX_SET_ARP_SPEED = 19,      // J
    AON_FX_VOL_VIBRATO = 20,        // K
    AON_FX_FINE_SLIDE_PORT_UP = 21, // L
    AON_FX_FINE_SLIDE_PORT_DN = 22, // M
    AON_FX_AVOID_NOISE = 23,        // N
    AON_FX_OVERSIZE = 24,           // O
    AON_FX_FINE_VOL_VIB = 25,       // P
    AON_FX_SYNTH_DRUMS = 26,        // Q
    AON_FX_VOL_TONE_PORT = 27,      // R
    AON_FX_FINE_VOL_TONE_PORT = 28, // S
    AON_FX_SET_TRACK_VOL = 29,      // T
    AON_FX_WAVE_TABLE_MODE = 30,    // U
    AON_FX_EXTERNAL_EVENT = 33,     // X
};

// E sub-effect high nibble values
enum {
    AON_EFX_SET_FILTER = 0x00,
    AON_EFX_FINE_SLIDE_UP = 0x10,
    AON_EFX_FINE_SLIDE_DOWN = 0x20,
    AON_EFX_SET_VIB_WAVE = 0x40,
    AON_EFX_SET_LOOP = 0x50,
    AON_EFX_PATTERN_LOOP = 0x60,
    AON_EFX_RETRIG_NOTE = 0x90,
    AON_EFX_FINE_VOL_UP = 0xA0,
    AON_EFX_FINE_VOL_DOWN = 0xB0,
    AON_EFX_NOTE_CUT = 0xC0,
    AON_EFX_NOTE_DELAY = 0xD0,
    AON_EFX_PATTERN_DELAY = 0xE0,
};

typedef struct {
    u8 ch_flag; // 0=no change, 1=new repeat, 2=new wave, 3=new sample wave
    u8 last_note;

    // Waveform data
    i8* waveform;
    u32 waveform_offset;
    u16 wave_len; // in words
    u16 old_wave_len;
    i8* repeat_start;
    u32 repeat_offset;
    u16 repeat_length; // in words

    AonInstrument* instrument;
    i16 instrument_number;
    u8 volume; // 0-64

    u8 step_fx_cnt;
    u8 ch_mode; // 0=sample, 1=synth

    u16 period;
    i16 per_slide;

    // Arpeggio
    u16 arpeggio_off;
    u8 arpeggio_fine_tune;
    i16 arpeggio_tab[8]; // 7 notes + end mark (-1)
    u8 arpeggio_spd;
    u8 arpeggio_cnt;

    // Synth wave table
    i8* synth_wave_act;
    u32 synth_wave_act_offset;
    u32 synth_wave_end_offset;
    i8* synth_wave_rep;
    u32 synth_wave_rep_offset;
    u32 synth_wave_rep_end_offset;
    i32 synth_wave_add_bytes;
    u8 synth_wave_cnt;
    u8 synth_wave_spd;
    u8 synth_wave_rep_ctrl; // 0=normal, 1=back, 2=ping-pong
    u8 synth_wave_cont;
    u8 synth_wave_stop;
    u8 synth_add;
    u8 synth_sub;
    u8 synth_end;
    AonEnvelopeState synth_env;
    u8 synth_vol; // 0-127

    // Vibrato
    u8 vib_on; // 0x21 = off, 1 = on
    bool vib_done;
    u8 vib_cont;
    u8 vibrato_spd;
    u8 vibrato_ampl;
    u8 vibrato_pos;
    i16 vibrato_trig_delay;

    // Current effect
    u8 fx_com;
    u8 fx_dat;
    bool slide_flag;
    u32 old_sample_offset;
    u8 gliss_spd;
    u8 track_volume; // 0-64
} AonVoice;

typedef struct {
    u8 tempo; // 32-255 BPM
    u8 speed; // ticks per row
    u8 frame_cnt;
    bool pattern_break;
    u8 pat_cnt; // current row (0-63)
    i8 pat_delay_cnt;
    bool loop_flag;
    u8 loop_point;
    u8 loop_cnt;
    u8 position;
    u8 new_position;
    u8 current_pattern;
    bool noise_avoid;
    bool oversize;
} AonGlobalState;

// Per-channel mixer state for audio output
typedef struct {
    // Current waveform being played
    i8* sample_data;
    u32 sample_offset;   // byte offset into sample_data
    u32 sample_length;   // total length in bytes from offset
    u32 sample_buf_size; // total allocation size of sample_data buffer

    // Loop
    i8* loop_data;
    u32 loop_offset;
    u32 loop_length;   // in bytes
    u32 loop_buf_size; // total allocation size of loop_data buffer
    bool has_loop;
    bool playing;

    // Resampling state
    f64 phase;     // fractional position within sample
    f64 phase_inc; // samples per output sample

    // Volume and panning
    f32 volume; // 0.0 - 1.0
    f32 pan_left;
    f32 pan_right;
} AonMixerChannel;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main song structure

struct AonSong {
    AonSongMetadata metadata;

    // Module data
    u8 num_channels;
    u8 num_positions;
    u8 restart_position;
    u8* position_list;
    u32 position_list_size;

    AonPattern* patterns;
    u8 num_patterns;

    AonInstrument* instruments;
    u8 num_instruments;

    u8 arpeggios[AON_MAX_ARPEGGIOS][4];

    i8** waveforms;        // array of waveform data pointers
    u32* waveform_lengths; // lengths in bytes
    u8 num_waveforms;

    // Playback state
    AonGlobalState global;
    AonVoice voices[AON_MAX_CHANNELS];
    AonMixerChannel mixer[AON_MAX_CHANNELS];

    // Configuration
    u32 sample_rate;
    i32 solo_channel; // -1 = all
    f32 stereo_mix;   // 0.0 = full stereo, 1.0 = mono

    // Decode state
    f64 samples_per_tick;
    f64 tick_accumulator;
    bool started;
    bool finished;
    bool end_reached;
    int loop_count; // Number of times song has looped

    // Public playback state snapshot
    AonPlaybackState playback_state;

    // Scope visualization
    float scope_buffer[AON_MAX_CHANNELS][AON_SCOPE_BUFFER_SIZE];
    uint32_t scope_write_pos[AON_MAX_CHANNELS];
    int scope_enabled;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations

static void aon_tick(AonSong* song);
static void play_new_step(AonSong* song);
static void get_da_channel(AonSong* song, const AonTrackCell* cell, AonVoice* v);
static void play_fx(AonSong* song);
static void do_fx(AonSong* song, AonVoice* v);
static void do_synth(AonSong* song, AonVoice* v);
static void setup_channel(AonSong* song, AonVoice* v, AonMixerChannel* mix);
static void use_old_instrument(AonSong* song, const AonTrackCell* cell, AonVoice* v, AonInstrument* instr);
static void start_sample(AonSong* song, const AonTrackCell* cell, AonVoice* v, AonInstrument* instr);
static void start_repeat(AonSong* song, const AonTrackCell* cell, AonVoice* v, AonInstrument* instr);
static void init_adsr(AonVoice* v, AonInstrument* instr);
static void init_synth(AonSong* song, const AonTrackCell* cell, AonVoice* v, AonInstrument* instr);

// Effect handlers
static void do_fx_portamento_up(AonVoice* v, u8 arg);
static void do_fx_portamento_down(AonVoice* v, u8 arg);
static void do_fx_tone_slide(AonVoice* v);
static void do_fx_vibrato(AonVoice* v, u8 arg);
static void do_fx_vib_old_ampl(AonVoice* v);
static void do_fx_volume_slide(AonVoice* v, u8 arg);
static void do_fx_fine_vol_up(AonSong* song, AonVoice* v, u8 arg);
static void do_fx_fine_vol_down(AonSong* song, AonVoice* v, u8 arg);
static void do_fx_fine_vol_up_down(AonSong* song, AonVoice* v, u8 arg);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Big-endian read helpers

static u16 read_be16(const u8* p) {
    return (u16)((p[0] << 8) | p[1]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static u32 read_be32(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Look up total allocation size for a waveform pointer

static u32 aon_waveform_buf_size(const AonSong* song, const i8* wf) {
    for (int i = 0; i < song->num_waveforms; i++) {
        if (song->waveforms[i] == wf) {
            return song->waveform_lengths[i];
        }
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File loader: parse IFF chunks

static bool aon_load(AonSong* song, const u8* data, u32 size) {
    if (size < 54) {
        return false;
    }

    // Check magic
    if (memcmp(data, "AON4", 4) == 0) {
        song->num_channels = 4;
    } else if (memcmp(data, "AON8", 4) == 0) {
        song->num_channels = 8;
    } else {
        return false;
    }

    // Skip header (4 bytes magic + 42 bytes author string = 46 bytes)
    u32 pos = 46;

    bool has_info = false;
    bool has_arpg = false;
    bool has_plst = false;
    bool has_patt = false;
    bool has_inst = false;
    bool has_wlen = false;
    bool has_wave = false;

    while (pos + 8 <= size) {
        char chunk_name[5] = { 0 };
        memcpy(chunk_name, data + pos, 4);
        u32 chunk_size = read_be32(data + pos + 4);
        pos += 8;

        // Reject zero-size chunks (would cause infinite loop) and overflow
        if (chunk_size == 0 || chunk_size > size - pos) {
            fprintf(stderr, "AoN: chunk '%s' has invalid size %u\n", chunk_name, chunk_size);
            continue;// break; // rePlayer allow to play fm-demosong
        }

        const u8* chunk = data + pos;

        if (memcmp(chunk_name, "NAME", 4) == 0) {
            u32 len = chunk_size < 255 ? chunk_size : 255;
            memcpy(song->metadata.song_name, chunk, len);
            song->metadata.song_name[len] = '\0';
        } else if (memcmp(chunk_name, "AUTH", 4) == 0) {
            u32 len = chunk_size < 255 ? chunk_size : 255;
            memcpy(song->metadata.author, chunk, len);
            song->metadata.author[len] = '\0';
        } else if (memcmp(chunk_name, "RMRK", 4) == 0) {
            // Skip remarks for now
        } else if (memcmp(chunk_name, "INFO", 4) == 0) {
            if (chunk_size >= 3) {
                // byte 0 = version (skip), byte 1 = num_positions, byte 2 = restart_position
                song->num_positions = chunk[1];
                song->restart_position = chunk[2];
                if (song->restart_position >= song->num_positions) {
                    song->restart_position = 0;
                }
                has_info = true;
            }
        } else if (memcmp(chunk_name, "ARPG", 4) == 0) {
            // 16 arpeggios, 4 bytes each
            for (int i = 0; i < 16 && (u32)(i * 4 + 4) <= chunk_size; i++) {
                memcpy(song->arpeggios[i], chunk + i * 4, 4);
            }
            has_arpg = true;
        } else if (memcmp(chunk_name, "PLST", 4) == 0) {
            free(song->position_list); // Free any previous duplicate PLST chunk
            song->position_list = (u8*)malloc(chunk_size);
            if (!song->position_list) {
                return false;
            }
            memcpy(song->position_list, chunk, chunk_size);
            song->position_list_size = chunk_size;
            has_plst = true;
        } else if (memcmp(chunk_name, "PATT", 4) == 0) {
            u32 bytes_per_pattern = 4 * song->num_channels * 64;
            if (bytes_per_pattern == 0) {
                return false;
            }
            free(song->patterns); // Free any previous duplicate PATT chunk
            song->num_patterns = (u8)(chunk_size / bytes_per_pattern);
            if (song->num_patterns == 0) {
                song->patterns = nullptr;
                return false;
            }
            song->patterns = (AonPattern*)calloc(song->num_patterns, sizeof(AonPattern));
            if (!song->patterns) {
                return false;
            }

            const u8* p = chunk;
            for (int pat = 0; pat < song->num_patterns; pat++) {
                for (int row = 0; row < 64; row++) {
                    for (int ch = 0; ch < song->num_channels; ch++) {
                        u8 b1 = *p++;
                        u8 b2 = *p++;
                        u8 b3 = *p++;
                        u8 b4 = *p++;

                        AonTrackCell* cell = &song->patterns[pat].cells[row][ch];
                        cell->instrument = b2 & 0x3f;
                        cell->note = b1 & 0x3f;
                        cell->arpeggio = (u8)(((b3 & 0xc0) >> 4) | ((b2 & 0xc0) >> 6));
                        cell->effect = b3 & 0x3f;
                        cell->effect_arg = b4;
                    }
                }
            }
            has_patt = true;
        } else if (memcmp(chunk_name, "INST", 4) == 0) {
            free(song->instruments); // Free any previous duplicate INST chunk
            song->num_instruments = (u8)(chunk_size / 32);
            song->instruments = (AonInstrument*)calloc(song->num_instruments, sizeof(AonInstrument));
            if (!song->instruments) {
                return false;
            }

            const u8* p = chunk;
            for (int i = 0; i < song->num_instruments; i++) {
                AonInstrument* inst = &song->instruments[i];
                inst->type = (AonInstrumentType)p[0];
                inst->volume = p[1];
                inst->fine_tune = p[2];
                inst->waveform = p[3];

                if (inst->type == AonInstrumentType_Sample) {
                    inst->start_offset = read_be32(p + 4);
                    inst->length = read_be32(p + 8);
                    inst->loop_start = read_be32(p + 12);
                    inst->loop_length = read_be32(p + 16);
                    // skip 8 bytes (p+20..p+27)
                } else if (inst->type == AonInstrumentType_Synth) {
                    inst->synth_length = p[4];
                    // skip 5 bytes (p+5..p+9)
                    inst->vib_param = p[10];
                    inst->vib_delay = p[11];
                    inst->vib_wave = p[12];
                    inst->wave_speed = p[13];
                    inst->wave_length = p[14];
                    inst->wave_loop_start = p[15];
                    inst->wave_loop_length = p[16];
                    inst->wave_loop_control = p[17];
                    // skip 10 bytes (p+18..p+27)
                } else {
                    fprintf(stderr, "AoN: unknown instrument type %d at index %d\n", inst->type, i);
                    return false;
                }

                // Envelope bytes are at the end of the 32-byte block
                inst->envelope_start = p[28];
                inst->envelope_add = p[29];
                inst->envelope_end = p[30];
                inst->envelope_sub = p[31];

                p += 32;
            }
            has_inst = true;
        } else if (memcmp(chunk_name, "INAM", 4) == 0) {
            // Instrument names: 32 bytes per instrument
            if (song->instruments) {
                const u8* p = chunk;
                for (int i = 0; i < song->num_instruments && (u32)((i + 1) * 32) <= chunk_size; i++) {
                    u32 len = 32;
                    if (len > 32) {
                        len = 32;
                    }
                    memcpy(song->instruments[i].name, p + i * 32, len);
                    song->instruments[i].name[len] = '\0';
                }
            }
        } else if (memcmp(chunk_name, "WLEN", 4) == 0) {
            // Free any previous duplicate WLEN chunk
            if (song->waveforms) {
                for (int i = 0; i < song->num_waveforms; i++) {
                    free(song->waveforms[i]);
                }
                free(song->waveforms);
            }
            free(song->waveform_lengths);
            song->num_waveforms = (u8)(chunk_size / 4);
            song->waveform_lengths = (u32*)calloc(song->num_waveforms, sizeof(u32));
            song->waveforms = (i8**)calloc(song->num_waveforms, sizeof(i8*));
            if (!song->waveform_lengths || !song->waveforms) {
                return false;
            }

            for (int i = 0; i < song->num_waveforms; i++) {
                u32 length = read_be32(chunk + i * 4);
                // Reject waveform lengths that exceed the total file size
                if (length > size) {
                    fprintf(stderr, "AoN: waveform %d length %u exceeds file size\n", i, length);
                    return false;
                }
                song->waveform_lengths[i] = length;
                if (length > 0) {
                    song->waveforms[i] = (i8*)calloc(length, 1);
                    if (!song->waveforms[i]) {
                        return false;
                    }
                }
            }
            has_wlen = true;
        } else if (memcmp(chunk_name, "WAVE", 4) == 0) {
            if (song->waveforms && song->waveform_lengths) {
                const u8* p = chunk;
                u32 remaining = chunk_size;

                for (int i = 0; i < song->num_waveforms; i++) {
                    u32 len = song->waveform_lengths[i];
                    if (song->waveforms[i] && len > 0) {
                        if (len > remaining) {
                            len = remaining;
                        }
                        memcpy(song->waveforms[i], p, len);
                        p += len;
                        remaining -= len;
                    }
                }
                has_wave = true;
            }
        }

        pos += chunk_size;
    }

    if (!has_info || !has_arpg || !has_plst || !has_patt || !has_inst || !has_wlen || !has_wave) {
        fprintf(stderr, "AoN: missing required chunks (info=%d arpg=%d plst=%d patt=%d inst=%d wlen=%d wave=%d)\n",
                has_info, has_arpg, has_plst, has_patt, has_inst, has_wlen, has_wave);
        return false;
    }

    // Clamp num_positions to position_list size
    if (song->num_positions > song->position_list_size) {
        song->num_positions = (u8)song->position_list_size;
    }
    if (song->num_positions == 0) {
        return false;
    }
    if (song->restart_position >= song->num_positions) {
        song->restart_position = 0;
    }

    // Fill metadata
    song->metadata.num_positions = song->num_positions;
    song->metadata.num_patterns = song->num_patterns;
    song->metadata.num_instruments = song->num_instruments;
    song->metadata.num_channels = song->num_channels;
    song->metadata.num_waveforms = song->num_waveforms;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialize playback state

static void aon_init_playback(AonSong* song, u8 start_position) {
    memset(&song->global, 0, sizeof(song->global));
    song->global.tempo = 125;
    song->global.speed = 6;
    song->global.position = start_position;
    u8 pat = song->position_list[start_position];
    song->global.current_pattern = (pat < song->num_patterns) ? pat : 0;

    for (int i = 0; i < song->num_channels; i++) {
        AonVoice* v = &song->voices[i];
        memset(v, 0, sizeof(*v));
        v->vib_on = 0;
        v->track_volume = 64;
        v->arpeggio_tab[0] = 0;
        v->arpeggio_tab[1] = -1;
    }

    for (int i = 0; i < song->num_channels; i++) {
        memset(&song->mixer[i], 0, sizeof(AonMixerChannel));
    }

    song->end_reached = false;
    song->finished = false;
    song->loop_count = 0; // rePlayer

    // Recalculate ticks per second: standard Amiga BPM formula
    // ticks_per_second = tempo * 2 / 5
    f64 ticks_per_second = song->global.tempo * 2.0 / 5.0;
    song->samples_per_tick = song->sample_rate / ticks_per_second;
    song->tick_accumulator = 0.0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Panning tables (Amiga LRRL for 4ch, LLRRRRLL for 8ch)

static const f32 s_pan4_left[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
static const f32 s_pan4_right[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
static const f32 s_pan8_left[8] = { 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f };
static const f32 s_pan8_right[8] = { 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f };

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Effect implementations

static void do_fx_portamento_up(AonVoice* v, u8 arg) {
    v->per_slide -= arg;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void do_fx_portamento_down(AonVoice* v, u8 arg) {
    v->per_slide += arg;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void do_fx_tone_slide(AonVoice* v) {
    if (v->slide_flag && v->per_slide != 0) {
        if (v->per_slide < 0) {
            v->per_slide += v->gliss_spd;
            if (v->per_slide >= 0) {
                v->per_slide = 0;
            }
        } else {
            v->per_slide -= v->gliss_spd;
            if (v->per_slide < 0) {
                v->per_slide = 0;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void do_fx_vibrato(AonVoice* v, u8 arg) {
    if (arg != 0) {
        u8 spd = (arg & 0xf0) >> 4;
        if (spd != 0) {
            v->vibrato_spd = spd;
        }
        u8 ampl = arg & 0x0f;
        if (ampl != 0) {
            v->vibrato_ampl &= 0xf0;
            v->vibrato_ampl |= ampl;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void do_fx_vib_old_ampl(AonVoice* v) {
    if (!v->vib_done) {
        v->vib_done = true;

        const u8* table;
        u8 vib_ampl = v->vibrato_ampl & 0x60;

        if (vib_ampl == 0) {
            table = aon_vibrato_sine;
        } else if (vib_ampl == 32) {
            table = aon_vibrato_ramp_down;
        } else {
            table = aon_vibrato_square;
        }

        u8 vib_val = table[v->vibrato_pos];
        u16 add = (u16)(((v->vibrato_ampl & 0x0f) * vib_val) >> 7);

        if (v->vibrato_ampl & 0x80) {
            v->period -= add;
        } else {
            v->period += add;
        }

        v->vibrato_pos += v->vibrato_spd;
        if (v->vibrato_pos >= 32) {
            v->vibrato_pos &= 0x1f;
            v->vibrato_ampl ^= 0x80;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void do_fx_volume_slide(AonVoice* v, u8 arg) {
    u8 lo = arg & 0x0f;
    u8 hi = (arg & 0xf0) >> 4;
    int vol = v->volume;

    if (hi == 0) {
        vol -= lo;
        if (vol < 0) {
            vol = 0;
        }
    } else {
        vol += hi;
        if (vol > 64) {
            vol = 64;
        }
    }
    v->volume = (u8)vol;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void do_fx_fine_vol_up(AonSong* song, AonVoice* v, u8 arg) {
    if (song->global.frame_cnt == 0) {
        int vol = v->volume + arg;
        if (vol > 64) {
            vol = 64;
        }
        v->volume = (u8)vol;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void do_fx_fine_vol_down(AonSong* song, AonVoice* v, u8 arg) {
    if (song->global.frame_cnt == 0) {
        int vol = v->volume - arg;
        if (vol < 0) {
            vol = 0;
        }
        v->volume = (u8)vol;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void do_fx_fine_vol_up_down(AonSong* song, AonVoice* v, u8 arg) {
    u8 hi = (arg & 0xf0) >> 4;
    if (hi == 0) {
        do_fx_fine_vol_down(song, v, arg & 0x0f);
    } else {
        do_fx_fine_vol_up(song, v, hi);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ADSR and synth init

static void init_adsr(AonVoice* v, AonInstrument* instr) {
    if (v->fx_com != AON_FX_SYNTH_CONTROL || (v->fx_dat & 0x01) == 0) {
        v->synth_vol = instr->envelope_start;
        if (instr->envelope_add != 0) {
            v->synth_add = instr->envelope_add;
            v->synth_sub = instr->envelope_sub;
            v->synth_end = instr->envelope_end;
            v->synth_env = AonEnvelopeState_Add;
        } else {
            v->synth_vol = 127;
            v->synth_env = AonEnvelopeState_Done;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void start_repeat(AonSong* song, const AonTrackCell* cell, AonVoice* v, AonInstrument* instr) {
    (void)cell;

    if (cell->note != 0) {
        v->per_slide = 0;
    }

    if (instr->waveform >= song->num_waveforms) {
        return;
    }
    i8* waveform = song->waveforms[instr->waveform];

    v->old_wave_len = v->wave_len;
    v->wave_len = (u16)instr->length;

    // Loop setup
    if (instr->loop_length != 0) {
        if (song->global.oversize || instr->loop_start != 0) {
            v->repeat_start = waveform;
            v->repeat_offset = instr->loop_start * 2;
            v->repeat_length = (u16)instr->loop_length;
            if (!song->global.oversize) {
                v->wave_len = (u16)(instr->loop_length + instr->loop_start);
            }
        } else {
            v->repeat_start = waveform;
            v->repeat_offset = 0;
            v->repeat_length = (u16)instr->loop_length;
        }
    } else {
        v->repeat_start = nullptr;
        v->repeat_offset = 0;
        v->repeat_length = 0;
    }

    u32 offset = instr->start_offset * 2;
    v->wave_len -= (u16)(v->old_sample_offset / 2);

    if (v->fx_com == AON_FX_SAMPLE_OFFSET) {
        u32 new_offset = (u32)v->fx_dat * 256;
        i32 new_wave_len = (i32)v->wave_len - (i32)(new_offset / 2);

        if (new_wave_len < 0) {
            v->waveform = v->repeat_start;
            v->waveform_offset = v->repeat_offset;
            v->wave_len = (u16)(v->repeat_offset + v->repeat_length);
        } else {
            v->wave_len = (u16)new_wave_len;
            v->old_sample_offset += new_offset;
            v->waveform = waveform;
            v->waveform_offset = offset + v->old_sample_offset;
        }
    } else {
        v->waveform = waveform;
        v->waveform_offset = offset + v->old_sample_offset;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void start_sample(AonSong* song, const AonTrackCell* cell, AonVoice* v, AonInstrument* instr) {
    v->vib_on = 0x21;
    v->ch_mode = 0;

    if (v->fx_com == AON_FX_EXTRA) {
        u8 extra = v->fx_dat & 0xf0;
        u8 lo = v->fx_dat & 0x0f;

        if (extra == AON_EFX_NOTE_DELAY) {
            v->step_fx_cnt = lo;
        } else {
            if (extra == AON_EFX_RETRIG_NOTE) {
                v->step_fx_cnt = lo;
            }
            v->ch_flag = 3;
        }
    } else {
        v->ch_flag = 3;
    }

    start_repeat(song, cell, v, instr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void init_synth(AonSong* song, const AonTrackCell* cell, AonVoice* v, AonInstrument* instr) {
    v->ch_mode = 1;

    u8 effect = v->fx_com;
    u8 extra = v->fx_dat & 0xf0;

    if (effect == AON_FX_EXTRA && extra == AON_EFX_RETRIG_NOTE) {
        v->step_fx_cnt = v->fx_dat & 0x0f;
    }

    if (cell->note != 0) {
        v->per_slide = 0;

        u8 arg = (effect == AON_FX_SYNTH_CONTROL) ? v->fx_dat : 0;

        if ((arg & 0x10) == 0) {
            if (effect == AON_FX_EXTRA && extra == AON_EFX_NOTE_DELAY) {
                v->step_fx_cnt = v->fx_dat & 0x0f;
            } else {
                v->ch_flag = 3;
            }

            if (instr->waveform >= song->num_waveforms) {
                return;
            }
            i8* waveform = song->waveforms[instr->waveform];
            bool reset = true;

            if (v->waveform == waveform && v->waveform_offset == 0) {
                v->ch_flag = 0;
                reset = (v->synth_wave_cont == 0);
            }

            if (reset) {
                v->waveform = waveform;
                v->waveform_offset = 0;

                u16 wave_length = instr->synth_length;
                u32 offset = 0;

                if (effect == AON_FX_SAMPLE_OFFSET) {
                    offset = (u32)v->fx_dat * wave_length;
                    if (v->synth_wave_stop != 0) {
                        v->synth_wave_act = waveform;
                        v->synth_wave_act_offset = offset;
                        v->repeat_start = waveform;
                        v->repeat_offset = offset;
                    }
                }

                if (v->synth_wave_stop == 0) {
                    v->synth_wave_act = waveform;
                    v->synth_wave_act_offset = offset;
                    v->old_wave_len = v->wave_len;
                    v->wave_len = wave_length;
                    v->repeat_length = wave_length;

                    u32 wl2 = (u32)wave_length * 2;
                    v->synth_wave_add_bytes = (i32)wl2;

                    v->repeat_start = waveform;
                    v->repeat_offset = (u32)instr->wave_loop_start * wl2 + offset;

                    v->synth_wave_end_offset = (u32)instr->wave_length * wl2 + offset;

                    v->synth_wave_rep = waveform;
                    v->synth_wave_rep_offset = (u32)instr->wave_loop_start * wl2 + offset;
                    v->synth_wave_rep_end_offset
                        = (u32)(instr->wave_loop_length + instr->wave_loop_start) * wl2 + offset;

                    v->synth_wave_cnt = instr->wave_speed;
                    v->synth_wave_spd = instr->wave_speed;
                    v->synth_wave_rep_ctrl = instr->wave_loop_control;
                }
            }
        }

        // Initialize vibrato
        v->vib_on = 0;

        if (instr->vib_wave != 3) {
            v->vibrato_trig_delay = (i16)instr->vib_delay;

            if (instr->vib_param != 0) {
                do_fx_vibrato(v, instr->vib_param);
                v->vibrato_ampl &= 0b10011111;
                v->vibrato_ampl |= (u8)((instr->vib_wave >> 3) | (instr->vib_wave << 5));
                v->vib_cont = 1;
            } else {
                v->vibrato_trig_delay = -2;
            }
        } else {
            v->vib_on = 0x21;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void use_old_instrument(AonSong* song, const AonTrackCell* cell, AonVoice* v, AonInstrument* instr) {
    v->vib_cont = 0;
    init_adsr(v, instr);

    if (instr->type == AonInstrumentType_Sample) {
        start_sample(song, cell, v, instr);
    } else {
        init_synth(song, cell, v, instr);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parse a new step for a single channel

static void get_da_channel(AonSong* song, const AonTrackCell* cell, AonVoice* v) {
    v->fx_com = cell->effect;
    v->fx_dat = cell->effect_arg;

    if (v->fx_com == AON_FX_PATTERN_BREAK) {
        if (song->global.pat_cnt == 63) {
            song->global.pat_cnt = 62;
        }
    }

    u8 arg_hi = v->fx_dat & 0xf0;
    u8 arg_lo = v->fx_dat & 0x0f;

    if (v->fx_com == AON_FX_NEW_VOLUME) {
        v->step_fx_cnt = arg_lo;
    } else if (v->fx_com == AON_FX_EXTRA) {
        if (arg_hi == AON_EFX_NOTE_CUT) {
            v->step_fx_cnt = arg_lo;
        } else if (arg_hi == AON_EFX_PATTERN_DELAY) {
            if (song->global.pat_delay_cnt < 0) {
                song->global.pat_delay_cnt = (i8)arg_lo;
            }
        } else if (arg_hi == AON_EFX_PATTERN_LOOP) {
            if (song->global.loop_cnt != 0xf0 && arg_lo != 0) {
                if (song->global.loop_cnt == 0) {
                    song->global.loop_cnt = arg_lo;
                }
                song->global.loop_cnt--;
                if (song->global.loop_cnt == 0) {
                    song->global.loop_cnt = 0xf0;
                }
                song->global.loop_flag = true;
            }
        }
    }

    AonInstrument* instr;
    i16 instr_num = (i16)(cell->instrument - 1);

    if (instr_num < 0) {
        // Old instrument
        instr = v->instrument;
        if (!instr) {
            return;
        }

        if (cell->note != 0) {
            bool is_port = (v->fx_com == AON_FX_TONE_PORTAMENTO || v->fx_com == AON_FX_TONE_PORT_VOL
                            || v->fx_com == AON_FX_VOL_TONE_PORT || v->fx_com == AON_FX_FINE_VOL_TONE_PORT);
            if (!is_port) {
                use_old_instrument(song, cell, v, instr);
            }
        }
    } else {
        if (instr_num >= song->num_instruments) {
            return;
        }

        if (cell->note == 0) {
            // No note, only set repeat
            instr = &song->instruments[instr_num];

            if (instr->type == AonInstrumentType_Sample) {
                if (v->instrument != instr) {
                    v->instrument = instr;
                    v->instrument_number = instr_num;
                    v->ch_flag = 1;
                    start_repeat(song, cell, v, instr);
                }
            }
            v->volume = instr->volume;
        } else {
            v->old_sample_offset = 0;
            instr = &song->instruments[instr_num];

            bool is_port = (v->fx_com == AON_FX_TONE_PORTAMENTO || v->fx_com == AON_FX_TONE_PORT_VOL
                            || v->fx_com == AON_FX_VOL_TONE_PORT || v->fx_com == AON_FX_FINE_VOL_TONE_PORT);

            if (v->instrument != instr || !is_port) {
                v->instrument = instr;
                v->instrument_number = instr_num;
                use_old_instrument(song, cell, v, instr);
            }
            v->volume = instr->volume;
        }
    }

    u8 note = cell->note;
    if (note == 0) {
        note = v->last_note;
        if (note == 0 || note > 60) {
            return;
        }
    } else {
        v->slide_flag = false;
        v->last_note = note;
        if (note > 60) {
            return;
        }
    }

    v->arpeggio_fine_tune = instr->fine_tune & 0x0f;
    note--;

    // Tone portamento target
    bool is_port = (v->fx_com == AON_FX_VOL_TONE_PORT || v->fx_com == AON_FX_FINE_VOL_TONE_PORT
                    || v->fx_com == AON_FX_TONE_PORT_VOL || v->fx_com == AON_FX_TONE_PORTAMENTO);
    if (is_port) {
        if (cell->note != 0) {
            v->slide_flag = true;
            u16 period = aon_period_table[v->arpeggio_fine_tune][note];
            v->per_slide = (i16)(v->period + v->per_slide - period);
        }
    }

    // Arpeggio reset check
    if (v->arpeggio_tab[1] == -1) {
        v->arpeggio_off = 0;
        v->arpeggio_cnt = 0;
    }

    // Arpeggio setup
    if (v->fx_com == AON_FX_ARPEGGIO && v->fx_dat != 0) {
        // ProTracker arpeggio
        u8 arp1 = (v->fx_dat & 0xf0) >> 4;
        u8 arp2 = v->fx_dat & 0x0f;

        v->arpeggio_tab[0] = note;
        v->arpeggio_tab[1] = (i16)(note + arp1);
        v->arpeggio_tab[2] = (i16)(note + arp2);
        v->arpeggio_tab[3] = -1;
    } else {
        // Professional arpeggio from ARPG table
        const u8* arp = song->arpeggios[cell->arpeggio];
        int read_off = 0;
        int write_off = 0;

        u8 arp_byte = arp[read_off++];
        u8 count = arp_byte >> 4;

        if (count != 0) {
            v->arpeggio_tab[write_off++] = (i16)((arp_byte & 0x0f) + note);
            count--;

            while (count > 0) {
                arp_byte = arp[read_off];
                v->arpeggio_tab[write_off++] = (i16)(((arp_byte & 0xf0) >> 4) + note);
                count--;
                if (count == 0) {
                    break;
                }

                v->arpeggio_tab[write_off++] = (i16)((arp_byte & 0x0f) + note);
                count--;
                read_off++;
            }
        } else {
            v->arpeggio_off = 0;
            v->arpeggio_cnt = (u8)(v->arpeggio_spd > 0 ? v->arpeggio_spd - 1 : 0);
            v->arpeggio_tab[write_off++] = note;
        }

        v->arpeggio_tab[write_off] = -1;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Move to next row/position

static void play_new_step(AonSong* song) {
    AonGlobalState* g = &song->global;
    bool one_more_time;

    do {
        one_more_time = false;

        if (!g->pattern_break) {
            if (g->pat_delay_cnt > 0) {
                g->pat_delay_cnt--;
                return;
            }

            g->pat_delay_cnt = -1;

            if (g->current_pattern >= song->num_patterns) {
                g->current_pattern = 0;
            }
            AonPattern* pattern = &song->patterns[g->current_pattern];
            for (int i = 0; i < song->num_channels; i++) {
                get_da_channel(song, &pattern->cells[g->pat_cnt][i], &song->voices[i]);
            }

            if (g->loop_flag) {
                g->loop_flag = false;
                g->pat_cnt = g->loop_point;
                return;
            }

            g->pat_cnt++;
            if (g->pat_cnt < 64) {
                return;
            }
        } else {
            g->position = g->new_position;
        }

        g->pat_cnt = 0;
        g->pat_delay_cnt = 0;
        g->loop_point = 0;
        g->loop_cnt = 0;

        // rePlayer begin: allow summertime by Pink
//         g->position++;
//         if (g->position >= song->num_positions) {
//             g->position = song->restart_position;
//             song->end_reached = true;
//         }
        u16 position = g->position + 1; // position can be 255
        if (position >= song->num_positions) {
            g->position = song->restart_position;
            song->end_reached = true;
        }
        else {
            g->position = (u8)position;
        }
        // rePlayer end

        if (g->pattern_break) {
            g->pattern_break = false;
            u8 pat = song->position_list[g->position];
            g->current_pattern = (pat < song->num_patterns) ? pat : 0;
            one_more_time = true;
        }
    } while (one_more_time);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Do effects for a single voice

static void do_fx(AonSong* song, AonVoice* v) {
    AonGlobalState* g = &song->global;

    if (v->vib_cont == 0) {
        v->vib_on = 0x21;
    }

    // Arpeggio cycling - assembly lines 958-977 (player8ch.S):
    // Only advance arpeggio when counter >= speed (bgt.s aon_dofx_nonewarpval
    // skips the period update when speed > cnt)
    v->arpeggio_cnt++;
    if (v->arpeggio_cnt >= v->arpeggio_spd) {
        v->arpeggio_cnt = 0;

        // Read arpeggio value and set period - only when counter triggers
        i16 period_offset;
        do {
            period_offset = v->arpeggio_tab[v->arpeggio_off];
            if (period_offset < 0) {
                v->arpeggio_off = 0; // End mark: wrap to start (asm line 970-971)
            }
        } while (period_offset < 0);

        if (period_offset >= 60) {
            period_offset = 59;
        }
        v->period = aon_period_table[v->arpeggio_fine_tune][period_offset];
        v->arpeggio_off++;       // asm line 976: addq.b #2 (byte offset, equiv to index++)
        v->arpeggio_off &= 0x07; // asm line 977: and.b #$0f (masks to 0-7 entries)
    }

    if (v->fx_com != AON_FX_ARPEGGIO) {
        u8 arg = v->fx_dat;

        // Tick-based effects (non-zero frames only)
        if (g->frame_cnt != 0) {
            switch (v->fx_com) {
                case AON_FX_SLIDE_UP:
                    do_fx_portamento_up(v, arg);
                    break;
                case AON_FX_SLIDE_DOWN:
                    do_fx_portamento_down(v, arg);
                    break;
                case AON_FX_TONE_PORTAMENTO:
                    if (arg != 0) {
                        v->gliss_spd = arg;
                    }
                    do_fx_tone_slide(v);
                    break;
                case AON_FX_VIBRATO:
                    v->vib_on = 1;
                    do_fx_vibrato(v, arg);
                    break;
                case AON_FX_TONE_PORT_VOL:
                    do_fx_tone_slide(v);
                    do_fx_volume_slide(v, arg);
                    break;
                case AON_FX_VIB_VOL:
                    do_fx_vib_old_ampl(v);
                    do_fx_volume_slide(v, arg);
                    break;
                case AON_FX_VOLUME_SLIDE:
                    do_fx_volume_slide(v, arg);
                    break;
            }
        }

        // Effects that run every frame
        switch (v->fx_com) {
            case AON_FX_POSITION_JUMP: {
                g->new_position = (u8)(arg - 1);
                g->pat_cnt = 0;
                g->pattern_break = true;
                break;
            }
            case AON_FX_SET_VOLUME: {
                if (arg > 64) {
                    arg = 64;
                }
                v->volume = arg;
                break;
            }
            case AON_FX_PATTERN_BREAK: {
                // Convert hex to decimal (Amiga-style BCD)
                u8 lo = arg & 0x0f;
                u8 hi = arg & 0xf0;
                u8 temp = hi >> 1;
                u8 p = (u8)(temp + (temp >> 2));
                p += lo;
                if (p >= 64) {
                    p = 0;
                }
                g->pat_cnt = p;
                g->new_position = g->position;
                g->pattern_break = true;
                break;
            }
            case AON_FX_EXTRA: {
                // E sub-effects
                u8 lo = arg & 0x0f;
                u8 hi = arg & 0xf0;

                switch (hi) {
                    case AON_EFX_SET_FILTER:
                        // Filter control - ignore in software player
                        break;
                    case AON_EFX_FINE_SLIDE_UP:
                        if (g->frame_cnt == 0) {
                            v->per_slide -= lo;
                        }
                        break;
                    case AON_EFX_FINE_SLIDE_DOWN:
                        if (g->frame_cnt == 0) {
                            v->per_slide += lo;
                        }
                        break;
                    case AON_EFX_SET_VIB_WAVE: {
                        u8 w = lo & 0x03;
                        w = (u8)((w >> 3) | (w << 5));
                        v->vibrato_ampl &= 0b10011111;
                        v->vibrato_ampl |= w;
                        break;
                    }
                    case AON_EFX_SET_LOOP: {
                        u8 lp = (u8)(g->pat_cnt - 1);
                        if (lp != g->loop_point) {
                            g->loop_point = lp;
                            g->loop_cnt = 0;
                        }
                        break;
                    }
                    case AON_EFX_PATTERN_LOOP:
                        if (lo == 0) {
                            // Set loop point
                            u8 lp = (u8)(g->pat_cnt - 1);
                            if (lp != g->loop_point) {
                                g->loop_point = lp;
                                g->loop_cnt = 0;
                            }
                        }
                        break;
                    case AON_EFX_RETRIG_NOTE:
                    case AON_EFX_NOTE_DELAY:
                        if (v->step_fx_cnt == 0) {
                            v->ch_flag = 3;
                            v->fx_com = 0xef;
                        } else {
                            v->step_fx_cnt--;
                        }
                        break;
                    case AON_EFX_FINE_VOL_UP:
                        do_fx_fine_vol_up(song, v, lo);
                        break;
                    case AON_EFX_FINE_VOL_DOWN:
                        do_fx_fine_vol_down(song, v, lo);
                        break;
                    case AON_EFX_NOTE_CUT:
                        if (v->step_fx_cnt == 0) {
                            v->volume = 0;
                        } else {
                            v->step_fx_cnt--;
                        }
                        break;
                }
                break;
            }
            case AON_FX_SET_SPEED: {
                if (arg != 0) {
                    if (arg <= 32) {
                        g->speed = arg;
                    } else if (arg <= 200) {
                        g->tempo = arg;
                        f64 tps = g->tempo * 2.0 / 5.0;
                        song->samples_per_tick = song->sample_rate / tps;
                    }
                } else {
                    song->end_reached = true;
                }
                break;
            }
            case AON_FX_NEW_VOLUME: {
                if (v->step_fx_cnt == 0) {
                    v->volume = (u8)(((arg & 0xf0) >> 4) * 4 + 4);
                } else {
                    v->step_fx_cnt--;
                }
                break;
            }
            case AON_FX_WAVE_SPEED: {
                v->synth_wave_spd = (u8)((arg & 0xf0) >> 4);
                break;
            }
            case AON_FX_SET_ARP_SPEED: {
                u8 lo = arg & 0x0f;
                if (lo != 0) {
                    v->arpeggio_spd = lo;
                }
                break;
            }
            case AON_FX_VOL_VIBRATO: {
                do_fx_vib_old_ampl(v);
                u8 vol = arg;
                if (vol > 64) {
                    vol = 64;
                }
                v->volume = vol;
                break;
            }
            case AON_FX_FINE_SLIDE_PORT_UP: {
                u8 lo = arg & 0x0f;
                u8 hi = (arg & 0xf0) >> 4;
                i8 nib = aon_nibble_tab[hi];
                if (nib < 0) {
                    do_fx_fine_vol_down(song, v, (u8)(-nib));
                } else {
                    do_fx_fine_vol_up(song, v, (u8)nib);
                }
                if (g->frame_cnt != 0) {
                    do_fx_portamento_up(v, lo);
                }
                break;
            }
            case AON_FX_FINE_SLIDE_PORT_DN: {
                u8 lo = arg & 0x0f;
                u8 hi = (arg & 0xf0) >> 4;
                i8 nib = aon_nibble_tab[hi];
                if (nib < 0) {
                    do_fx_fine_vol_down(song, v, (u8)(-nib));
                } else {
                    do_fx_fine_vol_up(song, v, (u8)nib);
                }
                if (g->frame_cnt != 0) {
                    do_fx_portamento_down(v, lo);
                }
                break;
            }
            case AON_FX_AVOID_NOISE: {
                g->noise_avoid = (arg != 0);
                break;
            }
            case AON_FX_OVERSIZE: {
                g->oversize = (arg != 0);
                break;
            }
            case AON_FX_FINE_VOL_VIB: {
                do_fx_vib_old_ampl(v);
                do_fx_fine_vol_up_down(song, v, arg);
                break;
            }
            case AON_FX_SYNTH_DRUMS: {
                do_fx_portamento_down(v, (u8)((arg >> 4) * 8));
                do_fx_volume_slide(v, (u8)(arg & 0x0f));
                break;
            }
            case AON_FX_VOL_TONE_PORT: {
                do_fx_tone_slide(v);
                u8 vol = arg;
                if (vol > 64) {
                    vol = 64;
                }
                v->volume = vol;
                break;
            }
            case AON_FX_FINE_VOL_TONE_PORT: {
                do_fx_tone_slide(v);
                do_fx_fine_vol_up_down(song, v, arg);
                break;
            }
            case AON_FX_SET_TRACK_VOL: {
                if (arg > 64) {
                    arg = 64;
                }
                v->track_volume = arg;
                break;
            }
            case AON_FX_WAVE_TABLE_MODE: {
                v->synth_wave_cont = arg & 0x0f;
                v->synth_wave_stop = (arg & 0xf0) >> 4;
                break;
            }
            case AON_FX_EXTERNAL_EVENT: {
                // Sync event - nothing to do in software player
                break;
            }
        }
    }

    do_synth(song, v);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Synth engine - wave table cycling and ADSR

static void synth_reset_loop(AonVoice* v) {
    switch (v->synth_wave_rep_ctrl) {
        case 0: // Normal
            v->synth_wave_act = v->synth_wave_rep;
            v->synth_wave_act_offset = v->synth_wave_rep_offset;
            v->synth_wave_end_offset = v->synth_wave_rep_end_offset;
            break;
        case 1: {
            // Backwards - assembly line 894 (player8ch.S):
            // actptr = synthwaveRependptr (NOT endptr)
            v->synth_wave_act = v->synth_wave_rep;
            v->synth_wave_act_offset = v->synth_wave_rep_end_offset;
            i32 offset = v->synth_wave_add_bytes;
            if (offset >= 0) {
                offset = -offset;
            }
            if (v->synth_wave_stop != 0) {
                return;
            }
            v->synth_wave_act_offset = (u32)((i32)v->synth_wave_act_offset + offset);
            v->synth_wave_add_bytes = offset;
            break;
        }
        default: // Ping-pong
            v->synth_wave_act = v->synth_wave_rep;
            v->synth_wave_end_offset = v->synth_wave_rep_end_offset;
            v->synth_wave_act_offset = (u32)((i32)v->synth_wave_act_offset - v->synth_wave_add_bytes);
            v->synth_wave_add_bytes = -v->synth_wave_add_bytes;
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void do_synth(AonSong* song, AonVoice* v) {
    (void)song;
    v->vib_done = false;

    if (v->ch_flag == 0) {
        // Wave table cycling for synth mode
        if (v->ch_mode != 0 && v->waveform != nullptr) {
            if (v->synth_wave_stop == 0) {
                v->synth_wave_cnt++;
                if (v->synth_wave_cnt >= v->synth_wave_spd) {
                    v->synth_wave_cnt = 0;

                    i32 add_bytes = v->synth_wave_add_bytes;
                    v->synth_wave_act_offset = (u32)((i32)v->synth_wave_act_offset + add_bytes);

                    if (add_bytes < 0) {
                        if ((i32)v->synth_wave_act_offset < (i32)v->synth_wave_rep_offset) {
                            synth_reset_loop(v);
                        }
                    } else {
                        if ((i32)v->synth_wave_act_offset >= (i32)v->synth_wave_end_offset) {
                            synth_reset_loop(v);
                        }
                    }

                    v->ch_flag = 1;
                    v->repeat_start = v->synth_wave_act;
                    v->repeat_offset = v->synth_wave_act_offset;
                }
            }
        }

        // ADSR envelope
        if (v->waveform != nullptr) {
            int vol = v->synth_vol;
            switch (v->synth_env) {
                case AonEnvelopeState_Add:
                    vol += v->synth_add;
                    if (vol > 127) {
                        vol = 127;
                        v->synth_env = AonEnvelopeState_Sub;
                    }
                    break;
                case AonEnvelopeState_Sub:
                    vol -= v->synth_sub;
                    if (vol <= v->synth_end) {
                        vol = v->synth_end;
                        v->synth_env = AonEnvelopeState_Done;
                    }
                    break;
                case AonEnvelopeState_Done:
                    break;
            }
            v->synth_vol = (u8)vol;

            // Vibrato trigger delay
            if (v->vib_on != 0x21) {
                if (v->vibrato_trig_delay == -1) {
                    v->vib_on = 1;
                } else {
                    v->vibrato_trig_delay--;
                }
            }
        }
    }

    if (v->vib_on == 1) {
        do_fx_vib_old_ampl(v);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle all effects for all channels

static void play_fx(AonSong* song) {
    u8 pat = song->position_list[song->global.position];
    if (pat >= song->num_patterns) {
        pat = 0;
    }
    if (pat != song->global.current_pattern) {
        song->global.current_pattern = pat;
    }

    for (int i = 0; i < song->num_channels; i++) {
        do_fx(song, &song->voices[i]);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup mixer channel from voice state

static void setup_channel(AonSong* song, AonVoice* v, AonMixerChannel* mix) {
    // Skip note delay
    if (v->fx_com == AON_FX_EXTRA && (v->fx_dat & 0xf0) == AON_EFX_NOTE_DELAY) {
        return;
    }

    if ((v->ch_flag & 0x02) && v->waveform != nullptr) {
        // New waveform trigger
        u32 wf_total_len = aon_waveform_buf_size(song, v->waveform);

        u32 length = v->wave_len * 2;
        if (v->waveform_offset + length > wf_total_len) {
            length = (wf_total_len > v->waveform_offset) ? (wf_total_len - v->waveform_offset) : 0;
        }

        mix->sample_data = v->waveform;
        mix->sample_offset = v->waveform_offset;
        mix->sample_length = length;
        mix->sample_buf_size = wf_total_len;
        mix->phase = 0.0;
        mix->playing = true;

        // Set loop
        if (v->repeat_start != nullptr && v->repeat_length > 1) {
            u32 loop_len = v->repeat_length * 2;
            u32 loop_buf = aon_waveform_buf_size(song, v->repeat_start);
            mix->loop_data = v->repeat_start;
            mix->loop_offset = v->repeat_offset;
            mix->loop_length = loop_len;
            mix->loop_buf_size = loop_buf;
            mix->has_loop = true;
        } else {
            mix->has_loop = false;
        }
    } else if (v->ch_flag == 1) {
        // Update loop/repeat only
        if (v->repeat_start != nullptr && v->repeat_length > 1) {
            u32 loop_buf = aon_waveform_buf_size(song, v->repeat_start);
            mix->loop_data = v->repeat_start;
            mix->loop_offset = v->repeat_offset;
            mix->loop_length = v->repeat_length * 2;
            mix->loop_buf_size = loop_buf;
            mix->has_loop = true;
        }
    }

    // If the mixer stopped (sample ended with no loop) but the voice has valid
    // repeat data, restart into the loop. On real Amiga, Paula DMA never stops -
    // it always loops. The C# code handles this via SetSample()+SetLoop() in
    // its else branch, which replaces the sample buffer with repeat data.
    if (!mix->playing && v->repeat_start != nullptr && v->repeat_length > 1) {
        u32 rep_buf = aon_waveform_buf_size(song, v->repeat_start);
        mix->loop_data = v->repeat_start;
        mix->loop_offset = v->repeat_offset;
        mix->loop_length = v->repeat_length * 2;
        mix->loop_buf_size = rep_buf;
        mix->has_loop = true;
        mix->sample_data = v->repeat_start;
        mix->sample_offset = v->repeat_offset;
        mix->sample_length = v->repeat_length * 2;
        mix->sample_buf_size = rep_buf;
        mix->phase = 0.0;
        mix->playing = true;
    }

    // Period and volume
    i32 period = (i32)v->period + v->per_slide;
    if (period < 103) {
        period = 103;
    }

    f64 freq = AON_PAL_CLOCK / (f64)period;
    mix->phase_inc = freq / (f64)song->sample_rate;

    // 8ch player (player8ch.S line 2728-2736): volume * synthVOL >> 6, * trackvol >> 6
    // result 0-127 for the software mixer.
    // 4ch player (player8ch.S line 1557-1566): synthVOL >>= 1 first (lsr.b #1),
    // then same >>6 shifts. Result 0-64 written to Amiga hardware volume register.
    //
    // UADE output scale (audio.c sample_backend): raw Paula output (sample*vol per ch,
    // 2 channels summed per stereo side) is shifted <<1 to fill 16-bit range, then
    // /32768 to float. For one channel: float = sample*vol*2/32768 = sample*vol/16384.
    // We match this with: (sample/128) * (vol/128) = sample*vol/16384.
    u16 vol_raw;
    if (song->num_channels == 8) {
        vol_raw = (u16)(((v->volume * v->synth_vol) >> 6) * v->track_volume >> 6);
        if (vol_raw > 64) {
            vol_raw = 64; // assembly clamps to 64 (player8ch.S:2047)
        }
        mix->volume = vol_raw / 254.0f; // 8ch: >>7 table halves vs 4ch >>6
    } else {
        u8 sv = v->synth_vol >> 1; // lsr.b #1,d2
        vol_raw = (u16)(((v->volume * sv) >> 6) * v->track_volume >> 6);
        mix->volume = vol_raw / 128.0f; // match UADE: sample*vol/16384
    }

    v->ch_flag = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main tick function

static void aon_tick(AonSong* song) {
    AonGlobalState* g = &song->global;

    g->frame_cnt++;

    if (g->speed != 0) {
        if (g->frame_cnt >= g->speed) {
            g->frame_cnt = 0;
            play_new_step(song);
        }
    }

    play_fx(song);

    // Setup mixer channels with panning
    const f32* pan_l = (song->num_channels == 8) ? s_pan8_left : s_pan4_left;
    const f32* pan_r = (song->num_channels == 8) ? s_pan8_right : s_pan4_right;

    for (int i = 0; i < song->num_channels; i++) {
        setup_channel(song, &song->voices[i], &song->mixer[i]);

        // Apply panning with stereo mix
        f32 l = pan_l[i];
        f32 r = pan_r[i];

        // Blend toward center based on stereo_mix
        f32 center = 0.5f;
        l = l + (center - l) * song->stereo_mix;
        r = r + (center - r) * song->stereo_mix;

        song->mixer[i].pan_left = l;
        song->mixer[i].pan_right = r;
    }

    // Track song loops but don't stop playback - the assembly replayer loops
    // indefinitely (player8ch.S line 2690 just wraps position), and UADE does
    // the same. The caller controls duration via the decode loop.
    if (song->end_reached) {
        song->loop_count++;
        song->end_reached = false;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mix one output sample (stereo)

static void mix_one_frame(AonSong* song, f32* left, f32* right) {
    f32 l = 0.0f;
    f32 r = 0.0f;

    for (int ch = 0; ch < song->num_channels; ch++) {
        if (song->solo_channel >= 0 && ch != song->solo_channel) {
            continue;
        }

        AonMixerChannel* mix = &song->mixer[ch];
        if (!mix->playing || !mix->sample_data) {
            continue;
        }

        // Read sample at current phase position
        u32 pos = (u32)mix->phase;
        f32 sample = 0.0f;

        if (mix->has_loop && mix->loop_data) {
            // Check if we've passed the initial sample, switch to loop
            if (pos >= mix->sample_length) {
                // Switch to loop data
                u32 loop_pos = (pos - mix->sample_length);
                if (mix->loop_length > 0) {
                    loop_pos = loop_pos % mix->loop_length;
                }
                u32 byte_pos = mix->loop_offset + loop_pos;
                if (byte_pos < mix->loop_buf_size) {
                    sample = mix->loop_data[byte_pos] / 128.0f;
                } else {
                    mix->playing = false;
                    continue;
                }
            } else {
                u32 byte_pos = mix->sample_offset + pos;
                if (byte_pos < mix->sample_buf_size) {
                    sample = mix->sample_data[byte_pos] / 128.0f;
                } else {
                    mix->playing = false;
                    continue;
                }
            }
        } else {
            if (pos < mix->sample_length) {
                u32 byte_pos = mix->sample_offset + pos;
                if (byte_pos < mix->sample_buf_size) {
                    sample = mix->sample_data[byte_pos] / 128.0f;
                } else {
                    mix->playing = false;
                    continue;
                }
            } else {
                mix->playing = false;
                continue;
            }
        }

        sample *= mix->volume;

        if (song->scope_enabled) {
            song->scope_buffer[ch][song->scope_write_pos[ch]] = sample;
            song->scope_write_pos[ch] = (song->scope_write_pos[ch] + 1) & AON_SCOPE_BUFFER_MASK;
        }

        l += sample * mix->pan_left;
        r += sample * mix->pan_right;

        // Advance phase
        mix->phase += mix->phase_inc;

        // Handle loop wrapping
        if (mix->has_loop && mix->loop_data && mix->loop_length > 0) {
            if (mix->phase >= mix->sample_length + mix->loop_length) {
                mix->phase -= mix->loop_length;
                if (mix->phase < mix->sample_length) {
                    mix->phase = mix->sample_length;
                }
            }
        }
    }

    *left = l;
    *right = r;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Public API

AonSong* aon_song_create(const uint8_t* data, uint32_t size) {
    if (!data || size < 54) {
        return nullptr;
    }

    AonSong* song = (AonSong*)calloc(1, sizeof(AonSong));
    if (!song) {
        return nullptr;
    }

    song->sample_rate = 48000;
    song->solo_channel = -1;
    song->stereo_mix = 0.0f;

    if (!aon_load(song, data, size)) {
        aon_song_destroy(song);
        return nullptr;
    }

    return song;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void aon_song_destroy(AonSong* song) {
    if (!song) {
        return;
    }

    free(song->position_list);
    free(song->patterns);
    free(song->instruments);

    if (song->waveforms) {
        for (int i = 0; i < song->num_waveforms; i++) {
            free(song->waveforms[i]);
        }
        free(song->waveforms);
    }
    free(song->waveform_lengths);

    free(song);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void aon_song_set_subsong(AonSong* song, int subsong) {
    (void)song;
    (void)subsong;
    // AoN doesn't have subsongs
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void aon_song_set_sample_rate(AonSong* song, uint32_t rate) {
    if (song) {
        song->sample_rate = rate;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void aon_song_set_solo_channel(AonSong* song, int32_t channel) {
    if (song) {
        song->solo_channel = channel;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void aon_song_set_stereo_mix(AonSong* song, float mix) {
    if (song) {
        if (mix < 0.0f) {
            mix = 0.0f;
        }
        if (mix > 1.0f) {
            mix = 1.0f;
        }
        song->stereo_mix = mix;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void aon_song_start(AonSong* song) {
    if (!song) {
        return;
    }

    aon_init_playback(song, 0);
    song->started = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void update_playback_state(AonSong* song) {
    AonPlaybackState* ps = &song->playback_state;
    const AonGlobalState* g = &song->global;

    ps->position = g->position;
    ps->pattern = g->current_pattern;
    ps->row = g->pat_cnt > 0 ? g->pat_cnt - 1 : 0;
    ps->speed = g->speed;
    ps->tempo = g->tempo;
    ps->ticks_remaining = g->frame_cnt;

    for (int i = 0; i < song->num_channels; i++) {
        const AonVoice* v = &song->voices[i];
        ps->channels[i].instrument = (u8)(v->instrument_number > 0 ? v->instrument_number : 0);
        ps->channels[i].volume = v->volume;
        ps->channels[i].synth_volume = v->synth_vol;
        ps->channels[i].track_volume = v->track_volume;
        ps->channels[i].period = v->period;
        ps->channels[i].effect = v->fx_com;
        ps->channels[i].effect_arg = v->fx_dat;
        ps->channels[i].envelope_phase = (u8)v->synth_env;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int aon_song_decode(AonSong* song, float* buffer, int num_frames) {
    if (!song || !buffer || !song->started) {
        return -1;
    }

    for (int i = 0; i < num_frames; i++) {
        // rePlayer begin
        if (song->loop_count) {
            if (i == 0)
                song->loop_count = 0;
            num_frames = i;
            break;
        }
        // rePlayer end
        // Run ticks at the correct rate
        if (song->tick_accumulator <= 0.0) {
            aon_tick(song);
            song->tick_accumulator += song->samples_per_tick;
        }

        song->tick_accumulator -= 1.0;

        f32 left, right;
        mix_one_frame(song, &left, &right);

        buffer[i * 2] = left;
        buffer[i * 2 + 1] = right;
    }

    update_playback_state(song);

    return num_frames;
}

// rePlayer begin
int aon_song_decode_muted(AonSong* song, int num_frames) {
    if (!song || !song->started) {
        return -1;
    }

    for (int i = 0; i < num_frames; i++) {
        // Run ticks at the correct rate
        if (song->tick_accumulator <= 0.0) {
            aon_tick(song);
            song->tick_accumulator += song->samples_per_tick;
        }

        song->tick_accumulator -= 1.0;
    }

    update_playback_state(song);

    return num_frames;
}
// rePlayer end

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool aon_song_is_finished(const AonSong* song) {
    return song ? song->loop_count > 0 : true;
}

// rePlayer begin
int aon_song_get_length(AonSong* song) {
    int frames = 0;
    while (!song->loop_count) {
        // Run ticks at the correct rate
        if (song->tick_accumulator <= 0.0) {
            aon_tick(song);
            song->tick_accumulator += song->samples_per_tick;
        }

        song->tick_accumulator -= 1.0;
        frames++;
    }
    return frames;
}
// rePlayer end

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const AonSongMetadata* aon_song_get_metadata(const AonSong* song) {
    return song ? &song->metadata : nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool aon_song_get_pattern_cell(const AonSong* song, uint8_t pattern, uint8_t row, uint8_t channel,
                               AonPatternCell* cell) {
    if (!song || !cell || pattern >= song->num_patterns || row >= AON_ROWS_PER_PATTERN
        || channel >= song->num_channels) {
        return false;
    }

    const AonTrackCell* tc = &song->patterns[pattern].cells[row][channel];
    cell->note = tc->note;
    cell->instrument = tc->instrument;
    cell->arpeggio = tc->arpeggio;
    cell->effect = tc->effect;
    cell->effect_arg = tc->effect_arg;
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool aon_song_get_position_pattern(const AonSong* song, uint8_t position, uint8_t* pattern) {
    if (!song || !pattern || position >= song->num_positions) {
        return false;
    }

    *pattern = song->position_list[position];
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool aon_song_get_arpeggio(const AonSong* song, uint8_t index, uint8_t values[AON_ARPEGGIO_LENGTH]) {
    if (!song || !values || index >= AON_MAX_ARPEGGIOS) {
        return false;
    }

    memcpy(values, song->arpeggios[index], AON_ARPEGGIO_LENGTH);
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const AonPlaybackState* aon_song_get_playback_state(const AonSong* song) {
    return song ? &song->playback_state : nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* aon_song_get_instrument_name(const AonSong* song, uint8_t index) {
    if (!song || index >= song->num_instruments) {
        return nullptr;
    }
    return song->instruments[index].name;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void aon_song_enable_scope_capture(AonSong* song, int enable) {
    if (!song) {
        return;
    }
    song->scope_enabled = enable;
    if (enable) {
        memset(song->scope_buffer, 0, sizeof(song->scope_buffer));
        memset(song->scope_write_pos, 0, sizeof(song->scope_write_pos));
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t aon_song_get_scope_data(AonSong* song, int channel, float* buffer, uint32_t num_samples) {
    if (!song || !buffer || channel < 0 || channel >= song->num_channels) {
        return 0;
    }

    if (num_samples > AON_SCOPE_BUFFER_SIZE) {
        num_samples = AON_SCOPE_BUFFER_SIZE;
    }

    uint32_t write_pos = song->scope_write_pos[channel];
    uint32_t read_pos = (write_pos - num_samples + AON_SCOPE_BUFFER_SIZE) & AON_SCOPE_BUFFER_MASK;

    for (uint32_t i = 0; i < num_samples; i++) {
        buffer[i] = song->scope_buffer[channel][read_pos];
        read_pos = (read_pos + 1) & AON_SCOPE_BUFFER_MASK;
    }

    return num_samples;
}
