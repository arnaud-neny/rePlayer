#pragma once

#include "GraphicsImpl.h"

namespace rePlayer
{
    class Graphics
    {
    public:
        static constexpr uint32_t kNumBackBuffers{ GraphicsImpl::kNumBackBuffers };
        static constexpr uint32_t kNumFrames{ GraphicsImpl::kNumFrames };

    public:
        static bool Init(HWND hWnd) { return ms_instance.Init(hWnd); }
        static void Destroy() { ms_instance.Destroy(); }

        static void BeginFrame() { ms_instance.BeginFrame(); }
        static bool EndFrame(float blendingFactor) { return ms_instance.EndFrame(blendingFactor); }

        static auto GetDevice() { return ms_instance.GetDevice(); }

        static int32_t Get3x5BaseRect() { return ms_instance.Get3x5BaseRect(); }

    private:
        static GraphicsImpl ms_instance;
    };
}
// namespace rePlayer
