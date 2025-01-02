#pragma once

#include "RenderSystem.h"
#include "ShaderResources.h"
#include "GraphicsResources.h"

class Camera;
class RenderObjectsStorage;

struct RenderProvider
{
	const FrameParameters& params;
	RenderSystem& renderSystem;
	GraphicsResources& resources;
};

struct RenderContext
{
	Camera* camera;
};

struct AsyncTaskInfo
{
	HANDLE finishEvent;
	CommandsData commands;
};

using AsyncTasksInfo = std::vector<AsyncTaskInfo>;
