#include "EntityInstancing.h"
#include "AaSceneManager.h"
#include "AaMaterial.h"

void InstancingManager::clear()
{
	groups.clear();
}

InstanceGroup* InstancingManager::build(AaSceneManager* sceneMgr, const InstanceGroupDescription& description)
{
	if (description.objects.empty())
		return nullptr;

	auto& it = groups[description.material];

	if (!it)
	{
		it = std::make_unique<InstanceGroup>();
		it->create(description);

		static int counter = 0;
		auto entity = sceneMgr->createEntity("Instancing" + std::to_string(counter++));
		entity->geometry.fromInstancedModel(*description.model, *it);
		entity->material = description.material;

		auto init = description.objects.front().position;
		BoundingBoxVolume volume{ init, init };
		for (auto& obj : description.objects)
			volume.add(obj.position);

		auto bbox = volume.createBbox();
		bbox.Extents += description.model->bbox.Extents;
		entity->setBoundingBox(bbox);

		entity->setScale(Vector3::One);
		entity->setOrientation(Quaternion::Identity);
	}

	return it.get();
}

XM_ALIGNED_STRUCT(16) InstanceData
{
	XMFLOAT4X4 transform;
};

void InstanceGroup::create(const InstanceGroupDescription& description)
{
	std::vector<InstanceData> data;
	data.resize(description.objects.size());
	for (size_t i = 0; i < description.objects.size(); i++)
	{
		XMStoreFloat4x4(&data[i].transform, XMMatrixTranspose(description.objects[i].createWorldMatrix()));
	}

	gpuBuffer = ShaderConstantBuffers::get().CreateStructuredBuffer(data.data(), sizeof(InstanceData) * data.size());

	count = description.objects.size();
}
