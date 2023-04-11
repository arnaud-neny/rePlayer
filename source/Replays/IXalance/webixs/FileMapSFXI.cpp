/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#include "FileMapSFXI.h"

#include <string.h>
//#include <string>
//#include <cctype>
//#include <algorithm>


const char *CHAR_MAP = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char tupper(char c) {
  if (c >= 'a' && c <='z') {
    int i= c - 'a';
    return  CHAR_MAP[i];
  }
  return c;
}
// use private impl since strcmpi does not seem to be available on all platforms.,
bool xstrcmpi(const char* str1, const char* str2) {
  if (str1 && str2) {
    int l1 = strlen(str1);
    int l2 = strlen(str2);
    if (l1 == l2) {
      for (int i = 0; i<l1; i++) {
        if (tupper(str1[i]) != tupper(str2[i])) {
          return true;
        }
      }
      return false;
    }
  }
  return true;
}

// handle unaligned data
inline void setUuint(byte *addr, uint val) {
  memcpy(addr, &val, sizeof(uint));
}

inline uint getUuint(byte *addr) {
  uint val;
  memcpy(&val, addr, sizeof(uint));
  return val;
}


namespace IXS {
  // "private methods"
  void IXS__FileMapSFXI__readFile_004139d0(FileMapSFXI *map, byte *dest, uint len);
  int IXS__FileMapSFXI__writeFile_00413a20(FileMapSFXI *map, byte *src, uint len);
  uint IXS__FileMapSFXI__getSize_00413a90(FileMapSFXI *map);
  uint IXS__FileMapSFXI__getPtr_00413aa0(FileMapSFXI *map);
  void IXS__FileMapSFXI__setPtr_00413ab0(FileMapSFXI *map, long lDistanceToMove, int op);
  byte *IXS__FileMapSFXI__getMemBuffer_00413b00(FileMapSFXI *map);
  void IXS__FileMapSFXI__deleteObject_00413b10(FileMapSFXI *map);

  static IXS__FileMapSFXI__VFTABLE IXS_FILEMAPSFXI_VFTAB_00430e74 = {
          IXS__FileMapSFXI__readFile_004139d0,
          IXS__FileMapSFXI__writeFile_00413a20,
          IXS__FileMapSFXI__setPtr_00413ab0,
          IXS__FileMapSFXI__getSize_00413a90,
          IXS__FileMapSFXI__getPtr_00413aa0,
          IXS__FileMap__setPos_004137a0,
          IXS__FileMapSFXI__getMemBuffer_00413b00,
          IXS__FileMapSFXI__deleteObject_00413b10
  };

  bool isDone(FileMapSFXI *map, char *filename, int mode) {
    if (!mode) {
      map->baseAddr_0x4 = nullptr;
      return true;
    }
    strcpy((char *) map->baseAddr_0x4, filename);

    setUuint((byte*)&map->baseAddr_0x4->lenData_0x100, 0);
    ++map->sfxiFileBuf_0x8->segmtCount_0x8;
    map->sfxiFileBuf_0x8->currentMemPtr_0x4 += 260;
    return false;
  }

  FileMapSFXI * __thiscall IXS__FileMapSFXI__ctor_00413900(FileMapSFXI *map, char *filename, BufSFXI *fileBuf, int mode) {

    map->vftable = &IXS_FILEMAPSFXI_VFTAB_00430e74;
    map->sfxiFileBuf_0x8 = fileBuf;
    map->currentPos_0x10 = 0;
    map->baseAddr_0x4 = &fileBuf->segmtHead_0xc;

    int segmtCount = fileBuf->segmtCount_0x8;

    if ((segmtCount <= 0) && isDone(map, filename, mode)) {
      return map;
    } else {
      while (xstrcmpi((const char *) map->baseAddr_0x4, filename)) {
        --segmtCount;
        map->baseAddr_0x4 = (SegmentHeadSFXI *) ((char *) map->baseAddr_0x4 + getUuint((byte*)&map->baseAddr_0x4->lenData_0x100) + 260);
        if ((segmtCount <= 0) && isDone(map, filename, mode)) {
          return map;
        }
      }
    }
    return map;
  }


  void __thiscall IXS__FileMapSFXI__ctor_004139c0(FileMapSFXI *map) {
    map->vftable = &IXS_FILEMAPSFXI_VFTAB_00430e74;
  }

  void IXS__FileMapSFXI__readFile_004139d0(FileMapSFXI *map, byte *dest, uint len) {
    unsigned int l;
    int pos;
    SegmentHeadSFXI *segmtHead;
    signed int segmtLen;

    l = len;
    pos = map->currentPos_0x10;
    segmtHead = map->baseAddr_0x4;
    segmtLen = getUuint((byte*)&segmtHead->lenData_0x100);
    if ((int) (pos + len) > segmtLen)
      l = segmtLen - pos;
    memcpy(dest, (char *) &segmtHead->data_0x104 + pos, l);
    map->currentPos_0x10 += l;
  }

  int IXS__FileMapSFXI__writeFile_00413a20(FileMapSFXI *map, byte *src, uint len) {
    int pos;
    signed int segmtLen;

    pos = map->currentPos_0x10;
    segmtLen = getUuint((byte*)&map->baseAddr_0x4->lenData_0x100);
    if ((int) (pos + len) > segmtLen) {
      map->sfxiFileBuf_0x8->currentMemPtr_0x4 += len + pos - segmtLen;
      setUuint((byte*)&map->baseAddr_0x4->lenData_0x100, map->currentPos_0x10 + len);
    }
    memcpy((char *) &map->baseAddr_0x4->data_0x104 + map->currentPos_0x10, src, len);
    map->currentPos_0x10 += len;
    return 0;
  }


  uint IXS__FileMapSFXI__getSize_00413a90(FileMapSFXI *map) {
    return getUuint((byte*)&map->baseAddr_0x4->lenData_0x100);
  }


  uint IXS__FileMapSFXI__getPtr_00413aa0(FileMapSFXI *map) {
    return map->currentPos_0x10;
  }


  void IXS__FileMapSFXI__setPtr_00413ab0(FileMapSFXI *map, long lDistanceToMove, int op) {
    if (op == 0) {
      map->currentPos_0x10 = lDistanceToMove;
    } else {
      if (op == 1) {
        map->currentPos_0x10 = map->currentPos_0x10 + lDistanceToMove;
        return;
      }
      if (op == 2) {
        map->currentPos_0x10 = getUuint((byte*)&map->baseAddr_0x4->lenData_0x100) + lDistanceToMove;
        return;
      }
    }
  }


  byte *IXS__FileMapSFXI__getMemBuffer_00413b00(FileMapSFXI *map) {
    return (byte *) &map->baseAddr_0x4->data_0x104;
  }


  void IXS__FileMapSFXI__deleteObject_00413b10(FileMapSFXI *map) {
    if (map != (FileMapSFXI *) nullptr) {
      IXS__FileMapSFXI__ctor_004139c0(map);
      operator delete(map);
    }
  }
}

