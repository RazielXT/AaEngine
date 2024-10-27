#pragma once

#include "RenderContext.h"
#include "TextureResource.h"
#include "AaCamera.h"
#include "AaMath.h"
#include "GenerateMipsComputeShader.h"
#include <thread>
#include "ShadowMap.h"

struct RenderQueue;
class AaSceneManager;

class VoxelizeSceneTask
{
public:

	VoxelizeSceneTask(AaShadowMap& shadows);
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
	void updateCBuffer(Vector3 orthoHalfSize, Vector3 offset, UINT frameIndex);
	void updateCBuffer(bool lighting, UINT frameIndex);


	XM_ALIGNED_STRUCT(16)
	{
		Vector3 voxelOffset;
		float voxelDensity;
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
