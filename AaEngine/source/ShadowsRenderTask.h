#pragma once

#include "MaterialInfo.h"
#include "RenderContext.h"
#include <thread>

struct RenderQueue;
class AaShadowMap;

class ShadowsRenderTask
{
public:

	ShadowsRenderTask(AaShadowMap& shadows);
	~ShadowsRenderTask();

	AsyncTasksInfo initialize(AaRenderSystem* renderSystem, RenderQueue* queue);
	void prepare(RenderContext& ctx, CommandsData& syncCommands);

	struct 
	{
		CommandsData commands;
		HANDLE eventBegin;
		HANDLE eventFinish;
		std::thread worker;
	}
	shadowsData[2];

	bool running = true;

	RenderContext ctx;

	RenderQueue* depthQueue;

	AaShadowMap& shadowMaps;
};
