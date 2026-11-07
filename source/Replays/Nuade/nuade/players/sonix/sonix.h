// SPDX-License-Identifier: MIT

#pragma once

#include <nuade/io.h>
#include <nuade/players/sonix.h>
#include <nuade/voice.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The CIA timer clock (PAL) that tick lengths are counted in.
#define SONIX_CIA_PAL_HZ 709379u

SonixSong* sonix_song_create(const uint8_t* data, uint32_t size);
void sonix_song_destroy(SonixSong* song);

// (Re)start from the beginning on these voices (one per Sonix channel).
void sonix_song_start(SonixSong* song, NuadeVoices* voices);
// Run one tick of the song on the voices. Returns the length of the period that follows, in SONIX_CIA_PAL_HZ units.
// The first call only waits for the first period. *looped is set when the song passed its loop point.
uint32_t sonix_song_tick(SonixSong* song, bool* looped);
bool sonix_song_is_finished(const SonixSong* song);

const SonixSongMetadata* sonix_song_get_metadata(const SonixSong* song);
const char* sonix_format_name(SonixFormat format);
const char* sonix_song_get_error(const SonixSong* song);
// Describe the score entry at byte offset `pos` of a channel's track; false if `pos` is not inside the track.
bool sonix_song_describe_event(const SonixSong* song, int channel, uint32_t pos, NuadeEvent* out);
const char* sonix_song_get_instrument_name(const SonixSong* song, uint8_t instrument_index);

// Load the song's external instrument and sample files, searched in the directories around song_path (Instruments/
// next to the song and up to two levels above). Needs io->list_dir: names are matched case-insensitively. Returns the
// number loaded.
int sonix_song_load_instruments(SonixSong* song, const NuadeIo* io, const char* song_path);

#ifdef __cplusplus
}
#endif
