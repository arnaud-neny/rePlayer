# :musical_note: zingzong :musical_note:

### A `Microdeal Quartet` music file player

[Quartet](https://demozoo.org/productions/131242/) is a music score
editor edited by Microdeal in 1989 for the Atari ST and the Amiga home
computers. It features a 4 digital channel sample sequencer and DSP
software rather rare at this time. It has been programmed by *Illusions
Programmers* Rob Povey and Kevin Cowtan.

You can find some more info and resources about Quartet at
[Amiga Source Preservation](https://amigasourcepres.gitlab.io/page/q/quartet/)


## Download

Source code, binaries for Windows, Atari ST and Amiga binaries ...

:floppy_disk: [GitHub Release page](https://github.com/benjihan/zingzong/releases)


## Quartet files and compatible

* Original `quartet` musics consist of:
  1. `.set` file extension of instrument files (voice set)
  2. `.4v` file extension of the score files (song)
It's not uncommon for a single voice set file to be shared by many
musics. while that's not a problem for zingzong command line player
where the user can pick the right files it can be more challenging to
integrate with common media players. Furthermore both file formats are
not very robust and lake a clear identifier.

`voice.set` is sometime used as the default or last resort instrument file.
   
 * Amiga archives
   1. `smp.` the Amiga default prefix for the instrument files (`.set`)
   2. `qts.` or `qta.` the Amiga default prefix for the song files (`.4v`)

`SMP.set` is sometime used as the default or last resort instrument file.

 * Atari ST archives
   1. `.q4` bundle files including song,instrument and info
   2. `.quar` are bundle files used privately by [sc68](http://sc68.atari.org)


#### Where to find Quartet musics ?

 * [Modland - Quartet ST directory](http://modland.com/pub/modules/Quartet%20ST/):
   `qts.` and `smp.` files
 * [Fading Twilight - Music from ATARI scene ](http://fading-twilight.atari.org/):
   DVD ISO contains many `.4q` files


## License and copyright

  * Copyright (c) 2017-2018 Benjamin Gerard AKA Ben/OVR
  * Licensed under MIT license


## Bugs

  Report bugs to [GitHub issues page](https://github.com/benjihan/zingzong/issues)
