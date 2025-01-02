#pragma once

#include "RenderSystem.h"
#include "CompositorTask.h"

class AssignedMaterial;
class AaShadowMap;

class FrameCompositor
{
public:

	FrameCompositor(RenderProvider provider, SceneManager& sceneMgr, AaShadowMap& shadows);
	~FrameCompositor();

	struct InitParams
	{
		bool renderToBackbuffer = true;
		DXGI_FORMAT outputFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	};
	void load(std::string path, const InitParams& params = InitParams{});
	void reloadPasses();
	void reloadTextures();

	void render(RenderContext& ctx);

	const RenderTargetTexture* getTexture(const std::string& name) const;

protected:

	void load(CompositorInfo info);

	CompositorInfo info;
	std::map<std::string, RenderTargetTexture> textures;
	bool renderToBackbuffer{};

	RenderTargetHeap rtvHeap;

	struct PassData : public CompositorPass
	{
		PassData(CompositorPassInfo& i) : CompositorPass{ i } {}

		AssignedMaterial* material{};
		std::shared_ptr<CompositorTask> task;

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
