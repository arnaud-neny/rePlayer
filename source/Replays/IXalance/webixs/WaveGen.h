/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#ifndef IXS_WAVEGEN_H
#define IXS_WAVEGEN_H

#include <string>

#include "basetypes.h"

#include "FileSFXI.h"

namespace IXS {

  /**
   * This generator provides the "wave sample" files used by the IXS "Impulse Tracker"
   * during playback.
   *
   * The respective "wave samples" are pre-calculated when a song is first loaded
   * and are then cached in a (per song) temporary file. Respective samples are
   * either based on compressed recorded data or on synthesized data (aka "softsynth").
   */
  typedef struct IXS__WAVEGEN__VFTABLE IXS__WAVEGEN__VFTABLE;
  struct IXS__WAVEGEN__VFTABLE {
    byte __thiscall (*fn1)(struct WaveGen *, struct Local400 *, uint);
  };

  typedef struct WaveGen WaveGen;
  struct WaveGen {
    struct IXS__WAVEGEN__VFTABLE *vftable;
    byte field1_0x4;
    byte field2_0x5;
    byte field3_0x6;
    byte field4_0x7;
    byte field5_0x8;
    byte field6_0x9;
    byte field7_0xa;
    byte field8_0xb;
    byte field9_0xc;
    byte field10_0xd;
    byte field11_0xe;
    byte field12_0xf;
    struct BufInt bytePtr_0x10;
    float *someFloatArray_0x14;
    float *someFloatArray_0x18;
    uint len_0x1c;
    double buf256Ptr_0x20[256 / sizeof(double)];
    double buf256Ptr_0x24[256 / sizeof(double)];
    int int16_0x28[16];
    byte byte16_0x68[16];
    float float16_0x78[16];
    struct BufInt byteInt_0xb8; // some int(?) read from the songfile
    int int_0xbc;
    int int_0xc0;
    struct Local400 *floats100_0xc4;
    int int_0xc8;
    byte someFlag_0xcc;
    byte someFlag_0xcd;
    byte field29_0xce;
    byte field30_0xcf;
    double double_0xd0;
    double double_0xd8;
    double double_0xe0;
    double double_0xe8;
    double double_0xf0;
    double double_0xf8;
    double double_0x100;
    double double_0x108;
    double double_0x110;
    double double_0x118;
    double double_0x120;
    double double_0x128;
    double double_0x130;
    double double_0x138;
    double double_0x140;
    double double_0x148;
    double double_0x150;
    double double_0x158;
    double double_0x160;
    double double_0x168;
    double double_0x170;
    double double_0x178;
    double double_0x180;
    double double_0x188;
    double double_0x190;
    double double_0x198;
    double double_0x1a0;
    double double_0x1a8;
    double double_0x1b0;
    double double_0x1b8;
    double double_0x1c0;
    double double_0x1c8;
    double double_0x1d0;
    double double_0x1d8;
    double double_0x1e0;
    double double_0x1e8;
    double double_0x1f0;
    double double_0x1f8;
    double double_0x200;
    double double_0x208;
    double double_0x210;
    double double_0x218;
    double double_0x220;
    double double_0x228;
    double double_0x230;
    double double_0x238;
    double double_0x240;
    double double_0x248;
    double double_0x250;
    double double_0x258;
    double double_0x260;
    double double_0x268;
    double double_0x270;
    double double_0x278;
    double double_0x280;
    double double_0x288;
    double double_0x290;
    double double_0x298;
    double double_0x2a0;
    double double_0x2a8;
    double double_0x2b0;
    double double_0x2b8;
    double double_0x2c0;
    double double_0x2c8;
    double double_0x2d0;
    double double_0x2d8;
    double double_0x2e0;
    double double_0x2e8;
    double double_0x2f0;
  };


  // "public methods"
  FileSFXI* IXS__WAVEGEN__createSampleCacheFile(char *filename, byte *dataBuf1, byte *dataBuf2, FileSFXI *destSFXI);
}
#endif //IXS_WAVEGEN_H
