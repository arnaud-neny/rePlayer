struct Vertex
{
    float2 position : POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};
Texture2D texture0 : register(t0);

SamplerState sampler0 : register(s0);

cbuffer Constants : register(b0)
{
    float4x4 ProjectionMatrix;
};

struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

PixelShaderInput MainVertexShader(Vertex vertex)//uint id : SV_VertexID
{
    //Vertex vertex = vertices[id];

    PixelShaderInput output;
    output.position = mul(ProjectionMatrix, float4(vertex.position.xy, 0.0, 1.0));
    //output.color.r = (vertex.color & 0xff) / 255.0;
    //output.color.g = ((vertex.color >> 8) & 0xff) / 255.0;
    //output.color.b = ((vertex.color >> 16) & 0xff) / 255.0;
    //output.color.a = (vertex.color >> 24) / 255.0;
	output.color = vertex.color;
    output.uv = vertex.uv;
    return output;
}

float4 MainPixelShader(PixelShaderInput input) : SV_Target
{
    float4 output = input.color * texture0.Sample(sampler0, input.uv);
    return output;
}