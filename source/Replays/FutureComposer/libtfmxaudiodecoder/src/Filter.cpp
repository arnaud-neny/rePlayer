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

#include <cmath>
#include "Filter.h"

namespace tfmxaudiodecoder {

void Filter::setup(float fc, udword freq) {
    float oc = M_PI*fc/freq;
    float k = tan(oc);
    float d = 1.0 / (1.0 + sqrt(2.0) * k + k*k);
    a[0] = k*k*d;
    a[1] = 2.0*a[0];
    a[2] = a[0];
    b[0] = 2.0*(k*k-1.0)*d;
    b[1] = (1.0-sqrt(2.0)*k+k*k) * d;
    x[0] = x[1] = y[0] = y[1] = 0;
}

}  // namespace
