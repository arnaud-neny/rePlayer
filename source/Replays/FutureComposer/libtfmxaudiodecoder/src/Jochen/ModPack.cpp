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

// Some .HIPC files contain multiple COSO modules, abusing one of the
// offsets within each COSO header to point at the next header. The last
// module contains the sample headers and sample data.
//
// One pack style uses the sample data offset in $1c to link the next
// header, but duplicates the sample headers in each module.
//
// Another pack style uses the sample headers offset in $18 as link
// offset, which sometimes is off by an extra +$18. Additionally, other
// header fields like number of songs within the individual modules may
// be wrong, too. What a mess!

namespace tfmxaudiodecoder {

bool HippelDecoder::COSO_isModPack(std::vector<udword>& vHeaders, bool& reject) {
    udword h = offsets.header;
    udword nextHeader;
    bool foundLink = false;
    reject = false;

    // DELIRIUM custom player module pack, e.g. cust.ingame_1-7.hipc
    // concatenates multiple COSO modules, linking from COSO header
    // sample data offset at +$1c to next COSO header.
    nextHeader = offsets.sampleData;
    while (nextHeader != 0 && COSO_HEX == readBEudword(fcBuf,nextHeader)) {
        foundLink = true;
        if (TFMX_HEX != readBEudword(fcBuf,nextHeader+0x20)) {
            reject = true;  // COSO but no TFMX
            return true;
        }
        vHeaders.push_back(nextHeader);
        stats.songs += readBEuword(fcBuf,nextHeader+0x20+0x10);

        offsets.sampleHeaders = nextHeader+readBEudword(fcBuf,nextHeader+0x18);
        nextHeader += readBEudword(fcBuf,nextHeader+0x1c);
        offsets.sampleData = nextHeader;
    }
    if (foundLink) {
        return true;
    }

    // astaroth.hipc (size 28924)
    // 1st header: $09f4 "COSO" + offset in $18 = $20d2 (CORRECT)
    //   in $14 offset subsong tab = $20ba (CORRECT)
    //   3 songs = 3*6+6 = $18 / 3 and an empty one (CORRECT)
    // 2nd header: $20d2 "COSO" + offset in $18 = $33d0 (WRONG, $18 too high!)
    //   in $14 offset subsong tab at $33b2 (CORRECT)
    //   4 songs = 4*6+6 = $1e (WRONG, it's one song only!)
    // 3rd header: $33b8 "COSO" + offset in $18 = sample headers (CORRECT)
    nextHeader = offsets.sampleHeaders;
    do {
        if (nextHeader == 0) {
            break;
        }
        if (COSO_HEX == readBEudword(fcBuf,nextHeader)) {
        }
        else if (COSO_HEX == readBEudword(fcBuf,nextHeader-0x18)) {
            // IMHO, in some of these module packs the offsets are broken.
            nextHeader -= 0x18;
        }
        else {
            break;
        }
        if (TFMX_HEX != readBEudword(fcBuf,nextHeader+0x20)) {
            reject = true;  // COSO but no TFMX
            return true;
        }
        h = nextHeader;
        vHeaders.push_back(h);

        offsets.subSongTab = h+readBEudword(fcBuf,h+0x14);
        // This may be a link-offset and, again, may be +$18 too high.
        offsets.sampleHeaders = h+readBEudword(fcBuf,h+0x18);
        nextHeader = offsets.sampleHeaders;
        // Check and adjust the offset to the next header (if any), since
        // then we can determine the number of songs for this module.
        if (COSO_HEX == readBEudword(fcBuf,nextHeader)) {
        }
        else if (COSO_HEX == readBEudword(fcBuf,nextHeader-0x18)) {
            nextHeader -= 0x18;
        }
        if (offsets.subSongTab != h) {
            uword songs = (nextHeader-offsets.subSongTab)/6;
            writeBEword(fcBuf,h+0x20+0x10,songs);  // fix the header data
            stats.songs += songs;
        }
        stats.samples = readBEuword(fcBuf,h+0x20+0x12);  // last one is correct
        offsets.sampleData = offsets.sampleHeaders+stats.samples*traits.sampleStructSize;
    } while (true);
    
    return false;
}

}  // namespace
