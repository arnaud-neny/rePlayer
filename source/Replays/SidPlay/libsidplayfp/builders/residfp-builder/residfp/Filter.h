/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 * Copyright 2011-2026 Leandro Nini <drfiemost@users.sourceforge.net>
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
#include "Integrator.h"
#include "Voice.h"

#include "siddefs-fp.h"

namespace reSIDfp
{

/**
 * SID filter base class
 */
class Filter
{
    friend class State;

private:
    uint16_t* mixer;
    uint16_t* summer;
    uint16_t* resonance;
    uint16_t* volume;

    const FilterModelConfig& m_fmc;

    const Integrator& m_hpIntegrator0;
    const Integrator& m_hpIntegrator1;

    const Integrator& m_bpIntegrator0;
    const Integrator& m_bpIntegrator1;

    /// Current filter/voice mixer setting.
    uint16_t* currentMixer[2] = { nullptr };

    /// Filter input summer setting.
    uint16_t* currentSummer[2] = { nullptr };

    /// Filter resonance value.
    uint16_t* currentResonance = nullptr;

    /// Current volume amplifier setting.
    uint16_t* currentVolume = nullptr;

    /// Filter highpass state.
    int32_t Vhp[2] = { 0 };

    /// Filter bandpass state.
    int32_t Vbp[2] = { 0 };

    /// Filter lowpass state.
    int32_t Vlp[2] = { 0 };

    /// Filter external input.
    float extin = 0;

    /// Filter cutoff frequency.
    uint16_t fc = 0;

    /// Routing to filter or outside filter
    //@{
    bool filt1 = false;
    bool filt2 = false;
    bool filt3 = false;
    bool filtE = false;
    //@}

    /// Switch voice 3 off.
    bool voice3off = false;

    /// Highpass, bandpass, and lowpass filter modes.
    //@{
    bool hp = false;
    bool bp = false;
    bool lp = false;
    //@}

private:
    /// Current volume.
    uint8_t vol = 0;

    /// Filter enabled.
    bool enabled = true;

    bool isSurroundEnabled = false;

    /// Selects which inputs to route through filter.
    uint8_t filt = 0;

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
    void updateResonance(uint8_t res) { currentResonance = resonance + (res * (1<<16)); }

    /**
     * Mixing configuration modified (offsets change)
     */
    void updateMixing();

    /**
     * Get the filter cutoff register value
     */
    inline unsigned int getFC() const { return static_cast<unsigned int>(fc); }

    virtual void restartIntegrators() = 0;

    inline int32_t getNormalizedVoice(float v, uint8_t env) const
    {
        return m_fmc.getNormalizedVoice(v, env);
    }

    virtual int32_t getNormalizedMixerVoice(float v, uint8_t env) const = 0;

public:
    Filter(const FilterModelConfig& fmc,
           const Integrator& hpIntegrator0, const Integrator& hpIntegrator1,
           const Integrator& bpIntegrator0, const Integrator& bpIntegrator1);

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
    void writeFC_LO(uint8_t fc_lo);

    /**
     * Write Frequency Cutoff High register.
     *
     * @param fc_hi Frequency Cutoff High-Byte
     */
    void writeFC_HI(uint8_t fc_hi);

    /**
     * Write Resonance/Filter register.
     *
     * @param res_filt Resonance/Filter
     */
    void writeRES_FILT(uint8_t res_filt);

    /**
     * Write filter Mode/Volume register.
     *
     * @param mode_vol Filter Mode/Volume
     */
    void writeMODE_VOL(uint8_t mode_vol);

    /**
     * Apply a signal to EXT-IN
     *
     * @param input a signed 16 bit sample
     */
    void input(int16_t input) { extin = input/65535.f; }

    void restart() { restartIntegrators(); Vhp[0] = 0; Vlp[0] = 0; Vbp[0] = 0; Vhp[1] = 0; Vlp[1] = 0; Vbp[1] = 0; }

    void surround(bool surroundEnabled);
};

} // namespace reSIDfp

#if RESIDFP_INLINING || defined(FILTER_CPP)

namespace reSIDfp
{

RESIDFP_INLINE
SampleU16 Filter::clock(Voice& voice1, Voice& voice2, Voice& voice3)
{
    // Waveform outputs
    const float wav1 = voice1.output();
    const float wav2 = voice2.output();
    const float wav3 = voice3.output();

    // Envelope outputs
    const uint8_t env1 = voice1.envelope()->output();
    const uint8_t env2 = voice2.envelope()->output();
    const uint8_t env3 = voice3.envelope()->output();

    int32_t v1 = getNormalizedVoice(wav1, env1);
    int32_t v2 = getNormalizedVoice(wav2, env2);
    int32_t v3 = getNormalizedVoice(wav3, env3);
    int32_t vE = getNormalizedVoice(extin, 0);

    // Voltage summer for filter input
    int32_t Vsum[2] = { 0 };
    Vsum[0] += filt1 ? v1 : 0;
    Vsum[0] += filt2 && !isSurroundEnabled ? v2 : 0;
    Vsum[0] += filt3 ? v3 : 0;
    Vsum[0] += filtE && !isSurroundEnabled ? vE : 0;
    Vsum[0] += Vlp[0];
    Vsum[0] += currentResonance[Vbp[0]];
    Vsum[1] += filt1 && !isSurroundEnabled ? v1 : 0;
    Vsum[1] += filt2 ? v2 : 0;
    Vsum[1] += filt3 && !isSurroundEnabled ? v3 : 0;
    Vsum[1] += filtE ? vE : 0;
    Vsum[1] += Vlp[1];
    Vsum[1] += currentResonance[Vbp[1]];

    // Filter
    Vhp[0] = currentSummer[0][Vsum[0]];
    Vbp[0] = m_hpIntegrator0.solve(Vhp[0]);
    Vlp[0] = m_bpIntegrator0.solve(Vbp[0]);
    Vhp[1] = currentSummer[1][Vsum[1]];
    Vbp[1] = m_hpIntegrator1.solve(Vhp[1]);
    Vlp[1] = m_bpIntegrator1.solve(Vbp[1]);

    int32_t Vfilt[2] =  { 0 };
    if (lp) Vfilt[0] += Vlp[0];
    if (bp) Vfilt[0] += Vbp[0];
    if (hp) Vfilt[0] += Vhp[0];
    if (lp) Vfilt[1] += Vlp[1];
    if (bp) Vfilt[1] += Vbp[1];
    if (hp) Vfilt[1] += Vhp[1];

    // Voltage summer for mixer input
    int32_t Vmix[2] = { 0 };
    Vmix[0] += filt1 ? 0 : v1;
    Vmix[0] += filt2 || isSurroundEnabled ? 0 : v2;
    // Voice 3 is silenced by voice3off if it is not routed through the filter
    Vmix[0] += (filt3 || voice3off) ? 0 : v3;
    Vmix[0] += filtE || isSurroundEnabled ? 0 : vE;
    Vmix[0] += Vfilt[0];
    Vmix[1] += filt1 || isSurroundEnabled ? 0 : v1;
    Vmix[1] += filt2 ? 0 : v2;
    // Voice 3 is silenced by voice3off if it is not routed through the filter
    Vmix[1] += (filt3 || voice3off || isSurroundEnabled) ? 0 : v3;
    Vmix[1] += filtE ? 0 : vE;
    Vmix[1] += Vfilt[1];

    return { currentVolume[currentMixer[0][Vmix[0]]], currentVolume[currentMixer[1][Vmix[1]]] };
}

} // namespace reSIDfp

#endif

#endif
