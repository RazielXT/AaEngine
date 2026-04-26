#include "ShadowsPssm.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"
#include "hlsl/common/BlueNoise.hlsl"
#include "hlsl/common/Srgb.hlsl"

float4x4 InvViewProjectionMatrix;
float3 CameraPosition;
uint TexIdNightSky;

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

struct VSOut
{
	float4 pos : SV_POSITION;
	float2 ndc : TEXCOORD0; // NDC in [-1,1]
};

VSOut VSMain(uint vertexID : SV_VertexID)
{
	VSOut o;

	// Fullscreen triangle trick
	float2 verts[3] = {
		float2(-1, -1),
		float2(-1, 3),
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

struct PSOutput
{
	float4 albedo : SV_Target0;
};

SamplerState LinearSampler : register(s0);

float2 SphericalUV(float3 d)
{
	float PI = 3.1416;
	float u = atan2(d.z, d.x) / (2.0 * PI) + 0.5;
	float v = asin(d.y) / PI + 0.5;
	return float2(u, v);
}

float3x3 Yaw60()
{
	float a = radians(60.0);
	float c = cos(a);
	float s = sin(a);

	return float3x3(
		 c, 0, s,
		 0, 1, 0,
		-s, 0, c
	);
}

float3x3 Roll60()
{
	float a = radians(60.0);
	float c = cos(a);
	float s = sin(a);

	return float3x3(
		 c, s, 0,
		-s, c, 0,
		 0, 0, 1
	);
}

float3 StarFieldTexture(float3 dir)
{
	float3 rotated = mul(mul(Yaw60(), Roll60()), dir);
	float2 uv = SphericalUV(rotated);

	float2 dx = ddx(uv);
	float2 dy = ddy(uv);

	// Fix the horizontal wrap-around seam
	dx.x = dx.x - round(dx.x);
	dy.x = dy.x - round(dy.x);

	// Scale down derivatives near the poles
	// to prevent the sampler from picking a tiny, blurry mipmap.
	float polarWeight = saturate(1.0 - abs(rotated.y));
	dx *= polarWeight;
	dy *= polarWeight;

	return GetTexture2D(TexIdNightSky).SampleGrad(LinearSampler, uv, dx, dy).rgb * 0.2;
}

PSOutput PSMain(VSOut input)
{
	float3 skyDir = ComputeSkyDir(input.ndc);
	float sunDot = dot(-skyDir, Sun.Direction);

	float sunZenithDot = -Sun.Direction.y;
	float viewZenithDot = skyDir.y;
	float sunViewDot01 = (sunDot + 1.0) * 0.5;
	float sunZenithDot01 = (sunZenithDot + 1.0) * 0.5;

	float3 sunZenithColor = GetTexture2D(Sun.TexIdSunZenith).Sample(LinearSampler, float2(sunZenithDot01, 0.5)).rgb * 0.5;

	float3 viewZenithColor = GetTexture2D(Sun.TexIdViewZenith).Sample(LinearSampler, float2(sunZenithDot01, 0.5)).rgb * 0.5;
	float vzMask = pow(saturate(1.0 - skyDir.y), 4);

	float3 sunViewColor = GetTexture2D(Sun.TexIdSunView).Sample(LinearSampler, float2(sunZenithDot01, 0.5)).rgb;
	float svMask = pow(saturate(sunDot), 4);

	// Night factor: fade stars in when sun is below horizon
	float night = saturate(Sun.Direction.y * 2.0);
	float3 stars = StarFieldTexture(skyDir) * night;

	float3 skyColor = stars + SrgbToLinear(sunZenithColor + vzMask * viewZenithColor + svMask * sunViewColor);

	float noise = BlueNoise(input.pos.xy);
	float ditherStrength = (2 - night) / 255.0;
	// Add noise before tone mapping
	skyColor += (noise - 0.5) * ditherStrength;

	// Sun core
	float coreSize = 0.999;
	float core = smoothstep(coreSize, 1.0, sunDot);
	float3 sunColor = core * 50 * Sun.Color;

	PSOutput output;
	output.albedo = float4(saturate(skyColor) + sunColor, 0);

	return output;
}
