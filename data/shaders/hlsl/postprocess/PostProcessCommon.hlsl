struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

float3 ReconstructWorldPosition(float2 uv, float depth, float4x4 InvProjectionMatrix, float4x4 InvViewMatrix)
{
	// Convert UV to clip-space XY
	float4 clipPos;
	clipPos.xy = float2(uv.x * 2 - 1, (1 - uv.y) * 2 - 1); // [0..1] → [-1..1]
	clipPos.z = depth;
	clipPos.w  = 1.0f;

	float4 viewPos = mul(clipPos, InvProjectionMatrix);
	viewPos = mul(viewPos, InvViewMatrix);
	return viewPos.xyz / viewPos.w;
}
