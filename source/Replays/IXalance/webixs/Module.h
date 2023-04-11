/*
* Handles the extended ImpulseTracker module format, explodes/synthesizes cached sample-recordings, etc.
*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

// todo: I've filled in those data structures that I could easily match
// to the respective "IT format specs" but there are still some gaps...

#ifndef IXS_MODULE_H
#define IXS_MODULE_H

#include "string.h"
#include <string>

#include "FileSFXI.h"
#include "MixerBase.h"

namespace IXS {

  // todo: model content..
  // it should probably be an array of 1000 blocks of 64 bytes.. see Buf0x40?
  typedef struct IntBuffer16k IntBuffer16k;
  struct IntBuffer16k {
    int buf_0x0[16000];
  };

  typedef struct ITPatternHead ITPatternHead;
  struct ITPatternHead {
    ushort len_0x0;
    ushort rows_0x2;  // Number of rows in this pattern (Ranges from 32->200)
    uint res_0x4;
  };


#define MASK_ENV_ON         1
#define MASK_ENV_LOOP       2
#define MASK_ENV_SUSLOOP    4

  typedef struct ITEnvelope ITEnvelope;
  struct ITEnvelope {
    byte flags_0x0;
    byte numNP_0x1;
    byte lpb_0x2;
    byte lpe_0x3;
    byte slb_0x4;
    byte sle_0x5;
    /*
        Node point = 1 byte for y-value
                        (0->64 for vol, -32->+32 for panning or pitch)
                     1 word (2 bytes) for tick number (0->9999)
     */
    byte nodePoints_0x6[75]; // 25 sets a 3 bytes
    byte reserved;
  };

  typedef struct ITInstrument ITInstrument;
  struct ITInstrument {
    uint id_0x0;            // magic "IMPI"
    char filename_0x4[12];
    byte zero_0x10;
    //        NNA = New Note Action
    //                0 = Cut                 1 = Continue
    //                2 = Note off            3 = Note fade
    byte nna_0x11;
    //         DCT = Duplicate Check Type
    //                0 = Off                 1 = Note
    //                2 = Sample              3 = Instrument
    byte dct_0x12;
    //         DCA: Duplicate Check Action
    //                0 = Cut
    //                1 = Note Off
    //                2 = Note fade
    byte dca_0x13;
    //       FadeOut:  Ranges between 0 and 128, but the fadeout "Count" is 1024
    //                See the Last section on how this works.
    //                Fade applied when:
    //                1) Note fade NNA is selected and triggered (by another note)
    //                2) Note off NNA is selected with no volume envelope
    //                   or volume envelope loop
    //                3) Volume envelope end is reached
    ushort fadeout_0x14;
    //        PPS: Pitch-Pan separation, range -32 -> +32
    char pps_0x16;
    //        PPC: Pitch-Pan center: C-0 to B-9 represented as 0->119 inclusive
    byte ppc_0x17;
    //        GbV: Global Volume, 0->128
    byte gbv_0x18;
    //        DfP: Default Pan, 0->64, &128 => Don't use
    byte dfp_0x19;
    //        RV: Random volume variation (percentage)
    byte rv_0x1a;
    //        RP: Random panning variation (panning change - not implemented yet)
    byte rp_0x1b;
    ushort trkvers_0x1c;
    byte nos_0x1e;
    byte res1_0x1f;
    char name_0x20[26];
    byte ifc_0x3a;
    byte ifr_0x3b;
    byte mch_0x3c;
    byte mpr_0x3d;
    ushort mbank_0x3e;
    //      Note-Sample/Keyboard Table.
    //       Each note of the instrument is first converted to a sample number
    //       and a note (C-0 -> B-9). These are stored as note/sample byte pairs
    //       (note first, range 0->119 for C-0 to B-9, sample ranges from
    //       1-99, 0=no sample)
    //
    byte keyboard_0x40[240];
    struct ITEnvelope volenv_0x130;   // alignment unproblemetic since bag of bytes..
    struct ITEnvelope panenv_0x182;
    struct ITEnvelope pitchenv_0x1d4;
    byte dummy_0x226[7]; // old format with dummy[7] instead of [4] (IT 2.17)
  };

  // IT sample flags
#define IT_SMPL  0x1        // Bit 0. On = sample associated with header.
#define IT_16BIT  0x2       // Bit 1. On = 16 bit, Off = 8 bit.
#define IT_STEREO 0x4       // Bit 2. On = stereo, Off = mono. Stereo samples not supported yet
#define IT_COMPRESSED 0x8   // Bit 3. On = compressed samples.
#define IT_LOOP 0x10        // Bit 4. On = Use loop
#define IT_SUSLOOP 0x20     // Bit 5. On = Use sustain loop
#define IT_PPLOOP 0x40      // Bit 6. On = Ping Pong loop, Off = Forwards loop
#define IT_PPSUSLOOP 0x80   // Bit 7. On = Ping Pong Sustain loop, Off = Forwards Sustain loop

  typedef struct ITSample ITSample;
  struct ITSample {
    uint magic_0x0; // "IMPS"
    char filename_0x4[12];
    byte zero_0x10;       // delim for filename
    byte gvl_0x11;
    byte flags_0x12;
    byte volume_0x13;
    char name_0x14[26];
    byte cvt_0x2e;
    byte dfp_0x2f;
    uint length_0x30;       // Length of sample in no. of samples NOT no. of bytes
    uint loopBegin_0x34;    // Start of loop (no of samples in, not bytes)
    uint loopEnd_0x38;      // Sample no. AFTER end of loop
    uint C5Speed_0x3c;      // Number of bytes a second for C-5 (ranges from 0->9999999)
    uint susLoopBegin_0x40; // Start of sustain loop
    uint susLoopEnd_0x44;   // Sample no. AFTER end of sustain loop
    uint samplePointer_0x48;// 'Long' Offset of sample in file.
    byte vis_0x4c;  // Vibrato Speed, ranges from 0->64
    byte vid_0x4d;  // Vibrato Depth, ranges from 0->64
    byte vir_0x4e;  // Vibrato Rate, rate at which vibrato is applied (0->64)
    byte vit_0x4f;  // Vibrato waveform type:
                    //    0=Sine wave
                    //    1=Ramp down
                    //    2=Square wave
                    //    3=Random (speed is irrelevant)
  };

  typedef struct Buf24 Buf24;
  struct Buf24 {
    byte envOn_0x0;
    byte nodeIdx_0x1;
    byte loopBegin_0x2;
    byte loopEnd_0x3;
    uint tickNum_0x4;
    int nodeY_0x8;
    int int_0xc;
    byte byte_0x10;
    byte field8_0x11;
    byte field9_0x12;
    byte field10_0x13;
    struct ITEnvelope *env_0x14;
  };

  typedef struct Buf0x40 Buf0x40;
  struct Buf0x40 {
    uint pos_0x0;
    byte *smplDataPtr_0x4;
    int int_0x8;
    uint bitsPerSmpl_0xc;
    uint channelPan_0x10; // quite a waste.. pan range is 0..64!
    uint volume_0x14;     // max. 17 bits!
    int smplForwardLoopFlag_0x18; // todo: 0,1,2,255 .. is this set to anything else? where?
    int smplLoopStart_0x1c;
    int smplLoopEnd_0x20;
    uint uint_0x24;   // C5*float-lookup; range 0->0x1312CFE0
    byte byte_0x28;
    byte byte_0x29;
    byte byte_0x2a;
    byte byte_0x2b;
    byte byte_0x2c;
    byte byte_0x2d;
    byte byte_0x2e;
    byte byte_0x2f;
    float float_0x30;
    uint uint_0x34;
    int int_0x38;
    struct BufInt uint_0x3c;
  };

  typedef struct Buf0x50 Buf0x50;
  struct Buf0x50 {
    int fadeoutCount_0x0;
    struct ITInstrument *insPtr_0x4;
    struct ITSample *smplHeadPtr_0x8;
//    int int_0xc;
    byte byte_0xc; // original code made an int write here but it is probably just 1 byte that is relevant
    byte unused_0xd;
    byte unused_0xe;
    byte unused_0xf;

    struct Buf0x40 buf40_0x10;
  };


  typedef struct Chan5 Chan5;
  struct Chan5 {
    byte note_0x0;      // Note ranges from 0->119 (C-0 -> B-9); 255 = note off, 254 = notecut
    byte numIns_0x1;    // instrument 1->99
    char vol_pan_0x2;   // volume/panning 0->212
    byte cmd_0x3;
    byte cmdArg_0x4;
  };

  // todo: map meaningful names to all those vars..
// todo: many fields still seem to get written using some other/shadow data structure - find & consolidate
  typedef struct Channel Channel;
  struct Channel {
    byte smplAssocWithHeaderFlag_0x0;
    Chan5 chan5_0x1;
    byte byte_0x6;
    byte byte_0x7;
    byte field8_0x8;
    byte field9_0x9;
    byte field10_0xa;
    byte byteNote_0xb;
    byte byte_0xc;
    byte field13_0xd;
    byte field14_0xe;
    byte field15_0xf;
    uint uint_0x10;
    uint uint_0x14;
    byte field18_0x18;
    byte field19_0x19;
    byte field20_0x1a;
    byte field21_0x1b;
    int floatTableOffset_0x1c; // range 0..0x1e00 used for float-table lookup
    int int_0x20;
    byte byte_0x24;
    byte flag_0x25;
    byte flag_0x26;
    byte flag_0x27;
    byte ChnlVol_0x28;
    byte ChnlPan_0x29;
    byte smplVol_0x2a;
    byte smplDfp_0x2b;
    byte byte_0x2c;
    byte byte_0x2d;
    byte byte_0x2e; // float lookup factor 1
    byte field35_0x2f;
    struct Buf24 envVol_0x30;
    struct Buf24 envPan_0x48;
    struct Buf24 envPitch_0x60;
    struct Buf0x50 subBuf80;
  };

  // the original header from the song file
  // note: alignment ok.. (if whole struct is aligned)
  typedef struct ImpulseHeaderStruct ImpulseHeaderStruct;
  struct ImpulseHeaderStruct {
    uint magic_0x0; // "IMPM" or "IXS!"
    char songName_0x4[12];
    byte unknown_0x10[14];
    ushort PHiligt_0x1e;
    ushort OrdNum_0x20;
    ushort InsNum_0x22;
    ushort SmpNum_0x24;
    ushort PatNum_0x26;
    ushort Cwtv_0x28;
    ushort Cmwt_0x2a;
    ushort Flags_0x2c;
    ushort Special_0x2e;
    byte GV_0x30;
    byte MV_0x31;
    byte Speed_0x32;
    byte Tempo_0x33;
    byte SEP_0x34;
    byte PWD_0x35;
    ushort MsgLgth_0x36;
    uint MsgOffset_0x38;
    uint reserved_0x3c;
    byte ChnlPan_0x40[64];
    byte ChnlVol_0x80[64];
  };

  typedef struct Module Module;
  struct Module {
    struct ImpulseHeaderStruct impulseHeader_0x0;
    byte *readPosition_0xc0;
    void *someBufferPtr_0xc4;
    byte *ordersBuffer_0xc8;
    struct ITInstrument **insPtrArray_0xcc;
    struct ITSample **smplHeadPtrArr0_0xd0;
    byte **smplDataPtrArr_0xd4;
    struct ITPatternHead **patHeadPtrArray_0xd8;
    byte **patDataPtrArray_0xdc;
    struct IntBuffer16k buf16k_0xe0;
    struct PlayerCore *ptrClassA_0xfae0;
    struct FileSFXI *optFileIXFS_0xfae4; // todo: what is this used for?
    byte *writeBufferPosPtr_0xfae8; // a max. 1048576 bytes sized buffer - shrunk to used size!
    int lastOrder_0xfaec;

    uint64_t channels;
  };

  extern float IXS_loadTotalLen_0043b898;
  extern float *IXS_ptrProgress_0043b89c;
  extern uint IXS_loadProgressCount_0043b8a0;

  // "protected/friend methods"
  void __thiscall IXS__Module__copyFromInputFile_00409370(Module *module, void *dest, uint len);
  byte __thiscall IXS__Module__readByte_0x409320(Module *module);
  ushort __thiscall IXS__Module__readShort_0x409330(Module *module);
  uint __thiscall IXS__Module__readInt_0x409350(Module *module);
  std::string IXS__Module__getTempFileName(const char *songname);


  // "public methods"
  char __thiscall IXS__Module__loadSongFileData_00407d60(
          Module *module, byte *inputFileBuf, uint fileSize, FileSFXI *alwaysNull, FileSFXI *tmpFile,
          float *loadProgressFeedbackPtr);

  byte *__thiscall IXS__Module__outputTrackerFile_00409430(Module *module, uint unused);

  IntBuffer16k *__thiscall IXS__Module__update16Kbuf_00408ec0(Module *module, int order);
  void __fastcall IXS__Module__dtor_00407bf0(Module *module);
  void __thiscall IXS__Module__clearVars_00407ba0(Module *module);
  Module *__thiscall IXS__Module__setPlayerCoreObj_00407b80(Module *module, PlayerCore *core);

  void __fastcall IXS__Module__initChannelBlock_00406380(Channel *chnl);
}
#endif //IXS_MODULE_H
