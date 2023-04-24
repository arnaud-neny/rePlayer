#ifndef MULDIV_H_
#define MULDIV_H_

#include "../include/nezplug/nezint.h"

#ifdef _WIN32
extern int __stdcall MulDiv(int nNumber,int nNumerator,int nDenominator);
#else
__inline static uint32_t MulDiv(uint32_t m, uint32_t n, uint32_t d)
{
	return ((double)m) * n / d;
}
#endif


#endif
