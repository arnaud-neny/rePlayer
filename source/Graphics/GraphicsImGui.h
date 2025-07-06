#pragma once

#include <Core/RefCounted.h>

struct ImTextureData;

namespace rePlayer
{
    using namespace core;

    class GraphicsImGui : public RefCounted
    {
    public:
        int32_t Get3x5BaseRect() const { return m_3x5BaseRect; }

    protected:
        GraphicsImGui(void* hWnd);
        bool Init(bool isTextureRGBA);

        virtual bool OnInit() = 0;
        void OnDelete() override;

        const uint8_t* GetFont3x5Data() const { return m_font3x5_data; }

        virtual void DestroyTexture(ImTextureData& imTextureData) = 0;

    private:
        void* m_hWnd;
        const uint8_t* m_font3x5_data;
        int32_t m_3x5BaseRect = 0;
    };
}
// namespace rePlayer