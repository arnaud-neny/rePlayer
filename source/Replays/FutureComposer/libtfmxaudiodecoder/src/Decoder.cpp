// TFMX audio decoder library by Michael Schwendt

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, see <https://www.gnu.org/licenses/>.

#include "Decoder.h"

//#include <fstream> // rePlayer

namespace tfmxaudiodecoder {

const std::string Decoder::UNKNOWN_FORMAT_ID = "???";

Decoder::Decoder() {
    formatID = formatName = UNKNOWN_FORMAT_ID;
    duration = 0;
    songEnd = true;
    loopMode = false;
    rate = 50<<8;
    blacklisted = false;

    input.buf = 0;
    input.bufLen = input.len = 0;
}

Decoder::~Decoder() {
    delete[] input.buf;
}

void Decoder::seek(sdword ms) {
    restart();
    while (ms>=0) {
        ms -= run();
        if ( getSongEndFlag() ) {
            break;
        }
    };
}

const char* Decoder::getInfoString(const std::string which) {
    if (which == "artist") {
        return author.c_str();
    }
    else if (which == "title") {
        return title.c_str();
    }
    else if (which == "game") {
        return game.c_str();
    }
    else if (which == "name") {
        return name.c_str();
    }
    else {
        return 0;
    }
}

// rePlayer begin
void Decoder::setPath(std::string pathArg, LoaderCallback loaderArg, void* loaderDataArg) {
    path = pathArg;
    loader = loaderArg;
    loaderData = loaderDataArg;
// rePlayer end
}

// For accuracy reasons, play speed cannot be specified as integral value,
// since 8-bit fixed point arithmetics are used.
void Decoder::setRate(udword r) {  // in [Hz]*256
    rate = r;
    tickFP = (1000<<16)/rate;  // in [ms]*256
}

void Decoder::setBPM(uword bpm) {
    setRate( ((bpm<<8)*2)/5 );
}

void Decoder::bufferInputData(void *data, udword dataLen, udword excessLen) {
    udword bufLenNew = dataLen + excessLen;
    ubyte* newInputBuf = new ubyte[bufLenNew];
    memcpy(newInputBuf,data,dataLen);
    delete[] input.buf;
    input.buf = newInputBuf;
    input.bufLen = bufLenNew;
    input.len = dataLen;
}

bool Decoder::appendSamplesFile(const std::string &pathSmpl) {
#if 0
    std::ifstream fIn( pathSmpl, std::ios::in|std::ios::binary|std::ios::ate );
    if ( !fIn.is_open() ) {
        return false;
    }
    //fIn.seekg(0,std::ios::end);
    uint32_t fLen = fIn.tellg();
    fIn.seekg(0,std::ios::beg);

    udword bufLenNew = input.bufLen + fLen;
    char* newInputBuf = new char[bufLenNew];
    memcpy(newInputBuf,input.buf,input.bufLen);
    delete[] input.buf;
    input.buf = reinterpret_cast<ubyte*>(newInputBuf);
    input.bufLen = bufLenNew;

    fIn.read(newInputBuf+input.len,fLen);
    if (fIn.bad()) {
        return false;
    }
    fIn.close();

    input.len += fLen;  // total size of loaded/copied data
    input.smplSize = fLen;
    return true;
#else
    auto res = loader(loaderData, pathSmpl.c_str());
    if (res.first)
    {
        uint32_t fLen = res.second;

        udword bufLenNew = input.bufLen + fLen;
        char* newInputBuf = new char[bufLenNew];
        memcpy(newInputBuf, input.buf, input.bufLen);
        delete[] input.buf;
        input.buf = reinterpret_cast<ubyte*>(newInputBuf);
        input.bufLen = bufLenNew;

        memcpy(newInputBuf + input.len, res.first, fLen);

        input.len += fLen;  // total size of loaded/copied data
        input.smplSize = fLen;
        return true;
    }
    return false;
#endif
}

}  // namespace
