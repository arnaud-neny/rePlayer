#ifdef _WIN64
#include "Graphics.h"
#include "GraphicsPremulDx12.h"

namespace rePlayer
{
    #include "GraphicsPremulVS.h"
    #include "GraphicsPremulPS.h"

    SmartPtr<GraphicsPremul> GraphicsPremul::Create()
    {
        SmartPtr<GraphicsPremul> graphicsPremul(kAllocate);
        if (graphicsPremul->Init())
            return nullptr;
        return graphicsPremul;
    }

    GraphicsPremul::GraphicsPremul()
    {
    }

    GraphicsPremul::~GraphicsPremul()
    {
    }

    bool GraphicsPremul::Init()
    {
        bool isSuccessFull = CreateRootSignature()
            && CreatePipelineState();
        return !isSuccessFull;
    }

    bool GraphicsPremul::CreateRootSignature()
    {
        D3D12_ROOT_PARAMETER param = {};

        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param.Constants.ShaderRegister = 0;
        param.Constants.RegisterSpace = 0;
        param.Constants.Num32BitValues = 4;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC desc = {};
        desc.NumParameters = 1;
        desc.pParameters = &param;
        desc.NumStaticSamplers = 0;
        desc.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        SmartPtr<ID3DBlob> blob;
        if (D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, NULL) < S_OK)
        {
            MessageBox(nullptr, "Premul: serialize root signature", "rePlayer", MB_ICONERROR);
            return false;
        }

        if (Graphics::GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)) < S_OK)
        {
            MessageBox(nullptr, "Premul: root signature", "rePlayer", MB_ICONERROR);
            return false;
        }
        return true;
    }

    bool GraphicsPremul::CreatePipelineState()
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.NodeMask = 1;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.pRootSignature = mRootSignature;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        psoDesc.VS = { gPremulVS, sizeof(gPremulVS) };
        psoDesc.PS = { gPremulPS, sizeof(gPremulPS) };

        // Create the blending setup
        {
            D3D12_BLEND_DESC& desc = psoDesc.BlendState;
            desc.AlphaToCoverageEnable = false;
            desc.RenderTarget[0].BlendEnable = true;
            desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
            desc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_ALPHA;
            desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE | D3D12_COLOR_WRITE_ENABLE_ALPHA;
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
            desc.StencilWriteMask = 0;
            desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            desc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
            desc.BackFace = desc.FrontFace;
*/
        }

        if (Graphics::GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPipelineState)) < S_OK)
        {
            MessageBox(nullptr, "Premul: pipeline state", "rePlayer", MB_ICONERROR);
            return false;
        }
        return true;
    }

    // Render function
    void GraphicsPremul::Render(GraphicsWindow* window, float blendingFactor)
    {
        float color[4] = { 1.0f, 1.0f, 1.0f, blendingFactor };

        window->m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        window->m_commandList->SetPipelineState(mPipelineState);
        window->m_commandList->SetGraphicsRootSignature(mRootSignature);
        window->m_commandList->SetGraphicsRoot32BitConstants(0, 4, color, 0);
/*
        window->m_commandList->OMSetStencilRef(1);
*/
        const D3D12_RECT r = { 0, 0, static_cast<LONG>(window->m_width), static_cast<LONG>(window->m_height) };
        window->m_commandList->RSSetScissorRects(1, &r);

        window->m_commandList->DrawInstanced(3, 1, 0, 0);
    }
}
// namespace rePlayer

#endif // _WIN64