/*
    HQ DSD->PCM converter 88.2/96 kHz
    Copyright (c) 2015-2016 Robert Tari <robert@tari.in>
    Copyright (c) 2012 Vladislav Goncharov <vl-g@yandex.ru>

    This file is part of SACD.

    SACD is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SACD is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SACD.  If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
*/

#ifndef _dither_h_
#define _dither_h_

#include <stdlib.h>
#include <stdint.h>

// inline dithering implementation
class Dither
{
public:

    Dither(unsigned int n_bits);
    Dither& operator=(const Dither &obj);
    double processSample(double x) { return (x + m_rand_max * (double)(fast_rand() - (RAND_MAX / 2)) / (double)(RAND_MAX / 2)); }

protected:

    double m_rand_max;
    int m_holdrand;
    int fast_rand() { return (((m_holdrand = m_holdrand * 214013L + 2531011L) >> 16) & 0x7fff); } // libc rand() is too slow
};

#endif
