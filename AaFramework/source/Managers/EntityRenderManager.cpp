#include "EntityRenderManager.h"
#include "Scene/Collection/SceneCollection.h"
#include "Scene/RenderWorld.h"
#include "Resources/Model/ModelResources.h"

EntityRenderManager::EntityRenderManager(Scene& s) : scene(s)
{
	scene.Events<EntityRenderInfoComponent>().onAdded =
		[&](Entity e, EntityRenderInfoComponent& c)
		{
		};
	scene.Events<EntityRenderInfoComponent>().onRemoved =
		[&](Entity e, EntityRenderInfoComponent& c)
		{
		};
}

void EntityRenderManager::create(Entity, const SceneCollection::Entity& desc, SceneCollection::LoadCtx ctx)
{
// 	auto material = ctx.resources.materials.getMaterial(desc.materialName, ctx.batch);
// 	auto model = ctx.resources.models.getModel(desc.mesh.name, ctx.batch, { data.path });
// 
// 	EntityCreateProperties props;
// 	props.groupId = 0;
// 	props.order = Order::Normal;
// 
// 	if (material->IsTransparent())
// 		props.order = Order::Transparent;
// 
// 	auto ent = ctx.renderWorld.createEntity(desc.tr, *model, props);
// 	ent->material = material;
}
