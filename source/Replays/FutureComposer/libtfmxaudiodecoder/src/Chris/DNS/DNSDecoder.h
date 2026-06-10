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

#ifndef DNSDECODER_H
#define DNSDECODER_H

#include "Decoder.h"
#include "SmartPtr.h"
#include "PaulaVoice.h"
#include "MyTypes.h"
#include "MyEndian.h"
#include "Dump.h"

#include <string>
#include <vector>

namespace tfmxaudiodecoder {

class DNSDecoder : public Decoder {
 public:
    DNSDecoder();
    ~DNSDecoder() override;

    void setPath(std::string, LoaderCallback loaderArg, void* loaderDataArg) override;// rePlayer

    bool init(void*, udword, int) override;
    bool detect(void*,udword) override;
    void restart() override;
    int run() override;

    int getSongs() override;
    ubyte getVoices() override;
    
    void setPaulaVoice(ubyte,PaulaVoice*) override;

 public:// rePlayer
    static const std::string FORMAT_NAME, TAG;
    static const int TRACKS_MAX = 6;
    static const int VOICES_MAX = 6;  // really 4, but can run with 6 voices too
    static const uword periods[0x30];

    int voices;
    udword songPosCurrent;
    smartPtr<ubyte> pBuf;   // for safe unsigned access

    struct ModuleOffsets {
        udword header;
        udword trackTable;
        udword songDefs;
        udword patterns;
        udword sampleHeaders;
        udword sampleData;
        udword silence;

        // The absolute load address to be subtracted from offsets read at runtime.
        udword base;
    } offsets;

    struct {
        udword checksum;  // the main one we use to identify the module
        
        sword speed, count;  // speed and speed count
        int startSong, songs;
        
        bool initialized;  // true => restartable
        bool looped;       // whether sequencer has processed last track

        ubyte nextVoiceNum;
    } admin;
    
    struct {  // flags to handle differences between file formats and players
        bool hppingame;
        bool starball, starballingame;
        bool ptc;
    } variant;
    
    struct {
        ubyte tracks;
        ubyte tables, tableSize;

        // Track progression. Same for all tracks.
        struct {
            uword first, last, current, loop;
            udword currentOffset;
            ubyte size;
            bool next;   // whether a pattern triggered next track step
        } step;

        // Pattern progression. Same for all tracks.
        struct {
            uword pos, length;
        } pattern;
    } sequencer;

    struct Track {
        ubyte num;
        bool off;
        bool keyDown;
        ubyte assignedVoiceNum;
        
        ubyte PT;  // pattern number
        sbyte TR;  // transpose is signed
        ubyte ST;  // sound transpose
        
        struct {
            udword offset;
            ubyte note;
            ubyte sample;
            ubyte addVolume;
        } pattern;
    };
    Track track[TRACKS_MAX];

    enum EnvPhase {
        ATTACK = 3,
        DECAY = 2,
        RELEASE = 1,
        END = 0
    };

    struct VoiceVars {
        PaulaVoice *ch;  // paula and mixer interface

        udword sampleHeader;
        uword period;
        ubyte pipelineState;

        struct {
            uword count;
            uword speed, decaySpeed, releaseSpeed;
            sword volume;
            uword targetVolume, sustainVolume;
            sword strength, decayStrength, releaseStrength;
            EnvPhase phase;
            bool keyUp;
            bool setSustain;
            uword duration;  // unsigned comparison
        } envelope;
        
        struct {
            udword offset;
            uword length;
        } paulaOrig;
    };

    struct VoiceVars voiceVars[VOICES_MAX];
    PaulaVoice dummyVoices[VOICES_MAX];

    bool loadSamplesFile();
    void softRestart();
    void processSequencer();
    void processPattern(Track&);
    udword getPattOffset(ubyte);

    ubyte chooseVoice();
    void updateVoices();
    void processEnvelope(VoiceVars&);
    void nextEnvelopePhase(VoiceVars&);
    void processPaulaPipeline(VoiceVars&);
    void triggerVoiceKeyUp(Track&);
    void triggerNote(Track&);
    uword capVolume(uword);

    ubyte* makeSamplePtr(udword offset);
    void toPaulaStart(VoiceVars&,udword);
    void toPaulaLength(VoiceVars&,uword);
    void takeNextBufChecked(VoiceVars&);
    
    struct {
        ubyte *buf;     // the allocated buffer
        udword bufLen;  // the allocated amount
        udword len;     // length of the data we copy

        std::string path, ext;
        LoaderCallback loader = nullptr; // rePlayer
        void* loaderData = nullptr; // rePlayer

        unsigned int mdatSize, smplSize, mdatSizeCurrent, smplSizeCurrent;
        bool smplLoaded;
    } input;
};

}  // namespace

#endif  // DNSDECODER_H
