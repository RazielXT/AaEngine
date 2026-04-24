#include "AnisoVoxelConeTracingCommon.hlsl"
#include "hlsl/common/ResourceAccess.hlsl"

float4x4 ViewProjectionMatrix;
float4x4 WorldMatrix;
float4x4 PreviousWorldMatrix;
float3 CameraPosition;
uint2 ViewportSize;
uint VoxelIndex;
uint VoxelMip;
uint ResId;

cbuffer SceneVoxelInfo : register(b1)
{
	AnisoSceneVoxelCbufferIndexed VoxelInfo;
};

struct VS_Input
{
	float4 p : POSITION;
	float2 uv : TEXCOORD0;
};

struct PS_Input
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 wp : TEXCOORD1;
};

PS_Input VSMain(VS_Input vin)
{
	PS_Input vsOut;

	vsOut.wp = mul(vin.p, WorldMatrix);
	vsOut.pos = mul(vsOut.wp, ViewProjectionMatrix);
	vsOut.uv = vin.uv;

	return vsOut;
}

struct PSOutput
{
	float4 color : SV_Target0;
};

SamplerState PointSampler : register(s0);

PSOutput PSMain(PS_Input pin)
{
	AnisoSceneVoxelChunkInfo Voxels = VoxelInfo.Voxels[VoxelIndex];

	// Resolve 6 face textures
	Texture3D facePosX = GetTexture3D(GetFaceTexId(Voxels, 0));
	Texture3D faceNegX = GetTexture3D(GetFaceTexId(Voxels, 1));
	Texture3D facePosY = GetTexture3D(GetFaceTexId(Voxels, 2));
	Texture3D faceNegY = GetTexture3D(GetFaceTexId(Voxels, 3));
	Texture3D facePosZ = GetTexture3D(GetFaceTexId(Voxels, 4));
	Texture3D faceNegZ = GetTexture3D(GetFaceTexId(Voxels, 5));

	// we presume camera is inside voxel, otherwise start pos needs to move inside first
	float3 rayStart = pin.wp.xyz;
	float3 rayDir = normalize(pin.wp.xyz - CameraPosition);
	if (rayDir.x == 0) rayDir.x = 1e-6;
	if (rayDir.y == 0) rayDir.y = 1e-6;
	if (rayDir.z == 0) rayDir.z = 1e-6;

	// Compute voxel size in world units
	const float3 voxelSize = pow(2,VoxelMip) / Voxels.Density;
	const float3 VoxelDimensions = Voxels.Density * Voxels.WorldSize;
	const float3 VoxelDimensionsInv = 1 / VoxelDimensions;

	// Compute initial voxel coordinate
	int3 voxelCoord = int3(floor((rayStart - Voxels.Offset) / voxelSize));

	// Compute reciprocal direction
	float3 invDir = 1.0 / rayDir;

	// Compute tMax: distance to next voxel boundary along each axis
	float3 stepSign = float3(
		(rayDir.x > 0.0f) ? 1.0f : 0.0f,
		(rayDir.y > 0.0f) ? 1.0f : 0.0f,
		(rayDir.z > 0.0f) ? 1.0f : 0.0f
	);
	float3 nextBoundary = (float3(voxelCoord) + stepSign) * voxelSize + Voxels.Offset;

	float3 tMax = (nextBoundary - rayStart) * invDir;

	// Compute tDelta: distance to cross one voxel along each axis
	float3 tDelta = voxelSize * abs(invDir);

	float4 nearVoxel = 0.xxxx;
	uint hitAxis = 2; // track which axis the DDA stepped on last (0=X, 1=Y, 2=Z)

	int i = 0;
	for (; i < 256; i++)
	{
		// Pick axis with smallest tMax
		if (tMax.x < tMax.y && tMax.x < tMax.z)
		{
			voxelCoord.x += (rayDir.x > 0 ? 1 : -1);
			tMax.x += tDelta.x;
			hitAxis = 0;
		}
		else if (tMax.y < tMax.z)
		{
			voxelCoord.y += (rayDir.y > 0 ? 1 : -1);
			tMax.y += tDelta.y;
			hitAxis = 1;
		}
		else
		{
			voxelCoord.z += (rayDir.z > 0 ? 1 : -1);
			tMax.z += tDelta.z;
			hitAxis = 2;
		}

		// Stop if outside grid
		if (voxelCoord.x < 0 || voxelCoord.x >= VoxelDimensions.x ||
			voxelCoord.y < 0 || voxelCoord.y >= VoxelDimensions.y ||
			voxelCoord.z < 0 || voxelCoord.z >= VoxelDimensions.z)
			break;

		// Sample the face texture matching the ray entry direction
		// When ray enters from +X side (rayDir.x < 0), we see Face +X
		Texture3D faceMap;
		if (hitAxis == 0)
			faceMap = (rayDir.x > 0) ? faceNegX : facePosX;
		else if (hitAxis == 1)
			faceMap = (rayDir.y > 0) ? faceNegY : facePosY;
		else
			faceMap = (rayDir.z > 0) ? faceNegZ : facePosZ;

		nearVoxel = faceMap.Load(int4(voxelCoord, VoxelMip));

		// If a filled voxel is hit
		if (nearVoxel.a > 0.f)
			break;
	}

	if (nearVoxel.a == 0.f)
		discard;

	PSOutput output;
	output.color = float4(nearVoxel.rgb, 1);

	return output;
}
