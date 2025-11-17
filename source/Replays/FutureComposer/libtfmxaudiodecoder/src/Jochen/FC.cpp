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

#include <cstring>

#include "HippelDecoder.h"

namespace tfmxaudiodecoder {

const std::string HippelDecoder::FC14_FORMAT_NAME = "Future Composer 1.4 (AMIGA)";
const std::string HippelDecoder::FC14_TAG = "FC14";

bool HippelDecoder::FC_init(int songNumber) {
#if defined(DEBUG)
    cout << "FC_init()" << endl;
#endif
    admin.startSong = 0;  // format doesn't feature a (sub-)song table
    
    setRate(50<<8);
    stats.voices = 4;
    stats.songs = 1;

    // Although this is a constant for most of the formats, some define
    // it within the header, and we assign this here since some of the
    // support functions use it.
    traits.patternSize = PATTERN_LENGTH;
    
    trackColumnSize = TFMX_TRACKTAB_COLUMN_SIZE;
    trackStepLen = TFMX_TRACKTAB_STEP_SIZE+1;
    if (traits.isSMOD) {
        offsets.trackTable = SMOD_TRACKTAB_OFFSET;
    }
    else {
        offsets.trackTable = FC14_TRACKTAB_OFFSET;
    }
    trackTabLen = readBEudword(fcBuf,4);
    
    // At +8 is offset to pattern data.
    offsets.patterns = readBEudword(fcBuf,8);

    // Invalid track table length in "cult.smod".
    // This workaround is sufficient for this player implementation.
    if (trackTabLen == 0) {
        trackTabLen = offsets.patterns-offsets.trackTable;
    }
    
    // At +12 is length of patterns.
    // Divide by pattern length to get number of used patterns.
    // The editor is limited to 128 patterns.
    stats.patterns = readBEudword(fcBuf,12)/traits.patternSize;
    // At +16 is offset to first sound modulation sequence.
    offsets.sndModSeqs = readBEudword(fcBuf,16);
    // At +20 is total length of sound modulation sequences.
    // Divide by sequence length to get number of used sequences.
    // Each sequence is 64 bytes long.
    stats.sndSeqs = readBEudword(fcBuf,20)/64;
    // At +24 is offset to first volume modulation sequence.
    offsets.volModSeqs = readBEudword(fcBuf,24);
    // At +28 is total length of volume modulation sequences.
    // Divide by sequence length to get number of used sequences.
    // Each sequence is 64 bytes long.
    stats.volSeqs = readBEudword(fcBuf,28)/64;

    pStartSongFunc = &HippelDecoder::FC_startSong;
    pNextNoteFunc = &HippelDecoder::FC_nextNote;
    pSoundFunc = &HippelDecoder::TFMX_soundModulation;
    pVibratoFunc = &HippelDecoder::TFMX_vibrato;
    pPortamentoFunc = &HippelDecoder::FC_portamento_pitchbend;
    
    traits.vibScaling = true;
    traits.sndSeqToTrans = true;
    traits.sndSeqGoto = false;
    traits.volSeqEA = true;
    traits.checkSSMPtag = true;

    TFMX_sndModFuncs[5] = &HippelDecoder::TFMX_sndSeq_UNDEF;
    TFMX_sndModFuncs[6] = &HippelDecoder::TFMX_sndSeq_UNDEF;
    TFMX_sndModFuncs[7] = &HippelDecoder::TFMX_sndSeq_E7;
    TFMX_sndModFuncs[9] = &HippelDecoder::FC_sndSeq_E9;
    TFMX_sndModFuncs[10] = &HippelDecoder::FC_sndSeq_EA_pitchbend;

    // At +32 is module offset to start of samples.
    udword sampleOffset = offsets.sampleData = readBEudword(fcBuf,32);
    udword sampleHeader = offsets.sampleHeaders = SMOD_SMPHEADERS_OFFSET;  // unchanged in FC14

    // Max. 10 samples ($0-$9) or 10 sample-packs of 10 samples each.
    // Maximum sample length = 50000.
    // Previously: 32KB.
    // Total: 100000 (gee, old times).
    //
    // Sample length in words, repeat offset in bytes (but even),
    // Repeat length (in words, min=1). But in editor: all in bytes!
    //
    // One-shot sample (*recommended* method):
    // repeat offset = length*2 (to zero word at end of sample) and
    // replength = 1
    for (int sam = 0; sam < 10; sam++) {
        samples[sam].startOffs = sampleOffset;
        // Sample length in words.
        uword sampleLength = readBEuword(fcBuf,sampleHeader);
        samples[sam].len =  sampleLength;
        samples[sam].repOffs = readBEuword(fcBuf,sampleHeader+2);
        samples[sam].repLen = readBEuword(fcBuf,sampleHeader+4);
        samples[sam].assertBoundaries(fcBuf);

        // Safety treatment of "one-shot" samples.
        // 
        // We erase a word (two sample bytes) in the right place to
        // ensure that one-shot samples do not beep at their end
        // when looping on that part of the sample.
        //
        // It might not be strictly necessary to do this as it is
        // documented that FC is supposed to do this. But better be
        // safe than sorry.
        //
        // It is done because a cheap mixer is implemented which can
        // be used in the same way AMIGA custom chip Paula is used to
        // play quick-and-dirty one-shot samples.
        //
        // (NOTE) There is a difference in how one-shot samples are treated
        // in Future Composer 1.4 in comparison with older versions.
        if (traits.isSMOD) {
            // Check whether this is a one-shot sample.
            if (samples[sam].repLen==1) {
                fcBuf[sampleOffset] = fcBuf[sampleOffset+1] = 0;
            }
        }

        // Skip to start of next sample data.
        sampleOffset += sampleLength;
        sampleOffset += sampleLength;
        
        if ( !traits.isSMOD) {
            udword pos = sampleOffset;
            // Make sure we do not erase the sample-pack ID "SSMP"
            // and check whether this is a one-shot sample.
            if ( SSMP_HEX != readBEudword(fcBuf,pos) &&
                samples[sam].repLen == 1) {
                fcBuf[pos] = fcBuf[pos+1] = 0;
            }
            // FC 1.4 keeps silent word behind sample.
            // Now skip that one to the next sample.
            //
            // (BUG-FIX) Add +2 to sample address is incorrect
            // for unused (i.e. empty) samples.
            if (sampleLength != 0)
                sampleOffset += 2;
        }
        
        sampleHeader += 6;  // skip unused rest of header
    }

    // 80 waveforms ($0a-$59), max $100 bytes length each.
    if (traits.isSMOD) {
        stats.samples = 10+47;
        // Old FC has built-in waveforms.
        const ubyte* wavePtr = SMOD_waveforms;
        int infoIndex = 0;
        for (int wave = 0; wave < 47; wave++) {
            int sam = 10+wave;
            samples[sam].startOffs = SMOD_waveInfo[infoIndex++];
            samples[sam].pStart = wavePtr+samples[sam].startOffs;
            samples[sam].len = SMOD_waveInfo[infoIndex++];
            samples[sam].repOffs = SMOD_waveInfo[infoIndex++];
            samples[sam].repLen = SMOD_waveInfo[infoIndex++];
        }
      }
      else if ( !traits.isSMOD) {
        stats.samples = 10+80;
        // At +36 is module offset to start of waveforms.
        udword waveOffset = readBEudword(fcBuf,36);
        // Module offset to array of waveform lengths.
        udword waveHeader = FC14_WAVEHEADERS_OFFSET;
        for (int wave = 0; wave < 80; wave++) {
            int sam = 10+wave;
            samples[sam].startOffs = waveOffset;
            ubyte waveLength = fcBuf[waveHeader++];
            samples[sam].len = waveLength;
            samples[sam].repOffs = 0;
            samples[sam].repLen = waveLength;
            samples[sam].assertBoundaries(fcBuf);
            waveOffset += waveLength;
            waveOffset += waveLength;
        }
    }

    #include "LiveFix.h"

    (this->*pStartSongFunc)();
    return true;
}

void HippelDecoder::FC_startSong() {
#if defined(DEBUG)
    cout << "FC_startSong()" << endl;
#endif
    firstStep = lastStep = -1;  // by default play full track table
    
    // (BUG-FIX) Some FC players here read the speed from the first step.
    // This is the wrong speed if a sub-song is selected by skipping steps.
    // 
    // (NOTE) If it is skipped to a step where no replay step is specified,
    // the default speed is taken. This can be wrong. The only solution
    // would be to fast-forward the song, i.e. read the speed from all
    // steps up to the starting step.
    
    udword offs = offsets.trackTable+TFMX_TRACKTAB_STEP_SIZE;
    admin.startSpeed = fcBuf[offs];
    setTrackRange();
    reset();
}

// --------------------------------------------------------------------------

void HippelDecoder::FC_nextNote(VoiceVars& voiceX) {
    udword pattOffs = getPattOffs(voiceX);

    // Check for pattern end or whether pattern BREAK command is set.
    if (voiceX.pattPos==traits.patternSize
        || (!traits.isSMOD && fcBuf[pattOffs]==FC_PATTERN_BREAK)) {

        // (NOTE) In order for pattern break to work correctly, the
        // pattern break value 0x49 must be at the same position in
        // each of the four patterns which are currently activated
        // for the four voices.
        //
        // Alternatively, one could advance all voices to the next
        // track step here in a 4-voice loop to make sure voices
        // stay in sync.

        // This points at the new _current_ position.
        udword trackOffs = voiceX.trackStart+voiceX.trackPos;
        voiceX.pattPos = 0;

        // (BUG-FIX) Some FC players here apply a normal
        // compare-if-equal which is not accurate enough and
        // can cause the player to step beyond the song end.
        if (trackOffs >= voiceX.trackEnd) {  // pattern table end?
            voiceX.trackPos = 0;     // restart by default
            trackOffs = voiceX.trackStart;
            songEnd = true;
            // (NOTE) Some songs better stop here or reset all
            // channels to cut off any pending sounds.
        }

#if defined(DEBUG_RUN)
    if (voiceX.voiceNum==0) {
        cout << endl;
        dumpTimestamp(songPosCurrent);
        cout << "  Step = " << hexW(voiceX.trackPos/ trackStepLen);
        cout << " | " << hexW(voiceX.trackStart+voiceX.trackPos) << " of " << hexW(voiceX.trackStart) << " to " << hexW(voiceX.trackEnd) << endl;
        udword tmp = voiceX.trackStart+voiceX.trackPos;
        for (int t = 0; t < trackStepLen; ++t)
            cout << hexB(fcBuf[tmp++]) << ' ';
        cout << endl;
        cout << endl;
    }
#endif
        // Advance one step in track table.
        voiceX.trackPos += trackStepLen;
        
        // Track table entry:
        //
        //   Voice 1    Voice 2    Voice 3    Voice 4    Speed
        //   PT TR ST   PT TR ST   PT TR ST   PT TR ST   RS
        //
        // PT = PATTERN
        // TR = TRANSPOSE
        // ST = SOUND TRANSPOSE
        // RS = REPLAY SPEED

        // (NOTE) As to read song speed from the track table at the right
        // time, FC player implementations increase a value here with each
        // call of nextNote() in order to determine whether current voice
        // is 1. We do it differently.
        
        // Read new song speed from end of track step entry,
        // if this is first voice.
        if ( voiceX.voiceNum == 0 ) {
            ubyte newSpeed = fcBuf[trackOffs+12];  // RS
            if (newSpeed != 0) {  // 0 would be underflow
                admin.count = admin.speed = newSpeed;
            }
        }

        uword pattern = fcBuf[trackOffs++];  // PT
        voiceX.transpose = fcBufS[trackOffs++];  // TR
        voiceX.soundTranspose = fcBufS[trackOffs++];  // ST
        
        voiceX.pattStart = offsets.patterns+(pattern<<6);
        // Get new pattern pointer (pattPos is 0 already, see above).
        pattOffs = voiceX.pattStart;
    }

    // Process pattern entry.
    
    ubyte note = fcBuf[pattOffs++];
    ubyte info = fcBuf[pattOffs];
#if defined(DEBUG_RUN)
    dumpByte(note);
    dumpByte(info);
    cout << "| ";
#endif
    
    if (note != 0) {
        voiceX.portaSpeed = 0;  // stop port., erase old parameters
        voiceX.portaOffs = 0;
        
        // (BUG-FIX) Disallow signed underflow here.
        voiceX.pattVal1 = note&0x7f;
        
        // (NOTE) Since first note is 0x01, first period at array
        // offset 0 cannot be accessed directly (i.e. without adding
        // transpose values from track table or modulation sequence).
        
        prepareChannelUpdate(voiceX);

        // Get instrument/volModSeq number from info byte #1
        // and add sound transpose value from track table.
        uword sound = (info&0x3f)+voiceX.soundTranspose;
        // (FC14 BUG-FIX) Better mask here to take care of overflow.
        sound &= 0x3f;

        // (NOTE) Some FC players here put pattern info byte #1
        // into an unused byte variable at structure offset 9.

        setInstrument(voiceX,sound);
    }

    // Portamento: bit 7 set = ON, bit 6 set = OFF, bits 5-0 = speed
    // New note resets and clears portamento working values.

    if ((info&0x40) != 0) {   // portamento OFF?
        voiceX.portaSpeed = 0;
    }
    
    if ((info&0x80) != 0) {  // portamento ON?
        // Get portamento speed from info byte #2.
        // Info byte #2 is info byte #1 in next line of pattern,
        // Therefore the +2 offset.
        // (FC14 BUG-FIX) Kill portamento ON/OFF bits.
        voiceX.portaSpeed = fcBuf[pattOffs+2]&0x3f;
    }

    // Advance to next pattern entry.
    voiceX.pattPos += 2;
}

}  // namespace
