#pragma once

#include "SceneLights.h"
#include <unordered_map>
#include "RenderQueue.h"

class AaSceneManager
{
public:

	AaSceneManager(AaRenderSystem* rs);
	~AaSceneManager();

	void clear();

	AaEntity* createEntity(std::string name);
	AaEntity* getEntity(std::string name) const;

	aa::SceneLights lights;

	RenderQueue* createQueue(const std::vector<DXGI_FORMAT>& targets, const char* material = nullptr, bool unique = false);
	void updateQueues();

private:

	EntityChanges changes;

	std::unordered_map<std::string, AaEntity*> entityMap;

	std::vector<std::unique_ptr<RenderQueue>> queues;

	AaRenderSystem* renderSystem;
};