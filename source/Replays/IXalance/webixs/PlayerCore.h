/*
* Core player implementation.
*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#ifndef IXS_PLAYERCORE_H
#define IXS_PLAYERCORE_H

#include "Module.h"

namespace IXS {

  typedef struct Channels64 Channels64;
  struct Channels64 {
    struct Channel channels[64];
  };

  typedef struct BufFloat7680 BufFloat7680;
  struct BufFloat7680 {
    float array[7680];
  };


  typedef struct PlayerCore PlayerCore;
  struct PlayerCore {
    byte isMMOutDisabled_0x0;
    byte unused_0x1;
    byte unused_0x2;
    byte unused_0x3;
#if !defined(EMSCRIPTEN) && !defined(LINUX)
    LPCRITICAL_SECTION critSect_audioGen_0x4; // synchronization with separate thread
#endif
    struct Module *ptrModule_0x8;
    struct Channels64 channels_0xc;
    byte Tempo_0x320c;
    byte Speed_0x320d;
    byte Speed_0x320e;
    byte unused_0x320f;
    struct BufInt byteInt_0x3210; // also accessed as int, reading the following 3 bytes as well
    byte order_0x3214;
    byte ordIdx_0x3215;
    byte currentRow_0x3216;
    byte GV_0x3217;
    uint ordIdx_rowIdx_0x3218; // 16-bit: 8-bit ordIdx + 8-bit rowIdx - unused for playback
    struct ITPatternHead *patternHeadPtr_0x321c;
    struct IntBuffer16k *buf16kPtr_0x3220;
    struct MixerBase *ptrMixer_0x3224; // audio buffer etc
#if !defined(EMSCRIPTEN) && !defined(LINUX)
    uint threadIdGenAudio_0x3228;
    HANDLE *genAudioThreadHandle_0x322c;
#endif
    char someUnusedInput_0x3230;
    byte unused_0x3231;
    byte unused_0x3232;
    byte unused_0x3233;
#if !defined(EMSCRIPTEN) && !defined(LINUX)
    uint audioDevId_0x3234;
#else
    uint currentTimeMs;
#endif
    int intArray255_0x3238[255];
    struct BufFloat7680 floatArrays12x7680_0x3634[12];
    uint startTimeMs_0x5d634;
    uint waveStartPos_0x5d638;

    uint32_t endReached;  // added feature..
  };


  // "public methods"
  PlayerCore *__fastcall IXS__PlayerCore__ctor_addModule_004063b0(PlayerCore *core);

  void __thiscall IXS__PlayerCore__dtor_00406430(PlayerCore *core);

  int __thiscall IXS__PlayerCore__ctor_00406470(PlayerCore *core, uint sampleRate);
#if !defined(EMSCRIPTEN) && !defined(LINUX)
  uint __thiscall IXS__PlayerCore__getAudioDevId_00406740(PlayerCore *core);

  int __thiscall IXS__PlayerCore__stopGenAudioThread_00406750(PlayerCore *core);
#else
  uint IXS__PlayerCore__isAudioOutput16Bit(PlayerCore *core);

  uint IXS__PlayerCore__isAudioOutputStereo(PlayerCore *core);

  uint IXS__PlayerCore__getAudioBufferLen(PlayerCore *core);

  byte *IXS__PlayerCore__getAudioBuffer(PlayerCore *core);

  void IXS__PlayerCore__genAudio(PlayerCore *core);

#endif

  int __fastcall IXS__PlayerCore__setupAudioDeviceWiring_004067b0(PlayerCore *core, uint audioDevId);

  uint __thiscall IXS__PlayerCore__getOrdAndRowIdx_00407330(PlayerCore *core);

  double __thiscall IXS__PlayerCore__getWaveProgress_00407340(PlayerCore *core);

  void __thiscall IXS__PlayerCore__init16kBuf_004073a0(PlayerCore *core, uint someWord);

  void __thiscall IXS__PlayerCore__pauseOutput_00407a10(PlayerCore *core);

  void __thiscall IXS__PlayerCore__resumeOutput_00407a40(PlayerCore *core);

  Module *__thiscall IXS__PlayerCore__createModule_00407a70(PlayerCore *core);

  char *__thiscall IXS__PlayerCore__getSongTitle_00407b20(PlayerCore *core);

  void IXS__PlayerCore__setMixer(PlayerCore *core, bool filterOn);

    extern PlayerCore *IXS_PLAYER_CORE_REF_0043b88c;
  extern uint IXS_nSamplesPerSec_0043b890;

  extern uint IXS_AudioOutFlags_0043b888;
}

#endif //IXS_PLAYERCORE_H
