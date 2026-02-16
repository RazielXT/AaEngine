#include "ShadowsPssm.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 InvViewMatrix;
uint2 ViewportSize;
float Time;
float DeltaTime;
float3 ViewCameraPosition;
uint TexIdDiffuse;
uint TexIdNormal;
uint TexIdEdges;
float3 ViewCameraDirection;

#ifdef ENTITY_ID
uint EntityId;
#endif

struct VegetationInfo
{
	float3 position;
	float rotation;
	float scale;
	float random;
};

cbuffer PSSMShadows : register(b1)
{
	SunParams Sun;
}

StructuredBuffer<VegetationInfo> GeometryBuffer : register(t0);

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
	float random : TEXCOORD1;
	float4 worldPosition : TEXCOORD2;
};

static const float2 coords[4] = {
	float2(1.0f, 1.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 0.0f)
};

PSInput VSMain(uint vertexIdx : SV_VertexId, uint instanceId : SV_InstanceID)
{
	PSInput output;

	float2 uv = coords[vertexIdx];
	float top = 1 - uv.y;

	VegetationInfo v = GeometryBuffer[instanceId];

	float yRot = clamp((ViewCameraPosition.y - v.position.y) / 1000, -0.5, 1);
	float3 pos = float3((uv.x - 0.5) * v.scale * 0.5, top * v.scale * yRot, top * v.scale);

	//billboard
	const float3 faceDir = float3(1, 0, 0);
	float3 basisX = mul(faceDir, (float3x3)InvViewMatrix);
	const float3 worldUp = float3(0, 1, 0);
	float3 basisY = normalize(cross(basisX, worldUp));
	float3 basisZ = cross(basisY, basisX);
	float3x3 transformMat = float3x3(basisX, basisY, basisZ);
	pos = mul(pos, transformMat);

	pos += v.position;

	// Output position and UV
	output.worldPosition = float4(pos,1);
	output.position = mul(output.worldPosition, ViewProjectionMatrix);
	output.uv = uv;
	output.random = v.random;

	return output;
}

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
}

struct PSOutput
{
	float4 albedo : SV_Target0;
	float4 normals : SV_Target1;
	float4 motionVectors : SV_Target2;
};

static const float AlphaThreshold = 0.8f;
static const float AlphaFadeThreshold = 5000;

float3 DecodeNormalUNORM(float2 xy)
{
	float2 nxy = xy * 2.0 - 1.0; // convert to -1..1
	float z = sqrt(saturate(1.0 - dot(nxy, nxy)));
	return float3(nxy, z);
}

float Hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

// Apply color variation to a base palette color
float3 ApplyTreeColorVariation(float3 baseColor, float3 otherColor, float random)
{
    // Use XZ so trees in a row don't all match vertically
    float rnd = Hash12(random.xx);

    return lerp(otherColor, baseColor, rnd);
}

float3 color_blend(float3 normalTex, float3 normalTex2)
{
	float3 r = lerp(
	2.0 * normalTex * normalTex2,
	1.0 - 2.0 * (1.0 - normalTex) * (1.0 - normalTex2),
	step(0.5, normalTex)
);

	return r;
}

PSOutput PSMain(PSInput input)
{
	SamplerState colorSampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(colorSampler, input.uv);
	albedo.rgb *= ApplyTreeColorVariation(float3(0.9, 0.55, 0.35), float3(0.9, 0.35, 0.15), input.random) * saturate(0.5 + 0.5 * abs(input.random * 2 - 1));

	if (albedo.a < AlphaThreshold) discard;

	float3 worldNormal = ViewCameraPosition - input.worldPosition.xyz;
	//worldNormal.y = 0;
	float camDistance = length(worldNormal);

	//if (albedo.a * AlphaFadeThreshold < camDistance) discard;

	worldNormal /= camDistance;
	//worldNormal = -ViewCameraDirection;

	float edges = GetTexture(TexIdEdges).Sample(colorSampler, input.uv.xy).r;
	//worldNormal = lerp(-Sun.Direction, worldNormal, edges);

	float3 binormal = float3(0, 1, 0);
	float3 tangent = cross(binormal, worldNormal);
	binormal = cross(worldNormal,tangent);
	float3x3 tbn = float3x3(tangent, binormal, worldNormal);

	float3 N = worldNormal;
	// 2. The Bitangent (B) is the vertical axis (y-axis)
	// We use (0,1,0) here because your billboard only rotates around Y
	float3 B = float3(0, 1, 0);
	// 3. The Tangent (T) must be perpendicular to both
	// This represents the horizontal 'right' vector of the billboard
	float3 T = normalize(cross(B, N));
	// 4. Re-calculate B to ensure exact orthonormality (optional but recommended)
	B = cross(N, T);
	// Construct the TBN matrix
	// Row-major construction for mul(vector, tbn)
	//float3x3 tbn = float3x3(T, B, N);

	float3 normalTex = GetTexture(TexIdNormal).Sample(colorSampler, input.uv.xy).rgb;
	//normalTex = DecodeNormalUNORM(normalTex.xy);
	//normalTex.x = 1 - saturate(lerp(normalTex.x, (input.uv.x - 0.25) * 2, 1));
	//normalTex.y = 0.5;
	//normalTex.b = saturate(normalTex.b + 0.9);

	float2 uv = input.uv * 2.0 - 1; // now in [-1,1]
	float x = -uv.x;
	float y = -uv.y * 0.5 + 0.5;
	float z = sqrt(saturate(1.0 - x*x - y*y)); // hemisphere facing camera

	normalTex = float3(x, y, z);
	//normalTex = color_blend(normalTex, float3(x, y, z));

	worldNormal = mul(normalTex.xyz, tbn);

	PSOutput output;
	output.albedo = float4(albedo.rgb, 0);
	output.normals = float4(worldNormal, 1);
	output.motionVectors = float4(0, 0, 0, 0);

	return output;
}

void PSMainDepth(PSInput input)
{
	SamplerState colorSampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(colorSampler, input.uv);

	if (albedo.a < AlphaThreshold) discard;

	float camDistance = length(ViewCameraPosition - input.worldPosition.xyz);

	//if (albedo.a * AlphaFadeThreshold < camDistance) discard;
}

#ifdef ENTITY_ID

struct PSOutputId
{
    uint4 id : SV_Target0;
    float4 position : SV_Target1;
	float4 normal : SV_Target2;
};

PSOutputId PSMainEntityId(PSInput input)
{
	SamplerState colorSampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(colorSampler, input.uv);

	if (albedo.a < AlphaThreshold) discard;

	float3 worldNormal = ViewCameraPosition - input.worldPosition.xyz;
	worldNormal.y = 0;
	float camDistance = length(worldNormal);

	//if (albedo.a * AlphaFadeThreshold < camDistance) discard;

	worldNormal /= camDistance;

	PSOutputId output;
	output.id = uint4(EntityId, 0, 0, 0);
	output.position = float4(input.worldPosition.xyz, 0);
	output.normal = float4(worldNormal, 0);
	return output;
}

#endif
