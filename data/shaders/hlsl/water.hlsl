#include "VoxelConeTracingCommon.hlsl"
#include "postprocess/WorldReconstruction.hlsl"

float4x4 WorldMatrix;
float4x4 ViewProjectionMatrix;
float4x4 InvViewProjectionMatrix;

float3 CameraPosition;
float Time;
float2 ViewportSizeInverse;
uint TexIdDiffuse;
uint TexIdNormal;
uint TexIdHeight;
uint TexIdSceneDepthHigh;
uint TexIdSceneDepthLow;
uint TexIdSceneDepthVeryLow;
uint TexIdSceneColor;
uint TexIdSkybox;
//float ZMagic;

#ifdef ENTITY_ID
uint EntityId;
#endif

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

float GetWavesSize(float2 uv)
{
	float4 albedo = GetTexture(TexIdHeight).SampleLevel(LinearWrapSampler, uv, 5);

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
	
	float waveSize = GetWavesSize(texUv) * 0.01;
	objPosition.y += waveSize;

	result.worldPosition = mul(objPosition, WorldMatrix);
    result.position = mul(result.worldPosition, ViewProjectionMatrix);
	result.normal = input.normal;//normalize(mul(input.normal, (float3x3)WorldMatrix));
	result.uv = texUv / 10;

    return result;
}

float4 GetWaves(float2 uv)
{
	float4 albedo = GetTexture(TexIdDiffuse).Sample(LinearWrapSampler, uv);
	uv.xy -= Time * 0.137;
	albedo += GetTexture(TexIdDiffuse).Sample(LinearWrapSampler, uv);

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
	Texture2D DepthBufferVeryLow = GetTexture(TexIdSceneDepthVeryLow);
	Texture2D DepthBufferLow = GetTexture(TexIdSceneDepthLow);
	Texture2D DepthBufferHigh = GetTexture(TexIdSceneDepthHigh);
	float3 PrevRaySample = ScreenSpacePos;
	float MaxOffset = 0.00005f;
	float Retry = 1;

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
			//if (false)
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

			//skip something too far in front, this allows marching behind objects
			if (ZDiff > MaxOffset * RayStepIdx)
			{
				Retry = 0;
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
    return frac(sin(dot(co.xy + Time, float2(12.9898, 78.233))) * 43758.5453);
}

float2 getJitter(float2 uv, float jitterAmount)
{
    float2 jitterOffset;
    jitterOffset.x = rand(uv + 0.5) * 2.0 - 1.0;
    jitterOffset.y = rand(uv + 1.5) * 2.0 - 1.0;
    return jitterOffset * jitterAmount;
}

struct PSOutput
{
    float4 color : SV_Target0;
    float4 fill : SV_Target1;
};

float checkerboard(float2 uv)
{
    // Compute the checkerboard pattern
    int checkX = int(floor(uv.x) % 2);
    int checkY = int(floor(uv.y) % 2);

    // Determine the color based on the pattern
    float color = (checkX == checkY) ? 1 : 0;

    return color;
}

PSOutput PSMain(PSInput input)
{
	float2 ScreenUV = input.position.xy * ViewportSizeInverse;

	Texture2D DepthBufferHigh = GetTexture(TexIdSceneDepthHigh);
	float groundZ = DepthBufferHigh.Sample(LinearWrapSampler, ScreenUV).r;
	float3 groundPosition = ReconstructWorldPosition(ScreenUV, groundZ, InvViewProjectionMatrix);
	float groundDistance = length(input.worldPosition.xyz - groundPosition);

	float3 cameraVector = input.worldPosition.xyz - CameraPosition;
	float camDistance = length(cameraVector);
	cameraVector = cameraVector / camDistance; //normalize
//	float2 clipSpacePos = input.position.xy * ViewportSizeInverse * 2.0 - 1.0;
//	float3 worldPosGround = getWorldPos(depthGround, clipSpacePos);

	const float FadeDistance = 20;
	float fade = groundDistance / FadeDistance;
	//float fade = smoothstep(0, FadeDistance, groundDistance - camDistance);

	float4 albedo = GetWaves(input.uv);
	albedo.a = albedo.r * 0.2 + fade;
	albedo.rgb *= float3(0.2,0.8, 0.6);
	float3 voxelUV = (input.worldPosition.xyz-VoxelInfo.FarVoxels.Offset)/VoxelInfo.FarVoxels.SceneSize;
	
	//albedo.rgb = length(input.worldPosition.xyz - worldPosGround);
	//albedo.rgb = depthGround.rrr;

	Texture3D voxelmap = ResourceDescriptorHeap[VoxelInfo.FarVoxels.TexId];
	albedo.rgb *= max(0.3, (voxelmap.SampleLevel(LinearWrapSampler, voxelUV, 3).rgb + voxelmap.SampleLevel(LinearWrapSampler, voxelUV, 4).rgb * 2) * 10);

	float3 normal = normalize(input.normal + 0.1 * GetTexture(TexIdNormal).Sample(LinearWrapSampler,input.uv).rgb); 
	float4 WorldNormal = float4(normal,1);
	float3 WorldPosition = input.worldPosition.xyz;

	float DeviceZ = input.position.z;
	float2 SsrPixelUV = ScreenUV;

	float4 ScreenSpacePos = float4(SsrPixelUV, DeviceZ, 1.f);
	float3 ReflectionVector = reflect(cameraVector, WorldNormal.xyz);
	//ReflectionVector.xy += getJitter(ScreenUV, 0.01);

	float4 PointAlongReflectionVec = float4(ReflectionVector + WorldPosition, 1.f);
	float4 ScreenSpaceReflectionPoint = mul(PointAlongReflectionVec, ViewProjectionMatrix);
	ScreenSpaceReflectionPoint /= ScreenSpaceReflectionPoint.w;
	ScreenSpaceReflectionPoint.xy = ScreenSpaceReflectionPoint.xy * float2(0.5, -0.5) + float2(0.5, 0.5);
	
	// Compute the sreen space reflection vector as the difference of the two screen space points
	float3 ScreenSpaceReflectionVec = normalize(ScreenSpaceReflectionPoint.xyz - ScreenSpacePos.xyz);

	float4 reflection = 0;

	//if (checkerboard(input.position.xy) != 0)
	{
		reflection = GetReflection(ScreenSpaceReflectionVec, ScreenSpacePos.xyz, -ReflectionVector);
		
		if (false && reflection.a == 0)
		{
			float3 jjj = getJitter(ScreenUV, 0.5).xyx;
			SceneVoxelChunkInfo Voxels = VoxelInfo.FarVoxels;

			float3 rayDir = ReflectionVector;
			float stepSize = 6.f; // Adjust as needed
			float3 rayStart = input.worldPosition.xyz + rayDir * 3 * stepSize;

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
	}

	float fresnel = saturate((1 - abs(dot(cameraVector, normal))));
	fresnel *= fresnel;

	albedo.rgb = lerp(albedo.rgb, reflection.rgb, fresnel);
	//albedo.rgb = reflection.rgb;
	float3 sceneColor = GetTexture(TexIdSceneColor).Sample(LinearWrapSampler, saturate(ScreenUV + normal.xz * 0.1f)).rgb;

	albedo.rgb = lerp(sceneColor, albedo.rgb, saturate(albedo.a));
	albedo.a = 1;

	float3 fogColor = float3(0.6,0.6,0.7);
	albedo.rgb = lerp(albedo.rgb, fogColor, min(0.7, saturate(camDistance / 15000)));

	PSOutput output;
	output.color = albedo;
	//output.color.rgb = reflection.rgb;
	output.fill = float4(1,1,1, reflection.a);

	return output;
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
	PSOutputId output;
	output.id = uint4(EntityId, 0, 0, 0);
	output.position = float4(input.worldPosition.xyz, 0);
	output.normal = float4(input.normal.xyz, 0);
	return output;
}

#endif
