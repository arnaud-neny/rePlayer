/*
* This is the base class for all impls that create the final audio outout.
*
*   The original player contained 4 subclasses - two of which (2nd + 3rd)
*   played using my PC's audio device.
*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#ifndef IXS_MIXERBASE_H
#define IXS_MIXERBASE_H

#include "basetypes.h"
#include "Module.h"

namespace IXS {

#define AUDIO_8BIT 1
#define AUDIO_16BIT 2
#define AUDIO_BYTESPERSAMPLE 3
#define AUDIO_STEREO 8
#define AUDIO_MASK10 0x10

  typedef struct Buf20 Buf20;
  struct Buf20 {
    int int_0x0;
    uint totalBytesOut_0x4;
    int int_0x8;
    int ordIdx_0xc; // high 24 bit
    int currentRow_0x10; // low 8 bit
  };


  typedef struct IXS__MixerBase__VFTABLE IXS__MixerBase__VFTABLE;

  struct IXS__MixerBase__VFTABLE {
    void __thiscall (*genChannelOutput)(struct MixerBase *, struct Buf0x40 *);

    void __thiscall (*clearSampleBuf)(struct MixerBase *, byte);

    void __thiscall (*copyToAudioBuf)(struct MixerBase *, uint);

    void __thiscall (*fn4)(struct MixerBase *, struct Buf0x40 *);

    void __thiscall (*fn5)(struct MixerBase *, struct Buf0x40 *);
  };

  typedef struct MixerBase MixerBase;

  struct MixerBase {
    IXS__MixerBase__VFTABLE *vftable;
    int flag_0x4;
    int sampleRate_0x8;
    uint sampleBuf16Length_0xc;
    uint audioOutBufPos_0x10;      // cycles within 0x7ffff
    uint blockLen_0x14;               // block len in bytes written to the audio buffer (differs between songs..)
    uint outBufOverflowCount_0x18;
    byte *circAudioOutBuffer_0x1c;
    short *smplBuf16Ptr_0x20;
    float outputVolume_0x24;
    uint arrBuf20Len_0x28;
    uint bytesPerSample_0x2c;
    uint count_0x30;
    byte field16_0x34;
    byte field17_0x35;
    byte field18_0x36;
    byte field19_0x37;
    struct Buf20 *arrBuf20Ptr_0x38; // pointer to array
    uint uint_0x3c;
  };

  // "protected methods"
  void __fastcall IXS__MixerBase__ctor_004098b0(MixerBase *mixer);

  void __thiscall IXS__MixerBase__virt0_00409ac0(MixerBase *mixer, Buf0x40 *addr);

  void __thiscall IXS__MixerBase__clearSampleBuf_00409af0(MixerBase *mixer, byte tempo);

  void __thiscall IXS__MixerBase__copyToAudioBuf_0x409b30(MixerBase *mixer, uint bufIdx);


  // "public methods"
  void __thiscall IXS__MixerBase__ctor_004098d0(MixerBase *mixer, uint flags, uint sampleRate);

  void __fastcall IXS__MixerBase__dtor_00409970(MixerBase *mixer);

  void __thiscall IXS__MixerBase__FUN_004099d0(MixerBase *mixer, uint wavePos);

  uint __thiscall IXS__MixerBase__getOrdAndRowIdx_00409a20(MixerBase *mixer, uint wavePos);

  int __thiscall IXS__MixerBase__findBufIdx_00409a80(MixerBase *mixer, uint wavePos);

#if defined(EMSCRIPTEN) || defined(LINUX)
  extern byte *DIRECT_AUDIOBUFFER;

  void IXS__MixerBase__assertOutputBuffer(MixerBase *mixer);
#endif
  }
#endif //IXS_MIXERBASE_H
