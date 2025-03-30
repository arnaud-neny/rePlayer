#pragma once

#include <Core.h>

namespace rePlayer
{
    class Graphics
    {
    public:
        static bool Init(void* hWnd);
        static void Destroy() { delete ms_instance; }

        static void BeginFrame() { ms_instance->OnBeginFrame(); }
        static bool EndFrame(float blendingFactor) { return ms_instance->OnEndFrame(blendingFactor); }

        static int32_t Get3x5BaseRect() { return ms_instance->OnGet3x5BaseRect(); }

    protected:
        virtual ~Graphics() {}
        virtual bool OnInit() = 0;

        virtual void OnBeginFrame() = 0;
        virtual bool OnEndFrame(float blendingFactor) = 0;

        virtual int32_t OnGet3x5BaseRect() const = 0;

    private:
        static Graphics* ms_instance;
    };
}
// namespace rePlayer
