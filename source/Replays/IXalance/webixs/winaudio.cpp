/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#include "winaudio.h"

#if !defined(EMSCRIPTEN) && !defined(LINUX)

#include <mmeapi.h>
#include <math.h>

#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "ws2_32.lib")


#include "PlayerCore.h"

namespace IXS {

  uint AUDIO_OUTPUTDEV_ID_00438630 = 0xffffffff;
  float IXS_FLOAT_0042ff14 = 0.000015259022;
  float IXS_FLOAT_0042ff18 = 65535.0;
//uint IXS_playPauseFlag_0043b884;

  HWAVEOUT IXS_audioDevHandle_0043b844;
  WAVEHDR hdr;
  WAVEFORMATEX wfx;

  void __cdecl MM_initAudioBuffer_00405dc0(uint *buffer, uint bufSize);

  int __cdecl MM_setupAudioDevice_004061a0(uint uDeviceID, uint sampleRate, uint channels, uint bitsPerSmpl) {
    int iVar1;
    MMRESULT result;
    WAVEFORMATEX waveformatex;
    uint bytesPerSample;

    memset(&waveformatex, 0, sizeof(waveformatex));

    waveformatex.nChannels = (ushort) channels;
    iVar1 = (bitsPerSmpl & 0xffff) * ((uint) channels & 0xffff);
    bytesPerSample = (int) (iVar1 + (iVar1 >> 0x1f & 7U)) >> 3 & 0xffff;

    waveformatex.nBlockAlign = bytesPerSample;
    waveformatex.wBitsPerSample = bitsPerSmpl;
    waveformatex.nAvgBytesPerSec = bytesPerSample * sampleRate;
    waveformatex.wFormatTag = 1;
    waveformatex.nSamplesPerSec = sampleRate;
    waveformatex.cbSize = 0;
    result = waveOutOpen(&IXS_audioDevHandle_0043b844, uDeviceID, &waveformatex, 0, 0, 0);
    waveOutReset(IXS_audioDevHandle_0043b844);
    waveOutClose(IXS_audioDevHandle_0043b844);
    if (result != 0) {
      return 0xffffffff;
    }
    return AUDIO_OUTPUTDEV_ID_00438630;
  }

  int MM_setupAudioDevice_00405f90() {
    uint n;
    int ret;
    uint dwFormats;
    uint uDeviceID;
    uint devId;
    tagWAVEOUTCAPSA outCaps;

    if (AUDIO_OUTPUTDEV_ID_00438630 == 0xffffffff) {
      n = waveOutGetNumDevs();
      dwFormats = 0;
      IXS_nSamplesPerSec_0043b890 = 0x2b11;
      IXS_AudioOutFlags_0043b888 = 0x105;
      AUDIO_OUTPUTDEV_ID_00438630 = 0xffffffff;
      if (n == 0) {
        AUDIO_OUTPUTDEV_ID_00438630 = 0xffffffff;
        IXS_AudioOutFlags_0043b888 = 0x105;
        IXS_nSamplesPerSec_0043b890 = 0x2b11;
        return 0xffffffff;
      }
      uDeviceID = 0;
      if (0 < (int) n) {
        do {
          waveOutGetDevCapsA(uDeviceID, &outCaps, 0x34);
          if ((dwFormats < outCaps.dwFormats) && ((outCaps.dwFormats & 0xfff) != 0)) {
            dwFormats = outCaps.dwFormats;
            devId = uDeviceID;

          }
          uDeviceID = uDeviceID + 1;
        } while ((int) uDeviceID < (int) n);
      }
      AUDIO_OUTPUTDEV_ID_00438630 = devId;
      if ((dwFormats & 0x800) != 0) {
        ret = MM_setupAudioDevice_004061a0(devId, 0xac44, 0x2, 0x10);
        if (ret != -1) {
          IXS_AudioOutFlags_0043b888 = 0x11a;
          IXS_nSamplesPerSec_0043b890 = 0xac44; // 44100
          return AUDIO_OUTPUTDEV_ID_00438630;
        }
      }
      if ((dwFormats & 0x80) != 0) {
        ret = MM_setupAudioDevice_004061a0
                (AUDIO_OUTPUTDEV_ID_00438630, 0x5622, 0x2, 0x10);
        if (ret != -1) {
          IXS_AudioOutFlags_0043b888 = 0x11a;
          IXS_nSamplesPerSec_0043b890 = 0x5622;
          return AUDIO_OUTPUTDEV_ID_00438630;
        }
      }
      if ((dwFormats & 0x400) != 0) {
        ret = MM_setupAudioDevice_004061a0
                (AUDIO_OUTPUTDEV_ID_00438630, 0xac44, 0x1, 0x10);
        if (ret != -1) {
          IXS_AudioOutFlags_0043b888 = 0x116;
          IXS_nSamplesPerSec_0043b890 = 0xac44;
          return AUDIO_OUTPUTDEV_ID_00438630;
        }
      }
      if ((dwFormats & 0x40) != 0) {
        ret = MM_setupAudioDevice_004061a0
                (AUDIO_OUTPUTDEV_ID_00438630, 0x5622, 0x1, 0x10);
        if (ret != -1) {
          IXS_AudioOutFlags_0043b888 = 0x116;
          IXS_nSamplesPerSec_0043b890 = 0x5622;
          return AUDIO_OUTPUTDEV_ID_00438630;
        }
      }
      if ((dwFormats & 8) != 0) {
        ret = MM_setupAudioDevice_004061a0
                (AUDIO_OUTPUTDEV_ID_00438630, 0x2b11, 0x2, 0x10);
        if (ret != -1) {
          IXS_AudioOutFlags_0043b888 = 0x11a;
          IXS_nSamplesPerSec_0043b890 = 0x2b11;
          return AUDIO_OUTPUTDEV_ID_00438630;
        }
      }
      if ((dwFormats & 4) != 0) {
        ret = MM_setupAudioDevice_004061a0
                (AUDIO_OUTPUTDEV_ID_00438630, 0x2b11, 0x1, 0x10);
        if (ret != -1) {
          IXS_AudioOutFlags_0043b888 = 0x116;
          IXS_nSamplesPerSec_0043b890 = 0x2b11;
          return AUDIO_OUTPUTDEV_ID_00438630;
        }
      }
      AUDIO_OUTPUTDEV_ID_00438630 = 0xffffffff;
    }
    return AUDIO_OUTPUTDEV_ID_00438630;
  }

  void __cdecl MM_initAudioBuffer_00405dc0(uint *buffer, uint bufSize) {
    uint uVar1;
    char cVar2;
    ushort uVar3;
    uint uVar4;
    uint uVar5;

    if ((IXS_AudioOutFlags_0043b888 & AUDIO_16BIT) == 0) {
      cVar2 = (~(byte) (IXS_AudioOutFlags_0043b888) & 0xf0) << 3;
      uint v = cVar2;
      v = (v << 8) | v;
      v = (v << 16) | v;

      if (0 < (int) bufSize) {
        // todo: use memset
        for (uVar4 = bufSize >> 2; uVar4 != 0; uVar4 = uVar4 - 1) {
          *buffer = v;
          buffer = buffer + 1;
        }
        for (uVar4 = bufSize & 3; uVar4 != 0; uVar4 = uVar4 - 1) {
          *(char *) buffer = cVar2;
          buffer = (uint *) ((int) buffer + 1);
        }
      }
    } else {
      uVar3 = ~IXS_AudioOutFlags_0043b888;
      uVar4 = (int) bufSize >> 1;
      if (0 < (int) uVar4) {
        uVar1 = (uVar3 & 0x10) << 0xb;
        for (uVar5 = uVar4 >> 1; uVar5 != 0; uVar5 = uVar5 - 1) {
          *buffer = uVar1 | (uVar3 & 0xfff0) << 0x1b;
          buffer = buffer + 1;
        }
        for (uVar4 = (uint) ((uVar4 & 1) != 0); uVar4 != 0; uVar4 = uVar4 - 1) {
          *(short *) buffer = (short) uVar1;
          buffer = (uint *) ((int) buffer + 2);
        }
      }
    }
  }

  void MM_audioclose_00405f40() {
    MM_initAudioBuffer_00405dc0((uint *) hdr.lpData, 0x80000);
    waveOutReset(IXS_audioDevHandle_0043b844);
    waveOutUnprepareHeader(IXS_audioDevHandle_0043b844, &hdr, 0x20);
    waveOutClose(IXS_audioDevHandle_0043b844);
  }

  LPWAVEHDR __cdecl M_configWaveOutWrite_00405e30(uint *lpData) {
    ushort bitsPerSample;
    uint blockSize;

    if (AUDIO_OUTPUTDEV_ID_00438630 == 0xffffffff) {
      return nullptr;
    }
    wfx.wFormatTag = 1;
    wfx.nChannels = ((IXS_AudioOutFlags_0043b888 & AUDIO_STEREO) != 0) + 1;
    bitsPerSample = (-(ushort) ((IXS_AudioOutFlags_0043b888 & AUDIO_16BIT) != 0) & 8) + 8;
    wfx.nSamplesPerSec = IXS_nSamplesPerSec_0043b890;
    blockSize = (int) ((uint) bitsPerSample * (uint) wfx.nChannels) >> 3;
    wfx.nBlockAlign = blockSize;
    wfx.wBitsPerSample = bitsPerSample;
    wfx.nAvgBytesPerSec = blockSize * wfx.nSamplesPerSec;
    wfx.cbSize = 0;
    waveOutOpen(&IXS_audioDevHandle_0043b844, AUDIO_OUTPUTDEV_ID_00438630,
                &wfx, 0, 0, 0);

    hdr.lpData = (char *) lpData;
    hdr.dwBufferLength = 0x80000;
    hdr.dwBytesRecorded = 0;
    hdr.dwLoops = 0xffffffff;
    hdr.dwUser = 0;
    hdr.dwFlags = 0xc;
    waveOutPrepareHeader(IXS_audioDevHandle_0043b844, &hdr, 0x20);
    waveOutWrite(IXS_audioDevHandle_0043b844, &hdr, 0x20);
    return &hdr;
  }

  void M_pauseOutput_00406280() {
    if (AUDIO_OUTPUTDEV_ID_00438630 == 0xffffffff) {
//    IXS_playPauseFlag_0043b884 = 1;
      return;
    }
    waveOutPause(IXS_audioDevHandle_0043b844);
  }

  void M_resumeOutput_004062b0() {
    if (AUDIO_OUTPUTDEV_ID_00438630 == 0xffffffff) {
//    IXS_playPauseFlag_0043b884 = 0;
      return;
    }
    waveOutRestart(IXS_audioDevHandle_0043b844);
  }

  void __cdecl M_getVolume_004062e0(float *outVolLeft, float *outVolRight) {
    uint64 rightLeft = 0;

    waveOutGetVolume(IXS_audioDevHandle_0043b844, (LPDWORD) &rightLeft);
    *outVolLeft = (float) (uint64) ((uint) rightLeft & 0xffff) * IXS_FLOAT_0042ff14;
    *outVolRight = (float) (uint64) ((uint) rightLeft >> 0x10) * IXS_FLOAT_0042ff14;
  }

  void M_setVolume_00406340(float volLeft, float volRight) {
    uint right = (uint) round(volRight * IXS_FLOAT_0042ff18) << 16;
    uint left = (uint) round(volLeft * IXS_FLOAT_0042ff18) & 0xffff;
    waveOutSetVolume(IXS_audioDevHandle_0043b844, right | left);
  }

  uint IXS__PlayerCore__getAudioDeviceWavePos_00406240() {
    MMTIME local_c;

    local_c.wType = 2;
    waveOutGetPosition(IXS_audioDevHandle_0043b844, &local_c, 0xc);
    return (wfx.nBlockAlign & 0xffff) * local_c.u.ms;
  }

}
#endif
