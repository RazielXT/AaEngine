#include "heightmapGridSampling.hlsl"

struct GridTileData
{
	uint x;
	uint y;
	uint lod;
	float range;
};

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
#ifdef GRID_LOD_DEBUG
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

	// 1. Calculate grid position
	// position = (TileCoord * SmallestSize) + (LocalPlanePos * UnitsSpanned * SmallestSize)
	float4 gridPosition = float4(0,0,0,1);
	gridPosition.x += (float)tile.x * SmallestLodTileSize;
	gridPosition.z += (float)tile.y * SmallestLodTileSize;
	// Scale the unit mesh to match the LOD size
	gridPosition.x += localPos.x * (float)unitsSpanned * SmallestLodTileSize;
	gridPosition.z += localPos.y * (float)unitsSpanned * SmallestLodTileSize;

#ifndef GRID_NO_MORPHING
	// 2. Calculate Morph Factor (k)
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
#endif

	float2 uv = gridPosition.xz / (TilesWidth * SmallestLodTileSize);
	float2 heightmapUv = RemapGridTextureUV(uv);

#ifdef GRID_SAMPLE_BICUBIC
	gridPosition.y = SampleBicubic(heightMap, sampler, heightmapUv, 1024.f);
#elif defined(GRID_SAMPLE_FAST_BICUBIC)
	gridPosition.y = SampleFastBicubic(heightMap, sampler, heightmapUv, 1024.f);
#else
	gridPosition.y = heightMap.SampleLevel(sampler, heightmapUv, 0);
#endif

	gridPosition.y *= HeightScale;
	gridPosition.xyz += WorldPosition;

#ifdef GRID_LOD_DEBUG
	#ifdef GRID_NO_MORPHING
	float morphMask = 0;
	#else
	float morphMask = (k > 0 && k < 1) ? 1 : 0;
	#endif
	GridVertexInfo info = { gridPosition, uv, morphMask };
#else
	GridVertexInfo info = { gridPosition, uv };
#endif

	return info;
}
