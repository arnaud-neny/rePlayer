// Simple AMIGA Paula Audio channel mixer -- Copyright (C) Michael Schwendt

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
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "LamePaulaMixer.h"
#include "LamePaulaVoice.h"

// Simple AMIGA Paula Audio channel mixer with a few enhancements like
// mono output, customizable stereo panning and TFMX "7 Voices" mode.

LamePaulaMixer::LamePaulaMixer()
    : AMIGA_CLOCK(AMIGA_CLOCK_PAL)
{
    // Start with no voice ptrs.
    voices = 0;
    for (ubyte v=0; v<MAX_VOICES; v++) {
        pVoice[v] = 0;
        muted[v] = false;
        voiceVol[v] = 0;
    }

    panning = 50+25;

    // Fill clipping table needed for the four virtual voices.
    for (int i=0; i<0x180; i++) {
        clipping4[i] = 0x80;
        clipping4[i+0x180+0x100] = 0x7f;
    }
    for (int i=0; i<0x100; i++) {
        clipping4[i+0x180] = -128+i;
    }

    // Enforce initialization to some defaults here as to avoid that
    // mixer is used completely uninitialized.
    init(4);
    init(44100,16,2,0,panning);
}

LamePaulaMixer::~LamePaulaMixer() {
    end();
}

void LamePaulaMixer::end() {
    for (ubyte v=0; v<voices; v++) {
        delete pVoice[v];
        pVoice[v] = 0;
        muted[v] = false;
    }
    voices = 0;
}

// Force Paula's voices to enter repeat range.
// Used when seeking quickly without a mixer processing Paula.
void LamePaulaMixer::drain() {
    for (ubyte v=0; v<voices; v++) {
        pVoice[v]->off();
        pVoice[v]->takeNextBuf();
        pVoice[v]->on();
    }
}

void LamePaulaMixer::mute(ubyte v, bool mute) {
    if (v < MAX_VOICES) {
        muted[v] = mute;
    }
    updateVoiceVolume();
}

bool LamePaulaMixer::isMuted(ubyte v) {
    if (v < MAX_VOICES) {
        return muted[v];
    }
    else {
        return false;
    }
}

PaulaVoice* LamePaulaMixer::getVoice(ubyte v) {
    if (v < MAX_VOICES) {
        return pVoice[v];
    }
    else {
        return 0;
    }
}

void LamePaulaMixer::setPanning(int p) {
    if (p < 0)
        p = 0;
    if (p > 100)
        p = 100;
    panning = p;
}

void LamePaulaMixer::init(ubyte vnew) {
    if ((vnew <= MAX_VOICES) && (vnew != voices)) {
        end();
        voices = vnew;
        for (ubyte v=0; v<voices; v++) {
            pVoice[v] = new LamePaulaVoice;
        }
    }
    // not used
    //for (ubyte v=0; v<voices; v++) {
    //    initVoice(v);
    //    panningPos[v] = (v&1) ? LEFT : RIGHT;
    //}
}

void LamePaulaMixer::initVoice(ubyte v) {
    LamePaulaVoice* pv = pVoice[v];
    pv->start = &emptySample;
    pv->end = &emptySample+1;
    pv->repeatStart = &emptySample;
    pv->repeatEnd = &emptySample+1;
    pv->length = 1;
    pv->curPeriod = 0;
    pv->stepSpeed = 0;
    pv->stepSpeedPnt = 0;
    pv->stepSpeedAddPnt = 0;
    pv->off();

    muted[v] = false;
}

void LamePaulaMixer::init(udword freq, ubyte bits, ubyte chn, uword zero, ubyte panLevel) {
    pcmFreq = freq;
    bitsPerSample = bits;
    channels = chn;

    basePeriod = (float)AMIGA_CLOCK / pcmFreq;
    playerRate = 0;  // force update
    updateRate(50);
    bufferScale = 0;
    toFill = 0;
    
    if (bits == 8) {
        zero8bit = zero;
        if (channels == 1) {
            mFillFunc = &LamePaulaMixer::fill8bitMono;
        }
        else {  // if (channels == 2)
            mFillFunc = &LamePaulaMixer::fill8bitStereoPanning;
            ++bufferScale;
        }
    }
    else {  // if (bits == 16)
        zero16bit = zero;
        ++bufferScale;
        if (channels == 1) {
            mFillFunc = &LamePaulaMixer::fill16bitMono;
        }
        else {  // if (channels == 2)
            mFillFunc = &LamePaulaMixer::fill16bitStereoPanning;
            ++bufferScale;
        }
    }
    setPanning(panLevel);
    initMixTables();
}

void LamePaulaMixer::initMixTables() {
    // -50=left, 0=middle, +50=right
    // =>  0=left, 50=middle, 100=right
    float panLeft = (50-(panning-50))/100.0;
    float panRight = (50+(panning-50))/100.0;

    ubyte voicesPerChannel;
    if (voices == 7) {
        voicesPerChannel = 4/channels;
    }
    else {
        // Ensure even number of voices per output channel.
        voicesPerChannel = ((voices+1)&0xfe)/channels;
    }

    // Input samples: 8-bit signed 80,81,82,...,FE,FF,00,01,02,...,7E,7F
    // Array: 00->x, 01->x, ...,7E->x,7F->x,80->x,81->,82->,...,FE->x/,FF->x
    uword ui;
    long si;
    for (udword vol=0; vol<=PaulaVoice::VOLUME_MAX; vol++) {
        long spc;

        ui = (vol<<8);
        si = 0;
        while (si < 128) {
            mix8vol[ui] = (si*vol)/PaulaVoice::VOLUME_MAX;
            spc = ((si/voicesPerChannel)*vol)/PaulaVoice::VOLUME_MAX;
            mix8mono[ui] = (sbyte)spc;
            mix8right[ui] = (sbyte)(spc*panRight);
            mix8left[ui] = (sbyte)(spc*panLeft);
            ui++;
            si++;
        }
        si = -128;
        while (si < 0) {
            mix8vol[ui] = (si*vol)/PaulaVoice::VOLUME_MAX;
            spc = ((si/voicesPerChannel)*vol)/PaulaVoice::VOLUME_MAX;
            mix8mono[ui] = (sbyte)spc;
            mix8right[ui] = (sbyte)(spc*panRight);
            mix8left[ui] = (sbyte)(spc*panLeft);
            ui++;
            si++;
        }

        ui = (vol<<8);
        si = 0;
        while (si < 128*256) {
            spc = ((si/voicesPerChannel)*vol)/PaulaVoice::VOLUME_MAX;
            mix16mono[ui] = (sword)spc;
            mix16right[ui] = (sword)(spc*panRight);
            mix16left[ui] = (sword)(spc*panLeft);
            si += 256;
            ui++;
        }
        si = -128*256;
        while (si < 0) {
            spc = ((si/voicesPerChannel)*vol)/PaulaVoice::VOLUME_MAX;
            mix16mono[ui] = (sword)spc;
            mix16right[ui] = (sword)(spc*panRight);
            mix16left[ui] = (sword)(spc*panLeft);
            si += 256;
            ui++;
        }
    }
}

void LamePaulaMixer::updateRate(ubyte f) {
    if (playerRate == f)
        return;
    playerRate = f;
    if (f != 0) {
        samples = ( samplesOrg = pcmFreq / f );
        samplesPnt = (( pcmFreq % f ) * 65536 ) / f;
    }
    samplesAdd = 0;
}

void LamePaulaMixer::updatePeriods() {
    for (ubyte v=0; v<voices; v++) {
        LamePaulaVoice *pv = pVoice[v];
        if ( pv->paula.period != pv->curPeriod ) {
            pv->curPeriod = pv->paula.period;
            if (pv->curPeriod != 0) {
                float step = basePeriod / pv->curPeriod;
                pv->stepSpeed = step;
                pv->stepSpeedPnt = (step - pv->stepSpeed)*65536;
            }
            else {
                pv->stepSpeed = pv->stepSpeedPnt = 0;
            }
        }
    }
}

// We copy Paula voice volume here and also support muting individual voices.
void LamePaulaMixer::updateVoiceVolume() {
    for (ubyte v=0; v<voices; v++) {
        LamePaulaVoice *pv = pVoice[v];
        ubyte vol;
        if (muted[v]) {
            vol = 0;
        }
        else {
            vol = pv->paula.volume;
            if (vol > PaulaVoice::VOLUME_MAX) {
                vol = PaulaVoice::VOLUME_MAX;
            }
        }
        voiceVol[v] = vol;
    }
}

ubyte LamePaulaMixer::getSample_7V() {
    sword sam = mix8vol[(voiceVol[3]<<8)+pVoice[3]->getSample()];
    sam += mix8vol[(voiceVol[4]<<8)+pVoice[4]->getSample()];
    sam += mix8vol[(voiceVol[5]<<8)+pVoice[5]->getSample()];
    sam += mix8vol[(voiceVol[6]<<8)+pVoice[6]->getSample()];
    return clipping4[0x200+sam];
}

udword LamePaulaMixer::fillBuffer(void* buffer, udword bufferLen, PaulaPlayer *player) { // rePlayer
    // Both, 16-bit and stereo samples take more memory.
    // Hence fewer samples fit into the buffer.
    bufferLen >>= bufferScale;
    udword oldBufferLen = bufferLen; // rePlayer

    while ( bufferLen > 0 ) {
        if ( toFill > bufferLen ) {
            buffer = (this->*mFillFunc)(buffer, bufferLen);
            toFill -= bufferLen;
            bufferLen = 0;
        }
        else if ( toFill > 0 ) {
            buffer = (this->*mFillFunc)(buffer, toFill);
            bufferLen -= toFill;
            toFill = 0;   
        }
        else if ( toFill == 0 ) { // rePlayer
            if (player->clearEnd(oldBufferLen == bufferLen)) break; // rePlayer
            player->run();
            updatePeriods();
            updateVoiceVolume();
            
            updateRate( player->getRate() );
            udword temp = ( samplesAdd += samplesPnt );
            samplesAdd = temp & 0xFFFF;
            toFill = samples + ( temp > 65535 );
        }
    }  // while bufferLen
    return (oldBufferLen - bufferLen) << bufferScale; // rePlayer
}

void* LamePaulaMixer::fill8bitMono(void* buffer, udword numberOfSamples) {
    ubyte* buffer8bit = static_cast<ubyte*>(buffer);
    sbyte* mix = mix8mono;
    bool mode7V = false;
    int normalVoices = voices;

    if (voices == 7) {
        normalVoices = 3;
        mode7V = true;
    }
    for (ubyte v=0; v<normalVoices; v++) {
        buffer8bit = static_cast<ubyte*>(buffer);
        LamePaulaVoice *pv = pVoice[v];
        uword vol = voiceVol[v] << 8;
        for (udword n = numberOfSamples; n>0; n--) {
            if (v == 0) {
                *buffer8bit = zero8bit;
            }
            *buffer8bit += mix[vol+pv->getSample()];
            buffer8bit++;
        }
    }
    if (!mode7V)
        return(buffer8bit);

    buffer8bit = (static_cast<ubyte*>(buffer));
    for (udword n = numberOfSamples; n>0; n--) {
        *buffer8bit += mix[0x4000+getSample_7V()];
        buffer8bit++;
    }
    return(buffer8bit);
}

void* LamePaulaMixer::fill8bitStereoPanning( void* buffer, udword numberOfSamples ) {
    ubyte* buffer8bit = static_cast<ubyte*>(buffer);
    sbyte *mixLeft, *mixRight;
    bool mode7V = false;
    int normalVoices = voices;

    if (voices == 7) {
        normalVoices = 3;
        mode7V = true;
    }
    for (ubyte v=0; v<normalVoices; v++) {
        buffer8bit = (static_cast<ubyte*>(buffer));

        if (panningPos[v] == RIGHT) {
            mixLeft = mix8left;
            mixRight = mix8right;
        }
        else {  // LEFT
            mixLeft = mix8right;
            mixRight = mix8left;
        }
        
        LamePaulaVoice *pv = pVoice[v];
        uword vol = voiceVol[v] << 8;
        for (udword n = numberOfSamples; n>0; n--) {
            if (v == 0) {
                buffer8bit[0] = zero8bit;
                buffer8bit[1] = zero8bit;
            }
            ubyte sam = pv->getSample();
            buffer8bit[0] += mixLeft[vol+sam];
            buffer8bit[1] += mixRight[vol+sam];
            buffer8bit += 2;
        }
    }
    if (!mode7V)
        return(buffer8bit);

    buffer8bit = (static_cast<ubyte*>(buffer));
    int vph = 3;
    if (panningPos[vph] == RIGHT) {
        mixLeft = mix8left;
        mixRight = mix8right;
    }
    else {  // LEFT
        mixLeft = mix8right;
        mixRight = mix8left;
    }
    for (udword n = numberOfSamples; n>0; n--) {
        ubyte sam = getSample_7V();
        buffer8bit[0] += mixLeft[0x4000+sam];
        buffer8bit[1] += mixRight[0x4000+sam];
        buffer8bit += 2;
    }
    return(buffer8bit);

}

void* LamePaulaMixer::fill16bitMono( void* buffer, udword numberOfSamples ) {
    sword* buffer16bit = static_cast<sword*>(buffer);
    sword* mix = mix16mono;
    bool mode7V = false;
    int normalVoices = voices;

    if (voices == 7) {
        normalVoices = 3;
        mode7V = true;
    }
    for (ubyte v=0; v<normalVoices; v++) {
        buffer16bit = static_cast<sword*>(buffer);
        LamePaulaVoice *pv = pVoice[v];
        uword vol = voiceVol[v] << 8;
        for (udword n = numberOfSamples; n>0; n--) {
            if (v == 0) {
                *buffer16bit = zero16bit;
            }
            *buffer16bit += mix[vol+pv->getSample()];
            buffer16bit++;
        }
    }
    if (!mode7V)
        return(buffer16bit);

    buffer16bit = (static_cast<sword*>(buffer));
    for (udword n = numberOfSamples; n>0; n--) {
        *buffer16bit += mix[0x4000+getSample_7V()];
        buffer16bit++;
    }
    return(buffer16bit);
}

void* LamePaulaMixer::fill16bitStereoPanning( void *buffer, udword numberOfSamples ) {
    sword* buffer16bit = static_cast<sword*>(buffer);
    sword *mixLeft, *mixRight;
    bool mode7V = false;
    int normalVoices = voices;

    if (voices == 7) {
        normalVoices = 3;
        mode7V = true;
    }
    
    for (int v=0; v<normalVoices; v++ ) {
        buffer16bit = (static_cast<sword*>(buffer));

        if (panningPos[v] == RIGHT) {
            mixLeft = mix16left;
            mixRight = mix16right;
        }
        else {  // LEFT
            mixLeft = mix16right;
            mixRight = mix16left;
        }
        
        LamePaulaVoice *pv = pVoice[v];
        uword vol = voiceVol[v] << 8;
        for (udword n = numberOfSamples; n>0; n--) {
            if (v == 0) {
                buffer16bit[0] = zero16bit;
                buffer16bit[1] = zero16bit;
            }
            ubyte sam = pv->getSample();
            buffer16bit[0] += mixLeft[vol+sam];
            buffer16bit[1] += mixRight[vol+sam];
            buffer16bit += 2;
        }
    }
    if (!mode7V)
        return(buffer16bit);

    buffer16bit = (static_cast<sword*>(buffer));
    int vph = 3;
    if (panningPos[vph] == RIGHT) {
        mixLeft = mix16left;
        mixRight = mix16right;
    }
    else {  // LEFT
        mixLeft = mix16right;
        mixRight = mix16left;
    }
    for (udword n = numberOfSamples; n>0; n--) {
        ubyte sam = getSample_7V();
        buffer16bit[0] += mixLeft[0x4000+sam];
        buffer16bit[1] += mixRight[0x4000+sam];
        buffer16bit += 2;
    }
    return(buffer16bit);
}
