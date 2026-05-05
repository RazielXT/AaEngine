#pragma once

#include "Scene/RenderQueue.h"
#include "Scene/EntityInstancing.h"
#include "Scene/RenderObject.h"
#include "Scene/SceneSkybox.h"
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
	SceneEntity* entity{};
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

class SceneManager
{
public:

	SceneManager(GraphicsResources& r);
	~SceneManager();

	void initialize(RenderSystem& rs);

	void update();
	void clear();

	SceneEntity* createEntity(EntityCreateProperties props = {});
	SceneEntity* createEntity(const ObjectTransformation&, VertexBufferModel&, EntityCreateProperties = {});
	void removeEntity(SceneEntity* entity);
	void removeEntity(ObjectId id);

	SceneEntity* getEntity(ObjectId globalId) const;
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

	SceneSkybox skybox;

private:
	
	GraphicsResources& resources;

	void updateQueues();
	void updateTransformations();

	std::vector<RenderObjectsStorage> renderables;
	Order getOrder(SceneEntity* entity);

	std::vector<EntityChangeDescritpion> changes;

	std::set<SceneEntity*> entities;

	std::vector<std::unique_ptr<RenderQueue>> queues;
};