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

namespace tfmxaudiodecoder {

// For the publicly available TFMX modules, the array of offsets to
// each macro script is located either at the end of the file (for
// compressed MDAT) or before the track table (for uncompressed MDAT)
// and, apparently, always _not before_ the array of pattern offsets.
//
// Thus, for truncated (!) MDAT one or more macro offsets may be undefined.
// For example, if the memory following the MDAT were cleared to 0 on
// Amiga, it would point at the beginning of the MDAT which would be wrong.
// Or it may point anywhere, if other data or the SMPL data are stored
// directly after the MDAT. Furthermore, macro script data may be missing,
// too.
//
// With smart pointer access, an OOB offset is no threat. The underlying
// implementation would read value 0. Since we store MDAT+SMPL data
// within the same buffer, for truncated MDAT we may read from SMPL
// space by mistake. But, and that is an important BUT, we cannot fix
// truncated data. Imagine cases like a Goto/Cont macro command jumping
// to a bad offset. Rejecting that would be wrong. And it could also be
// damaged data, not just truncated data.
//
// So, nothing is won if trying to reject obviously bad offsets here by
// applying range checks (e.g. to see whether an offset is < input.mdatSize)
// and handling the return value in the calling function, too.
udword TFMXDecoder::getMacroOffset(ubyte macro) {
    return offsets.header + readBEudword(pBuf,offsets.macros+((macro&0x7f)<<2));
}

void TFMXDecoder::dumpMacros() {
    // Unfortunately, there is nothing like a TFMX structure field that
    // tells how many macros have been defined. So, if we want to dump all
    // macros, all that's there is the unterminated list of macro offsets.
    // And some macros may hold data for random play.
#if defined(DEBUG)  
    int macro = -1;
    udword prevMacroStart = 0;
    do {
        macro++;
        udword macroStart = getMacroOffset(macro);
        if ( (macroStart < prevMacroStart) ||
             (macroStart == offsets.header) ||
             (macroStart >= offsets.sampleData) ) {
            // Not a macro offset or out of bounds, so end here.
            break;
        }
        prevMacroStart = macroStart;

        udword macroEnd = getMacroOffset(macro+1);
        if ( (macroEnd < macroStart) ||
             (macroEnd == getPattOffset(0)) ||
             (macroEnd == offsets.header) ||
             (macroEnd > offsets.sampleData) ) {
            // Not a macro offset or out of bounds, so find a valid end offset.
            macroEnd = offsets.patterns;
            if (macroEnd < macroStart) {  // still bad?
                if (getPattOffset(0) > macroStart) {
                    macroEnd = getPattOffset(0);
                }
                else {
                    macroEnd = offsets.sampleData;
                }
            }
        }
        // Skip all the "empty" macros.
        if (pBuf[macroStart]==4 && pBuf[macroStart+4]==7 ) {
            continue;
        }

        cout << "Macro 0x" << hexB(macro)
             << " at 0x" << tohex(macroStart) << " to 0x" << tohex(macroEnd) << endl;
        int step = 0;
        while (macroStart < macroEnd) {
            int cmd = 0x3f & pBuf[macroStart];
            cout << "  " << hexW(step)
                 << ' ' << hexB(cmd)
                 << ' ' << hex << std::setw(6) << std::setfill('0') << (int)(0x00ffffff&readBEudword(pBuf,macroStart))
                 << ' ' << MacroDefs[cmd]->text << endl;
            if (cmd == 7) {
                break;
            }
			macroStart += 4;
            step++;
		};

	} while (true);
#endif  // DEBUG
}

// ----------------------------------------------------------------------

void TFMXDecoder::initMacro(VoiceVars& voice) {
    voice.macro.step = 0;
    voice.macro.wait = 0;
    voice.macro.loop = 0xff;
    voice.macro.state = -1;
    voice.waitOnDMACount = 0;
    if (variant.execOrder == MAC_MOD_SEQ) {
        voice.effectsMode = 0;
    }
    else {
        voice.effectsMode = 1;
    }
    voice.macro.extraWait = true;
}

void TFMXDecoder::processMacroMain(VoiceVars& voice) {
    if (voice.macro.state == 0) {
        return;
    }
    else if (voice.macro.state == 1) {
        initMacro(voice);
    }
    else {
        if (voice.macro.wait>0 ) {
            voice.macro.wait--;
            return;
        }
    }

    int macroLen = 0;
    do {
        macroEvalAgain = false;
        udword p = voice.macro.offset + (voice.macro.step<<2);
        int command = pBuf[p];
        cmd.aa = 0;
        cmd.bb = pBuf[p+1];
        cmd.cd = pBuf[p+2];
        cmd.ee = pBuf[p+3];
#if defined(DEBUG_RUN)
        // Dump the macro step BEFORE masking the command number!
        cout << std::hex << std::setw(1) << '[' << (int)voice.voiceNum << "] "
             << tohex(voice.macro.offset) << '/'
             << hexW(voice.macro.step) << ' '
             << hexB(command) << ' '
             << std::setw(6) << std::setfill('_') << (int)(0x00ffffff&makeDword(0,cmd.bb,cmd.cd,cmd.ee));
#endif
        macroCmdUsed[command&0x3f] = true;
        MacroDef* pM = MacroDefs[command&0x3f];
#if defined(DEBUG_RUN)
        // At this point the descriptive text tells which macro command
        // will be executed.
        cout << ' ' << pM->text << endl;
#endif
        (this->*pM->ptr)(voice);
    }
    while ( macroEvalAgain && (++macroLen < 32) );
}

// ----------------------------------------------------------------------

// In the official TFMX v1.5 and v2.2 releases, an extra wait is
// hardcoded in specific locations as an undocumented implementation
// detail.
//
// Only variants of the player could set it to ON/OFF via the DMAoff
// macro commands $00 and $13 in conjunction with delayed DMACON write.
// By default, it is set to $ff (ON, true). Else >= 0 (OFF, false).
// Only a very few TFMX files use customized DMAoff.
void TFMXDecoder::macroFunc_ExtraWait(VoiceVars& voice) {
    voice.macro.step++;
    if (voice.macro.extraWait || variant.extraWaitV1) {
        return;
    }
    // No extra wait, but reset it to enabled.
    voice.macro.extraWait = true;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_NOP(VoiceVars& voice) {
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_StopSound(VoiceVars& voice) {
    voice.envelope.flag = 0;
    voice.vibrato.time = 0;
    voice.portamento.speed = 0;
    voice.sid.targetLength = 0;
    voice.rnd.flag = 0;
    // The variant that also does AddVolume/SetVolume.
    macroFunc_StopSample_sub(voice);
}

void TFMXDecoder::macroFunc_StartSample(VoiceVars& voice) {
    // There are variants of the "DMAon" macro command, which
    // are not needed because we don't emulate access to Amiga
    // custom chip registers like DMACON, INTENA and INTREQ.
    if ( variant.noDelayedDMAon ) {
        voice.ch->on();
    }
    else {
        voice.macro.delayedOn = true;
    }
    voice.macro.step++;

    // Variants of the player can set the macro wait value here.
    // The high byte (in cmd.aa) is set to zero early, so only the
    // low byte (in cmd.bb) would matter. However, as the low byte
    // is 0 for all but a very few TFMX files, the resulting wait
    // value would stay at 0, which would be pointless.
    //
    // Furthermore, of the existing files that run a subsequent Wait
    // command, that wait value would take precedence. It can be
    // assumed that setting the wait value here is not the real goal.
    //
    // Instead, of the few remaining files that set the first parameter
    // to 1, they want the player to run sound synthesis via the effects
    // processor a first time before turning on the audio channel.
    if (cmd.bb != 0) {
        voice.effectsMode = 1;
        if (variant.execOrder == MOD_MAC_SEQ) {
            processModulation(voice);
        }
    }
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_SetBegin(VoiceVars& voice) {
    voice.addBeginCount = 0;
    udword start = offsets.sampleData + makeDword(0,cmd.bb,cmd.cd,cmd.ee);
    macroFunc_SetBegin_sub( voice, start );
}

void TFMXDecoder::macroFunc_SetBegin_sub(VoiceVars& voice, udword start) {
    voice.sample.start = start;
    toPaulaStart(voice,start);
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_SetLen(VoiceVars& voice) {
    uword len = makeWord(cmd.cd,cmd.ee);
    voice.sample.length = len;
    toPaulaLength(voice,len);
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_Wait(VoiceVars& voice) {
    if ((cmd.bb&1)!=0) {  // special behaviour related to macro command Random Play
        if (voice.rnd.blockWait) {
            return;
        }
        voice.rnd.blockWait = true;
        voice.macro.step++;
        macroEvalAgain = true;
    }
    else {  // normal behaviour
        voice.macro.wait = makeWord(cmd.cd,cmd.ee);
        macroFunc_ExtraWait(voice);
    }
}

void TFMXDecoder::macroFunc_Loop(VoiceVars& voice) {
    if (voice.macro.loop == 0) {
        voice.macro.loop = 0xff;
        voice.macro.step++;
        // Possibly unique to R-Type, which does an extra wait here
        // unlike TFMX v1 and later.
        if (variant.macroLoopExtraWait) {
            return;
        }
    }
    else {
        if (voice.macro.loop == 0xff) {
            voice.macro.loop = cmd.bb-1;
        }
        else {
            voice.macro.loop--;
        }
        voice.macro.step = makeWord(cmd.cd,cmd.ee);
    }
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_Cont(VoiceVars& voice) {
    voice.macro.offset = getMacroOffset(cmd.bb&0x7f);
    voice.macro.step = makeWord(cmd.cd,cmd.ee);
    voice.macro.loop = 0xff;
    voice.macro.branchIfSame = false;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_Stop(VoiceVars& voice) {
    voice.macro.state = 0;
}

void TFMXDecoder::macroFunc_AddNote(VoiceVars& voice) {
    macroFunc_AddNote_sub(voice,voice.note,voice.detune);
    macroFunc_ExtraWait(voice);
}

void TFMXDecoder::macroFunc_AddNote_sub(VoiceVars& voice, ubyte noteAdd, sword detuneAdd) {
    sbyte n = noteAdd+(sbyte)cmd.bb;
    uword p = noteToPeriod(n);
    sword finetune = detuneAdd + (sword)makeWord(cmd.cd,cmd.ee);
    if (variant.finetuneUnscaled) {
        p += finetune;
    }
    else if (finetune != 0) {
        p = ((finetune+0x100)*p)>>8;
    }
    voice.period = p;
    if (voice.portamento.speed == 0) {
        voice.outputPeriod = p;
    }
}

void TFMXDecoder::macroFunc_SetNote(VoiceVars& voice) {
    sword detune = voice.detune;
    // TFMX v1 SetNote ignores voice detune.
    if (variant.setNoteV1) {
        detune = 0;
    }
    macroFunc_AddNote_sub(voice,0,detune);
    macroFunc_ExtraWait(voice);
}

void TFMXDecoder::macroFunc_Reset(VoiceVars& voice) {
    voice.addBeginCount = 0;
    voice.envelope.flag = 0;
    voice.vibrato.time = 0;
    voice.portamento.speed = 0;
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_Portamento(VoiceVars& voice) {
    voice.portamento.count = cmd.bb;
    voice.portamento.wait = 1;
    if (variant.portaOverride || (voice.portamento.speed == 0)) {
        voice.portamento.period = voice.period;
    }
    voice.portamento.speed = makeWord(cmd.cd,cmd.ee);
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_Vibrato(VoiceVars& voice) {
    voice.vibrato.time = cmd.bb;
    // Original v1 and v2 apply this bit mask here.
    // Since a composer may have entered an uneven vibrato parameter,
    // the masked value can affect vibrato amplitude.
    if (variant.vibratoTimeMask) {
        voice.vibrato.time &= 0xfe;
    }
    voice.vibrato.count = cmd.bb>>1;
    voice.vibrato.intensity = cmd.ee;
    if (voice.portamento.speed == 0) {
        voice.outputPeriod = voice.period;
        voice.vibrato.delta = 0;
    }
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_AddVolume(VoiceVars& voice) {
    ubyte vol = voice.noteVolume + voice.noteVolume + voice.noteVolume;
    vol += cmd.ee;  // ignore cmd.cd as only a byte value is used
    voice.volume = vol;
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_AddVolNote(VoiceVars& voice) {
    // Replaces AddVolume by default. Potentially harmless,
    // since 'bb' arg must be set to 0xfe in order to activate the
    // extra behaviour, and AddVolume doesn't use 'bb' arg, so
    // it's set to 0 in macro scripts.
    if (cmd.cd == 0xfe) {
        ubyte ee = cmd.ee;
        cmd.cd = cmd.ee = 0;
        macroFunc_AddNote_sub(voice,voice.note,voice.detune);
        cmd.ee = ee;
    }
    ubyte vol = voice.noteVolume + voice.noteVolume + voice.noteVolume;
    vol += cmd.ee;  // ignore cmd.cd as only a byte value is used
    voice.volume = vol;
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_SetVolume(VoiceVars& voice) {
    if (cmd.cd == 0xfe) {  // +AddNote variant
        ubyte ee = cmd.ee;
        cmd.cd = cmd.ee = 0;
        macroFunc_AddNote_sub(voice,voice.note,voice.detune);
        cmd.ee = ee;
    }
    voice.volume = cmd.ee;
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_Envelope(VoiceVars& voice) {
    voice.envelope.speed = cmd.bb;
    voice.envelope.flag = voice.envelope.count = cmd.cd;
    voice.envelope.target = cmd.ee;
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_LoopKeyUp(VoiceVars& voice) {
    if (!voice.keyUp) {
        macroFunc_Loop(voice);
    }
    else { 
        voice.macro.step++;
        macroEvalAgain = true;
    }
}

void TFMXDecoder::macroFunc_AddBegin(VoiceVars& voice) {
    // If the .bb parameter is non-zero, that's extra behaviour not
    // supported by old TFMX (particularly not by the official v1.5
    // and v2.2 releases and some variants used during that era).
    // Those don't use that parameter, it defaults to 0, and they
    // accept only a 16-bit offset. Lacking the count parameter,
    // the AddBegin macro command had to be run whenever wanting
    // to apply the sample offset.
    //
    // Modernized TFMX variants introduced the count parameter as
    // to trigger automatic updates of the sample offset during
    // modulation/effects processing.
    //
    // So, the core of this macro implementation can become the default.
    //
    // However: Some music files based on the old TFMX design set
    // that parameter by mistake when entering a negative offset
    // (and expanding the two's complement value to 24 bits and
    // storing the upper bits in the first .bb parameter). That would
    // activate the modernized behaviour and cause a conflict, if
    // the player added automatic updates to the effect.
    //
    // NB! We handle variant.noAddBeginCount during modulation.
    voice.addBeginCount = voice.addBeginArg = cmd.bb;
    sdword offset = (sword)makeWord(cmd.cd,cmd.ee);
    voice.addBeginOffset = offset;

    udword begin = voice.sample.start + offset;
    if (begin < offsets.sampleData) {
        begin = offsets.sampleData;
    }
        
    if (voice.sid.targetLength == 0) {
        macroFunc_SetBegin_sub( voice, begin );
    }
    else {
        voice.sample.start = begin;
        voice.sid.sourceOffset = begin;

        voice.macro.step++;
        macroEvalAgain = true;
   }
}

void TFMXDecoder::macroFunc_AddLen(VoiceVars& voice) {
    voice.macro.step++;
    macroEvalAgain = true;

    sword len = (sword)makeWord(cmd.cd,cmd.ee);
    voice.sample.length += len;
    if (voice.sid.targetLength == 0) {
        toPaulaLength(voice,voice.sample.length);
    }
    else {
        voice.sid.sourceLength = len;
    }
}

void TFMXDecoder::macroFunc_StopSample(VoiceVars& voice) {
    macroFunc_StopSample_sub(voice);
}

void TFMXDecoder::macroFunc_StopSample_sub(VoiceVars& voice) {
    voice.macro.step++;

    // Rare variants of TFMX implement it as a count value, but no module
    // sets the value to anything above 1.
    if (cmd.bb != 0) {
        voice.macro.extraWait = false;
        voice.macro.delayedOff = true;
    }
    else {
        voice.ch->off();
        voice.macro.extraWait = true;
        macroEvalAgain = true;
    }
    // The variant that also does AddVolume/SetVolume.
    if (cmd.cd == 0) {
        if (cmd.ee == 0) {
            return;
        }
        ubyte vol = voice.noteVolume+voice.noteVolume+voice.noteVolume;
        voice.volume = vol+makeWord(cmd.cd,cmd.ee);
    }
    else {
        voice.volume = cmd.ee;
    }
    // Apply current fade volume.
    sbyte vol = voice.volume;
    if (fade.volume < 64) {
        vol = (4*voice.volume*fade.volume)/(4*0x40);
    }
    voice.ch->paula.volume = vol;
}

void TFMXDecoder::macroFunc_WaitKeyUp(VoiceVars& voice) {
    if (voice.keyUp) {
        voice.macro.step++;
        macroEvalAgain = true;
    }
    else {
        if (voice.macro.loop == 0) {
            voice.macro.loop = 0xff;
            voice.macro.step++;
            macroEvalAgain = true;
        }
        else if (voice.macro.loop == 0xff) {
            voice.macro.loop = cmd.ee-1;
        }
        else {
            voice.macro.loop--;
        }
    }
}

void TFMXDecoder::macroFunc_Goto(VoiceVars& voice) {
    voice.macro.offsetSaved = voice.macro.offset;
    voice.macro.stepSaved = voice.macro.step;
    macroFunc_Cont(voice);
}

void TFMXDecoder::macroFunc_Return(VoiceVars& voice) {
    voice.macro.offset = voice.macro.offsetSaved;
    voice.macro.step = voice.macro.stepSaved;
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_SetPeriod(VoiceVars& voice) {
    uword period = makeWord(cmd.cd,cmd.ee);
    voice.period = period;
    if (voice.portamento.speed == 0) {
        voice.outputPeriod = period;
    }
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_SampleLoop(VoiceVars& voice) {
    udword offset = makeDword(0,cmd.bb,cmd.cd,cmd.ee);
    voice.sample.start += offset;
    voice.sample.length -= (offset>>1);
    
    toPaulaStart(voice,voice.sample.start);
    toPaulaLength(voice,voice.sample.length);
    
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_OneShot(VoiceVars& voice) {
    voice.addBeginCount = 0;
    voice.sample.start = offsets.sampleData;
    voice.sample.length = 1;
    
    toPaulaStart(voice,voice.sample.start);
    toPaulaLength(voice,voice.sample.length);
    
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_WaitOnDMA(VoiceVars& voice) {
    // The rarely used variant where 'cdee' arg is the number of waits.
    voice.waitOnDMACount = makeWord(cmd.cd,cmd.ee);
    voice.macro.state = 0;
    voice.waitOnDMAPrevLoops = voice.ch->getLoopCount();
    macroFunc_ExtraWait(voice);
}

void TFMXDecoder::macroFunc_RandomPlay(VoiceVars& voice) {
    voice.rnd.macro = cmd.bb;
    voice.rnd.speed = cmd.cd;
    voice.rnd.mode = cmd.ee;
    voice.rnd.count = 1;
    voice.rnd.flag = 1;
    randomPlay(voice);
    voice.rnd.blockWait = true;
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_SplitKey(VoiceVars& voice) {
    if (cmd.bb >= voice.note) {
        voice.macro.step++;
    }
    else {
        voice.macro.step = makeWord(cmd.cd,cmd.ee);
    }
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_SplitVolume(VoiceVars& voice) {
    if (cmd.bb >= voice.volume) {
        voice.macro.step++;
    }
    else {
        voice.macro.step = makeWord(cmd.cd,cmd.ee);
    }
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_RandomMask(VoiceVars& voice) {
    voice.rnd.mask = cmd.bb;
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_SetPrevNote(VoiceVars& voice) {
    // Non-existant in TFMX v1, so add voice detune by default.
    macroFunc_AddNote_sub(voice,voice.notePrevious,voice.detune);
    macroFunc_ExtraWait(voice);
}

void TFMXDecoder::macroFunc_PlayMacro(VoiceVars& voice) {
    cmd.aa = voice.note;
    cmd.cd |= (voice.noteVolume<<4);
    noteCmd();
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_22(VoiceVars& voice) {  // SID setbeg
    voice.addBeginCount = 0;
    
    voice.sid.sourceOffset = offsets.sampleData + makeDword(0,cmd.bb,cmd.cd,cmd.ee);
    voice.sample.start = voice.sid.sourceOffset;

    toPaulaStart(voice, offsets.sampleData + voice.sid.targetOffset);
    
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_23(VoiceVars& voice) {  // SID setlen
    uword len = makeWord(cmd.aa,cmd.bb);
    if (len == 0) {
        len = 0x100;
    }
    toPaulaLength(voice,len>>1);
    voice.sid.targetLength = (len-1) & 0xff;
    
    uword len2 = makeWord(cmd.cd,cmd.ee);
    voice.sid.sourceLength = len2;
    voice.sample.length = len2;
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_24(VoiceVars& voice) {  // SID op3 ofs
    voice.sid.op3.offset = makeDword(cmd.bb,cmd.cd,cmd.ee,0);
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_25(VoiceVars& voice) {  // SID op3 frq
    // SID op3 modifies SOURCE sample start offset.
    voice.sid.op3.speed = voice.sid.op3.count = makeWord(cmd.aa,cmd.bb);
    voice.sid.op3.delta = makeWord(cmd.cd,cmd.ee);
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_26(VoiceVars& voice) {  // SID op2 ofs
    voice.sid.op2.offset = makeDword(0,cmd.bb,cmd.cd,cmd.ee);
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_27(VoiceVars& voice) {  // SID op2 frq
    // SID op2 modifies step freq.
    voice.sid.op2.speed = voice.sid.op2.count = makeWord(cmd.aa,cmd.bb);
    voice.sid.op2.delta = makeWord(cmd.cd,cmd.ee);
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_28(VoiceVars& voice) {  // SID op1
    // SID op1 modifies interpolation step.
    voice.sid.op1.interDelta = makeWord(cmd.ee,voice.sid.op1.interDelta&0xff);  // change the high-byte, keep the low-byte
    voice.sid.op1.interMod = 16*(sbyte)cmd.cd;
    voice.sid.op1.speed = voice.sid.op1.count = makeWord(cmd.aa,cmd.bb);
    voice.macro.step++;
    macroEvalAgain = true;
}

void TFMXDecoder::macroFunc_29(VoiceVars& voice) {  // SID stop
    voice.macro.step++;
    voice.sid.targetLength = 0;
    
    if (cmd.bb != 0) {
        voice.sid.op1.speed = voice.sid.op1.count = 0;
        voice.sid.op1.interDelta = voice.sid.op1.interMod = 0;
        voice.sid.op2.speed = voice.sid.op2.count = 0;
        voice.sid.op2.offset = 0;
        voice.sid.op2.delta = 0;
        voice.sid.op3.speed = voice.sid.op3.count = 0;
        voice.sid.op3.offset = 0;
        voice.sid.op3.delta = 0;
    }
    macroEvalAgain = true;
}

// Macro command $30. Available in e.g. Gem'Z and Turrican III players,
// used also by Denny (unreleased game), but the way it's used, it only
// triggers in Gem'Z soundtrack, because on new note and Cont/Goto the
// boolean variable is reset.
//
// If this voice uses the same macro script since last note,
// branch to specified macro position.
void TFMXDecoder::macroFunc_BranchIfSame(VoiceVars& voice) {
    macroEvalAgain = true;
    if (voice.macro.branchIfSame) {
        voice.macro.step = makeWord(cmd.cd,cmd.ee);
    }
    else {
        voice.macro.branchIfSame = true;
        voice.macro.step++;
    }
}

// Macro command $31. Available in e.g. Gem'Z and Turrican III players,
// but only used by T3 title.
void TFMXDecoder::macroFunc_KeyUp(VoiceVars& voice) {
    cmd.aa = 0xf5;  // key up command
    noteCmd();      // with cmd.cd = channel number
    voice.macro.step++;
    macroEvalAgain = true;
}

}  // namespace
