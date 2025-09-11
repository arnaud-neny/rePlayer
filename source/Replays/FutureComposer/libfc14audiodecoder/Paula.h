// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef PAULA_H
#define PAULA_H

#include "MyTypes.h"

class PaulaMixer;

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
};

class PaulaMixer {
 public:
    virtual ~PaulaMixer() { };
    virtual void init(ubyte voices);   // number of audio input sources
    virtual PaulaVoice* getVoice(ubyte);

    virtual ubyte playerRate(ubyte freq);  // set & get, 0 = default

    virtual void mute(ubyte voice, bool);
    virtual bool isMuted(ubyte voice);
};

class PaulaPlayer {
 public:
    virtual ~PaulaPlayer() { };
    virtual int run() = 0;
    virtual bool clearEnd(bool isCleared) = 0; // rePlayer
    ubyte getRate() {
        return rate;
    }
    
protected:
    ubyte rate;
};

#endif  // PAULA_H
