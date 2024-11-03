#pragma once
#include "AaRenderSystem.h"
#include "CompositorFileParser.h"
#include "ShadowsRenderTask.h"
#include "SceneRenderTask.h"
#include "VoxelizeSceneTask.h"
#include "DebugOverlayTask.h"
#include <set>
#include "SceneTestTask.h"

class AaMaterial;

class FrameCompositor
{
public:

	FrameCompositor(RenderProvider provider, AaSceneManager* sceneMgr, AaShadowMap* shadows);
	~FrameCompositor();

	void load(std::string path);
	void reloadTextures();

	void render(RenderContext& ctx);

protected:

	void load(CompositorInfo info);

	CompositorInfo info;
	std::map<std::string, RenderTargetTexture> textures;
	RenderTargetHeap rtvHeap;

	ShadowsRenderTask shadowRender;
	SceneRenderTask sceneRender;
	VoxelizeSceneTask sceneVoxelize;
	DebugOverlayTask debugOverlay;
	SceneTestTask testTask;

	struct PassData
	{
		CompositorPass& info;
		std::vector<ShaderTextureView*> inputs;
		AaMaterial* material = nullptr;
		RenderTargetTexture* target = nullptr;
		bool present = false;

		CommandsData generalCommands;
		bool startCommands = false;

	};
	std::vector<PassData> passes;

	std::vector<CommandsData> generalCommandsArray;

	struct TasksGroup
	{
		std::vector<HANDLE> finishEvents;
		std::vector<CommandsData> data;
	};
	std::vector<TasksGroup> tasks;

	void initializeCommands();
	void executeCommands();

	RenderProvider provider;
	AaSceneManager* sceneMgr;
};
