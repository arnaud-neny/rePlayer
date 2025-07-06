#pragma once

#ifdef _WIN64
#include <d3d12.h>
#include <Containers/SmartPtr.h>
#include "GraphicsImGui.h"

struct ImDrawData;

namespace rePlayer
{
    class GraphicsImGuiDX12 : public GraphicsImGui
    {
        friend class SmartPtr<GraphicsImGuiDX12>;
    public:
        static SmartPtr<GraphicsImGuiDX12> Create(GraphicsDX12* const graphics);

        void Render(GraphicsWindowDX12* window, const ImDrawData& drawData);

        void OnCreateWindow(GraphicsWindowDX12* window);

    protected:
        GraphicsImGuiDX12(GraphicsDX12* const graphics);

    private:
        struct Item;
        struct FrameResources
        {
            SmartPtr<ID3D12Resource> indexBuffer;
            SmartPtr<ID3D12Resource> vertexBuffer;
            uint32_t indexBufferSize = 0;
            uint32_t vertexBufferSize = 0;
            uint64_t frameIndex = 0;
        };

        struct Texture
        {
            SmartPtr<ID3D12Resource> m_resource;
            D3D12_CPU_DESCRIPTOR_HANDLE  m_srvCpuDescHandle = {};
            D3D12_GPU_DESCRIPTOR_HANDLE  m_srvGpuDescHandle = {};
        };

        static constexpr uint32_t kMaxDescriptors = 64;

    private:
        bool OnInit() override;

        bool CreateRootSignature();
        bool CreatePipelineState();
        bool CreateTextureFont();

        void SetupRenderStates(GraphicsWindowDX12* window, const ImDrawData& drawData, FrameResources* frameResources);
        void UpdateTexture(GraphicsWindowDX12* window, ImTextureData& imTextureData);
        void DestroyTexture(ImTextureData& imTextureData) override;

    private:
        GraphicsDX12* const m_graphics;
        SmartPtr<ID3D12RootSignature> m_rootSignature;
        SmartPtr<ID3D12PipelineState> m_pipelineState;
        SmartPtr<ID3D12DescriptorHeap> m_srvDescHeap; //temporary until the descriptor manager is available
        D3D12_CPU_DESCRIPTOR_HANDLE  m_srvCpuDescHandle = {};
        D3D12_GPU_DESCRIPTOR_HANDLE  m_srvGpuDescHandle = {};

        uint32_t m_freeDescriptors[kMaxDescriptors];
        int32_t m_lastFreeDescriptorIndex = kMaxDescriptors - 1;
    };
}
// namespace rePlayer

#endif // _WIN64