#pragma once

#include "RenderSystem.h"
#include "CompositorTask.h"

class AssignedMaterial;
class ShadowMaps;

class FrameCompositor
{
public:

	struct InitConfig
	{
		std::string file = "frame";
		bool renderToBackbuffer = true;
		DXGI_FORMAT outputFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	};

	FrameCompositor(const InitConfig& params, RenderProvider provider, SceneManager& sceneMgr, ShadowMaps& shadows);
	~FrameCompositor();

	void reloadPasses();
	void reloadTextures();

	void render(RenderContext& ctx);

	const RenderTargetTexture* getTexture(const std::string& name) const;

	using CreateTaskFunc = std::function<std::shared_ptr<CompositorTask>(RenderProvider&, SceneManager&)>;
	void registerTask(const std::string& name, CreateTaskFunc);

	CompositorTask* getTask(const std::string& name);

protected:

	InitConfig config;

	CompositorInfo info;
	std::map<std::string, RenderTargetTexture> textures;

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
	ShadowMaps& shadowMaps;

	std::map<std::string, CreateTaskFunc> externTasks;
};
