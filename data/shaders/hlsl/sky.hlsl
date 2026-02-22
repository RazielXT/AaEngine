#include "ShadowsPssm.hlsl"
#include "hlsl/utils/blueNoise.hlsl"

float4x4 InvViewProjectionMatrix;
float3 CameraPosition;
uint ResId;

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

struct VSOut
{
	float4 pos  : SV_POSITION;
	float2 ndc  : TEXCOORD0;   // NDC in [-1,1]
};

VSOut VSMain(uint vertexID : SV_VertexID)
{
	VSOut o;

	// Fullscreen triangle trick
	float2 verts[3] = {
		float2(-1, -1),
		float2(-1,  3),
		float2( 3, -1)
	};

	float2 ndc = verts[vertexID];
	o.pos = float4(ndc, 0, 1);
	o.ndc = ndc;

	return o;
}

float3 ComputeSkyDir(float2 ndc)
{
	// Reconstruct clip-space position
	float4 clip = float4(ndc, 1.0, 1.0);

	// Transform to world space
	float4 world = mul(clip, InvViewProjectionMatrix);
	world /= world.w;

	// Ray direction from camera
	return normalize(world.xyz - CameraPosition);
}

float3 SrgbToLinear(float3 srgbColor)
{
	return pow(srgbColor, 2.233333333);
}

struct PSOutput
{
	float4 albedo : SV_Target0;
};

SamplerState LinearSampler : register(s0);

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
}

PSOutput PSMain(VSOut input)
{
	float3 skyDir = ComputeSkyDir(input.ndc);
	float sunDot = dot(-skyDir, Sun.Direction);

	float sunZenithDot = -Sun.Direction.y;
	float viewZenithDot = skyDir.y;
	float sunViewDot01 = (sunDot + 1.0) * 0.5;
	float sunZenithDot01 = (sunZenithDot + 1.0) * 0.5;

	float3 sunZenithColor = GetTexture(Sun.TexIdSunZenith).Sample(LinearSampler, float2(sunZenithDot01, 0.5)).rgb * 0.5;

	float3 viewZenithColor = GetTexture(Sun.TexIdViewZenith).Sample(LinearSampler, float2(sunZenithDot01, 0.5)).rgb * 0.5 * float3(1,0.5,0.5);
	float vzMask = pow(saturate(1.0 - skyDir.y), 4);

	float3 sunViewColor = GetTexture(Sun.TexIdSunView).Sample(LinearSampler, float2(sunZenithDot01, 0.5)).rgb * float3(1,0.5,0.5);
	float svMask = pow(saturate(sunDot), 4);

	float3 skyColor = SrgbToLinear(sunZenithColor + vzMask * viewZenithColor + svMask * sunViewColor);

	float noise = BlueNoise(input.pos.xy);
	float ditherStrength = 2.0 / 255.0;
	// Add noise before tone mapping
	skyColor += noise * ditherStrength;

	// Sun core
	float coreSize = 0.999;
	float core = smoothstep(coreSize, 1.0, sunDot);
	float3 sunColor = core * 50 * Sun.Color;

	PSOutput output;
	output.albedo = float4(saturate(skyColor) + sunColor, 0);

	return output;
}
