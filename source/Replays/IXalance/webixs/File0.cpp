/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#include "File0.h"

#if defined(EMSCRIPTEN) || defined(LINUX)
#include <stdio.h>
#include <stdlib.h>
// rePlayer
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace IXS {
  // "private methods"
  FileMap *IXS__File__newFileMap_00413850(File0 *unused, char *fileName);

  FileMap *IXS__File__initFileMap_00413880(File0 *unused, char *fileName);

  void IXS__File__deleteFile_004138d0(File0 *unused, char *fileName);

  static IXS__File__VFTABLE IXS_FILEMAP0_VFTAB_00430e60 = {
          IXS__File__noop_2param_00413be0,
          IXS__File__initFileMap_00413880,
          IXS__File__newFileMap_00413850,
          IXS__File__deleteFile_004138d0,
          IXS__File__delete_00413c10
  };


  FileMap *IXS__File__newFileMap_00413850(File0 *unused, char *fileName) {
    FileMap *obj;

    obj = (FileMap *) malloc(sizeof(FileMap));
    if (obj != (FileMap *) nullptr) {
      obj = IXS__FileMap__ctor_00413620(obj, fileName, 1);
      return obj;
    }
    return (FileMap *) nullptr;
  }


  FileMap *IXS__File__initFileMap_00413880(File0 *unused, char *fileName) {
    FileMap *obj;

    obj = (FileMap *) malloc(sizeof(FileMap));
    if (obj == (FileMap *) nullptr) {
      obj = (FileMap *) nullptr;
    } else {
      obj = IXS__FileMap__ctor_00413620(obj, fileName, 0);
    }
    if (obj->fileHandle != INVALID_HANDLE_VALUE) {
      return obj;
    }
    if (obj != (FileMap *) nullptr) {
      IXS__FileMap__dtor_004136a0(obj);
      operator delete(obj);
    }
    return (FileMap *) nullptr;
  }


  void IXS__File__deleteFile_004138d0(File0 *unused, char *fileName) {
    // boolean DeleteFileA(
    //   [in] char* lpFileName
    // );

#if defined(EMSCRIPTEN) || defined(LINUX)
    remove(fileName);
#else
    DeleteFileA(fileName);
#endif
  }


  File0 *IXS__File__Z_ctor_FUN_004138e0() {
    File0 *obj;

    obj = (File0 *) malloc(sizeof(File0));
    if (obj != (File0 *) nullptr) {
      obj->vftable = &IXS_FILEMAP0_VFTAB_00430e60;
      return obj;
    }
    return (File0 *) nullptr;
  }


  void IXS__File__noop_2param_00413be0(uint param_1, uint param_2) {
  }

  void IXS__File__delete_00413c10(LPVOID obj) {
    operator delete(obj);
  }

}
