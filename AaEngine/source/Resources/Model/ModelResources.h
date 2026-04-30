#pragma once

#include <string>
#include "RenderCore/RenderSystem.h"
#include "Resources/Model/VertexBufferModel.h"
#include "ResourceUploadBatch.h"

struct ModelLoadContext
{
	std::string folder;
	bool persistent = false;
};

class ModelResources
{
public:

	ModelResources(RenderSystem& rs);
	~ModelResources();

	VertexBufferModel* getModel(const std::string& name, ResourceUploadBatch& batch, const ModelLoadContext& ctx);
	VertexBufferModel* getModel(const std::string& path, ResourceUploadBatch& batch);
	VertexBufferModel* getCoreModel(const std::string& name);

	VertexBufferModel* addLoadedModel(const std::string& name, const std::string& group);
	void addLoadedModel(const std::string& name, VertexBufferModel*, const std::string& group);

	void clear();

private:

	ID3D12Device& device;

	UINT preloadFolder(ResourceUploadBatch& batch, const ModelLoadContext& ctx);

	VertexBufferModel* loadModel(const std::string& name, ResourceUploadBatch& batch, const ModelLoadContext& ctx);

	using ModelLibrary = std::map<std::string, VertexBufferModel*>;
	struct ModelLibraryGroup
	{
		std::string name;
		bool persistent{};
		ModelLibrary models;
	};
	std::vector<ModelLibraryGroup> groups;

	ModelLibrary& getGroupLibrary(const ModelLoadContext& ctx);
};
