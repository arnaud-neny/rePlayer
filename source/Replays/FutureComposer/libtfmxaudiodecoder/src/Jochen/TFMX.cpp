// Future Composer & Hippel player library
// by Michael Schwendt
//
// Experimental support for Jochen Hippel's TFMX format

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

#include <cstring>

#include "HippelDecoder.h"

namespace tfmxaudiodecoder {

const std::string HippelDecoder::TFMX_FORMAT_NAME = "TFMX/Hippel (AMIGA)";
const std::string HippelDecoder::TFMX_TAG = "TFMX";

bool HippelDecoder::TFMX_init(int songNumber) {
    admin.startSong = (songNumber < 0) ? 0 : songNumber;

    setRate(50<<8);
    stats.voices = 4;
    
    trackColumnSize = TFMX_TRACKTAB_COLUMN_SIZE;
    trackStepLen = TFMX_TRACKTAB_STEP_SIZE;
    traits.sampleStructSize = TFMX_SAMPLE_STRUCT_SIZE;
    
    pStartSongFunc = &HippelDecoder::TFMX_startSong;
    pNextNoteFunc = &HippelDecoder::TFMX_nextNote;
    pSoundFunc = &HippelDecoder::TFMX_soundModulation;
    pVibratoFunc = &HippelDecoder::TFMX_vibrato;
    pPortamentoFunc = &HippelDecoder::TFMX_portamento;
    
    traits.vibScaling = true;
    traits.vibSpeedFlag = true;
    traits.sndSeqGoto = true;
    traits.porta40SetSnd = true;   // harmless for modules that don't use it
    traitsByChecksum();

    // For convenience, since we don't relocate the module to offset 0.
    udword h = offsets.header;

    stats.sndSeqs = 1+readBEuword(fcBuf,h+4);
    stats.volSeqs = 1+readBEuword(fcBuf,h+6);
    stats.patterns = 1+readBEuword(fcBuf,h+8);
    stats.trackSteps = 1+readBEuword(fcBuf,h+0xa);
    traits.patternSize = fcBuf[h+0xd];
    if (traits.patternSize != PATTERN_LENGTH) {  // potentially never true
        return false;
    }
    stats.songs = readBEuword(fcBuf,h+0x10);
    stats.samples = readBEuword(fcBuf,h+0x12);
    if (stats.samples > SAMPLES_MAX)
        stats.samples = SAMPLES_MAX;
    
    udword offs = h+0x20;  // this is constant
    offsets.sndModSeqs = offs;
    
    offs += stats.sndSeqs*64;
    offsets.volModSeqs = offs;
    
    offs += stats.volSeqs*64;
    offsets.patterns = offs;
    
    offs += stats.patterns*traits.patternSize;
    offsets.trackTable = offs;

    trackTabLen = stats.trackSteps*trackStepLen;
    offs += trackTabLen;
    offsets.subSongTab = offs;
    
    offs += (stats.songs+1)*TFMX_SONGTAB_ENTRY_SIZE;  // subsong definitions, one is undefined
    if ( offs >= input.len ) {  // something is fubar then
        return false;
    }
    offsets.sampleHeaders = offs;
    
    offs += (stats.samples)*traits.sampleStructSize;  // skip sample headers
    offsets.sampleData = offs;

    // Reject Atari ST TFMX modules. As it seems to use different sound
    // sequence commands, a command like $E2 $E5 with less than $E5 samples
    // would be invalid for TFMX on Amiga.
    for (ubyte seq = 0; seq<stats.sndSeqs; seq++) {
        udword seqOffs = getSndSeq(seq);
        if ( stats.samples < 0xe5 && fcBuf[seqOffs] == 0xe2 &&
             fcBuf[seqOffs+1] >= 0xe5 ) {
            return false;
        }
    }

    bool maybe4V = TFMX_4V_maybe();
    bool maybe7V = TFMX_7V_maybe();
    if ( !maybe4V && maybe7V ) {
        TFMX_7V_subInit();
     }
        
    udword sh = offsets.sampleHeaders;
    sh += 0x12;  // skip file name
    for (int sam = 0; sam < stats.samples; sam++) {
        udword startOffs = readBEudword(fcBuf,sh);
        startOffs += offsets.sampleData;  // offsets are relative to start of sample data
        samples[sam].startOffs = startOffs;
        samples[sam].len = readBEuword(fcBuf,sh+4);
        samples[sam].repOffs = readBEuword(fcBuf,sh+8);  // lower word only!
        samples[sam].repLen = readBEuword(fcBuf,sh+10);
        samples[sam].assertBoundaries(fcBuf);

        if ( SSMP_HEX == readBEudword(fcBuf,startOffs) ) {  // "SSMP"?
            traits.foundSSMP = true;
        }
        else {
            // Check whether this is a one-shot sample.
            if (samples[sam].repLen == 1) {
                fcBuf[samples[sam].startOffs+samples[sam].repOffs] = fcBuf[samples[sam].startOffs+samples[sam].repOffs+1] = 0;
            }
        }
        sh += traits.sampleStructSize;  // skip to next header
    }

    (this->*pStartSongFunc)();
    return true;
}

void HippelDecoder::TFMX_startSong() {
    udword offsetSongDef = offsets.subSongTab+admin.startSong*TFMX_SONGTAB_ENTRY_SIZE;
    firstStep = readBEuword(fcBuf,offsetSongDef);
    lastStep = readBEuword(fcBuf,offsetSongDef+2);
    admin.speed = admin.startSpeed = readBEuword(fcBuf,offsetSongDef+4);
    setTrackRange();
    reset();
}

// ----------------------------------------------------------------------

void HippelDecoder::TFMX_nextNote(VoiceVars& voiceX) {
    udword pattOffs = getPattOffs(voiceX);

    // Check for pattern end or whether pattern BREAK command is set.
    if ( voiceX.pattPos==traits.patternSize || ((fcBuf[pattOffs]&0x7f)==0x01) ) {

        // This points at the new _current_ position.
        udword trackOffs = voiceX.trackStart+voiceX.trackPos;
        
        if ( trackOffs >= voiceX.trackEnd ) {  // pattern table end?
            voiceX.trackPos = 0;     // restart by default
            trackOffs = voiceX.trackStart;
            songEnd = true;
        }
        
#if defined(DEBUG_RUN)
        if (voiceX.voiceNum==0) {
            cout << endl;
            dumpTimestamp(songPosCurrent);
            cout << "  Step = " << hexW((trackOffs-offsets.trackTable)/trackStepLen) << endl;
            udword tmp = trackOffs;
            for (ubyte v = 0; v < stats.voices; ++v) {
                for (int t = 0; t < trackColumnSize; ++t) {
                    cout << hexB(fcBuf[tmp++]) << ' ';
                }
                if (v < (stats.voices-1))
                    cout << "| ";
            }
            cout << endl;
            cout << endl;
            cout << "Patterns:" << endl;
        }
#endif
        // Prepare position of _next_ step in track table.
        voiceX.trackPos += trackStepLen;
        
        uword pattern = fcBuf[trackOffs++];  // PT
        voiceX.pattStart = offsets.patterns+(pattern<<6);
        voiceX.transpose = fcBufS[trackOffs++];  // TR
        voiceX.soundTranspose = fcBufS[trackOffs++];  // ST
        if (trackColumnSize==3 && (ubyte)voiceX.soundTranspose == 0x80) {
        // ST 0x80 is "player off".
            songEnd = true;
        }
        else if (trackColumnSize==4) {
            TFMX_7V_trackTabCmd(voiceX,trackOffs);
        }
        voiceX.pattPos = 0;
    }
    TFMX_processPattern(voiceX);
}

void HippelDecoder::TFMX_processPattern(VoiceVars& voiceX) {
    udword pattOffs = getPattOffs(voiceX);

    ubyte note = fcBuf[pattOffs++];
#if defined(DEBUG_RUN)
    dumpByte(note);
    dumpByte(fcBuf[pattOffs]);
    cout << "| ";
#endif
    if ( (note & 0x7f) != 0 ) {  // 0 = nothing more to do here
        
        voiceX.pattVal1 = note&0x7f;
        voiceX.pattVal2 = fcBuf[pattOffs];
        voiceX.portaOffs = 0;   // reset portamento offset

        // Get previous info byte, but stay within same pattern.
        // pattOffs is +1 here already, so relative offset must be -2.
        if ( voiceX.pattPos == 0 ) {
            voiceX.portaSpeed = fcBufS[(pattOffs+traits.patternSize)-2];
        }
        else {
            voiceX.portaSpeed = fcBufS[pattOffs-2];
        }

        if ( (note & 0x80) == 0 ) {  // not negative?
            // (NOTE) Some players here disable Paula channel as if preparing
            // new samples to play, which breaks some modules.

            // Get instrument/volModSeq number from info byte #1
            // and add sound transpose value from track table.
            uword instr = (voiceX.pattVal2&0x1f)+voiceX.soundTranspose;
            setInstrument(voiceX,instr);
        }
    }

    // Advance to next pattern entry.
    voiceX.pattPos += 2;
}

}  // namespace
