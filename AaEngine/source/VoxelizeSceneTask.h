#pragma once

#include "CompositorTask.h"
#include "TextureResource.h"
#include "AaCamera.h"
#include "AaMath.h"
#include "GenerateMipsComputeShader.h"
#include <thread>
#include "ShadowMap.h"

struct RenderQueue;
class SceneManager;

struct VoxelSceneSettings
{
	Vector3 center = Vector3(0, 0, 0);
	Vector3 extends = Vector3(150, 150, 150);

	static VoxelSceneSettings& get();
};

class VoxelizeSceneTask : public CompositorTask
{
public:

	VoxelizeSceneTask(RenderProvider provider, SceneManager&, AaShadowMap& shadows);
	~VoxelizeSceneTask();

	AsyncTasksInfo initialize(CompositorPass& pass) override;
	void run(RenderContext& ctx, CommandsData& syncCommands, CompositorPass& pass) override;

private:

	CommandsData commands;
	HANDLE eventBegin{};
	HANDLE eventFinish;
	std::thread worker;

	bool running = true;

	RenderQueue* sceneQueue{};

	AaCamera camera;

	TextureResource voxelSceneTexture;
	TextureResource voxelPreviousSceneTexture;
	TextureResource clearSceneTexture;

	CbufferView cbuffer;
	void updateCBuffer(Vector3 orthoHalfSize, Vector3 offset, UINT frameIndex);
	void updateCBuffer(bool lighting, UINT frameIndex);

	XM_ALIGNED_STRUCT(16)
	{
		Vector3 voxelOffset;
		float voxelDensity;
		Vector3 voxelSceneSize;
		float padding;
		Vector2 middleConeRatioDistance = { 1, 0.9 };
		Vector2 sideConeRatioDistance = { 2, 0.8 };
		float lerpFactor = 0.01f;
		float steppingBounces = 0.07f;
		float steppingDiffuse = 0.03f;
		float voxelizeLighting = 0.0f;
	}
	cbufferData;

	GenerateMipsComputeShader computeMips;

	AaShadowMap& shadowMaps;
};
