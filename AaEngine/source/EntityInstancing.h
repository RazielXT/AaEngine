#pragma once

#include "ShaderDataBuffers.h"
#include "RenderObject.h"
#include <vector>

class SceneManager;
class SceneEntity;
class MaterialInstance;
class VertexBufferModel;

struct InstanceGroupDescription
{
	MaterialInstance* material;
	VertexBufferModel* model;
	std::vector<ObjectTransformation> objects;
};

struct InstancedEntity
{
	ObjectTransformation transformation;
};

struct InstanceGroup
{
	InstanceGroup() = default;
	~InstanceGroup();

	void create(const InstanceGroupDescription&, UINT groupIdx);
	void update(UINT idx, ObjectTransformation);
	void updateBbox();
	ComPtr<ID3D12Resource> gpuBuffer;
	ComPtr<ID3D12Resource> gpuIdsBuffer;
	UINT gpuIdsBufferHeapIdx{};

	UINT count{};
	SceneEntity* entity{};
	std::vector<InstancedEntity> entities;

	BoundingBox modelBbox;
};

class InstancingManager
{
public:

	InstancingManager() = default;

	void update();
	InstanceGroup* build(SceneManager& sceneMgr, const InstanceGroupDescription&);
	void clear();

	InstanceGroup* getGroup(UINT) const;

	void updateEntity(ObjectId, const ObjectTransformation&);

private:

	std::map<UINT, std::unique_ptr<InstanceGroup>> groups;

	struct ScheduledUpdate
	{
		ObjectId id;
		ObjectTransformation transform;
	};
	std::vector<ScheduledUpdate> updates;
};