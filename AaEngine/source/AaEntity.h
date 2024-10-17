#pragma once

#include "AaModel.h"
#include "AaMaterial.h"
#include "AaRenderables.h"
#include <vector>

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

	void setModel(AaModel* model);
	AaModel* model{};

	MaterialInstance* material{};

	std::string name;

	Order order = Order::Normal;

	InstanceGroup* instancingGroup{};
};
