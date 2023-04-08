#include "SoundMonModule.h"

#include <memory.h>
#include <new>

using namespace core;

namespace SoundMon
{
    Module* Module::Load(const uint8_t* data, size_t size)
    {
        auto module = new (Alloc<Module>(size + sizeof(Module))) Module();
        memcpy(module + 1, data, size);

        auto modptr = reinterpret_cast<uint8_t*>(module + 1);
        if (memcmp(modptr + 26, "BPSM", 4) == 0)
        {
            module->m_version = 1;
        }
        else if (memcmp(modptr + 26, "V.2", 3) == 0)
        {
            module->m_version = 2;
            ConvertHeader(modptr);
        }
        else if (memcmp(modptr + 26, "V.3", 3) != 0)
        {
            module->Release();
            return nullptr;
        }

        module->Init(modptr);

        return module;
    }

    void Module::Release()
    {
        this->~Module();
        Free(this);
    }

    uint16_t Module::GetPattern(uint16_t step, uint16_t voice) const
    {
        uint8_t* ptr = m_startsteps + kStepVoiceSize * voice + step * kStepSize;

        return 256 * (*ptr) + *(ptr + 1);
    }

    bool Module::GetSampledInstrument(int16_t instr, const char*& name, uint16_t& size, uint16_t& repeat, uint16_t& replen, uint16_t& volume) const
    {
        if (instr > kNumInstr || instr < 1 || IsSynthInstrument(instr))
            return false;
        return m_instr[instr - 1].GetSampledInstr(name, size, repeat, replen, volume);
    }

    bool Module::GetSampledInstrument(int16_t instr, uint16_t& size, uint16_t& repeat, uint16_t& replen, uint16_t& volume, uint8_t*& start) const
    {
        const char* dummy;
        if (instr > kNumInstr || instr < 1 || IsSynthInstrument(instr))
            return false;
        start = m_instr[instr - 1].GetSampledInstrStart();
        return m_instr[instr - 1].GetSampledInstr(dummy, size, repeat, replen, volume);
    }


    bool Module::GetSynthInstrument(int16_t instr, uint16_t& size, uint8_t*& start) const
    {
        if (instr > kNumInstr || instr < 1 || !IsSynthInstrument(instr))
            return false;
        start = m_instr[instr - 1].GetSynthInstrStart();
        size = m_instr[instr - 1].GetSynthInstrSize();
        return true;
    }

    bool Module::GetSynthDelays(int16_t instr, uint8_t& egdelay, uint8_t& lfodelay, uint8_t& fxdelay, uint8_t& moddelay) const
    {
        if (instr > kNumInstr || instr < 1 || !IsSynthInstrument(instr))
            return false;
        return m_instr[instr - 1].GetSynthDelays(egdelay, lfodelay, fxdelay, moddelay);
    }


    bool Module::GetSynthEGData(int16_t instr, uint8_t& table, uint16_t& size, uint8_t& speed) const
    {
        if (instr > kNumInstr || instr < 1 || !IsSynthInstrument(instr))
            return false;
        return m_instr[instr - 1].GetSynthEGData(table, size, speed);
    }


    bool Module::GetSynthLFOData(int16_t instr, uint8_t& table, uint16_t& size, uint8_t& speed, uint8_t& depth) const
    {
        if (instr > kNumInstr || instr < 1 || !IsSynthInstrument(instr))
            return false;
        return m_instr[instr - 1].GetSynthLFOData(table, size, speed, depth);
    }


    bool Module::GetSynthFXData(int16_t instr, uint8_t& table, uint8_t& speed) const
    {
        if (instr > kNumInstr || instr < 1 || !IsSynthInstrument(instr))
            return false;
        return m_instr[instr - 1].GetSynthFXData(table, speed);
    }


    bool Module::GetSynthMODData(int16_t instr, uint8_t table, uint16_t& size, uint8_t& speed) const
    {
        if (instr > kNumInstr || instr < 1 || !IsSynthInstrument(instr))
            return false;
        return m_instr[instr - 1].GetSynthMODData(table, size, speed);
    }


    bool Module::GetSynthADSRData(int16_t instr, uint8_t& table, uint16_t& size, uint8_t& speed) const
    {
        if (instr > kNumInstr || instr < 1 || !IsSynthInstrument(instr))
            return false;
        return m_instr[instr - 1].GetSynthADSRData(table, size, speed);
    }


    bool Module::GetSynthControls(int16_t instr, uint8_t& adsr, uint8_t& lfo, uint8_t& eg, uint8_t& fx, uint8_t& mod) const
    {
        if (instr > kNumInstr || instr < 1 || !IsSynthInstrument(instr))
            return false;
        return m_instr[instr - 1].GetSynthControls(adsr, lfo, eg, fx, mod);
    }

    const char* Module::GetInstrumentName(int16_t instr) const
    {
        if (instr > kNumInstr || instr < 1)
            return "";
        return m_instr[instr - 1].GetName();
    }

    void Module::Init(uint8_t* modptr)
    {
        memcpy(m_songname, modptr, 26);
        m_songname[26] = 0;
        m_numtables = (uint16_t) * (modptr + 29);
        m_numsteps = (uint16_t)(*(modptr + 30) * 256 + *(modptr + 31));
        m_startsteps = modptr + kHeaderSize;
        m_startpatterns = m_startsteps + kStepSize * m_numsteps;

        auto ptr = m_startsteps;
        uint16_t maxpattern = 0;
        for (uint32_t i = 0; i < m_numsteps * kVoices; i++)
        {
            uint16_t pattern = 256 * (*ptr) + *(ptr + 1);
            if (maxpattern < pattern)
                maxpattern = pattern;
            ptr += 4;
        }

        m_starttables = m_startpatterns + kPatternSize * maxpattern;
        m_numpatterns = maxpattern;
        m_startsamples = m_starttables + kTableSize * m_numtables;
        if (m_numtables != 0)
            ConvertAmigaSample(m_starttables, m_numtables * kTableSize);
        ptr = modptr + kSongDataSize;
        auto sampptr = m_startsamples;
        for (uint32_t i = 0; i < kNumInstr; i++)
        {
            m_instr[i].Parse(ptr, sampptr, m_starttables);
            if (!m_instr[i].IsSynthInstr())
            {
                ConvertAmigaSample(sampptr, m_instr[i].GetSampledInstrSize());
                sampptr += m_instr[i].GetSampledInstrSize();
            }
            ptr += kInstrSize;
        }
    }

    void Module::ConvertAmigaSample(uint8_t* ptr, size_t size)
    {
        while (size > 0)
        {
            (*ptr) += 128;
            ptr++;
            size--;
        }
    }
    void Module::ConvertHeader(uint8_t* modptr)
    {
        auto ptr = modptr + kSongDataSize;
        for (uint32_t i = 0; i < kNumInstr; i++)
        {
            if (ptr[0] == 0xFF)
            {
                ptr[31] = 0;
                ptr[30] = 0;
                ptr[29] = ptr[25];
                ptr[28] = 0;
                ptr[27] = 1;
                ptr[26] = 0;
                ptr[25] = 0;
                ptr[14] = ptr[15];
                ptr[15] = ptr[16];
                ptr[16] = ptr[17];
                ptr[17] = ptr[18];
                ptr[18] = ptr[20];
                ptr[19] = ptr[21];
                ptr[20] = ptr[23];
                ptr[21] = ptr[24];
                ptr[22] = 0;
                ptr[23] = 1;
                ptr[24] = 0;
            }
            ptr += kInstrSize;
        }
    }
}
//namespace SoundMon