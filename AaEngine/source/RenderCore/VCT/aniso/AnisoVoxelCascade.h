#pragma once

#include "RenderCore/GpuTexture.h"
#include "Utils/MathUtils.h"
#include "Utils/Directx.h"
#include <string>

struct GraphicsResources;
class ClearBufferComputeShader;

struct AnisoVoxelCascade
{
	static constexpr UINT FaceCount = 6;

	// 6 directional textures: +X, -X, +Y, -Y, +Z, -Z
	GpuTexture3D voxelFaceTextures[FaceCount];
	GpuTexture3D voxelPreviousFaceTextures[FaceCount];

	std::string name;
	UINT idx{};

	void initialize(const std::string& name, ID3D12Device* device, GraphicsResources&);
	void clearAll(ID3D12GraphicsCommandList* commandList, const GpuTexture3D& clear, ClearBufferComputeShader&);

	void prepareForVoxelization(ID3D12GraphicsCommandList* commandList, TextureStatePair faceStates[FaceCount], TextureStatePair prevFaceStates[FaceCount], const GpuTexture3D& clear, ClearBufferComputeShader&);

	struct
	{
		Vector3 center = Vector3(0, 0, 0);
		float extends = 200;

		Vector3 lastCenter = center;
	}
	settings;

	Vector3 update(const Vector3& cameraPosition);

	static constexpr UINT VoxelSize = 128;
	static constexpr DXGI_FORMAT VoxelFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	struct VoxelSceneData
	{
		UINT Diffuse;
		UINT Normal;
	};
	const UINT DataElementSize = sizeof(VoxelSceneData);
	const UINT DataElementCount = VoxelSize * VoxelSize * VoxelSize;

	struct DataBuffer
	{
		ComPtr<ID3D12Resource> data;
		ShaderViewUAV view;
	};

	DataBuffer voxelInfoBuffer;
};
