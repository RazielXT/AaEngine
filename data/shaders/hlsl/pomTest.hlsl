#include "hlsl/utils/normalReconstruction.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float3 CameraPosition;
uint TexIdAlbedo;
uint TexIdNormalMap;
uint TexIdHeightMap;

struct VSInput
{
	float4 position : POSITION;
	float2 texCoord : TEXCOORD0;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float2 TexCoords : TEXCOORD0;
	float4 WorldPos : TEXCOORD1;
	float3 Normal : TEXCOORD2;
	float3 Tangent : TEXCOORD3;
};

struct PSInput
{
	float2 TexCoords : TEXCOORD0;
	float4 WorldPos : TEXCOORD1;
	float3 Normal : TEXCOORD2;
	float3 Tangent : TEXCOORD3;
};

VSOutput VSMain(VSInput IN)
{
	VSOutput vsOut;
	vsOut.WorldPos = mul(IN.position, WorldMatrix);
	vsOut.position = mul(vsOut.WorldPos, ViewProjectionMatrix);
	vsOut.TexCoords = IN.texCoord;
	vsOut.Normal = IN.normal;
	vsOut.Tangent = IN.tangent;

	return vsOut;
}

float2 ParallaxMapping(float2 texCoords, float3 viewDir, Texture2D heightmap)
{
	const float minLayers = 8.0;
	const float maxLayers = 64.0;
	float numLayers = lerp(maxLayers, minLayers, max(dot(float3(0.0, 0.0, -1.0), viewDir), 0.0)); 

	const float heightScale = 0.2;
	//const float numLayers = 32.0;

	const float layerDepth = 1.0 / numLayers;
	float currentLayerDepth = 0.0;

	float2 P = viewDir.xy * heightScale;
	float2 deltaTexCoords = P / numLayers;

	SamplerState samplerState = GetDynamicMaterialSamplerLinear();

	float2 currentTexCoords = texCoords;
	float currentDepthMapValue = heightmap.Sample(samplerState, currentTexCoords).r;

	while (currentLayerDepth < currentDepthMapValue)
	{
		currentTexCoords -= deltaTexCoords;
		currentDepthMapValue = heightmap.Sample(samplerState, currentTexCoords).r;
		currentLayerDepth += layerDepth;
	}

	// get texture coordinates before collision (reverse operations)
	float2 prevTexCoords = currentTexCoords + deltaTexCoords;

	// get depth after and before collision for linear interpolation
	float afterDepth = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = heightmap.Sample(samplerState, prevTexCoords).r - currentLayerDepth + layerDepth;

	// interpolation of texture coordinates
	float weight = afterDepth / (afterDepth - beforeDepth);
	float2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

	return finalTexCoords;
}

SamplerState ShadowSampler : register(s0);

struct PSOutput
{
	float4 color : SV_Target0;
	float4 normals : SV_Target1;
	float4 motionVectors : SV_Target2;
};

PSOutput PSMain(VSOutput input)
{
	float3 cameraView = CameraPosition - input.WorldPos.xyz;
	float camDistance = length(cameraView);

	float3 T = normalize(mul(input.Tangent, (float3x3) WorldMatrix));
	float3 N = normalize(mul(input.Normal, (float3x3) WorldMatrix));
	float3 B = normalize(cross(N, T));
	float3x3 TBN_WS = float3x3(T, B, N);
	float3x3 TBN = transpose(TBN_WS);

	float3 TangentCamPos = mul(CameraPosition, TBN);
	float3 TangentWorldPos = mul(input.WorldPos.xyz, TBN);
	float3 tangentViewDir = normalize(TangentCamPos - TangentWorldPos);
	tangentViewDir.x *= -1;

	float2 texCoords = ParallaxMapping(input.TexCoords, tangentViewDir, GetTexture2D(TexIdHeightMap));

	SamplerState samplerState = GetDynamicMaterialSamplerLinear();
	float3 albedo = GetTexture2D(TexIdAlbedo).Sample(samplerState, texCoords).rgb;
	float3 normalTex = DecodeNormalTexture(GetTexture2D(TexIdNormalMap).Sample(samplerState, texCoords).xy);

	float3 worldNormal = mul(TBN, normalTex.xyz);

/*	// Ambient
	float ambientStrength = 0.1;
	float3 ambient = ambientStrength;

	// Diffuse
	float dotLighting = saturate(dot(-Sun.Direction, worldNormal));

	float4 shadowPos = input.WorldPos;
	//{
		Texture2D depthMap = GetTexture2D(TexIdHeightMap);
		float offsetHeight = depthMap.Sample(samplerState, texCoords).r;

		// Assuming you have the surface normal and tangent space basis
		shadowPos.xyz = shadowPos.xyz - (1 - offsetHeight) * worldNormal;
	//}
	float directShadow = getPssmShadow(shadowPos, camDistance, dotLighting, ShadowSampler, Sun);
	float3 diffuse = saturate(dotLighting) * directShadow;

	// Final color
	float3 lighting = (max(ambient, diffuse)) * albedo * saturate(offsetHeight + 0.3);
*/

	{
		Texture2D depthMap = GetTexture2D(TexIdHeightMap);
		float offsetHeight = depthMap.Sample(samplerState, texCoords).r;
		albedo *= saturate(offsetHeight + 0.3);
	}
	PSOutput OUT;
	OUT.color = float4(albedo.rgb, 0);
	OUT.normals = float4(worldNormal, 1);
	OUT.motionVectors = 0;
	return OUT;
}