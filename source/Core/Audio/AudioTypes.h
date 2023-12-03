#pragma once

#include <Core/Types.h>

namespace core
{
    class Surround;

    struct StereoSample
    {
        float left;
        float right;

        StereoSample* Convert(const int16_t* input, uint32_t numSamples, float scale = 1.0f);
        StereoSample* Convert(const int16_t* inputLeft, const int16_t* inputRight, uint32_t numSamples, float scale = 1.0f);
        StereoSample* ConvertMono(const float* input, uint32_t numSamples, float scale = 1.0f);
        StereoSample* ConvertMono(const int16_t* input, uint32_t numSamples, float scale = 1.0f);

        StereoSample* Convert(Surround& surround, uint32_t numSamples, float scale = 1.0f);
        StereoSample* Convert(Surround& surround, const float* input, uint32_t numSamples, uint32_t stereoSeparation, float scale = 1.0f);
        StereoSample* Convert(Surround& surround, const int16_t* input, uint32_t numSamples, uint32_t stereoSeparation, float scale = 1.0f);
        StereoSample* Convert(Surround& surround, const int16_t* inputLeft, const int16_t* inputRight, uint32_t numSamples, uint32_t stereoSeparation, float scale = 1.0f);

        StereoSample operator*(float scale) const { return { left * scale, right * scale }; }
    };
}
// namespace core