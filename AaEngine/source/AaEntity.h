#pragma once

#include "AaModel.h"
#include "AaMaterial.h"
#include "AaRenderables.h"

class AaEntity : public RenderObject
{
public:

	AaEntity();
	AaEntity(std::string name);
	~AaEntity();

	std::string getName() { return name; }
	void setModel(std::string filename);

	AaModel* model{};

	void setMaterial(std::string name);
	AaMaterial* material{};

	AaMaterial* depthMaterial{};

private:

	std::string name;
};
