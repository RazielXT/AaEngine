#pragma once

#include "AaRenderSystem.h"
#include "ShaderResources.h"

class AaCamera;
class Renderables;

struct RenderProvider
{
	FrameGpuParameters& params;
	AaRenderSystem* renderSystem;
};

struct RenderContext
{
	AaCamera* camera;
};

struct AsyncTaskInfo
{
	HANDLE finishEvent;
	CommandsData commands;
};

using AsyncTasksInfo = std::vector<AsyncTaskInfo>;
