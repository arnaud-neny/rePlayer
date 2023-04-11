/*
* Subclass of FileMap used handle tmp "cached samples file".
*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/
// todo: refactor to actual C++ class def

#ifndef IXS_FILEMAPSFXI_H
#define IXS_FILEMAPSFXI_H

#include "FileMap.h"

namespace IXS {
  typedef struct BufSFXI BufSFIX;

  // CAUTION: since this is part of a variable length sequence, the fields
  // are potentially unaligned!
  typedef struct SegmentHeadSFXI SegmentHeadSFXI;
  struct SegmentHeadSFXI { // buffer with a 260 bytes header and a variable sized data segment
    byte array_0x0[255];
    uint lenData_0x100; // length of the following data in bytes
    uint data_0x104; // variable sized buffer starts here
  };

  struct BufSFXI { // header struct of SFXI file - complete file may be up to 32MB large
    uint magic_0x0; // "SFXI" magic identifier
    byte *currentMemPtr_0x4;
    uint segmtCount_0x8;
    // a list of variable sized segments up to 32MB
    struct SegmentHeadSFXI segmtHead_0xc;
    // variable sized part of the segment
    byte variablePart;
    byte field5_0x115;
    byte field6_0x116;
    byte field7_0x117;
    // etc..
  };

  typedef struct FileMapSFXI FileMapSFXI;

  typedef struct IXS__FileMapSFXI__VFTABLE IXS__FileMapSFXI__VFTABLE;

  struct IXS__FileMapSFXI__VFTABLE {
    void (*readFile)(struct FileMapSFXI *, byte *, uint);

    int (*writeFile)(struct FileMapSFXI *, byte *, uint);

    void (*setPtr)(struct FileMapSFXI *, long, int);

    uint (*getSize)(struct FileMapSFXI *);

    uint (*getPtr)(struct FileMapSFXI *);

    void (*virt_setPtr)(struct FileMap *, long);

    byte *(*getMemBuffer)(struct FileMapSFXI *);

    void (*deleteObj)(struct FileMapSFXI *);
  };

  struct FileMapSFXI {
    struct IXS__FileMapSFXI__VFTABLE *vftable;
    struct SegmentHeadSFXI *baseAddr_0x4;
    struct BufSFXI *sfxiFileBuf_0x8;
    int int_0xc;
    int currentPos_0x10;
  };


  // "public methods"
  FileMapSFXI * __thiscall IXS__FileMapSFXI__ctor_00413900(FileMapSFXI *map, char *filename, BufSFXI *fileBuf, int mode);
  void __thiscall IXS__FileMapSFXI__ctor_004139c0(FileMapSFXI *map);

}


#endif //IXS_FILEMAPSFXI_H
