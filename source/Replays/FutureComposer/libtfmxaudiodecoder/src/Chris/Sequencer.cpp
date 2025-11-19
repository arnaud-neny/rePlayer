// TFMX audio decoder library by Michael Schwendt

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

#include "TFMXDecoder.h"

// TFMX's sequencer is designed as a track table with N columns (= tracks).
// Each track can assign patterns to any audio channel or execute a small
// number of commands to affect either the track or song progression.
//
// Some modules use the LOOP command to escape from their initial start/end
// range within the track table.

namespace tfmxaudiodecoder {

const std::string TFMXDecoder::TrackCmdInfo[TRACK_CMD_MAX+1] = {
    "Track Cmd STOP    ",
    "Track Cmd LOOP    ",
    "Track Cmd SPEED   ",
    "Track Cmd 7VOICE  ",
    "Track Cmd FADE    "
};

bool TFMXDecoder::getTrackMute(ubyte t) {
    return (0==readBEuword(pBuf,offsets.header+0x1c0+(t<<1)));
}

void TFMXDecoder::processTrackStep() {
    int evalMaxLoops = RECURSE_LIMIT;  // NB! Around 5 would suffice.
    do {
        sequencer.evalNext = false;

        if (sequencer.step.current > (TRACK_STEPS_MAX-1)) {  // fubar then
            sequencer.step.current = sequencer.step.first;
        }
        sequencer.stepSeenBefore[sequencer.step.current] = true;

        udword stepOffset = offsets.trackTable+(sequencer.step.current<<4);
#if defined(DEBUG_RUN)
        cout << "# Step = " << hexW(sequencer.step.current) << " at 0x" << tohex(stepOffset) << endl;
#endif
        // 0xEFFE is the prefix of a track command.
        if ( 0xeffe == readBEuword(pBuf,stepOffset) ) {
            stepOffset += 2;
            uword command = readBEuword(pBuf,stepOffset);
#if defined(DEBUG_RUN)
            cout << "  ";
            dumpBytes(pBuf,stepOffset,2);
            cout << " ";
#endif
            stepOffset += 2;
            if (command > TRACK_CMD_MAX) {  // fubar then
                command = 0;  // choose command "Stop" as override
            }
            trackCmdUsed[command] = true;
#if defined(DEBUG_RUN)
            cout << TrackCmdInfo[command];
#endif
            (this->*TrackCmdFuncs[command])(stepOffset);
#if defined(DEBUG_RUN)
            cout << endl;
#endif
        }
        else {
#if defined(DEBUG_RUN)
            cout << "  ";
#endif
            // Set PT TR for each track.
            for (ubyte t=0; t<sequencer.tracks; t++) {
                Track& tr = track[t];
                tr.PT = pBuf[stepOffset++];
                tr.TR = pBuf[stepOffset++];
#if defined(DEBUG_RUN)
                cout << hexB(tr.PT&255) << ' '
                     << hexB(tr.TR&255) << " | ";
#endif
                if (tr.PT < 0x80) {
                    tr.pattern.offset = getPattOffset(tr.PT);
                    tr.pattern.step = 0;
                    tr.pattern.wait = 0;
                    tr.pattern.loops = -1;
                    tr.pattern.infiniteLoop = false;
                }
            }
#if defined(DEBUG_RUN)
            cout << endl;
#endif
        }
    }
    while ( sequencer.evalNext && --evalMaxLoops>0 );
}

void TFMXDecoder::trackCmd_Stop(udword stepOffset) {
    songEnd = true;
    triggerRestart = true;
}

void TFMXDecoder::trackCmd_Loop(udword stepOffset) {
#if defined(DEBUG_RUN)
    dumpBytes(pBuf,stepOffset,4);
#endif
    if (sequencer.loops == 0) {  // end of loop
        sequencer.loops = -1;    // unlock
        sequencer.step.current++;
    }
    else if (sequencer.loops < 0) {  // unlocked? loop init permitted
        sequencer.step.current = readBEuword(pBuf,stepOffset);
        sequencer.loops = (sword)readBEuword(pBuf,stepOffset+2) -1;
        
        if ( (sequencer.step.current > (TRACK_STEPS_MAX-1)) ||  // fubar then
             (sequencer.step.current > sequencer.step.last) ) {  // bad loop
            songEnd = true;
            triggerRestart = true;
        }
        // Starting a loop with a negative count would be infinite.
        else if (sequencer.stepSeenBefore[sequencer.step.current] && sequencer.loops < 0) {
            songEnd = true;
        }
        // Limit number of loops. Only "Ramses" title sets 0xf00, and "mdat.cyberzerk-ingame" subsongs
        // set a number of 0x2e00 loops.
        if (sequencer.loops == 0xeff || sequencer.loops > 0x100) {
            sequencer.loops = 0;
        }
    }
    else {
        sequencer.loops--;
        sequencer.step.current = readBEuword(pBuf,stepOffset);
    }
    sequencer.evalNext = true;
}

void TFMXDecoder::trackCmd_Speed(udword stepOffset) {
#if defined(DEBUG_RUN)
    dumpBytes(pBuf,stepOffset,4);
#endif
    admin.speed = admin.count = readBEuword(pBuf,stepOffset);
    // Ignore negative values like 0xff00.
    uword arg2 = 0x81ff & readBEuword(pBuf,stepOffset+2);
    if ( (arg2 != 0) && (arg2 < 0x200) ) {
        if (arg2 < 32) {
            arg2 = 125;
        }
        setRate(arg2);
    }
    sequencer.step.current++;
    sequencer.evalNext = true;
}

void TFMXDecoder::trackCmd_Fade(udword stepOffset) {
#if defined(DEBUG_RUN)
    dumpBytes(pBuf,stepOffset,4);
#endif
    fadeInit(pBuf[stepOffset+3],pBuf[stepOffset+1]);
    sequencer.step.current++;
    sequencer.evalNext = true;
}

void TFMXDecoder::trackCmd_7V(udword stepOffset) {
#if defined(DEBUG_RUN)
    dumpBytes(pBuf,stepOffset,4);
#endif
    //sword arg1 = (sword)readBEuword(pBuf,stepOffset);
    sword arg2 = (sword)readBEuword(pBuf,stepOffset+2);
    if (arg2 >= 0) {
        sbyte x = (sbyte)pBuf[stepOffset+3];
        if (x < -0x20) {
            x = -0x20;
        }
        setBPM( (125*100)/(100+x) );
    }
    sequencer.step.current++;
    sequencer.evalNext = true;
}

}  // namespace
