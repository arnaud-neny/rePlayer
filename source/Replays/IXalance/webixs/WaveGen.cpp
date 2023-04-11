/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#include "WaveGen.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string>

#include "mem.h"
#include "FileMapSFXI.h"

#ifndef EMSCRIPTEN
#include "File0.h"
// only used for the "progress" feedback
#include "Module.h"
#else
static char MSG_BUFFER[256]; // only used for short messages
extern "C" void JS_printStatus(char *msg); // defined in callback.js
extern "C" void JS_saveCacheFile(char *filename, byte *dataBuf, uint lenBuf);
#endif

namespace IXS {

  /**
   * Copy/paste reuse of "Module" file reading functionality to reduce compile
   * dependencies of WaveGen for use in Web context.
   *
   * todo: add some shared interface to avoid the code duplication.
   */
  typedef struct SkeletonModule SkeletonModule;
  struct SkeletonModule {
    byte *readPosition_0xc0;
  };
  uint __thiscall IXS__SkeletonModule__readInt_0x409350(SkeletonModule *module) {
    uint result;  // make it mem alignment safe
    memcpy(&result, module->readPosition_0xc0, sizeof(uint));
    module->readPosition_0xc0 += sizeof(uint);
    return result;
  }
  byte *__thiscall IXS__SkeletonModule__copyFromInputFile_004092f0(SkeletonModule *module, void *dest, uint len) {
    byte *result;

    memcpy(dest, module->readPosition_0xc0, len);
    result = &module->readPosition_0xc0[len];
    module->readPosition_0xc0 = result;
    return result;
  }
  void __thiscall IXS__SkeletonModule__copyFromInputFile_00409370(SkeletonModule *module, void *dest, uint len) {
    IXS__SkeletonModule__copyFromInputFile_004092f0(module, dest, len);
  }
  byte __thiscall IXS__SkeletonModule__readByte_0x409320(SkeletonModule *module) {
    byte result = *module->readPosition_0xc0;
    module->readPosition_0xc0 += sizeof(byte);
    return result;
  }



  //  todo: the calculations in the impl could use some un-obfuscation

  typedef struct Local400 Local400;
  struct Local400 {
    float floatArray_0x0[100];
  };

  typedef struct Local72 Local72;
  struct Local72 {
    double double_0x0;
    double double_0x8;
    double double_0x10;
    double double_0x18;
    double double_0x20;
    double double_0x28;
    struct BufInt byteInt_0x30;
    int int_0x34;
    uint uint_0x38;
    uint uint_0x3c;
    double double_0x40;
  };

  const double IXS_2PI_004300d0 = 6.2831854820251465;  // different from what is used in PlayerCore... and less precise!

  // logic uses weird negative offset access starting at IXS_wav .. keeping original mem layout just in case:
  struct WaveExt {
    byte IXS_r_wav[8];
    byte IXS_l_wav[8];
    byte IXS_wav[8];
  } waveExt = {
          {0x72, 0x2e, 0x77, 0x61, 0x76, 0x00, 0x00, 0x00},  // "r.wav"
          {0x6c, 0x2e, 0x77, 0x61, 0x76, 0x00, 0x00, 0x00},  // "l.wav"
          {0x2e, 0x77, 0x61, 0x76, 0x00, 0x00, 0x00, 0x00}    // ".wav"
  };

  const char *IXS_s_r_wav_00438674 = (const char *) waveExt.IXS_r_wav;
  const char *IXS_s_l_wav_0043867c = (const char *) waveExt.IXS_l_wav;
  const char *IXS_s_wav_00438684 = (const char *) waveExt.IXS_wav;



  // "private methods"
  double __cdecl IXS__WAVEGEN__FUN_0040cec0(float param_1, float param_2, float param_3, float param_4, float param_5);
  void __cdecl IXS__WAVEGEN__writeSynthSub1_0040cfc0(short *tab1, short *outBuffer, int len, int outLen,
                                                     float param_5, float *blocksIn, int numBlocks, int param_8);
  void __cdecl IXS__WAVEGEN__writeSynthSub2_0040d270(short *tab1, short *outBuffer, int dataLen, int step,
                                                     int param_5, int n, float *param_7, uint param_8);
  uint __cdecl IXS__WAVEGEN__FUN_0040d6e0(byte *buf, uint *bytesUsed);
  uint __cdecl IXS__WAVEGEN__writeSynthSample_0040d740(byte *buf200, byte *someBuf, FileSFXI *tmpFile);
  void IXS__WAVEGEN__writeSynthSamples_0040dcb0(byte *srcBuf, FileSFXI *tmpFile);
  void IXS__WAVEGEN__updateProgress_00407b50();
  void __cdecl IXS__WAVEGEN__FUN_0040d540(short *ioBuf, int len, uint flags);
  byte __thiscall IXS__WAVEGEN__FUN_0x40adb0(WaveGen *obj, Local400 *floats100, uint numFloats100);

  int __thiscall IXS__WAVEGEN__writeRecordedSamples_0x408020(SkeletonModule *module, byte *byteArrayBuf, FileSFXI *tmpFile);

  double __cdecl IXS__WAVEGEN__FUN_0040acf0(double in);

  double __cdecl IXS__WAVEGEN__FUN_0040ad30(double a, double b);

  double __thiscall IXS__WAVEGEN__FUN_0040bd20(WaveGen *obj, double param_1, double param_2, double param_3, char notNull);

  double __thiscall IXS__WAVEGEN__FUN_0040bd70(WaveGen *obj, double param_1, double param_2, double param_3, char param_4);

  void __thiscall IXS__WAVEGEN__FUN_0040bdc0(WaveGen *obj, uint idx);

  void __thiscall IXS__WAVEGEN__fmod_0040be20(double *param_1, double param_2);

  void __thiscall IXS__WAVEGEN__FUN_0040be40(WaveGen *obj);

  void __thiscall IXS__WAVEGEN__FUN_0040bf50(WaveGen *obj, int min);

  void __thiscall IXS__WAVEGEN__FUN_0040bfd0(WaveGen *obj, const float *floats100);

  void __thiscall IXS__WAVEGEN__FUN_0040c380(WaveGen *obj, Local72 *loc72);

// rePlayer
#if 1//!defined(EMSCRIPTEN) && !defined(LINUX)
  const double M_PI = 3.14159265358979323846;
  const double M_1_PI = 0.318309886183790671538;
#else
  // defined in EMSCRIPTEN's math.h
#endif
  const double PI2 = M_PI * 2.0;

  const double IXS_DOUBLE_00430070 = 0.00014247585714285716;

  typedef struct Local7200 Local7200;
  struct Local7200 {
    int intArray_0x0[200];
    int intArray_0x320[200];
    double doubleArray_0x640[200];
    int intArray_0xc80[200];
    double doubleArray_0xfa0[200];
    double doubleArray_0x15e0[200];
  };


  WaveGen *IXS_WAVEGEN_PTR = nullptr;

  static IXS__WAVEGEN__VFTABLE IXS_WAVEGEN_VFTAB_0042ffc8 = {
          IXS__WAVEGEN__FUN_0x40adb0
  };

  WaveGen* IXS__WAVEGEN__getInstance() {
    if (IXS_WAVEGEN_PTR == nullptr) {
      IXS_WAVEGEN_PTR = new WaveGen();
      IXS_WAVEGEN_PTR->vftable = &IXS_WAVEGEN_VFTAB_0042ffc8;
    }
    return IXS_WAVEGEN_PTR;
  }


  double __cdecl IXS__WAVEGEN__FUN_0040acf0(double in) {
    double m = fmod(in, PI2);
    if (m < 0.0) {
      m += PI2;
    }
    double result = m * M_1_PI;
    if (result >= 1.0) {
      return result - 2.0;
    }
    return result;
  }


  double __cdecl IXS__WAVEGEN__FUN_0040ad30(double a, double b) {
    double m;
    double n;

    if (b < -0.99) {
      b = -0.99;
    }
    m = fmod(a, PI2);
    if (m < 0.0) {
      m += PI2;
    }
    n = b + 1.0;

    if (m <= n * 0.5 * M_PI) {
      return -1.0;
    }
    return 1.0;
  }

  inline float clamp(float f1) {
    if (f1 <= 1.0f) {
      if (f1 < -1.0f) {
        f1 = -1.0f;
      } else {
        // crop values to -1..1 range
      }
    } else {
      f1 = 1.0;
    }
    return f1;
  }


// todo: yet another monolith that should better be chopped up
  byte __thiscall IXS__WAVEGEN__FUN_0x40adb0(WaveGen *obj, Local400 *floats100, uint numFloats100) {
    Local72 local_48; // used original stack to prevent Ghidra from making a total mess.. todo: cleanup now that it works

    obj->floats100_0xc4 = floats100;
    obj->len_0x1c = obj->int_0xbc * *(uint *) &obj->byteInt_0xb8;

    obj->someFloatArray_0x14 = (float *) malloc(4 * obj->len_0x1c);

    obj->someFloatArray_0x18 = (float *) malloc(4 * obj->len_0x1c);

    for (int i = 0; i < (int) obj->len_0x1c; i++) {
      obj->someFloatArray_0x14[i] = 0.0f;
      obj->someFloatArray_0x18[i] = 0.0f;
    }
    obj->double_0x1c0 = 0.0;
    obj->double_0x1c8 = 0.0;
    obj->double_0x1e0 = 0.0;
    obj->double_0x1e8 = 0.0;
    obj->double_0x1d0 = 0.0;
    obj->double_0x1d8 = 0.0;
    obj->double_0x2b8 = 0.0;
    obj->double_0x2c0 = 0.0;
    obj->double_0x2c8 = 0.0;
    obj->double_0x2d0 = 0.0;


    /*
    const float DIV12 = 1.0f / 12;  // 0.083333336f
    obj->double_0x2e8 = pow(2.0, obj->buf256Ptr_0x24[*(uint *) &obj->byteInt_0xb8] * DIV12) * 110.0f;

    const double LOGE2 = 0.69314718055994530941723212145818;
    const double LOG2E = 1.4426950408889634073599246810019;
    double d1 = fyl2x(23.4, LOGE2) * floats100->floatArray_0x0[88] * LOG2E;
    double r1 = round(d1);
//  __asm { FRNDINT }
    const double IXS_DOUBLE_00430080 = 0.0000144404;
    double d2 =
            fscale(f2xm1(d1 - r1) + 1.0, r1) * ((double) (*(int *) &obj->byteInt_0xb8) * IXS_DOUBLE_00430080);
    */
    /*
     * this code gets called 1x per generated regular sample ("Movements of the gods" is a good testcase)
     * tested: the above convoluted impl produces the correct result
     *
     * example inputs / result (how do the constants k1=23.4, k2=0.0000144404, k3=110.0 and k4=1.0/12 fit in?):
     *
     *  f=floatArray_0x0 (-1 to 1?);    n= byteInt_0xb8(1 to 16?); result
     *
        0.0000000000000000000000000     16                         0.0002310463999999999974797    (n*k2)
        0.0000000000000000000000000     1                          0.0000144403999999999998425    (n*k2)
        -0.4881889820098876953125000    16                         0.0000495750442428492512021
        -0.6377952694892883300781250    15                         0.0000289995384401302927501
        1.0000000000000000000000000     1                          0.0003379053599999999688702    (pow(k1,f) * (n*k2))
     */

    // it seems the above can be simplified to:
    double d2 = pow(23.4, floats100->floatArray_0x0[88]) * ((double) (*(int *) &obj->byteInt_0xb8) * 0.0000144404);

    float *fPtr50 = (float *) &floats100->floatArray_0x0[50];  // middle of the 100 float's buffer
    int i2 = 0;
    do {
      // why use the 32 floatArray_0x0 entries "to the right" of the center?
      obj->buf256Ptr_0x24[i2] = *fPtr50 * d2 + 1.0;
      obj->buf256Ptr_0x20[i2] = obj->buf256Ptr_0x24[i2];

      fPtr50 += 1;
      i2 += 1;
    } while (i2 < 32);

    obj->someFlag_0xcc = floats100->floatArray_0x0[36] < 0.0f;
    obj->someFlag_0xcd = floats100->floatArray_0x0[85] < 0.0f;

    obj->int_0xc8 = 0;
    if (*(uint *) &obj->byteInt_0xb8 > 0) {

      do {
        IXS__WAVEGEN__FUN_0040bfd0(obj, floats100->floatArray_0x0);

        obj->double_0x2a8 = (double) (floats100->floatArray_0x0[21] + 1.0f) * 256.0f;

        if (floats100->floatArray_0x0[22] != 0.0f) {
          int q1 = obj->int16_0x28[obj->int_0xc8];
          uint q2 = (int) (((uint64) ((uint64)0x77777777 * q1) >> 32) - q1) >> 3;
          obj->double_0x2a8 = pow(2.0, (double) (int) ((q2 >> 31) + q2)) * obj->double_0x2a8;
        }
        const float DIV12 = 1.0f / 12;  // 0.083333336f
        obj->double_0x2e0 = pow(2.0, (double) obj->int16_0x28[obj->int_0xc8] * DIV12) * 110.0f;

        obj->double_0x2f0 = obj->double_0x2e8;
        obj->double_0x1f0 = obj->double_0x118;
        obj->double_0x220 = obj->double_0x120;
        obj->double_0x250 = obj->double_0x128;
        obj->double_0x1f8 = obj->double_0x130;
        obj->double_0x228 = obj->double_0x138;
        obj->double_0x258 = obj->double_0x140;
        obj->double_0x200 = obj->double_0x148;
        obj->double_0x230 = obj->double_0x150;
        obj->double_0x260 = obj->double_0x158;
        obj->double_0x208 = obj->double_0x160;
        obj->double_0x238 = obj->double_0x168;
        obj->double_0x268 = obj->double_0x170;
        obj->double_0x210 = obj->double_0x178;
        obj->double_0x240 = obj->double_0x180;
        obj->double_0x270 = obj->double_0x188;
        obj->double_0x218 = obj->double_0x190;
        obj->double_0x248 = obj->double_0x198;
        obj->double_0x278 = obj->double_0x1a0;
        obj->double_0x280 = obj->double_0x290;


        int offsetA = obj->int_0xbc * obj->int_0xc8;
        int offsetInA = offsetA;

        if (obj->int_0xbc > 0) {
          int i = 0;

          do {
            for (int j = 0; j < 32; ++j) {
              double *dp1 = &obj->buf256Ptr_0x20[j];
              *dp1 = obj->buf256Ptr_0x24[j] * *dp1;
            }
            obj->double_0x118 = obj->buf256Ptr_0x20[0] * obj->double_0x1f0;
            obj->double_0x120 = obj->buf256Ptr_0x20[1] * obj->double_0x220;
            obj->double_0x128 = obj->buf256Ptr_0x20[2] * obj->double_0x250;
            obj->double_0x130 = obj->buf256Ptr_0x20[3] * obj->double_0x1f8;
            obj->double_0x138 = obj->buf256Ptr_0x20[4] * obj->double_0x228;
            obj->double_0x140 = obj->buf256Ptr_0x20[5] * obj->double_0x258;
            obj->double_0x148 = obj->buf256Ptr_0x20[6] * obj->double_0x200;
            obj->double_0x150 = obj->buf256Ptr_0x20[7] * obj->double_0x230;
            obj->double_0x158 = obj->buf256Ptr_0x20[8] * obj->double_0x260;
            obj->double_0x160 = obj->buf256Ptr_0x20[9] * obj->double_0x208;
            obj->double_0x168 = obj->buf256Ptr_0x20[10] * obj->double_0x238;
            obj->double_0x170 = obj->buf256Ptr_0x20[11] * obj->double_0x268;

            double *dp = obj->buf256Ptr_0x20;
            obj->double_0x178 = dp[12] * obj->double_0x210;
            obj->double_0x180 = dp[13] * obj->double_0x240;
            obj->double_0x188 = dp[14] * obj->double_0x270;
            obj->double_0x190 = dp[15] * obj->double_0x218;
            obj->double_0x198 = dp[16] * obj->double_0x248;
            obj->double_0x1a0 = dp[17] * obj->double_0x278;
            obj->double_0x290 = dp[27] * obj->double_0x280;
            obj->double_0x1a8 = dp[18] * obj->double_0x1a8;
            obj->double_0x1b0 = dp[19] * obj->double_0x1b0;
            obj->double_0x1b8 = dp[20] * obj->double_0x1b8;

            if (obj->byte16_0x68[obj->int_0xc8]) {
              IXS__WAVEGEN__FUN_0040be40(obj);
            } else {
              obj->double_0x2f0 = obj->double_0x2e0;
            }
            obj->double_0x1a0 = obj->double_0x2f0 * obj->double_0x198 * IXS_DOUBLE_00430070;

            IXS__WAVEGEN__fmod_0040be20(&obj->double_0x1e8, obj->double_0x1a0);
            obj->double_0xf8 = sin(obj->double_0x1a0 + obj->double_0x110 + obj->double_0x1e8)
                               * obj->double_0x2a8 * obj->double_0x190;
            obj->double_0x188 = obj->double_0x2f0 * obj->double_0x180 * IXS_DOUBLE_00430070;

            IXS__WAVEGEN__fmod_0040be20(&obj->double_0x1e0, obj->double_0x188);
            obj->double_0xf0 = sin(obj->double_0x188 * obj->double_0xf8 + obj->double_0x1e0)
                               * obj->double_0x2a8 * obj->double_0x178;
            obj->double_0x170 = obj->double_0x168 * obj->double_0x2f0 * IXS_DOUBLE_00430070;

            IXS__WAVEGEN__fmod_0040be20(&obj->double_0x1d8, obj->double_0x170);
            obj->double_0xe8 = sin((obj->double_0x108 + obj->double_0xf0) * obj->double_0x170 + obj->double_0x1d8)
                               * obj->double_0x160 * obj->double_0x2a8;
            obj->double_0x158 = obj->double_0x2f0 * obj->double_0x150 * IXS_DOUBLE_00430070;

            IXS__WAVEGEN__fmod_0040be20(&obj->double_0x1d0, obj->double_0x158);
            obj->double_0xe0 = sin(obj->double_0x158 * obj->double_0xe8 + obj->double_0x1d0) * obj->double_0x148;
            obj->double_0x140 = obj->double_0x2f0 * obj->double_0x138 * IXS_DOUBLE_00430070;

            IXS__WAVEGEN__fmod_0040be20(&obj->double_0x1c8, obj->double_0x140);
            obj->double_0xd8 = sin(obj->double_0x100 * obj->double_0x140 + obj->double_0x1c8)
                               * obj->double_0x2a8 * obj->double_0x130;
            obj->double_0x128 = obj->double_0x2f0 * obj->double_0x120 * IXS_DOUBLE_00430070;

            IXS__WAVEGEN__fmod_0040be20(&obj->double_0x1c0, obj->double_0x128);
            double ddd = obj->double_0x128 * obj->double_0xd8 + obj->double_0x1c0;
            double d3;
            if (floats100->floatArray_0x0[91] >= -0.3) {
              if (floats100->floatArray_0x0[91] <= 0.3)
                d3 = sin(ddd);
              else
                d3 = IXS__WAVEGEN__FUN_0040ad30(ddd, floats100->floatArray_0x0[90]);
            } else {
              d3 = IXS__WAVEGEN__FUN_0040acf0(ddd);
            }

            double d4 = obj->double_0x118 * d3;
            obj->double_0xd0 = obj->double_0x118 * d3;

            obj->double_0x100 = d4 * obj->double_0x1a8 * obj->double_0x2a8;
            obj->double_0x108 = obj->double_0xe0 * obj->double_0x2a8 * obj->double_0x1b0;
            obj->double_0x110 = obj->double_0xe0 * obj->double_0x2a8 * obj->double_0x1b8;
            obj->double_0x2b0 = obj->float16_0x78[obj->int_0xc8] * (d4 + obj->double_0xe0);

            obj->someFloatArray_0x14[offsetA] = (float)
                    (IXS__WAVEGEN__FUN_0040bd20(obj, obj->double_0x2b0, obj->double_0x290,
                                                obj->double_0x298, (char) obj->someFlag_0xcc) * 0.5);

            IXS__WAVEGEN__FUN_0040bdc0(obj, i + offsetInA);

            obj->double_0x1f0 = obj->double_0x1f0 * obj->double_0x250;
            obj->double_0x1f8 = obj->double_0x258 * obj->double_0x1f8;
            obj->double_0x200 = obj->double_0x260 * obj->double_0x200;
            obj->double_0x208 = obj->double_0x208 * obj->double_0x268;
            obj->double_0x210 = obj->double_0x210 * obj->double_0x270;
            obj->double_0x218 = obj->double_0x278 * obj->double_0x218;
            obj->double_0x280 = (1.0 - obj->double_0x288 + 1.0) * obj->double_0x280;

            ++i;
            offsetA += 1;
          } while (i < obj->int_0xbc);
        }

        if (floats100->floatArray_0x0[86] >= 0.0f) {
          obj->double_0x2e8 = obj->double_0x2e0;
        } else {
          obj->double_0x2e8 = obj->double_0x2f0;
        }
        obj->int_0xc8 += 1;
      } while (obj->int_0xc8 < *(uint *) &obj->byteInt_0xb8);
    }


    if (floats100->floatArray_0x0[30] > (double) 0.0f) {
      IXS__WAVEGEN__FUN_0040bf50(obj, 1000);

      float *newFloatArray_0x14 = (float *) malloc(4 * obj->len_0x1c);
      int ll = (int) obj->len_0x1c;
      float *floatArrPtr;
      int n5 = 0;
      if (ll > 0) {
        floatArrPtr = newFloatArray_0x14;
        do {
          int idx2 = ll - n5++;
          *floatArrPtr++ = obj->someFloatArray_0x14[idx2 - 1];
          ll = (int) obj->len_0x1c;
        } while (n5 < ll);
      }

      free (obj->someFloatArray_0x14);
      obj->someFloatArray_0x14 = newFloatArray_0x14;
    }
    IXS__WAVEGEN__FUN_0040bf50(obj, 1000);

    local_48.double_0x0 = (floats100->floatArray_0x0[92] - 1.0) * -0.5;
    local_48.double_0x8 = (floats100->floatArray_0x0[93] - 1.0) * -0.5;

    uint t1 = (uint) ((floats100->floatArray_0x0[94] - 1.0) * 0.5 * -16.0);
    if (floats100->floatArray_0x0[87] < 0.0f) {
      t1 *= (uint) (floats100->floatArray_0x0[87] * -8.0);
    }
    local_48.uint_0x38 = t1;
    local_48.double_0x10 = (floats100->floatArray_0x0[95] - 1.0) * -0.5;
    local_48.double_0x18 = (floats100->floatArray_0x0[96] - 1.0) * -0.5;
    local_48.double_0x20 = -((floats100->floatArray_0x0[97] - 1.0) * 0.5 * 512.0 + 1.0);
    local_48.double_0x28 = (floats100->floatArray_0x0[98] - 1.0) * -0.5;

    local_48.int_0x34 = (int) -((floats100->floatArray_0x0[99] - 1.0) * 0.5 * 100.0);
    local_48.byteInt_0x30.b1 = floats100->floatArray_0x0[31] > 0.0;


    if (floats100->floatArray_0x0[33] >= 0.0f) {
      local_48.double_0x40 = -1.0;
    } else {
      local_48.double_0x40 = 0.5f - floats100->floatArray_0x0[33] * 0.5f;
    }


    if (floats100->floatArray_0x0[84] < 0.0f) {
      int n0 = (int) (-floats100->floatArray_0x0[84] * (double) (int) obj->len_0x1c);
      const float ff = (float) n0;

      for (int j = 0; j < n0; j++) {
        double d4 = (double) j / ff;
        d4 *= d4;

        float *fp = &obj->someFloatArray_0x14[j];
        *fp *= (float) d4;
      }
    }


    if (obj->bytePtr_0x10.b1) {
      IXS__WAVEGEN__FUN_0040c380(obj, &local_48);
      if (floats100->floatArray_0x0[31] <= 0.0f
          && local_48.double_0x40 < 0.0) {
        IXS__WAVEGEN__FUN_0040bf50(obj, 1000);
      }
      if (floats100->floatArray_0x0[23] < 0.0f) {
        IXS__WAVEGEN__FUN_0040bf50(obj, (int) obj->len_0x1c / 4);
      }
      if (floats100->floatArray_0x0[23] > 0.0f) {
        IXS__WAVEGEN__FUN_0040bf50(obj, (int) obj->len_0x1c);
      }
    } else {
      if (floats100->floatArray_0x0[23] < 0.0f) {
        IXS__WAVEGEN__FUN_0040bf50(obj, (int) obj->len_0x1c / 4);
      }
      if (floats100->floatArray_0x0[23] > 0.0f) {
        IXS__WAVEGEN__FUN_0040bf50(obj, (int) obj->len_0x1c);
      }
      for (int k = 0; k < (int) obj->len_0x1c; ++k) {
        obj->someFloatArray_0x18[k] = obj->someFloatArray_0x14[k];
      }
    }

    if (floats100->floatArray_0x0[34] < 0.0f) {
      int idx = 0;
      for (float f3 = floats100->floatArray_0x0[34] * -100.0f; idx < (int) obj->len_0x1c; ++idx) {
        obj->someFloatArray_0x14[idx] = clamp(f3 * obj->someFloatArray_0x14[idx]);;
        obj->someFloatArray_0x18[idx] = clamp(f3 * obj->someFloatArray_0x18[idx]);
      }
    }


    if (floats100->floatArray_0x0[83] < 0.0f) {
      int oldLen = obj->len_0x1c;
      float *oldFloatArray_0x14 = obj->someFloatArray_0x14;
      float *oldFloatArray_0x18 = obj->someFloatArray_0x18;

      int le = (int) (-floats100->floatArray_0x0[83] * (double) obj->len_0x1c);
      obj->int_0xc0 = le;
      obj->len_0x1c += le;

      obj->someFloatArray_0x14 = (float *) malloc(4 * obj->len_0x1c);
      obj->someFloatArray_0x18 = (float *) malloc(4 * obj->len_0x1c);


      if ((int) obj->len_0x1c > 0) {
        float f7 = (float) obj->int_0xc0;
        int i1 = 0;
        int n = 0;
        do {
          if (n <= obj->int_0xc0) {
            double d3 = (double) i1 / f7;
            d3 *= d3;

            obj->someFloatArray_0x14[n] = (float) (d3 * oldFloatArray_0x14[n - 1 + (oldLen - obj->int_0xc0)]);
            obj->someFloatArray_0x18[n] = (float) (d3 * oldFloatArray_0x18[n - 1 + (oldLen - obj->int_0xc0)]);
          } else {
            obj->someFloatArray_0x14[n] = oldFloatArray_0x14[n - obj->int_0xc0];
            obj->someFloatArray_0x18[n] = oldFloatArray_0x18[n - obj->int_0xc0];
          }
          i1 = ++n;
        } while (n < (int) obj->len_0x1c);
      }

      free (oldFloatArray_0x14);
      free (oldFloatArray_0x18);
    }
    return 1;
  }


  double __thiscall IXS__WAVEGEN__FUN_0040bd20(WaveGen *obj, double param_1, double param_2, double param_3, char notNull) {
    double v5 = param_2;
    if (param_2 < 1.0) {
      v5 = 1.0;
    }
    double v6 = (obj->double_0x2c0 - (param_1 + obj->double_0x2d0) / v5) * param_3;
    obj->double_0x2c0 = v6;

    double result = v6 + obj->double_0x2d0;
    obj->double_0x2d0 = result;
    if (notNull)
      return result + param_1;
    return result;
  }


  double __thiscall IXS__WAVEGEN__FUN_0040bd70(WaveGen *obj, double param_1, double param_2,
                                               double param_3, char param_4) {

    double ret =  (param_2 < 1.0) ? 1.0 : param_2;

    ret = (obj->double_0x2b8 - (param_1 + obj->double_0x2c8) / ret) * param_3;
    obj->double_0x2b8 = ret;
    ret += obj->double_0x2c8;
    obj->double_0x2c8 = ret;

    if (param_4 != '\0') {
      ret += param_1;
    }
    return ret;
  }


  void __thiscall IXS__WAVEGEN__FUN_0040bdc0(WaveGen *obj, uint idx) {

    obj->someFloatArray_0x14[idx] = (float) (obj->double_0x2d8 * 8.0 + 1.0)
                                    * obj->someFloatArray_0x14[idx];

    if (obj->someFloatArray_0x14[idx] > 1.0f) {
      obj->someFloatArray_0x14[idx] = 1.0f;
    }
    if (obj->someFloatArray_0x14[idx] < -1.0f)
      obj->someFloatArray_0x14[idx] = -1.0f;
  }


  void __thiscall IXS__WAVEGEN__fmod_0040be20(double *param_1, double param_2) {
    *param_1 = fmod(param_2 + *param_1, PI2);
  }


  void __thiscall IXS__WAVEGEN__FUN_0040be40(WaveGen *obj) {
    double d;

    if (((obj->double_0x2e8 < obj->double_0x2e0) && (obj->double_0x2f0 < obj->double_0x2e0)) &&
        (d = (obj->double_0x2a0 + 1.0) * obj->double_0x2f0,
                obj->double_0x2f0 = d, obj->double_0x2e0 <= d)) {

      obj->double_0x2f0 = obj->double_0x2e0;
    }
    if (((ushort) ((ushort) (obj->double_0x2e8 < obj->double_0x2e0) << 8 |
                   (ushort) (obj->double_0x2e8 == obj->double_0x2e0) << 0xe) == 0) &&
        ((ushort) ((ushort) (obj->double_0x2f0 < obj->double_0x2e0) << 8 |
                   (ushort) (obj->double_0x2f0 == obj->double_0x2e0) << 0xe) == 0)) {
      d = (1.0 - obj->double_0x2a0) * obj->double_0x2f0;
      obj->double_0x2f0 = d;
      if ((ushort) ((ushort) (d < obj->double_0x2e0) << 8 |
                    (ushort) (d == obj->double_0x2e0) << 0xe) != 0) {

        obj->double_0x2f0 = obj->double_0x2e0;
      }
    }
    if ((obj->double_0x2e8 < obj->double_0x2e0) &&
        ((ushort) ((ushort) (obj->double_0x2f0 < obj->double_0x2e0) << 8 |
                   (ushort) (obj->double_0x2f0 == obj->double_0x2e0) << 0xe) == 0)) {

      obj->double_0x2f0 = obj->double_0x2e0;
    }
  }


  void __thiscall IXS__WAVEGEN__FUN_0040bf50(WaveGen *obj, int min) {
    int len = (int) obj->len_0x1c;
    if (min > len) {
      min = len;
    }

    int idx = len - min;
    for (float f1 = (float) (1.0f / (double) min); idx < len; len = (int) obj->len_0x1c) {
      obj->someFloatArray_0x14[idx] = (float) ((double) (len - idx) * f1 * obj->someFloatArray_0x14[idx]);

      int i1 = (int) obj->len_0x1c - idx;
      float *fp = &obj->someFloatArray_0x18[idx++];
      *fp = (float) ((double) i1 * f1 * *fp);
    }
  }


  void __thiscall IXS__WAVEGEN__FUN_0040bfd0(WaveGen *obj, const float *floats100) {
    const float DIV256 = 1.0f / 256; // 0.00390625;
    float f = (floats100[0x20] + 1.0f) * DIV256;
    int i = obj->int_0xc8;
    obj->double_0x2a0 = (double) f;

    obj->double_0xd0 = 0.0;
    obj->double_0xd8 = 0.0;
    obj->double_0xe0 = 0.0;
    obj->double_0xe8 = 0.0;
    obj->double_0xf0 = 0.0;
    obj->double_0xf8 = 0.0;
    obj->double_0x100 = 0.0;
    obj->double_0x108 = 0.0;
    obj->double_0x110 = 0.0;

    if (obj->byte16_0x68[i] == 0) {
      obj->double_0x118 = (double) ((floats100[0] - 1.0f) * 0.5f);
      obj->double_0x130 = (double) ((floats100[3] - 1.0f) * 0.5f);
      obj->double_0x148 = (double) ((floats100[6] - 1.0f) * 0.5f);
      obj->double_0x160 = (double) ((floats100[9] - 1.0f) * 0.5f);
      obj->double_0x178 = (double) ((floats100[0xc] - 1.0f) * 0.5f);
      obj->double_0x190 = (double) ((floats100[0xf] - 1.0f) * 0.5f);
    }
    obj->double_0x120 = (double) ((floats100[1] - 1.0f) * 0.5f);
    obj->double_0x128 = (double) ((floats100[2] + 1.0f) * 0.5f);

    obj->double_0x138 = (double) ((floats100[4] - 1.0f) * 0.5f);
    obj->double_0x140 = (double) ((floats100[5] + 1.0f) * 0.5f);

    obj->double_0x150 = (double) ((floats100[7] - 1.0f) * 0.5f);
    obj->double_0x158 = (double) ((floats100[8] + 1.0f) * 0.5f);

    obj->double_0x168 = (double) ((floats100[10] - 1.0f) * 0.5f);
    obj->double_0x170 = (double) ((floats100[0xb] + 1.0f) * 0.5f);

    obj->double_0x180 = (double) ((floats100[0xd] - 1.0f) * 0.5f);
    obj->double_0x188 = (double) ((floats100[0xe] + 1.0f) * 0.5f);

    obj->double_0x198 = (double) ((floats100[0x10] - 1.0f) * 0.5f);
    obj->double_0x1a0 = (double) ((floats100[0x11] + 1.0f) * 0.5f);

    obj->double_0x1a8 = (double) ((floats100[0x12] - 1.0f) * 0.5f);
    obj->double_0x1b0 = (double) ((floats100[0x13] - 1.0f) * 0.5f);
    obj->double_0x1b8 = (double) ((floats100[0x14] - 1.0f) * 0.5f);

    obj->double_0x288 = (double) ((floats100[0x1a] + 1.0f) * 0.5f);

    if (obj->byte16_0x68[i] == 0) {
      f = (floats100[0x1b] + 1.0f) * 0.5f;
      obj->double_0x290 = (double) (f * f * 512.0f + 1.0f);
    }
    obj->double_0x298 = (double) ((floats100[0x1c] + 1.0f) * 0.5f);
    obj->double_0x2d8 = (double) ((floats100[0x19] + 1.0f) * 0.5f);

    // 9 times sqrt = pow(x, (1/pow(2, 9)))
    obj->double_0x128 = pow(obj->double_0x128, (1 / pow(2, 9)));
    obj->double_0x140 = pow(obj->double_0x140, (1 / pow(2, 9)));
    obj->double_0x158 = pow(obj->double_0x158, (1 / pow(2, 9)));
    obj->double_0x170 = pow(obj->double_0x170, (1 / pow(2, 9)));
    obj->double_0x188 = pow(obj->double_0x188, (1 / pow(2, 9)));
    obj->double_0x1a0 = pow(obj->double_0x1a0, (1 / pow(2, 9)));
    obj->double_0x288 = pow(obj->double_0x288, (1 / pow(2, 9)));

//    obj->double_0x128 = sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(obj->double_0x128)))))))));
//    obj->double_0x140 = sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(obj->double_0x140)))))))));
//    obj->double_0x158 = sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(obj->double_0x158)))))))));
//    obj->double_0x170 = sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(obj->double_0x170)))))))));
//    obj->double_0x188 = sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(obj->double_0x188)))))))));
//    obj->double_0x1a0 = sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(obj->double_0x1a0)))))))));
//    obj->double_0x288 = sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(obj->double_0x288)))))))));
  }


  // use the same algorithm used by the original (no point taking the risk of
  // having different impls on different platforms..)
  static uint random;
  void _srand(uint seed) {
    random = seed;
  }
  uint _rand() {
    uint val = random * 0x343fd + 0x269ec3;
    random = val;
    return val >> 0x10 & 0x7fff;
  }


  // todo: break down this monolith into more manageable chucks
  void __thiscall IXS__WAVEGEN__FUN_0040c380(WaveGen *obj, Local72 *loc72) {
    Local7200 local7200;

    uint oLen = obj->len_0x1c;
    obj->len_0x1c = oLen * loc72->uint_0x38;

    float *origFloatArray14 = obj->someFloatArray_0x14;
    obj->someFloatArray_0x14 = (float *) malloc(4 * obj->len_0x1c);

    free (obj->someFloatArray_0x18);
    obj->someFloatArray_0x18 = (float *) malloc(4 * obj->len_0x1c);

    if (obj->len_0x1c > 0) {
      int i2 = 0;
      do {
        obj->someFloatArray_0x14[i2] = 0.0f;
        obj->someFloatArray_0x18[i2] = 0.0f;
        i2++;
      } while (i2 < (int) obj->len_0x1c);
    }

    char *buffer18000 = (char *) malloc(0x60000);
    memset(buffer18000, 0, 0x60000);

    _srand(0xdead - (int) (obj->floats100_0xc4->floatArray_0x0[89] * -100.0f));

    int dj = 0;
    int djj = 0;

    int di = 0;
    int dii = 0;
    do {
      int i1 = 0;
      if (loc72->int_0x34 > 0) {
        int ii = dj;

        const float X1 = 1.0 / 0x7fff; // 0.00003051851;
        const float X18000 = (float) 0x18000;
        do {

          local7200.intArray_0x320[ii] = (int) (X1 * X18000 * (float) _rand());
          local7200.intArray_0x0[ii] = (int) (X1 * X18000 * (float) _rand());

          local7200.doubleArray_0xfa0[di] = loc72->double_0x18;
          local7200.doubleArray_0x15e0[di] = loc72->double_0x10;

          int s = 0x18000 / (2 * loc72->int_0x34);
          float f3 = (float) (0x18000 - i1 * s);
          local7200.intArray_0xc80[ii] = (int) (f3 - X1 * (float) _rand() * (float) (s >> 1));
          local7200.doubleArray_0x640[di] = (X1 * (float) _rand() > 0.5) ? -1.0 : 1.0;

          ii += 2;
          di += 2;
          ++i1;
        } while (i1 < loc72->int_0x34);

        dj = djj;
      }
      dj += 1;
      djj = dj;

      di = dii + 1;
      dii += 1;
    } while (dj < 2);

    int l0 = 0;
    for (int i = 0; l0 < (int) obj->len_0x1c; i = l0) {
      double f1 = (l0 >= oLen) ? 0.0f : origFloatArray14[l0];

      int dl = 0;
      int dll;
      int dk = 0;
      int dkk = 0;
      do {
        if (loc72->int_0x34 > 0) {
          dll = dl;

          int i4 = 0;
          do {
            int v1 = local7200.intArray_0x320[dk] + 1;
            local7200.intArray_0x320[dk] = v1;

            int v2 = local7200.intArray_0xc80[dk];
            if (v1 > v2) {
              local7200.intArray_0x320[dk] = 0;
            }
            int v3 = local7200.intArray_0x0[dk] + 1;
            local7200.intArray_0x0[dk] = v3;
            if (v3 > v2) {
              local7200.intArray_0x0[dk] = 0;
            }
            double d1 = local7200.doubleArray_0x640[dll] * local7200.doubleArray_0xfa0[dll];

            float *fp = (float *) &buffer18000[4 * local7200.intArray_0x320[dk]];
            *fp = (float) (d1 * f1 + *fp * local7200.doubleArray_0x15e0[dll]);

            dll += 2;
            dk += 2;
            ++i4;
          } while (i4 < loc72->int_0x34);

          l0 = i;
          dk = dkk;
        }
        dk += 1;
        dkk = dk;
        dl += 1;
      } while (dk < 2);

      const double divLen = 1.0f / (double) loc72->int_0x34;

      int n1 = loc72->int_0x34;
      double sum1 = 0.0;
      if (n1 > 0) {
        int *iPtr = (int *) &local7200;
        do {
          sum1 += *(float *) &buffer18000[4 * *iPtr] * loc72->double_0x8;
          --n1;
          iPtr += 2;
        } while (n1);
        l0 = i;
      }
      obj->someFloatArray_0x14[l0] = (float) (divLen * sum1);

      int n2 = loc72->int_0x34;
      double sum2 = 0.0;
      if (n2 > 0) {
        int *iPtr2 = &local7200.intArray_0x0[1];
        do {
          sum2 += *(float *) &buffer18000[4 * *iPtr2] * loc72->double_0x8;
          --n2;
          iPtr2 += 2;
        } while (n2);
        l0 = i;
      }
      obj->someFloatArray_0x18[l0++] = (float) (divLen * sum2);
    }


    if (loc72->byteInt_0x30.b1) {
      obj->len_0x1c = oLen;

      float *oldFloatArray14 = obj->someFloatArray_0x14;
      float *oldFloatArray18 = obj->someFloatArray_0x18;

      obj->someFloatArray_0x14 = (float *) malloc(4 * oLen);
      obj->someFloatArray_0x18 = (float *) malloc(4 * oLen);

      if (oLen > 0) {
        int i0 = 0;
        do {
          obj->someFloatArray_0x14[i0] = 0.0f;
          obj->someFloatArray_0x18[i0] = 0.0f;

          for (int j = 0; j < (int) loc72->uint_0x38; j++) {
            uint idx = i0 + j * oLen;
            obj->someFloatArray_0x14[i0] = oldFloatArray14[idx] + obj->someFloatArray_0x14[i0];
            obj->someFloatArray_0x18[i0] = oldFloatArray18[idx] + obj->someFloatArray_0x18[i0];
          }
          ++i0;
        } while (i0 < (int) oLen);
      }

      free (oldFloatArray14);
      free (oldFloatArray18);
    }


    obj->double_0x2c8 = 0.0;
    obj->double_0x2d0 = 0.0;

    if (obj->len_0x1c > 0) {
      int i5 = 0;
      do {
        obj->someFloatArray_0x14[i5] = (float)
                IXS__WAVEGEN__FUN_0040bd20(obj, obj->someFloatArray_0x14[i5], loc72->double_0x20,
                                           loc72->double_0x28, (char) obj->someFlag_0xcd);

        obj->someFloatArray_0x18[i5] = (float)
                IXS__WAVEGEN__FUN_0040bd70(obj, obj->someFloatArray_0x18[i5], loc72->double_0x20,
                                           loc72->double_0x28, (char) obj->someFlag_0xcd);

        i5 += 1;
      } while (i5 < (int) obj->len_0x1c);
    }

    for (int k = 0; k < (int) obj->len_0x1c; k++) {
      double d2 = (k >= oLen) ? 0.0f : origFloatArray14[k];
      d2 *= loc72->double_0x0;
      obj->someFloatArray_0x14[k] += (float) d2;
      obj->someFloatArray_0x18[k] += (float) d2;
    }

    if (loc72->double_0x40 > 0.0) {
      int l = (int) (loc72->double_0x40 * obj->len_0x1c);
      int len = (int) obj->len_0x1c - l;
      obj->len_0x1c = l;

      for (int m = 0; m < len; m++) {
        double a = (double) (int) m / (float) len;
        double b = 1.0f - a;

        obj->someFloatArray_0x14[m] = (float) (b * obj->someFloatArray_0x14[m + obj->len_0x1c]
                                               + a * obj->someFloatArray_0x14[m]);

        obj->someFloatArray_0x18[m] = (float) (b * obj->someFloatArray_0x18[m + obj->len_0x1c]
                                               + a * obj->someFloatArray_0x18[m]);
      }
    }

    obj->double_0x2c0 = 0.0;
    obj->double_0x2d0 = 0.0;
    obj->double_0x2b8 = 0.0;
    obj->double_0x2c8 = 0.0;

    double d1 = 0.0; // 0.0f;

    if (obj->len_0x1c > 0) {
      float *src1 = obj->someFloatArray_0x18;
      float *src2 = obj->someFloatArray_0x14;

      int n1 = (int) obj->len_0x1c;
      do {
        float fMin = (float) fabs((double) *src2);
        float fMin2 = (float) fabs((double) *src1);
        if (d1 < fMin) {
          d1 = fMin;
        }
        if (d1 < fMin2) {
          d1 = fMin2;
        }
        ++src1;
        ++src2;
        --n1;
      } while (n1);
    }
    int i1 = 0;
    float f2 = (float) (1.0f / d1);    // potential division by 0!
    if (obj->len_0x1c > 0) {
      do {
        obj->someFloatArray_0x14[i1] = f2 * obj->someFloatArray_0x14[i1];
        obj->someFloatArray_0x18[i1] = f2 * obj->someFloatArray_0x18[i1];
        i1++;
      } while (i1 < (int) obj->len_0x1c);
    }

    free (buffer18000);
    free (origFloatArray14);
  }

  void IXS__WAVEGEN__updateProgress_00407b50() {
#ifndef EMSCRIPTEN
    IXS_loadProgressCount_0043b8a0 = IXS_loadProgressCount_0043b8a0 + 1;
    if (IXS_ptrProgress_0043b89c != (float *) nullptr) {
      *IXS_ptrProgress_0043b89c =
              (float) IXS_loadProgressCount_0043b8a0 / (float) (int) IXS_loadTotalLen_0043b898;
    }
#else
    // todo: add some feedback mechanism campatible with web workers..?
#endif
  }

  typedef struct Local560 Local560;
  struct Local560 {
    uint someLen_0x0;
    ushort *bufPtr_0x4;
    uint uint_0x8;
    ushort *bufPtr_0xc;
    uint uint_0x10;
    int int_0x14;
    byte *readPos_0x18;
    uint uint_0x1c;
    uint uint_0x20;
    byte bytes12_0x24[12]; // 9-bytes are copied here, i.e. below modelling still incorrect
    byte bufWavL256_0x30[256];
    byte bufWavR256_0x130[256]; // start of an area that data is copied to
  };

  typedef struct Local1500 Local1500;
  struct Local1500 {
    struct BufInt someInt_0x0;
    struct Local560 a_0x4;
    byte b_0x234[268];
    byte c_0x340[268];
    struct Local400 d_0x44c;
  };

// generates the extracted raw wave-samples and appends them to the "temp cache file"
  int __thiscall IXS__WAVEGEN__writeRecordedSamples_0x408020(SkeletonModule *module, byte *byteArrayBuf, FileSFXI *tmpFile) {
    Local1500 local1500;  // todo: explode into regular vars - fix array layout dependencies 1st

    WaveGen* wavegen = IXS__WAVEGEN__getInstance();

#ifndef EMSCRIPTEN
    fprintf(stderr, "generating regular samples\n");
#else
    sprintf(MSG_BUFFER, "generating regular samples\n");
    JS_printStatus(MSG_BUFFER);
#endif
    module->readPosition_0xc0 = byteArrayBuf;
    uint someInt = IXS__SkeletonModule__readInt_0x409350(module);
    uint result = 0;
    memset(local1500.c_0x340, 0, sizeof(local1500.c_0x340));
    local1500.someInt_0x0.b3 = 1;
    local1500.someInt_0x0.b4 = 1;
    local1500.someInt_0x0.b2 = 0;

    if (someInt > 0) {
      local1500.a_0x4.uint_0x10 = someInt;
      do {
        IXS__SkeletonModule__copyFromInputFile_00409370(module, local1500.b_0x234, 268u);
        int i = 0;
        do {
          byte b = local1500.c_0x340[i + 9] + local1500.b_0x234[i + 9];

          local1500.b_0x234[i + 9] = b;
          local1500.c_0x340[i + 9] = b;
          i += 1;
        } while (i + 9 < 268);

        local1500.a_0x4.readPos_0x18 = module->readPosition_0xc0;
        module->readPosition_0xc0 = local1500.b_0x234;

        IXS__SkeletonModule__copyFromInputFile_00409370(module, local1500.a_0x4.bytes12_0x24, 9u);

        float *loc400 = (float *) &local1500.d_0x44c;
        int n = 100;
        do {
          local1500.a_0x4.someLen_0x0 = (uint) IXS__SkeletonModule__readByte_0x409320(module);
          --n;
          *loc400 = (float) (((double) local1500.a_0x4.someLen_0x0 - 127.0) * ((double) 1.0 / 127.0));
          loc400 += 1;
        } while (n);

        IXS__SkeletonModule__copyFromInputFile_00409370(module, wavegen->int16_0x28, 0x40);
        IXS__SkeletonModule__copyFromInputFile_00409370(module, wavegen->byte16_0x68, 0x10);
        IXS__SkeletonModule__copyFromInputFile_00409370(module, wavegen->float16_0x78, 0x40);
        IXS__SkeletonModule__copyFromInputFile_00409370(module, &wavegen->int_0xbc, 4);
        IXS__SkeletonModule__copyFromInputFile_00409370(module, &local1500.a_0x4.uint_0x20, 4);
        IXS__SkeletonModule__copyFromInputFile_00409370(module, &wavegen->byteInt_0xb8, 4);
        IXS__SkeletonModule__copyFromInputFile_00409370(module, &local1500.someInt_0x0.b4, 1);
        IXS__SkeletonModule__copyFromInputFile_00409370(module, &local1500.someInt_0x0.b3, 1);
        IXS__SkeletonModule__copyFromInputFile_00409370(module, &local1500.someInt_0x0.b2, 1);
        wavegen->bytePtr_0x10.b1 = local1500.someInt_0x0.b2;

        if (wavegen->someFloatArray_0x14) {
          free (wavegen->someFloatArray_0x14);
          wavegen->someFloatArray_0x14 = nullptr;
        }
        if (wavegen->someFloatArray_0x18) {
          free (wavegen->someFloatArray_0x18);
          wavegen->someFloatArray_0x18 = nullptr;
        }
        if (local1500.someInt_0x0.b3) {
          IXS__WAVEGEN__FUN_0x40adb0(wavegen, &local1500.d_0x44c, 100);

          strcpy((char *) local1500.a_0x4.bufWavL256_0x30, (const char *) local1500.a_0x4.bytes12_0x24);
          strcpy((char *) local1500.a_0x4.bufWavR256_0x130, (const char *) local1500.a_0x4.bufWavL256_0x30);

          std::string sampleName= (char*)local1500.a_0x4.bufWavL256_0x30;

          int len;
          if (local1500.d_0x44c.floatArray_0x0[35] < -0.5) {
            local1500.a_0x4.someLen_0x0 = 2;
            len = (int) wavegen->len_0x1c >> 1;
            local1500.a_0x4.uint_0x8 = (int) wavegen->len_0x1c >> 1;
          } else {
            local1500.a_0x4.someLen_0x0 = 1;
            local1500.a_0x4.uint_0x8 = wavegen->len_0x1c;
            len = wavegen->len_0x1c;
          }

          int len2 = 2 * len;
          local1500.a_0x4.bufPtr_0x4 = (ushort *) malloc(2 * len);
          local1500.a_0x4.bufPtr_0xc = (ushort *) malloc(2 * len);

          if (len > 0) {
            int fOffset = 0;
            local1500.a_0x4.uint_0x1c = 4 * local1500.a_0x4.someLen_0x0;
            local1500.a_0x4.someLen_0x0 = local1500.a_0x4.uint_0x8;

            int j = 0;
            do {
              local1500.a_0x4.bufPtr_0x4[j] = (int) (
                      *(float *) ((char *) wavegen->someFloatArray_0x14 + fOffset) * 32767.5 - 0.5f);

              local1500.a_0x4.bufPtr_0xc[j] = (int) (
                      *(float *) ((char *) wavegen->someFloatArray_0x18 + fOffset) * 32767.5 - 0.5f);

              fOffset += local1500.a_0x4.uint_0x1c;
              --local1500.a_0x4.someLen_0x0;
              j += 1;
            } while (local1500.a_0x4.someLen_0x0);
          }


          // append the extracted wave files to the cache file

          if (!local1500.someInt_0x0.b3 || local1500.someInt_0x0.b2) {
#ifndef EMSCRIPTEN
            fprintf(stderr, "   stereo: %s\n", sampleName.c_str());
#else
            sprintf(MSG_BUFFER, "   stereo: %s\n", sampleName.c_str());
            JS_printStatus(MSG_BUFFER);
#endif

            strcat((char *) local1500.a_0x4.bufWavL256_0x30, IXS_s_l_wav_0043867c);
            strcat((char *) local1500.a_0x4.bufWavR256_0x130, IXS_s_r_wav_00438674);

            FileMapSFXI *smplFileL = tmpFile->vftptr_0x0->newMapSFXI1(tmpFile,
                                                                      (char *) local1500.a_0x4.bufWavL256_0x30);
            smplFileL->vftable->writeFile(smplFileL, (byte *) local1500.a_0x4.bufPtr_0x4, len2);
            smplFileL->vftable->deleteObj(smplFileL);

            FileMapSFXI *smplFileR = tmpFile->vftptr_0x0->newMapSFXI1(tmpFile,
                                                                      (char *) local1500.a_0x4.bufWavR256_0x130);
            smplFileR->vftable->writeFile(smplFileR, (byte *) local1500.a_0x4.bufPtr_0xc, len2);
            smplFileR->vftable->deleteObj(smplFileR);

          } else {
#ifndef EMSCRIPTEN
            fprintf(stderr, "   mono:   %s\n", sampleName.c_str());
#else
            sprintf(MSG_BUFFER, "   mono:   %s\n", sampleName.c_str());
            JS_printStatus(MSG_BUFFER);
#endif

            strcat((char *) local1500.a_0x4.bufWavL256_0x30, IXS_s_wav_00438684);

            FileMapSFXI *smplFile = tmpFile->vftptr_0x0->newMapSFXI1(tmpFile, (char *) local1500.a_0x4.bufWavL256_0x30);
            smplFile->vftable->writeFile(smplFile, (byte *) local1500.a_0x4.bufPtr_0x4, len2);
            smplFile->vftable->deleteObj(smplFile);
          }
          free (local1500.a_0x4.bufPtr_0x4);
          free (local1500.a_0x4.bufPtr_0xc);

          module->readPosition_0xc0 = local1500.a_0x4.readPos_0x18;
          IXS__WAVEGEN__updateProgress_00407b50();
        }
        result = --local1500.a_0x4.uint_0x10;
      } while (local1500.a_0x4.uint_0x10);
    }

    free(wavegen->someFloatArray_0x14);
    free(wavegen->someFloatArray_0x18);

    delete IXS_WAVEGEN_PTR;
    IXS_WAVEGEN_PTR = nullptr;

    return result;
  }

  typedef struct Struct24 Struct24;
  struct Struct24 {
    float float_0x0;
    float float_0x4;
    float float_0x8;
    float float_0xc;    // range seems to no 0-100
    float float_0x10;
    float float_0x14;
  };

  // todo: explode struct into regular local vars (and cleanup redundancies)
  typedef struct Local232 Local232;
  struct Local232 {
    float float_0x0;
    int int_0x4;
    uint numBytesRead_0x8;
    uint uint_0xc;
    uint countC4_0x10;
    int int_0x14;
    uint u0to7_0x18;
    short *bufn4096_0x1c; // size multiple of 4096.. sample data for synthesized samples
    uint uint_0x20;
    int int_0x24;
    uint countB_0x28;
    int int_0x2c;
    uint u0to7_0x30;
    uint numD_0x34;
    short *bufPtrA_0x38;
    short *bufPtrC_0x3c; // 16 x countC bytes
    float *floatPtrB_0x40;
    uint uint_0x44;
    uint uint_0x48;
    byte *someBufPtr_0x4c;
    Struct24 *bufPtrD_0x50;
    uint uint_0x54;
    float float_0x58;         // "array" of 4 floats (todo: model as array)
    float float_0x5c;
    float float_0x60;
    float float_0x64;
    byte headerRightSample_0x68[64];
    byte headerLeftSample_0xa8[64];
  };

  std::string getSynthSampleName(char *buf) {
    int n= 0;
    while ((buf[n] >= 32) && (buf[n] < 127) && (buf[n] != '.') && n < 14) { n++; }

    return std::string(buf, n);
  }

  uint __cdecl IXS__WAVEGEN__writeSynthSample_0040d740(byte *buf200, byte *someBuf, FileSFXI *tmpFile) {
    Local232 local_e8;

    local_e8.u0to7_0x30 = 7;
    local_e8.u0to7_0x18 = 7;
    local_e8.countC4_0x10 = 0;
    local_e8.numD_0x34 = 0;
    local_e8.int_0x14 = 0;
    local_e8.int_0x24 = 0;
    local_e8.countB_0x28 = 0;
    local_e8.int_0x2c = 0;

    uint r1 = IXS__WAVEGEN__FUN_0040d6e0(someBuf, &local_e8.numBytesRead_0x8);
    if (r1) {
      return local_e8.numBytesRead_0x8;
    }

    byte *bufPtr = &someBuf[local_e8.numBytesRead_0x8];
    uint numBytesRead = local_e8.numBytesRead_0x8;

    r1 = IXS__WAVEGEN__FUN_0040d6e0(bufPtr, &local_e8.numBytesRead_0x8);
    bufPtr = &bufPtr[local_e8.numBytesRead_0x8];
    local_e8.uint_0x44 = r1;
    local_e8.someBufPtr_0x4c = bufPtr;
    local_e8.uint_0x48 = r1 + local_e8.numBytesRead_0x8 + numBytesRead;

    if (r1 > 0) {
      uint offset = 0;

      while (true) {
        uint g1 = IXS__WAVEGEN__FUN_0040d6e0(&bufPtr[offset], &local_e8.numBytesRead_0x8);
        uint offset2 = local_e8.numBytesRead_0x8 + offset;
        uint g2 = IXS__WAVEGEN__FUN_0040d6e0(&bufPtr[offset2], &local_e8.numBytesRead_0x8);
        offset2 = local_e8.numBytesRead_0x8 + offset2;

        local_e8.uint_0x54 = g2;
        local_e8.uint_0xc = offset2;

        char *inputBuf = (char *) &bufPtr[offset2];  // buffer contains various types and re-casts are needed later

        switch (g1) {

          case 1: {
            uint n1 = 0;
            float *dest1f = &local_e8.float_0x58;
            do {
              *dest1f = (float) (0.005 * (float) *(byte *) &inputBuf[n1++]);
              dest1f += 1;
            } while (n1 < 4);

            local_e8.countC4_0x10 = 4 * *(byte *) &inputBuf[4];   // 1 byte len should never be signed..

            local_e8.bufPtrC_0x3c = (short *) malloc(4 * local_e8.countC4_0x10);;
            if (local_e8.countC4_0x10 > 0) {

              short *dest1s = local_e8.bufPtrC_0x3c;
              local_e8.float_0x0 = (float) local_e8.countC4_0x10;

              for (uint i = 0; i < local_e8.countC4_0x10; i++) {
                float f1 = (float) ((double) i / local_e8.float_0x0);
                double d1 = IXS__WAVEGEN__FUN_0040cec0(f1, local_e8.float_0x5c, local_e8.float_0x64,
                                                       local_e8.float_0x60, local_e8.float_0x58);

                dest1s[0] = dest1s[1] = (short) round(d1);
                dest1s += 2;
              }
            }
          }
            goto LABEL_11;


          case 3: {
            ushort u1 = U16(inputBuf);

            byte *src3 = (byte *) (inputBuf + 2);
            int l3 = (int) (g2 - 2) / 8;
            local_e8.uint_0xc = u1;
            local_e8.numD_0x34 = l3;
            local_e8.bufPtrD_0x50 = (Struct24 *) malloc(24 * l3);

            Struct24 *struct24Ptr0 = local_e8.bufPtrD_0x50;

            float *dest;
            int n;

            if (l3 > 0) {
              dest = &struct24Ptr0->float_0x0;
              n = l3;
              do {
                --n;
                *dest = (float) ((double) U16(src3) * 0.0005);
                dest += 6;
                src3 += 2;
              } while (n);


              dest = &struct24Ptr0->float_0x4;
              n = l3;
              do {
                int b3b = *(byte *) src3;
                *dest = (float) ((int) (local_e8.countC4_0x10 * b3b + 8) >> 8);
                dest += 6;
                src3 += 1;
                --n;
              } while (n);


              dest = &struct24Ptr0->float_0x8;
              n = l3;
              do {
                char b3c = *(char *) src3;
                src3 += 1;
                *dest = (float) (2 * b3c);
                dest += 6;
                --n;
              } while (n);

              dest = &struct24Ptr0->float_0xc;
              n = l3;
              do {
                float b3d = (float) (*(byte *) src3);
                src3 += 1;
                --n;
                *dest = b3d;
                dest += 6;
              } while (n);


              // writes the last 2 floats..
              dest = &struct24Ptr0->float_0x10;
              do {
                // process array of 3-byte ints

                int w0 = *((byte *) src3 + 0);
                int w1 = *((byte *) src3 + 1);
                int w2 = *((byte *) src3 + 2);
                src3 += 3;

                int bit24 = (w2 << 16) | (w1 << 8) | w0;

                int v1 = (int) (bit24 & 0xfff);           // low 12-bits
                int v2 = (int) ((bit24 >> 12) & 0xfff);  // high 12-bits

                if (v1 > 0xc00) {
                  v1 -= 0x1000;
                }
                if (v2 > 0xc00) {
                  v2 -= 0x1000;
                }
                dest[0] = (float) v1;
                dest[1] = (float) v2;
                dest += 6;

                --l3;
              } while (l3);

            }
            local_e8.int_0x14 = local_e8.countC4_0x10 * local_e8.uint_0xc;
            local_e8.bufPtrA_0x38 = (short *) malloc(4 * local_e8.countC4_0x10 * local_e8.uint_0xc);
          }
            break;


          case 4: {
            uint b4 = *(byte *) &inputBuf[0];
            uint l4 = (b4 + *(byte *) &inputBuf[1]) << 10;  // * 0x400 aka *1024

            local_e8.int_0x24 = b4 << 10;
            local_e8.int_0x2c = l4;

            if (local_e8.int_0x24 >= local_e8.int_0x2c) {
              local_e8.int_0x24 = (7 * l4) >> 3;
            }

            uint num4 = g2 - 2;
            local_e8.countB_0x28 = num4;
            local_e8.floatPtrB_0x40 = (float *) malloc(4 * num4);;
            local_e8.bufn4096_0x1c = (short *) malloc(4 * l4);

            if (num4 > 0) {
              char *src = inputBuf + 2;
              float *dest = local_e8.floatPtrB_0x40;
              for (int i = 0; i < num4; i++) {
                *dest++ = (float) *src++;
              }
            }
          }
          LABEL_11:
            offset2 = local_e8.uint_0xc;
            break;


          case 5:
            // for both the high and the low nibble
            local_e8.u0to7_0x30 = *(byte *) inputBuf & 7; // 0..7
            local_e8.u0to7_0x18 = ((int) *(byte *) inputBuf >> 4) & 7;
            break;
          default:
            local_e8.u0to7_0x18 = local_e8.u0to7_0x18 & 0xff;
            break;
        }
        offset = local_e8.uint_0x54 + offset2;
        if (offset >= (int) local_e8.uint_0x44) {
          break;
        }
        bufPtr = local_e8.someBufPtr_0x4c;
      }
    }


    int i = 0;
    int sLen = strlen((const char *) buf200) + 1;
    int i0 = -1;
    int end = sLen - 1;
    if (sLen - 1 > 0) {
      do {
        byte c;
        c = buf200[i];
        if (c == '\\')
          i0 = i;
        if (c == '.')
          end = i;
        ++i;
      } while (i < sLen - 1);
    }

    std::string sampleName;

    int bLen = end - i0 - 1;
    if (bLen > 0) {
      for (int m = 0; m < bLen; m++) {
        local_e8.headerRightSample_0x68[m] = local_e8.headerLeftSample_0xa8[m] = buf200[i0 + 1 + m];
      }
      sampleName = getSynthSampleName((char*)local_e8.headerRightSample_0x68);
    }
    // add the respective L/R to the sample name
    local_e8.headerLeftSample_0xa8[bLen] = 'L';
    local_e8.headerRightSample_0x68[bLen] = 'R';
    int bN = bLen;
    const char *fileExt = &IXS_s_wav_00438684[-bLen];

    // append file extension to name
    for (int n5 = 0; n5 < 5; n5++) {
      byte b = fileExt[bN];
      local_e8.headerLeftSample_0xa8[bN + 1] = b;
      local_e8.headerRightSample_0x68[bN + 1] = b;
      bN++;
    }

    Struct24 *bufPtrD_0x50 = local_e8.bufPtrD_0x50;
    short *bufPtrA_0x38 = local_e8.bufPtrA_0x38;
    short *bufPtrC_0x3c = local_e8.bufPtrC_0x3c;

    IXS__WAVEGEN__writeSynthSub1_0040cfc0(
            local_e8.bufPtrC_0x3c, local_e8.bufPtrA_0x38, local_e8.countC4_0x10, local_e8.int_0x14,
            1.0, &local_e8.bufPtrD_0x50->float_0x0, local_e8.numD_0x34, local_e8.u0to7_0x30);

    uint len = local_e8.int_0x2c;

    IXS__WAVEGEN__writeSynthSub2_0040d270(
            bufPtrA_0x38, local_e8.bufn4096_0x1c, local_e8.int_0x14, local_e8.int_0x2c,
            local_e8.int_0x24, local_e8.countB_0x28, local_e8.floatPtrB_0x40, local_e8.u0to7_0x18);

    free (bufPtrD_0x50);
    free (bufPtrC_0x3c);
    free (bufPtrA_0x38);

    short *sDest = (short *) malloc(2 * len);

    short *sSrc;
    short *sDest0;;

    if (len > 0) {
      sSrc = local_e8.bufn4096_0x1c;
      sDest0 = sDest;
      for (int j = 0; j < len; j++) {
        *sDest0++ = *sSrc;
        sSrc += 2;
      }
    }

    FileMapSFXI *mapSFXI = tmpFile->vftptr_0x0->newMapSFXI1(tmpFile, (char *) local_e8.headerLeftSample_0xa8);
    mapSFXI->vftable->writeFile(mapSFXI, (byte *) sDest, 2 * len);
    mapSFXI->vftable->deleteObj(mapSFXI);

    if (len > 0) {
      sSrc = local_e8.bufn4096_0x1c + 1;
      sDest0 = sDest;
      for (int j = 0; j < len; j++) {
        *sDest0++ = *sSrc;
        sSrc += 2;
      }
    }

    mapSFXI = tmpFile->vftptr_0x0->newMapSFXI1(tmpFile, (char *) &local_e8.headerRightSample_0x68);
    mapSFXI->vftable->writeFile(mapSFXI, (byte *) sDest, 2 * len);
    (mapSFXI->vftable->deleteObj)(mapSFXI);

#ifndef EMSCRIPTEN
    fprintf(stderr, "   stereo: %s\n", sampleName.c_str());
#else
    sprintf(MSG_BUFFER, "   stereo: %s\n", sampleName.c_str());
    JS_printStatus(MSG_BUFFER);
#endif

    free (sDest);
    free(local_e8.floatPtrB_0x40);
    free (local_e8.bufn4096_0x1c);

    return local_e8.uint_0x48;
  }

  typedef struct Local200 Local200;
  struct Local200 {
    byte array200_0x0[200];
  };

  void IXS__WAVEGEN__writeSynthSamples_0040dcb0(byte *srcBuf, FileSFXI *tmpFile) {
    byte *src;
    int len;
    byte *dest;
    byte c;
    Local200 local_c8;

    src = srcBuf + 1;
    if (*srcBuf) {
      len = *srcBuf;
#ifndef EMSCRIPTEN
      fprintf(stderr, "generating synth samples\n");
#else
      sprintf(MSG_BUFFER, "generating synth samples\n");
      JS_printStatus(MSG_BUFFER);
#endif
      do {
        dest = (byte *) &local_c8;
        do {
          c = *src;
          *dest = c;
          dest++;
          src++;
        } while (c);
        int l = IXS__WAVEGEN__writeSynthSample_0040d740((byte *) &local_c8, src, tmpFile);
        src += l;
        IXS__WAVEGEN__updateProgress_00407b50();
        --len;
      } while (len);
    }
  }

  uint __cdecl IXS__WAVEGEN__FUN_0040d6e0(byte *buf, uint *bytesUsed) {
    uint result;

    uint b1 = *buf;
    byte flag = *buf & 0xc0;
    if (flag != 0xc0) {
      *bytesUsed = 1;
      return b1;
    }

    uint b2 = buf[1];
    result = (b1 & 0x3f) + ((b2 & 0x7f) << 6); // resembles the "read volume/panning (byte value)"
    if ((b2 & 0x80) != 0) {
      *bytesUsed = 4;
      result |= U16(buf + 2) << 13;
    } else {
      *bytesUsed = 2;
    }

    return result;
  }


  double __cdecl IXS__WAVEGEN__FUN_0040cec0(float param_1, float param_2, float param_3, float param_4, float param_5) {

    float m = fmod(param_1, 1.0);
    double a = 32767.0;

    double b;
    if (m >= (double) param_2) {
      a = -32767.0;
      b = 1.0f - param_2;
      m = 1.0f - m;
    } else {
      b = param_2;
    }
    double c = (m >= b) ? 1.0f : m / b;

    double d;
    if (c >= param_3) {
      float e = 1.0f - param_3;
      double f = 1.0f - c;
      d = (f >= e) ? 1.0f : f / e;
    } else {
      d = c / param_3;
    }
    double q = 1.0f - param_4;
    double r = (d >= q) ? 1.0f : d / q;

    return a * (sin(r * 1.5707964) * param_5 + (1.0f - param_5) * r); // 1.5707964 rad = 90 deg
  }

  inline float __cdecl writeSynthSub1_calcSomething(float *blocksIn, int numBlocks) {
    double sum1 = 0.0;
    double sum2 = 0.0;

    for (int j = 0; j < numBlocks; j++) {
      double t1 = fabs(blocksIn[2]);
      double t2 = 100.0 - blocksIn[3];
      sum2 += t2 * t1;
      sum1 += t1 * blocksIn[3];
      blocksIn += 6;
    }

    float in = (sum1 >= sum2) ? sum1 : sum2;

    // added hack: it seems silly that an "in" marginally smaller than
    // 0.0001 should cause the output to jump from 10000 to 0 (original impl
    // returned "in" instead of 10000)! todo: find testcase
    return (in > 0.0001) ? in = 1.0f / in : 10000;
  }

  inline short *__cdecl writeSynthSub1_createLookupBuf(const short *tab1, uint len) {
    short *res = (short *) malloc(4 * len + 4);
    memcpy(res, tab1, 4 * ((4 * len) >> 2));
    res[2 * len] = tab1[0];
    res[2 * len + 1] = tab1[1];
    return res;
  }


  void __cdecl IXS__WAVEGEN__writeSynthSub1_0040cfc0(short *tab1, short *outBuffer, int len, int outLen,
                                                     float one, float *blocksIn, int numBlocks, int u0to7) {
    // todo: blocksIn is of type Struct24 * - cleanup API

    if (numBlocks >= 1 && numBlocks <= 1000) {

      float f1 = writeSynthSub1_calcSomething(blocksIn, numBlocks);

      memset(outBuffer, 0, 4 * outLen);

      short *sTab = writeSynthSub1_createLookupBuf(tab1, len);

      float *fBlockPtr = blocksIn;
      float *fBlockPtr0 = blocksIn;
      uint blockLen = len << 10;

      float scale = 1.0f / one;

      for (int k = 0; k < numBlocks; k++) {
        uint q = (uint) (fBlockPtr[1] * 1024.0);
        // todo: original ASM used ROUND
        int v2 = (int) (scale * fBlockPtr[0] * 1024.0);
        int v3 = (int) (fBlockPtr[3] * 1024.0 * fBlockPtr[2] * f1);
        int v4 = (int) ((100.0 - fBlockPtr[3]) * 1024.0 * fBlockPtr[2] * f1);
        double d1 = fBlockPtr[5] * IXS_2PI_004300d0 / (double) outLen;
        double d2 = fBlockPtr[4] * d1 * 1024.0;

        if (outLen > 0) {
          short *outBuffer0 = outBuffer;
          int nextSin = 1;
          int s; // initialized via nextSinCountdown
          for (int i = 0; i < outLen; i++) {
            if (!--nextSin) {
              s = (int) (sin((double) i * d1) * d2 + (double) v2);  // todo: original ASM used ROUND
              nextSin = 64;
            }
            for (; q >= blockLen; q -= blockLen);

            int q0 = q & 0x3ff;     // must be signed! ... 1024
            int s1 = sTab[2 * (q >> 10)];
            int s2 = sTab[2 * (q >> 10) + 1];
            int s3 = v3 * (s2 + ((q0 * (sTab[2 * (q >> 10) + 3] - s2)) >> 10));

            outBuffer0[0] += ((v4 * (s1 + ((q0 * (sTab[2 * (q >> 10) + 2] - s1)) >> 10))) >> 10);
            outBuffer0[1] += (s3 >> 10);
            outBuffer0 += 2;
            q += s;
          }
          fBlockPtr = fBlockPtr0;
        }
        fBlockPtr += 6;
        fBlockPtr0 = fBlockPtr;
      }

      free((void *) sTab);
      IXS__WAVEGEN__FUN_0040d540(outBuffer, outLen, u0to7);
    }
  }

  // synthesized waveforms.. testcase: "Ixalance theme"
  void __cdecl IXS__WAVEGEN__writeSynthSub2_0040d270(short *tab1, short *outBuffer, int dataLen, int step,
                                                     int param_5, int n, float *param_7, uint param_8) {

    const double DIV12 = 1.0 / 12; //  0.08333333333333333

    uint prefixLen0 = step;
    memset(outBuffer, 0, 4 * step);

    const  int s1 = 0x10000 / n;

    short *tmpBuf = (short *) malloc(4 * dataLen + 4);
    int dataLen2 = 2 * dataLen;
    memcpy(tmpBuf, tab1, 4 * ((uint) (4 * dataLen) >> 2));

    tmpBuf[dataLen2] = tab1[0];
    tmpBuf[dataLen2 + 1] = tab1[1];

    const double X = (double) 0x100000000;

    if (n > 0) {
      int c = 0;

//      const double LOGE2 = 0.69314718055994530941723212145818;
//      const double LOG2E = 1.4426950408889634073599246810019;
//      double d0 = fyl2x(2.0, LOGE2);

      for (int k = 0; k < n; k++) {
//        double d1 = d0 * *floatArr * DIV12 * LOG2E;
//        double r1 = round(d1);
//        double d2 = fscale(f2xm1(d1 - r1) + 1.0, r1);

        double d2 = pow(2.0, *param_7 * DIV12);

        // "Wirth's canonical form".. todo: some descriptive names would be nice here

        int64 a = (int64)(d2 * X);
        int64 as = a * step;
        int cn = c / n;
        double cnd = (double)cn;

        uint h = (int64) (cnd * d2 * X);
        int g = (int) ((uint64) (int64) (cnd * d2 * X) >> 32) % dataLen;

        if (step > 0) {
          for (int i = 0; ; ) {
            int q = (cn + 1 == step) ? 0 : cn + 1;

            uint64 m = a + NEW_UINT64(g, h);
            g = (a + NEW_UINT64(g, h)) >> 32;
            h = m;

            int b1 = tmpBuf[2 * (g % dataLen)];
            int b2 = tmpBuf[2 * (g % dataLen) + 1];
            int c1 = b1 + ((GET_USHORT(m, 1) * (tmpBuf[2 * (g % dataLen) + 2] - b1)) >> 16);
            int c2 = b2 + ((GET_USHORT(m, 1) * (tmpBuf[2 * (g % dataLen) + 3] - b2)) >> 16);

            if (i < param_5) {
              uint ma = (uint) (m + as) >> 16;
              uint mb = (int) ((NEW_UINT64(g, m) + as) >> 32) % dataLen;
              int mc = tmpBuf[2 * mb + 1];
              int md = tmpBuf[2 * mb];

              uint z = (uint) ((double) i * 16384.0 / (double) param_5);
              c2 = (int) (c2 * z + (0x4000 - z) * (mc + ((int) (ma * (tmpBuf[2 * mb + 3] - mc)) >> 16))) >> 14;
              c1 = (int) (c1 * z + (0x4000 - z) * (md + ((int) (ma * (tmpBuf[2 * mb + 2] - md)) >> 16))) >> 14;
            }
            outBuffer[2 * q] += (int) (s1 * c1) >> 16;
            outBuffer[2 * q + 1] += (int) (s1 * c2) >> 16;

            if (++i >= step) {
              break;
            }
            cn = q;
          }
        }
        ++param_7;
        c += step;
      }
    }
    free(tmpBuf);

    IXS__WAVEGEN__FUN_0040d540(outBuffer, step, param_8);
  }

  // todo: un-obfuscate
  void __cdecl IXS__WAVEGEN__FUN_0040d540(short *ioBuf, int len, uint flags) {

    if (flags) {
      int u1 = 32767;
      int l1 = -32767;
      int u2 = 32767;
      int l2 = -32767;
      int u3 = 32767;
      int l3 = -32767;

      if (len > 0) {
        short *ioBuf0 = ioBuf;
        int l = len;
        do {
          int s0 = ioBuf0[0];
          int s1 = ioBuf0[1];
          ioBuf0 += 2;
          if (s0 < u1) {
            u1 = s0;
          }
          if (s0 > l1) {
            l1 = s0;
          }
          if (s1 < u2) {
            u3 = s1;
            u2 = s1;
          }
          if (s1 > l2) {
            l3 = s1;
            l2 = s1;
          }
          --l;
        } while (l);
      }


      if ((flags & 4) != 0) {
        if (u1 >= u2) {
          u1 = u2;
        } else {
          u3 = u1;
        }
        if (l1 <= l2) {
          l1 = l2;
        } else {
          l3 = l1;
        }
      }


      int x0 = 0;
      int y0 = 0;
      int q1 = (l1 + u1) / 2;
      int q2 = (u3 + l3) / 2;
      if ((flags & 1) != 0) {
        u1 -= q1;
        l1 -= q1;
        u3 -= q2;
        x0 = -q1;
        y0 = -q2;
        l3 -= q2;
      }


      if ((flags & 2) != 0) {
        int al1 = abs(l1);
        int au1 = abs(u1);

        if (au1 <= al1) {
          au1 = al1;
        }

        int al3 = abs(l3);
        int au3 = abs(u3);
        if (au3 > al3) {
          al3 = au3;
        }

        int x = au1 ? 0x1fffffff / au1 : 0;
        int y = al3 ? 0x1fffffff / al3 : 0;
                      
        int n = len;
        if (len > 0) {
          short *ioBuf0 = ioBuf;
          do {
            ioBuf0[0] = (x * (x0 + ioBuf0[0])) >> 14;
            ioBuf0[1] = (y * (y0 + ioBuf0[1])) >> 14;
            ioBuf0 += 2;
            --n;
          } while (n);
        }
      } else {
        int n = len;
        if (len > 0) {
          short *ioBuf0 = ioBuf;
          do {
            ioBuf0[0] += x0;
            ioBuf0[1] += y0;
            ioBuf0 += 2;
            --n;
          } while (n);
        }
      }

    }
  }

#ifdef EMSCRIPTEN
  // better reuse the same buffer than risk fragmenting the memory in EMSCRIPTEN context..
  static BufSFXI *cacheFileBuf = 0;  // todo: this is a memory leak..
#endif

  FileSFXI* IXS__WAVEGEN__createSampleCacheFile(char* filename, byte *dataBuf1, byte *dataBuf2, FileSFXI *destSFXI) {

    SkeletonModule module;

    BufSFXI* cacheFileBuf = nullptr;
    FileSFXI *fileSFXI;
    if (destSFXI) {
      fileSFXI= destSFXI;
    } else {
#ifndef EMSCRIPTEN
      cacheFileBuf = (BufSFXI *) _GlobalAlloc(0x1000, 0x1e00000); // this seems to be a memory leak in the original impl
#else
      if (!cacheFileBuf) {
        cacheFileBuf = (BufSFXI *) malloc(0x1e00000);
      } else {
        cacheFileBuf->magic_0x0 = 0;  // trigger reset
      }
#endif
      fileSFXI = IXS__FileSFXI__newFileSFXI_00413c20(cacheFileBuf);
    }

    // start of the tmp file generation logic -
    IXS__WAVEGEN__writeRecordedSamples_0x408020(&module, dataBuf1, fileSFXI);
    IXS__WAVEGEN__writeSynthSamples_0040dcb0(dataBuf2, fileSFXI);

#ifndef EMSCRIPTEN
    fprintf(stderr, "sample generation completed\n\n");
#else
    sprintf(MSG_BUFFER, "sample generation completed\n\n");
    JS_printStatus(MSG_BUFFER);
#endif

    if (destSFXI != nullptr) {
      return destSFXI;
    }
#ifndef EMSCRIPTEN
    // save the newly generated ".tmp" file
/*
    File0 *fileMap0 = IXS__File__Z_ctor_FUN_004138e0();
    FileMap* fileMap = (*fileMap0->vftable->newFileMap)(fileMap0, filename);
    if (fileMap) {
      (*fileSFXI->vftptr_0x0->virtWrite)(fileSFXI, fileMap);
      (*fileSFXI->vftptr_0x0->deleteObject)(fileSFXI);
      (*fileMap->vftable->delete0)(fileMap);
      delete fileMap0;
    } else {
      fprintf(stderr, "error: could not save tmp file: %s\n",filename);
    }
*/
#else
    JS_saveCacheFile((char*) filename, (byte*)fileSFXI->buffer_0x4, (uint) fileSFXI->buffer_0x4->currentMemPtr_0x4);
#endif
//     if (cacheFileBuf)
//         _GlobalFree(cacheFileBuf);
    return fileSFXI;
  }
}

