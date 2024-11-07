#pragma once

#include "AaRenderSystem.h"
#include "CompositorTask.h"

class AaMaterial;
class AaShadowMap;

class FrameCompositor
{
public:

	FrameCompositor(RenderProvider provider, AaSceneManager& sceneMgr, AaShadowMap& shadows);
	~FrameCompositor();

	void load(std::string path);
	void reloadTextures();

	void render(RenderContext& ctx);

protected:

	void load(CompositorInfo info);

	CompositorInfo info;
	std::map<std::string, RenderTargetTexture> textures;
	RenderTargetHeap rtvHeap;

	struct PassData : public CompositorPass
	{
		PassData(CompositorPassInfo& i) : CompositorPass{ i } {}

		AaMaterial* material{};
		std::unique_ptr<CompositorTask> task;

		CommandsData generalCommands;
		bool startCommands = false;
	};
	std::vector<PassData> passes;

	std::vector<CommandsData> generalCommandsArray;

	struct TasksGroup
	{
		std::vector<HANDLE> finishEvents;
		std::vector<CommandsData> data;
		std::vector<std::string> pass;
	};
	std::vector<TasksGroup> tasks;

	void initializeCommands();
	void initializeTextureStates();
	D3D12_RESOURCE_STATES getInitialTextureState(const std::string& name) const;

	void executeCommands();

	RenderProvider provider;
	AaSceneManager& sceneMgr;
	AaShadowMap& shadowMaps;
};
