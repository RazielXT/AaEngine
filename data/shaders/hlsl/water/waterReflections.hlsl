#include "../VoxelConeTracingCommon.hlsl"
#include "../postprocess/WorldReconstruction.hlsl"
#include "../postprocess/PostProcessCommon.hlsl"

float4x4 WorldMatrix;
float4x4 ViewProjectionMatrix;
float4x4 InvViewProjectionMatrix;

float3 CameraPosition;
float Time;
uint TexIdSceneColor;
uint TexIdSceneDepthHigh;
uint TexIdWaterColor;
uint TexIdWaterNormal;
uint TexIdWaterDepth;
uint TexIdSkybox;

cbuffer SceneVoxelInfo : register(b1)
{
	SceneVoxelCbuffer VoxelInfo;
};

Texture2D<float4> GetTexture(uint index)
{
	return ResourceDescriptorHeap[index];
}

TextureCube<float4> GetTextureCube(uint index)
{
	return ResourceDescriptorHeap[index];
}

SamplerState LinearWrapSampler : register(s0);
SamplerState DepthSampler : register(s1);
SamplerState PointSampler : register(s2);

static const int NUM_RAY_MARCH_SAMPLES = 64;
static const float MAX_REFLECTION_RAY_MARCH_STEP = (1.0f / (NUM_RAY_MARCH_SAMPLES + 1));
static const int NUM_BINARY_SEARCH_SAMPLES = 8;

float borderBlend(float2 uv, float edgeThreshold)
{
	// Calculate the distance from the UV coordinates to the nearest edge
	float left = uv.x;
	float right = 1.0 - uv.x;
	float top = uv.y;
	float bottom = 1.0 - uv.y;

	// Determine the minimum distance to an edge
	float minDist = min(min(left, right), min(top, bottom));

	// Perform linear interpolation (lerp) based on the dist / edgeThresholdance to the edge
	float alpha = saturate(minDist / edgeThreshold);

	return alpha;
}


float4 GetReflection(
	float3 ScreenSpaceReflectionVec, 
	float3 ScreenSpacePos,
	float3 reflection)
{
	//Texture2D DepthBufferVeryLow = GetTexture(TexIdSceneDepthVeryLow);
	//Texture2D DepthBufferLow = GetTexture(TexIdSceneDepthLow);
	Texture2D DepthBufferHigh = GetTexture(TexIdSceneDepthHigh);
	float3 PrevRaySample = ScreenSpacePos;

	float3 skyboxColor = GetTextureCube(TexIdSkybox).Sample(LinearWrapSampler, reflection).rgb;

	// Raymarch in the direction of the ScreenSpaceReflectionVec until you get an intersection with your z buffer
	//if (ScreenSpaceReflectionVec.z < 0 && !(ScreenSpaceReflectionVec.x == 0 && ScreenSpaceReflectionVec.y == 0))
	for (int RayStepIdx = 0; RayStepIdx<NUM_RAY_MARCH_SAMPLES; RayStepIdx++)
	{
		float3 RaySample = (RayStepIdx * MAX_REFLECTION_RAY_MARCH_STEP) * ScreenSpaceReflectionVec + ScreenSpacePos;

		if (RaySample.x < 0 || RaySample.y < 0 || RaySample.x > 1 || RaySample.y > 1)
			break;

		float ZBufferVal = DepthBufferHigh.Sample(DepthSampler, RaySample.xy).r;

		if (RaySample.z < ZBufferVal)
		{
			float3 MinRaySample = PrevRaySample;
			float3 MaxRaySample = RaySample;
			float3 MidRaySample;
			float MaxDistance = 1;
			float MinDistance = 0;
			float DistanceWeight = 1;

			for (int i = 0; i < NUM_BINARY_SEARCH_SAMPLES; i++)
			{
				MidRaySample = lerp(MinRaySample, MaxRaySample, 0.5);
				ZBufferVal = DepthBufferHigh.Sample(DepthSampler, MidRaySample.xy).r;
				
				if (MidRaySample.z > ZBufferVal)
				{
					MinRaySample = MidRaySample;
					MinDistance += DistanceWeight;
				}
				else
				{
					MaxRaySample = MidRaySample;
					MaxDistance -= DistanceWeight;
				}

				DistanceWeight *= 0.5f;
			}

			float ZDiff = abs(ZBufferVal - MidRaySample.z);
			
			//secant
			{
				const float TotalDistance = MaxDistance - MinDistance;
				const float3 RefinedMidRaySample = lerp(MinRaySample, MaxRaySample, MinDistance / TotalDistance);

				ZBufferVal = DepthBufferHigh.Sample(DepthSampler, RefinedMidRaySample.xy).r;
				float refinedDiff = abs(ZBufferVal - RefinedMidRaySample.z);

				if (refinedDiff < ZDiff && MaxRaySample.z < ZBufferVal)
				{
					ZDiff = refinedDiff;
					MidRaySample = RefinedMidRaySample;
				}
			}

			//background
			if (ZBufferVal == 0)
				break;

			const float MaxOffset = 0.00005f;
			//skip something too far in front, this allows marching behind objects
			if (ZDiff > MaxOffset * RayStepIdx)
			{
				continue;
			}

			float alpha = borderBlend(MidRaySample.xy, 0.15f);
			float3 color = lerp(skyboxColor, GetTexture(TexIdSceneColor).Sample(LinearWrapSampler, MidRaySample.xy).rgb, alpha);
			return float4(color, 1);
		}

		PrevRaySample = RaySample;
	}

	return float4(skyboxColor, 0);
}

float rand(float2 co)
{
	return frac(sin(dot(co.xy + Time * 0.000001, float2(12.9898, 78.233))) * 43758.5453);
}

float2 getJitter(float2 uv, float jitterAmount)
{
	float2 jitterOffset;
	jitterOffset.x = rand(uv + 0.5) * 2.0 - 1.0;
	jitterOffset.y = rand(uv + 1.5) * 2.0 - 1.0;
	return jitterOffset * jitterAmount;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
	const float2 ScreenUV = input.TexCoord;

	Texture2D WaterBuffer = GetTexture(TexIdWaterColor);
	if (WaterBuffer.Sample(LinearWrapSampler, ScreenUV).a == 0)
		return float4(0,0,0,0);

	Texture2D DepthBuffer = GetTexture(TexIdWaterDepth);
	float worldZ = DepthBuffer.Sample(DepthSampler, ScreenUV).r;
	float3 WorldPosition = ReconstructWorldPosition(ScreenUV, worldZ, InvViewProjectionMatrix);

	float3 cameraVector = normalize(WorldPosition - CameraPosition);

	float3 normal = GetTexture(TexIdWaterNormal).Sample(LinearWrapSampler,ScreenUV).rgb; 
	float4 WorldNormal = float4(normal,1);

	float4 ScreenSpacePos = float4(ScreenUV, worldZ, 1.f);
	float3 ReflectionVector = reflect(cameraVector, WorldNormal.xyz);
	//ReflectionVector.xy += getJitter(ScreenUV, 0.01);

	float4 PointAlongReflectionVec = float4(ReflectionVector + WorldPosition, 1.f);
	float4 ScreenSpaceReflectionPoint = mul(PointAlongReflectionVec, ViewProjectionMatrix);
	ScreenSpaceReflectionPoint /= ScreenSpaceReflectionPoint.w;
	ScreenSpaceReflectionPoint.xy = ScreenSpaceReflectionPoint.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
	
	// Compute the sreen space reflection vector as the difference of the two screen space points
	float3 ScreenSpaceReflectionVec = normalize(ScreenSpaceReflectionPoint.xyz - ScreenSpacePos.xyz);

	float4 reflection = GetReflection(ScreenSpaceReflectionVec, ScreenSpacePos.xyz, -ReflectionVector);
	if (false && reflection.a == 0)
	{
		float3 jjj = getJitter(ScreenUV, 0.5).xyx;
		SceneVoxelChunkInfo Voxels = VoxelInfo.FarVoxels;
		Texture3D voxelmap = ResourceDescriptorHeap[Voxels.TexId];

		float3 rayDir = ReflectionVector;
		float stepSize = 6.f; // Adjust as needed
		float3 rayStart = WorldPosition + rayDir * 3 * stepSize;

		float4 nearVoxel = 0.xxxx;
		for (int i = 1; i < 64; ++i)
		{
			// Move the ray position
			float3 rayPos = rayStart + rayDir * stepSize * i;
			stepSize += 1 / 16.f;

			// Convert to UVW coordinates (range [0, 1])
			float3 voxelUV = (rayPos - Voxels.Offset) / Voxels.SceneSize;

			if (voxelUV.x <= 0 || voxelUV.x >= 1 || voxelUV.y <= 0 || voxelUV.y >= 1 || voxelUV.z <= 0 || voxelUV.z >= 1)
			{
				nearVoxel.a = 0;
				break;
			}

			// Sample the 3D texture
			nearVoxel = voxelmap.SampleLevel(PointSampler, voxelUV, 2);

			// If a filled voxel is hit
			if (nearVoxel.a > 0.f)
			{		
				float detailStepSize = stepSize / 4.f; // Adjust as needed

				for (int i = 4; i >= 0; i--)
				{
					float3 rayPosDetail = rayPos - rayDir * i * detailStepSize + jjj;
					float3 voxelUV = (rayPosDetail - Voxels.Offset) / Voxels.SceneSize;
					nearVoxel = voxelmap.SampleLevel(PointSampler, voxelUV, 0);
					
					if (nearVoxel.a > 0.f)
						break;
				}
			}

			// If a filled voxel is hit
			if (nearVoxel.a > 0.f)
				break;
		}

		if (nearVoxel.a != 0.f)	
			reflection = float4(nearVoxel.rgb , 1);
	}

	float fresnel = saturate((1 - abs(dot(cameraVector, normal))));
	fresnel *= fresnel;

	reflection.a = fresnel;

	return reflection;
}
