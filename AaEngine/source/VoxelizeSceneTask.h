#pragma once

#include "CompositorTask.h"
#include "TextureResource.h"
#include "Camera.h"
#include "MathUtils.h"
#include "GenerateMipsComputeShader.h"
#include <thread>
#include "ShadowMaps.h"
#include "ClearBufferCS.h"
#include <span>

struct RenderQueue;
class SceneManager;

struct SceneVoxelsCascade
{
	TextureResource voxelSceneTexture;
	TextureResource voxelPreviousSceneTexture;
	std::string name;
	UINT idx{};

	void initialize(const std::string& name, ID3D12Device* device, GraphicsResources&);
	void clearAll(ID3D12GraphicsCommandList* commandList, const TextureResource& clear, ClearBufferComputeShader&);

	void prepareForVoxelization(ID3D12GraphicsCommandList* commandList, const TextureResource& clear, ClearBufferComputeShader&);
	void prepareForReading(ID3D12GraphicsCommandList* commandList);

	struct
	{
		Vector3 center = Vector3(0, 0, 0);
		float extends = 200;

		Vector3 lastCenter = center;
	}
	settings;

	Vector3 update(const Vector3& cameraPosition);

	const UINT DataElementSize = sizeof(float) * 8;
	const UINT DataElementCount = 128 * 128 * 128;

	ComPtr<ID3D12Resource> dataBuffer;
	UINT dataBufferHeapIdx{};
	D3D12_GPU_DESCRIPTOR_HANDLE dataBufferUavHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE dataBufferUavCpuHandle;
};

class BounceVoxelsCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, const std::span<float>& data, const ShaderTextureView&, const ShaderTextureView&, D3D12_GPU_VIRTUAL_ADDRESS);
};

class VoxelizeSceneTask : public CompositorTask
{
public:

	VoxelizeSceneTask(RenderProvider provider, SceneManager&, ShadowMaps& shadows);
	~VoxelizeSceneTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	static VoxelizeSceneTask& Get();

	void revoxelize();
	void clear();
	void clear(ID3D12GraphicsCommandList* c);

	void showVoxelsInfo(bool show);

private:

	static constexpr UINT CascadesCount = 4;

	CommandsData commands;
	HANDLE eventBegin{};
	HANDLE eventFinish;
	std::thread worker;

	bool running = true;

	RenderQueue* sceneQueue{};

	RenderContext ctx{};

	XM_ALIGNED_STRUCT(16) SceneVoxelChunkInfo
	{
		Vector3 voxelOffset;
		float voxelDensity;
		Vector3 diff;
		float voxelSceneSize;
		float lerpFactor = 0.01f;
		UINT texId;
		UINT texIdBounces;
		UINT resIdDataBuffer;
	};

	XM_ALIGNED_STRUCT(16) SceneVoxelCbuffer
	{
		Vector2 middleConeRatioDistance = { 1, 0.9f };
		Vector2 sideConeRatioDistance = { 2, 0.8f };
		float steppingBounces = 0.07f;
		float steppingDiffuse = 0.03f;
		float voxelizeLighting = 0.0f;
		float padding;

		SceneVoxelChunkInfo cascadeInfo[CascadesCount];
	}
	cbufferData;

	CbufferView cbuffer;
	CbufferView frameCbuffer;
	void updateCBufferCascade(SceneVoxelChunkInfo& info, Vector3 diff, SceneVoxelsCascade& chunk);
	void updateCBuffer(UINT frameIndex);

	void voxelizeCascade(CommandsData& commands, CommandsMarker& marker, PassTarget& viewportOutput, SceneVoxelsCascade& chunk);
	void bounceCascade(CommandsData& commands, CommandsMarker& marker, PassTarget& viewportOutput, SceneVoxelsCascade& chunk);

	UINT buildCounter[CascadesCount];
	bool reset = false;

	BounceVoxelsCS bouncesCS;
	GenerateMipsComputeShader computeMips;
	ClearBufferComputeShader clearBufferCS;

	ShadowMaps& shadowMaps;

	TextureResource clearSceneTexture;

	SceneVoxelsCascade voxelCascades[CascadesCount];

	bool showVoxelsEnabled = false;
	void showVoxelsUpdate(Camera& camera);
	SceneEntity* showVoxels();
	void hideVoxels();
};
