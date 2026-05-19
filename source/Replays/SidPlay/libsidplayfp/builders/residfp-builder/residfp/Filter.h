/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 * Copyright 2011-2025 Leandro Nini <drfiemost@users.sourceforge.net>
 * Copyright 2007-2010 Antti Lankila
 * Copyright 2004 Dag Lem <resid@nimrod.no>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef FILTER_H
#define FILTER_H

#include "FilterModelConfig.h"
#include "Voice.h"

#include "siddefs-fp.h"

namespace reSIDfp
{

/**
 * SID filter base class
 */
class Filter
{
private:
    unsigned short* mixer;
    unsigned short* summer;
    unsigned short* resonance;
    unsigned short* volume;

    FilterModelConfig& fmc;

    /// Current filter/voice mixer setting.
    unsigned short* currentMixer[2] = { nullptr };

    /// Filter input summer setting.
    unsigned short* currentSummer[2] = { nullptr };

    /// Filter resonance value.
    unsigned short* currentResonance = nullptr;

    /// Current volume amplifier setting.
    unsigned short* currentVolume = nullptr;

protected:
    /// Filter highpass state.
    int Vhp[2] = { 0 };

    /// Filter bandpass state.
    int Vbp[2] = { 0 };

    /// Filter lowpass state.
    int Vlp[2] = { 0 };

private:
    /// Filter external input.
    int Ve = 0;

    /// Filter cutoff frequency.
    unsigned int fc = 0;

    /// Routing to filter or outside filter
    //@{
    bool filt1 = false;
    bool filt2 = false;
    bool filt3 = false;
    bool filtE = false;
    //@}

    /// Switch voice 3 off.
    bool voice3off = false;

protected:
    /// Highpass, bandpass, and lowpass filter modes.
    //@{
    bool hp = false;
    bool bp = false;
    bool lp = false;
    //@}

private:
    /// Current volume.
    unsigned char vol = 0;

    /// Filter enabled.
    bool enabled = true;

    bool isSurroundEnabled = false;

    /// Selects which inputs to route through filter.
    unsigned char filt = 0;

private:
    inline int getNormalizedVoice(Voice& v) const
    {
        return fmc.getNormalizedVoice(v.output(), v.envelope()->output());
    }

    // If voice 3 is off we still need to clock the waveform generator
    inline static int getSilentVoice(Voice& v)
    {
        v.wave()->output();
        return 0;
    }

protected:
    /**
     * Update filter cutoff frequency.
     */
    virtual void updateCenterFrequency() = 0;

    /**
     * Update filter resonance.
     *
     * @param res the new resonance value
     */
    void updateResonance(unsigned char res) { currentResonance = resonance + (res * (1<<16)); }

    /**
     * Mixing configuration modified (offsets change)
     */
    void updateMixing();

    /**
     * Get the filter cutoff register value
     */
    inline unsigned int getFC() const { return fc; }

    virtual int solveIntegrators(int i) = 0;

public:
    Filter(FilterModelConfig& fmc);

    virtual ~Filter() = default;

    /**
     * SID clocking - 1 cycle
     *
     * @param voice1 voice 1 in
     * @param voice2 voice 2 in
     * @param voice3 voice 3 in
     * @return filtered output, unsigned 16 bit
     */
    SampleU16 clock(Voice& voice1, Voice& voice2, Voice& voice3);

    /**
     * Enable filter.
     *
     * @param enable
     */
    void enable(bool enable);

    /**
     * SID reset.
     */
    void reset();

    /**
     * Write Frequency Cutoff Low register.
     *
     * @param fc_lo Frequency Cutoff Low-Byte
     */
    void writeFC_LO(unsigned char fc_lo);

    /**
     * Write Frequency Cutoff High register.
     *
     * @param fc_hi Frequency Cutoff High-Byte
     */
    void writeFC_HI(unsigned char fc_hi);

    /**
     * Write Resonance/Filter register.
     *
     * @param res_filt Resonance/Filter
     */
    void writeRES_FILT(unsigned char res_filt);

    /**
     * Write filter Mode/Volume register.
     *
     * @param mode_vol Filter Mode/Volume
     */
    void writeMODE_VOL(unsigned char mode_vol);

    /**
     * Apply a signal to EXT-IN
     *
     * @param input a signed 16 bit sample
     */
    void input(short input) { Ve = fmc.getNormalizedVoice(input/32768.f, 0); }

    void surround(bool surroundEnabled);
};

} // namespace reSIDfp

#if RESIDFP_INLINING || defined(FILTER_CPP)

namespace reSIDfp
{

RESIDFP_INLINE
SampleU16 Filter::clock(Voice& voice1, Voice& voice2, Voice& voice3)
{
    const int V1 = getNormalizedVoice(voice1);
    const int V2 = getNormalizedVoice(voice2);
    // Voice 3 is silenced by voice3off if it is not routed through the filter.
    const int V3 = (filt3 || !voice3off) ? getNormalizedVoice(voice3) : getSilentVoice(voice3);

    int Vsum[2] = { 0 };
    int Vmix[2] = { 0 };

    (filt1 ? Vsum[0] : Vmix[0]) += V1;
    if (!isSurroundEnabled)
        (filt2 ? Vsum[0] : Vmix[0]) += V2;
    (filt3 ? Vsum[0] : Vmix[0]) += V3;
    (filtE ? Vsum[0] : Vmix[0]) += Ve;

    if (!isSurroundEnabled)
        (filt1 ? Vsum[1] : Vmix[1]) += V1;
    (filt2 ? Vsum[1] : Vmix[1]) += V2;
    if (!isSurroundEnabled)
        (filt3 ? Vsum[1] : Vmix[1]) += V3;
    (filtE ? Vsum[1] : Vmix[1]) += Ve;

    Vhp[0] = currentSummer[0][currentResonance[Vbp[0]] + Vlp[0] + Vsum[0]];
    Vhp[1] = currentSummer[1][currentResonance[Vbp[1]] + Vlp[1] + Vsum[1]];

    Vmix[0] += solveIntegrators(0);
    Vmix[1] += solveIntegrators(1);

    return { currentVolume[currentMixer[0][Vmix[0]]], currentVolume[currentMixer[1][Vmix[1]]] };
}

} // namespace reSIDfp

#endif

#endif
