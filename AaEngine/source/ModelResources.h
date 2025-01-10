#pragma once

#include <string>
#include "RenderSystem.h"
#include "VertexBufferModel.h"
#include "ResourceUploadBatch.h"
#include "ResourceGroup.h"

struct ModelLoadContext
{
	ModelLoadContext(RenderSystem&);

	ResourceUploadBatch batch;
	std::string folder;
	ResourceGroup group = ResourceGroup::General;
};

class ModelResources
{
public:

	ModelResources(ID3D12Device& device);
	~ModelResources();

	VertexBufferModel* getModel(const std::string& name, ModelLoadContext& ctx);
	VertexBufferModel* getLoadedModel(const std::string& name, ResourceGroup group);

	UINT preloadFolder(ModelLoadContext& ctx);

	void clear(ResourceGroup group = ResourceGroup::General);

private:

	ID3D12Device& device;

	VertexBufferModel* loadModel(const std::string& name, ModelLoadContext& ctx);

	using ModelLibrary = std::map<std::string, VertexBufferModel*>;
	struct ModelLibraryGroup
	{
		ResourceGroup groupType;
		ModelLibrary models;
	};
	std::vector<ModelLibraryGroup> groups;
};