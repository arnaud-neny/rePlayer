#pragma once

#include <d3d11.h>
#include <Containers/SmartPtr.h>
#include "GraphicsImGui.h"

struct ImDrawData;

namespace rePlayer
{
    class GraphicsImGuiDX11 : public GraphicsImGui
    {
        friend class SmartPtr<GraphicsImGuiDX11>;
    public:
        static SmartPtr<GraphicsImGuiDX11> Create(const GraphicsDX11* graphics);

        void Render(GraphicsWindowDX11* window, const ImDrawData& drawData);

    protected:
        GraphicsImGuiDX11(const GraphicsDX11* graphics);

    private:
        struct Texture
        {
            SmartPtr<ID3D11Texture2D> m_texture;
            SmartPtr<ID3D11ShaderResourceView> m_view;
        };

    private:
        bool OnInit() override;
        bool CreateStates();

        void SetupRenderStates(GraphicsWindowDX11* window, const ImDrawData& drawData);
        void UpdateTexture(GraphicsWindowDX11* window, ImTextureData& imTextureData);
        void DestroyTexture(ImTextureData& imTextureData) override;

    private:
        const GraphicsDX11* m_graphics;
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
    };
}
// namespace rePlayer