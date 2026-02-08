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

uword HippelDecoder::TFMX_portamento(VoiceVars& voiceX, uword period) {
    if ( (!traits.portaIsBit6 && (voiceX.pattVal2 & 0x20) != 0) ||  // TFMX
         (traits.portaIsBit6 && (voiceX.pattVal2 & 0x40) != 0) ) {  // MCMD
        udword x;
        if (voiceX.portaSpeed < 0 ) {
            x = (-voiceX.portaSpeed);
            if (traits.portaWeaker) {
                voiceX.portaOffs += (x<<8);
            }
            else {
                voiceX.portaOffs += (x<<11);
            }
            period += (voiceX.portaOffs>>16);
        }
        else {
            x = voiceX.portaSpeed;
            if (traits.portaWeaker) {
                voiceX.portaOffs += (x<<8);
            }
            else {
                voiceX.portaOffs += (x<<11);
            }
            period -= (voiceX.portaOffs>>16);
        }
    }
    return period;
}

// ----------------------------------------------------------------------
// COSO features a slightly different portamento than TFMX:
//  - relative adjustment to current period with 1024 as base period
//
// (NOTE) Yet some COSO players implement TFMX portamento instead!

uword HippelDecoder::COSO_portamento(VoiceVars& voiceX, uword period) {
    if ( (voiceX.pattVal2 & 0x20) != 0 ) {
        udword x;
        if (voiceX.portaSpeed < 0 ) {
            voiceX.portaOffs += (-voiceX.portaSpeed);
            x = period*(0xffff & voiceX.portaOffs);
            period += (x>>10);
        }
        else {
            voiceX.portaOffs += voiceX.portaSpeed;
            x = period*(0xffff & voiceX.portaOffs);
            period -= (x>>10);
        }
    }
    return period;
}

// ----------------------------------------------------------------------
// Future Composer implemented its own portamento, which also resulted
// in different parameters within pattern data and affected conversion
// of modules from TFMX.
//
// (NOTE) As of FC 1.4 portamento plays at half speed compared to
// old versions.

uword HippelDecoder::FC_portamento_pitchbend(VoiceVars& voiceX, uword period) {
    // Following flag divides the portamento speed by two
    // for FC14 modules.
    voiceX.portaDelayFlag ^= 0xff;  // = NOT
    if (traits.isSMOD || (voiceX.portaDelayFlag!=0) ) {
        sbyte x = voiceX.portaSpeed;
        if (x != 0) {
            if (x > 0x1f) {  // > 0x20 = portamento down
                x &= 0x1f;
                x = (-x);
            }
            voiceX.portaOffs -= x;
        }
    }
    
    // Pitchbend effect. Rarely used.
    // Following flag divides the pitch bending speed by two.
    voiceX.pitchBendDelayFlag ^= 0xff;  // not
    if (voiceX.pitchBendDelayFlag != 0) {
        if (voiceX.pitchBendTime != 0) {
            --voiceX.pitchBendTime;
            sbyte speed = voiceX.pitchBendSpeed;
            if (speed != 0) {
                voiceX.portaOffs -= speed;
            }
        }
    }

    period += voiceX.portaOffs;
    return period;
}

}  // namespace
