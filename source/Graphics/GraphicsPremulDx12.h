#pragma once

#ifdef _WIN64
#include <d3d12.h>
#include <Containers/SmartPtr.h>
#include <Core/RefCounted.h>

namespace rePlayer
{
    class GraphicsDX12;

    class GraphicsPremulDX12 : public RefCounted
    {
        friend class SmartPtr<GraphicsPremulDX12>;
    public:
        static SmartPtr<GraphicsPremulDX12> Create(const GraphicsDX12* graphics);

        void Render(GraphicsWindowDX12* window, float blendingFactor);

    protected:
        GraphicsPremulDX12(const GraphicsDX12* graphics);
        ~GraphicsPremulDX12() override;

    private:
        bool Init();
        bool CreateRootSignature();
        bool CreatePipelineState();

    private:
        const GraphicsDX12* m_graphics;
        SmartPtr<ID3D12RootSignature> mRootSignature;
        SmartPtr<ID3D12PipelineState> mPipelineState;
    };
}
// namespace rePlayer

#endif // _WIN64