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
// You should have received a copy of the GNU General Public License along
// with this program; if not, see <https://www.gnu.org/licenses/>.

#include "HippelDecoder.h"

// File name extension: .mcmd
// On AMIGA, file name prefix: MCMD.
//
// The MCMD format is just a slightly modified TFMX. According to aminet
// the author is not known. A machine code player object is prepended
// to some of the available MCMD module files.
//
// MCMD vs TFMX:
//  - trimmed module header structure
//  - shorter sample header structure
//  - $3f instead of $1f sounds/instruments in pattern arg2 (like FC)
//  - vibrato from TFMX COSO
//  - portamento from TFMX, but with modified flag bits
//    - flag bit 6 becomes 7
//    - flag bit 5 becomes 6
//  - tracktab special commands $F and $8
//  - different default set of sound sequence commands E0 to E8
//  - a modified E5 command that implements E7 Set Different Wave
//  - a E6 command that is a NOP
//  - via note arg1 bit 7, note arg2 can set the sound sequence number

namespace tfmxaudiodecoder {

const std::string HippelDecoder::MCMD_FORMAT_NAME = "MCMD (AMIGA)";
const std::string HippelDecoder::MCMD_TAG = "MCMD";

bool HippelDecoder::MCMD_init(int songNumber) {
    admin.startSong = (songNumber < 0) ? 0 : songNumber;

    setRate(50<<8);
    stats.voices = 4;
        
    trackColumnSize = TFMX_TRACKTAB_COLUMN_SIZE;
    trackStepLen = TFMX_TRACKTAB_STEP_SIZE;
    traits.sampleStructSize = MCMD_SAMPLE_STRUCT_SIZE;

    pStartSongFunc = &HippelDecoder::MCMD_startSong;
    pNextNoteFunc = &HippelDecoder::TFMX_nextNote;
    pSoundFunc = &HippelDecoder::TFMX_soundModulation;
    pVibratoFunc = &HippelDecoder::COSO_vibrato;
    pPortamentoFunc = &HippelDecoder::TFMX_portamento;

    traits.sndSeqGoto = true;
    traits.portaIsBit6 = true;
    traits.porta80SetSnd = true;

    TFMX_sndModFuncs[1] = &HippelDecoder::TFMX_sndSeq_E1_waveMod;
    TFMX_sndModFuncs[5] = &HippelDecoder::TFMX_sndSeq_E7_setDiffWave;
    TFMX_sndModFuncs[6] = TFMX_sndModFuncs[9] =
        TFMX_sndModFuncs[10] = &HippelDecoder::TFMX_sndSeq_UNDEF;

    // For convenience, since we don't relocate the module to offset 0.
    udword h = offsets.header;
    
    stats.sndSeqs = readBEuword(fcBuf,h+4);
    stats.volSeqs = readBEuword(fcBuf,h+6);
    stats.patterns = readBEuword(fcBuf,h+8);
    stats.trackSteps = readBEuword(fcBuf,h+0xa);
    traits.patternSize = fcBuf[h+0xd];
    stats.songs = readBEuword(fcBuf,h+0xe);;
    stats.samples = readBEuword(fcBuf,h+0x10);
    
    udword offs = h+0x12;  // this is fixed
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
    
    offs += stats.songs*TFMX_7V_SONGTAB_ENTRY_SIZE;  // subsong definitions
    if ( offs >= input.len ) {  // something is fubar then
        return false;
    }
    offsets.sampleHeaders = offs;
    
    offs += stats.samples*traits.sampleStructSize;  // skip sample headers
    offsets.sampleData = offs;
    
    udword sh = offsets.sampleHeaders;
    sh += 0x12;  // skip file name
    for (int sam = 0; sam < stats.samples; sam++) {
        udword startOffs = readBEudword(fcBuf,sh);
        startOffs += offsets.sampleData;  // offsets are relative to start of sample data
        samples[sam].startOffs = startOffs;
        samples[sam].len = readBEuword(fcBuf,sh+4);
        samples[sam].repOffs = readBEuword(fcBuf,sh+6);
        samples[sam].repLen = readBEuword(fcBuf,sh+8);
        samples[sam].assertBoundaries(fcBuf);

        // Check whether this is a one-shot sample.
        if (samples[sam].repLen == 1) {
            fcBuf[samples[sam].startOffs+samples[sam].repOffs] = fcBuf[samples[sam].startOffs+samples[sam].repOffs+1] = 0;
        }
        sh += traits.sampleStructSize;  // skip to next header
    }

    (this->*pStartSongFunc)();
    return true;
}

void HippelDecoder::MCMD_startSong() {
    udword offsetSongDef = offsets.subSongTab+admin.startSong*TFMX_7V_SONGTAB_ENTRY_SIZE;
    firstStep = readBEuword(fcBuf,offsetSongDef);
    lastStep = readBEuword(fcBuf,offsetSongDef+2);
    admin.startSpeed = readBEuword(fcBuf,offsetSongDef+4);
    setTrackRange();
    reset();
}

}  // namespace
