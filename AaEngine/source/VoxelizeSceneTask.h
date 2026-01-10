#pragma once

#include "CompositorTask.h"
#include "GpuTexture.h"
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

class BounceVoxelsCS : public ComputeShader
{
public:

	void dispatch(ID3D12GraphicsCommandList* commandList, const std::span<float>& data, const ShaderTextureView&, const ShaderTextureView&, D3D12_GPU_VIRTUAL_ADDRESS);
};

struct VoxelTracingParams
{
	float voxelSteppingBounces = 0.075f;
	float voxelSteppingDiffuse = 0.0f;
	Vector2 middleConeRatioDistance = { 1.05f, 1.5f };
	Vector2 sideConeRatioDistance = { 2.2f, 5.f };
};

class VoxelizeSceneTask : public CompositorTask
{
public:

	VoxelizeSceneTask(RenderProvider provider, SceneManager&, ShadowMaps& shadows);
	~VoxelizeSceneTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;

	void run(RenderContext& ctx, CompositorPass& pass) override;

	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;
	bool writesSyncCommands(CompositorPass&) const override;

	void runCompute(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;
	bool writesSyncComputeCommands(CompositorPass&) const override;

	static VoxelizeSceneTask& Get();

	void revoxelize();
	void clear();
	void clear(ID3D12GraphicsCommandList* c);

	VoxelTracingParams params;

private:

	static constexpr UINT CascadesCount = 4;

	CommandsData voxelizeCommands;
	HANDLE eventBegin{};
	HANDLE eventFinish{};
	std::thread worker;

	bool running = true;

	RenderQueue* sceneQueue{};

	RenderContext ctx{};

	XM_ALIGNED_STRUCT(16) SceneVoxelChunkInfo
	{
		Vector3 voxelOffset;
		float voxelDensity;
		Vector3 moveOffset;
		float voxelSceneSize;
		UINT texId;
		UINT texIdPrev;
		UINT resIdDataBuffer;
	};

	XM_ALIGNED_STRUCT(16) SceneVoxelCbuffer
	{
		Vector2 middleConeRatioDistance = { 1, 0.9f };
		Vector2 sideConeRatioDistance = { 2, 0.8f };

		SceneVoxelChunkInfo cascadeInfo[CascadesCount];
	}
	cbufferData;

	CbufferView cbuffer;
	CbufferView frameCbuffer;
	void updateCBufferCascade(SceneVoxelChunkInfo& info, Vector3 diff, SceneVoxelsCascade& chunk);
	void updateCBuffer(UINT frameIndex);

	void voxelizeCascades(GpuTextureStates& viewportOutput);
	void voxelizeCascade(TextureStatePair& voxelScene, TextureStatePair& prevVoxelScene, GpuTextureStates& viewportOutput, SceneVoxelsCascade& chunk);

	void bounceCascade(CommandsData& commands, SceneVoxelsCascade& chunk);

	UINT buildCounter[CascadesCount]{};
	bool reset = false;

	BounceVoxelsCS bouncesCS;
	Generate3DMips3xCS computeMips;
	ClearBufferComputeShader clearBufferCS;

	ShadowMaps& shadowMaps;

	GpuTexture3D clearSceneTexture;

	SceneVoxelsCascade voxelCascades[CascadesCount];
};
