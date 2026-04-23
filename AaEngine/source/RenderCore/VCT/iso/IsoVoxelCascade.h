#pragma once

#include "RenderCore/GpuTexture.h"
#include "Utils/MathUtils.h"
#include "Utils/Directx.h"
#include <string>

struct GraphicsResources;
class ClearBufferComputeShader;

struct IsoVoxelCascade
{
	GpuTexture3D voxelSceneTexture;
	GpuTexture3D voxelPreviousSceneTexture;

	std::string name;
	UINT idx{};

	void initialize(const std::string& name, ID3D12Device* device, GraphicsResources&);
	void clearAll(ID3D12GraphicsCommandList* commandList, const GpuTexture3D& clear, ClearBufferComputeShader&);

	void prepareForVoxelization(ID3D12GraphicsCommandList* commandList, TextureStatePair& voxelScene, TextureStatePair& prevVoxelScene, const GpuTexture3D& clear, ClearBufferComputeShader&);

	struct
	{
		Vector3 center = Vector3(0, 0, 0);
		float extends = 200;

		Vector3 lastCenter = center;
	}
	settings;

	Vector3 update(const Vector3& cameraPosition);

	const UINT DataElementSize = sizeof(UINT) * 2;
	const UINT DataElementCount = 128 * 128 * 128;

	struct DataBuffer
	{
		ComPtr<ID3D12Resource> data;
		ShaderViewUAV view;
	};

	DataBuffer voxelInfoBuffer;
};
