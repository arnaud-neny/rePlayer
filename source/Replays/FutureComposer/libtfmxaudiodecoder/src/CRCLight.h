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

#ifndef CRCLIGHT_H
#define CRCLIGHT_H

#include "MyTypes.h"
#include "SmartPtr.h"

namespace tfmxaudiodecoder {

class CRCLight {
public:
    udword get(smartPtr<const ubyte> ptr, udword offset, udword len);
    
private:
    static const udword crc32tab[];
};

}  // namespace

#endif  // CRCLIGHT_H
