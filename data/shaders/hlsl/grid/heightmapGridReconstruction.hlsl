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
	// Level 0 spans TilesWidth units, Level 4 spans 1 unit
	uint unitsSpanned = TilesWidth >> tile.lod; 

	const float2 MorphRange = float2(tile.range * 1.45f, tile.range * 0.2f);

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
	float k = saturate((d - MorphRange.x) * MorphRange.y);

	// 3. The "Collapse" Logic
	// localPos is 0.0 to 1.0. Multiply by (TilesWidth) to get vertex integer index.
	float2 fraction = frac(localPos.xy * (TileWidthQuads * 0.5f)); 
	// This tells us if we are on an "odd" vertex in the 16x16 grid relative to the next LOD
	float2 offset = fraction * (float)unitsSpanned * SmallestLodTileSize * k;

	// Adjust the grid position: if k=1 and we are an odd vertex, 
	// we move to sit exactly on top of our even neighbor.
	gridPosition.xz -= offset * (2.0f / (float)TileWidthQuads);

	float2 uv = gridPosition.xz / (TilesWidth * SmallestLodTileSize);

	gridPosition.y = heightMap.SampleLevel(sampler, uv, 0) * HeightScale;
	gridPosition.xyz += WorldPosition;

	GridVertexInfo info = { gridPosition, uv };
	return info;
}
