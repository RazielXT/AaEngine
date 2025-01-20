#include "SceneManager.h"
#include "MaterialResources.h"

SceneManager::SceneManager(GraphicsResources& r) : resources(r), skybox(r)
{
	renderables.reserve(10); //need to be enough! distributed by ptr
}

SceneManager::~SceneManager()
{
	for (auto& [name, entity] : entityMap)
	{
		delete entity;
	}
}

void SceneManager::initialize(RenderSystem& renderSystem)
{
	grass.initializeGpuResources(renderSystem, resources, getQueueTargetFormats());
	terrain.initialize(renderSystem, resources);
}

void SceneManager::update()
{
	updateQueues();
	updateTransformations();
}

SceneEntity* SceneManager::createEntity(const std::string& name, Order order)
{
	auto nameIt = entityMap.try_emplace(name, nullptr).first;
	auto ent = nameIt->second = new SceneEntity(*getRenderables(order), nameIt->first);

	changes.emplace_back(EntityChange::Add, order, ent);

	return ent;
}

SceneEntity* SceneManager::createEntity(const std::string& name, const ObjectTransformation& transformation, BoundingBox bbox, Order order)
{
	auto entity = createEntity(name, order);
	entity->setBoundingBox(bbox);
	entity->setTransformation(transformation, true);

	return entity;
}

void SceneManager::removeEntity(SceneEntity* entity)
{
	entityMap.erase(entity->name);
	changes.emplace_back(EntityChange::Delete, getOrder(entity), entity);

	delete entity;
}

SceneEntity* SceneManager::getEntity(const std::string& name) const
{
	auto it = entityMap.find(name);
	if (it != entityMap.end())
		return it->second;

	return nullptr;
}

SceneEntity* SceneManager::getEntity(ObjectId globalId) const
{
	auto type = globalId.getObjectType();

	if (type == ObjectType::Entity)
	{
		auto o = globalId.getOrder();
		for (auto& r : renderables)
		{
			if (r.order == o)
			{
				auto id = globalId.getLocalIdx();
				return (SceneEntity*)r.getObject(id);
			}
		}
	}
	else if (type == ObjectType::Instanced)
	{
		auto group = instancing.getGroup(globalId.getGroupId());

		return group->entity;
	}

	return nullptr;
}

SceneObject SceneManager::getObject(ObjectId globalId)
{
	auto type = globalId.getObjectType();

	if (type == ObjectType::Entity)
	{
		auto o = globalId.getOrder();
		for (auto& r : renderables)
		{
			if (r.order == o)
			{
				auto id = globalId.getLocalIdx();
				return { globalId, type, (SceneEntity*)r.getObject(id), this };
			}
		}
	}
	else if (type == ObjectType::Instanced)
	{
		auto group = instancing.getGroup(globalId.getGroupId());
		auto& item = group->entities[globalId.getLocalIdx()];

		return { globalId, type, group->entity, &instancing, &item };
	}

	return {};
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

RenderQueue* SceneManager::getQueue(MaterialTechnique technique /*= MaterialTechnique::Default*/, Order order /*= Order::Normal*/)
{
	for (auto& q : queues)
	{
		if (q->technique == technique && q->targetOrder == order)
			return q.get();
	}

	return nullptr;
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
		for (auto& c : changes)
		{
			queue->update(c, resources);
		}
	}

	changes.clear();
}

void SceneManager::updateTransformations()
{
	for (auto& r : renderables)
	{
		r.updateTransformation();
	}

	instancing.update();
}

Order SceneManager::getOrder(SceneEntity* entity)
{
	return entity->getStorage().order;
}

RenderObjectsStorage* SceneManager::getRenderables(Order order)
{
	for (auto& r : renderables)
	{
		if (r.order == order)
			return &r;
	}

	return &renderables.emplace_back(order);
}

void SceneManager::clear()
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

ObjectTransformation SceneObject::getTransformation() const
{
	if (type == ObjectType::Entity)
		return entity->getTransformation();
	if (type == ObjectType::Instanced)
		return ((InstancedEntity*)item)->transformation;

	return {};
}

void SceneObject::setTransformation(const ObjectTransformation& transformation)
{
	if (type == ObjectType::Entity)
		entity->setTransformation(transformation, true);
	if (type == ObjectType::Instanced)
	{
		((InstancingManager*)owner)->updateEntity(id, transformation);
		((InstancedEntity*)item)->transformation = transformation;
	}
}

Vector3 SceneObject::getCenterPosition() const
{
	if (type == ObjectType::Entity)
		return entity->getWorldBoundingBox().Center;
	if (type == ObjectType::Instanced)
		return ((InstancedEntity*)item)->transformation.position;

	return {};
}

const char* SceneObject::getName() const
{
	if (type == ObjectType::Entity)
		return entity->name;
	if (type == ObjectType::Instanced)
		return entity->name;

	return "";
}
