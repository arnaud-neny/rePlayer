# rePlayer (another multi-formats music player)

## General purpose libraries:
- Curl: https://curl.se/windows
- Dear ImGui: https://github.com/ocornut/imgui/tree/docking  
  To merge newer versions, replace imgui files in source\Core\ImGui and run batch DiffApplyImGui.bat (this will apply our changes to ImGui).  
  Then copy the imgui folder in rePlayer\external and run batch DiffMakeImGui.bat (this will create a patch file from the new version).
- dllloader: https://github.com/tapika/dllloader
- JSON for Modern C++: https://github.com/nlohmann/json
- stb (sprintf): https://github.com/nothings/stb
- libarchive: https://github.com/libarchive/libarchive
- libxml2: https://gitlab.gnome.org/GNOME/libxml2
- TagLib: https://taglib.org

## Replays Libraries:
- adplug: https://github.com/adplug/adplug
- ASAP: http://asap.sourceforge.net
- Ayfly: https://github.com/l29ah/ayfly
- Ayumi: https://github.com/true-grue/ayumi & https://bitbucket.org/wothke/webayumi
- Buzzic 2: https://www.pouet.net/prod.php?which=54407
- Direct Stream Digital: https://github.com/Sound-Linux-More/sacd
- dr_libs (MP3/FLAC/WAV): https://github.com/mackron/dr_libs
- eupmini: https://github.com/gzaffin/eupmini
- FAAD2: https://github.com/knik0/faad2
- Farbrausch ViruZ II: https://github.com/farbrausch/fr_public  
  Made the x86 version available as "emulation" as it renders closer to the original than the C version.
- Furnace: https://github.com/tildearrow/furnace
- game-music-emu: https://github.com/libgme/game-music-emu
- gbsplay: https://github.com/mmitch/gbsplay
- GoatTracker: https://sourceforge.net/p/goattracker2
- Highly Advanced: https://bitbucket.org/losnoco/cog
- Highly Competitive/snsf9x: https://github.com/loveemu/snsf9x
- Highly Experimental: https://bitbucket.org/losnoco/highly_experimental and https://bitbucket.org/losnoco/psflib
- Highly Quixotic: https://gitlab.com/kode54/highly_quixotic, https://gitlab.com/kode54/psflib and https://gitlab.com/kode54/foo_input_qsf
- Highly Theoretical: https://bitbucket.org/losnoco/highly_theoretical and https://bitbucket.org/losnoco/psflib
- HivelyTracker: https://github.com/pete-gordon/hivelytracker  
  With minor changes.
- iXalance: https://bitbucket.org/wothke/webixs  
  Fixed some memory leaks and made it x64 compliant.
- Ken: http://advsys.net/ken/ & https://bitbucket.org/wothke/webken
- LazyUSF: https://bitbucket.org/losnoco/lazyusf2, https://gitlab.com/kode54/lazyusf2 and https://bitbucket.org/losnoco/psflib
- LIBKSS: https://github.com/digital-sound-antiques/libkss
- libnezplug: https://github.com/jprjr/libnezplug
- libtfmxaudiodecoder: https://github.com/mschwendt/libtfmxaudiodecoder
- libvgm: https://github.com/ValleyBell/libvgm
- mdxmini: https://github.com/mistydemeo/mdxmini
- MTPng: https://aminet.net/package/mus/play/mtpng_68k (and https://github.com/mrkite/soundsmith)
- OpenMPT: https://source.openmpt.org
  Some changes have been made to manually change the protracker timings (cia or vblank).
- Opus: https://opus-codec.org
- Organya: https://bitbucket.org/losnoco/cog/src/main/Plugins/Organya & https://github.com/Strultz/orgmaker-3
- ProTrekkr: https://github.com/hitchhikr/protrekkr
- Psycle: https://sourceforge.net/projects/psycle
- sc68: https://sourceforge.net/projects/sc68
- SidPlay: https://github.com/libsidplayfp/libsidplayfp
- SkaleTracker: https://www.pouet.net/prod.php?which=20929
- SNDH-Player: https://github.com/arnaud-carre/sndh-player
- SoundMon: Brian Postma website is gone.  
  Partially rewritten and fixed from original code.
- StSound: https://github.com/arnaud-carre/StSound  
  Patched some players for continuous play and loop/end detection, added stereo support.
- SunVox: https://warmplace.ru/soft/sunvox
- TIATracker: https://bitbucket.org/kylearan/tiatracker
- UADE: https://zakalwe.fi/uade and https://gitlab.com/uade-music-player/uade
- vgmstream: https://github.com/vgmstream/vgmstream
- vio2sf: https://bitbucket.org/losnoco/vio2sf and https://bitbucket.org/losnoco/psflib
- Vorbis: https://github.com/edubart/minivorbis  
  Some changes there to improved the loops (score) and some windows port.
- WavPack: https://github.com/dbry/WavPack
- WonderSwan: https://foobar2000.xrea.jp/?Input+64bit#f44897bc
- XMP: https://github.com/libxmp/libxmp
- zingzong: https://github.com/benjihan/zingzong
- ZXTune: https://zxtune.bitbucket.io  
  Stripped to use only ZX stuff and "fixed" for MSVC.

## External tools:
- DXC: https://github.com/microsoft/DirectXShaderCompiler
- UPX: https://github.com/upx/upx