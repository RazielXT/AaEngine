#pragma once

#include "RenderQueue.h"
#include "EntityInstancing.h"
#include <unordered_map>
#include "RenderObject.h"
#include "GrassArea.h"
#include "SceneSkybox.h"

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

class SceneManager
{
public:

	SceneManager(GraphicsResources& r);
	~SceneManager();

	void initialize(RenderSystem& rs);

	void update();
	void clear();

	SceneEntity* createEntity(const std::string& name, Order order = Order::Normal);
	SceneEntity* createEntity(const std::string& name, const ObjectTransformation&, BoundingBox, Order order = Order::Normal);
	void removeEntity(SceneEntity* entity);

	SceneEntity* getEntity(const std::string& name) const;
	SceneEntity* getEntity(ObjectId globalId) const;
	SceneObject getObject(ObjectId globalId);

	RenderQueue* createQueue(const std::vector<DXGI_FORMAT>& targets, MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal);
	std::vector<DXGI_FORMAT> getQueueTargetFormats(MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal) const;
	RenderQueue createManualQueue(MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal);

	RenderObjectsStorage* getRenderables(Order order);

	InstancingManager instancing;

	GrassAreaGenerator grass;

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
};