/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#include "PlayerIXS.h"

#include "winaudio.h"
#include "PlayerCore.h"

namespace IXS {

  // "private methods"
  PlayerIXS *__thiscall IXS__PlayerIXS__ctor_00405b50(PlayerIXS *player, uint sampleRate, int p1, int p2, char p3);
  void __fastcall IXS__PlayerIXS__dtor_00405be0(PlayerIXS *player);
  void IXS__PlayerIXS__getVolume_00405c00(float *outVolLeft, float *outVolRight);
  void IXS__PlayerIXS__setVolume_00405c20(float volLeft, float volRight);
  byte *IXS__PlayerIXS__outputTrackerFile00405c70(PlayerIXS *player, uint param_2);
  int IXS__PlayerIXS__initAudioOut_00405c40(PlayerIXS *player);
  char IXS__PlayerIXS__loadIxsFileData_00405c90(PlayerIXS *player, byte *inputFileBuf, uint fileSize, FileSFXI *optionalFile,
                                                FileSFXI *tmpFile, float *loadProgressFeedbackPtr);
  void IXS__PlayerIXS__stopGenAudioThread_00405cc0(PlayerIXS *player);
  void IXS__PlayerIXS__pauseOutput_00405cd0(PlayerIXS *player);
  void IXS__PlayerIXS__resumeOutput_00405ce0(PlayerIXS *player);
  uint IXS__PlayerIXS__getOrdAndRowIdx_00405cf0(PlayerIXS *player);
  double IXS__PlayerIXS__getWaveProgress_00405d00(PlayerIXS *player);
  void IXS__PlayerIXS__init16kBuf_00405d10(PlayerIXS *player, uint someWord);
  void IXS__PlayerIXS__delete_00405d30(PlayerIXS *player);
  Module *IXS__PlayerIXS__createModule_00405d50(PlayerIXS *player);
  char *IXS__PlayerIXS__getSongTitle_00405d60(PlayerIXS *player);
  void IXS__PlayerIXS__setOutputVolume_00405d70(PlayerIXS *player, float f);

  uint IXS__PlayerIXS__isAudioOutput16Bit(PlayerIXS *player);
  uint IXS__PlayerIXS__isAudioOutputStereo(PlayerIXS *player);
  uint IXS__PlayerIXS__getAudioBufferLen(PlayerIXS *player);
  byte *IXS__PlayerIXS__getAudioBuffer(PlayerIXS *player);
  void IXS__PlayerIXS__genAudio(PlayerIXS *player);
  uint IXS__PlayerIXS__getSampleRate();
  void IXS__PlayerIXS__setMixer(PlayerIXS *player, bool filterOn);

  uint32_t IXS__PlayerIXS__isSongEnd(PlayerIXS *player) {
    return player->ptrCore_0x4->endReached;
  }

  static IXS__PlayerIXS__VFTABLE IXS_PLAYERIXS_VFTAB_0042fed8 = {
          IXS__PlayerIXS__loadIxsFileData_00405c90,
          IXS__PlayerIXS__initAudioOut_00405c40,
          IXS__PlayerIXS__stopGenAudioThread_00405cc0,
          IXS__PlayerIXS__pauseOutput_00405cd0,
          IXS__PlayerIXS__resumeOutput_00405ce0,
          IXS__PlayerIXS__getOrdAndRowIdx_00405cf0,
          IXS__PlayerIXS__getWaveProgress_00405d00,
          IXS__PlayerIXS__getSongTitle_00405d60,
          IXS__PlayerIXS__init16kBuf_00405d10,
          IXS__PlayerIXS__delete_00405d30,
          IXS__PlayerIXS__createModule_00405d50,
          IXS__PlayerIXS__setOutputVolume_00405d70,
#if !defined(EMSCRIPTEN) && !defined(LINUX)
          IXS__PlayerIXS__getVolume_00405c00,
          IXS__PlayerIXS__setVolume_00405c20,
#else
          IXS__PlayerIXS__isAudioOutput16Bit,
          IXS__PlayerIXS__isAudioOutputStereo,
          IXS__PlayerIXS__getAudioBufferLen,
          IXS__PlayerIXS__getAudioBuffer,
          IXS__PlayerIXS__genAudio,
          IXS__PlayerIXS__getSampleRate,
          IXS__PlayerIXS__setMixer,

#endif
          IXS__PlayerIXS__outputTrackerFile00405c70,
          IXS__PlayerIXS__isSongEnd
  };


#if defined(EMSCRIPTEN) || defined(LINUX)
  uint IXS__PlayerIXS__isAudioOutput16Bit(PlayerIXS *player) {
    return IXS__PlayerCore__isAudioOutput16Bit(player->ptrCore_0x4);
  }
  uint IXS__PlayerIXS__isAudioOutputStereo(PlayerIXS *player) {
    return IXS__PlayerCore__isAudioOutputStereo(player->ptrCore_0x4);
  }
  uint IXS__PlayerIXS__getAudioBufferLen(PlayerIXS *player) {
    return IXS__PlayerCore__getAudioBufferLen(player->ptrCore_0x4);
  }
  byte *IXS__PlayerIXS__getAudioBuffer(PlayerIXS *player) {
    return IXS__PlayerCore__getAudioBuffer(player->ptrCore_0x4);
  }
  void IXS__PlayerIXS__genAudio(PlayerIXS *player) {
    IXS__PlayerCore__genAudio(player->ptrCore_0x4);
  }
  uint IXS__PlayerIXS__getSampleRate() {
    return IXS_nSamplesPerSec_0043b890;
  }
  void IXS__PlayerIXS__setMixer(PlayerIXS *player, bool filterOn) {
     IXS__PlayerCore__setMixer(player->ptrCore_0x4, filterOn);
  }

#endif

  PlayerIXS *__thiscall IXS__PlayerIXS__ctor_00405b50(PlayerIXS *player, uint sampleRate) {
    player->vftable = &IXS_PLAYERIXS_VFTAB_0042fed8;

    PlayerCore *core = (PlayerCore *) malloc(sizeof(PlayerCore));// 382524 bytes
    memset(core, 0, sizeof(PlayerCore));
    core = IXS__PlayerCore__ctor_addModule_004063b0(core);
    core->ptrMixer_0x3224 = nullptr;

    player->ptrCore_0x4 = core;
    IXS__PlayerCore__ctor_00406470(core, sampleRate);
    return player;
  }


  void __fastcall IXS__PlayerIXS__dtor_00405be0(PlayerIXS *player) {
    PlayerCore *core = player->ptrCore_0x4;
    player->vftable = &IXS_PLAYERIXS_VFTAB_0042fed8;
    if (core != (PlayerCore *) nullptr) {
      IXS__PlayerCore__dtor_00406430(core);
      operator delete(core);
    }
  }


  void IXS__PlayerIXS__getVolume_00405c00(float *outVolLeft, float *outVolRight) {
#if !defined(EMSCRIPTEN) && !defined(LINUX)
    M_getVolume_004062e0(outVolLeft, outVolRight);
#endif
  }


  void IXS__PlayerIXS__setVolume_00405c20(float volLeft, float volRight) {
#if !defined(EMSCRIPTEN) && !defined(LINUX)
    M_setVolume_00406340(volLeft, volRight);
#endif
  }


  int IXS__PlayerIXS__initAudioOut_00405c40(PlayerIXS *player) {
#if !defined(EMSCRIPTEN) && !defined(LINUX)
    uint audioDevId = IXS__PlayerCore__getAudioDevId_00406740(player->ptrCore_0x4);
#else
    uint audioDevId = 0;  // unused
#endif
    IXS__PlayerCore__setupAudioDeviceWiring_004067b0(player->ptrCore_0x4, audioDevId);
    return 0;
  }


// does not seem to be called in original player.
// maybe a functionality used to save IXS files?
  byte *IXS__PlayerIXS__outputTrackerFile00405c70(PlayerIXS *player, uint param_2) {
    byte *buf = IXS__Module__outputTrackerFile_00409430(player->ptrCore_0x4->ptrModule_0x8, param_2);
    return buf;
  }


// triggered by dialog from async_IXS_load_004055a0() .. both file params are 0
  char IXS__PlayerIXS__loadIxsFileData_00405c90(PlayerIXS *player, byte *inputFileBuf, uint fileSize, FileSFXI *optionalFile,
                                                FileSFXI *tmpFile,
                                                float *loadProgressFeedbackPtr) {

    return IXS__Module__loadSongFileData_00407d60
            (player->ptrCore_0x4->ptrModule_0x8, inputFileBuf, fileSize, optionalFile, tmpFile,
             loadProgressFeedbackPtr);
  }


  void IXS__PlayerIXS__stopGenAudioThread_00405cc0(PlayerIXS *player) {
#if !defined(EMSCRIPTEN) && !defined(LINUX)
    IXS__PlayerCore__stopGenAudioThread_00406750(player->ptrCore_0x4);
#endif
  }


  void IXS__PlayerIXS__pauseOutput_00405cd0(PlayerIXS *player) {
#if !defined(EMSCRIPTEN) && !defined(LINUX)
    IXS__PlayerCore__pauseOutput_00407a10(player->ptrCore_0x4);
#endif
  }


  void IXS__PlayerIXS__resumeOutput_00405ce0(PlayerIXS *player) {
#if !defined(EMSCRIPTEN) && !defined(LINUX)
    IXS__PlayerCore__resumeOutput_00407a40(player->ptrCore_0x4);
#endif
  }


  uint IXS__PlayerIXS__getOrdAndRowIdx_00405cf0(PlayerIXS *player) { // unused in player
    uint r = IXS__PlayerCore__getOrdAndRowIdx_00407330(player->ptrCore_0x4);
    return r;
  }


  double IXS__PlayerIXS__getWaveProgress_00405d00(PlayerIXS *player) {
    double r = IXS__PlayerCore__getWaveProgress_00407340(player->ptrCore_0x4);
    return r;
  }


  void IXS__PlayerIXS__init16kBuf_00405d10(PlayerIXS *player, uint someWord) {
    IXS__PlayerCore__init16kBuf_004073a0(player->ptrCore_0x4, someWord);
  }


  void IXS__PlayerIXS__delete_00405d30(PlayerIXS *player) {
    if (player != (PlayerIXS *) nullptr) {
      IXS__PlayerIXS__dtor_00405be0(player);
      operator delete(player);
    }
  }


  Module *IXS__PlayerIXS__createModule_00405d50(PlayerIXS *player) {
    Module *mod = IXS__PlayerCore__createModule_00407a70(player->ptrCore_0x4);
    return mod;
  }


  char *IXS__PlayerIXS__getSongTitle_00405d60(PlayerIXS *player) {
    char *r = IXS__PlayerCore__getSongTitle_00407b20(player->ptrCore_0x4);
    return r;
  }

/* does not seem to get used in the player */
  void IXS__PlayerIXS__setOutputVolume_00405d70(PlayerIXS *player, float f) {
    player->ptrCore_0x4->ptrMixer_0x3224->outputVolume_0x24 = f; // some master volume?
  }

// debugger shows that all three params always are set to 1 todo: get rid of unused p1-p3
  PlayerIXS *__cdecl IXS__PlayerIXS__createPlayer_00405d90(uint sampleRate) {
    PlayerIXS *player = (PlayerIXS *) malloc(sizeof(PlayerIXS));
    if (player != (PlayerIXS *) nullptr) {
      player = IXS__PlayerIXS__ctor_00405b50(player, sampleRate);

      (player->vftable->createModule)(player); // added..

      return player;
    }
    return (PlayerIXS *) nullptr;
  }
}