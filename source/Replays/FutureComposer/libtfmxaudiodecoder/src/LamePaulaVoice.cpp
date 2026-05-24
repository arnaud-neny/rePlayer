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
}

LamePaulaVoice::~LamePaulaVoice() {
    LamePaulaVoice::off();
}

void LamePaulaVoice::off() {
    isOn = false;
    loopCount = 0;
}

void LamePaulaVoice::on() {
    takeNextBuf();
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
    stepSpeedAddPnt += stepSpeedPnt;
    start += ( stepSpeed + ( stepSpeedAddPnt > 65535 ) );
    stepSpeedAddPnt &= 65535;

    if ( start >= end && looping ) {
        start = repeatStart + ( stepSpeed + ( stepSpeedAddPnt > 65535 ) );
        end = repeatEnd;
        loopCount++;
    }
    if ( start < end ) {
        lastSample = *start;
    }
    return lastSample;
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
