#pragma once

#include "RenderCore/RenderSystem.h"
#include "Resources/Shader/ShaderResources.h"
#include "Resources/GraphicsResources.h"

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
	Camera* camera{};
};

struct AsyncTaskInfo
{
	HANDLE finishEvent;
	CommandsData commands;
};

using AsyncTasksInfo = std::vector<AsyncTaskInfo>;
