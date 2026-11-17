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

#include "HippelDecoder.h"

// Support for loading the separate file containing the samples.
// NB! There is some code duplication in the other decoders, but moving
// more into the decoder base class is not worthwhile yet.

#include <cstring>
//#include <fstream> // rePlayer
using namespace std;

bool tfmxaudiodecoder::HippelDecoder::loadSamplesFile() {
    if ( path.empty() ) {
        return false;
    }

    // Oh, the irony! In the Hippel decoder we wanted to avoid looking for
    // the extremely rare and poorly named "smp.set" and "hipc.samp" files,
    // because they exist only in copies of ancient collections and have
    // become obsolete. And now this! A single game soundtrack in TFMX format
    // has been ripped with a single "smpl.set" file to be reused by all of
    // its short 90 MDAT files.

    std::vector<std::string> vSetNames = {
        "SMP.set",   // Wanted Team rips (e.g. Amberstar)
        "smp.set"
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
    
    std::vector<std::string> vExtNames = {
        "samp", "SAMP"  // ExoticA old
    };
    for ( std::vector<std::string>::iterator it = vExtNames.begin(); it != vExtNames.end(); ++it ) {
        std::string pathSmpl = path;
        std::size_t found = pathSmpl.find_last_of(".");
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
    }  // next sample set extension

    return false;
}
