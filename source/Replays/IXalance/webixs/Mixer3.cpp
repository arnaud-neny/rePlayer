/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#include "Mixer3.h"
#include "asmEmu.h"

// todo: this impl still seems to have clamping issues leading to clicks in the output
// check if this was merely meant as a performance optimization (as compared to Mixer2)
// or if there were actual playback quality improvements built into this..
// if so then try to clean it up..

namespace IXS {

  void __thiscall IXS__Mixer3__virt4_3_00409c30(MixerBase *mixer, Buf0x40 *buf);
  void __thiscall IXS__Mixer3__virt3_3_00409eb0(MixerBase *mixer, Buf0x40 *buf);


  static IXS__MixerBase__VFTABLE IXS_MIX3_VFTAB_0042ff80 = {
          IXS__MixerBase__virt0_00409ac0,
          IXS__MixerBase__clearSampleBuf_00409af0,
          IXS__MixerBase__copyToAudioBuf_0x409b30,
          IXS__Mixer3__virt3_3_00409eb0,
          IXS__Mixer3__virt4_3_00409c30
  };

  MixerBase *IXS__Mixer3__ctor(byte *audioOutBuffer) {
    MixerBase *mixer = (MixerBase *) malloc(sizeof(MixerBase));
    memset(mixer, 0, sizeof(MixerBase));
    if (mixer != (MixerBase *) nullptr) {
      IXS__MixerBase__ctor_004098b0(mixer);
      mixer->vftable = &IXS_MIX3_VFTAB_0042ff80;
      mixer->circAudioOutBuffer_0x1c = audioOutBuffer;
    }
    return mixer;
  }

  void IXS__Mixer3__switchTo(MixerBase *mixer) {
    if (mixer != (MixerBase *) nullptr) {
      mixer->vftable = &IXS_MIX3_VFTAB_0042ff80;
    }
  }

  inline void addClamped(short *dest, int val) {
    val += *dest;
    if (val > 0x7fff) val = 0x7fff;
    else if (val < -0x7fff) val = -0x7fff;
    *dest = (short)val;
  }

  // unused..
  void __thiscall IXS__Mixer3__virt4_3_00409c30(MixerBase *mixer, Buf0x40 *buf) {

    Buf0x40 *buf0 = buf;
    if (buf->smplForwardLoopFlag_0x18 != 255) {
      int a1 = mixer->blockLen_0x14 / (int) mixer->bytesPerSample_0x2c;
      int b = (int64) ((double) buf->uint_0x24 / (double) mixer->sampleRate_0x8 * 256.0);
      int b0 = b;
      byte *smplData = buf->smplDataPtr_0x4;
      short *smplBuf = mixer->smplBuf16Ptr_0x20;
      int64 vol = (int) (int64) ((double) (int) (buf->volume_0x14 << 6) * mixer->outputVolume_0x24);
      uint64 vol0 = vol | ((vol | ((vol | (vol << 16)) << 16)) << 16);
      if (a1 > 0) {
        while (1) {
          int pos = buf0->pos_0x0;
          if (buf0->int_0x8 == -1) {
            int loopStart = buf0->smplLoopStart_0x1c;
            if (pos <= loopStart) {
              buf0->int_0x8 = 1;
              buf0->pos_0x0 = 2 * loopStart - pos;
            }
          } else {
            int loopEnd = buf0->smplLoopEnd_0x20;
            if (pos >= loopEnd) {
              int forwardLoop = buf0->smplForwardLoopFlag_0x18;
              if (!forwardLoop) {
                buf0->smplForwardLoopFlag_0x18 = 255;
                return;
              }
              if (forwardLoop == 2) {
                buf0->int_0x8 = 255;
                buf0->pos_0x0 = 2 * loopEnd - pos;
              } else {
                buf0->pos_0x0 = pos + buf0->smplLoopStart_0x1c - loopEnd;
              }
            }
          }

          int bn;
          uint hi;
          uint lo;

          if (buf0->int_0x8 == -1) {
            bn = -b;
            hi = (uint64) (((b + (int64) (buf0->pos_0x0 - buf0->smplLoopStart_0x1c)) << 8) - b) >> 32;
            lo = ((b + buf0->pos_0x0 - buf0->smplLoopStart_0x1c) << 8) - b;
          } else {
            bn = b;
            hi = (uint64) (((b + (int64) (buf0->smplLoopEnd_0x20 - buf0->pos_0x0)) << 8) - b) >> 32;
            lo = ((b + buf0->smplLoopEnd_0x20 - buf0->pos_0x0) << 8) - b;
          }

          int64 x = NEW_INT64(hi, lo) / ((int64) b << 8);
          int x0 = x;
          if (!(uint) x) {
            GET_UINT(x, 0) = 1;
            x0 = 1;
          }
          if ((int) x > a1) {
            GET_UINT(x, 0) = a1;
            x0 = a1;
          }
          a1 -= x;

          uint pos0 = buf0->pos_0x0;
          buf0->pos_0x0 += bn * x;

          uint *smplBuf0 = (uint *) smplBuf;

          if (buf0->bitsPerSmpl_0xc == 8) {
            uint h = pos0;
            uint h0 = pos0 >> 8;

            for (int i = 0; i<x0; i++) {
        //      uint64 w0 = mm_cvtsi32_si64(*smplBuf0);
              uint64 w1 = m_pmulhw(
                      m_psllqi(
                              m_pand(mm_cvtsi32_si64(U32( &smplData[h0]) ), 0xff),
                              8),
                      vol0);
              h += bn;
              h0 = h >> 8;
         //     *(uint *)smplBuf0 = mm_cvtsi64_si32(m_paddsw(w0, w1));

              addClamped(((short*)smplBuf0), (short)w1);
              addClamped(((short*)smplBuf0)+1, (short)(w1>>16));

              smplBuf0 = (uint *) ((char *) smplBuf0 + 2);  // writes 4 bytes and step +2.. makes no sense..
            }

          } else {

            uint h = pos0;
            uint h0 = pos0 >> 8;

            for (int i = 0; i<x0; i++) {
//              int64 w0 = mm_cvtsi32_si64(*smplBuf0);
              int64 w1 = m_pmulhw(
                        m_pand(mm_cvtsi32_si64(U32(&smplData[2 * h0])), 0xffff),
                      vol0);
//              *smplBuf0 = mm_cvtsi64_si32(m_paddsw(w0, w1));

              addClamped(((short*)smplBuf0), (short)w1);
              addClamped(((short*)smplBuf0)+1, (short)(w1>>16));

              h += bn;
              h0 = h >> 8;
              smplBuf0 = (uint *) ((char *) smplBuf0 + 2);
            }
          }
          smplBuf = (short *) smplBuf0;
          if (a1 <= 0)
            return;
          buf0 = buf;
          b = b0;
        }
      }
    }
  }

  void __thiscall IXS__Mixer3__virt3_3_00409eb0(MixerBase *mixer, Buf0x40 *buf) {
    Buf0x40 *buf0 = buf;

    if (buf->smplForwardLoopFlag_0x18 != 255) {
      int numSmpls = mixer->blockLen_0x14 / (int) mixer->bytesPerSample_0x2c;
      uint v = buf->volume_0x14;
      int64 volL = (int) (int64) ((double) (int) (v * (64 - buf->channelPan_0x10)) * mixer->outputVolume_0x24);
      int volR =                  (int64) ((double) (int) (buf->channelPan_0x10 * v) * mixer->outputVolume_0x24);
      uint64 vol = (volL | ((volR | (uint64) (volL << 16)) << 16)) << 16;
      GET_UINT(vol, 1) |= volR >> 31;
      uint64 vol_ = (uint) volR | vol;

      byte *smplData = buf->smplDataPtr_0x4;
      short *smplBuf = mixer->smplBuf16Ptr_0x20;

      int t;
      int q2;

      int b = (int) (256.0 * buf->uint_0x24 / mixer->sampleRate_0x8 );

      while (numSmpls > 0) {
        int pos = buf0->pos_0x0;
        if (buf0->int_0x8 == -1) {
          int loopStart = buf0->smplLoopStart_0x1c;
          if (pos <= loopStart) {
            buf0->int_0x8 = 1;
            buf0->pos_0x0 = 2 * loopStart - pos;
          }
        } else {
          int loopEnd = buf0->smplLoopEnd_0x20;
          if (pos >= loopEnd) {
            int forwardLoop = buf0->smplForwardLoopFlag_0x18;
            if (!forwardLoop) {
              buf0->smplForwardLoopFlag_0x18 = 255;
              return;
            }
            if (forwardLoop == 2) {
              buf0->int_0x8 = 255;
              buf0->pos_0x0 = 2 * loopEnd - pos;
            } else {
              buf0->pos_0x0 = pos + buf0->smplLoopStart_0x1c - loopEnd;
            }
          }
        }

        int bn;
        uint hi;
        uint lo;

        if (buf0->int_0x8 == -1) {
          bn = -b;
          hi = (uint64) (((b + (int64) (buf0->pos_0x0 - buf0->smplLoopStart_0x1c)) << 8) - b) >> 32;
          lo = ((b + buf0->pos_0x0 - buf0->smplLoopStart_0x1c) << 8) - b;
        } else {
          bn = b;
          hi = (uint64) (((b + (int64) (buf0->smplLoopEnd_0x20 - buf0->pos_0x0)) << 8) - b) >> 32;
          lo = ((b + buf0->smplLoopEnd_0x20 - buf0->pos_0x0) << 8) - b;
        }
        int64 x = NEW_INT64(hi, lo) / ((int64) b << 8);
        int bn_ = bn;

        int x0 = x;
        if (!(uint) x) {
          GET_UINT(x, 0) = 1;
          x0 = 1;
        }
        if ((int) x > numSmpls) {
          GET_UINT(x, 0) = numSmpls;
          x0 = numSmpls;
        }
        numSmpls -= x;

        uint pos0 = buf0->pos_0x0;
        buf0->pos_0x0 += bn * x;

        char keepGoing = 0;
        int int_0x8 = buf0->int_0x8;

        if (!(((int_0x8 == -1) && (buf0->pos_0x0 > buf0->smplLoopStart_0x1c))
            || (buf0->pos_0x0 < buf0->smplLoopEnd_0x20))) {

          GET_UINT(x, 0) = x - 1;
          keepGoing = 1;
          x0 = x;
        }

        if ((uint) x) {

          uint *smplBuf0 = (uint *) smplBuf;

          if (buf0->bitsPerSmpl_0xc == 8) {
            uint h = pos0;
            uint h0 = pos0 >> 8;

            for(int i = 0; i<x0; i++) {
              uint64 w0 = mm_cvtsi32_si64(*smplBuf0);
              uint64 w1 = m_psllqi(
                      m_pand(mm_cvtsi32_si64(Ub32(&smplData[h0]) ), 0xff), 8);
              h += bn;
              h0 = h >> 8;
              
              *smplBuf0 = mm_cvtsi64_si32(m_paddsw(w0, m_pmulhw(m_por(w1, m_psllqi(w1, 0x10)), vol_)));
              
              
              ++smplBuf0;
            }
          } else {
            uint offset = 2 * int_0x8;  // this should take care of the original self-modifying code..

            uint h = pos0;
            uint h0 = pos0 >> 8;

            for(int i = 0; i<x0; i++) {
              int64 s0a = mm_cvtsi32_si64(U32(&smplData[2 * h0]));
              int64 s0b = mm_cvtsi32_si64(U32(&smplData[2 * h0 + offset]));
              uint64 m1 = m_pand(
                      m_psradi(
                              m_pmaddwd(
                                      m_por(
                                              m_pand(s0a, 0xffff),
                                              m_psllqi(
                                                      m_pand(s0b, 0xffff),
                                                      0x20)),
                                      m_por(
                                              m_psllqi(mm_cvtsi32_si64(((ushort) h << 6) & 0x3fff), 0x20),
                                              mm_cvtsi32_si64(0x4000 - (((ushort) h << 6) & 0x3fff)))),
                              0xE),
                      (int64)0xffff0000ffff);

              uint64 m2 = m_pmulhw(m_por(m1, m_psllqi(m1, 0x10)), vol_);
              int64 w0 = mm_cvtsi32_si64(*smplBuf0);

              *smplBuf0 = mm_cvtsi64_si32(
                      m_paddsw(
                              w0,
                              m_paddsw(
                                      m_pand(m2, (int64)0xffffffff),
                                      m_pand(m_psrlqi(m2, 0x20), (int64)0xffffffff))));
              ++smplBuf0;

              h += bn;
              h0 = h >> 8;
            }
          }
          smplBuf = (short *) smplBuf0;
          buf0 = buf;
          bn_ = bn;
        }
        if (!keepGoing) {
          continue;
        }
        int q1;

        if (buf0->int_0x8 != -1) {
          int t1;
          int t2 = buf0->smplForwardLoopFlag_0x18;
          switch (t2) {
            case 0:
              t1 = buf0->pos_0x0 - bn_;
              t = t1;
              q2 = t1;
              break;
            case 2:
              t = buf0->pos_0x0 - bn_;
              t1 = 2 * buf0->smplLoopEnd_0x20 - buf0->pos_0x0;
              q2 = t1;
              break;
            case 1:
              t = buf0->pos_0x0 - bn_;
              t1 = buf0->pos_0x0 + buf0->smplLoopStart_0x1c - buf0->smplLoopEnd_0x20;
              q2 = t1;
              break;
          }
          q1 = t;
        } else {
          q1 = buf0->pos_0x0 - bn_;
          t = q1;
          q2 = 2 * buf0->smplLoopStart_0x1c - buf0->pos_0x0;
        }

        uint64 w0 = mm_cvtsi32_si64(*(uint *) smplBuf);
        uint64 w1;
        if (buf0->bitsPerSmpl_0xc == 8) {
          uint64 w = m_psllqi(
                  m_pand(
                          mm_cvtsi32_si64(
                                  (byte) q1 * (char) smplData[q1 >> 8]
                                  + (256 - (uint) (byte) q1) * (char) smplData[q2 >> 8]),
                          0xffff),
                  8);
          w1 = m_pmulhw(m_por(w, m_psllqi(w, 0x10)), vol_);
        } else {
          uint64 w = m_pand(
                  mm_cvtsi32_si64(S16(&smplData[2 * (q1 >> 8)])  << 8 >> 8),
                  0xffff);
          w1 = m_pmulhw(m_por(w, m_psllqi(w, 0x10)), vol_);
        }
        *(uint *) smplBuf = mm_cvtsi64_si32(m_paddsw(w0, w1));

        smplBuf += 2;
      }
    }
  }

}