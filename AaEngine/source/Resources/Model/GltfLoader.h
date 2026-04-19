#pragma once

#include <SimpleMath.h>
#include "Resources/Model/VertexBufferModel.h"
#include "Resources/Model/ModelParseOptions.h"
#include "Scene/SceneCollection.h"

class SceneManager;
class RenderSystem;
struct GraphicsResources;

namespace GltfLoader
{
	SceneCollection::Data load(const std::string& path, SceneCollection::LoadCtx ctx);
};
