#pragma once

#include <string>
#include "AaRenderSystem.h"
#include "AaModel.h"
#include "ResourceUploadBatch.h"

struct ModelLoadContext
{
	ModelLoadContext(AaRenderSystem*);

	ResourceUploadBatch batch;
	std::string folder;
};

class AaModelResources
{
public:

	AaModelResources(AaRenderSystem* mRenderSystem);
	~AaModelResources();

	static AaModelResources& get();

	AaModel* getModel(const std::string& name, ModelLoadContext& ctx);

	void clear();

private:

	AaRenderSystem* mRenderSystem;

	AaModel* loadModel(const std::string& name, ModelLoadContext& ctx);
	std::map<std::string, AaModel*> loadedModels;
};
