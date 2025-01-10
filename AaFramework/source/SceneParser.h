#pragma once

#include "SceneManager.h"
#include "PhysicsManager.h"

namespace SceneParser
{
	struct Ctx
	{
		SceneManager& sceneMgr;
		RenderSystem& renderSystem;
		GraphicsResources& resources;
		PhysicsManager& physicsMgr;
	};

	struct Result
	{
		std::vector<GrassAreaPlacementTask> grassTasks;
		std::map<MaterialInstance*, InstanceGroupDescription> instanceDescriptions;
	};

	Result load(std::string filename, Ctx ctx);
}
