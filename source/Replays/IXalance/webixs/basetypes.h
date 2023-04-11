/*
* Just some types I like to use.
*
*   Copyright (C) 2022 Juergen Wothke
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#ifndef IXS_BASETYPES_H
#define IXS_BASETYPES_H

// hack to get rid of respective exported code
#define __thiscall


typedef unsigned char       byte;
typedef unsigned short      ushort;
typedef unsigned int        uint;

// rePlayer
#if 0//defined(EMSCRIPTEN) || defined(LINUX)
#define __fastcall
#define __cdecl


typedef long long           int64;
typedef unsigned long long  uint64;

#ifndef LINUX
typedef unsigned long  size_t;
#else
typedef unsigned int  size_t;
#endif

typedef int HANDLE;

#else
typedef __int64             int64;
typedef unsigned __int64    uint64;

typedef void * HANDLE;
#endif

typedef void * LPVOID;


// hack to prevent Ghidra from playing with useless CONCAT crap
typedef struct BufInt BufInt;
struct BufInt {
  byte b1;
  byte b2;
  byte b3;
  byte b4;
};

// helpers
#define NEW_INT64(high, low) (((int64) (high) << 32) | (uint)(low))
#define NEW_UINT64(high, low) (((uint64) (high) << 32) | (uint)(low))
#define GET_USHORT(addr, idx) (*((ushort*)&(addr)+idx))
#define GET_UINT(addr, idx) (*((uint*)&(addr)+idx))

  // for "slightly unaligned" memory access
#define U16(addr) (((ushort)((byte*)(addr))[0]) | (((ushort)((byte*  )(addr))[1]) << 8 ))
#define S16(addr) ((short)(((ushort)((byte*)(addr))[0]) | (((ushort)((byte*  )(addr))[1]) << 8 )))
#define U32(addr) (((uint)((ushort*)(addr))[0]) | (((uint  )((ushort*)(addr))[1]) << 16))
  // for "totally unaligned" memory access
#define Ub32(addr) (((uint)((byte*)(addr))[0]) | (((uint  )((byte*)(addr))[1]) << 8) | (((uint  )((byte*)(addr))[2]) << 16) | (((uint  )((byte*)(addr))[3]) << 24))


#endif


