/*
    Copyright 2015-2016 Robert Tari <robert@tari.in>
    Copyright 2012 Vladislav Goncharov <vl-g@yandex.ru>

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

#ifndef _upsampler_h_
#define _upsampler_h_

#include "dither.h"

// history buffer for FIR
class FirHistory
{
public:
    FirHistory(unsigned int fir_size);
    ~FirHistory();
    FirHistory&  operator=(const FirHistory &obj);
    void pushSample(double x);
    void reset(bool reset_to_1 = false);
    double *getBuffer() { return &m_x[m_head]; }
    unsigned int getSize() const { return m_fir_size; }

private:
    double *m_x; // [m_fir_size * 2]
    unsigned int m_fir_size;
    unsigned int m_head; // write to m_x[--head]
};

// FIR filter
class FirFilter
{
public:
    FirFilter(const double *fir, unsigned int fir_size, bool no_history = false);
    FirFilter();
    ~FirFilter();
    FirFilter&  operator=(const FirFilter &obj);
    double processSample(double x);
    void reset(bool reset_to_1 = false);
    void pushSample(double x);
    double fast_convolve(double *x);
    const double *getFir() const { return m_fir; }
    unsigned int getFirSize() const { return m_org_fir_size; }

private:
    double *m_fir; // [m_fir_size], aligned
    double *m_fir_alloc;
    FirHistory m_x;
    unsigned int m_fir_size; // aligned
    unsigned int m_org_fir_size;
};

// Nx/Mx resampler
class ResamplerNxMx
{
public:
    ResamplerNxMx(unsigned int nX, unsigned int mX, const double *fir, unsigned int fir_size);
    ~ResamplerNxMx();
    void processSample(const double *x, unsigned int x_n, double *y, unsigned int *y_n);
    void reset(bool reset_to_1 = false);
    unsigned int getFirSize() const { return m_fir_size; }

private:
    unsigned int m_xN; // up^
    unsigned int m_xM; // down_
    unsigned int m_fir_size;
    FirFilter *m_flt; // [m_xN]
    FirHistory m_x;
    unsigned int m_xN_counter; // how many virtually upsampled samples we have in FirHistory?
};

// generate windowed sinc impulse response for low-pass filter
void generateFilter(double *impulse, int taps, double sinc_freq);

#endif
