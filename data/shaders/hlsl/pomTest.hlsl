#include "ShadowsPssm.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float3 CameraPosition;
uint TexIdAlbedo;
uint TexIdNormalMap;
uint TexIdHeightMap;

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct VSInput
{
    float4 position	  : POSITION;
    float2 texCoord   : TEXCOORD0;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
};

struct VSOutput
{
    float4 position          : SV_POSITION; // Output position
    float2 TexCoords          : TEXCOORD0;
    float4 WorldPos : TEXCOORD1;
	float3 Normal : TEXCOORD2;
	float3 Tangent : TEXCOORD3;
};

struct PSInput
{
    float2 TexCoords          : TEXCOORD0;
    float4 WorldPos : TEXCOORD1;
	float3 Normal : TEXCOORD2;
	float3 Tangent : TEXCOORD3;
};

VSOutput VSMain(VSInput IN)
{
	VSOutput vsOut;
	vsOut.WorldPos   = mul(IN.position, WorldMatrix);   
    vsOut.position = mul(vsOut.WorldPos, ViewProjectionMatrix);  
    vsOut.TexCoords = IN.texCoord;
    //vsOut.Pos = IN.position;
    vsOut.Normal = IN.normal;
    vsOut.Tangent = IN.tangent;

    /*
    float3 T   = normalize(mul(IN.tangent, (float3x3) WorldMatrix));
	float3 binormal = normalize(cross(IN.normal, IN.tangent));
    float3 B   = normalize(mul(binormal, (float3x3) WorldMatrix));
    float3 N   = normalize(mul(IN.normal, (float3x3) WorldMatrix));
    float3x3 TBN = transpose(float3x3(T, B, N));

    vs_out.TangentLightPos = mul(Sun.Direction, TBN);
    vs_out.TangentViewPos  = mul(CameraPosition, TBN);
    vs_out.TangentFragPos  = mul(vs_out.FragPos.xyz, TBN);
    vs_out.TangentNormal  = mul(IN.normal, TBN);
	*/
	return vsOut;
}  

Texture2D GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}

float2 ParallaxMapping(float2 texCoords, float3 viewDir, Texture2D heightmap)
{
	const float minLayers = 8.0;
	const float maxLayers = 64.0;
	float numLayers = lerp(maxLayers, minLayers, max(dot(float3(0.0, 0.0, -1.0), viewDir), 0.0)); 

	const float height_scale = 0.2;
    //const float numLayers = 32.0;

    const float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;

    float2 P = viewDir.xy * height_scale;
    float2 deltaTexCoords = P / numLayers;

	SamplerState samplerState = SamplerDescriptorHeap[0];

    float2 currentTexCoords = texCoords;
    float currentDepthMapValue = heightmap.Sample(samplerState, currentTexCoords).r;

    while (currentLayerDepth < currentDepthMapValue)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = heightmap.Sample(samplerState, currentTexCoords).r;
        currentLayerDepth += layerDepth;
    }

    //return currentTexCoords;
	
	// get texture coordinates before collision (reverse operations)
	float2 prevTexCoords = currentTexCoords + deltaTexCoords;

	// get depth after and before collision for linear interpolation
	float afterDepth  = currentDepthMapValue - currentLayerDepth;
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
    float4 camDistance : SV_Target2;
    float4 motionVectors : SV_Target3;
};

// Pixel shader main function
PSOutput PSMain(VSOutput input)
{
	float3 cameraView = CameraPosition - input.WorldPos.xyz;
	float camDistance = length(cameraView);

	float3 T   = normalize(mul(input.Tangent, (float3x3) WorldMatrix));
	float3 binormal = normalize(cross(input.Normal, input.Tangent));
    float3 B   = normalize(mul(binormal, (float3x3) WorldMatrix));
    float3 N   = normalize(mul(input.Normal, (float3x3) WorldMatrix));
	float3x3 TBN_WS = float3x3(T, B, N);
    float3x3 TBN = transpose(TBN_WS);

    //float3 TangentLightPos = mul(Sun.Direction, TBN);
    float3 TangentCamPos  = mul(CameraPosition, TBN);
    float3 TangentWorldPos  = mul(input.WorldPos.xyz, TBN);
    //float3 TangentNormal  = mul(input.Normal, TBN);

    float3 tangentViewDir = normalize(TangentCamPos - TangentWorldPos);
	tangentViewDir.x *= -1;

    float2 texCoords = ParallaxMapping(input.TexCoords, tangentViewDir, GetTexture(TexIdHeightMap));

	SamplerState samplerState = SamplerDescriptorHeap[0];
    float3 albedo = GetTexture(TexIdAlbedo).Sample(samplerState, texCoords).rgb;
    float3 normalTex  = float3(GetTexture(TexIdNormalMap).Sample(samplerState, texCoords).rg, 1);
    //normal = normalize(normal * 2.0 - 1.0);
    //normal = mul(normal, TBN_WS);

	float3 bin = cross(input.Normal, input.Tangent);
    float3x3 tbn = float3x3(input.Tangent, bin, input.Normal);
	
	float3 sampledNormal = normalTex.xyz * 2 - 1;
    float3 worldNormal = mul(sampledNormal, tbn); // to object space
	worldNormal = normalize(mul(worldNormal, (float3x3)WorldMatrix));
	worldNormal.z *= -1;

	//{
		// Ambient
		float ambientStrength = 0.1;
		float3 ambient = ambientStrength;

		// Diffuse
		float dotLighting = saturate(dot(-Sun.Direction, worldNormal));

		float4 shadowPos = input.WorldPos;
		//{
			Texture2D depthMap = GetTexture(TexIdHeightMap);
			float offsetHeight = depthMap.Sample(samplerState, texCoords).r;

			// Assuming you have the surface normal and tangent space basis
			shadowPos.xyz = shadowPos.xyz - (1 - offsetHeight) * worldNormal;
		//}
		float directShadow = getPssmShadow(shadowPos, camDistance, dotLighting, ShadowSampler, Sun);
		float3 diffuse = saturate(dotLighting) * directShadow;

		// Final color
		float3 lighting = (max(ambient, diffuse)) * albedo * saturate(offsetHeight + 0.3);
	//}

	PSOutput OUT;
    OUT.color = float4(lighting, 1);
	OUT.normals = float4(worldNormal, 1);
	OUT.camDistance = 0;
	OUT.motionVectors = 0;
    return OUT;
}