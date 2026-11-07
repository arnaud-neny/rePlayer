# Nuade

*Pronounced /ˈnuː.ɑː.di/ (NEW-ah-dee).*

Nuade is a port of the player routines used in [UADE](https://gitlab.com/uade-music-player/uade) (Unix Amiga
Delitracker Emulator) from 68000 assembly to C.

If you only need a player for the formats UADE supports, and nothing else, keep using UADE.

## Why port the UADE players to C?

- **Understanding the formats.** Many of the formats are undocumented. Readable C code makes it much easier to see
  how they work.
- **Pattern/note displays and extra metadata.** With the player state in plain C structs, a front end can show
  patterns, notes, instruments and other information about a song.
- **Editors.** Writing an editor on top of C code is far easier than doing it with assembly.
- **Better output quality.** For some synth-based formats, the C version can produce higher-quality output than the
  original.

## Status

Nuade is at a very early stage of development. Expect bugs.

Most of the work on Nuade, including translating the players and A/B testing them, has been done by AI agents.

UADE is used as the reference player, and each port is A/B tested against it. In some cases Nuade may diverge from
UADE where that makes sense.

[docs/players.md](docs/players.md) lists every UADE player and whether Nuade has ported it, has yet to port it, or
skips it.

## Using Nuade in your project

Add it as a subdirectory and link the `nuade::nuade` target. That builds the library with every player.

```cmake
add_subdirectory(nuade)
target_link_libraries(my_app PRIVATE nuade::nuade)
```

Set `NUADE_PLAYERS` to a list of player ids (for example `"sonix;soundmon"`) to build only those players.

## Example

Open a song, print some info about it, and render it to a file of raw 48 kHz interleaved float stereo:

```c
#include <nuade/nuade.h>
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <song> <out.raw>\n", argv[0]);
        return 1;
    }

    NuadeSongResult result = nuade_open(argv[1]);
    if (!result.song) {
        fprintf(stderr, "Unable to open %s: %s\n", argv[1], result.error.message);
        return 1;
    }

    NuadeSong* song = result.song;
    const NuadeSongInfo* info = nuade_info(song);
    const char* title = nuade_meta(song, NUADE_META_TITLE);

    printf("Player:   %s\n", nuade_player(song)->name);
    printf("Title:    %s\n", title ? title : "(none)");
    printf("Subsongs: %u\n", info->subsong_count);

    // Stop at the first loop point instead of playing forever
    nuade_set_repeat_count(song, 1);
    nuade_start(song, info->default_subsong, 48000);

    FILE* out = fopen(argv[2], "wb");
    if (!out) {
        nuade_close(song);
        return 1;
    }

    float buffer[1024 * 2];

    for (;;) {
        NuadeRenderResult r = nuade_render(song, buffer, NULL, 1024);
        fwrite(buffer, sizeof(float) * 2, r.frames, out);
        if (r.flags & NuadeRenderFlags_Ended) {
            break;
        }
    }

    fclose(out);
    nuade_close(song);
    return 0;
}
```

The full API is in `include/nuade/nuade.h`. Each player's own state (for custom displays) is in
`include/nuade/players/`.

## Building on its own

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## License

Nuade is MIT licensed, see [LICENSE](LICENSE). Most players are translations of the original Amiga replay routines
(the eagleplayers UADE ships, or the replayer code inside the modules); most of those state no license. The other
players are ports of [NostalgicPlayer](https://github.com/neumatho/NostalgicPlayer) players and keep its MIT license.
Some players include data copied from an eagleplayer. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
