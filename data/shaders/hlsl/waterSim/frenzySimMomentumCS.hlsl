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
    int2 pL = int2(max(int(id.x) - 1, 0),                int(id.y));
    int2 pR = int2(min(int(id.x) + 1, int(gridSize.x)-1),int(id.y));
    int2 pD = int2(int(id.x),                            max(int(id.y) - 1, 0));
    int2 pU = int2(int(id.x),                            min(int(id.y) + 1, int(gridSize.y)-1));

    Texture2D<float>   Height    = ResourceDescriptorHeap[ResIdWaterMap];
    Texture2D<float>   TerrainUN = ResourceDescriptorHeap[ResIdHeightMap];
    Texture2D<float2>  Velocity  = ResourceDescriptorHeap[ResIdVelocityMap];
    RWTexture2D<float2> VelocityOut = ResourceDescriptorHeap[ResIdOutVelocityMap];

    // scaling for terrain
    float terrainScale = 30.0;

    // ---- Sample height ----
    float hL = Height.Load(int3(pL, 0));
    float hR = Height.Load(int3(pR, 0));
    float hD = Height.Load(int3(pD, 0));
    float hU = Height.Load(int3(pU, 0));
    float hC = Height.Load(int3(p, 0));

    // ---- Sample terrain ----
    float tL = TerrainUN.Load(int3(pL, 0)) * terrainScale;
    float tR = TerrainUN.Load(int3(pR, 0)) * terrainScale;
    float tD = TerrainUN.Load(int3(pD, 0)) * terrainScale;
    float tU = TerrainUN.Load(int3(pU, 0)) * terrainScale;
    float tC = TerrainUN.Load(int3(p, 0)) * terrainScale;

    // effective depth (positive only)
    float dL = max(hL - tL, 0.0);
    float dR = max(hR - tR, 0.0);
    float dD = max(hD - tD, 0.0);
    float dU = max(hU - tU, 0.0);
    float dC = max(hC - tC, 0.0);

    // ---- Sample velocity ----
    float2 uC = Velocity.Load(int3(p, 0));
    float2 uL = Velocity.Load(int3(pL, 0));
    float2 uR = Velocity.Load(int3(pR, 0));
    float2 uD = Velocity.Load(int3(pD, 0));
    float2 uU = Velocity.Load(int3(pU, 0));

    // ========================================================================
    // 1) GRAVITY FORCE from height slope
    // ========================================================================
    float dhdx = (hR - hL) / (2.0 * cellSize);
    float dhdy = (hU - hD) / (2.0 * cellSize);

    float2 u = uC;

    u.x += -gravity * dhdx * dt;
    u.y += -gravity * dhdy * dt;

    // ========================================================================
    // 2) NONLINEAR ADVECTION:  (u · ∇) u
    // ========================================================================
    float2 du_dx = (uR - uL) / (2.0 * cellSize);
    float2 du_dy = (uU - uD) / (2.0 * cellSize);

    float2 adv = float2(
        uC.x * du_dx.x + uC.y * du_dy.x,
        uC.x * du_dx.y + uC.y * du_dy.y
    );

    u -= adv * dt;

    // ========================================================================
    // 3) VISCOSITY (shock capturing)
    // ========================================================================
    // tweak this value for look/performance
    float nu = 0.05;

    // laplacian
    float2 lap = (uL + uR + uD + uU - 4.0 * uC) / (cellSize * cellSize);

    u += nu * lap * dt;

    // ========================================================================
    // 4) Clamp (optional)
    // ========================================================================
    float mag = length(u);
    float maxVel = 1.0; // raise if needed for big waves

    if (mag > maxVel) {
        u *= maxVel / mag;
    }

    // ========================================================================
    // Write back
    // ========================================================================
    VelocityOut[p] = u;
}
