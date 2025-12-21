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

#include <algorithm>
#include <set>
#include <tuple>

// Some false positives look like valid song definitions, but define a
// false speed. It may be necessary to add some sort of blacklist based
// on checksum. Possibly MD5 then and not CRC.

int tfmxaudiodecoder::TFMXDecoder::getSongs() {
    return vSongs.size();
}

void tfmxaudiodecoder::TFMXDecoder::findSongs() {
    typedef std::tuple<int,int,int> SongArgs;
    typedef std::set<SongArgs> SongArgsSet;
    SongArgsSet setSongArgs;

    // Examine all (sub-)song definitions.
    vSongs.clear();
    for (int so=0; so<32; so++) {
        int s1 = readBEuword(pBuf,offsets.header+0x100+(so<<1));
        int s2 = readBEuword(pBuf,offsets.header+0x140+(so<<1));
        int s3 = readBEuword(pBuf,offsets.header+0x180+(so<<1));
        // Skip invalid defs.
        if (s1>s2 || s1>=0x1ff || s2>=0x1ff) {
#if defined(DEBUG)
            cout << "WARNING: Skipping song " << so << ": " << s1 << " to " << s2 << " speed " << s3 << "  for " << input.path << endl;
#endif
            continue;
        }
        // First step == last step isn't invalid per se,
        // but in corner-cases the tracks don't advance either.
        if (s1==s2) {
            sequencer.step.current = sequencer.step.first = s1;
            sequencer.step.last = s2;
            processTrackStep();
            int countInactive = 0;
            for (ubyte t=0; t<sequencer.tracks; t++) {
                Track& tr = track[t];
                if (tr.PT >= 0x90) {
                    countInactive++;
                }
            }
            if (countInactive == sequencer.tracks) {
                continue;
            }
        }
        
        // Avoid duplicates.
        SongArgs a = std::make_tuple( s1, s2, s3 );
        bool skipSong = false;
        for ( SongArgsSet::iterator it = setSongArgs.begin(); it != setSongArgs.end(); ++it ) {
            if ( std::get<0>(*it) == s1 && s2 == std::get<0>(*it) ) {
                skipSong = true;
#if defined(DEBUG)
                cout << "WARNING: Skipping song fragment " << so << " (" << s1 << ',' << s2 << ',' << s3 << ')' << endl;
#endif
                break;
            }
        }
        if ( !skipSong && setSongArgs.count(a)==0 ) {
            vSongs.push_back(so);
            setSongArgs.insert(a);
#if defined(DEBUG)
            cout << "Song " << so << ": " << s1 << " to " << s2 << " speed " << s3 << endl;
#endif
        }
    }
}
