#include "../VoxelConeTracingCommon.hlsl"
#include "../postprocess/WorldReconstruction.hlsl"

float4x4 WorldMatrix;
float4x4 ViewProjectionMatrix;
float4x4 InvViewProjectionMatrix;

float2 ViewportSizeInverse;
float Time;
uint TexIdDiffuse;
uint TexIdNormal;
uint TexIdHeight;
uint TexIdSceneDepthHigh;
uint TexIdCaustics;
uint TexIdWaterInfoTexture;
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
	float4 worldPosition : TEXCOORD1;
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

PSInput VSMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
	const uint GlobalGridSize = 1024;
    const uint ChunksPerSide = 16;
    const uint ChunkSize = GlobalGridSize / ChunksPerSide;
    const float cellSize = GlobalGridSize * 0.1f;

    // --- 2. Compute Local Coordinates (Within the 128x128 chunk) ---
    // x_local: 0 to 127
    uint x_local = vertexID % ChunkSize;
    // y_local: 0 to 127
    uint y_local = vertexID / ChunkSize;
    
    // Compute normalized UV for the local chunk (0 to 1.0)
    // We use (ChunkSize - 1) because the 128x128 vertices define 127x127 quads.
    float2 localUv = float2((float)x_local / (ChunkSize - 1), (float)y_local / (ChunkSize - 1));
    
    // --- 3. Compute Instance Offset (Where the 128x128 chunk starts globally) ---
    
    // i_chunk: Instance column (0 to 7)
    uint i_chunk = instanceID % ChunksPerSide;
    // j_chunk: Instance row (0 to 7)
    uint j_chunk = instanceID / ChunksPerSide;
    
    // Global UV Offset (Normalized position where this chunk starts globally)
    // Each chunk occupies 1/8th of the normalized UV space (1.0 / 8 = 0.125)
    float2 globalUvOffset = float2((float)i_chunk / ChunksPerSide, 
                                   (float)j_chunk / ChunksPerSide);

    // --- 4. Compute Global Coordinates ---
    
    // Global UV: normalized coordinate across the entire 1024x1024 grid (0 to 1.0)
    // The local UV [0, 1] is mapped to the chunk's 1/8th slot.
    float2 inputUv = globalUvOffset + (localUv * (1.0f / ChunksPerSide));

    // Compute Global Vertex Indices (0 to 1023) if needed for wave function:
    // uint x_global = i_chunk * (ChunkSize - 1) + x_local;
    // uint y_global = j_chunk * (ChunkSize - 1) + y_local;
    
    // --- 5. Generate World-Space Position (Using the original logic) ---
    
    float4 objPosition;
    objPosition.x = inputUv.x * cellSize; // Global X position
    objPosition.z = inputUv.y * cellSize; // Global Z position
	objPosition.w = 1;

    // Generate UV based on grid coordinates
	float2 texUv = inputUv * 0.5 - Time / 18;
	
	//float waveSize = GetWavesSize(texUv) * 0.01;
	//objPosition.y += waveSize;

	float3 waterInfo = GetTexture(TexIdWaterInfoTexture).SampleLevel(LinearWrapSampler, inputUv, 0).xyz;
	objPosition.y = waterInfo.x;

    PSInput result;
	result.worldPosition = mul(objPosition, WorldMatrix);
	result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.normal = DecodeNormalOctahedral(waterInfo.yz);//normalize(mul(input.normal, (float3x3)WorldMatrix));
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

	Texture2D DepthBufferHigh = GetTexture(TexIdSceneDepthHigh);
	float groundZ = DepthBufferHigh.Sample(LinearWrapSampler, ScreenUV).r;
	float3 groundPosition = ReconstructWorldPosition(ScreenUV, groundZ, InvViewProjectionMatrix);
	float groundDistance = length(input.worldPosition.xyz - groundPosition);

	const float FadeDistance = 0.5f;
	float fade = groundDistance / FadeDistance;

	float4 albedo = GetWaves(input.uv);
	albedo.a = albedo.r * 0.2 + fade;
	albedo.rgb *= float3(0.2,0.8, 0.6);

	float3 voxelUV = (input.worldPosition.xyz-VoxelInfo.FarVoxels.Offset)/VoxelInfo.FarVoxels.SceneSize;
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
	output.caustics = projection * (input.worldPosition.y - groundPosition.y) / FadeDistance;

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
	output.position = input.worldPosition;
	output.normal = float4(input.normal.xyz, 0);
	return output;
}

#endif
