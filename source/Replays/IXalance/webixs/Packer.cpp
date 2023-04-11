/*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/


#include "Packer.h"

#include <stdlib.h>
#include <string.h>


namespace IXS {
  // "private methods"
  void IXS__Packer__noop_3param_0x40cdf0(uint param_1, uint param_2, uint param_3);

  void IXS__Packer__noop_1param_0x40cbd0(uint param_1);

  ByteArray *IXS__Packer__deflate_0x40ca70(Packer *packer, ByteArray *in, int level);

  ByteArray *IXS__Packer__inflate_0x40cbe0(Packer *packer, ByteArray *in);

  int __thiscall IXS__Packer__writeDestBuffer_0x40cd30(Packer *packer);

  uint __thiscall IXS__Packer__readSrcBuffer_0x40cd90(Packer *packer);

  double __thiscall IXS__Packer__completion_0x40ce00(Packer *packer);


  ByteArray *__thiscall IXS__ByteArray__Z_ctor_0x40f050(ByteArray *byteArray) {
    byteArray->bufPtr_0x0 = (int *) nullptr;
    byteArray->bufSize_0x4 = 0;
    return byteArray;
  }


  void __thiscall IXS__ByteArray__Z_resize_0x40f060(ByteArray *byteArray, size_t size) {
    int *buf0;
    int *buf;
	
    if (byteArray->bufPtr_0x0 == (int *) nullptr) {
      buf0 = (int *) malloc(size);
      byteArray->bufSize_0x4 = size;
      byteArray->bufPtr_0x0 = buf0;
	  
    } else if ((size != 0) && (size != byteArray->bufSize_0x4)) {
      buf = (int *) realloc(byteArray->bufPtr_0x0, size);
      if (buf != (int *) nullptr) {
        byteArray->bufSize_0x4 = size;
        byteArray->bufPtr_0x0 = buf;
      }
    }
  }


  static IXS__Packer__VFTABLE IXS_PACKER_VFTABLE_4300a8 = {
          IXS__Packer__deflate_0x40ca70,
          IXS__Packer__inflate_0x40cbe0,
          IXS__Packer__noop_3param_0x40cdf0,
          IXS__Packer__noop_1param_0x40cbd0
  };

  void __thiscall IXS__Packer__dtor_0x40ca60(Packer *packer) {
    packer->vftptr_0x0 = &IXS_PACKER_VFTABLE_4300a8;
  }


  ByteArray *IXS__Packer__deflate_0x40ca70(Packer *packer, ByteArray *in, int level) {
    char cVar1;
    ByteArray *bytesArray;
    int iVar2;
    uint uVar3;
    double dVar4;
////  z_streamp piVar1;
//    IXS__Packer__VFTABLE *vftable;
    z_stream *z;

    z = &packer->zstream_0x4;
    packer->byteArray_0x44 = in;
    (packer->zstream_0x4).avail_in = 0;
    z->next_in = packer->destBuffer4096_0x54;
    packer->int_0x4c = 0;
    bytesArray = (ByteArray *) malloc(sizeof(ByteArray));

    if (bytesArray == (ByteArray *) nullptr) {
//    bytesArray = (ByteArray *) nullptr;
    } else {
      bytesArray = IXS__ByteArray__Z_ctor_0x40f050(bytesArray);
    }
    packer->byteArray_0x48 = bytesArray;
    (packer->zstream_0x4).avail_out = 0x1000;
    (packer->zstream_0x4).next_out = packer->srcBuffer4096_0x1054;
    packer->lenCount_0x50 = 0;
    packer->zlib_status_0x3c = 0;
    packer->abortFlag_0x40 = '\0';
    deflateInit_(z, level, ZLIB_VERSION, sizeof(z_stream));
    if (packer->abortFlag_0x40 == '\0') {
      do {
        iVar2 = IXS__Packer__writeDestBuffer_0x40cd30(packer);
        if (iVar2 == 0) break;
        uVar3 = deflate(z, 0);
        packer->zlib_status_0x3c = uVar3;
        IXS__Packer__readSrcBuffer_0x40cd90(packer);
        if (packer->zlib_status_0x3c != 0) break;
//        vftable = packer->vftptr_0x0;
        dVar4 = IXS__Packer__completion_0x40ce00(packer);
        //     (*vftable->fn3_maybeTraceLog)((uint)obj,SUB84(dVar4,0),(uint)((uint64)dVar4 >> 0x20));
      } while (packer->abortFlag_0x40 == '\0');
      cVar1 = packer->abortFlag_0x40;
      while (cVar1 == '\0') {
        uVar3 = deflate(z, 4);
        packer->zlib_status_0x3c = uVar3;
        uVar3 = IXS__Packer__readSrcBuffer_0x40cd90(packer);
        if ((uVar3 == 0) || (packer->zlib_status_0x3c != 0)) break;
        cVar1 = packer->abortFlag_0x40;
      }
    }
//    vftable = packer->vftptr_0x0;
    dVar4 = IXS__Packer__completion_0x40ce00(packer);
//  (*vftable->fn3_maybeTraceLog)((uint)obj,SUB84(dVar4,0),(uint)((uint64)dVar4 >> 0x20));

    deflateEnd(z);
    bytesArray = packer->byteArray_0x48;
    return bytesArray;
  }


  void IXS__Packer__noop_1param_0x40cbd0(uint param_1) {
  }


  ByteArray *IXS__Packer__inflate_0x40cbe0(Packer *packer, ByteArray *in) {
    ByteArray *byteArray;
    uint writtenBytes;
    uint zStatus;
    double dVar1;
    char abortFlag;
//    IXS__Packer__VFTABLE *vfTab;
    z_stream *z;

    z = &packer->zstream_0x4;
    packer->byteArray_0x44 = in;
    (packer->zstream_0x4).avail_in = 0;
    z->next_in = packer->destBuffer4096_0x54;
    packer->int_0x4c = 0;
    byteArray = (ByteArray *) malloc(sizeof(ByteArray));
    if (byteArray == (ByteArray *) nullptr) {
      byteArray = (ByteArray *) nullptr;
    } else {
      byteArray = IXS__ByteArray__Z_ctor_0x40f050(byteArray);
    }
    packer->byteArray_0x48 = byteArray;
    (packer->zstream_0x4).avail_out = 0x1000;
    (packer->zstream_0x4).next_out = packer->srcBuffer4096_0x1054;
    packer->lenCount_0x50 = 0;
    packer->zlib_status_0x3c = 0;
    packer->abortFlag_0x40 = '\0';
    inflateInit_
            (z, ZLIB_VERSION, sizeof(z_stream));
    if (packer->abortFlag_0x40 == '\0') {
      do {
        writtenBytes = IXS__Packer__writeDestBuffer_0x40cd30(packer);
        if (writtenBytes == 0) break;
        zStatus = inflate(z, 0);
        packer->zlib_status_0x3c = zStatus;
        IXS__Packer__readSrcBuffer_0x40cd90(packer);
        if (packer->zlib_status_0x3c != 0) break;
//        vfTab = packer->vftptr_0x0;
        dVar1 = IXS__Packer__completion_0x40ce00(packer);
      } while (packer->abortFlag_0x40 == '\0');
      abortFlag = packer->abortFlag_0x40;
      while (abortFlag == '\0') {
        zStatus = inflate(z, 4);
        packer->zlib_status_0x3c = zStatus;
        zStatus = IXS__Packer__readSrcBuffer_0x40cd90(packer);
        if ((zStatus == 0) || (packer->zlib_status_0x3c != 0)) break;
        abortFlag = packer->abortFlag_0x40;
      }
    }
//    vfTab = packer->vftptr_0x0;
    dVar1 = IXS__Packer__completion_0x40ce00(packer);
    inflateEnd(z);
    byteArray = packer->byteArray_0x48;
    return byteArray;
  }


  int __thiscall IXS__Packer__writeDestBuffer_0x40cd30(Packer *packer) {
    uint i;
    byte *src;
    byte *dest;
    ByteArray *byteArray;
    uint len;
    int offset;

    if ((packer->zstream_0x4).avail_in == 0) {
      byteArray = packer->byteArray_0x44;
      offset = packer->int_0x4c;
	  	  
     (packer->zstream_0x4).next_in = packer->destBuffer4096_0x54;
      len = byteArray->bufSize_0x4 - offset;
      (packer->zstream_0x4).avail_in = len;
      if (0x1000 < len) {
        (packer->zstream_0x4).avail_in = 0x1000;
      }
      len = (packer->zstream_0x4).avail_in;
      src = (byte *) ((uintptr_t) byteArray->bufPtr_0x0 + offset);
      dest = (byte *) packer->destBuffer4096_0x54;

      memcpy(dest, src, len);

      len = (packer->zstream_0x4).avail_in;
      packer->int_0x4c = packer->int_0x4c + len;
      return len;
    }
    return (packer->zstream_0x4).avail_in;
  }


  uint __thiscall IXS__Packer__readSrcBuffer_0x40cd90(Packer *packer) {
    uint i;
    uint len;
    byte *src;
    byte *dest;
    uint oldLen;

    len = 0x1000 - (packer->zstream_0x4).avail_out;
    if (len != 0) {
      IXS__ByteArray__Z_resize_0x40f060(packer->byteArray_0x48, packer->byteArray_0x48->bufSize_0x4 + len);
      src = packer->srcBuffer4096_0x1054;
      dest = (byte *) ((uintptr_t) packer->byteArray_0x48->bufPtr_0x0 + packer->lenCount_0x50);

      memcpy(dest, src, len);

      oldLen = packer->lenCount_0x50;
      (packer->zstream_0x4).next_out = packer->srcBuffer4096_0x1054;
      packer->lenCount_0x50 = oldLen + len;
      (packer->zstream_0x4).avail_out = 0x1000;
      return len;
    }
    return 0;
  }


// called before "inflateEnd" - maybe an unused hook for debug trace output logging
  void IXS__Packer__noop_3param_0x40cdf0(uint param_1, uint param_2, uint param_3) {
  }


  double __thiscall IXS__Packer__completion_0x40ce00(Packer *packer) {
    uint bufSize;

    bufSize = packer->byteArray_0x44->bufSize_0x4;
    if (bufSize == 0) {
      return 1.0L;
    }
    return (double) (uint64) (packer->zstream_0x4).total_in / (double) bufSize;
  }


  ByteArray *IXS__Packer__newByteArray_0040ce40() {
    ByteArray *byteArray = (ByteArray *) malloc(sizeof(ByteArray));

    if (byteArray != (ByteArray *) nullptr) {
      byteArray = IXS__ByteArray__Z_ctor_0x40f050(byteArray);
      return byteArray;
    }
    return (ByteArray *) 0;
  }


  Packer *IXS__Packer__ctor_0040cea0() {
    Packer *obj;

    obj = (Packer *) malloc(sizeof(Packer));
    if (obj != (Packer *) nullptr) {
      obj->vftptr_0x0 = &IXS_PACKER_VFTABLE_4300a8;
      (obj->zstream_0x4).zalloc = nullptr;
      (obj->zstream_0x4).zfree = nullptr;
      (obj->zstream_0x4).opaque = (void *) nullptr;
      return obj;
    }
    return (Packer *) nullptr;
  }
}
