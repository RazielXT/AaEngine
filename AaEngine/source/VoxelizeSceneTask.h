#pragma once

#include "CompositorTask.h"
#include "TextureResource.h"
#include "Camera.h"
#include "MathUtils.h"
#include "GenerateMipsComputeShader.h"
#include <thread>
#include "ShadowMap.h"

struct RenderQueue;
class SceneManager;

struct SceneVoxelsChunk
{
	TextureResource voxelSceneTexture;
	TextureResource voxelPreviousSceneTexture;
	std::string name;
	UINT idx{};

	void initialize(const std::string& name, ID3D12Device* device, GraphicsResources&);
	void clear(ID3D12GraphicsCommandList* commandList, const TextureResource& clear);

	void prepareForVoxelization(ID3D12GraphicsCommandList* commandList);
	void prepareForReading(ID3D12GraphicsCommandList* commandList);

	struct
	{
		Vector3 center = Vector3(0, 0, 0);
		float extends = 200;

		Vector3 lastCenter = center;
	}
	settings;

	Vector3 update(const Vector3& cameraPosition);
};

class VoxelizeSceneTask : public CompositorTask
{
public:

	VoxelizeSceneTask(RenderProvider provider, SceneManager&, AaShadowMap& shadows);
	~VoxelizeSceneTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

	static VoxelizeSceneTask& Get();

	void reset();

private:

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
		float padding;
	};

	XM_ALIGNED_STRUCT(16) SceneVoxelCbuffer
	{
		Vector2 middleConeRatioDistance = { 1, 0.9f };
		Vector2 sideConeRatioDistance = { 2, 0.8f };
		float steppingBounces = 0.07f;
		float steppingDiffuse = 0.03f;
		float voxelizeLighting = 0.0f;
		float padding;

		SceneVoxelChunkInfo nearVoxels;
		SceneVoxelChunkInfo farVoxels;
	}
	cbufferData;

	CbufferView cbuffer;
	void updateCBufferChunk(SceneVoxelChunkInfo& info, Vector3 diff, SceneVoxelsChunk& chunk);
	void updateCBuffer(Vector3 diff, Vector3 diffFar, UINT frameIndex);

	void voxelizeChunk(CommandsData& commands, CommandsMarker& marker, PassTarget& viewportOutput, SceneVoxelsChunk& chunk);

	GenerateMipsComputeShader computeMips;

	AaShadowMap& shadowMaps;

	TextureResource clearSceneTexture;

	SceneVoxelsChunk nearVoxels;
	SceneVoxelsChunk farVoxels;
};
