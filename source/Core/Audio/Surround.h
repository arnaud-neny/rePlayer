#pragma once

#include "AudioTypes.h"

#include <Core.h>

namespace core
{
    class Surround
    {
    public:
        class Context
        {
            friend class Surround;
        public:
            StereoSample operator()(const StereoSample& sample);

        private:
            Context(uint32_t samplingRate);

        private:
            const uint32_t m_delaySize;
            uint32_t m_delayIndex{ 0 };
            StereoSample* const m_data;
            bool m_isEnabled;
        };

    public:
        Surround(uint32_t samplingRate);
        ~Surround();

        void Enable(bool isEnabled);
        bool IsEnabled() const;

        Context Begin() const;
        void End(const Context& context);

        void Reset();

    private:
        Context m_context;
    };

    inline StereoSample Surround::Context::operator()(const StereoSample& sample)
    {
        StereoSample d = m_data[m_delayIndex];
        m_data[m_delayIndex] = sample;
        m_delayIndex = (m_delayIndex + 1) % m_delaySize;

        StereoSample output = sample;
        if (m_isEnabled)
        {
            output.left = (d.right + sample.left) / 1.414f;
            output.right = (d.left + sample.right) / 1.414f;
        }
        return output;
    }

    inline void Surround::Enable(bool isEnabled)
    {
        m_context.m_isEnabled = isEnabled;
    }

    inline bool Surround::IsEnabled() const
    {
        return m_context.m_isEnabled;
    }

    inline Surround::Context Surround::Begin() const
    {
        return m_context;
    }

    inline void Surround::End(const Context& context)
    {
        m_context.m_delayIndex = context.m_delayIndex;
    }
}
// namespace core