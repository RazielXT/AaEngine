cbuffer SimParams : register(b0)
{
	uint2 gridSize; // simulation dimensions
	float dt; // timestep (seconds)
	float gravity; // gravity (not used here)
	float cellSize; // grid spacing (meters), e.g., 52.1 / 512
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
	int2 pL = int2(max(int(id.x) - 1, 0), int(id.y));
	int2 pR = int2(min(int(id.x) + 1, int(gridSize.x) - 1), int(id.y));
	int2 pD = int2(int(id.x), max(int(id.y) - 1, 0));
	int2 pU = int2(int(id.x), min(int(id.y) + 1, int(gridSize.y) - 1));

	Texture2D<float> Height = ResourceDescriptorHeap[ResIdWaterMap];
	Texture2D<float> TerrainUN = ResourceDescriptorHeap[ResIdHeightMap];
	Texture2D<float2> Velocity = ResourceDescriptorHeap[ResIdOutVelocityMap];
	RWTexture2D<float> HeightOut = ResourceDescriptorHeap[ResIdOutWaterMap];

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

	// Calculate Average Velocity at the face midpoint (more stable than neighbor velocity)
	float uAvgX_L = (uL.x + uC.x) * 0.5; // Avg X-velocity across the Left face
	float uAvgX_R = (uR.x + uC.x) * 0.5; // Avg X-velocity across the Right face
	float uAvgY_D = (uD.y + uC.y) * 0.5; // Avg Y-velocity across the Down face
	float uAvgY_U = (uU.y + uC.y) * 0.5; // Avg Y-velocity across the Up face

	// Recalculate Flux using the Averaged Velocity and Donor Cell depth logic:
	// Note: We use the SIGN of the *average* velocity for donor selection.

	// Left Flux (Flowing IN is positive, Flowing OUT is negative)
	float qxL = (uAvgX_L > 0.0) ? (uAvgX_L * dL) : (uAvgX_L * dC);

	// Right Flux (Flowing OUT is positive, Flowing IN is negative)
	float qxR = (uAvgX_R > 0.0) ? (uAvgX_R * dC) : (uAvgX_R * dR);

	// Down Flux (Flowing IN is positive, Flowing OUT is negative)
	float qyD = (uAvgY_D > 0.0) ? (uAvgY_D * dD) : (uAvgY_D * dC);

	// Up Flux (Flowing OUT is positive, Flowing IN is negative)
	float qyU = (uAvgY_U > 0.0) ? (uAvgY_U * dC) : (uAvgY_U * dU);

	// === Apply Wall Boundary Conditions (Must be kept!) ===
	if (id.x == 0) qxL = 0.0;
	if (id.x == gridSize.x - 1) qxR = 0.0;
	if (id.y == 0) qyD = 0.0;
	if (id.y == gridSize.y - 1) qyU = 0.0;

	// Divergence
	float divQ = (qxR - qxL + qyU - qyD) / cellSize;

	// Height update (mass conservation)
	float hNew = hC - dt * divQ;

	// Non-negative clamp (optional; remove to test conservation)
	hNew = max(hNew, tC * 5 - 0.1f);

	HeightOut[p] = hNew;
}
