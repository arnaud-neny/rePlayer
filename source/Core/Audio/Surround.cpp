#include "Surround.h"

namespace core
{
    Surround::Context::Context(uint32_t samplingRate)
        : m_delaySize(static_cast<uint32_t>(samplingRate * 0.015f))
        , m_data(new StereoSample[m_delaySize])
    {}

    Surround::Surround(uint32_t samplingRate)
        : m_context(samplingRate)
    {
        Reset();
    }

    Surround::~Surround()
    {
        delete[] m_context.m_data;
    }

    void Surround::Reset()
    {
        memset(m_context.m_data, 0, m_context.m_delaySize * sizeof(StereoSample));
        m_context.m_delayIndex = 0; // not very useful but could help tracking bugs
    }
}
// namespace core