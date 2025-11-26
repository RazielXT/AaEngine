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

int2 wrap(int2 p) {
    return int2( ( (p.x % int(gridSize.x)) + int(gridSize.x) ) % int(gridSize.x),
                 ( (p.y % int(gridSize.y)) + int(gridSize.y) ) % int(gridSize.y) );
}

[numthreads(8,8,1)]
void CS_Momentum(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gridSize.x || id.y >= gridSize.y) return;

    int2 p  = int2(id.xy);
    int2 pL = int2(max(int(id.x) - 1, 0), int(id.y));
    int2 pR = int2(min(int(id.x) + 1, int(gridSize.x) - 1), int(id.y));
    int2 pD = int2(int(id.x), max(int(id.y) - 1, 0));
    int2 pU = int2(int(id.x), min(int(id.y) + 1, int(gridSize.y) - 1));

    Texture2D<float> Height = ResourceDescriptorHeap[ResIdWaterMap];
    RWTexture2D<float2> VelocityOut = ResourceDescriptorHeap[ResIdOutVelocityMap];

    float hC = Height.Load(int3(p,0));
    float hL = Height.Load(int3(wrap(pL),0));
    float hR = Height.Load(int3(wrap(pR),0));
    float hD = Height.Load(int3(wrap(pD),0));
    float hU = Height.Load(int3(wrap(pU),0));

    // Simple slope gradients
    float dhdx = (hR - hL) / (2.0 * cellSize);
    float dhdy = (hU - hD) / (2.0 * cellSize);

    // Velocity = -g * slope * dt
    float2 u = float2(-gravity * dhdx, -gravity * dhdy);

    float mag = length(u);
    if (mag > 1.0) {
        u *= (1.0 / mag);
    }

    VelocityOut[p] = u;
}

[numthreads(8,8,1)]
void CS_Continuity(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gridSize.x || id.y >= gridSize.y) return;

    int2 p  = int2(id.xy);
    int2 pL = int2(max(int(id.x) - 1, 0), int(id.y));
    int2 pR = int2(min(int(id.x) + 1, int(gridSize.x) - 1), int(id.y));
    int2 pD = int2(int(id.x), max(int(id.y) - 1, 0));
    int2 pU = int2(int(id.x), min(int(id.y) + 1, int(gridSize.y) - 1));

    Texture2D<float> Height = ResourceDescriptorHeap[ResIdWaterMap];
    Texture2D<float2> Velocity = ResourceDescriptorHeap[ResIdOutVelocityMap];
    RWTexture2D<float> HeightOut = ResourceDescriptorHeap[ResIdOutWaterMap];

    float hC = Height.Load(int3(p,0));
    float hL = Height.Load(int3(pL,0));
    float hR = Height.Load(int3(pR,0));
    float hD = Height.Load(int3(pD,0));
    float hU = Height.Load(int3(pU,0));

    float2 uC = Velocity.Load(int3(p,0));
    float2 uL = Velocity.Load(int3(pL,0));
    float2 uR = Velocity.Load(int3(pR,0));
    float2 uD = Velocity.Load(int3(pD,0));
    float2 uU = Velocity.Load(int3(pU,0));

    // Donor-cell fluxes (depth = height since terrain=0)
    float qxL = (uL.x > 0.0) ? uL.x * hL : uL.x * hC;
    float qxR = (uR.x > 0.0) ? uR.x * hC : uR.x * hR;
    float qyD = (uD.y > 0.0) ? uD.y * hD : uD.y * hC;
    float qyU = (uU.y > 0.0) ? uU.y * hC : uU.y * hU;

    // Divergence
    float divQ = (qxR - qxL + qyU - qyD) / cellSize;

    // Height update
    float hNew = hC - dt * divQ;
    hNew = max(hNew, 0.0);

    HeightOut[p] = hNew;
}