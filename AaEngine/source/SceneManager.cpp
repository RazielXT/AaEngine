#include "SceneManager.h"
#include "AaMaterialResources.h"

SceneManager::SceneManager(AaRenderSystem* rs)
{
	renderSystem = rs;
}

SceneManager::~SceneManager()
{
	for (auto& [name, entity] : entityMap)
	{
		delete entity;
	}
}

void SceneManager::update()
{
	updateQueues();
	updateTransformations();
}

AaEntity* SceneManager::createEntity(const std::string& name, Order order)
{
	auto nameIt = entityMap.try_emplace(name, nullptr).first;
	auto ent = nameIt->second = new AaEntity(renderables[order], nameIt->first);

	changes.emplace_back(EntityChange::Add, order, ent);

	return ent;
}

AaEntity* SceneManager::createEntity(const std::string& name, const ObjectTransformation& transformation, BoundingBox bbox, Order order)
{
	auto entity = createEntity(name, order);
	entity->setBoundingBox(bbox);
	entity->setTransformation(transformation, true);

	return entity;
}

AaEntity* SceneManager::getEntity(const std::string& name) const
{
	auto it = entityMap.find(name);
	if (it != entityMap.end())
		return it->second;

	return nullptr;
}

RenderQueue* SceneManager::createQueue(const std::vector<DXGI_FORMAT>& targets, MaterialTechnique technique, Order order)
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

std::vector<DXGI_FORMAT> SceneManager::getQueueTargetFormats(MaterialTechnique technique, Order order) const
{
	for (auto& q : queues)
	{
		if (q->technique == technique && q->targetOrder == order)
			return q->targets;
	}

	return {};
}

RenderQueue SceneManager::createManualQueue(MaterialTechnique technique, Order order)
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

void SceneManager::updateQueues()
{
	for (auto& queue : queues)
	{
		queue->update(changes);
	}

	changes.clear();
}

void SceneManager::updateTransformations()
{
	for (auto& r : renderables)
	{
		r.second.updateTransformation();
	}
}

RenderObjectsStorage* SceneManager::getRenderables(Order order)
{
	return &renderables[order];
}

void SceneManager::clear()
{
	changes.emplace_back(EntityChange::DeleteAll);

	for (auto& [name, entity] : entityMap)
	{
		delete entity;
	}

	entityMap.clear();
}
