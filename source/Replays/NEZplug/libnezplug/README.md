# libnezplug

This is a fork of [nezplug++](http://offgao.net/program/nezplug++.html),
converted into a library. I've removed all global variables (save for
some read-only `static const` variables), I'm still working to remove
a few other quirks but it *should* be thread-safe.

There's decoders for:

* NSF
* NSFE
* HES
* KSS
* GBR
* GBS
* AY
* SGC
* NSD

It can additionally load metadata from M3U files. You can load M3U files
in a "streaming" manner. Say you have a chiptune file with multiple M3U
files, you can load each M3U file individually instead of combining them
ahead of time.

See the example `dump-all` program for some ideas on how to use this.

# LICENSE

The original source is public domain. This version is available
as Public Domain, or under an MIT No Attribution license. See
`LICENSE`
