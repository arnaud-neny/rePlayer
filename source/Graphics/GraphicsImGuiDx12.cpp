#ifdef _WIN64
#include <Core/Log.h>
#include <Imgui.h>

#include "GraphicsDX12.h"
#include "GraphicsImGuiDx12.h"

extern DWORD g_imguiWindowDefaultStyle;

namespace rePlayer
{
    #include "GraphicsImGuiVS.h"
    #include "GraphicsImGuiPS.h"

    struct GraphicsImGuiDX12::Item : GraphicsWindowDX12::Item
    {
        FrameResources m_frameResources[GraphicsWindowDX12::kNumBackBuffers];
    };

    SmartPtr<GraphicsImGuiDX12> GraphicsImGuiDX12::Create(GraphicsDX12* const graphics)
    {
        SmartPtr<GraphicsImGuiDX12> graphicsImGui(kAllocate, graphics);
        if (graphicsImGui->Init(false))
            return nullptr;
        return graphicsImGui;
    }

    GraphicsImGuiDX12::GraphicsImGuiDX12(GraphicsDX12* const graphics)
        : GraphicsImGui(graphics->GetHWND())
        , m_graphics(graphics)
    {
        g_imguiWindowDefaultStyle = WS_EX_NOREDIRECTIONBITMAP;
    }

    bool GraphicsImGuiDX12::OnInit()
    {
        bool isSuccessFull = CreateRootSignature()
            && CreatePipelineState()
            && CreateTextureFont();
        return !isSuccessFull;
    }

    bool GraphicsImGuiDX12::CreateRootSignature()
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
        desc.NumParameters = NumItemsOf(param);
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
            Log::Error("ImGui: serialize root signature\n");
            return false;
        }

        if (m_graphics->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)) < S_OK)
        {
            Log::Error("ImGui: root signature\n");
            return false;
        }
        return true;
    }

    bool GraphicsImGuiDX12::CreatePipelineState()
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

        if (m_graphics->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)) < S_OK)
        {
            Log::Error("ImGui: pipeline state\n");
            return false;
        }
        return true;
    }

    bool GraphicsImGuiDX12::CreateTextureFont()
    {
        //temporary: create the heap
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = kMaxDescriptors;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (m_graphics->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvDescHeap)) < S_OK)
        {
            Log::Error("ImGui: descriptor heap\n");
            return false;
        }

        m_srvCpuDescHandle = m_srvDescHeap->GetCPUDescriptorHandleForHeapStart();
        m_srvGpuDescHandle = m_srvDescHeap->GetGPUDescriptorHandleForHeapStart();
        for (uint32_t i = 0; i < kMaxDescriptors; ++i)
            m_freeDescriptors[i] = kMaxDescriptors - i - 1;

        return true;
    }

    void GraphicsImGuiDX12::SetupRenderStates(GraphicsWindowDX12* window, const ImDrawData& drawData, FrameResources* frameResources)
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

    void GraphicsImGuiDX12::OnCreateWindow(GraphicsWindowDX12* window)
    {
        window->m_items[GraphicsWindowDX12::kImGuiItemId] = new Item();
    }

    // Render function
    void GraphicsImGuiDX12::Render(GraphicsWindowDX12* window, const ImDrawData& drawData)
    {
        // Avoid rendering when minimized
        if (drawData.DisplaySize.x <= 0.0f || drawData.DisplaySize.y <= 0.0f)
            return;

        // Catch up with texture updates. Most of the times, the list will have 1 element with an OK status, aka nothing to do.
        // (This almost always points to ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow overriding or disabling texture updates).
        if (drawData.Textures != nullptr)
            for (ImTextureData* tex : *drawData.Textures)
                if (tex->Status != ImTextureStatus_OK)
                    UpdateTexture(window, *tex);

        window->m_commandList->SetDescriptorHeaps(1, &m_srvDescHeap);

        auto frameIndex = window->m_frameIndex;
        auto frameResources = window->m_items[GraphicsWindowDX12::kImGuiItemId].As<Item>()->m_frameResources + (frameIndex % GraphicsDX12::kNumFrames);
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
                if (m_graphics->GetDevice()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&frameResources->vertexBuffer)) < S_OK)
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
                if (m_graphics->GetDevice()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&frameResources->indexBuffer)) < S_OK)
                    return;
            }

            // Upload vertex/index data into a single contiguous GPU buffer
            void* vtxResource, *idxResource;
            D3D12_RANGE range = {};
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
            range.End = uintptr_t(vtxDst) - uintptr_t(vtxResource);
            frameResources->vertexBuffer->Unmap(0, &range);
            range.End = uintptr_t(idxDst) - uintptr_t(idxResource);
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
                        window->m_commandList->SetGraphicsRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE(pcmd->GetTexID()));
                        window->m_commandList->RSSetScissorRects(1, &r);
                        window->m_commandList->DrawIndexedInstanced(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
                    }
                }
            }
            global_idx_offset += cmd_list->IdxBuffer.Size;
            global_vtx_offset += cmd_list->VtxBuffer.Size;
        }
    }

    void GraphicsImGuiDX12::UpdateTexture(GraphicsWindowDX12* window, ImTextureData& imTextureData)
    {
//         ImGui_ImplDX12_Data* bd = ImGui_ImplDX12_GetBackendData();
        bool need_barrier_before_copy = true; // Do we need a resource barrier before we copy new data in?

        if (imTextureData.Status == ImTextureStatus_WantCreate)
        {
            // Create and upload new texture to graphics system
            IM_ASSERT(imTextureData.TexID == ImTextureID_Invalid && imTextureData.BackendUserData == nullptr);
            IM_ASSERT(imTextureData.Format == ImTextureFormat_Alpha8);
            auto* backend_tex = new Texture();

            assert(m_lastFreeDescriptorIndex >= 0);
            uint32_t textureIndex = m_freeDescriptors[m_lastFreeDescriptorIndex--];
            backend_tex->m_srvCpuDescHandle.ptr = m_srvCpuDescHandle.ptr + m_graphics->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * textureIndex;
            backend_tex->m_srvGpuDescHandle.ptr = m_srvGpuDescHandle.ptr + m_graphics->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * textureIndex;

            D3D12_HEAP_PROPERTIES props = {};
            props.Type = D3D12_HEAP_TYPE_DEFAULT;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Alignment = 0;
            desc.Width = imTextureData.Width;
            desc.Height = imTextureData.Height;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Flags = D3D12_RESOURCE_FLAG_NONE;

            SmartPtr<ID3D12Resource> pTexture;
            m_graphics->GetDevice()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pTexture));

            // Create SRV
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_A8_UNORM;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
                D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1,
                D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1,
                D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1,
                D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3);
            m_graphics->GetDevice()->CreateShaderResourceView(pTexture, &srvDesc, backend_tex->m_srvCpuDescHandle);
            backend_tex->m_resource = std::move(pTexture);

            // Store identifiers
            imTextureData.SetTexID(ImTextureID(backend_tex->m_srvGpuDescHandle.ptr));
            imTextureData.BackendUserData = backend_tex;
            need_barrier_before_copy = false; // Because this is a newly-created texture it will be in D3D12_RESOURCE_STATE_COMMON and thus we don't need a barrier
            // We don't set imTextureData.Status to ImTextureStatus_OK to let the code fall through below.
        }

        if (imTextureData.Status == ImTextureStatus_WantCreate || imTextureData.Status == ImTextureStatus_WantUpdates)
        {
            Texture* backend_tex = (Texture*)imTextureData.BackendUserData;
            IM_ASSERT(imTextureData.Format == ImTextureFormat_Alpha8);

            // We could use the smaller rect on _WantCreate but using the full rect allows us to clear the texture.
            // FIXME-OPT: Uploading single box even when using ImTextureStatus_WantUpdates. Could use tex->Updates[]
            // - Copy all blocks contiguously in upload buffer.
            // - Barrier before copy, submit all CopyTextureRegion(), barrier after copy.
            const int upload_x = (imTextureData.Status == ImTextureStatus_WantCreate) ? 0 : imTextureData.UpdateRect.x;
            const int upload_y = (imTextureData.Status == ImTextureStatus_WantCreate) ? 0 : imTextureData.UpdateRect.y;
            const int upload_w = (imTextureData.Status == ImTextureStatus_WantCreate) ? imTextureData.Width : imTextureData.UpdateRect.w;
            const int upload_h = (imTextureData.Status == ImTextureStatus_WantCreate) ? imTextureData.Height : imTextureData.UpdateRect.h;

            if (imTextureData.Status == ImTextureStatus_WantCreate)
            {
                ImGuiIO& io = ImGui::GetIO();

                for (int32_t i = 1; i < 16 * 6 - 1; i++)
                {
                    auto x = i % 16;
                    auto y = i / 16;
                    ImFontAtlasRect rect;
                    if (io.Fonts->GetCustomRect(Get3x5BaseRect() + i - 1, &rect))
                    {
                        for (int32_t p = 0; p < 5; p++)
                        {
                            auto* src = GetFont3x5Data() + x * 3 + (y * 5 + p) * 48;
                            auto* dst = reinterpret_cast<uint8_t*>(imTextureData.GetPixelsAt(rect.x, rect.y + p));
                            dst[0] = src[0] * 0xff;
                            dst[1] = src[1] * 0xff;
                            dst[2] = src[2] * 0xff;
                        }
                    }
                }
            }

            // Update full texture or selected blocks. We only ever write to textures regions which have never been used before!
            // This backend choose to use tex->UpdateRect but you can use tex->Updates[] to upload individual regions.
            UINT upload_pitch_src = upload_w * imTextureData.BytesPerPixel;
            UINT upload_pitch_dst = (upload_pitch_src + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
            UINT upload_size = upload_pitch_dst * upload_h;

            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Alignment = 0;
            desc.Width = upload_size;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            desc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12_HEAP_PROPERTIES props = {};
            props.Type = D3D12_HEAP_TYPE_UPLOAD;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            // FIXME-OPT: Can upload buffer be reused?
            SmartPtr<ID3D12Resource> uploadBuffer;
            HRESULT hr = m_graphics->GetDevice()->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));
            IM_ASSERT(SUCCEEDED(hr));

            // Copy to upload buffer
            void* mapped = nullptr;
            D3D12_RANGE range = { 0, upload_size };
            hr = uploadBuffer->Map(0, &range, &mapped);
            IM_ASSERT(SUCCEEDED(hr));
            for (int y = 0; y < upload_h; y++)
                memcpy((void*)((uintptr_t)mapped + y * upload_pitch_dst), imTextureData.GetPixelsAt(upload_x, upload_y + y), upload_pitch_src);
            uploadBuffer->Unmap(0, &range);

            if (need_barrier_before_copy)
            {
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource = backend_tex->m_resource;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                window->m_commandList->ResourceBarrier(1, &barrier);
            }

            D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
            D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
            {
                srcLocation.pResource = uploadBuffer;
                srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_A8_UNORM;
                srcLocation.PlacedFootprint.Footprint.Width = upload_w;
                srcLocation.PlacedFootprint.Footprint.Height = upload_h;
                srcLocation.PlacedFootprint.Footprint.Depth = 1;
                srcLocation.PlacedFootprint.Footprint.RowPitch = upload_pitch_dst;
                dstLocation.pResource = backend_tex->m_resource;
                dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dstLocation.SubresourceIndex = 0;
            }
            window->m_commandList->CopyTextureRegion(&dstLocation, upload_x, upload_y, 0, &srcLocation, nullptr);

            {
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource = backend_tex->m_resource;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                window->m_commandList->ResourceBarrier(1, &barrier);
            }

            m_graphics->FreeResource(std::move(uploadBuffer));

            imTextureData.SetStatus(ImTextureStatus_OK);
        }

        if (imTextureData.Status == ImTextureStatus_WantDestroy && imTextureData.UnusedFrames >= int(m_graphics->kNumFrames))
            DestroyTexture(imTextureData);
    }

    void GraphicsImGuiDX12::DestroyTexture(ImTextureData& imTextureData)
    {
        auto* backend_tex = (Texture*)imTextureData.BackendUserData;

        m_freeDescriptors[++m_lastFreeDescriptorIndex] = uint32_t((backend_tex->m_srvCpuDescHandle.ptr - m_srvCpuDescHandle.ptr) / m_graphics->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

        delete backend_tex;

        imTextureData.SetTexID(ImTextureID_Invalid);
        imTextureData.SetStatus(ImTextureStatus_Destroyed);
        imTextureData.BackendUserData = nullptr;
    }
}
// namespace rePlayer

#endif // _WIN64