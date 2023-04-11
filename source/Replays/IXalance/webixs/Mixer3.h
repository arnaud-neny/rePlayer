/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#ifndef IXS_MIXER3_H
#define IXS_MIXER3_H

#include "MixerBase.h"

namespace IXS {
  /**
   * This is the default impl used by the original player.
   *
   * It seems to generate the highest quality audio for IXS files - but causes
   * "overflows" in some old IT tunes.
   *
   * The impl heavily relies on x86 MMX based optimizations - which will likely
   * have the opposite effect when emulating them here 1:1.
   * todo: the Emscripten build seems to be causing glitches in this impl
   * todo: replace with some higher-abstraction C logic
   */
  typedef struct Mixer3 Mixer3;
  struct Mixer3 {
    IXS__MixerBase__VFTABLE* vftable;
  };

  void __thiscall IXS__Mixer3__virt4_3_00409c30(MixerBase *mixer, Buf0x40 *addr);
  void __thiscall IXS__Mixer3__virt3_3_00409eb0(MixerBase *mixer, Buf0x40 *addr);

  MixerBase *IXS__Mixer3__ctor(byte *audioOutBuffer);
  void IXS__Mixer3__switchTo(MixerBase *mixer);

}
#endif //IXS_MIXER3_H
