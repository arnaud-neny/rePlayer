/*
* Base class for memory mapped file handling.
*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

// todo: refactor to original C++ class def

#ifndef IXS_FILEMAP_H
#define IXS_FILEMAP_H

#if !defined(EMSCRIPTEN) && !defined(LINUX)
// Microsoft garbage seems to need these includes here or the dumbshit compiler
// will fail with some useless error message..

#include <Windows.h>
#include <stdlib.h>
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "ws2_32.lib")

#include <winbase.h>
#include <memoryapi.h>
#include <handleapi.h>
#include <fileapi.h>

#else

#endif

#include "basetypes.h"


namespace IXS {

  typedef struct IXS__FileMap__VFTABLE IXS__FileMap__VFTABLE;
  struct IXS__FileMap__VFTABLE {
    uint (* readFile)(struct FileMap *, void*, uint);
    uint (* writeFile)(struct FileMap *, void*, uint);
    void (* setFilePtr)(struct FileMap *, long, uint);
    int (* getFileSize)(struct FileMap *);
    uint (* getFilePtr)(struct FileMap *);
    void (* virtFn)(struct FileMap *, long);
    byte * (* getMemBuffer)(struct FileMap *);
    void (* delete0)(struct FileMap *);
  };

  typedef struct FileMap FileMap;
  struct FileMap {
    struct IXS__FileMap__VFTABLE * vftable;
    HANDLE fileHandle;
    HANDLE mapHandle;
    LPVOID memAddr;
  };

  // "public methods"
  FileMap * __thiscall IXS__FileMap__ctor_00413620(FileMap *map, char* fileName, int mode);
  void __fastcall IXS__FileMap__dtor_004136a0(FileMap *map);

  // "pritected methods"
  void IXS__FileMap__setPos_004137a0(FileMap *map, long pos);

}

#endif //IXS_FILEMAP_H
