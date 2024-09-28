#include "AaSceneManager.h"
#include "AaMaterialResources.h"
#include "AaMaterialConstants.h"

AaSceneManager::AaSceneManager(AaRenderSystem* rs)
{
	renderSystem = rs;
}

AaSceneManager::~AaSceneManager()
{
	for (auto& [name, entity] : entityMap)
	{
		delete entity;
	}
}

AaEntity* AaSceneManager::createEntity(std::string name)
{
	auto ent = new AaEntity(name);
	entityMap[name] = ent;

	changes.emplace_back(EntityChange::Add, ent);

	return ent;
}

AaEntity* AaSceneManager::getEntity(std::string name) const
{
	auto it = entityMap.find(name);
	if (it != entityMap.end())
		return it->second;

	return nullptr;
}

RenderQueue* AaSceneManager::createQueue(const std::vector<DXGI_FORMAT>& targets, const char* material, bool unique)
{
	MaterialInstance* materialInstance = nullptr;
	if (material)
		materialInstance = AaMaterialResources::get().getMaterial(material);

	if (!unique)
	{
		for (auto& q : queues)
		{
			if (q->targets == targets && q->materialOverride == materialInstance)
				return q.get();
		}
	}

	auto queue = std::make_unique<RenderQueue>();
	queue->targets = targets;
	queue->materialOverride = materialInstance;

	return queues.emplace_back(std::move(queue)).get();
}

void AaSceneManager::updateQueues()
{
	for (auto& queue : queues)
	{
		queue->update(changes);
	}

	changes.clear();
}

void AaSceneManager::clear()
{
	changes.emplace_back(EntityChange::DeleteAll, nullptr);

	for (auto& [name, entity] : entityMap)
	{
		delete entity;
	}

	entityMap.clear();
}
