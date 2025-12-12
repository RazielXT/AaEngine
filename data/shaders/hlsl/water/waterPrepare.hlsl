#include "../VoxelConeTracingCommon.hlsl"
#include "../postprocess/WorldReconstruction.hlsl"

float4x4 WorldViewProjectionMatrix;
float4x4 InvViewProjectionMatrix;

float2 ViewportSizeInverse;
float Time;
uint TexIdDiffuse;
uint TexIdNormal;
uint TexIdHeight;
uint TexIdSceneDepthHigh;
uint TexIdCaustics;
//uint TexIdSceneDepthLow;
//uint TexIdSceneDepthVeryLow;
//uint TexIdSceneColor;
//uint TexIdSkybox;

#ifdef ENTITY_ID
uint EntityId;
#endif

cbuffer SceneVoxelInfo : register(b1)
{
    SceneVoxelCbuffer VoxelInfo;
};

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}

TextureCube<float4> GetTextureCube(uint index)
{
    return ResourceDescriptorHeap[index];
}

SamplerState LinearWrapSampler : register(s0);
SamplerState DepthSampler : register(s1);
SamplerState PointSampler : register(s2);

float GetWavesSize(float2 uv)
{
	float4 albedo = GetTexture(TexIdHeight).SampleLevel(LinearWrapSampler, uv, 5);
	return albedo.r;
}

struct VSInput
{
	float height : TEXCOORD0;
	float2 normal : NORMAL;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD0;
};

// Function to decode a float2 packed normal back to float3
float3 DecodeNormalOctahedral(float2 p)
{
    // 1. Calculate the magnitude of the projection onto the XY plane (L1-norm)
    float abs_p_sum = abs(p.x) + abs(p.y);
    
    // 2. Calculate Z component: Z = 1 - L1-norm
    float z = 1.0f - abs_p_sum;

    // 3. Reconstruct the vector components (handling the reflection/back face)
    float3 n = float3(p.x, p.y, z);

    // 4. Handle the sign swap from the reflection step:
    // If Z < 0, then we must restore the sign of the X and Y components.
    // This is mathematically equivalent to:
    if (z < 0.0f)
    {
		n.xy = (1.0f - abs(n.yx)) * select(n.xy >= 0.0f, 1.0f, -1.0f); // CORRECTED
    }
    
    // 5. Final normalization is often required due to minor precision loss:
    return normalize(n);
}

PSInput VSMain(VSInput input, uint vertexID : SV_VertexID)
{
	PSInput result;

	const uint gridSize = 1024;
	const float cellSize = gridSize * 0.1f;

    // Compute 2D grid coordinates from vertexID
    uint x = vertexID % gridSize;
    uint y = vertexID / gridSize;

    float2 inputUv = float2((float)x / (gridSize - 1), (float)y / (gridSize - 1));

    // Generate world-space position
    float4 objPosition;
    objPosition.x = inputUv.x * cellSize;
    objPosition.z = inputUv.y * cellSize;
    objPosition.y = input.height; // height from VB
	objPosition.w = 1;

    // Generate UV based on grid coordinates
	float2 texUv = inputUv * 0.5 - Time / 18;
	
	//float waveSize = GetWavesSize(texUv) * 0.01;
	//objPosition.y += waveSize;

	result.position = mul(objPosition, WorldViewProjectionMatrix);
	result.normal = DecodeNormalOctahedral(input.normal);//normalize(mul(input.normal, (float3x3)WorldMatrix));
	result.uv = texUv / 10;

	return result;
}

float4 GetWaves(float2 uv)
{
	//uv.xy -= Time * 0.025;
	float4 albedo = GetTexture(TexIdDiffuse).Sample(LinearWrapSampler, uv);
	uv.xy -= Time * 0.007;
	albedo += GetTexture(TexIdDiffuse).Sample(LinearWrapSampler, uv);

	return albedo / 2;
}

struct PSOutput
{
	float4 color : SV_Target0;
	float4 normal : SV_Target1;
	float caustics : SV_Target2;
};

PSOutput PSMain(PSInput input)
{
	float2 ScreenUV = input.position.xy * ViewportSizeInverse;

	float3 worldPosition = float3(0,0,0);
	Texture2D DepthBufferHigh = GetTexture(TexIdSceneDepthHigh);
	float groundZ = DepthBufferHigh.Sample(LinearWrapSampler, ScreenUV).r;
	float3 groundPosition = ReconstructWorldPosition(ScreenUV, groundZ, InvViewProjectionMatrix);
	float groundDistance = length(worldPosition.xyz - groundPosition);

	const float FadeDistance = 0.1;
	float fade = groundDistance / FadeDistance;

	float4 albedo = GetWaves(input.uv);
	albedo.a = albedo.r * 0.2 + fade;
	albedo.rgb *= float3(0.2,0.8, 0.6);

	float3 voxelUV = (worldPosition.xyz-VoxelInfo.FarVoxels.Offset)/VoxelInfo.FarVoxels.SceneSize;
	Texture3D voxelmap = ResourceDescriptorHeap[VoxelInfo.FarVoxels.TexId];
	albedo.rgb *= max(0.3, (voxelmap.SampleLevel(LinearWrapSampler, voxelUV, 3).rgb + voxelmap.SampleLevel(LinearWrapSampler, voxelUV, 4).rgb * 2) * 10);

	float3 normalTex = 0.5 * (GetTexture(TexIdNormal).Sample(LinearWrapSampler,input.uv).rgr * 2.0f - 1.0f);
	normalTex += 0.5 * (GetTexture(TexIdNormal).Sample(LinearWrapSampler,input.uv * 0.7).rgr * 2.0f - 1.0f);
	normalTex.z  = sqrt(saturate(1.0f - dot(normalTex.xy, normalTex.xy)));

	float3 normal = lerp(input.normal, normalize(normalTex), 0.1);

	PSOutput output;
	output.color = albedo;
	output.normal = float4(normal,1);

	float2 projSpeed = float2(-1,-1);
	float2 projOffset = Time*projSpeed * 8;
	float projScale = 0.02;
	float projection = GetTexture(TexIdCaustics).Sample(LinearWrapSampler, (groundPosition.xy+groundPosition.z+projOffset)*projScale).r;
	projection *= GetTexture(TexIdCaustics).Sample(LinearWrapSampler, (groundPosition.xy+groundPosition.z+ float2(0.5,0.5) + projOffset*1.11)*projScale).r;
	output.caustics = projection * (worldPosition.y - groundPosition.y) / FadeDistance;

	return output;
}

#ifdef ENTITY_ID

struct PSOutputId
{
    uint4 id : SV_Target0;
    float4 position : SV_Target1;
	float4 normal : SV_Target2;
};

PSOutputId PSMainEntityId(PSInput input)
{
	PSOutputId output;
	output.id = uint4(EntityId, 0, 0, 0);
	output.position = float4(input.worldPosition.xyz, 0);
	output.normal = float4(input.normal.xyz, 0);
	return output;
}

#endif
