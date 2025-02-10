#include "VoxelConeTracingCommon.hlsl"
#include "ShadowsCommon.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float4x4 PreviousWorldMatrix;
float3 CameraPosition;
uint2 ViewportSize;
uint VoxelIndex;
uint VoxelMip;

cbuffer SceneVoxelInfo : register(b2)
{
    SceneVoxelCbufferIndexed VoxelInfo;
};

struct VS_Input
{
    float4 p : POSITION;
    float3 n : NORMAL;
    float3 t : TANGENT;
    float2 uv : TEXCOORD0;
};

struct PS_Input
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float4 wp : TEXCOORD1;
	float4 currentPosition : TEXCOORD2;
	float4 previousPosition : TEXCOORD3;
};

PS_Input VSMain(VS_Input vin)
{
    PS_Input vsOut;

    vsOut.wp = mul(vin.p, WorldMatrix);
    vsOut.pos = mul(vsOut.wp, ViewProjectionMatrix);
    vsOut.uv = vin.uv;
    vsOut.normal = vin.n;
    vsOut.tangent = vin.t;
	
	float4 previousWorldPosition = mul(vin.p, PreviousWorldMatrix);
	vsOut.previousPosition = mul(previousWorldPosition, ViewProjectionMatrix);
	vsOut.currentPosition = vsOut.pos;

    return vsOut;
}

Texture3D<float4> GetTexture3D(uint index)
{
    return ResourceDescriptorHeap[index];
}

struct PSOutput
{
    float4 color : SV_Target0;
    float4 normals : SV_Target1;
    float4 camDistance : SV_Target2;
    float4 motionVectors : SV_Target3;
};

SamplerState PointSampler : register(s0);

PSOutput PSMain(PS_Input pin)
{
	SceneVoxelChunkInfo Voxels = VoxelInfo.Voxels[VoxelIndex];

	Texture3D voxelmap = GetTexture3D(Voxels.TexId);

	float3 rayDir = normalize(pin.wp.xyz - CameraPosition);
	float3 rayStart = pin.wp.xyz;
	float stepSize = 1.f / Voxels.Density; // Adjust as needed

	float4 nearVoxel = 0.xxxx;
	int i = 0;
	for (; i < 128; ++i)
	{
		// Move the ray position
		float3 rayPos = rayStart + rayDir * stepSize * i;
		//stepSize += Voxels.Density * 32.f;

		// Convert to UVW coordinates (range [0, 1])
		float3 voxelUV = (rayPos - Voxels.Offset) / Voxels.SceneSize;

		if (voxelUV.x <= 0 || voxelUV.x >= 1 || voxelUV.y <= 0 || voxelUV.y >= 1 || voxelUV.z <= 0 || voxelUV.z >= 1)
		{
			nearVoxel.a = 0;
			break;
		}

		// Sample the 3D texture
		nearVoxel = voxelmap.SampleLevel(PointSampler, voxelUV, min(VoxelMip + 2, 7));

		// If a filled voxel is hit
		if (nearVoxel.a > 0.f)
		{		
			float detailStepSize = stepSize / 32.f; // Adjust as needed

			for (int i = 32; i >= 0; i--)
			{
				float3 rayPosDetail = rayPos - rayDir * i * detailStepSize;
				float3 voxelUV = (rayPosDetail - Voxels.Offset) / Voxels.SceneSize;
				nearVoxel = voxelmap.SampleLevel(PointSampler, voxelUV, VoxelMip);
				
				if (nearVoxel.a > 0.f)
					break;
			}
		}

		// If a filled voxel is hit
		if (nearVoxel.a > 0.f)
			break;
	}

	if (nearVoxel.a == 0.f)	
		discard;

	PSOutput output;
    output.color = float4(nearVoxel.rgb + i / 1000.f, 1);
	
	float3 worldNormal = normalize(mul(pin.normal, (float3x3)WorldMatrix));
	output.normals = float4(worldNormal, 1);

	float camDistance = length(CameraPosition - pin.wp.xyz);
	output.camDistance = float4(camDistance, 0, 0, 0);

	output.motionVectors = float4((pin.previousPosition.xy / pin.previousPosition.w - pin.currentPosition.xy / pin.currentPosition.w) * ViewportSize, 0, 0);
	output.motionVectors.xy *= 0.5;
	output.motionVectors.y *= -1;

	return output;
}
