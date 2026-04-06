#pragma once

#include <SimpleMath.h>
#include "VertexBufferModel.h"
#include "ModelParseOptions.h"
#include "SceneCollection.h"

class SceneManager;
class RenderSystem;
struct GraphicsResources;

namespace PrefabLoader
{
	void load(const std::string& filename, SceneCollection::LoadCtx ctx);
};
