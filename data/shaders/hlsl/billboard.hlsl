float4x4 ViewProjectionMatrix;
float4x4 InvViewMatrix;
uint2 ViewportSize;
float Time;
float DeltaTime;
float3 CameraPosition;
uint TexIdDiffuse;
uint TexIdNormal;

#ifdef ENTITY_ID
uint EntityId;
#endif

struct VegetationInfo
{
	float3 position;
	float rotation;
	float scale;
};

StructuredBuffer<VegetationInfo> GeometryBuffer : register(t0);

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 worldPosition : TEXCOORD1;
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

	float yRot = clamp((CameraPosition.y - v.position.y) / 1000, -0.5, 1);

	float3 pos = float3((uv.x - 0.5) * v.scale, top * v.scale * yRot, top * v.scale);

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

PSOutput PSMain(PSInput input)
{
	SamplerState colorSampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(colorSampler, input.uv);
	//albedo.rgb *= float3(0.6, 0.6, 0.8);

	if (albedo.a < AlphaThreshold) discard;

	float3 worldNormal = CameraPosition - input.worldPosition.xyz;
	float camDistance = length(worldNormal);

	if (albedo.a * AlphaFadeThreshold < camDistance) discard;

	worldNormal /= camDistance;

	float3 tangent = float3(0, 1, 0);
	float3 binormal = cross(worldNormal, tangent);
	float3x3 tbn = float3x3(tangent, binormal, worldNormal);
	float3 normalTex = GetTexture(TexIdNormal).Sample(colorSampler, 1 - input.uv.yx).rgb;
	//normalTex.b = 1;
	worldNormal = mul(normalTex.xyz * 2 - 1, tbn);

	PSOutput output;
	output.albedo = albedo;
	output.normals = float4(worldNormal, 1);
	output.motionVectors = float4(0, 0, 0, 0);

	return output;
}

void PSMainDepth(PSInput input)
{
	SamplerState colorSampler = SamplerDescriptorHeap[0];
	float4 albedo = GetTexture(TexIdDiffuse).Sample(colorSampler, input.uv);

	if (albedo.a < AlphaThreshold) discard;

	float camDistance = length(CameraPosition - input.worldPosition.xyz);

	if (albedo.a * AlphaFadeThreshold < camDistance) discard;
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

	float3 worldNormal = CameraPosition - input.worldPosition.xyz;
	float camDistance = length(worldNormal);

	if (albedo.a * AlphaFadeThreshold < camDistance) discard;

	worldNormal /= camDistance;

	PSOutputId output;
	output.id = uint4(EntityId, 0, 0, 0);
	output.position = float4(input.worldPosition.xyz, 0);
	output.normal = float4(worldNormal, 0);
	return output;
}

#endif
