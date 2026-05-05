#include "Scene/RenderWorld.h"
#include "Resources/Material/MaterialEvents.h"

RenderWorld::RenderWorld(GraphicsResources& r) : resources(r), skybox(r), graph(*this)
{
	renderables.reserve(10); //need to be enough! distributed by ptr

	MaterialEvents::Get().addReloadListener([this](const std::vector<MaterialBase*>& reloaded)
	{
		for (auto& queue : queues)
			queue->rebuildEntries(reloaded);
	});

	MaterialEvents::Get().addEntityParamChangeListener([this](RenderEntity& entity)
	{
		for (auto& queue : queues)
			queue->rebuildEntries(&entity);
	});
}

RenderWorld::~RenderWorld()
{
	for (auto entity : entities)
	{
		delete entity;
	}
}

void RenderWorld::initialize(RenderSystem& renderSystem)
{
	GlobalQueueMarker marker(renderSystem.core.commandQueue, "InitializeSceneManager");

	ResourceUploadBatch batch(renderSystem.core.device);
	batch.Begin();

	vegetation.initialize(renderSystem, resources, batch);
	grass.initialize(renderSystem, resources, batch);

	auto uploadResourcesFinished = batch.End(renderSystem.core.commandQueue);
	uploadResourcesFinished.wait();
}

void RenderWorld::update()
{
	updateQueues();
	updateTransformations();
}

RenderEntity* RenderWorld::createEntity(EntityCreateProperties props)
{
	auto ent = new RenderEntity(*getRenderables(props.order), props.groupId);

	entities.insert(ent);
	changes.emplace_back(EntityChange::Add, props.order, ent, ent->getGlobalId(), props.suborder);

	return ent;
}

RenderEntity* RenderWorld::createEntity(const ObjectTransformation& transformation, VertexBufferModel& model, EntityCreateProperties props)
{
	auto entity = createEntity(props);
	entity->setBoundingBox(model.bbox);
	entity->setTransformation(transformation, true);
	entity->geometry.fromModel(model);

	return entity;
}

void RenderWorld::removeEntity(RenderEntity* entity)
{
	changes.emplace_back(EntityChange::Delete, getOrder(entity), entity, entity->getGlobalId(), 0);
}

void RenderWorld::removeEntity(ObjectId id)
{
	if (auto e = getEntity(id))
		removeEntity(e);
}

RenderEntity* RenderWorld::getEntity(ObjectId globalId) const
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
				return (RenderEntity*)r.getObject(id);
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

SceneObject RenderWorld::getObject(ObjectId globalId)
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
				return { globalId, type, (RenderEntity*)r.getObject(id), this };
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

RenderQueue* RenderWorld::createQueue(const std::vector<DXGI_FORMAT>& targets, MaterialTechnique technique, Order order)
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
			queue->update({ EntityChange::Add, order, (RenderEntity*) &obj}, resources);
		});

	return queues.emplace_back(std::move(queue)).get();
}

RenderQueue* RenderWorld::getQueue(MaterialTechnique technique /*= MaterialTechnique::Default*/, Order order /*= Order::Normal*/)
{
	for (auto& q : queues)
	{
		if (q->technique == technique && q->targetOrder == order)
			return q.get();
	}

	return nullptr;
}

RenderQueue RenderWorld::createManualQueue(MaterialTechnique technique, Order order)
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

void RenderWorld::updateQueues()
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

void RenderWorld::updateTransformations()
{
	for (auto& r : renderables)
	{
		r.updateTransformation();
	}

	instancing.update();
}

Order RenderWorld::getOrder(RenderEntity* entity)
{
	return entity->getStorage().order;
}

RenderObjectsStorage* RenderWorld::getRenderables(Order order)
{
	for (auto& r : renderables)
	{
		if (r.order == order)
			return &r;
	}

	return &renderables.emplace_back(order);
}

void RenderWorld::clear()
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
