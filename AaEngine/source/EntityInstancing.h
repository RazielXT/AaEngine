#pragma once

#include "ShaderConstantBuffers.h"
#include "AaRenderables.h"
#include <vector>

struct InstanceGroup
{
	void create(std::string materialName);
	void update();

	std::vector<RenderObject> objects;
	CbufferView buffer;
	RenderObject& createEntity(std::string name);
};

class AaSceneManager;
class MaterialInstance;
class AaModel;

class InstancingManager
{
public:

	InstancingManager() = default;

	void create();
	void clear();

	InstanceGroup* getGroup(AaSceneManager* mgr, MaterialInstance* material, AaModel* model);

private:

	std::map<MaterialInstance*, std::unique_ptr<InstanceGroup>> groups;
};