#include "SoundMonMixer.h"
#include "SoundMonModule.h"

#include <cmath>

using namespace core;

namespace SoundMon
{
    Mixer::Mixer(Module* modptr)
        : m_modptr(modptr)
    {}

    Mixer::~Mixer()
    {
        CleanUp();
    }

    void Mixer::CleanUp()
    {
        delete[] m_channels;
        m_channels = nullptr;
        delete[] m_playbuffer;
        m_playbuffer = nullptr;
    }

    bool Mixer::Initialize(int16_t nrofvoices, uint32_t samplerate)
    {
        if (m_modptr == nullptr)
            return false;
        m_nrofvoices = nrofvoices;
        m_samplerate = samplerate;

        if ((m_channels = new Channel[nrofvoices]) == nullptr)
            return false;
        if ((m_playbuffer = new int16_t[GetBufferSize() * 2]) == nullptr)
            return false;
        return true;
    }

    void Mixer::Stop()
    {
        m_bufferpos = 0;
        m_remainingsize = 0;
        for (int16_t i = 0; i < m_nrofvoices; i++)
            m_channels[i].StopPlay();
    }

    bool Mixer::Play(int16_t voicenr, uint8_t instrument, int32_t period, int32_t volume)
    {
        uint16_t size, repeat, replen, dummy;
        uint8_t* sampptr;

        if (instrument < 1 || instrument>15 || voicenr >= m_nrofvoices || voicenr < 0)
            return false;
        if (volume > 64) volume = 64;
        if (m_modptr->IsSynthInstrument(instrument))
        {
            m_modptr->GetSynthInstrument(instrument, size, sampptr);
            replen = size;
            repeat = 0;
        }
        else
        {
            m_modptr->GetSampledInstrument(instrument, size, repeat, replen, dummy, sampptr);
        }
        m_channels[voicenr].SetPlayBuffer(sampptr, size, repeat, replen);
        m_channels[voicenr].SetVolume(volume);
        m_channels[voicenr].SetPeriod(period, m_samplerate);
        m_channels[voicenr].StartPlay();
        return true;
    }

    bool Mixer::Stop(int16_t voicenr)
    {
        if (voicenr < 0 || voicenr >= m_nrofvoices)
            return false;
        m_channels[voicenr].StopPlay();
        return true;
    }

    bool Mixer::SetPan(int16_t voicenr, bool left)
    {
        if (voicenr < 0 || voicenr >= m_nrofvoices)
            return false;
        m_channels[voicenr].SetLeftRight(left);
        return true;
    }

    bool Mixer::SetPeriod(int16_t voicenr, int32_t period)
    {
        if (voicenr < 0 || voicenr >= m_nrofvoices)
            return false;
        m_channels[voicenr].SetPeriod(period, m_samplerate);
        return true;
    }

    bool Mixer::SetVolume(int16_t voicenr, int32_t volume)
    {
        if (voicenr < 0 || voicenr >= m_nrofvoices)
            return false;
        if (volume > 64) volume = 64;
        m_channels[voicenr].SetVolume(volume);
        return true;
    }

    bool Mixer::Update(int16_t voicenr, uint8_t* memptr, int32_t offset, int32_t numofbytes)
    {
        if (voicenr < 0 || voicenr >= m_nrofvoices)
            return false;
        m_channels[voicenr].Update(memptr, offset, numofbytes);
        return true;
    }

    uint32_t Mixer::Fill(float* data, uint32_t numsamples)
    {
        numsamples = Min(numsamples, m_remainingsize);
        if (numsamples)
        {
            float ratioleft = 0;
            float ratioright = 0;
            for (int16_t i = 0; i < m_nrofvoices; i++)
            {
                if (m_channels[i].IsLeft())
                    ratioleft++;
                else
                    ratioright++;
            }
            constexpr float kGain = 0.70f;
            ratioleft = Max(ratioleft, 1.0f) * 128.0f / kGain;
            ratioright = Max(ratioright, 1.0f) * 128.0f / kGain;

            auto* ptr = m_playbuffer + (GetBufferSize() - m_remainingsize) * 2;
            for (auto size = numsamples; size; --size)
            {
                auto left = *(ptr++) / ratioleft;
                auto right = *(ptr++) / ratioright;

                *(data++) = std::lerp(right, left, m_stereoseparation);
                *(data++) = std::lerp(left, right, m_stereoseparation);
            }
            m_remainingsize -= numsamples;
        }
        return numsamples;
    }

    void Mixer::Mix()
    {
        auto* ptr = m_playbuffer;

        auto buffersize = GetBufferSize();
        for (uint32_t j = 0; j < buffersize; j++)
        {
            int32_t left = 0, right = 0;
            for (int16_t i = 0; i < m_nrofvoices; i++)
            {
                if (m_channels[i].IsLeft())
                    left += (m_channels[i].GetNextBytes() - 128);
                else
                    right += (m_channels[i].GetNextBytes() - 128);
            }
            *(ptr++) = left;
            *(ptr++) = right;
        }

        m_remainingsize = buffersize;
    }

    void Mixer::Channel::SetPlayBuffer(uint8_t* memptr, uint16_t size, uint16_t repeat, uint16_t replen)
    {
        m_memptr = memptr;
        m_size = size;
        m_repeat = repeat;
        m_replen = replen;
        m_playcursor = 0;
        m_altmemptr = nullptr;
        m_offset = 0;
        m_numofbytes = 0;
        m_count = 0;
    }

    uint8_t Mixer::Channel::GetNextBytes()
    {
        uint8_t sample = 128;
        if (m_playing)
        {
            int32_t newval;
            if (m_numofbytes != 0 && m_playcursor >= m_offset && m_playcursor <= (m_offset + m_numofbytes))
            { // updated waveform
                newval = *(m_altmemptr + m_playcursor - m_offset);
            }
            else
            {
                newval = *(m_memptr + m_playcursor);
            }
            newval = ((newval - 128) * m_volume) >> 6; //64
            sample += newval;
            m_count += kPerToFreqConst;
            if (m_count >= m_rate)
            {
                m_count -= m_rate;
                m_playcursor++;
                if (m_replen > 2)
                {
                    if (m_playcursor >= (m_repeat + m_replen))
                        m_playcursor = m_repeat;    // for repeating samples
                }
                else
                {
                    if (m_playcursor >= m_size)    // non repeating
                        m_playing = false;
                }
            }
        }
        return sample;
    }

    void Mixer::Channel::Update(uint8_t* memptr, uint16_t offset, uint16_t numofbytes)
    {
        m_altmemptr = memptr;
        m_offset = offset;
        m_numofbytes = numofbytes;
    }
}
//namespace SoundMon