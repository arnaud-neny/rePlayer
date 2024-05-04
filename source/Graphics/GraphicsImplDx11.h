#pragma once

#ifndef _WIN64
#include <d3d11.h>
#include <Containers/SmartPtr.h>
#include <Core/RefCounted.h>

namespace rePlayer
{
    using namespace core;

    class GraphicsImGui;

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

    struct GraphicsWindow
    {
        static constexpr uint32_t kNumBackBuffers = 3;

        SmartPtr<IDXGISwapChain> m_swapChain;

        SmartPtr<ID3D11RenderTargetView> m_rtView;

        uint32_t m_width = 0;
        uint32_t m_height = 0;

        ID3D11Device* m_device;
        ID3D11DeviceContext* m_deviceContext;

        bool m_isMainWindow;

        GraphicsWindow(class GraphicsImpl* graphics, bool isMainWindow);
        ~GraphicsWindow();
        bool Resize(uint32_t width, uint32_t height);
    };

    class GraphicsImpl
    {
        friend struct GraphicsWindow;
    public:
        GraphicsImpl();
        ~GraphicsImpl();

        bool Init(HWND hWnd);
        void Destroy();

        GraphicsWindow* OnCreateWindow(HWND hWnd, uint32_t width, uint32_t height, bool isMainWindow = false);

        void BeginFrame();
        bool EndFrame(float blendingFactor);

        auto GetDevice() const;

        int32_t Get3x5BaseRect() const;

    public:
        static constexpr uint32_t kNumFrames = 3;
        static constexpr uint32_t kNumBackBuffers = GraphicsWindow::kNumBackBuffers;

    private:
        SmartPtr<ID3D11Device> m_device;
        SmartPtr<ID3D11DeviceContext> m_deviceContext;
        SmartPtr<IDXGIFactory> m_dxgiFactory;

        GraphicsWindow* m_mainWindow = nullptr;

        SmartPtr<GraphicsImGui> m_imGui;

        bool m_isCrashed = false;
    };
}
// namespace rePlayer

#include "GraphicsImplDx11.inl"

#endif // _WIN64