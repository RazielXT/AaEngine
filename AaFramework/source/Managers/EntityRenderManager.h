#pragma once

#include "Scene/RenderEntity.h"
#include "../ECS/ComponentStorage.h"
#include "../ECS/Scene.h"
#include "Scene/Collection/SceneCollection.h"

struct EntityRenderInfoComponent
{
	RenderEntity* entity{};
};

class EntityRenderManager
{
public:

	EntityRenderManager(ECS::Scene& scene);

	void create(ECS::Entity, const SceneCollection::Entity& desc, SceneCollection::LoadCtx ctx);

private:

	ECS::Scene& scene;
};