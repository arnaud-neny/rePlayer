/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#include "Mixer2.h"
#include "asmEmu.h"

namespace IXS {

  static IXS__MixerBase__VFTABLE IXS_MIX2_VFTAB_0042ff6c = {
          IXS__MixerBase__virt0_00409ac0,
          IXS__MixerBase__clearSampleBuf_00409af0,
          IXS__MixerBase__copyToAudioBuf_0x409b30,
          IXS__Mixer2__virt3_2_0040a590,
          IXS__Mixer2__virt4_2_0040a340
  };

  MixerBase *IXS__Mixer2__ctor(byte *audioOutBuffer) {
    MixerBase *mixer = (MixerBase *) malloc(sizeof(MixerBase));
    memset(mixer, 0, sizeof(MixerBase));
    if (mixer != (MixerBase *) nullptr) {
      IXS__MixerBase__ctor_004098b0(mixer);
      mixer->vftable = &IXS_MIX2_VFTAB_0042ff6c;
      mixer->circAudioOutBuffer_0x1c = audioOutBuffer;
    }
    return mixer;
  }

  void IXS__Mixer2__switchTo(MixerBase *mixer) {
    if (mixer != (MixerBase *) nullptr) {
      mixer->vftable = &IXS_MIX2_VFTAB_0042ff6c;
    }
  }

  // unused..?
  void __thiscall IXS__Mixer2__virt4_2_0040a340(MixerBase *mixer, Buf0x40 *buf) {
    // todo: replace MMX optimizations with intelligible higher-level logic

    Buf0x40 *buf0 = buf;
    if (buf->smplForwardLoopFlag_0x18 != 255) {
      int numSmpls = (int)(mixer->blockLen_0x14 / mixer->bytesPerSample_0x2c);
      int b = (int) ((double) buf->uint_0x24 / (double) mixer->sampleRate_0x8 * 256.0f);
      int b0 = b;
      byte *smplData = buf->smplDataPtr_0x4;
      short *smplBuf = mixer->smplBuf16Ptr_0x20;
      int64 vol = (int) (int64) ((double) (int) (buf->volume_0x14 << 6) * mixer->outputVolume_0x24);
      if (numSmpls > 0) {
        while (true) {
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
              int forwardsLoop = buf0->smplForwardLoopFlag_0x18;
              if (!forwardsLoop) {
                buf0->smplForwardLoopFlag_0x18 = 255;
                return;
              }
              if (forwardsLoop == 2) {
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
          if ((int) x > numSmpls) {
            GET_UINT(x, 0) = numSmpls;
            x0 = numSmpls;
          }
          numSmpls -= x;

          uint pos0 = buf0->pos_0x0;
          buf0->pos_0x0 += bn * x;

          uint *smplBuf0 =  (uint *) smplBuf;

          if (buf0->bitsPerSmpl_0xc == 8) {
            uint h = pos0;
            uint h0 = pos0 >> 8;
            for (int i= 0; i<x0; i++) {
              int64 w0 = mm_cvtsi32_si64(*smplBuf0);
              int64 w1 = m_pmulhw(
                      m_psllqi(
                              m_pand(mm_cvtsi32_si64(Ub32(&smplData[h0]) ), 0xff),
                              8),
                      vol);
              *(uint *) smplBuf0 = mm_cvtsi64_si32(m_paddsw(w0, w1));
              smplBuf0 = (uint *) ((char *) smplBuf0 + 2);

              h += bn;
              h0 = h >> 8;
            }
          } else {
            uint h = pos0;
            uint h0 = pos0 >> 8;


            for (int i= 0; i<x0; i++) {
              int64 w0 = mm_cvtsi32_si64(*smplBuf0);
              int64 w1 = m_pand(mm_cvtsi32_si64(Ub32(&smplData[2 * h0]) ), 0xffff);
              *smplBuf0 = mm_cvtsi64_si32(m_paddsw(w0, m_pmulhw(w1,vol)));

// difference to below method seems to be the output to only 1 channel.. instead of 2 - irrelevant since not used..
//            int64 w0 = mm_cvtsi32_si64(*smplBuf0);
//            int64 w1 = m_pand(mm_cvtsi32_si64(U32(&smplData[2 * h0])), IXS_QWORD_00438780);
//            *smplBuf0 = mm_cvtsi64_si32(m_paddsw(w0, m_pmulhw(m_por(w1, m_psllqi(w1, 0x10)), vol)));

              smplBuf0 = (uint *) ((char *) smplBuf0 + 2);

              h += bn;
              h0 = h >> 8;
            }
          }

          smplBuf = (short *) smplBuf0;
          if (numSmpls <= 0) {
            return;
          }
          buf0 = buf;
          b = b0;
        }
      }
    }
  }

  inline void addClamped(short *dest, int val) {
    val += *dest;
    if (val > 0x7fff) val = 0x7fff;
    else if (val < -0x7fff) val = -0x7fff;
    *dest = (short)val;
  }

  void __thiscall IXS__Mixer2__virt3_2_0040a590(MixerBase *mixer, Buf0x40 *buf) {
// FIXME: "buf0->int_0x8 == -1" checks seem to be dead code - not one song that seems to be triggering those
// branches!! is there some 255 vs -1 mismatch? where is this actually updated? in here it is only ever set to
// 1 or to 255.. ()

// testcase: "Paranoia" uses buf->int_0x8 != 1

    Buf0x40 *buf0 = buf;
    if (buf->smplForwardLoopFlag_0x18 != 255) {
      int numSmpls = (int)(mixer->blockLen_0x14 / mixer->bytesPerSample_0x2c);
      int b = (int)((double) buf->uint_0x24 / (double) mixer->sampleRate_0x8 * 256.0f);
      byte *smplData = buf->smplDataPtr_0x4;
      short *smplBuf = mixer->smplBuf16Ptr_0x20;

//      int64 vol = (int) (((uint) (int64) ((double)
//              (buf->volume_0x14 * (64 - buf->channelPan_0x10)) * mixer->outputVolume_0x24) << 16) |
//              (int64) ((double)
//              (buf->volume_0x14 *       buf->channelPan_0x10)  * mixer->outputVolume_0x24));

      // 1.0 * 17-bit-int * 64 => issue: result may not always fit into 16-bits!
      int vol= (ushort)(mixer->outputVolume_0x24 * (float)(buf->volume_0x14 * (64 - buf->channelPan_0x10)));
//      int vol= (ushort)(mixer->outputVolume_0x24 * (float)(buf->volume_0x14 * (buf->channelPan_0x10)));
//
      // original impl: overkill since only volL is used below..
//      ushort volR= mixer->outputVolume_0x24 * (buf->volume_0x14 *       buf->channelPan_0x10);
//      int64 vol = (((uint) volL) << 16) || ((uint) volR);

      for (; numSmpls > 0; ) {
        int pos = buf0->pos_0x0;

        if (buf0->int_0x8 == -1) {
          // dead code..? todo: might be a sign that something is still wrong with the player logic..
          int loopStart = buf0->smplLoopStart_0x1c;
          if (pos <= loopStart) {
            buf0->int_0x8 = 1;
            buf0->pos_0x0 = 2 * loopStart - pos;
          }
        } else {
          int loopEnd = buf0->smplLoopEnd_0x20;
          if (pos >= loopEnd) {
            int forwardsLoop = buf0->smplForwardLoopFlag_0x18;
            if (!forwardsLoop) {
              buf0->smplForwardLoopFlag_0x18 = 255;
              return;
            }
            if (forwardsLoop == 2) {
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
          // still dead code.. like above
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
        if ((int) x > numSmpls) {
          GET_UINT(x, 0) = numSmpls;
          x0 = numSmpls;
        }
        numSmpls -= x;

        uint pos0 = buf0->pos_0x0;
        buf0->pos_0x0 += bn * x;

        if (buf0->bitsPerSmpl_0xc == 8) {
//          uint *smplBuf0 = (uint *) smplBuf;

          uint h = pos0;
          uint h0 = pos0 >> 8;

          // testcase: Pacmania
          for(int i = 0; i < x0; i++) {
//            int64 w0 = mm_cvtsi32_si64(*smplBuf0);
//            int64 w1 = m_pand(mm_cvtsi32_si64(Ub32(&smplData[h0])), IXS_QWORD_00438788);  // & 0xff
//            int64 w2 = m_psllqi(w1, 8);
//            *smplBuf0 = mm_cvtsi64_si32(m_paddsw(w0, m_pmulhw(m_por(w2, m_psllqi(w2, 0x10)), vol)));

// the above MMX garbage performs the two sample updates in parallel.. and is equivalent to this:
            char *smpl = (char*)&smplData[h0];
//            int a= (int)(((vol>>16) * ((int)*smpl) << 8) >> 16);
//            int a= (int)((vol * ((int)*smpl) << 8) >> 16);
            int a= vol * *smpl >> 8;    // no need to shift back and forth...
            addClamped(&smplBuf[0], a);  // L
            addClamped(&smplBuf[1], a);  // R

//            ++smplBuf0;
            smplBuf += 2;

            h += bn;
            h0 = h >> 8;
          }
//          smplBuf = (short *) smplBuf0;

        } else {
          // testcase: "World of noise"
//    fprintf(stderr, "X\n");
//          uint *smplBuf0 = (uint *) smplBuf;
          uint h = pos0;
          uint h0 = pos0 >> 8; // added
          for(int i = 0; i < x0; i++) {
// ORIGINAL
//            int64 w0 = mm_cvtsi32_si64(*smplBuf0);
//            int64 w1 = m_pand(mm_cvtsi32_si64(U32(&smplData[2 * h0])), IXS_QWORD_00438780); // & 0xffff
//            *smplBuf0 = mm_cvtsi64_si32(m_paddsw(w0, m_pmulhw(m_por(w1, m_psllqi(w1, 0x10)), vol)));

// the above MMX garbage performs the two sample updates in parallel.. and is equivalent to this:
            short *smpl = (short*)&smplData[2 * h0];
//            int a= (int)(((vol>>16) * smpl[0]) >> 16);
            int a= (int)((vol * smpl[0]) >> 16);
            addClamped(&smplBuf[0], a);   // mono output
            addClamped(&smplBuf[1], a);

//            ++smplBuf0;
            smplBuf += 2;
            h += bn;
            h0 = h >> 8;
          }

//          smplBuf = (short *) smplBuf0;
        }
        buf0 = buf;
      }
    }
  }

}