// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include "Paula.h"

void PaulaVoice::off() {
    // intentionally left blank
}

void PaulaVoice::on() {
    // intentionally left blank
}

void PaulaVoice::takeNextBuf() {
    // intentionally left blank
}

void PaulaMixer::init(ubyte) {
    // intentionally left blank
}

PaulaVoice* PaulaMixer::getVoice(ubyte) {
    // intentionally left blank
    return 0;
}

ubyte PaulaMixer::playerRate(ubyte) {
    // intentionally left blank
    return 0;
}

void PaulaMixer::mute(ubyte, bool) {
    // intentionally left blank
}

bool PaulaMixer::isMuted(ubyte) {
    // intentionally left blank
    return false;
}
