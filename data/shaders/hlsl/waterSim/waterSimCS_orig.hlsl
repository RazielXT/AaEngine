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

int2 wrap(int2 p, uint2 size) {
    return int2( ( (p.x % int(size.x)) + int(size.x) ) % int(size.x),
                 ( (p.y % int(size.y)) + int(size.y) ) % int(size.y) );
}

#define BOUNDARY_OUTFLOW  0
#define BOUNDARY_PERIODIC 1
#define BOUNDARY_NEUMANN  2

static const uint boundaryMode = BOUNDARY_PERIODIC;

[numthreads(8,8,1)]
void CS_Momentum(uint3 id : SV_DispatchThreadID)
{
	Texture2D<float> TerrainHeight = ResourceDescriptorHeap[ResIdHeightMap];
	Texture2D<float> WaterHeightRead = ResourceDescriptorHeap[ResIdWaterMap];
	Texture2D<float2> VelocityRead = ResourceDescriptorHeap[ResIdVelocityMap];

	//RWTexture2D<float> WaterHeightWrite = ResourceDescriptorHeap[ResIdOutWaterMap];
	RWTexture2D<float2> VelocityWrite = ResourceDescriptorHeap[ResIdOutVelocityMap];

    if (id.x >= gridSize.x || id.y >= gridSize.y) return;

    int2 P = int2(id.xy);
    int2 L = int2(P.x-1, P.y);
    int2 R = int2(P.x+1, P.y);
    int2 D = int2(P.x, P.y-1);
    int2 U = int2(P.x, P.y+1);

    bool atL = (P.x == 0);
    bool atR = (P.x == int(gridSize.x)-1);
    bool atB = (P.y == 0);
    bool atT = (P.y == int(gridSize.y)-1);

    // Apply boundary policy
    if (boundaryMode == BOUNDARY_PERIODIC) {
        L = wrap(L, gridSize);
        R = wrap(R, gridSize);
        D = wrap(D, gridSize);
        U = wrap(U, gridSize);
    } else {
        L.x = max(L.x, 0);       R.x = min(R.x, int(gridSize.x)-1);
        D.y = max(D.y, 0);       U.y = min(U.y, int(gridSize.y)-1);
    }

    float hC = WaterHeightRead[P];
    float bC = TerrainHeight[P];
    float dC = max(hC - bC, 0.0);

    float dL = max(WaterHeightRead[L] - TerrainHeight[L], 0.0);
    float dR = max(WaterHeightRead[R] - TerrainHeight[R], 0.0);
    float dD = max(WaterHeightRead[D] - TerrainHeight[D], 0.0);
    float dU = max(WaterHeightRead[U] - TerrainHeight[U], 0.0);

    // Neumann: mirror depth at edges
    if (boundaryMode == BOUNDARY_NEUMANN) {
        if (atL) dL = dC;
        if (atR) dR = dC;
        if (atB) dD = dC;
        if (atT) dU = dC;
    }

    // Gradient of depth
    float2 grad = float2((dR - dL) / (2.0 * cellSize),
                         (dU - dD) / (2.0 * cellSize));

    // Update velocity
    float2 vel = VelocityRead[P];
    vel += -gravity * 4 * grad * dt;
	const float damping = 0.01f;
    vel *= (1.0 - damping); // optional damping

    if (dC <= 0.0)
        vel = float2(0,0);

    VelocityWrite[P] = vel;
}

[numthreads(8,8,1)]
void CS_Continuity(uint3 id : SV_DispatchThreadID)
{
	Texture2D<float> TerrainHeight = ResourceDescriptorHeap[ResIdHeightMap];
	Texture2D<float> WaterHeightRead = ResourceDescriptorHeap[ResIdWaterMap];
	//Texture2D<float2> VelocityRead = ResourceDescriptorHeap[ResIdVelocityMap];

	RWTexture2D<float> WaterHeightWrite = ResourceDescriptorHeap[ResIdOutWaterMap];
	Texture2D<float2> VelocityWrite = ResourceDescriptorHeap[ResIdOutVelocityMap];

    if (id.x >= gridSize.x || id.y >= gridSize.y) return;

	const uint boundaryMode = BOUNDARY_PERIODIC;

    int2 P = int2(id.xy);
    int2 L = int2(P.x-1, P.y);
    int2 R = int2(P.x+1, P.y);
    int2 D = int2(P.x, P.y-1);
    int2 U = int2(P.x, P.y+1);

    bool atL = (P.x == 0);
    bool atR = (P.x == int(gridSize.x)-1);
    bool atB = (P.y == 0);
    bool atT = (P.y == int(gridSize.y)-1);

    // Apply boundary policy
    if (boundaryMode == BOUNDARY_PERIODIC) {
        L = wrap(L, gridSize);
        R = wrap(R, gridSize);
        D = wrap(D, gridSize);
        U = wrap(U, gridSize);
    } else {
        L.x = max(L.x, 0);       R.x = min(R.x, int(gridSize.x)-1);
        D.y = max(D.y, 0);       U.y = min(U.y, int(gridSize.y)-1);
    }

    float bC = TerrainHeight[P];
    float dC = max(WaterHeightRead[P] - bC, 0.0);

    float dL = max(WaterHeightRead[L] - TerrainHeight[L], 0.0);
    float dR = max(WaterHeightRead[R] - TerrainHeight[R], 0.0);
    float dD = max(WaterHeightRead[D] - TerrainHeight[D], 0.0);
    float dU = max(WaterHeightRead[U] - TerrainHeight[U], 0.0);

    float2 uC = VelocityWrite[P];
    float2 uL = VelocityWrite[L];
    float2 uR = VelocityWrite[R];
    float2 uD = VelocityWrite[D];
    float2 uU = VelocityWrite[U];

    // Face velocities
    float ux_L = 0.5 * (uL.x + uC.x);
    float ux_R = 0.5 * (uC.x + uR.x);
    float uy_B = 0.5 * (uD.y + uC.y);
    float uy_T = 0.5 * (uC.y + uU.y);

    // Outflow: zero inflow at all edges
    if (boundaryMode == BOUNDARY_OUTFLOW) {
        if (atL && ux_L > 0.0) ux_L = 0.0;
        if (atR && ux_R < 0.0) ux_R = 0.0;
        if (atB && uy_B > 0.0) uy_B = 0.0;
        if (atT && uy_T < 0.0) uy_T = 0.0;
    }

    // Neumann: mirror depth at edges
    if (boundaryMode == BOUNDARY_NEUMANN) {
        if (atL) dL = dC;
        if (atR) dR = dC;
        if (atB) dD = dC;
        if (atT) dU = dC;
    }

    // Upwind fluxes
    float qx_L = (ux_L > 0.0) ? (ux_L * dL) : (ux_L * dC);
    float qx_R = (ux_R > 0.0) ? (ux_R * dC) : (ux_R * dR);
    float qy_B = (uy_B > 0.0) ? (uy_B * dD) : (uy_B * dC);
    float qy_T = (uy_T > 0.0) ? (uy_T * dC) : (uy_T * dU);

    float invDx = 1.0 / cellSize;
    float divQ = (qx_R - qx_L + qy_T - qy_B) * invDx;

    float d_new = dC - dt * divQ;
    d_new = max(d_new, 0.0);



    WaterHeightWrite[P] = bC + d_new;
}