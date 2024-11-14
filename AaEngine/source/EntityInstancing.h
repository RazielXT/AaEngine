#pragma once

#include "ShaderConstantBuffers.h"
#include "AaRenderables.h"
#include <vector>

class AaSceneManager;
class MaterialInstance;
class AaModel;

struct InstanceGroupDescription
{
	MaterialInstance* material;
	AaModel* model;
	std::vector<ObjectTransformation> objects;
};

struct InstanceGroup
{
	void create(const InstanceGroupDescription&);
	ComPtr<ID3D12Resource> gpuBuffer;
	UINT count{};
};

class InstancingManager
{
public:

	InstancingManager() = default;

	InstanceGroup* build(AaSceneManager* sceneMgr, const InstanceGroupDescription&);
	void clear();

private:

	std::map<MaterialInstance*, std::unique_ptr<InstanceGroup>> groups;
};