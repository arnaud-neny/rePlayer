/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/


#include "FileSFXI.h"
#include "File0.h"

#include <stdio.h>

#if defined(EMSCRIPTEN) || defined(LINUX)
#include <stdlib.h>
#endif


namespace IXS {
  // "private methods"
  FileSFXI *__thiscall IXS__FileSFXI__ctor_0x430e94(FileSFXI *file, BufSFXI *buffer);

  FileMapSFXI *IXS__FileSFXI__newMapSFXI1_00413b50(FileSFXI *file, char *filename);

  FileMapSFXI *IXS__FileSFXI__newMapSFXI0_00413b80(FileSFXI *file, char *filename);

  uint IXS__FileSFXI__write_0x413bf0(FileSFXI *file, FileMap *map);

  void IXS__FileSFXI__delete_00413c10(LPVOID obj);
  void IXS__FileSFXI__noop_2param_00413be0(uint param_1, uint param_2);

  static IXS__FileSFXI__VFTABLE IXS_FILESFXI_VFTAB_430e94 = {
          IXS__FileSFXI__write_0x413bf0,
          IXS__FileSFXI__newMapSFXI0_00413b80,
          IXS__FileSFXI__newMapSFXI1_00413b50,
          IXS__FileSFXI__noop_2param_00413be0,
          IXS__FileSFXI__delete_00413c10
  };

  void IXS__FileSFXI__noop_2param_00413be0(uint param_1, uint param_2) {
    // was originally shared with "File"
  }

  void IXS__FileSFXI__delete_00413c10(LPVOID obj) {
    // was originally shared with "File"
    operator delete(obj);
  }

  FileSFXI *__thiscall IXS__FileSFXI__ctor_0x430e94(FileSFXI *file, BufSFXI *buffer) {
    file->vftptr_0x0 = &IXS_FILESFXI_VFTAB_430e94;
    file->buffer_0x4 = buffer;
    return file;
  }


  FileMapSFXI *IXS__FileSFXI__newMapSFXI1_00413b50(FileSFXI *file, char *filename) {
    FileMapSFXI *map = (FileMapSFXI *) malloc(sizeof(FileMapSFXI));
    if (map != (FileMapSFXI *) nullptr) {
      map = IXS__FileMapSFXI__ctor_00413900(map, filename, file->buffer_0x4, 1);
      return map;
    }
    return (FileMapSFXI *) nullptr;
  }

  // opens a new file for writing
  FileMapSFXI *IXS__FileSFXI__newMapSFXI0_00413b80(FileSFXI *file, char *filename) {
    FileMapSFXI *map = (FileMapSFXI *) malloc(sizeof(FileMapSFXI));
    if (map == (FileMapSFXI *) nullptr) {
      fprintf(stderr, "out of memory - crash imminent\n");
      return (FileMapSFXI *) nullptr;
    } else {
      map = IXS__FileMapSFXI__ctor_00413900(map, filename, file->buffer_0x4, 0);
    }
    if (map->baseAddr_0x4 != (SegmentHeadSFXI *) nullptr) {
      return map;
    }
    IXS__FileMapSFXI__ctor_004139c0(map);
    operator delete(map);

    return (FileMapSFXI *) nullptr;
  }


  uint IXS__FileSFXI__write_0x413bf0(FileSFXI *file, FileMap *map) {
    uint bytesWritten = (*map->vftable->writeFile)(map, file->buffer_0x4, (uint) file->buffer_0x4->currentMemPtr_0x4);
    return bytesWritten;
  }


  FileSFXI *IXS__FileSFXI__newFileSFXI_00413c20(BufSFXI *fileBuf) {
    FileSFXI *file;

    if (fileBuf->magic_0x0 != 0x49584653) {
      fileBuf->magic_0x0 = 0x49584653;    // magic "SFXI"
      fileBuf->segmtCount_0x8 = 0;
      fileBuf->currentMemPtr_0x4 = (byte *) nullptr;
    }
    file = (FileSFXI *) malloc(sizeof(FileSFXI));
    if (file != (FileSFXI *) nullptr) {
      file = IXS__FileSFXI__ctor_0x430e94(file, fileBuf);
      return file;
    }
    return (FileSFXI *) nullptr;
  }

  FileSFXI *IXS__FileSFXI__FUN_00413c70(FileMap *map) {
    BufSFXI *fileBuf;
    FileSFXI *file;

    fileBuf = (BufSFXI *) (*map->vftable->getMemBuffer)(map);
    file = IXS__FileSFXI__newFileSFXI_00413c20(fileBuf);
    return file;
  }
}
