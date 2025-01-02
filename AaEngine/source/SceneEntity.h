#pragma once

#include "VertexBufferModel.h"
#include "Material.h"
#include "RenderObject.h"
#include <vector>
#include "EntityGeometry.h"

struct InstanceGroup;

class SceneEntity : public RenderObject
{
public:

	SceneEntity(RenderObjectsStorage&, std::string_view name);
	SceneEntity(RenderObjectsStorage&, SceneEntity& source);
	~SceneEntity();

	const char* name;

	MaterialInstance* material{};

	EntityGeometry geometry;
};
