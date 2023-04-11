/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#include "PlayerCore.h"

#include <math.h>

#include "mem.h"
#include "winaudio.h"
#include "Mixer2.h"
#include "Mixer3.h"

namespace IXS {
  // "private methods"
#if defined(EMSCRIPTEN) || defined(LINUX)
  uint __thiscall IXS__PlayerCore__getPlayTime(PlayerCore *core);
#else
  uint IXS__PlayerCore__getAudioDeviceWavePos_00406240();
#endif
  uint __thiscall IXS__PlayerCore__getWavePos_00406990(PlayerCore *core);
  void __thiscall IXS__PlayerCore__genAudio_004069e0(PlayerCore *core, char modeAlways1);
  void __thiscall IXS__PlayerCore__oncePreMixerBuf20_00406a50(PlayerCore *core);
  uint __fastcall IXS__PlayerCore__readByteAsInt_00406a80(byte *buf);
  void __thiscall IXS__PlayerCore__FUN_00406a90(PlayerCore *core, int n);
  void IXS__PlayerCore__FUN_00406b80(Channel *chnl);
  byte __fastcall IXS__PlayerCore__FUN_00406be0(Buf24 *buf);
  void __thiscall IXS__PlayerCore__FUN_00406d90(PlayerCore *core, Channel *chnl);
  void IXS__PlayerCore__initInstrument_00407040(Channel *chnl);
  uint __thiscall IXS__PlayerCore__initEnvelopes_004070b0(PlayerCore *core, Channel *chnl);
  void __thiscall IXS__PlayerCore_initSample_00407160(PlayerCore *core, Channel *chnl);
  uint __thiscall IXS__PlayerCore__getVolume_00407220(PlayerCore *core, Channel *chnl);
  byte IXS__PlayerCore__getChannelPanning_00407280(Channel *chnl);
  uint __thiscall IXS__PlayerCore__useFloatTab_004072c0(PlayerCore *core, Channel *chnl);
  void IXS__PlayerCore__initSmplVolAndPan_00407310(Channel *chnl);
  void __thiscall IXS__PlayerCore__FUN_00407440(PlayerCore *core, Channel *chnl, byte mode, byte param_3);
  void __thiscall IXS__PlayerCore__FUN_004076f0(PlayerCore *core, Channel *chnl, byte param_2, byte param_3);
  void __thiscall IXS__PlayerCore__initSampleStuff_00407920(PlayerCore *core, Channel *chnl);
  bool IXS__PlayerCore__isABC_00407af0(Channel *chnl);

  const double IXS_2PI_0042ffb8 = 6.283185308;


// local vars
  uint IXS_nSamplesPerSec_0043b890 = 44100;
  uint IXS_AudioOutFlags_0043b888;

  bool IXS_stopAudioThreadFlag_0043b894;
  byte IXS_channelEnable_00438634[64] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                         0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                         0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                         0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                                         0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};

// global var
  PlayerCore *IXS_PLAYER_CORE_REF_0043b88c;

  // main circular audio output buffer
  static byte *sAudioOutBuffer = nullptr;


  PlayerCore *__fastcall IXS__PlayerCore__ctor_addModule_004063b0(PlayerCore *core) {

    int count = 0x40;
    Channel *channels64 = (Channel *) &core->channels_0xc;
    do {
      IXS__Module__initChannelBlock_00406380(channels64);
      channels64 = channels64 + 1;
      count = count + -1;
    } while (count != 0);

    Module *module = (Module *) malloc(sizeof(Module));
    memset(module, 0, sizeof(Module));
    if (module == (Module *) nullptr) {
      module = (Module *) nullptr;
    } else {
      module = IXS__Module__setPlayerCoreObj_00407b80(module, core);
    }
    core->ptrModule_0x8 = module;
    return core;
  }


  void __thiscall IXS__PlayerCore__dtor_00406430(PlayerCore *core) {
    MixerBase *mixer;
    Module *mod = core->ptrModule_0x8;
    if (mod != (Module *) nullptr) {
      IXS__Module__dtor_00407bf0(mod);
      operator delete(mod);
    }
    mixer = core->ptrMixer_0x3224;
    if (mixer != (MixerBase *) nullptr) {
      IXS__MixerBase__dtor_00409970(mixer);
      operator delete(mixer);
    }

#if defined(EMSCRIPTEN) || defined(LINUX)
    if (DIRECT_AUDIOBUFFER) free(DIRECT_AUDIOBUFFER);
#else
    if (sAudioOutBuffer) _GlobalFree(sAudioOutBuffer);  // moved responsibility from MixerBase
#endif
  }

  void IXS__PlayerCore__setMixer(PlayerCore *core, bool filterOn) {

    if (!core->ptrMixer_0x3224) {
      if (filterOn) {
        // lets just say this seems to be the "better"/"more expensive" impl..
        // (this is used in the original player.. it plays fine in the Linux
        // and Win32 builds but produces glitches in the EMSCRIPTEN build...)
        core->ptrMixer_0x3224 = IXS__Mixer3__ctor(sAudioOutBuffer);
      } else {
        // this seems to work more robustly for the EMSCRIPTEN build
        core->ptrMixer_0x3224 = IXS__Mixer2__ctor(sAudioOutBuffer);
      }

    } else {
      if (filterOn) {
        IXS__Mixer3__switchTo(core->ptrMixer_0x3224);
      } else {
        IXS__Mixer2__switchTo(core->ptrMixer_0x3224);
      }
    }
  }

// WARNING: Inlined function: _ftol_round_0041474c
  int __thiscall IXS__PlayerCore__ctor_00406470(PlayerCore *core, uint sampleRate) {
    core->isMMOutDisabled_0x0 = 0;

#if !defined(EMSCRIPTEN) && !defined(LINUX)
    sAudioOutBuffer = (byte *) _GlobalAlloc(0, 0x80000);  //524288 bytes
#else
    sAudioOutBuffer = nullptr;  // unused in this scenario
#endif

    IXS__PlayerCore__setMixer(core, true);

#if !defined(EMSCRIPTEN) && !defined(LINUX)
    uint audioDevId = MM_setupAudioDevice_00405f90();
    core->audioDevId_0x3234 = audioDevId;
#else
    // always using 16-bit samples..
    IXS_AudioOutFlags_0043b888 = 0x11a;   // 2 channel: 0x11a vs 1 channel: 0x116
    IXS_nSamplesPerSec_0043b890 = sampleRate;
#endif

    IXS__Module__clearVars_00407ba0(core->ptrModule_0x8);
    int *arrAddr = core->intArray255_0x3238;

    int k = 0;
    do {
      *arrAddr = (int) round((double) k * (double) 0.4 * (double) 0.008 *
                             (double) 0x1000000);
      k = k + 1;
      arrAddr = arrAddr + 1;
    } while (k < 0xff);

    int i = 0;
    BufFloat7680 *fPtrPtr = core->floatArrays12x7680_0x3634;
    float fVar1;
    double dVar2;
    do {
      if (i < 0xf00) {
        dVar2 = pow(2.0, (double) (i + -0xf00) * (-(double) 1.0 / 768.0));
        fVar1 = 1.0f / (float) dVar2;
      } else {
        dVar2 = pow(2.0, (double) (i + -0xf00) * ((double) 1.0 / 768.0));
        fVar1 = (float) dVar2;
      }

      fPtrPtr->array[0] = fVar1;
      i = i + 1;
      fPtrPtr = (BufFloat7680 *) (fPtrPtr->array + 1);
    } while (i < 0x1e00);

    return 1;
  }


#if !defined(EMSCRIPTEN) && !defined(LINUX)
  uint __thiscall IXS__PlayerCore__getAudioDevId_00406740(PlayerCore *core) {
    return core->audioDevId_0x3234;
  }


  int __thiscall IXS__PlayerCore__stopGenAudioThread_00406750(PlayerCore *core) {
    unsigned long exitStatus;

    IXS_stopAudioThreadFlag_0043b894 = true;
    exitStatus = (unsigned long) core;
    do {
      GetExitCodeThread(core->genAudioThreadHandle_0x322c, &exitStatus);
    } while (exitStatus == 0x103);

    if (core->isMMOutDisabled_0x0 == 0) {
      MM_audioclose_00405f40();
    }
    DeleteCriticalSection(core->critSect_audioGen_0x4);
    operator delete(core->critSect_audioGen_0x4);
    return 1;
  }

  int threadGenAudio_004066f0(uint sleep8Ms) {
    LPCRITICAL_SECTION critSect;

    do {
      critSect = IXS_PLAYER_CORE_REF_0043b88c->critSect_audioGen_0x4;
      EnterCriticalSection(critSect);
      IXS__PlayerCore__genAudio_004069e0(IXS_PLAYER_CORE_REF_0043b88c, '\x01');
      LeaveCriticalSection(critSect);
      Sleep(sleep8Ms);
    } while (IXS_stopAudioThreadFlag_0043b894 == false);
    return 0;
  }
#endif


  int __fastcall IXS__PlayerCore__setupAudioDeviceWiring_004067b0(PlayerCore *core, uint audioDevId) {
    IntBuffer16k *buf16k;
    unsigned long timeMs;
#if !defined(EMSCRIPTEN) && !defined(LINUX)
    LPCRITICAL_SECTION lpCriticalSection;
    HANDLE *hThread;
#endif
//    int iVar2;
    Channel *channelPtr;
    int ii;
    int i;
    byte order;
    Module *module;

    core->endReached = 0;

    module = core->ptrModule_0x8;
    core->Tempo_0x320c = (module->impulseHeader_0x0).Tempo_0x33;
    module->channels = 0;

    byte speed = (module->impulseHeader_0x0).Speed_0x32;
    core->Speed_0x320e = speed;
    core->Speed_0x320d = speed;

    core->ordIdx_0x3215 = 0;
    core->currentRow_0x3216 = 0;
    core->ordIdx_rowIdx_0x3218 = 0;
    *(uint *) &(core->byteInt_0x3210.b1) = 0x0;
    core->GV_0x3217 = (module->impulseHeader_0x0).GV_0x30;
    byte ordIdx = *module->ordersBuffer_0xc8;
    do {
      if (ordIdx != 0xfe) {
        order = module->ordersBuffer_0xc8[core->ordIdx_0x3215];
        core->order_0x3214 = order;
        buf16k = IXS__Module__update16Kbuf_00408ec0(module, order);
        core->buf16kPtr_0x3220 = buf16k;
        // loop initializes the 64*200 area..
        core->patternHeadPtr_0x321c = core->ptrModule_0x8->patHeadPtrArray_0xd8[core->order_0x3214];
        channelPtr = (core->channels_0xc).channels;
        i = 0;
        do {
          ii = i + 1;
          memset(channelPtr, 0, 200);

          channelPtr->ChnlVol_0x28 = (core->ptrModule_0x8->impulseHeader_0x0).ChnlVol_0x80[i];
          channelPtr->ChnlPan_0x29 = (core->ptrModule_0x8->impulseHeader_0x0).ChnlPan_0x40[i];
          channelPtr->smplAssocWithHeaderFlag_0x0 = 0;
          channelPtr = channelPtr + 1;
          i = ii;
        } while (ii < 64);

        IXS__MixerBase__ctor_004098d0
                (core->ptrMixer_0x3224, IXS_AudioOutFlags_0043b888 & 0xffff,
                 IXS_nSamplesPerSec_0043b890 & 0xffff);


        i = 0;
        core->ptrMixer_0x3224->audioOutBufPos_0x10 = 0;

        if (core->ptrMixer_0x3224->arrBuf20Len_0x28 > 0) {
          do {
            IXS__PlayerCore__oncePreMixerBuf20_00406a50(core);
            IXS__PlayerCore__FUN_00406a90(core, i);
            i = i + 1;
          } while (i < (int) core->ptrMixer_0x3224->arrBuf20Len_0x28);
        }

#if !defined(EMSCRIPTEN) && !defined(LINUX)
        if (core->isMMOutDisabled_0x0 == 0) {
          M_configWaveOutWrite_00405e30((uint *) core->ptrMixer_0x3224->circAudioOutBuffer_0x1c);
        } else {
          timeMs = timeGetTime();
          core->startTimeMs_0x5d634 = timeMs;
        }
#else
        timeMs = 0;   // use a manually controlled "time offset"
        core->startTimeMs_0x5d634 = core->currentTimeMs = timeMs;
#endif
        core->waveStartPos_0x5d638 = 0;

        IXS_stopAudioThreadFlag_0043b894 = false;
        IXS_PLAYER_CORE_REF_0043b88c = core;

#if !defined(EMSCRIPTEN) && !defined(LINUX)
        lpCriticalSection = (LPCRITICAL_SECTION) malloc(0x18);
        core->critSect_audioGen_0x4 = lpCriticalSection;
        InitializeCriticalSection(lpCriticalSection);

        hThread = (HANDLE *)
                CreateThread((LPSECURITY_ATTRIBUTES) nullptr, 0,
                             reinterpret_cast<LPTHREAD_START_ROUTINE>(threadGenAudio_004066f0),
                             reinterpret_cast<LPVOID>(8), 0, reinterpret_cast<LPDWORD>(&core->threadIdGenAudio_0x3228));
        core->genAudioThreadHandle_0x322c = hThread;
        SetThreadPriority(hThread, 1);
#endif
        return 1;
      }
      ordIdx = core->ordIdx_0x3215 + 1;
      core->ordIdx_0x3215 = ordIdx;
      ordIdx = module->ordersBuffer_0xc8[ordIdx];
    } while (ordIdx != 0xff);
    return 0;
  }




#if !defined(EMSCRIPTEN) && !defined(LINUX)
  uint __thiscall IXS__PlayerCore__getWavePos_00406990(PlayerCore *core) {
    if (core->isMMOutDisabled_0x0 != 0) {
      uint now = timeGetTime();
      return (uint) round((float) (uint64) (now - core->startTimeMs_0x5d634) *
                          0.001f * (float) core->ptrMixer_0x3224->bytesPerSample_0x2c
                          * (float) core->ptrMixer_0x3224->sampleRate_0x8);
    }
    // used by original player..
    return IXS__PlayerCore__getAudioDeviceWavePos_00406240();
  }

  // during playback this is called continuously (with only short 8ms
  // pauses that allow the player thread to be paused/stopped).. the
  // method waits until enough time has passed before it generates
  // the next "chunk" of data
  void __thiscall IXS__PlayerCore__genAudio_004069e0(PlayerCore *core, char modeAlways1) {

    while (true) {
      uint ordIdx_rowIdx;
      int pos;

      if (modeAlways1 == '\0') {
        pos = 0x7fffffff;
        core->ordIdx_rowIdx_0x3218 = 0;
      } else {
        pos = IXS__PlayerCore__getWavePos_00406990(core);
        ordIdx_rowIdx = IXS__MixerBase__getOrdAndRowIdx_00409a20(core->ptrMixer_0x3224, pos);
        core->ordIdx_rowIdx_0x3218 = ordIdx_rowIdx;
      }
      IXS__MixerBase__FUN_004099d0(core->ptrMixer_0x3224, pos);
      int n = IXS__MixerBase__findBufIdx_00409a80(core->ptrMixer_0x3224, pos);
      if (n == -1) break;

      IXS__PlayerCore__oncePreMixerBuf20_00406a50(core);
      IXS__PlayerCore__FUN_00406a90(core, n);
    }
  }
#else

  uint __thiscall IXS__PlayerCore__getPlayTime(PlayerCore *core) {
    uint now = core->currentTimeMs;
    return (uint) round((float) (uint64) (now - core->startTimeMs_0x5d634) *
                        0.001f * (float) core->ptrMixer_0x3224->bytesPerSample_0x2c
                        * (float) core->ptrMixer_0x3224->sampleRate_0x8);
  }

  byte *IXS__PlayerCore__getAudioBuffer(PlayerCore *core) {
    return DIRECT_AUDIOBUFFER;
  }
  uint IXS__PlayerCore__isAudioOutput16Bit(PlayerCore *core) {
    return  core->ptrMixer_0x3224->flag_0x4 & AUDIO_16BIT;
  }

  uint IXS__PlayerCore__isAudioOutputStereo(PlayerCore *core) {
    return  core->ptrMixer_0x3224->flag_0x4 & AUDIO_STEREO;
  }

  uint IXS__PlayerCore__getAudioBufferLen(PlayerCore *core) {
    uint blockBytesLen=  core->ptrMixer_0x3224->blockLen_0x14;
    uint blockLen= IXS__PlayerCore__isAudioOutput16Bit(core) ? blockBytesLen >> 1 : blockBytesLen;
    return IXS__PlayerCore__isAudioOutputStereo(core) ? blockLen >> 1 : blockLen;
  }

  // use IXS__PlayerCore__getAudioBufferLen (etc, for meta infos)
  void IXS__PlayerCore__genAudio(PlayerCore *core) {

    uint numSamples= IXS__PlayerCore__getAudioBufferLen(core);

    MixerBase *mixer = core->ptrMixer_0x3224;
    core->currentTimeMs += 1000 * numSamples / mixer->sampleRate_0x8;
//    uint pos = IXS__PlayerCore__getPlayTime(core);

    // note: idx range 0..31
/*
    while (true) {
      IXS__MixerBase__FUN_004099d0(mixer, pos);
      int idx = IXS__MixerBase__findBufIdx_00409a80(mixer, pos);
      if (idx == -1) break;

      IXS__PlayerCore__oncePreMixerBuf20_00406a50(core);
      IXS__PlayerCore__FUN_00406a90(core, idx);

    }
    */
    // no need for the circulr buffer handling
    IXS__PlayerCore__oncePreMixerBuf20_00406a50(core);
    IXS__PlayerCore__FUN_00406a90(core, 0);


    // note: one mixer->blockLen_0x14 worth of audio has been produced
    // into the DIRECT_AUDIOBUFFER
  }

#endif

  void __thiscall IXS__PlayerCore_initSample_00407160(PlayerCore *core, Channel *chnl) {
    ITInstrument *ins = chnl->subBuf80.insPtr_0x4;
    int byte_0xb = chnl->byteNote_0xb;
    byte note = ins->keyboard_0x40[2 * byte_0xb];
    byte sample = ins->keyboard_0x40[2 * byte_0xb + 1];

    chnl->subBuf80.byte_0xc = sample;
    chnl->subBuf80.smplHeadPtr_0x8 = core->ptrModule_0x8->smplHeadPtrArr0_0xd0[sample - 1];
    int note0 = ((int) note) << 6;
    chnl->floatTableOffset_0x1c = note0;
    chnl->int_0x20 = note0;

    ITSample *smplHead = chnl->subBuf80.smplHeadPtr_0x8;
    if ((smplHead->flags_0x12 & IT_SMPL) != 0) {
      chnl->smplAssocWithHeaderFlag_0x0 = 1;
      Buf0x40 &buf40 = chnl->subBuf80.buf40_0x10;
      buf40.pos_0x0 = chnl->uint_0x14 << 8;

      buf40.int_0x8 = 1;
      buf40.smplDataPtr_0x4 = core->ptrModule_0x8->smplDataPtrArr_0xd4[chnl->subBuf80.byte_0xc - 1];
      buf40.bitsPerSmpl_0xc = (smplHead->flags_0x12 & IT_16BIT) != 0 ? 16 : 8;
      chnl->smplVol_0x2a = smplHead->volume_0x13;
      chnl->smplDfp_0x2b = smplHead->dfp_0x2f;
    } else {
      chnl->smplAssocWithHeaderFlag_0x0 = 0;
    }
  }


  byte __thiscall IXS_PlayerCore_FUN_00406E10(PlayerCore *core) {

    core->Speed_0x320e = core->Speed_0x320d;;

    ITPatternHead *patHead = core->patternHeadPtr_0x321c;
    ushort row = core->currentRow_0x3216;

    if (row >= patHead->rows_0x2 || (*(uint *) &core->byteInt_0x3210 & 0x100) != 0) {
      Module *module = core->ptrModule_0x8;
      core->currentRow_0x3216 = core->byteInt_0x3210.b1;
      *(uint *) &core->byteInt_0x3210 = 0;

      do {
        byte ordIdx = core->ordIdx_0x3215 + 1;
        core->ordIdx_0x3215 = ordIdx;
        if (module->ordersBuffer_0xc8[ordIdx] == 0xff) {  // "end of song" marker
          MixerBase *mixer = core->ptrMixer_0x3224;
          core->ordIdx_0x3215 = 0;
          core->waveStartPos_0x5d638 = mixer->audioOutBufPos_0x10 + (mixer->outBufOverflowCount_0x18 << 19);

          core->endReached++;
        }
      } while (module->ordersBuffer_0xc8[core->ordIdx_0x3215] == 0xfe); // Skip to next order

      byte ord = module->ordersBuffer_0xc8[core->ordIdx_0x3215];
      core->order_0x3214 = ord;
      core->buf16kPtr_0x3220 = IXS__Module__update16Kbuf_00408ec0(module, ord);
      core->patternHeadPtr_0x321c = module->patHeadPtrArray_0xd8[core->order_0x3214];
    }

    int i = 0;
    int i0 = 0;
    Channel *chnlPtr = &core->channels_0xc.channels[0];
    do {
      char *src = (char *) core->buf16kPtr_0x3220 + 320 * core->currentRow_0x3216 + 5 * i;
      memcpy(&chnlPtr->chan5_0x1.note_0x0, src, 5); // copy 5 bytes

      if (IXS_channelEnable_00438634[i]) {

        byte cmd = chnlPtr->chan5_0x1.cmd_0x3;
        chnlPtr->byte_0x24 = 0;
        bool isInst = cmd != 7;

        if (IXS__PlayerCore__isABC_00407af0(chnlPtr)) {
          isInst = false;
        }
        Buf0x40 &buf40 = chnlPtr->subBuf80.buf40_0x10;
        if (chnlPtr->chan5_0x1.cmd_0x3 == 8) {
          buf40.uint_0x3c.b1 = 1;

        } else {
          if (buf40.uint_0x3c.b1) {
            chnlPtr->floatTableOffset_0x1c = chnlPtr->int_0x20;
          }
          buf40.uint_0x3c.b1 = 0;
        }

        if (isInst) {
          IXS__PlayerCore__initInstrument_00407040(chnlPtr);
          IXS__PlayerCore__initEnvelopes_004070b0(core, chnlPtr);
        }

        if (chnlPtr->chan5_0x1.cmd_0x3 == 15) {
          chnlPtr->uint_0x14 = ((ushort) chnlPtr->chan5_0x1.cmdArg_0x4) << 8;
        }

        if (chnlPtr->byte_0x24) {
          IXS__PlayerCore_initSample_00407160(core, chnlPtr);
        }
        if (isInst) {
          IXS__PlayerCore__initSmplVolAndPan_00407310(chnlPtr);
        }
        IXS__PlayerCore__FUN_004076f0(core, chnlPtr, chnlPtr->chan5_0x1.cmd_0x3, chnlPtr->chan5_0x1.cmdArg_0x4);
        IXS__PlayerCore__FUN_00407440(core, chnlPtr, chnlPtr->chan5_0x1.cmd_0x3, chnlPtr->chan5_0x1.cmdArg_0x4);
        IXS__PlayerCore__FUN_00406b80(chnlPtr);
        IXS__PlayerCore__FUN_00406d90(core, chnlPtr);
        IXS__PlayerCore__initSampleStuff_00407920(core, chnlPtr);
        i = i0;
      }
      ++i;
      chnlPtr += 1;
      i0 = i;
    } while (i < 64);

    core->currentRow_0x3216 += 1;
    return core->currentRow_0x3216;
  }

  void __thiscall IXS__PlayerCore__oncePreMixerBuf20_00406a50(PlayerCore *core) {
    byte speed;
    byte isEnabled;
    int i;
    Channel *channelPtr;

    speed = core->Speed_0x320e;
    if (speed)
      core->Speed_0x320e = speed - 1;
    if (!core->Speed_0x320e) {
      IXS_PlayerCore_FUN_00406E10(core);

    } else {
      i = 0;
      channelPtr = (Channel *) &core->channels_0xc;
      do {
        isEnabled = IXS_channelEnable_00438634[i];
        if (isEnabled) {
          channelPtr->byte_0x24 = 0;
          if (!IXS__PlayerCore__isABC_00407af0(channelPtr)) {
            IXS__PlayerCore__initSmplVolAndPan_00407310(channelPtr);
          }
          IXS__PlayerCore__FUN_00407440(core, channelPtr, channelPtr->chan5_0x1.cmd_0x3, channelPtr->chan5_0x1.cmdArg_0x4);
          IXS__PlayerCore__FUN_00406b80(channelPtr);
          IXS__PlayerCore__FUN_00406d90(core, channelPtr);
        }
        ++i;
        channelPtr += 1;
      } while (i < 64);
    }
  }



  uint __fastcall IXS__PlayerCore__readByteAsInt_00406a80(byte *buf) {
    return (uint) *buf;
  }


  void __thiscall IXS__PlayerCore__FUN_00406a90(PlayerCore *core, int n) {
    core->ptrMixer_0x3224->vftable->clearSampleBuf(core->ptrMixer_0x3224, core->Tempo_0x320c);
    Channel *chnl = &core->channels_0xc.channels[0];

    for (int i = 0; i<64; i++) {
      if (IXS__PlayerCore__readByteAsInt_00406a80((byte *) chnl)) {
        core->ptrMixer_0x3224->vftable->genChannelOutput(core->ptrMixer_0x3224, &chnl->subBuf80.buf40_0x10);
      }
      chnl += 1;
    }

    core->ptrMixer_0x3224->vftable->copyToAudioBuf(core->ptrMixer_0x3224, n);
    core->ptrMixer_0x3224->arrBuf20Ptr_0x38[n].ordIdx_0xc = core->ordIdx_0x3215;
    core->ptrMixer_0x3224->arrBuf20Ptr_0x38[n].currentRow_0x10 = core->currentRow_0x3216;
  }

  void IXS__PlayerCore__FUN_00406b80(Channel *chnl) {
    Buf24 &volBuf = chnl->envVol_0x30;
    if (volBuf.envOn_0x0 != 0) {
      chnl->flag_0x27 |=  IXS__PlayerCore__FUN_00406be0(&volBuf); // that was a nasty bug.. not having the function return AL..
      chnl->byte_0x2c = (byte) (volBuf.nodeY_0x8 >> 8);
    }

    Buf24 &panBuf = chnl->envPan_0x48;
    if (panBuf.envOn_0x0 != 0) {
      IXS__PlayerCore__FUN_00406be0(&panBuf);
      chnl->byte_0x2d = (byte) (panBuf.nodeY_0x8 >> 8);
    }

    Buf24 &pitchBuf = chnl->envPitch_0x60;
    if (pitchBuf.envOn_0x0 != 0) {
      IXS__PlayerCore__FUN_00406be0(&pitchBuf);
      chnl->byte_0x2e = (byte) (pitchBuf.nodeY_0x8 >> 8);
    }
  }

  byte __fastcall IXS__PlayerCore__FUN_00406be0(Buf24 *buf)
  {
    ITEnvelope *env;

    byte byte_0x10 = buf->byte_0x10;
    if ( byte_0x10 || (env = buf->env_0x14, (env->flags_0x0 & MASK_ENV_SUSLOOP) == 0) ) {
      env = buf->env_0x14;
      byte b;
      byte lpe;
      if ( (env->flags_0x0 & MASK_ENV_LOOP) != 0 ) {
        b = byte_0x10 | 2;    // set bit #1
        buf->loopBegin_0x2 = env->lpb_0x2;
        lpe = env->lpe_0x3;
      } else {
        buf->loopBegin_0x2 = 0;
        lpe = env->numNP_0x1 - 1;
        b = byte_0x10 & 0xfd;    // clear bit #1
      }
      buf->loopEnd_0x3 = lpe;
      buf->byte_0x10 = b;
    } else {
      buf->loopBegin_0x2 = env->slb_0x4;
      buf->loopEnd_0x3 = env->sle_0x5;
      buf->byte_0x10 = 2;         // set exclusively bit #1
    }


    char bufA[3];
    char bufB[3];

    byte * np1 = (byte*)&env->nodePoints_0x6[3 * buf->nodeIdx_0x1];
    memcpy(bufB, np1, 3);

    byte byte_0x1 = buf->nodeIdx_0x1;
    byte loopEnd = buf->loopEnd_0x3;

    if (byte_0x1 != loopEnd || (buf->byte_0x10 & 2) != 0 ) {
      byte nodeIdx1 = buf->loopBegin_0x2;
      if (nodeIdx1 == loopEnd && byte_0x1 == nodeIdx1 ) {
        buf->nodeY_0x8 = bufB[0] << 8;
        return 0;
      } else {
        ITEnvelope *envPtr = buf->env_0x14;
        if (byte_0x1 == loopEnd ) {
          buf->nodeIdx_0x1 = nodeIdx1;

          byte *np2 = (byte*)&envPtr->nodePoints_0x6[3 * nodeIdx1];
          memcpy(bufB, np2, 3);

          buf->nodeY_0x8 = bufB[0] << 8;
          buf->tickNum_0x4 = U16(bufB + 1);
        }

        byte nodeIdx = buf->nodeIdx_0x1 + 1;

        byte *np3 = (byte*)&envPtr->nodePoints_0x6[3 * nodeIdx];
        memcpy(bufA, np3, 3);

        if (U16(&bufA[1]) == U16(&bufB[1]) ) {
          buf->int_0xc = 0;
        } else {
          buf->int_0xc = (int)((double)(bufA[0] - bufB[0]) * 256.0f) / (U16(&bufA[1]) - U16(&bufB[1]));
        }
        uint tickNum = buf->tickNum_0x4 + 1;
        buf->nodeY_0x8 += buf->int_0xc;
        buf->tickNum_0x4 = tickNum;

        if (tickNum > U16(&bufA[1]) ) {
          buf->nodeIdx_0x1 = nodeIdx;
          buf->nodeY_0x8 = bufA[0] << 8;
        }
        return 0;
      }
    } else {
      buf->nodeY_0x8 = bufB[0] << 8;
      return 1;
    }
  }


  void __thiscall IXS__PlayerCore__FUN_00406d90(PlayerCore *core, Channel *chnl) {

    Buf0x50 &buf50 = chnl->subBuf80;
    if (chnl->flag_0x27 != 0) {
      ITInstrument *ins = buf50.insPtr_0x4;
      buf50.fadeoutCount_0x0 -= ins->fadeout_0x14;

      if (buf50.fadeoutCount_0x0 < 0) { // i.e. must be a signed int
        buf50.fadeoutCount_0x0 = 0;
      }
      if ((buf50.fadeoutCount_0x0 == 0) || (ins->fadeout_0x14 == 0)) {
        chnl->smplAssocWithHeaderFlag_0x0 = 0;
      }
    }
    if (chnl->flag_0x26 != 0) {
      chnl->flag_0x26 = 0;
      chnl->smplAssocWithHeaderFlag_0x0 = 0;
    }
    if (chnl->smplAssocWithHeaderFlag_0x0 != 0) {
      Buf0x40 &buf40 = buf50.buf40_0x10;
      buf40.volume_0x14 = IXS__PlayerCore__getVolume_00407220(core, chnl);
      buf40.channelPan_0x10 = IXS__PlayerCore__getChannelPanning_00407280(chnl);

      buf40.uint_0x24 = IXS__PlayerCore__useFloatTab_004072c0(core, chnl);
    }
  }


  void IXS__PlayerCore__initInstrument_00407040(Channel *chnl) {
    byte note = chnl->chan5_0x1.note_0x0;
    if (note != 0) {
      if (note == 0xfe) {
        chnl->flag_0x26 = 1;
        return;
      }
      if (note == 0xff) {
        chnl->flag_0x25 = 1;
        return;
      }
      if (note < 120) {
        chnl->byte_0x6 = note;
        chnl->byteNote_0xb = note;
        chnl->byte_0x2c = 0x40;
        chnl->byte_0x2e = 0x20;
        chnl->byte_0x2d = 0x20;
        chnl->uint_0x10 = 0x2000;
        (chnl->subBuf80).fadeoutCount_0x0 = 0x400;
        chnl->flag_0x26 = 0;
        chnl->flag_0x25 = 0;
        chnl->flag_0x27 = 0;
        chnl->byte_0x24 = 1;
        chnl->uint_0x14 = 0;
        return;
      }
      chnl->flag_0x27 = 1;
    }
  }


  uint __thiscall IXS__PlayerCore__initEnvelopes_004070b0(PlayerCore *core, Channel *chnl) {
    byte b = chnl->chan5_0x1.numIns_0x1;
    if (b != 0) {
      chnl->byte_0x7 = b;
      chnl->byte_0xc = b;

      ITInstrument *insPtr = core->ptrModule_0x8->insPtrArray_0xcc[b - 1];
      (chnl->subBuf80).insPtr_0x4 = insPtr;
      chnl->byte_0x24 |=  2;

      Buf24 &chnlVol = chnl->envVol_0x30;
      ITEnvelope &insVol = insPtr->volenv_0x130;
      chnlVol.nodeIdx_0x1 = 0;
      chnlVol.tickNum_0x4 = 0;
      chnlVol.env_0x14 = &insVol;
      chnlVol.envOn_0x0 = insVol.flags_0x0 & MASK_ENV_ON;
      chnlVol.nodeY_0x8 = (int) (char) insVol.nodePoints_0x6[0] << 8;

      Buf24 &chnlPan = chnl->envPan_0x48;
      ITEnvelope &insPan = insPtr->panenv_0x182;
      chnlPan.nodeIdx_0x1 = 0;
      chnlPan.tickNum_0x4 = 0;
      chnlPan.env_0x14 = &insPan;
      chnlPan.envOn_0x0 = insPan.flags_0x0 & MASK_ENV_ON;
      chnlPan.nodeY_0x8 = (int) (char) insPan.nodePoints_0x6[0] << 8;

      Buf24 &chnlPitch = chnl->envPitch_0x60;
      ITEnvelope &insPitch = insPtr->pitchenv_0x1d4;
      chnlPitch.nodeIdx_0x1 = 0;
      chnlPitch.tickNum_0x4 = 0;
      chnlPitch.env_0x14 = &insPitch;
      chnlPitch.envOn_0x0 = insPitch.flags_0x0 & MASK_ENV_ON;
      chnlPitch.nodeY_0x8 = (int) (char) insPitch.nodePoints_0x6[0] << 8;
    }
    return (uint) chnl;
  }


  void __thiscall IXS__PlayerCore__initSample_00407160(PlayerCore *core, Channel *chnl) {

    Module *module = core->ptrModule_0x8;
    Buf0x50 &chnl50 = chnl->subBuf80;

    uint noteOffset = (uint) chnl->byteNote_0xb * 2;
    ITInstrument *ins = chnl50.insPtr_0x4;
    byte sampleByte = ins->keyboard_0x40[noteOffset + 1];
    ITSample *smplHeadPtr = module->smplHeadPtrArr0_0xd0[sampleByte - 1];
    int note = ((int) ins->keyboard_0x40[noteOffset] << 6);

    chnl50.byte_0xc = sampleByte;
    chnl50.smplHeadPtr_0x8 = smplHeadPtr;

    chnl->floatTableOffset_0x1c = note;
    chnl->int_0x20 = note;

    if ((smplHeadPtr->flags_0x12 & IT_SMPL) == 0) {
      // NO sample associated with header.
      chnl->smplAssocWithHeaderFlag_0x0 = 0;
      return;
    }
    // sample associated with header.
    chnl->smplAssocWithHeaderFlag_0x0 = 1;
    Buf0x40 &buf40 = chnl50.buf40_0x10;
    buf40.pos_0x0 = chnl->uint_0x14 << 8;
    buf40.int_0x8 = 1;
    buf40.smplDataPtr_0x4 = module->smplDataPtrArr_0xd4[chnl50.byte_0xc - 1];
    buf40.bitsPerSmpl_0xc = (-(uint) ((smplHeadPtr->flags_0x12 & IT_16BIT) != 0) & 8) + 8;
    chnl->smplVol_0x2a = smplHeadPtr->volume_0x13;
    chnl->smplDfp_0x2b = smplHeadPtr->dfp_0x2f;
  }


  uint __thiscall IXS__PlayerCore__getVolume_00407220(PlayerCore *core, Channel *chnl) {

    ImpulseHeaderStruct &head = core->ptrModule_0x8->impulseHeader_0x0;
    Buf0x50 &buf50 = chnl->subBuf80;
    ITInstrument *ins = buf50.insPtr_0x4;
    ITSample *smpl = buf50.smplHeadPtr_0x8;

    uint v1 = ((uint) smpl->gvl_0x11 * chnl->smplVol_0x2a * chnl->ChnlVol_0x28) >> 10;  // fits into 14 bits (max 0x3F40)
    v1 = (v1 * ins->gbv_0x18 * chnl->byte_0x2c) >> 10;     // fits 32-bits (pre shift); max result: 0xFB06F

    // max fadeoutCount_0x0 is 1024
    return ((v1 * head.GV_0x30 * (uint)buf50.fadeoutCount_0x0) >> 21); // fits 32-bits (pre shift) .. result can be up to 1F417!
  }


  byte IXS__PlayerCore__getChannelPanning_00407280(Channel *chnl) {
    byte bVar1;
    char cVar2;
    uint uVar3;

    if ((chnl->smplDfp_0x2b & 0x80) == 0) {
      bVar1 = chnl->ChnlPan_0x29;
    } else {
      bVar1 = chnl->smplDfp_0x2b & 0x7f;
    }
    cVar2 = bVar1 - 0x20;
    uVar3 = (int) cVar2 >> 0x1f;
    return (char) ((int) ((0x20 - (((int) cVar2 ^ uVar3) - uVar3)) * (chnl->byte_0x2d - 0x20)) >> 5) +
           cVar2 + 0x20;
  }


  uint __thiscall IXS__PlayerCore__useFloatTab_004072c0(PlayerCore *core, Channel *chnl) {
//  return (uint) round((float) (uint64) ((chnl->subBuf80).smplHeadPtr_0x8)->C5Speed_0x3c *
//                      *(float *) ((int) &(core->channels_0xc).channels[0x30].subBuf80 +
//                                  ((uint) chnl->byte_0x2e * 0x20 + chnl->floatTableOffset_0x1c) * 4 + 0x30));

    // this is what the above gibberish actually does (i.e. use of pre-generated float table)
    // todo: find where the -0x1000 offset comes from..

    // note: range of the float values is 0.031250->31.971132
    float f= *(float *) ((uintptr_t) (byte *) core->floatArrays12x7680_0x3634 - 0x1000 +
                         ((uint) chnl->byte_0x2e * 0x20 + chnl->floatTableOffset_0x1c) * 4);

    // C5 supposedly ranges: 0->9999999   .. i.e. max below is 0x1312CFE0 (no 64-bit needed)
    return (uint) round(f * ((chnl->subBuf80).smplHeadPtr_0x8)->C5Speed_0x3c);  // todo: round here is probably overkill
  }


  void IXS__PlayerCore__initSmplVolAndPan_00407310(Channel *chnl) {
    byte b = chnl->chan5_0x1.vol_pan_0x2;
    if (b < 65) {
      // Volume ranges from 0->64
      chnl->smplVol_0x2a = b;
    }
    if ((127 < b) && (b < 193)) {
      // Panning ranges from 0->64, mapped onto 128->192
//      chnl->smplDfp_0x2b = b + 128;   original
      chnl->smplDfp_0x2b = b - 128;
    }
  }


  uint __thiscall IXS__PlayerCore__getOrdAndRowIdx_00407330(PlayerCore *core) {
    return core->ordIdx_rowIdx_0x3218;  // only 16-bit used in result
  }

  double __thiscall IXS__PlayerCore__getWaveProgress_00407340(PlayerCore *core) {
    uint pos;
    uint totalSamples;
    int d;

    totalSamples = IXS_nSamplesPerSec_0043b890 & 0xffff;
    if ((IXS_AudioOutFlags_0043b888 & AUDIO_16BIT) != 0) {
      totalSamples = totalSamples << 1;
    }
    if ((IXS_AudioOutFlags_0043b888 & AUDIO_STEREO) != 0) {
      totalSamples = totalSamples << 1;
    }
#if !defined(EMSCRIPTEN) && !defined(LINUX)
    pos = IXS__PlayerCore__getAudioDeviceWavePos_00406240();
#else
    pos = IXS__PlayerCore__getPlayTime(core); // just use the rendering position
#endif
    d = pos - core->waveStartPos_0x5d638;
    if (d < 0) {
      d = 0;
    }
    return (double) d / (double) totalSamples;
  }


  void __thiscall IXS__PlayerCore__init16kBuf_004073a0(PlayerCore *core, uint someWord) {
    IntBuffer16k *buf16k;
    byte hi;
    Module *module;

    module = core->ptrModule_0x8;
    core->currentRow_0x3216 = (byte) someWord;
    *(uint *) &(core->byteInt_0x3210.b1) = 0;
    core->ordIdx_0x3215 = (char) (someWord >> 8) - 1;
    do {
      hi = core->ordIdx_0x3215 + 1;
      core->ordIdx_0x3215 = hi;
      if (module->ordersBuffer_0xc8[hi] == 0xff) {
        core->ordIdx_0x3215 = 0;
      }
    } while (module->ordersBuffer_0xc8[core->ordIdx_0x3215] == 0xfe);
    hi = module->ordersBuffer_0xc8[core->ordIdx_0x3215];
    core->order_0x3214 = hi;
    buf16k = IXS__Module__update16Kbuf_00408ec0(module, hi);
    core->buf16kPtr_0x3220 = buf16k;
    core->patternHeadPtr_0x321c = core->ptrModule_0x8->patHeadPtrArray_0xd8[core->order_0x3214];
  }

  void __thiscall IXS__PlayerCore__FUN_00407440(PlayerCore *core, Channel *chnl, byte cmd, byte cmdArg) {
    const float DIV256 = 1.0f / 256; // 0.00390625;

    byte bVar1;
    uint uVar2;
    uint uVar4;
    double fVar6;
    double fVar7;
    double dVar8;

    float _param_3 = (float) (uint) cmdArg;
    Buf0x40 &buf40 = (chnl->subBuf80).buf40_0x10;
    int vol = buf40.int_0x38 + 1;
    buf40.int_0x38 = vol;
    switch (cmd) {
      case 4:
        if (cmdArg == 0) {
          cmdArg = buf40.byte_0x28;
          _param_3 = (float) (uint) cmdArg;
        } else {
          buf40.byte_0x28 = cmdArg;
        }
        if ((cmdArg & 0xf) == 0) {
          vol = ((uint) _param_3 >> 4) + (uint) chnl->smplVol_0x2a;
          if (64 < vol) {
            vol = 64;
          }
          chnl->smplVol_0x2a = (byte) vol;
          return;
        }
        if ((cmdArg & 0xf0) == 0) {
          vol = (uint) chnl->smplVol_0x2a - (cmdArg & 0xf);
          if (vol < 0) {
            vol = 0;
          }
          chnl->smplVol_0x2a = (byte) vol;
          return;
        }
        break;
      case 5: {
        if (cmdArg == 0) {
          cmdArg = buf40.byte_0x29;
          _param_3 = (float) (uint) cmdArg;
        } else {
          buf40.byte_0x29 = cmdArg;
        }
        if ((cmdArg & 0xf0) == 0xf0) {
          return;
        }
        if ((cmdArg & 0xf0) == 0xe0) {
          return;
        }
        int os = chnl->floatTableOffset_0x1c + (int) _param_3 * -4;
        if (os < 0) {
          chnl->floatTableOffset_0x1c = 0;
        } else {
          chnl->floatTableOffset_0x1c = os;
        }
      }
        goto LAB_00407579;
      case 6: {
        if (cmdArg == 0) {
          cmdArg = buf40.byte_0x29;
          _param_3 = (float) (uint) cmdArg;
        } else {
          buf40.byte_0x29 = cmdArg;
        }
        if ((cmdArg & 0xf0) == 0xf0) {
          return;
        }
        if ((cmdArg & 0xf0) == 0xe0) {
          return;
        }
        // 1e00-float table lookup handling
        int os = chnl->floatTableOffset_0x1c + (int) _param_3 * 4;
        if (0x1e00 < os) {
          chnl->floatTableOffset_0x1c = 0x1e00;
        } else {
          chnl->floatTableOffset_0x1c = os;
        }
      }
      LAB_00407579:
        if (chnl->smplAssocWithHeaderFlag_0x0 != 0) {
          buf40.uint_0x24 = IXS__PlayerCore__useFloatTab_004072c0(core, chnl);
          return;
        }
        break;
      case 7:
        vol = buf40.uint_0x34;
        uVar2 = chnl->floatTableOffset_0x1c;
        uVar4 = (uint) buf40.byte_0x2a;
        if (uVar2 == vol) {
          return;
        }
        if ((int) uVar2 < (int) vol) {
          uVar2 = uVar2 + uVar4 * 4;
          if ((int) vol < (int) uVar2) {
            LAB_004075c2:
            uVar2 = vol;
          }
        } else {
          uVar2 = uVar2 + uVar4 * -4;
          if ((int) uVar2 < (int) vol) goto LAB_004075c2;
        }
        if (chnl->smplAssocWithHeaderFlag_0x0 != 0) {
          chnl->floatTableOffset_0x1c = uVar2;
          buf40.uint_0x24 = IXS__PlayerCore__useFloatTab_004072c0(core, chnl);
          return;
        }
        break;
      case 8:
        _param_3 = 0.0;
        bVar1 = buf40.byte_0x2b;
        fVar6 = 0.0;
        if (bVar1 >> 4 != 0) {
          _param_3 = (float) (uint) (bVar1 >> 4) * (double) 4.0;
        }
        if ((bVar1 & 0xf) != 0) {
          fVar6 = (double) (bVar1 & 0xf) * (double) 4.0;
        }
        fVar7 = (double) sin((double) buf40.float_0x30 *
                             (double) IXS_2PI_0042ffb8);
        dVar8 = fmod((double) (_param_3 * DIV256 + buf40.float_0x30),
                     1.0L);
        buf40.float_0x30 = (float) dVar8;
        chnl->floatTableOffset_0x1c = (int) round((float) chnl->int_0x20 + (float) -(fVar6 * fVar7));
      case 0x13:
        if (((buf40.byte_0x2c & 0xf0) == 0xd0) &&
            (buf40.byte_0x2d == vol)) {
          IXS__PlayerCore__initInstrument_00407040(chnl);
          IXS__PlayerCore__initEnvelopes_004070b0(core, chnl);
          if (chnl->byte_0x24 != 0) {
            IXS__PlayerCore__initSample_00407160(core, chnl);
            IXS__PlayerCore__initSmplVolAndPan_00407310(chnl);
          }
          IXS__PlayerCore__initSampleStuff_00407920(core, chnl);
        }
      default:;
    }
  }


  void __thiscall IXS__PlayerCore__FUN_004076f0(PlayerCore *core, Channel *chnl, byte cmd, byte cmdArg) {
    ITSample *pIVar1;
    byte bVar2;
    uint uVar3;
    int iVar4;

    Buf0x40 &buf40 = (chnl->subBuf80).buf40_0x10;
    buf40.int_0x38 = -1;
    switch (cmd) {
      case 3:
        *((uint *) &core->byteInt_0x3210) = cmdArg + 0x100;
        return;
      case 4:
        uVar3 = (uint) cmdArg;
        if (cmdArg == 0) {
          uVar3 = (uint) buf40.byte_0x28;
        } else {
          buf40.byte_0x28 = cmdArg;
        }
        if (((uVar3 & 0xf) != 0) && ((uVar3 & 0xf0) != 0)) {
          if (((byte) uVar3 & 0xf) == 0xf) {
            bVar2 = chnl->smplVol_0x2a;
            if (0x40 < bVar2) {
              bVar2 = 0x40;
            }
            chnl->smplVol_0x2a = bVar2;
            return;
          }
          if (((byte) uVar3 & 0xf0) == 0xf0) {
            iVar4 = (uint) chnl->smplVol_0x2a - (uVar3 & 0xf);
            if (iVar4 < 0) {
              iVar4 = 0;
            }
            chnl->smplVol_0x2a = (byte) iVar4;
            return;
          }
        }
        break;
      case 5:
        if (cmdArg == 0) {
          cmdArg = buf40.byte_0x29;
        } else {
          buf40.byte_0x29 = cmdArg;
        }
        if ((cmdArg & 0xf0) == 0xf0) {
          uVar3 = (cmdArg & 0xf) << 2;
        } else {
          if ((cmdArg & 0xf0) != 0xe0) {
            return;
          }
          uVar3 = cmdArg & 0xf;
        }
        iVar4 = chnl->floatTableOffset_0x1c - uVar3;
        chnl->floatTableOffset_0x1c = iVar4;
        if (iVar4 < 0) {
          chnl->floatTableOffset_0x1c = 0;
        }
        goto LAB_0040783d;
      case 6:
        if (cmdArg == 0) {
          cmdArg = buf40.byte_0x29;
        } else {
          buf40.byte_0x29 = cmdArg;
        }
        if ((cmdArg & 0xf0) == 0xf0) {
          uVar3 = (cmdArg & 0xf) << 2;
        } else {
          if ((cmdArg & 0xf0) != 0xe0) {
            return;
          }
          uVar3 = cmdArg & 0xf;
        }
        iVar4 = chnl->floatTableOffset_0x1c + uVar3;
        chnl->floatTableOffset_0x1c = iVar4;
        if (0x1e00 < iVar4) {
          chnl->floatTableOffset_0x1c = 0x1e00;
        }
      LAB_0040783d:
        if (chnl->smplAssocWithHeaderFlag_0x0 != 0) {
          buf40.uint_0x24 = IXS__PlayerCore__useFloatTab_004072c0(core, chnl);
          return;
        }
        break;
      case 7:
        if (cmdArg != 0) {
          buf40.byte_0x2a = cmdArg;
          pIVar1 = (chnl->subBuf80).smplHeadPtr_0x8;
          if (pIVar1 != (ITSample *) nullptr) {
            chnl->smplVol_0x2a = pIVar1->volume_0x13;
          }
        }
        if (chnl->chan5_0x1.note_0x0 != 0) {
          buf40.uint_0x34 = (uint) chnl->chan5_0x1.note_0x0 << 6;
          return;
        }
        break;
      case 8:
        if (cmdArg != 0) {
          buf40.float_0x30 = 0.0;
          buf40.byte_0x2b = cmdArg;
        }
        chnl->int_0x20 = chnl->floatTableOffset_0x1c;
        return;
      case 0xf:
        chnl->uint_0x14 = (uint) cmdArg << 8;
        return;
      case 0x13:
        if (cmdArg != 0) {
          buf40.byte_0x2c = cmdArg;
        }
        bVar2 = buf40.byte_0x2c;
        if (((bVar2 & 0xf0) == 0xd0) && (bVar2 = bVar2 & 0xf, bVar2 != 0)) {
          buf40.byte_0x2d = bVar2;
        }
    }
  }


  void __thiscall IXS__PlayerCore__initSampleStuff_00407920(PlayerCore *core, Channel *chnl) {
    if (chnl->smplAssocWithHeaderFlag_0x0 != 0) {
      Buf0x40 &buf40 = (chnl->subBuf80).buf40_0x10;
      buf40.volume_0x14 = IXS__PlayerCore__getVolume_00407220(core, chnl);
      buf40.channelPan_0x10 = (uint) IXS__PlayerCore__getChannelPanning_00407280(chnl);

      buf40.uint_0x24 = IXS__PlayerCore__useFloatTab_004072c0(core, chnl);

      ITSample *smpl = (chnl->subBuf80).smplHeadPtr_0x8;

      if (((smpl->flags_0x12 & IT_SUSLOOP) == 0) || (chnl->flag_0x25 != 0)) {
        // "Use sustain loop" || something else
        //
        if ((smpl->flags_0x12 & IT_LOOP) == 0) {
          // Use loop
          //
          buf40.smplLoopStart_0x1c = 0;
          buf40.smplLoopEnd_0x20 = smpl->length_0x30 << 8;
          buf40.smplForwardLoopFlag_0x18 = 0;
        } else {
          buf40.smplLoopStart_0x1c = smpl->loopBegin_0x34 << 8;
          buf40.smplLoopEnd_0x20 = smpl->loopEnd_0x38 << 8;
          // pingpong loop VS forwards loop
          //
          buf40.smplForwardLoopFlag_0x18 = ((smpl->flags_0x12 & IT_PPLOOP) != 0) + 1;   // i.e. 1 or 2
        }
      } else {
        buf40.smplLoopStart_0x1c = smpl->susLoopBegin_0x40 << 8;
        buf40.smplLoopEnd_0x20 = smpl->susLoopEnd_0x44 << 8;
        // pingpong sustain loop
        buf40.smplForwardLoopFlag_0x18 = ((smpl->flags_0x12 & IT_PPSUSLOOP) != 0) + 1;
      }
      if (chnl->flag_0x25 != 0) {
        chnl->flag_0x27 = 1;
      }
    }
  }

#if !defined(EMSCRIPTEN) && !defined(LINUX)
  void __thiscall IXS__PlayerCore__pauseOutput_00407a10(PlayerCore *core) {
    EnterCriticalSection(core->critSect_audioGen_0x4);
    SuspendThread(core->genAudioThreadHandle_0x322c);
    M_pauseOutput_00406280();
  }


  void __thiscall IXS__PlayerCore__resumeOutput_00407a40(PlayerCore *core) {
    unsigned long DVar1;

    M_resumeOutput_004062b0();
    do {
      DVar1 = ResumeThread(core->genAudioThreadHandle_0x322c);
    } while (1 < DVar1);
    LeaveCriticalSection(core->critSect_audioGen_0x4);
  }
#endif

  Module *__thiscall IXS__PlayerCore__createModule_00407a70(PlayerCore *core) {
    Module *mod = core->ptrModule_0x8;
    if (mod != (Module *) nullptr) {
      IXS__Module__dtor_00407bf0(mod);
      operator delete(mod);
    }
    mod = (Module *) malloc(sizeof(Module));
    memset(mod, 0, sizeof(Module));
    if (mod != (Module *) nullptr) {
      mod = IXS__Module__setPlayerCoreObj_00407b80(mod, core);
      core->ptrModule_0x8 = mod;
      return mod;
    }
    core->ptrModule_0x8 = (Module *) nullptr;
    return (Module *) nullptr;
  }


  bool IXS__PlayerCore__isABC_00407af0(Channel *chnl) {

    Chan5 &c = chnl->chan5_0x1;
    Buf0x40 &b = (chnl->subBuf80).buf40_0x10;

    if ((c.cmd_0x3 == 0x13) &&
        (((c.cmdArg_0x4 == 0 && ((b.byte_0x2c & 0xf0) == 0xd0)) ||
          ((c.cmdArg_0x4 & 0xf0) == 0xd0)))) {
      return true;
    }
    return false;
  }


  char *__thiscall IXS__PlayerCore__getSongTitle_00407b20(PlayerCore *core) {
    return (core->ptrModule_0x8->impulseHeader_0x0).songName_0x4;
  }
}