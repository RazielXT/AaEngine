#include "Scene/RenderEntity.h"
#include "Resources/Model/ModelResources.h"
#include "Resources/Material/MaterialResources.h"
#include "Resources/Material/MaterialEvents.h"

RenderEntity::RenderEntity(RenderObjectsStorage& r, uint16_t groupId) : RenderObject(r, groupId)
{
}

RenderEntity::RenderEntity(RenderObjectsStorage& r, RenderEntity& source) : RenderObject(r, source.getGlobalId().getGroupId())
{
	material = source.material;
	geometry = source.geometry;

	setTransformation(source.getTransformation(), true);
}

RenderEntity::~RenderEntity()
{
}

EntityMaterialInterface RenderEntity::Material()
{
	if (!materialOverride)
		materialOverride = new MaterialPropertiesOverrideDescription();

	return { *materialOverride, *this };
}

bool RenderEntity::GetMaterialParam(const std::string& name, void* data) const
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

EntityMaterialInterface::EntityMaterialInterface(MaterialPropertiesOverrideDescription& s, RenderEntity& e) : storage(s), entity(e)
{
}

void EntityMaterialInterface::setParam(const std::string& name, const void* data, UINT sizeBytes)
{
	for (auto& p : storage.params)
	{
		if (p.name == name)
		{
			memcpy(p.value, data, sizeBytes);
			MaterialEvents::Get().notifyEntityParamChanged(entity);
			return;
		}
	}

	auto& param = storage.params.emplace_back(name, sizeBytes);
	memcpy(param.value, data, sizeBytes);

	MaterialEvents::Get().notifyEntityParamChanged(entity);
}
