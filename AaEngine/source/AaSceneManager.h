#pragma once

#include "RenderQueue.h"
#include "EntityInstancing.h"
#include <unordered_map>
#include "AaRenderables.h"
#include "GrassArea.h"

class AaSceneManager
{
public:

	AaSceneManager(AaRenderSystem* rs);
	~AaSceneManager();

	void clear();

	AaEntity* createEntity(std::string name, Order order = Order::Normal);
	AaEntity* createGrassEntity(std::string name, GrassAreaDescription& desc);
	AaEntity* getEntity(std::string name) const;

	RenderQueue* createQueue(const std::vector<DXGI_FORMAT>& targets, MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal);
	RenderQueue createManualQueue(MaterialTechnique technique = MaterialTechnique::Default, Order order = Order::Normal);
	void updateQueues();

	void updateTransformations();
	Renderables* getRenderables(Order order);

	InstancingManager instancing;
	GrassManager grass;

private:

	std::map<Order, Renderables> renderables;

	EntityChanges changes;

	std::unordered_map<std::string, AaEntity*> entityMap;

	std::vector<std::unique_ptr<RenderQueue>> queues;

	AaRenderSystem* renderSystem;
};