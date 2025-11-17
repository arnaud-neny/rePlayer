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

#ifndef TFMXDECODER_H
#define TFMXDECODER_H

#include "Decoder.h"
#include "SmartPtr.h"
#include "PaulaVoice.h"
#include "MyTypes.h"
#include "MyEndian.h"
#include "Dump.h"

#include <string>
#include <vector>

namespace tfmxaudiodecoder {

class TFMXDecoder : public Decoder {
 public:
    TFMXDecoder();
    ~TFMXDecoder() override;

    void setPath(std::string, LoaderCallback loaderArg, void* loaderDataArg) override;// rePlayer

    bool init(void*, udword, int) override;
    bool detect(void*,udword) override;
    void restart() override;
    int run() override;

    int getSongs() override;
    ubyte getVoices() override;
    
    void setPaulaVoice(ubyte,PaulaVoice*) override;

 public:// rePlayer
    void reset();
    void adjustTraitsPost();
    void dumpMacros();
    void dumpModule();
    
    static const std::string FORMAT_NAME;
    static const std::string FORMAT_NAME_PRO;
    static const std::string FORMAT_NAME_7V;
    static const std::string TAG, TAG_TFMXSONG, TAG_TFMXSONG_LC, TAG_TFMXPAK, TAG_TFHD, TAG_TFMXMOD;
    static const udword TFMX_HEX = 0x54464d58;

    static const int TRACKS_MAX = 8;
    static const int TRACK_STEPS_MAX = 512;
    
    static const uword periods[0x40+0xd];

    static const int VOICES_MAX = 8;
    PaulaVoice dummyVoices[VOICES_MAX];
    
    static const int RECURSE_LIMIT = 16;  // way more than needed

    smartPtr<ubyte> pBuf;   // for safe unsigned access

    std::vector<ubyte> vSongs;
    udword songPosCurrent;
    int voices;
    bool /*loopMode, */triggerRestart; // rePlayer

    struct ModuleOffsets {
        udword header;
        udword trackTable;
        udword patterns;
        udword macros;
        udword sampleData;
        udword silence;
    } offsets;

    struct Admin {
        sword speed, count;  // speed and speed count
        int startSpeed, startSong;
        
        bool initialized;  // true => restartable
        uword randomWord;
    } admin;

    struct {
        ubyte tracks;
        bool evalNext;   // whether to evaluate next PT.TR pair
        sword loops;
        struct {
            int first, last, current;
            ubyte size;
            bool next;   // whether a pattern triggered next track step
        } step;
        bool stepSeenBefore[TRACK_STEPS_MAX];
    } sequencer;

    struct Track {
        ubyte PT;
        sbyte TR;
        bool on;  // = mute flag

        struct Pattern {
            udword offset, offsetSaved;
            uword step, stepSaved;
            ubyte wait;
            sbyte loops;
            bool evalNext;
            bool infiniteLoop;
        } pattern;
    };
    Track track[TRACKS_MAX];

    struct Cmd {
        ubyte aa, bb, cd, ee;
    } cmd;

    struct {
        ubyte volume, target;
        ubyte speed, count;
        sbyte delta;
        bool active;
    } fade;

    ubyte channelToVoiceMap[VOICES_MAX];

    struct VoiceVars {
        PaulaVoice *ch;  // paula and mixer interface
        uword outputPeriod, outputVolume;
    
        ubyte voiceNum;  // 0 = first
        sbyte effectsMode;
        
        sbyte volume;

        ubyte note, notePrevious;
        ubyte noteVolume;
        uword period;
        sword detune;
        bool keyUp;
        
        struct {
            udword offset, offsetSaved;
            udword step, stepSaved;
            sword wait;
            ubyte loop;
            bool skip, extraWait;
        } macro;
        
        sword waitOnDMACount;
        uword waitOnDMAPrevLoops;
        
        ubyte addBeginCount, addBeginArg;
        sdword addBeginOffset;
        
        struct {
            ubyte time, count;
            sbyte intensity;
            sword delta;
        } vibrato;

        struct {
            ubyte flag;
            ubyte count, speed;
            ubyte target;
        } envelope;

        struct {
            ubyte count, wait;
            uword speed;
            uword period;
        } portamento;

        struct {
            udword start;
            uword length;
        } sample;

        struct {
            udword offset;
            uword length;
        } paulaOrig;

        struct {
            udword sourceOffset;
            uword sourceLength;
            udword targetOffset;
            uword targetLength;
            sbyte lastSample;
            struct {
                uword speed, count;
                uword interDelta;
                sword interMod;
            } op1;
            struct {
                uword speed, count;
                udword offset;
                sword delta;
            } op2;
            struct {
                uword speed, count;
                udword offset;
                sword delta;
            } op3;
        } sid;

        struct {
            ubyte macro;
            sbyte count, speed;
            sbyte flag;
            ubyte mode;
            bool blockWait;
            ubyte mask;
            struct {
                udword offset;
                uword pos;
            } arp;
        } rnd;
    };
    struct VoiceVars voiceVars[VOICES_MAX];

    bool isMerged();
    bool loadSamplesFile();
    void setTFMXv1();
    void traitsByChecksum();  // final step after successful initialization
    void findSongs();

    ubyte* makeSamplePtr(udword offset);
    void toPaulaStart(VoiceVars&,udword);
    void toPaulaLength(VoiceVars&,uword);
    void takeNextBufChecked(VoiceVars&);
    void noteCmd();
    uword noteToPeriod(int);
    void processPTTR(Track&);

    void processModulation(VoiceVars&);
    void addBegin(VoiceVars&);
    void envelope(VoiceVars&);
    void vibrato(VoiceVars&);
    void portamento(VoiceVars&);
    void sid(VoiceVars&);
    void randomPlay(VoiceVars&);
    void randomPlayReverb(VoiceVars&);
    void randomPlayMask(VoiceVars&);
    void randomPlayCheckWait(VoiceVars&);
    void randomize();
    void fadeInit(ubyte, ubyte);
    void fadeApply(VoiceVars&);

    udword getPattOffset(ubyte);
    void processPattern(Track&);
    void pattCmd_NOP(Track&);
    void pattCmd_End(Track&);
    void pattCmd_Loop(Track&);
    void pattCmd_Goto(Track&);
    void pattCmd_Wait(Track&);
    void pattCmd_Stop(Track&);
    void pattCmd_Note(Track&);
    void pattCmd_SaveAndGoto(Track&);
    void pattCmd_ReturnFromGoto(Track&);
    void pattCmd_Fade(Track&);

    typedef void (TFMXDecoder::*PattCmdFuncPtr)(Track&);
    PattCmdFuncPtr PattCmdFuncs[(0xff-0xf0)+1] = { };
    
    bool getTrackMute(ubyte);
    void processTrackStep();
    void trackCmd_Stop(udword);
    void trackCmd_Loop(udword);
    void trackCmd_Speed(udword);
    void trackCmd_7V(udword);
    void trackCmd_Fade(udword);

    static const int TRACK_CMD_MAX = 4;
    typedef void (TFMXDecoder::*TrackCmdFuncPtr)(udword);
    TrackCmdFuncPtr TrackCmdFuncs[TRACK_CMD_MAX+1] = { };
    static const std::string TrackCmdInfo[TRACK_CMD_MAX+1];
    
    udword getMacroOffset(ubyte);
    void processMacroMain(VoiceVars&);

    typedef void (TFMXDecoder::*MacroFuncPtr)(VoiceVars&);
    struct MacroDef {
        const std::string text;
        MacroFuncPtr ptr;
    };
    MacroDef *MacroDefs[0x40] = {};
    bool macroEvalAgain;

    void macroFunc_ExtraWait(VoiceVars&);

    void macroFunc_NOP(VoiceVars&);
    MacroDef macroDef_NOP = { "No Entry - - - - - - - - - - - - - - -",
                              &TFMXDecoder::macroFunc_NOP
    };

    // Only a stub for some macro text in debug output.
    MacroDef macroDef_UNDEF = { "Undefined - - - - - - - - - - - - -",
                                &TFMXDecoder::macroFunc_NOP
    };
    
    void macroFunc_StopSound(VoiceVars&);
    MacroDef macroDef_StopSound = { "DMAoff+Reset (stop sample & reset all)",
                                    &TFMXDecoder::macroFunc_StopSound
    };
    
    void macroFunc_StartSample(VoiceVars&);
    MacroDef macroDef_StartSample = { "DMAon (start sample at selected begin)",
                                      &TFMXDecoder::macroFunc_StartSample
    };
    
    void macroFunc_SetBegin(VoiceVars&);
    void macroFunc_SetBegin_sub(VoiceVars&, udword);
    MacroDef macroDef_SetBegin = { "SetBegin    xxxxxx   sample-startadress",
                                   &TFMXDecoder::macroFunc_SetBegin
    };

    void macroFunc_SetLen(VoiceVars&);
    MacroDef macroDef_SetLen = { "SetLen      ..xxxx   sample-length",
                                 &TFMXDecoder::macroFunc_SetLen
    };
    
    void macroFunc_Wait(VoiceVars&);
    MacroDef macroDef_Wait = { "Wait        ..xxxx   count (VBI's)",
                               &TFMXDecoder::macroFunc_Wait
    };

    void macroFunc_Loop(VoiceVars&);
    MacroDef macroDef_Loop = { "Loop        xx/xxxx  count/step",
                               &TFMXDecoder::macroFunc_Loop
    };

    void macroFunc_Cont(VoiceVars&);
    MacroDef macroDef_Cont = { "Cont        xx/xxxx  macro-number/step",
                               &TFMXDecoder::macroFunc_Cont
    };
    
    void macroFunc_Stop(VoiceVars&);
    MacroDef macroDef_Stop = { "-------------STOP----------------------",
                               &TFMXDecoder::macroFunc_Stop
    };
    
    void macroFunc_AddNote(VoiceVars&);
    void macroFunc_AddNote_sub(VoiceVars&,ubyte);
    MacroDef macroDef_AddNote = { "AddNote     xx/xxxx  note/detune",
                                  &TFMXDecoder::macroFunc_AddNote
    };

    void macroFunc_SetNote(VoiceVars&);
    MacroDef macroDef_SetNote = { "SetNote     xx/xxxx  note/detune",
                                  &TFMXDecoder::macroFunc_SetNote
    };

    void macroFunc_Reset(VoiceVars&);
    MacroDef macroDef_Reset = { "Reset   Vibrato-Portamento-Envelope",
                                &TFMXDecoder::macroFunc_Reset
    };

    void macroFunc_Portamento(VoiceVars&);
    MacroDef macroDef_Portamento = { "Portamento  xx/../xx count/speed",
                                     &TFMXDecoder::macroFunc_Portamento
    };

    void macroFunc_Vibrato(VoiceVars&);
    MacroDef macroDef_Vibrato = { "Vibrato     xx/../xx speed/intensity",
                                  &TFMXDecoder::macroFunc_Vibrato
    };

    void macroFunc_AddVolume(VoiceVars&);  // actually $00-$40
    MacroDef macroDef_AddVolume = { "AddVolume   ....xx   volume 00-3F",
                                    &TFMXDecoder::macroFunc_AddVolume
    };

    void macroFunc_AddVolNote(VoiceVars&);  // actually $00-$40
    // +AddNote variant
    // Replaces AddVolume by default. Potentially harmless,
    // since 'bb' arg must be set to 0xfe in order to activate the
    // extra behaviour, and AddVolume doesn't use 'bb' arg.
    MacroDef macroDef_AddVolNote = { "Addvol+note xx/fe/xx note/CONST./volume",
                                    &TFMXDecoder::macroFunc_AddVolNote
    };

    void macroFunc_SetVolume(VoiceVars&);  // actually $00-$40
    // +AddNote variant
    MacroDef macroDef_SetVolume = { "SetVolume   ....xx   volume 00-3F",
                                    &TFMXDecoder::macroFunc_SetVolume
    };

    void macroFunc_Envelope(VoiceVars&);
    MacroDef macroDef_Envelope = { "Envelope    xx/xx/xx speed/count/endvol",
                                   &TFMXDecoder::macroFunc_Envelope
    };

    void macroFunc_LoopKeyUp(VoiceVars&);
    MacroDef macroDef_LoopKeyUp = { "Loop key up xx/xxxx  count/step",
                                    &TFMXDecoder::macroFunc_LoopKeyUp
    };

    void macroFunc_AddBegin(VoiceVars&);
    // +count variant
    MacroDef macroDef_AddBegin = { "AddBegin    ..xxxx   add to startadress",
                                   &TFMXDecoder::macroFunc_AddBegin
    };

    void macroFunc_AddLen(VoiceVars&);
    MacroDef macroDef_AddLen = {"AddLen      ..xxxx   add to sample-len",
                                &TFMXDecoder::macroFunc_AddLen
    };

    void macroFunc_StopSample(VoiceVars&);
    void macroFunc_StopSample_sub(VoiceVars&);
    MacroDef macroDef_StopSample = { "DMAoff stop sample but no clear",
                                     &TFMXDecoder::macroFunc_StopSample
    };

    void macroFunc_WaitKeyUp(VoiceVars&);
    MacroDef macroDef_WaitKeyUp = { "Wait key up ....xx   count (VBI's)",
                                    &TFMXDecoder::macroFunc_WaitKeyUp
    };

    void macroFunc_Goto(VoiceVars&);
    MacroDef macroDef_Goto = { "Go submacro xx/xxxx  macro-number/step",
                               &TFMXDecoder::macroFunc_Goto
    };

    void macroFunc_Return(VoiceVars&);
    MacroDef macroDef_Return = { "--------Return to old macro------------",
                                 &TFMXDecoder::macroFunc_Return
    };

    void macroFunc_SetPeriod(VoiceVars&);
    MacroDef macroDef_SetPeriod = { "Setperiod   ..xxxx   DMA period",
                                    &TFMXDecoder::macroFunc_SetPeriod
    };

    void macroFunc_SampleLoop(VoiceVars&);  // actually ..xxxxxx
    MacroDef macroDef_SampleLoop = { "Sampleloop  ..xxxx   relative adress",
                                     &TFMXDecoder::macroFunc_SampleLoop
    };

    void macroFunc_OneShot(VoiceVars&);
    MacroDef macroDef_OneShot = { "-------Set one shot sample-------------",
                                  &TFMXDecoder::macroFunc_OneShot
    };

    void macroFunc_WaitOnDMA(VoiceVars&);
    MacroDef macroDef_WaitOnDMA = { "Wait on DMA ..xxxx   count (Wavecycles)",
                                    &TFMXDecoder::macroFunc_WaitOnDMA
    };

    void macroFunc_RandomPlay(VoiceVars&);
    MacroDef macroDef_RandomPlay = { "Random play xx/xx/xx macro/speed/mode",
                                    &TFMXDecoder::macroFunc_RandomPlay
    };

    void macroFunc_SplitKey(VoiceVars&);
    MacroDef macroDef_SplitKey = { "Splitkey    xx/xxxx  key/macrostep",
                                   &TFMXDecoder::macroFunc_SplitKey
    };

    void macroFunc_SplitVolume(VoiceVars&);
    MacroDef macroDef_SplitVolume = { "Splitvolume xx/xxxx  volume/macrostep",
                                      &TFMXDecoder::macroFunc_SplitVolume
    };

    //MacroDef macroDef_Signal = { "Signal      xx/xxxx  signalnumber/value",
    //                             &TFMXDecoder::macroFunc_NOP };
    
    void macroFunc_RandomMask(VoiceVars&);
    MacroDef macroDef_RandomMask = { "Random mask xx       mask",
                                    &TFMXDecoder::macroFunc_RandomMask
    };
    void macroFunc_SetPrevNote(VoiceVars&);
    MacroDef macroDef_SetPrevNote = { "SetPrevNote xx/xxxx  note/detune",
                                      &TFMXDecoder::macroFunc_SetPrevNote
    };

    void macroFunc_PlayMacro(VoiceVars&);
    MacroDef macroDef_PlayMacro = { "Play macro  xx/.x/xx macro/chan/detune",
                                    &TFMXDecoder::macroFunc_PlayMacro
    };

    void macroFunc_22(VoiceVars&);
    MacroDef macroDef_22 = { "SID setbeg  xxxxxx   sample-startadress",
                             &TFMXDecoder::macroFunc_22
    };

    void macroFunc_23(VoiceVars&);
    MacroDef macroDef_23 = { "SID setlen  xx/xxxx  buflen/sourcelen",
                             &TFMXDecoder::macroFunc_23
    };

    void macroFunc_24(VoiceVars&);
    MacroDef macroDef_24 = { "SID op3 ofs xxxxxx   offset",
                             &TFMXDecoder::macroFunc_24
    };

    void macroFunc_25(VoiceVars&);
    MacroDef macroDef_25 = { "SID op3 frq xx/xxxx  speed/amplitude",
                             &TFMXDecoder::macroFunc_25
    };

    void macroFunc_26(VoiceVars&);
    MacroDef macroDef_26 = { "SID op2 ofs xxxxxx   offset",
                             &TFMXDecoder::macroFunc_26
    };

    void macroFunc_27(VoiceVars&);
    MacroDef macroDef_27 = { "SID op2 frq xx/xxxx  speed/amplitude",
                             &TFMXDecoder::macroFunc_27
    };

    void macroFunc_28(VoiceVars&);
    MacroDef macroDef_28 = { "SID op1     xx/xx/xx speed/amplitude/TC",
                             &TFMXDecoder::macroFunc_28
    };

    void macroFunc_29(VoiceVars&);
    MacroDef macroDef_29 = { "SID stop    xx....   flag (1=clear all)",
                             &TFMXDecoder::macroFunc_29
    };

    
    bool trackCmdUsed[TRACK_CMD_MAX+1];
    bool patternCmdUsed[16];
    bool macroCmdUsed[0x40];

    struct {
        // format
        bool compressed;
        // player variants
        bool finetuneUnscaled;
        bool vibratoUnscaled;
        bool portaUnscaled;
        bool portaOverride;
        bool noNoteDetune;
        bool bpmSpeed5;
    } variant;

    struct {
        ubyte *buf;     // the allocated buffer
        udword bufLen;  // the allocated amount
        udword len;     // length of the data we copy

        std::string path, ext;
        LoaderCallback loader = nullptr; // rePlayer
        void* loaderData = nullptr; // rePlayer

        unsigned int mdatSize, smplSize, mdatSizeCurrent, smplSizeCurrent;
        bool smplLoaded;

        int versionHint;  // from TFHD flag
        int startSongHint;  // from TFMX-MOD struct
    } input;
};

}  // namespace

#endif  // TFMXDECODER_H
