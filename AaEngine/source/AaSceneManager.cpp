#include "AaSceneManager.h"
#include "AaMaterialResources.h"

AaSceneManager::AaSceneManager(AaRenderSystem* rs) : grass(rs)
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

void AaSceneManager::update()
{
	updateQueues();
	updateTransformations();
}

AaEntity* AaSceneManager::createEntity(const std::string& name, Order order)
{
	auto nameIt = entityMap.try_emplace(name, nullptr).first;
	auto ent = nameIt->second = new AaEntity(renderables[order], nameIt->first);

	changes.emplace_back(EntityChange::Add, order, ent);

	return ent;
}

AaEntity* AaSceneManager::createEntity(const std::string& name, const ObjectTransformation& transformation, BoundingBox bbox, Order order)
{
	auto entity = createEntity(name, order);
	entity->setBoundingBox(bbox);
	entity->setTransformation(transformation, true);

	return entity;
}

AaEntity* AaSceneManager::getEntity(const std::string& name) const
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

RenderQueue AaSceneManager::createManualQueue(MaterialTechnique technique, Order order)
{
	for (auto& q : queues)
	{
		if (q->technique == technique)
		{
			RenderQueue queue;
			queue.technique = technique;
			queue.targets = q->targets;
			queue.targetOrder = order;

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

void AaSceneManager::updateTransformations()
{
	for (auto& r : renderables)
	{
		r.second.updateTransformation();
	}
}

Renderables* AaSceneManager::getRenderables(Order order)
{
	return &renderables[order];
}

void AaSceneManager::clear()
{
	changes.emplace_back(EntityChange::DeleteAll);

	for (auto& [name, entity] : entityMap)
	{
		delete entity;
	}

	entityMap.clear();
	instancing.clear();
	grass.clear();
}
