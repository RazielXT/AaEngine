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

SamplerState LinearSampler : register(s0);

[numthreads(8,8,1)]
void CS_Momentum(uint3 id : SV_DispatchThreadID)
{
if (id.x >= gridSize.x || id.y >= gridSize.y) return;

    int2 p  = int2(id.xy);
    int2 pL = int2(max(int(id.x) - 1, 0),              int(id.y));
    int2 pR = int2(min(int(id.x) + 1, int(gridSize.x) - 1), int(id.y));
    int2 pD = int2(int(id.x),                          max(int(id.y) - 1, 0));
    int2 pU = int2(int(id.x),                          min(int(id.y) + 1, int(gridSize.y) - 1));

    Texture2D<float>   Height    = ResourceDescriptorHeap[ResIdWaterMap];
    Texture2D<float>   TerrainUN = ResourceDescriptorHeap[ResIdHeightMap];
    Texture2D<float2>  Velocity  = ResourceDescriptorHeap[ResIdVelocityMap];
    RWTexture2D<float2> VelocityOut = ResourceDescriptorHeap[ResIdOutVelocityMap];

	float terrainScale = 10;
    // Center values
    float hC = Height.Load(int3(p, 0));
    float tC = TerrainUN.Load(int3(p, 0)) * terrainScale;

    // Neighbors
    float hL = Height.Load(int3(pL, 0));
    float hR = Height.Load(int3(pR, 0));
    float hD = Height.Load(int3(pD, 0));
    float hU = Height.Load(int3(pU, 0));

    float tL = TerrainUN.Load(int3(pL, 0)) * terrainScale;
    float tR = TerrainUN.Load(int3(pR, 0)) * terrainScale;
    float tD = TerrainUN.Load(int3(pD, 0)) * terrainScale;
    float tU = TerrainUN.Load(int3(pU, 0)) * terrainScale;

    // Previous velocity
    float2 uC = Velocity.Load(int3(p, 0));

    // --- Compute slopes ---
    float dhdx = (hR - hL) / (2.0 * cellSize);
    float dhdy = (hU - hD) / (2.0 * cellSize);

    // --- Apply gravity force ---
    uC.x += -gravity * dhdx * dt;
    uC.y += -gravity * dhdy * dt;

    // --- Clamp velocity magnitude (like Frenzy) ---
    float mag = length(uC);
    if (mag > 1.0) {
        uC *= (1.0 / mag);
    }

    VelocityOut[p] = uC;
}
