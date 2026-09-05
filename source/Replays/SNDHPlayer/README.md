# AtariAudio Library v1.06

src/ contains all files needed to compile AtariAudio library. It allows you to play ATARI SNDH music files. You can also directly use YM2149 emulator if you want to write your own YM tracker.
The libray doesn't use any dependency, and should compile on any platform, including embeded systems (it doesn't even use float )

# Playing SNDH file in your own app

AtariAudio library doesn't use any file IO. You should provide data from memory. Entry point is class SndhFile.
Look at SndhFile.h for API details but here is the absolute minimal:

````
bool	Load(const void* rawSndhFile, int sndhFileSize, uint32_t hostReplayRate);
````
Load a raw SNDH file from memory. You should provide the memory buffer, size of the raw file, and host replay rate. ( ex 44100 for 44.1Khz )

````
bool	InitSubSong(int subSongId);
````
Atari SNDH musics could contain several subsongs. You should *always* call InitSubsong before any audio rendering function. By convention, subsongs starts at 1.

````
int		AudioRender(int16_t* buffer, int count);
````
This is the main audio rendering function. Render "count" samples into buffer. Buffer is a 16bits, signed, mono, sample buffer.
Like, let's say your replay rate is 44.1Khz and you want to generate 1 second of music:

````
  int16_t* buffer = buffer of 44100*2 bytes ( one sample is 16bits, mono )
  AudioRender(buffer, 44100);
````

AudioRender returns the amount of samples generated. If it's lower than "count", it means you reached the end of the music

# Versions

- 1.06 : added SetDefaultSongDuration for SNDH files without any duration info
- 1.05 : SndhFile::AudioRender API change (now returns sample count). Use timedb database for SNDH without music len
- 1.04 : added SndhFile::FastForward function
- 1.03 : added Ripper & Converter into SubSongInfo struct. some minor linux compilation fixes

# Examples

The repo also contains a sndh2wav project to show how to convert a .sndh file into a WAV file

# Credits

- AtariAudio library written by Arnaud Carré aka Leonard/Oxygene.
- MUSASHI 68000 emulation written by Karl Stenerud
- Atari ICE depacker C version written by Hans Wessels
