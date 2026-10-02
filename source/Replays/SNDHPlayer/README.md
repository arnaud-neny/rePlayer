# AtariAudio 1.26

src/ contains all files needed to compile AtariAudio. It allows you to play ATARI .sndh and .ym music files. You can also directly use YM2149 emulator if you want to write your own YM tracker.
The library doesn't use any dependency, and should compile on any platform, including embedded systems (it doesn't even use float).

**NOTE 1: .ym replay has been completely rewritten and is now cycle accurate. The old [StSound](https://github.com/arnaud-carre/StSound) library is now deprecated**

**NOTE 2: Since 1.10, AtariAudio is thread safe! (see notes bellow)**

# Playing .sndh and .ym file in your own app

AtariAudio doesn't use any file IO. You should provide data from memory. Entry point is AtariAudioRenderer class.
Look at AtariAudioRenderer.h for API details but here is the absolute minimal:

````
static AtariAudioRenderer* Create(const void* fileMemoryData, uint32_t fileMemorySize, uint32_t hostReplayRate, uint32_t defaultYm2149Clock = kDefaultAtariYmClock);
````
Load a .sndh or .ym file from memory. You should provide the memory buffer, size of the raw file, and host replay rate in Hz. (ex. 44100 for 44.1kHz)

````
bool	InitSubSong(int subSongId);
````
Atari .sndh musics could contain several subsongs. You should *always* call InitSubsong before any audio rendering function. By convention, subsongs start at 1.

````
void	AudioRender(int16_t* buffer, uint32_t sampleCount);
````
This is the main audio rendering function. Render *count* samples into buffer. Buffer is a 16bits, signed, mono, sample buffer.

Musics don't have an end by default, so AudioRender doesn't return anything. If you want to generate the exact amount of samples, you can use GetSubsongDurationSample().
NOTE: some .sndh files don't provide any song duration information. In that case, GetSubsongDurationSample() will return 0 and it's up to you to render any duration

````
static void Destroy(AtariAudioRenderer* sr);
````
Destroy AtariAudioRenderer object and free any internal allocated memory


# Version history

- 1.26 : STE blitter support for some music drivers (ie BlitZwav demo by Cybernetics)
- 1.25 : optional ym2149 clock parameter for missing information in some music files ; added dc adjust for YMT and DigiMix (some YMT files aren't properly centered)
- 1.24 : added xbios(32) support for some .sndh files, fixed voice muting for digimix and ymtracker files
- 1.23 : Fix time duration with YM MIX1. Added fileFormat string in SongInfo
- 1.22 : Add YMT1 & YMT2 support. Now AtariAudio has full coverage of deprecated StSound library
- 1.21 : Add old YM2 support
- 1.20 : Major update: now supports both .sndh and .ym files, with a brand new rewritten cycle accurate .ym driver
- 1.10 : AtariAudio is now fully thread-safe! (Uses a custom Musashi 68k emulation version)
- 1.09 : API refactor and MuteVoices function added
- 1.08 : more robust API
- 1.07 : some API changes and cleanup
- 1.06 : added SetDefaultSongDuration for .sndh files without any duration info
- 1.05 : SndhFile::AudioRender API change (now returns sample count). Use timedb database for SNDH without music len
- 1.04 : added SndhFile::FastForward function
- 1.03 : added Ripper & Converter into SubSongInfo struct. some minor linux compilation fixes

# Examples

The repo also contains a AtariAudio2Wav project to show how to convert a .sndh or .ym file into a WAV audio file

# Applications using AtariAudio

[SndhArchivePlayer](https://github.com/arnaud-carre/sndh-player) - Player able to directly open a large 100MiB SNDH ZIP archive file and instantly play any of thousands Atari music files

[BZR Player 2](https://github.com/aargirakis/BZRPlayer) - Audio player for Windows and Linux supporting a wide array of multi-platform exotic file formats

[rePlayer](https://github.com/arnaud-neny/rePlayer) - "another multi-formats music player"

# About the threading model

AtariAudio is thread-safe, so any number of threads can create and use their own AtariAudioRenderer instances. (Obviously a single AtariAudioRenderer instance must not be used concurrently from multiple threads)

To make this possible, the Musashi 68k emulator used by AtariAudio has been modified to support thread-safe operation.

# About memory and object lifetime

AtariAudio is memory-efficient. When you call AtariAudioRenderer::Create(), only two allocations are performed:
- One allocation for the AtariAudioRenderer object itself (using standard new operator)
- One single allocation (using malloc) containing all the song data required for playback

Once AtariAudioRenderer::Create() returns, you are free to do whatever you want with the fileMemoryData buffer passed to it. AtariAudio keeps all the data required for playback in its single internal allocation, so the original buffer is no longer needed.

When you call AtariAudioRenderer::Destroy(), the AtariAudioRenderer instance is deleted and the single internal memory allocation is freed.

# Credits

- AtariAudio written by Arnaud Carré aka Leonard/Oxygene.
- MUSASHI 68000 emulation written by Karl Stenerud, thread safe version by Arnaud Carré
- Atari ICE depacker C version written by Hans Wessels
- timedb.inc.h database by Benjamin Gerard & SNDH Community
- ym2149 now uses a volume mixing table measured and generated by Paulo Simoes on real hardware (updated to a `32*32*32` table by Arnaud Carré)
