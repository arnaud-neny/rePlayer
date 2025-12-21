// Simple AMIGA Paula Audio channel mixer -- Copyright (C) Michael Schwendt

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include "LamePaulaVoice.h"

namespace tfmxaudiodecoder {

LamePaulaVoice::LamePaulaVoice() {
    looping = true;
    LamePaulaVoice::off();
    lastSample = 0;
    smoothUp = false;
    smoothCount = 0;
}

LamePaulaVoice::~LamePaulaVoice() {
    LamePaulaVoice::off();
}

void LamePaulaVoice::off() {
    isOn = false;
    paula.period = 0;
    paula.volume = 0;
    loopCount = 0;
}

void LamePaulaVoice::on() {
    takeNextBuf();
    if (!isOn) {
        if (start) {
            if ((sbyte)*start < lastSample ) {
                smoothUp = false;
                smoothCount = (lastSample-*start)/4;
            }
            else {
                smoothUp = true;
                smoothCount = (*start-lastSample)/4;
            }
        }
    }
    isOn = true;
}

void LamePaulaVoice::takeNextBuf() {
    if (!isOn) {
        loopCount = 0;
        // If channel is off, take sample START parameters.
        start = paula.start;
        length = paula.length;
        length <<= 1;
        if (length == 0) {  // Paula would play $FFFF words (!)
            length = 1;
        }
        end = start+length;
    }
    repeatStart = paula.start;
    repeatLength = paula.length;
    repeatLength <<= 1;
    if (repeatLength == 0) {  // Paula would play $FFFF words (!)
        repeatLength = 1;
    }
    repeatEnd = repeatStart+repeatLength;
}

ubyte LamePaulaVoice::getSample() {
    if (!isOn) {
        lastSample = 0;
        return 0;
    }
    if (smoothCount > 0) {
        sbyte s;
        if (smoothUp) {
            s = lastSample + 3;
            if (s >= (sbyte)*start) {
                smoothCount = 0;
                s = *start;
            }
        }
        else {
            s = lastSample - 3;
            if (s <= (sbyte)*start) {
                smoothCount = 0;
                s = *start;
            }
        }
        lastSample = s;
        return s;
    }
    stepSpeedAddPnt += stepSpeedPnt;
    start += ( stepSpeed + ( stepSpeedAddPnt > 65535 ) );
    stepSpeedAddPnt &= 65535;

    if ( start >= end && looping ) {
        start = repeatStart;
        end = repeatEnd;
        loopCount++;
    }
    if ( start < end ) {
        lastSample = *start;
        return lastSample;
    }
    else {
        return 0;
    }
}

uword LamePaulaVoice::getLoopCount() {
    return loopCount;
}

void LamePaulaVoice::drain() {
    bool turnOn = isOn;
    off();
    takeNextBuf();
    if (turnOn) {
        on();
    }
}

}  // namespace
