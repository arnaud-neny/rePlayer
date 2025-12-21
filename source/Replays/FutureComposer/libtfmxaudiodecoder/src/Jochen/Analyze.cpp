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

#include "Analyze.h"

namespace tfmxaudiodecoder {

void Analyze::clear() {
    vibratoAmplSet.clear();
    vibratoSpeedSet.clear();
    
    sndSeqSet.clear();
    sndSeqCmdMap.clear();
    volSeqCmdMap.clear();
    
    samplesSet.clear();
}

void Analyze::dump() {
#if defined(DEBUG)
    cout << "Sound sequence commands used:" << endl;
    for ( SeqTraitsMap::iterator it = sndSeqCmdMap.begin();
          it != sndSeqCmdMap.end(); ++it ) {
        cout << hexB(it->first) << " : ";
        SeqTraits p = it->second;
        std::for_each(p.valuesUsed.begin(), p.valuesUsed.end(), [](ubyte c) {
            cout << hexB(c) << ' ';
        });
        cout << endl;
    }
    cout << endl;

    cout << "Volume sequence commands used:" << endl;
    for ( SeqTraitsMap::iterator it = volSeqCmdMap.begin();
          it != volSeqCmdMap.end(); ++it ) {
        cout << hexB(it->first) << " : ";
        SeqTraits p = it->second;
        std::for_each(p.valuesUsed.begin(), p.valuesUsed.end(), [](ubyte c) {
            cout << hexB(c) << ' ';
        });
        cout << endl;
    }

    cout << "Samples used (max=0x" << hex << (int)maxSampleNum() << "): ";
    std::for_each(samplesSet.begin(), samplesSet.end(), [](ubyte c) {
        cout << hex << (int)c << ' ';
    });
    cout << endl;

    cout << "Vibrato amplitudes: ";
    std::for_each(vibratoAmplSet.begin(), vibratoAmplSet.end(), [](ubyte c) {
        cout << hexB(c) << ' ';
    });
    cout << endl;
    cout << "Vibrato speeds: ";
    std::for_each(vibratoSpeedSet.begin(), vibratoSpeedSet.end(), [](ubyte c) {
        cout << hexB(c) << ' ';
    });
    cout << endl;

    cout << "Negative vibrato speed : " << (int)usesNegVibSpeed() << endl;

    cout << "Sound sequence transpose/pitch values used:" << endl;
    for ( SeqTraitsMap::iterator it = seqTransMap.begin();
          it != seqTransMap.end(); ++it ) {
        cout << hexB(it->first) << " : ";
        SeqTraits p = it->second;
        std::for_each(p.valuesUsed.begin(), p.valuesUsed.end(), [](ubyte c) {
            cout << hexB(c) << ' ';
        });
        cout << endl;
    }
#endif  // DEBUG
}

void Analyze::gatherVibrato(HippelDecoder::VoiceVars& voiceX) {
    vibratoAmplSet.insert(voiceX.vibAmpl);
    vibratoSpeedSet.insert(voiceX.vibSpeed);
}

bool Analyze::usesNegVibSpeed() {
    std::set<ubyte>::iterator it;
    for (it = vibratoSpeedSet.begin(); it!=vibratoSpeedSet.end(); ++it) {
        if (*it > 0x80) {
            return true;
        }
    }
    return false;
}

void Analyze::gatherSndSeq(ubyte seq) {
    sndSeqSet.insert(seq);
}

bool Analyze::usesSndSeq(ubyte seq) {
    return sndSeqSet.count(seq)==1;
}

void Analyze::gatherSndSeqCmd(ubyte seq, ubyte cmd) {
    sndSeqCmdMap[seq].valuesUsed.insert(cmd);
    sndSeqCmdMap[seq].valuesCounted[cmd]++;
}

void Analyze::gatherVolSeqCmd(ubyte seq, ubyte cmd) {
    volSeqCmdMap[seq].valuesUsed.insert(cmd);
    volSeqCmdMap[seq].valuesCounted[cmd]++;
}

void Analyze::gatherSampleNum(ubyte num) {
    samplesSet.insert(num);
}

ubyte Analyze::maxSampleNum() {
    ubyte s = 0;
    std::set<ubyte>::iterator it;
    for (it = samplesSet.begin(); it!=samplesSet.end(); ++it) {
        if (*it > s) {
            s = *it;
        }
    }
    return s;
}

// Examine all sndmod sequences that use E7 AND (E0 OR E1).
// Then it is unlikely to be old E7 SETSEQ command.
bool Analyze::usesE7setDiffWave(HippelDecoder* decoder) {
    for ( SeqTraitsMap::iterator it = sndSeqCmdMap.begin();
          it != sndSeqCmdMap.end(); ++it ) {
        SeqTraits p = it->second;
        if ( p.valuesUsed.count(0xE7) == 1 ) {
            ubyte seq = it->first;
            udword seqOffs = decoder->getSndSeq(seq);
            udword seqOffsEnd = decoder->getSndSeqEnd(seq);
            for (udword i=0; i<(seqOffsEnd-seqOffs); i++) {
                if ( decoder->fcBuf[seqOffs+i] == 0xE0 ||
                     decoder->fcBuf[seqOffs+i] == 0xE1 ) {
                    return true;
                }
            }
        }
    }
    return false;
}

void Analyze::gatherSeqTrans(ubyte seq, ubyte tr) {
    seqTransMap[seq].valuesUsed.insert(tr);
    //seqTransMap[seq].valuesCounted[tr]++;
}

}  // namespace
