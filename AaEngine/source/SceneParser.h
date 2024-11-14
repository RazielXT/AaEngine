#pragma once

#include "AaSceneManager.h"

struct SceneParseResult
{
	std::vector<GrassAreaPlacementTask> grassTask;
	std::map<MaterialInstance*, InstanceGroupDescription> instanceDescriptions;
};

namespace SceneParser
{
	SceneParseResult load(std::string filename, AaSceneManager* mSceneMgr, AaRenderSystem* renderSystem);
}
