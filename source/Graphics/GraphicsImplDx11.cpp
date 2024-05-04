#ifndef _WIN64
#include "GraphicsImplDx11.h"
#include "GraphicsImGuiDx11.h"

#include <Core/Log.h>
#include <Imgui.h>

#pragma comment(lib, "d3d11.lib")

#ifdef _DEBUG
#define IS_DX11_DEBUG_ENABLED 1
#else
#define IS_DX11_DEBUG_ENABLED 0
#endif

namespace rePlayer
{
    GraphicsWindow::GraphicsWindow(GraphicsImpl* graphics, bool isMainWindow)
        : m_device(graphics->m_device)
        , m_deviceContext(graphics->m_deviceContext)
        , m_isMainWindow(isMainWindow)
    {}

    GraphicsWindow::~GraphicsWindow()
    {}

    bool GraphicsWindow::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return false;

        if (m_width == width && m_height == height)
            return false;
        m_width = width;
        m_height = height;

        m_rtView.Reset();
        SmartPtr<ID3D11Texture2D> backBuffer;
        m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (backBuffer == nullptr)
            return true;
        m_device->CreateRenderTargetView(backBuffer, nullptr, &m_rtView);

        return false;
    }

    GraphicsImpl::GraphicsImpl()
    {}

    GraphicsImpl::~GraphicsImpl()
    {}

    bool GraphicsImpl::Init(HWND hWnd)
    {
        // Create the device
        {
            UINT createDeviceFlags = 0;
#if IS_DX11_DEBUG_ENABLED
            createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
            const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
            HRESULT res = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, &featureLevel, 1, D3D11_SDK_VERSION, &m_device, nullptr, &m_deviceContext);
            if (res == DXGI_ERROR_UNSUPPORTED)
                res = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, &featureLevel, 1, D3D11_SDK_VERSION, &m_device, nullptr, &m_deviceContext);
            if (res != S_OK)
                return true;

            SmartPtr<IDXGIDevice> dxgiDevice;
            SmartPtr<IDXGIAdapter> dxgiAdapter;
            if (m_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)) != S_OK
                || dxgiDevice->GetParent(IID_PPV_ARGS(&dxgiAdapter)) != S_OK
                || dxgiAdapter->GetParent(IID_PPV_ARGS(&m_dxgiFactory)) != S_OK)
                return true;

        }

        // Create ImGui Renderer
        m_imGui = GraphicsImGui::Create();
        if (!m_imGui)
            return true;

        ImGuiIO& io = ImGui::GetIO();
        io.BackendRendererUserData = this;
        struct
        {
            static void OnCreateWindow(ImGuiViewport* viewport)
            {
                ImGuiIO& io = ImGui::GetIO();
                auto graphics = reinterpret_cast<GraphicsImpl*>(io.BackendRendererUserData);
                viewport->RendererUserData = graphics->OnCreateWindow(viewport->PlatformHandleRaw ? (HWND)viewport->PlatformHandleRaw : (HWND)viewport->PlatformHandle
                    , static_cast<uint32_t>(viewport->Size.x)
                    , static_cast<uint32_t>(viewport->Size.y));
            }
            static void OnDestroyWindow(ImGuiViewport* viewport)
            {
                if (auto window = reinterpret_cast<GraphicsWindow*>(viewport->RendererUserData))
                {
                    delete window;
                    viewport->RendererUserData = nullptr;
                }
            }
            static void OnResize(ImGuiViewport* viewport, ImVec2 size)
            {
                auto window = reinterpret_cast<GraphicsWindow*>(viewport->RendererUserData);
                window->Resize(static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y));
            }
        } callbacks;

        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        platform_io.Renderer_CreateWindow = callbacks.OnCreateWindow;
        platform_io.Renderer_DestroyWindow = callbacks.OnDestroyWindow;
        platform_io.Renderer_SetWindowSize = callbacks.OnResize;

        m_mainWindow = OnCreateWindow(hWnd, 1, 1, true);

        return false;
    }

    void GraphicsImpl::Destroy()
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

        *this = GraphicsImpl();
    }

    void GraphicsImpl::BeginFrame()
    {
        ImGui::UpdatePlatformWindows();
    }

    bool GraphicsImpl::EndFrame(float blendingFactor)
    {
        (void)blendingFactor;
        float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_deviceContext->OMSetRenderTargets(1, &m_mainWindow->m_rtView, nullptr);
        m_deviceContext->ClearRenderTargetView(m_mainWindow->m_rtView, clear_color);

        // Skip the main viewport (index 0), which is always fully handled by the application!
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        for (int i = 1; i < platform_io.Viewports.Size; i++)
        {
            ImGuiViewport* viewport = platform_io.Viewports[i];
            if (viewport->Flags & ImGuiViewportFlags_IsMinimized)
                continue;

            auto window = reinterpret_cast<GraphicsWindow*>(viewport->RendererUserData);

            m_deviceContext->OMSetRenderTargets(1, &window->m_rtView, nullptr);
            m_deviceContext->ClearRenderTargetView(window->m_rtView, clear_color);

            m_imGui->Render(window, *viewport->DrawData);
        }

        for (int i = 1; i < platform_io.Viewports.Size; i++)
        {
            ImGuiViewport* viewport = platform_io.Viewports[i];
            if (viewport->Flags & ImGuiViewportFlags_IsMinimized)
                continue;

            auto window = reinterpret_cast<GraphicsWindow*>(viewport->RendererUserData);
            window->m_swapChain->Present(0, 0); // Present without vsync
        }

        m_isCrashed = m_mainWindow->m_swapChain->Present(1, 0) < 0; // Present with vsync
        return m_isCrashed;
    }

    GraphicsWindow* GraphicsImpl::OnCreateWindow(HWND hWnd, uint32_t width, uint32_t height, bool isMainWindow)
    {
        auto window = new GraphicsWindow(this, isMainWindow);
        window->m_width = width;
        window->m_height = height;

        // Create swap chain
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 1;
        sd.OutputWindow = hWnd;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        m_dxgiFactory->CreateSwapChain(m_device, &sd, &window->m_swapChain);

        // Create the render target
        if (window->m_swapChain)
        {
            SmartPtr<ID3D11Texture2D> backBuffer;
            window->m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
            m_device->CreateRenderTargetView(backBuffer, nullptr, &window->m_rtView);
        }
        else
        {
            delete window;
            return nullptr;
        }

        return window;
    }

    int32_t GraphicsImpl::Get3x5BaseRect() const
    {
        return m_imGui->Get3x5BaseRect();
    }
}
// namespace rePlayer

#endif // _WIN64