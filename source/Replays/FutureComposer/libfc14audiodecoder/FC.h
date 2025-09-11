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
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef FC_H
#define FC_H

#include <string>
#include <vector>

#include "fc14audiodecoder.h"
#include "MyTypes.h"
#include "MyEndian.h"
#include "SmartPtr.h"
#include "Paula.h"

class Analyze;

class FC : public PaulaPlayer {
 public:
    FC();
    ~FC();

    void setMixer(PaulaMixer*);
    bool init(void*,udword,int);
    bool init(int);
    
    bool isOurData(void*,udword);  // optional
    bool restart();
    int run();
    bool clearEnd(bool isCleared) { bool ret = songEnd; if (isCleared) songEnd = false; return ret; } // rePlayer
    void off();
    
    udword getDuration();  // [ms] duration of initialized song
    bool getSongEndFlag(); // whether song end has been reached
    int getSongs();  // return number of (sub-)songs found
    void endShorts(int,int);
    void setLoopMode(int);
    void setTrackRange(int=-1,int=-1);

    const char* getFormatID();
    const char* getFormatName();

    bool copyStats(struct fc14dec_mod_stats* targetStats);
    
    uword getSampleLength(uword num);
    uword getSampleRepOffset(uword num);
    uword getSampleRepLength(uword num);

    void dumpModule();
    
 //private: // rePlayer
    friend class Analyze;
    
    std::string formatID;
    std::string formatName;
    
    static const std::string SMOD_FORMAT_NAME;
    static const std::string FC14_FORMAT_NAME;
    static const std::string TFMX_FORMAT_NAME;
    static const std::string TFMX_7V_FORMAT_NAME;
    static const std::string COSO_FORMAT_NAME;
    static const std::string MCMD_FORMAT_NAME;
    static const std::string UNKNOWN_FORMAT_ID;

    static const std::string SMOD_TAG;
    static const uword SMOD_SMPHEADERS_OFFSET = 0x0028;   // 40
    static const uword SMOD_TRACKTAB_OFFSET = 0x0064;     // 100
    static const uword SMOD_PERIOD_MIN = 0x0071;
    static const uword SMOD_PERIOD_MAX = 0x06b0;

    static const std::string FC14_TAG;
    static const uword FC14_WAVEHEADERS_OFFSET = 0x0064;  // 100
    static const uword FC14_TRACKTAB_OFFSET = 0x00b4;     // 180
    static const uword FC14_PERIOD_MIN = SMOD_PERIOD_MIN;
    // (NOTE) This should be 0x1ac0 since FC 1.3, which introduced
    // the extra low octave as a non-working hack due to this
    // range-check. Some player sources fixed it temporarily,
    // but FC 1.4 was released with this value again.
    static const uword FC14_PERIOD_MAX = 0x0d60;

    static const udword SSMP_HEX = 0x53534d50;  // "SSMP"

    // don't need these, can be derived from TFMX constants
    //static const ubyte FC_TRACKTAB_STEP_SIZE = 0x0d;   // 3*4+1
    //static const ubyte FC_TRACKTAB_COLUMN_SIZE = 0x03;
    // don't need this since code does <<6 in most places
    //static const ubyte FC_SEQ_SIZE = 64;
    static const ubyte FC_PATTERN_BREAK = 0x49;

    // TFMX defines the pattern size within a field in its header structure.
    // The released machine code players evaluate that actually.
    // Yet all published modules use either pattern size 64 or the
    // basic compression introduced with the TFMX COSO format.
    // Future Composer has inherited size 64 as an implicit constant
    // and has hardcoded it further with a left-shift by 6.
    // This library has copied the <<6 style and for TFMX checks that
    // the defined pattern size is 64.
    static const uword PATTERN_LENGTH = 0x0040;  // 32*2

    static const uword TFMX_PROBE_SIZE = 0xb80;
    static const std::string TFMX_TAG;
    static const udword TFMX_HEX = 0x54464d58;  // "TFMX"
    static const ubyte TFMX_TRACKTAB_COLUMN_SIZE = 0x03;
    static const ubyte TFMX_TRACKTAB_STEP_SIZE = 0x0c;   // 3*4
    static const ubyte TFMX_SONGTAB_ENTRY_SIZE = 6;
    static const ubyte TFMX_SAMPLE_STRUCT_SIZE = 0x12+4+2+4+2;

    static const std::string COSO_TAG;
    static const udword COSO_HEX = 0x434f534f;  // "COSO"
    static const ubyte COSO_SAMPLE_STRUCT_SIZE = 4+2+2+2;

    static const std::string TFMX_7V_ID;
    static const ubyte TFMX_7V_TRACKTAB_STEP_SIZE = 0x1c;   // 4*7
    static const ubyte TFMX_7V_TRACKTAB_COLUMN_SIZE = 0x04;
    static const ubyte TFMX_7V_SONGTAB_ENTRY_SIZE = 8;

    static const std::string MCMD_TAG;
    static const uword MCMD_PROBE_SIZE = 0x8d2;
    static const ubyte MCMD_SAMPLE_STRUCT_SIZE = 0x12+4+2+2+2;

    // With this size we can skip all prepended machine code players.
    static const uword PROBE_SIZE = TFMX_PROBE_SIZE;

    static const int VOICES_MAX = 7;
    
    static const int RECURSE_LIMIT = 16;  // way more than needed

    PaulaVoice dummyVoices[VOICES_MAX];
    PaulaMixer *pMixer;

    ubyte *input;        // the allocated buffer
    udword inputBufLen;  // the allocated amount
    udword inputLen;     // length of the data we copy

    smartPtr<ubyte> fcBuf;   // for safe unsigned access
    smartPtr<sbyte> fcBufS;  // for safe signed access

    // This array will be moved behind the input file.
    // Therefore we allocate additional sizeof(..) bytes during init.
    static const ubyte silenceData[8];

    // Index is AND 0x7f.
    static const uword periods[0x80];

    static const uword SMOD_waveInfo[47*4];
    static const ubyte SMOD_waveforms[];

    struct ModuleOffsets {
        udword header;
        udword trackTable;
        udword patterns;
        udword sndModSeqs;
        udword volModSeqs;
        udword subSongTab;
        udword sampleHeaders;
        udword sampleData;
        udword silence;
    } offsets;

    fc14dec_mod_stats stats;

    struct PlayerTraits {
        bool compressed;
        bool vibScaling;     // FC does it, but not all TFMX versions
        bool vibSpeedFlag;
        bool sndSeqGoto;     // whether cmds branch back to SndSeq eval
        bool sndSeqToTrans;  // from end of SndSeq commands
        bool skipToWaveMod;  // during SndSeq sustain
        bool checkSSMPtag;   // some players expect sample packs to start with "SSMP"
        bool foundSSMP;
        bool volSeqSnd80;    // handle SndSeq $80 in VolSeq
        bool volSeqEA;       // FC volume slide command $EA
        bool randomVolume;
        bool voiceVolume;    // for the extra volume level 0-100 for each voice
        bool portaIsBit6;
        bool porta40SetSnd;  // handle portamento flags bit 6 can set SndSeq
        bool porta80SetSnd;  // handle portamento flags bit 7 can set SndSeq
        
        bool isSMOD;         // Future Composer 1.0 to 1.3, NOT 1.4
        int patternSize;  /* 0, if it's a compressed module */
        int sampleStructSize;
        uword periodMin, periodMax;
    } traits;

    struct Admin {
        ubyte speed, count;  // speed and speed count
        int startSpeed, startSong;
        
        bool initialized;  // true => restartable
        bool isEnabled;    // player on => true, else false
        int readModRecurse;
    } admin;

    ubyte trackStepLen, trackColumnSize;
    int trackTabLen;
    int firstStep, lastStep;
    
    udword tickFP, tickFPadd;

    udword duration, songPosCurrent;  // [ms]
    udword endShortsDuration;
    bool endShortsMode;
    bool songEnd;
    bool loopMode;
    
    uword randomWord;
    
    class Sample {
    public:
        udword startOffs, repOffs;  // repeat offset is added to start offset
        uword len, repLen;
        const ubyte* pStart;

        // this is not automatic/implicit because of SMOD's internal waveforms
        void assertBoundaries(smartPtr<ubyte>& fcBuf);
    };
    Sample silentSample;
    // SMOD: 10 samples, 47 waveforms
    // FC14: 10 samples/sample-packs, 80 waveforms
    // TFMX: sample number stored as unsigned 8-bit
    static const uword SAMPLES_MAX = 256;
    Sample samples[SAMPLES_MAX];

    struct VoiceVars {
        PaulaVoice *ch;  // paula and mixer interface
    
        ubyte voiceNum;  // 0 = first
        bool trigger;
        
        udword trackStart;     // track/step pattern table
        udword trackEnd;
        uword trackPos;

        udword pattStart;
        uword pattPos;
    
        sbyte transpose;       // TR
        sbyte soundTranspose;  // ST
        sbyte seqTranspose;    // from sndModSeq
    
        ubyte pattVal1, pattVal2;
    
        sbyte pitchBendSpeed;
        ubyte pitchBendTime, pitchBendDelayFlag;
    
        sbyte portaSpeed, portaDelayFlag;
        sdword portaOffs;
    
        ubyte currentVolSeq;  // the number, not the offset
        udword volSeq;
        uword volSeqPos;
    
        ubyte volSlideSpeed, volSlideTime, volSustainTime,
            volSlideDelayFlag;
    
        ubyte envelopeSpeed, envelopeCount;
    
        ubyte currentSndSeq;  // the number, not the offset
        udword sndSeq;
        uword sndSeqPos;
    
        ubyte sndModSustainTime;
    
        ubyte vibFlag, vibDelay, vibSpeed,
            vibAmpl, vibCurOffs;
    
        sbyte volume;
        ubyte volRandomThreshold, volRandomCurrent;

        uword outputPeriod, outputVolume;
    
        const ubyte* pSampleStart;
        uword repeatOffset;
        uword repeatLength;
        uword repeatDelay;

        ubyte currentWave;
        // switch1 init to 0 = main switch ON
        ubyte waveModSwitch1;  // var $32
        ubyte waveModSwitch2;  // var $33
        sbyte waveModSpeed, waveModSpeedCount;
        bool waveModInit;  // var $2e
        udword waveModStartOffs, waveModEndOffs;  // var $20, $24
        uword waveModLen;
        sword waveModOffs;
        uword waveModCurrentOffs;  // in words

        sbyte pattCompress, pattCompressCount;

        ubyte voiceVol;  // 0-100

        uword lastPeriod;
        sdword portaDiffOld;
    };
    struct VoiceVars voiceVars[VOICES_MAX];

    void clearFormat();
    void mixerInit();
    void setRate(ubyte f);
    void killChannel(VoiceVars&);
    void prepareChannelUpdate(VoiceVars&);
    void setInstrument(VoiceVars&, ubyte);
    void setWave(VoiceVars&, ubyte num);
    
    udword getPattOffs(VoiceVars&);
    udword getVolSeq(ubyte seq);  // offset to volume modulation sequence
    udword getSndSeq(ubyte seq);  // offset to sound modulation sequence
    udword getVolSeqEnd(ubyte seq);
    udword getSndSeqEnd(ubyte seq);

    bool havePattern(int n, const ubyte (&pattWanted)[PATTERN_LENGTH]);
    void replacePattern(int n, const ubyte (&pattNew)[PATTERN_LENGTH]);


    bool findTag(const ubyte*, udword, const std::string, udword&);  // generic
    void TraitsByChecksum();

    bool FC_detect(ubyte*,udword);
    bool SMOD_init(int songNumber);
    bool FC_init(int songNumber);  // SMOD and FC14 formats
    void FC_chooseSong(ubyte);
    void FC_nextNote(VoiceVars&);
    uword FC_portamento_pitchbend(VoiceVars&, uword period);
    void FC_volSlide(VoiceVars&);
    void FC_sndSeq_E9(VoiceVars&);
    void FC_sndSeq_EA_pitchbend(VoiceVars&);

    bool TFMX_detect(ubyte*,udword);
    bool TFMX_findTag(const ubyte*, udword);
    bool TFMX_4V_maybe();
    bool TFMX_findPortamentoCode();
    bool TFMX_init(int songNumber);
    void TFMX_chooseSong(ubyte);
    void TFMX_nextNote(VoiceVars&);
    void TFMX_processPattern(VoiceVars&);
    void TFMX_soundModulation(VoiceVars&);
    void TFMX_processSndModSustain(VoiceVars&);
    void TFMX_processSndModSeq(VoiceVars&);
    void TFMX_sndSeqTrans(VoiceVars&);
    void TFMX_waveModulation(VoiceVars&);
    void TFMX_processPerVol(VoiceVars&);
    uword TFMX_envelope(VoiceVars&);
    uword TFMX_vibrato(VoiceVars&, uword period, ubyte note);
    uword TFMX_portamento(VoiceVars&, uword period);

    void TFMX_sndSeq_UNDEF(VoiceVars&);
    void TFMX_sndSeq_E0(VoiceVars&);
    void TFMX_sndSeq_E1(VoiceVars&);
    void TFMX_sndSeq_E1_waveMod(VoiceVars&);
    void TFMX_sndSeq_E2(VoiceVars&);
    void TFMX_sndSeq_E3(VoiceVars&);
    void TFMX_sndSeq_E4(VoiceVars&);
    void TFMX_sndSeq_E5(VoiceVars&);
    void TFMX_sndSeq_E5_repOffs(VoiceVars&);
    void TFMX_sndSeq_E6(VoiceVars&);
    void TFMX_sndSeq_E7(VoiceVars&);
    void TFMX_sndSeq_E7_setDiffWave(VoiceVars&);
    void TFMX_sndSeq_E8(VoiceVars&);
    void TFMX_sndSeq_E9(VoiceVars&);
    void TFMX_sndSeq_EA(VoiceVars&);
    void TFMX_sndSeq_EA_skip(VoiceVars&);

    typedef void (FC::*ModFuncPtr)(VoiceVars&);
    ModFuncPtr TFMX_sndModFuncs[(0xea-0xe0)+1] = { };
    std::vector<ModFuncPtr> vModFunc;
    
    bool TFMX_COSO_findTags(const ubyte*, udword);
    bool COSO_detect(ubyte*,udword);
    bool COSO_findPlayer(const ubyte*, udword);
    bool COSO_4V_maybe();
    bool COSO_isModPack(std::vector<udword>&, bool&);
    bool COSO_init(int songNumber);
    void COSO_nextNote(VoiceVars&);
    void COSO_processPattern(VoiceVars&);
    uword COSO_vibrato(VoiceVars&, uword period, ubyte note);
    uword COSO_portamento(VoiceVars&, uword period);

    bool TFMX_7V_findPlayer(const ubyte*, udword);
    bool TFMX_7V_maybe();
    void TFMX_7V_subInit();
    void TFMX_7V_chooseSong(ubyte);
    void TFMX_7V_trackTabCmd(VoiceVars&, udword trackOffs);
    void TFMX_7V_setRate(sbyte);

    bool COSO_7V_maybe();

    bool MCMD_detect(ubyte*,udword);
    bool MCMD_init(int songNumber);
    void MCMD_chooseSong(ubyte);


    bool (FC::*pInitFunc)(int);
    void (FC::*pChooseSongFunc)(ubyte);
    void (FC::*pNextNoteFunc)(VoiceVars&);
    void (FC::*pSoundFunc)(VoiceVars&);
    uword (FC::*pVibratoFunc)(VoiceVars&, uword, ubyte);
    uword (FC::*pPortamentoFunc)(VoiceVars&, uword);

    Analyze* analyze;
};

#endif  // FC_H
