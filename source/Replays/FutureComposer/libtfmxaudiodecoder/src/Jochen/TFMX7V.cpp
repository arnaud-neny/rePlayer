// Future Composer & Hippel player library
// by Michael Schwendt
//
// Experimental support for Jochen Hippel's TFMX 7 Voices format

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

namespace tfmxaudiodecoder {

const std::string HippelDecoder::TFMX_7V_FORMAT_NAME = "TFMX 7V/Hippel (AMIGA)";
const std::string HippelDecoder::TFMX_7V_ID = "TFMX7V";

// Switch from TFMX or COSO to 7V mode.
void HippelDecoder::TFMX_7V_subInit() {
    stats.voices = 7;

    trackColumnSize = TFMX_7V_TRACKTAB_COLUMN_SIZE;
    trackStepLen = TFMX_7V_TRACKTAB_STEP_SIZE;
    trackTabLen = stats.trackSteps*trackStepLen;

    udword offs = offsets.trackTable;
    // offs+1 : for 7 voices modules only, set Paula channel 3 period
    TFMX_7V_setRate( fcBufS[offs+2] );  // modify player rate

    offs += trackTabLen;
    offsets.subSongTab = offs;
    offs += (stats.songs+1)*TFMX_7V_SONGTAB_ENTRY_SIZE;
    offsets.sampleHeaders = offs;
    offs += (stats.samples)*traits.sampleStructSize;  // skip sample headers
    offsets.sampleData = offs;

    pStartSongFunc = &HippelDecoder::TFMX_7V_startSong;
    pVibratoFunc = &HippelDecoder::COSO_vibrato;
    pPortamentoFunc = &HippelDecoder::COSO_portamento;

    traits.sndSeqGoto = true;
    traits.sndSeqToTrans = false;
    traits.skipToWaveMod = true;
    traits.volSeqSnd80 = traits.porta40SetSnd = true;
    traits.voiceVolume = true;
    traits.randomVolume = true;

    TFMX_sndModFuncs[1] = &HippelDecoder::TFMX_sndSeq_E1_waveMod;
    TFMX_sndModFuncs[7] = &HippelDecoder::TFMX_sndSeq_E7_setDiffWave;
    TFMX_sndModFuncs[10] = &HippelDecoder::TFMX_sndSeq_EA;

    formatID = TFMX_7V_ID;
    formatName = TFMX_7V_FORMAT_NAME;
}

void HippelDecoder::TFMX_7V_startSong() {
    udword offsetSongDef = offsets.subSongTab+admin.startSong*TFMX_7V_SONGTAB_ENTRY_SIZE;
    firstStep = readBEuword(fcBuf,offsetSongDef);
    lastStep = readBEuword(fcBuf,offsetSongDef+2);
    admin.startSpeed = readBEuword(fcBuf,offsetSongDef+6);
    setTrackRange();
    reset();
}

// ----------------------------------------------------------------------

void HippelDecoder::TFMX_7V_setRate(sbyte x) {
    setRate( ((50L<<8)*256)/(256+x) );  // [Hz]
}

// This is also used by COSO.
void HippelDecoder::TFMX_7V_trackTabCmd(VoiceVars& voiceX, udword trackOffs) {
    ubyte val = fcBuf[trackOffs];
    ubyte v1 = val>>4;
    ubyte v2 = val&0x0f;
    if (v1 == 0x0f) {  // set voice volume?
        if (v2 == 0) {
            voiceX.voiceVol = 100;
        }
        else {
            voiceX.voiceVol = ((15-v2)+1)*6;
        }
    }
    else if (v1 == 0x0d) {  // 7V only: set rate / not everything implemented
        sbyte x = fcBufS[trackOffs+2*trackColumnSize-1];
        TFMX_7V_setRate(x);
    }
    else if (v1 == 0x0e) {  // set speed
        admin.speed = v2;
    }
    else if (v1 == 0x08) {  // off
        songEnd = true;
    }
}

}  // namespace
