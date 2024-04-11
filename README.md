# rePlayer (another multi-formats music player)

| [Getting started](#get-started) - [The deck](#the-deck) - [The library](#the-library) - [The song editor](#the-song-editor) - [The playlist](#the-playlist) - [The settings](#the-settings) - [The tray icon](#the-tray-icon) |
:----------------------------------------------------------: |
| [Web sites](#web-sites) - [Third Parties](#third-parties) |

With this piece of software, you can play a lot of oldschool music format from the old times, such as the <b>Amiga</b> (which was the original purpose of this player), Atari, C64, Nintendo, Sega...  
But why doing that when you already have other softwares like xmplay, foobar, vlc... ? Well, nowadays, you download your music package from websites like Amiga Music Preservation, The Mod Archive, Modland... and drop your files in the software.  
As a maniac collector, it started to get very annoyed to keep up to date with newer songs from an artist, check for songs duplicates, and also some missing playback functionalities.  
So I started to write my own player, first to play default modules from the amiga using ImGui (for the interface), OpenMPT (to play amiga modules) and libcurl (to handle the downloads from the websites).  
I've improved it in my spare time, adding more playable formats, a way to handle my database(s), some features I've always wanted in other software...

So now, it's time for me to release it as I think some people can have interest in it.  
Oh, and occasionally, you can use it as a normal player as it can also play mowadays formats (mp3, wav, ogg...).

## Getting started

The first time, download the [lastest release](https://github.com/arnaud-neny/rePlayer/releases/latest/download/rePlayer.zip), unzip it and start rePlayer.exe  
The software will try to auto-update itself each time you start it, so you won't have to download it again.  
If you want to play your own files, just drag and drop them into the playlist. Or if you want to download new ones in the library, just click on import and select what you want it from (songs, artist or files), then search and pick your songs, you can later drag and drop from the library to the playlist.
The player works only on PC with Windows 10 (x64) and later versions.

## The deck

![rePlayer's Deck](/doc/deck.png)

The main panel where you can see your playing song, and interact with the playback:
1. VU meter (just a fancy visualizer).
2. Next to it is the vertical volume bar (drag and drop the slider, or use the mouse wheel or the media keys).
3. Song title.
4. Artist(s).
5. Playback informations (number of channels, playback type, player).
6. Seek bar displaying playback time and song length (and playlist position and size). It's possible you can't interact with it if the player doesn't allow it.
7. Classic playback buttons (go to previous song in the playlist, stop, play/pause, go to next song in the playlist, toggle the playlist or endless song loop).
8. Menu (access to settings, enable/disable other panels, close the application...).
9. Song metadata, click to expand/fold the deck (protracker instruments, id tags, extra informations...).

## The library

This where you manage all your songs and artists you've imported (or going to import), so you have there two tabs, one for songs and the other for the artists

- ### The song tab (main library view)

![rePlayer's Library Songs](/doc/libsongs.png)

1. Title bar, displaying the number of filtered songs (and selected) and some extra informations of our current state of the database.
2. Import button: click it to import files from you disk drives or artists or songs from online websites/databases.
3. Song tab button: the current tab button for the songs
4. Artists tab button (see next paragraph).
5. Tags button: filter your database from the tags you've setin your songs.
6. Combo to select the behavior of the search filter (7): all, songs or artists.
7. Search filter: type text their and it will filter your songs list.
8. Song list.
9. Rating button: the rating you set to your song, from N/A (not available aka not set) and 0 to 100%.
10. End of playback button: to define the type end you want in your song. U means undefined (the player will detect it when it will reach the end of the song); F to fade out at the end, L to loop the song once.

Double click on a song will try to play it. If you already have a song playing from the playlist, you are not going to loose its tracking: this song is "shelved" and will resume at the end of the other song or if you go to the next one.  
Middle click on a song will open the song editor with this song as editable  
Right click on selected songs bring a menu:

    - Invert selection
    - Add to playlist
    - Add to artist (shortcut to assign the song to another artist)
    - Tags: you can assign the tags to the songs
    - Discard: remove the song from the database
    - Export as WAV
    - Merge songs: open a pop up where you can "merge" the songs (to remove duplicates but keep references between each other from different sources).
      In this editor, songs are by default merged if there are identical.
      Then you can do some drag and drop to force some merge.
      Double-click on a song will cancel the merge, but Ctrl+Double click will play it.
      Press F2 to rename the latest selected song.
      Press F7/F8 to focus on previous/next set of merged songs

- ### The artist tab

![rePlayer's Library Artists](/doc/libartists.png)

1. Search filter: type text their and it will filter your artists list.
2. Artist list.
3. Editable Artist informations.
4. Source list where the artist has been imported from; each time you click the import button, it will automatically fetch all songs from this sources (The main purpose of this player for me).
5. Merge With Another Artist button: when importing from a website, it will create an artist from there. So you can have the same artist duplicated in the database. This is the way to combine these artists.

## The song editor

![rePlayer's Song Editor](/doc/songed.png)

You can edit all the properties of a song there.
- Info: database id (with L for library and P for playlist), file size, date when the file was added to the database and release year.
- Title: main song title.
- Subsong: if there are subsongs you can unfold by click the Subsong label. Change here the subsong title, end of playback behavior and rating.
- Artist: the artist(s); you can add, remove and re-order these.
- Tags: select/unselect your song tags here.
- Player: the current player used for the song and its extension. Sometimes, you can use different player for a song, select the one you want (click scan to put all usable on top of the combo). You can also change the extension here as sometimes, the player can identify the right format only with it.
- Settings: you can override here all the player parameters just for this song.
- Source: list of sources where the song comes from. You can discard a source there.

For some players, you can edit the song length. For that you can simply enter the length (click on the numbers or drag and drop like a slider), or use the wave editor (E) where a wave form of the song is generated. There you can left click to set your end of song marker; you can play the generated waveform and move its cursor with the right click.

## The playlist

![rePlayer's Playlist](/doc/playlist.png)

Add songs from the library here (from the library context menu, or drag and drop a selection from the library there).  
Or add songs from you disk drive (drag and drop).  
You can select the songs and reorder these. Click on the numbered button to track the song in its database (library/playlist).  
An Url button to add a list of links to the playlist.  
Buttons to load, save or clear the playlist.  
And a button to sort (by type, artists, duration, type or random/shuffle). If you press Ctrl, the sort will happen only for entries after the current playing one.  
Note 1: if you drag and drop files in there, the playlist will have a sidebar with buttons which open a database like the library managing all these files and urls. It's almost working the same way as the library.  
Note 2: while drag and dropping files, keep Ctrl pressed to force all files in the playlist (by default, they are filtered by their extension or prefix). Keep Shift pressed and the current playlist is cleared. You can also drop on the deck.  
Note 3: you can drag and drop url. On load, it will download the song or stream the online radio.

## The settings

![rePlayer's Settings](/doc/settings.png)

Enable/disable the songs focus there (when a song start to play, it is focused in the playlist and/or database).  
AutoSave frequency of the rePlayer. (it will save on quit anyway).  
Enable/disable the media hot keys (for playback and volume).  
Transparency of the windows.  
Live loggin can be enabled/disabled as well as its save on the disk. The log retention can be changed too (older log files than the retention days limit are deleted).  
You can change the global settings of each players. I will not enter into the details here, I'm too lazy (I've just enabled by default my simple surround filter because I like it).

## The tray icon

There is a tray icon where you can see the playing song as a bubble info.  
There is a context menu (right click), to edit the windows states:
- Visible: is rePlayer visible or running in the background.
- Always On Top: the windows are always on top.
- Passthrough: the mouse clicks pass through the window and will click behind it (usefull when transparent).
- Minimal: only the deck is opened.
- Invert States: invert the values of the always on top, passthough and minimal (I like to invert all these to have the transparent deck only in the top right corner I can't interact with).
- Exit: guess what.

Left click will turn on/off the Visible flag.  
Middle click will turn on/off the invert States flag.

## The patterns

Realtime display of the current played pattern.
Options are available to show or hide some parts of the patterns. (Usefull if you want to watch the [Wrecklamation](https://modland.com/pub/modules/Fasttracker%202/Hoffman/wrecklamation.xm) from Hoffman with only the colors enabled).

## Web Sites

Here are the actual web sites you can import from (and thank you for all these repositories):
- [Amiga Music Preservation](https://amp.dascene.net): one of the best organized amiga music repository.
- [ASMA](https://asma.atari.org): Atari SAP Music Archive.
- [High Voltage SID Collection](https://hvsc.de): an exclusive Commodore 64 repository.
- [Modland](http://modland.com/pub/modules): a multi-formats repository organized by format.
- [SNDH](https://sndh.atari.org): Atari ST YM2149 archive.
- [The Mod Archive](https://modarchive.org): a huge module repository.
- [VGMRips](https://vgmrips.net): the biggest and most active site for retro Video Game Music rips.
- [ZX-Art](https://zxart.ee): Large collection of 8-bit ZX Spectrum software, games, demoscene, chiptune music and pixel art graphics, sorted by demoparties, authors and years.

You can chat about some of these on the [Demozoo](https://discord.io/demozoo) discord server in the modland channel.

Or add some comments (and likes) there:
- [pouet](https://www.pouet.net/prod.php?which=94326)
- [demozoo](https://demozoo.org/productions/323598)
- [twitter/x](https://twitter.com/ArnaudNeny)
- [mastodon](https://mastodon.gamedev.place/@arnaud)
- [bluesky](https://bsky.app/profile/replayer.bsky.social)

## Third Parties

### System 3rd parties:
- [Curl](https://curl.se): library for transferring data with URLs.
- [Dear ImGui](https://github.com/ocornut/imgui/tree/docking): the docking version of Dear ImGui.
- [dllloader](https://github.com/tapika/dllloader): a windows tool to be able to load multiple times the same dll as if it is a new one (very helpful to avoid refactoring old code with global variables everywhere).
- [JSON](https://github.com/nlohmann/json): JSON for Modern C++.
- [libarchive](https://github.com/libarchive/libarchive): to read and write streaming archives (zip, rar...).
- [libxml2](https://gitlab.gnome.org/GNOME/libxml2): XML toolkit.
- [stb](https://github.com/nothings/stb): for the optimized sprintf.
- [TagLib](https://taglib.org): to read tag from music formats.

### Replays 3rd parties:
- [adplug](https://github.com/adplug/adplug): a free AdLib sound player library.
- [ASAP](http://asap.sourceforge.net): Another Slight Atari Player.
- [Ayfly](https://github.com/l29ah/ayfly): Cross-platform AY-3-8910 music player.
- [Ayumi](http://sovietov.com/app/ayumi/ayumi.html): Highly precise emulation of AY-3-8910 and YM2149 sound chips. webAyumi for amad & fxm backend.
- [Buzzic 2](https://www.pouet.net/prod.php?which=54407): Music synthesizing tool.
- [Direct Stream Digital](https://github.com/Sound-Linux-More/sacd): Plays Philips DSDIFF and Sony DSF files, heavily inspired by SACD Decoder from Robert Tari.
- [dr_libs](https://github.com/mackron/dr_libs): FLAC and MP3 audio decoders; WAV audio loader and writer.
- [eupmini](https://github.com/gzaffin/eupmini): Music driver EUPHONY (Extension ".EUP") format player.
- [FAAD2](https://github.com/knik0/faad2): Freeware Advanced Audio (AAC) Decoder.
- [Farbrausch ViruZ II](https://github.com/farbrausch/fr_public): Synthetizer (v2m). The C engine is there, but I've also ported the x86 assembly (some sort of emulation) as the C one is not working the same way.
- [Furnace](https://github.com/tildearrow/furnace): a multi-system chiptune tracker compatible with DefleMask modules.
- [FutureComposer](https://sourceforge.net/projects/xmms-fc/files/libfc14audiodecoder): fc-smod formats.
- [game-music-emu](https://github.com/libgme/game-music-emu): collection of audio emulators for assorted video game console hardware (NES, Megadrive, Gameboy...).
- [gbsplay](https://github.com/mmitch/gbsplay): Gameboy sound player.
- [Highly Advanced](https://bitbucket.org/losnoco/cog): Gameboy Advanced player.
- [Highly Competitive/snsf9x](https://github.com/loveemu/snsf9x): Super Nintendo music player.
- [Highly Experimental](https://bitbucket.org/losnoco/highly_experimental): Playstation and Playstation 2 music player.
- [Highly Quixotic](https://gitlab.com/kode54/highly_quixotic): QSound arcade audio sound chip and processor emulator library.
- [Highly Theoretical](https://bitbucket.org/losnoco/highly_theoretical): Saturn and Dreamcast music player.
- [HivelyTracker](https://github.com/pete-gordon/hivelytracker): ahx-thx-hvl formats.
- [iXalance](https://bitbucket.org/wothke/webixs): IXS player (Impulse Tracker modules with procedural samples).
- [Ken](http://advsys.net/ken): in_ken plays all Ken Silverman's formats.
- [LazyUSF](https://bitbucket.org/losnoco/lazyusf2): Nintendo 64 music player.
- [LIBKSS](https://github.com/digital-sound-antiques/libkss): LIBKSS is a music player library for MSX music formats, forked from MSXplug. Supported formats are .kss, .mgs, .bgm, .opx, .mpk, .mbm.
- [libvgm](https://github.com/ValleyBell/libvgm): to play vgm files (consoles).
- [mdxmini](https://github.com/mistydemeo/mdxmini): play back MDX chiptunes from the X68000 home computer.
- [NEZplug++](https://github.com/jprjr/libnezplug): using the [libnezplug](http://offgao.net/program/nezplug++.html) fork to decode .nsf, .nsfe, .hes, .kss, .gbr, .gbs, .ay, .sgc, .nsd, .mus.
- [OpenMPT](https://lib.openmpt.org/libopenmpt): all protracker like formats (ProTracker, Fast Tracker, Scream Tracker, Impulse Tracker...).
- [Opus](https://opus-codec.org): Opus Interactive Audio Codec.
- [Organya](https://www.cavestory.org/pixels-works/org-maker.php): music format used in Cave Story.
- [ProTrekkr](https://github.com/hitchhikr/protrekkr): Tracker/Synth program.
- [sc68](https://sourceforge.net/projects/sc68): Atari ST and Amiga music player (sc68, sndh).
- [SidPlay](https://github.com/libsidplayfp/libsidplayfp): Commodore 64 formats (sid, mus).
- [SNDH-Player](https://github.com/arnaud-carre/sndh-player): ATARI-ST SNDH music player by Leonard/Oxygene.
- SoundMon: Sound Monitor (bp-bp3). I've rewritten some parts of the code as it didn't work as intented.
- [StSound](https://github.com/arnaud-carre/StSound): ym files.
- TFMX: library to play Chris Huelsbeck files (customized).
- [TIATracker](https://bitbucket.org/kylearan/tiatracker): a music tracker for making Atari VCS 2600 music on the PC, including a new sound routine for the VCS.
- [UADE](https://zakalwe.fi/uade): Unix Amiga Delitracker Emulator, customized a little bit for a better playback.
- [vgmstream](https://github.com/vgmstream/vgmstream): A library for playback of various streamed audio formats used in video games.
- [vio2sf](https://bitbucket.org/losnoco/vio2sf): Nintendo DS music player.
- [Vorbis](https://github.com/edubart/minivorbis): to decode OGG sound files.
- [WavPack](https://github.com/dbry/WavPack): Hybrid Lossless Wavefile Compressor.
- [WonderSwan](https://foobar2000.xrea.jp/?Input+64bit#f44897bc): aka foo_input_wsr. You can play the song data of BANDAI's Wonder Swan.
- [zingzong](https://github.com/benjihan/zingzong): A simple Microdeal quartet music file player.
- [ZXTune](https://zxtune.bitbucket.io): Crossplatform chiptunes player (without already owned libraries).

## Known issues
- Doesn't work on Intel® HD Graphics 4600 (fails in root signature or pipeline state creation, but without the hardware, I can't pinpoint the cause)