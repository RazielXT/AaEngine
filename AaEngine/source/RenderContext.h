#pragma once

#include "AaRenderSystem.h"
#include "MaterialInfo.h"

class AaCamera;
class Renderables;

struct RenderContext
{
	AaCamera* camera;
	AaRenderSystem* renderSystem;
	Renderables* renderables;
	RenderTargetTexture* target;

	FrameGpuParameters params;
};

struct AsyncTaskInfo
{
	HANDLE finishEvent;
	CommandsData commands;
};

using AsyncTasksInfo = std::vector<AsyncTaskInfo>;
