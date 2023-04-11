/*
* Some 1:1 emulation of low-level x86 CPU operations.
*
*   Copyright (C) 2022 Juergen Wothke
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

// todo: cleanup calculations to completely get rid of the 1:1 x86 ASM emulation

#ifndef IXS_ASMEMU_H
#define IXS_ASMEMU_H

#include "basetypes.h"

#include <math.h>

#if defined(EMSCRIPTEN) || defined(LINUX)
#define OLINE
#else
#define OLINE inline
#endif

// stuff used for log/pow impl
extern OLINE double fyl2x(double a, double b) ;
extern OLINE double fscale(double a, double b);
extern OLINE double f2xm1(double a);

// low-level SSE / MMX CPU instructions..
extern OLINE int64 m_psllqi(int64 in, uint c);
extern OLINE int64 m_psrlqi(int64 in, uint c);
extern OLINE int64 m_por(int64 a, int64 b);
extern OLINE int64 m_pand(int64 a, int64 b) ;
extern OLINE int64 mm_cvtsi32_si64 (uint a);
extern OLINE uint mm_cvtsi64_si32 (int64 in);
extern OLINE int64 m_paddsw (int64 a, int64 b) ;
extern OLINE void mul_high(signed short *result, int a, signed short *b) ;
extern OLINE int64 m_pmulhw (int64 a, int64 b);
extern OLINE int64 m_psradi(int64 a, uint c);
extern OLINE int64 m_pmaddwd (int64 a, int64 b);

extern OLINE void clamped_add(short *a, int b);


#endif //IXS_ASMEMU_H
