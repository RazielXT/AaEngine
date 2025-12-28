#pragma once

#include "VertexBufferModel.h"
#include "Material.h"
#include "RenderObject.h"
#include <vector>
#include "EntityGeometry.h"

struct InstanceGroup;

class EntityMaterialInterface
{
public:

	EntityMaterialInterface(MaterialPropertiesOverrideDescription&);

	template<typename T>
	void setParam(const std::string& name, const T& value)
	{
		setParam(name, &value, sizeof(T));
	}

	void setParam(const std::string& name, const void* data, UINT sizeBytes);

private:
	MaterialPropertiesOverrideDescription& storage;
};

class SceneEntity : public RenderObject
{
public:

	SceneEntity(RenderObjectsStorage&, std::string_view name);
	SceneEntity(RenderObjectsStorage&, SceneEntity& source);
	~SceneEntity();

	const char* name;

	MaterialInstance* material{};

	EntityMaterialInterface Material();
	MaterialPropertiesOverrideDescription* materialOverride{};

	EntityGeometry geometry;
};
