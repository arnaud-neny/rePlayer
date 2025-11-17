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

#ifndef DECODERPROXY_H
#define DECODERPROXY_H

#include <string>

#include "MyTypes.h"
#include "PaulaVoice.h"
#include "LamePaulaMixer.h"

namespace tfmxaudiodecoder {

class Decoder;

class DecoderProxy {
 public:
    DecoderProxy();
    ~DecoderProxy();

    void setPath(const char*, LoaderCallback, void*); // rePlayer

    // The mandatory call to initialize the decoder with the music data
    // provided in the buffer. The first dry-run for a closer look at the
    // data and also the play duration is implicit and shall not be disabled. 
    bool init(void *buffer, udword bufferLen, int songNum);
    
    bool reinit(int);  // with new song number or current song (<0)

    void setMixer(LamePaulaMixer*);
    udword mixerFillBuffer(void* buffer, udword bufferLen);// rePlayer

    bool seek(sdword ms);

    bool load(const char* path, int songNum);

    // Optional, quick but shallow! Just a brief look at header tags.
    // A later init call may reject the same buffer contents (e.g. due
    // due finding errors or running into problems) and may change the
    // detected format, too.
    bool maybeOurs(void *buffer, udword bufferLen);
    
    udword getDuration();  // [ms] duration of initialized song
    bool getSongEndFlag(); // whether song end has been reached
    int getSongs();  // return number of (sub-)songs found
    void endShorts(int,int);
    void setLoopMode(int);

    const char* getFormatID();
    const char* getFormatName();

    // "artist", "title", "game" since some TFMX-MOD files provide these.
    const char* getInfoString(const std::string);
    int getVoices();

    int run();

 private:
    bool initDecoder(void*, udword, int);
    void mixerInit();
    
    Decoder *pDecoder;
    LamePaulaMixer *pMixer;

    int currentSong;
    udword endShortsDuration;
    bool endShortsMode;
    
    std::string path, ext;
    std::string formatName, formatID;

    LoaderCallback loader = nullptr; // rePlayer
    void* loaderData = nullptr; // rePlayer
};

}  // namespace

#endif  // DECODERPROXY_H
