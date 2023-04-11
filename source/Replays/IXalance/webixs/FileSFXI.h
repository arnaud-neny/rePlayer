/*
* A subclass of FileHelper used to tmp "sample cache file".
*
*   SFXI file contains a sequence of named sample-recordings (usually separate samples for left/right
*   channel). The file is cached in the %TEMP% folder and only re-generated if not found.
*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#ifndef IXS_FILESFXI_H
#define IXS_FILESFXI_H

#include "FileMapSFXI.h"

namespace IXS {

  typedef struct IXS__FileSFXI__VFTABLE IXS__FileSFXI__VFTABLE;
  struct IXS__FileSFXI__VFTABLE {
    uint (*virtWrite)(struct FileSFXI *, struct FileMap *);

    FileMapSFXI *(*newMapSFXI)(struct FileSFXI *, char *);

    FileMapSFXI *(*newMapSFXI1)(struct FileSFXI *, char *);

    void (*noop)(uint, uint);

    void (*deleteObject)(LPVOID);
  };

  typedef struct FileSFXI FileSFXI;
  struct FileSFXI {
    struct IXS__FileSFXI__VFTABLE *vftptr_0x0;
    struct BufSFXI *buffer_0x4;
  };

  // "public methods"
  FileSFXI *IXS__FileSFXI__newFileSFXI_00413c20(BufSFXI *fileBuf);

  FileSFXI *IXS__FileSFXI__FUN_00413c70(FileMap *map);
}

#endif //IXS_FILESFXI_H
