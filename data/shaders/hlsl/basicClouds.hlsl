#include "ShadowsPssm.hlsl"

float4x4 ViewProjectionMatrix;
uint TexIdNoises;
uint TexIdNoisesLow;
float3 CameraPosition;
float Time;

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

struct PSInput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD0;
	float3 worldPosition : TEXCOORD1;
};

SamplerState LinearWrapSampler : register(s0);

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
}

float GetHeight(float2 uv)
{
    float noiseHeight  = GetTexture(TexIdNoises).SampleLevel(LinearWrapSampler, (uv + Time * Sun.CloudsSpeed), 0).x;
    float noiseHeight2 = GetTexture(TexIdNoises).SampleLevel(LinearWrapSampler, (uv + Time * Sun.CloudsSpeed * 2.1) / 2, 0).z;
    return (noiseHeight + noiseHeight2 + Sun.CloudsAmount) / 2;
}

PSInput VSMain(uint vertexID : SV_VertexID)
{
	const float gridWidth = 256;
	const float heightScale = 12;
	const float halfSize = 50;
	const float falloffRadius = 1;

	uint x = vertexID % gridWidth;
	uint y = vertexID / gridWidth;
	float2 uv = float2(x, y) / float2(gridWidth - 1, gridWidth - 1);
	float2 posXZ = uv * 2.0f - 1.0f; // [-1,1] range

	float dist = length(posXZ);
	//float falloff = exp(-pow(dist / falloffRadius, 2.0f)); // Gaussian
	float falloff = smoothstep(1.0f, 0.0f, dist / falloffRadius);
	float height = falloff * heightScale;

	const float scale = 1000;
	float4 worldPosition = float4(posXZ.x * halfSize, height, posXZ.y * halfSize, 1);

	float noiseHeight = GetHeight(uv);

	worldPosition.y += noiseHeight * 5 - 5;
	worldPosition.xyz *= scale;

	PSInput result;
	result.uv = uv;
	result.worldPosition = worldPosition.xyz;
	result.position = mul(worldPosition, ViewProjectionMatrix);
	result.normal = normalize(float3(uv.x - 0.5f, 0.05f + noiseHeight, uv.y - 0.5f));

	return result;
}

struct PSOutput
{
	float4 albedo : SV_Target0;
};

float borderBlend(float2 uv, float edgeThreshold)
{
	// Calculate the distance from the UV coordinates to the nearest edge
	float left = uv.x;
	float right = 1.0 - uv.x;
	float top = uv.y;
	float bottom = 1.0 - uv.y;

	// Determine the minimum distance to an edge
	float minDist = min(min(left, right), min(top, bottom));

	// Perform linear interpolation (lerp) based on the dist / edgeThresholdance to the edge
	float alpha = saturate(minDist / edgeThreshold);

	return alpha;
}

float3 SrgbToLinear(float3 srgbColor)
{
	return pow(srgbColor, 2.233333333);
}

float3 getFogColor(float3 dir)
{
	float sunZenithDot = -Sun.Direction.y;
	float sunZenithDot01 = (sunZenithDot + 1.0) * 0.5;

	float3 sunZenithColor = GetTexture(Sun.TexIdSunZenith).Sample(LinearWrapSampler, float2(sunZenithDot01, 0.5)).rgb;

	float3 viewZenithColor = GetTexture(Sun.TexIdViewZenith).Sample(LinearWrapSampler, float2(sunZenithDot01, 0.5)).rgb * float3(1,0.5,0.5);;
	float vzMask = pow(saturate(1.0 - dir.y), 4);

	return SrgbToLinear(sunZenithColor + viewZenithColor * vzMask);
}

float3 applyFog(float3 worldPosition, float3 baseColor)
{
	float3 fogAtmDir = normalize(CameraPosition - worldPosition);
	fogAtmDir.y = saturate(fogAtmDir.y);
	const float3 fogColor = getFogColor(fogAtmDir) / 2;
	float camDistance = length(CameraPosition.xz - worldPosition.xz);

	float fogDensity = 0.000000001;
	float fogFactor = 1.0 - exp(-camDistance * camDistance * fogDensity);
	return lerp(baseColor.rgb, fogColor, fogFactor * fogFactor);
}

float3 getSkyAmbientColor(float3 worldPosition)
{
	float3 fogAtmDir = normalize(CameraPosition - worldPosition);
	fogAtmDir.y = saturate(fogAtmDir.y);
	return getFogColor(fogAtmDir) / 2;
}

PSOutput PSMain(PSInput input)
{
	SamplerState sampler = SamplerDescriptorHeap[0];

	float2 borders = input.uv;
	float fade = borderBlend(borders, 0.2f);

	float weight = GetTexture(TexIdNoises).Sample(sampler, (input.uv + Time * Sun.CloudsSpeed)).x + Sun.CloudsAmount * 0.5;
	float weight2 = GetTexture(TexIdNoises).Sample(sampler, (input.uv + Time * Sun.CloudsSpeed * 2.1) / 2).z + Sun.CloudsAmount * 0.5;
	weight = weight * weight2 * 3;

	float weightDetail = GetTexture(TexIdNoises).Sample(sampler, (input.uv + Time * Sun.CloudsSpeed * 0.81) * 10).z;
	weightDetail *= GetTexture(TexIdNoises).Sample(sampler, (input.uv + float2(0, 0.5) + Time * Sun.CloudsSpeed * 1.1331) * 9).z;
	weight = smoothstep(0.4, 1, weight - weightDetail * 0.3 + 0.1);

	float eps = 0.01f;
	float h  = GetHeight(input.uv) + weightDetail * 0.05;
	float hx = GetHeight(input.uv + float2(eps, 0));
	float hy = GetHeight(input.uv + float2(0, eps));
	float3 dx = float3(eps, hx - h, 0);
	float3 dy = float3(0, hy - h, eps);
	float3 normal = cross(dy, dx);
	normal.y -= saturate(weight) * 0.001;
	normal = -normalize(normal);

	float3 cameraDirection = normalize(CameraPosition - input.worldPosition);
	float normalDot = saturate(dot(cameraDirection, -normal) * 0.5 + 0.5);
	float3 globeNormal = input.normal;
	float3 finalNormal = normalize(lerp(globeNormal, normal, saturate(normalDot * 0.8 - Sun.CloudsAmount * 0.8)));

	float faceLighting = dot(Sun.Direction, finalNormal);
	float lighting = saturate(faceLighting);
	float globeLighting = saturate(-dot(Sun.Direction, globeNormal));
	float fullLighting = pow(faceLighting * 0.5 + 0.5, 1);
	float ambientLighting = (1 - faceLighting) * (abs(Sun.Direction.y) * 0.8 + 0.2);
	float edgesLighting = pow(globeLighting * (1 - weight), 2);

	float3 cloudAlbedo = lerp(float3(0.5,0.5,0.5), float3(0.17,0.21,0.3), Sun.CloudsDensity);
	float3 ambientAlbedo = getSkyAmbientColor(input.worldPosition);

	float3 cloudsColor = cloudAlbedo;
	{
		float3 cloudAmbient = fullLighting * fullLighting;
		//cloudAmbient += 0.25 - saturate(Sun.CloudsDensity * Sun.CloudsAmount * ambientLighting) * 0.25;
		cloudAmbient += edgesLighting; //cloudAmbient += globeLighting * (1 - weight) * 2;
		cloudsColor *= Sun.Color * cloudAmbient;
		
		float ambientTerm = abs(Sun.Direction.y) * 0.3;
		//cloudsColor = (cloudsColor) * (1 - ambientTerm) + Sun.Color * (fullLighting * 0.6 + 0.3) * ambientTerm;
	}
	{
		float3 sunLighting = Sun.Color * saturate(edgesLighting + lighting);
		cloudsColor += cloudAlbedo * sunLighting;
	}
	cloudsColor = applyFog(input.worldPosition, cloudsColor);

	float alphaTex = weight;

	float3 finalColor = cloudsColor * 0.5;
	//finalColor = Sun.Color * lerp((abs(Sun.Direction.y) * fullLighting * 0.5 + 0.2) * cloudAlbedo, cloudAlbedo, saturate(edgesLighting + lighting));
	//finalColor = fullLighting.xxx;

	PSOutput output;
	output.albedo = float4(finalColor, smoothstep(0,0.5,alphaTex) * fade * fade);

	return output;
}