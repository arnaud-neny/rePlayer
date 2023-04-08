#pragma once

#include <d3d12.h>
#include <Containers/SmartPtr.h>
#include <Core/RefCounted.h>

namespace rePlayer
{
    class GraphicsPremul : public RefCounted
    {
        friend class SmartPtr<GraphicsPremul>;
    public:
        static SmartPtr<GraphicsPremul> Create();

        void Render(GraphicsWindow* window, float blendingFactor);

    protected:
        GraphicsPremul();
        ~GraphicsPremul() override;

    private:
        bool Init();
        bool CreateRootSignature();
        bool CreatePipelineState();

    private:
        SmartPtr<ID3D12RootSignature> mRootSignature;
        SmartPtr<ID3D12PipelineState> mPipelineState;
    };
}
// namespace rePlayer
