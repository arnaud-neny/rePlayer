# SNDH-Archive-Player
ATARI-ST SNDH ZIP Archive music browser/player by [Leonard/Oxygene](https://twitter.com/leonard_coder)

![image info](./thumbnail.png)

# Versions

- v0.80 : fix minizip library when dealing with weird zip path name
- v0.79 : minor STE DAC potential fix
- v0.78 : fix oscilloscope scale issue in v0.77
- v0.77 : ym2149 has now proper 32 steps volume enveloppe
- v0.76 : Remove custom docking windows ability
- v0.75 : Light speed large SNDH ZIP archive loading (multi-thread)
- v0.74 : fix crash when loading some SNDH files
- v0.73 : uninitialized vars potential crash fixed
- v0.72 : "low level buzzers" support
- v0.71 : minor fixes for some sndh musics
- v0.70 : un-predictable YM square tone edges as on real hardware
- v0.60 : Tao "MS3" songs support! Added subsong count in list
- v0.50 : reload last loaded SNDH archive, bug fixes for some SNDH
- v0.40 : per voice visualization oscilloscopes, 2MiB emulated Atari machine, AtariAudio API cleanup, some optimizations
- v0.30 : WAV export, play/pause button, and low CPU usage when app minimized
- v0.20 : SNDH music are time seekable! enjoy!
- v0.10 : first version



# Why?
While having fun writing a YM7 format player on embeded device, I started to re-write my 30 years old ym2149 emulation (StSound). The new emulation is more accurate and source code is really simple.
To be able to properly test new emulatior (ym2149, mfp & STE DAC), I wrote a SNDH player (using great Musashi 68k emulator) to listen to thousands of SNDH files from sndh.atari.org.
You can use this library to play SNDH in your own player. Everything is in AtariAudio/ directory

# Drop single large ZIP file and enjoy
Just drop the latest 100MiB SNDH ZIP archive file downloaded from awesome [SNDH-YM2149 Archive website](https://sndh.atari.org/download.php) and start to browse & play!

[You can watch a running example here!](https://youtu.be/c0lH98TNtGg)

# How to build
This repo comes with a pre-build player (look at the GitHub Release section) but you can also compile the executable by your own. Easy way is to have a windows system, download latest "Visual Studio 2022 community" (it's free & awesome) and open SndhArchivePlayer.sln

If you're still stuck in past century you can also create a makefile by yourself :)

Enjoy!

[https://github.com/arnaud-carre/sndh-player](https://github.com/arnaud-carre/sndh-player)
