cbuffer SimParams : register(b0)
{
    uint2 gridSize;            // simulation dimensions
    float dt;                  // timestep (seconds)
    float gravity;             // gravity (not used here)
    float cellSize;            // grid spacing (meters), e.g., 52.1 / 512
	uint ResIdHeightMap;
	uint ResIdWaterMap;
	uint ResIdVelocityMap;
	uint ResIdOutWaterMap;
	uint ResIdOutVelocityMap;
}

SamplerState PointSampler : register(s0);

[numthreads(8,8,1)]
void CS_Continuity(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gridSize.x || id.y >= gridSize.y) return;

    int2 p  = int2(id.xy);
    int2 pL = int2(max(int(id.x) - 1, 0),              int(id.y));
    int2 pR = int2(min(int(id.x) + 1, int(gridSize.x) - 1), int(id.y));
    int2 pD = int2(int(id.x),                          max(int(id.y) - 1, 0));
    int2 pU = int2(int(id.x),                          min(int(id.y) + 1, int(gridSize.y) - 1));

    Texture2D<float>    Height    = ResourceDescriptorHeap[ResIdWaterMap];
    Texture2D<float>    TerrainUN = ResourceDescriptorHeap[ResIdHeightMap];
    Texture2D<float2>   Velocity  = ResourceDescriptorHeap[ResIdOutVelocityMap];
    RWTexture2D<float>  HeightOut = ResourceDescriptorHeap[ResIdOutWaterMap];

	float terrainScale = 10;

    // Center values
    float hC = Height.Load(int3(p, 0));
    float tC = TerrainUN.Load(int3(p, 0)) * terrainScale;
    float dC = max(hC - tC, 0.0);

    // Neighbors
    float hL = Height.Load(int3(pL, 0));
    float hR = Height.Load(int3(pR, 0));
    float hD = Height.Load(int3(pD, 0));
    float hU = Height.Load(int3(pU, 0));

    float tL = TerrainUN.Load(int3(pL, 0)) * terrainScale;
    float tR = TerrainUN.Load(int3(pR, 0)) * terrainScale;
    float tD = TerrainUN.Load(int3(pD, 0)) * terrainScale;
    float tU = TerrainUN.Load(int3(pU, 0)) * terrainScale;

    float dL = max(hL - tL, 0.0);
    float dR = max(hR - tR, 0.0);
    float dD = max(hD - tD, 0.0);
    float dU = max(hU - tU, 0.0);

    // Velocities (assumed in meters/sec)
    float2 uC = Velocity.Load(int3(p, 0));
    float2 uL = Velocity.Load(int3(pL, 0));
    float2 uR = Velocity.Load(int3(pR, 0));
    float2 uD = Velocity.Load(int3(pD, 0));
    float2 uU = Velocity.Load(int3(pU, 0));

    // Donor-cell fluxes across faces
    float qxL = (uL.x > 0.0) ? (uL.x * dL) : (uL.x * dC);
    float qxR = (uR.x > 0.0) ? (uR.x * dC) : (uR.x * dR);
    float qyD = (uD.y > 0.0) ? (uD.y * dD) : (uD.y * dC);
    float qyU = (uU.y > 0.0) ? (uU.y * dC) : (uU.y * dU);

    // Divergence (world units): flux difference over cell size
    float divQ = (qxR - qxL + qyU - qyD) / cellSize;

    // Height update (mass conservation)
    float hNew = hC - dt * divQ;

    // Non-negative clamp (optional; remove to test conservation)
    hNew = max(hNew, 0.0);

    HeightOut[p] = hNew;
}
