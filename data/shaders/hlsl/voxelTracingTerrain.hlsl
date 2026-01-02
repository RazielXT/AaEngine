float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float3 CameraPosition;
float BlendDistance;
uint2 ViewportSize;
uint TexIdDiffuse;
uint TexIdGrass;
uint TexIdNormal;
uint TexIdGrassNormal;
uint TexIdSpread;
uint TexIdDisplacement;

struct VS_Input
{
    float4 p : POSITION;
    float3 n : NORMAL;
    float3 t : TANGENT;
    float4 nhLod : TEXCOORD1;
    float3 tLod : TEXCOORD2;
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float4 wp : TEXCOORD1;
	float www : TEXCOORD4;
};

float getDistanceBlend(float dist, float sceneSize)
{
	sceneSize *= 0.5f;
	float maxDistance = sceneSize * 0.75f;

	const float fadeStart = maxDistance * 0.8f;
	const float fadeRange = maxDistance * 0.2f;

	return saturate((dist - fadeStart) / fadeRange); 
}

float getDistanceBlend2(float dist, float maxDistance)
{
	const float fadeStart = maxDistance * 0.8f;
	const float fadeRange = maxDistance * 0.2f;

	return saturate((dist - fadeStart) / fadeRange); 
}

PS_Input VSMain(VS_Input vin)
{
    PS_Input vsOut;

    vsOut.wp = mul(vin.p, WorldMatrix);

	float camDistance = length(CameraPosition.xz - vsOut.wp.xz);
	float distanceLerp = getDistanceBlend2(camDistance, BlendDistance);
	vsOut.wp.y -= vin.nhLod.w * distanceLerp;
	vsOut.www = distanceLerp;
    vsOut.pos = mul(vsOut.wp, ViewProjectionMatrix);
    vsOut.uv = vsOut.wp.xz / 25.f;
    vsOut.normal = lerp(vin.n, vin.nhLod.xyz, distanceLerp);
    vsOut.tangent = lerp(vin.t, vin.tLod.xyz, distanceLerp);

    return vsOut;
}

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}

float3 CreateDistanceWeigths3(float camDistance, float distances[3], float scales[3])
{
	float lerpWeight = 1;
	float closeDetailUv = 1;
	float farDetailUv = scales[0];

	for (int i = 0; i < 2; i++)
	{
		lerpWeight = saturate((camDistance - distances[i]) / (distances[i + 1] - distances[i]));
		closeDetailUv = farDetailUv;
		farDetailUv = scales[i + 1];
		
		if (lerpWeight != 1.f)
			break;
	}
	
	return float3(closeDetailUv, farDetailUv, lerpWeight);
}

float3 CreateDistanceWeigths5(float camDistance, float distances[5], float scales[5])
{
	float lerpWeight = 1;
	float closeDetailUv = 1;
	float farDetailUv = scales[0];

	for (int i = 0; i < 4; i++)
	{
		lerpWeight = saturate((camDistance - distances[i]) / (distances[i + 1] - distances[i]));
		closeDetailUv = farDetailUv;
		farDetailUv = scales[i + 1];
		
		if (lerpWeight != 1.f)
			break;
	}
	
	return float3(closeDetailUv, farDetailUv, lerpWeight);
}

float3 ReadDistanceTexture(uint texId, SamplerState sampler, float2 uv, float3 weights)
{
	float3 normalTexClose = GetTexture(texId).Sample(sampler, uv * weights.x).rgb;
	float3 normalTex = GetTexture(texId).Sample(sampler, uv * weights.y).rgb;

	return lerp(normalTexClose, normalTex, weights.z);
}

#ifdef TERRAIN_SCAN

struct PSOutput
{
    float4 color : SV_Target0;
    float4 normals : SV_Target1;
    float4 types : SV_Target2;
    float4 height : SV_Target3;
};

PSOutput PSMain(PS_Input pin)
{
	SamplerState diffuse_sampler = SamplerDescriptorHeap[0];
	
	float3 rock = GetTexture(TexIdDiffuse).Sample(diffuse_sampler, pin.uv).rgb;
	float3 green = GetTexture(TexIdGrass).Sample(diffuse_sampler, pin.uv * 5).rgb * float3(0.9, 1, 0.9);
	float3 brown = float3(0.45, 0.35, 0.25) * 1.5f * rock; // Color for brown
	
	float spread = GetTexture(TexIdSpread).Sample(diffuse_sampler, pin.uv / 10).r;
	float detailSpread = GetTexture(TexIdSpread).Sample(diffuse_sampler, pin.uv * 2).r * spread * spread  * 0.1;
	float spread2 = GetTexture(TexIdSpread).Sample(diffuse_sampler, (pin.uv  + 0.5) / 3).r;
	float detailSpread2 = GetTexture(TexIdSpread).Sample(diffuse_sampler, (pin.uv  + 0.5) * 10).r * spread2 * spread2 * 0.1;
	
	float rockWeight = smoothstep(0.77 + detailSpread, 0.78 + detailSpread, pin.normal.y);
	float3 albedoMid = lerp(rock, brown, rockWeight);
	float grassWeight = smoothstep(0.78 + detailSpread2, 0.8 + detailSpread2, pin.normal.y);
	float3 albedo = lerp(albedoMid, green, grassWeight);
	
	PSOutput output;
    output.color = float4(albedo, 1);
	output.normals = float4(pin.normal, 1);
	output.types = float4(rockWeight, grassWeight, 0, 0);
	output.height = float4(pin.wp.y, 0, 0, 0);

	return output;
}

#else

struct PSOutput
{
	float4 albedo : SV_Target0;
	float4 normals : SV_Target1;
	float4 motionVectors : SV_Target2;
};

PSOutput PSMain(PS_Input pin)
{
	SamplerState diffuse_sampler = SamplerDescriptorHeap[0];
	float3 cameraView = CameraPosition - pin.wp.xyz;
	float camDistance = length(cameraView);

	float3 inNormal = pin.normal;
	//inNormal.y /= 2;
	//inNormal = normalize(inNormal);
	float3 bin = cross(inNormal, pin.tangent);

	float spread = GetTexture(TexIdSpread).Sample(diffuse_sampler, pin.uv / 10).r;
	float detailSpread = GetTexture(TexIdSpread).Sample(diffuse_sampler, pin.uv * 2).r * spread * spread  * 0.1;
	float spread2 = GetTexture(TexIdSpread).Sample(diffuse_sampler, (pin.uv  + 0.5) / 3).r;
	float detailSpread2 = GetTexture(TexIdSpread).Sample(diffuse_sampler, (pin.uv  + 0.5) * 10).r * spread2 * spread2 * 0.1;

	float rockWeight = smoothstep(0.77 + detailSpread, 0.78 + detailSpread, pin.normal.y);
	float grassWeight = smoothstep(0.78 + detailSpread2, 0.8 + detailSpread2, pin.normal.y);

	float diffuseStepDistance[5] = { 0, 50, 500, 2000, 6000 };
	float diffuseStepScale[5] = { 4, 2, 1.f /2, 1.f /5, 1.f /20 };
	float3 diffuseDistanceWeights = CreateDistanceWeigths5(camDistance, diffuseStepDistance, diffuseStepScale);
	float3 rock = ReadDistanceTexture(TexIdDiffuse, diffuse_sampler, pin.uv, diffuseDistanceWeights);

	float3 green = GetTexture(TexIdGrass).Sample(diffuse_sampler, pin.uv * 5).rgb * float3(0.9, 1, 0.9);
	float3 brown = float3(0.45, 0.35, 0.25) * 1.5f * rock; // Color for brown

	float3 albedoMid = lerp(rock, brown, rockWeight);
	float3 albedo = lerp(albedoMid, green, grassWeight);

	uint NormalTex = rockWeight > 0.5 ? TexIdGrassNormal : TexIdNormal;
	float3 normalTex = ReadDistanceTexture(NormalTex, diffuse_sampler, pin.uv, diffuseDistanceWeights);
	normalTex.b = 1;

    float3x3 tbn = float3x3(pin.tangent, bin, pin.normal);
    float3 normal = mul(normalTex.xyz * 2 - 1, tbn); // to object space

	float3x3 worldMatrix = (float3x3)WorldMatrix;
	float3 worldDetailNormal = normalize(mul(normal, worldMatrix));

	PSOutput output;
    output.albedo = float4(albedo, 1);
	output.normals = float4(worldDetailNormal, 1);
	output.motionVectors = float4(0, 0, 0, 0);

	return output;
}

#endif
