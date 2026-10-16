# PreTracker C Player

A C replayer for PreTracker modules — the synth-driven tracker format by Pink / Abyss.

This is a C port of the [Raspberry Casket](https://github.com/chrisly42/raspberry-casket) Amiga replayer by Chris 'platon42' Hodges, which itself reimplements the original PreTracker playback routine.

## Credits and status

- Original PreTracker format and player by **Pink / Abyss**.
- Amiga/68k replayer **Raspberry Casket** by **Chris 'platon42' Hodges**, which this port is based on.
- C port by **Daniel Collin**.
- Released with permission from **Pink / Abyss**. Note that this is not the official source — it is an independent re-implementation.
- Parts of the porting work were done with the help of AI tools.
- The code almost certainly still contains bugs. Pull requests and fixes are very welcome — feel free to contribute back.

Licensed under MIT (see `LICENSE`).

## Files

- `pretracker.h` — public API.
- `pretracker.c` — public API facade and metadata queries.
- `pretracker_parse.c` — module validation and parsing.
- `pretracker_wavegen.c` — oscillator and waveform synthesis.
- `pretracker_sequencer.c` — pattern, instrument, ADSR, and tick processing.
- `pretracker_mixer.c` — interpolation and stereo rendering.
- `pretracker_tables.c` — immutable playback coefficient data.
- `pretracker_internal.h` — internal structures (not part of the public API).

The repository root provides a CMake target named `pretracker`:

```sh
cmake -S . -B build
cmake --build build
```

For manual integration, compile every `pretracker/*.c` source as C11 and link
the math library on platforms that require it. The core has no other external
dependencies.

## API overview

All functions operate on an opaque `struct PreSong*` handle. The typical flow is:

1. Load a `.pt` module into memory.
2. Create a song from the buffer.
3. Configure sample rate / options.
4. Call `pre_song_start`.
5. Repeatedly call `pre_song_decode` to fill a float audio buffer.
6. Destroy the song when done.

### Lifecycle

```c
struct PreSong* pre_song_create(const uint8_t* data, uint32_t size);
void            pre_song_destroy(struct PreSong* song);
```

`pre_song_create` returns `NULL` if the data is not a valid PreTracker module. The song keeps its own copy of whatever state it needs, so the input buffer can be freed after creation.

### Configuration

Call these before `pre_song_start` (or between restarts):

```c
void pre_song_set_sample_rate(struct PreSong* song, uint32_t rate);
void pre_song_set_subsong(struct PreSong* song, int subsong);
void pre_song_set_solo_channel(struct PreSong* song, int32_t channel); // -1 = all
void pre_song_set_stereo_mix(struct PreSong* song, float mix);         // 0.0 = mono, 1.0 = full stereo
void pre_song_set_stereo_width(struct PreSong* song, float delay_ms);
void pre_song_set_interp_mode(struct PreSong* song, PreInterpMode mode);
```

`PreInterpMode` is either `PRE_INTERP_BLEP` (default — nearest-neighbor + BLEP, matches `PreTracker.exe`) or `PRE_INTERP_SINC` (windowed sinc, cleaner for HQ buffers).

### Playback

```c
void pre_song_start(struct PreSong* song);
int  pre_song_decode(struct PreSong* song, float* buffer, int num_frames);
int  pre_song_decode_with_scopes(struct PreSong* song, float* buffer, int num_frames,
                                 float** scopes, int num_scopes);
bool pre_song_is_finished(const struct PreSong* song);
```

`buffer` must hold `num_frames * 2` floats (interleaved stereo). The function returns the number of frames actually written.

`pre_song_decode_with_scopes` additionally writes per-channel mono output into `scopes[i]` (one float buffer per scope, `num_frames` floats each). Useful for drawing oscilloscope visualisations.

### Song info and track data

```c
const PreSongMetadata* pre_song_get_metadata(const struct PreSong* song);

bool pre_song_get_position_entry(const struct PreSong* song, uint16_t position, uint8_t channel,
                                 uint8_t* track_num, int8_t* pitch_shift);
bool pre_song_get_track_cell(const struct PreSong* song, uint8_t track, uint8_t row,
                             PreTrackCell* cell);

const PrePlaybackState* pre_song_get_playback_state(const struct PreSong* song);
```

`PreSongMetadata` contains the song name, author, counts, and wave/instrument name tables. `PrePlaybackState` is a live snapshot updated every tick during `pre_song_decode` — handy for UI, pattern displays, and VU meters.

See the struct definitions in `pretracker.h` for the exact fields.

## Minimal example

```c
#include "pretracker.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    FILE* f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);

    struct PreSong* song = pre_song_create(data, (uint32_t)size);
    free(data);
    if (!song) return 1;

    pre_song_set_sample_rate(song, 48000);
    pre_song_start(song);

    const int frames = 1024;
    float buffer[frames * 2];
    while (!pre_song_is_finished(song)) {
        int got = pre_song_decode(song, buffer, frames);
        // hand `buffer` (got*2 floats, interleaved stereo) to your audio output
        (void)got;
    }

    pre_song_destroy(song);
    return 0;
}
```

## Contributing

Bugs, mismatches against the original player, or missing effects — please open an issue or PR. Test modules that reproduce problems are especially appreciated.
