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
// more into the decoder base class is not worthwhile yet.

#include <cstring>
//#include <fstream> // rePlayer
using namespace std;

bool tfmxaudiodecoder::TFMXDecoder::loadSamplesFile() {
    if ( path.empty() ) {
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
        std::string pathSmpl = path;
        std::size_t found = pathSmpl.rfind(it->first);
        if (found != std::string::npos) {
            pathSmpl.replace(found,it->first.length(),it->second);
            if (appendSamplesFile( pathSmpl ) ) {
#if defined(DEBUG)
                cout << "Samples loaded: " << pathSmpl << endl;
#endif
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
        std::string pathSmpl = path;
        // since C++17 could use std::filesystem::path::filename
        std::size_t found = pathSmpl.find_last_of("/\\");
        if (found != std::string::npos) {
            pathSmpl.erase(found+1);
            pathSmpl.append(*it);
            if (appendSamplesFile( pathSmpl ) ) {
#if defined(DEBUG)
                cout << "Samples loaded: " << pathSmpl << endl;
#endif
                return true;
            }
        }
    }  // next sample set filename
    return false;
}
