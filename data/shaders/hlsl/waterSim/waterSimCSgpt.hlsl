cbuffer SimParams : register(b0)
{
	float2 uvScale; // size of grid in world
	uint2 gridSize;	// simulation dimensions
	float4 misc12;
	float worldScale; // 1/uvScale
	float dt; // timestep
	float gravity; // gravity constant
	float terrainScale;
    float4 terrainSampleOffset; // cb2[1]
    float4 heightSampleOffset;  // cb2[2]
	float2 gridScale;      // cb2[0]
	uint ResIdHeightMap;
	uint ResIdWaterMap;
	uint ResIdVelocityMap;
	uint ResIdOutWaterMap;
	uint ResIdOutVelocityMap;
};

SamplerState LinearSampler    : register(s0);

[numthreads(8,8,1)]
void CS_Momentum(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gridSize.x || id.y >= gridSize.y) return;

	Texture2D<float> TerrainHeight = ResourceDescriptorHeap[ResIdHeightMap];
	Texture2D<float> PrevHeight = ResourceDescriptorHeap[ResIdWaterMap];
	Texture2D<float2> PrevVelocity = ResourceDescriptorHeap[ResIdVelocityMap];
	RWTexture2D<float2> VelocityWrite = ResourceDescriptorHeap[ResIdOutVelocityMap];

    float2 uv = id.xy / (float2)gridSize.xy;

    // ---------------------------------------------------------------------
    // 1. Sample previous velocity at UV
    // ---------------------------------------------------------------------
    float2 v0 = PrevVelocity.SampleLevel(LinearSampler, uv, 0).xy;

    // scaled velocity
    float2 scaledVel = v0 * dt * gridScale;

    // ---------------------------------------------------------------------
    // 2. Trace-back sample position (backward advection step)
    // position = uv - scaledVel * uvScale
    // ---------------------------------------------------------------------
    float2 uvBack = uv - scaledVel * uvScale.xy;

    // sample velocity again at traced position
    float2 v1 = PrevVelocity.SampleLevel(LinearSampler, uvBack, 0).xy;

    float2 velSum = v0 + v1;

    // gravity factor for later
    float grav = dt * gravity.x;

    // ============= SAMPLE HEIGHT FROM NEIGHBORS ========================

    // cb2[2] - PrevHeight offsets:
    float2 hOffL = uv + heightSampleOffset.xy;
    float2 hOffR = uv + heightSampleOffset.zw;

    float hL = PrevHeight.SampleLevel(LinearSampler, hOffL, 0).x;
    float hR = PrevHeight.SampleLevel(LinearSampler, hOffR, 0).x;

    // cb2[1] - Terrain offsets:
    float2 tOffL = uv + terrainSampleOffset.xy;
    float2 tOffR = uv + terrainSampleOffset.zw;

    float tL = TerrainHeight.SampleLevel(LinearSampler, tOffL, 0).x;
    float tR = TerrainHeight.SampleLevel(LinearSampler, tOffR, 0).x;

    // combine terrain & height
    float HL = hL + tL * terrainScale.x;
    float HR = hR + tR * terrainScale.x;

    // local height/terrain at UV
    float H0 = PrevHeight.SampleLevel(LinearSampler, uv, 0).x;
    float T0 = TerrainHeight.SampleLevel(LinearSampler, uv, 0).x;
    float H0T = H0 + T0 * terrainScale.x;

    // ---------------------------------------------------------------------
    // Compute slopes / height differences
    // ---------------------------------------------------------------------
    bool slopeL_neg = (HL < H0T);
    bool slopeR_neg = (HR < H0T);

    float2 slopeDiff = float2(HL - H0T, HR - H0T);

    // scaled by uvScale
    slopeDiff *= uvScale.xy;

    // multiply by gravity*dt
    slopeDiff *= grav;

    // length, normalization clamp
	float accelClamp = 10;
    float slen = length(slopeDiff);
    float invS = min(accelClamp / max(slen, 1e-6), 1.0);
    slopeDiff *= invS;

    // blend into velocity
    float2 newVel = velSum * 0.5 + slopeDiff;

    // speed clamp by dampingParams.z (7.0)
	float dampingParams = 7;
    float speed = length(newVel);
    float invD = min(dampingParams / max(speed, 1e-6), 1.0);
    newVel *= invD;

    // terrain height test for sign adjustments
    bool t0BelowThresh = (misc12.z >= H0T);

    // ------------------------------------------------------------------
    // Water flow directional constraints
    // (these match the movc/or/and/lt logic in bytecode)
    // ------------------------------------------------------------------
    bool chooseZeroX = ((HR < HL) && slopeR_neg) || slopeL_neg;
    bool chooseZeroY = ((HL < HR) && slopeL_neg) || slopeR_neg;

    if (chooseZeroX) newVel.x = 0;
    if (chooseZeroY) newVel.y = 0;

    // ------------------------------------------------------------------
    // Normalize velocity and clamp
    // ------------------------------------------------------------------
    // scale by misc12.x (0.00042) & misc12.y (0.00104)
    float2 vtemp = newVel * misc12.xy;

    // shift
    vtemp = vtemp * -0.5 + newVel;

    // nonlinear terrain factor:
    float t0scaled = saturate(T0 * 10000.0);
    float k = (3 - 2 * t0scaled) * t0scaled; // cubic smoothstep
    k = saturate(k * dt * 10);

    newVel = lerp(newVel, -newVel, k);

    // final clamp
    float4 vmax = worldScale * 0.5 / dt;
    float4 vmin = -vmax;

    float4 outVel = float4(newVel, 0, 0);
    outVel = clamp(outVel, vmin, vmax);

    // border clamp using gridScale:
    bool outB =
        (uv.x < gridScale.x) ||
        (uv.y < gridScale.y) ||
        (uv.x > 1 - gridScale.x * 2) ||
        (uv.y > 1 - gridScale.y * 2);

    if (outB) outVel = float4(0,0,0,0);

    VelocityWrite[uv] = outVel.xy;
}

[numthreads(8,8,1)]
void CS_Continuity(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gridSize.x || id.y >= gridSize.y) return;

	Texture2D<float> PrevHeight = ResourceDescriptorHeap[ResIdWaterMap];
	RWTexture2D<float> WaterHeightWrite = ResourceDescriptorHeap[ResIdOutWaterMap];
	Texture2D<float2> NewVelocity = ResourceDescriptorHeap[ResIdOutVelocityMap];

    float2 uv = id.xy / (float2)gridSize.xy;

    // ======================================================
    // 1. Compute neighbor sum of height   (same as laplacian)
    // ======================================================

    // dt * gravity
    float gdt = dt * gravity.x;

    // worldScale.x / gdt
    float invCoeff = worldScale.x / gdt;
    float baseTerm = invCoeff * 2.0;

    // sample prevHeight in ±x direction
    float2 uvL = uv + terrainSampleOffset.xy * float2(-1, 0);
    float2 uvR = uv + terrainSampleOffset.xy * float2(+1, 0);

    float HL = PrevHeight.SampleLevel(LinearSampler, uvL, 0).x;
    float HR = PrevHeight.SampleLevel(LinearSampler, uvR, 0).x;

    float lapX = HL * 2.0 + HR;

    // sample prevHeight in ±y direction
    float2 uvD = uv + terrainSampleOffset.xy * float2(0, -1);
    float2 uvU = uv + terrainSampleOffset.xy * float2(0, +1);

    float HU = PrevHeight.SampleLevel(LinearSampler, uvU, 0).x;
    float HD = PrevHeight.SampleLevel(LinearSampler, uvD, 0).x;

    float lap = lapX + HU + HD;

    // newHeightBase = 0.25*lap - baseTerm
    float newH = lap * 0.25f - baseTerm;

    newH = max(newH, 0.0f);

    // ======================================================
    // 2. Velocity-based adjustments
    // ======================================================

    // sample velocity in ±x direction
    float2 uvVelL = uv + terrainSampleOffset.xy * float2(-1, 0);
    float2 uvVelD = uv + terrainSampleOffset.xy * float2(0, -1);

    float2 vL = NewVelocity.SampleLevel(LinearSampler, uvVelL, 0).xy;
    float2 vD = NewVelocity.SampleLevel(LinearSampler, uvVelD, 0).xy;

    float2 vC = NewVelocity.SampleLevel(LinearSampler, uv, 0).xy;

    //float2 hC2      = PrevHeight.SampleLevel(LinearSampler, uv, 0).xy; 

    // r0.y : signed vertical influence
    float hUsel = (vD.y >= 0) ? HD : PrevHeight.SampleLevel(LinearSampler, uv, 0).x;
    float dY = hUsel - newH;
    float termY = vD.y * dY;     // multiply by flow

    // ====================
    // right-side influence
    // ====================
    bool posVX = (vL.x >= 0);
    float hxSel = posVX ? HR : PrevHeight.SampleLevel(LinearSampler, uv, 0).x;

    float dX = hxSel - newH;

    // combine X & Y
    float Xterm = dX * vC.x;
    float flowTerm = (Xterm - (vL.x * dX)); // matches bytecode
    float fullTerm = flowTerm * uvScale.x + termY * uvScale.y;

    // apply dt
    float dh = fullTerm * dt;

    // ======================================================
    // 3. Final height update with border clipping
    // ======================================================
    float H0 = PrevHeight.SampleLevel(LinearSampler, uv, 0).x;

    float hNew = H0 - dh;

    // border removal – prevent updating height outside region
    float2 h2 = float2(hNew, 0);
    //h2 = h2 - borderParams.xz;
    //h2 = h2 * borderParams.yw + h2;
    float2 clamp2 = max(h2, float2(0,0));

    bool smallEnough = (clamp2.x < misc12.z);

    float finalH = smallEnough ? 0 : clamp2.x;

	WaterHeightWrite[uv] = finalH;
}
