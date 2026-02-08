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

void TFMXDecoder::processModulation(VoiceVars& voice) {
    if (voice.effectsMode > 0) { 
        addBegin(voice);
        sid(voice);
        vibrato(voice);
        portamento(voice);
        envelope(voice);
    }
    else if (voice.effectsMode == 0) {
        voice.effectsMode = 1;
    }
    randomPlay(voice);
    fadeApply(voice);
}

// ----------------------------------------------------------------------

void TFMXDecoder::addBegin(VoiceVars& voice) {
    if (voice.addBeginCount == 0) {
        return;
    }
    voice.sample.start += voice.addBeginOffset;
    if (voice.sample.start < offsets.sampleData) {
    }
    if (voice.sid.targetLength != 0) {
        voice.sid.sourceOffset = voice.sample.start;
    }
    else {
        toPaulaStart(voice,voice.sample.start);
    }
    if (--voice.addBeginCount == 0) {
        voice.addBeginCount = voice.addBeginArg;
        voice.addBeginOffset *= -1;
    }
}

// ----------------------------------------------------------------------

void TFMXDecoder::envelope(VoiceVars& voice) {
    if (voice.envelope.flag == 0) {
        return;
    }
    if (voice.envelope.count > 0) {
        voice.envelope.count--;
        return;
    }
    voice.envelope.count = voice.envelope.flag;

    if (voice.volume < voice.envelope.target) {  // up
        voice.volume += voice.envelope.speed;
        if (voice.volume >= voice.envelope.target) {
            voice.volume = voice.envelope.target;
            voice.envelope.flag = 0;
        }
    }
    else {  // down
        voice.volume -= voice.envelope.speed;
        if (voice.volume <= voice.envelope.target) {
            voice.volume = voice.envelope.target;
            voice.envelope.flag = 0;
        }
    }
}

// ----------------------------------------------------------------------
// Vibrato with relative adjustment depending on period.

void TFMXDecoder::vibrato(VoiceVars& voice) {
    if (voice.vibrato.time == 0) {
        return;
    }
    voice.vibrato.delta += voice.vibrato.intensity;
    uword p;
    if (variant.vibratoUnscaled) {
        p = voice.period + voice.vibrato.delta;
    }
    else {
        p = ((0x800+voice.vibrato.delta)*voice.period)>>11;
    }
    if (voice.portamento.speed == 0) {
        voice.outputPeriod = p;
    }
    if (--voice.vibrato.count == 0) {
        voice.vibrato.count = voice.vibrato.time;
        voice.vibrato.intensity *= -1;
    }
}

// ----------------------------------------------------------------------
// Portamento with relative adjustment depending on period.

void TFMXDecoder::portamento(VoiceVars& voice) {
    if ( (voice.portamento.speed == 0) || (--voice.portamento.wait != 0) ) {
        return;
    }
    voice.portamento.wait = voice.portamento.count;

    uword current = voice.portamento.period;
    uword target = voice.period;  // target period
    
    if (current == target) {
        goto end;
    }
    else if (current < target) {
        // down, increase period
        if (variant.portaUnscaled) {
            current += voice.portamento.speed;
        }
        else {
            current = ((0x100+voice.portamento.speed)*current)>>8;
        }
        if (current < target) {
            goto set;
        }
        else {
            goto end;
        }
    }
    else {
        // up, decrease period
        if (variant.portaUnscaled) {
            current -= voice.portamento.speed;
        }
        current = ((0x100-voice.portamento.speed)*current)>>8;
        if (current > target) {
            goto set;
        }
        else {
            goto end;
        }
    }
    return;
 end:
    voice.portamento.speed = 0;
    current = target;
 set:
    current &= 0x7ff;
    voice.portamento.period = current;
    voice.outputPeriod = current;
}

// ----------------------------------------------------------------------
// Real-time synthesizer based on interpolating or sampling values
// from a varying source sample area.

void TFMXDecoder::sid(VoiceVars& voice) {
    if (voice.sid.targetLength == 0) {
        return;
    }
    ubyte* pTarget = pBuf.tellBegin() + offsets.sampleData + voice.sid.targetOffset;
    udword currSourceOffset = 0;
    udword x = voice.sid.op3.offset;
    ubyte d = voice.sid.op1.interDelta>>8;
    ubyte dx = 0;
    sword cur = voice.sid.lastSample;

    for (int i=voice.sid.targetLength; i>=0; i--) {
        x += voice.sid.op2.offset;
        currSourceOffset += x;
        // target sample
        sword s = (sbyte)pBuf[voice.sid.sourceOffset + ((currSourceOffset>>16) & voice.sid.sourceLength)];
        if (( d == 0 ) ||    // direct sampling mode
            ( s == cur )) {  // target reached
            *pTarget++ = s;
        }
        else if ( s > cur) {  // target higher
            cur += (d+dx);
            dx = (cur > 0x7f)? 1 : 0;
            if (cur > 0x7f || cur >= s) {  // overflow or reached
                cur = s;
                *pTarget++ = s;
            }
            else {  // s > cur 
                *pTarget++ = cur;
            }
        }
        else if (s < cur) {  // target lower
            cur -= (d+dx);
            dx = (cur < -128) ? 1 : 0;
            if (cur < -128 || cur <=s ) {  // underflow or reached
                cur = s;
                *pTarget++ = s;
            }
            else {  // cur > s
                *pTarget++ = cur;
            }
        }
    }
    voice.sid.lastSample = cur;

    if (d != 0) {
        voice.sid.op1.interDelta += voice.sid.op1.interMod;
        if (--voice.sid.op1.count == 0) {
            voice.sid.op1.count = voice.sid.op1.speed;
            voice.sid.op1.interMod *= -1;
        }
    }
    voice.sid.op3.offset += voice.sid.op3.delta;
    if (--voice.sid.op3.count == 0) {
        if ((voice.sid.op3.count = voice.sid.op3.speed) != 0) {
            voice.sid.op3.delta *= -1;
        }
    }
    voice.sid.op2.offset += voice.sid.op2.delta;
    if (--voice.sid.op2.count == 0) {
        if ((voice.sid.op2.count = voice.sid.op2.speed) != 0) {
            voice.sid.op2.delta *= -1;
        }
    }
}

// ----------------------------------------------------------------------
// Master volume with target modes "set, up, down".

void TFMXDecoder::fadeInit(ubyte target, ubyte speed) {
    if (fade.active) {
        return;
    }
    fade.active = true;
    fade.target = target;
    fade.count = fade.speed = speed;
    // With speed=0 target volume can be set directly. 
    if ( (fade.speed==0) || (fade.volume==fade.target) ) {
        fade.volume = fade.target;
        fade.delta = 0;
        fade.active = false;
        return;
    }
    if (fade.volume < fade.target) {
        fade.delta = 1;
    }
    else {
        fade.delta = -1;
    }
}

void TFMXDecoder::fadeApply(VoiceVars& voice) {
    if ((fade.delta != 0) && (--fade.count == 0)) {
        fade.count = fade.speed;
        fade.volume += fade.delta;
        if (fade.volume == fade.target) {
            fade.delta = 0;
            fade.active = false;
        }
    }
    sbyte vol = voice.volume;
    if (fade.volume < 64) {
        vol = (4*voice.volume*fade.volume)/(4*0x40);
    }
    voice.ch->paula.volume = vol;
}

// ----------------------------------------------------------------------
// Runtime effect support for two very rarely used macros commands.
// $1b RandomPlay + $1e RandomMask resp. RandomLimit.
//
// There are multiple implementations, which are incompatible with
// eachother. They are not needed, since no module uses them.

void TFMXDecoder::randomPlay(VoiceVars& voice) {
    if (voice.rnd.flag == 0) {
        return;
    }
    else if (voice.rnd.flag > 0) {
        // The specified macro is abused as arpeggiator input array.
        voice.rnd.arp.offset = getMacroOffset(voice.rnd.macro);
        voice.rnd.arp.pos = 0;
        voice.rnd.flag = -1;

        if ( (voice.rnd.mode&1) != 0 ) {
            // mode bit 0 set
            randomPlayMask(voice);
        }
    }

    if ( --voice.rnd.count == 0 ) {
        voice.rnd.count = voice.rnd.speed;
        do {
            ubyte aa = pBuf[voice.rnd.arp.offset + voice.rnd.arp.pos];
            if ((cmd.aa = aa) != 0) {
                break;
            }
            if (voice.rnd.arp.pos == 0) {
                return;
            }
            voice.rnd.arp.pos = 0;
        }
        while (true);
    }
    else {
        randomPlayReverb(voice);
        return;
    }

    uword n = ((sbyte)cmd.aa + voice.note) & 0x3f;
    if (n == 0) {
        randomPlayMask(voice);
        return;
    }
    uword p = noteToPeriod(n);
    sword finetune = voice.detune;
    if (finetune != 0) {
        p = ((finetune+0x100)*p)>>8;
    }
    if ( (voice.rnd.mode&1) == 0 ) {  // mode bit 0
        voice.period = p;
        if (voice.portamento.speed != 0) {
            return;
        }
        voice.outputPeriod = p;
        randomPlayCheckWait(voice);
        voice.rnd.arp.pos++;
        return;
    }

    // mode bit 0 set
    randomize();
    if ( ((voice.rnd.mode&4) != 0) ||
         ((voice.rnd.arp.pos&3) != 0) ||
         ((admin.randomWord&0xff)>16) ) {
        // mode bit 2 set
        randomPlayCheckWait(voice);
        voice.period = p;
        if (voice.portamento.speed == 0) {
            voice.outputPeriod = p;
        }
    }
    voice.rnd.arp.pos++;
                
    if ( (cmd.aa&0x40) == 0) {
        return;
    }
    randomize();
    if ( (admin.randomWord>>8) > 6) {
        randomPlayMask(voice);
        return;
    }
}

void TFMXDecoder::randomPlayReverb(VoiceVars& voice) {
    if ( ((voice.rnd.mode&2) == 0) ||
         (((voice.rnd.speed*3)/8) != voice.rnd.count) ) {
        return;
    }
    // Next voice, and loop from 3 to 0.
    VoiceVars& voice2 = voiceVars[(voice.voiceNum+1)&3];
    voice2.volume = (voice.volume*5)/8;
    if (voice.period != voice2.period) {
        voice2.period = voice.period;
        voice2.outputPeriod = voice.period;
        randomPlayCheckWait(voice);
    }
}

void TFMXDecoder::randomPlayMask(VoiceVars& voice) {
    randomize();
    voice.rnd.arp.pos = (admin.randomWord&0xff)&voice.rnd.mask;
}

void TFMXDecoder::randomPlayCheckWait(VoiceVars& voice) {
    if ((cmd.aa&0x80) != 0) {
        voice.rnd.blockWait = false;
    }
}

void TFMXDecoder::randomize() {
    // The RNG implementation varies in publicly released TFMX player
    // object code such as TFMX Professional. Usually it retrieves
    // the vertical raster position from DFF006/VHPOSR and modifies
    // it in varying ways.
    admin.randomWord = (admin.randomWord ^ rand()) + 0x57294335;
}

}  // namespace
