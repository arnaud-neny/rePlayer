#include "SoundMonInstrument.h"

#include <memory.h>

namespace SoundMon
{
    void Instrument::Parse(uint8_t ptr[kInstrSize], uint8_t* memptr, uint8_t* starttables)
    {
        if (*(ptr + kSynthIndicatorOffset) == 0xFF)
        { // synth sound
            m_issynth = true;
            m_instrdata.synthinstr.indicator = 0xFF;
            m_instrdata.synthinstr.wavetable = *(ptr + kSynthWaveTableOffset);
            m_instrdata.synthinstr.wavelength = 2 * (*(ptr + kSynthWaveLengthOffset) * 256 + *(ptr + kSynthWaveLengthOffset + 1));
            m_instrdata.synthinstr.adsrcontrol = *(ptr + kSynthAdsrControlOffset);
            m_instrdata.synthinstr.adsrtable = *(ptr + kSynthAdsrTableOffset);
            m_instrdata.synthinstr.adsrlength = (*(ptr + kSynthAdsrLengthOffset) * 256 + *(ptr + kSynthAdsrLengthOffset + 1));
            m_instrdata.synthinstr.adsrspeed = *(ptr + kSynthAdsrSpeedOffset);
            m_instrdata.synthinstr.lfocontrol = *(ptr + kSynthLfoControlOffset);
            m_instrdata.synthinstr.lfodelay = *(ptr + kSynthLfoDelayOffset);
            m_instrdata.synthinstr.lfodepth = *(ptr + kSynthLfoDepthOffset);
            m_instrdata.synthinstr.lfolength = (*(ptr + kSynthLfoLengthOffset) * 256 + *(ptr + kSynthLfoLengthOffset + 1));
            m_instrdata.synthinstr.lfospeed = *(ptr + kSynthLfoSpeedOffset);
            m_instrdata.synthinstr.lfotable = *(ptr + kSynthLfoTableOffset);
            m_instrdata.synthinstr.egcontrol = *(ptr + kSynthEgControlOffset);
            m_instrdata.synthinstr.egdelay = *(ptr + kSynthEgDelayOffset);
            m_instrdata.synthinstr.eglength = (*(ptr + kSynthEgLengthOffset) * 256 + *(ptr + kSynthEgLengthOffset + 1));
            m_instrdata.synthinstr.egspeed = *(ptr + kSynthEgSpeedOffset);
            m_instrdata.synthinstr.egtable = *(ptr + kSynthEgTableOffset);
            m_instrdata.synthinstr.fxcontrol = *(ptr + kSynthFxControlOffset);
            m_instrdata.synthinstr.fxdelay = *(ptr + kSynthFxDelayOffset);
            m_instrdata.synthinstr.fxspeed = *(ptr + kSynthFxSpeedOffset);
            m_instrdata.synthinstr.modcontrol = *(ptr + kSynthModControlOffset);
            m_instrdata.synthinstr.moddelay = *(ptr + kSynthModDelayOffset);
            m_instrdata.synthinstr.modspeed = *(ptr + kSynthModSpeedOffset);
            m_instrdata.synthinstr.modtable = *(ptr + kSynthModTableOffset);
            m_instrdata.synthinstr.volume = *(ptr + kSynthVolumeOffset);
            m_instrdata.synthinstr.modlength = (*(ptr + kSynthModLengthOffset) * 256 + *(ptr + kSynthModLengthOffset + 1));
            m_instrdata.synthinstr.ptr = starttables + kTableSize * m_instrdata.synthinstr.wavetable;
        }
        else
        { // sample
            m_issynth = false;
            memcpy(m_instrdata.sampledinstr.name, ptr + kSampleNameOffset, kSampleNameSize);
            m_instrdata.sampledinstr.size = 2 * (*(ptr + kSampleSizeOffset) * 256 + *(ptr + kSampleSizeOffset + 1));
            m_instrdata.sampledinstr.repeat = *(ptr + kSampleRepeatOffset) * 256 + *(ptr + kSampleRepeatOffset + 1);
            m_instrdata.sampledinstr.replen = 2 * (*(ptr + kSampleReplenOffset) * 256 + *(ptr + kSampleReplenOffset + 1));
            m_instrdata.sampledinstr.volume = *(ptr + kSampleVolumeOffset) * 256 + *(ptr + kSampleVolumeOffset + 1);
            m_instrdata.sampledinstr.ptr = memptr;
        }
    }

    bool Instrument::GetSampledInstr(const char*& name, uint16_t& size, uint16_t& repeat, uint16_t& replen, uint16_t& volume) const
    {
        if (m_issynth)
            return false;
        name = m_instrdata.sampledinstr.name;
        size = m_instrdata.sampledinstr.size;
        repeat = m_instrdata.sampledinstr.repeat;
        replen = m_instrdata.sampledinstr.replen;
        volume = m_instrdata.sampledinstr.volume;
        return true;
    }

    bool Instrument::GetSynthADSRData(uint8_t& table, uint16_t& size, uint8_t& speed) const
    {
        table = m_instrdata.synthinstr.adsrtable;
        size = m_instrdata.synthinstr.adsrlength;
        speed = m_instrdata.synthinstr.adsrspeed;
        return m_issynth;
    }

    bool Instrument::GetSynthEGData(uint8_t& table, uint16_t& size, uint8_t& speed) const
    {
        table = m_instrdata.synthinstr.egtable;
        size = m_instrdata.synthinstr.eglength;
        speed = m_instrdata.synthinstr.egspeed;
        return m_issynth;
    }

    bool Instrument::GetSynthLFOData(uint8_t& table, uint16_t& size, uint8_t& speed, uint8_t& depth) const
    {
        table = m_instrdata.synthinstr.lfotable;
        size = m_instrdata.synthinstr.lfolength;
        speed = m_instrdata.synthinstr.lfospeed;
        depth = m_instrdata.synthinstr.lfodepth;
        return m_issynth;
    }

    bool Instrument::GetSynthFXData(uint8_t& table, uint8_t& speed) const
    {
        table = m_instrdata.synthinstr.wavetable;
        speed = m_instrdata.synthinstr.fxspeed;
        return m_issynth;
    }

    bool Instrument::GetSynthMODData(uint8_t table, uint16_t& size, uint8_t& speed) const
    {
        table = m_instrdata.synthinstr.modtable;
        size = m_instrdata.synthinstr.modlength;
        speed = m_instrdata.synthinstr.modspeed;
        return m_issynth;
    }

    bool Instrument::GetSynthControls(uint8_t& adsr, uint8_t& lfo, uint8_t& eg, uint8_t& fx, uint8_t& mod) const
    {
        adsr = m_instrdata.synthinstr.adsrcontrol;
        eg = m_instrdata.synthinstr.egcontrol;
        lfo = m_instrdata.synthinstr.lfocontrol;
        fx = m_instrdata.synthinstr.fxcontrol;
        mod = m_instrdata.synthinstr.modcontrol;
        return m_issynth;
    }

    bool Instrument::GetSynthDelays(uint8_t& egdelay, uint8_t& lfodelay, uint8_t& fxdelay, uint8_t& moddelay) const
    {
        egdelay = m_instrdata.synthinstr.egdelay;
        lfodelay = m_instrdata.synthinstr.lfodelay;
        fxdelay = m_instrdata.synthinstr.fxdelay;
        moddelay = m_instrdata.synthinstr.moddelay;
        return m_issynth;
    }
}
//namespace SoundMon