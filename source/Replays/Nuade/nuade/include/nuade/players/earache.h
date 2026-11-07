// SPDX-License-Identifier: MIT
// EarAche player (Morten Grouleff, 1990), translated from the UADE eagleplayer "EarAche" (Wanted Team). A song is one
// file: the "EASO" mark, a header of six offsets, then the rows, the position table, the instruments, the volume and
// pitch tables and the samples. The eagleplayer holds the replayer.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <nuade/nuade.h>
#include <nuade/paula.h>
#include <nuade/voice.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EA_VOICES 4

// The CIA clock: the timebase of EA_TICK_LENGTH.
#define EA_TIMEBASE_HZ 709379
// One player tick (DeliTracker's default 50 Hz CIA timer), in 1 / EA_TIMEBASE_HZ seconds.
#define EA_TICK_LENGTH 14187

typedef struct EaPlayer EaPlayer;

typedef struct EaTickResult {
    bool looped; // the position table jumped: the eagleplayer calls SongEnd
    bool ended;  // the position table cannot be followed (crafted data): later ticks do nothing
} EaTickResult;

typedef struct EaVoiceState {
    int period;     // the period table entry of the last note started, 0 if none
    int instrument; // the instrument number of that note, -1 if none
} EaVoiceState;

// The six offsets after the mark, all counted from the header's own start.
typedef struct EaHeader {
    uint32_t rows;        // 8 bytes per row: one word per voice, the first byte is the note, the second the instrument
    uint32_t positions;   // 4 bytes per entry: first row, last row + speed << 12
    uint32_t instruments; // 16 bytes each
    uint32_t samples;     // 4 bytes each: start in longwords from the header start, length in words
    uint32_t volumes;     // volume tables, 64 bytes each
    uint32_t pitches;     // pitch tables, 64 bytes each
} EaHeader;

typedef struct EaModule {
    const uint8_t* bytes; // the whole file
    size_t size;
    size_t base; // where the header starts in the file
    EaHeader header;
    uint32_t row_count;      // rows between the row table and the position table
    uint32_t position_count; // entries between the position table and the instruments
} EaModule;

typedef struct EaVoice {
    NuadePaulaChannel paula;
    uint16_t period;      // the period of the note, kept by the pitch slide; 0 while the voice is idle
    uint8_t note;         // index in the period table, the arpeggio adds to it
    uint8_t gate_ticks;   // ticks left until the voice stops (0 = no gate)
    uint8_t volume_ticks; // ticks until the next volume table byte (0 = table finished)
    uint8_t volume_reload;
    uint32_t volume_pos;     // next volume table byte
    uint32_t volume_restart; // where a restart byte goes back to
    bool arpeggio;
    uint8_t arpeggio_phase;      // 0 = no offset, 1..3 = arpeggio_offsets[phase - 1]
    uint8_t arpeggio_offsets[3]; // 0 ends the cycle at phase 2
    uint8_t slide_ticks;         // ticks until the next pitch table byte (0 = no slide)
    uint8_t slide_reload;
    uint32_t slide_pos;
    uint32_t slide_restart;
    int instrument; // for the display
} EaVoice;

struct EaPlayer {
    uint8_t* bytes; // the copy of the file the module points into, with the Paula tail (nuade_paula_padded_copy)
    EaModule module;
    uint16_t row;        // the row the last tick played
    uint16_t last_row;   // the last row of the current pattern
    uint32_t next_entry; // the position table entry the next pattern comes from
    uint8_t speed_ticks; // ticks until the next row
    uint8_t speed;
    bool ended;
    EaVoice voices[EA_VOICES];
};

// The eagleplayer's Check2 (the header at the start of the file, after "EASO", or behind the two branches and the
// DMACON write that a module with its own player starts with), plus the bounds this port needs: every table inside the
// file, every row and period table index the song can reach.
bool ea_probe(const uint8_t* data, size_t size);

// Copies and parses the song; null when `ea_probe` rejects it.
EaPlayer* ea_create(const uint8_t* data, size_t size);
void ea_destroy(EaPlayer* player);

// (Re)start the song from its first position; the player drives `voices` (Paula channel n = voice n).
bool ea_start(EaPlayer* player, NuadeVoices* voices);

// Run one player tick and hand the Paula registers it wrote to the voices.
EaTickResult ea_tick(EaPlayer* player, NuadeVoices* voices);

// Ticks per row of the first position entry (the speed changes per entry).
uint32_t ea_grid_ticks(const EaPlayer* player);

EaVoiceState ea_voice_state(const EaPlayer* player, int voice);

// The player's state behind a song it opened; 0 when the song is another player's.
EaPlayer* ea_from_nuade(NuadeSong* song);

#ifdef __cplusplus
}
#endif
