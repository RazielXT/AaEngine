#include "ShadowsPssm.hlsl"
#include "hlsl/utils/blueNoise.hlsl"
#include "hlsl/utils/srgb.hlsl"

float4x4 InvViewProjectionMatrix;
float3 CameraPosition;
uint TexIdNightSky;

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

struct VSOut
{
	float4 pos  : SV_POSITION;
	float2 ndc  : TEXCOORD0;   // NDC in [-1,1]
};

VSOut VSMain(uint vertexID : SV_VertexID)
{
	VSOut o;

	// Fullscreen triangle trick
	float2 verts[3] = {
		float2(-1, -1),
		float2(-1,  3),
		float2( 3, -1)
	};

	float2 ndc = verts[vertexID];
	o.pos = float4(ndc, 0, 1);
	o.ndc = ndc;

	return o;
}

float3 ComputeSkyDir(float2 ndc)
{
	// Reconstruct clip-space position
	float4 clip = float4(ndc, 1.0, 1.0);

	// Transform to world space
	float4 world = mul(clip, InvViewProjectionMatrix);
	world /= world.w;

	// Ray direction from camera
	return normalize(world.xyz - CameraPosition);
}

struct PSOutput
{
	float4 albedo : SV_Target0;
};

SamplerState LinearSampler : register(s0);

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
}

float hash(float3 p)
{
    p = frac(p * 0.3183099 + 0.1);
    p *= 17.0;
    return frac(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float3 hash3(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac(float3(
        p.x * p.y,
        p.y * p.z,
        p.z * p.x
    ));
}

float3 StarColor(float rnd)
{
    // Use rnd to pick a spectral class
    // Weighted distribution (approximate real star population)
    float t = rnd;

    float3 color;

    if (t < 0.70) {
        // White (G/F type)
        color = float3(1.0, 0.95, 0.90);
    }
    else if (t < 0.90) {
        // Yellow / Orange (K type)
        color = float3(1.0, 0.85, 0.65);
    }
    else if (t < 0.95) {
        // Blue (O/B type)
        color = float3(0.65, 0.75, 1.0);
    }
    else {
        // Red (M type)
        color = float3(1.0, 0.55, 0.55);
    }

    // Slight random brightness variation
    float brightness = lerp(0.8, 1.3, frac(rnd * 53.123));
    return color * brightness;
}

float3 StarField(float3 dir)
{
	// 1. Build a rotation matrix based on the Sun's current direction
    float3 forward = Sun.Direction;
    
    // We need a temporary 'up' to start. If the sun is at the very top, 
    // we use 'forward' as 'up', so we swap to 'z' to avoid a math error.
    float3 tempUp = abs(forward.y) > 0.99 ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 right = normalize(cross(tempUp, forward));
    float3 actualUp = cross(forward, right);

    // 2. Create the matrix and transform your view direction.
    // This "locks" the star coordinates to the Sun's orientation.
    float3x3 sunSyncMatrix = float3x3(right, actualUp, forward);
    float3 rotatedDir = mul(sunSyncMatrix, dir);

    // 1. Scale the direction vector to define star density.
    // Instead of a 2D grid, we use a 3D cube grid wrapped around the sphere.
    float density = 300.0; 
    float3 uvw = rotatedDir * density;

    // 2. Determine which 3D "cell" this direction falls into.
    float3 cell = floor(uvw);

    // 3. Use hash on the 3D cell coordinate.
    float rnd = hash(cell);
	float3 rnd3 = hash3(cell);

    // Star probability
    float star = step(0.995, rnd3.x);

    // Soft sparkle
    float sparkle = pow(frac(rnd3.y * 43758.5453), 12.0);

	float3 color = StarColor(rnd3.z);

	float3 g = frac(uvw) - 0.5;
	float dist = length(g);
	float starShape = smoothstep(0.35, 0.2, dist); // Creates a soft circle in the cell

	return color * starShape * star * sparkle;
}

float2 SphericalUV(float3 d)
{
	float PI = 3.1416;
    float u = atan2(d.z, d.x) / (2.0 * PI) + 0.5;
    float v = asin(d.y) / PI + 0.5;
    return float2(u, v);
}

float3x3 Yaw60()
{
    float a = radians(60.0);
    float c = cos(a);
    float s = sin(a);

    return float3x3(
         c, 0,  s,
         0, 1,  0,
        -s, 0,  c
    );
}

float3x3 Roll60()
{
    float a = radians(60.0);
    float c = cos(a);
    float s = sin(a);

    return float3x3(
         c,  s, 0,
        -s,  c, 0,
         0,  0, 1
    );
}

float3 StarFieldTexture(float3 dir)
{
    float3 rotated = mul(mul(Yaw60(), Roll60()), dir);
    float2 uv = SphericalUV(rotated);

    float2 dx = ddx(uv);
    float2 dy = ddy(uv);

    // Fix the horizontal wrap-around seam
    dx.x = dx.x - round(dx.x);
    dy.x = dy.x - round(dy.x);

    // Scale down derivatives near the poles
    // to prevent the sampler from picking a tiny, blurry mipmap.
    float polarWeight = saturate(1.0 - abs(rotated.y));
    dx *= polarWeight;
    dy *= polarWeight;

    return GetTexture(TexIdNightSky).SampleGrad(LinearSampler, uv, dx, dy).rgb * 0.2;
}

PSOutput PSMain(VSOut input)
{
	float3 skyDir = ComputeSkyDir(input.ndc);
	float sunDot = dot(-skyDir, Sun.Direction);

	float sunZenithDot = -Sun.Direction.y;
	float viewZenithDot = skyDir.y;
	float sunViewDot01 = (sunDot + 1.0) * 0.5;
	float sunZenithDot01 = (sunZenithDot + 1.0) * 0.5;

	float3 sunZenithColor = GetTexture(Sun.TexIdSunZenith).Sample(LinearSampler, float2(sunZenithDot01, 0.5)).rgb * 0.5;

	float3 viewZenithColor = GetTexture(Sun.TexIdViewZenith).Sample(LinearSampler, float2(sunZenithDot01, 0.5)).rgb * 0.5;
	float vzMask = pow(saturate(1.0 - skyDir.y), 4);

	float3 sunViewColor = GetTexture(Sun.TexIdSunView).Sample(LinearSampler, float2(sunZenithDot01, 0.5)).rgb;
	float svMask = pow(saturate(sunDot), 4);

	// Night factor: fade stars in when sun is below horizon
	float night = saturate(Sun.Direction.y * 2.0);
	float3 stars = StarFieldTexture(skyDir) * night;

	float3 skyColor = stars + SrgbToLinear(sunZenithColor + vzMask * viewZenithColor + svMask * sunViewColor);

	float noise = BlueNoise(input.pos.xy);
	float ditherStrength = (2 - night) / 255.0;
	// Add noise before tone mapping
	skyColor += (noise - 0.5) * ditherStrength;

	// Sun core
	float coreSize = 0.999;
	float core = smoothstep(coreSize, 1.0, sunDot);
	float3 sunColor = core * 50 * Sun.Color;

	PSOutput output;
	output.albedo = float4(saturate(skyColor) + sunColor, 0);

	return output;
}
