/*
* Base class for handling respective memory mapped files.
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

#ifndef IXS_FILE0_H
#define IXS_FILE0_H

#include "FileMap.h"

namespace IXS {
  typedef struct IXS__File__VFTABLE IXS__File__VFTABLE;

  // todo: the names originally chosen during API discovery here are not good and should be revisited..
  struct IXS__File__VFTABLE {
    void (*noop)(uint, uint);

    FileMap *(*initFileMap)(struct File0 *, char *);  // open existing file for reading

    FileMap *(*newFileMap)(struct File0 *, char *);   // create new file for writing

    void (*deleteFile)(struct File0 *, char *);

    void (*deleteObject)(LPVOID);
  };


  typedef struct File0 File0;
  struct File0 {
    struct IXS__File__VFTABLE *vftable;
  };

  // "public methods"
  File0 *IXS__File__Z_ctor_FUN_004138e0();

  // "protected methods"
  void IXS__File__delete_00413c10(LPVOID obj);

  void IXS__File__noop_2param_00413be0(uint param_1, uint param_2);

}

#endif //IXS_FILE0_H
