v0.16.0
- Added XMP
- Updated adplug to 2.3.3.834
- Updated Furnace to 0.6.5
- Updated OpenMPT to 0.8.0-pre.5 r21107
- Updated libsc68 to 3.0.0a r706
- Updated libxml2 to 2.13.1
- Bug fixes

v0.15.21:
- Added Ayumi
- Added Ken
- Added Organya
- Updated adplug to 2.3.3.826
- Updated ASAP to 6.0.3
- Updated Furnace to 0.6.3
- Updated gbsplay to 0.0.97
- Updated libsidplay to 2.8.0
- Updated OpenMPT to 0.7.8
- Updated Opus to 1.5.2
- Updated ProTrekkr to 2.6.7
- Updated vgmstream to r1917
- Updated ImGui to 1.90.8
- Updated libarchive to 3.7.4 as static library
- Updated libcurl to 8.8.0 as static library with minimal content instead of full dynamic library
- Updated libxml2 to 2.13.0
- Updated TagLib to 2.0.1
- Archive loader
- Library auto merge option on download
- Patterns display (OpenMPT, Hively)
- Playlist sort from current playing position while pressing Ctrl
- Refactor file importer
- Refactor song packages
- Remote database validation (Modland, High Voltage SID Collection, Atari SAP Music Archive)
- Resizable UI
- Win32 port
- Bug fixes

v0.14.0:
- Added vgmstream
- Added WonderSwan
- Added VGMRips import
- Updated libvgm to 223b6f9
- Updated OpenMPT to 0.7.5
- Updated SNDH-Player to 0.74
- Bug fixes

v0.13.5 (Khatchadour's Release):
- Added Direct Stream Digital
- Added WavePack
- Added libxml2
- Updated OpenMPT to 0.7.4
- Updated Opus to 1.5.1
- Updated ProTrekkr to 2.6.6
- Updated StSound with surround
- Updated ImGui to 1.90.4
- Removed ImGuiFileDialog
- Removed Tidy Html
- Removed TinyXML-2
- Bug fixes

v0.12.7:
- Added Advanced Audio Coding
- Added Opus
- Updated adplug to 2.3.3.818
- Updated ASAP to 6.0.1
- Updated Furnace to 0.6.1
- Updated gbsplay to 0.0.95
- Updated libsc68 to 3.0.0a r705
- Updated libsidplay to 2.6.0
- Updated ProTrekkr to 2.6.5
- Updated UADE with Jochen Hippel ST player 1.3
- Updated ZXTune to r5056+
- Updated dr_flac to 0.12.42
- Updated dr_mp3 to 0.6.38
- Updated dr_wav to 0.13.15
- Updated ImGui to 1.90.3
- Updated ImGuiFileDialog to 0.6.6.1 fixed
- Updated libcurl to 8.6.0
- Updated TagLib to 2.0
- Updated TinyXML-2 to 10.0.0
- Ability to play directly from an URL (drag and drop from the browser or import), including streaming (online radios)
- New job system to remove blocking tasks (removed interface freezes, asynchronous files check, less thread creation/deletion spam...)
- Bug fixes

v0.11.1 (Speedvicio's Release):
- Added Furnace
- Updated SNDH-Player to 0.72
- Bug fixes

v0.10.0:
- Added Euphony
- Added Quartet
- Updated modland to import Euphony
- Updated libcurl to 8.4.0
- Bug fixes

v0.9.4:
- Added SNDH-Player
- Updated adplug to 2.3.3.808
- Updated ASAP to 6.0.0 (fixed)
- Updated Highly Advanced (cog 2023-09-30)
- Updated libkss to 1.2.0, fix MBM loader
- Updated libsc68 to 3.0.0a r702
- Updated libvgm to 079c4e7
- Updated OpenMPT to 0.7.3
- Updated NEZplug to support FAC SoundTracker
- Updated UADE to 3.0.3 (and to handle of big modules and to allow player selection)
- Updated ZXTune to r5052
- Updated modland to import FAC SoundTracker
- Updated modland to import Hippel ST COSO
- Updated modland to import MoonBlaster
- Updated modland to import Startrekker AM
- Updated modland to import TFMX ST
- Updated modland to import Unique Development
- Updated ImGui to 1.89.90
- Updated libcurl to 8.3.0
- Updated zlib to 1.3
- Bug fixes

v0.8.7:
- Added ZXTune
- Added ZX-Art import
- Updated ZXTune to r5040
- Updated modland to import packaged IFF-SMUS
- Updated TagLib to 1.13.1
- Updated ImGui to 1.89.82
- Updated libcurl to 8.2.1
- Updated artist tab to display and interact with songs using a table
- Updated Amiga Music Preservation scanner
- Disable useless graphics shader detection
- Bug fixes

v0.7.1:
- Added Buzzic 2
- Updated OpenMPT to 0.7.2
- Updated modland to convert Octamed extensions
- Bug fixes

v0.6.1:
- Added Highly Competitive/Snsf9x
- Updated adplug to 2.3.3.804
- Updated gbsplay to 0.0.94@e358ee9
- Updated libsidplay to 2.5.0
- Updated libvgm to fd7da37
- Updated UADE to support PreTracker
- Updated ImGui to 1.89.60
- Updated ImGuiFileDialog to 0.6.5
- Updated modland to import Super Nintendo Sound Format
- Bug fixes

v0.5.2:
- Added vio2sf
- Added Highly Theoretical
- Added Highly Experimental
- Added LazyUSF
- Updated modland to import Nintendo DS Sound Format
- Updated modland to import Saturn Sound Format
- Updated modland to import Dreamcast Sound Format
- Updated modland to import Playstation Sound Format
- Updated modland to import Playstation 2 Sound Format
- Updated modland to import Ultra64 Sound Format
- Updated modland to import packaged Delitracker Custom Format
- Updated libcurl to 8.1.2
- Added missing Tim Follin extension (.tf)
- Added missing Delitracker Custom extension (.cust)
- Bug fixes

v0.4.1:
- Added TIATracker
- Added Atari SAP Music Archive import
- Added JSON for Modern C++
- Replaced deck buttons labels with icons
- Added endless song playback
- Added export to wav
- Updated libcurl to 8.1.1
- Updated dr_flac to 0.12.40
- Updated dr_mp3 to 0.6.35
- Updated dr_wav to 0.13.9
- Bug fixes

v0.3.4:
- Added NEZplug player
- Added SNDH import
- Updated adplug to 2.3.3.801
- Updated OpenMPT to 0.7.1
- Updated ImGui to 1.89.53 (to fix the modals getting closed on focus) with single axis auto resize patch
- Settings for the media hot keys
- Unfold deck option to display song metadata
- Added missing support for splitted tfmx files (mdat & smpl)
- External drag and drop is filtered with extensions and prefixes
- Display song source within tooltip
- Select currently used replay when opening settings panel
- Bug fixes

v0.2.1:
- Added KSS player
- Updated OpenMPT to 0.6.10
- Updated ImGui to 1.89.5
- Playlist columns reorderable and hideable
- Drag and drop files onto the deck; press shift on drop to clear the current playlist
- Bug fixes

v0.1.3:
- Added iXalance player
- Bug fixes

v0.0.0:
- First release!