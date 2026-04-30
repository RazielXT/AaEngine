#pragma once

#include <SimpleMath.h>
#include "Resources/Model/VertexBufferModel.h"
#include "Resources/Model/ModelParseOptions.h"
#include "Scene/Collection/SceneCollection.h"

class SceneManager;
class RenderSystem;
struct GraphicsResources;

namespace GltfLoader
{
	SceneCollection::ResourceData load(const std::string& path, SceneCollection::LoadCtx ctx);
};
