#include "hlsl/sky/SkyParams.hlsl"
#include "hlsl/sky/SkyColor.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 InvViewMatrix;
uint TexIdDiffuse;
uint TexIdNormal;

cbuffer PSSMShadows : register(b1)
{
	SkyParams Sky;
}

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 worldPosition : TEXCOORD1;
};

static const uint IndexBuffer[6] = { 0, 1, 2, 2, 1, 3 };

static const float2 coords[4] = {
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 0.0f)
};

PSInput VSMain(uint vertexIdx : SV_VertexID)
{
    PSInput output;

    float2 uv = coords[IndexBuffer[vertexIdx]];
    float2 centered = uv - 0.5f;
    float distanceScale = 100000.0f;
    float3 localPos = float3(centered.x, centered.y, 0.0f) * distanceScale;

    float3 moonDir = length(Sky.MoonDirection) > 0.001 ? normalize(Sky.MoonDirection) : float3(0, -1, 0);

    float3 worldUp = float3(0.0001f, 1.0f, 0.0001f);
    float3 moonRight = normalize(cross(worldUp, moonDir));
    float3 moonUp    = cross(moonDir, moonRight);

    float3 worldPos = (localPos.x * moonRight) + (localPos.y * moonUp);
    worldPos += moonDir * distanceScale * 10;

    output.worldPosition = float4(worldPos, 1.0f);
    output.position = mul(output.worldPosition, ViewProjectionMatrix);
    output.uv = uv;

    return output;
}

struct PSOutput
{
	float4 albedo : SV_Target0;
};

static const float AlphaThreshold = 0.8f;

SamplerState LinearWrapSampler : register(s0);

float getEclipseFactor(float3 pixelLookDir)
{
	//float3 pixelLookDir = Sky.MoonDirection;

	// 1. Get normalized sun direction (invert Sky.SunDirection if it points *toward* light)
	float3 sunDir = normalize(-Sky.SunDirection); 
	
	// 2. Find how closely this pixel's look direction aligns with the sun
	float sunAlignment = dot(pixelLookDir, sunDir);

	// 3. Define the eclipse range using dot product values.
	// Since cos(0.5 degrees) is roughly 0.99996, we use very high thresholds.
	// Adjust these two values to make the dimming zone wider or sharper.
	float eclipseStart = 0.998f; // Begins dimming when very close to the sun
	float eclipseFull  = 0.9995f; // Reaches maximum dimming/totality

	// smoothstep returns 0 at start, 1 at full alignment
	float eclipseFactor = smoothstep(eclipseStart, eclipseFull, sunAlignment);
	
	// 4. Determine how dark the moon gets at peak eclipse (e.g., 0.0 = pure black silhouette)
	float minimumMoonBrightness = 0.05f; 
	float moonDimming = lerp(1.0f, minimumMoonBrightness, eclipseFactor);

	return moonDimming;
}

PSOutput PSMain(PSInput input)
{
	SamplerState materialSampler = GetDynamicMaterialSamplerLinear();
	float4 albedo = GetTexture2D(TexIdDiffuse).Sample(materialSampler, input.uv);
	float3 normalMap = GetTexture2D(TexIdNormal).Sample(materialSampler, input.uv).xyz * 2 - 1;

	if (albedo.a < AlphaThreshold) discard;

	{
		float2 uv = input.uv;

		// Convert UV to [-1,1] for sphere projection
		float2 p = uv.xy * 2 - 1;

		float r2 = dot(p, p);
		// Reconstruct sphere normal
		float3 normal = float3(p.x, p.y, sqrt(1 - r2));
		normal = (normal + normalMap.xyz * 0.3) / 1.3;

		// Calculate moon phase from sun-moon geometry
		float3 moonDir = normalize(Sky.MoonDirection);
		float3 worldUp = float3(0.0001f, 1.0f, 0.0001f);
		float3 mRight = normalize(cross(worldUp, moonDir));
		float3 mUp = cross(moonDir, mRight);

		// Sun direction toward light source, projected into billboard local space
		// Local z = toward viewer = -moonDir
		float3 sunToLight = -Sky.SunDirection;
		float3 lightDir;
		lightDir.x = dot(sunToLight, mRight);
		lightDir.y = dot(sunToLight, mUp);
		lightDir.z = dot(sunToLight, -moonDir);
		lightDir = normalize(lightDir);

		float phase = dot(normal, lightDir);
		phase = saturate(phase);

		albedo.rgb *= phase;

		// Extract camera world position from the 4th column of InvViewMatrix
		float3 cameraWorldPos = InvViewMatrix[3].xyz; 
		// Direction pointing from the camera toward this specific pixel on the moon
		float3 pixelLookDir = normalize(input.worldPosition.xyz - cameraWorldPos);

		float3 skyColor = getSkyFullColor(pixelLookDir, Sky, LinearWrapSampler);
		albedo.rgb += skyColor;
		albedo.rgb *= getEclipseFactor(pixelLookDir);
	}


	PSOutput output;
	output.albedo = float4(albedo.rgb, 1);

	return output;
}
