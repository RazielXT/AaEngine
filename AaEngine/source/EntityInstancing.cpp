#include "EntityInstancing.h"
#include "AaSceneManager.h"
#include "AaMaterial.h"

void InstancingManager::clear()
{
	groups.clear();
}

struct BoundingBoxExtends
{
	Vector3 min{};
	Vector3 max{};

	void add(Vector3 point)
	{
		min = Vector3::Min(min, point);
		max = Vector3::Max(max, point);
	}

	BoundingBox createBbox() const
	{
		BoundingBox bbox;
		bbox.Center = (max + min) / 2;
		bbox.Extents = (max - min) / 2;

		return bbox;
	}
};

InstanceGroup* InstancingManager::build(AaSceneManager* sceneMgr, const InstanceGroupDescription& description)
{
	if (description.objects.empty())
		return nullptr;

	auto& it = groups[description.material];

	if (!it)
	{
		it = std::make_unique<InstanceGroup>();

		static int counter = 0;
		auto entity = sceneMgr->createEntity("Instancing" + std::to_string(counter++));
		entity->setModel(description.model);
		entity->material = description.material;
		entity->instancingGroup = it.get();

		auto init = description.objects.front().position;
		BoundingBoxExtends volume{ init, init };
		for (auto& obj : description.objects)
			volume.add(obj.position);

		auto bbox = volume.createBbox();
		bbox.Extents += description.model->bbox.Extents;
		entity->setBoundingBox(bbox);

		entity->setScale(Vector3::One);
		entity->setOrientation(Quaternion::Identity);

		it->create(description);
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

	buffer = ShaderConstantBuffers::get().CreateCbufferResource(sizeof(InstanceData) * data.size());

	for (auto& b : buffer.data)
	{
		memcpy(b.Memory(), data.data(), sizeof(InstanceData) * data.size());
	}

	count = description.objects.size();
}
