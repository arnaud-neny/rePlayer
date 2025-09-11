// This is only an excerpt of a header originally from libsidplay.
// Since libfc14audiodecoder uses a smart pointer implementation,
// and all FC/TFMX modules used big-endian byte order, only the
// big-endian functions are needed.

#ifndef MYENDIAN_H
#define MYENDIAN_H

#include "Config.h"
#include "MyTypes.h"
#include "SmartPtr.h"

inline udword readBEudword(smartPtr<ubyte> ptr, udword offset) {
	return ( (((udword)ptr[offset])<<24) + (((udword)ptr[offset+1])<<16)
			+ (((udword)ptr[offset+2])<<8) + ((udword)ptr[offset+3]) );
}

inline udword readBEudword(smartPtr<const ubyte> ptr, udword offset) {
	return ( (((udword)ptr[offset])<<24) + (((udword)ptr[offset+1])<<16)
			+ (((udword)ptr[offset+2])<<8) + ((udword)ptr[offset+3]) );
}

inline uword readBEuword(smartPtr<ubyte> ptr, udword offset) {
    return ( (((uword)ptr[offset])<<8) + ((uword)ptr[offset+1]) );
}

inline uword readBEuword(smartPtr<const ubyte> ptr, udword offset) {
    return ( (((uword)ptr[offset])<<8) + ((uword)ptr[offset+1]) );
}


inline void writeBEdword(smartPtr<ubyte> ptr, udword offset, udword someDword) {
	ptr[offset+3] = (ubyte)(someDword&0xFF);
	ptr[offset+2] = (ubyte)((someDword>>8)&0xFF);
	ptr[offset+1] = (ubyte)((someDword>>16)&0xFF);
	ptr[offset+0] = (ubyte)((someDword>>24)&0xFF);
}

inline void writeBEword(smartPtr<ubyte> ptr, udword offset, uword someWord) {
	ptr[offset+1] = (ubyte)(someWord&0xFF);
	ptr[offset+0] = (ubyte)(someWord>>8);
}

#endif  // MYENDIAN_H
