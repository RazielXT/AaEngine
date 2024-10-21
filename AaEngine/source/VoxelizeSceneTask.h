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
	TextureResource clearSceneTexture;

	CbufferView cbuffer;
	void updateCBuffer(Vector3 offset, UINT frameIndex);

	GenerateMipsComputeShader computeMips;
};
