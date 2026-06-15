// TFMX audio decoder library by Michael Schwendt
//
// Dynamic Synthesizer decoder

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

#include "DNSDecoder.h"
#include "CRCLight.h"

#include <cstring>
#include <fstream>
#include <set>

namespace tfmxaudiodecoder {

class PaulaVoice;

const std::string DNSDecoder::FORMAT_NAME = "Dynamic Synthesizer (AMIGA)";
const std::string DNSDecoder::TAG = "DNS";

DNSDecoder::DNSDecoder() {
    input.buf = 0;
    input.bufLen = input.len = 0;
    input.smplLoaded = false;

    admin.initialized = false;

    // Set up some dummy voices to decouple the decoder from the mixer.
    for (ubyte v=0; v<VOICES_MAX; v++) {
        voiceVars[v].ch = &dummyVoices[v];
    }

    loopMode = false;
}

DNSDecoder::~DNSDecoder() {
    delete[] input.buf;
}

// ----------------------------------------------------------------------
// Since there are only three publicly released game soundtracks made
// with Dynamic Synthesizer and their player machine code and setup
// parameters differ quite a bit, we don't overdo with the probing.
// There is no header structure, and trying to identify the music module
// based on searching for machine code fragments and data pointers/offsets
// within it isn't worthwhile. Instead, we rely on checksums, also during
// the initialization step.

bool DNSDecoder::detect(void* data, udword len) {
#if defined(DEBUG)
    cout << "DNSDecoder::detect()" << endl;
#endif
    ubyte *d = static_cast<ubyte*>(data);
    smartPtr<const ubyte> sBuf(d,len);
    bool maybe = false;

    // Invalidate these two essential ones.
    admin.initialized = false;
    admin.checksum = 0;

    // The ripped modules start with player machine code, and at least
    // five BRA main entry points must be present before we take
    // a closer look.
    if ( (len >= 18) &&
         d[0]==0x60 && d[4]==0x60 && d[8]==0x60 && d[12]==0x60 && d[16]==0x60 &&
         d[1]==0 && d[5]==0 && d[9]==0 && d[13]==0 && d[17]==0 &&
         // Also check the branch offset to the "off" routine,
         // which is located fairly near the beginning for all modules.
         makeWord(d[6],d[7])<0x100) {
        maybe = true;
    }
    else {
        return false;
    }

    // If at least 256 bytes of the music file are available, such as
    // during initialization, calculate a primary checksum.
    if (len >= 0x100) {
        CRCLight crc;
        udword crc1 = crc.get(sBuf,0,0x100);
#if defined(DEBUG)
        cout << "CRC(1) = " << tohex(crc1) << "  " << input.path << endl;
#endif
        std::set<udword> checksums1 = {
            0x4f593264,  // Hollywood Poker Pro title
            0x32db24cc,  // Hollywood Poker Pro ingame
            0xf195c217,  // Starball title
            0x48ea5657,  // Starball ingame
            0xd9511944   // PTC
        };
        if (checksums1.count(crc1) < 1) {
            return false;
        }
        // We use this checksum also in main init().
        admin.checksum = crc1;
    }

    // If at least 2048 bytes of the music data are available, such as
    // during initialization, verify a second checksum.
    if (len >= 0x800) {
        CRCLight crc;
        udword crc2 = crc.get(sBuf,0x800-0x100,0x100);
#if defined(DEBUG)
        cout << "CRC(2) = " << tohex(crc2) << "  " << input.path << endl;
#endif
        std::set<udword> checksums2 = {
            0x04ec1634,  // Hollywood Poker Pro title
            0xb5e99994,  // Hollywood Poker Pro ingame
            0x07ecc48b,  // Starball title
            0xd4c2b312,  // Starball ingame
            0xefb03b1a   // PTC
        };
        if (checksums2.count(crc2) < 1) {
            return false;
        }
    }

    if (maybe) {
        offsets.header = 0;
        formatID = TAG;
        formatName = FORMAT_NAME;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------

bool DNSDecoder::init(void *data, udword length, int songNumber) {
#if defined(DEBUG)
    cout << "DNSDecoder::init()" << endl;
#endif

    if (data==0 || length==0 ) {  // re-init mode
        if (!admin.initialized) {
            return false;
        }
    }
    else {  // invalidate what has been found out before
        input.smplLoaded = false;
        input.mdatSize = input.smplSize = 0;

        if ( !detect(data,length) ) {
            return false;
        }
        // If we still have a sufficiently large buffer, reuse it.
        udword newLen = length;
        if (newLen > input.bufLen) {
            if (input.buf) {
                delete[] input.buf;
            }
            input.bufLen = 0;
            input.buf = new ubyte[newLen];
        }
        memcpy(input.buf,data,length);
        input.bufLen = newLen;
        input.len = length;
        
        // Set up smart pointer for unsigned input buffer access.
        pBuf.setBuffer(input.buf,input.bufLen);
    }

    if ( loadSamplesFile() ) {
        input.smplLoaded = true;
        // Make these the current ones.
        input.mdatSizeCurrent = input.mdatSize;
        input.smplSizeCurrent = input.smplSize;
    }
    else {
        // Can't proceed without samples data.
        return false;
    }

#if defined(DEBUG)
    cout << "samples loaded" << endl;
#endif
    offsets.sampleData = offsets.header+input.mdatSize;
    offsets.silence = offsets.sampleData;
    // Clear the first two words for one-shot samples.
    udword o = offsets.sampleData;
    pBuf[o] = pBuf[o+1] = pBuf[o+2] = pBuf[o+3] = 0;

    // Defaults.
    setRate(50<<8);
    voices = 4;
    duration = 0;

    sequencer.tracks = 6;
    sequencer.step.size = 24;
    sequencer.tables = 1;
    sequencer.tableSize = 0;  // irrelevant, if it's only one table

    // ----------

    // Despite the description "DYNAMIC SYNTHESIZER MODUL V1.34" in all
    // but one of the music modules, there are some differences in the
    // music data format and the machine code players. The features are
    // the same though.
    //
    // Assuming "Hollywood Poker Pro" title as a default, we handle the
    // differences via some booleans.
   
    variant.hppingame = false;
    variant.starball = variant.starballingame = false;
    variant.ptc = false;

    // Hollywood Poker Pro title
    if (admin.checksum == 0x4f593264) {
        offsets.base = 0x1f400;
        offsets.trackTable = 0xdc4;
        offsets.songDefs = 0x1244;
        offsets.patterns = 0x1324;
        offsets.sampleHeaders = 0x1fd4;
        admin.songs = 1;  // actually 8 at different start positions
        writeBEword(pBuf,offsets.sampleHeaders+0x2*0x60+8+6,0x7e2);
        writeBEword(pBuf,offsets.sampleHeaders+0xf*0x60+8+6,0x7e2);
        sequencer.tables = 1;
    }
    // Hollywood Poker Pro ingame
    else if (admin.checksum == 0x32db24cc) {
        offsets.base = 0x1f400;
        offsets.trackTable = 0x19b0;
        offsets.songDefs = 0x27d8;
        offsets.patterns = 0x28b8;  // the last pattern pointers are filled with the address of the first sample header
        offsets.sampleHeaders = 0x39c0;
        admin.songs = 5;
        variant.hppingame = true;
        sequencer.tables = 1;
    }
    // Starball title
    else if (admin.checksum == 0xf195c217) {
        offsets.base = 0x7a9e;
        offsets.trackTable = 0xe56;
        offsets.songDefs = 0x10d6;
        offsets.patterns = 0x11b6;
        offsets.sampleHeaders = 0x1c86;
        sequencer.tables = 3;  // it uses separate arrays for PT, ST, TR values
        sequencer.tableSize = 0x20;
        sequencer.step.size = 1;
        admin.songs = 1;
        variant.starball = true;
    }
    // Starball ingame
    else if (admin.checksum == 0x48ea5657) {
        offsets.base = 0x4be0;
        offsets.trackTable = 0x10a2;
        offsets.songDefs = 0x1562 +2;  // +2 because first song is silent
        offsets.patterns = 0x1642;
        offsets.sampleHeaders = 0x2642;
        sequencer.tables = 3;  // it uses separate arrays for PT, ST, TR values
        sequencer.tableSize = 0x40;
        sequencer.step.size = 1;
        admin.songs = 6;
        variant.starball = variant.starballingame = true;
    }
    // PTC
    else if (admin.checksum == 0xd9511944) {
        offsets.base = 0x0;
        offsets.trackTable = 0xdac;
        offsets.songDefs = 0x198c;
        offsets.patterns = 0x1a8c;  // but no array of pattern offsets
        offsets.sampleHeaders = 0x2e0c;
        sequencer.tables = 3;  // it uses separate arrays for PT, ST, TR values
        sequencer.tableSize = 0xa0;
        sequencer.step.size = 1;
        admin.songs = 3;
        variant.ptc = true;
        setRate(100<<8);
    }

    // ----------

    admin.startSong = (songNumber < 0) ? 0 : songNumber;
    restart();

    // Determine duration with a dry-run till song-end.
    duration = 0;
    bool loopModeBak = loopMode;
    loopMode = false;
    do {
        duration += run();
    } while ( !songEnd && (duration<1000*60*59));
    loopMode = loopModeBak;
#if defined(DEBUG)
    cout << "Duration of " << input.path << "  #" << admin.startSong << "  ";
    dumpTimestamp(duration);
    cout << endl;
#endif

    restart();
#if defined(DEBUG)
    cout << "initialized" << endl;
#endif
    return admin.initialized;
}
    
// With an initialized decoder, calling this should (re)start the
// decoder currently chosen song.
void DNSDecoder::restart() {
    // Assign default values to essential runtime variables.
    for (ubyte t=0; t<sequencer.tracks; t++) {
        Track& tr = track[t];
        tr.num = t;
        tr.off = false;
        tr.keyDown = false;
        tr.assignedVoiceNum = 0xff;
        tr.PT = 0;
    }
    for (ubyte vNum=0; vNum<voices; ++vNum) {
        VoiceVars& v = voiceVars[vNum];
        v.envelope.phase = EnvPhase::END;
        v.envelope.volume = 0;
        v.envelope.duration = 0xffff;
        v.envelope.keyUp = true;

        v.sampleHeader = 0;
        v.pipelineState = 0;
        v.ch->off();
        toPaulaLength(v,1);
        toPaulaStart(v,offsets.silence);
        v.ch->paula.volume = 0;
        v.ch->paula.period = 0;
    }

    softRestart();
    processSequencer();
    
    admin.nextVoiceNum = 0;
    admin.initialized = true;
}

void DNSDecoder::softRestart() {
    songEnd = false;
    songPosCurrent = 0;
    tickFPadd = 0;
    admin.looped = false;

    uword so = admin.startSong<<1;
    sequencer.step.first = sequencer.step.current = readBEuword(pBuf,offsets.songDefs+so);
    sequencer.step.loop = readBEuword(pBuf,offsets.songDefs+0x20+so);
    sequencer.step.last = readBEuword(pBuf,offsets.songDefs+0x40+so);
    sequencer.step.currentOffset = sequencer.step.current * sequencer.step.size;
    // Not set by any of the modules/songs.
    uword trackMute = readBEuword(pBuf,offsets.songDefs+0x80+so);
    for (ubyte t=0; t<sequencer.tracks; t++) {
        track[t].off = ((trackMute & (1<<t)) != 0);
    }
    // The others end a pattern with a special value.
    if (variant.starball) {
        sequencer.pattern.length = 2*readBEuword(pBuf,offsets.songDefs+0xc0+so);
    }
    else if (variant.ptc) {
        sequencer.pattern.length = readBEuword(pBuf,offsets.songDefs+0xe0+so);
    }
    else {
        sequencer.pattern.length = 0;  // means we need to check for pattern end flag
    }
    
    admin.speed = readBEuword(pBuf,offsets.songDefs+0x60+so);
    admin.count = 1;  // quick start
#if defined(DEBUG)
    cout << "Sequencer: " << tohex((uword)sequencer.step.first)
         << " to " << tohex((uword)sequencer.step.last)
         << " / speed " << tohex(admin.speed) << endl;
#endif
}

// ----------------------------------------------------------------------

int DNSDecoder::run() {
    if (!admin.initialized) {
        return 0;
    }
    if ( !songEnd || loopMode ) {
        // The main play loop differs slightly between variants.
        // Not a big deal, but because voices get assigned dynamically,
        // we don't want to deviate from the machine code players much.

        if (variant.starball || variant.ptc) {
            updateVoices();
            if (--admin.count == 0) {
                admin.count = admin.speed;
#if defined(DEBUG_RUN)
    dumpTimestamp(songPosCurrent);
#endif
                for (ubyte t=0; t<sequencer.tracks; t++) {
                    processPattern( track[t] );
                }
                sequencer.pattern.pos += 2;
#if defined(DEBUG_RUN)
    cout << endl;
#endif
                if (sequencer.pattern.pos >= sequencer.pattern.length) {
                    processSequencer();
                }
            }
        }
        else {
            if (--admin.count <= 0) {
                admin.count = admin.speed;
                if (sequencer.step.next) {  // pattern can trigger next step
                    processSequencer();
                }
#if defined(DEBUG_RUN)
    dumpTimestamp(songPosCurrent);
#endif
                for (ubyte t=0; t<sequencer.tracks; t++) {
                    processPattern( track[t] );
                }
                sequencer.pattern.pos += 2;
#if defined(DEBUG_RUN)
    cout << endl;
#endif
            }
            updateVoices();
        }

    }
    tickFPadd += tickFP;
    int tick = tickFPadd>>8;
    tickFPadd &= 0xff;
    songPosCurrent += tick;
    return tick;
}

void DNSDecoder::processSequencer() {
#if defined(DEBUG_RUN)
    cout << "  Step = " << tohex(sequencer.step.current) << " : ";
#endif
    // Since the sequencer advances to next step before tracks and
    // patterns are processed, handling song end must be deferred to
    // next run of sequencer.
    if (admin.looped) {  // end seen before?
        songEnd = true;
        if (songEnd && loopMode) {
            songEnd = false;
            admin.looped = false;
        }
    }

    udword o = offsets.trackTable + sequencer.step.currentOffset;
    for (ubyte t=0; t<sequencer.tracks; t++) {
        Track& tr = track[t];
        if (sequencer.tables == 1) {  // interleaved parameters
            tr.PT = pBuf[o];
            tr.TR = (sbyte)pBuf[o+1];
            tr.ST = pBuf[o+2];
            o += 4;
        }
        else {  // each parameter in its own table
            uword gap = sequencer.tracks*sequencer.tableSize;
            tr.PT = pBuf[o];
            tr.TR = (sbyte)pBuf[o+gap];
            tr.ST = pBuf[o+2*gap];
            o += sequencer.tableSize;
        }
#if defined(DEBUG_RUN)
        cout << tohex(tr.PT) << ' ' << tohex((ubyte)(tr.TR&0xff)) << ' ' << tohex(tr.ST) << " | ";
#endif
    }
#if defined(DEBUG_RUN)
    cout << endl;
#endif
    // Advance to next step.
    if (++sequencer.step.current < sequencer.step.last) {
        sequencer.step.currentOffset += sequencer.step.size;
    }
    else {  // Sometimes the loop is to a silent step, though.
        sequencer.step.current = sequencer.step.loop;
        sequencer.step.currentOffset = sequencer.step.current * sequencer.step.size;
        // Trigger song end next time we run the sequencer.
        admin.looped = true;
    }
    sequencer.step.next = false;
    sequencer.pattern.pos = 0;
}

void DNSDecoder::processPattern(Track& tr) {
    // Check track mute and pattern number.
    if (tr.off || tr.PT==0) {
#if defined(DEBUG_RUN)
        cout << "  ----";;
#endif
        return;
    }
    udword pattOffs = getPattOffset( tr.PT -1 );
    // Check pattern end flag for the variant that doesn't define
    // pattern length.
    if (sequencer.pattern.length == 0 &&
        readBEuword(pBuf,pattOffs+sequencer.pattern.pos+2) == 0xffff) {
        sequencer.step.next = true;
    }
    // First word from pattern.
    uword pattVal = readBEuword(pBuf,pattOffs+sequencer.pattern.pos);
#if defined(DEBUG_RUN)
    cout << "  " << tohex(pattVal);
#endif
    if (pattVal == 0) {
        return;
    }
    if (pattVal == 0x8000) {  // key up event
        tr.keyDown = false;
        triggerVoiceKeyUp(tr);
        return;
    }
    // TODO
    // Pattern value is a note command.
    if ( tr.keyDown ) {
        if ( !variant.ptc ) {       // only PTC doesn't do this here
            triggerVoiceKeyUp(tr);  // for previously used voice
        }
    }
    tr.keyDown = true;
        
    // NB! An inconsistency in the original players. Sample number
    // is in lowest four bits, and even if only three bits are
    // used by the published modules because of sound transpose
    // parameter, after this >>2 shift, the lowest bit may be set
    // accidentally. Not a big deal, though.
    if (variant.starball || variant.ptc) {
        tr.pattern.addVolume = (pattVal&0xff) >>2;
    }
    else if (variant.hppingame) {
        tr.pattern.addVolume = (pattVal&0xf0) >>2;
    }
    else {
        tr.pattern.addVolume = (pattVal>>4) & 0x0f;
    }
    tr.pattern.sample = (pattVal&0x0f) + tr.ST;
    tr.pattern.note = (((pattVal>>8) & 0xff) + tr.TR) +1;
    triggerNote(tr);
}

void DNSDecoder::triggerVoiceKeyUp(Track& tr) {
    ubyte voiceNum = tr.assignedVoiceNum;
    if (voiceNum == 0xff) {
        return;
    }
    tr.assignedVoiceNum = 0xff;  // track released this voice

    VoiceVars& v = voiceVars[voiceNum];
    if (variant.starballingame) {
        if (v.envelope.duration >= 0x7fff) {
            return;
        }
    }
    v.envelope.duration |= 0x8000;  // make it much "older"
    // These two together ensure Release phase is started.
    v.envelope.keyUp = true;
    v.envelope.phase = EnvPhase::DECAY;
    nextEnvelopePhase(v);
}

void DNSDecoder::triggerNote(Track& tr) {
    ubyte voiceNum;
    if (voices == 4) {
        voiceNum = chooseVoice();
    }
    else {  // just for fun, if increasing voices to 6
        voiceNum = tr.num;
    }
    tr.assignedVoiceNum = voiceNum;
    VoiceVars& v = voiceVars[voiceNum];

    v.sampleHeader = offsets.sampleHeaders + tr.pattern.sample*0x60;
    if (tr.pattern.note < 0x30) {
        v.period = periods[tr.pattern.note];
    }
    else {  // none of the modules/songs cause a note value overflow
        v.period = 0;
    }             
    v.ch->off();
    udword sh = v.sampleHeader;
    toPaulaStart(v,readBEudword(pBuf,sh)+offsets.sampleData);
    toPaulaLength(v,readBEuword(pBuf,sh+6));
    if (variant.starball || variant.ptc) {
        v.pipelineState = 2;
    }
    else {
        v.pipelineState = 3;
    }

    v.envelope.phase = EnvPhase::ATTACK;
    v.envelope.keyUp = false;
    v.envelope.duration = 0;
    // Copy envelope parameters.
    v.envelope.strength = (sword)readBEuword(pBuf,sh+0x1a);
    v.envelope.speed = readBEuword(pBuf,sh+0x18);
    v.envelope.count = v.envelope.speed;
    v.envelope.volume = capVolume( readBEuword(pBuf,sh+0x1c) + tr.pattern.addVolume );
    v.envelope.targetVolume = capVolume( readBEuword(pBuf,sh+0x1e) + tr.pattern.addVolume );
    v.envelope.decaySpeed = readBEuword(pBuf,sh+0x20);
    v.envelope.decayStrength = (sword)readBEuword(pBuf,sh+0x22);
    v.envelope.setSustain = (readBEuword(pBuf,sh+0x24) == 1);
    v.envelope.sustainVolume = readBEuword(pBuf,sh+0x26);
    v.envelope.releaseSpeed = readBEuword(pBuf,sh+0x28);
    v.envelope.releaseStrength = (sword)readBEuword(pBuf,sh+0x2a);

    if (variant.starball || variant.ptc) {
        v.ch->paula.volume = v.envelope.volume;
    }
    else {
        v.ch->paula.volume = 0x32;  // overwritten in updateVoices() though
    }

    sword detune = (sword)readBEuword(pBuf,sh+0x4a);
    sword period = v.period + detune;
    // Because of the detune value we need to check the resulting
    // period as to avoid going below Paula's "lowest" period.
    if (period < 0x71) {
        period = 0x71;
    }
    v.ch->paula.period = period;
}

udword DNSDecoder::getPattOffset(ubyte pt) {
    if (variant.ptc) {
        return offsets.header + offsets.patterns + pt*0x60 - offsets.base;
    }
    else {
        return offsets.header + ( readBEudword(pBuf,offsets.patterns+(pt<<2)) - offsets.base );
    }
}

ubyte DNSDecoder::chooseVoice() {
    ubyte chosenVoiceNum = admin.nextVoiceNum;
    ubyte tryVoiceNum = chosenVoiceNum;
    uword cmpValue = 0;
    for (int i=0; i<voices; ++i) {
        VoiceVars& v = voiceVars[tryVoiceNum];
        if (cmpValue < v.envelope.duration) {
            cmpValue = v.envelope.duration;
            chosenVoiceNum = tryVoiceNum;
        }
        if (tryVoiceNum == 0) {
            tryVoiceNum = voices;
        }
        --tryVoiceNum;
    }
    admin.nextVoiceNum += 2;
    if (admin.nextVoiceNum >= voices) {
        admin.nextVoiceNum -= voices;
    }

    // Remove the voice from the track it is assigned to.
    for (int i=0; i<sequencer.tracks; ++i) {
        if (track[i].assignedVoiceNum == chosenVoiceNum) {
            track[i].assignedVoiceNum = 0xff;
        }
    }
    return chosenVoiceNum;
}

void DNSDecoder::updateVoices() {
    for (int i=0; i<voices; ++i) {
        VoiceVars& v = voiceVars[i];
        v.ch->paula.volume = v.envelope.volume;
        processEnvelope(v);
        processPaulaPipeline(v);
    }
}

void DNSDecoder::processPaulaPipeline(VoiceVars& v) {
    if (variant.starball || variant.ptc) {
        ++v.envelope.duration;
    }
    switch (v.pipelineState) {
    case 3:
        {
            --v.pipelineState;
            break;
        }
    case 2:
        {
            if (variant.starball || variant.ptc) {
                v.envelope.duration = 0;
            }
            v.ch->on();
            --v.pipelineState;
            break;
        }
    case 1:
        {
            udword sh = v.sampleHeader;
            toPaulaStart(v,readBEudword(pBuf,sh+0x08)+offsets.sampleData);
            toPaulaLength(v,readBEuword(pBuf,sh+0x0e));
            --v.pipelineState;
            break;
        }
    case 0:
    default:
        {
            break;
        }
    }
}

// ----------------------------------------------------------------------
// ADSR volume envelope including key up/down events.

void DNSDecoder::processEnvelope(VoiceVars& v) {
    if (v.envelope.volume == 0) {
        if (variant.starball || variant.ptc) {
            v.envelope.duration = 0xfff0;
        }
        else {
            v.envelope.duration = 0xffff;
        }
        return;
    }
    if ( !variant.starball && !variant.ptc) {
        ++v.envelope.duration;
    }

    if (v.envelope.phase == EnvPhase::END) {
        return;
    }
    else if (v.envelope.phase != EnvPhase::RELEASE) {
        v.envelope.duration &= 0x7fff;
    }

    if ( --v.envelope.count != 0) {
        return;
    }
    v.envelope.count = v.envelope.speed;

    v.envelope.volume += v.envelope.strength;
    if (v.envelope.strength < 0) {  // down?
        if (v.envelope.volume > v.envelope.targetVolume) {
            return;
        }
    }
    else {  // up
        if (v.envelope.volume < v.envelope.targetVolume) {
            return;
        }
    }
    v.envelope.volume = v.envelope.targetVolume;
    nextEnvelopePhase(v);
}

void DNSDecoder::nextEnvelopePhase(VoiceVars& v) {
    switch (v.envelope.phase) {
    case EnvPhase::ATTACK:
        {
            // start Decay
            v.envelope.targetVolume = v.envelope.sustainVolume;
            v.envelope.strength = v.envelope.decayStrength;
            v.envelope.speed = v.envelope.decaySpeed;
            v.envelope.count = v.envelope.speed;
            v.envelope.phase = EnvPhase::DECAY;
            break;
        }
    case EnvPhase::DECAY:
        {
            // only on key up start Release
            if (!v.envelope.keyUp) {
                return;
            }
            if (v.envelope.setSustain) {
                v.envelope.volume = v.envelope.sustainVolume;
            }
            v.envelope.targetVolume = 0;
            v.envelope.strength = v.envelope.releaseStrength;
            v.envelope.speed = v.envelope.releaseSpeed;
            v.envelope.count = v.envelope.speed;
            v.envelope.phase = EnvPhase::RELEASE;
            break;
        }
    case EnvPhase::RELEASE:
    default:
        {
            v.envelope.phase = EnvPhase::END;
            break;
        }
    }
}

uword DNSDecoder::capVolume(uword volume) {
    uword v = (volume > 64) ? 64 : volume;
    return v;
}

// ----------------------------------------------------------------------

// Cache the original pair of Paula start+length parameters,
// so it can be checked and adjusted in case of out of bounds
// access to sample data area.
void DNSDecoder::toPaulaStart(VoiceVars& v, udword offset) {
    v.paulaOrig.offset = offset;
    v.ch->paula.start = makeSamplePtr( offset );
    takeNextBufChecked(v);
}

void DNSDecoder::toPaulaLength(VoiceVars& v, uword length) {
    v.paulaOrig.length = length;
    v.ch->paula.length = length;
    takeNextBufChecked(v);
}

void DNSDecoder::takeNextBufChecked(VoiceVars& v) {
    if (v.paulaOrig.offset >= input.len) {  // start outside sample data space
        v.ch->paula.start = makeSamplePtr( offsets.silence );
        v.ch->paula.length = 1;
    }
    // end outside sample data space
    else if ( (v.paulaOrig.offset+(v.paulaOrig.length<<1)) > input.len ) {
        v.ch->paula.length = (input.len - v.paulaOrig.offset)>>1;
        v.ch->paula.start = makeSamplePtr( v.paulaOrig.offset );
    }
    // not needed by players that always set start before length
    else {
        v.ch->paula.length = v.paulaOrig.length;
        v.ch->paula.start = makeSamplePtr( v.paulaOrig.offset );
    }
    v.ch->takeNextBuf();
}

ubyte* DNSDecoder::makeSamplePtr(udword offset) {
    return(pBuf.tellBegin() + offset);
}

// ----------------------------------------------------------------------

int DNSDecoder::getSongs() {
    return admin.songs;
}

ubyte DNSDecoder::getVoices() {
    return voices;
}

void DNSDecoder::setPaulaVoice(ubyte v, PaulaVoice* p) {
    voiceVars[v].ch = p;
}
// rePlayer begin
void DNSDecoder::setPath(std::string path, LoaderCallback loaderArg, void* loaderDataArg) {
    input.path = path;
    input.loader = loaderArg;
    input.loaderData = loaderDataArg;
// rePlayer end
    // Since most modules in these formats don't store a title/name internally,
    // this helper function contructs a name from the filename.
    name.clear();
    std::size_t found = path.find_last_of("/\\");
    if (found != std::string::npos) {
        std::string fname = path.substr(found+1);
        std::string mdatLC = "dns.";
        std::string mdatUC = "DNS.";
        if (fname.compare(0,mdatLC.length(),mdatLC) == 0 ||
            fname.compare(0,mdatUC.length(),mdatUC) == 0) {
            name = fname.substr( mdatLC.length() );
        }
        else {
            // Strip the filename extension.
            name = fname.substr(0, fname.rfind('.'));
        }
    }
}

bool DNSDecoder::loadSamplesFile() {
    // Got both the DNS and SMP file?
    if ( input.smplLoaded ) {  // if loaded before, reuse it
        input.mdatSize = input.mdatSizeCurrent;
        input.smplSize = input.smplSizeCurrent;
        return true;
    }
    if ( input.path.empty() ) {
        return false;
    }

    typedef std::pair<std::string,std::string> ExtPair;
    std::vector<ExtPair> vExtPairs = {
        { "dns.", "smp." },
        { "DNS.", "SMP." },
        { ".dns", ".smp" },
        { ".DNS", ".SMP" }
    };
    for ( std::vector<ExtPair>::iterator it = vExtPairs.begin(); it != vExtPairs.end(); ++it ) {
        std::string pathSmpl = input.path;
        std::size_t found = pathSmpl.rfind(it->first);
        if (found != std::string::npos) {
            pathSmpl.replace(found,it->first.length(),it->second);
#if 0 // rePlayer begin
            std::ifstream fIn( pathSmpl, std::ios::in|std::ios::binary|std::ios::ate );
            if ( fIn.is_open() ) {
                //fIn.seekg(0,std::ios::end);
                uint32_t fLen = fIn.tellg();
                fIn.seekg(0,std::ios::beg);

                char* newInputBuf = new char[fLen+input.bufLen];
                memcpy(newInputBuf,input.buf,input.bufLen);
                delete[] input.buf;
                input.buf = reinterpret_cast<ubyte*>(newInputBuf);

                fIn.read(newInputBuf+input.len,fLen);
                if (fIn.bad()) {
                    return false;
                }
                fIn.close();
                input.smplSize = fLen;
                input.mdatSize = input.len - offsets.header;
                offsets.sampleData = input.len;
                input.bufLen += input.smplSize;
                input.len += input.smplSize;

                // Update smart pointers.
                pBuf.setBuffer(input.buf,input.bufLen);
                return true;
            }
#else
            auto res = input.loader(input.loaderData, pathSmpl.c_str());
            if (res.first)
            {
                char* newInputBuf = new char[res.second + input.bufLen];
                memcpy(newInputBuf, input.buf, input.bufLen);
                delete[] input.buf;
                input.buf = reinterpret_cast<ubyte*>(newInputBuf);

                memcpy(newInputBuf + input.len, res.first, res.second);
                delete[] res.first;

                input.smplSize = res.second;
                input.mdatSize = input.len - offsets.header;
                offsets.sampleData = input.len;
                input.bufLen += input.smplSize;
                input.len += input.smplSize;

                // Update smart pointers.
                pBuf.setBuffer(input.buf, input.bufLen);
                return true;
            }
#endif // rePlayer end
        }
    }  // next ExtPair
    return false;
}

// ----------------------------------------------------------------------

const uword DNSDecoder::periods[0x30] = {
    0x06ae, 0x064e, 0x05f4, 0x059e, 0x054d, 0x0501, 0x04b9, 0x0475,
    0x0435, 0x03f9, 0x03c0, 0x038c, 
    // +0x0c
    0x0358, 0x032a, 0x02fc, 0x02d0, 0x02a8, 0x0282, 0x025e, 0x023b, 
    0x021b, 0x01fd, 0x01e0, 0x01c6,
    // +0x18
    0x01ac, 0x0194, 0x017d, 0x0168, 0x0154, 0x0140, 0x012f, 0x011e,
    0x010e, 0x00fe, 0x00f0, 0x00e3, 
    // +0x24
    0x00d6, 0x00ca, 0x00bf, 0x00b4, 0x00aa, 0x00a0, 0x0097, 0x008f,
    0x0087, 0x007f, 0x0078, 0x0071
    // +0x30
};

}  // namespace
