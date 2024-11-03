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

	AaEntity* createEntity(std::string name);
	AaEntity* createGrassEntity(std::string name, BoundingBoxVolume extends);

	AaEntity* getEntity(std::string name) const;

	RenderQueue* createQueue(const std::vector<DXGI_FORMAT>& targets, MaterialTechnique technique = MaterialTechnique::Default);

	RenderQueue createManualQueue(MaterialTechnique technique = MaterialTechnique::Default);

	void updateQueues();

	InstancingManager instancing;
	GrassManager grass;

	Renderables renderables;

private:

	EntityChanges changes;

	std::unordered_map<std::string, AaEntity*> entityMap;

	std::vector<std::unique_ptr<RenderQueue>> queues;

	AaRenderSystem* renderSystem;
};