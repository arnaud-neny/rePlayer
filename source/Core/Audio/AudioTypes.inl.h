#pragma once

#include "AudioTypes.h"
#include "Surround.h"

namespace core
{
    inline StereoSample* StereoSample::Convert(const int16_t* input, uint32_t numSamples, float scale)
    {
        auto output = this;
        for (; numSamples; numSamples--)
        {
            StereoSample s;
            s.left = *input++ / 32767.0f;
            s.right = *input++ / 32767.0f;
            *output++ = s * scale;
        }
        return output;
    }

    inline StereoSample* StereoSample::Convert(const int16_t* inputLeft, const int16_t* inputRight, uint32_t numSamples, float scale)
    {
        auto output = this;
        for (; numSamples; numSamples--)
        {
            StereoSample s;
            s.left = *inputLeft++ / 32767.0f;
            s.right = *inputRight++ / 32767.0f;
            *output++ = s * scale;
        }
        return output;
    }

    inline StereoSample* StereoSample::ConvertMono(const float* input, uint32_t numSamples, float scale)
    {
        auto output = this;
        for (; numSamples; numSamples--)
        {
            StereoSample s;
            s.left = s.right = scale * *input++;
            *output++ = s;
        }
        return output;
    }

    inline StereoSample* StereoSample::ConvertMono(const int16_t* input, uint32_t numSamples, float scale)
    {
        auto output = this;
        for (; numSamples; numSamples--)
        {
            StereoSample s;
            s.left = s.right = scale * *input++ / 32767.0f;
            *output++ = s;
        }
        return output;
    }

    inline StereoSample* StereoSample::Convert(Surround& surround, uint32_t numSamples, float scale)
    {
        auto output = this;
        auto ctx = surround.Begin();
        for (uint32_t i = 0; i < numSamples; i++)
            *output++ = ctx(*output * scale);
        surround.End(ctx);
        return output;
    }

    inline StereoSample* StereoSample::Convert(Surround& surround, const float* input, uint32_t numSamples, uint32_t stereoSeparation, float scale)
    {
        auto output = this;
        auto ctx = surround.Begin();
        float stereo = 0.5f - 0.5f * stereoSeparation / 100.0f;
        for (uint32_t i = 0; i < numSamples; i++)
        {
            float l = scale * *input++;
            float r = scale * *input++;
            StereoSample s;
            s.left = l + (r - l) * stereo;
            s.right = r + (l - r) * stereo;

            *output++ = ctx(s);
        }
        surround.End(ctx);
        return output;
    }

    inline StereoSample* StereoSample::Convert(Surround& surround, const int16_t* input, uint32_t numSamples, uint32_t stereoSeparation, float scale)
    {
        auto output = this;
        auto ctx = surround.Begin();
        float stereo = 0.5f - 0.5f * stereoSeparation / 100.0f;
        for (auto count = numSamples; count; --count)
        {
            float l = scale * *input++ / 32767.0f;
            float r = scale * *input++ / 32767.0f;
            StereoSample s;
            s.left = l + (r - l) * stereo;
            s.right = r + (l - r) * stereo;

            *output++ = ctx(s);
        }
        surround.End(ctx);
        return output;
    }

    inline StereoSample* StereoSample::Convert(Surround& surround, const int16_t* inputLeft, const int16_t* inputRight, uint32_t numSamples, uint32_t stereoSeparation, float scale)
    {
        auto output = this;
        auto ctx = surround.Begin();
        float stereo = 0.5f - 0.5f * stereoSeparation / 100.0f;
        for (auto count = numSamples; count; --count)
        {
            float l = scale * *inputLeft++ / 32767.0f;
            float r = scale * *inputRight++ / 32767.0f;
            StereoSample s;
            s.left = l + (r - l) * stereo;
            s.right = r + (l - r) * stereo;

            *output++ = ctx(s);
        }
        surround.End(ctx);
        return output;
    }

    inline bool LoopInfo::IsValid() const
    {
        return start != 0 || length != 0;
    }

    inline uint32_t LoopInfo::GetDuration() const
    {
        return start + length;
    }

    inline LoopInfo LoopInfo::GetFixed() const
    {
        LoopInfo loop = *this;
        if (loop.length == 0)
            std::swap(loop.start, loop.length);
        return loop;
    }
}
// namespace core