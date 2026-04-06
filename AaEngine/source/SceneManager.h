#pragma once

#include "RenderQueue.h"
#include "EntityInstancing.h"
#include <unordered_map>
#include "RenderObject.h"
#include "GrassArea.h"
#include "SceneSkybox.h"
#include "WaterSim.h"
#include "ProgressiveTerrain.h"
#include "Vegetation.h"
#include "SceneGraph.h"

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

	const char* getName() const;
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

	SceneEntity* createEntity(const std::string& name, EntityCreateProperties props = {});
	SceneEntity* createEntity(const std::string& name, const ObjectTransformation&, VertexBufferModel&, EntityCreateProperties = {});
	void removeEntity(SceneEntity* entity);
	void removeEntity(ObjectId id);

	SceneEntity* getEntity(const std::string& name) const;
	SceneEntity* getEntity(ObjectId globalId) const;
	SceneObject getObject(ObjectId globalId);

	uint16_t createEntityGroup(const std::string& name);
	const std::string& getEntityGroup(uint16_t id);

	RenderQueue* createQueue(const std::vector<DXGI_FORMAT>& targets, MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal);
	RenderQueue* getQueue(MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal);
	RenderQueue createManualQueue(MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal);

	RenderObjectsStorage* getRenderables(Order order);

	SceneGraph graph;

	InstancingManager instancing;

	GrassAreaGenerator grass;

	WaterSim water;

	ProgressiveTerrain newTerrain;

	Vegetation vegetation;

	SceneSkybox skybox;

private:
	
	GraphicsResources& resources;

	void updateQueues();
	void updateTransformations();

	std::vector<RenderObjectsStorage> renderables;
	Order getOrder(SceneEntity* entity);

	std::vector<EntityChangeDescritpion> changes;

	std::unordered_map<std::string, SceneEntity*> entityMap;

	std::vector<std::unique_ptr<RenderQueue>> queues;

	struct EntityGroupInfo
	{
		std::unique_ptr<std::string> name;
	};
	std::vector<EntityGroupInfo> entityGroups;
	void resetEntityGroups();
};