#pragma once

#include "RenderSystem.h"
#include "ShaderResources.h"

class AaCamera;
class RenderObjectsStorage;

struct RenderProvider
{
	const FrameParameters& params;
	RenderSystem* renderSystem;
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
