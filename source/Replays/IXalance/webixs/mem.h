/*
* just a wrapper for platform specific API calls
*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#ifndef IXS_MEM_H
#define IXS_MEM_H

#include "basetypes.h"

#if !defined(EMSCRIPTEN) && !defined(LINUX)
// the Visual Studio specific APIs used in the original player

inline void* _GlobalAlloc(uint flags, size_t dwBytes) { return GlobalAlloc(flags, dwBytes); }
inline void _GlobalFree(void* vp) { GlobalFree(vp); }

#else
// use replacement APIs actually available on other platforms
inline void* _GlobalAlloc(uint flags, size_t dwBytes) { return malloc(dwBytes); }
inline void _GlobalFree(void* vp) { free(vp); }

#endif

#endif //IXS_MEM_H
