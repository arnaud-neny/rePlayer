cbuffer Constants : register(b0)
{
    float4 Color;
};

float4 MainVertexShader(uint id : SV_VertexID) : SV_POSITION
{
	float4 pos[3] = {
		float4(-1.0, 1.0, 0.0, 1.0),
		float4( 3.0, 1.0, 0.0, 1.0),
		float4(-1.0,-3.0, 0.0, 1.0)
	};

    return pos[id];
}

float4 MainPixelShader() : SV_Target
{
    return Color;
}