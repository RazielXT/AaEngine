#pragma once

#include <string>
#include "RenderSystem.h"
#include "AaModel.h"
#include "ResourceUploadBatch.h"

struct ModelLoadContext
{
	ModelLoadContext(RenderSystem*);

	ResourceUploadBatch batch;
	std::string folder;
};

class AaModelResources
{
public:

	AaModelResources(RenderSystem* mRenderSystem);
	~AaModelResources();

	static AaModelResources& get();

	AaModel* getModel(const std::string& name, ModelLoadContext& ctx);

	void clear();

private:

	RenderSystem* mRenderSystem;

	AaModel* loadModel(const std::string& name, ModelLoadContext& ctx);
	std::map<std::string, AaModel*> loadedModels;
};
