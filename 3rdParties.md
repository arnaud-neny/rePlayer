# rePlayer (another multi-formats music player)

## General purpose libraries:
- Dear ImGui: https://github.com/ocornut/imgui/tree/docking  
  To merge newer versions, replace imgui files in source\Core\ImGui and run batch DiffApplyImGui.bat (this will apply our changes to ImGui).  
  Then copy the imgui folder in rePlayer\external and run batch DiffMakeImGui.bat (this will create a patch file from the new version).
- stb (sprintf): https://github.com/nothings/stb
- ImGuiFileDialog: https://github.com/aiekick/ImGuiFileDialog
- Curl: https://curl.se/windows
- Tidy Html: https://www.html-tidy.org/developer
- TinyXML-2: https://github.com/leethomason/tinyxml2
- libarchive: https://github.com/libarchive/libarchive
- TagLib: https://taglib.org
- dllloader: https://github.com/tapika/dllloader
- JSON for Modern C++: https://github.com/nlohmann/json

## Replays Libraries:
- OpenMPT: https://lib.openmpt.org/libopenmpt  
  Some changes have been made to manually change the protracker timings (cia or vblank).
- HivelyTracker: https://github.com/pete-gordon/hivelytracker  
  With minor changes.
- SoundMon: http://www.brianpostma.com (unfortunately down now)  
  Partially rewritten and fixed from original code.
- StSound: https://github.com/arnaud-carre/StSound
- FutureComposer: https://sourceforge.net/projects/xmms-fc/files/libfc14audiodecoder
- SidPlay: https://github.com/libsidplayfp/libsidplayfp
- Farbrausch ViruZ II: https://github.com/farbrausch/fr_public  
  Made the x86 version available as "emulation" as it renders closer to the original than the C version.
- sc68: https://sourceforge.net/projects/sc68
- adplug: https://github.com/adplug/adplug  
  Patched some players for continuous play and loop/end detection.
- ASAP: http://asap.sourceforge.net
- dr_libs (MP3/FLAC/WAV): https://github.com/mackron/dr_libs
- Vorbis: https://github.com/edubart/minivorbis
- Ayfly: https://github.com/l29ah/ayfly
- gbsplay: https://github.com/mmitch/gbsplay
- game-music-emu: https://bitbucket.org/mpyne/game-music-emu
- mdxmini: https://github.com/mistydemeo/mdxmini
- libvgm: https://github.com/ValleyBell/libvgm
- UADE: https://zakalwe.fi/uade and https://gitlab.com/uade-music-player/uade  
  Some changes there to improved the loops (score) and some windows port.
- Highly Quixotic: https://gitlab.com/kode54/highly_quixotic, https://gitlab.com/kode54/psflib and https://gitlab.com/kode54/foo_input_qsf
- Highly Advanced: https://gitlab.com/kode54/mgba, https://gitlab.com/kode54/psflib and https://gitlab.com/kode54/foo_input_gsf
- ProTrekkr: https://github.com/hitchhikr/protrekkr
- iXalance: https://bitbucket.org/wothke/webixs  
  Fixed some memory leaks and made it x64 compliant.
- LIBKSS: https://github.com/digital-sound-antiques/libkss
- libnezplug: https://github.com/jprjr/libnezplug
- TIATracker: https://bitbucket.org/kylearan/tiatracker
- vio2sf: https://bitbucket.org/losnoco/vio2sf and https://bitbucket.org/losnoco/psflib
- Highly Theoretical: https://bitbucket.org/losnoco/highly_theoretical and https://bitbucket.org/losnoco/psflib
- Highly Experimental: https://bitbucket.org/losnoco/highly_experimental and https://bitbucket.org/losnoco/psflib
- LazyUSF: https://bitbucket.org/losnoco/lazyusf2 and https://bitbucket.org/losnoco/psflib
- Highly Competitive/snsf9x: https://github.com/loveemu/snsf9x
- Buzzic 2: https://www.pouet.net/prod.php?which=54407
- ZXTune: https://zxtune.bitbucket.io  
  Stripped to use only ZX stuff and "fixed" for MSVC.
- SNDH-Player: https://github.com/arnaud-carre/sndh-player

## External tools:
- DXC: https://github.com/microsoft/DirectXShaderCompiler
- UPX: https://github.com/upx/upx