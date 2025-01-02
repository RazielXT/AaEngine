#include "EntityInstancing.h"
#include "SceneManager.h"
#include "Material.h"

void InstancingManager::clear()
{
	groups.clear();
	updates.clear();
}

InstanceGroup* InstancingManager::getGroup(UINT id) const
{
	auto it = groups.find(id);
	return it == groups.end() ? nullptr : it->second.get();
}

void InstancingManager::updateEntity(ObjectId id, ObjectTransformation transform)
{
	updates.emplace_back(id, transform);
}

void InstancingManager::update()
{
	for (auto& info : updates)
	{
		auto& group = groups[info.id.getGroupId()];

		group->update(info.id.getLocalIdx(), info.transform);
		group->updateBbox();
	}

	updates.clear();
}

InstanceGroup* InstancingManager::build(SceneManager& sceneMgr, const InstanceGroupDescription& description)
{
	if (description.objects.empty())
		return nullptr;

	auto& it = groups[groups.size()];
	it = std::make_unique<InstanceGroup>();
	it->create(description, groups.size() - 1);

	static int counter = 0;
	auto entity = sceneMgr.createEntity("Instancing" + std::to_string(counter++));
	entity->geometry.fromInstancedModel(*description.model, *it);
	entity->material = description.material;

	entity->setScale(Vector3::One);
	entity->setOrientation(Quaternion::Identity);

	it->entity = entity;
	it->modelBbox = description.model->bbox;
	it->updateBbox();

	return it.get();
}

XM_ALIGNED_STRUCT(16) InstanceData
{
	XMFLOAT4X4 transform;
};

InstanceGroup::~InstanceGroup()
{
	if (gpuIdsBuffer)
		DescriptorManager::get().removeDescriptorIndex(gpuIdsBufferHeapIdx);
}

void InstanceGroup::create(const InstanceGroupDescription& description, UINT groupIdx)
{
	std::vector<InstanceData> data;
	data.resize(description.objects.size());
	for (size_t i = 0; i < description.objects.size(); i++)
	{
		XMStoreFloat4x4(&data[i].transform, XMMatrixTranspose(description.objects[i].createWorldMatrix()));
	}

	gpuBuffer = ShaderDataBuffers::get().CreateUploadStructuredBuffer(data.data(), sizeof(InstanceData) * data.size());

	count = description.objects.size();

	std::vector<UINT> ids;
	ids.resize(count);
	entities.resize(count);
	for (size_t i = 0; i < description.objects.size(); i++)
	{
		entities[i].transformation = description.objects[i];
		ids[i] = ObjectId(i, ObjectType::Instanced, groupIdx).value;
	}
	gpuIdsBuffer = ShaderDataBuffers::get().CreateUploadStructuredBuffer(ids.data(), sizeof(UINT) * ids.size());
	gpuIdsBufferHeapIdx = DescriptorManager::get().createBufferView(gpuIdsBuffer.Get(), sizeof(UINT), count);
}

void InstanceGroup::update(UINT idx, ObjectTransformation transform)
{
	XMFLOAT4X4 data;
	XMStoreFloat4x4(&data, XMMatrixTranspose(transform.createWorldMatrix()));

	size_t dataSize = sizeof(data);
	size_t offset = idx * dataSize;

	D3D12_RANGE readRange = { offset, offset + dataSize };
	void* pData = nullptr;
	gpuBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pData));
	memcpy(static_cast<uint8_t*>(pData) + offset, &data, dataSize);
	gpuBuffer->Unmap(0, nullptr);
}

void InstanceGroup::updateBbox()
{
	auto init = entities.front().transformation.position - modelBbox.Center;
	BoundingBoxVolume volume{ init, init };
	for (auto& obj : entities)
		volume.add(obj.transformation.position - modelBbox.Center);

	auto bbox = volume.createBbox();
	bbox.Extents += modelBbox.Extents;

	entity->setPosition(bbox.Center);

	bbox.Center = {};
	entity->setBoundingBox(bbox);
}
