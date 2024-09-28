#pragma once

#include "AaModel.h"
#include "AaMaterial.h"
#include "AaRenderables.h"

enum class Order
{
	Normal = 50,
};

class AaEntity : public RenderObject
{
public:

	AaEntity();
	AaEntity(std::string name);
	~AaEntity();

	void setModel(AaModel* model);
	AaModel* model{};

	MaterialInstance* material{};

	std::string name;

	Order order = Order::Normal;
};
