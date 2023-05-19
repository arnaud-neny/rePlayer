# TIATracker #

A music tracker for making Atari VCS 2600 music on the PC, including a new sound routine for the VCS. Current version: 1.3

(c) 2016-2017 by Andre "Kylearan" Wichmann (andre.wichmann@gmx.de)

* Manual: [https://bitbucket.org/kylearan/tiatracker/raw/master/data/TIATracker_manual.pdf](https://bitbucket.org/kylearan/tiatracker/raw/master/data/TIATracker_manual.pdf)
* Windows binaries: [http://www.compoundeye.net/demoscene/TIATracker.zip](http://www.compoundeye.net/demoscene/TIATracker.zip)
* For Linux and OS X, use "wine" or compile the source (uses Qt and SDL)
* Source: [https://bitbucket.org/kylearan/tiatracker](https://bitbucket.org/kylearan/tiatracker)
* Seminar talk from Revision 2016 about VCS music in general and TIATracker in particular: [https://www.youtube.com/watch?v=PVujzQySZls](https://www.youtube.com/watch?v=PVujzQySZls)

## Features ##

VCS sound routine features:

* Up to 7 melodic and 15 percussion instruments
* ADSR envelopes for volume and frequency for melodic instruments
* "Overlay" percussion which will play the next melodic notes immediately
* Arbitrary and variable pattern lengths individual for each channel
* An option for different tempo values for odd and even rows ("Funkspeed")
* An option to have different tempo values per pattern instead of global tempo
* Highly optimized and configurable replayer routine
* Richly documented source code, including specifications for all data structures

Tracker features:

* Graphical representation for available notes per waveform and how off-tune they are
* Customizable pitch guides optimizing the number of in-tune notes
* Graphical editors for melodic and percussion instruments
* Integrated pattern editor and sequencer
* Timeline "mini map" displaying the pattern sequences
* Sound emulation from the Stella emulator for playback
* Export to dasm, k65 and .csv

For feedback, bug reports and feature requests, send a mail to andre.wichmann@gmx.de!

## Compiling from source

You need Qt5 and SDL to build TIATracker from source. Open the project in Qt Creator and add a "make install" build step to the project, then compile it.