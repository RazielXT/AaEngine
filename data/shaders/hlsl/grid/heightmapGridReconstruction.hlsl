#include "hlsl/common/NormalDecoding.hlsl"

struct GridTileData
{
	uint x;
	uint y;
	uint lod;
	float range;
};

float2 RemapGridTextureUV(float2 gridUV)
{
#ifdef GRID_PADDING
	// We want to skip 32 pixels on the left, and use the next 960 pixels
	float margin = 32.0f / 1024.0f; // 0.03125
	float content = 960.0f / 1024.0f; // 0.9375

	return margin + (gridUV * content);
#else
	return gridUV;
#endif
}

struct GridRuntimeParams
{
	float3 cameraPos;
	float3 worldPos;

	float heightScale;
	float gridSize;
	uint tilesWidth;
	uint tileResolution;
};

struct GridVertexInfo
{
	float4 position;
	float2 uv;
#ifdef GRID_DEBUG
	float morphMask;
#endif
};

GridVertexInfo ReadGridVertexInfo(GridTileData tile, uint vertexID, Texture2D<float> heightMap, SamplerState sampler, GridRuntimeParams params)
{
	const float3 WorldPosition = params.worldPos;
	const float3 CameraPosition = params.cameraPos;
	const float HeightScale = params.heightScale;
	const float GridSize = params.gridSize;
	const uint TilesWidth = params.tilesWidth;
	const uint TileResolution = params.tileResolution;

	const uint TileWidthQuads = TileResolution - 1;
	const float SmallestLodTileSize = GridSize / TilesWidth;

	// Calculate integer X and Z indices
	uint x = vertexID % TileResolution;
	uint z = vertexID / TileResolution;
	// Normalize to 0.0 - 1.0 range
	float2 localPos = float2(x, z) / (float)(TileWidthQuads);

	// Determine how many integer units this LOD spans
	// Level 0 spans TilesWidth units
	uint unitsSpanned = TilesWidth >> tile.lod; 

	// Morph range derived from CPU subdivision geometry:
	// CPU threshold = tileSize * 3 (multiplier 1.5), so tileSize = range/3
	// Tile half-diagonal = range*sqrt(2)/6 = 0.236*range
	//
	// Fine boundary (tile just appeared, dist_to_center = range):
	//   farthest vertex = range + 0.236*range = 1.236*range -> must have k=0
	// Coarse boundary (parent removes tile, dist_to_parent_center = 2*range):
	//   nearest child vertex = 2*range - 2*0.236*range = 1.529*range -> must have k=1
	//
	// morphStart = 1.25*range (> 1.236 -> k=0 for all vertices when tile appears)
	// morphEnd   = 1.5*range  (< 1.529 -> k=1 for all vertices before removal)
	const float morphStart = tile.range * 1.25f;
	const float morphInvWidth = 4.0f / tile.range;

	// 1. Calculate grid position
	// position = (TileCoord * SmallestSize) + (LocalPlanePos * UnitsSpanned * SmallestSize)
	float4 gridPosition = float4(0,0,0,1);
	gridPosition.x += (float)tile.x * SmallestLodTileSize;
	gridPosition.z += (float)tile.y * SmallestLodTileSize;
	// Scale the unit mesh to match the LOD size
	gridPosition.x += localPos.x * (float)unitsSpanned * SmallestLodTileSize;
	gridPosition.z += localPos.y * (float)unitsSpanned * SmallestLodTileSize;

	// 2. Calculate Morph Factor (k)
	float d = distance(float3(gridPosition.x, 0, gridPosition.z), float3(CameraPosition.x - WorldPosition.x, 0, CameraPosition.z - WorldPosition.z));
	// k = 0 (no morph), k = 1 (fully collapsed to next LOD)
	float k = saturate((d - morphStart) * morphInvWidth);

	// 3. The "Collapse" Logic
	// localPos is 0.0 to 1.0. Multiply by (TilesWidth) to get vertex integer index.
	float2 fraction = frac(localPos.xy * (TileWidthQuads * 0.5f)); 
	// This tells us if we are on an "odd" vertex in the 16x16 grid relative to the next LOD

	// For center vertices (both x,z odd), match the alternating diagonal pattern
	// Coarse quad parity determines / or \ diagonal direction for collapse
	if ((x & 1) && (z & 1) && ((x / 2 + z / 2) & 1))
		fraction.x = -fraction.x;

	float2 offset = fraction * (float)unitsSpanned * SmallestLodTileSize * k;

	// Adjust the grid position: if k=1 and we are an odd vertex, 
	// we move to sit exactly on top of our even neighbor.
	gridPosition.xz -= offset * (2.0f / (float)TileWidthQuads);

	float2 uv = gridPosition.xz / (TilesWidth * SmallestLodTileSize);

	gridPosition.y = heightMap.SampleLevel(sampler, RemapGridTextureUV(uv), 0) * HeightScale;
	gridPosition.xyz += WorldPosition;

#ifdef GRID_DEBUG
	float morphMask = (k > 0 && k < 1) ? 1 : 0;
	GridVertexInfo info = { gridPosition, uv, morphMask };
#else
	GridVertexInfo info = { gridPosition, uv };
#endif

	return info;
}

float2 SampleGridNormalBlurred(Texture2D<float2> tex, SamplerState samp, float2 uv, float invResHalf)
{
	float2 s1 = tex.Sample(samp, uv + float2( invResHalf, 0));
	float2 s2 = tex.Sample(samp, uv + float2(-invResHalf, 0));
	float2 s3 = tex.Sample(samp, uv + float2(0, invResHalf));
	float2 s4 = tex.Sample(samp, uv + float2(0, -invResHalf));

	return (s1 + s2 + s3 + s4) * 0.25;
}

float3 ReadGridNormal(Texture2D<float2> texture, SamplerState sampler, float2 uv)
{
	float2 normalXZ = SampleGridNormalBlurred(texture, sampler, RemapGridTextureUV(uv), 0.5f/1024);//NormalMap.Sample(LinearSampler, uv);

	return DecodeNormalSNORM(normalXZ);
}

struct GridTBN
{
	float3 N;
	float3 T;
	float3 B;
};

GridTBN ReadGridTBN(Texture2D<float2> texture, SamplerState sampler, float2 uv)
{
	float invResHalf = 0.5f / 1024.0f;
	float2 texcoord = RemapGridTextureUV(uv);

	// 1. Sample neighboring normals to get the average surface normal
	float2 s1 = texture.Sample(sampler, texcoord + float2( invResHalf, 0));
	float2 s2 = texture.Sample(sampler, texcoord + float2(-invResHalf, 0));
	float2 s3 = texture.Sample(sampler, texcoord + float2(0,  invResHalf));
	float2 s4 = texture.Sample(sampler, texcoord + float2(0, -invResHalf));

	float2 normalXZ = (s1 + s2 + s3 + s4) * 0.25f;
	float3 N = DecodeNormalSNORM(normalXZ);

	// 2. Define a stable world-space tangent reference.
	// For most terrains, the Tangent (U direction) is simply +X (1, 0, 0).
	float3 worldTangent = float3(1, 0, 0);

	// 3. Orthonormalize the tangent to the normal (Gram-Schmidt)
	// This ensures T is perpendicular to N even on flat surfaces.
	float3 T = normalize(worldTangent - N * dot(worldTangent, N));

	// 4. Calculate Bitangent via cross product
	// This completes the basis and handles the "slope" automatically.
	float3 B = cross(N, T);

	GridTBN tbn;
	tbn.N = N;
	tbn.T = T;
	tbn.B = B;

	return tbn;
}

GridTBN ReadGridTBN_(Texture2D<float2> texture, SamplerState sampler, float2 uv)
{
	float invResHalf = 0.5f/1024;
	float2 texcoord = RemapGridTextureUV(uv);

	float2 s1 = texture.Sample(sampler, texcoord + float2( invResHalf, 0));
	float2 s2 = texture.Sample(sampler, texcoord + float2(-invResHalf, 0));
	float2 s3 = texture.Sample(sampler, texcoord + float2(0, invResHalf));
	float2 s4 = texture.Sample(sampler, texcoord + float2(0, -invResHalf));

	float2 normalXZ = (s1 + s2 + s3 + s4) * 0.25;
	float3 N = DecodeNormalSNORM(normalXZ);

	float3 dNdx = normalize(DecodeNormalSNORM((s1 - s2) * 0.5));
	float3 dNdy = normalize(DecodeNormalSNORM((s3 - s4) * 0.5));
	// Build tangent
	float3 T = dNdx - N * dot(N, dNdx);
	T = normalize(T);

	// Build bitangent
	float3 B = normalize(cross(N, T));

	GridTBN tbn;
	tbn.N = N;
	tbn.T = T;
	tbn.B = B;
	
	return tbn;
}
