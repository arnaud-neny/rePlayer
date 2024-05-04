#pragma once

#ifndef _WIN64
#include <d3d11.h>
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

        int32_t Get3x5BaseRect() const { return m_3x5BaseRect; }

    protected:
        GraphicsImGui();
        ~GraphicsImGui() override;

    private:
        bool Init();
        bool CreateStates();
        bool CreateTextureFont();

        void SetupRenderStates(GraphicsWindow* window, const ImDrawData& drawData);

    private:
        SmartPtr<ID3D11Buffer> m_indexBuffer;
        SmartPtr<ID3D11Buffer> m_vertexBuffer;
        uint32_t m_indexBufferSize = 0;
        uint32_t m_vertexBufferSize = 0;

        SmartPtr<ID3D11VertexShader> m_vertexShader;
        SmartPtr<ID3D11InputLayout> m_inputLayout;
        SmartPtr<ID3D11Buffer> m_vertexConstantBuffer;
        SmartPtr<ID3D11PixelShader> m_pixelShader;
        SmartPtr<ID3D11SamplerState> m_fontSampler;
        SmartPtr<ID3D11ShaderResourceView> m_fontTextureView;
        SmartPtr<ID3D11RasterizerState> m_rasterizerState;
        SmartPtr<ID3D11BlendState> m_blendState;
        SmartPtr<ID3D11DepthStencilState> m_depthStencilState;

        int32_t m_3x5BaseRect = 0;
    };
}
// namespace rePlayer

#endif // _WIN64