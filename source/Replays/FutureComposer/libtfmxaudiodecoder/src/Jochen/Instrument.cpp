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
#include <cmath>

#include "HippelDecoder.h"
#include "Analyze.h"

namespace tfmxaudiodecoder {

// Activate the specified instrument, i.e. set the volume modulation
// sequence and sound modulation sequence.
// Compatible with all players.
void HippelDecoder::setInstrument(VoiceVars& voiceX, ubyte instr) {
    voiceX.currentVolSeq = instr;
    udword seqOffs = getVolSeq(instr);
    
    voiceX.envelopeSpeed = voiceX.envelopeCount = fcBuf[seqOffs++];
    // (NOTE) Future Composer GUI did not validate modulation sequence
    // definitions, and the result of an illegal envelope speed of 0
    // would be implementation dependent and would affect external players.
    // Handle validation in processPerVol();
    
    ubyte sound = fcBuf[seqOffs++];  // sound modulation sequence number
    voiceX.vibSpeed = fcBuf[seqOffs++];
    voiceX.vibFlag = 0x40;  // vibrato up at start
    voiceX.vibAmpl = voiceX.vibCurOffs = fcBuf[seqOffs++];
    voiceX.vibDelay = fcBuf[seqOffs++];
    voiceX.volSeq = seqOffs;  // envelope start
    voiceX.volSeqPos = 0;
    voiceX.volSustainTime = 0;

    analyze->gatherSndSeq(sound);  // also if it's $80
    // Set sound sequence.
    // Older TFMX and FC set it directly.
    // COSO and newer TFMX players can modify it via note arg #2 bit 6.
    // TFMX 7V keeps the current sound sequence, if the new one is $80.
    if ( !traits.volSeqSnd80 ||  // FC and some TFMX players
         (sound != 0x80) ) {     // 7 voices players
        // COSO and sometimes also TFMX.
        if ( traits.porta40SetSnd && (voiceX.pattVal2 & 0x40) != 0 ) {
            sound = voiceX.portaSpeed;
        }
        // MCMD
        else if ( traits.porta80SetSnd && (voiceX.pattVal2 & 0x80) != 0 ) {
            sound = voiceX.portaSpeed;
        }
        // Original behaviour of TFMX and unchanged in FC.
        voiceX.currentSndSeq = sound;
        voiceX.sndSeq = getSndSeq(sound);
        voiceX.sndSeqPos = 0;
        voiceX.sndModSustainTime = 0;
    }

    analyze->gatherVibrato(voiceX);
}

void HippelDecoder::setWave(VoiceVars& voiceX, ubyte num) {
    voiceX.pSampleStart = samples[num].pStart;
    voiceX.ch->paula.start = samples[num].pStart;
    voiceX.ch->paula.length = samples[num].len;
    voiceX.ch->takeNextBuf();
    voiceX.repeatOffset = samples[num].repOffs;
    voiceX.repeatLength = samples[num].repLen;
    voiceX.repeatDelay = 3;

    analyze->gatherSampleNum(num);
}

// ----------------------------------------------------------------------
// Sound modulation sequence.

void HippelDecoder::TFMX_soundModulation(VoiceVars& voiceX) {
    admin.readModRecurse = 0;
    vModFunc.clear();
    vModFunc.push_back( &HippelDecoder::TFMX_processSndModSustain );
    while ( !vModFunc.empty() && (++admin.readModRecurse < RECURSE_LIMIT) ) {
        ModFuncPtr pMF = vModFunc.back();
        vModFunc.pop_back();
        (this->*pMF)(voiceX);
    }
    TFMX_processPerVol(voiceX);
}

void HippelDecoder::TFMX_processSndModSustain(VoiceVars& voiceX) {
    if (voiceX.sndModSustainTime == 0) {
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
    }
    else {
        --voiceX.sndModSustainTime;
        if (traits.skipToWaveMod) {  // some player versions
            vModFunc.push_back( &HippelDecoder::TFMX_waveModulation );
        }
    }
}

void HippelDecoder::TFMX_processSndModSeq(VoiceVars& voiceX) {
    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos;
    ubyte f = fcBuf[seqOffs];
    // Commands are E0 to EA. Positive bytes are transpose values,
    // negative bytes are pitch (aka locked notes). But table of periods
    // contains only 0x48 values, so theoretically no valid pitch value is
    // larger than 0xc8 (0x48+0x80) and thus cannot conflict with Ex cmds.
    // Some player versions read the next value following a command
    // (and its parameters) as a pitch/trans value.
    if (f<0xe0) {
        vModFunc.push_back( &HippelDecoder::TFMX_waveModulation );
        vModFunc.push_back( &HippelDecoder::TFMX_sndSeqTrans );
    }
    else if (f<0xeb) {
        analyze->gatherSndSeqCmd(voiceX.currentSndSeq,f);
        f -= 0xe0;
        vModFunc.push_back( TFMX_sndModFuncs[f] );
    }
}

void HippelDecoder::TFMX_sndSeq_UNDEF(VoiceVars& voiceX) {
    if (traits.sndSeqToTrans)
        vModFunc.push_back( &HippelDecoder::TFMX_sndSeqTrans );
    else if (traits.sndSeqGoto)
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::TFMX_sndSeq_E0(VoiceVars& voiceX) {  // loop
    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;
    voiceX.sndSeqPos = fcBuf[seqOffs++]&0x3f;
    vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::TFMX_sndSeq_E1(VoiceVars& voiceX) {  // end
    // basically NOP
}

void HippelDecoder::TFMX_sndSeq_E1_waveMod(VoiceVars& voiceX) {
    vModFunc.push_back( &HippelDecoder::TFMX_waveModulation );
}

void HippelDecoder::TFMX_sndSeq_E2(VoiceVars& voiceX) {  // set wave
    prepareChannelUpdate(voiceX);

    voiceX.volRandomThreshold = 0;
    voiceX.currentWave = 0xff;
    voiceX.waveModSwitch1 = 0;
    // Restart envelope.
    voiceX.volSeqPos = 0;
    voiceX.envelopeCount = 1;
        
    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;
    setWave(voiceX,fcBuf[seqOffs++]);
    voiceX.sndSeqPos += 2;
    
    if (traits.sndSeqToTrans)
        vModFunc.push_back( &HippelDecoder::TFMX_sndSeqTrans );
    else if (traits.sndSeqGoto)
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::TFMX_sndSeq_E3(VoiceVars& voiceX) {  // set vibrato
    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;
    voiceX.vibSpeed = fcBuf[seqOffs++];
    voiceX.vibAmpl = fcBuf[seqOffs++];
    voiceX.sndSeqPos += 3;
    
    analyze->gatherVibrato(voiceX);

    if (traits.sndSeqToTrans)
        vModFunc.push_back( &HippelDecoder::TFMX_sndSeqTrans );
    else if (traits.sndSeqGoto)
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::TFMX_sndSeq_E4(VoiceVars& voiceX) {  // new wave
    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;
    setWave(voiceX,fcBuf[seqOffs++]);
    voiceX.repeatDelay = 0;
    voiceX.sndSeqPos += 2;
    voiceX.waveModSwitch1 = 0;

    if (traits.sndSeqToTrans)
        vModFunc.push_back( &HippelDecoder::TFMX_sndSeqTrans );
    else if (traits.sndSeqGoto)
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::TFMX_sndSeq_E5_repOffs(VoiceVars& voiceX) {
    prepareChannelUpdate(voiceX);

    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;
    ubyte s = fcBuf[seqOffs++];  // sound number
    ubyte x = fcBuf[seqOffs++];  // repLen multiplier

    Sample snd = samples[s];
    analyze->gatherSampleNum(s);
    snd.startOffs += snd.repLen*x;
    snd.len = snd.repLen;
    snd.repOffs += snd.repLen*x;  // relative to startOffs
    snd.assertBoundaries(fcBuf);

    voiceX.pSampleStart = snd.pStart;
    voiceX.ch->paula.start = voiceX.pSampleStart;
    voiceX.ch->paula.length = snd.len;
    voiceX.ch->takeNextBuf();
    voiceX.repeatOffset = snd.repOffs;
    voiceX.repeatLength = snd.repLen;
    voiceX.repeatDelay = 3;

    voiceX.volSeqPos = 0;
    voiceX.envelopeCount = 1;
    voiceX.sndSeqPos += 3;

    if (traits.sndSeqGoto)
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::TFMX_sndSeq_E5(VoiceVars& voiceX) {
    voiceX.volRandomThreshold = 0;
    prepareChannelUpdate(voiceX);

    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;

    ubyte s = fcBuf[seqOffs++];
    analyze->gatherSampleNum(s);
    voiceX.waveModStartOffs = samples[s].startOffs;
    voiceX.waveModEndOffs = samples[s].startOffs+samples[s].len+samples[s].len;

    uword p0 = fcBuf[seqOffs++];
    p0 <<= 8;
    p0 |= fcBuf[seqOffs++];
    if (p0 == 0xffff) {
        p0 = samples[s].len;
    }
    voiceX.waveModCurrentOffs = p0;

    uword p1 = fcBuf[seqOffs++];
    p1 <<= 8;
    p1 |= fcBuf[seqOffs++];
    voiceX.waveModLen = p1;

    sword p2 = fcBuf[seqOffs++];
    p2 <<= 8;
    p2 |= fcBuf[seqOffs++];
    voiceX.waveModOffs = p2;

    voiceX.waveModSpeedCount = 0;
    voiceX.waveModSpeed = fcBuf[seqOffs++];
    voiceX.waveModInit = true;  // the first step copies modified repOffs/repLen from seq
    voiceX.waveModSwitch1 = 0xff;
    voiceX.waveModSwitch2 = 0;
    voiceX.volSeqPos = 0;
    voiceX.envelopeCount = 1;
    voiceX.sndSeqPos += 9;

    if (traits.sndSeqGoto)
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::TFMX_sndSeq_E6(VoiceVars& voiceX) {
    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;
    uword p1 = fcBuf[seqOffs++];
    p1 <<= 8;
    p1 |= fcBuf[seqOffs++];
    voiceX.waveModLen = p1;

    sword p2 = fcBuf[seqOffs++];
    p2 <<= 8;
    p2 |= fcBuf[seqOffs++];
    voiceX.waveModOffs = p2;
    
    voiceX.waveModSpeedCount = 0;
    voiceX.waveModSpeed = fcBuf[seqOffs++];
    voiceX.waveModInit = true;  // the first step copies modified repOffs/repLen from seq
    voiceX.waveModSwitch2 = 0;
    voiceX.sndSeqPos += 6;
    
    if (traits.sndSeqGoto)
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::TFMX_sndSeq_E7(VoiceVars& voiceX) {  // set seq
    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;
    uword seq = fcBuf[seqOffs++];
    voiceX.currentSndSeq = seq;
    voiceX.sndSeq = getSndSeq(seq);
    voiceX.sndSeqPos = 0;

    // Default for E7 set seq.
    vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::TFMX_sndSeq_E7_setDiffWave(VoiceVars& voiceX) {
    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;
    ubyte s = fcBuf[seqOffs++];
    if (s != voiceX.currentWave) {
        voiceX.volRandomThreshold = 0;
        voiceX.currentWave = s;
        prepareChannelUpdate(voiceX);
        setWave(voiceX,s);
    }
    voiceX.waveModSwitch1 = 0;
    voiceX.volSeqPos = 0;
    voiceX.envelopeCount = 1;
    voiceX.sndSeqPos += 2;
    
    if (traits.sndSeqToTrans)
        vModFunc.push_back( &HippelDecoder::TFMX_sndSeqTrans );
    else if (traits.sndSeqGoto)
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::TFMX_sndSeq_E8(VoiceVars& voiceX) {  // set sustain
    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;
    voiceX.sndModSustainTime = fcBuf[seqOffs++];
    voiceX.sndSeqPos += 2;

    vModFunc.push_back( &HippelDecoder::TFMX_processSndModSustain );
}

void HippelDecoder::TFMX_sndSeq_E9(VoiceVars& voiceX) {
    voiceX.volRandomThreshold = 0;
    voiceX.currentWave = 0xff;
    prepareChannelUpdate(voiceX);

    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;

    uword s = fcBuf[seqOffs++];  // sample/pack nr.
    analyze->gatherSampleNum(s);
    udword sndOffs = samples[s].startOffs;

    // Some of the players don't check for the "SSMP" tag!
    if ( !traits.checkSSMPtag ||
         SSMP_HEX == readBEudword(fcBuf,sndOffs) ) {
        uword packLen = readBEuword(fcBuf,sndOffs+4);
        uword packOffs = readBEuword(fcBuf,sndOffs+6);
        sndOffs += 8;
        // Start of sample data within sample-pack.
        udword samplesOffset = sndOffs+packLen*24+packOffs*4;
        
        s = fcBuf[seqOffs++];  // sample nr. within pack
        sndOffs += s*24;
        
        udword startOffset = readBEudword(fcBuf,sndOffs);
        udword endOffset = readBEudword(fcBuf,sndOffs+4);
        startOffset &= 0xfffffffe;
        endOffset &= 0xfffffffe;
        uword len = (endOffset-startOffset)>>1;  // in words

        Sample snd;
        snd.startOffs = startOffset+samplesOffset;
        snd.len = len;
        snd.repOffs = 0;  // relative to startOffs
        snd.repLen = 1;
        snd.assertBoundaries(fcBuf);

        voiceX.pSampleStart = snd.pStart;
        voiceX.ch->paula.start = voiceX.pSampleStart;
        voiceX.ch->paula.length = snd.len;
        voiceX.ch->takeNextBuf();
        voiceX.repeatOffset = snd.repOffs;
        voiceX.repeatLength = snd.repLen;
        fcBuf[snd.startOffs+snd.repOffs+1] = fcBuf[snd.startOffs+snd.repOffs];
        voiceX.repeatDelay = 3;

        voiceX.waveModSwitch1 = 0;
        // Restart envelope.
        voiceX.volSeqPos = 0;
        voiceX.envelopeCount = 1;
    }
    else {
        // Apparently, no FC module triggers this.
        // TODO: Check TFMX modules.
    }
    voiceX.sndSeqPos += 3;

    if (traits.sndSeqToTrans)
        vModFunc.push_back( &HippelDecoder::TFMX_sndSeqTrans );
    else if (traits.sndSeqGoto)
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::TFMX_sndSeq_EA_skip(VoiceVars& voiceX) {  // NOP + skip
    voiceX.sndSeqPos += 2;
}

void HippelDecoder::TFMX_sndSeq_EA(VoiceVars& voiceX) {  // volume randomization
    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;

    if (traits.randomVolume) {
        voiceX.volRandomThreshold = fcBuf[seqOffs++];
        voiceX.volRandomCurrent = 0;
    }
    voiceX.sndSeqPos += 2;
    
    if (traits.sndSeqGoto)
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::TFMX_sndSeqTrans(VoiceVars& voiceX) {
    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos;
    // Not a command, but a transpose value.
    voiceX.seqTranspose = fcBufS[seqOffs];
    ++voiceX.sndSeqPos;
    
    analyze->gatherSeqTrans(voiceX.currentSndSeq, voiceX.seqTranspose);
}

void HippelDecoder::TFMX_waveModulation(VoiceVars& voiceX) {
    // NOP if it's off by default via switch1.
    if (voiceX.waveModSwitch1==0 || voiceX.waveModSwitch2!=0 ||
        --voiceX.waveModSpeedCount>=0 ) {
        return;
    }
    voiceX.waveModSpeedCount = voiceX.waveModSpeed;

    sdword offs = voiceX.waveModCurrentOffs;
    uword len = voiceX.waveModLen;
    if ( voiceX.waveModInit ) {
        voiceX.waveModInit = false;
    }
    else {
        offs += voiceX.waveModOffs;
        if (offs < 0) {
            voiceX.waveModSwitch2 = 0xff;
            offs -= voiceX.waveModOffs;  // revert underflow
        }
        else {
            udword repeatStartOffs = voiceX.waveModStartOffs+(offs<<1);
            udword repeatEndOffs = repeatStartOffs+len+len;
            if (repeatEndOffs > voiceX.waveModEndOffs) {
                voiceX.waveModSwitch2 = 0xff;
                offs -= voiceX.waveModOffs;  // revert overflow
            }
        }
    }
    voiceX.waveModCurrentOffs = offs;

    Sample snd;
    snd.startOffs = voiceX.waveModStartOffs+(offs<<1);
    snd.len = len;
    snd.repOffs = 0;  // relative to snd.startOffs
    snd.repLen = len;
    snd.assertBoundaries(fcBuf);

    voiceX.ch->paula.start = snd.pStart;
    voiceX.ch->paula.length = snd.len;
    voiceX.ch->takeNextBuf();
    voiceX.repeatOffset = snd.repOffs;  // = start offset
    voiceX.repeatLength = snd.len;
    voiceX.repeatDelay = 0;
}

// ----------------------------------------------------------------------
// (NOTE) The lowest octave in the period table is unreachable
// due to a hardcoded range-check (see bottom).

// Determine period and volume.
void HippelDecoder::TFMX_processPerVol(VoiceVars& voiceX) {
    voiceX.outputVolume = TFMX_envelope(voiceX);
    
    // Now determine note and period value to play.
    sbyte t = voiceX.seqTranspose;
    if (t >= 0) {
        t += voiceX.pattVal1;
        t += voiceX.transpose;
    }
    // Loop into low octave at 0x48 downwards, if excessive transpose
    // would underflow by just a few semitones accidentally.
    if (t<0 && (t&0x7f)>=0x78) {
        t += 0x48;
    }
    // else: lock note (i.e. transpose value from sequence is note to play)
    ubyte note = t&0x7f;
    sword period;
    if (traits.lowerPeriods) {
        note = t&0x3f;
        period = periodsLower[note];
    }
    else {
        period = periods[note];
    }
    // Check it a first time here, so we can keep the non-standard
    // double rate period values in the same array.
    if (period < traits.periodMin) {
        period = traits.periodMin;
    }

    period = (this->*pVibratoFunc)(voiceX,period,note);
    period = (this->*pPortamentoFunc)(voiceX,period);

    // Final range-check.
    if (period < traits.periodMin) {
        period = traits.periodMin;
    }
    else if (period > traits.periodMax) {
        period = traits.periodMax;
    }
    voiceX.outputPeriod = period;
}

// ----------------------------------------------------------------------

// Future Composer introduced its own "SSMP" sample packs.
void HippelDecoder::FC_sndSeq_E9(VoiceVars& voiceX) {
    voiceX.volRandomThreshold = 0;
    voiceX.currentWave = 0xff;
    prepareChannelUpdate(voiceX);

    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;
    uword s = fcBuf[seqOffs++];  // sample/pack nr.
    analyze->gatherSampleNum(s);

    if (s < 10) {  // sample or waveform?
        udword sndOffs = samples[s].startOffs;
        if ( SSMP_HEX == readBEudword(fcBuf,sndOffs) ) {
            sndOffs += 4;
            // Skip header and 10*2 info blocks of size 16.
            udword smpStart = sndOffs+320;
            s = fcBuf[seqOffs++];  // sample nr.
            s <<= 4;  // *16 (block size)
            sndOffs += s;
                
            Sample snd;
            snd.startOffs = smpStart + readBEudword(fcBuf,sndOffs);
            snd.len = readBEuword(fcBuf,sndOffs+4);;
            snd.repOffs = readBEuword(fcBuf,sndOffs+6);
            snd.repLen = readBEuword(fcBuf,sndOffs+8);
            snd.assertBoundaries(fcBuf);

            voiceX.pSampleStart = snd.pStart;
            voiceX.ch->paula.start = voiceX.pSampleStart;
            voiceX.ch->paula.length = snd.len;
            voiceX.ch->takeNextBuf();
            voiceX.repeatDelay = 3;
        
            // (FC14 BUG-FIX): Players set period here by accident.
            // m68k code move.l 4(a2),4(a3), but 6(a3) is period.
                
            voiceX.repeatOffset = snd.repOffs;
            voiceX.repeatLength = snd.repLen;
            if (voiceX.repeatLength == 1) {
                // Erase first word behind sample to avoid beeping
                // one-shot mode upon true emulation of Paula.
                fcBuf[smpStart+voiceX.repeatOffset] = 0;
                fcBuf[smpStart+voiceX.repeatOffset+1] = 0;
            }
            // Restart envelope.
            voiceX.volSeqPos = 0;
            voiceX.envelopeCount = 1;
        }        
    }
    voiceX.sndSeqPos += 3;

    if (traits.sndSeqToTrans)
        vModFunc.push_back( &HippelDecoder::TFMX_sndSeqTrans );
    else if (traits.sndSeqGoto)
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

void HippelDecoder::FC_sndSeq_EA_pitchbend(VoiceVars& voiceX) {  // FC modification
    udword seqOffs = voiceX.sndSeq+voiceX.sndSeqPos+1;
    voiceX.pitchBendSpeed = fcBufS[seqOffs+1];
    voiceX.pitchBendTime = fcBuf[seqOffs+2];
    voiceX.sndSeqPos += 3;
    
    if (traits.sndSeqToTrans)
        vModFunc.push_back( &HippelDecoder::TFMX_sndSeqTrans );
    else if (traits.sndSeqGoto)
        vModFunc.push_back( &HippelDecoder::TFMX_processSndModSeq );
}

}  // namespace
