# C language wrapper library for TFMX audio decoding
https://github.com/mschwendt/libtfmxaudiodecoder


Another music player backend library supporting several music file
formats originally created by two renowned game soundtrack pioneers from the
Commodore Amiga era of computing:

 - TFMX by [Jochen Hippel](https://en.wikipedia.org/wiki/Jochen_Hippel) (incl. the ripoffs ``Future Composer`` and MCMD)

 - TFMX by [Chris Hülsbeck](https://www.huelsbeck.com) (``The Final Musicsystem eXtended``)

 - file format modifications TFMXPACK, TFMX-MOD, TFHD + some unnamed modpacks

Despite sharing the name tag TFMX and the 7V feature, it's two music players
that are vastly different. Even simple concepts like ADSR volume envelopes
are done differently.

This library is a successor of [libfc14audiodecoder](TFMX_HIP_FC.md) and
is in the same style. Hopefully some of this will help with improving the
TFMX support of projects like [UADE](https://www.exotica.org.uk/wiki/UADE) or
[tfmx-play](http://www.boomerangsworld.de/cms/patches/tfmxplay.html).
Particularly tfmx-play has been merged into various multi-format music players,
but remains incomplete, e.g. with regard to TFMX macros strictly required by
some modules.

## Compatibility

Tell me! 
https://github.com/mschwendt/libtfmxaudiodecoder/issues

I've mostly listened to the ``Logical`` soundtrack by
[Rudolf Stember](http://www.stember.com/) in a looping playlist (since
it's among my favourite TFMX modules), and heard ``Monster Time`` and
``Rest In Peace`` by Jan Krolzig for the first time. Whoah!

The library has been tested with a large number of files from old
and current major collections like [Modland](https://modland.com/) and
[ExoticA](https://www.exotica.org.uk/wiki/Category:Amiga_Music_Formats).
During pre-release testing, multiple issues reported in bug trackers of
other music players have been reviewed, too.

If you are an author of a music player, please give this library a try,
and consider adding a plug-in, if you like what you hear.

## Important

Within music collections, the music data files recognized by this library
usually use a file name extension from this list:

    .tfx, .tfm, .mdat, .tfmx
    .hip, .hipc, .hip7, .mcmd
    .fc, .fc3, .fc4, .fc13, .fc14, .smod

Some of the music comes as a pair of files (music data + separate samples file), if it has not been converted
into a single-file format, e.g.:

    foo.mdat + foo.smpl
    bar.tfx + bar.sam

In case it is the original file naming style from Commodore Amiga, where
the file type name extension was a prefix,

    mdat.foo + smpl.foo

it is strongly recommended to rename your files and give them PC-style
extensions instead. For example, ``.tfx`` and ``.sam`` is a good compromise
for TFMX ``mdat.`` and ``smpl.`` files.

The library backend itself inspects the actual input data while it tries to
determine the underlying file format. Yet if it's a pair of files, it
tries to find the second file based on guessing its file name extension.

The TFMX-editor also used a third file starting with ``info.`` to store
stuff like names for patterns and macros. As those files are entirely optional,
they have not been published and have not been preserved in module
collections either.

## Players & Plugins

* [Audacious](https://audacious-media-player.org/) with [this input plugin](https://github.com/mschwendt/audacious-plugins-fc)
* [DeaDBeeF](https://deadbeef.sourceforge.io/) with [this input plugin](https://github.com/mschwendt/deadbeef-plugins-fc)
* [Qmmp](https://qmmp.ylsoftware.com/) with [this input plugin](https://github.com/TTK-qmmp/qmmp-tfmx)
* [rePlayer](https://github.com/arnaud-neny/rePlayer) includes a prebuilt plugin among a ton of others as to handle a high number of music file formats
* ...

## Links

* https://www.exotica.org.uk/wiki/TFMX
* https://de.wikipedia.org/wiki/TFMX
* https://modland.com/ and https://www.exotica.org.uk/wiki/Category:Amiga_Music_Formats
* https://chrishuelsbeck.bandcamp.com/
* http://thethalionsource.w4f.eu/Hippel/hippelm.htm
* https://remix64.com/ as a hub for the C64 and Amiga remix community.
