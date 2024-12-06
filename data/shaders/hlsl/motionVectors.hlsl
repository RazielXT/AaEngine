cbuffer TemporalAA : register(b0)
{
	float4x4 reprojectionMatrix;
	uint2 ViewportSize;
};

Texture2D<float> DepthBuffer : register(t0);

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    float depth = DepthBuffer[input.Position.xy].x;
    
    float4 clipPos;
    clipPos.x = input.TexCoord.x * 2 - 1;
    clipPos.y = 1 - input.TexCoord.y * 2;
    clipPos.z = depth;
    clipPos.w = 1;

    float4 prevClipPos = mul(clipPos, reprojectionMatrix);

    if (prevClipPos.w <= 0)
        return 0;
    
    prevClipPos.xyz /= prevClipPos.w;
    float2 prevUV;
    prevUV.x = 0.5 + prevClipPos.x * 0.5;
    prevUV.y = 0.5 - prevClipPos.y * 0.5;

    float2 prevWindowPos = prevUV * ViewportSize;

	float4 output = 0;
    output.xy = prevWindowPos.xy - input.Position.xy;
	
	return output;
}
