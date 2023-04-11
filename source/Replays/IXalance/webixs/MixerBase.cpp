/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/


#include "MixerBase.h"
#include <math.h>
//#include <cstdio>

#include "mem.h"

namespace IXS {

  void __thiscall IXS__MixerBase__initBlockLen_004099a0(MixerBase *mixer, byte tempo);
  void __thiscall IXS__MixerBase__virt34_0_0041554d(MixerBase *mixer, Buf0x40 *param_1);

  static IXS__MixerBase__VFTABLE IXS_MYSTBASE_VFTAB_0042ffe8 = {
          IXS__MixerBase__virt0_00409ac0,
          IXS__MixerBase__clearSampleBuf_00409af0,
          IXS__MixerBase__copyToAudioBuf_0x409b30,
          IXS__MixerBase__virt34_0_0041554d,
          IXS__MixerBase__virt34_0_0041554d
  };

#if defined(EMSCRIPTEN) || defined(LINUX)
  byte *DIRECT_AUDIOBUFFER = nullptr;
  uint DIRECT_AUDIOBUFFER_SIZE = 0;

  void IXS__MixerBase__assertOutputBuffer(MixerBase *mixer) {
    uint len = mixer->blockLen_0x14;
    if (!len) len = 6000; // funtion is also used in context where blockLen hasn't been set yet

    if (DIRECT_AUDIOBUFFER_SIZE < len) {
      if (DIRECT_AUDIOBUFFER != nullptr) {
        free(DIRECT_AUDIOBUFFER);
      }
      DIRECT_AUDIOBUFFER_SIZE = len;
      DIRECT_AUDIOBUFFER = (byte*) malloc(DIRECT_AUDIOBUFFER_SIZE);
      if (!DIRECT_AUDIOBUFFER) {
        fprintf(stderr, "error: mem allocation failed\n");
      }
    }
  }

#endif

  void __fastcall IXS__MixerBase__ctor_004098b0(MixerBase *mixer) {
    mixer->circAudioOutBuffer_0x1c = (byte *) nullptr;
    mixer->outputVolume_0x24 = 1.0f;
    mixer->vftable = &IXS_MYSTBASE_VFTAB_0042ffe8;
  }

  void __thiscall IXS__MixerBase__ctor_004098d0(MixerBase *mixer, uint flags, uint sampleRate) {

    const double DIV12 = 1.0 / 12; //  0.08333333333333333

    mixer->flag_0x4 = flags;

    uint d = (flags & AUDIO_BYTESPERSAMPLE) * (((flags & AUDIO_STEREO) != 0) + 1);
    double v5 = DIV12 * sampleRate;

    mixer->sampleRate_0x8 = sampleRate;
    mixer->audioOutBufPos_0x10 = 0;
    mixer->outBufOverflowCount_0x18 = 0;
    mixer->uint_0x3c = 0;
    mixer->bytesPerSample_0x2c = d;

    uint len = d * v5; // todo: use round?
    mixer->sampleBuf16Length_0xc = len;  // e.g. 14700
    if (mixer->smplBuf16Ptr_0x20 == nullptr)
        mixer->smplBuf16Ptr_0x20 = (short *) malloc(2 * len);

    int bufLen = 0x80000 / mixer->sampleBuf16Length_0xc;
    mixer->arrBuf20Len_0x28 = bufLen;  // e.g. 35

    if (mixer->arrBuf20Ptr_0x38 == nullptr)
        mixer->arrBuf20Ptr_0x38 = (Buf20 *) malloc(20 * bufLen);  // e.g.len 700

    if ((int) mixer->arrBuf20Len_0x28 > 0) {
      int i = 0;
      int j = 0;
      do {
        ++i;
        mixer->arrBuf20Ptr_0x38[j].int_0x0 = 0;
        mixer->arrBuf20Ptr_0x38[j++].totalBytesOut_0x4 = 0;
      } while (i < (signed int) mixer->arrBuf20Len_0x28);
    }
  }


  void __fastcall IXS__MixerBase__dtor_00409970(MixerBase *mixer) {
    mixer->vftable = &IXS_MYSTBASE_VFTAB_0042ffe8;
    operator delete(mixer->arrBuf20Ptr_0x38);
    operator delete((LPVOID) mixer->smplBuf16Ptr_0x20);

//    _GlobalFree(mixer->circAudioOutBuffer_0x1c);  // moved responsibility to PlayerCore
    free(DIRECT_AUDIOBUFFER);
    DIRECT_AUDIOBUFFER = nullptr;
    DIRECT_AUDIOBUFFER_SIZE = 0;
  }


  void __thiscall IXS__MixerBase__initBlockLen_004099a0(MixerBase *mixer, byte tempo) {
    // todo: this might not need actual rounding.. see ftol
    uint r = (uint) ((2.5 / (double) tempo) * ((double) mixer->sampleRate_0x8));
 //   int r = (int) round((2.5 / (double) tempo) * ((double) mixer->sampleRate_0x8));
    mixer->blockLen_0x14 = r * mixer->bytesPerSample_0x2c;

#if defined(EMSCRIPTEN) || defined(LINUX)
    IXS__MixerBase__assertOutputBuffer(mixer);
#endif
  }


  void __thiscall IXS__MixerBase__FUN_004099d0(MixerBase *mixer, uint wavePos) {
    if (mixer->arrBuf20Len_0x28 > 0) {
      for (uint i = 0; i<mixer->arrBuf20Len_0x28; i++) {
        Buf20 *buf20 = mixer->arrBuf20Ptr_0x38 + i;
        if ((buf20->int_0x0 != 0) && (buf20->int_0x8 + buf20->totalBytesOut_0x4 < wavePos)) {
          buf20->int_0x0 = 0;
        }
      }
    }
  }


  uint __thiscall IXS__MixerBase__getOrdAndRowIdx_00409a20(MixerBase *mixer, uint wavePos) {
    uint i;
    uint pos;
    Buf20 *buf20;
    int maxPos;
    uint p;
    Buf20 *arrBuf20;

    maxPos = -1;
    i = 0;
    if (0 < (int) mixer->arrBuf20Len_0x28) {
      arrBuf20 = mixer->arrBuf20Ptr_0x38;
      pos = wavePos;
      buf20 = arrBuf20;
      do {
        if (((buf20->int_0x0 != 0) && (p = buf20->totalBytesOut_0x4, (int) p < (int) wavePos)) &&
            (maxPos < (int) p)) {
          pos = i;
          maxPos = p;
        }
        i = i + 1;
        buf20 = buf20 + 1;
      } while ((int) i < (int) mixer->arrBuf20Len_0x28);
      if (maxPos != 0xffffffff) {
        return arrBuf20[pos].ordIdx_0xc << 8 | arrBuf20[pos].currentRow_0x10;
      }
    }
    return 0;
  }


  int __thiscall IXS__MixerBase__findBufIdx_00409a80(MixerBase *mixer, uint wavePos) {
    int result;
    uint pos;
    int i;
    uint minPos;
    Buf20 *buf20;

    result = -1;
    minPos = 0xffffffff;
    i = 0;
    if (0 < (int) mixer->arrBuf20Len_0x28) {
      buf20 = mixer->arrBuf20Ptr_0x38;
      do {
        pos = buf20->int_0x8 + buf20->totalBytesOut_0x4;
        if (((pos < wavePos) && (pos < minPos)) && (buf20->int_0x0 == 0)) {
          result = i;
          minPos = pos;
        }
        i = i + 1;
        buf20 = buf20 + 1;
      } while (i < (int) mixer->arrBuf20Len_0x28);
    }
    return result;
  }


  void __thiscall IXS__MixerBase__virt0_00409ac0(MixerBase *mixer, Buf0x40 *addr) {
    mixer->count_0x30 = mixer->count_0x30 + 1;
    if ((*(byte *) &mixer->flag_0x4 & AUDIO_STEREO) != 0) {
      (*mixer->vftable->fn4)(mixer, addr);
      return;
    }
    (*mixer->vftable->fn5)(mixer, addr);
  }


  void __thiscall IXS__MixerBase__clearSampleBuf_00409af0(MixerBase *mixer, byte tempo) {
    uint len;
    uint i;
    uint *buf;

    IXS__MixerBase__initBlockLen_004099a0(mixer, tempo);
    len = *(int *) &mixer->sampleBuf16Length_0xc << 1;
    buf = (uint *) mixer->smplBuf16Ptr_0x20;

    memset(buf, 0, len);
    mixer->count_0x30 = 0;
  }


  void __thiscall IXS__MixerBase__copyToAudioBuf_0x409b30(MixerBase *mixer, uint bufIdx) {

    mixer->arrBuf20Ptr_0x38[bufIdx].totalBytesOut_0x4 =
            mixer->outBufOverflowCount_0x18 * 0x80000 + mixer->audioOutBufPos_0x10;
    mixer->arrBuf20Ptr_0x38[bufIdx].int_0x8 = mixer->blockLen_0x14;
    mixer->arrBuf20Ptr_0x38[bufIdx].int_0x0 = 1;

    uint flag = mixer->flag_0x4;
    uint len = mixer->blockLen_0x14;
    if ((flag & AUDIO_16BIT) != 0) {
      len = (int) len >> 1;
    }
    uint outBufPos = mixer->audioOutBufPos_0x10;
#if !defined(EMSCRIPTEN) && !defined(LINUX)
    byte *audioOutBuffer = mixer->circAudioOutBuffer_0x1c;
#else
    // hack: just keep the original audioOutBufPos_0x10 logic going
    // to leave existing code "as-is"
    byte *audioOutBuffer = DIRECT_AUDIOBUFFER;
#endif
    ushort *srcBuf = (ushort *) mixer->smplBuf16Ptr_0x20;
    // aka: uVar1 = (flag & 2) != 0 ? 0x80008000 : 0x80808080;
    //
    ushort uVar1 = (-(ushort) ((flag & AUDIO_16BIT) != 0) & 0xff80) + 0x8080;
    if ((flag & AUDIO_MASK10) != 0) {
      uVar1 = 0;
    }

    if (flag & AUDIO_16BIT) {
      for(int i= 0; i<len; i++) {
        ushort sample = *srcBuf++;
        uint j = outBufPos & 0x7ffff;

#if !defined(EMSCRIPTEN) && !defined(LINUX)
        *(ushort *) (audioOutBuffer + j) = sample ^ uVar1;
#else
        *(ushort *) (audioOutBuffer + (i<<1)) = sample ^ uVar1;
#endif
        outBufPos = j + 2;
      }
    } else {
      for(int i= 0; i<len; i++) {
        ushort sample = *srcBuf++;
        uint j = outBufPos & 0x7ffff;
#if !defined(EMSCRIPTEN) && !defined(LINUX)
        audioOutBuffer[j] = (byte) ((uint) sample >> 8) ^ (byte) uVar1;
#else
        audioOutBuffer[i] = (byte) ((uint) sample >> 8) ^ (byte) uVar1;
#endif
        outBufPos = j + 1;
      }
    }
    if (outBufPos < mixer->audioOutBufPos_0x10) {
      mixer->outBufOverflowCount_0x18 = mixer->outBufOverflowCount_0x18 + 1;
    }
    mixer->audioOutBufPos_0x10 = outBufPos;
  }

  void __thiscall IXS__MixerBase__virt34_0_0041554d(MixerBase *mixer, Buf0x40 *param_1) {
    // irrelevant: used at program exit
    //
    // _amsg_exit_00414c43(0x19);
  }

}