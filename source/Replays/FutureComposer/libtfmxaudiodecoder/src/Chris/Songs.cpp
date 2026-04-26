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
//
// There are song definitions that define start/end steps outside the
// range of the sequencer's track table. For example, Jim Power (End Level):
// Track table length is $e0, so the end step must be at most $e, but
// subsong is defined as $e to $f which would be behind the end of
// the track table. The inaccessible pattern data between end of track
// table and beginning of the actual first pattern are not helpful in
// that case.

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
        int s1next = s1;
        uword stepMax = (offsets.trackTableEnd-offsets.trackTable)/0x10;
        // If the first song's track end is out of bounds, fix it.
        if (so==0 and s2>stepMax) {
            s2 = stepMax;
        }
        // Skip invalid defs.
        // Largest track number $1ff is not invalid per se,
        // but (1ff,1ff,0) was the dummy placeholder entry for song 32,
        // and some files have changed it to (0,1ff,5).
        // Yet we cannot reject (0,1ff,SPEED) entirely, since e.g.
        // the composer Erno used that in the first song definition.
        if (s1>s2 || s1>0x1ff || s2>0x1ff ||
            s1>=stepMax ||
            (so>0 && (s1==0x1ff || s2==0x1ff)) ) {
#if defined(DEBUG)
            cout << "WARNING: Skipping invalid song " << hexB(so) << ": " << hexW(s1) << " to " << hexW(s2) << " speed " << hexW(s3) << "  for " << input.path << endl;
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
#if defined(DEBUG)
            cout << "WARNING: Skipping inactive song " << hexB(so) << ": " << hexW(s1) << " to " << hexW(s2) << " speed " << hexW(s3) << "  for " << input.path << endl;
#endif
                continue;
            }
            if (s1 != sequencer.step.current) {
                s1next = sequencer.step.current;
            }
        }

        // Avoid two types of duplicates.
        //
        // 1: All like (0,0,5) and basically (X,X,SPEED) where
        //    a previously accepted song had the same start step X already.
        // 2: Exact dupes of (X,Y,SPEED) will be skipped, too.
        SongArgs a = std::make_tuple( s1, s2, s3 );
        bool skipSong = false;
        for ( SongArgsSet::iterator it = setSongArgs.begin(); it != setSongArgs.end(); ++it ) {
            if ( (std::get<0>(*it) == s1 && s2 == std::get<0>(*it)) ||
                 // Also ignore songs which immediately advance to
                 // the start step of a previously seen song def.
                 (s1next!=s1 && (std::get<0>(*it) == s1next)) ) {
                skipSong = true;
#if defined(DEBUG)
                cout << "WARNING: Skipping song fragment " << hexB(so) << " (" << hexW(s1) << ',' << hexW(s2) << ',' << hexW(s3) << ')' << endl;
#endif
                break;
            }
        }
        if ( !skipSong && setSongArgs.count(a)==0 ) {
            vSongs.push_back(so);
            setSongArgs.insert(a);
#if defined(DEBUG)
            cout << "Song " << hexB(so) << ": " << hexW(s1) << " to " << hexW(s2) << " speed " << hexW(s3) << endl;
#endif
        }
    }
}
