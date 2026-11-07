// SPDX-License-Identifier: MIT
// Synth Pack: the song file (check, subsongs, positions) and the open-time proof over the positions subsongs reach.

#include "sp_internal.h"
#include <stdlib.h>
#include <string.h>

// The byte at SP_NOTES_OFFSET tells the layouts apart.
#define SP_LONGS_MARK 0x0100
// The song with the title "mainfx" of one game: its first position (rows 0x20, pattern 0, control stop) makes the
// eagleplayer count 7 subsongs.
#define SP_SPECIAL_ROWS 0x20
#define SP_SPECIAL_SUBSONGS 7

bool sp_song_layout(const uint8_t* data, size_t size, SpLayout* layout) {
    if (size < SP_NOTES_OFFSET + 4 || memcmp(data, SP_MAGIC, SP_MAGIC_SIZE) != 0) {
        return false;
    }
    const uint8_t* check = data + SP_NOTES_OFFSET;
    if (((check[0] << 8) | check[1]) == SP_LONGS_MARK) {
        *layout = SpLayout_Longs;
        return true;
    }
    if (check[0] == 0 && check[1] == 0 && check[2] == 0 && check[3] == 0) {
        *layout = SpLayout_Words;
        return true;
    }
    return false;
}

bool sp_set_has_magic(const SpMemory* set) {
    return set->size >= SP_MAGIC_SIZE && memcmp(set->data, SP_MAGIC, SP_MAGIC_SIZE) == 0;
}

SpPosition sp_position(const SpMemory* song, unsigned index) {
    const uint32_t at = SP_POSITIONS_OFFSET + 4u * index;
    return (SpPosition) { .rows = sp_u8(song, at),
                          .pattern = sp_u8(song, at + 1),
                          .control = sp_u8(song, at + 2),
                          .voices = sp_u8(song, at + 3) };
}

SpCell sp_cell(const SpMemory* song, unsigned pattern, unsigned column) {
    const uint32_t at = SP_PATTERN_LINES_OFFSET + SP_PATTERN_LINE_SIZE * pattern + 4u * column;
    return (SpCell) { .notes = sp_u8(song, at),
                      .instrument_shift = sp_s8(song, at + 1),
                      .note_shift = sp_s8(song, at + 2),
                      .tempo = sp_u8(song, at + 3) };
}

unsigned sp_event_instrument(uint8_t event_instrument, int8_t instrument_shift) {
    const int instrument = (uint8_t)(event_instrument - 1) + instrument_shift;
    return instrument < 0 ? 0 : (unsigned)instrument;
}

int sp_event_note(uint8_t note, int8_t instrument_note_offset, int8_t note_shift) {
    return (uint8_t)(note + instrument_note_offset) + note_shift;
}

bool sp_check_note_reach(int8_t note_shift, unsigned arpeggio_reach) {
    return note_shift >= 0 ? (unsigned)note_shift + arpeggio_reach < SP_PERIOD_COUNT : arpeggio_reach < SP_PERIOD_COUNT;
}

bool sp_check_spend(SpCheck* check, uint32_t amount) {
    if (amount > check->work_left) {
        check->work_left = 0;
        return false;
    }
    check->work_left -= amount;
    return true;
}

// The states of the walk: a position with the loop point it was reached with (one past the last position for none).

typedef struct SpWalk {
    uint16_t pending[SP_POSITION_COUNT * (SP_POSITION_COUNT + 1)];
    unsigned pending_count;
    bool visited[SP_POSITION_COUNT][SP_POSITION_COUNT + 1];
} SpWalk;

static unsigned sp_walk_loop_index(unsigned loop_point) {
    return loop_point < SP_POSITION_COUNT ? loop_point : SP_POSITION_COUNT;
}

static void sp_walk_push(SpWalk* walk, unsigned position, unsigned loop_point) {
    const unsigned loop = sp_walk_loop_index(loop_point);
    if (!walk->visited[position][loop]) {
        walk->visited[position][loop] = true;
        walk->pending[walk->pending_count++] = (uint16_t)(position * (SP_POSITION_COUNT + 1) + loop);
    }
}

// Walks the positions all subsongs reach, once each, calling `check` for each state. The successors of a position:
// the next one, or the loop point, or none when the control ends the song.

static bool sp_walk_positions(SpCheck* check, const SpRules* rules, const unsigned* starts, int start_count) {
    SpWalk* walk = calloc(1, sizeof(*walk));
    if (walk == nullptr) {
        return false;
    }
    for (int i = 0; i < start_count; i++) {
        sp_walk_push(walk, starts[i] % SP_POSITION_COUNT, rules->loop_point);
    }
    bool ok = true;
    while (ok && walk->pending_count != 0) {
        const unsigned state = walk->pending[--walk->pending_count];
        const unsigned position = state / (SP_POSITION_COUNT + 1);
        const unsigned loop = state % (SP_POSITION_COUNT + 1);
        const unsigned next = (position + 1) % SP_POSITION_COUNT;
        const SpPosition entry = sp_position(check->song, position);
        ok = sp_check_spend(check, 1)
             && sp_memory_has(check->song, SP_PATTERN_LINES_OFFSET + SP_PATTERN_LINE_SIZE * (uint32_t)entry.pattern,
                              SP_PATTERN_LINE_SIZE)
             && rules->check_position(check, &entry);
        if (!ok) {
            break;
        }
        if (entry.control < 0x80) {
            sp_walk_push(walk, next, loop);
        } else if (entry.control == SpControl_SetLoop) {
            sp_walk_push(walk, next, position);
        } else if (entry.control == SpControl_Loop) {
            sp_walk_push(walk, loop < SP_POSITION_COUNT ? loop : next, loop);
        }
    }
    free(walk);
    return ok;
}

bool sp_validate(const SpRules* rules, const SpMemory* song, const SpMemory* set, int subsong_count) {
    if (subsong_count > rules->max_subsongs) {
        return false;
    }
    SpCheck* check = calloc(1, sizeof(*check));
    if (check == nullptr) {
        return false;
    }
    *check = (SpCheck) { .song = song, .set = set, .work_left = SP_WORK_BUDGET };
    unsigned starts[SP_POSITION_COUNT + 1];
    for (int subsong = 0; subsong < subsong_count; subsong++) {
        starts[subsong] = rules->start_position(song, subsong);
    }
    const bool ok
        = sp_walk_positions(check, rules, starts, subsong_count) && (set == nullptr || rules->check_instruments(check));
    free(check);
    return ok;
}

// InitPlayer counts the songs two ways and takes the larger: the positions that start a subsong (control 0, voices
// 0xF0) and the positions that end one (a stop or a loop), over the leading run of non-empty positions.

static bool sp_position_empty(const SpPosition* position) {
    return position->rows == 0 && position->pattern == 0 && position->control == 0 && position->voices == 0;
}

int sp_count_subsongs(const SpMemory* song) {
    int starts = 0;
    for (unsigned i = 0; i < SP_POSITION_COUNT; i++) {
        const SpPosition position = sp_position(song, i);
        if (sp_position_empty(&position)) {
            break;
        }
        if (position.control == 0 && position.voices == SP_SUBSONG_MARK) {
            starts++;
        }
    }
    int ends = 0;
    unsigned walked = 0;
    for (unsigned i = 0; walked != SP_POSITION_COUNT; i++) {
        const SpPosition position = sp_position(song, i);
        if (sp_position_empty(&position)) {
            const SpPosition after = sp_position(song, i + 1);
            if (sp_position_empty(&after)) {
                break;
            }
        } else if (position.control == SpControl_Stop || position.control == SpControl_Loop) {
            ends++;
        }
        walked++;
    }
    int count = ends >= starts ? ends : starts;
    const SpPosition first = sp_position(song, 0);
    if (first.rows == SP_SPECIAL_ROWS && first.pattern == 0 && first.control == SpControl_Stop) {
        count = SP_SPECIAL_SUBSONGS;
    }
    return count > 0 ? count : 1;
}
