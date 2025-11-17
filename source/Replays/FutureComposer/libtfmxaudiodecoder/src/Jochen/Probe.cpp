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

#include <algorithm>
#include <cstring>

#include "HippelDecoder.h"

namespace tfmxaudiodecoder {

// In ideal circumstances, we are provided with a buffer containing
// the complete file contents. Else, if we only can examine a file's
// beginning, maybe that is where a machine code player is located,
// and then we may need to guess and defer closer inspection to the
// time when we can examine the full file.
//
// Set offsets.header to an actual finding. Else keep it at 0xffffffff,
// and full init can fail, if it doesn't find anything either.

// --------------------------------------------------------------------------
// Files with .hip extension typically start with player machine code of
// varying size. Only few .hipc files do that.

bool HippelDecoder::TFMX_detect(ubyte* d, udword len) {
    bool maybe = false;
    // Raw TFMX without a prepended player is rare. Furthermore,
    // Chris Hülsbeck's different TFMX format starts with either
    // "TFMX-SONG" or "TFMX ", so checking for "TFMX",0 may be good
    // enough before taking a closer look later.
    if ( (len >= 5) && !memcmp(d,TFMX_TAG.c_str(),4) && d[4]==0x00 ) {
        offsets.header = 0;
        maybe = true;
    }
    else if ( len >= TFMX_PROBE_SIZE && TFMX_findTag(d,len) ) {
        maybe = true;
    }
    else if ( TFMX_7V_findPlayer(d,len) ) {
        maybe = true;
    }
    if (maybe) {
        formatID = TFMX_TAG;
        formatName = TFMX_FORMAT_NAME;
        pInitFunc = &HippelDecoder::TFMX_init;
        return true;
    }
    return false;
}

bool HippelDecoder::TFMX_findTag(const ubyte* buf, udword len) {
    // First search for the "TFMX" header.
    const ubyte* r = std::search(buf,buf+len,TFMX_TAG.c_str(),TFMX_TAG.c_str()+4);
    if (r != (buf+len) ) {
        udword h = (udword)(r-buf);
        if (buf[h+4] != 0) {  // require trailing zero
            return false;
        }
        // An earlier "COSO" header means this is not uncompressed TFMX,
        // then reject the module.
        if ( (h >= 0x20) && !memcmp(buf+h-0x20,COSO_TAG.c_str(),4) ) {
            return false;
        }
        offsets.header = h;
        return true;
    }
    return false;
}

bool HippelDecoder::TFMX_findPortamentoCode() {
    bool foundIt = false;
    const ubyte* r;
    udword pos = 0;

    const ubyte portaCode[] = {
        0x76,0x0b,      // MOVEQ #11,D3
        0x24,0x28,0x00  // MOVE.l ($00??,A0),D2
    };

    do {
        udword o = 0;
        r = std::search(input.buf+pos,input.buf+input.len,portaCode,portaCode+sizeof(portaCode));
        if (r != (input.buf+input.len) ) {
            o = (udword)(r-input.buf);
            uword voiceVar = readBEuword(fcBuf,o+sizeof(portaCode)-1);
            // at -6 or -4: 0x4a,0x01   TST.b D1
            if ( (readBEuword(fcBuf,o-6) == 0x4a01 ||
                  readBEuword(fcBuf,o-4) == 0x4a01) &&
                 readBEuword(fcBuf,o+12) == voiceVar &&
                 // LSL.l D3,D1   or   ASL.l D3,D1
                 (readBEuword(fcBuf,o+6)&0xfff1) == 0xe7a1 &&
                 // ADD.l D1,D2
                 readBEuword(fcBuf,o+8) == 0xd481 &&
                 // MOVE.l D2,$00??(A0)
                 readBEuword(fcBuf,o+10) == 0x2142 &&
                 readBEuword(fcBuf,o+12) == voiceVar &&
                 // SWAP D2
                 readBEuword(fcBuf,o+14) == 0x4842 ) {
                foundIt = true;
                break;
            }
        }
        pos = o;
    }
    while (r != (input.buf+input.len) && pos<input.len);
    return foundIt;
}

bool HippelDecoder::TFMX_7V_findPlayer(const ubyte* buf, udword len) {
    // The sequence command JMP code fragment without the
    // last byte 0x?? (= the length+2 of the JMP array).
    const ubyte seqJmp[] = {
        0x04,0x40,0x00,0xe0,0xd0,0x40,0x30,0x3b,0x00,0x06,
        0x4e,0xfb,0x00 // ,0x?? <--
    };
    // The sound sequence JMP code:
    // For TFMX & COSO players the array size is $14-2 (9 words)
    // or $16-2 (10 words), or the player is older and doesn't use
    // a JMP implementation.
    // For TFMX 7V player it is $18-2 (11 words).
    //
    // The instrument/volume sequence:
    // For TFMX & COSO players the array size is $14-2 (9 words)
    // with the words at index 1-7 all being $16.
    // For MCMD the words at index 1-7 are all $1E.

    smartPtr<const ubyte> sBuf(buf,len);
    bool foundVolSeqJmp = false;
    //bool foundSndSeqJmp = false;  // avoid compiler warning

    const ubyte* r;
    udword pos = 0;
    do {
        udword o = 0;
        r = std::search(buf+pos,buf+len,seqJmp,seqJmp+sizeof(seqJmp));
        if (r != (buf+len) ) {
            o = (udword)(r-buf) + sizeof(seqJmp);
            if (sBuf[o] == 0x14 &&
                readBEuword(sBuf,o+1+2) == 0x16 && readBEuword(sBuf,o+1+4) == 0x16 &&
                readBEuword(sBuf,o+1+6) == 0x16 && readBEuword(sBuf,o+1+8) == 0x16 &&
                readBEuword(sBuf,o+1+10) == 0x16 && readBEuword(sBuf,o+1+12) == 0x16 &&
                readBEuword(sBuf,o+1+14) == 0x16) {
                if (sBuf) {  // no buffer access violation?
                    foundVolSeqJmp = true;
                }
            }
            else if (sBuf[o] == 0x18) {  // TFMX 7V only
                //foundSndSeqJmp = true;  // avoid compiler warning
            }
        }
        pos = o+1;
    }
    while (r != (buf+len) && pos<len);

    if (foundVolSeqJmp) {
        // Not true, but main init with access to the full file can do
        // a more thorough check.
        return true;
    }
    return false;
}

// Plausibility checks to distinguish between 4-voice TFMX and 7-voice TFMX.
bool HippelDecoder::TFMX_4V_maybe() {
    // The subsong table contains one undefined entry where
    // only the start speed may be set.
    udword probeOffset = offsets.subSongTab+TFMX_SONGTAB_ENTRY_SIZE*stats.songs;
    if ( probeOffset >= input.len ||
         readBEuword(fcBuf,probeOffset)!=0 ||
         readBEuword(fcBuf,probeOffset+2)!=0 ||
         fcBuf[probeOffset+4]!=0 ) {  // speed, ignore the lower byte here
        return false;
    }
    // Examine the sample headers a bit, which follow the subsong table.
    udword sh = offsets.sampleHeaders;
    for (int s = 0; s < stats.samples; s++) {
        sh += 0x12;  // skip file name
        // Expecting upper 24 bits of sample offsets to be zero,
        // because we're far from dealing with modules as large as 16 MiB.
        if ( fcBuf[sh] != 0 || fcBuf[sh+6]!=0 ) {
            return false;
        }
        // Expecting sample start offsets to be within buffer area.
        udword startOffs = readBEudword(fcBuf,sh);
        if ( startOffs+offsets.sampleData > input.len ) {
            return false;
        }
        sh += (4+2+4+2);  // skip to next header
    }
    return true;
}

// With the TFMX header offsets determined so far,
// check whether this could be a 7 voices module.
bool HippelDecoder::TFMX_7V_maybe() {
    // TFMX 7 voices track table entries are longer, which results in
    // different offsets. Subsong table entries are longer, too.
    // Last subsong table entry is empty.
    udword probeOffset = offsets.trackTable
        +stats.trackSteps*TFMX_7V_TRACKTAB_STEP_SIZE
        +stats.songs*TFMX_7V_SONGTAB_ENTRY_SIZE;
    if ( probeOffset >= input.len ||
         readBEudword(fcBuf,probeOffset)!=0 ||
         readBEudword(fcBuf,probeOffset+4)!=0 ) {
        return false;
    }
    // Examine the sample headers a bit, which follow the subsong table.
    udword sh = probeOffset+TFMX_7V_SONGTAB_ENTRY_SIZE;
    sh += 0x12;  // skip file name
    for (int s = 0; s < stats.samples; s++) {
        // Expecting upper 24 bits of sample offsets to be zero,
        // because we're far from dealing with modules as large as 16 MiB.
        if ( fcBuf[sh] != 0 || fcBuf[sh+6]!=0 ) {
            return false;
        }
        // Expecting sample start offsets to be within buffer area.
        udword startOffs = readBEudword(fcBuf,sh);
        if ( startOffs+offsets.sampleData > input.len ) {
            return false;
        }
        sh += traits.sampleStructSize;
    }
    return true;
}

// --------------------------------------------------------------------------

bool HippelDecoder::COSO_detect(ubyte* d, udword len) {
    bool maybe = false;
    // Check for "COSO" ID and "TFMX" ID at offset +$20.
    if ( (len >= 0x24) && !memcmp(d,COSO_TAG.c_str(),4) && !memcmp(d+0x20,TFMX_TAG.c_str(),4) ) {
        offsets.header = 0;
        maybe = true;
    }
    // Search for pair of COSO and TFMX.
    else if ( len >= PROBE_SIZE && TFMX_COSO_findTags(d,len) ) {
        maybe = true;
    }
    else if ( COSO_findPlayer(d,len) ) {
        maybe = true;
    }
    if (maybe) {
        formatID = COSO_TAG;
        formatName = COSO_FORMAT_NAME;
        pInitFunc = &HippelDecoder::COSO_init;
        return true;
    }
    return false;
}

bool HippelDecoder::TFMX_COSO_findTags(const ubyte* buf, udword len) {
    const ubyte* r;
    udword pos = 0;
    // First search for the "COSO" header.
    do {
        udword h = 0;
        r = std::search(buf+pos,buf+len,COSO_TAG.c_str(),COSO_TAG.c_str()+4);
        if (r != (buf+len) ) {
            h = (udword)(r-buf);
            // Next require a "TFMX" header at +$20, else reject the module.
            if ( ((h+0x40) < len) & !memcmp(buf+h+0x20,TFMX_TAG.c_str(),4) ) {
                offsets.header = h;
                return true;
            }
        }
        pos = h+1;
    }
    while (r != (buf+len) && pos<len);
    return false;
}

// Plausibility checks to distinguish between 4-voice TFMX and 7-voice TFMX.
bool HippelDecoder::COSO_4V_maybe() {
    // The subsong table contains one undefined entry where
    // only the start speed may be set.
    udword probeOffset = offsets.subSongTab+TFMX_SONGTAB_ENTRY_SIZE*stats.songs;
    if ( probeOffset >= input.len ||
         readBEuword(fcBuf,probeOffset)!=0 ||
         readBEuword(fcBuf,probeOffset+2)!=0 ||
         fcBuf[probeOffset+4]!=0 ) {  // speed, ignore the lower byte here
        return false;
    }
    return true;
}

// With the TFMX header offsets determined so far,
// check whether this could be a 7 voices module.
bool HippelDecoder::COSO_7V_maybe() {
    // TFMX 7 voices track table entries are longer, which results in
    // different offsets. Subsong table entries are longer, too.
    // Last subsong table entry is empty.
    udword probeOffset = offsets.trackTable
        +stats.trackSteps*TFMX_7V_TRACKTAB_STEP_SIZE
        +stats.songs*TFMX_7V_SONGTAB_ENTRY_SIZE;
    if ( probeOffset >= input.len ||
         readBEudword(fcBuf,probeOffset)!=0 ||
         readBEudword(fcBuf,probeOffset+4)!=0 ) {
        return false;
    }
    // Examine the sample headers a bit, which follow the subsong table.
    udword sh = probeOffset+TFMX_7V_SONGTAB_ENTRY_SIZE;
    for (int s = 0; s < stats.samples; s++) {
        // Expecting sample start offsets to be within buffer area.
        udword startOffs = readBEudword(fcBuf,sh);
        if ( startOffs+offsets.sampleData > input.len ) {
            return false;
        }
        sh += traits.sampleStructSize;
    }
    return true;
}

bool HippelDecoder::COSO_findPlayer(const ubyte* buf, udword len) {
    const ubyte pattCode[] = {
        0x0c,0x00, 0x00,0xfe, 0x66
    };
    // at +$06: 0x1151, 0x00??  // offset1
    // at +$0a: 0x1159, 0x00??  // offset2
    // at +$10: 0x0c00, 0x00fd, 0x66
    // at +$16: 0x1151, 0x00??  // offset1
    // at +$1a: 0x1159, 0x00??  // offset2

    smartPtr<const ubyte> sBuf(buf,len);
    bool foundIt = false;

    const ubyte* r;
    udword pos = 0;
    do {
        udword o = 0;
        r = std::search(buf+pos,buf+len,pattCode,pattCode+sizeof(pattCode));
        if (r != (buf+len) ) {
            o = (udword)(r-buf);
            if (
                readBEuword(sBuf,o+6) == 0x1151 && readBEuword(sBuf,o+0xa) == 0x1159 &&
                readBEuword(sBuf,o+0x10) == 0x0c00 && readBEuword(sBuf,o+0x12) == 0x00fd &&
                readBEuword(sBuf,o+0x16) == 0x1151 && readBEuword(sBuf,o+0x1a) == 0x1159 &&
                readBEuword(sBuf,o+8) == readBEuword(sBuf,o+0x18) &&  // offset1
                readBEuword(sBuf,o+0xc) == readBEuword(sBuf,o+0x1c) ) {  // offset2
                if (sBuf) {  // no buffer access violation?
                    foundIt = true;
                    break;
                }
            }
        }
        pos = o+sizeof(pattCode);
    }
    while (r != (buf+len) && pos<len);

    if (foundIt) {
        // Not true, but main init with access to the full file can do
        // a more thorough check.
        return true;
    }
    return false;
}

// --------------------------------------------------------------------------

bool HippelDecoder::findTag(const ubyte* buf, udword len, const std::string tag,
                 udword& location) {
    const ubyte* r = std::search(buf,buf+len,tag.c_str(),tag.c_str()+tag.length());
    if (r != (buf+len) ) {
        location = (udword)(r-buf);
        return true;
    }
    return false;
}

// --------------------------------------------------------------------------

bool HippelDecoder::FC_detect(ubyte* d, udword len) {
    // Check "track table length" definition. It is stored as a 32-bit value,
    // but its two uppermost bytes will be 0 always, because no existing
    // FC module uses as many track table steps as to require more than the
    // low-order 16-bit value.
    if ( (len >= 6) && !(d[4]==0x00 && d[5]==0x00) ) {
        return false;
    }

    if (len >= 5) {
        // Check for "SMOD" ID (Future Composer 1.0 to 1.3).
        if ( !memcmp(d,SMOD_TAG.c_str(),4) && d[4]==0x00 ) {
            offsets.header = 0;
            formatID = SMOD_TAG;
            formatName = SMOD_FORMAT_NAME;
            pInitFunc = &HippelDecoder::SMOD_init;
            return true;
        }
        // Check for "FC14" ID (Future Composer 1.4).
        else if ( !memcmp(d,FC14_TAG.c_str(),4) && d[4]==0x00 ) {
            offsets.header = 0;
            formatID = FC14_TAG;
            formatName = FC14_FORMAT_NAME;
            pInitFunc = &HippelDecoder::FC_init;
            return true;
        }
    }
    // (NOTE) A very few hacked "SMOD" modules exist which contain an ID
    // string "FC13". Although this could be supported easily, it should
    // NOT. Format detection must be able to rely on the ID field. As no
    // version of Future Composer has ever created a "FC13" ID, such hacked
    // modules are likely to be incompatible in other parts due to incorrect
    // conversion, e.g. effect parameters. It is like creating non-standard
    // module files whose only purpose is to confuse accurate music players.
    return false;
}

bool HippelDecoder::MCMD_detect(ubyte* d,udword len) {
    bool maybe = false;
    // Raw MCMD files.
    if ( len>0x12 && !memcmp(d,MCMD_TAG.c_str(),4) && d[4]==0x00 ) {
        offsets.header = 0;
        maybe = true;
    }
    // The MCMD player object is 0x8c0 bytes long.
    else if ( len>=MCMD_PROBE_SIZE && findTag(d,len,MCMD_TAG,offsets.header) ) {
        maybe = true;
    }
    if (maybe) {
        formatID = MCMD_TAG;
        formatName = MCMD_FORMAT_NAME;
        pInitFunc = &HippelDecoder::MCMD_init;
        return true;
    }
    return false;
}

}  // namespace
