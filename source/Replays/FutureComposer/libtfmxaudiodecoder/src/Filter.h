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

#ifndef FILTER_H
#define FILTER_H

#include "MyTypes.h"

namespace tfmxaudiodecoder {

class Filter {
 public:
    void setup(float fc, udword freq);
    float process(float in);

 private:
    float a[3], b[2], x[2], y[2];
};

inline float Filter::process(float in) {
    float out = 0.75*in*a[0] + x[0]*a[1] + x[1]*a[2] - y[0]*b[0] - y[1]*b[1];
    x[1] = x[0];
    x[0] = in;
    y[1] = y[0];
    y[0] = out;
    return out;
}
 
}  // namespace

#endif  // FILTER_H
