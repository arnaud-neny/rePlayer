#pragma once

#ifdef _WIN64
#include <d3d12.h>
#include <Containers/SmartPtr.h>
#include <Core/RefCounted.h>

struct ImDrawData;

namespace rePlayer
{
    class GraphicsImGuiDX12 : public RefCounted
    {
        friend class SmartPtr<GraphicsImGuiDX12>;
    public:
        static SmartPtr<GraphicsImGuiDX12> Create(const GraphicsDX12* graphics);

        void Render(GraphicsWindowDX12* window, const ImDrawData& drawData);

        void OnCreateWindow(GraphicsWindowDX12* window);

        int32_t Get3x5BaseRect() const { return m_3x5BaseRect; }

    protected:
        GraphicsImGuiDX12(const GraphicsDX12* graphics);
        ~GraphicsImGuiDX12() override;

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

    private:
        bool Init();
        bool CreateRootSignature();
        bool CreatePipelineState();
        bool CreateTextureFont();

        void SetupRenderStates(GraphicsWindowDX12* window, const ImDrawData& drawData, FrameResources* frameResources);

    private:
        const GraphicsDX12* m_graphics;
        SmartPtr<ID3D12RootSignature> m_rootSignature;
        SmartPtr<ID3D12PipelineState> m_pipelineState;
        SmartPtr<ID3D12Resource> m_fontTextureResource;
        SmartPtr<ID3D12DescriptorHeap> m_srvDescHeap; //temporary until the descriptor manager is available
        D3D12_CPU_DESCRIPTOR_HANDLE  m_fontSrvCpuDescHandle = {};
        D3D12_GPU_DESCRIPTOR_HANDLE  m_fontSrvGpuDescHandle = {};
        int32_t m_3x5BaseRect = 0;
    };
}
// namespace rePlayer

#endif // _WIN64