#pragma once

#include "Scene/RenderQueue.h"
#include "Scene/EntityInstancing.h"
#include "Scene/RenderObject.h"
#include "RenderObject/WaterSim/WaterSim.h"
#include "RenderObject/Terrain/ProgressiveTerrain.h"
#include "RenderObject/Vegetation/Vegetation.h"
#include "RenderObject/Grass/Grass.h"
#include "Scene/SceneGraph.h"
#include <unordered_map>

struct SceneObject
{
	ObjectId id;
	ObjectType type;
	RenderEntity* entity{};
	void* owner{};
	void* item{};

	operator bool() const { return entity; }

	ObjectTransformation getTransformation() const;
	void setTransformation(const ObjectTransformation&);

	Vector3 getCenterPosition() const;

	std::string getName() const;
};

struct EntityCreateProperties
{
	Order order = Order::Normal;
	uint16_t groupId = 0;
	int suborder = 0;
};

class RenderWorld
{
public:

	RenderWorld(GraphicsResources& r);
	~RenderWorld();

	void initialize(RenderSystem& rs);

	void update();
	void clear();

	RenderEntity* createEntity(EntityCreateProperties props = {});
	RenderEntity* createEntity(const ObjectTransformation&, VertexBufferModel&, EntityCreateProperties = {});

	void removeEntity(RenderEntity* entity);
	void removeEntity(ObjectId id);

	RenderEntity* getEntity(ObjectId globalId) const;
	SceneObject getObject(ObjectId globalId);

	RenderQueue* createQueue(const std::vector<DXGI_FORMAT>& targets, MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal);
	RenderQueue* getQueue(MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal);
	RenderQueue createManualQueue(MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal);

	RenderObjectsStorage* getRenderables(Order order);

	SceneGraph graph;

	InstancingManager instancing;

	WaterSim water;

	ProgressiveTerrain terrain;

	Vegetation vegetation;

	Grass grass;

private:
	
	GraphicsResources& resources;

	void updateQueues();
	void updateTransformations();

	std::vector<RenderObjectsStorage> renderables;
	Order getOrder(RenderEntity* entity);

	std::vector<EntityChangeDescritpion> changes;

	std::set<RenderEntity*> entities;

	std::vector<std::unique_ptr<RenderQueue>> queues;
};