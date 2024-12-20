#pragma once

#include "RenderSystem.h"
#include "CompositorTask.h"

class AaMaterial;
class AaShadowMap;

class FrameCompositor
{
public:

	FrameCompositor(RenderProvider provider, SceneManager& sceneMgr, AaShadowMap& shadows);
	~FrameCompositor();

	void load(std::string path);
	void reloadPasses();
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
		std::vector<CompositorPassInfo*> pass;
	};
	std::vector<TasksGroup> tasks;

	void initializeCommands();
	void initializeTextureStates();
	std::map<std::string, D3D12_RESOURCE_STATES> lastTextureStates;

	void executeCommands();

	void renderQuad(PassData& pass, RenderContext& ctx, ID3D12GraphicsCommandList* commandList);

	RenderProvider provider;
	SceneManager& sceneMgr;
	AaShadowMap& shadowMaps;
};
