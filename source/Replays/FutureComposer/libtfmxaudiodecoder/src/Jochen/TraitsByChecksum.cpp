// Future Composer & Hippel player library
// by Michael Schwendt
//
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

#include <algorithm>

#include "HippelDecoder.h"
#include "Analyze.h"
#include "CRCLight.h"

void tfmxaudiodecoder::HippelDecoder::traitsByChecksum() {
    CRCLight crc;
    smartPtr<const ubyte> sBuf(fcBuf.tellBegin(),fcBuf.tellLength());
    const ubyte* pEnd;
    const ubyte* r;
    
    // If there's player machine code before the TFMX header, checksum it.
    if ( offsets.header != 0 ) {
        udword crc1 = 0;

        // Search for silence seq in player object,
        // because that is before the tons of voice variables.
        pEnd = sBuf.tellBegin()+offsets.header;
        const ubyte* pSilence = silenceData;
        r = std::search(sBuf.tellBegin(),pEnd,pSilence,pSilence+sizeof(silenceData));
        if (r != pEnd) {
            udword silenceOffs = (r-sBuf.tellBegin());
            crc1 = crc.get(sBuf,0,silenceOffs);
        }
            
        // Wings of Death  end, intro, outro, title
        // skip from sndmod sustain to volmod seq processing
        // the others skip to to wave modulation
        // audible in "outro" bass line beginning
        if (crc1 == 0x3bcb814b) {
            traits.skipToWaveMod = true;
        }
        else if (crc1 == 0x4c0a6454) {
            traits.skipToWaveMod = false;
        }
        if (crc1 == 0x3bcb814b || crc1 == 0x4c0a6454) {
            traits.vibScaling = false;
            TFMX_sndModFuncs[1] = &HippelDecoder::TFMX_sndSeq_E1_waveMod;
            TFMX_sndModFuncs[5] = &HippelDecoder::TFMX_sndSeq_E5;
            TFMX_sndModFuncs[6] = &HippelDecoder::TFMX_sndSeq_E6;
            TFMX_sndModFuncs[7] = &HippelDecoder::TFMX_sndSeq_E7_setDiffWave;
            TFMX_sndModFuncs[9] = &HippelDecoder::TFMX_sndSeq_UNDEF;
        }

        // The Seven Gates of Jambala
        // Leaving Teramis
        // some others
        if (crc1 == 0xb6105cbf || crc1 == 0xb4f798e6 ||
            crc1 == 0xed5cfac0 ||
            crc1 == 0x6c55f30a ) {
            traits.sndSeqToTrans = true;
            traits.sndSeqGoto = false;
            TFMX_sndModFuncs[5] = &HippelDecoder::TFMX_sndSeq_E5_repOffs;  // NB: but not used by those modules!
            TFMX_sndModFuncs[7] = &HippelDecoder::TFMX_sndSeq_E7_setDiffWave;
        }
    }

    if (traits.compressed) {
        udword crc2 = crc.get(sBuf,offsets.trackTable,trackTabLen);

        // Astaroth
        // Chambers of Shaolin
        // Dragonflight (musf)
        if (crc2 == 0x5495cc19 || crc2 == 0x7866963a ||
            crc2 == 0xc04a79a7 ||
            crc2 == 0x2158d5fc || crc2 == 0x2518e0aa || crc2 == 0x3e8f7323 ) {
            pPortamentoFunc = &HippelDecoder::TFMX_portamento;
        }
        // Wings of Death level 1-7
        if (crc2 == 0x6084ae9f || crc2 == 0x8ec439c || crc2 == 0x664a0708 ||
            crc2 == 0x5cbcc3e9 || crc2 == 0x58514e2 || crc2 == 0x9beab3a4 ||
            crc2 == 0xb6185651 ) {
            traits.skipToWaveMod = true;  // default for COSO, though
            pPortamentoFunc = &HippelDecoder::TFMX_portamento;
        }
    }
}
