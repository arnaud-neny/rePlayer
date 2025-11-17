// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef DUMP_H
#define DUMP_H

#include "Config.h"
#include "MyTypes.h"
#include "SmartPtr.h"

#if defined(DEBUG) || defined(DEBUG_RUN)
#include <iomanip>
#include <iostream>
using std::cout;
using std::endl;
using std::dec;
using std::hex;
#include <sstream>
#include <string>
#endif  // DEBUG*

namespace tfmxaudiodecoder {

#if defined(DEBUG) || defined(DEBUG_RUN)
template<class T> std::string tohex( const T& val ) {
    std::ostringstream oss;
    oss << std::setw(sizeof(T)*2) << std::setfill('0') << std::hex << static_cast<long int>(val);
    return std::string( oss.str() );
}

std::string hexB( const ubyte& val );
std::string hexW( const uword& val );
#endif  // DEBUG*

void dumpBlocks(smartPtr<ubyte>& fcBuf, udword startOffset, sdword length, int blockLen);
void dumpLines(smartPtr<ubyte>& fcBuf, udword startOffset, sdword length, int blockLen, int index=-1);
void dumpBytes(smartPtr<ubyte>& fcBuf, udword startOffset, int n);
void dumpByte(int b);
void dumpTimestamp(udword ms);

}  // namespace

#endif  // DUMP_H
