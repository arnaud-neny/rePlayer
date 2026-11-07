// SPDX-License-Identifier: MIT
// Maniacs of Noise player: the walk of the songs. A sequence is a list of words that the voice reads in order (a jump
// goes back into the same list, a stop ends it) and a pattern a list of commands with a fixed number of argument bytes
// that ends with the end command, so the walk of every voice of every song finds every command the replayer can run.
//
// The player refuses a module in which the replayer would do something no player can reproduce:
// - a command behind its command tables (it jumps into its variables, with registers nobody knows),
// - a sequence loop in which no pattern ends a tick (it hangs),
// - a pattern, sequence word or argument outside the image, or a sequence word at an odd address (it reads host memory
//   or takes an address error),
// - a first tick in which a voice plays a note or slide before any voice has loaded an instrument (it works with the
//   register the caller of the interrupt left).

#include "mon_internal.h"
#include <stdlib.h>
#include <string.h>

#define MAX_IMAGE_BYTES 0x200000 // the chip memory of the largest Amiga a module can be loaded to

// What the walk of one pattern found
typedef struct PatternInfo {
    bool valid;
    bool ends_tick; // a note, slide note, release, wait or rest: the voice's tick ends there
} PatternInfo;

typedef struct Walk {
    const MonModule* module;
    uint8_t* patterns;      // a bit per address: the pattern starting there has been walked
    uint8_t* pattern_ticks; // a bit per address: ... and ends a tick
    uint32_t* stamps;       // per sequence word: the sequence it was read in last
    uint32_t* tick_counts;  // per sequence word: tick-ending patterns the sequence had passed when it read it
    uint32_t stamp;
    size_t bit_bytes;
} Walk;

static bool test_bit(const uint8_t* bits, uint32_t address) {
    return (bits[address >> 3] & (1u << (address & 7))) != 0;
}

static void set_bit(uint8_t* bits, uint32_t address) {
    bits[address >> 3] |= (uint8_t)(1u << (address & 7));
}

static bool ends_tick(MonPatternCommand command) {
    return command == MonPatternCommand_Note || command == MonPatternCommand_SlideNote
           || command == MonPatternCommand_Release || command == MonPatternCommand_Wait
           || command == MonPatternCommand_Rest;
}

// A pattern from `start` to its end command; the arguments of the commands have to be inside the image too.

static PatternInfo walk_pattern(Walk* walk, uint32_t start) {
    const MonModule* m = walk->module;
    if (start >= m->size) {
        return (PatternInfo) { 0 };
    }
    if (test_bit(walk->patterns, start)) {
        return (PatternInfo) { .valid = true, .ends_tick = test_bit(walk->pattern_ticks, start) };
    }
    set_bit(walk->patterns, start);
    bool ticks = false;
    uint32_t at = start;
    while (at < m->size) {
        const MonPatternCommand command = mon_pattern_command(m->layout->version, mon_u8(m, at));
        if (command == MonPatternCommand_Unknown) {
            return (PatternInfo) { 0 };
        }
        if (command == MonPatternCommand_End) {
            if (ticks) {
                set_bit(walk->pattern_ticks, start);
            }
            return (PatternInfo) { .valid = true, .ends_tick = ticks };
        }
        ticks = ticks || ends_tick(command);
        const uint32_t next = at + 1 + mon_pattern_arguments(command);
        if (next > m->size) {
            return (PatternInfo) { 0 };
        }
        at = next;
    }
    return (PatternInfo) { 0 };
}

// A voice's sequence from its start; the patterns its words select. It ends at a stop or when a jump comes back to a
// word it has read, which must not be without a tick-ending pattern in between.

static bool walk_sequence(Walk* walk, uint32_t track) {
    const MonModule* m = walk->module;
    if (track >= m->size || (track & 1) != 0) {
        return false;
    }
    walk->stamp++;
    uint32_t ticks = 0;
    uint32_t at = track;
    for (;;) {
        if (at >= m->size) {
            return false;
        }
        const uint32_t index = at >> 1;
        if (walk->stamps[index] == walk->stamp) {
            return walk->tick_counts[index] != ticks;
        }
        walk->stamps[index] = walk->stamp;
        walk->tick_counts[index] = ticks;
        const uint16_t word = mon_u16(m, at);
        at += 2;
        if (mon_sequence_is_pattern(word)) {
            const PatternInfo info = walk_pattern(walk, mon_song_address(m, word));
            if (!info.valid) {
                return false;
            }
            ticks += info.ends_tick ? 1 : 0;
            continue;
        }
        switch (mon_sequence_command(word)) {
            case MonSequenceCommand_Unknown:
                return false;
            case MonSequenceCommand_Stop:
                return true;
            case MonSequenceCommand_Jump:
                at = track + (uint32_t)mon_sequence_parameter(word) * 2;
                break;
            case MonSequenceCommand_Transpose:
            case MonSequenceCommand_Repeat:
            case MonSequenceCommand_FadeIn:
            case MonSequenceCommand_FadeOut:
            case MonSequenceCommand_Tempo:
                break;
        }
    }
}

// The first tick of a voice of a song: its first pattern, up to the command that ends its tick. `loaded`: a voice of
// this tick has loaded an instrument. False when the voice plays a note or slide before that, or never gets to the end
// of its tick.

static bool first_tick_voice(const MonModule* m, uint32_t track, bool* loaded, bool* stopped) {
    uint32_t sequence = track;
    uint32_t pattern = 0;
    uint32_t pattern_start = 0;
    uint8_t repeat = 1;
    bool in_pattern = false;
    for (int guard = 0; guard < MON_COMMAND_GUARD * 16; guard++) {
        if (!in_pattern) {
            const uint16_t word = mon_u16(m, sequence);
            sequence += 2;
            if (mon_sequence_is_pattern(word)) {
                pattern_start = mon_song_address(m, word);
                pattern = pattern_start;
                in_pattern = true;
                continue;
            }
            switch (mon_sequence_command(word)) {
                case MonSequenceCommand_Repeat:
                    repeat = mon_sequence_parameter(word);
                    break;
                case MonSequenceCommand_Jump:
                    sequence = track + (uint32_t)mon_sequence_parameter(word) * 2;
                    break;
                case MonSequenceCommand_Stop:
                    *stopped = true;
                    return true;
                default:
                    break;
            }
            continue;
        }
        const MonPatternCommand command = mon_pattern_command(m->layout->version, mon_u8(m, pattern));
        pattern += 1 + mon_pattern_arguments(command);
        switch (command) {
            case MonPatternCommand_End:
                repeat--;
                if (repeat != 0) {
                    pattern = pattern_start;
                } else {
                    repeat = 1;
                    in_pattern = false;
                }
                break;
            case MonPatternCommand_Instrument:
                *loaded = true;
                break;
            case MonPatternCommand_Note:
            case MonPatternCommand_SlideNote:
                return *loaded;
            case MonPatternCommand_Slide:
                if (!*loaded) {
                    return false;
                }
                break;
            case MonPatternCommand_Release:
            case MonPatternCommand_Wait:
            case MonPatternCommand_Rest:
                return true;
            case MonPatternCommand_Volume:
            case MonPatternCommand_Arpeggio:
            case MonPatternCommand_Swing:
            case MonPatternCommand_Unknown:
                break;
        }
    }
    return false;
}

// The first tick of a song: the voices run from the last to the first, a sequence stop ends the interrupt.

static bool first_tick(const MonModule* m, const MonSong* song) {
    bool loaded = false;
    bool stopped = false;
    for (int voice = MON_VOICES - 1; voice >= 0 && !stopped; voice--) {
        if (song->tracks[voice] != 0
            && !first_tick_voice(m, mon_song_address(m, song->tracks[voice]), &loaded, &stopped)) {
            return false;
        }
    }
    return true;
}

bool mon_walk_songs(const MonModule* m) {
    if (m->size > MAX_IMAGE_BYTES) {
        return false;
    }
    Walk walk = { .module = m, .bit_bytes = (size_t)m->size / 8 + 1 };
    walk.patterns = calloc(walk.bit_bytes, 1);
    walk.pattern_ticks = calloc(walk.bit_bytes, 1);
    walk.stamps = calloc((size_t)m->size / 2 + 1, sizeof(uint32_t));
    walk.tick_counts = calloc((size_t)m->size / 2 + 1, sizeof(uint32_t));
    bool ok = walk.patterns != nullptr && walk.pattern_ticks != nullptr && walk.stamps != nullptr
              && walk.tick_counts != nullptr;
    for (uint32_t number = 0; ok && number < m->subsong_count; number++) {
        MonSong song;
        ok = mon_read_song(m, number, &song);
        for (int voice = 0; ok && voice < MON_VOICES; voice++) {
            if (song.tracks[voice] != 0) {
                ok = walk_sequence(&walk, mon_song_address(m, song.tracks[voice]));
            }
        }
        ok = ok && first_tick(m, &song);
    }
    free(walk.patterns);
    free(walk.pattern_ticks);
    free(walk.stamps);
    free(walk.tick_counts);
    return ok;
}
