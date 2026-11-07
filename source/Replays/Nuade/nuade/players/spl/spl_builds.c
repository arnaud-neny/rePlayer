// SPDX-License-Identifier: MIT
// The builds of the replayer that the port knows, one for each game module, and what each init routine sets up.

#include "spl_module.h"

#include "nuade_utils.h"

#define ALL_VOICES { true, true, true, true }

static const SplSong s_boulder_dash[] = {
    { .streams = { 0xA26, 0xABE, 0xBBE, 0xC96 } },
};
static const SplSong s_clever_smart[] = {
    { .streams = { 0xD3A, 0xDD2, 0xEEA, 0xEFA } },
};
static const SplSong s_extensor[] = {
    { .streams = { 0x1072, 0x1122, 0x11DA, 0x12C2 } },
    { .streams = { 0x150E, 0x152E, 0x157E, 0x15D6 } },
    { .streams = { 0x176E, 0x1786, 0x179E, 0x17B6 } },
    { .streams = { 0x168E, 0x16A6, 0x16BE, 0x16D6 } },
};
static const SplSong s_graffiti_man[] = {
    { .streams = { 0x1380, 0x1440, 0x14F0, 0x1518 } },
    { .streams = { 0x15C0, 0x18E0, 0x1950, 0x1978 }, .voices = 2 },
    { .streams = { 0x1690, 0x18F0, 0x1950, 0x1978 }, .voices = 2 },
    { .streams = { 0x1748, 0x1900, 0x1950, 0x1978 }, .voices = 2 },
    { .streams = { 0x17F0, 0x1910, 0x1950, 0x1978 }, .voices = 2 },
    { .streams = { 0x1850, 0x1920, 0x1950, 0x1978 }, .voices = 2 },
    { .streams = { 0x18B8, 0x1930, 0x1960, 0x1988 } },
    { .streams = { 0x18CC, 0x1940, 0x196C, 0x1994 } },
};
static const SplSong s_happy_phone[] = {
    { .streams = { 0x8AC, 0x90C, 0x9A4, 0x9FC }, .tempo = 6, .sets_transpose = true, .transpose = 6 },
    { .streams = { 0x5E4, 0x5F4, 0x60C, 0x62C }, .tempo = 8, .sets_transpose = true, .transpose = 5 },
};
static const SplSong s_hollywood_poker[] = {
    { .streams = { 0x862, 0x676, 0x6DE, 0x71E } },
    { .streams = { 0x862, 0x872, 0x8BA, 0x912 } },
};
static const SplSong s_sanxion[] = {
    { .streams = { 0x6B8, 0x800, 0x8B0, 0x8C0 } },
};
static const SplSong s_space_port[] = {
    { .streams = { 0x7D4, 0x81C, 0x864, 0x8AC }, .tempo = 8, .sets_transpose = true, .transpose = 5 },
    { .streams = { 0xB64, 0xBC4, 0xC5C, 0xCB4 }, .tempo = 6, .sets_transpose = true, .transpose = 6 },
    { .streams = { 0xD4C, 0xD5C, 0xD6C, 0xD7C }, .tempo = 6, .sets_transpose = true, .transpose = 5 },
};
static const SplSong s_western_games[] = {
    { .streams = { 0x884, 0x8AC, 0x8BC, 0x894 } },
    { .streams = { 0x98C, 0x9BC, 0x9CC, 0x9A4 } },
    { .streams = { 0x9DC, 0xA0C, 0xA1C, 0x9F4 } },
};

const SplBuild spl_builds[] = {
    // Boulder Dash
    {
        .hash = 0xB65EA0229BF7FB11ULL,
        .load_base = 0x322B8,
        .prefix_end = 0x13A2,
        .family = SplFamily_Full,
        .divider = SplDivider_Swing,
        .divider_reload = 7, .sample_base = 0x1372,
        .period_table = 0x2F6,
        .transpose = 9,
        .stacks = { 0x12CC, 0x12F4, 0x131C, 0x1344 },
        .startup = {
            .dma_on = ALL_VOICES,
            .plays_sample = true,
            .sample = 0x13A2,
            .length_cell = 0x1378,
            .record_octave = 2,
        },
        .songs = s_boulder_dash,
        .song_count = nuade_sizeof_array(s_boulder_dash),
    },
    // Clever & Smart
    {
        .hash = 0x46069CCA41CF058CULL,
        .load_base = 0x32E78,
        .prefix_end = 0x2182,
        .family = SplFamily_Table,
        .divider = SplDivider_Bits,
        .pattern = 0x92492492,
        .period_table = 0x2F8,
        .transpose = 9,
        .stacks = { 0x1102, 0x112A, 0x1152, 0x117A },
        .size_table = 0x1182,
        .address_table = 0x11BE,
        .songs = s_clever_smart,
        .song_count = nuade_sizeof_array(s_clever_smart),
    },
    // Extensor
    {
        .hash = 0xE3FEF79090DB13E8ULL,
        .load_base = 0x3C0C8,
        .prefix_end = 0x1FD4,
        .family = SplFamily_Full,
        .divider = SplDivider_Phased,
        .sample_base = 0x1FA4,
        .period_table = 0x35A,
        .transpose = 8,
        .stacks = { 0x1F04, 0x1F2C, 0x1F54, 0x1F7C },
        .startup = {
            .dma_on = ALL_VOICES,
            .plays_sample = true,
            .sample = 0x1FD4,
            .length_cell = 0x1FAA,
            .record_octave = 2,
        },
        .songs = s_extensor,
        .song_count = nuade_sizeof_array(s_extensor),
    },
    // Graffiti Man
    {
        .hash = 0x93793D74A1BBDDE9ULL,
        .load_base = 0x31678,
        .prefix_end = 0x1AAC,
        .family = SplFamily_Table,
        .divider = SplDivider_Bits,
        .pattern = 0x92492492,
        .period_table = 0x2F2,
        .transpose = 7,
        .stacks = { 0x19A0, 0x19C8, 0x19F0, 0x1A18 },
        .size_table = 0x1A6C,
        .address_table = 0x1A40,
        .stop_f5 = true,
        .songs = s_graffiti_man,
        .song_count = nuade_sizeof_array(s_graffiti_man),
    },
    // Happy Phone
    {
        .hash = 0x78FA24278FAD12CCULL,
        .load_base = 0x31C38,
        .prefix_end = 0x10B2,
        .family = SplFamily_Lite,
        .divider = SplDivider_Tempo,
        .period_table = 0x2C0,
        .transpose = 4,
        .stacks = { 0xFE2, 0x100A, 0x1032, 0x105A },
        .startup = {
            .dma_on = ALL_VOICES,
            .plays_sample = true,
            .sample = 0x10B2,
            .length_cell = 0x1088,
            .record_octave = 2,
        },
        .songs = s_happy_phone,
        .song_count = nuade_sizeof_array(s_happy_phone),
    },
    // Hollywood Poker
    {
        .hash = 0xC9E367E4A313366BULL,
        .load_base = 0x314F8,
        .prefix_end = 0xD6E,
        .family = SplFamily_Tiny,
        .divider = SplDivider_Every,
        .period_table = 0x1F2,
        .transpose = 5,
        .stacks = { 0xCBE, 0xCC6, 0xCEE, 0xD16 },
        .startup = {
            .dma_on = ALL_VOICES,
            .plays_sample = true,
            .sample = 0xD6E,
            .length_cell = 0xD44,
            .record_octave = 2,
        },
        .songs = s_hollywood_poker,
        .song_count = nuade_sizeof_array(s_hollywood_poker),
    },
    // Sanxion
    {
        .hash = 0xD81271B94588BCBCULL,
        .load_base = 0x2FCA8,
        .prefix_end = 0x10B4,
        .family = SplFamily_Lite,
        .divider = SplDivider_Swing,
        .divider_reload = 5,
        .period_table = 0xC0A,
        .transpose = 4,
        .stacks = { 0x1016, 0x103E, 0x1066, 0x108E },
        .startup = {
            .dma_on = ALL_VOICES,
            .plays_sample = true,
            .sample = 0x10B4,
            .length_cell = 0x108A,
            .record_octave = 2,
        },
        .songs = s_sanxion,
        .song_count = nuade_sizeof_array(s_sanxion),
    },
    // Space Port
    {
        .hash = 0xB648DFD2EDD1B93DULL,
        .load_base = 0x4DA58,
        .prefix_end = 0x1DC8,
        .family = SplFamily_Lite,
        .divider = SplDivider_Tempo,
        .period_table = 0x258,
        .transpose = 5,
        .stacks = { 0x1508, 0x1530, 0x1558, 0x1580 },
        .startup = {
            .dma_on = ALL_VOICES,
            .plays_sample = true,
            .sample = 0x1DC8,
            .length_cell = 0x1D9E,
            .record_octave = 2,
        },
        .songs = s_space_port,
        .song_count = nuade_sizeof_array(s_space_port),
    },
    // Western Games
    {
        .hash = 0xF333B3B4265CAE0DULL,
        .load_base = 0x21CE8,
        .prefix_end = 0x116A,
        .family = SplFamily_Full,
        .divider = SplDivider_Swing,
        .divider_reload = 6, .sample_base = 0x113A,
        .period_table = 0x298,
        .transpose = 8,
        .stacks = { 0x109A, 0x10C2, 0x10EA, 0x1112 },
        .silent_middle_voices = true,
        .skip_fe = true,
        .startup = {
            .dma_on = { true, false, false, true },
            .record_sample = 0x116A,
            .record_octave = 2,
        },
        .songs = s_western_games,
        .song_count = nuade_sizeof_array(s_western_games),
    },
};

const uint32_t spl_build_count = nuade_sizeof_array(spl_builds);
