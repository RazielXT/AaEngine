#pragma once

#include <string>
#include "AaRenderSystem.h"
#include "AaModel.h"

class AaModelResources
{
public:

	AaModelResources(AaRenderSystem* mRenderSystem);
	~AaModelResources();

	static AaModelResources& get();

	AaModel* getModel(const std::string& filename);

private:

	AaRenderSystem* mRenderSystem;

	AaModel* loadModel(const std::string& filename);
	std::map<std::string, AaModel*> mModelMap;
};
