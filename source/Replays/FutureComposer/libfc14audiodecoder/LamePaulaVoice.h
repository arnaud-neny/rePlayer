// Simple AMIGA Paula Audio channel mixer -- Copyright (C) Michael Schwendt

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef LAMEPAULAVOICE_H
#define LAMEPAULAVOICE_H

#include "Paula.h"

class LamePaulaMixer;

class LamePaulaVoice : public PaulaVoice
{
 public:
    LamePaulaVoice();
    ~LamePaulaVoice();

    void on();
    void off();
    void takeNextBuf();    // take parameters from paula.* (or just to repeat.*)
    ubyte getSample();
    
    friend class LamePaulaMixer;

 protected:
    bool isOn;
    bool looping;  // whether to loop sample buffer continously (PAULA emu)
    
    const ubyte* start;
    const ubyte* end;
    udword length;
    
    const ubyte* repeatStart;
    const ubyte* repeatEnd;
    udword repeatLength;
    
    uword curPeriod;
    udword stepSpeed;
    udword stepSpeedPnt;
    udword stepSpeedAddPnt;
};

#endif  // LAMEPAULAVOICE_H
