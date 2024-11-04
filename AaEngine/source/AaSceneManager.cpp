#include "AaSceneManager.h"
#include "AaMaterialResources.h"

AaSceneManager::AaSceneManager(AaRenderSystem* rs)
{
	renderSystem = rs;
}

AaSceneManager::~AaSceneManager()
{
	for (auto& [name, entity] : entityMap)
	{
		delete entity;
	}
}

AaEntity* AaSceneManager::createEntity(std::string name)
{
	auto ent = new AaEntity(renderables, name);
	entityMap[name] = ent;

	changes.emplace_back(EntityChange::Add, ent);

	return ent;
}

AaEntity* AaSceneManager::createGrassEntity(std::string name, BoundingBoxVolume extends)
{
	auto grassEntity = createEntity(name);

	auto& g = *grass.addGrass(extends);
	grassEntity->geometry.fromGrass(g);
	grassEntity->setBoundingBox(g.bbox);
	grassEntity->material = AaMaterialResources::get().getMaterial("GrassLeaves");

	return grassEntity;
}

AaEntity* AaSceneManager::getEntity(std::string name) const
{
	auto it = entityMap.find(name);
	if (it != entityMap.end())
		return it->second;

	return nullptr;
}

RenderQueue* AaSceneManager::createQueue(const std::vector<DXGI_FORMAT>& targets, MaterialTechnique technique, Order order)
{
	for (auto& q : queues)
	{
		if (q->targets == targets && q->technique == technique && q->targetOrder == order)
			return q.get();
	}

	auto queue = std::make_unique<RenderQueue>();
	queue->targets = targets;
	queue->technique = technique;
	queue->targetOrder = order;

	return queues.emplace_back(std::move(queue)).get();
}

RenderQueue AaSceneManager::createManualQueue(MaterialTechnique technique)
{
	for (auto& q : queues)
	{
		if (q->technique == technique)
		{
			RenderQueue queue;
			queue.technique = technique;
			queue.targets = q->targets;

			return queue;
		}
	}

	return {};
}

void AaSceneManager::updateQueues()
{
	for (auto& queue : queues)
	{
		queue->update(changes);
	}

	changes.clear();
}

void AaSceneManager::clear()
{
	changes.emplace_back(EntityChange::DeleteAll, nullptr);

	for (auto& [name, entity] : entityMap)
	{
		delete entity;
	}

	entityMap.clear();
	instancing.clear();
	grass.clear();
}
