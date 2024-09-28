#pragma once

#include "AaRenderSystem.h"
#include "MaterialInfo.h"

class AaCamera;

struct RenderContext
{
	AaCamera* camera;
	AaRenderSystem* renderSystem;
	RenderTargetTexture* target;

	FrameGpuParameters params;
};

struct AsyncTaskInfo
{
	HANDLE finishEvent;
	CommandsData commands;
};

using AsyncTasksInfo = std::vector<AsyncTaskInfo>;
