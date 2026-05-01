#pragma once

#include <Core/Types.h>

namespace core
{
    class Surround;

    struct StereoSample
    {
        float left;
        float right;

        StereoSample* Convert(const int16_t* input, uint32_t numSamples);
        StereoSample* Convert(const int16_t* inputLeft, const int16_t* inputRight, uint32_t numSamples);
        StereoSample* ConvertMono(const float* input, uint32_t numSamples);
        StereoSample* ConvertMono(const int16_t* input, uint32_t numSamples);

        StereoSample* Convert(Surround& surround, uint32_t numSamples);
        StereoSample* Convert(Surround& surround, const float* input, uint32_t numSamples, uint32_t stereoSeparation);
        StereoSample* Convert(Surround& surround, const int16_t* input, uint32_t numSamples, uint32_t stereoSeparation);
        StereoSample* Convert(Surround& surround, const int16_t* inputLeft, const int16_t* inputRight, uint32_t numSamples, uint32_t stereoSeparation);

        StereoSample operator*(float scale) const { return { left * scale, right * scale }; }
    };

    struct LoopInfo
    {
        uint32_t start;
        uint32_t length;

        bool IsValid() const;
        uint32_t GetDuration() const;
        LoopInfo GetFixed() const;
    };
}
// namespace core