/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 *  Copyright (C) 2025-2026 Leandro Nini
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// Based on cRSID lightweight RealSID by Hermit (Mihaly Horvath)

#ifndef SIDLITE_FILTER_H
#define SIDLITE_FILTER_H

#include <map>

struct SampleI32;

namespace SIDLite
{

class settings;

class Filter
{
public:
    Filter(settings *s, unsigned char *regs);
    void reset();
    SampleI32 clock(SampleI32 FilterInput, SampleI32 NonFiltered);

    inline int getLevel(int i) const { return Level[i]; }

    void rebuildCutoffTables(unsigned short samplerate);

private:
    unsigned char *regs;
    settings      *s;

    unsigned short *CutoffMul8580;
    unsigned short *CutoffMul6581;

    signed int     PrevVolume[2]; // lowpass-filtered version of Volume-band register
    int            PrevLowPass[2];
    int            PrevBandPass[2];
    int            Level[2];      // filtered version, good for VU-meter display
    unsigned char  VUmeterUpdateCounter[2];

    static constexpr int CF_LEN = 0x800;

    using co_tab_t = std::array<unsigned short, CF_LEN>;
    using co_cache_t = std::map<unsigned short, co_tab_t>;

    co_cache_t CUTOFF_CACHE_8580;
    co_cache_t CUTOFF_CACHE_6581;
};

}

#endif // SIDLITE_FILTER_H

