#include "EntityInstancing.h"
#include "AaSceneManager.h"
#include "AaMaterial.h"

void InstancingManager::create()
{
	for (auto& [material, g] : groups)
	{
		g->create("Name");
		g->update();
	}
}

void InstancingManager::clear()
{
	groups.clear();
}

InstanceGroup* InstancingManager::getGroup(AaSceneManager* mgr, MaterialInstance* material, AaModel* model)
{
	auto& it = groups[material];

	if (!it)
	{
		it = std::make_unique<InstanceGroup>();

		auto entity = mgr->createEntity("Instance");
		entity->setModel(model);
		entity->material = material;
		entity->instancingGroup = it.get();

		it->objects.reserve(200);
	}

	return it.get();
}

XM_ALIGNED_STRUCT(16) InstanceData
{
	XMFLOAT4X4 transform;
};

void InstanceGroup::create(std::string materialName)
{
	buffer = ShaderConstantBuffers::get().CreateCbufferResource(sizeof(InstanceData) * objects.size(), "Instancing");
}

void InstanceGroup::update()
{
	std::vector<InstanceData> data;
	data.resize(objects.size());
	for (size_t i = 0; i < objects.size(); i++)
	{
		XMStoreFloat4x4(&data[i].transform, XMMatrixTranspose(objects[i].getWorldMatrix()));
	}

	for (int i = 0; i < 2; i++)
	{
		memcpy(buffer.data[i]->Memory(), data.data(), sizeof(InstanceData) * objects.size());
	}
}

RenderObject& InstanceGroup::createEntity(std::string name)
{
	return objects.emplace_back();
}
