#ifndef NEZINT_H
#define NEZINT_H

#include <stddef.h>

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef   signed char      int8_t;
typedef unsigned char     uint8_t;
typedef   signed short    int16_t;
typedef unsigned short   uint16_t;
typedef   signed int      int32_t;
typedef size_t           uint32_t;
typedef   signed __int64  int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

#endif
