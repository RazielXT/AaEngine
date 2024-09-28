#pragma once
#include "AaRenderSystem.h"
#include "CompositorFileParser.h"
#include <thread>
#include <optional>

class AaSceneManager;
class AaMaterial;
class AaCamera;
class AaShadowMap;
struct FrameGpuParameters;
namespace imgui
{
	class DebugWindow;
}

class Compositor
{
public:

	Compositor(AaRenderSystem* rs);

	void load(std::string path);
	void reload();

	void render(FrameGpuParameters& params, AaSceneManager* sceneMgr, AaCamera& camera, AaShadowMap& shadowMap, imgui::DebugWindow& gui);

protected:

	void load(CompositorInfo info);

	CompositorInfo info;
	AaRenderSystem* renderSystem;
	std::map<std::string, RenderTargetTexture> textures;

	struct PassData
	{
		CompositorPass& info;
		std::vector<RenderTargetTexture*> inputs;
		AaMaterial* material = nullptr;
		RenderTargetTexture* target = nullptr;
		bool present = false;
	};
	std::vector<PassData> passes;

	void initializeRenderTasks();

	struct RenderTask
	{
		RenderTask(int count, AaRenderSystem* rs);

		std::vector<AaRenderSystem::CommandsData> commands;
		std::vector<HANDLE> workerBegin;
		std::vector<HANDLE> workerFinish;
	};
	struct PassCommands
	{
		uint32_t generalCommandsIdx{};
		std::vector<RenderTask> tasks;
	};
	std::vector<AaRenderSystem::CommandsData> generalCommandsData;
	std::vector<PassCommands> passCommands;
	std::vector<std::thread> workers;

	void prepareEarlyZCommandList(AaRenderSystem::CommandsData& commands, RenderTargetTexture*);
	void prepareSceneCommandList(AaRenderSystem::CommandsData& commands, RenderTargetTexture*);
	void prepareShadowsCommandList(AaRenderSystem::CommandsData& commands);

	bool running = true;
};
