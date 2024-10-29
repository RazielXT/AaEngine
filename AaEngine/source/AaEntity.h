#pragma once

#include "AaModel.h"
#include "AaMaterial.h"
#include "AaRenderables.h"
#include <vector>
#include "EntityGeometry.h"

enum class Order
{
	Normal = 50,
};

struct InstanceGroup;

class AaEntity : public RenderObject
{
public:

	AaEntity(Renderables&, std::string name);
	~AaEntity();

	std::string name;

	Order order = Order::Normal;

	MaterialInstance* material{};

	EntityGeometry geometry;

	GrassArea* grass{};
};
