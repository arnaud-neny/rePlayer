#ifdef _WIN64
#include <Imgui.h>

#include "Graphics.h"
#include "GraphicsFont3x5.h"
#include "GraphicsFontLog.h"
#include "GraphicsImGuiDx12.h"
#include "MediaIcons.h"

namespace rePlayer
{
    #include "GraphicsImGuiVS.h"
    #include "GraphicsImGuiPS.h"

    struct GraphicsImGui::Item : GraphicsWindow::Item
    {
        FrameResources m_frameResources[GraphicsWindow::kNumBackBuffers];
    };

    SmartPtr<GraphicsImGui> GraphicsImGui::Create()
    {
        SmartPtr<GraphicsImGui> graphicsImGui(kAllocate);
        if (graphicsImGui->Init())
            return nullptr;
        return graphicsImGui;
    }

    GraphicsImGui::GraphicsImGui()
    {}

    GraphicsImGui::~GraphicsImGui()
    {}

    bool GraphicsImGui::Init()
    {
        bool isSuccessFull = CreateRootSignature()
            && CreatePipelineState()
            && CreateTextureFont();
        return !isSuccessFull;
    }

    bool GraphicsImGui::CreateRootSignature()
    {
        D3D12_DESCRIPTOR_RANGE descRange = {};
        descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        descRange.NumDescriptors = 1;
        descRange.BaseShaderRegister = 0;
        descRange.RegisterSpace = 0;
        descRange.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER param[2] = {};

        param[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param[0].Constants.ShaderRegister = 0;
        param[0].Constants.RegisterSpace = 0;
        param[0].Constants.Num32BitValues = 16;
        param[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        param[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param[1].DescriptorTable.NumDescriptorRanges = 1;
        param[1].DescriptorTable.pDescriptorRanges = &descRange;
        param[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC staticSampler = {};
        staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSampler.MipLODBias = 0.f;
        staticSampler.MaxAnisotropy = 0;
        staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        staticSampler.MinLOD = 0.f;
        staticSampler.MaxLOD = 0.f;
        staticSampler.ShaderRegister = 0;
        staticSampler.RegisterSpace = 0;
        staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC desc = {};
        desc.NumParameters = _countof(param);
        desc.pParameters = param;
        desc.NumStaticSamplers = 1;
        desc.pStaticSamplers = &staticSampler;
        desc.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        SmartPtr<ID3DBlob> blob;
        if (D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, NULL) < S_OK)
        {
            MessageBox(nullptr, "ImGui: serialize root signature", "rePlayer", MB_ICONERROR);
            return false;
        }

        if (Graphics::GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)) < S_OK)
        {
            MessageBox(nullptr, "ImGui: root signature", "rePlayer", MB_ICONERROR);
            return false;
        }
        return true;
    }

    bool GraphicsImGui::CreatePipelineState()
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.NodeMask = 1;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.pRootSignature = m_rootSignature;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
/*
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
*/
        psoDesc.SampleDesc.Count = 1;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        psoDesc.VS = { gImGuiVS, sizeof(gImGuiVS) };
        psoDesc.PS = { gImGuiPS, sizeof(gImGuiPS) };

        static D3D12_INPUT_ELEMENT_DESC local_layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(ImDrawVert, uv),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)offsetof(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };
        psoDesc.InputLayout = { local_layout, 3 };

        // Create the blending setup
        {
            D3D12_BLEND_DESC& desc = psoDesc.BlendState;
            desc.AlphaToCoverageEnable = false;
            desc.RenderTarget[0].BlendEnable = true;
            desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
            desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;
            desc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
        }

        // Create the rasterizer state
        {
            D3D12_RASTERIZER_DESC& desc = psoDesc.RasterizerState;
            desc.FillMode = D3D12_FILL_MODE_SOLID;
            desc.CullMode = D3D12_CULL_MODE_NONE;
            desc.FrontCounterClockwise = FALSE;
            desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
            desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
            desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
            desc.DepthClipEnable = true;
            desc.MultisampleEnable = FALSE;
            desc.AntialiasedLineEnable = FALSE;
            desc.ForcedSampleCount = 0;
            desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        }

        // Create depth-stencil State
        {
            D3D12_DEPTH_STENCIL_DESC& desc = psoDesc.DepthStencilState;
            desc.DepthEnable = false;
            desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
/*
            desc.StencilEnable = true;
            desc.StencilReadMask = 0xff;
            desc.StencilWriteMask = 0xff;
            desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
            desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_REPLACE;
            desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            desc.BackFace = desc.FrontFace;
*/
        }

        if (Graphics::GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)) < S_OK)
        {
            MessageBox(nullptr, "ImGui: pipeline state", "rePlayer", MB_ICONERROR);
            return false;
        }
        return true;
    }

    bool GraphicsImGui::CreateTextureFont()
    {
        // Build texture atlas
        ImGuiIO& io = ImGui::GetIO();

        auto* font = io.Fonts->AddFontDefault();

        // add media icons
        int32_t widths[] = { 19, 19, 15, 14, 14, 25, 25, 25, 21, 25, 25, 15, 15, 15 };
        int32_t rectIds[_countof(widths)];
        for (uint32_t i = 0; i < _countof(widths); i++)
            rectIds[i] = io.Fonts->AddCustomRectFontGlyph(font, ImWchar(0xE000 + i), widths[i], 15, float(widths[i]), ImVec2(0, -1));
/*
        {
            ImFontConfig fontConfig;
            fontConfig.SizePixels = 12.0f;
            fontConfig.OversampleH = fontConfig.OversampleV = 1;
            fontConfig.PixelSnapH = true;
            fontConfig.GlyphOffset.y = 0.0f;
            strcpy_s(fontConfig.Name, "test");
            io.Fonts->AddFontFromMemoryCompressedBase85TTF(s_font_compressed_data_base85, 0, &fontConfig);
        }
*/
        {
            ImFontConfig fontConfig;
            fontConfig.SizePixels = 9.0f;
            fontConfig.OversampleH = fontConfig.OversampleV = 1;
            fontConfig.PixelSnapH = true;
            fontConfig.GlyphOffset.y = 0.0f;
#ifdef _DEBUG
            strcpy_s(fontConfig.Name, "Log");
#endif
            io.Fonts->AddFontFromMemoryCompressedBase85TTF(s_fontLog_compressed_data_base85, 0, &fontConfig);
        }
        {
            m_3x5BaseRect = io.Fonts->AddCustomRectRegular(3, 5); // first character is '!' (we don't need the ' ')
            for (int32_t i = 2; i < 16 * 6 - 1; ++i)
            {
                auto b = io.Fonts->AddCustomRectRegular(3, 5);
                assert(b == (m_3x5BaseRect + i - 1));
                (void)b;
            }
        }

        uint8_t* pixels;
        int width, height;
        io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

        uint32_t mediaIconsOffset = 0;
        for (uint32_t i = 0; i < _countof(widths); i++)
        {
            if (const ImFontAtlasCustomRect* rect = io.Fonts->GetCustomRectByIndex(rectIds[i]))
            {
                for (int32_t y = 0; y < rect->Height; y++)
                {
                    const uint8_t* src = ((const uint8_t*)s_mediaIcons) + mediaIconsOffset + 272 * y;
                    uint8_t* dst = pixels + (rect->Y + y) * width + rect->X;
                    memcpy(dst, src, widths[i]);
                }
            }
            mediaIconsOffset += widths[i];
        }
        for (int32_t i = 1; i < 16 * 6 - 1; i++)
        {
            auto x = i % 16;
            auto y = i / 16;
            if (const ImFontAtlasCustomRect* rect = io.Fonts->GetCustomRectByIndex(m_3x5BaseRect + i - 1))
            {
                for (int32_t p = 0; p < 5; p++)
                {
                    auto* src = ((const uint8_t*)s_font3x5_data) + x * 3 + (y * 5 + p) * 48;
                    auto* dst = pixels + (rect->Y + p) * width + rect->X;
                    dst[0] = src[0] * 0xff;
                    dst[1] = src[1] * 0xff;
                    dst[2] = src[2] * 0xff;
                }
            }
        }

        //temporary: create the heap
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (Graphics::GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvDescHeap)) < S_OK)
        {
            MessageBox(nullptr, "ImGui: descriptor heap", "rePlayer", MB_ICONERROR);
            return false;
        }

        m_fontSrvCpuDescHandle = m_srvDescHeap->GetCPUDescriptorHandleForHeapStart();
        m_fontSrvGpuDescHandle = m_srvDescHeap->GetGPUDescriptorHandleForHeapStart();

        // Upload texture to graphics system
        {
            D3D12_HEAP_PROPERTIES props;
            memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
            props.Type = D3D12_HEAP_TYPE_DEFAULT;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            D3D12_RESOURCE_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Alignment = 0;
            desc.Width = width;
            desc.Height = height;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Flags = D3D12_RESOURCE_FLAG_NONE;

            SmartPtr<ID3D12Resource> pTexture;
            Graphics::GetDevice()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&pTexture));

            UINT uploadPitch = (width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
            UINT uploadSize = height * uploadPitch;
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Alignment = 0;
            desc.Width = uploadSize;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            desc.Flags = D3D12_RESOURCE_FLAG_NONE;

            props.Type = D3D12_HEAP_TYPE_UPLOAD;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            SmartPtr<ID3D12Resource> uploadBuffer;
            HRESULT hr = Graphics::GetDevice()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&uploadBuffer));
            IM_ASSERT(SUCCEEDED(hr));

            void* mapped = NULL;
            D3D12_RANGE range = { 0, uploadSize };
            hr = uploadBuffer->Map(0, &range, &mapped);
            IM_ASSERT(SUCCEEDED(hr));
            for (int y = 0; y < height; y++)
                memcpy((void*)((uintptr_t)mapped + y * uploadPitch), pixels + y * width, width);
            uploadBuffer->Unmap(0, &range);

            D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
            srcLocation.pResource = uploadBuffer;
            srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_A8_UNORM;
            srcLocation.PlacedFootprint.Footprint.Width = width;
            srcLocation.PlacedFootprint.Footprint.Height = height;
            srcLocation.PlacedFootprint.Footprint.Depth = 1;
            srcLocation.PlacedFootprint.Footprint.RowPitch = uploadPitch;

            D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
            dstLocation.pResource = pTexture;
            dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLocation.SubresourceIndex = 0;

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = pTexture;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

            SmartPtr<ID3D12Fence> fence;
            hr = Graphics::GetDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
            IM_ASSERT(SUCCEEDED(hr));

            HANDLE event = CreateEvent(0, 0, 0, 0);
            IM_ASSERT(event != NULL);

            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            queueDesc.NodeMask = 1;

            SmartPtr<ID3D12CommandQueue> cmdQueue;
            hr = Graphics::GetDevice()->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
            IM_ASSERT(SUCCEEDED(hr));

            SmartPtr<ID3D12CommandAllocator> cmdAlloc;
            hr = Graphics::GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
            IM_ASSERT(SUCCEEDED(hr));

            SmartPtr<ID3D12GraphicsCommandList> cmdList;
            hr = Graphics::GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, NULL, IID_PPV_ARGS(&cmdList));
            IM_ASSERT(SUCCEEDED(hr));

            cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);
            cmdList->ResourceBarrier(1, &barrier);

            hr = cmdList->Close();
            IM_ASSERT(SUCCEEDED(hr));

            cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&cmdList);
            hr = cmdQueue->Signal(fence, 1);
            IM_ASSERT(SUCCEEDED(hr));

            fence->SetEventOnCompletion(1, event);
            WaitForSingleObject(event, INFINITE);

            CloseHandle(event);

            // Create texture view
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
            ZeroMemory(&srvDesc, sizeof(srvDesc));
            srvDesc.Format = DXGI_FORMAT_A8_UNORM;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
                D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1,
                D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1,
                D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1,
                D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3);
            Graphics::GetDevice()->CreateShaderResourceView(pTexture, &srvDesc, m_fontSrvCpuDescHandle);
            m_fontTextureResource = pTexture;
        }

        // Store our identifier
        static_assert(sizeof(ImTextureID) >= sizeof(m_fontSrvGpuDescHandle.ptr), "Can't pack descriptor handle into TexID, 32-bit not supported yet.");
        io.Fonts->SetTexID((ImTextureID)m_fontSrvGpuDescHandle.ptr);

        return true;
    }

    void GraphicsImGui::SetupRenderStates(GraphicsWindow* window, const ImDrawData& drawData, FrameResources* frameResources)
    {
        // Setup orthographic projection matrix into our constant buffer
        // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right).
        struct
        {
            float mvp[4][4];
        } vertex_constant_buffer;
        {
            float L = drawData.DisplayPos.x;
            float R = drawData.DisplayPos.x + drawData.DisplaySize.x;
            float T = drawData.DisplayPos.y;
            float B = drawData.DisplayPos.y + drawData.DisplaySize.y;
            float mvp[4][4] =
            {
                { 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
                { 0.0f,         2.0f / (T - B),     0.0f,       0.0f },
                { 0.0f,         0.0f,           0.5f,       0.0f },
                { (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
            };
            memcpy(&vertex_constant_buffer.mvp, mvp, sizeof(mvp));
        }

        // Setup viewport
        D3D12_VIEWPORT vp{};
        vp.Width = drawData.DisplaySize.x;
        vp.Height = drawData.DisplaySize.y;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = vp.TopLeftY = 0.0f;
        window->m_commandList->RSSetViewports(1, &vp);

        // Bind shader and vertex buffers
        unsigned int stride = sizeof(ImDrawVert);
        D3D12_VERTEX_BUFFER_VIEW vbv{};
        vbv.BufferLocation = frameResources->vertexBuffer->GetGPUVirtualAddress();
        vbv.SizeInBytes = frameResources->vertexBufferSize * stride;
        vbv.StrideInBytes = stride;
        window->m_commandList->IASetVertexBuffers(0, 1, &vbv);
        D3D12_INDEX_BUFFER_VIEW ibv{};
        ibv.BufferLocation = frameResources->indexBuffer->GetGPUVirtualAddress();
        ibv.SizeInBytes = frameResources->indexBufferSize * sizeof(ImDrawIdx);
        ibv.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
        window->m_commandList->IASetIndexBuffer(&ibv);
        window->m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        window->m_commandList->SetPipelineState(m_pipelineState);
        window->m_commandList->SetGraphicsRootSignature(m_rootSignature);
        window->m_commandList->SetGraphicsRoot32BitConstants(0, 16, &vertex_constant_buffer, 0);
/*
        window->m_commandList->OMSetStencilRef(1);
*/
    }

    void GraphicsImGui::OnCreateWindow(GraphicsWindow* window)
    {
        window->m_items[GraphicsWindow::kImGuiItemId] = new Item();
    }

    // Render function
    void GraphicsImGui::Render(GraphicsWindow* window, const ImDrawData& drawData)
    {
        // Avoid rendering when minimized
        if (drawData.DisplaySize.x <= 0.0f || drawData.DisplaySize.y <= 0.0f)
            return;

        window->m_commandList->SetDescriptorHeaps(1, &m_srvDescHeap);

        auto frameIndex = window->m_frameIndex;
        auto frameResources = window->m_items[GraphicsWindow::kImGuiItemId].As<Item>()->m_frameResources + (frameIndex % Graphics::kNumFrames);
        if (frameResources->frameIndex != frameIndex)
        {
            auto totalVtxCount = static_cast<uint32_t>(drawData.TotalVtxCount);
            auto totalIdxCount = static_cast<uint32_t>(drawData.TotalIdxCount);

            // Create and grow vertex/index buffers if needed
            if (frameResources->vertexBuffer == nullptr || frameResources->vertexBufferSize < totalVtxCount)
            {
                frameResources->vertexBuffer.Reset();
                frameResources->vertexBufferSize = totalVtxCount + 5000;
                D3D12_HEAP_PROPERTIES props{};
                props.Type = D3D12_HEAP_TYPE_UPLOAD;
                props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
                props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
                D3D12_RESOURCE_DESC desc{};
                desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                desc.Width = frameResources->vertexBufferSize * sizeof(ImDrawVert);
                desc.Height = 1;
                desc.DepthOrArraySize = 1;
                desc.MipLevels = 1;
                desc.Format = DXGI_FORMAT_UNKNOWN;
                desc.SampleDesc.Count = 1;
                desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                desc.Flags = D3D12_RESOURCE_FLAG_NONE;
                if (Graphics::GetDevice()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&frameResources->vertexBuffer)) < S_OK)
                    return;
            }
            if (frameResources->indexBuffer == nullptr || frameResources->indexBufferSize < totalIdxCount)
            {
                frameResources->indexBuffer.Reset();
                frameResources->indexBufferSize = totalIdxCount + 10000;
                D3D12_HEAP_PROPERTIES props{};
                props.Type = D3D12_HEAP_TYPE_UPLOAD;
                props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
                props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
                D3D12_RESOURCE_DESC desc{};
                desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                desc.Width = frameResources->indexBufferSize * sizeof(ImDrawIdx);
                desc.Height = 1;
                desc.DepthOrArraySize = 1;
                desc.MipLevels = 1;
                desc.Format = DXGI_FORMAT_UNKNOWN;
                desc.SampleDesc.Count = 1;
                desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                desc.Flags = D3D12_RESOURCE_FLAG_NONE;
                if (Graphics::GetDevice()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&frameResources->indexBuffer)) < S_OK)
                    return;
            }

            // Upload vertex/index data into a single contiguous GPU buffer
            void* vtxResource, *idxResource;
            D3D12_RANGE range{};
            if (frameResources->vertexBuffer->Map(0, &range, &vtxResource) < S_OK)
                return;
            if (frameResources->indexBuffer->Map(0, &range, &idxResource) < S_OK)
                return;
            auto vtxDst = reinterpret_cast<ImDrawVert*>(vtxResource);
            auto idxDst = reinterpret_cast<ImDrawIdx*>(idxResource);
            for (int n = 0; n < drawData.CmdListsCount; n++)
            {
                const auto cmdList = drawData.CmdLists[n];
                memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
                memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
                vtxDst += cmdList->VtxBuffer.Size;
                idxDst += cmdList->IdxBuffer.Size;
            }
            frameResources->vertexBuffer->Unmap(0, &range);
            frameResources->indexBuffer->Unmap(0, &range);

            frameResources->frameIndex = frameIndex;
        }

        // Setup desired DX state
        SetupRenderStates(window, drawData, frameResources);

        // Render command lists
        // (Because we merged all buffers into a single one, we maintain our own offset into them)
        int global_vtx_offset = 0;
        int global_idx_offset = 0;
        ImVec2 clip_off = drawData.DisplayPos;
        for (int n = 0; n < drawData.CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = drawData.CmdLists[n];
            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
            {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
                if (pcmd->UserCallback != NULL)
                {
                    // User callback, registered via ImDrawList::AddCallback()
                    // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                    if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                        SetupRenderStates(window, drawData, frameResources);
                    else
                        pcmd->UserCallback(cmd_list, pcmd);
                }
                else
                {
                    // Apply Scissor, Bind texture, Draw
                    const D3D12_RECT r = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
                    if (r.right > r.left && r.bottom > r.top)
                    {
                        window->m_commandList->SetGraphicsRootDescriptorTable(1, *(D3D12_GPU_DESCRIPTOR_HANDLE*)&pcmd->TextureId);
                        window->m_commandList->RSSetScissorRects(1, &r);
                        window->m_commandList->DrawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
                    }
                }
            }
            global_idx_offset += cmd_list->IdxBuffer.Size;
            global_vtx_offset += cmd_list->VtxBuffer.Size;
        }
    }
}
// namespace rePlayer

#endif // _WIN64