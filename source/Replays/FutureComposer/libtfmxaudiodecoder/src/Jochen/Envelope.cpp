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

#include "HippelDecoder.h"
#include "Analyze.h"

namespace tfmxaudiodecoder {

uword HippelDecoder::TFMX_envelope(VoiceVars& voiceX) {
    bool repeatVolSeq;  // JUMP/GOTO - WHILE conversion
    int jumpCount = 0;
    do {
        repeatVolSeq = false;
        
        // Sustain current volume level? NE => yes, EQ => no.
        if (voiceX.volSustainTime != 0) {
            --voiceX.volSustainTime;
        }
        // Slide volume? NE => yes, EQ => no.
        else if (traits.volSeqEA && voiceX.volSlideTime != 0) {
            FC_volSlide(voiceX);
        }
        // Time to set next volume level? NE => no, EQ => yes.
        // speed==0 is illegal and as a safety measure must not advance the
        // sequence position.
        else if ( (voiceX.envelopeSpeed == 0) || (--voiceX.envelopeCount == 0) ) {
            voiceX.envelopeCount = voiceX.envelopeSpeed;

            bool readNextVal;  // JUMP/GOTO - WHILE conversion
            do {
                readNextVal = false;
                if ( ++jumpCount > RECURSE_LIMIT ) {
                    break;
                }

                udword seqOffs = voiceX.volSeq+voiceX.volSeqPos;
                // Envelope position is permitted to advance beyond 64,
                // and some modules take advantage of that actually.
                // But potentially it can lead to unwanted side-effects,
                // if e.g. the envelope enters sample data.
                // Chambers_of_Shaolin_7.fc is setting a broken envelope
                // 0x16 missing end 0xe1 and with a missing sound
                // modulation sequence 0x16.
                ubyte cmd = fcBuf[seqOffs];
                if ((cmd&0xe0) == 0xe0) {
                    analyze->gatherVolSeqCmd(voiceX.currentVolSeq,cmd);
                }
                if (cmd == 0xE8) {  // SUSTAIN
                    voiceX.volSustainTime = fcBuf[seqOffs+1];
                    // Zero would be illegal.
                    if ( voiceX.volSustainTime == 0 ) {
                        voiceX.volSustainTime = 1;
                    }
                    voiceX.volSeqPos += 2;
                    // This shall loop to beginning of proc.
                    repeatVolSeq = true;
                    break;
                }
                else if (traits.volSeqEA && cmd == 0xEA) {  // VOLUME SLIDE
                    voiceX.volSlideSpeed = fcBuf[seqOffs+1];
                    voiceX.volSlideTime = fcBuf[seqOffs+2];
                    voiceX.volSeqPos += 3;
                    FC_volSlide(voiceX);
                    break;
                }
                else if (cmd == 0xE0) {  // LOOP
                    // Valid position would be 5 to 0x3f.
                    voiceX.volSeqPos = fcBuf[seqOffs+1]&0x3f;
                    // Skip the sequence header.
                    if ( voiceX.volSeqPos >= 5 ) {
                        voiceX.volSeqPos -= 5;
                    } else {  // Less than 5 would be an illegal loop pos.
                        voiceX.volSeqPos = 0;
                    }
                    // (FC14 BUG) Some FC players here do not read a
                    // parameter at the new sequence position. They
                    // leave the pos value in d0, which then passes
                    // as parameter through all the command comparisons.
                    readNextVal = true;
                }
                else if (cmd == 0xE1) {  // END
                    break;
                }
                else {  // Read volume value.
                    voiceX.volume = fcBuf[seqOffs];
                    if ( voiceX.envelopeSpeed != 0 ) {
                        ++voiceX.volSeqPos;
                    }
                    // Full range check for volume 0-64.
                    if (voiceX.volume > PaulaVoice::VOLUME_MAX) {
                        voiceX.volume = PaulaVoice::VOLUME_MAX;
                    }
                    else if (voiceX.volume < 0) {
                        voiceX.volume = 0;
                    }
                    break;
                }
            }
            while (readNextVal);
        }
    }
    while (repeatVolSeq);

    if (traits.randomVolume && voiceX.volRandomThreshold != 0) {
        if (voiceX.volRandomCurrent == 0) {
            ubyte t = voiceX.volRandomThreshold;
            voiceX.volRandomThreshold = 0;
            
            uword v = randomWord + 0x4793;
            // ROR #6 then EOR with self
            randomWord = v ^ ((v>>6)|(v<<(16-6)));
            voiceX.volRandomCurrent = ((randomWord&0xff)*t)/0xff;
        }
        voiceX.volume -= voiceX.volRandomCurrent;
        if (voiceX.volume < 0) {
            voiceX.volume = 0;
        }
    }

    if (traits.voiceVolume)
        return (voiceX.volume*voiceX.voiceVol)/100;
    else
        return voiceX.volume;
}

// ----------------------------------------------------------------------

void HippelDecoder::FC_volSlide(VoiceVars& voiceX) {
    // Following flag divides the volume sliding speed by two.
    voiceX.volSlideDelayFlag ^= 0xff;  // = NOT
    if (voiceX.volSlideDelayFlag != 0) {
        --voiceX.volSlideTime;
        voiceX.volume += voiceX.volSlideSpeed;
        if (voiceX.volume < 0) {
            voiceX.volume = voiceX.volSlideTime = 0;
        }
        // (NOTE) Most FC players do not check whether Paula's
        // maximum volume level is exceeded.
        if (voiceX.volume > PaulaVoice::VOLUME_MAX) {
            voiceX.volume = PaulaVoice::VOLUME_MAX;
            voiceX.volSlideTime = 0;
        }
    }
}

}  // namespace
