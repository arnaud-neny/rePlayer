### The DNS decoder in this library

Although unrelated to TFMX, the music player called ``Dynamic Synthesizer`` has
been used briefly by [Chris Huelsbeck](https://www.huelsbeck.com/) while
migrating from the Commodore 64 to the Amiga.

In response to this library, it has been asked how the earlier music format
relates to Soundtracker and the much more elaborate TFMX? After taking a look
at disassemblings of the DNS player found at the beginning of music files,
the short answer is:

Dynamic Synthesizer is a light-weight, header-less music system that can replay
samples via patterns similar to Soundtracker, but with an almost minimal set of
features only. For example, no effects like pitchbend, no vibrato, no arpeggio,
no modification of pattern progression either. Just one-shot samples and looping
samples, but with a full ADSR volume envelope including key down/up events. 
Yet the player organizes the patterns within six tracks, and when playing
a sound, it dynamically assigns a track to one of the four audio channels.
As such, sounds chop off each other more often than with a fixed assignment
of tracks to audio channels. And with stereo output, they jump from left to
right or vice versa seemingly spontaneously. This dynamic assignment of audio
channels has not been copied into TFMX.

The DNS decoder in this library is written from scratch, but closely mimics
the behaviour of the available machine code players. All the music files but
one mention "V1.34" but the underlying music format and players differ and
are not compatible with each other.
