// Simple AMIGA Paula Audio channel mixer -- Copyright (C) Michael Schwendt

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef LAMEPAULAMIXER_H
#define LAMEPAULAMIXER_H

#include "Paula.h"
class LamePaulaVoice;

// We feed audio output "channels" with data from input "voices".
// So, Paula's channels are called voices within the mixer code.

class LamePaulaMixer : public PaulaMixer
{
 public:
    LamePaulaMixer();
    ~LamePaulaMixer();
    void init(udword freq, ubyte bits, ubyte channels, uword zero, ubyte panning);
    void init(ubyte voices);
    PaulaVoice* getVoice(ubyte);

    void setPanning(int);
    
    void mute(ubyte voice, bool);
    bool isMuted(ubyte voice);

    void enableNtsc(bool isEnabled) { *(udword*)&AMIGA_CLOCK = isEnabled ? AMIGA_CLOCK_NTSC : AMIGA_CLOCK_PAL; basePeriod = (float)AMIGA_CLOCK / pcmFreq; } // rePlayer

    udword fillBuffer(void* buffer, udword bufferLen, PaulaPlayer *player); // rePlayer
    void drain();

 protected:
    void initVoice(ubyte num);
    void initMixTables();
    void end();
    void updatePeriods();
    void updateRate(ubyte);
    void updateVoiceVolume();
    ubyte getSample_7V();

    void* (LamePaulaMixer::*mFillFunc)(void*, udword);

    void* fill8bitMono(void*, udword);
    void* fill8bitStereoPanning(void*, udword);
    void* fill16bitMono(void*, udword);
    void* fill16bitStereoPanning(void*, udword);

    static const ubyte MAX_VOICES = 7;  // can be increased, though
    
    LamePaulaVoice* pVoice[MAX_VOICES];
    ubyte voiceVol[MAX_VOICES];
    bool muted[MAX_VOICES];

    ubyte voices, channels;
    int panning;
    
    // Mapping of input voices to stereo output channels.
    enum VoicePos {
        LEFT,
        RIGHT
    };
    // Paula voices 0 and 3 are connected to left output, 1 and 2 to right output.
    VoicePos panningPos[MAX_VOICES] = {
        LEFT, RIGHT, RIGHT, LEFT,
        LEFT, LEFT, LEFT
    };

    udword pcmFreq;
    ubyte bitsPerSample;

    static const udword AMIGA_CLOCK_PAL = 3546895;
    static const udword AMIGA_CLOCK_NTSC = 3579546;
    const udword AMIGA_CLOCK;

    ubyte playerRate;  // we receive this from the player

    sbyte mix8vol[(64+1)*256];
    ubyte clipping4[4*256];

    sbyte mix8mono[(64+1)*256];
    sbyte mix8right[(64+1)*256];
    sbyte mix8left[(64+1)*256];
    
    sword mix16mono[(64+1)*256];
    sword mix16right[(64+1)*256];
    sword mix16left[(64+1)*256];

    ubyte zero8bit;   // ``zero''-sample
    uword zero16bit;  // either signed or unsigned
    
    ubyte bufferScale;
    
    udword samplesAdd;
    udword samplesPnt;
    uword samples, samplesOrg;

    udword toFill;
    
    ubyte emptySample;

    float basePeriod;
};

#endif  // LAMEPAULAMIXER_H
