#include "ShadowsPssm.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 InvViewMatrix;
uint TexIdDiffuse;
uint TexIdNormal;
float MoonPhase;

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 worldPosition : TEXCOORD1;
};

static const uint IndexBuffer[6] = { 0, 1, 2, 2, 1, 3 };

static const float2 coords[4] = {
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 0.0f)
};

PSInput VSMain(uint vertexIdx : SV_VertexID)
{
	PSInput output;

	// --- UV and quad coordinates ---
	float2 uv = coords[IndexBuffer[vertexIdx]];

	// Convert UV [0,1] → centered quad [-0.5, 0.5]
	float2 centered = uv - 0.5f;

	// Billboard size
	float scale = 100000.0f;

	// Local quad position (square, centered)
	float3 localPos = float3(centered.x, centered.y, 0.0f) * scale;

	// --- Billboard orientation using camera basis ---
	float3 camRight = normalize(InvViewMatrix[0].xyz);
	float3 camUp    = normalize(InvViewMatrix[1].xyz);

	// Build transform matrix (no forward axis needed)
	float3 worldPos =
		localPos.x * camRight +
		localPos.y * camUp;

	// Push billboard far away in sun direction
	worldPos += Sun.Direction * scale * 10;

	// Output
	output.worldPosition = float4(worldPos, 1.0f);
	output.position = mul(output.worldPosition, ViewProjectionMatrix);
	output.uv = uv;

	return output;
}

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct PSOutput
{
	float4 albedo : SV_Target0;
};

static const float AlphaThreshold = 0.8f;

SamplerState LinearWrapSampler : register(s0);

float3 SrgbToLinear(float3 srgbColor)
{
	return pow(srgbColor, 2.233333333);
}

float3 getFogColor(float3 dir)
{
	float sunZenithDot = -Sun.Direction.y;
	float sunZenithDot01 = (sunZenithDot + 1.0) * 0.5;

	float3 sunZenithColor = GetTexture(Sun.TexIdSunZenith).Sample(LinearWrapSampler, float2(sunZenithDot01, 0.5)).rgb;

	float3 viewZenithColor = GetTexture(Sun.TexIdViewZenith).Sample(LinearWrapSampler, float2(sunZenithDot01, 0.5)).rgb;
	float vzMask = pow(saturate(1.0 - dir.y), 4);

	return SrgbToLinear(sunZenithColor + viewZenithColor * vzMask);
}

PSOutput PSMain(PSInput input)
{
	SamplerState colorSampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(colorSampler, input.uv);
	float3 normalMap = GetTexture(TexIdNormal).Sample(colorSampler, input.uv).xyz * 2 - 1;

	if (albedo.a < AlphaThreshold) discard;

	{
		float2 uv = input.uv;

		// Convert UV to [-1,1] for sphere projection
		float2 p = uv.xy * 2 - 1;

		// Outside circle = transparent
		float r2 = dot(p, p);
		// Reconstruct sphere normal
		float3 normal = float3(p.x, p.y, sqrt(1 - r2));
		normal = (normal + normalMap.xyz * 0.3) / 1.3;
		float angle = MoonPhase * 3.14;
		float3 lightDir = float3(cos(angle), 0, sin(angle));

		// Compute moon phase (lit side)
		float phase = dot(normal, lightDir);
		phase = saturate(phase);
		//phase = smoothstep(0.0, 0.1, phase);

		albedo.rgb *= phase;
	}

	albedo.rgb = lerp(getFogColor(Sun.Direction) * 0.25, albedo.rgb, saturate(Sun.Direction.y));

	PSOutput output;
	output.albedo = float4(albedo.rgb, 1);

	return output;
}
