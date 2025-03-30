#pragma once

#include <d3d11.h>
#include <Containers/SmartPtr.h>
#include <Core/RefCounted.h>

#include "Graphics.h"

namespace rePlayer
{
    using namespace core;

    class GraphicsImGuiDX11;

/*
    struct ScopedHandle
    {
        ~ScopedHandle()
        {
            if (handle)
                CloseHandle(handle);
        }

        ScopedHandle& operator=(HANDLE other)
        {
            if (handle != NULL)
                ::CloseHandle(handle);
            handle = other;
            return *this;
        }

        operator HANDLE() const { return handle; }

        HANDLE handle = NULL;
    };
*/

    struct GraphicsWindowDX11
    {
        static constexpr uint32_t kNumBackBuffers = 3;

        SmartPtr<IDXGISwapChain> m_swapChain;

        SmartPtr<ID3D11RenderTargetView> m_rtView;

        uint32_t m_width = 0;
        uint32_t m_height = 0;

        ID3D11Device* m_device;
        ID3D11DeviceContext* m_deviceContext;

        bool m_isMainWindow;

        GraphicsWindowDX11(class GraphicsDX11* graphics, bool isMainWindow);
        ~GraphicsWindowDX11();
        bool Resize(uint32_t width, uint32_t height);
    };

    class GraphicsDX11 : public Graphics
    {
        friend struct GraphicsWindowDX11;
    public:
        GraphicsDX11(HWND hWnd);
        bool OnInit() override;
        ~GraphicsDX11() override;

        GraphicsWindowDX11* OnCreateWindow(HWND hWnd, uint32_t width, uint32_t height, bool isMainWindow = false);

        void OnBeginFrame() override;
        bool OnEndFrame(float blendingFactor) override;

        auto GetDevice() const;

        int32_t OnGet3x5BaseRect() const override;

        HWND GetHWND() const { return m_hWnd; }

    public:
        static constexpr uint32_t kNumFrames = 3;
        static constexpr uint32_t kNumBackBuffers = GraphicsWindowDX11::kNumBackBuffers;

    private:
        SmartPtr<ID3D11Device> m_device;
        SmartPtr<ID3D11DeviceContext> m_deviceContext;
        SmartPtr<IDXGIFactory> m_dxgiFactory;

        HWND m_hWnd;
        GraphicsWindowDX11* m_mainWindow = nullptr;

        SmartPtr<GraphicsImGuiDX11> m_imGui;

        bool m_isCrashed = false;
    };
}
// namespace rePlayer

#include "GraphicsDx11.inl"