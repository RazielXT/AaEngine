#include "SceneManager.h"
#include "MaterialResources.h"

SceneManager::SceneManager(GraphicsResources& r) : resources(r), skybox(r), graph(*this)
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
	GlobalQueueMarker marker(renderSystem.core.commandQueue, "InitializeSceneManager");

	ResourceUploadBatch batch(renderSystem.core.device);
	batch.Begin();

	grass.initializeGpuResources(renderSystem, resources, batch);
	//terrain.initialize(renderSystem, resources, batch);
	vegetation.initialize(renderSystem, resources, batch);

	auto uploadResourcesFinished = batch.End(renderSystem.core.commandQueue);
	uploadResourcesFinished.wait();

	resetEntityGroups();
}

void SceneManager::update()
{
	updateQueues();
	updateTransformations();
}

SceneEntity* SceneManager::createEntity(const std::string& name, EntityCreateProperties props)
{
	auto nameIt = entityMap.try_emplace(name, nullptr).first;
	auto ent = nameIt->second = new SceneEntity(*getRenderables(props.order), nameIt->first, props.groupId);

	changes.emplace_back(EntityChange::Add, props.order, ent, ent->getGlobalId(), props.suborder);

	return ent;
}

SceneEntity* SceneManager::createEntity(const std::string& name, const ObjectTransformation& transformation, VertexBufferModel& model, EntityCreateProperties props)
{
	auto entity = createEntity(name, props);
	entity->setBoundingBox(model.bbox);
	entity->setTransformation(transformation, true);
	entity->geometry.fromModel(model);

	return entity;
}

void SceneManager::removeEntity(SceneEntity* entity)
{
	changes.emplace_back(EntityChange::Delete, getOrder(entity), entity, entity->getGlobalId(), 0);
}

void SceneManager::removeEntity(ObjectId id)
{
	if (auto e = getEntity(id))
		removeEntity(e);
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

uint16_t SceneManager::createEntityGroup(const std::string& name)
{
	entityGroups.emplace_back(std::make_unique<std::string>(name));

	return (uint16_t)entityGroups.size() - 1;
}

const std::string& SceneManager::getEntityGroup(uint16_t id)
{
	if (id >= entityGroups.size())
		__debugbreak();

	return *entityGroups[id].name;
}

RenderQueue* SceneManager::createQueue(const std::vector<DXGI_FORMAT>& targets, MaterialTechnique technique, Order order)
{
	for (auto& q : queues)
	{
		if (q->targetFormats == targets && q->technique == technique && q->targetOrder == order)
			return q.get();
	}

	auto queue = std::make_unique<RenderQueue>();
	queue->targetFormats = targets;
	queue->technique = technique;
	queue->targetOrder = order;

	auto r = getRenderables(order);
	r->iterateObjects([&](RenderObject& obj)
		{
			queue->update({ EntityChange::Add, order, (SceneEntity*) &obj}, resources);
		});

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

RenderQueue SceneManager::createManualQueue(MaterialTechnique technique, Order order)
{
	for (auto& q : queues)
	{
		if (q->technique == technique)
		{
			RenderQueue queue;
			queue.technique = technique;
			queue.targetFormats = q->targetFormats;
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

	graph.updateEntity(changes);

	for (auto& c : changes)
	{
		if (c.type == EntityChange::Delete)
		{
			entityMap.erase(c.entity->name);
			delete c.entity;
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

void SceneManager::resetEntityGroups()
{
	entityGroups.clear();
	entityGroups.emplace_back(std::make_unique<std::string>("root"));
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

	resetEntityGroups();

	instancing.clear();

	grass.clear();
	
	water.clear();
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
