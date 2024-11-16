#pragma once

#include "AaModel.h"
#include "AaMaterial.h"
#include "RenderObject.h"
#include <vector>
#include "EntityGeometry.h"

enum class Order
{
	Normal = 50,
	Transparent = 60,
};

struct InstanceGroup;

class AaEntity : public RenderObject
{
public:

	AaEntity(RenderObjectsStorage&, std::string_view name);
	AaEntity(RenderObjectsStorage&, AaEntity& source);
	~AaEntity();

	const char* name;

	MaterialInstance* material{};

	EntityGeometry geometry;
};
