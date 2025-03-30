#ifdef _WIN64
#include "GraphicsDx12.h"
#include "GraphicsImGuiDx12.h"
#include "GraphicsPremulDx12.h"

#include <Core/Log.h>
#include <Imgui.h>

#include <dxgi1_6.h>

#include <bit>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")

#ifdef _DEBUG
#define IS_DX12_DEBUG_ENABLED 1
#else
#define IS_DX12_DEBUG_ENABLED 0
#endif

#if IS_DX12_DEBUG_ENABLED
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

namespace rePlayer
{
    GraphicsWindowDX12::~GraphicsWindowDX12()
    {
        WaitForLastSubmittedFrame();
        for (auto& rt : m_backBuffers)
            m_graphics->ReleaseRenderTargetView(rt.descriptor);
    }

    bool GraphicsWindowDX12::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return false;

        DXGI_SWAP_CHAIN_DESC1 sd;
        m_swapChain->GetDesc1(&sd);
        if (width == sd.Width && height == sd.Height)
            return false;
        m_width = sd.Width = width;
        m_height = sd.Height = height;

        WaitForLastSubmittedFrame();

        for (auto& backBuffer : m_backBuffers)
            backBuffer.resource.Reset();

        m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, m_isMainWindow ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0);

        m_graphics->CreateRenderTargets(this);

        return false;
    }

    void GraphicsWindowDX12::WaitForLastSubmittedFrame()
    {
        auto& frame = m_frameContexts[m_frameIndex % kNumBackBuffers];

        auto fenceValue = frame.fenceValue;
        if (fenceValue != 0)
        {
            frame.fenceValue = 0;
            if (m_fence->GetCompletedValue() < fenceValue)
            {
                m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
                WaitForSingleObject(m_fenceEvent, INFINITE);
            }
        }
    }

    GraphicsDX12::GraphicsDX12(HWND hWnd)
        : m_hWnd(hWnd)
    {
        Window::ms_isPassthroughAvailable = Window::Passthrough::IsAvailable;
    }

    bool GraphicsDX12::OnInit()
    {
        m_dxgiFactoryFlags = 0;
#if IS_DX12_DEBUG_ENABLED
        // Enable the debug layer (requires the Graphics Tools "optional feature").
        // NOTE: Enabling the debug layer after device creation will invalidate the active device.
        {
            SmartPtr<ID3D12Debug> dx12Debug;
            if (D3D12GetDebugInterface(IID_PPV_ARGS(&dx12Debug)) >= S_OK)
            {
                dx12Debug->EnableDebugLayer();

                // Enable additional debug layers.
                m_dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
#endif

        // Create the device
        {
            SmartPtr<IDXGIFactory6> dxgiFactory;
            if (CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)) < 0)
                return true;
            IDXGIAdapter3* dxgiAdapter;
            for (uint32_t i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&dxgiAdapter)) != DXGI_ERROR_NOT_FOUND; i++)
            {
                DXGI_ADAPTER_DESC1 adapterDesc;
                dxgiAdapter->GetDesc1(&adapterDesc);

                if ((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
                {
                    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
                    if (FAILED(D3D12CreateDevice(dxgiAdapter, featureLevel, IID_PPV_ARGS(&m_device))))
                        continue;

//                     D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{ D3D_SHADER_MODEL_6_5 };
//                     if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))) || shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_5)
//                         m_device.Reset();
                }

                dxgiAdapter->Release();
                if (m_device)
                {
                    char desc[128];
                    ::WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, 128, desc, 128, nullptr, nullptr);
                    Log::Info("Graphics: D3D12 Adapter \"%s\"\n", desc);
                    break;
                }
            }
            if (m_device.IsInvalid())
            {
                Log::Error("Graphics: can't find a valid D3D12 Device\n");
                return true;
            }
        }

#if IS_DX12_DEBUG_ENABLED
        {
            SmartPtr<ID3D12InfoQueue> infoQueue;
            if (m_device->QueryInterface(IID_PPV_ARGS(&infoQueue)) >= S_OK)
            {
                // Suppress whole categories of messages.
                //D3D12_MESSAGE_CATEGORY categories[] = {};

                // Suppress messages based on their severity level.
                D3D12_MESSAGE_SEVERITY severities[] =
                {
                    D3D12_MESSAGE_SEVERITY_INFO,
                };

                // Suppress individual messages by their ID.
                D3D12_MESSAGE_ID denyIds[] =
                {
                    D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                    D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE //windows 11 bug
                };

                D3D12_INFO_QUEUE_FILTER filter = {};
                //filter.DenyList.NumCategories = NumItemsOf(categories);
                //filter.DenyList.pCategoryList = categories;
                filter.DenyList.NumSeverities = NumItemsOf(severities);
                filter.DenyList.pSeverityList = severities;
                filter.DenyList.NumIDs = NumItemsOf(denyIds);
                filter.DenyList.pIDList = denyIds;

                infoQueue->PushStorageFilter(&filter);

                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
                infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
            }
        }
#endif

        // Create the DirectComposition device
        if (DCompositionCreateDevice(nullptr, IID_PPV_ARGS(&m_dcompDevice)) < S_OK)
        {
            Log::Error("Graphics: DComp device\n");
            return true;
        }

        // Create the render target view descriptor heaps (max 64 entries)
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            desc.NumDescriptors = 64;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            desc.NodeMask = 1;
            if (m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvDescriptorHeap)) < S_OK)
            {
                Log::Error("Graphics: D3D12 RTV descriptor heap\n");
                return true;
            }
        }

        // Create ImGui Renderer
        m_imGui = GraphicsImGuiDX12::Create(this);
        if (!m_imGui)
            return true;

        // Create PreMul Renderer
        m_premul = GraphicsPremulDX12::Create(this);
        if (!m_premul)
            return true;

        ImGuiIO& io = ImGui::GetIO();
        io.BackendRendererUserData = this;
        struct
        {
            static void OnCreateWindow(ImGuiViewport* viewport)
            {
                ImGuiIO& io = ImGui::GetIO();
                auto graphics = reinterpret_cast<GraphicsDX12*>(io.BackendRendererUserData);
                viewport->RendererUserData = graphics->OnCreateWindow(viewport->PlatformHandleRaw ? (HWND)viewport->PlatformHandleRaw : (HWND)viewport->PlatformHandle
                    , static_cast<uint32_t>(viewport->Size.x)
                    , static_cast<uint32_t>(viewport->Size.y));
            }
            static void OnDestroyWindow(ImGuiViewport* viewport)
            {
                if (auto window = reinterpret_cast<GraphicsWindowDX12*>(viewport->RendererUserData))
                {
                    delete window;
                    viewport->RendererUserData = nullptr;
                }
            }
            static void OnResize(ImGuiViewport* viewport, ImVec2 size)
            {
                auto window = reinterpret_cast<GraphicsWindowDX12*>(viewport->RendererUserData);
                window->Resize(static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y));
            }
        } callbacks;

        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        platform_io.Renderer_CreateWindow = callbacks.OnCreateWindow;
        platform_io.Renderer_DestroyWindow = callbacks.OnDestroyWindow;
        platform_io.Renderer_SetWindowSize = callbacks.OnResize;

        m_mainWindow = OnCreateWindow(m_hWnd, 1, 1, true);

        return false;
    }

    GraphicsDX12::~GraphicsDX12()
    {
        ImGuiIO& io = ImGui::GetIO();
        if (m_isCrashed)
        {
            io.BackendRendererUserData = nullptr;

            MessageBox(nullptr, "Graphic engine has crashed", "rePlayer", MB_OK);
            return;
        }

        ImGui::DestroyPlatformWindows();
        io.BackendRendererUserData = nullptr;

        delete m_mainWindow;

        m_imGui.Reset();
        m_premul.Reset();
        m_rtvDescriptorHeap.Reset();
        m_dcompDevice.Reset();
        m_device.Reset();

#if IS_DX12_DEBUG_ENABLED
        SmartPtr<IDXGIDebug1> dxgiDebug;
        if (DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)) >= S_OK)
        {
            dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, (DXGI_DEBUG_RLO_FLAGS)(DXGI_DEBUG_RLO_DETAIL + DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
#endif
    }

    void GraphicsDX12::OnBeginFrame()
    {
        auto nextFrameIndex = m_mainWindow->m_frameIndex + 1;
        m_mainWindow->m_frameIndex = nextFrameIndex;

        HANDLE waitableObjects[] = { m_mainWindow->m_swapChainWaitableObject, nullptr };
        DWORD numWaitableObjects = 1;

        auto frameContext = m_mainWindow->m_currentFrameContext = m_mainWindow->m_frameContexts + (nextFrameIndex % kNumFrames);
        UINT64 fenceValue = frameContext->fenceValue;
        if (fenceValue != 0) // means no fence was signaled
        {
            frameContext->fenceValue = 0;
            m_mainWindow->m_fence->SetEventOnCompletion(fenceValue, m_mainWindow->m_fenceEvent);
            waitableObjects[1] = m_mainWindow->m_fenceEvent;
            numWaitableObjects = 2;
        }

        WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

        auto backBufferIdx = m_mainWindow->m_swapChain->GetCurrentBackBufferIndex();
        frameContext->commandAllocator->Reset();
        m_mainWindow->m_commandList->Reset(frameContext->commandAllocator, nullptr);

        /**/

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = m_mainWindow->m_backBuffers[backBufferIdx].resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        m_mainWindow->m_commandList->ResourceBarrier(1, &barrier);

        // Render Dear ImGui graphics
        //float clear_color[4] = { 0.45f, 0.55f, 0.60f, 1.00f };
        float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_mainWindow->m_commandList->ClearRenderTargetView(m_mainWindow->m_backBuffers[backBufferIdx].descriptor, clear_color, 0, nullptr);

        // Skip the main viewport (index 0), which is always fully handled by the application!
        ImGui::UpdatePlatformWindows();
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        for (int i = 1; i < platform_io.Viewports.Size; i++)
        {
            ImGuiViewport* viewport = platform_io.Viewports[i];
            if (viewport->Flags & ImGuiViewportFlags_IsMinimized || viewport->RendererUserData == nullptr)
                continue;

            auto window = reinterpret_cast<GraphicsWindowDX12*>(viewport->RendererUserData);

            nextFrameIndex = window->m_frameIndex + 1;
            window->m_frameIndex = nextFrameIndex;

            frameContext = window->m_currentFrameContext = window->m_frameContexts + (nextFrameIndex % kNumFrames);
            fenceValue = frameContext->fenceValue;
            while (window->m_fence->GetCompletedValue() < fenceValue)
                ::SwitchToThread();

            backBufferIdx = window->m_swapChain->GetCurrentBackBufferIndex();
            frameContext->commandAllocator->Reset();
            window->m_commandList->Reset(frameContext->commandAllocator, nullptr);

            barrier.Transition.pResource = window->m_backBuffers[backBufferIdx].resource;
            window->m_commandList->ResourceBarrier(1, &barrier);

            window->m_commandList->ClearRenderTargetView(window->m_backBuffers[backBufferIdx].descriptor, clear_color, 0, nullptr);
            window->m_commandList->OMSetRenderTargets(1, &window->m_backBuffers[backBufferIdx].descriptor, FALSE, nullptr);
        }
    }

    bool GraphicsDX12::OnEndFrame(float blendingFactor)
    {
        auto backBufferIdx = m_mainWindow->m_swapChain->GetCurrentBackBufferIndex();

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = m_mainWindow->m_backBuffers[backBufferIdx].resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        m_mainWindow->m_commandList->ResourceBarrier(1, &barrier);

        m_mainWindow->m_commandList->Close();
        m_mainWindow->m_commandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&m_mainWindow->m_commandList);

        // Skip the main viewport (index 0), which is always fully handled by the application!
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        for (int i = 1; i < platform_io.Viewports.Size; i++)
        {
            ImGuiViewport* viewport = platform_io.Viewports[i];
            if (viewport->Flags & ImGuiViewportFlags_IsMinimized)
                continue;

            auto window = reinterpret_cast<GraphicsWindowDX12*>(viewport->RendererUserData);
            backBufferIdx = window->m_swapChain->GetCurrentBackBufferIndex();

            m_imGui->Render(window, *viewport->DrawData);
            m_premul->Render(window, blendingFactor);

            barrier.Transition.pResource = window->m_backBuffers[backBufferIdx].resource;
            window->m_commandList->ResourceBarrier(1, &barrier);

            window->m_commandList->Close();
            window->m_commandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&window->m_commandList);
            window->m_swapChain->Present(0, 0);

            UINT64 fenceValue = window->m_fenceLastSignaledValue + 1;
            window->m_commandQueue->Signal(window->m_fence, fenceValue);
            window->m_fenceLastSignaledValue = fenceValue;
            window->m_currentFrameContext->fenceValue = fenceValue;
        }

        m_isCrashed = m_mainWindow->m_swapChain->Present(1, 0) < 0; // Present with vsync
        //m_isCrashed = m_mainWindow->m_swapChain->Present(0, 0) < 0; // Present without vsync
        if (m_isCrashed)
            return true;

        UINT64 fenceValue = m_mainWindow->m_fenceLastSignaledValue + 1;
        m_mainWindow->m_commandQueue->Signal(m_mainWindow->m_fence, fenceValue);
        m_mainWindow->m_fenceLastSignaledValue = fenceValue;
        m_mainWindow->m_currentFrameContext->fenceValue = fenceValue;

        return false;
    }

    SmartPtr<ID3D12CommandQueue> GraphicsDX12::CreateCommandQueue()
    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        SmartPtr<ID3D12CommandQueue> commandQueue;
        if (m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)) < 0)
            commandQueue.Reset();
        return commandQueue;
    }

    bool GraphicsDX12::CreateSwapChainForComposition(HWND hWnd, GraphicsWindowDX12* window)
    {
        SmartPtr<IDXGIFactory6> dxgiFactory;
        if (CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)) < 0)
            return true;

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount = kNumBackBuffers;
        swapChainDesc.Width = window->m_width;
        swapChainDesc.Height = window->m_height;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.Flags = window->m_isMainWindow ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0;

        SmartPtr<IDXGISwapChain1> swapChain1;
        if (dxgiFactory->CreateSwapChainForComposition(window->m_commandQueue, &swapChainDesc, nullptr, &swapChain1) < 0)
            return true;

        // Create a DirectComposition target associated with the window (pass in hWnd here)
        if (m_dcompDevice->CreateTargetForHwnd(hWnd, true, &window->m_dcompTarget) < 0)
            return true;

        // Create a DirectComposition "visual"
        if (m_dcompDevice->CreateVisual(&window->m_dcompVisual) < 0)
            return true;

        // Associate the visual with the swap chain
        if (window->m_dcompVisual->SetContent(swapChain1) < 0)
            return true;

        // Set the visual as the root of the DirectComposition target's composition tree
        if (window->m_dcompTarget->SetRoot(window->m_dcompVisual) < 0)
            return true;

        if (m_dcompDevice->Commit() < 0)
            return true;

        if (swapChain1->QueryInterface(IID_PPV_ARGS(&window->m_swapChain)) < 0)
            return true;

        if (window->m_isMainWindow)
        {
            window->m_swapChain->SetMaximumFrameLatency(kNumBackBuffers);
            window->m_swapChainWaitableObject = window->m_swapChain->GetFrameLatencyWaitableObject();
        }

        CreateRenderTargets(window);

        return false;
    }

    void GraphicsDX12::CreateRenderTargets(GraphicsWindowDX12* window)
    {
        for (uint32_t i = 0; i < kNumBackBuffers; i++)
        {
            window->m_swapChain->GetBuffer(i, IID_PPV_ARGS(&window->m_backBuffers[i].resource));
            window->m_backBuffers[i].resource->SetName(L"Swapchain");
            m_device->CreateRenderTargetView(window->m_backBuffers[i].resource, nullptr, window->m_backBuffers[i].descriptor);
        }
    }

    void GraphicsDX12::ReleaseRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE descriptor)
    {
        auto descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        auto descriptorHandle = m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_rtvDescritorUsage &= ~(1ull << ((descriptor.ptr - descriptorHandle.ptr) / descriptorSize));
    }

    GraphicsWindowDX12* GraphicsDX12::OnCreateWindow(HWND hWnd, uint32_t width, uint32_t height, bool isMainWindow)
    {
        auto window = new GraphicsWindowDX12(this, isMainWindow);
        window->m_width = width;
        window->m_height = height;

        window->m_commandQueue = CreateCommandQueue();
        if (window->m_commandQueue.IsInvalid())
            return nullptr;

        auto descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        auto descriptorHandle = m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        for (uint32_t i = 0; i < GraphicsWindowDX12::kNumBackBuffers; i++)
        {
            auto count = std::countr_one(m_rtvDescritorUsage);
            assert(count != 64);

            window->m_backBuffers[i].descriptor = descriptorHandle;
            window->m_backBuffers[i].descriptor.ptr += count * descriptorSize;
            m_rtvDescritorUsage |= 1ull << count;
        }

        if (CreateSwapChainForComposition(hWnd, window))
            return nullptr;

        if (m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&window->m_fence)) < 0)
            return nullptr;

        window->m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (window->m_fenceEvent == nullptr)
            return nullptr;

        for (auto& frameContext : window->m_frameContexts)
        {
            if (m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frameContext.commandAllocator)) < 0)
                return nullptr;
        }

        if (m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, window->m_currentFrameContext->commandAllocator, nullptr, IID_PPV_ARGS(&window->m_commandList)) < 0)
            return nullptr;


        if (window->m_commandList->Close() < 0)
            return nullptr;

        m_imGui->OnCreateWindow(window);

        return window;
    }

    int32_t GraphicsDX12::OnGet3x5BaseRect() const
    {
        return m_imGui->Get3x5BaseRect();
    }
}
// namespace rePlayer

#endif // _WIN64