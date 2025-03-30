#pragma once

#ifdef _WIN64
#include <d3d12.h>
#include <dcomp.h>
#include <Containers/SmartPtr.h>
#include <Core/RefCounted.h>

#include "Graphics.h"

namespace rePlayer
{
    using namespace core;

    class GraphicsImGuiDX12;
    class GraphicsPremulDX12;

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

    struct GraphicsWindowDX12
    {
        enum
        {
            kImGuiItemId = 0,
            kItemIdCount
        };
        struct Item : public RefCounted
        {
        };

        static constexpr uint32_t kNumBackBuffers = 3;

        SmartPtr<ID3D12CommandQueue> m_commandQueue;
        SmartPtr<ID3D12GraphicsCommandList> m_commandList;

        SmartPtr<IDCompositionTarget> m_dcompTarget;
        SmartPtr<IDCompositionVisual> m_dcompVisual;
        SmartPtr<IDXGISwapChain3> m_swapChain;
        ScopedHandle m_swapChainWaitableObject;

        SmartPtr<ID3D12Fence> m_fence;
        ScopedHandle m_fenceEvent;
        uint64_t m_fenceLastSignaledValue = 0;

        struct RenderTarget
        {
            SmartPtr<ID3D12Resource> resource;
            D3D12_CPU_DESCRIPTOR_HANDLE descriptor;
        } m_backBuffers[kNumBackBuffers];

        uint32_t m_width = 0;
        uint32_t m_height = 0;

        struct FrameContext
        {
            SmartPtr<ID3D12CommandAllocator> commandAllocator;
            uint64_t fenceValue = 0;
        } m_frameContexts[kNumBackBuffers];
        FrameContext* m_currentFrameContext = m_frameContexts;
        uint64_t m_frameIndex = 0;

        class GraphicsDX12* m_graphics;
        SmartPtr<Item> m_items[kItemIdCount];//replace with render graph

        bool m_isMainWindow;

        GraphicsWindowDX12(GraphicsDX12* graphics, bool isMainWindow)
            : m_graphics(graphics)
            , m_isMainWindow(isMainWindow)
        {}
        ~GraphicsWindowDX12();
        bool Resize(uint32_t width, uint32_t height);
        void WaitForLastSubmittedFrame();
    };

    class GraphicsDX12 : public Graphics
    {
        friend struct GraphicsWindowDX12;
    public:
        GraphicsDX12(HWND hWnd);
        bool OnInit() override;
        ~GraphicsDX12() override;

        GraphicsWindowDX12* OnCreateWindow(HWND hWnd, uint32_t width, uint32_t height, bool isMainWindow = false);

        void OnBeginFrame() override;
        bool OnEndFrame(float blendingFactor) override;

        auto GetDevice() const;

        int32_t OnGet3x5BaseRect() const override;

        HWND GetHWND() const { return m_hWnd; }

    public:
        static constexpr uint32_t kNumFrames = 3;
        static constexpr uint32_t kNumBackBuffers = GraphicsWindowDX12::kNumBackBuffers;

    private:
        SmartPtr<ID3D12CommandQueue> CreateCommandQueue();
        bool CreateSwapChainForComposition(HWND hWnd, GraphicsWindowDX12* window);

        void CreateRenderTargets(GraphicsWindowDX12* window);
        void ReleaseRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE descriptor);

    private:
        SmartPtr<ID3D12Device> m_device;
        SmartPtr<IDCompositionDevice> m_dcompDevice;

        SmartPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
        uint64_t m_rtvDescritorUsage = 0;

        HWND m_hWnd;
        GraphicsWindowDX12* m_mainWindow = nullptr;

        uint32_t m_dxgiFactoryFlags = 0;

        SmartPtr<GraphicsImGuiDX12> m_imGui;
        SmartPtr<GraphicsPremulDX12> m_premul;

        bool m_isCrashed = false;
    };
}
// namespace rePlayer

#include "GraphicsDx12.inl"

#endif // _WIN64