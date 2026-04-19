#include "Scene/SceneCollection.h"
#include "Resources/GraphicsResources.h"
#include "Scene/SceneManager.h"

void SceneCollection::loadScene(const Data& data, LoadCtx ctx)
{
	auto groupId = ctx.sceneMgr.createEntityGroup(data.name);

	for (const auto& e : data.entities)
	{
		auto material = ctx.resources.materials.getMaterial(e.materialName, ctx.batch);
		auto model = ctx.resources.models.getModel(e.mesh.name, ctx.batch, { data.path });

		EntityCreateProperties props;
		props.groupId = groupId;
		props.order = Order::Normal;

		if (material->IsTransparent())
			props.order = Order::Transparent;

		auto ent = ctx.sceneMgr.createEntity(e.name, e.tr, *model, props);
		ent->material = material;
	}
}
