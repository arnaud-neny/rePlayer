## C language wrapper library for Future Composer & Hippel TFMX audio decoding  
https://github.com/mschwendt/libfc14audiodecoder

The family of music files supported by this library typically uses one
of the following file name extensions:  

    .fc, .fc13, .fc14, .fc3, .fc4, .smod
    .hip, .hipc, .hip7
    .mcmd

If this library is used with an audio player (via a plug-in or built-in)
that by default performs file filtering by file name extension, it is
strongly recommended to keep that feature enabled and, ultimately, rename
audio files in meaningful ways. There is no standard, but reducing yourself
to a subset of the listed file name extensions would be a good idea.

The library backend itself inspects the actual input data while trying to
determine the underlying file format. Furthermore, on Commodore AMIGA (the
origin of those music files) usually the file name extension was put at the
beginning, and that still applies to some music collections found on the
Internet.

### Players & Plugins

* [Audacious](https://audacious-media-player.org/) with [this input plugin](https://github.com/mschwendt/audacious-plugins-fc) as the primary audio player that is being used while developing this library.

* [DeaDBeeF](https://deadbeef.sourceforge.io/) with [this input plugin](https://github.com/mschwendt/deadbeef-plugins-fc) as another cross-plattform audio player that has been tested with.

* [rePlayer](https://github.com/arnaud-neny/rePlayer) includes a prebuilt plugin among a ton of others as to handle a high number of music file formats.
