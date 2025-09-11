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
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <map>
#include <set>
#include <vector>

#include "FC.h"
#include "SmartPtr.h"

class Analyze {
public:
    void clear();
    void dump();

    udword crc(smartPtr<const ubyte> ptr, udword offset, udword len);

    void gatherSndSeq(ubyte seq);
    void gatherSndSeqCmd(ubyte seq, ubyte cmd);
    void gatherVolSeqCmd(ubyte seq, ubyte cmd);
    void gatherSeqTrans(ubyte seq, ubyte tr);
    void gatherVibrato(FC::VoiceVars&);
    void gatherSampleNum(ubyte num);

    bool usesSndSeq(ubyte seq);
    bool usesE7setDiffWave(FC*);
    bool usesNegVibSpeed();
    ubyte maxSampleNum();
    
private:
    static const udword crc32tab[];

    std::set<ubyte> sndSeqSet;
    std::set<ubyte> volSeqSet;
    std::set<ubyte> samplesSet;
    std::set<ubyte> vibratoAmplSet;
    std::set<ubyte> vibratoSpeedSet;
    
    struct SeqTraits {
        std::set<ubyte> valuesUsed;
        std::map<ubyte,udword> valuesCounted;
    };

    typedef std::map<ubyte,SeqTraits> SeqTraitsMap;   // seq# -> ...
    SeqTraitsMap sndSeqCmdMap;
    SeqTraitsMap volSeqCmdMap;
    SeqTraitsMap seqTransMap;
};
