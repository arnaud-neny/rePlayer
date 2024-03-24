# webAyumi

Copyright (C) 2019 Juergen Wothke

This is a JavaScript/WebAudio plugin of "ayumi" . This plugin is designed to work with my generic WebAudio 
ScriptProcessor music player (see separate project) but the API exposed by the lib can be used in any 
JavaScript program (it should look familiar to anyone that has ever done some sort of music player plugin). 

I mainly added this to fill a gap regarding the .fym format and to have a more light weight player for
some of the formats that are already covered by spectreZX. I added support for some "Fuxoft AY Language"
based formats later.

Files that can be played include: .text (the proprietary internal format used by ayumi), .fym, .ym, 
.psg, .afx, .ay (subtype ZXAYAMAD), .fxm. 


Known limitations: The support for some of the formats seems to be somewhat limited (e.g. no digi drums
in .ym).

A live demo of this program can be found here: http://www.wothke.ch/webAyumi/


## Credits

The project is based on: Peter Sovietov's https://github.com/true-grue/ayumi
The used interpreter for "Fuxoft AY Language" is based on Pavel Dovgalyuk's https://github.com/Dovgalyuk/ArduinoFXMPlayer

## Project

The original ayumi sources can be found in the 'src' folder. Any changes are marked using respective
EMSCRIPTEN ifdefs. 

I am aware that there is already a manually rewritten JavaScript port of ayumi, but unfortunately that 
port seems to be incomplete with regard to the supported formats. I therefore decided to give ayumi a go..
   
My version integrates with my generic player infrastucture (same as all my other ports). Eventhough 
the Emscripten generated code used here may be somewhat larger than handwritten JavaScript, the 
difference should be neglegible especially when compiling to WASM. Also it should be easier to update 
to newer versions of Peter Sovietov's code in the future since the respective core emulator code did not 
need to be touched/rewritten. As compared to Peter Sovietov's original code the Python scripts where translated 
to JavaScript and support for LHA & zlib compressed formats was added (ayumi_adapter.js).

For the handling of LHA and zlib compressed formats the 3rd party 
lh4.min.js (https://github.com/erlandranvinge/lh4.js) and pako_inflate.min.js (https://github.com/nodeca/pako) libs 
are used. (For ease of use they are currently inlined directly into the backend_ayumi.js  (see makeEmscripten.bat). 
Reminder: lh4.min.js was patched to allow for the data extraction even without having the correct file name. 

## Supported formats

The code uses converters for various AY-3-8910 and YM2149 related input music formats - to generate the proprietary 
TEXT format (not related to Vortex Tracker II format) used by the player:

.ym: Limited to YM5 and YM6 formats - without support for the "special extensions" like digi-drums, etc (see http://leonard.oxg.free.fr/ymformat.html)
.fym: another YM register-recording format used here http://ym.mmcm.ru (2 chip configurations - see files marked as 'ts' - are not implemented)
.psg: recorded in ZX Spectrum emulator 'Z80 Stealth' 
.afx: sound effects from "AYFX Editor v0.6" (see https://shiru.untergrund.net/software.shtml)
.fxm: original "Fuxoft AY Language" music format
.ay (subtype: ZXAYAMAD): an Amiga specific variation of the "Fuxoft AY Language" music format


## Howto build

You'll need Emscripten (http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html). The make script 
is designed for use of emscripten version 1.37.29 (unless you want to create WebAssembly output, older versions might 
also still work).

The below instructions assume that the webAyumi project folder has been moved into the main emscripten 
installation folder (maybe not necessary) and that a command prompt has been opened within the 
project's "emscripten" sub-folder, and that the Emscripten environment vars have been previously 
set (run emsdk_env.bat).

The Web version is then built using the makeEmscripten.bat that can be found in this folder. The 
script will compile directly into the "emscripten/htdocs" example web folder, were it will create 
the backend_ayumi.js library - and an additional ayumi.wasm if you are compiling WASM output (This can be enabled in the 
makeEmscripten.bat to generate WASM instead of asm.js.). 
The content of the "htdocs" can be tested by first copying it into some 
document folder of a web server. 


## Depencencies

The current version requires version >=1.2.1 of my https://bitbucket.org/wothke/webaudio-player/

This project comes without any music files, so you'll also have to get your own and place them
in the htdocs/music folder (you can configure them in the 'songs' list in index.html).


## License

The MIT license is used for Peter Sovietov's original code as well as for the 
Web plugin version.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.