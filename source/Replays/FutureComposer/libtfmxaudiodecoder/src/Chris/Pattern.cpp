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

namespace tfmxaudiodecoder {

udword TFMXDecoder::getPattOffset(ubyte pt) {
    // With this TFMX format it's always an array of offsets to patterns.
    return offsets.header + readBEudword(pBuf,offsets.patterns+(pt<<2));
}

void TFMXDecoder::processPattern(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "  processPattern() at 0x" << tohex(tr.pattern.offset) << endl;
#endif
    int evalMaxLoops = RECURSE_LIMIT;  // NB! Around 8 would suffice.
    do {
        tr.pattern.evalNext = false;
#if defined(DEBUG_RUN)
        cout << "  " << hexW(tr.pattern.step) << ":";
#endif
        // Offset to current step position within pattern.
        udword p = tr.pattern.offset+(tr.pattern.step<<2);
        // Fetch pattern entry, four bytes aka 'aabbcdee'.
        cmd.aa = pBuf[p];
        cmd.bb = pBuf[p+1];
        cmd.cd = pBuf[p+2];
        cmd.ee = pBuf[p+3];
#if defined(DEBUG_RUN)
        cout << "  " << hexB(cmd.aa) << hexB(cmd.bb) << hexB(cmd.cd) << hexB(cmd.ee) << "  ";
#endif
        ubyte aaBak = cmd.aa;
        if (cmd.aa < 0xf0) {  // >= 0xf0 pattern state command
            if (cmd.aa < 0xc0 && cmd.aa >= 0x7f) {  // note + wait (instead of detune)
                tr.pattern.wait = cmd.ee;
                cmd.ee = 0;
            }
            cmd.aa += tr.TR;
            if (aaBak < 0xc0) {
                cmd.aa &= 0x3f;
            }
            if ( tr.on ) {
                noteCmd();
            }
            if (aaBak >= 0xc0 || aaBak < 0x7f) {
                tr.pattern.step++;
                tr.pattern.evalNext = true;
            }
            else {
                tr.pattern.step++;
            }
#if defined(DEBUG_RUN)
            cout << endl;
#endif
        }
        else {  // cmd.aa >= 0xf0   pattern state command
            ubyte command = cmd.aa & 0xf;
            patternCmdUsed[command] = true;
            (this->*PattCmdFuncs[command])(tr);
        }
    }
    while ( tr.pattern.evalNext && --evalMaxLoops>0 );
}

void TFMXDecoder::pattCmd_NOP(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "NOP >>>>>>>>" << endl;
#endif
    tr.pattern.step++;
    tr.pattern.evalNext = true;
}

void TFMXDecoder::pattCmd_End(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "End >>>>>>>>--Next track  step--" << endl;
#endif
    tr.PT = 0xff;
    if (sequencer.step.current == sequencer.step.last) {
        songEnd = true;
        triggerRestart = true;
        return;
    }
    else {
        sequencer.step.current++;
    }
    processTrackStep();
    sequencer.step.next = true;
}

void TFMXDecoder::pattCmd_Loop(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "Loop>>>>>>>>[count     / step.w]" << endl;
#endif
    if (tr.pattern.loops == 0) {  // end of loop
        tr.pattern.loops = -1;
        tr.pattern.step++;
        tr.pattern.evalNext = true;
        return;
    }
    else if (tr.pattern.loops == -1) {  // init permitted
        tr.pattern.loops = cmd.bb-1;

        // This would be an infinite loop that potentially affects
        // song-end detection, if all tracks loop endlessly.
        // So, let's evaluate that elsewhere.
        if (tr.pattern.loops == -1) {  // infinite loop
            tr.pattern.infiniteLoop = true;
        }
    }
    else {
        tr.pattern.loops--;
    }
    tr.pattern.step = makeWord(cmd.cd,cmd.ee);
    tr.pattern.evalNext = true;
}

void TFMXDecoder::pattCmd_Goto(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "Cont>>>>>>>>[patternno./ step.w]" << endl;
#endif
    tr.PT = cmd.bb;
    tr.pattern.offset = getPattOffset(tr.PT);
    tr.pattern.step = makeWord(cmd.cd,cmd.ee);
    tr.pattern.evalNext = true;
}

void TFMXDecoder::pattCmd_Wait(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "Wait>>>>>>>>[count 00-FF--------" << endl;
#endif
    tr.pattern.wait = cmd.bb;
    tr.pattern.step++;
}

void TFMXDecoder::pattCmd_Stop(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "Stop>>>>>>>>--Stop this pattern-" << endl;
#endif
    tr.PT = 0xff;
}

void TFMXDecoder::pattCmd_Note(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "Note>>>" << endl;
#endif
    noteCmd();
    tr.pattern.step++;
    tr.pattern.evalNext = true;
}

void TFMXDecoder::pattCmd_SaveAndGoto(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "GsPt>>>>>>>>[patternno./ step.w]" << endl;
#endif
    tr.pattern.offsetSaved = tr.pattern.offset;
    tr.pattern.stepSaved = tr.pattern.step;
    pattCmd_Goto(tr);
}

void TFMXDecoder::pattCmd_ReturnFromGoto(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "RoPt>>>>>>>>-Return old pattern-" << endl;
#endif
    tr.pattern.offset = tr.pattern.offsetSaved;
    tr.pattern.step = tr.pattern.stepSaved;
    tr.pattern.step++;
    tr.pattern.evalNext = true;
}

void TFMXDecoder::pattCmd_Fade(Track& tr) {
    fadeInit(cmd.ee,cmd.bb);
    tr.pattern.step++;
    tr.pattern.evalNext = true;
}

}  // namespace
