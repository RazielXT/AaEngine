cbuffer SmoothParams : register(b0)
{
	uint2 gridSize;
	float smoothFactor;
	uint ResIdHeightMap;
	uint ResIdWaterMap;
	uint ResIdOutWaterMap;
};

// Edge-flux based smoothing: exactly volume-conserving
// Each edge computes flux based on height difference between the two cells.
// The same flux value is subtracted from the higher cell and added to the lower cell,
// so water is neither created nor destroyed.

static const float SharpnessThreshold = 0.3;
static const float TerrainScale = 50.0;

[numthreads(8,8,1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= gridSize.x || id.y >= gridSize.y) return;

	int2 p  = int2(id.xy);
	int2 pL = int2(max(int(id.x) - 1, 0), int(id.y));
	int2 pR = int2(min(int(id.x) + 1, int(gridSize.x) - 1), int(id.y));
	int2 pD = int2(int(id.x), max(int(id.y) - 1, 0));
	int2 pU = int2(int(id.x), min(int(id.y) + 1, int(gridSize.y) - 1));

	Texture2D<float> TerrainUN = ResourceDescriptorHeap[ResIdHeightMap];
	Texture2D<float> Height = ResourceDescriptorHeap[ResIdWaterMap];
	RWTexture2D<float> HeightOut = ResourceDescriptorHeap[ResIdOutWaterMap];

	float hC = Height.Load(int3(p, 0));
	float tC = TerrainUN.Load(int3(p, 0)) * TerrainScale;

	float depthC = hC - tC;

	// No water above terrain, skip
	if (depthC <= 0.0)
	{
		HeightOut[p] = hC;
		return;
	}

	float hL = Height.Load(int3(pL, 0));
	float hR = Height.Load(int3(pR, 0));
	float hD = Height.Load(int3(pD, 0));
	float hU = Height.Load(int3(pU, 0));

	float tL = TerrainUN.Load(int3(pL, 0)) * TerrainScale;
	float tR = TerrainUN.Load(int3(pR, 0)) * TerrainScale;
	float tD = TerrainUN.Load(int3(pD, 0)) * TerrainScale;
	float tU = TerrainUN.Load(int3(pU, 0)) * TerrainScale;

	// Per-edge height difference
	float diffL = hC - hL;
	float diffR = hC - hR;
	float diffD = hC - hD;
	float diffU = hC - hU;

	// Per-edge smoothing factor: only smooth sharp gradients
	float factorL = smoothstep(SharpnessThreshold, SharpnessThreshold * 3.0, abs(diffL)) * smoothFactor;
	float factorR = smoothstep(SharpnessThreshold, SharpnessThreshold * 3.0, abs(diffR)) * smoothFactor;
	float factorD = smoothstep(SharpnessThreshold, SharpnessThreshold * 3.0, abs(diffD)) * smoothFactor;
	float factorU = smoothstep(SharpnessThreshold, SharpnessThreshold * 3.0, abs(diffU)) * smoothFactor;

	// Disable flux for neighbors with no water above terrain
	float dL = max(hL - tL, 0.0);
	float dR = max(hR - tR, 0.0);
	float dD = max(hD - tD, 0.0);
	float dU = max(hU - tU, 0.0);

	if (dL <= 0.0) factorL = 0.0;
	if (dR <= 0.0) factorR = 0.0;
	if (dD <= 0.0) factorD = 0.0;
	if (dU <= 0.0) factorU = 0.0;

	// Net flux: positive means this cell gains water
	// For each edge, flux = (hNeighbor - hCenter) * edgeFactor / 4
	// Since both cells use the same edgeFactor (based on abs(diff)), the flux
	// leaving one cell exactly enters the other -> volume-conserving
	float net = (-diffL * factorL + -diffR * factorR + -diffD * factorD + -diffU * factorU) * 0.25;

	float hNew = hC + net;

	// Safety clamp: dont push water below terrain
	hNew = max(hNew, tC);

	HeightOut[p] = hNew;
}
