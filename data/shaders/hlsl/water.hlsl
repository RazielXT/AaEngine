float4x4 WorldViewProjectionMatrix;
float4x4 WorldMatrix;
float4x4 ViewProjectionMatrix;
//float4x4 InvViewMatrix;
//float4x4 InvProjectionMatrix;
//float4x4 InvViewProjectionMatrix;

float3 CameraPosition;
float Time;
float2 ViewportSizeInverse;
uint TexIdDiffuse;
uint TexIdNormal;
uint TexIdHeight;
uint TexIdDepth;

uint TexIdSceneDepth;
uint TexIdSceneColor;
uint TexIdSceneVoxel;
float ZMagic;

cbuffer SceneVoxelInfo : register(b1)
{
	float3 sceneCorner : packoffset(c0);
	float voxelDensity : packoffset(c0.w);
	float3 voxelSceneSize : packoffset(c1);
    float2 middleCone : packoffset(c2);
    float2 sideCone : packoffset(c2.z);
};

Texture2D<float4> GetTexture(uint index)
{
    return ResourceDescriptorHeap[index];
}

SamplerState g_sampler : register(s0);
SamplerState DepthSampler : register(s1);

float GetWavesSize(float2 uv)
{
	float4 albedo = GetTexture(TexIdHeight).SampleLevel(g_sampler, uv, 5);

	return albedo.r;
}

struct VSInput
{
    float4 position : POSITION;
	float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD0;
	float4 worldPosition : TEXCOORD1;
};

PSInput VSMain(VSInput input)
{
    PSInput result;

	float4 objPosition = input.position;
	
	float2 texUv = input.uv * 0.5 - Time / 4;
	
	float waveSize = GetWavesSize(texUv) * 1.01;
	objPosition.y += waveSize;

    result.position = mul(objPosition, WorldViewProjectionMatrix);
	result.worldPosition = mul(objPosition, WorldMatrix);
	result.normal = input.normal;//normalize(mul(input.normal, (float3x3)WorldMatrix));
	result.uv = texUv;

    return result;
}

float4 GetWaves(float2 uv)
{
	float4 albedo = GetTexture(TexIdDiffuse).Sample(g_sampler, uv);
	uv.xy -= Time * 0.137;
	albedo += GetTexture(TexIdDiffuse).Sample(g_sampler, uv);

	return albedo / 2;
}

/* float3 getWorldPos(float depth, float2 clipSpacePos)
{
    float4 screenPos = float4(clipSpacePos, depth, 1.0);
    float4 worldPosition = mul(screenPos, InvProjectionMatrix);
	//worldPosition = mul(worldPosition, InvViewMatrix);
	worldPosition /= worldPosition.w;
	return worldPosition.xyz / 10;
}
 */
/* 
float3 traceReflection(float3 wp, float3 normal, float3 camDirection, float2 uv, float currentDepthLinear)
{
	float3 traceDir = reflect(camDirection, normal);
	traceDir = mul(traceDir, (float3x3)ViewProjectionMatrix);

//	uv.y = 1 - uv.y;

	float3 screenStep = (traceDir) * 0.2;
	screenStep.x *= -1;
	float3 screenOffset = screenStep;
	Texture2D depthTex = GetTexture(TexIdLinearDepth);

	for (int i = 0; i < 10; i++)
	{
		float stepDepth = depthTex.Sample(g_sampler, uv - screenOffset.xy).r;
		if (stepDepth > currentDepthLinear + screenOffset.z)
			return GetTexture(TexIdSceneColor).Sample(g_sampler, uv - screenOffset.xy).rgb;
		
		screenOffset += screenStep;
	}

	return GetTexture(TexIdSceneColor).Sample(g_sampler, uv.xy).rgb;
} */

#define MAX_REFLECTION_RAY_MARCH_STEP 0.04f
#define NUM_RAY_MARCH_SAMPLES 32
#define NUM_BINARY_SEARCH_SAMPLES 8

float3 GetReflection(
	float3 ScreenSpaceReflectionVec, 
	float3 ScreenSpacePos,
	float dotView)
{
	float3 defaultColor = float3(0.55f, 0.75f, 0.9f);
	Texture2D DepthBuffer = GetTexture(TexIdSceneDepth);
	float3 PrevRaySample = ScreenSpacePos;
	float offset = 0.003f;

	// Raymarch in the direction of the ScreenSpaceReflectionVec until you get an intersection with your z buffer
	for (int RayStepIdx = 0; RayStepIdx<NUM_RAY_MARCH_SAMPLES; RayStepIdx++)
	{
		float3 RaySample = (RayStepIdx * MAX_REFLECTION_RAY_MARCH_STEP) * ScreenSpaceReflectionVec + ScreenSpacePos;
		float ZBufferVal = DepthBuffer.Sample(DepthSampler, RaySample.xy).r;

		//skip something too far in front
		float diff = ZBufferVal - RaySample.z;

		if (RaySample.z < ZBufferVal && diff < offset * dotView * 5)
		{
			float3 MinRaySample = PrevRaySample;
			float3 MaxRaySample = RaySample;
			float3 MidRaySample;
			for (int i = 0; i < NUM_BINARY_SEARCH_SAMPLES; i++)
			{
				MidRaySample = lerp(MinRaySample, MaxRaySample, 0.5);
				float ZBufferVal = DepthBuffer.Sample(DepthSampler, MidRaySample.xy).r;

				if (MidRaySample.z > ZBufferVal)
					MinRaySample = MidRaySample;
				else
					MaxRaySample = MidRaySample;
			}
			float diff = ZBufferVal - MidRaySample.z;
			
			//skip something too far in front
			if (diff > offset) return defaultColor;
			//background
			if (ZBufferVal == 0) return defaultColor;

			return GetTexture(TexIdSceneColor).Sample(g_sampler, MidRaySample.xy).rgb;
		}
		
		PrevRaySample = RaySample;
	}

	return defaultColor;
}

float rand(float2 co)
{
    return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}

float2 getJitter(float2 uv, float jitterAmount)
{
    float2 jitterOffset;
    jitterOffset.x = rand(uv + 0.5) * 2.0 - 1.0;
    jitterOffset.y = rand(uv + 1.5) * 2.0 - 1.0;
    return jitterOffset * jitterAmount;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float groundDistance = GetTexture(TexIdDepth).Load(int3(input.position.xy, 0)).r * 10000;
	float3 cameraVector = input.worldPosition.xyz - CameraPosition;
	float waterDistance = length(cameraVector);
	cameraVector = cameraVector / waterDistance;
//	float2 clipSpacePos = input.position.xy * ViewportSizeInverse * 2.0 - 1.0;
//	float3 worldPosGround = getWorldPos(depthGround, clipSpacePos);

	float fade = (groundDistance - waterDistance) / 20;
	float4 albedo = GetWaves(input.uv);
	albedo.a = albedo.r * 0.2 + fade;
	
	float3 voxelUV = (input.worldPosition.xyz-sceneCorner)/voxelSceneSize;
	
	//albedo.rgb = length(input.worldPosition.xyz - worldPosGround);
	//albedo.rgb = depthGround.rrr;
	
/* 	float4 screenPos = float4(clipSpacePos * -1, input.position.z, 1.0);
	float4 worldPosition = mul(screenPos, InvProjectionMatrix);
	worldPosition /= worldPosition.w;
	worldPosition = mul(worldPosition, InvViewMatrix);
	worldPosition /= worldPosition.w;
	albedo.rgb += worldPosition.xyz; */

	Texture3D voxelmap = ResourceDescriptorHeap[TexIdSceneVoxel];
	albedo.rgb *= (voxelmap.SampleLevel(g_sampler, voxelUV, 3).rgb + voxelmap.SampleLevel(g_sampler, voxelUV, 4).rgb * 2) * 10;

	float3 normal = normalize(input.normal + 0.2 * GetTexture(TexIdNormal).Sample(g_sampler,input.uv * 2).rgb); 
	float4 WorldNormal = float4(normal,1);
	float3 WorldPosition = input.worldPosition.xyz;

	float DeviceZ = input.position.z;
	float2 PixelUV = input.position.xy * ViewportSizeInverse;
	PixelUV += getJitter(PixelUV, 0.004);

	float4 ScreenSpacePos = float4(PixelUV, DeviceZ, 1.f);
	float3 ReflectionVector = reflect(cameraVector, WorldNormal.xyz);

	float4 PointAlongReflectionVec = float4(10.f*ReflectionVector + WorldPosition, 1.f);
	float4 ScreenSpaceReflectionPoint = mul(PointAlongReflectionVec, ViewProjectionMatrix);
	ScreenSpaceReflectionPoint /= ScreenSpaceReflectionPoint.w;
	ScreenSpaceReflectionPoint.xy = ScreenSpaceReflectionPoint.xy * float2(0.5, -0.5) + float2(0.5, 0.5);

// Compute the sreen space reflection vector as the difference of the two screen space points
	float3 ScreenSpaceReflectionVec = normalize(ScreenSpaceReflectionPoint.xyz - ScreenSpacePos.xyz);
	float dotView = dot(ScreenSpaceReflectionPoint.xyz, ScreenSpacePos.xyz);

	if (dotView > 0)
		albedo.rgb += GetReflection(ScreenSpaceReflectionVec, ScreenSpacePos.xyz, dotView) * 0.5;

	//float currentDepthLinear = 1.0 / (ZMagic * depth + 1.0);
/* 	albedo.rgb += traceReflection(input.worldPosition.xyz, input.normal.xyz, cameraVector / waterDistance, input.position.xy * ViewportSizeInverse, currentDepthLinear); */

	return albedo;
}
