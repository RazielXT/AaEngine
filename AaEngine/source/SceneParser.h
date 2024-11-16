#pragma once

#include "SceneManager.h"

struct SceneParseResult
{
	std::vector<GrassAreaPlacementTask> grassTasks;
	std::map<MaterialInstance*, InstanceGroupDescription> instanceDescriptions;
};

namespace SceneParser
{
	SceneParseResult load(std::string filename, SceneManager* mSceneMgr, AaRenderSystem* renderSystem);
}
