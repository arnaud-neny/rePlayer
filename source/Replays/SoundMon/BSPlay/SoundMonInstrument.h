#pragma once

#include "SoundMonTypes.h"

namespace SoundMon
{
    class Instrument
    {
    public:
        void Parse(uint8_t ptr[kInstrSize], uint8_t *memptr, uint8_t *starttables);
        bool GetSampledInstr(const char *&name, uint16_t &size, uint16_t &repeat, uint16_t &replen, uint16_t &volume) const;
        bool GetSynthADSRData(uint8_t &table, uint16_t &size, uint8_t &speed) const;
        bool GetSynthEGData(uint8_t &table, uint16_t &size, uint8_t &speed) const;
        bool GetSynthLFOData(uint8_t &table, uint16_t &size, uint8_t &speed, uint8_t &depth) const;
        bool GetSynthFXData(uint8_t &table, uint8_t &speed) const;
        bool GetSynthMODData(uint8_t table, uint16_t &size, uint8_t &speed) const;
        bool GetSynthControls(uint8_t &adsr, uint8_t &lfo, uint8_t &eg, uint8_t &fx, uint8_t &mod) const;
        bool GetSynthDelays(uint8_t &egdelay, uint8_t &lfodelay, uint8_t &fxdelay, uint8_t &moddelay) const;

        uint16_t GetSampledInstrSize() const { return m_instrdata.sampledinstr.size; };
        uint16_t GetSampledInstrRepeat() const { return m_instrdata.sampledinstr.repeat; };
        uint16_t GetSampledInstrReplen() const { return m_instrdata.sampledinstr.replen; };
        uint16_t GetSampledInstrVolume() const { return m_instrdata.sampledinstr.volume; };
        uint8_t* GetSampledInstrStart() const { return m_instrdata.sampledinstr.ptr; }
        uint8_t* GetSynthInstrStart() const { return m_instrdata.synthinstr.ptr; }
        uint16_t GetSynthInstrSize() const { return m_instrdata.synthinstr.wavelength; }
        bool IsSynthInstr() const { return m_issynth; }
        uint16_t GetInstrVolume() const { return m_issynth ? m_instrdata.synthinstr.volume : m_instrdata.sampledinstr.volume; }
        const char* GetName() const { return m_issynth ? "SYNTHETIC" : m_instrdata.sampledinstr.name; }

    private:
        static constexpr uint32_t kSampleNameSize = 24;
        static constexpr uint32_t kSampleNameOffset = 0;
        static constexpr uint32_t kSampleSizeOffset = 24;
        static constexpr uint32_t kSampleRepeatOffset = 26;
        static constexpr uint32_t kSampleReplenOffset = 28;
        static constexpr uint32_t kSampleVolumeOffset = 30;

        static constexpr uint32_t kSynthIndicatorOffset = 0;
        static constexpr uint32_t kSynthWaveTableOffset = 1;
        static constexpr uint32_t kSynthWaveLengthOffset = 2;
        static constexpr uint32_t kSynthAdsrControlOffset = 4;
        static constexpr uint32_t kSynthAdsrTableOffset = 5;
        static constexpr uint32_t kSynthAdsrLengthOffset = 6;
        static constexpr uint32_t kSynthAdsrSpeedOffset = 8;
        static constexpr uint32_t kSynthLfoControlOffset = 9;
        static constexpr uint32_t kSynthLfoTableOffset = 10;
        static constexpr uint32_t kSynthLfoDepthOffset = 11;
        static constexpr uint32_t kSynthLfoLengthOffset = 12;
        static constexpr uint32_t kSynthLfoDelayOffset = 14;
        static constexpr uint32_t kSynthLfoSpeedOffset = 15;
        static constexpr uint32_t kSynthEgControlOffset = 16;
        static constexpr uint32_t kSynthEgTableOffset = 17;
        static constexpr uint32_t kSynthEgLengthOffset = 18;
        static constexpr uint32_t kSynthEgDelayOffset = 20;
        static constexpr uint32_t kSynthEgSpeedOffset = 21;
        static constexpr uint32_t kSynthFxControlOffset = 22;
        static constexpr uint32_t kSynthFxSpeedOffset = 23;
        static constexpr uint32_t kSynthFxDelayOffset = 24;
        static constexpr uint32_t kSynthModControlOffset = 25;
        static constexpr uint32_t kSynthModTableOffset = 26;
        static constexpr uint32_t kSynthModSpeedOffset = 27;
        static constexpr uint32_t kSynthModDelayOffset = 28;
        static constexpr uint32_t kSynthVolumeOffset = 29;
        static constexpr uint32_t kSynthModLengthOffset = 30;

        struct SampledInstr
        {
            char name[24];
            uint16_t size;
            uint16_t repeat;
            uint16_t replen;
            uint16_t volume;
            uint8_t* ptr;
        };

        struct SynthInstr
        {
            uint8_t indicator;
            uint8_t wavetable;
            uint16_t wavelength;
            uint8_t adsrcontrol;
            uint8_t adsrtable;
            uint16_t adsrlength;
            uint8_t adsrspeed;
            uint8_t lfocontrol;
            uint8_t lfotable;
            uint8_t lfodepth;
            uint16_t lfolength;
            uint8_t lfodelay;
            uint8_t lfospeed;
            uint8_t egcontrol;
            uint8_t egtable;
            uint16_t eglength;
            uint8_t egdelay;
            uint8_t egspeed;
            uint8_t fxcontrol;
            uint8_t fxspeed;
            uint8_t fxdelay;
            uint8_t modcontrol;
            uint8_t modtable;
            uint8_t modspeed;
            uint8_t moddelay;
            uint8_t volume;
            uint16_t modlength;
            uint8_t* ptr;
        };

    private:
        bool m_issynth{ false };
        union InstrData
        {
            SampledInstr sampledinstr;
            SynthInstr synthinstr;
        } m_instrdata;
    };
}
//namespace SoundMon