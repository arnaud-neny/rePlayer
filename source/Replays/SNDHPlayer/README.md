# AtariAudio Library v1.10

src/ contains all files needed to compile AtariAudio library. It allows you to play ATARI SNDH music files. You can also directly use YM2149 emulator if you want to write your own YM tracker.
The libray doesn't use any dependency, and should compile on any platform, including embeded systems (it doesn't even use float )

NOTE: Since 1.10 AtariAudio library is thread safe! Any thread could create any amount of SndhRenderer instances. (obviously two different threads can't use the same instance of SndhRenderer)

# Playing SNDH file in your own app

AtariAudio library doesn't use any file IO. You should provide data from memory. Entry point is SndhRenderer class.
Look at SndhRenderer.h for API details but here is the absolute minimal:

````
static SndhRenderer* Create(const void* sndhMemoryData, uint32_t sndhMemorySize, uint32_t hostReplayRate);

````
Load a SNDH file from memory. You should provide the memory buffer, size of the raw file, and host replay rate. ( ex 44100 for 44.1Khz )

````
bool	InitSubSong(int subSongId);
````
Atari SNDH musics could contain several subsongs. You should *always* call InitSubsong before any audio rendering function. By convention, subsongs starts at 1.

````
void	AudioRender(int16_t* buffer, uint32_t sampleCount);
````
This is the main audio rendering function. Render "count" samples into buffer. Buffer is a 16bits, signed, mono, sample buffer.

Musics doesn't have an end by default, so AudioRender doesn't returns anything. If you want to generate the exact amount of samples, you can use GetSubsongDurationSample()
NOTE: some SNDH files doesn't provide any song duration information. In this case GetSubsongDurationSample() will return 0.

````
static void Destroy(SndhRenderer* sr);
````
Destroy SndhRenderer object and free any internal allocated memory


# Versions

- 1.10 : AtariAudio is now fully thread safe! (Use a custom Musashi 68k emulation version)
- 1.09 : API refactor and MuteVoices function added
- 1.08 : more robust API
- 1.07 : some API changes and cleanup
- 1.06 : added SetDefaultSongDuration for SNDH files without any duration info
- 1.05 : SndhFile::AudioRender API change (now returns sample count). Use timedb database for SNDH without music len
- 1.04 : added SndhFile::FastForward function
- 1.03 : added Ripper & Converter into SubSongInfo struct. some minor linux compilation fixes

# Examples

The repo also contains a sndh2wav project to show how to convert a .sndh file into a WAV file

# Applications using AtariAudio library

[SndhArchivePlayer](https://github.com/arnaud-carre/sndh-player) - Player able to directly open the large 100MiB SNDH ZIP archive file and instant play any of thousand Atari music

[BZR Player 2](https://github.com/aargirakis/BZRPlayer) - Audio player for Windows and Linux supporting a wide array of multi-platform exotic file formats

# Credits

- AtariAudio library written by Arnaud Carré aka Leonard/Oxygene.
- MUSASHI 68000 emulation written by Karl Stenerud
- Atari ICE depacker C version written by Hans Wessels
- timedb.inc.h database by Benjamin Gerard & SNDH Community
