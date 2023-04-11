/*
*   Copyright (C) 2022 Juergen Wothke
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/


#include "asmEmu.h"

#include <math.h>


OLINE double fyl2x(double a, double b) {
  //fyl2x	Replace ST(1) with (ST(1) * log_2(ST(0))) and pop the register stack.
  return b * log2(a);
}
OLINE double fscale(double a, double b) {
  // T(0) = ST(0) * 2RoundTowardZero(ST(1));
  return a*pow(2, trunc(b));
}
OLINE double f2xm1(double a) {
  // exponential value of 2 to the power of the source operand minus 1
  return pow(2, a) - 1.0;
}


// see https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html

// attempt to simulate respective SSE / MMX instructions..
OLINE int64 m_psllqi(int64 in, uint c) {
  return in << c;
}

OLINE int64 m_psrlqi(int64 in, uint c) { // new
  return in >> c;
}

OLINE int64 m_psradi(int64 a, uint c) {
  // Shift packed 32-bit integers in a right by imm8 while shifting
  // in sign bits, and store the results in dst.
  int* p= (int*)&a;
  *p++ >>= c;
  *p >>= c;
  return a;
}

OLINE int64 m_pmaddwd (int64 a, int64 b){
  // Multiply packed signed 16-bit integers in a and b, producing
  // intermediate signed 32-bit integers. Horizontally add adjacent
  // pairs of intermediate 32-bit integers, and pack the results in dst.

  int64 result;
  int* dst = (int*)&result;
  short* srcA = (short*)&a;
  short* srcB = (short*)&b;

  dst[0] = ((int)srcA[1] * (int)srcB[1]) + ((int)srcA[0] * (int)srcB[0]);
  dst[1] = ((int)srcA[3] * (int)srcB[3]) + ((int)srcA[2] * (int)srcB[2]);
  return result;
}

OLINE int64 m_por(int64 a, int64 b) {
  return a | b;
}
OLINE int64 m_pand(int64 a, int64 b) {
  return a & b;
}
OLINE int64 mm_cvtsi32_si64 (uint a) {
  return a;
}

OLINE uint mm_cvtsi64_si32 (int64 in) {
  return in;
}

OLINE void clamped_add(short *a, int b) {
  // aka "saturated add", i.e. the maximum
  // is set in case of an overflow
  b += *a;
  if (b>0x7fff) b= 0x7fff;
  if (b<-0x7fff) b= -0x7fff;
  *a = b;
}
OLINE int64 m_paddsw (int64 a, int64 b) {
  clamped_add((short*)&a + 0, *((short*)&b + 0));
  clamped_add((short*)&a + 1, *((short*)&b + 1));
  clamped_add((short*)&a + 2, *((short*)&b + 2));
  clamped_add((short*)&a + 3, *((short*)&b + 3));

  return a;
}
OLINE void mul_high(short *result, int a, short *b) {
  a *= *b;
  *result = a >> 16;
}
OLINE int64 m_pmulhw (int64 a, int64 b) {
  int64 result;
  mul_high(((short*)&result + 0), *((short*)&a + 0), ((short*)&b + 0));
  mul_high(((short*)&result + 1), *((short*)&a + 1), ((short*)&b + 1));
  mul_high(((short*)&result + 2), *((short*)&a + 2), ((short*)&b + 2));
  mul_high(((short*)&result + 3), *((short*)&a + 3), ((short*)&b + 3));

  return result;
}

