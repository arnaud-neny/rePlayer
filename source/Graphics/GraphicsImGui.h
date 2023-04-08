#pragma once

#include <d3d12.h>
#include <Containers/SmartPtr.h>
#include <Core/RefCounted.h>

struct ImDrawData;

namespace rePlayer
{
    class GraphicsImGui : public RefCounted
    {
        friend class SmartPtr<GraphicsImGui>;
    public:
        static SmartPtr<GraphicsImGui> Create();

        void Render(GraphicsWindow* window, const ImDrawData& drawData);

        void OnCreateWindow(GraphicsWindow* window);

    protected:
        GraphicsImGui();
        ~GraphicsImGui() override;

    private:
        struct Item;
        struct FrameResources
        {
            SmartPtr<ID3D12Resource> indexBuffer;
            SmartPtr<ID3D12Resource> vertexBuffer;
            uint32_t indexBufferSize{ 0 };
            uint32_t vertexBufferSize{ 0 };
            uint64_t frameIndex{ 0 };
        };

    private:
        bool Init();
        bool CreateRootSignature();
        bool CreatePipelineState();
        bool CreateTextureFont();

        void SetupRenderStates(GraphicsWindow* window, const ImDrawData& drawData, FrameResources* frameResources);

    private:
        SmartPtr<ID3D12RootSignature> mRootSignature;
        SmartPtr<ID3D12PipelineState> mPipelineState;
        SmartPtr<ID3D12Resource> mFontTextureResource;
        SmartPtr<ID3D12DescriptorHeap> mSrvDescHeap; //temporary until the descriptor manager is available
        D3D12_CPU_DESCRIPTOR_HANDLE  mFontSrvCpuDescHandle = {};
        D3D12_GPU_DESCRIPTOR_HANDLE  mFontSrvGpuDescHandle = {};
    };
}
// namespace rePlayer
