#pragma once

#include <SimpleMath.h>
#include "Resources/Model/VertexBufferModel.h"
#include "Resources/Model/ModelParseOptions.h"
#include "Scene/SceneCollection.h"

class SceneManager;
class RenderSystem;
struct GraphicsResources;

namespace PrefabLoader
{
	void load(const std::string& filename, SceneCollection::LoadCtx ctx);
};
