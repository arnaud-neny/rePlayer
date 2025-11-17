// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef PAULAVOICE_H
#define PAULAVOICE_H

#include "MyTypes.h"

namespace tfmxaudiodecoder {

class PaulaVoice {
 public:
    // to simulate basic access to Paula registers
    struct _paula {
        const ubyte* start;  // start address
        uword length;        // length as number of 16-bit words
        uword period;        // clock/frequency
        uword volume;        // 0-64
    } paula;

    static const uword VOLUME_MAX = 64;

    virtual ~PaulaVoice() { };
    virtual void on();
    virtual void off();
    virtual void takeNextBuf();   // take parameters from paula.* (or just to repeat.*)
    virtual uword getLoopCount();
};

}  // namespace

#endif  // PAULAVOICE_H
