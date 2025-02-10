#include "ModelResources.h"
#include "FileLogger.h"
#include "Directories.h"
#include <sys/stat.h>
#include "OgreMeshFileParser.h"
#include <memory>
#include "BinaryModelLoader.h"
#include <filesystem>

static ModelResources* instance = nullptr;

ModelResources::ModelResources(ID3D12Device& d) : device(d)
{
	if (instance)
		throw std::exception("Duplicate AaModelResources");

	instance = this;
}

ModelResources::~ModelResources()
{
	for (auto it = groups.begin(); it != groups.end(); it++)
	{
		for (auto& m : it->models)
			delete m.second;
	}

	instance = nullptr;
}

VertexBufferModel* ModelResources::getModel(const std::string& filename, ResourceUploadBatch& batch, const ModelLoadContext& ctx)
{
	ModelLibraryGroup* group = nullptr;

	for (auto& g : groups)
	{
		if (g.groupType == ctx.group)
		{
			group = &g;
		}
	}

	if (group)
	{
		auto it = group->models.find(filename);

		if (it != group->models.end())
			return it->second;
	}
	else
	{
		group = &groups.emplace_back(ModelLibraryGroup{ ctx.group });
	}

	VertexBufferModel* m = loadModel(filename, batch, ctx);
	if (m)
		group->models[filename] = m;

	return m;
}

VertexBufferModel* ModelResources::getLoadedModel(const std::string& filename, ResourceGroup group)
{
	for (auto& g : groups)
	{
		if (g.groupType == group)
		{
			auto it = g.models.find(filename);

			return it == g.models.end() ? nullptr : it->second;
		}
	}

	return nullptr;
}

UINT ModelResources::preloadFolder(ResourceUploadBatch& batch, const ModelLoadContext& ctx)
{
	UINT c{};
	std::error_code ec;

	for (const auto& entry : std::filesystem::directory_iterator(ctx.folder, ec))
	{
		if (entry.is_regular_file())
		{
			getModel(entry.path().filename().generic_string(), batch, ctx);
			c++;
		}
	}

	return c;
}

void ModelResources::clear(ResourceGroup group)
{
	for (auto it = groups.begin(); it != groups.end(); it++)
	{
		if (it->groupType == group)
		{
			for (auto& m : it->models)
				delete m.second;

			groups.erase(it);
			return;
		}
	}
}

VertexBufferModel* ModelResources::loadModel(const std::string& filename, ResourceUploadBatch& batch, const ModelLoadContext& ctx)
{
	VertexBufferModel* model{};

	FileLogger::log("Loading mesh file " + filename);

	std::string filepath = ctx.folder + filename;

	if (filename.ends_with("mesh"))
	{
		ModelParseOptions o;
		o.batch = &batch;
		o.device = &device;

		struct stat attrib;
		if (stat(filepath.c_str(), &attrib) == 0)
		{
			auto mesh = OgreMeshFileParser::load(filepath, o);

			if (!mesh.submeshes.empty())
			{
				model = mesh.submeshes[0].model;
				model->bbox = mesh.boundingBox;
			}
		}
		else if (stat((filepath + ".model").c_str(), &attrib) == 0)
		{
			model = BinaryModelLoader::load(filepath + ".model", o);
		}
	}

	if (!model)
		FileLogger::logError("Model " + filename + " failed to load");

	return model;
}
