/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#ifndef IXS_MIXER2_H
#define IXS_MIXER2_H

#include "MixerBase.h"

namespace IXS {

  /**
   * This impl seems to produce a somewhat lower output quality then Mixer3.
   *
   * There are issues with high-pitched noise but it plays some IT songs
   * better than Mixer2.
   */
  typedef struct Mixer2 Mixer2;
  struct Mixer2 {
    IXS__MixerBase__VFTABLE *vftable;
  };

  void __thiscall IXS__Mixer2__virt4_2_0040a340(MixerBase *mixer, Buf0x40 *buf);

  void __thiscall IXS__Mixer2__virt3_2_0040a590(MixerBase *mixer, Buf0x40 *buf);

  MixerBase *IXS__Mixer2__ctor(byte *audioOutBuffer);
  void IXS__Mixer2__switchTo(MixerBase *mixer);

}
#endif //IXS_MIXER2_H
