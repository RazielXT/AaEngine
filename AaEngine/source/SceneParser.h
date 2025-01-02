#pragma once

#include "SceneManager.h"

namespace SceneParser
{
	struct Ctx
	{
		SceneManager& sceneMgr;
		RenderSystem& renderSystem;
		GraphicsResources& resources;
	};

	struct Result
	{
		std::vector<GrassAreaPlacementTask> grassTasks;
		std::map<MaterialInstance*, InstanceGroupDescription> instanceDescriptions;
	};

	Result load(std::string filename, Ctx ctx);
}
