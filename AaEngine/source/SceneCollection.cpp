#include "SceneCollection.h"
#include "GraphicsResources.h"
#include "SceneManager.h"

void SceneCollection::loadScene(const Data& data, LoadCtx ctx)
{
	for (const auto& e : data.entities)
	{
		auto material = ctx.resources.materials.getMaterial(e.materialName, ctx.batch);
		auto model = e.mesh.model;

		auto renderQueue = Order::Normal;
		if (material->IsTransparent())
			renderQueue = Order::Transparent;

		auto ent = ctx.sceneMgr.createEntity(e.name, e.tr, *model, renderQueue);
		ent->material = material;
	}
}
