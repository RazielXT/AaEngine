#pragma once

#include "SceneLights.h"
#include "RenderQueue.h"
#include "EntityInstancing.h"
#include <unordered_map>
#include "AaRenderables.h"

class AaSceneManager
{
public:

	AaSceneManager(AaRenderSystem* rs);
	~AaSceneManager();

	void clear();

	AaEntity* createEntity(std::string name);
	AaEntity* getEntity(std::string name) const;

	aa::SceneLights lights;

	RenderQueue* createQueue(const std::vector<DXGI_FORMAT>& targets, bool depth = false, bool unique = false);

	void updateQueues();

	InstancingManager instancing;

	Renderables renderables;

private:

	EntityChanges changes;

	std::unordered_map<std::string, AaEntity*> entityMap;

	std::vector<std::unique_ptr<RenderQueue>> queues;

	AaRenderSystem* renderSystem;
};