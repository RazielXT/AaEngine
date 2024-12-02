#pragma once

#include "RenderQueue.h"
#include "EntityInstancing.h"
#include <unordered_map>
#include "RenderObject.h"
#include "GrassArea.h"

class SceneManager
{
public:

	SceneManager(RenderSystem* rs);
	~SceneManager();

	void update();
	void clear();

	AaEntity* createEntity(const std::string& name, Order order = Order::Normal);
	AaEntity* createEntity(const std::string& name, const ObjectTransformation&, BoundingBox, Order order = Order::Normal);

	AaEntity* getEntity(const std::string& name) const;

	RenderQueue* createQueue(const std::vector<DXGI_FORMAT>& targets, MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal);
	std::vector<DXGI_FORMAT> getQueueTargetFormats(MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal) const;
	RenderQueue createManualQueue(MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal);

	RenderObjectsStorage* getRenderables(Order order);

private:

	void updateQueues();
	void updateTransformations();

	std::map<Order, RenderObjectsStorage> renderables;

	EntityChanges changes;

	std::unordered_map<std::string, AaEntity*> entityMap;

	std::vector<std::unique_ptr<RenderQueue>> queues;

	RenderSystem* renderSystem;
};