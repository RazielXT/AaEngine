#pragma once

#include "Resources/Model/VertexBufferModel.h"
#include "Resources/Material/Material.h"
#include "Scene/RenderObject.h"
#include <vector>
#include "Scene/EntityGeometry.h"

struct InstanceGroup;

class EntityMaterialInterface
{
public:

	EntityMaterialInterface(MaterialPropertiesOverrideDescription&, RenderEntity& entity);

	template<typename T>
	void setParam(const std::string& name, const T& value)
	{
		setParam(name, &value, sizeof(T));
	}

	void setParam(const std::string& name, const void* data, UINT sizeBytes);

private:
	MaterialPropertiesOverrideDescription& storage;
	RenderEntity& entity;
};

class RenderEntity : public RenderObject
{
public:

	RenderEntity(RenderObjectsStorage&, uint16_t groupId = 0);
	RenderEntity(RenderObjectsStorage&, RenderEntity& source);
	~RenderEntity();

	MaterialInstance* material{};

	EntityMaterialInterface Material();
	bool GetMaterialParam(const std::string& name, void* data) const;
	MaterialPropertiesOverrideDescription* materialOverride{};

	EntityGeometry geometry;
};
