/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/


#include "FileMap.h"

// rePlayer
#if 0//defined(EMSCRIPTEN) || defined(LINUX)
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h> // read/write
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace IXS {
  // "private methods"
  uint IXS__FileMap__readFile_004136e0(FileMap *map, void* buffer, uint nNumberOfBytesToRead);

  uint IXS__FileMap__writeFile_00413710(FileMap *map, void *buffer, uint nNumberOfBytesToWrite);

  int IXS__FileMap__getFileSize_00413740(FileMap *map);

  uint IXS__FileMap__getFilePtr_00413760(FileMap *map);

  void IXS__FileMap__setFilePtr_00413780(FileMap *map, long lDistanceToMove, uint dwMoveMethod);

  byte *IXS__FileMap__getMemBuffer_004137c0(FileMap *map);

  void IXS__FileMap__delete_00413830(FileMap *map);

  static IXS__FileMap__VFTABLE IXS_FILEMAP_VFTAB_00430e40 = {
          IXS__FileMap__readFile_004136e0,
          IXS__FileMap__writeFile_00413710,
          IXS__FileMap__setFilePtr_00413780,
          IXS__FileMap__getFileSize_00413740,
          IXS__FileMap__getFilePtr_00413760,
          IXS__FileMap__setPos_004137a0,
          IXS__FileMap__getMemBuffer_004137c0,
          IXS__FileMap__delete_00413830
  };

// rePlayer
#if 1//!defined(EMSCRIPTEN) && !defined(LINUX)

  FileMap *__thiscall IXS__FileMap__ctor_00413620(FileMap *map, char *fileName, int mode) {
    HANDLE fHandle;


    map->vftable = &IXS_FILEMAP_VFTAB_00430e40;
    if (mode == 0) {
      // HANDLE CreateFileA(
      //   [in]           char*                lpFileName,
      //   [in]           uint                 dwDesiredAccess,
      //   [in]           uint                 dwShareMode,
      //   [in, optional] LPSECURITY_ATTRIBUTES lpSecurityAttributes,
      //   [in]           uint                 dwCreationDisposition,
      //   [in]           uint                 dwFlagsAndAttributes,
      //   [in, optional] HANDLE                hTemplateFile
      // );

      fHandle = CreateFileA(fileName, 0xc0000000, 1, (LPSECURITY_ATTRIBUTES) nullptr,
                            OPEN_EXISTING, 0x80, (HANDLE) nullptr);
      map->fileHandle = fHandle;
      if (fHandle != INVALID_HANDLE_VALUE) {
        map->memAddr = (LPVOID) nullptr;
        map->mapHandle = (HANDLE) nullptr;
        return map;
      }
      fHandle = CreateFileA(fileName, 0x80000000, 1, (LPSECURITY_ATTRIBUTES) nullptr,
                            OPEN_EXISTING, 0x80, (HANDLE) nullptr);
    } else {
      fHandle = CreateFileA(fileName, 0xc0000000, 1, (LPSECURITY_ATTRIBUTES) nullptr,
                            OPEN_ALWAYS, 0x80, (HANDLE) nullptr);
    }
    map->fileHandle = fHandle;
    map->memAddr = (LPVOID) nullptr;
    map->mapHandle = (HANDLE) nullptr;
    return map;
  }


  void __fastcall IXS__FileMap__dtor_004136a0(FileMap *map) {
    map->vftable = &IXS_FILEMAP_VFTAB_00430e40;
    if (map->mapHandle != (HANDLE) nullptr) {
      CloseHandle(map->mapHandle);
    }
    if (map->memAddr != (void *) nullptr) {
      UnmapViewOfFile(map->memAddr);
    }
    CloseHandle(map->fileHandle);
  }


  uint IXS__FileMap__readFile_004136e0(FileMap *map, LPVOID buffer, uint nNumberOfBytesToRead) {
    // boolean ReadFile(
    //   [in]                HANDLE       hFile,
    //   [out]               LPVOID       lpBuffer,
    //   [in]                uint        nNumberOfBytesToRead,
    //   [out, optional]     LPuint      lpNumberOfBytesRead,
    //   [in, out, optional] LPOVERLAPPED lpOverlapped
    // );
    //
    ReadFile(map->fileHandle, buffer, nNumberOfBytesToRead, (LPDWORD) &nNumberOfBytesToRead, (LPOVERLAPPED) nullptr);
    return nNumberOfBytesToRead;
  }


  uint IXS__FileMap__writeFile_00413710(FileMap *map, void *buffer, uint nNumberOfBytesToWrite) {
    WriteFile(map->fileHandle, buffer, nNumberOfBytesToWrite, (LPDWORD) &nNumberOfBytesToWrite,
              (LPOVERLAPPED) nullptr);
    return nNumberOfBytesToWrite;
  }


  int IXS__FileMap__getFileSize_00413740(FileMap *map) {
    uint s = GetFileSize(map->fileHandle, (LPDWORD) nullptr);
    return s;
  }


  uint IXS__FileMap__getFilePtr_00413760(FileMap *map) {
    uint DVar1 = SetFilePointer(map->fileHandle, 0, (long*) nullptr, 1);
    return DVar1;
  }


  void IXS__FileMap__setFilePtr_00413780(FileMap *map, long lDistanceToMove, uint dwMoveMethod) {
    SetFilePointer(map->fileHandle, lDistanceToMove, (long*) nullptr, dwMoveMethod);
  }


  void IXS__FileMap__setPos_004137a0(FileMap *map, long pos) {
    (*map->vftable->setFilePtr)(map, pos, 0);
  }


  byte *IXS__FileMap__getMemBuffer_004137c0(FileMap *map) {
    HANDLE pvVar1;
    byte *memAddr;

    pvVar1 = CreateFileMappingA(map->fileHandle, (LPSECURITY_ATTRIBUTES) nullptr, 0x8000004, 0, 0, (char *) nullptr);
    map->mapHandle = pvVar1;
    if (pvVar1 == (HANDLE) nullptr) {
      pvVar1 = CreateFileMappingA(map->fileHandle, (LPSECURITY_ATTRIBUTES) nullptr, 0x8000002, 0, 0, (char *) nullptr
      );
      map->mapHandle = pvVar1;
      memAddr = (byte *) MapViewOfFile(pvVar1, 4, 0, 0, 0);
      map->memAddr = memAddr;
      return memAddr;
    }
    memAddr = (byte *) MapViewOfFile(pvVar1, 2, 0, 0, 0);
    map->memAddr = memAddr;
    return memAddr;
  }
#else

  FileMap *__thiscall IXS__FileMap__ctor_00413620(FileMap *map, char *fileName, int mode) {

    HANDLE fHandle;
    mode_t mode2 = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    map->vftable = &IXS_FILEMAP_VFTAB_00430e40;
    if (mode == 0) {
      // read file
      fHandle = open(fileName, O_RDWR, mode2);

      map->fileHandle = fHandle;
      if (fHandle != (HANDLE) 0xffffffff) {
        // this one is usually used..
        map->memAddr = (LPVOID) nullptr;
        map->mapHandle = (HANDLE) nullptr;
        return map;
      }
      fHandle = open(fileName, O_RDONLY, mode2);
    } else {
      // save file
      fHandle = open(fileName, O_WRONLY | O_CREAT, mode2);

    }
    map->fileHandle = fHandle;
    map->memAddr = (LPVOID) nullptr;
    map->mapHandle = (HANDLE) nullptr;
    return map;
  }


  void __fastcall IXS__FileMap__dtor_004136a0(FileMap *map) {
    map->vftable = &IXS_FILEMAP_VFTAB_00430e40;

    int size= IXS__FileMap__getFileSize_00413740(map);

//    if (map->mapHandle != (HANDLE) nullptr) {
//      CloseHandle(map->mapHandle);
//    }
    if (map->memAddr != (void *) nullptr) {
      munmap(map->memAddr, size);
    }
    close(map->fileHandle);
  }


  uint IXS__FileMap__readFile_004136e0(FileMap *map, LPVOID buffer, uint nNumberOfBytesToRead) {
     return read(map->fileHandle, buffer, nNumberOfBytesToRead);
  }


  uint IXS__FileMap__writeFile_00413710(FileMap *map, void *buffer, uint nNumberOfBytesToWrite) {
    return write(map->fileHandle, buffer, nNumberOfBytesToWrite);
  }


  int IXS__FileMap__getFileSize_00413740(FileMap *map) {
    struct stat sb;
    if (fstat(map->fileHandle, &sb) == -1) {
//      fprintf(stderr, "error: fstat failed\n");
      return -1;
    }
    return sb.st_size;
  }


#if defined(EMSCRIPTEN) || defined(LINUX)
  uint mapDw2Whence(uint dwMoveMethod) {
    // Microsaft:
//      FILE_BEGIN 0
//      FILE_CURRENT 1
//      FILE_END 2
// VS: <unistd.h>
//    If whence is SEEK_SET, the file offset shall be set to offset bytes.
//    If whence is SEEK_CUR, the file offset shall be set to its current location plus offset.
//    If whence is SEEK_END, the file offset shall be set to the size of the file plus offset.
    int whence;
    switch(dwMoveMethod) {  // dwMoveMethod might already be identical to whence
      case 0:
        whence = SEEK_SET;
        break;
      case 1:
        whence = SEEK_CUR;
        break;
      case 2:
        whence = SEEK_END;
        break;
      default:
        fprintf(stderr, "error: invalid seek mode\n");
    }
    return whence;
  }
#endif

  uint IXS__FileMap__getFilePtr_00413760(FileMap *map) {
    return lseek(map->fileHandle, 0, mapDw2Whence(1));
  }


  void IXS__FileMap__setFilePtr_00413780(FileMap *map, long lDistanceToMove, uint dwMoveMethod) {
    lseek(map->fileHandle, lDistanceToMove, mapDw2Whence(dwMoveMethod));
  }


  void IXS__FileMap__setPos_004137a0(FileMap *map, long pos) {
    (*map->vftable->setFilePtr)(map, pos, 0);
  }


  byte *IXS__FileMap__getMemBuffer_004137c0(FileMap *map) {
    map->mapHandle = 0; // unused

    uint size= IXS__FileMap__getFileSize_00413740(map);

    // read/write
//    handle = CreateFileMappingA(map->fileHandle, (LPSECURITY_ATTRIBUTES) nullptr, 0x8000004, 0, 0, (char *) nullptr);
    void *memAddr = mmap(NULL,size, PROT_READ|PROT_WRITE,MAP_PRIVATE, map->fileHandle,0);
    if(memAddr == MAP_FAILED) {
      // readonly
//      handle = CreateFileMappingA(map->fileHandle, (LPSECURITY_ATTRIBUTES) nullptr, 0x8000002, 0, 0, (char *) nullptr);
      memAddr = mmap(NULL,size, PROT_READ,MAP_PRIVATE, map->fileHandle,0);
      if(memAddr == MAP_FAILED) {
        map->memAddr = nullptr;
        return nullptr;
      }
    }
//      memAddr = (byte *) MapViewOfFile(handle, 4, 0, 0, 0);
    map->memAddr = (byte*)memAddr;
    return (byte*)memAddr;
  }


#endif

  void IXS__FileMap__delete_00413830(FileMap *map) {
    if (map != (FileMap *) nullptr) {
      IXS__FileMap__dtor_004136a0(map);
      operator delete(map);
    }
  }
}
