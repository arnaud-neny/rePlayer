// Future Composer & Hippel player library
// by Michael Schwendt
//
// Experimental support for Jochen Hippel's TFMX Compressed Songs (COSO)

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

#include <vector>

#include "HippelDecoder.h"

namespace tfmxaudiodecoder {

const std::string HippelDecoder::COSO_FORMAT_NAME = "Compressed TFMX/Hippel (AMIGA)";
const std::string HippelDecoder::COSO_TAG = "COSO";

bool HippelDecoder::COSO_init(int songNumber) {
    admin.startSong = (songNumber < 0) ? 0 : songNumber;
    
    setRate(50<<8);
    stats.voices = 4;
    
    trackColumnSize = TFMX_TRACKTAB_COLUMN_SIZE;
    trackStepLen = TFMX_TRACKTAB_STEP_SIZE;
    traits.sampleStructSize = COSO_SAMPLE_STRUCT_SIZE;

    pStartSongFunc = &HippelDecoder::TFMX_startSong;
    pNextNoteFunc = &HippelDecoder::COSO_nextNote;
    pSoundFunc = &HippelDecoder::TFMX_soundModulation;
    pVibratoFunc = &HippelDecoder::COSO_vibrato;
    pPortamentoFunc = &HippelDecoder::COSO_portamento;
    // Some COSO machine code players use TFMX portamento. D'oh!
    if ( TFMX_findPortamentoCode() ) {
        pPortamentoFunc = &HippelDecoder::TFMX_portamento;
    }

    traits.compressed = true;
    traits.vibSpeedFlag = true;
    traits.skipToWaveMod = true;
    traits.sndSeqGoto = true;
    traits.volSeqSnd80 = traits.porta40SetSnd = true;
    traits.voiceVolume = true;

    // Modify TFMX base player setup.
    TFMX_sndModFuncs[1] = &HippelDecoder::TFMX_sndSeq_E1_waveMod;
    TFMX_sndModFuncs[7] = &HippelDecoder::TFMX_sndSeq_E7_setDiffWave;
    TFMX_sndModFuncs[10] = &HippelDecoder::TFMX_sndSeq_EA;

    // For convenience, since we don't relocate the module to offset 0.
    udword h = offsets.header;  // offset COSO header
    udword ht = h+0x20;  // offset TFMX header

    // Reject broken/truncated files like "cryptoburners - pack*.hipc"
    // pack2, pack4 : missing sample defs and sample data
    // pack3 : zero headers, missing module data, appended
    //         player doesn't reference that module
    // pack5 : truncated sample data
    if ( readBEudword(fcBuf,h+0xc)==0 ||
         readBEudword(fcBuf,h+0x10)==0 ||
         readBEudword(fcBuf,h+0x14)==0 ||
         readBEudword(fcBuf,h+0x1c)==0 ||
         readBEudword(fcBuf,h+0x18)>=input.len ) {
        return false;
    }
    
    // As to support module packs, we maintain a list of headers.
    std::vector<udword> vHeaders;
    vHeaders.push_back(h);
    stats.songs = readBEuword(fcBuf,ht+0x10);

    offsets.sampleHeaders = h+readBEudword(fcBuf,h+0x18);
    offsets.sampleData = h+readBEudword(fcBuf,h+0x1c);

    bool reject = false;
    if ( COSO_isModPack(vHeaders,reject) && reject ) {
        return false;
    }

    // Traverse through found modules and select the right song number.
    for (udword i=0; i<vHeaders.size(); i++) {
        h = vHeaders[i];
        ubyte songsInThisMod = readBEuword(fcBuf,h+0x20+0x10);
        if (admin.startSong < songsInThisMod) {
            offsets.header = h;
            ht = h+0x20;
            break;
        }
        admin.startSong -= songsInThisMod;
    }

    offsets.sndModSeqs = h+readBEudword(fcBuf,h+0x4);
    offsets.volModSeqs = h+readBEudword(fcBuf,h+0x8);
    offsets.patterns = h+readBEudword(fcBuf,h+0xc);
    offsets.trackTable = h+readBEudword(fcBuf,h+0x10);
    offsets.subSongTab = h+readBEudword(fcBuf,h+0x14);

    stats.sndSeqs = 1+readBEuword(fcBuf,ht+0x4);
    stats.volSeqs = 1+readBEuword(fcBuf,ht+0x6);
    stats.patterns = 1+readBEuword(fcBuf,ht+0x8);
    stats.trackSteps = 1+readBEuword(fcBuf,ht+0xa);
    // pattern size for compressed songs isn't constant, of course
    traits.patternSize = 0;  // fcBuf[ht+0xd];

    stats.samples = readBEuword(fcBuf,ht+0x12);  // not +1 like TFMX
    if (stats.samples > SAMPLES_MAX)
        stats.samples = SAMPLES_MAX;

    bool maybe4V = COSO_4V_maybe();
    bool maybe7V = COSO_7V_maybe();
    if ( !maybe4V && maybe7V ) {
        TFMX_7V_subInit();
    }

    // (NOTE) Debug output only. Sometimes number of samples is off by one.
    if ( (offsets.sampleHeaders+stats.samples*traits.sampleStructSize) != offsets.sampleData ) {
#ifdef DEBUG
        cout << "WARNING: mismatch between offset sample data 0x" << hex << offsets.sampleData << " and calculated via number of samples 0x" << (offsets.sampleHeaders+stats.samples*traits.sampleStructSize) << endl;
#endif
    }
    
    trackTabLen = stats.trackSteps*trackStepLen;
    
    // Reject Atari ST TFMX COSO modules. As it seems to use different sound
    // sequence commands, a command like $E2 $E5 with less than $E5 samples
    // would be invalid for TFMX on Amiga.
    for (ubyte seq = 0; seq<stats.sndSeqs; seq++) {
        udword seqOffs = getSndSeq(seq);
        if ( stats.samples < 0xe5 && fcBuf[seqOffs] == 0xe2 &&
             fcBuf[seqOffs+1] >= 0xe5 ) {
            return false;
        }
    }

    udword sh = offsets.sampleHeaders;
    for (int sam = 0; sam < stats.samples; sam++) {
        udword startOffs = readBEudword(fcBuf,sh)+offsets.sampleData;
        samples[sam].startOffs = startOffs;
        samples[sam].len = readBEuword(fcBuf,sh+4);
        samples[sam].repOffs = readBEuword(fcBuf,sh+6);
        samples[sam].repLen = readBEuword(fcBuf,sh+8);
        samples[sam].assertBoundaries(fcBuf);
        // Fix one-shot samples.
        if (samples[sam].repLen == 1) {
            fcBuf[samples[sam].startOffs+samples[sam].repOffs] = fcBuf[samples[sam].startOffs+samples[sam].repOffs+1] = 0;
        }
        sh += traits.sampleStructSize;
    }
    
    (this->*pStartSongFunc)();
    return true;
}

// ----------------------------------------------------------------------

void HippelDecoder::COSO_nextNote(VoiceVars& voiceX) {
    if ( --voiceX.pattCompressCount >= 0 ) {
#if defined(DEBUG_RUN)
        cout << "-- -- -- | ";
#endif
        return;
    }
    voiceX.pattCompressCount = voiceX.pattCompress;

    udword pattOffs;
    ubyte pattVal;
    
    int maxLoops = RECURSE_LIMIT;
    while ( --maxLoops >= 0 ) {

        pattOffs = getPattOffs(voiceX);
        if (voiceX.pattStart == 0xffffffff)
            pattVal = 0xff;
        else
            pattVal = fcBuf[pattOffs++];
        voiceX.pattPos++;

        if ( pattVal == 0xff ) {  // end pattern?
            
            // This points at the new _current_ position.
            udword trackOffs = voiceX.trackStart+voiceX.trackPos;

            if ( trackOffs >= voiceX.trackEnd ) {  // track table end?
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
            for (ubyte v = 0; v<stats.voices; ++v) {
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
        voiceX.pattStart = offsets.header+readBEuword(fcBuf,offsets.patterns+(pattern<<1));
        voiceX.pattPos = 0;
        voiceX.transpose = fcBufS[trackOffs++];  // TR
        ubyte st = fcBuf[trackOffs];  // ST
        if (trackColumnSize == 4) {
            voiceX.soundTranspose = fcBufS[trackOffs++];  // ST
            TFMX_7V_trackTabCmd(voiceX,trackOffs);
        }
        else if (trackColumnSize==3 && (st & 0x80) != 0) {
            TFMX_7V_trackTabCmd(voiceX,trackOffs);
        }
        else {
            voiceX.soundTranspose = fcBufS[trackOffs];  // ST
        }            
        // continue while loop
        }
        else if ( pattVal == 0xfe ) {  // change pattern speed + next note
            voiceX.pattCompress = voiceX.pattCompressCount = fcBuf[pattOffs++];
            voiceX.pattPos++;
            // continue while loop
        }
        else if ( pattVal == 0xfd ) {  // change pattern speed
            voiceX.pattCompress = voiceX.pattCompressCount = fcBuf[pattOffs++];
            voiceX.pattPos++;
#if defined(DEBUG_RUN)
            cout << "-- -- -- | ";
#endif
            return;
        }
        else {  // pattern value
            voiceX.pattVal1 = pattVal;
            break;
        }
    }  // while
    COSO_processPattern(voiceX);
}

void HippelDecoder::COSO_processPattern(VoiceVars& voiceX) {
    udword pattOffs = getPattOffs(voiceX);
    voiceX.pattVal2 = fcBuf[pattOffs++];
    voiceX.pattPos++;

#if defined(DEBUG_RUN)
    dumpByte(voiceX.pattVal1);
    dumpByte(voiceX.pattVal2);
#endif
    
    if ( (voiceX.pattVal2 & 0xe0) != 0 ) {
        voiceX.portaSpeed = fcBufS[pattOffs++];
        voiceX.pattPos++;
#if defined(DEBUG_RUN)
        dumpByte(voiceX.portaSpeed);
#endif
    }
    else {
#if defined(DEBUG_RUN)
        cout << "-- ";
#endif
    }

    voiceX.portaOffs = 0;   // reset portamento offset
    
    if ( (voiceX.pattVal1 & 0x80) == 0 ) {  // if note is not negative
        // Get instrument/volModSeq number from pattern arg2
        // and add sound transpose value from track table.
        uword instr = (voiceX.pattVal2&0x1f)+voiceX.soundTranspose;
        setInstrument(voiceX,instr);
    }
#if defined(DEBUG_RUN)
    cout << "| ";
    cout << std::flush;
#endif
}

}  // namespace
