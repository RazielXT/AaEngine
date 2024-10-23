#pragma once

#include "RenderContext.h"
#include "TextureResource.h"
#include "AaCamera.h"
#include "AaMath.h"
#include "GenerateMipsComputeShader.h"
#include <thread>

struct RenderQueue;
class AaSceneManager;

class VoxelizeSceneTask
{
public:

	VoxelizeSceneTask();
	~VoxelizeSceneTask();

	AsyncTasksInfo initialize(AaRenderSystem* renderSystem, AaSceneManager* sceneMgr, RenderTargetTexture* target);
	void prepare(RenderContext& ctx, CommandsData& syncCommands);

	CommandsData commands;
	HANDLE eventBegin{};
	HANDLE eventFinish;
	std::thread worker;

	bool running = true;

	RenderContext ctx;

	RenderQueue* sceneQueue;

	AaCamera camera;

	TextureResource voxelSceneTexture;
	TextureResource voxelPreviousSceneTexture;
	TextureResource clearSceneTexture;

	CbufferView cbuffer;
	void updateCBuffer(Vector3 offset, UINT frameIndex);
	void updateCBuffer(float tslf, float stepping, float stepping2, UINT frameIndex);

	XM_ALIGNED_STRUCT(16)
	{
		Vector3 voxelOffset;
		float voxelSize;
		Vector2 middleCone = { 1, 0.9 };
		Vector2 sideCone = { 2, 0.8 };
		float radius = 2;
		float steppingBounces = 0.07f;
		float steppingDiffuse = 0.03f;
		float lerpFactor = 0.01f;
	}
	cbufferData;

	GenerateMipsComputeShader computeMips;
};
