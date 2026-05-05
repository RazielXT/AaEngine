#pragma once

#include "Scene/RenderWorld.h"
#include "PhysicsManager.h"
#include <filesystem>

namespace SceneParser
{
	struct Ctx
	{
		ResourceUploadBatch& batch;
		RenderWorld& renderWorld;
		RenderSystem& renderSystem;
		GraphicsResources& resources;
		PhysicsManager& physicsMgr;
		ObjectTransformation placement = {};
	};

	struct Result
	{
		std::map<MaterialInstance*, InstanceGroupDescription> instanceDescriptions;
	};

	Result load(std::filesystem::path, Ctx ctx);
}
