#pragma once

#include "Scene/Collection/SceneCollection.h"

namespace GltfLoader
{
	SceneCollection::ResourceData load(const std::string& path, SceneCollection::LoadCtx ctx);
};
