cbuffer SimParams : register(b0)
{
	uint2 gridSize;	// simulation dimensions
	float dt; // timestep
	float gravity; // gravity constant
	float cellSize; // grid spacing
	uint ResIdHeightMap;
	uint ResIdWaterMap;
	uint ResIdVelocityMap;
	uint ResIdOutWaterMap;
	uint ResIdOutVelocityMap;
};

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
	Texture2D<float> TerrainHeight = ResourceDescriptorHeap[ResIdHeightMap];
	Texture2D<float> LastWaterHeight = ResourceDescriptorHeap[ResIdWaterMap];
	Texture2D<float2> LastVelocity = ResourceDescriptorHeap[ResIdVelocityMap];

	RWTexture2D<float> OutWaterHeight = ResourceDescriptorHeap[ResIdOutWaterMap];
	RWTexture2D<float2> OutVelocity = ResourceDescriptorHeap[ResIdOutVelocityMap];

    // Current state
    float h = LastWaterHeight[id.xy];
    float terrain = TerrainHeight[id.xy];
    float depth = max(h - terrain, 0.0);   // water depth above terrain
    float2 vel = LastVelocity[id.xy];

    // Neighbor samples
    int2 left  = int2(max(id.x-1,0), id.y);
    int2 right = int2(min(id.x+1,gridSize.x-1), id.y);
    int2 down  = int2(id.x, max(id.y-1,0));
    int2 up    = int2(id.x, min(id.y+1,gridSize.y-1));

    float dL = max(LastWaterHeight[left]  - TerrainHeight[left],  0.0);
    float dR = max(LastWaterHeight[right] - TerrainHeight[right], 0.0);
    float dD = max(LastWaterHeight[down]  - TerrainHeight[down],  0.0);
    float dU = max(LastWaterHeight[up]    - TerrainHeight[up],    0.0);

    float2 velL = LastVelocity[left];
    float2 velR = LastVelocity[right];
    float2 velD = LastVelocity[down];
    float2 velU = LastVelocity[up];

    // --- 1. Update velocity from depth gradient (gravity downhill) ---
    float2 grad;
    grad.x = (dR - dL) / (2.0 * cellSize);
    grad.y = (dU - dD) / (2.0 * cellSize);

    vel += -gravity * grad * dt;

    // --- 2. Update water depth from velocity divergence ---
    float div = (velR.x - velL.x + velU.y - velD.y) / (2.0 * cellSize);
    depth += -depth * div * dt;

    // Clamp depth to non-negative
    if (depth < 0.0) depth = 0.0;

    // Reconstruct water height = terrain + depth
    h = terrain + depth;

    // Write back
    OutWaterHeight[id.xy] = h;
    OutVelocity[id.xy] = vel;
}