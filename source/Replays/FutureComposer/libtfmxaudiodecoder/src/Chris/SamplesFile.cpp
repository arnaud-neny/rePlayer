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

#include "TFMXDecoder.h"

// Support for loading the separate file containing the samples.
// NB! There is some code duplication in the DNS decoder, but moving
// it into the decoder base class is not worthwhile yet.

#include <cstring>
//#include <fstream> // rePlayer
using namespace std;

bool tfmxaudiodecoder::TFMXDecoder::loadSamplesFile() {
    // Got both the MDAT and SMPL file?
    if ( input.smplLoaded ) {  // if loaded before, reuse it
        input.mdatSize = input.mdatSizeCurrent;
        input.smplSize = input.smplSizeCurrent;
        return true;
    }
    if ( input.path.empty() ) {
        return false;
    }

    typedef std::pair<std::string,std::string> ExtPair;
    std::vector<ExtPair> vExtPairs = {
        { ".tfx", ".sam" },
        { ".TFX", ".SAM" },
        { ".mdat", ".smpl" },
        { ".MDAT", ".SMPL" },
        { "mdat.", "smpl." },
        { "MDAT.", "SMPL." }
    };
    // since C++17 could use std::filesystem::path::replace_extension
    for ( std::vector<ExtPair>::iterator it = vExtPairs.begin(); it != vExtPairs.end(); ++it ) {
        std::string pathSmpl = input.path;
        std::size_t found = pathSmpl.rfind(it->first);
        if (found != std::string::npos) {
            pathSmpl.replace(found,it->first.length(),it->second);
            if (loadSamplesFile(pathSmpl)) {
                return true;
            }
        }
    }  // next ExtPair

    // Oh, the irony! In the Hippel decoder we wanted to avoid looking for
    // the extremely rare and poorly named "smp.set" and "hipc.samp" files,
    // because they exist only in copies of ancient collections and have
    // become obsolete. And now this! A single game soundtrack in TFMX format
    // has been ripped with a single "smpl.set" file to be reused by all of
    // its short 90 MDAT files.

    std::vector<std::string> vSetNames = {
        "smpl.set",  // as used by the ExoticA modules collection
        "SMPL.set"   // Wanted Team EaglePlayers EP_TFMX.readme
    };
    for ( std::vector<std::string>::iterator it = vSetNames.begin(); it != vSetNames.end(); ++it ) {
        std::string pathSmpl = input.path;
        // since C++17 could use std::filesystem::path::filename
        std::size_t found = pathSmpl.find_last_of("/\\");
        if (found != std::string::npos) {
            pathSmpl.erase(found+1);
            pathSmpl.append(*it);
            if (loadSamplesFile(pathSmpl)) {
                return true;
            }
        }
    }  // next sample set filename
    return false;
}

bool tfmxaudiodecoder::TFMXDecoder::loadSamplesFile(const std::string &pathSmpl) {
#if 0 // rePlayer begin
    ifstream fIn( pathSmpl, ios::in|ios::binary|ios::ate );
    if ( !fIn.is_open() ) {
        return false;
    }
    //fIn.seekg(0,ios::end);
    uint32_t fLen = fIn.tellg();
    fIn.seekg(0,ios::beg);

    char* newInputBuf = new char[fLen+input.bufLen];
    memcpy(newInputBuf,input.buf,input.bufLen);
    delete[] input.buf;
    input.buf = reinterpret_cast<ubyte*>(newInputBuf);

    fIn.read(newInputBuf+input.len,fLen);
    if (fIn.bad()) {
        return false;
    }
    fIn.close();
    input.smplSize = fLen;
    input.mdatSize = input.len - offsets.header;
    offsets.sampleData = input.len;
    input.bufLen += input.smplSize;
    input.len += input.smplSize;

    // Update smart pointers.
    pBuf.setBuffer(input.buf,input.bufLen);
    return true;
#else
    auto res = input.loader(input.loaderData, pathSmpl.c_str());
    if (!res.first)
		return false;

    char* newInputBuf = new char[res.second + input.bufLen];
    memcpy(newInputBuf, input.buf, input.bufLen);
    delete[] input.buf;
    input.buf = reinterpret_cast<ubyte*>(newInputBuf);

    memcpy(newInputBuf + input.len, res.first, res.second);
    delete[] res.first;

    input.smplSize = res.second;
    input.mdatSize = input.len - offsets.header;
    offsets.sampleData = input.len;
    input.bufLen += input.smplSize;
    input.len += input.smplSize;

    // Update smart pointers.
    pBuf.setBuffer(input.buf, input.bufLen);
    return true;
#endif // rePlayer end
}
