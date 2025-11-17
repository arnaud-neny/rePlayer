// Future Composer & Hippel player library
// by Michael Schwendt

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, see <https://www.gnu.org/licenses/>.

#include "HippelDecoder.h"

namespace tfmxaudiodecoder {

// (NOTE) This is compatible with Future Composer, which had copied its
// vibrato implementation from a TFMX player release including a bug
// that resulted in disabling half speed mode always.
//
// However, for 100% compatibility with FC, the half speed mode is
// made configurable, so no FC module activates half speed mode by mistake.
// Two known FC modules set vibrato speed to a negative value
// in a strange way (speed $bb, amplitude $00 = effectively no vibrato,
// and speed $ff, amplitude $01),

uword HippelDecoder::TFMX_vibrato(VoiceVars& voiceX, uword period, ubyte note) {
    if (voiceX.vibDelay != 0) {
        --voiceX.vibDelay;
        return period;
    }
    sword offs = voiceX.vibCurOffs;
    
    // Check half speed mode.
    //
    // Since the flag byte is initialized to $40 (vibrato up) always,
    // TFMX player releases that test whether flag byte is positive
    // effectively disable half speed mode always and run vibrato at
    // full speed. FC had copied that bug.
    //
    // This implementation removed the non-working check of flag bit 7,
    // since no TFMX or FC music module can make the flag byte become
    // negative.
    //
    // Later TFMX player releases moved the half speed mode flag
    // into the vibrato speed parameter bit 7 (1=on, 0=off).
    // TFMX modules typically don't set vibrato speed as high as $8x,
    // unless they want to activate half speed mode. So, for TFMX the
    // check can be enabled by default.
    ubyte speed = voiceX.vibSpeed;
    if (traits.vibSpeedFlag && (speed&0x80)!=0) {  // half speed mode enabled?
        speed &= 0x7f;
        voiceX.vibFlag ^= 0x01;  // bit 0 decides when to update vibrato
    }
    // Check half speed mode primary flag.
    if ((voiceX.vibFlag&0x01)==0) {
        // Vibrato offset changes between [0,1,...,2*vibAmpl]
        // Offset minus vibAmpl is value to apply.
        sword vibDelta = voiceX.vibAmpl;
        vibDelta <<= 1;  // pos/neg amplitude

        // vibFlag bit 5: 0 => vibrato down, 1 => vibrato up
        if ((voiceX.vibFlag&(1<<5))==0) {  // vibrato down
            offs -= voiceX.vibSpeed;
            if (offs < 0) {  // lowest value reached?
                offs = 0;
                voiceX.vibFlag |= (1<<5);   // switch to vibrato up
            }
        }
        else {  // vibrato up
            offs += voiceX.vibSpeed;
            if (offs > vibDelta) {  // highest value reached?
                offs = vibDelta;
                voiceX.vibFlag &= ~(1<<5);  // switch to vibrato down
            }
        }
        voiceX.vibCurOffs = offs;
    }
    offs -= voiceX.vibAmpl;

    // Whereas TFMX and FC calculate this using an array byte-offset,
    // we do it with a word index.
    //
    // Octave 5 at period table word index 48 contains the highest
    // period only. 48+80 = 128. This next bit ensures that vibrato
    // does not exceed the five octaves in the period table.
    // Octave 6 (but lowest!) is FC14 only.
    if (traits.vibScaling ) {
        note += 80;
        while (note < 128) {
            offs += offs;  // double vibrato value for each octave
            note += 12;  // advance octave index
        };
    }
    period += offs;
    return period;
}

// ----------------------------------------------------------------------
// COSO features a slightly different vibrato than TFMX:
//  - half amplitude
//  - relative adjustment to current period with 1024 as base period
//  - no half speed mode (which was not working anyway)

uword HippelDecoder::COSO_vibrato(VoiceVars& voiceX, uword period, ubyte note) {
    if (voiceX.vibDelay != 0) {
        --voiceX.vibDelay;
        return period;
    }
    sword offs = voiceX.vibCurOffs;
    if ( (voiceX.vibFlag&(1<<5)) == 0 ) {  // vibrato down
        offs -= voiceX.vibSpeed;
        if (offs < 0) {
            offs = 0;
            voiceX.vibFlag |= (1<<5);   // switch to vibrato up
        }
    }
    else {  // vibrato up
        offs += voiceX.vibSpeed;
        if (offs > voiceX.vibAmpl) {
            offs = voiceX.vibAmpl;
            voiceX.vibFlag &= ~(1<<5);  // switch to vibrato down
        }
    }
    voiceX.vibCurOffs = offs;
    // Shift offset to zero as base with neg/pos amplitude.
    offs -= (voiceX.vibAmpl>>1);
    // Scale offset to period with base 1024. 
    offs *= period;
    offs >>= 10;
    
    period += offs;
    return period;
}

}  // namespace
