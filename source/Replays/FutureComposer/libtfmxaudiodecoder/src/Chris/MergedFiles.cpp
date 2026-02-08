// TFMX audio decoder library by Michael Schwendt

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

#include "TFMXDecoder.h"

// Support for single-file TFMX files:
//
//  - TFMXPAK
//  - TFHD
//  - TFMX-MOD
//
// There are multiple solutions that merge the two mdat.* and smpl.* files,
// prepend a header structure and sometimes append an extra structure.
//
// TFHD : While it can specify the required TFMX version a player must support,
// that doesn't cover all variants of TFMX players.
//
// TFMX-MOD : Its primary feature is to specify the (sub-)song to play by default
// in addition to providing Title/Author/Game for it. So, if a TFMX module contains
// multiple songs, TFMX-MOD usually is used to create a separate file for each song.
// The player could still select other songs found within the module data, but that
// defeats the purpose of this file format.

#include <string>
#include <cstring>

namespace tfmxaudiodecoder {
    const std::string TFMXDecoder::TAG_TFMXPAK = "TFMXPAK ";
    const std::string TFMXDecoder::TAG_TFHD = "TFHD";
    const std::string TFMXDecoder::TAG_TFMXMOD = "TFMX-MOD";
}

bool tfmxaudiodecoder::TFMXDecoder::isMerged() {
    input.versionHint = 0;
    input.startSongHint = -1;

    // ----------------------------------------------------------------------
    // "TFMXPAK 012345 0123456 >>>" ASCII header from the early 90s.
    //  01234567890123456789012345
    const int tfmxPakParseSize = 31;  // large enough
    if ( input.len > tfmxPakParseSize && !memcmp(input.buf,TAG_TFMXPAK.c_str(),TAG_TFMXPAK.length()) ) {
        char probeStr[tfmxPakParseSize+1];
        memcpy(probeStr,input.buf,tfmxPakParseSize);
        probeStr[tfmxPakParseSize] = 0;

#if defined(DEBUG)
        cout << probeStr << endl;
#endif
        int r = sscanf(probeStr,"TFMXPAK %u %u >>>",&input.mdatSize,&input.smplSize);
        char* s = strstr(probeStr,">>>");
        if (r!=2 || s==NULL || input.mdatSize==0 || input.smplSize==0) {
            return false;
        }
        offsets.header = (s+3)-probeStr;
        return true;
    }
    
    // ----------------------------------------------------------------------
    // "TFHD" structure. Big-endian.
    const int tfmxTFHDParseSize = 0x12;
    if ( input.len > tfmxTFHDParseSize && !memcmp(input.buf,TAG_TFHD.c_str(),TAG_TFHD.length()) ) {
        // Strictly require two zero bytes here, although they belong to the
        // 32-bit header size value.
        if ( readBEuword(pBuf,4) !=0 ) {
            return false;
        }
#if defined(DEBUG)
        cout << TAG_TFHD << endl;
#endif
        offsets.header = readBEudword(pBuf,4);
       
        // 0 = unspecified / 1 = TFMX v1.5 / 2 = TFMX Pro / 3 = TFMX 7V
        // bit 7 = forced
        // Only few TFHD files specify the version.
        input.versionHint = pBuf[8];
        if (input.versionHint != 0) {
#if defined(DEBUG)
            cout << "TFHD version=" << input.versionHint << "  for  " << input.path << endl;
#endif
        }
        
        //ubyte version = pBuf[9];  // 0 as the only header version so far
        input.mdatSize = readBEudword(pBuf,10);
        input.smplSize = readBEudword(pBuf,14);
        if (input.mdatSize==0 || input.smplSize==0) {
            return false;
        }
        return true;
    }

    // ----------------------------------------------------------------------
    // TFMX-MOD header as another single-file format modification.
    // Little-endian!
    const int tfmxMODParseSize = 16;
    if ( input.len > tfmxMODParseSize && !memcmp(input.buf,TAG_TFMXMOD.c_str(),TAG_TFMXMOD.length()) ) {
        // Strictly require zero bytes here, although they belong to the
        // 32-bit offsets.
        if ( pBuf[11] !=0 || pBuf[15] !=0 ) {
            return false;
        }
#if defined(DEBUG)
        cout << TAG_TFMXMOD << endl;
#endif
        offsets.sampleData = makeDword(pBuf[11],pBuf[10],pBuf[9],pBuf[8]);
        offsets.header = 8+12;
        input.mdatSize = offsets.sampleData-offsets.header;
        
        udword offs = makeDword(pBuf[15],pBuf[14],pBuf[13],pBuf[12]);
        input.smplSize = offs-input.mdatSize;
        
        int startSong = -1;
        //int flag = 0;
        do {
            ubyte what = pBuf[offs++];
            uword len = makeWord(pBuf[offs+1],pBuf[offs]);
            if (len == 0) {
                break;
            }
            offs += 2;
            // Need this, since there is crap at the end that
            // triggers definitions accidentally.
            if ((offs+len) > input.len) {
                break;
            }
            switch (what) {
            case 1:
                while (len-- > 0) {
                    ubyte c = pBuf[offs++];
                    if (c<128) {
                        author.push_back((char)c);
                    }
                    else {
                        author.push_back(0xc2+(c>0xbf));
                        author.push_back((c&0x3f)+0x80);
                    }
                }
                break;
            case 2:
                while (len-- > 0) {
                    ubyte c = pBuf[offs++];
                    if (c<128) {
                        game.push_back((char)c);
                    }
                    else {
                        game.push_back(0xc2+(c>0xbf));
                        game.push_back((c&0x3f)+0x80);
                    }
                }
                break;
            case 6:
                while (len-- > 0) {
                    ubyte c = pBuf[offs++];
                    if (c<128) {
                        title.push_back((char)c);
                    }
                    else {
                        title.push_back(0xc2+(c>0xbf));
                        title.push_back((c&0x3f)+0x80);
                    }
                }
                break;
            case 0:
                startSong = makeWord(pBuf[offs+3],pBuf[offs+2]);
                offs += (len*4);  // one dword
                break;
            case 5:
                //flag = pBuf[offs];  // documentation missing!
                offs += len;
                break;
            default:
                offs += len;
                break;
            };
        }
        while (offs < input.len);
        input.startSongHint = startSong;
        return true;
    }

    // No single-file format recognized.
    return false;
}
