float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float4x4 PreviousWorldMatrix;
uint TexIdDiffuse;
uint TexIdNormal;
uint2 ViewportSize;
float3 MaterialColor;
float Emission;

struct VS_Input
{
	float4 p : POSITION;
	float3 n : NORMAL;
	float3 t : TANGENT;
	float2 uv : TEXCOORD0;
};

struct PS_Input
{
	float4 pos : SV_POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD0;
	float4 wp : TEXCOORD1;
	float4 currentPosition : TEXCOORD2;
	float4 previousPosition : TEXCOORD3;
};

PS_Input VSMain(VS_Input vin)
{
	PS_Input vsOut;

	vsOut.wp = mul(vin.p, WorldMatrix);
	vsOut.pos = mul(vsOut.wp, ViewProjectionMatrix);
	vsOut.uv = vin.uv;
	vsOut.normal = vin.n;
	vsOut.tangent = vin.t;
	
	float4 previousWorldPosition = mul(vin.p, PreviousWorldMatrix);
	vsOut.previousPosition = mul(previousWorldPosition, ViewProjectionMatrix);
	vsOut.currentPosition = vsOut.pos;

	return vsOut;
}

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct PSOutput
{
	float4 albedo : SV_Target0;
	float4 normals : SV_Target1;
	float4 motionVectors : SV_Target2;
};

SamplerState VoxelSampler : register(s0);

PSOutput PSMain(PS_Input pin)
{
	SamplerState diffuse_sampler = SamplerDescriptorHeap[0];

	float4 albedo = GetTexture(TexIdDiffuse).Sample(diffuse_sampler, pin.uv);

#ifdef ALPHA_TEST
	if (albedo.a < 0.37f)
		discard;
#endif

	albedo.rgb *= MaterialColor;

	float3 normalTex = float3(GetTexture(TexIdNormal).Sample(diffuse_sampler, pin.uv).rg, 1);

	float3x3 worldMatrix = (float3x3)WorldMatrix;
	float3 worldNormalT = normalize(mul(pin.normal, worldMatrix));
	float3 worldTangentT = normalize(mul(pin.tangent, worldMatrix));
	float3 worldBinormalT = cross(worldNormalT, worldTangentT);
	float3x3 tbn = float3x3(worldTangentT, worldBinormalT, worldNormalT);
	float3 worldNormal = mul(normalTex.xyz * 2 - 1, tbn);

	PSOutput output;
	output.albedo = float4(albedo.rgb, 0);
	output.normals = float4(worldNormal, 1);

	output.motionVectors = float4((pin.previousPosition.xy / pin.previousPosition.w - pin.currentPosition.xy / pin.currentPosition.w) * ViewportSize, 0, 0);
	output.motionVectors.xy *= 0.5;
	output.motionVectors.y *= -1;

	return output;
}
