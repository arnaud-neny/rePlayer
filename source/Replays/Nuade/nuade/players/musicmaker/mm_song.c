// SPDX-License-Identifier: MIT
// MusicMaker V8 player: the song. The replayers read it in place; the position lists and the pattern table are located
// once when the song is opened.
//
// Header ("SE" at 0, the title at 2), by variant:
//   byte 0x16     0xFF = 8 voice song
//   4 voices      0x16 gap marker (0xFE), 0x17 rows of a pause, 0x18 tempo, 0x1A start, 0x1C loop, 0x1E gap size and
//                 the gap (the position lists follow it)
//   8 voices      0x17 the tracks the song has (bit n = track n), 0x18 the channel pairs the 68020 mixer routines
//                 mix (bit n = pair n), 0x19 the largest period the 68000 routines remap, 0x1A gap marker (0xFE),
//                 0x1C gap size, 0x1E the gap, then a table of 0xE2 bytes (the channel periods of the 68000
//                 routines), the pause (word), tempo, start and loop, and the position lists of the tracks

#include "mm_internal.h"
#include <stdlib.h>
#include <string.h>

#define SONG_SIGNATURE_0 'S'
#define SONG_SIGNATURE_1 'E'
#define SONG_HEADER_SIZE 0x1E
#define SONG_TITLE_OFFSET 2
#define SONG_TITLE_LENGTH 20
#define SONG_VARIANT_OFFSET 0x16
#define SONG_VARIANT_8V 0xFF
#define SONG_GAP_MARKER 0xFE
#define SONG_LISTS_OFFSET 0x1E // the position lists, or the gap in front of them

#define SONG_4V_GAP_MARKER_OFFSET 0x16
#define SONG_4V_PAUSE_OFFSET 0x17
#define SONG_4V_PAUSE_MIN 0x10
#define SONG_4V_PAUSE_MAX 0x40
#define SONG_4V_TEMPO_OFFSET 0x18
#define SONG_4V_START_OFFSET 0x1A
#define SONG_4V_LOOP_OFFSET 0x1C
#define SONG_4V_GAP_SIZE_OFFSET 0x1E

#define SONG_8V_TRACKS_OFFSET 0x17
#define SONG_8V_MIXER_OFFSET 0x18
#define SONG_8V_LIMIT_OFFSET 0x19
#define SONG_8V_GAP_MARKER_OFFSET 0x1A
#define SONG_8V_GAP_SIZE_OFFSET 0x1C
#define SONG_8V_TABLE_SIZE 0xE2

#define END_OF_PATTERNS 0xFE
#define END_OF_PATTERN 0xFF

bool mm_song_identify(const MmBlock* song, MmVariant* variant) {
    if (song->size < SONG_HEADER_SIZE || mm_u8(song, 0) != SONG_SIGNATURE_0 || mm_u8(song, 1) != SONG_SIGNATURE_1) {
        return false;
    }
    if (mm_u8(song, SONG_VARIANT_OFFSET) == SONG_VARIANT_8V) {
        *variant = MmVariant_8V;
        return true;
    }
    *variant = MmVariant_4V;
    const uint8_t pause = mm_u8(song, SONG_4V_PAUSE_OFFSET);
    return pause >= SONG_4V_PAUSE_MIN && pause <= SONG_4V_PAUSE_MAX;
}

static void read_title(MmSong* s) {
    for (int i = 0; i < SONG_TITLE_LENGTH; i++) {
        const uint8_t c = mm_u8(&s->block, SONG_TITLE_OFFSET + (uint32_t)i);
        s->title[i] = c >= ' ' && c <= '~' ? (char)c : ' ';
    }
    s->title[SONG_TITLE_LENGTH] = '\0';
}

// A position list runs to the word 0x03E7; returns the offset after it, 0 if the song ends first.

static uint32_t skip_list(const MmBlock* b, uint32_t offset) {
    while (offset + 2 <= b->size) {
        const uint16_t entry = mm_u16(b, offset);
        offset += 2;
        if (entry == MM_END_OF_LIST) {
            return offset;
        }
    }
    return 0;
}

// The position lists of the tracks the song has, one after the other from `at`; returns the offset behind them, 0 if
// the song ends first.

static uint32_t locate_lists(MmSong* s, uint32_t at) {
    for (int t = 0; t < MM_MAX_TRACKS; t++) {
        if (!s->has_track[t]) {
            continue;
        }
        s->track_list[t] = at;
        at = skip_list(&s->block, at);
        if (at == 0) {
            return 0;
        }
    }
    return at;
}

// The patterns follow the lists: events of 3 bytes up to a first byte 0xFF, 0xFE after the last pattern. A pattern that
// starts with 0xFF is empty and reads as the first pattern, like the entries after the last one.

static void index_patterns(MmSong* s, uint32_t start) {
    const MmBlock* b = &s->block;
    uint32_t at = start;
    uint32_t count = 0;
    s->pattern_fallback = start;
    while (count < MM_MAX_PATTERNS && at < b->size) {
        const uint8_t first = mm_u8(b, at);
        if (first == END_OF_PATTERNS) {
            break;
        }
        if (first == END_OF_PATTERN) {
            s->pattern[count++] = start;
            at++;
            continue;
        }
        s->pattern[count++] = at;
        do {
            at += MM_EVENT_SIZE;
        } while (at < b->size && mm_u8(b, at) != END_OF_PATTERN);
        at++;
    }
    while (count < MM_MAX_PATTERNS) {
        s->pattern[count++] = start;
    }
}

static bool open_4v(MmSong* s) {
    const MmBlock* b = &s->block;
    const uint32_t gap
        = mm_u8(b, SONG_4V_GAP_MARKER_OFFSET) == SONG_GAP_MARKER ? mm_u16(b, SONG_4V_GAP_SIZE_OFFSET) : 0;
    s->pause_rows = mm_u8(b, SONG_4V_PAUSE_OFFSET);
    s->tempo = mm_u16(b, SONG_4V_TEMPO_OFFSET);
    s->start_index = mm_u16(b, SONG_4V_START_OFFSET);
    s->loop_index = mm_u16(b, SONG_4V_LOOP_OFFSET);
    if (s->tempo < MM_TEMPO_MIN_4V || s->tempo > MM_TEMPO_MAX_4V) {
        return false;
    }
    for (int t = 0; t < 4; t++) {
        s->has_track[t] = true;
    }
    const uint32_t end = locate_lists(s, SONG_LISTS_OFFSET + gap);
    if (end == 0) {
        return false;
    }
    index_patterns(s, end);
    return true;
}

static bool open_8v(MmSong* s) {
    const MmBlock* b = &s->block;
    const uint32_t gap
        = mm_u8(b, SONG_8V_GAP_MARKER_OFFSET) == SONG_GAP_MARKER ? mm_u16(b, SONG_8V_GAP_SIZE_OFFSET) : 0;
    const uint32_t table = SONG_LISTS_OFFSET + gap;
    const uint32_t base = table + SONG_8V_TABLE_SIZE;
    const uint8_t tracks = mm_u8(b, SONG_8V_TRACKS_OFFSET);
    const uint8_t mixer = mm_u8(b, SONG_8V_MIXER_OFFSET);
    for (int t = 0; t < MM_MAX_TRACKS; t++) {
        s->has_track[t] = (tracks >> t & 1) != 0;
    }
    for (int p = 0; p < MM_PAIRS; p++) {
        s->pair_uses_020[p] = (mixer >> p & 1) != 0;
    }
    s->slow_limit = mm_u8(b, SONG_8V_LIMIT_OFFSET);
    s->slow_table = (int32_t)table - SONG_8V_TABLE_SIZE;
    s->pause_rows = mm_u16(b, base);
    s->tempo = mm_u16(b, base + 2);
    s->start_index = mm_u16(b, base + 4);
    s->loop_index = mm_u16(b, base + 6);
    if (s->tempo < MM_TEMPO_MIN_8V || s->tempo > MM_TEMPO_MAX_8V) {
        return false;
    }
    const uint32_t end = locate_lists(s, base + 8);
    if (end == 0) {
        return false;
    }
    index_patterns(s, end);
    return true;
}

bool mm_song_open(MmSong* s, const MmBlock* data, MmVariant variant) {
    *s = (MmSong) { 0 };
    s->storage = malloc(data->size != 0 ? data->size : 1);
    if (s->storage == nullptr) {
        return false;
    }
    memcpy(s->storage, data->data, data->size);
    s->block = (MmBlock) { .data = s->storage, .size = data->size };
    read_title(s);
    if (!(variant == MmVariant_4V ? open_4v(s) : open_8v(s))) {
        mm_song_close(s);
        return false;
    }
    return true;
}

void mm_song_close(MmSong* s) {
    free(s->storage);
    *s = (MmSong) { 0 };
}

// The table is indexed by the period and starts 0xE2 bytes before the data after the header, so its first entry is the
// one of the period 113 (the lowest). A read outside the song gives the period itself, a table period below the lowest
// the replayer's buffers are sized for is raised to it.

uint16_t mm_song_slow_period(const MmSong* s, uint16_t period) {
    if (period > s->slow_limit) {
        return period;
    }
    const int32_t at = s->slow_table + 2 * (int32_t)period;
    if (at < 0 || (uint32_t)at > s->block.size || s->block.size - (uint32_t)at < 2) {
        return period;
    }
    const uint16_t mapped = mm_u16(&s->block, (uint32_t)at);
    return mapped < MM_PERIOD_MIN ? MM_PERIOD_MIN : mapped;
}

// The pattern a position list entry names (an index past the table reads the first pattern).

uint32_t mm_song_pattern(const MmSong* s, uint16_t number) {
    return number < MM_MAX_PATTERNS ? s->pattern[number] : s->pattern_fallback;
}
