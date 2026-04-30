#include "Scene/SceneEntity.h"
#include "Resources/Model/ModelResources.h"
#include "Resources/Material/MaterialResources.h"

SceneEntity::SceneEntity(RenderObjectsStorage& r, std::string_view n, uint16_t groupId) : RenderObject(r, groupId), name(n.data())
{
}

SceneEntity::SceneEntity(RenderObjectsStorage& r, SceneEntity& source) : RenderObject(r, source.getGlobalId().getGroupId())
{
	name = source.name;
	material = source.material;
	geometry = source.geometry;

	setTransformation(source.getTransformation(), true);
}

SceneEntity::~SceneEntity()
{
}

EntityMaterialInterface SceneEntity::Material()
{
	if (!materialOverride)
		materialOverride = new MaterialPropertiesOverrideDescription();

	return { *materialOverride };
}

bool SceneEntity::GetMaterialParam(const std::string& name, void* data) const
{
	if (materialOverride)
	{
		for (auto& p : materialOverride->params)
		{
			if (p.name == name)
			{
				memcpy(data, p.value, p.sizeBytes);
				return true;
			}
		}
	}

	return material->GetParameter(name, (float*)data);
}

EntityMaterialInterface::EntityMaterialInterface(MaterialPropertiesOverrideDescription& s) : storage(s)
{
}

void EntityMaterialInterface::setParam(const std::string& name, const void* data, UINT sizeBytes)
{
	for (auto& p : storage.params)
	{
		if (p.name == name)
		{
			memcpy(p.value, data, sizeBytes);
			return;
		}
	}

	auto& param = storage.params.emplace_back(name, sizeBytes);
	memcpy(param.value, data, sizeBytes);
}
