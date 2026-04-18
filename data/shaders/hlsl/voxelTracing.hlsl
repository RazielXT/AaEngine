#include "hlsl/utils/normalReconstruction.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"
#include "hlsl/common/MotionVectors.hlsl"
#include "hlsl/common/ShaderOutputs.hlsl"

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

SamplerState VoxelSampler : register(s0);

GBufferOutput PSMain(PS_Input pin)
{
	SamplerState diffuseSampler = GetDynamicMaterialSamplerLinear();

	float4 albedo = GetTexture2D(TexIdDiffuse).Sample(diffuseSampler, pin.uv);

#ifdef ALPHA_TEST
	if (albedo.a < 0.37f)
		discard;
#endif

	albedo.rgb *= MaterialColor;

	float3 normalTex = DecodeNormalTexture(GetTexture2D(TexIdNormal).Sample(diffuseSampler, pin.uv).rg);

	float3x3 worldMatrix = (float3x3)WorldMatrix;
	float3 worldNormalT = normalize(mul(pin.normal, worldMatrix));
	float3 worldTangentT = normalize(mul(pin.tangent, worldMatrix));
	float3 worldBinormalT = cross(worldNormalT, worldTangentT);
	float3x3 tbn = float3x3(worldTangentT, worldBinormalT, worldNormalT);
	float3 worldNormal = mul(normalTex.xyz, tbn);

	GBufferOutput output;
	output.albedo = float4(albedo.rgb, 0);
	output.normals = float4(worldNormal, 1);

	output.motionVectors = EncodeMotionVector(pin.previousPosition, pin.currentPosition, ViewportSize);

	return output;
}
