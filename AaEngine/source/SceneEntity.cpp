#include "SceneEntity.h"
#include "ModelResources.h"
#include "MaterialResources.h"

SceneEntity::SceneEntity(RenderObjectsStorage& r, std::string_view n) : RenderObject(r), name(n.data())
{
}

SceneEntity::SceneEntity(RenderObjectsStorage& r, SceneEntity& source) : RenderObject(r)
{
	name = source.name;
	material = source.material;
	geometry = source.geometry;

	setTransformation(source.getTransformation(), true);
}

SceneEntity::~SceneEntity()
{
}
