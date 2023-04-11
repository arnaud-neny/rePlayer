/*
* Used to directly play audio on win32 (this was use by original player).
*
* just using it for initial testing..
*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#ifndef IXS_WINAUDIO_H
#define IXS_WINAUDIO_H

#include "basetypes.h"

#if !defined(EMSCRIPTEN) && !defined(LINUX)
// fucking windows garbage: just including mmeapi.h of course doesn't work
// but gives you completely useless error messages instead.. damn retards

//typedef long NTSTATUS;
#include <Windows.h>
#include <timeapi.h>
#include <stdlib.h>
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "ws2_32.lib")

#include <mmeapi.h>

namespace IXS {
  extern int MM_setupAudioDevice_00405f90();

  extern void MM_audioclose_00405f40();

  extern LPWAVEHDR __cdecl M_configWaveOutWrite_00405e30(uint *lpData);

  extern void M_pauseOutput_00406280();

  extern void M_resumeOutput_004062b0();

  extern void __cdecl M_getVolume_004062e0(float *outVolLeft, float *outVolRight);

  extern void M_setVolume_00406340(float volLeft, float volRight);
}
#endif

#endif
