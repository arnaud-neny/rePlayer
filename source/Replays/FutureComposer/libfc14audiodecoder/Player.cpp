// Future Composer & Hippel player library
// by Michael Schwendt

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

#include <cstring>
#ifdef FC_HAVE_NOTHROW
#include <new>
#endif
#include <algorithm>

#include "FC.h"
#include "Analyze.h"
#include "Debug.h"
//#include "Paula.h"
class PaulaMixer;

const std::string FC::UNKNOWN_FORMAT_ID = "???";

FC::FC() {
    analyze = new Analyze;
    
    input = 0;
    inputBufLen = inputLen = 0;

    pMixer = 0;
    
    admin.initialized = false;
    admin.isEnabled = false;
    
    loopMode = false;
    endShortsMode = false;
    endShortsDuration = 10*1000;

    clearFormat();
 
    // Set up some dummy voices to decouple the decoder from the mixer.
    for (ubyte v=0; v<VOICES_MAX; v++) {
        voiceVars[v].ch = &dummyVoices[v];
    }
}

FC::~FC() {
    delete analyze;
    
    delete[] input;
    inputLen = 0;
}

void FC::clearFormat() {
    memset(&stats,0,sizeof(stats));
    
    songEnd = false;
    songPosCurrent = 0;
    duration = 0;

    pInitFunc = 0;
    
    formatID = UNKNOWN_FORMAT_ID;
    formatName = UNKNOWN_FORMAT_ID;

    // Important: Reset all traits to (for booleans it's "false").
    traits.compressed =
        traits.vibScaling =
        traits.vibSpeedFlag =
        traits.sndSeqGoto =
        traits.sndSeqToTrans =
        traits.skipToWaveMod =
        traits.checkSSMPtag =
        traits.foundSSMP =
        traits.volSeqSnd80 =
        traits.volSeqEA =
        traits.randomVolume =
        traits.voiceVolume =
        traits.portaIsBit6 =
        traits.porta40SetSnd =
        traits.porta80SetSnd =
        traits.isSMOD = false;
    traits.periodMin = FC14_PERIOD_MIN;
    traits.periodMax = FC14_PERIOD_MAX;

    // Default sound modulation sequence commands setup.
    ModFuncPtr defaultSndModFuncs[(0xea-0xe0)+1] = {
        &FC::TFMX_sndSeq_E0,
        &FC::TFMX_sndSeq_E1,
        &FC::TFMX_sndSeq_E2,
        &FC::TFMX_sndSeq_E3,
        &FC::TFMX_sndSeq_E4,
        &FC::TFMX_sndSeq_E5,
        &FC::TFMX_sndSeq_E6,
        &FC::TFMX_sndSeq_E7,
        &FC::TFMX_sndSeq_E8,
        &FC::TFMX_sndSeq_E9,
        &FC::TFMX_sndSeq_EA_skip
    };
    std::copy(std::begin(defaultSndModFuncs),std::end(defaultSndModFuncs),std::begin(TFMX_sndModFuncs));
}

void FC::off() {
    admin.isEnabled = false;

    for (ubyte v=0; v<stats.voices; v++) {
        voiceVars[v].ch->off();
    }
    // (AMIGA) Power-LED on = low-pass filter on.
    // May be simulated by external audio post-processing.
}

void FC::killChannel(VoiceVars& voiceX) {
    voiceX.ch->off();
    voiceX.ch->paula.start = fcBuf.tellBegin()+offsets.silence+1;
    // (NOTE) Some implementations set this to 0x0100.
    voiceX.ch->paula.length = 1;
    voiceX.ch->takeNextBuf();
}

// The interface to a cheap Paula simulator/mixer.
void FC::setMixer(PaulaMixer* mixer) {
    pMixer = mixer;
}

// Create needed number of voices and replace the dummies.
void FC::mixerInit() {
    if ( !pMixer) {
        return;
    }
    pMixer->init(stats.voices);
    for (ubyte v=0; v<stats.voices; v++) {
        voiceVars[v].ch = pMixer->getVoice(v);
        killChannel(voiceVars[v]);
    }
    //pMixer->mute(0,true);
    //pMixer->mute(1,true);
    //pMixer->mute(2,true);
    //pMixer->mute(3,true);
    //pMixer->mute(4,true);
    //pMixer->mute(5,true);
    //pMixer->mute(6,true);
}

udword FC::getDuration() {
    return duration;
}

bool FC::getSongEndFlag() {
    return songEnd;
}

void FC::endShorts(int flag, int maxSecs) {
    endShortsMode = (flag==1);
    endShortsDuration = maxSecs*1000;
}

void FC::setLoopMode(int flag) {
    loopMode = (flag==1);
}

const char* FC::getFormatID() {
    return formatID.c_str();
}

const char* FC::getFormatName() {
    return formatName.c_str();
}

bool FC::init(int songNumber) {
    if (!admin.initialized)
        return false;
    return init(input,inputLen,songNumber);
}

bool FC::init(void *data, udword length, int songNumber) {
    songNumber = (songNumber < 0) ? 0 : songNumber;
    
    clearFormat();
    
    if ( !isOurData(data,length) || offsets.header==0xffffffff) {
        return false;
    }

    if (data != input) {
        // If we still have a sufficiently large buffer, reuse it.
        udword newLen = length+sizeof(silenceData);
        if (newLen > inputBufLen) {
            delete[] input;
            inputBufLen = 0;
#ifdef FC_HAVE_NOTHROW
            if ( (input = new(std::nothrow) ubyte[newLen]) == 0 )
#else
            if ( (input = new ubyte[newLen]) == 0 )
#endif
            {
                return false;
            }
        }
        memcpy(input,data,length);
        inputBufLen = newLen;
        inputLen = length;
        
        // Set up smart pointers for signed and unsigned input buffer access.
        // Ought to be read-only (const), but this implementation appends
        // a few values to the end of the buffer (see further below) and
        // may repair some data, too.
        fcBufS.setBuffer((sbyte*)input,inputBufLen);
        fcBuf.setBuffer((ubyte*)input,inputBufLen);
        
        // (NOTE) This next bit is just for convenience.
        //
        // Copy ``silent'' modulation sequence to end of FC module buffer so it
        // is in the address space of the FC module and thus allows using the
        // same smart pointers as throughout the rest of the code.
        offsets.silence = inputLen;
        for (ubyte i=0; i<sizeof(silenceData); i++) {
            fcBuf[offsets.silence+i] = silenceData[i];
        }
        
        // A short silent sample to use,
        // if sample definitions don't pass buffer range checks.
        silentSample.pStart = fcBuf.tellBegin()+offsets.silence+1;
        silentSample.startOffs = silentSample.repOffs = 0;
        silentSample.len = silentSample.repLen = 1;
        
        for (int s=0; s<SAMPLES_MAX; s++) {
            samples[s] = silentSample;
        }
        
        // (NOTE) Instead of addresses (pointers) use 32-bit offsets everywhere.
        // This is just for added safety and convenience. One could range-check
        // each pointer/offset where appropriate to avoid segmentation faults
        // caused by damaged input data.
        //
        // Smart pointer usage avoids the problem of out-of-bounds access.
        //
        // On the topic of rejecting damaged/corrupted input data, over many
        // years of browsing MOD collections containing Future Composer modules,
        // no extremely corrupted modules have been found. Those with bad header
        // data or truncated sample data have been replaced with the help of
        // a module collection's maintainers. Bad/broken modules may still be
        // encountered "in the wild" and as copies in private collections.
        // So, if not implementing pedantic validation of all input data (such
        // as via a dry-run through the player and watching out for problematic
        // data), only checking that some offsets are within the module boundaries
        // is not worthwhile. A broken module may sound broken in unknown ways.
    }

    analyze->clear();
    // Main initialization.
    if ( pInitFunc && !(this->*pInitFunc)(songNumber) ) {
        return false;
    }
    admin.initialized = true;
    // Determine duration with a dry-run till song-end.
    duration = 0;
    bool loopModeBak = loopMode;
    loopMode = false;
    restart();
    do {
        duration += run();
    } while ( !songEnd && (duration<1000*60*59));
    loopMode = loopModeBak;

    analyze->dump();
    if (analyze->usesE7setDiffWave(this) ) {
        TFMX_sndModFuncs[7] = &FC::TFMX_sndSeq_E7_setDiffWave;
    }
    if (analyze->usesNegVibSpeed() ) {
        traits.vibSpeedFlag = true;
    }
    if (analyze->usesSndSeq(0x80) ) {
        traits.volSeqSnd80 = true;
    }
    TraitsByChecksum();
    restart();

#if defined(DEBUG)
    dumpModule();
#endif
    return true;
}

// --------------------------------------------------------------------------

bool FC::restart() {
    if (!admin.initialized) {
        return false;
    }

    off();
    mixerInit();
    tickFPadd = 0;
    songEnd = false;
    songPosCurrent = 0;
    
    for (ubyte v=0; v<stats.voices; v++) {
        voiceVars[v].voiceNum = v;
        voiceVars[v].trackPos =
            voiceVars[v].pattPos = 0;
        voiceVars[v].volSlideSpeed =
            voiceVars[v].volSlideTime =
            voiceVars[v].volSlideDelayFlag =
            voiceVars[v].volSustainTime = 0;
        voiceVars[v].envelopeCount =
            voiceVars[v].envelopeSpeed = 1;
        voiceVars[v].vibSpeed =
            voiceVars[v].vibDelay =
            voiceVars[v].vibAmpl =
            voiceVars[v].vibCurOffs =
            voiceVars[v].vibFlag = 0;
        voiceVars[v].pitchBendSpeed = 
            voiceVars[v].pitchBendTime =
            voiceVars[v].pitchBendDelayFlag = 0;
        voiceVars[v].transpose =
            voiceVars[v].soundTranspose =
            voiceVars[v].seqTranspose = 0;
            voiceVars[v].portaSpeed =
            voiceVars[v].portaOffs =
            voiceVars[v].portaDelayFlag = 0;
        voiceVars[v].volSeq = offsets.silence;
        voiceVars[v].sndSeq = offsets.silence+1;
        voiceVars[v].volSeqPos =
            voiceVars[v].currentVolSeq =
            voiceVars[v].currentSndSeq =
            voiceVars[v].sndSeqPos = 0;
        voiceVars[v].sndModSustainTime = 0;
        voiceVars[v].pattVal1 =
            voiceVars[v].pattVal2 = 0;
        voiceVars[v].outputPeriod = 
            voiceVars[v].outputVolume = 
            voiceVars[v].volume = 0;
        voiceVars[v].voiceVol = 100;
        voiceVars[v].volRandomThreshold =
            voiceVars[v].volRandomCurrent = 0;
        voiceVars[v].repeatDelay = 0;

        voiceVars[v].waveModSwitch1 = 0;
        voiceVars[v].currentWave = 0xff;
        voiceVars[v].pattCompress = voiceVars[v].pattCompressCount = 0;

        killChannel(voiceVars[v]);

        // Trigger evaluation of the track table step to start with.
        if (traits.compressed) {
            voiceVars[v].pattStart = 0xffffffff;
        }
        else {
            voiceVars[v].pattPos = traits.patternSize;
        }
    }

    // Copy track table first step, last step, song speed.
    (this->*pChooseSongFunc)(admin.startSong);
    setTrackRange();
    
    admin.speed = admin.startSpeed;
    if (admin.speed == 0) {
        admin.speed = 3;  // documented default
    }
    admin.count = 1; // quick start

    randomWord = 0;
    
    admin.isEnabled = true;
    if (endShortsMode && duration && (duration<endShortsDuration)) {
        songEnd = true;
    }
    else
        songEnd = false;
    
    return true;
}

int FC::getSongs() {
    return stats.songs;
}

void FC::setTrackRange(int startStep, int endStep) {
    if (startStep > 0)
        firstStep = startStep;
    if (endStep > 0)
        lastStep = endStep;

    for (ubyte v=0; v<stats.voices; v++) {
        // Track table start and end.
        if ( (firstStep>=0) && ((firstStep*trackStepLen)<trackTabLen) ) {
            voiceVars[v].trackStart = offsets.trackTable+(firstStep*trackStepLen);
            voiceVars[v].trackStart += v*trackColumnSize;
            voiceVars[v].trackPos = 0;
        }
        else {  // by default start at the very beginning
            voiceVars[v].trackStart = offsets.trackTable;
            voiceVars[v].trackStart += v*trackColumnSize;
            voiceVars[v].trackPos = 0;
        }
        // The subsong's track table end is at next step after end step.
        if ( (lastStep>=0) && (((lastStep+1)*trackStepLen)<=trackTabLen) ) {
            voiceVars[v].trackEnd = offsets.trackTable+(lastStep+1)*trackStepLen;
        }
        else {  // by default end at the very end
            voiceVars[v].trackEnd = offsets.trackTable+trackTabLen;
        }
    }
}

// --------------------------------------------------------------------------

int FC::run() {  // --> PaulaPlayer::run()
    if (!admin.isEnabled)  // on/off flag
        return 0;

    if ( !songEnd || loopMode ) {
        if (--admin.count == 0) {
            admin.count = admin.speed;  // reload

            // Prepare next note for each voice.
            for (ubyte v=0; v<stats.voices; v++) {
                (this->*pNextNoteFunc)(voiceVars[v]);
            }
#if defined(DEBUG2)
            cout << endl << flush;
#endif
        }  // songEnd flag may have changed after this
    }

    for (ubyte v=0; v<stats.voices; v++) {
        if ( !songEnd || loopMode ) {
            // Next function will will decide whether to turn audio channel on.
            voiceVars[v].trigger = false;
            
            // Start or update instrument.
            (this->*pSoundFunc)(voiceVars[v]);

            voiceVars[v].ch->paula.period = voiceVars[v].outputPeriod;
            voiceVars[v].ch->paula.volume = voiceVars[v].outputVolume;
        
            if (voiceVars[v].repeatDelay != 0) {
                if (--voiceVars[v].repeatDelay == 1) {
                    voiceVars[v].repeatDelay = 0;
                    voiceVars[v].ch->paula.start = voiceVars[v].pSampleStart +
                        voiceVars[v].repeatOffset;
                    voiceVars[v].ch->paula.length = voiceVars[v].repeatLength;
                    voiceVars[v].ch->takeNextBuf();
                }
            }
            // Enable channel? Else, do not touch it.
            if ( voiceVars[v].trigger ) {
                voiceVars[v].ch->on();
            }
        }
        else {  // cut off channel volume at song end
            voiceVars[v].ch->paula.volume = 0;
        }
    }
    // If all modules ran at 50 Hz, we could simply return 20 ms,
    // but the rate for some modules is different.
    tickFPadd += tickFP;
    int tick = tickFPadd>>8;
    tickFPadd &= 0xff;
    songPosCurrent += tick;
    return tick;  // in [ms]
}

void FC::setRate(ubyte f) {
    rate = f;
    tickFP = (1000<<8)/rate;
}
    
// --------------------------------------------------------------------------

void FC::Sample::assertBoundaries(smartPtr<ubyte>& pB) {
    udword bufLen = pB.tellLength()-8;  // FC::silenceData at the end!
    // Avoid potentially unprecise or corrupted sample buffer boundaries.
    if ( startOffs > bufLen-2 )
        startOffs = bufLen-2;
    if ( startOffs+len+len > bufLen )
        len = (bufLen-startOffs)>>1;
    if ( repOffs+startOffs > bufLen-2 )
        repOffs = (bufLen-2)-startOffs;
    if ( repOffs+startOffs+repLen+repLen > bufLen )
        repLen = (bufLen-(repOffs+startOffs))>>1;

    pStart = pB.tellBegin() + startOffs;
}

void FC::prepareChannelUpdate(VoiceVars& voiceX) {
    voiceX.ch->off();       // Disable channel right now.
    voiceX.trigger = true;  // Enable channel later.
}

// --------------------------------------------------------------------------

// Get offset to current pattern position.
udword FC::getPattOffs(VoiceVars& voiceX) {
    return voiceX.pattStart + voiceX.pattPos;
}

// (NOTE) Silent sound modulation sequence is different
// from silent instrument definition sequence.

// Return offset to volume modulation sequence (instrument definition).
udword FC::getVolSeq(ubyte seq) {
    if (seq > (stats.volSeqs-1)) {
        return offsets.silence;
    } else {
        if (traits.compressed)
            return offsets.header + readBEuword(fcBuf,offsets.volModSeqs+(seq<<1));
        else
            return offsets.volModSeqs+(seq<<6);
    }
}

// Return offset to sound modulation sequence.
udword FC::getSndSeq(ubyte seq) {
    if (seq > (stats.sndSeqs-1)) {
        return offsets.silence+1;
    } else {
        if (traits.compressed)
            return offsets.header + readBEuword(fcBuf,offsets.sndModSeqs+(seq<<1));
        else
            return offsets.sndModSeqs+(seq<<6);
    }
}

// This is not needed by the player, but a utility method
// to examine sound sequences including trimmed ones in COSO format.
udword FC::getVolSeqEnd(ubyte seq) {
    udword begin = getVolSeq(seq);
    udword end = getVolSeq(seq+1);
    if ( begin==offsets.silence ) {
        end = begin+8;
    }
    else if ( end==offsets.silence ) {
        end = offsets.patterns;
    }
    return end;
}

udword FC::getSndSeqEnd(ubyte seq) {
    udword begin = getSndSeq(seq);
    udword end = getSndSeq(seq+1);
    if ( begin==offsets.silence+1 ) {
        end = begin+7;
    }
    else if ( end==offsets.silence+1 ) {
        end = offsets.volModSeqs;
    }
    return end;
}
 
// --------------------------------------------------------------------------

bool FC::havePattern(int n, const ubyte (&pattWanted)[PATTERN_LENGTH]) {
    udword pattStart = offsets.patterns+(n*traits.patternSize);
    if ( (n<stats.patterns) && (pattStart+n) < inputLen ) {
        return ( memcmp(input+pattStart,pattWanted,traits.patternSize) == 0 );
    }
    return false;
}

void FC::replacePattern(int n, const ubyte (&pattNew)[PATTERN_LENGTH]) {
    udword pattStart = offsets.patterns+(n*traits.patternSize);
    memcpy(input+pattStart,pattNew,traits.patternSize);
}

bool FC::copyStats(struct fc14dec_mod_stats* targetStats) {
    if (admin.initialized) {
        *targetStats = stats;
        return true;
    }
    return false;
}

uword FC::getSampleLength(uword num) {
    if (num < stats.samples)
        return samples[num].len*2;
    else
        return 0;
}

uword FC::getSampleRepOffset(uword num) {
    if (num < stats.samples)
        return samples[num].repOffs;
    else
        return 0;
}

uword FC::getSampleRepLength(uword num) {
    if (num < stats.samples)
        return samples[num].repLen*2;
    else
        return 0;
}

// --------------------------------------------------------------------------

const ubyte FC::silenceData[8] = {
    // Used as ``silent'' instrument definition.
    // seqSpeed, sndSeq, vibSpeed, vibAmp, vibDelay, initial Volume, ...
    //
    // Silent volume sequence starts here.
    0x01,
    // Silent modulation sequence starts here.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE1
};

const uword FC::periods[0x80] = {
    0x06b0, 0x0650, 0x05f4, 0x05a0, 0x054c, 0x0500, 0x04b8, 0x0474,
    0x0434, 0x03f8, 0x03c0, 0x038a, 
    // +0x0c (*2 = byte-offset)
    0x0358, 0x0328, 0x02fa, 0x02d0, 0x02a6, 0x0280, 0x025c, 0x023a, 
    0x021a, 0x01fc, 0x01e0, 0x01c5,
    // +0x18 (*2 = byte-offset)
    0x01ac, 0x0194, 0x017d, 0x0168, 0x0153, 0x0140, 0x012e, 0x011d,
    0x010d, 0x00fe, 0x00f0, 0x00e2, 
    // +0x24 (*2 = byte-offset)
    0x00d6, 0x00ca, 0x00be, 0x00b4, 0x00aa, 0x00a0, 0x0097, 0x008f,
    0x0087, 0x007f, 0x0078, 0x0071,
    // +0x30 (*2 = byte-offset)
    0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071,
    0x0071, 0x0071, 0x0071, 0x0071, 
    // +0x3c (*2 = byte-offset)
    0x0d60, 0x0ca0, 0x0be8, 0x0b40, 0x0a98, 0x0a00, 0x0970, 0x08e8,
    0x0868, 0x07f0, 0x0780, 0x0714,
    // +0x48 (*2 = byte-offset)
    //
    // (NOTE) 0x49 = PATTERN BREAK, so the extra low octave would be
    // useless for direct access. Transpose values would be required.
    // However, the FC 1.4 player still has hardcoded 0x0d60 as the lowest
    // sample period and does a range-check prior to writing a period to
    // the AMIGA custom chip. In short: This octave is useless!  Plus:
    // Since some music modules access the periods at offset 0x54 via
    // transpose, the useless octave cause breakage.
    // 0x1ac0, 0x1940, 0x17d0, 0x1680, 0x1530, 0x1400, 0x12e0, 0x11d0,
    // 0x10d0, 0x0fe0, 0x0f00, 0x0e28,
    //    
    // End of Future Composer 1.0 - 1.3 period table.
    //
    // End of TFMX/COSO period table. There may be some more 0x0071
    // values at the end, but not enough to cover also array index
    // up to and including "note value && 0x7f".
    0x06b0, 0x0650, 0x05f4, 0x05a0, 0x054c, 0x0500, 0x04b8, 0x0474,
    0x0434, 0x03f8, 0x03c0, 0x038a,
    // +0x54 (*2 = byte-offset)
    0x0358, 0x0328, 0x02fa, 0x02d0, 0x02a6, 0x0280, 0x025c, 0x023a,
    0x021a, 0x01fc, 0x01e0, 0x01c5,
    // +0x60 (*2 = byte-offset)
    0x01ac, 0x0194, 0x017d, 0x0168, 0x0153, 0x0140, 0x012e, 0x011d,
    0x010d, 0x00fe, 0x00f0, 0x00e2,
    // +0x6c (*2 = byte-offset)
    0x00d6, 0x00ca, 0x00be, 0x00b4, 0x00aa, 0x00a0, 0x0097, 0x008f,
    0x0087, 0x007f, 0x0078, 0x0071,
    // +0x78 (*2 = byte-offset)
    0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071
    // +0x80 (*2 = byte-offset), everything from here on is unreachable
    // due to 0x7f AND.
};
