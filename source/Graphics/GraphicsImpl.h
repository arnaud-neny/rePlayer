#pragma once

#include <d3d12.h>
#include <dcomp.h>
#include <Containers/SmartPtr.h>
#include <Core/RefCounted.h>

namespace rePlayer
{
    using namespace core;

    class GraphicsImGui;
    class GraphicsPremul;

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

        HANDLE handle{ NULL };
    };

    struct GraphicsWindow
    {
        enum
        {
            kImGuiItemId = 0,
            kItemIdCount
        };
        struct Item : public RefCounted
        {
        };

        static constexpr uint32_t kNumBackBuffers{ 3 };

        SmartPtr<ID3D12CommandQueue> m_commandQueue;
        SmartPtr<ID3D12GraphicsCommandList> m_commandList;

        SmartPtr<IDCompositionTarget> m_dcompTarget;
        SmartPtr<IDCompositionVisual> m_dcompVisual;
        SmartPtr<IDXGISwapChain3> m_swapChain;
        ScopedHandle m_swapChainWaitableObject;

        SmartPtr<ID3D12Fence> m_fence;
        ScopedHandle m_fenceEvent;
        uint64_t m_fenceLastSignaledValue{ 0 };

        struct RenderTarget
        {
            SmartPtr<ID3D12Resource> resource;
            D3D12_CPU_DESCRIPTOR_HANDLE descriptor;
        } m_backBuffers[kNumBackBuffers];

        uint32_t m_width{ 0 };
        uint32_t m_height{ 0 };

        struct FrameContext
        {
            SmartPtr<ID3D12CommandAllocator> commandAllocator;
            uint64_t fenceValue{ 0 };
        } m_frameContexts[kNumBackBuffers];
        FrameContext* m_currentFrameContext{ m_frameContexts };
        uint64_t m_frameIndex{ 0 };

        class GraphicsImpl* m_graphics;
        SmartPtr<Item> m_items[kItemIdCount];//replace with render graph

        bool m_isMainWindow;

        GraphicsWindow(GraphicsImpl* graphics, bool isMainWindow)
            : m_graphics{ graphics }
            , m_isMainWindow{ isMainWindow }
        {}
        ~GraphicsWindow();
        bool Resize(uint32_t width, uint32_t height);
        void WaitForLastSubmittedFrame();
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

    public:
        static constexpr uint32_t kNumFrames{ 3 };
        static constexpr uint32_t kNumBackBuffers{ 3 };

    private:
        SmartPtr<ID3D12CommandQueue> CreateCommandQueue();
        bool CreateSwapChainForComposition(HWND hWnd, GraphicsWindow* window);

        void CreateRenderTargets(GraphicsWindow* window);
        void ReleaseRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE descriptor);

    private:
        SmartPtr<ID3D12Device> m_device;
        SmartPtr<IDCompositionDevice> m_dcompDevice;

        SmartPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
        uint64_t m_rtvDescritorUsage{ 0 };

        GraphicsWindow* m_mainWindow{ nullptr };

        uint32_t m_dxgiFactoryFlags{ 0 };

        SmartPtr<GraphicsImGui> m_imGui;
        SmartPtr<GraphicsPremul> m_premul;

        bool m_isCrashed = false;
    };
}
// namespace rePlayer

#include "GraphicsImpl.inl"