#pragma once

#include "Scene/RenderEntity.h"
#include "../SceneGraph/ComponentStorage.h"
#include "../SceneGraph/Scene.h"
#include "Scene/Collection/SceneCollection.h"

struct EntityRenderInfoComponent
{
	RenderEntity* entity{};
};

class EntityRenderManager
{
public:

	EntityRenderManager(Scene& scene);

	void create(Entity, const SceneCollection::Entity& desc, SceneCollection::LoadCtx ctx);

private:

	Scene& scene;
};