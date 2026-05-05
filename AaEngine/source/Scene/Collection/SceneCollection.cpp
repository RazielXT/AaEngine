#include "Scene/Collection/SceneCollection.h"
#include "Resources/GraphicsResources.h"
#include "Scene/SceneManager.h"

void SceneCollection::loadResource(const ResourceData& data, LoadCtx ctx)
{
	for (const auto& e : data.entities)
	{
		auto material = ctx.resources.materials.getMaterial(e.materialName, ctx.batch);
		auto model = ctx.resources.models.getModel(e.mesh.name, ctx.batch, { data.path });

		EntityCreateProperties props;
		props.groupId = 0;
		props.order = Order::Normal;

		if (material->IsTransparent())
			props.order = Order::Transparent;

		auto ent = ctx.sceneMgr.createEntity(e.tr, *model, props);
		ent->material = material;
	}
}

void SceneCollection::loadResources(const std::vector<ResourceData>& data, LoadCtx ctx)
{
	for (const auto& r : data)
	{
		loadResource(r, ctx);
	}
}
