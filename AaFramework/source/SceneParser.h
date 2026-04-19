#pragma once

#include "Scene/SceneManager.h"
#include "PhysicsManager.h"
#include <filesystem>

namespace SceneParser
{
	struct Ctx
	{
		ResourceUploadBatch& batch;
		SceneManager& sceneMgr;
		RenderSystem& renderSystem;
		GraphicsResources& resources;
		PhysicsManager& physicsMgr;
		ObjectTransformation placement = {};
	};

	struct Result
	{
		std::vector<GrassAreaPlacementTask> grassTasks;
		std::map<MaterialInstance*, InstanceGroupDescription> instanceDescriptions;
	};

	Result load(std::filesystem::path, Ctx ctx);
}
