#include "Scene/SceneManager.h"
#include "Resources/Material/MaterialEvents.h"

SceneManager::SceneManager(GraphicsResources& r) : resources(r), skybox(r), graph(*this)
{
	renderables.reserve(10); //need to be enough! distributed by ptr

	MaterialEvents::Get().addReloadListener([this](const std::vector<MaterialBase*>& reloaded)
	{
		for (auto& queue : queues)
			queue->rebuildEntries(reloaded);
	});

	MaterialEvents::Get().addEntityParamChangeListener([this](SceneEntity& entity)
	{
		for (auto& queue : queues)
			queue->rebuildEntries(&entity);
	});
}

SceneManager::~SceneManager()
{
	for (auto entity : entities)
	{
		delete entity;
	}
}

void SceneManager::initialize(RenderSystem& renderSystem)
{
	GlobalQueueMarker marker(renderSystem.core.commandQueue, "InitializeSceneManager");

	ResourceUploadBatch batch(renderSystem.core.device);
	batch.Begin();

	vegetation.initialize(renderSystem, resources, batch);
	grass.initialize(renderSystem, resources, batch);

	auto uploadResourcesFinished = batch.End(renderSystem.core.commandQueue);
	uploadResourcesFinished.wait();
}

void SceneManager::update()
{
	updateQueues();
	updateTransformations();
}

SceneEntity* SceneManager::createEntity(EntityCreateProperties props)
{
	auto ent = new SceneEntity(*getRenderables(props.order), props.groupId);

	entities.insert(ent);
	changes.emplace_back(EntityChange::Add, props.order, ent, ent->getGlobalId(), props.suborder);

	return ent;
}

SceneEntity* SceneManager::createEntity(const ObjectTransformation& transformation, VertexBufferModel& model, EntityCreateProperties props)
{
	auto entity = createEntity(props);
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
			entities.erase(c.entity);
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

	for (auto entity : entities)
	{
		delete entity;
	}

	entities.clear();

	instancing.clear();
	
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

#include <format>

std::string SceneObject::getName() const
{
	return std::format("{:08X}", id.value);
}
