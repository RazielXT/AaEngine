#include "grassCommon.hlsl"

RWStructuredBuffer<float4x4> transformBuffer : register(u0);
RWByteAddressBuffer drawCommandsBuffer : register(u1);

StructuredBuffer<GrassInfo> infoBuffer : register(t0);
ByteAddressBuffer infoCounterBuffer : register(t1);

float4x4 RotationMatrix(float yaw)
{
	float cosYaw = cos(yaw);
	float sinYaw = sin(yaw);
	return float4x4(
		cosYaw, 0, sinYaw, 0,
		0,      1, 0,      0,
		-sinYaw, 0, cosYaw, 0,
		0,      0, 0,      1
	);
}

float4x4 TranslationScaleMatrix(float3 position, float scale)
{
	return float4x4(
		scale, 0, 0, position.x,
		0, scale, 0, position.y,
		0, 0, scale, position.z,
		0, 0, 0, 1
	);
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	const uint ItemsPerThread = 64;
	uint totalCount = infoCounterBuffer.Load(0);

	uint start = dispatchThreadID.x * ItemsPerThread;

	for (uint x = start; x < min(totalCount, start + ItemsPerThread); x++)
	{
		GrassInfo info = infoBuffer[x];

		float4x4 rotationMatrix = RotationMatrix(info.rotation);
		float4x4 translationScaleMatrix = TranslationScaleMatrix(info.position, info.scale);

		uint index;
		drawCommandsBuffer.InterlockedAdd(4, 1, index);
		transformBuffer[x] = transpose(mul(translationScaleMatrix, rotationMatrix));
	}
}
